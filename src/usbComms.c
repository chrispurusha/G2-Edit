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

#define VENDOR_ID     (0xffc)
#define PRODUCT_ID    (2)

typedef enum {
    eStateNone,
    eStateFindDevice,
    eStateInit,     // Items from here increment in order for init sequence states
    eStateStop,
    eStateGetSynthSettings,
    eStateGetMidiCc, // A.k.a. Unknown 1!?
    eStateGetUnknown2,
    eStateSelectSlot,
    eStateGetPatchVersionSlotA,
    eStateGetPatchVersionSlotB,
    eStateGetPatchVersionSlotC,
    eStateGetPatchVersionSlotD,
    eStateGetPatchSlotA,
    eStateGetPatchSlotB,
    eStateGetPatchSlotC,
    eStateGetPatchSlotD,
    eStateGetPatchNameSlotA,
    eStateGetPatchNameSlotB,
    eStateGetPatchNameSlotC,
    eStateGetPatchNameSlotD,
    eStateStart,     // Last one to increment in order
    eStatePoll,
    eStateExit
} eState;

// Atomic flags for state synchronization
static _Atomic bool gotBadConnectionIndication = false;
static _Atomic bool gotPatchChangeIndication   = false;

// Protected by usbStaticMutex
static uint8_t                slotVersion[MAX_SLOTS] = {0};
static pthread_t              usbThread              = NULL;
static libusb_context *       libUsbCtx              = NULL;
static libusb_device_handle * devHandle              = NULL;
static char *                 usbSemName             = NULL;

// Callback pointers protected by callbackMutex
static void (*wake_glfw_func_ptr)(void) = NULL;
static void (*full_patch_change_notify_func_ptr)(void) = NULL;

// Mutexes for synchronization
static pthread_mutex_t usbStaticMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t callbackMutex  = PTHREAD_MUTEX_INITIALIZER;

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
        LOG_ERROR("Wake GLFW callback function not registered\n");
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
        LOG_ERROR("Full patch change callback function not registered\n");
        exit(1);
    }
    func_ptr();
}

static int parse_synth_settings(uint8_t * buff, int length) {
    int      retVal = EXIT_FAILURE;
    int      i      = 0;
    uint32_t bitPos = 0;
    uint8_t  ch     = 0;

    if (buff == NULL) {
        return retVal;
    }
    LOG_DEBUG("Clavia string '");

    for (i = 0; i < 11; i++) {
        ch = read_bit_stream(buff, &bitPos, 8);

        if ((ch <= 0x7f) && (ch >= 0x20)) {
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
    LOG_DEBUG("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));     // 16 = off/disabled
    LOG_DEBUG("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global chan 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Sysex ID 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Local on (bit 0)0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 7);
    read_bit_stream(buff, &bitPos, 6);
    LOG_DEBUG("Prog Change Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    LOG_DEBUG("Prog Change Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 6);
    LOG_DEBUG("Controllers Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    LOG_DEBUG("Controllers Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    LOG_DEBUG("Send Clock (bit 1), ignore ext clock (bit 2) , 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Tune cent 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global Shift Active (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global Octave Shift 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Tune semi 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Filler 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Pedal Polarity (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Control Pedal Gain 0x%x\n", read_bit_stream(buff, &bitPos, 8));

    retVal = EXIT_SUCCESS;

    return retVal;
}

static int parse_midi_cc(uint8_t * buff, int length) {
    int      retVal      = EXIT_FAILURE;
    uint32_t bitPos      = 0;
    uint8_t  subResponse = 0;
    bool     quitLoop    = false;

    if (buff == NULL) {
        return retVal;
    }

    while (quitLoop == false) {
        LOG_DEBUG("MIDI Chan 0x%x\n", read_bit_stream(buff, &bitPos, 8));
        LOG_DEBUG("CC Numb (and value?) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
        subResponse = read_bit_stream(buff, &bitPos, 8);

        if ((subResponse != SUB_RESPONSE_MIDI_CC) || (BIT_TO_BYTE_ROUND_UP(bitPos) > length)) {
            LOG_DEBUG("MIDI CC stream finishing on 0x%02x bitPos (byte) = %u, length = %d\n", subResponse, BIT_TO_BYTE_ROUND_UP(bitPos), length);
            quitLoop = true;
        }
    }
    retVal = EXIT_SUCCESS;

    return retVal;
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

        // Validate count before using it to advance bitOffset or index arrays.
        // A negative or oversized count from malformed data would cause reads
        // past the end of the buffer.
        if (type != SUB_SEL_PARAM_PAGE) {
            count = (int16_t)read_bit_stream(buff, &bitOffset, 16);
            
            if (count < 0) {
                LOG_ERROR("parse_patch: negative count %d for type 0x%02x, aborting\n", count, type);
                return EXIT_FAILURE;
            }
            if (BIT_TO_BYTE(bitOffset) + count > length) {
                LOG_ERROR("parse_patch: count %d for type 0x%02x would exceed buffer length %d, aborting\n", count, type, length);
                return EXIT_FAILURE;
            }
        }
        
        subOffset = bitOffset;

        LOG_DEBUG("Type = 0x%x, Count = %d\n", type, count);

        switch (type) {
            case SUB_RESPONSE_MODULE_LIST:
            {
                parse_module_list(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_CABLE_LIST:
            {
                parse_cable_list(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_PARAM_LIST:
            {
                parse_param_list(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_PARAM_NAMES:
            {
                parse_param_names(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_MODULE_NAMES:
            {
                parse_module_names(slot, buff, &subOffset);
                break;
            }

            case SUB_SEL_PARAM_PAGE:
            {
                read_bit_stream(buff, &bitOffset, 8);  /* TODO - read the param page single byte here */
                break;
            }

            case SUB_RESPONSE_PATCH_DESCRIPTION:
            {
                LOG_DEBUG("Patch Descr\n");

                uint32_t tmpSubOffset = subOffset;

                for (int i = 0; i < 32; i++) {
                    LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, &subOffset, 8));
                }

                LOG_DEBUG_DIRECT("\n");
                subOffset = tmpSubOffset;
                parse_patch_descr(buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_MORPH_PARAMS:
            {
                parse_morph_params(slot, buff, &subOffset, subOffset + BYTE_TO_BIT(count));
                break;
            }

            case SUB_RESPONSE_KNOBS:
            {
                parse_knobs(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_CONTROLLERS:
            {
				parse_controllers(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_CURRENT_NOTE_2:
            {
                LOG_DEBUG("Current note 2\n");

                if (slot < MAX_SLOTS) {
                    uint32_t safeCount = ((uint32_t)count < sizeof(gNote2[slot])) ? (uint32_t)count : sizeof(gNote2[slot]);

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
                    uint32_t safeCount = ((uint32_t)count < sizeof(gPatchNotes[slot])) ? (uint32_t)count : sizeof(gPatchNotes[slot]);

                    for (uint32_t i = 0; i < safeCount; i++) {
                        gPatchNotes[slot][i] = read_bit_stream(buff, &subOffset, 8);
                    }
                    gPatchNotesSize[slot] = safeCount;
                }

                break;
            }

            default:
            {
                LOG_DEBUG("Unprocessed type 0x%02x\n", type);
                break;
            }
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
    pthread_mutex_lock(&usbStaticMutex);
    slotVersion[slot] = version;
    pthread_mutex_unlock(&usbStaticMutex);

    return EXIT_SUCCESS;
}

static void parse_param_change(uint32_t slot, uint8_t * buff, int length) {
    uint32_t   bitPos    = 0;
    tModule    module    = {0};
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

    read_module(key, &module);
    module.param[variation][param].value = value;
    write_module(key, &module);

    LOG_DEBUG("Param change - module %u:%u param = %u value = %u\n", key.location, key.index, param, value);
}

static int parse_command_response(uint8_t * buff, uint32_t * bitPos, uint8_t commandResponse, uint8_t subCommand, int length) {
    tModule  module = {0};
    uint32_t slot   = commandResponse & 0x03;

    switch (subCommand) {
        case SUB_RESPONSE_VOLUME_INDICATOR:
        {
            read_bit_stream(buff, bitPos, 8);

            for (int32_t location = 1; location >= 0; location--) {
                for (int k = 0; k <= 255; k++) {
                    module.key.slot     = slot;
                    module.key.location = location;
                    module.key.index    = k;

                    if (read_module(module.key, &module) == true) {
                        switch (gModuleProperties[module.type].volumeType) {
                            case volumeTypeStereo:
                            {
                                module.volume.value1 = read_bit_stream(buff, bitPos, 16);
                                module.volume.value2 = read_bit_stream(buff, bitPos, 16);
                                break;
                            }
                            case volumeTypeMono:
                            {
                                module.volume.value1 = read_bit_stream(buff, bitPos, 16);
                                module.volume.value2 = 0;
                                break;
                            }
                            case volumeTypeCompress:
                            {
                                module.volume.value1 = read_bit_stream(buff, bitPos, 16);
                                module.volume.value2 = 0;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }

                        if (gModuleProperties[module.type].volumeType != volumeTypeNone) {
                            write_module(module.key, &module);
                        }
                    }
                }
            }

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_LED_DATA:
        {
            for (int i = 4; i < (length - 2); i++) {
                buff[i] = reverse_bits_in_byte(buff[i]);
            }

            read_bit_stream(buff, bitPos, 8);

            for (int k = 0; k <= 255; k++) {
                module.key.slot     = slot;
                module.key.location = gLocation;
                module.key.index    = k;

                if (read_module(module.key, &module) == true) {
                    if (gModuleProperties[module.type].ledType == ledTypeYes) {
                        module.led.value = read_bit_stream(buff, bitPos, 1);
                        read_bit_stream(buff, bitPos, 1);

                        write_module(module.key, &module);
                    }
                }
            }

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_ERROR:
        {
            LOG_DEBUG("Got Error!!!\n");
            return EXIT_FAILURE;
        }

        case SUB_RESPONSE_RESOURCES_USED:
        {
            LOG_DEBUG("Got resources in use slot %u\n", commandResponse);
            return EXIT_SUCCESS;
        }
        case SUB_RESPONSE_PARAM_CHANGE:
        {
            parse_param_change(slot, &buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_VERSION:
        {
            LOG_DEBUG("Got get patch version\n");
            return parse_patch_version(&buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
        }

        case SUB_RESPONSE_SYNTH_SETTINGS:
        {
            LOG_DEBUG("Got synth settings\n");
            return parse_synth_settings(&buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
        }

        case SUB_RESPONSE_MIDI_CC:
        {
            LOG_DEBUG("Got MIDI cc response for slot %u\n", slot);
            return parse_midi_cc(&buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
        }

        case SUB_RESPONSE_GLOBAL_PAGE:
        {
            LOG_DEBUG("Got Global page\n");
            {
                uint32_t tmpBitPos = *bitPos;

                for (int i = 0; i < length; i++) {
                    LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, bitPos, 8));
                }

                LOG_DEBUG_DIRECT("\n");
                *bitPos = tmpBitPos;
            }
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_VERSION_CHANGE:
        {
            LOG_DEBUG("Got Patch load\n");
            atomic_store(&gotPatchChangeIndication, true);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_ASSIGNED_VOICES:
        {
            LOG_DEBUG("Got assigned voices Response\n");
            return EXIT_SUCCESS;
        }

        case SUB_COMMAND_SET_ASSIGNED_VOICES:
        {
            LOG_DEBUG("Got assigned voiced command - shouldn't happen!?\n");
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PERFORMANCE_NAME:
        {
            LOG_DEBUG("Got performance name\n");
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_MASTER_CLOCK:
        {
            LOG_DEBUG("Got master clock\n");
            return EXIT_SUCCESS;
        }

        case SUB_COMMAND_SELECT_SLOT:
        {
            uint32_t newSlot = 0;

            LOG_DEBUG("Got slot select\n");
            newSlot = read_bit_stream(buff, bitPos, 8);

            pthread_mutex_lock(&gGlobalVarsMutex);
            gSlot = newSlot;
            set_exclusive_button_highlight(slotAButtonId, slotDButtonId, (tButtonId)(slotAButtonId + newSlot));
            pthread_mutex_unlock(&gGlobalVarsMutex);

            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_DESCRIPTION:
        {
            LOG_DEBUG("Got Patch info\n");
            parse_patch(slot, &buff[BIT_TO_BYTE(*bitPos) - 1], (length - BIT_TO_BYTE(*bitPos) - CRC_BYTES) + 1);
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_NAME:
        {
            uint32_t   tmpBitPos    = *bitPos;
            LOG_DEBUG("Got Patch name (length %d)\n", length);

            for (int i = 0; i < (length - 6); i++) {
                uint8_t ch = read_bit_stream(buff, bitPos, 8);

                    LOG_DEBUG_DIRECT("0x%02x ", ch);
            }
            LOG_DEBUG_DIRECT("\n");
            
            LOG_DEBUG_DIRECT("'");
            *bitPos = tmpBitPos;
            
            for (int i = 0; i < (length - 6); i++) {
                uint8_t ch = read_bit_stream(buff, bitPos, 8);

                if (ch <= 0x7F) {
                    LOG_DEBUG_DIRECT("%c", ch);
                }
            }

            LOG_DEBUG_DIRECT("'\n");
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_OK:
        {
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_SELECT_PARAM:
        {
            LOG_DEBUG("Got select param\n");
            return EXIT_SUCCESS;
        }

        default:
        {
            LOG_DEBUG("Got unknown sub-command 0x%02x\n", subCommand);
            return EXIT_FAILURE;
        }
    }
}

static int parse_incoming(uint8_t * buff, int length) {
    int ret = EXIT_FAILURE;

    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }
    uint32_t bitPos       = 0;
    uint8_t  responseType = read_bit_stream(buff, &bitPos, 8);

    switch (responseType) {
        case RESPONSE_TYPE_INIT:
        {
            LOG_DEBUG("Got Response init\n");

            ret = EXIT_SUCCESS;
            break;
        }

        case RESPONSE_TYPE_COMMAND:
        {
            uint8_t commandResponse = read_bit_stream(buff, &bitPos, 8);
            /*uint8_t version         =*/ read_bit_stream(buff, &bitPos, 8);
            uint8_t subCommand = read_bit_stream(buff, &bitPos, 8);

            ret = parse_command_response(buff, &bitPos, commandResponse, subCommand, length);
            break;
        }

        default:
        {
            LOG_DEBUG("Got Unknown response type 0x%02x\n", responseType);
            ret = EXIT_FAILURE;
            break;
        }
    }
    call_wake_glfw();
    return ret;
}

static int rcv_extended(int dataLength) {
    int                    retVal                      = EXIT_FAILURE;
    uint32_t               bitPos                      = 0;
    uint8_t                buff[EXTENDED_MESSAGE_SIZE] = {0};
    int                    readLength                  = 0;
    int                    tries                       = 0;
    uint8_t                responseType                = 0;
    libusb_device_handle * devHandle_local             = NULL;

    pthread_mutex_lock(&usbStaticMutex);
    devHandle_local = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (devHandle_local == NULL) {
        LOG_ERROR("Attempted to use uninitialized USB device handle\n");
        return EXIT_FAILURE;
    }

    for (tries = 1; tries < 5; tries++) {
        memset(buff, 0, sizeof(buff));
        readLength = 0;
        retVal     = libusb_bulk_transfer(devHandle_local, 0x82, buff, sizeof(buff), &readLength, 100);

        if (retVal == LIBUSB_SUCCESS) {
            if (readLength > 0) {
                responseType = buff[0];

                if ((responseType == RESPONSE_TYPE_INIT) || (responseType == RESPONSE_TYPE_COMMAND)) {
                    break;
                }
            }
        } else if (retVal == LIBUSB_ERROR_NO_DEVICE) {
            readLength = 0;
            atomic_store(&gotBadConnectionIndication, true);
            break;
        }
        usleep(1000);
    }

    if (readLength == dataLength) {
        bitPos = SIGNED_BYTE_TO_BIT(dataLength - 2);

        if (calc_crc16(buff, dataLength - 2) == read_bit_stream(buff, &bitPos, 16)) {
            retVal = parse_incoming(buff, dataLength);
        } else {
            LOG_DEBUG("Bad CRC on ext message\n");
            retVal = -1;
        }
    } else {
        LOG_DEBUG("Read ext problem! Read length = %d, data length = %d\n", readLength, dataLength);
        retVal = -1;
    }
    return retVal;
}

static int int_rec(void) {
    int                    retVal                       = EXIT_FAILURE;
    uint32_t               bitPos                       = 0;
    uint8_t                buff[INTERRUPT_MESSAGE_SIZE] = {0};
    int                    readLength                   = 0;
    int                    dataLength                   = 0;
    int                    type                         = 0;
    int                    i                            = 0;
    bool                   foundNoneZero                = false;
    int                    tries                        = 0;
    libusb_device_handle * devHandle_local              = NULL;

    pthread_mutex_lock(&usbStaticMutex);
    devHandle_local = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (devHandle_local == NULL) {
        LOG_ERROR("Attempted to use uninitialized USB device handle\n");
        return EXIT_FAILURE;
    }

    for (tries = 1; tries < 5; tries++) {
        memset(buff, 0, sizeof(buff));
        readLength = 0;
        retVal     = libusb_bulk_transfer(devHandle_local, 0x81, buff, sizeof(buff), &readLength, 100);

        if (retVal == LIBUSB_SUCCESS) {
            if (readLength > 0) {
                break;
            }
        } else if (retVal == LIBUSB_ERROR_NO_DEVICE) {
            readLength = 0;
            atomic_store(&gotBadConnectionIndication, true);
            break;
        }
        usleep(1000);
    }

    if (readLength > 0) {
        dataLength = read_bit_stream(buff, &bitPos, 4);
        type       = read_bit_stream(buff, &bitPos, 4);

        if (type == RESPONSE_TYPE_EXTENDED) {
            for (i = 3; i < readLength; i++) {
                if (buff[i] != 0) {
                    foundNoneZero = true;
                    break;
                }
            }

            if (foundNoneZero == false) {
                dataLength = read_bit_stream(buff, &bitPos, 16);

                retVal = rcv_extended(dataLength);
            }
        } else if (type == RESPONSE_TYPE_EMBEDDED) {
            retVal = parse_incoming(buff + 1, dataLength);
        }
    }
    return retVal;
}

static int send_command(int state) {
    int                    retVal                  = EXIT_FAILURE;
    uint8_t                buff[SEND_MESSAGE_SIZE] = {0};
    uint16_t               crc                     = 0;
    int                    msgLength               = 0;
    int                    pos                     = COMMAND_OFFSET;
    int                    actualLength            = 0;
    uint8_t                slotVersion_local[MAX_SLOTS];
    libusb_device_handle * devHandle_local = NULL;

    pthread_mutex_lock(&usbStaticMutex);
    memcpy(slotVersion_local, slotVersion, sizeof(slotVersion));
    devHandle_local = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (devHandle_local == NULL) {
        LOG_ERROR("Attempted to use uninitialized USB device handle\n");
        return EXIT_FAILURE;
    }

    switch (state) {
        case eStateInit:
            buff[pos++] = 0x80;
            break;

        case eStateStop:
        case eStateStart:
        case eStateSelectSlot:
        case eStateGetPatchVersionSlotA:
        case eStateGetPatchVersionSlotB:
        case eStateGetPatchVersionSlotC:
        case eStateGetPatchVersionSlotD:
        case eStateGetSynthSettings:
        case eStateGetMidiCc:
        case eStateGetUnknown2:
        case eStateGetPatchSlotA:
        case eStateGetPatchSlotB:
        case eStateGetPatchSlotC:
        case eStateGetPatchSlotD:
        case eStateGetPatchNameSlotA:
        case eStateGetPatchNameSlotB:
        case eStateGetPatchNameSlotC:
        case eStateGetPatchNameSlotD:
            buff[pos++] = 0x01;

            switch (state) {
                case eStateStop:
                case eStateStart:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = SUB_COMMAND_START_STOP;

                    switch (state) {
                        case eStateStop:
                            buff[pos++] = 0x01;
                            break;

                        case eStateStart:
                            buff[pos++] = 0x00;
                            break;

                        default:
                            LOG_DEBUG("Unknown state %d\n", state);
                            break;
                    }
                    break;

                case eStateSelectSlot:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x00;
                    buff[pos++] = SUB_COMMAND_SELECT_SLOT;
                    buff[pos++] = 0;
                    break;

                case eStateGetSynthSettings:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = SUB_COMMAND_GET_SYNTH_SETTINGS;
                    break;

                case eStateGetMidiCc:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = SUB_COMMAND_GET_MIDI_CC;
                    break;

                case eStateGetUnknown2:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x00;
                    buff[pos++] = SUB_COMMAND_UNKNOWN_2;
                    break;

                case eStateGetPatchVersionSlotA:
                case eStateGetPatchVersionSlotB:
                case eStateGetPatchVersionSlotC:
                case eStateGetPatchVersionSlotD:
                {
                    uint32_t slot = state - eStateGetPatchVersionSlotA;

                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = SUB_COMMAND_GET_PATCH_VERSION;
                    buff[pos++] = slot;
                    break;
                }

                case eStateGetPatchSlotA:
                case eStateGetPatchSlotB:
                case eStateGetPatchSlotC:
                case eStateGetPatchSlotD:
                {
                    uint32_t slot = state - eStateGetPatchSlotA;

                    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
                    buff[pos++] = slotVersion_local[slot];
                    buff[pos++] = SUB_COMMAND_GET_PATCH_SLOT;
                    break;
                }

                case eStateGetPatchNameSlotA:
                case eStateGetPatchNameSlotB:
                case eStateGetPatchNameSlotC:
                case eStateGetPatchNameSlotD:
                {
                    uint32_t slot = state - eStateGetPatchNameSlotA;

                    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
                    buff[pos++] = slotVersion_local[slot];
                    buff[pos++] = SUB_COMMAND_GET_PATCH_NAME;
                    break;
                }

                default:
                    LOG_DEBUG("Unknown state %d\n", state);
                    break;
            }
            break;

        default:
            LOG_DEBUG("Unknown state %d\n", state);
            break;
    }
    msgLength = pos - COMMAND_OFFSET;

    if (msgLength > 0) {
        crc = calc_crc16(&buff[COMMAND_OFFSET], msgLength);
        write_uint16(&buff[msgLength + 2], crc);
        msgLength += 4;

        write_uint16(&buff[0], msgLength);

        if (libusb_bulk_transfer(devHandle_local, 3, buff, msgLength, &actualLength, 100) == 0) {
            retVal = EXIT_SUCCESS;
        } else {
            atomic_store(&gotBadConnectionIndication, true);
        }
    }
    return retVal;
}

static int send_write_data(tMessageContent * messageContent) {
    int                    retVal                  = EXIT_FAILURE;
    uint8_t                buff[SEND_MESSAGE_SIZE] = {0};
    uint16_t               crc                     = 0;
    int                    msgLength               = 0;
    int                    pos                     = COMMAND_OFFSET;
    int                    actualLength            = 0;
    uint8_t                slotVersion_local[MAX_SLOTS];
    libusb_device_handle * devHandle_local = NULL;

    pthread_mutex_lock(&usbStaticMutex);
    memcpy(slotVersion_local, slotVersion, sizeof(slotVersion));
    devHandle_local = devHandle;
    pthread_mutex_unlock(&usbStaticMutex);

    if (devHandle_local == NULL) {
        LOG_ERROR("Attempted to use uninitialized USB device handle\n");
        return EXIT_FAILURE;
    }

    switch (messageContent->cmd) {
        case eMsgCmdSetValue:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_PARAM;
            buff[pos++] = messageContent->paramData.moduleKey.location;
            buff[pos++] = messageContent->paramData.moduleKey.index;
            buff[pos++] = messageContent->paramData.param;
            buff[pos++] = messageContent->paramData.value;
            buff[pos++] = messageContent->paramData.variation;
            break;

        case eMsgCmdSetMode:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_MODE;
            buff[pos++] = messageContent->modeData.moduleKey.location;
            buff[pos++] = messageContent->modeData.moduleKey.index;
            buff[pos++] = messageContent->modeData.mode;
            buff[pos++] = messageContent->modeData.value;
            LOG_DEBUG("SET MODE %u %u\n", messageContent->modeData.mode, messageContent->modeData.value);
            break;

        case eMsgCmdWriteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_WRITE_CABLE;
            buff[pos++] = 0x10 | (messageContent->cableData.location << 3) | 0x00;
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex;
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;
            break;

        case eMsgCmdWriteModule:
            LOG_DEBUG("Writing module!\n");
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
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
            pos += strlen(messageContent->moduleData.name) + 1;
            break;

        case eMsgCmdMoveModule:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_MOVE_MODULE;
            buff[pos++] = messageContent->moduleData.moduleKey.location;
            buff[pos++] = messageContent->moduleData.moduleKey.index;
            buff[pos++] = messageContent->moduleData.column;
            buff[pos++] = messageContent->moduleData.row;
            break;

        case eMsgCmdDeleteModule:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_DELETE_MODULE;
            buff[pos++] = messageContent->moduleData.moduleKey.location;
            buff[pos++] = messageContent->moduleData.moduleKey.index;
            break;

        case eMsgCmdSetModuleUpRate:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_MODULE_UPRATE;
            buff[pos++] = messageContent->moduleData.moduleKey.location;
            buff[pos++] = messageContent->moduleData.moduleKey.index;
            buff[pos++] = messageContent->moduleData.upRate;
            break;

        case eMsgCmdDeleteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_DELETE_CABLE;
            buff[pos++] = 0x2 | messageContent->cableData.location;
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex;
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;
            break;

        case eMsgCmdSetParamMorph:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_MORPH_RANGE;
            buff[pos++] = messageContent->paramMorphData.moduleKey.location;
            buff[pos++] = messageContent->paramMorphData.moduleKey.index;
            buff[pos++] = messageContent->paramMorphData.param;
            buff[pos++] = messageContent->paramMorphData.paramMorph;
            buff[pos++] = messageContent->paramMorphData.value;
            buff[pos++] = messageContent->paramMorphData.negative;
            buff[pos++] = messageContent->paramMorphData.variation;
            break;

        case eMsgCmdSelectVariation:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SELECT_VARIATION;
            buff[pos++] = messageContent->variationData.variation;
            break;

        case eMsgCmdSelectSlot:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SYS;
            buff[pos++] = 0;
            buff[pos++] = SUB_COMMAND_SELECT_SLOT;
            buff[pos++] = messageContent->slotData.slot;
            break;
            
        case eMsgCmdInitPatch:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SYS | messageContent->slot;
            buff[pos++] = slotVersion_local[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET; // 0x37
            buff[pos++] = 0;
            buff[pos++] = 0;
            buff[pos++] = 0;
            buff[pos++] = 'N';
            buff[pos++] = 'o';
            buff[pos++] = ' ';
            buff[pos++] = 'N';
            buff[pos++] = 'a';
            buff[pos++] = 'm';
            buff[pos++] = 'e';
            buff[pos++] = 0;
            buff[pos++] = 0x21;
            buff[pos++] = 0;
            buff[pos++] = 0xf;
            buff[pos++] = 0;
            buff[pos++] = 0;
            // Name length = 16
            
            // Todo - obvs.
            break;


        default:
            break;
    }
    msgLength = pos - COMMAND_OFFSET;

    if (msgLength > 0) {
        crc = calc_crc16(&buff[COMMAND_OFFSET], msgLength);
        write_uint16(&buff[msgLength + 2], crc);
        msgLength += 4;

        write_uint16(&buff[0], msgLength);

        if (libusb_bulk_transfer(devHandle_local, 3, buff, msgLength, &actualLength, 100) == 0) {
            retVal = EXIT_SUCCESS;
        } else {
            atomic_store(&gotBadConnectionIndication, true);
        }
    }
    return retVal;
}

static void state_handler(void) {
    static eState   state          = eStateFindDevice;
    tMessageContent messageContent = {0};

    switch (state) {
        case eStateFindDevice:
        {
            libusb_device_handle * devHandle_local = libusb_open_device_with_vid_pid(libUsbCtx, VENDOR_ID, PRODUCT_ID);

            if (devHandle_local != NULL) {
                if (libusb_reset_device(devHandle_local) == LIBUSB_SUCCESS) {
                    if (libusb_claim_interface(devHandle_local, 0) == LIBUSB_SUCCESS) {
                        pthread_mutex_lock(&usbStaticMutex);
                        devHandle = devHandle_local;
                        pthread_mutex_unlock(&usbStaticMutex);
                        state = eStateInit;
                    }
                }
            }

            if (state != eStateInit) {
                usleep(100000);
            }
            break;
        }

        case eStateInit:
        case eStateStop:
        case eStateSelectSlot:
        case eStateGetPatchVersionSlotA:
        case eStateGetPatchVersionSlotB:
        case eStateGetPatchVersionSlotC:
        case eStateGetPatchVersionSlotD:
        case eStateGetPatchSlotA:
        case eStateGetPatchSlotB:
        case eStateGetPatchSlotC:
        case eStateGetPatchSlotD:
        case eStateGetPatchNameSlotA:
        case eStateGetPatchNameSlotB:
        case eStateGetPatchNameSlotC:
        case eStateGetPatchNameSlotD:
        case eStateGetSynthSettings:
        case eStateGetMidiCc:
        case eStateGetUnknown2:
        case eStateStart:

            if (send_command(state) == EXIT_SUCCESS) {
                if (int_rec() == EXIT_SUCCESS) {
                    if (state == eStateStop) {

                        // TODO - Should possibly just be per slot and clear global vars? Review below
                        database_clear_cables();
                        database_clear_modules();
                        
                        call_full_patch_change_notify();
                        call_wake_glfw();
                    } else if (state == eStateStart) {
                        call_full_patch_change_notify();
                        call_wake_glfw();
                    }
                    state += 1;
                }
            }
            break;

        case eStatePoll:

            if (msg_receive(&gCommandQueue, eRcvPoll, &messageContent) == EXIT_SUCCESS) {
                send_write_data(&messageContent);
            } else {
                int_rec();
            }
            break;

        default:
            LOG_DEBUG("State unknown %d\n", state);
            usleep(1000);
            break;
    }

    if (atomic_load(&gotBadConnectionIndication)) {
        LOG_DEBUG("Bad connection indication\n");
        state = eStateFindDevice;
        atomic_store(&gotBadConnectionIndication, false);
    }

    if (atomic_load(&gotPatchChangeIndication)) {
        LOG_DEBUG("Patch change indication\n");
        state = eStateStop;
        atomic_store(&gotPatchChangeIndication, false);
    }
}

static void usb_comms_signal_handler(int sigraised) {
    LOG_DEBUG("\nUSBComms Sig Handler!!! %d\n", sigraised);

    _exit(0);
}

static int usb_comms_init_signals(void) {
    int retVal = EXIT_FAILURE;

    if (signal(SIGINT, usb_comms_signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGBUS, usb_comms_signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGSEGV, usb_comms_signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGTERM, usb_comms_signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGABRT, usb_comms_signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }
    return retVal;
}

static void * usb_thread_loop(void * arg) {
    usb_comms_init_signals();

    msg_init(&gCommandQueue, "command");

    if (libusb_init(&libUsbCtx) != LIBUSB_SUCCESS) {
        return NULL;
    }
    libusb_set_option(libUsbCtx, LIBUSB_OPTION_LOG_LEVEL, 1);

    while (gQuitAll == false) {
        state_handler();
    }
    libusb_exit(libUsbCtx);

    return NULL;
}

void start_usb_thread(void) {
    if (pthread_create(&usbThread, NULL, usb_thread_loop, 0) != EXIT_SUCCESS) {
        exit(EXIT_FAILURE);
    }
}

#ifdef __cplusplus
}
#endif

