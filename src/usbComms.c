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

#include "defs.h"
#include "types.h"
#include <libusb-1.0/libusb.h>
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

#define VENDOR_ID              (0xffc)
#define PRODUCT_ID             (2)

// USB transfer timeouts (milliseconds)
#define USB_SEND_TIMEOUT_MS    (100)
#define USB_RECV_TIMEOUT_MS    (100)   // Extra headroom for G2 patch computation
#define USB_INIT_TIMEOUT_MS    (1000)  // Extra headroom during init sequence

// Atomic flags for cross-thread signalling
static _Atomic bool           gotBadConnectionIndication = false;
static _Atomic bool           gotPatchChangeIndication   = false;

// Protected by usbStaticMutex
static pthread_t              usbThread                  = NULL;
static libusb_context *       libUsbCtx                  = NULL;
static libusb_device_handle * devHandle                  = NULL;

// Callback pointers protected by callbackMutex
static void                   (*wake_glfw_func_ptr)(void) = NULL;
static void                   (*full_patch_change_notify_func_ptr)(void) = NULL;

// Mutexes
static pthread_mutex_t        usbStaticMutex             = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t        callbackMutex              = PTHREAD_MUTEX_INITIALIZER;

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
        libusb_close(devHandle);
        devHandle = NULL;
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
    uint8_t  ch     = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }
    LOG_DEBUG("Clavia string '");

    for (int i = 0; i < 11; i++) {
        ch = read_bit_stream(buff, &bitPos, 8);

        if ((ch >= 0x20) && (ch <= 0x7f)) {
            LOG_DEBUG_DIRECT("%c", ch);
        }
    }

    LOG_DEBUG_DIRECT("'\n");

    LOG_DEBUG("Perf Mode 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Perf Bank 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Perf Location 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Memory Protect (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 7);
    LOG_DEBUG("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("MIDI chan Slot B 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("MIDI chan Slot C 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("MIDI chan Slot D 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global chan 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Sysex ID 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Local on (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 7);
    read_bit_stream(buff, &bitPos, 6);
    LOG_DEBUG("Prog Change Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    LOG_DEBUG("Prog Change Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 6);
    LOG_DEBUG("Controllers Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    LOG_DEBUG("Controllers Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    LOG_DEBUG("Send Clock (bit 1) ignore ext clock (bit 2) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Tune cent 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global Shift Active (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global Octave Shift 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Tune semi 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Filler 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Pedal Polarity (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Control Pedal Gain 0x%x\n", read_bit_stream(buff, &bitPos, 8));

    return EXIT_SUCCESS;
}

static int parse_midi_cc(uint8_t * buff, int length) {
    uint32_t bitPos      = 0;
    uint8_t  subResponse = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }

    while (true) {
        LOG_DEBUG("MIDI Chan 0x%x\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("CC Numb/value 0x%x\n", read_bit_stream(buff, &bitPos, 8));
        subResponse = read_bit_stream(buff, &bitPos, 8);

        if ((subResponse != SUB_RESPONSE_MIDI_CC) || (BIT_TO_BYTE_ROUND_UP(bitPos) > length)) {
            LOG_DEBUG("MIDI CC stream end on 0x%02x bitPos(byte)=%u length=%d\n",
                      subResponse, BIT_TO_BYTE_ROUND_UP(bitPos), length);
            break;
        }
    }
    return EXIT_SUCCESS;
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

        if (type != SUB_SEL_PARAM_PAGE) {
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

            case SUB_SEL_PARAM_PAGE:
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
                parse_morph_params(slot, buff, &subOffset, subOffset + BYTE_TO_BIT(count));
                break;

            case SUB_RESPONSE_KNOBS:
                parse_knobs(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CONTROLLERS:
                parse_controllers(slot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CURRENT_NOTE_2:
            {
                LOG_DEBUG("Current note 2\n");

                if (slot < MAX_SLOTS) {
                    uint32_t safeCount = ((uint32_t)count < sizeof(gNote2[slot]))
                                       ? (uint32_t)count : sizeof(gNote2[slot]);

                    for (uint32_t i = 0; i < safeCount; i++) {
                        gNote2[slot][i] = read_bit_stream(buff, &subOffset, 8);
                    }

                    gNote2Size[slot] = safeCount;
                }
                break;
            }

            case SUB_RESPONSE_PATCH_NOTES:
            {
                LOG_DEBUG("Patch notes\n");

                if (slot < MAX_SLOTS) {
                    uint32_t safeCount = ((uint32_t)count < sizeof(gPatchNotes[slot]))
                                       ? (uint32_t)count : sizeof(gPatchNotes[slot]);

                    for (uint32_t i = 0; i < safeCount; i++) {
                        gPatchNotes[slot][i] = read_bit_stream(buff, &subOffset, 8);
                    }

                    gPatchNotesSize[slot] = safeCount;
                }
                break;
            }

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

    if (slot >= MAX_SLOTS) {
        return EXIT_FAILURE;
    }
    atomic_store(&gPatchVersion[slot], version);

    LOG_DEBUG("Patch version slot %u = 0x%02x\n", slot, version);
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
                                  int length, bool * unsolicited) {
    tModule  module   = {0};
    uint32_t slot     = commandResponse & 0x03;
    uint32_t location = atomic_load(&gLocation);

    *unsolicited = false;

    switch (subCommand) {
        case SUB_RESPONSE_VOLUME_INDICATOR:
        {
            for (int32_t location = 1; location >= 0; location--) {
                for (int k = 0; k <= 255; k++) {
                    module.key.slot     = slot;
                    module.key.location = location;
                    module.key.index    = k;

                    if (read_module(module.key, &module) == true) {
                        if (gModuleProperties[module.type].volumeType == volumeTypeNone) {
                            continue;
                        }
                        read_bit_stream(buff, bitPos, 8);  // unknown — per entry
                        module.volume.value1 = read_bit_stream(buff, bitPos, 8);

                        if (gModuleProperties[module.type].volumeType == volumeTypeStereo) {
                            read_bit_stream(buff, bitPos, 8);  // second unknown
                            module.volume.value2 = read_bit_stream(buff, bitPos, 8);
                        } else {
                            module.volume.value2 = 0;
                        }
                        write_module(module.key, &module);
                    }
                }
            }

            *unsolicited = true;
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_LED_DATA:
        {
            for (int i = 4; i < (length - 2); i++) {
                buff[i] = reverse_bits_in_byte(buff[i]);
            }

            read_bit_stream(buff, bitPos, 8);  // unknown byte

            // Iterate both locations in VA-first order to match wire order.
            // Consume bits for all modules regardless of displayed location.
            for (int32_t location = 1; location >= 0; location--) {
                for (int k = 0; k <= 255; k++) {
                    module.key.slot     = slot;
                    module.key.location = location;
                    module.key.index    = k;

                    if (read_module(module.key, &module) == true) {
                        if (gModuleProperties[module.type].ledType == ledTypeYes) {
                            module.led.value = read_bit_stream(buff, bitPos, 1);
                            read_bit_stream(buff, bitPos, 1);  // spare bit

                            if (module.key.location == location) {
                                write_module(module.key, &module);
                            }
                        }
                    }
                }
            }

            *unsolicited = true;
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_ERROR:
            LOG_DEBUG("Got Error!!!\n");
            return EXIT_FAILURE;

        case SUB_RESPONSE_RESOURCES_USED:
            LOG_DEBUG("Got resources in use slot %u\n", commandResponse);
            *unsolicited = true;
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PARAM_CHANGE:
            parse_param_change(slot, &buff[BIT_TO_BYTE(*bitPos)],
                               length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);

            *unsolicited = true;
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
            return parse_midi_cc(&buff[BIT_TO_BYTE(*bitPos)],
                                 length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);

        case SUB_RESPONSE_GLOBAL_PAGE:
        {
            LOG_DEBUG("Got global page\n");
            uint32_t tmpBitPos = *bitPos;

            for (int i = 0; i < length; i++) {
                LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, bitPos, 8));
            }

            LOG_DEBUG_DIRECT("\n");
            *bitPos = tmpBitPos;
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
            *unsolicited = true;
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_ASSIGNED_VOICES:
            LOG_DEBUG("Got assigned voices response\n");
            return EXIT_SUCCESS;

        case SUB_COMMAND_SET_ASSIGNED_VOICES:
            LOG_DEBUG("Got assigned voices command — unexpected\n");
            return EXIT_SUCCESS;

        case SUB_RESPONSE_PERFORMANCE_NAME:
            LOG_DEBUG("Got performance name\n");
            return EXIT_SUCCESS;

        case SUB_RESPONSE_MASTER_CLOCK:
            LOG_DEBUG("Got master clock\n");
            return EXIT_SUCCESS;

        case SUB_COMMAND_SELECT_SLOT:
        {
            uint32_t newSlot = read_bit_stream(buff, bitPos, 8);
            LOG_DEBUG("Got slot select %u\n", newSlot);

            atomic_store(&gSlot, newSlot);
            set_exclusive_button_highlight(slotAButtonId, slotDButtonId,
                                           (tButtonId)(slotAButtonId + newSlot));
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_DESCRIPTION:
            LOG_DEBUG("Got patch description\n");
            parse_patch(slot, &buff[BIT_TO_BYTE(*bitPos) - 1],
                        (length - BIT_TO_BYTE(*bitPos) - CRC_BYTES) + 1);
            return EXIT_SUCCESS;

        case SUB_RESPONSE_GET_PATCH_NAME:
        {
            char patchName[PATCH_NAME_SIZE + 1] = {0};

            LOG_DEBUG("Got patch name (length %d)\n", length);

            memset(patchName, 0, sizeof(patchName));

            for (int i = 0; i < (length - 6) && i < PATCH_NAME_SIZE; i++) {
                uint8_t ch = read_bit_stream(buff, bitPos, 8);
                patchName[i] = ch;
                LOG_DEBUG_DIRECT("%c", patchName[i]);
            }

            LOG_DEBUG_DIRECT("\n");

            patch_name_set(slot, patchName);

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_OK:
            //printf("GOT OK %f\n", get_time_ms());
            return EXIT_SUCCESS;

        case SUB_RESPONSE_SELECT_PARAM:
            LOG_DEBUG("Got select param\n");
            return EXIT_SUCCESS;

        default:
            LOG_DEBUG("Got unknown sub-command 0x%02x\n", subCommand);
            return EXIT_FAILURE;
    }
}

static int parse_incoming(uint8_t * buff, int length, int * response, bool * unsolicited) {
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
            ret = parse_command_response(buff, &bitPos, commandResponse, subCommand, length, unsolicited);

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

static int rcv_extended(int dataLength, int * response, bool * unsolicited) {
    uint8_t                buff[EXTENDED_MESSAGE_SIZE] = {0};
    int                    readLength                  = 0;
    int                    retVal                      = EXIT_FAILURE;
    libusb_device_handle * devHandle_local             = NULL;

    pthread_mutex_lock(&usbStaticMutex);
    devHandle_local = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (devHandle_local == NULL) {
        LOG_ERROR("rcv_extended: device handle is NULL\n");
        return EXIT_FAILURE;
    }

    for (int tries = 0; tries < 5; tries++) {
        memset(buff, 0, sizeof(buff));
        readLength = 0;
        retVal     = libusb_bulk_transfer(devHandle_local, 0x82, buff,
                                          sizeof(buff), &readLength,
                                          USB_RECV_TIMEOUT_MS);

        if (retVal == LIBUSB_SUCCESS) {
            if (readLength > 0) {
                uint8_t responseType = buff[0];

                if (  (responseType == RESPONSE_TYPE_INIT)
                   || (responseType == RESPONSE_TYPE_COMMAND)) {
                    break;
                }
            }
        } else if (is_disconnect_error(retVal)) {
            LOG_DEBUG("rcv_extended: disconnect error %s\n", libusb_error_name(retVal));
            atomic_store(&gotBadConnectionIndication, true);
            return EXIT_FAILURE;
        } else {
            LOG_DEBUG("rcv_extended: transfer error %s\n", libusb_error_name(retVal));
        }
        usleep(1000);
    }

    if (readLength != dataLength) {
        LOG_DEBUG("rcv_extended: length mismatch read=%d expected=%d\n",
                  readLength, dataLength);
        return EXIT_FAILURE;
    }
    uint32_t bitPos = SIGNED_BYTE_TO_BIT(dataLength - 2);

    if (calc_crc16(buff, dataLength - 2) != read_bit_stream(buff, &bitPos, 16)) {
        LOG_DEBUG("rcv_extended: bad CRC\n");
        return EXIT_FAILURE;
    }
    return parse_incoming(buff, dataLength, response, unsolicited);
}

static int int_rec(tPoll poll, int * response) {
    uint8_t                buff[INTERRUPT_MESSAGE_SIZE] = {0};
    int                    readLength                   = 0;
    int                    retVal                       = EXIT_FAILURE;
    libusb_device_handle * devHandle_local              = NULL;
    int                    timeout                      = USB_RECV_TIMEOUT_MS;
    bool                   unsolicited                  = false;
    bool                   doLoop                       = true;

    while (doLoop == true) {
        unsolicited     = false;

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

        for (int tries = 0; tries < 5; tries++) {
            memset(buff, 0, sizeof(buff));
            readLength = 0;
            retVal     = libusb_bulk_transfer(devHandle_local, 0x81, buff,
                                              sizeof(buff), &readLength,
                                              timeout);

            if (retVal == LIBUSB_SUCCESS) {
                if (readLength > 0) {
                    break;
                }
            } else if (retVal == LIBUSB_ERROR_TIMEOUT) {
                // Normal in poll — G2 has nothing to send, not an error
                return EXIT_SUCCESS;       // ← was falling through to retry loop
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
                retVal     = rcv_extended(dataLength, response, &unsolicited);
            }
        } else if (type == RESPONSE_TYPE_EMBEDDED) {
            retVal = parse_incoming(buff + 1, dataLength, response, &unsolicited);
        }

        if ((unsolicited == false) || (poll == ePollYes)) {
            doLoop = false;
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

    if (msgLength <= 0) {
        return EXIT_FAILURE;
    }
    uint16_t               crc          = calc_crc16(&buff[COMMAND_OFFSET], msgLength);
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
    int                    result       = libusb_bulk_transfer(handle, 3, buff, msgLength,
                                                               &actualLength, USB_SEND_TIMEOUT_MS);

    if (result == 0) {
        return EXIT_SUCCESS;
    }

    if (is_disconnect_error(result)) {
        LOG_DEBUG("send_message: disconnect error %s\n", libusb_error_name(result));
        atomic_store(&gotBadConnectionIndication, true);
    } else {
        LOG_ERROR("send_message: transfer error %s\n", libusb_error_name(result));
    }
    return EXIT_FAILURE;
}

static int send_init(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x80;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("INIT RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_OK) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_stop(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
    buff[pos++] = 0x41;
    buff[pos++] = SUB_COMMAND_START_STOP;
    buff[pos++] = 0x01;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("STOP RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_OK) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_start(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
    buff[pos++] = 0x41;
    buff[pos++] = SUB_COMMAND_START_STOP;
    buff[pos++] = 0x00;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("START RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_OK) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_select_slot(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
    buff[pos++] = 0x00;
    buff[pos++] = SUB_COMMAND_SELECT_SLOT;
    buff[pos++] = (uint8_t)slot;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("SELECT SLOT RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_OK) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_get_synth_settings(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
    buff[pos++] = 0x41;
    buff[pos++] = SUB_COMMAND_GET_SYNTH_SETTINGS;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("GET STNTH SETTINGS RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_SYNTH_SETTINGS) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_get_midi_cc(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
    buff[pos++] = 0x41;
    buff[pos++] = SUB_COMMAND_GET_MIDI_CC;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("GET MIDI CC RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_MIDI_CC) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_get_global_page(void) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
    buff[pos++] = 0x00;
    buff[pos++] = SUB_COMMAND_GET_GLOBAL_PAGE;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("GET UNKNOWN 2 RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_GLOBAL_PAGE) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_get_patch_version(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
    buff[pos++] = 0x41;
    buff[pos++] = SUB_COMMAND_GET_PATCH_VERSION;
    buff[pos++] = (uint8_t)slot;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("GET PATCH VERSION RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_PATCH_VERSION) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_get_patch(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
    buff[pos++] = atomic_load(&gPatchVersion[slot]);
    buff[pos++] = SUB_COMMAND_GET_PATCH_SLOT;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("GET PATCH RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_PATCH_DESCRIPTION) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_get_patch_name(uint32_t slot) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
    buff[pos++] = atomic_load(&gPatchVersion[slot]);
    buff[pos++] = SUB_COMMAND_GET_PATCH_NAME;
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("GET PATCH NAME RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_GET_PATCH_NAME) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_set_module_label(uint32_t slot, tModuleKey moduleKey, const char * name) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     i                       = 0;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
    buff[pos++] = atomic_load(&gPatchVersion[slot]);
    buff[pos++] = SUB_COMMAND_SET_MODULE_LABEL;
    buff[pos++] = moduleKey.location;
    buff[pos++] = moduleKey.index;

    // WriteClaviaString: chars up to 16, then null terminator if < 16
    while (i < MODULE_NAME_SIZE && name[i] != '\0') {
        buff[pos++] = (uint8_t)name[i++];
    }

    if (i < MODULE_NAME_SIZE) {
        buff[pos++] = 0x00;
    }
    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("SET MODULE LABEL RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_OK) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

static int send_set_param_value(uint32_t slot, tModuleKey moduleKey, uint32_t param, uint32_t value, uint32_t variation) {
    int     retVal                  = EXIT_FAILURE;
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | slot;
    buff[pos++] = atomic_load(&gPatchVersion[slot]);
    buff[pos++] = SUB_COMMAND_SET_PARAM;
    buff[pos++] = moduleKey.location;
    buff[pos++] = moduleKey.index;
    buff[pos++] = param;
    buff[pos++] = value;
    buff[pos++] = variation;
    retVal      = send_message(buff, pos);

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

// Fetch patch data for a single slot. Synth must be stopped before calling.
static int send_get_patch_data(uint32_t slot) {
    clear_slot_data(slot);

    send_get_patch_version(slot);
    send_get_patch(slot);
    send_get_patch_name(slot);

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------
// Patch upload helper — builds and sends one slot to the device.
// Synth must already be stopped before calling.
// Consumes the SUB_RESPONSE_PATCH_VERSION_CHANGE reply internally.
// ---------------------------------------------------------------------------

static int push_slot_to_device(uint32_t slot) {
    uint8_t  buff[SEND_MESSAGE_SIZE]        = {0};
    int      pos                            = COMMAND_OFFSET;
    uint32_t bitPos                         = 0;
    uint32_t i                              = 0;
    int      retVal                         = EXIT_FAILURE;
    char     patchName[PATCH_NAME_SIZE + 1] = {0};

    LOG_DEBUG("Pushing slot %u to device\n", slot);

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
    buff[pos++] = UPLOAD_PATCH_VERSION;
    buff[pos++] = SUB_COMMAND_SET_PATCH;
    buff[pos++] = 0x00;
    buff[pos++] = 0x00;
    buff[pos++] = 0x00;

    patch_name_get(slot, patchName, sizeof(patchName));

    while ((i < PATCH_NAME_SIZE) && (patchName[i] != '\0')) {
        buff[pos++] = patchName[i++];
    }
    buff[pos++] = 0x00;

    bitPos      = BYTE_TO_BIT(pos);

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
        retVal = int_rec(ePollNo, NULL);
    }
    return retVal;
}

static int send_set_patch_name(uint32_t slot, const char * name) {
    uint8_t buff[SEND_MESSAGE_SIZE] = {0};
    int     pos                     = COMMAND_OFFSET;
    int     i                       = 0;
    int     retVal                  = EXIT_FAILURE;
    int     response                = SUB_RESPONSE_ERROR;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
    buff[pos++] = atomic_load(&gPatchVersion[slot]);
    buff[pos++] = SUB_COMMAND_SET_PATCH_NAME;  // 0x27 — used for set AND response

    // Null-terminated string, max 16 chars
    while (i < PATCH_NAME_SIZE && name[i] != '\0') {
        buff[pos++] = (uint8_t)name[i++];
    }
    buff[pos++] = 0x00;

    retVal      = send_message(buff, pos);

    if (retVal == EXIT_SUCCESS) {
        retVal = int_rec(ePollNo, &response);
        LOG_DEBUG("SET PATCH NAME RESPONSE = 0x%02x\n", response);

        if (response != SUB_RESPONSE_OK) {
            retVal = EXIT_FAILURE;
        }
    }
    return retVal;
}

// ---------------------------------------------------------------------------
// Init sequences — linear, no state machine
// ---------------------------------------------------------------------------

// First connection: G2 is authoritative — pull all patch data from hardware.
static int send_init_sequence_pull(void) {
    LOG_DEBUG("Init sequence: pulling from G2\n");
    atomic_store(&gCommsState, eCommsInitialising);

    // Clear any stale data before pulling fresh state
    database_clear_cables();
    database_clear_modules();

    send_init();
    send_stop();
    send_get_synth_settings();
    send_get_midi_cc();
    send_get_global_page();
    send_select_slot(0);

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        send_get_patch_version(slot);
    }

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if (send_get_patch_data(slot) != EXIT_SUCCESS) {
            atomic_store(&gCommsState, eCommsReconnecting);
            return EXIT_FAILURE;
        }
    }

    send_start();

    LOG_DEBUG("Pull init sequence complete\n");
    atomic_store(&gotPatchChangeIndication, false);
    atomic_store(&gCommsState, eCommsOnLine);
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

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if (push_slot_to_device(slot) != EXIT_SUCCESS) {
            atomic_store(&gCommsState, eCommsReconnecting);
            return EXIT_FAILURE;
        }
    }

    send_start();

    LOG_DEBUG("Push init sequence complete\n");
    atomic_store(&gotPatchChangeIndication, false);
    atomic_store(&gCommsState, eCommsOnLine);
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
    int     response                = SUB_RESPONSE_ERROR;

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
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("SET MODE RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
            }
            break;

        case eMsgCmdWriteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = patchVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_WRITE_CABLE;
            buff[pos++] = 0x10 | (messageContent->cableData.location << 3) | 0x00;
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex;
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("WRITE CABLE RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
            }
            break;

        case eMsgCmdWriteModule:
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

            strcpy((char *)&buff[pos], messageContent->moduleData.name);
            pos        += strlen(messageContent->moduleData.name) + 1;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("WRITE MODULE RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
            }
            break;

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
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("MOVE MODULE RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
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
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("DELETE MODULE RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
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
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("SET MODULE UPRATE RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
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
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("DELETE CABLE RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
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
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("SELECT VARIATION RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
            }
            break;

        case eMsgCmdSelectSlot:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SYS;
            buff[pos++] = 0;
            buff[pos++] = SUB_COMMAND_SELECT_SLOT;
            buff[pos++] = messageContent->slotData.slot;
            retVal      = send_message(buff, pos);

            if (retVal == EXIT_SUCCESS) {
                retVal = int_rec(ePollNo, &response);
                LOG_DEBUG("SELECT SLOT RESPONSE = 0x%02x\n", response);

                if (response != SUB_RESPONSE_OK) {
                    retVal = EXIT_FAILURE;
                }
            }
            break;

        case eMsgCmdSetModuleLabel:
            retVal      = send_set_module_label(messageContent->slot, messageContent->moduleLabelData.moduleKey, messageContent->moduleLabelData.name);
            break;

        case eMsgCmdSetPatchName:
            retVal      = send_set_patch_name(messageContent->slot, messageContent->patchName.name);
            break;

        case eMsgCmdWritePatch:
        {
            send_stop();
            push_slot_to_device(messageContent->slot);
            send_start();

            call_full_patch_change_notify(); // TODO - not sure we need to do this here
            call_wake_glfw();
            retVal = EXIT_SUCCESS;
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
    int             response       = SUB_RESPONSE_ERROR;

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

            if (result != EXIT_SUCCESS) {
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
    // Nothing to do — poll for unsolicited messages (LED, volume, param change)
    int_rec(ePollYes, NULL);
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

void start_usb_thread(void) {
    if (pthread_create(&usbThread, NULL, usb_thread_loop, NULL) != EXIT_SUCCESS) {
        LOG_ERROR("Failed to create USB thread\n");
        exit(EXIT_FAILURE);
    }
}

#ifdef __cplusplus
}
#endif
