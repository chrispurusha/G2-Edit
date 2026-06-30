/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * Reference credit on some of the excellent G2 comms protocol work by
 * Bruno Verhue in his Delphi editor application:
 *
 * https://www.bverhue.nl/g2dev/
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <time.h>
#include "defs.h"
#include "usbLog.h"
#include "types.h"
#include <libusb.h>
#include "utils.h"
#include "msgQueue.h"
#include "protocol.h"
#include "usbComms.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
#include <stdatomic.h>
#include <pthread.h>

#define VENDOR_ID                   (0xffc)
#define PRODUCT_ID                  (2)

// USB transfer timeouts (milliseconds)
#define USB_SEND_TIMEOUT_MS         (50)
#define USB_RECV_POLL_MS            (50)   // ePollYes idle poll — timeout is expected and normal
#define USB_RECV_ACK_MS             (500)  // simple command acknowledgment (SUB_RESPONSE_OK)
#define USB_RECV_DATA_MS            (3000) // data response — may be large or slow to prepare
#define USB_KEEPALIVE_INTERVAL_S    (2)    // macOS suspends USB after ~3s idle; keep well inside that

// Atomic flags for cross-thread signalling
static _Atomic bool           gotBadConnectionIndication          = false;
static _Atomic bool           gotPatchChangeIndication[MAX_SLOTS] = {0};
static _Atomic bool           gotPerfSettingsChangeIndication     = false;
static int32_t                stopCount                           = 0;

// Protected by usbStaticMutex
static pthread_t              usbThread                           = NULL;
static libusb_context *       libUsbCtx                           = NULL;
static libusb_device_handle * devHandle                           = NULL;

// Callback pointers protected by callbackMutex
static void                   (*wake_glfw_func_ptr)(void) = NULL;
static void                   (*full_patch_change_notify_func_ptr)(void) = NULL;

// Mutexes
static pthread_mutex_t        usbStaticMutex                      = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t        callbackMutex                       = PTHREAD_MUTEX_INITIALIZER;

// Keepalive: timestamp of the last successful inbound or outbound USB transfer
static time_t                 gLastActivityTime                   = 0;

// ---------------------------------------------------------------------------
// Callback registration
// ---------------------------------------------------------------------------

void register_glfw_wake_cb(void ( *func_ptr )(void)) {
    pthread_mutex_lock(&callbackMutex);
    wake_glfw_func_ptr = func_ptr;
    pthread_mutex_unlock(&callbackMutex);
}

void register_full_patch_change_notify_cb(void ( *func_ptr )(void)) {
    pthread_mutex_lock(&callbackMutex);
    full_patch_change_notify_func_ptr = func_ptr;
    pthread_mutex_unlock(&callbackMutex);
}

static void call_wake_glfw(void) {
    void (*func_ptr)(void) = NULL;

    pthread_mutex_lock(&callbackMutex);
    func_ptr = wake_glfw_func_ptr;
    pthread_mutex_unlock(&callbackMutex);

    if (func_ptr == NULL) {
        LOG_ERROR("Wake GLFW callback not registered\n");
        exit(1);
    }
    func_ptr();
}

static void call_full_patch_change_notify(void) {
    void (*func_ptr)(void) = NULL;

    pthread_mutex_lock(&callbackMutex);
    func_ptr = full_patch_change_notify_func_ptr;
    pthread_mutex_unlock(&callbackMutex);

    if (func_ptr == NULL) {
        LOG_ERROR("Full patch change callback not registered\n");
        exit(1);
    }
    func_ptr();
}

// ---------------------------------------------------------------------------
// libusb helpers
// ---------------------------------------------------------------------------

// On macOS, LIBUSB_ERROR_OTHER is typically a genuine protocol error rather
// than a disconnect, so it is excluded here and logged separately.
static bool is_disconnect_error(int err) {
    return err == LIBUSB_ERROR_NO_DEVICE
           || err == LIBUSB_ERROR_IO
           || err == LIBUSB_ERROR_PIPE;
}

// Must be called with usbStaticMutex held, or from a context where devHandle
// is not yet shared (e.g. open_and_claim_device on failure path).
static void close_device(void) {
    if (devHandle != NULL) {
        libusb_release_interface(devHandle, 0);
        libusb_close(devHandle);
        devHandle = NULL;
        LOG_DEBUG("Device closed\n");
    }
}

// Opens the G2 and claims interface 0. Returns true on success.
// On macOS: no kernel driver detach needed — libusb uses IOKit directly.
// libusb_reset_device on macOS triggers USBDeviceReEnumerate, which resets
// the bulk endpoint DATA0/DATA1 toggle bits — without it the host and device
// can be out of phase after a reconnect, causing all transfers to time out.
static bool open_and_claim_device(void) {
    devHandle = libusb_open_device_with_vid_pid(libUsbCtx, VENDOR_ID, PRODUCT_ID);

    if (devHandle == NULL) {
        return false;  // Device not found — normal while searching
    }
    int result = libusb_claim_interface(devHandle, 0);

    if (result != LIBUSB_SUCCESS) {
        LOG_ERROR("Failed to claim interface: %s\n", libusb_error_name(result));
        close_device();
        return false;
    }
    result    = libusb_reset_device(devHandle);

    if (result != LIBUSB_SUCCESS) {
        LOG_ERROR("Failed to reset device: %s\n", libusb_error_name(result));
        close_device();
        return false;
    }
    LOG_DEBUG("Device opened and interface claimed\n");
    return true;
}

// ---------------------------------------------------------------------------
// Async-backed synchronous transfer — replaces libusb_bulk_transfer.
// Drives the libusb event loop in 50ms slices so the wall-clock timeout is
// reliable even when the macOS IOKit backend ignores per-transfer timeouts.
// ---------------------------------------------------------------------------

static void LIBUSB_CALL usb_transfer_cb(struct libusb_transfer * xfer) {
    int * completed = (int *)xfer->user_data;

    *completed = 1;
}

static int usb_bulk_transfer_sync(libusb_device_handle * handle, uint8_t endpoint,
                                  uint8_t * buffer, int length, int * actual_length,
                                  unsigned int timeout_ms) {
    struct libusb_transfer * xfer      = NULL;
    int                      completed = 0;
    int                      r         = LIBUSB_SUCCESS;
    struct timeval           tv        = {0, 50 * 1000};
    struct timespec          start     = {0};
    struct timespec          now       = {0};
    int                      status    = LIBUSB_TRANSFER_ERROR;

    *actual_length = 0;

    xfer           = libusb_alloc_transfer(0);

    if (xfer == NULL) {
        return LIBUSB_ERROR_NO_MEM;
    }
    libusb_fill_bulk_transfer(xfer, handle, endpoint, buffer, length,
                              usb_transfer_cb, &completed, 0);

    r              = libusb_submit_transfer(xfer);

    if (r != LIBUSB_SUCCESS) {
        libusb_free_transfer(xfer);
        return r;
    }
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (!completed) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000L
                          + (now.tv_nsec - start.tv_nsec) / 1000000L;

        if ((unsigned int)elapsed_ms >= timeout_ms) {
            break;
        }
        r = libusb_handle_events_timeout_completed(libUsbCtx, &tv, &completed);

        if (r != LIBUSB_SUCCESS) {
            break;
        }
    }

    if (!completed) {
        libusb_cancel_transfer(xfer);
        clock_gettime(CLOCK_MONOTONIC, &start);

        while (!completed) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            long drain_ms = (now.tv_sec - start.tv_sec) * 1000L
                            + (now.tv_nsec - start.tv_nsec) / 1000000L;

            if (drain_ms >= 500L) {
                LOG_ERROR("Cancel drain timed out — leaking transfer\n");
                *actual_length = 0;
                return LIBUSB_ERROR_IO;
            }
            r = libusb_handle_events_timeout_completed(libUsbCtx, &tv, &completed);

            if (r != LIBUSB_SUCCESS) {
                LOG_ERROR("Event loop error during cancel drain: %s\n", libusb_error_name(r));
                *actual_length = 0;
                return LIBUSB_ERROR_IO;
            }
        }
    }
    *actual_length = xfer->actual_length;
    status         = xfer->status;
    libusb_free_transfer(xfer);

    switch (status) {
        case LIBUSB_TRANSFER_COMPLETED: return LIBUSB_SUCCESS;

        case LIBUSB_TRANSFER_CANCELLED: return LIBUSB_ERROR_TIMEOUT;

        case LIBUSB_TRANSFER_TIMED_OUT: return LIBUSB_ERROR_TIMEOUT;

        case LIBUSB_TRANSFER_STALL:     return LIBUSB_ERROR_PIPE;

        case LIBUSB_TRANSFER_NO_DEVICE: return LIBUSB_ERROR_NO_DEVICE;

        case LIBUSB_TRANSFER_OVERFLOW:  return LIBUSB_ERROR_OVERFLOW;

        default:                         return LIBUSB_ERROR_IO;
    }
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

// CMPerformanceHeaderDump (USB sub-command 0x11): G2 broadcasts this unsolicited when perf
// settings change. Uses CPerformanceHeader_11::ReadStream — the same struct as the .prf2 file
// header section, but on USB it carries a 2-byte CStreamSizer size word and fixed-width 16-byte
// names (not null-terminated-variable-length as in the 0x29 settings dump).
static void parse_perf_header(uint8_t * buff, uint32_t * bitPos) {
    int i = 0;

    // CStreamSizer size word (2 bytes) — read to advance past it; content size not needed
    read_bit_stream(buff, bitPos, 8);
    read_bit_stream(buff, bitPos, 8);

    // 8 global setting bytes — store for echo-back when sending perf header to G2
    gPerfSettings.globalMode    = (uint8_t)read_bit_stream(buff, bitPos, 8);
    gPerfSettings.rangeAndFlags = (uint8_t)read_bit_stream(buff, bitPos, 8);
    gPerfSettings.keyboardRange = (uint8_t)read_bit_stream(buff, bitPos, 8);
    read_bit_stream(buff, bitPos, 8);
    read_bit_stream(buff, bitPos, 8);
    gGlobalSettings.perfMode    = read_bit_stream(buff, bitPos, 8);
    LOG_DEBUG("  GlobalMode        = %u\n", gPerfSettings.globalMode);
    LOG_DEBUG("  RangeAndFlags     = %u\n", gPerfSettings.rangeAndFlags);
    LOG_DEBUG("  KeyboardRange     = %u\n", gPerfSettings.keyboardRange);
    LOG_DEBUG("  PerfMode          = %u\n", gGlobalSettings.perfMode); // TODO - Does this belong in synth or perf?
    read_bit_stream(buff, bitPos, 8);                                  // fixed 0x00
    read_bit_stream(buff, bitPos, 8);                                  // fixed 0x00

    for (i = 0; i < MAX_SLOTS; i++) {
        read_clavia_string(buff, bitPos, gGlobalSettings.slot[i].patchName, sizeof(gGlobalSettings.slot[i].patchName));
        uint8_t enabled = (uint8_t)read_bit_stream(buff, bitPos, 8);
        gGlobalSettings.slot[i].enabled       = enabled;
        gPerfSettings.slot[i].keyboardEnabled = (uint8_t)read_bit_stream(buff, bitPos, 8);
        gPerfSettings.slot[i].holdEnabled     = (uint8_t)read_bit_stream(buff, bitPos, 8);
        gPerfSettings.slot[i].rangeLower      = (uint8_t)read_bit_stream(buff, bitPos, 8);
        gPerfSettings.slot[i].rangeUpper      = (uint8_t)read_bit_stream(buff, bitPos, 8);
        read_bit_stream(buff, bitPos, 8);
        read_bit_stream(buff, bitPos, 8);
        read_bit_stream(buff, bitPos, 8);
        read_bit_stream(buff, bitPos, 8);  // 0x00
        read_bit_stream(buff, bitPos, 8);  // 0x00
        LOG_DEBUG("Slot %d:\n", i);
        LOG_DEBUG("  Name              = '%s'\n", gGlobalSettings.slot[i].patchName);
        LOG_DEBUG("  SlotEnabled       = %u\n", enabled);
        LOG_DEBUG("  KeyboardEnabled   = %u\n", gPerfSettings.slot[i].keyboardEnabled);
        LOG_DEBUG("  HoldEnabled       = %u\n", gPerfSettings.slot[i].holdEnabled);
        LOG_DEBUG("  RangeLower        = %u\n", gPerfSettings.slot[i].rangeLower);
        LOG_DEBUG("  RangeUpper        = %u\n", gPerfSettings.slot[i].rangeUpper);
    }

    exit(1); // TEMPORARY!!!!! Not sure we've ever actually seen one of these messages
}

static void parse_param_change(uint32_t slot, uint8_t * buff, int length) {
    uint32_t   bitPos    = 0;
    tModuleKey key       = {0};
    uint32_t   param     = 0;
    uint32_t   variation = 0;
    uint32_t   value     = 0;

    key.slot     = slot;
    key.location = read_bit_stream(buff, &bitPos, 8);
    key.index    = read_bit_stream(buff, &bitPos, 8);
    param        = read_bit_stream(buff, &bitPos, 8);
    value        = read_bit_stream(buff, &bitPos, 8);
    variation    = read_bit_stream(buff, &bitPos, 8);

    tModule *  module    = get_module(key);

    if (module != NULL) {
        if (variation < NUM_VARIATIONS_USB && param < MAX_NUM_PARAMETERS) {
            module->param[variation][param].value = value;
        } else {
            LOG_ERROR("parse_param_change: out-of-range variation=%u param=%u from G2\n", variation, param);
        }
    }
    LOG_DEBUG("Param change - module %u:%u param=%u value=%u\n",
              key.location, key.index, param, value);

    if (key.location == (uint32_t)locationMorph) {
        call_wake_glfw();
    }
}

static void parse_volume_indicator(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    int volumesToRead = 0;

    read_bit_stream(buff, bitPos, 8);  // start_idx (always 0 in practice)

    for (int32_t location = 1; location >= 0; location--) {
        for (int k = 0; k < MAX_NUM_MODULES; k++) {
            tModuleKey key    = {slot, (uint32_t)location, (uint32_t)k};
            tModule *  module = get_module(key);

            if (module != NULL) {
                switch (gModuleProperties[module->type].volumeType) {
                    case volumeTypeMono:
                    case volumeTypeCompress:
                    case volumeTypeSequencer:
                        volumesToRead = 1;
                        break;
                    case volumeTypeStereo:
                        volumesToRead = 2;
                        break;
                    case volumeTypeQuad:
                        volumesToRead = 4;
                        break;
                    case volumeTypeNone:
                        volumesToRead = 0;
                        break;
                }

                for (int i = 0; i < volumesToRead; i++) {
                    module->volume.value[i] = read_bit_stream(buff, bitPos, 8);
                    read_bit_stream(buff, bitPos, 8);  // hi byte: unused
                }
            }
        }
    }
}

static void parse_led_data(uint32_t slot, uint8_t * buff, uint32_t * bitPos, int length) {
    uint8_t  start_idx = read_bit_stream(buff, bitPos, 8);
    uint32_t led_count = 0;

    // G2 packs 2-bit LED values LSB-first (bits 0-1 = first LED, bits 2-3 = second,
    // etc.), but read_bit_stream reads MSB-first.  Reversing each data byte reconciles
    // the two orderings.  Only the packed data bytes are reversed — start_idx (buff[4])
    // is already consumed above and must not be touched.
    for (int i = 5; i < (length - 2); i++) {
        buff[i] = reverse_bits_in_byte(buff[i]);
    }

    // VA (location 1) first, then FX (location 0), each sorted by ascending module
    // index — this matches the order in which the G2 assigns LED sequence numbers.
    for (int32_t location = 1; location >= 0; location--) {
        for (int k = 0; k < MAX_NUM_MODULES; k++) {
            tModuleKey key    = {slot, (uint32_t)location, (uint32_t)k};
            tModule *  module = get_module(key);

            if (module != NULL) {
                if (gModuleProperties[module->type].ledType == ledTypeYes) {
                    if (led_count >= start_idx && led_count < (uint32_t)(start_idx + 40)) {
                        module->led.value = read_bit_stream(buff, bitPos, 2);
                    }
                    led_count++;
                }
            }
        }
    }
}

static int parse_resources_used(uint32_t slot, uint8_t * buff, uint32_t * bitPos, int length) {
    uint16_t cyclesRed  = 0;
    uint16_t cyclesBlue = 0;
    uint8_t  zpMem      = 0;
    uint16_t xmemV1     = 0;
    uint16_t ymemV1     = 0;
    uint16_t pmemV1     = 0;
    uint16_t xmemV2     = 0;
    uint16_t ymemV2     = 0;
    uint16_t pmemV2     = 0;
    uint16_t ramRaw     = 0;
    uint32_t qmemRaw    = 0;
    uint16_t rmemRaw    = 0;
    uint16_t unknown    = 0;
    uint8_t  location   = 0;
    uint8_t  sub        = 0;
    float    cyclesLoad = 0.0f;
    float    memLoad    = 0.0f;

    LOG_DEBUG("Got resources in use slot %u\n", slot);

    if (*bitPos < 8) {
        LOG_ERROR("Resources used: bitPos underflow (%u)\n", *bitPos);
        return EXIT_FAILURE;
    }
    *bitPos -= 8; // Multiple messages in here, so need to move back a byte to process each sub response

    while ((BIT_TO_BYTE(*bitPos)) < (length - CRC_BYTES)) {
        sub                                   = read_bit_stream(buff, bitPos, 8);

        if (sub != SUB_RESPONSE_RESOURCES_USED) {
            break;
        }
        location                              = read_bit_stream(buff, bitPos, 8);
        cyclesRed                             = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        cyclesBlue                            = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        zpMem                                 = read_bit_stream(buff, bitPos, 8);
        unknown                               = read_bit_stream(buff, bitPos, 16); // unknown5, discard
        xmemV1                                = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        ymemV1                                = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        pmemV1                                = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        xmemV2                                = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        ymemV2                                = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        pmemV2                                = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        ramRaw                                = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);
        qmemRaw                               = read_bit_stream(buff, bitPos, 32);
        rmemRaw                               = read_bit_stream(buff, bitPos, 8) * 128 + read_bit_stream(buff, bitPos, 8);

        cyclesLoad                            = ((float)cyclesRed + (float)cyclesBlue * 0.25f) * 100.0f / 1371.0f;

        float xmem  = (float)(xmemV1 + xmemV2) * 100.0f / 4336.0f;
        float ymem  = (float)(ymemV1 + ymemV2) * 100.0f / 2992.0f;
        float pmem  = (float)(pmemV1 + pmemV2) * 100.0f / 6498.0f;
        float zpmem = (float)zpMem * 100.0f / 128.0f;
        float ram   = (float)ramRaw * 100.0f * 7.6293945e-06f;
        float qmem  = (float)qmemRaw * 100.0f / 260096.0f;
        float rmem  = (float)rmemRaw * 100.0f * 0.00390625f;

        memLoad                               = fmaxf(fmaxf(fmaxf(fmaxf(fmaxf(fmaxf(xmem, ymem), pmem), zpmem), ram), qmem), rmem);

        cyclesLoad                            = roundf(cyclesLoad * 10.0f) / 10.0f;
        memLoad                               = roundf(memLoad * 10.0f) / 10.0f;
        gResourceAlloc[slot].mem[location]    = memLoad;
        gResourceAlloc[slot].cycles[location] = cyclesLoad;
    }
    return EXIT_SUCCESS;
}

static void parse_global_page(uint8_t * buff, uint32_t * bitPos) {
    static uint32_t count      = 0;
    uint8_t         globalPage = 0;

    globalPage  = read_bit_stream(buff, bitPos, 8);
    gGlobalPage = globalPage;
    LOG_DEBUG("%u Got global page Page=%u Pos=%u\n", count++, globalPage / 3, globalPage % 3);
}

static void parse_patch_version_change(uint8_t * buff, uint32_t * bitPos) {
    uint8_t changedSlot = read_bit_stream(buff, bitPos, 8);
    uint8_t newVersion  = read_bit_stream(buff, bitPos, 8);

    LOG_DEBUG("Patch version change: slot %u new version 0x%02x\n", changedSlot, newVersion);

    if (changedSlot < MAX_SLOTS) {
        if (newVersion != gGlobalSettings.slot[changedSlot].patchVersion) {
            gGlobalSettings.slot[changedSlot].patchVersion = newVersion;
            gotPatchChangeIndication[changedSlot]          = true;
        }
    }
}

static void parse_slot_selection(uint8_t * buff, uint32_t * bitPos) {
    LOG_DEBUG("Got slot selection dump\n");
    read_bit_stream(buff, bitPos, 4);  // 4 padding bits (always 0)

    for (uint32_t i = 0; i < MAX_SLOTS; i++) {
        uint8_t status = (uint8_t)read_bit_stream(buff, bitPos, 1);
        gGlobalSettings.slot[i].enabled = status;
        LOG_DEBUG("  Slot %u enabled: %u\n", i, status != 0 ? 1 : 0);
    }
}

static void parse_assigned_voices(uint8_t * buff, uint32_t * bitPos) {
    LOG_DEBUG("Got assigned voices response\n");

    for (int i = 0; i < MAX_SLOTS; i++) {
        gAssignedVoices[i] = read_bit_stream(buff, bitPos, 8);  // TODO - might have to set target assigned voices to lower number, before attempting increase?
    }
}

static void parse_perf_mode_change(uint8_t * buff, uint32_t * bitPos) {
    gGlobalSettings.perfMode = read_bit_stream(buff, bitPos, 8);
    LOG_DEBUG("Got perf mode change: %u\n", gGlobalSettings.perfMode);  // TODO - check this one
}

static void parse_master_clock(uint8_t * buff, uint32_t * bitPos) {
    uint8_t clock   = 0;
    uint8_t running = 0;
    uint8_t type    = 0;

    LOG_DEBUG("Got master clock\n");
    read_bit_stream(buff, bitPos, 8);  // 0xff - not sure what this is, or if it ever changes
    type = read_bit_stream(buff, bitPos, 8);

    if (type == 1) {
        clock                       = read_bit_stream(buff, bitPos, 8);
        gGlobalSettings.masterClock = clock;
        LOG_DEBUG_DIRECT("Master clock = %u\n", clock);
    } else if (type == 0) {
        running                            = read_bit_stream(buff, bitPos, 8);
        gGlobalSettings.masterClockRunning = running;
        LOG_DEBUG_DIRECT("Clock running = %u\n", running);
    }
}

static void parse_select_slot(uint8_t * buff, uint32_t * bitPos) {
    uint32_t newSlot = read_bit_stream(buff, bitPos, 8);

    LOG_DEBUG("Got slot select %u\n", newSlot);

    gSlot                 = newSlot;
    gPatchParamsEdit.slot = newSlot;
    set_exclusive_button_highlight(topbarSlotAId, topbarSlotDId,
                                   (tTopbarControlId)(topbarSlotAId + newSlot));
    set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId,
                                   (tTopbarControlId)((uint32_t)topbarVariation1Id + gPatchDescr[newSlot].activeVariation));
}

static int parse_get_patch_name(uint32_t slot, uint8_t * buff, uint32_t * bitPos, int length) {
    int nameBytes = length - 6;

    LOG_DEBUG("Got patch name (length %d)\n", length);

    if ((nameBytes < 0) || (nameBytes > CLAVIA_NAME_SIZE)) {
        LOG_ERROR("Patch name length out of range: %d\n", nameBytes);
        return EXIT_FAILURE;
    }
    read_clavia_string(buff, bitPos, gGlobalSettings.slot[slot].patchName, sizeof(gGlobalSettings.slot[slot].patchName));
    LOG_DEBUG("Patch name: %s\n", gGlobalSettings.slot[slot].patchName);
    return EXIT_SUCCESS;
}

static void parse_select_param(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    uint32_t unknown     = read_bit_stream(buff, bitPos, 8);
    uint32_t location    = read_bit_stream(buff, bitPos, 8);
    uint32_t moduleIndex = read_bit_stream(buff, bitPos, 8);
    uint32_t paramIndex  = read_bit_stream(buff, bitPos, 8);

    (void)unknown;

    if (slot < MAX_SLOTS) {
        gSelectedParam[slot].location    = location;
        gSelectedParam[slot].moduleIndex = moduleIndex;
        gSelectedParam[slot].paramIndex  = paramIndex;
    }
    LOG_DEBUG("Got select param: slot=%u location=%u module=%u param=%u\n",
              slot, location, moduleIndex, paramIndex);
}

static void parse_select_variation(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    uint8_t variation = read_bit_stream(buff, bitPos, 8);

    LOG_DEBUG("Got variation select\n");
    gPatchDescr[slot].activeVariation = variation;
    set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId,
                                   (tTopbarControlId)(topbarVariation1Id + variation));
}

static void parse_sel_param_page(uint8_t * buff, uint32_t * bitPos) {
    uint8_t paramPage = read_bit_stream(buff, bitPos, 8);

    LOG_DEBUG("Got param page Page=%u Pos=%u\n", paramPage / 3, paramPage % 3);
}

static void parse_store_patch(uint8_t * buff, uint32_t * bitPos) {
    uint32_t savedBitPos = *bitPos;

    LOG_DEBUG("Got store patch\n");

    for (int i = 0; i < 32; i++) {
        LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, bitPos, 8));
    }

    LOG_DEBUG_DIRECT("\n");
    *bitPos = savedBitPos;
}

static void parse_perf_patch_versions(uint8_t * buff, uint32_t * bitPos) {
    uint8_t newVersion  = 0;
    uint8_t readSlot    = 0;
    uint8_t subResponse = 0;

    LOG_DEBUG("\nGot performance patch versions\n\n");

    newVersion = read_bit_stream(buff, bitPos, 8);
    LOG_DEBUG("Old perf = %u new = %u\n", gGlobalSettings.perfVersion, newVersion);

    if (newVersion != gGlobalSettings.perfVersion) {
        gGlobalSettings.perfVersion     = newVersion;
        // Use a flag rather than queuing so rapid switches coalesce into one resync.
        gotPerfSettingsChangeIndication = true;
    }

    for (int i = 0; i < MAX_SLOTS; i++) {
        subResponse = read_bit_stream(buff, bitPos, 8);
        readSlot    = read_bit_stream(buff, bitPos, 8);
        newVersion  = read_bit_stream(buff, bitPos, 8);

        if (subResponse == SUB_RESPONSE_PATCH_VERSION) {
            LOG_DEBUG("Store old patch %u ver = %u new = %u\n", readSlot, gGlobalSettings.slot[readSlot].patchVersion, newVersion);

            if (newVersion != gGlobalSettings.slot[readSlot].patchVersion) {
                gGlobalSettings.slot[readSlot].patchVersion = newVersion;
                gotPatchChangeIndication[readSlot]          = true;
            }
        }
    }
}

static int parse_command_response(uint8_t * buff, uint32_t * bitPos,
                                  uint8_t commandResponse, uint8_t subCommand,
                                  int length) {
    uint32_t slot = commandResponse & 0x03;

    switch (subCommand) {
        case SUB_RESPONSE_VOLUME_INDICATOR:
            parse_volume_indicator(slot, buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_LED_DATA:
            parse_led_data(slot, buff, bitPos, length);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_ERROR:
            LOG_DEBUG("Got Error!!!\n");
            return EXIT_FAILURE;

        case SUB_RESPONSE_RESOURCES_USED:
            return parse_resources_used(slot, buff, bitPos, length);

        case SUB_RESPONSE_KNOBS:
            LOG_DEBUG("Got knob snapshot slot %u\n", slot);
            parse_knobs(slot, buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PARAM_CHANGE:
            parse_param_change(slot, &buff[BIT_TO_BYTE(*bitPos)],
                               length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PATCH_VERSION:
            LOG_DEBUG("Got patch version\n");
            return parse_patch_version(&buff[BIT_TO_BYTE(*bitPos)],
                                       length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);

        case SUB_RESPONSE_SYNTH_SETTINGS:
            LOG_DEBUG("Got synth settings\n");
            return parse_synth_settings(&buff[BIT_TO_BYTE(*bitPos)],
                                        length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);

        case SUB_RESPONSE_MIDI_CC:
            LOG_DEBUG("Got MIDI CC response slot %u\n", slot);
            parse_midi_cc(&buff[BIT_TO_BYTE(*bitPos)],
                          length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_GLOBAL_PAGE:
            parse_global_page(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PATCH_VERSION_CHANGE:
            parse_patch_version_change(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SLOT_SELECTION:
            parse_slot_selection(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_ASSIGNED_VOICES:
            parse_assigned_voices(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SET_ASSIGNED_VOICES:
            LOG_DEBUG("Got assigned voices command — unexpected\n");
            return EXIT_SUCCESS;

        case SUB_COMMAND_SET_PARAM_MODE:
            parse_perf_mode_change(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PERF_HEADER:
            LOG_DEBUG("Got perf header dump\n");
            parse_perf_header(buff, bitPos);  // TODO - I don't think we've ever seen one of these. Seems to always be SUB_RESPONSE_PERFORMANCE_SETTINGS. Might want to remove
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PERFORMANCE_SETTINGS:
            LOG_DEBUG("Got performance settings\n");
            parse_performance_settings(&buff[BIT_TO_BYTE(*bitPos)],
                                       length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_MASTER_CLOCK:
            parse_master_clock(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SELECT_SLOT:
            parse_select_slot(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PATCH_DESCRIPTION:
            LOG_DEBUG("Got patch description\n");
            parse_patch(slot, &buff[BIT_TO_BYTE(*bitPos) - 1],
                        (length - BIT_TO_BYTE(*bitPos) - CRC_BYTES) + 1);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_GET_PATCH_NAME:
            return parse_get_patch_name(slot, buff, bitPos, length);

        case SUB_RESPONSE_OK:
            //LOG_DEBUG("GOT OK %f\n", get_time_ms());
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SELECT_PARAM:
            parse_select_param(slot, buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SELECT_VARIATION:
            parse_select_variation(slot, buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SEL_PARAM_PAGE:
            parse_sel_param_page(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_STORE_PATCH:
            parse_store_patch(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PERF_PATCH_VERSIONS:
            parse_perf_patch_versions(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_EXT_MASTER_CLOCK:
            // Don't think we need to process this since we're already setting data via unsolited and a bigger block of incoming data
            return EXIT_SUCCESS;

        case SUB_RESPONSE_GLOBAL_KNOBS:
            LOG_DEBUG("Got global knobs\n");
            read_bit_stream(buff, bitPos, 16);  // section byte count — consumed, not used
            parse_global_knobs(buff, bitPos);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_CURRENT_NOTE_2:
        {
            uint32_t count = read_bit_stream(buff, bitPos, 16);

            LOG_DEBUG("Got current note slot %u count %u\n", slot, count);
            store_note2(slot, buff, bitPos, count);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_NOTES:
        {
            uint32_t count = read_bit_stream(buff, bitPos, 16);

            LOG_DEBUG("Got patch notes slot %u count %u\n", slot, count);
            store_patch_notes(slot, buff, bitPos, count);
            return EXIT_SUCCESS;
        }

        default:
            LOG_DEBUG("Got unknown sub-command 0x%02x - must implement!!!\n", subCommand);
            exit(1);
    }
}

static int parse_incoming(uint8_t * buff, int length, int * response) {
    if (response != NULL) {
        *response = SUB_RESPONSE_ERROR;
    }

    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }
    uint32_t bitPos       = 0;
    uint8_t  responseType = read_bit_stream(buff, &bitPos, 8);
    int      ret          = EXIT_FAILURE;

    switch (responseType) {
        case RESPONSE_TYPE_INIT:
            LOG_DEBUG("Got response init\n");

            if (response != NULL) {
                *response = SUB_RESPONSE_OK;
            }
            ret = EXIT_SUCCESS;
            break;

        case RESPONSE_TYPE_COMMAND:
        {
            uint8_t commandResponse = read_bit_stream(buff, &bitPos, 8);
            /* version */            read_bit_stream(buff, &bitPos, 8);
            uint8_t subCommand      = read_bit_stream(buff, &bitPos, 8);
            ret = parse_command_response(buff, &bitPos, commandResponse, subCommand, length);

            if (ret == EXIT_SUCCESS) {
                if (response != NULL) {
                    *response = (int)subCommand;
                }
            }
            break;
        }

        default:
            LOG_DEBUG("Got unknown response type 0x%02x\n", responseType);
            ret = EXIT_FAILURE;
            break;
    }
    call_wake_glfw();
    return ret;
}

// ---------------------------------------------------------------------------
// USB receive functions
// ---------------------------------------------------------------------------

static int rcv_extended(int dataLength, int * response, unsigned int timeout_ms) {
    uint8_t                buff[EXTENDED_MESSAGE_SIZE] = {0};
    int                    readLength                  = 0;
    int                    retVal                      = EXIT_FAILURE;
    int                    try                         = 1;
    libusb_device_handle * devHandle_local             = NULL;
    double                 timeDelta                   = 0.0f;
    static double          largestDelta                = 0.0f;

    if (dataLength > EXTENDED_MESSAGE_SIZE) {
        LOG_ERROR("Expected message too large (%u > %u)\n", dataLength, EXTENDED_MESSAGE_SIZE);
        return EXIT_FAILURE;
    }
    pthread_mutex_lock(&usbStaticMutex);
    devHandle_local = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (devHandle_local == NULL) {
        LOG_ERROR("Device handle is NULL\n");
        return EXIT_FAILURE;
    }

    // The G2 sometimes sends a zero-length packet on 0x82 before the real data.
    // Retry up to 3 times on a 0-byte success to absorb ZLPs.
    for (try = 1; try <= 3; try++) {
        memset(buff, 0, sizeof(buff));
        readLength = 0;
        get_time_delta();
        retVal     = usb_bulk_transfer_sync(devHandle_local, 0x82, buff,
                                            sizeof(buff), &readLength,
                                            timeout_ms);
        timeDelta  = get_time_delta();

        if (timeDelta > largestDelta) {
            largestDelta = timeDelta;
        }
        //LOG_DEBUG("RX Ext Delta %dms largest %dms\n", (int)timeDelta, (int)largestDelta);

        if (retVal == LIBUSB_SUCCESS) {
            if (readLength > 0) {
                uint8_t responseType = buff[0];

                if (  (responseType == RESPONSE_TYPE_INIT)
                   || (responseType == RESPONSE_TYPE_COMMAND)) {
                    gUsbRxTime = (uint64_t)get_time_ms();
                    usb_log_message("EX", buff, (size_t)readLength);
                    break;
                } else {
                    LOG_DEBUG("Unexpected extended responseType 0x%02x — discarding\n", responseType);
                    break;
                }
            }
            // readLength == 0: ZLP — device not ready yet, retry
        } else if (is_disconnect_error(retVal)) {
            LOG_DEBUG("Disconnect error %s\n", libusb_error_name(retVal));
            gotBadConnectionIndication = true;
            return EXIT_FAILURE;
        } else {
            LOG_DEBUG("Transfer error %s\n", libusb_error_name(retVal));
            break;
        }
    }

    if (readLength != dataLength) {
        LOG_DEBUG("Length mismatch read=%d expected=%d\n", readLength, dataLength);

        if (readLength == 0) {
            LOG_DEBUG("Extended receive got no data — triggering reconnect\n");
            gotBadConnectionIndication = true;
        }
        return EXIT_FAILURE;
    }
    uint32_t bitPos = SIGNED_BYTE_TO_BIT(dataLength - 2);

    if (calc_crc16(buff, dataLength - 2) != read_bit_stream(buff, &bitPos, 16)) {
        LOG_DEBUG("Bad CRC\n");
        return EXIT_FAILURE;
    }
    return parse_incoming(buff, dataLength, response);
}

static int int_rec(tPoll poll, int expectedResponse, unsigned int timeout_ms) {
    uint8_t                buff[INTERRUPT_MESSAGE_SIZE] = {0};
    int                    readLength                   = 0;
    int                    retVal                       = EXIT_FAILURE;
    libusb_device_handle * devHandle_local              = NULL;
    bool                   doLoop                       = true;
    int                    response                     = SUB_RESPONSE_ERROR;
    double                 timeDelta                    = 0.0f;
    static double          largestDelta                 = 0.0f;
    int                    try                          = 0;

    for (try = 1; try <= 5 && doLoop == true; try++) {
        pthread_mutex_lock(&usbStaticMutex);
        devHandle_local = devHandle;
        pthread_mutex_unlock(&usbStaticMutex);

        if (devHandle_local == NULL) {
            LOG_ERROR("int_rec: device handle is NULL\n");
            return EXIT_FAILURE;
        }
        memset(buff, 0, sizeof(buff));
        readLength      = 0;
        get_time_delta();
        retVal          = usb_bulk_transfer_sync(devHandle_local, 0x81, buff,
                                                 sizeof(buff), &readLength,
                                                 timeout_ms);
        timeDelta       = get_time_delta();

        if (timeDelta > largestDelta) {
            largestDelta = timeDelta;
        }
        //LOG_DEBUG("RX Int Delta %dms largest %dms\n", (int)timeDelta, (int)largestDelta);

        if (retVal == LIBUSB_SUCCESS) {
            if (readLength > 0) {
                gUsbRxTime = (uint64_t)get_time_ms();
                usb_log_message("RX", buff, (size_t)readLength);
            }
        } else if (retVal == LIBUSB_ERROR_TIMEOUT) {
            if (poll == ePollYes) {
                return EXIT_SUCCESS;
            } else {
                return EXIT_FAILURE;
            }
        } else if (is_disconnect_error(retVal)) {
            LOG_DEBUG("int_rec: disconnect error %s\n", libusb_error_name(retVal));
            gotBadConnectionIndication = true;
            return EXIT_FAILURE;
        } else {
            LOG_DEBUG("int_rec: transfer error %s\n", libusb_error_name(retVal));
        }

        if (readLength <= 0) {
            return EXIT_FAILURE;
        }
        uint32_t bitPos     = 0;
        int      dataLength = read_bit_stream(buff, &bitPos, 4);
        int      type       = read_bit_stream(buff, &bitPos, 4);

        if (type == RESPONSE_TYPE_EXTENDED) {
            bool foundNoneZero = false;

            for (int i = 3; i < readLength; i++) {
                if (buff[i] != 0) {
                    foundNoneZero = true;
                    break;
                }
            }

            if (!foundNoneZero) {
                dataLength = read_bit_stream(buff, &bitPos, 16);
                retVal     = rcv_extended(dataLength, &response, timeout_ms);
            }
        } else if (type == RESPONSE_TYPE_EMBEDDED) {
            uint32_t crcBitPos = SIGNED_BYTE_TO_BIT(dataLength - 1);

            if (calc_crc16(&buff[1], dataLength - 2) != (uint16_t)read_bit_stream(buff, &crcBitPos, 16)) {
                LOG_DEBUG("Bad embedded CRC\n");
                retVal = EXIT_FAILURE;
            } else {
                retVal = parse_incoming(buff + 1, dataLength, &response);
            }
        }

        if (poll == ePollYes) {
            doLoop = false;                        // Idle poll — always exit after one
        } else {
            LOG_DEBUG("response = 0x%02x expected = 0x%02x\n", response, expectedResponse);

            if (response == expectedResponse) {
                doLoop = false;                   // Got what we wanted — retVal already correct
            } else {
                if (response == SUB_RESPONSE_ERROR) {
                    doLoop = false;
                    retVal = EXIT_FAILURE;            // Terminal response but not the expected one
                }
            }
        }
    }

    return retVal;
}

// ---------------------------------------------------------------------------
// USB send functions
// ---------------------------------------------------------------------------

static int send_message(uint8_t * buff, int pos) {
    int                    msgLength    = pos - COMMAND_OFFSET;
    int                    actualLength = 0;
    int                    result       = 0;
    uint16_t               crc          = 0;
    double                 timeDelta    = 0.0f;
    static double          largestDelta = 0.0f;

    if (msgLength <= 0) {
        return EXIT_FAILURE;
    }
    crc          = calc_crc16(&buff[COMMAND_OFFSET], msgLength);
    write_uint16(&buff[msgLength + 2], crc);
    msgLength   += 4;
    write_uint16(&buff[0], msgLength);

    pthread_mutex_lock(&usbStaticMutex);
    libusb_device_handle * handle       = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (handle == NULL) {
        gotBadConnectionIndication = true;
        return EXIT_FAILURE;
    }
    actualLength = 0;
    get_time_delta();
    result       = usb_bulk_transfer_sync(handle, 3, buff, msgLength,
                                          &actualLength, USB_SEND_TIMEOUT_MS);
    timeDelta    = get_time_delta();

    if (timeDelta > largestDelta) {
        largestDelta = timeDelta;
    }
    //LOG_DEBUG("TX Delta %dms largest %dms\n", (int)timeDelta, (int)largestDelta);

    if ((result == 0) && (actualLength == msgLength)) {
        gLastActivityTime = time(NULL);
        gUsbTxTime        = (uint64_t)get_time_ms();
        usb_log_message("TX", buff, (size_t)msgLength);
        return EXIT_SUCCESS;
    }

    if (actualLength != msgLength) {
        LOG_ERROR("Mismatch: actual length %d, msg length %d\n", actualLength, msgLength);
    }

    if (is_disconnect_error(result)) {
        LOG_DEBUG("disconnect error %s\n", libusb_error_name(result));
        gotBadConnectionIndication = true;
        return EXIT_FAILURE;
    } else {
        LOG_ERROR("transfer error %s, Time taken %f with timeout of %u\n", libusb_error_name(result), timeDelta, USB_SEND_TIMEOUT_MS);
    }
    return EXIT_FAILURE;
}

// For non-idempotent commands (add/delete module/cable): retry TX failures only.
// Never resends after a successful TX because the G2 already actioned the command.
static int send_and_receive_once(uint8_t * buff, int pos, int expectedResponse, unsigned int timeout_ms) {
    int retVal  = EXIT_FAILURE;
    int attempt = 0;

    for (attempt = 1; attempt <= 3; attempt++) {
        retVal = send_message(buff, pos);

        if (retVal == EXIT_SUCCESS) {
            retVal = int_rec(ePollNo, expectedResponse, timeout_ms);
            break;
        }
        LOG_ERROR("send failed, attempt %d\n", attempt);
    }

    return retVal;
}

static int send_and_receive(uint8_t * buff, int pos, int expectedResponse, unsigned int timeout_ms) {
    int retVal  = EXIT_FAILURE;
    int attempt = 0;

    for (attempt = 1; attempt <= 3; attempt++) {
        retVal = send_message(buff, pos);

        if (retVal == EXIT_SUCCESS) {
            retVal = int_rec(ePollNo, expectedResponse, timeout_ms);

            if (retVal == EXIT_SUCCESS) {
                break;
            }
            LOG_ERROR("receive failed, attempt %d\n", attempt);
        } else {
            LOG_ERROR("send failed, attempt %d\n", attempt);
        }
    }

    return retVal;
}

static int send_init(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    write_bit_stream(buff, &bitPos, 8, 0x80);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static void usb_cmd_sys(uint8_t * buff, uint32_t * bitPos, uint8_t version, uint8_t subCommand) {
    write_bit_stream(buff, bitPos, 8, 0x01);
    write_bit_stream(buff, bitPos, 8, COMMAND_REQ | COMMAND_SYS);
    write_bit_stream(buff, bitPos, 8, version);
    write_bit_stream(buff, bitPos, 8, subCommand);
}

static void usb_cmd_slot(uint8_t * buff, uint32_t * bitPos, uint32_t slot, uint8_t commandFlags, uint8_t subCommand) {
    write_bit_stream(buff, bitPos, 8, 0x01);
    write_bit_stream(buff, bitPos, 8, commandFlags | COMMAND_SLOT | (uint8_t)slot);
    write_bit_stream(buff, bitPos, 8, (uint8_t)gGlobalSettings.slot[slot].patchVersion);
    write_bit_stream(buff, bitPos, 8, subCommand);
}

static int send_stop(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);
    int      retVal                  = EXIT_SUCCESS;

    if (stopCount == 0) {
        usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_START_STOP);
        write_bit_stream(buff, &bitPos, 8, 0x01);
        retVal = send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
    }
    stopCount++;

    if (stopCount > 10) {
        LOG_ERROR("Stop message count went greater than 10\n");
        exit(1);
    }
    return retVal;
}

static int send_start(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);
    int      retVal                  = EXIT_SUCCESS;

    stopCount--;

    if (stopCount < 0) {
        LOG_ERROR("Stop message count went negative\n");
        exit(1);
    }

    if (stopCount == 0) {
        usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_START_STOP);  // Note: this sub command also starts, with correct param
        write_bit_stream(buff, &bitPos, 8, 0x00);
        retVal = send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
    }
    return retVal;
}

static int send_select_slot(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_SELECT_SLOT);   // Note that this is focus, not keyboard selection
    write_bit_stream(buff, &bitPos, 8, (uint8_t)slot);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_get_synth_settings(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_GET_SYNTH_SETTINGS);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_SYNTH_SETTINGS, USB_RECV_DATA_MS);
}

static int send_get_midi_cc(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_GET_MIDI_CC);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_MIDI_CC, USB_RECV_DATA_MS);
}

static int send_get_slot_selection(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_GET_SLOT_SELECTION);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_SLOT_SELECTION, USB_RECV_DATA_MS);
}

static int send_get_assigned_voices(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_GET_ASSIGNED_VOICES);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_ASSIGNED_VOICES, USB_RECV_DATA_MS);
}

static int send_get_master_clock(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_QUERY_MASTER_CLOCK);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_EXT_MASTER_CLOCK, USB_RECV_DATA_MS);
}

static int send_get_global_page(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_GET_GLOBAL_PAGE);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_GLOBAL_PAGE, USB_RECV_DATA_MS);
}

static int send_get_performance_settings(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    LOG_DEBUG("Send get performance settings\n");
    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_PERFORMANCE_SETTINGS);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_PERFORMANCE_SETTINGS, USB_RECV_DATA_MS);
}

static int send_get_patch_version(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    LOG_DEBUG("Send get patch version\n");
    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_GET_PATCH_VERSION);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)slot);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_PATCH_VERSION, USB_RECV_DATA_MS);
}

static int send_get_patch(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_GET_PATCH_SLOT);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_PATCH_DESCRIPTION, USB_RECV_DATA_MS);
}

static int send_get_patch_name(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_GET_PATCH_NAME);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_GET_PATCH_NAME, USB_RECV_DATA_MS);
}

static int send_get_resources_used(uint32_t slot, tLocation location) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_QUERY_RESOURCES);
    write_bit_stream(buff, &bitPos, 8, location);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_RESOURCES_USED, USB_RECV_DATA_MS);
}

static int send_set_module_label(uint32_t slot, tModuleKey moduleKey, const char * name) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_MODULE_LABEL);
    write_bit_stream(buff, &bitPos, 8, moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, moduleKey.index);
    write_clavia_string(buff, &bitPos, name);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_param_label(uint32_t slot, tModuleKey moduleKey, uint32_t paramIndex, const char * name) {
    uint8_t   buff[SEND_MESSAGE_SIZE]          = {0};
    uint32_t  bitPos                           = BYTE_TO_BIT(COMMAND_OFFSET);
    int       i                                = 0;
    uint32_t  pi                               = 0;
    uint32_t  labelCount                       = 0;
    uint32_t  labelIndices[MAX_NUM_PARAMETERS] = {0};

    LOG_DEBUG("SET PARAM LABEL slot=%u location=%u index=%u param=%u name='%s'\n",
              slot, moduleKey.location, moduleKey.index, paramIndex, name);

    tModule * module                           = get_module(moduleKey);

    if (module == NULL) {
        LOG_DEBUG("SET PARAM LABEL get_module FAILED\n");
        return EXIT_FAILURE;
    }

    for (pi = 0; pi < MAX_NUM_PARAMETERS; pi++) {
        if (module->paramNameSet[pi][0]) {
            labelIndices[labelCount++] = pi;
        }
    }

    if (labelCount == 0) {
        return EXIT_SUCCESS;
    }
    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_PARAM_LABEL);
    write_bit_stream(buff, &bitPos, 8, moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, moduleKey.index);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)(labelCount * (3 + PROTOCOL_PARAM_NAME_SIZE)));

    for (uint32_t j = 0; j < labelCount; j++) {
        pi = labelIndices[j];
        write_bit_stream(buff, &bitPos, 8, 1);                             // isString
        write_bit_stream(buff, &bitPos, 8, PROTOCOL_PARAM_NAME_SIZE + 1);  // paramLength
        write_bit_stream(buff, &bitPos, 8, (uint8_t)pi);                   // paramIndex

        for (i = 0; i < PROTOCOL_PARAM_NAME_SIZE; i++) {
            write_bit_stream(buff, &bitPos, 8, (uint8_t)module->paramName[pi][0][i]);
        }
    }

    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_param_value(uint32_t slot, tModuleKey moduleKey, uint32_t param, uint32_t value, uint32_t variation) {
    int      retVal                  = EXIT_FAILURE;
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_WRITE_NO_RESP, SUB_COMMAND_SET_PARAM);
    write_bit_stream(buff, &bitPos, 8, moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, moduleKey.index);
    write_bit_stream(buff, &bitPos, 8, param);
    write_bit_stream(buff, &bitPos, 8, value);
    write_bit_stream(buff, &bitPos, 8, variation);
    retVal = send_message(buff, BIT_TO_BYTE(bitPos));

    return retVal;
}

static int send_set_module_colour(uint32_t slot, uint32_t location,
                                  uint32_t moduleIndex, uint32_t colour) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_MODULE_COLOUR);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)colour);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_mode(uint32_t slot, tModeData * modeData) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_MODE);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)modeData->moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)modeData->moduleKey.index);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)modeData->mode);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)modeData->value);
    LOG_DEBUG("SET MODE %u %u\n", modeData->mode, modeData->value);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_write_cable(uint32_t slot, tCableData * cableData) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_WRITE_CABLE);
    write_bit_stream(buff, &bitPos, 8, 0x10 | ((uint8_t)cableData->location << 3) | (uint8_t)cableData->colour);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)cableData->moduleFromIndex);
    write_bit_stream(buff, &bitPos, 8, ((uint8_t)cableData->linkType << 6) | (uint8_t)cableData->connectorFromIoIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)cableData->moduleToIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)cableData->connectorToIoIndex);
    return send_and_receive_once(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_delete_cable(uint32_t slot, tCableData * cableData) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_DELETE_CABLE);
    write_bit_stream(buff, &bitPos, 8, 0x2 | (uint8_t)cableData->location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)cableData->moduleFromIndex);
    write_bit_stream(buff, &bitPos, 8, ((uint8_t)cableData->linkType << 6) | (uint8_t)cableData->connectorFromIoIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)cableData->moduleToIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)cableData->connectorToIoIndex);
    return send_and_receive_once(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_add_module(uint32_t slot, tModuleData * moduleData) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);
    int      avail                   = 0;
    int      written                 = 0;
    int      i                       = 0;

    LOG_DEBUG("Writing module\n");
    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_ADD_MODULE);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->type);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->moduleKey.index);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->column);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->row);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->colour);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->upRate);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->isLed);

    for (i = 0; i < (int)moduleData->modeCount; i++) {
        write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleData->mode[i]);
    }

    avail   = SEND_MESSAGE_SIZE - (int)BIT_TO_BYTE(bitPos);
    written = snprintf((char *)&buff[BIT_TO_BYTE(bitPos)], avail, "%s", moduleData->name);
    bitPos += (uint32_t)(((written >= 0) && (written < avail)) ? written + 1 : avail) * 8;
    return send_and_receive_once(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_move_module(uint32_t slot, tModuleKey moduleKey, uint32_t column, uint32_t row) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_MOVE_MODULE);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleKey.index);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)column);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)row);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_delete_module(uint32_t slot, tModuleKey moduleKey) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_DELETE_MODULE);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleKey.index);
    return send_and_receive_once(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_module_uprate(uint32_t slot, tModuleKey moduleKey, uint32_t upRate) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_MODULE_UPRATE);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleKey.index);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)upRate);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_morph_range(uint32_t slot, tParamMorphData * paramMorphData) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_WRITE_NO_RESP, SUB_COMMAND_SET_MORPH_RANGE);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramMorphData->moduleKey.location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramMorphData->moduleKey.index);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramMorphData->param);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramMorphData->paramMorph);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramMorphData->value);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramMorphData->negative);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramMorphData->variation);
    return send_message(buff, BIT_TO_BYTE(bitPos));
}

static int send_select_variation(uint32_t slot, uint32_t variation) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SELECT_VARIATION);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)variation);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

// ---------------------------------------------------------------------------
// Slot data management
// ---------------------------------------------------------------------------

static void clear_slot_data(uint32_t slot) {
    database_delete_modules_by_slot(slot);
    database_delete_cables_by_slot(slot);

    memset(&gPatchDescr[slot], 0, sizeof(tPatchDescr));
    memset(&gKnobArray[slot], 0, sizeof(tKnobArray));
    memset(&gControllerArray[slot], 0, sizeof(tControllerArray));
    gControllerCount[slot] = 0;
    gMorphCount[slot]      = 8;
    gPatchNotesSize[slot]  = 0;
    gNote2Size[slot]       = 0;
}

static int send_get_global_knobs(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_QUERY_GLOBAL_KNOBS);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_GLOBAL_KNOBS, USB_RECV_DATA_MS);
}

static int send_get_current_note(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_CURRENT_NOTE);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_CURRENT_NOTE_2, USB_RECV_DATA_MS);
}

static int send_get_patch_notes(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_QUERY_PATCH_TEXT);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_PATCH_NOTES, USB_RECV_DATA_MS);
}

static int send_get_selected_param(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_GET_SELECTED_PARAM);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_SELECT_PARAM, USB_RECV_DATA_MS);
}

static int send_get_knob_snapshot(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_KNOB_SNAPSHOT);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS); // Really expected SUB_RESPONSE_KNOBS here
}

static int send_set_seq_note_custom_data(uint32_t slot, tModuleKey key, uint32_t magnifier, uint32_t octave) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_PARAM_LABEL);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)key.location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)key.index);
    write_bit_stream(buff, &bitPos, 8, 6);                     // combined TLV data size
    write_bit_stream(buff, &bitPos, 8, 0);                     // zoom TLV: type
    write_bit_stream(buff, &bitPos, 8, 1);                     // zoom TLV: len
    write_bit_stream(buff, &bitPos, 8, (uint8_t)magnifier);    // zoom TLV: val (0=1oct, 1=2oct, 2=3oct)
    write_bit_stream(buff, &bitPos, 8, 0);                     // offset TLV: type
    write_bit_stream(buff, &bitPos, 8, 1);                     // offset TLV: len
    write_bit_stream(buff, &bitPos, 8, (uint8_t)octave);       // offset TLV: val (0=C0..7=C7)
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

// Fetch patch data for a single slot. Synth must be stopped before calling.
static int send_get_patch_data(uint32_t slot) {
    int retVal = EXIT_SUCCESS;

    clear_slot_data(slot);

    retVal |= send_get_patch_version(slot);
    retVal |= send_get_patch(slot);
    retVal |= send_get_patch_name(slot);
    retVal |= send_get_current_note(slot);
    retVal |= send_get_patch_notes(slot);
    retVal |= send_get_resources_used(slot, locationVa);
    retVal |= send_get_resources_used(slot, locationFx);

    if (send_get_knob_snapshot(slot) != EXIT_SUCCESS) {
        LOG_DEBUG("send_get_knob_snapshot slot %u failed — skipping\n", slot);
    }
    retVal |= send_get_selected_param(slot);

    return retVal;
}

// ---------------------------------------------------------------------------
// Patch upload helper — builds and sends one slot to the device.
// Synth must already be stopped before calling.
// Consumes the SUB_RESPONSE_PATCH_VERSION_CHANGE reply internally.
// ---------------------------------------------------------------------------

static int push_slot_to_device(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    LOG_DEBUG("Pushing slot %u to device\n", slot);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_PATCH);
    write_bit_stream(buff, &bitPos, 8, 0x00);
    write_bit_stream(buff, &bitPos, 8, 0x00);
    write_bit_stream(buff, &bitPos, 8, 0x00);

    write_clavia_string(buff, &bitPos, gGlobalSettings.slot[slot].patchName);

    write_patch_descr(slot, buff, &bitPos);
    write_module_list(slot, locationVa, buff, &bitPos);
    write_module_list(slot, locationFx, buff, &bitPos);
    write_current_note_2(slot, buff, &bitPos);
    write_cable_list(slot, locationVa, buff, &bitPos);
    write_cable_list(slot, locationFx, buff, &bitPos);
    write_param_list(slot, locationMorph, buff, &bitPos, NUM_VARIATIONS_USB);
    write_param_list(slot, locationVa, buff, &bitPos, NUM_VARIATIONS_USB);
    write_param_list(slot, locationFx, buff, &bitPos, NUM_VARIATIONS_USB);
    write_morph_params(slot, buff, &bitPos, NUM_VARIATIONS_USB);
    write_knobs(slot, buff, &bitPos);
    write_controllers(slot, buff, &bitPos);
    write_param_names(slot, locationMorph, buff, &bitPos);
    write_param_names(slot, locationVa, buff, &bitPos);
    write_param_names(slot, locationFx, buff, &bitPos);
    write_module_names(slot, locationVa, buff, &bitPos);
    write_module_names(slot, locationFx, buff, &bitPos);
    write_patch_notes(slot, buff, &bitPos);

    int expectedResp = SUB_RESPONSE_PATCH_VERSION;
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), expectedResp, USB_RECV_DATA_MS);
}

static int send_set_patch_name(uint32_t slot, const char * name) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_SET_PATCH_NAME);  // 0x27 — used for set AND response
    write_clavia_string(buff, &bitPos, name);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_assign_knob(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex, uint32_t knobIndex) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_ASSIGN_KNOB);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)(location << 6));
    write_bit_stream(buff, &bitPos, 8, 0x00);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)knobIndex);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_deassign_knob(uint32_t slot, uint32_t knobIndex) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_DEASSIGN_KNOB);
    write_bit_stream(buff, &bitPos, 8, 0x00);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)knobIndex);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_assign_global_knob(uint32_t slotIndex, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex, uint32_t knobIndex) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_ASSIGN_GLOBAL_KNOB);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)((slotIndex << 4) | (location << 2)));
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramIndex);
    write_bit_stream(buff, &bitPos, 8, 0x00);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)knobIndex);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_deassign_global_knob(uint32_t knobIndex) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_DEASSIGN_GLOBAL_KNOB);
    write_bit_stream(buff, &bitPos, 8, 0x00);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)knobIndex);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_assign_midi_cc(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex, uint32_t midiCC) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_ASSIGN_MIDICC);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)location);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)moduleIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)paramIndex);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)midiCC);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_deassign_midi_cc(uint32_t slot, uint32_t midiCC) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_DEASSIGN_MIDICC);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)midiCC);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_copy_variation(uint32_t slot, uint32_t fromVariation, uint32_t toVariation) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_slot(buff, &bitPos, slot, COMMAND_REQ, SUB_COMMAND_COPY_VARIATION);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)fromVariation);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)toVariation);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_master_clock_bpm(uint32_t bpm) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_SET_MASTER_CLOCK);
    write_bit_stream(buff, &bitPos, 8, 0xFF);
    write_bit_stream(buff, &bitPos, 8, 0x01);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)bpm);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_master_clock_run(uint32_t running) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_COMMAND_SET_MASTER_CLOCK);
    write_bit_stream(buff, &bitPos, 8, 0xFF);
    write_bit_stream(buff, &bitPos, 8, 0x00);
    write_bit_stream(buff, &bitPos, 8, running ? 0x01 : 0x00);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

// Write all synth (global) settings back to the G2.
static int send_synth_settings(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);
    uint32_t i                       = 0;

    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_SET_SYNTH_SETTINGS);

    write_clavia_string(buff, &bitPos, gSynthSettings.name);

    write_bit_stream(buff, &bitPos, 1, gGlobalSettings.perfMode);
    write_bit_stream(buff, &bitPos, 5, 0);
    write_bit_stream(buff, &bitPos, 2, gSynthSettings.patchSortMode);
    write_bit_stream(buff, &bitPos, 6, 0);
    write_bit_stream(buff, &bitPos, 2, gSynthSettings.perfSortMode);

    write_bit_stream(buff, &bitPos, 8, gSynthSettings.perfBank);
    write_bit_stream(buff, &bitPos, 8, gSynthSettings.perfLocation);
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.memoryProtect);
    write_bit_stream(buff, &bitPos, 7, 0);        // unknown

    for (i = 0; i < 4; i++) {
        write_bit_stream(buff, &bitPos, 8, gSynthSettings.midiChanSlot[i]);
    }

    write_bit_stream(buff, &bitPos, 8, gSynthSettings.globalChan);
    write_bit_stream(buff, &bitPos, 8, gSynthSettings.sysexId);
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.localOn);
    write_bit_stream(buff, &bitPos, 7, 0);        // unknown
    write_bit_stream(buff, &bitPos, 6, 0);        // unknown
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.progChangeRcv);
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.progChangeSnd);
    write_bit_stream(buff, &bitPos, 6, 0);        // unknown
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.controllersRcv);
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.controllersSnd);

    write_bit_stream(buff, &bitPos, 1, 0);
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.sendClock);
    write_bit_stream(buff, &bitPos, 1, !gSynthSettings.receiveClock);
    write_bit_stream(buff, &bitPos, 5, 0);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)gSynthSettings.tuneCent);
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.globalShiftActive);
    write_bit_stream(buff, &bitPos, 7, 0);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)gSynthSettings.globalOctaveShift);
    write_bit_stream(buff, &bitPos, 8, (uint8_t)gSynthSettings.tuneSemi);

    write_bit_stream(buff, &bitPos, 8, 0);        // filler - possibly should be vibrato rate, but doesn't seem to work
    write_bit_stream(buff, &bitPos, 1, gSynthSettings.pedalPolarity);
    write_bit_stream(buff, &bitPos, 1, 1);
    write_bit_stream(buff, &bitPos, 6, 0);
    write_bit_stream(buff, &bitPos, 8, gSynthSettings.pedalGain);

    for (i = 0; i < 16; i++) {
        write_bit_stream(buff, &bitPos, 8, 0);
    }

    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_set_patch_descr(uint32_t slot) { // Note - currently using values straight from patchDescr in sub-function
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    write_bit_stream(buff, &bitPos, 8, 0x01);
    write_bit_stream(buff, &bitPos, 8, COMMAND_REQ | COMMAND_SLOT | slot);
    write_bit_stream(buff, &bitPos, 8, gGlobalSettings.slot[slot].patchVersion);
    write_patch_descr(slot, buff, &bitPos);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_perf_header(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint8_t  payload[512]            = {0};
    uint32_t buffBitPos              = BYTE_TO_BIT(COMMAND_OFFSET);
    uint32_t bitPos                  = 0;
    uint32_t i                       = 0;

    // Build payload starting at byte 2, reserving bytes 0-1 for the CStreamSizer size word
    bitPos = BYTE_TO_BIT(2);

    // 8 global bytes
    write_bit_stream(payload, &bitPos, 8, gPerfSettings.globalMode);
    write_bit_stream(payload, &bitPos, 8, gPerfSettings.rangeAndFlags);
    write_bit_stream(payload, &bitPos, 8, gPerfSettings.keyboardRange);
    write_bit_stream(payload, &bitPos, 8, 0);
    write_bit_stream(payload, &bitPos, 8, 0);
    write_bit_stream(payload, &bitPos, 8, gGlobalSettings.perfMode);
    write_bit_stream(payload, &bitPos, 8, 0);
    write_bit_stream(payload, &bitPos, 8, 0);

    // Per-slot data — slot names are fixed CLAVIA_NAME_SIZE bytes (null-padded), per WriteStream
    for (i = 0; i < MAX_SLOTS; i++) {
        write_clavia_string(payload, &bitPos, gGlobalSettings.slot[i].patchName);

        write_bit_stream(payload, &bitPos, 8, gGlobalSettings.slot[i].enabled);
        write_bit_stream(payload, &bitPos, 8, gPerfSettings.slot[i].keyboardEnabled);
        write_bit_stream(payload, &bitPos, 8, gPerfSettings.slot[i].holdEnabled);
        write_bit_stream(payload, &bitPos, 8, gPerfSettings.slot[i].rangeLower);
        write_bit_stream(payload, &bitPos, 8, gPerfSettings.slot[i].rangeUpper);
        write_bit_stream(payload, &bitPos, 8, 0);
        write_bit_stream(payload, &bitPos, 8, 0);
        write_bit_stream(payload, &bitPos, 8, 0);
        write_bit_stream(payload, &bitPos, 8, 0);
        write_bit_stream(payload, &bitPos, 8, 0);
    }

    // Back-fill the CStreamSizer size word (big-endian; counts bytes after the word)
    uint32_t totalBytes   = BIT_TO_BYTE(bitPos);
    uint32_t contentBytes = totalBytes - 2;

    payload[0] = (uint8_t)((contentBytes >> 8) & 0xff);
    payload[1] = (uint8_t)(contentBytes & 0xff);

    usb_cmd_sys(buff, &buffBitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_RESPONSE_PERF_HEADER);

    for (i = 0; i < totalBytes && BIT_TO_BYTE(buffBitPos) < SEND_MESSAGE_SIZE; i++) {
        write_bit_stream(buff, &buffBitPos, 8, payload[i]);
    }

    return send_and_receive(buff, BIT_TO_BYTE(buffBitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

static int send_perf_name(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, (uint8_t)gGlobalSettings.perfVersion, SUB_RESPONSE_PERFORMANCE_SETTINGS);

    write_clavia_string(buff, &bitPos, gGlobalSettings.perfName);

    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_OK, USB_RECV_ACK_MS);
}

// SUB_COMMAND_SET_PARAM_MODE (0x3e) is CMPerformanceModeChange in the reference.
// Version byte 0x41 matches all other connection-level sys commands.
static int send_perf_mode_change(uint8_t perfMode) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint32_t bitPos                  = BYTE_TO_BIT(COMMAND_OFFSET);

    usb_cmd_sys(buff, &bitPos, 0x41, SUB_COMMAND_SET_PARAM_MODE);
    write_bit_stream(buff, &bitPos, 8, perfMode ? 1 : 0);
    write_bit_stream(buff, &bitPos, 8, 0x00);
    return send_and_receive(buff, BIT_TO_BYTE(bitPos), SUB_RESPONSE_PERF_PATCH_VERSIONS, USB_RECV_ACK_MS);
}

// ---------------------------------------------------------------------------
// Init sequences — linear, no state machine
// ---------------------------------------------------------------------------

// First connection: G2 is authoritative — pull all patch data from hardware.
static int send_init_sequence_pull(void) {
    LOG_DEBUG("Init sequence: pulling from G2\n");
    gCommsState = eCommsInitialising;

    // Clear any stale data before pulling fresh state
    database_clear_cables();
    database_clear_modules();

    send_init();
    send_stop();

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        send_get_patch_version(slot);
    }

    send_get_patch_version(4); // Performance slot
    send_get_synth_settings();
    send_get_midi_cc();
    send_select_slot(0);
    send_get_global_page();
    send_get_performance_settings();

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if (send_get_patch_data(slot) != EXIT_SUCCESS) {
            LOG_DEBUG("Setting to eCommsReconnecting state, due to send_get_patch_data(slot) failing\n");
            gCommsState = eCommsReconnecting;
            return EXIT_FAILURE;
        }
    }

    send_get_slot_selection();
    send_get_assigned_voices();
    send_get_global_knobs();
    send_get_master_clock();

    send_start();

    LOG_DEBUG("Pull init sequence complete\n");

    for (int i = 0; i < MAX_SLOTS; i++) {
        gotPatchChangeIndication[i] = false;
    }

    call_full_patch_change_notify();
    call_wake_glfw();

    return EXIT_SUCCESS;
}

// Push all editor state to the G2. Not called on reconnection; reserved for
// a future "push to device" menu action.
static int send_init_sequence_push(void) {
    LOG_DEBUG("Init sequence: pushing editor data to G2\n");
    gCommsState = eCommsInitialising;

    send_init();
    send_stop();
    send_synth_settings();

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if (push_slot_to_device(slot) != EXIT_SUCCESS) {
            LOG_DEBUG("Setting to eCommsReconnecting state, due to push_slot_to_device(slot) failing\n");
            gCommsState = eCommsReconnecting;
            return EXIT_FAILURE;
        }
    }

    // TODO - Maybe if/when we can write some of these items, we send data from the editor
    send_get_midi_cc();
    send_get_performance_settings();
    send_get_assigned_voices();
    send_get_global_knobs();
    send_get_master_clock();

    send_start();

    LOG_DEBUG("Push init sequence complete\n");

    for (int i = 0; i < MAX_SLOTS; i++) {
        gotPatchChangeIndication[i] = false;
    }

    call_full_patch_change_notify();
    call_wake_glfw();

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// send_write_data — runtime command dispatch
// ---------------------------------------------------------------------------

static int send_write_data(tMessageContent * messageContent) {
    int retVal = EXIT_FAILURE;

    switch (messageContent->cmd) {
        case eMsgCmdSetValue:
            retVal = send_set_param_value(messageContent->slot, messageContent->paramData.moduleKey, messageContent->paramData.param, messageContent->paramData.value, messageContent->paramData.variation);
            break;

        case eMsgCmdSetMode:
            send_stop();
            retVal = send_set_mode(messageContent->slot, &messageContent->modeData);
            send_start();
            break;

        case eMsgCmdWriteCable:
            send_stop();
            retVal = send_write_cable(messageContent->slot, &messageContent->cableData);
            send_start();
            break;

        case eMsgCmdWriteModule:
            send_stop();
            retVal = send_add_module(messageContent->slot, &messageContent->moduleData);
            send_start();
            break;
        case eMsgCmdMoveModule:
            send_stop();
            retVal = send_move_module(messageContent->slot, messageContent->moduleData.moduleKey,
                                      messageContent->moduleData.column, messageContent->moduleData.row);
            send_start();
            break;

        case eMsgCmdDeleteModule:
            send_stop();
            retVal = send_delete_module(messageContent->slot, messageContent->moduleData.moduleKey);
            send_start();
            break;

        case eMsgCmdSetModuleUpRate:
            send_stop();
            retVal = send_set_module_uprate(messageContent->slot, messageContent->moduleData.moduleKey,
                                            messageContent->moduleData.upRate);
            send_start();
            break;

        case eMsgCmdDeleteCable:
            send_stop();
            retVal = send_delete_cable(messageContent->slot, &messageContent->cableData);
            send_start();
            break;

        case eMsgCmdSetParamMorph:
            retVal = send_set_morph_range(messageContent->slot, &messageContent->paramMorphData);
            break;

        case eMsgCmdSelectVariation:
            send_stop();
            retVal = send_select_variation(messageContent->slot, messageContent->variationData.variation);
            send_start();
            break;

        case eMsgCmdSelectSlot:
            send_stop();
            retVal = send_select_slot(messageContent->slotData.slot);
            send_start();
            break;

        case eMsgCmdSetModuleLabel:
            send_stop();
            retVal = send_set_module_label(messageContent->slot, messageContent->moduleLabelData.moduleKey, messageContent->moduleLabelData.name);
            send_start();
            break;

        case eMsgCmdSetPatchName:
            send_stop();
            retVal = send_set_patch_name(messageContent->slot, messageContent->patchName.name);
            send_start();
            break;

        case eMsgCmdSetModuleColour:
            send_stop();
            retVal = send_set_module_colour(messageContent->slot, messageContent->moduleColourData.moduleKey.location, messageContent->moduleColourData.moduleKey.index, messageContent->moduleColourData.colour);
            send_start();
            break;

        case eMsgCmdWritePatch:
        {
            send_stop();
            retVal                                         = push_slot_to_device(messageContent->slot);
            send_start();

            gotPatchChangeIndication[messageContent->slot] = false; // TODO - consider if this is the right thing to do here
            call_full_patch_change_notify();                        // TODO - not sure we need to do this here
            call_wake_glfw();
            break;
        }

        case eMsgCmdWritePatchDescr:
        {
            send_stop();
            retVal = send_set_patch_descr(messageContent->slot);
            send_start();
            break;
        }

        case eMsgCmdAssignKnob:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            uint32_t kSlot  = messageContent->slot;
            uint32_t kLoc   = messageContent->knobAssignData.moduleKey.location;
            uint32_t kMod   = messageContent->knobAssignData.moduleKey.index;
            uint32_t kParam = messageContent->knobAssignData.paramIndex;
            uint32_t kKnob  = messageContent->knobAssignData.knobIndex;

            retVal = send_assign_knob(kSlot, kLoc, kMod, kParam, kKnob);
            send_start();
            break;
        }

        case eMsgCmdDeassignKnob:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_deassign_knob(messageContent->slot, messageContent->knobDeassignData.knobIndex);
            send_start();
            break;
        }

        case eMsgCmdAssignGlobalKnob:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            uint32_t gkSlot  = messageContent->globalKnobAssignData.slotIndex;
            uint32_t gkLoc   = messageContent->globalKnobAssignData.location;
            uint32_t gkMod   = messageContent->globalKnobAssignData.moduleIndex;
            uint32_t gkParam = messageContent->globalKnobAssignData.paramIndex;
            uint32_t gkKnob  = messageContent->globalKnobAssignData.knobIndex;

            retVal = send_assign_global_knob(gkSlot, gkLoc, gkMod, gkParam, gkKnob);
            send_start();
            break;
        }

        case eMsgCmdDeassignGlobalKnob:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_deassign_global_knob(messageContent->globalKnobDeassignData.knobIndex);
            send_start();
            break;
        }

        case eMsgCmdAssignMidiCC:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            uint32_t mLoc   = messageContent->midiCCAssignData.moduleKey.location;
            uint32_t mMod   = messageContent->midiCCAssignData.moduleKey.index;
            uint32_t mParam = messageContent->midiCCAssignData.paramIndex;
            uint32_t mCC    = messageContent->midiCCAssignData.midiCC;

            retVal = send_assign_midi_cc(messageContent->slot, mLoc, mMod, mParam, mCC);
            send_start();
            break;
        }

        case eMsgCmdDeassignMidiCC:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_deassign_midi_cc(messageContent->slot, messageContent->midiCCDeassignData.midiCC);
            send_start();
            break;
        }

        case eMsgCmdCopyVariation:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_copy_variation(messageContent->slot,
                                         messageContent->copyVariationData.fromVariation,
                                         messageContent->copyVariationData.toVariation);
            send_start();
            break;
        }

        case eMsgCmdSetMasterClockBPM:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_set_master_clock_bpm(messageContent->masterClockBPMData.bpm);
            send_start();
            break;
        }

        case eMsgCmdSetMasterClockRun:
        {
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_set_master_clock_run(messageContent->masterClockRunData.running);
            send_start();
            break;
        }

        case eMsgCmdSetParamLabel:
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_set_param_label(messageContent->slot, messageContent->paramLabelData.moduleKey, messageContent->paramLabelData.paramIndex, messageContent->paramLabelData.name);
            send_start();
            break;

        case eMsgCmdWriteSynthSettings:
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_synth_settings();
            send_start();
            break;

        case eMsgCmdWriteModePerf:
            send_stop();                     // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_perf_mode_change(1);
            send_get_performance_settings(); // At least need the perf settings, otherwise name etc. don't come in after change. Might need other data too
            send_start();
            break;

        case eMsgCmdWriteModePatch:
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_perf_mode_change(0);
            send_start();
            break;

        case eMsgCmdWritePerf:
        {
            send_stop();

            for (uint32_t s = 0; s < MAX_SLOTS; s++) {
                retVal = push_slot_to_device(s);
            }

            if (retVal == EXIT_SUCCESS) {
                retVal = send_perf_name();
            }

            if (retVal == EXIT_SUCCESS) {
                retVal = send_perf_header();
            }

            if (retVal == EXIT_SUCCESS) {
                retVal = send_set_master_clock_bpm(gGlobalSettings.masterClock);
            }

            if (retVal == EXIT_SUCCESS) {
                retVal = send_set_master_clock_run(gGlobalSettings.masterClockRunning);
            }

            if (retVal == EXIT_SUCCESS) {
                for (int i = 0; i < MAX_SLOTS; i++) {
                    gotPatchChangeIndication[i] = false;
                }

                call_full_patch_change_notify();
                call_wake_glfw();
            }
            send_start();
            break;
        }

        case eMsgCmdWritePerfSettings:
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_perf_header();
            send_start();
            break;

        case eMsgCmdWritePerfName:
            send_stop(); // Should stop any unsolicited messages TODO: might want to do this elsewhere
            retVal = send_perf_name();
            send_start();
            break;

        //case eMsgCmdReloadAllPatchData:  // TODO - do we really want to reload all patch data, and is it just patch data - this is currently doing a lot more
        //    LOG_DEBUG("\nGot msg queue command to read all patch data\n\n");
        //    retVal                         = reload_all_patch_data();
        //    gSlot= 0;
        //    gPatchDescr[0].activeVariation = 0;
        //   set_exclusive_button_highlight(topbarSlotAId, topbarSlotDId,
        //                                  (tTopbarControlId)(topbarSlotAId));
        //    set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId,
        //                                  (tTopbarControlId)((uint32_t)topbarVariation1Id));
        //   call_full_patch_change_notify();
        //   call_wake_glfw();
        //   break;

        case eMsgCmdSetCustomData:
        {
            tModuleKey key    = messageContent->customDataMsg.moduleKey;
            tModule *  module = get_module_slot(messageContent->slot, key.location, key.index);

            if (module->type == moduleTypeSeqNote) {
                retVal = send_set_seq_note_custom_data(
                    messageContent->slot, key,
                    messageContent->customDataMsg.customData[0],
                    messageContent->customDataMsg.customData[1]);
            }
            break;
        }

        default:
            LOG_DEBUG("Unknown command %d\n", messageContent->cmd);
            break;
    }
    return retVal;
}

// ---------------------------------------------------------------------------
// Main state handler — called in a tight loop from usb_thread_loop
// ---------------------------------------------------------------------------

static void state_handler(void) {
    tMessageContent messageContent = {0};
    bool            foundOneChange = false;

    //TODO - Don't like early returns. Use retVal

    if (gotBadConnectionIndication) {
        LOG_DEBUG("Bad connection — closing device\n");
        gotBadConnectionIndication = false;
        gCommsState                = eCommsReconnecting;

        pthread_mutex_lock(&usbStaticMutex);
        close_device();
        pthread_mutex_unlock(&usbStaticMutex);

        // Do NOT clear the database here — send_init_sequence_pull clears it
        // once a connection is re-established and fresh state is pulled.

        call_full_patch_change_notify();
        call_wake_glfw();
        return;
    }

    if (gCommsState == eCommsNeverConnected || gCommsState == eCommsReconnecting) {
        bool opened = false;

        pthread_mutex_lock(&usbStaticMutex);
        opened = open_and_claim_device();
        pthread_mutex_unlock(&usbStaticMutex);

        if (opened) {
            gCommsState = eCommsWaitingReady;
        } else {
            usleep(500000);  // 500ms between open attempts — don't hammer the bus
        }
        return;
    }

    if (gCommsState == eCommsWaitingReady) {
        if (send_get_patch_version(0) == EXIT_SUCCESS) {
            LOG_DEBUG("G2 ready — starting init sequence\n");
            int result = send_init_sequence_pull();

            if (result == EXIT_SUCCESS) {
                gCommsState = eCommsOnLine;
            } else {
                LOG_DEBUG("Init sequence failed — will retry\n");
                pthread_mutex_lock(&usbStaticMutex);
                close_device();
                pthread_mutex_unlock(&usbStaticMutex);
                gCommsState = eCommsReconnecting;
            }
        } else if (!gotBadConnectionIndication) {
            LOG_DEBUG("G2 not ready yet — polling\n");
            usleep(500000);  // 500ms between readiness polls
        }
        return;
    }

    // Performance/patch settings changed (e.g. perf mode switch on the G2 panel).
    // Flag coalesces rapid switches — only one full resync runs per batch.
    if (gotPerfSettingsChangeIndication) {
        gotPerfSettingsChangeIndication = false;

        LOG_DEBUG("\nPerf settings change — reloading all slots via reload_all_patch_date()\n\n");

        //if (reload_all_patch_data() != EXIT_SUCCESS) {
        //    LOG_ERROR("reload_all_patch_data failed\n");
        //}

        send_stop();
        send_get_performance_settings();  // TODO - maybe be some more items we need to get here
        send_start();

        gSlot                           = 0;
        gPatchDescr[0].activeVariation  = 0;
        set_exclusive_button_highlight(topbarSlotAId, topbarSlotDId,
                                       (tTopbarControlId)(topbarSlotAId));
        set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId,
                                       (tTopbarControlId)((uint32_t)topbarVariation1Id));

        call_full_patch_change_notify();
        call_wake_glfw();
        return;
    }

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (gotPatchChangeIndication[i] == true) {
            gotPatchChangeIndication[i] = false;
            LOG_DEBUG("Patch change on slot %u — reloading\n", i);
            send_stop();
            send_get_patch_data(i);
            send_start();
            foundOneChange              = true;
        }
    }

    if (foundOneChange == true) {
        call_full_patch_change_notify();
        call_wake_glfw();
        return;
    }

    // Command from UI thread
    if (msg_receive(&gCommandQueue, eRcvPoll, &messageContent) == EXIT_SUCCESS) {
        send_write_data(&messageContent);

        return;
    }
#if 0
    // Keepalive: if no outbound traffic for a while, send a lightweight request.
    // If the G2 doesn't respond, treat it as a bad connection and force a reconnect.
    if (time(NULL) - gLastActivityTime >= USB_KEEPALIVE_INTERVAL_S) {
        LOG_DEBUG("USB keepalive\n");

        if (send_get_patch_version(0) != EXIT_SUCCESS) {
            LOG_DEBUG("Keepalive failed — forcing reconnect\n");
            gotBadConnectionIndication = true;
        }
        return;
    }
#endif

    // Nothing to do — poll for unsolicited messages (LED, volume, param change)
    int_rec(ePollYes, SUB_RESPONSE_NULL, USB_RECV_POLL_MS);
}

// ---------------------------------------------------------------------------
// Signal handler and thread entry
// ---------------------------------------------------------------------------

static void usb_comms_signal_handler(int sigraised) {
    LOG_DEBUG("USBComms signal %d\n", sigraised);
    _exit(0);
}

static int usb_comms_init_signals(void) {
    signal(SIGINT, usb_comms_signal_handler);
    signal(SIGBUS, usb_comms_signal_handler);
    signal(SIGSEGV, usb_comms_signal_handler);
    signal(SIGTERM, usb_comms_signal_handler);
    signal(SIGABRT, usb_comms_signal_handler);
    return EXIT_SUCCESS;
}

static void * usb_thread_loop(void * arg) {
    usb_comms_init_signals();
    msg_init(&gCommandQueue, "command");
    usb_log_open();

    if (libusb_init(&libUsbCtx) != LIBUSB_SUCCESS) {
        LOG_ERROR("libusb_init failed\n");
        return NULL;
    }
    // Only if needed: libusb_set_option(libUsbCtx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    while (gQuitAll == false) {
        state_handler();
    }
    pthread_mutex_lock(&usbStaticMutex);
    close_device();
    pthread_mutex_unlock(&usbStaticMutex);

    libusb_exit(libUsbCtx);
    usb_log_close();
    return NULL;
}

void usb_signal_reconnect(void) {
    LOG_DEBUG("System wake detected — forcing USB reconnect\n");
    gotBadConnectionIndication = true;
}

void start_usb_thread(void) {
    if (pthread_create(&usbThread, NULL, usb_thread_loop, NULL) != EXIT_SUCCESS) {
        LOG_ERROR("Failed to create USB thread\n");
        exit(EXIT_FAILURE);
    }
}

#ifdef __cplusplus
}
#endif
