/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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
#include "types.h"
#include <libusb.h>
#include "utils.h"
#include "msgQueue.h"
#include "protocol.h"
#include "usbComms.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
#include "mouseHandle.h"
#include <stdatomic.h>
#include <pthread.h>

#define VENDOR_ID                   (0xffc)
#define PRODUCT_ID                  (2)

// USB transfer timeouts (milliseconds)
#define USB_SEND_TIMEOUT_MS         (400)
#define USB_EXT_RECV_TIMEOUT_MS     (2000)
#define USB_INT_RECV_TIMEOUT_MS     (100)
#define USB_INIT_TIMEOUT_MS         (1000) // Extra headroom during init sequence
#define USB_KEEPALIVE_INTERVAL_S    (2)    // macOS suspends USB after ~3s idle; keep well inside that

// Atomic flags for cross-thread signalling
static _Atomic bool           gotBadConnectionIndication      = false;
static _Atomic bool           gotPatchChangeIndication        = false;
static _Atomic bool           gotPerfSettingsChangeIndication = false;

// Protected by usbStaticMutex
static pthread_t              usbThread                       = NULL;
static libusb_context *       libUsbCtx                       = NULL;
static libusb_device_handle * devHandle                       = NULL;

// Callback pointers protected by callbackMutex
static void                   (*wake_glfw_func_ptr)(void) = NULL;
static void                   (*full_patch_change_notify_func_ptr)(void) = NULL;

// Mutexes
static pthread_mutex_t        usbStaticMutex                  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t        callbackMutex                   = PTHREAD_MUTEX_INITIALIZER;

// Keepalive: timestamp of the last successful inbound or outbound USB transfer
static time_t                 gLastActivityTime               = 0;

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
static bool open_and_claim_device(void) {
    devHandle = libusb_open_device_with_vid_pid(libUsbCtx, VENDOR_ID, PRODUCT_ID);

    if (devHandle == NULL) {
        return false;  // Device not found — normal while searching
    }
    int result = libusb_claim_interface(devHandle, 0);

    if (result != LIBUSB_SUCCESS) {
        LOG_ERROR("Failed to claim interface: %s\n", libusb_error_name(result));
        // Close cleanly — don't leak the opened handle
        close_device();
        return false;
    }
    result    = libusb_reset_device(devHandle);

    if (result != LIBUSB_SUCCESS) {
        LOG_ERROR("Failed to reset device: %s\n", libusb_error_name(result));
        // Close cleanly — don't leak the opened handle
        libusb_close(devHandle);
        devHandle = NULL;
        return false;
    }
    LOG_DEBUG("Device opened and interface claimed\n");
    return true;
}


// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

static int parse_synth_settings(uint8_t * buff, int length) {
    uint32_t bitPos = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }
    read_clavia_string(buff, &bitPos, gSynthSettings.name, sizeof(gSynthSettings.name));

    gSynthSettings.perfMode          = read_bit_stream(buff, &bitPos, 1);
    atomic_store(&gPerfMode, gSynthSettings.perfMode);
    read_bit_stream(buff, &bitPos, 5);  // Unused
    gSynthSettings.patchSortMode     = read_bit_stream(buff, &bitPos, 2);
    read_bit_stream(buff, &bitPos, 6);  // perfSortMode - unused
    gSynthSettings.perfSortMode      = read_bit_stream(buff, &bitPos, 2);
    gSynthSettings.perfBank          = read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.perfLocation      = read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.memoryProtect     = read_bit_stream(buff, &bitPos, 1);
    read_bit_stream(buff, &bitPos, 7);

    for (int i = 0; i < 4; i++) {
        gSynthSettings.midiChanSlot[i] = read_bit_stream(buff, &bitPos, 8);
    }

    gSynthSettings.globalChan        = read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.sysexId           = read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.localOn           = read_bit_stream(buff, &bitPos, 1);
    read_bit_stream(buff, &bitPos, 7);
    read_bit_stream(buff, &bitPos, 6);
    gSynthSettings.progChangeRcv     = read_bit_stream(buff, &bitPos, 1);
    gSynthSettings.progChangeSnd     = read_bit_stream(buff, &bitPos, 1);
    read_bit_stream(buff, &bitPos, 6);

    gSynthSettings.controllersRcv    = read_bit_stream(buff, &bitPos, 1);
    gSynthSettings.controllersSnd    = read_bit_stream(buff, &bitPos, 1);

    read_bit_stream(buff, &bitPos, 1);
    gSynthSettings.sendClock         = read_bit_stream(buff, &bitPos, 1);
    gSynthSettings.receiveClock      = !read_bit_stream(buff, &bitPos, 1);      // Note that this one is inverted. Is more of an ignore-external-clock
    read_bit_stream(buff, &bitPos, 5);

    gSynthSettings.tuneCent          = (int8_t)read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.globalShiftActive = read_bit_stream(buff, &bitPos, 1);
    read_bit_stream(buff, &bitPos, 7);
    gSynthSettings.globalOctaveShift = (int8_t)read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.tuneSemi          = (int8_t)read_bit_stream(buff, &bitPos, 8);

    read_bit_stream(buff, &bitPos, 8);                                         // vibratoRate - unused, it's a parameter instead
    gSynthSettings.pedalPolarity     = read_bit_stream(buff, &bitPos, 1);      // Bit 0. Next bit is always 1. 1 = closed, 0 = open
    read_bit_stream(buff, &bitPos, 7);
    //read_bit_stream(buff, &bitPos, 8);                                    // constant (always 1 in write) - unused
    gSynthSettings.pedalGain         = read_bit_stream(buff, &bitPos, 8);

    LOG_DEBUG("Name=%s\n",
              gSynthSettings.name);
    LOG_DEBUG("MIDI chan A=%u B=%u C=%u D=%u Global=%u SysexID=%u\n",
              gSynthSettings.midiChanSlot[0], gSynthSettings.midiChanSlot[1],
              gSynthSettings.midiChanSlot[2], gSynthSettings.midiChanSlot[3],
              gSynthSettings.globalChan, gSynthSettings.sysexId);
    LOG_DEBUG("LocalOn=%u MemProt=%u ProgRcv=%u ProgSnd=%u CtrlRcv=%u CtrlSnd=%u\n",
              gSynthSettings.localOn, gSynthSettings.memoryProtect,
              gSynthSettings.progChangeRcv, gSynthSettings.progChangeSnd,
              gSynthSettings.controllersRcv, gSynthSettings.controllersSnd);
    LOG_DEBUG("SendClock=%u ReceiveClock=%u TuneCent=%d TuneSemi=%d OctShift=%d ShiftActive=%u\n",
              gSynthSettings.sendClock, gSynthSettings.receiveClock,
              gSynthSettings.tuneCent, gSynthSettings.tuneSemi,
              gSynthSettings.globalOctaveShift, gSynthSettings.globalShiftActive);
    LOG_DEBUG("PedalPol=%u PedalGain=%u PerfMode=%u PerfBank=%u PerfLoc=%u\n",
              gSynthSettings.pedalPolarity, gSynthSettings.pedalGain,
              gSynthSettings.perfMode, gSynthSettings.perfBank, gSynthSettings.perfLocation);
    LOG_DEBUG("Patch Sort Mode=%u Perf Sort Mode=%u\n",
              gSynthSettings.patchSortMode, gSynthSettings.perfSortMode);

    { // Debug to highlight what we're changing on the G2 itself
        static uint8_t prevBuff[64] = {0};
        LOG_DEBUG("synth settings change!\n");

        for (int i = 0; i < 40; i++) {
            if (buff[i] != prevBuff[i]) {
                prevBuff[i] = buff[i];
                LOG_DEBUG("%d 0x%02x %u\n", i, buff[i], buff[i]);
            }
        }
    }

    return EXIT_SUCCESS;
}

static int parse_performance_settings(uint8_t * buff, int length) {
    uint32_t bitPos                     = 0;
    uint32_t selectedSlot               = 0;
    char     name[CLAVIA_NAME_SIZE + 1] = {0};
    int      i                          = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }
    // Performance name (ClaviaString — null terminated, max 16 bytes)
    read_clavia_string(buff, &bitPos, name, sizeof(name));
    LOG_DEBUG("Performance Name     = '%s'\n", name);

    // WriteSettings
    LOG_DEBUG("Unknown              = %u\n", read_bit_stream(buff, &bitPos, 8)); // Regular val of 17?
    LOG_DEBUG("Unknown              = %u\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Keyboard Range Enab  = %u\n", read_bit_stream(buff, &bitPos, 8)); // Regular val of 82 for standard mode and 87 for performance mode? Seen 0x74 for standard an 0x80 for perf too. Seems to be keyboard range enabled!
    LOG_DEBUG("Unknown              = %u\n", read_bit_stream(buff, &bitPos, 8));

    LOG_DEBUG("Unknown              = %u\n", read_bit_stream(buff, &bitPos, 4));
    selectedSlot = read_bit_stream(buff, &bitPos, 2);   // For focus rather than keyboard?
    LOG_DEBUG("SelectedSlot         = %u\n", selectedSlot);
    LOG_DEBUG("Unknown              = %u\n", read_bit_stream(buff, &bitPos, 2));
    LOG_DEBUG("RangeEnable          = %u\n", read_bit_stream(buff, &bitPos, 8));
    atomic_store(&gMasterClock, read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("MasterClock          = %u\n", atomic_load(&gMasterClock));
    LOG_DEBUG("KeyboardSplit        = %u\n", read_bit_stream(buff, &bitPos, 8));
    atomic_store(&gMasterClockRunning, read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("MasterClockRun       = %u\n", atomic_load(&gMasterClockRunning));
    LOG_DEBUG("Unknown              = %u\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Unknown              = %u\n", read_bit_stream(buff, &bitPos, 8));

    // 4 slots — TG2FileSlot.Write
    for (i = 0; i < MAX_SLOTS; i++) {
        // Patch name per slot (ReadName — null terminated, max 16 bytes)
        memset(name, 0, sizeof(name));
        read_clavia_string(buff, &bitPos, name, sizeof(name));
        LOG_DEBUG("Slot %d:\n", i);
        LOG_DEBUG("  PatchName         = '%s'\n", name);
        LOG_DEBUG("  Active            = %u\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("  Key               = %u\n", read_bit_stream(buff, &bitPos, 8)); // Seems to follow selected slot for keyboard selection?
        LOG_DEBUG("  Hold              = %u\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("  BankIndex         = %u\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("  PatchIndex        = %u\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("  RangeLower        = %u\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("  RangeUpper        = %u\n", read_bit_stream(buff, &bitPos, 8));
        read_bit_stream(buff, &bitPos, 8);
        read_bit_stream(buff, &bitPos, 8);
        read_bit_stream(buff, &bitPos, 8);
    }

    return EXIT_SUCCESS;
}

static int parse_midi_cc(uint8_t * buff, int length) {
    uint32_t bitPos      = 0;
    uint8_t  subResponse = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }

    while ((BIT_TO_BYTE_ROUND_UP(bitPos) + 3) <= length) {
        LOG_DEBUG("MIDI Chan 0x%x\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("CC Numb/value 0x%x\n", read_bit_stream(buff, &bitPos, 8));
        subResponse = read_bit_stream(buff, &bitPos, 8);

        if (subResponse != SUB_RESPONSE_MIDI_CC) {
            LOG_DEBUG("MIDI CC stream end on 0x%02x bitPos(byte)=%u length=%d\n",
                      subResponse, BIT_TO_BYTE_ROUND_UP(bitPos), length);
            break;
        }
    }
    return EXIT_SUCCESS;
}

static void store_note2(uint32_t slot, uint8_t * buff, uint32_t * bitPos, uint32_t count) {
    if (slot >= MAX_SLOTS) {
        return;
    }
    uint32_t safeCount = (count < sizeof(gNote2[slot])) ? count : (uint32_t)sizeof(gNote2[slot]);

    for (uint32_t i = 0; i < safeCount; i++) {
        gNote2[slot][i] = read_bit_stream(buff, bitPos, 8);
    }

    gNote2Size[slot] = safeCount;
}

static void store_patch_notes(uint32_t slot, uint8_t * buff, uint32_t * bitPos, uint32_t count) {
    if (slot >= MAX_SLOTS) {
        return;
    }
    uint32_t notesSize = count;

    if (notesSize > sizeof(gPatchNotes[0]) - 1) {
        LOG_ERROR("Patch notes size %u exceeds limit\n", notesSize);
        notesSize = (uint32_t)(sizeof(gPatchNotes[0]) - 1);
    }
    gPatchNotesSize[slot] = 0;
    memset(gPatchNotes[slot], 0, sizeof(gPatchNotes[0]));

    for (uint32_t i = 0; i < notesSize; i++) {
        gPatchNotes[slot][i] = read_bit_stream(buff, bitPos, 8);
    }

    gPatchNotesSize[slot] = notesSize;
}

int parse_patch(uint32_t slot, uint8_t * buff, int length) {
    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }
    uint32_t bitOffset = 0;

    while (BIT_TO_BYTE(bitOffset) < length) {
        uint8_t  type      = read_bit_stream(buff, &bitOffset, 8);
        int16_t  count     = 0;
        uint32_t subOffset = 0;

        if (type != SUB_RESPONSE_SEL_PARAM_PAGE) {
            count = (int16_t)read_bit_stream(buff, &bitOffset, 16);

            if (count < 0) {
                LOG_ERROR("parse_patch: negative count %d for type 0x%02x, aborting\n", count, type);
                return EXIT_FAILURE;
            }

            if (BIT_TO_BYTE(bitOffset) + count > length) {
                LOG_ERROR("parse_patch: count %d for type 0x%02x would exceed buffer length %d, aborting\n",
                          count, type, length);
                return EXIT_FAILURE;
            }
        }
        subOffset = bitOffset;

        LOG_DEBUG("Type = 0x%x, Count = %d\n", type, count);

        switch (type) {
            case SUB_RESPONSE_MODULE_LIST:
                parse_module_list(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CABLE_LIST:
                parse_cable_list(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_PARAM_LIST:
                parse_param_list(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_PARAM_NAMES:
                parse_param_names(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_MODULE_NAMES:
                parse_module_names(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_SEL_PARAM_PAGE:
                read_bit_stream(buff, &bitOffset, 8);
                break;

            case SUB_RESPONSE_PATCH_DESCRIPTION:
            {
                LOG_DEBUG("Patch Descr\n");
                uint32_t tmpSubOffset = subOffset;

                for (int i = 0; i < 32; i++) {
                    LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, &subOffset, 8));
                }

                LOG_DEBUG_DIRECT("\n");
                subOffset = tmpSubOffset;
                parse_patch_descr(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_MORPH_PARAMS:
                parse_morph_params(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_KNOBS:
                parse_knobs(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CONTROLLERS:
                parse_controllers(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CURRENT_NOTE_2:
                LOG_DEBUG("Current note 2\n");
                store_note2(slot, buff, &subOffset, (uint32_t)count);
                break;

            case SUB_RESPONSE_PATCH_NOTES:
                LOG_DEBUG("Patch notes\n");
                store_patch_notes(slot, buff, &subOffset, (uint32_t)count);
                break;

            default:
                LOG_DEBUG("Unprocessed type 0x%02x\n", type);
                break;
        }
        bitOffset += SIGNED_BYTE_TO_BIT(count);
    }
    return EXIT_SUCCESS;
}

static int parse_patch_version(uint8_t * buff, int length) {
    if ((buff == NULL) || (length < 2)) {
        return EXIT_FAILURE;
    }
    uint32_t bitPos  = 0;
    uint8_t  slot    = read_bit_stream(buff, &bitPos, 8);
    uint8_t  version = read_bit_stream(buff, &bitPos, 8);

    if (slot < MAX_SLOTS) {
        atomic_store(&gPatchVersion[slot], version);
    } else if (slot == MAX_SLOTS) {
        atomic_store(&gPerfVersion, version);
    } else {
        return EXIT_FAILURE;
    }
    LOG_DEBUG("Parsed patch version slot %u = 0x%02x or %u\n", slot, version, version);
    return EXIT_SUCCESS;
}

static void parse_param_change(uint32_t slot, uint8_t * buff, int length) {
    uint32_t   bitPos    = 0;
    tModule    module    = {0};
    tModuleKey key       = {0};
    uint32_t   param     = 0;
    uint32_t   variation = 0;
    uint32_t   value     = 0;

    key.slot                             = slot;
    key.location                         = read_bit_stream(buff, &bitPos, 8);
    key.index                            = read_bit_stream(buff, &bitPos, 8);
    param                                = read_bit_stream(buff, &bitPos, 8);
    value                                = read_bit_stream(buff, &bitPos, 8);
    variation                            = read_bit_stream(buff, &bitPos, 8);

    read_module(key, &module);
    module.param[variation][param].value = value;
    write_module(key, &module);

    LOG_DEBUG("Param change - module %u:%u param=%u value=%u\n",
              key.location, key.index, param, value);
}

static int parse_command_response(uint8_t * buff, uint32_t * bitPos,
                                  uint8_t commandResponse, uint8_t subCommand,
                                  int length) {
    tModule  module        = {0};
    uint32_t slot          = commandResponse & 0x03;
    int      i             = 0;
    int      volumesToRead = 0;

    switch (subCommand) {
        case SUB_RESPONSE_VOLUME_INDICATOR:
        {
            read_bit_stream(buff, bitPos, 8);  // start_idx (always 0 in practice)

            for (int32_t location = 1; location >= 0; location--) {
                for (int k = 0; k <= 255; k++) {
                    module.key.slot     = slot;
                    module.key.location = location;
                    module.key.index    = k;

                    if (read_module(module.key, &module) == true) {
                        switch (gModuleProperties[module.type].volumeType) {
                            case volumeTypeMono:
                            case volumeTypeCompress:
                            case volumeTypeSequencer:
                            {
                                volumesToRead = 1;
                                break;
                            }
                            case volumeTypeStereo:
                            {
                                volumesToRead = 2;
                                break;
                            }
                            case volumeTypeQuad:
                            {
                                volumesToRead = 4;
                                break;
                            }
                            case volumeTypeNone:
                            {
                                volumesToRead = 0;
                                break;
                            }
                        }

                        for (i = 0; i < volumesToRead; i++) {
                            module.volume.value[i] = read_bit_stream(buff, bitPos, 8);  // lo byte: level/state
                            read_bit_stream(buff, bitPos, 8);                           // hi byte: unused (always 0 for normal levels)
                        }

                        if (volumesToRead > 0) {
                            write_module(module.key, &module);
                        }
                    }
                }
            }

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_LED_DATA:
        {
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
                for (int k = 0; k <= 255; k++) {
                    module.key.slot     = slot;
                    module.key.location = location;
                    module.key.index    = k;

                    if (read_module(module.key, &module)) {
                        if (gModuleProperties[module.type].ledType == ledTypeYes) {
                            if (led_count >= start_idx && led_count < (uint32_t)(start_idx + 40)) {
                                module.led.value = read_bit_stream(buff, bitPos, 2);
                                write_module(module.key, &module);
                            }
                            led_count++;
                        }
                    }
                }
            }

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_ERROR:
            LOG_DEBUG("Got Error!!!\n");
            return EXIT_FAILURE;

        case SUB_RESPONSE_RESOURCES_USED:
        {
            LOG_DEBUG("Got resources in use slot %u\n", commandResponse);

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
                unknown                               = read_bit_stream(buff, bitPos, 16);              // unknown5, discard
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
        {
            static uint32_t count      = 0;
            uint8_t         globalPage = 0;

            globalPage = read_bit_stream(buff, bitPos, 8);
            LOG_DEBUG("%u Got global page Page=%u Pos=%u\n", count++, globalPage / 3, globalPage % 3);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_VERSION_CHANGE:
        {
            uint8_t changedSlot = read_bit_stream(buff, bitPos, 8);
            uint8_t newVersion  = read_bit_stream(buff, bitPos, 8);

            LOG_DEBUG("Patch version change: slot %u new version 0x%02x\n", changedSlot, newVersion);

            if (changedSlot < MAX_SLOTS) {
                atomic_store(&gPatchVersion[changedSlot], newVersion);
                atomic_store(&gChangedSlot, (uint32_t)changedSlot);
            }
            atomic_store(&gotPatchChangeIndication, true);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_ASSIGNED_VOICES:
        {
            int i = 0;
            LOG_DEBUG("Got assigned voices response\n");

            for (i = 0; i < MAX_SLOTS; i++) {
                gAssignedVoices[i] = read_bit_stream(buff, bitPos, 8);   // TODO - might have to set target assigned voices to lower number, before attempting increase?
            }

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_SET_ASSIGNED_VOICES:
            LOG_DEBUG("Got assigned voices command — unexpected\n");
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PERFORMANCE_SETTINGS:
            LOG_DEBUG("Got performance settings\n");
            parse_performance_settings(&buff[BIT_TO_BYTE(*bitPos)],
                                       length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_MASTER_CLOCK:
        {
            uint8_t clock   = 0;
            uint8_t running = 0;
            uint8_t type    = 0;

            LOG_DEBUG("Got master clock\n");

            read_bit_stream(buff, bitPos, 8); // 0xff - not sure what this is, or if it ever changes
            type = read_bit_stream(buff, bitPos, 8);

            if (type == 1) {
                clock = read_bit_stream(buff, bitPos, 8);
                atomic_store(&gMasterClock, clock);
                LOG_DEBUG_DIRECT("Master clock = %u\n", clock);
            } else if (type == 0) {
                running = read_bit_stream(buff, bitPos, 8);
                atomic_store(&gMasterClockRunning, running);
                LOG_DEBUG_DIRECT("Clock running = %u\n", running);
            }
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_SELECT_SLOT:
        {
            uint32_t newSlot = read_bit_stream(buff, bitPos, 8);
            LOG_DEBUG("Got slot select %u\n", newSlot);

            atomic_store(&gSlot, newSlot);
            set_exclusive_button_highlight(slotAButtonId, slotDButtonId,
                                           (tButtonId)(slotAButtonId + newSlot));
            set_exclusive_button_highlight(variation1ButtonId, variationInitButtonId,
                                           (tButtonId)((uint32_t)variation1ButtonId + gPatchDescr[newSlot].activeVariation));
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_DESCRIPTION:
            LOG_DEBUG("Got patch description\n");
            parse_patch(slot, &buff[BIT_TO_BYTE(*bitPos) - 1],
                        (length - BIT_TO_BYTE(*bitPos) - CRC_BYTES) + 1);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_GET_PATCH_NAME:
        {
            char patchName[CLAVIA_NAME_SIZE + 1] = {0};
            int  nameBytes                       = length - 6;

            LOG_DEBUG("Got patch name (length %d)\n", length);

            if ((nameBytes < 0) || (nameBytes > CLAVIA_NAME_SIZE)) {
                LOG_ERROR("Patch name length out of range: %d\n", nameBytes);
                return EXIT_FAILURE;
            }
            read_clavia_string(buff, bitPos, patchName, sizeof(patchName));
            LOG_DEBUG("Patch name: %s\n", patchName);

            patch_name_set(slot, patchName);

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_OK:
            //LOG_DEBUG("GOT OK %f\n", get_time_ms());
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SELECT_PARAM:
        {
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
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_SELECT_VARIATION:
            LOG_DEBUG("Got variation select\n");
            uint8_t variation = 0;

            variation                         = read_bit_stream(buff, bitPos, 8);
            gPatchDescr[slot].activeVariation = variation;

            set_exclusive_button_highlight(variation1ButtonId, variationInitButtonId, (tButtonId)(variation1ButtonId + variation));
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SEL_PARAM_PAGE:
        {
            uint8_t paramPage = 0;

            paramPage = read_bit_stream(buff, bitPos, 8);
            LOG_DEBUG("Got param page Page=%u Pos=%u\n", paramPage / 3, paramPage % 3);

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_STORE_PATCH:
        {
            uint32_t tmpSubOffset = *bitPos;

            LOG_DEBUG("Got store patch\n");

            for (int i = 0; i < 32; i++) {
                LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, bitPos, 8));
            }

            LOG_DEBUG_DIRECT("\n");
            *bitPos = tmpSubOffset;

            return EXIT_SUCCESS;
        }
        case SUB_RESPONSE_PERF_PATCH_VERSIONS:
        {
            uint8_t newVersion  = 0;
            uint8_t readSlot    = 0;
            uint8_t subResponse = 0;
            int     i           = 0;

            LOG_DEBUG("Got performance patch versions\n");

            newVersion = read_bit_stream(buff, bitPos, 8);
            LOG_DEBUG("Old perf = %u new = %u\n", atomic_load(&gPerfVersion), newVersion);
            atomic_store(&gPerfVersion, newVersion);

            for (i = 0; i < MAX_SLOTS; i++) {
                subResponse = read_bit_stream(buff, bitPos, 8);
                readSlot    = read_bit_stream(buff, bitPos, 8);
                newVersion  = read_bit_stream(buff, bitPos, 8);

                if (subResponse == SUB_RESPONSE_PATCH_VERSION) {
                    LOG_DEBUG("Store old patch %u ver = %u new = %u\n", readSlot, atomic_load(&gPatchVersion[readSlot]), newVersion);
                    atomic_store(&gPatchVersion[readSlot], newVersion);
                }
            }

            // Use a flag rather than queuing so rapid switches coalesce into one resync.
            atomic_store(&gotPerfSettingsChangeIndication, true);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_EXT_MASTER_CLOCK:
        {
            uint8_t  unknown = read_bit_stream(buff, bitPos, 8);
            uint16_t midiClk = (uint16_t)read_bit_stream(buff, bitPos, 8) << 8;

            midiClk |= (uint16_t)read_bit_stream(buff, bitPos, 8);
            LOG_DEBUG("Got ext master clock unknown=0x%02x clock=%u\n", unknown, midiClk);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_GLOBAL_KNOBS:
            LOG_DEBUG("Got global knobs\n");
            read_bit_stream(buff, bitPos, 16); // section byte count — consumed, not used
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

static int rcv_extended(int dataLength, int * response) {
    uint8_t                buff[EXTENDED_MESSAGE_SIZE] = {0};
    int                    readLength                  = 0;
    int                    retVal                      = EXIT_FAILURE;
    int                    try                         = 1;
    libusb_device_handle * devHandle_local             = NULL;

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

    for (try = 1; try <= 3; try++) {
        memset(buff, 0, sizeof(buff));
        readLength = 0;
        retVal     = libusb_bulk_transfer(devHandle_local, 0x82, buff,
                                          sizeof(buff), &readLength,
                                          USB_EXT_RECV_TIMEOUT_MS);

        if (retVal == LIBUSB_SUCCESS) {
            if (readLength > 0) {
                uint8_t responseType = buff[0];

                if (  (responseType == RESPONSE_TYPE_INIT)
                   || (responseType == RESPONSE_TYPE_COMMAND)) {
                    break;
                }
            }
        } else if (is_disconnect_error(retVal)) {
            LOG_DEBUG("Disconnect error %s\n", libusb_error_name(retVal));
            atomic_store(&gotBadConnectionIndication, true);
            return EXIT_FAILURE;
        } else {
            LOG_DEBUG("Transfer error %s\n", libusb_error_name(retVal));
        }
        usleep(1000);
    }

    if (readLength != dataLength) {
        LOG_DEBUG("Length mismatch read=%d expected=%d\n",
                  readLength, dataLength);

        if (readLength == 0) {
            // All retries returned no data — libusb endpoint 0x82 is stuck.
            // Trigger reconnect so recovery is automatic without restarting.
            LOG_DEBUG("Extended receive got no data after retries — triggering reconnect\n");
            atomic_store(&gotBadConnectionIndication, true);
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

static int int_rec(tPoll poll, int expectedResponse) {
    uint8_t                buff[INTERRUPT_MESSAGE_SIZE] = {0};
    int                    readLength                   = 0;
    int                    retVal                       = EXIT_FAILURE;
    libusb_device_handle * devHandle_local              = NULL;
    int                    timeout                      = USB_INT_RECV_TIMEOUT_MS;
    bool                   doLoop                       = true;
    int                    response                     = SUB_RESPONSE_ERROR;
    int                    try                          = 1;

    while (doLoop == true) {
        if (atomic_load(&gCommsState) != eCommsOnLine) {
            timeout = USB_INIT_TIMEOUT_MS;
        }
        pthread_mutex_lock(&usbStaticMutex);
        devHandle_local = devHandle;
        pthread_mutex_unlock(&usbStaticMutex);

        if (devHandle_local == NULL) {
            LOG_ERROR("int_rec: device handle is NULL\n");
            return EXIT_FAILURE;
        }

        for (try = 1; try <= 3; try++) {
            memset(buff, 0, sizeof(buff));
            readLength = 0;
            retVal     = libusb_bulk_transfer(devHandle_local, 0x81, buff,
                                              sizeof(buff), &readLength,
                                              timeout);

            if (retVal == LIBUSB_SUCCESS) {
                if (readLength > 0) {
                    gLastActivityTime = time(NULL);
                    break;
                }
            } else if (retVal == LIBUSB_ERROR_TIMEOUT) {
                if (poll == ePollYes) {
                    return EXIT_SUCCESS;
                } else {
                    return EXIT_FAILURE;
                }
            } else if (is_disconnect_error(retVal)) {
                LOG_DEBUG("int_rec: disconnect error %s\n", libusb_error_name(retVal));
                atomic_store(&gotBadConnectionIndication, true);
                return EXIT_FAILURE;
            } else {
                LOG_DEBUG("int_rec: transfer error %s\n", libusb_error_name(retVal));
            }
            usleep(1000);
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
                retVal     = rcv_extended(dataLength, &response);
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

            // SUB_RESPONSE_EXT_MASTER_CLOCK (0x5d) is an alternative form of the master clock
            // response; the G2 sends either depending on whether external clock is active.
            bool altMasterClock = (expectedResponse == SUB_RESPONSE_MASTER_CLOCK)
                                  && (response == SUB_RESPONSE_EXT_MASTER_CLOCK);

            if (response == expectedResponse || altMasterClock) {
                doLoop = false;                   // Got what we wanted — retVal already correct
            } else if (  (response == SUB_RESPONSE_OK)
                      || (response == SUB_RESPONSE_ERROR)) {
                doLoop = false;
                retVal = EXIT_FAILURE;            // Terminal response but not the expected one
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
    double                 timeDelta    = 0.0f;
    uint16_t               crc          = 0;
    int                    try          = 1;

    if (msgLength <= 0) {
        return EXIT_FAILURE;
    }
    crc        = calc_crc16(&buff[COMMAND_OFFSET], msgLength);
    write_uint16(&buff[msgLength + 2], crc);
    msgLength += 4;
    write_uint16(&buff[0], msgLength);

    pthread_mutex_lock(&usbStaticMutex);
    libusb_device_handle * handle       = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (handle == NULL) {
        atomic_store(&gotBadConnectionIndication, true);
        return EXIT_FAILURE;
    }

    for (try = 1; try <= 3; try++) {
        actualLength = 0;
        get_time_delta();
        result       = libusb_bulk_transfer(handle, 3, buff, msgLength,
                                            &actualLength, USB_SEND_TIMEOUT_MS);
        timeDelta    = get_time_delta();

        if ((result == 0) && (actualLength == msgLength)) {
            gLastActivityTime = time(NULL);
            return EXIT_SUCCESS;
        }

        if (actualLength != msgLength) {
            LOG_ERROR("Mismatch: actual length %d, msg length %d\n", actualLength, msgLength);
        }

        if (is_disconnect_error(result)) {
            LOG_DEBUG("disconnect error %s\n", libusb_error_name(result));
            atomic_store(&gotBadConnectionIndication, true);
            return EXIT_FAILURE;
        } else {
            LOG_ERROR("transfer error %s, Time taken %f with timeout of %u\n", libusb_error_name(result), timeDelta, USB_SEND_TIMEOUT_MS);
        }
        usleep(10000);
    }

    return EXIT_FAILURE;
}

static int send_init(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    buff[pos++] = 0x80;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("INIT RESPONSE\n");
    }
    return retVal;
}

static void usb_cmd_sys(uint8_t * buff, int * pos, uint8_t version, uint8_t subCommand) {
    buff[(*pos)++] = 0x01;
    buff[(*pos)++] = COMMAND_REQ | COMMAND_SYS;
    buff[(*pos)++] = version;
    buff[(*pos)++] = subCommand;
}

static void usb_cmd_slot(uint8_t * buff, int * pos, uint32_t slot, uint8_t commandFlags, uint8_t subCommand) {
    buff[(*pos)++] = 0x01;
    buff[(*pos)++] = commandFlags | COMMAND_SLOT | (uint8_t)slot;
    buff[(*pos)++] = (uint8_t)atomic_load(&gPatchVersion[slot]);
    buff[(*pos)++] = subCommand;
}

static int send_stop(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_START_STOP);
    buff[pos++] = 0x01;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("STOP RESPONSE\n");
    }
    return retVal;
}

static int send_start(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_START_STOP);
    buff[pos++] = 0x00;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("START RESPONSE\n");
    }
    return retVal;
}

static int send_select_slot(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_SELECT_SLOT);   // Note that this is focus, not keyboard selection
    buff[pos++] = (uint8_t)slot;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SELECT SLOT RESPONSE\n");
    }
    return retVal;
}

static int send_get_synth_settings(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_GET_SYNTH_SETTINGS);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_SYNTH_SETTINGS);
    }
    return retVal;
}

static int send_get_midi_cc(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_GET_MIDI_CC);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_MIDI_CC);
    }
    return retVal;
}

static int send_get_assigned_voices(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_SET_ASSIGNED_VOICES);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_ASSIGNED_VOICES);
    }
    return retVal;
}

static int send_get_master_clock(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_QUERY_MASTER_CLOCK);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_MASTER_CLOCK);
    }
    return retVal;
}

static int send_get_global_page(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_GET_GLOBAL_PAGE);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_GLOBAL_PAGE);
    }
    return retVal;
}

static int send_get_performance_settings(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    LOG_DEBUG("Send get performance settings\n");
    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_PERFORMANCE_SETTINGS);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_PERFORMANCE_SETTINGS);
    }
    return retVal;
}

static int send_get_patch_version(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    LOG_DEBUG("Send get patch version\n");
    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_GET_PATCH_VERSION);
    buff[pos++] = (uint8_t)slot;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_PATCH_VERSION);
    }
    return retVal;
}

static int send_get_patch(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_GET_PATCH_SLOT);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_PATCH_DESCRIPTION);
    }
    return retVal;
}

static int send_get_patch_name(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_GET_PATCH_NAME);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_GET_PATCH_NAME);
    }
    return retVal;
}

static int send_get_resources_used(uint32_t slot, tLocation location) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_QUERY_RESOURCES);
    buff[pos++] = location;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_RESOURCES_USED);
    }
    return retVal;
}

static int send_set_module_label(uint32_t slot, tModuleKey moduleKey, const char * name) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_SET_MODULE_LABEL);
    buff[pos++] = moduleKey.location;
    buff[pos++] = moduleKey.index;
    {
        uint32_t bitPos = BYTE_TO_BIT(pos);
        write_clavia_string(buff, &bitPos, name);
        pos = BIT_TO_BYTE(bitPos);
    }
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SET MODULE LABEL RESPONSE\n");
    }
    return retVal;
}

static int send_set_param_label(uint32_t slot, tModuleKey moduleKey, uint32_t paramIndex, const char * name) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_SET_PARAM_LABEL);
    buff[pos++] = moduleKey.location;
    buff[pos++] = moduleKey.index;
    buff[pos++] = 3 + PROTOCOL_PARAM_NAME_SIZE;  // size: isString + paramLength + paramIndex + name bytes
    buff[pos++] = 1;                             // isString
    buff[pos++] = 1 + PROTOCOL_PARAM_NAME_SIZE;  // paramLength
    buff[pos++] = (uint8_t)paramIndex;

    for (int i = 0; i < PROTOCOL_PARAM_NAME_SIZE; i++) {
        buff[pos++] = (uint8_t)name[i];  // fixed-length, zero-padded
    }

    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SET PARAM LABEL RESPONSE\n");
    }
    return retVal;
}

static int send_set_param_value(uint32_t slot, tModuleKey moduleKey, uint32_t param, uint32_t value, uint32_t variation) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    usb_cmd_slot(buff, &pos, slot, COMMAND_WRITE_NO_RESP, SUB_COMMAND_SET_PARAM);
    buff[pos++] = moduleKey.location;
    buff[pos++] = moduleKey.index;
    buff[pos++] = param;
    buff[pos++] = value;
    buff[pos++] = variation;
    retVal      = send_message(buff, pos);

    return retVal;
}

static int send_set_module_colour(uint32_t slot, uint32_t location,
                                  uint32_t moduleIndex, uint32_t colour) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_SET_MODULE_COLOUR);
    buff[pos++] = (uint8_t)location;
    buff[pos++] = (uint8_t)moduleIndex;
    buff[pos++] = (uint8_t)colour;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SET MODULE COLOUR RESPONSE\n");
    }
    return retVal;
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
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_QUERY_GLOBAL_KNOBS);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_GLOBAL_KNOBS);
        LOG_DEBUG("GLOBAL KNOBS RESPONSE\n");
    }
    return retVal;
}

static int send_get_current_note(uint32_t slot) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_CURRENT_NOTE);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_CURRENT_NOTE_2);
        LOG_DEBUG("CURRENT NOTE RESPONSE\n");
    }
    return retVal;
}

static int send_get_patch_notes(uint32_t slot) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_QUERY_PATCH_TEXT);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_PATCH_NOTES);
        LOG_DEBUG("PATCH NOTES RESPONSE\n");
    }
    return retVal;
}

static int send_get_selected_param(uint32_t slot) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_GET_SELECTED_PARAM);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_SELECT_PARAM);
        LOG_DEBUG("SELECTED PARAM RESPONSE\n");
    }
    return retVal;
}

static int send_get_knob_snapshot(uint32_t slot) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_KNOB_SNAPSHOT);
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_KNOBS);
        LOG_DEBUG("KNOB SNAPSHOT RESPONSE\n");
    }
    return retVal;
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
    retVal |= send_get_knob_snapshot(slot);
    retVal |= send_get_selected_param(slot);

    return retVal;
}

// ---------------------------------------------------------------------------
// Patch upload helper — builds and sends one slot to the device.
// Synth must already be stopped before calling.
// Consumes the SUB_RESPONSE_PATCH_VERSION_CHANGE reply internally.
// ---------------------------------------------------------------------------

static int push_slot_to_device(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE]         = {0};
    int      pos                             = COMMAND_OFFSET;
    uint32_t bitPos                          = 0;
    int      retVal                          = EXIT_FAILURE;
    char     patchName[CLAVIA_NAME_SIZE + 1] = {0};

    LOG_DEBUG("Pushing slot %u to device\n", slot);

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_SET_PATCH);
    buff[pos++] = 0x00;
    buff[pos++] = 0x00;
    buff[pos++] = 0x00;

    patch_name_get(slot, patchName, sizeof(patchName));
    bitPos      = BYTE_TO_BIT(pos);
    write_clavia_string(buff, &bitPos, patchName);

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

    pos    = BIT_TO_BYTE(bitPos);

    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_PATCH_VERSION);

        LOG_DEBUG("PUSH SLOT RESPONSE\n");
    }
    return retVal;
}

static int send_set_patch_name(uint32_t slot, const char * name) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_SET_PATCH_NAME);  // 0x27 — used for set AND response
    {
        uint32_t bitPos = BYTE_TO_BIT(pos);
        write_clavia_string(buff, &bitPos, name);
        pos = BIT_TO_BYTE(bitPos);
    }
    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SET PATCH NAME RESPONSE\n");
    }
    return retVal;
}

static int send_assign_knob(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex, uint32_t knobIndex) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_ASSIGN_KNOB);
    buff[pos++] = (uint8_t)moduleIndex;
    buff[pos++] = (uint8_t)paramIndex;
    buff[pos++] = (uint8_t)(location << 6);
    buff[pos++] = 0x00;
    buff[pos++] = (uint8_t)knobIndex;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("ASSIGN KNOB RESPONSE\n");
    }
    return retVal;
}

static int send_deassign_knob(uint32_t slot, uint32_t knobIndex) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_DEASSIGN_KNOB);
    buff[pos++] = 0x00;
    buff[pos++] = (uint8_t)knobIndex;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("DEASSIGN KNOB RESPONSE\n");
    }
    return retVal;
}

static int send_assign_global_knob(uint32_t slotIndex, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex, uint32_t knobIndex) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_ASSIGN_GLOBAL_KNOB);
    buff[pos++] = (uint8_t)((slotIndex << 4) | (location << 2));
    buff[pos++] = (uint8_t)moduleIndex;
    buff[pos++] = (uint8_t)paramIndex;
    buff[pos++] = 0x00;
    buff[pos++] = (uint8_t)knobIndex;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("ASSIGN GLOBAL KNOB RESPONSE\n");
    }
    return retVal;
}

static int send_deassign_global_knob(uint32_t knobIndex) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_DEASSIGN_GLOBAL_KNOB);
    buff[pos++] = 0x00;
    buff[pos++] = (uint8_t)knobIndex;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("DEASSIGN GLOBAL KNOB RESPONSE\n");
    }
    return retVal;
}

static int send_assign_midi_cc(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex, uint32_t midiCC) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_ASSIGN_MIDICC);
    buff[pos++] = (uint8_t)location;
    buff[pos++] = (uint8_t)moduleIndex;
    buff[pos++] = (uint8_t)paramIndex;
    buff[pos++] = (uint8_t)midiCC;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("ASSIGN MIDI CC RESPONSE\n");
    }
    return retVal;
}

static int send_deassign_midi_cc(uint32_t slot, uint32_t midiCC) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_DEASSIGN_MIDICC);
    buff[pos++] = (uint8_t)midiCC;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("DEASSIGN MIDI CC RESPONSE\n");
    }
    return retVal;
}

static int send_copy_variation(uint32_t slot, uint32_t fromVariation, uint32_t toVariation) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_slot(buff, &pos, slot, COMMAND_REQ, SUB_COMMAND_COPY_VARIATION);
    buff[pos++] = (uint8_t)fromVariation;
    buff[pos++] = (uint8_t)toVariation;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("COPY VARIATION RESPONSE\n");
    }
    return retVal;
}

static int send_set_master_clock_bpm(uint32_t bpm) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_SET_MASTER_CLOCK);
    buff[pos++] = 0xFF;
    buff[pos++] = 0x01;
    buff[pos++] = (uint8_t)bpm;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SET MASTER CLOCK BPM RESPONSE\n");
    }
    return retVal;
}

static int send_set_master_clock_run(uint32_t running) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;

    usb_cmd_sys(buff, &pos, (uint8_t)atomic_load(&gPerfVersion), SUB_COMMAND_SET_MASTER_CLOCK);
    buff[pos++] = 0xFF;
    buff[pos++] = 0x00;
    buff[pos++] = running ? 0x01 : 0x00;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SET MASTER CLOCK RUN RESPONSE\n");
    }
    return retVal;
}

// Write all synth (global) settings back to the G2.
static int send_synth_settings(void) {
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    int      pos                     = COMMAND_OFFSET;
    int      retVal                  = EXIT_FAILURE;
    uint32_t bitPos                  = 0;
    uint8_t  payload[64]             = {0};
    uint32_t i                       = 0;

    write_clavia_string(payload, &bitPos, gSynthSettings.name);

    write_bit_stream(payload, &bitPos, 1, gSynthSettings.perfMode);
    write_bit_stream(payload, &bitPos, 5, 0);
    write_bit_stream(payload, &bitPos, 2, gSynthSettings.patchSortMode);
    write_bit_stream(payload, &bitPos, 6, 0);
    write_bit_stream(payload, &bitPos, 2, gSynthSettings.perfSortMode);

    write_bit_stream(payload, &bitPos, 8, gSynthSettings.perfBank);
    write_bit_stream(payload, &bitPos, 8, gSynthSettings.perfLocation);
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.memoryProtect);
    write_bit_stream(payload, &bitPos, 7, 0);        // unknown

    for (i = 0; i < 4; i++) {
        write_bit_stream(payload, &bitPos, 8, gSynthSettings.midiChanSlot[i]);
    }

    write_bit_stream(payload, &bitPos, 8, gSynthSettings.globalChan);
    write_bit_stream(payload, &bitPos, 8, gSynthSettings.sysexId);
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.localOn);
    write_bit_stream(payload, &bitPos, 7, 0);        // unknown
    write_bit_stream(payload, &bitPos, 6, 0);        // unknown
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.progChangeRcv);
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.progChangeSnd);
    write_bit_stream(payload, &bitPos, 6, 0);        // unknown
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.controllersRcv);
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.controllersSnd);

    write_bit_stream(payload, &bitPos, 1, 0);
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.sendClock);
    write_bit_stream(payload, &bitPos, 1, !gSynthSettings.receiveClock);
    write_bit_stream(payload, &bitPos, 5, 0);
    write_bit_stream(payload, &bitPos, 8, (uint8_t)gSynthSettings.tuneCent);
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.globalShiftActive);
    write_bit_stream(payload, &bitPos, 7, 0);
    write_bit_stream(payload, &bitPos, 8, (uint8_t)gSynthSettings.globalOctaveShift);
    write_bit_stream(payload, &bitPos, 8, (uint8_t)gSynthSettings.tuneSemi);

    write_bit_stream(payload, &bitPos, 8, 0);        // filler - possibly should be vibrato rate, but doesn't seem to work
    write_bit_stream(payload, &bitPos, 1, gSynthSettings.pedalPolarity);
    write_bit_stream(payload, &bitPos, 1, 1);
    write_bit_stream(payload, &bitPos, 6, 0);
    write_bit_stream(payload, &bitPos, 8, gSynthSettings.pedalGain);

    for (i = 0; i < 16; i++) {
        write_bit_stream(payload, &bitPos, 8, 0);
    }

    uint32_t payloadBytes = BIT_TO_BYTE_ROUND_UP(bitPos);

    usb_cmd_sys(buff, &pos, 0x41, SUB_COMMAND_SET_SYNTH_SETTINGS);  // S_SYNTH_SETTINGS = 0x03

    for (uint32_t i = 0; i < payloadBytes && pos < SEND_MESSAGE_SIZE; i++) {
        buff[pos++] = payload[i];
    }

    retVal = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
        LOG_DEBUG("SET SYNTH SETTINGS RESPONSE\n");
    }
    return retVal;
}

static int send_set_patch_descr(uint32_t slot) { // Note - currently using values straight from patchDescr in sub-function
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    int      pos                     = COMMAND_OFFSET;
    uint32_t bitPos                  = 0;
    int      retVal                  = EXIT_FAILURE;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
    buff[pos++] = atomic_load(&gPatchVersion[slot]);
    bitPos      = BYTE_TO_BIT(pos);
    write_patch_descr(slot, buff, &bitPos);
    pos         = BIT_TO_BYTE(bitPos);

    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
    }
    return retVal;
}

// ---------------------------------------------------------------------------
// Init sequences — linear, no state machine
// ---------------------------------------------------------------------------

// Core resync: stop, fetch all perf+patch data, restart.
// Called both on explicit request and on G2-initiated perf mode changes.
static int reload_all_patch_data(void) {
    int      retVal = EXIT_SUCCESS;
    uint32_t slot   = 0;

    retVal |= send_stop();
    retVal |= send_get_midi_cc();
    retVal |= send_select_slot(0);
    retVal |= send_get_performance_settings();

    for (slot = 0; slot < MAX_SLOTS; slot++) {
        retVal |= send_get_patch_data(slot);
    }

    retVal |= send_get_assigned_voices();
    retVal |= send_get_global_knobs();
    retVal |= send_get_master_clock();
    retVal |= send_start();

    return retVal;
}

// First connection: G2 is authoritative — pull all patch data from hardware.
static int send_init_sequence_pull(void) {
    LOG_DEBUG("Init sequence: pulling from G2\n");
    atomic_store(&gCommsState, eCommsInitialising);

    // Clear any stale data before pulling fresh state
    database_clear_cables();
    database_clear_modules();

    send_init();
    send_stop();
    send_get_patch_version(4); // Performance slot
    send_get_synth_settings();
    send_get_midi_cc();
    send_select_slot(0);
    send_get_global_page();
    send_get_performance_settings();

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        send_get_patch_version(slot);
    }

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if (send_get_patch_data(slot) != EXIT_SUCCESS) {
            atomic_store(&gCommsState, eCommsReconnecting);
            return EXIT_FAILURE;
        }
    }

    send_get_assigned_voices();
    send_get_global_knobs();
    send_get_master_clock();

    send_start();

    LOG_DEBUG("Pull init sequence complete\n");
    atomic_store(&gotPatchChangeIndication, false);
    call_full_patch_change_notify();
    call_wake_glfw();

    return EXIT_SUCCESS;
}

// Reconnection: editor is authoritative — push all slots to the hardware.
static int send_init_sequence_push(void) {
    LOG_DEBUG("Init sequence: pushing editor data to G2\n");
    atomic_store(&gCommsState, eCommsInitialising);

    send_init();
    send_stop();
    send_synth_settings();

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if (push_slot_to_device(slot) != EXIT_SUCCESS) {
            atomic_store(&gCommsState, eCommsReconnecting);
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
    atomic_store(&gotPatchChangeIndication, false);
    call_full_patch_change_notify();
    call_wake_glfw();

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// send_write_data — runtime command dispatch
// ---------------------------------------------------------------------------

static int send_write_data(tMessageContent * messageContent) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     retVal                  = EXIT_FAILURE;
    uint8_t patchVersion[MAX_SLOTS] = {0};
    int     i                       = 0;

    for (i = 0; i < MAX_SLOTS; i++) {
        patchVersion[i] = atomic_load(&gPatchVersion[i]);
    }

    // TODO - these should move to functions where we can do: SEND_RECV(function());
    switch (messageContent->cmd) {
        case eMsgCmdSetValue:
            retVal      = send_set_param_value(messageContent->slot, messageContent->paramData.moduleKey, messageContent->paramData.param, messageContent->paramData.value, messageContent->paramData.variation);
            break;

        case eMsgCmdSetMode:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_MODE;
            buff[pos++] = messageContent->modeData.moduleKey.location;
            buff[pos++] = messageContent->modeData.moduleKey.index;
            buff[pos++] = messageContent->modeData.mode;
            buff[pos++] = messageContent->modeData.value;
            LOG_DEBUG("SET MODE %u %u\n", messageContent->modeData.mode, messageContent->modeData.value);
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("SET MODE RESPONSE\n");
            }
            break;

        case eMsgCmdWriteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_WRITE_CABLE;
            buff[pos++] = 0x10 | (messageContent->cableData.location << 3) | messageContent->cableData.colour;
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex;
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("WRITE CABLE RESPONSE\n");
            }
            break;

        case eMsgCmdWriteModule:
        {
            int avail   = 0;
            int written = 0;

            LOG_DEBUG("Writing module\n");
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_ADD_MODULE;
            buff[pos++] = messageContent->moduleData.type;
            buff[pos++] = messageContent->moduleData.moduleKey.location;
            buff[pos++] = messageContent->moduleData.moduleKey.index;
            buff[pos++] = messageContent->moduleData.column;
            buff[pos++] = messageContent->moduleData.row;
            buff[pos++] = messageContent->moduleData.colour;
            buff[pos++] = messageContent->moduleData.upRate;
            buff[pos++] = messageContent->moduleData.isLed;

            for (int i = 0; i < messageContent->moduleData.modeCount; i++) {
                buff[pos++] = messageContent->moduleData.mode[i];
            }

            avail       = SEND_MESSAGE_SIZE - pos;
            written     = snprintf((char *)&buff[pos], avail, "%s", messageContent->moduleData.name);

            pos        += ((written >= 0) && (written < avail)) ? written + 1 : avail;

            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("WRITE MODULE RESPONSE\n");
            }
            break;
        }
        case eMsgCmdMoveModule:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_MOVE_MODULE;
            buff[pos++] = messageContent->moduleData.moduleKey.location;
            buff[pos++] = messageContent->moduleData.moduleKey.index;
            buff[pos++] = messageContent->moduleData.column;
            buff[pos++] = messageContent->moduleData.row;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("MOVE MODULE RESPONSE\n");
            }
            break;

        case eMsgCmdDeleteModule:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_DELETE_MODULE;
            buff[pos++] = messageContent->moduleData.moduleKey.location;
            buff[pos++] = messageContent->moduleData.moduleKey.index;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("DELETE MODULE RESPONSE\n");
            }
            break;

        case eMsgCmdSetModuleUpRate:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_MODULE_UPRATE;
            buff[pos++] = messageContent->moduleData.moduleKey.location;
            buff[pos++] = messageContent->moduleData.moduleKey.index;
            buff[pos++] = messageContent->moduleData.upRate;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("SET MODULE UPRATE RESPONSE\n");
            }
            break;

        case eMsgCmdDeleteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_DELETE_CABLE;
            buff[pos++] = 0x2 | messageContent->cableData.location;
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex;
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("DELETE CABLE RESPONSE\n");
            }
            break;

        case eMsgCmdSetParamMorph:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_MORPH_RANGE;
            buff[pos++] = messageContent->paramMorphData.moduleKey.location;
            buff[pos++] = messageContent->paramMorphData.moduleKey.index;
            buff[pos++] = messageContent->paramMorphData.param;
            buff[pos++] = messageContent->paramMorphData.paramMorph;
            buff[pos++] = messageContent->paramMorphData.value;
            buff[pos++] = messageContent->paramMorphData.negative;
            buff[pos++] = messageContent->paramMorphData.variation;
            retVal      = send_message(buff, pos);
            break;

        case eMsgCmdSelectVariation:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SELECT_VARIATION;
            buff[pos++] = messageContent->variationData.variation;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("SELECT VARIATION RESPONSE\n");
            }
            break;

        case eMsgCmdSelectSlot:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SYS;
            buff[pos++] = atomic_load(&gPerfVersion);
            buff[pos++] = SUB_COMMAND_SELECT_SLOT;
            buff[pos++] = messageContent->slotData.slot;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, SUB_RESPONSE_OK);
                LOG_DEBUG("SELECT SLOT RESPONSE\n");
            }
            break;

        case eMsgCmdSetModuleLabel:
            retVal      = send_set_module_label(messageContent->slot, messageContent->moduleLabelData.moduleKey, messageContent->moduleLabelData.name);
            break;

        case eMsgCmdSetPatchName:
            retVal      = send_set_patch_name(messageContent->slot, messageContent->patchName.name);
            break;

        case eMsgCmdSetModuleColour:
            retVal      = send_set_module_colour(messageContent->slot, messageContent->moduleColourData.moduleKey.location, messageContent->moduleColourData.moduleKey.index, messageContent->moduleColourData.colour);
            break;

        case eMsgCmdWritePatch:
        {
            send_stop();
            retVal = push_slot_to_device(messageContent->slot);
            send_start();

            atomic_store(&gotPatchChangeIndication, false); // TODO - consider if this is the right thing to do here
            call_full_patch_change_notify();                // TODO - not sure we need to do this here
            call_wake_glfw();
            break;
        }

        case eMsgCmdWritePatchDescr:
        {
            retVal = send_set_patch_descr(messageContent->slot);
            break;
        }

        case eMsgCmdAssignKnob:
        {
            uint32_t kSlot  = messageContent->slot;
            uint32_t kLoc   = messageContent->knobAssignData.moduleKey.location;
            uint32_t kMod   = messageContent->knobAssignData.moduleKey.index;
            uint32_t kParam = messageContent->knobAssignData.paramIndex;
            uint32_t kKnob  = messageContent->knobAssignData.knobIndex;

            retVal = send_assign_knob(kSlot, kLoc, kMod, kParam, kKnob);
            break;
        }

        case eMsgCmdDeassignKnob:
        {
            retVal = send_deassign_knob(messageContent->slot, messageContent->knobDeassignData.knobIndex);
            break;
        }

        case eMsgCmdAssignGlobalKnob:
        {
            uint32_t gkSlot  = messageContent->globalKnobAssignData.slotIndex;
            uint32_t gkLoc   = messageContent->globalKnobAssignData.location;
            uint32_t gkMod   = messageContent->globalKnobAssignData.moduleIndex;
            uint32_t gkParam = messageContent->globalKnobAssignData.paramIndex;
            uint32_t gkKnob  = messageContent->globalKnobAssignData.knobIndex;

            retVal = send_assign_global_knob(gkSlot, gkLoc, gkMod, gkParam, gkKnob);
            break;
        }

        case eMsgCmdDeassignGlobalKnob:
        {
            retVal = send_deassign_global_knob(messageContent->globalKnobDeassignData.knobIndex);
            break;
        }

        case eMsgCmdAssignMidiCC:
        {
            uint32_t mLoc   = messageContent->midiCCAssignData.moduleKey.location;
            uint32_t mMod   = messageContent->midiCCAssignData.moduleKey.index;
            uint32_t mParam = messageContent->midiCCAssignData.paramIndex;
            uint32_t mCC    = messageContent->midiCCAssignData.midiCC;

            retVal = send_assign_midi_cc(messageContent->slot, mLoc, mMod, mParam, mCC);
            break;
        }

        case eMsgCmdDeassignMidiCC:
        {
            retVal = send_deassign_midi_cc(messageContent->slot, messageContent->midiCCDeassignData.midiCC);
            break;
        }

        case eMsgCmdCopyVariation:
        {
            retVal = send_copy_variation(messageContent->slot,
                                         messageContent->copyVariationData.fromVariation,
                                         messageContent->copyVariationData.toVariation);
            break;
        }

        case eMsgCmdSetMasterClockBPM:
        {
            retVal = send_set_master_clock_bpm(messageContent->masterClockBPMData.bpm);
            break;
        }

        case eMsgCmdSetMasterClockRun:
        {
            retVal = send_set_master_clock_run(messageContent->masterClockRunData.running);
            break;
        }

        case eMsgCmdSetParamLabel:
            retVal = send_set_param_label(messageContent->slot, messageContent->paramLabelData.moduleKey, messageContent->paramLabelData.paramIndex, messageContent->paramLabelData.name);
            break;

        case eMsgCmdWriteSynthSettings:
            retVal = send_synth_settings();
            break;

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

    //TODO - Don't like early returns. Use retVal

    if (atomic_load(&gotBadConnectionIndication)) {
        LOG_DEBUG("Bad connection — closing device\n");
        atomic_store(&gotBadConnectionIndication, false);
        atomic_store(&gCommsState, eCommsReconnecting);

        pthread_mutex_lock(&usbStaticMutex);
        close_device();
        pthread_mutex_unlock(&usbStaticMutex);

        // Do NOT clear the database — editor data is preserved so it can be
        // pushed back to the hardware on reconnection.

        call_full_patch_change_notify();
        call_wake_glfw();
        return;
    }

    if (atomic_load(&gCommsState) != eCommsOnLine) {
        bool opened = false;

        pthread_mutex_lock(&usbStaticMutex);
        opened = open_and_claim_device();
        pthread_mutex_unlock(&usbStaticMutex);

        if (opened) {
            int result = EXIT_FAILURE;

            if (atomic_load(&gCommsState) == eCommsNeverConnected) {
                // First ever connection — G2 is authoritative, pull everything
                result = send_init_sequence_pull();
            } else {
                // Reconnection after disconnect — editor is authoritative, push all slots
                result = send_init_sequence_push();
            }

            if (result == EXIT_SUCCESS) {
                atomic_store(&gCommsState, eCommsOnLine);
            } else {
                LOG_DEBUG("Init sequence failed — will retry\n");
                pthread_mutex_lock(&usbStaticMutex);
                close_device();
                pthread_mutex_unlock(&usbStaticMutex);
                atomic_store(&gCommsState, eCommsReconnecting);
            }
        } else {
            usleep(500000);  // 500ms between open attempts — don't hammer the bus
        }
        return;
    }

    // Performance/patch settings changed (e.g. perf mode switch on the G2 panel).
    // Flag coalesces rapid switches — only one full resync runs per batch.
    if (atomic_load(&gotPerfSettingsChangeIndication)) {
        atomic_store(&gotPerfSettingsChangeIndication, false);

        LOG_DEBUG("Perf settings change — reloading all slots\n");

        reload_all_patch_data();

        atomic_store(&gSlot, 0);
        gPatchDescr[0].activeVariation = 0;
        set_exclusive_button_highlight(slotAButtonId, slotDButtonId,
                                       (tButtonId)(slotAButtonId));
        set_exclusive_button_highlight(variation1ButtonId, variationInitButtonId,
                                       (tButtonId)((uint32_t)variation1ButtonId));

        call_full_patch_change_notify();
        call_wake_glfw();
        return;
    }

    // Patch changed on the hardware (e.g. user loaded a patch from the G2 panel)
    if (atomic_load(&gotPatchChangeIndication)) {
        atomic_store(&gotPatchChangeIndication, false);
        uint32_t slot = atomic_load(&gChangedSlot);

        LOG_DEBUG("Patch change on slot %u — reloading\n", slot);

        send_stop();
        send_get_patch_data(slot);
        send_start();

        call_full_patch_change_notify();
        call_wake_glfw();
        return;
    }

    // Command from UI thread
    if (msg_receive(&gCommandQueue, eRcvPoll, &messageContent) == EXIT_SUCCESS) {
        send_write_data(&messageContent);

        return;
    }

    // Keepalive: if no outbound traffic for a while, send a lightweight request.
    // If the G2 doesn't respond, treat it as a bad connection and force a reconnect.
    if (time(NULL) - gLastActivityTime >= USB_KEEPALIVE_INTERVAL_S) {
        LOG_DEBUG("USB keepalive\n");

        if (send_get_global_page() != EXIT_SUCCESS) {
            LOG_DEBUG("Keepalive failed — forcing reconnect\n");
            atomic_store(&gotBadConnectionIndication, true);
        }
        return;
    }
    // Nothing to do — poll for unsolicited messages (LED, volume, param change)
    int_rec(ePollYes, SUB_RESPONSE_NULL);
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

    if (libusb_init(&libUsbCtx) != LIBUSB_SUCCESS) {
        LOG_ERROR("libusb_init failed\n");
        return NULL;
    }
    // Only if needed: libusb_set_option(libUsbCtx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    while (atomic_load(&gQuitAll) == false) {
        state_handler();
    }
    pthread_mutex_lock(&usbStaticMutex);
    close_device();
    pthread_mutex_unlock(&usbStaticMutex);

    libusb_exit(libUsbCtx);
    return NULL;
}

void usb_signal_reconnect(void) {
    LOG_DEBUG("System wake detected — forcing USB reconnect\n");
    atomic_store(&gotBadConnectionIndication, true);
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
