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
#include "iokit.h"
#include "utils.h"
#include "msgQueue.h"
#include "usbComms.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"

typedef enum {
    eStateNone,
    eStateFindDevice,
    eStateInit,     // Items from here increment in order for init sequence states
    eStateStop,
    eStateGetSynthSettings,
    eStateGetUnknown1,
    eStateGetUnknown2,
    eStateSelectSlot,
    eStateGetPatchVersion,
    eStateGetPatchSlot,
    eStateGetPatchNameSlot,
    eStateStart,     // Last one to increment in order
    eStatePoll,
    eStateExit
} eState;

tMessageQueue    gCommandQueue = {0};

static bool      gotBadConnectionIndication = false;
static bool      gotPatchChangeIndication   = false;
static uint8_t   slotVersion[MAX_SLOTS]     = {0};
static pthread_t usbThread                  = NULL;
static void      (*wake_glfw_func_ptr)(void) = NULL;
static void      (*full_patch_change_notify_func_ptr)(void) = NULL;

extern uint32_t  gLocation;

void register_glfw_wake_cb(void ( *func_ptr )(void)) {
    wake_glfw_func_ptr = func_ptr;
}

void register_full_patch_change_notify_cb(void ( *func_ptr )(void)) {
    full_patch_change_notify_func_ptr = func_ptr;
}

static void call_wake_glfw(void) {
    if (wake_glfw_func_ptr == NULL) {
        printf("Wake GLFW callback function not registered\n");
        exit(1);
    }
    wake_glfw_func_ptr();
}

static void call_full_patch_change_notify(void) {
    if (full_patch_change_notify_func_ptr == NULL) {
        printf("Full patch change callback function not registered\n");
        exit(1);
    }
    full_patch_change_notify_func_ptr();
}

static int parse_synth_settings(uint8_t * buff, int length) {
    int      retVal = EXIT_FAILURE;
    int      i      = 0;
    uint32_t bitPos = 0;
    uint8_t  ch     = 0;

    if (buff == NULL) {
        return retVal;
    }
    printf("Clavia string '");

    for (i = 0; i < 11; i++) {
        ch = read_bit_stream(buff, &bitPos, 8);

        if ((ch <= 0x7f) && (ch >= 0x20)) {
            printf("%c", ch);
        }
    }

    printf("'\n");

    printf("Perf Mode 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Perf Bank 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Perf Location 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Memory Protect (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 7);
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));     // 16 = off/disabled
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("MIDI chan Slot A 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Global chan 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Sysex ID 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Local on (bit 0)0x%x\n", read_bit_stream(buff, &bitPos, 1));     //Actually top bit = 0x80
    read_bit_stream(buff, &bitPos, 7);
    read_bit_stream(buff, &bitPos, 6);
    printf("Prog Change Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    printf("Prog Change Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 6);
    printf("Controllers Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));     // send actually bit 0
    printf("Controllers Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));     // send actually bit 0
    printf("Send Clock (bit 1), ignore ext clock (bit 2) , 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Tune cent 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Global Shift Active (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Global Octave Shift 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Tune semi 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Filler 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    printf("Pedal Polarity (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));     //0xC0 (1100) = closed, 0x40 (0100) = open therefore bit 7
    printf("Control Pedal Gain 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    //17 other unknown bytes to go

    retVal = EXIT_SUCCESS;

    return retVal;
}

static void parse_module_list(uint8_t * buff, uint32_t * subOffset) {
    uint32_t   i      = 0;
    uint32_t   j      = 0;
    uint32_t   mode   = 0;
    uint32_t   type   = 0;
    tModuleKey key    = {0};
    tModule    module = {0};

    printf("Module list\n");

    key.location = read_bit_stream(buff, subOffset, 2);
    printf("Location       0x%x\n", key.location);     // Discerns between FX and main, could put in the module itself
    uint32_t moduleCount = read_bit_stream(buff, subOffset, 8);
    printf("Module Count   %d\n", moduleCount);

    for (i = 0; i < moduleCount; i++) {
        type      = read_bit_stream(buff, subOffset, 8);
        key.index = read_bit_stream(buff, subOffset, 8);

        if (read_module(key, &module) == true) {
            printf("Module already created\n");
        }
        module.type      = type;
        module.column    = read_bit_stream(buff, subOffset, 7);        // 7
        module.row       = read_bit_stream(buff, subOffset, 7);        // 7
        module.colour    = read_bit_stream(buff, subOffset, 8);        // 8
        module.upRate    = read_bit_stream(buff, subOffset, 1);        // 1
        module.isLed     = read_bit_stream(buff, subOffset, 1);        // 1
        module.unknown1  = read_bit_stream(buff, subOffset, 6);        // 6
        module.modeCount = read_bit_stream(buff, subOffset, 4);        // 4

        for (j = 0; j < module.modeCount; j++) {
            module.mode[j].value = read_bit_stream(buff, subOffset, 6);
            printf("Mode index %u = %u\n", j, module.mode[j].value);
            printf("MODE %u %u\n", i, module.mode[j].value);
        }

        allocate_module_parameters(&module, module_param_count(type)); // Also done on parameter set-up, so whichever's first
        allocate_module_connectors(&module, module_connector_count(type));

        printf("Number connectors for module %u\n", module_connector_count(type));
        write_module(key, &module);
    }
}

static void parse_cable_list(uint8_t * buff, uint32_t * subOffset) {
    tCableKey key   = {0};
    tCable    cable = {0};

    printf("Cable list\n");

    key.location = read_bit_stream(buff, subOffset, 2);
    printf("Location       0x%x\n", key.location);
    printf("Unknown        0x%x\n", read_bit_stream(buff, subOffset, 12));
    uint32_t cableCount = read_bit_stream(buff, subOffset, 10);
    printf("Cable Count    %d\n", cableCount);

    for (uint32_t i = 0; i < cableCount; i++) {
        cable.colour = read_bit_stream(buff, subOffset, 3);
        //printf(" Colour         0x%x\n", cable.colour);
        key.moduleFromIndex = read_bit_stream(buff, subOffset, 8);         // key will get written into struct on write
        //printf(" Module From    %d\n", key.moduleFromIndex);
        key.connectorFromIoCount = read_bit_stream(buff, subOffset, 6);
        //printf(" Connector From %d\n", key.connectorFromIoCount);
        key.linkType = read_bit_stream(buff, subOffset, 1);   // 1 = output to input, 0 = input to input
        //printf(" Link Type      0x%x\n", key.linkType);
        key.moduleToIndex = read_bit_stream(buff, subOffset, 8);
        //printf(" Module To      %d\n", key.moduleToIndex);
        key.connectorToIoCount = read_bit_stream(buff, subOffset, 6);
        //printf(" Connector To   %d\n", key.connectorToIoCount);

        write_cable(key, &cable);
    }
}

static void parse_param_list(uint8_t * buff, uint32_t * subOffset) {
    //file.pas at around line 3166 or 3511, possibly pivoted on location?
    uint32_t   variationCount = 0;
    uint32_t   paramCount     = 0;
    uint32_t   moduleCount    = 0;
    uint32_t   paramValue     = 0;
    tModuleKey key            = {0};
    tModule    module         = {0};
    int        i              = 0;
    int        j              = 0;
    int        k              = 0;

    printf("Param list\n");
    key.location = read_bit_stream(buff, subOffset, 2);
    printf("Location       0x%x\n", key.location);     // 0..1 = param list, 2 = patch settings 2=morph!?
    // SWITCH ON LOC BEING 0..1 or 2

    moduleCount = read_bit_stream(buff, subOffset, 8);
    printf("Module Count      %u\n", moduleCount);
    variationCount = read_bit_stream(buff, subOffset, 8);     // Should always be 10 (VARIATIONS) - todo: sanity check
    printf("Variation Count      %u\n", variationCount);

    for (i = 0; i < moduleCount; i++) {
        key.index = read_bit_stream(buff, subOffset, 8);
        printf(" Module Index        %u\n", key.index);

        paramCount = read_bit_stream(buff, subOffset, 7);
        printf("  variation list param count = %u\n", paramCount);

        read_module(key, &module);

        allocate_module_parameters(&module, paramCount); // Also done on module creation, so whichever's first

        for (j = 0; j < variationCount; j++) {                                                          // 0 to 9, but last 2 not available on old editor. Possibly/probably init values?
            uint32_t variation = read_bit_stream(buff, subOffset, 8);
            //printf("   Variation %02d Variation %u\n", j, variation);         // Should match!!!

            for (k = 0; k < paramCount; k++) {
                paramValue = read_bit_stream(buff, subOffset, 7);
                //printf("   Param number %02d param value %02d\n", k, paramValue);
                module.param[j][k].value = paramValue;
            }
        }

        write_module(key, &module);         // Careful with type 2, morphs!
    }
}

static void parse_param_names(uint8_t * buff, uint32_t * subOffset, int count) {
    //uint32_t location = 0;
    uint32_t   nameCount    = 0;
    uint32_t   paramLength  = 0;
    uint32_t   moduleLength = 0;
    //uint32_t index = 0;
    tModule    module = {0};
    tModuleKey key    = {0};
    int        i      = 0;
    int        j      = 0;
    int        k      = 0;

    printf("Param names\n");

    key.location = read_bit_stream(buff, subOffset, 2);
    printf("Location       0x%x\n", key.location);
    //printf("Unknown        %d\n", read_bit_stream(buff, subOffset, 6));
    nameCount = read_bit_stream(buff, subOffset, 8);
    printf("NameCount      %d\n", nameCount);
    //printf("Module count      %d\n", read_bit_stream(buff, subOffset, 8));

    // SWITCH ON LOC BEING 0..1 or 2
    for (i = 0; i < nameCount; i++) {
        key.index = read_bit_stream(buff, subOffset, 8);
        printf("Module index      %d\n", key.index);

        if (read_module(key, &module) == false) {
            write_module(key, &module);
        }
        moduleLength = read_bit_stream(buff, subOffset, 8);
        printf("Module length     %d\n", moduleLength);         // 5004

        for (j = 0; j < moduleLength;) {
            printf("IsString     %d\n", read_bit_stream(buff, subOffset, 8));
            paramLength = read_bit_stream(buff, subOffset, 8);
            printf("ParamLen     %d\n", paramLength);
            printf("Param Index  %d\n", read_bit_stream(buff, subOffset, 8));
            j += 3;
            printf("Param name: ");

            if (paramLength > 0) { // Shouldn't ever be zero, so since we've seen that - something strange happening, generate error!?
                for (k = 0; k < paramLength - 1; k++) {
                    uint8_t ch = read_bit_stream(buff, subOffset, 8);

                    if ((ch >= 0x20) && (ch <= 0x7f)) {
                        printf("%c", ch);
                    }
                }

                printf("\n");
                j += paramLength - 1;
            }
        }
    }
}

static void parse_module_names(uint8_t * buff, uint32_t * subOffset) {
    tModule    module = {0};
    tModuleKey key    = {0};
    uint32_t   i      = 0;
    char       name[MODULE_NAME_SIZE + 1];


    printf("Module names\n");

    key.location = read_bit_stream(buff, subOffset, 2);
    read_bit_stream(buff, subOffset, 6);
    printf("Location 0x%x\n", key.location);
    uint32_t items = read_bit_stream(buff, subOffset, 8);
    printf("Items %u\n", items);

    for (i = 0; i < items; i++) {
        key.index = read_bit_stream(buff, subOffset, 8);
        printf(" Module Name Index %u\n", key.index);

        printf(" Module loc %u index %u\n", module.key.location, module.key.index);

        for (int k = 0; k < MODULE_NAME_SIZE; k++) {
            name[k] = read_bit_stream(buff, subOffset, 8);

            if (name[k] == '\0') {
                break;
            }
        }

        name[sizeof(name) - 1] = '\0'; // Make sure we're null terminated
        printf("%s\n", name);

        if (read_module(key, &module) == true) {
            strncpy(module.name, name, sizeof(module.name));
            module.name[sizeof(module.name) - 1] = '\0';
            write_module(key, &module);
        }
    }
}

int parse_patch(uint8_t * buff, int length) { // Todo - also accessed from file, so need to decide how to access from USB and file
    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }
    uint32_t bitOffset = 0;

    while (BIT_TO_BYTE(bitOffset) < length) {
        uint8_t  type      = read_bit_stream(buff, &bitOffset, 8);
        int16_t  count     = read_bit_stream(buff, &bitOffset, 16);
        uint32_t subOffset = bitOffset;

        printf("Type = 0x%x, Count = %d\n", type, count);

        switch (type) {
            case 0x4a:     // Module list
                parse_module_list(buff, &subOffset);
                break;

            case 0x52:     // Cable list
                parse_cable_list(buff, &subOffset);
                break;

            case 0x4d:     // Param list
                parse_param_list(buff, &subOffset);
                break;

            case 0x5b:     // Param names
                parse_param_names(buff, &subOffset, count);
                break;

            case 0x5a:     // Module names
                parse_module_names(buff, &subOffset);
                break;

            case 0x2d:     // Special case (ignore)
                // This 0x2d 0x00 sequence reportedly only appears on USB comms, not in files
                // so ignore as a size by moving back a byte
                count = -1;
                break;

            case 0x21:
                printf("Patch Descr\n");
                break;

            case 0x65:
                printf("Morph Params\n");
                break;

            case 0x62:
                printf("Knobs\n");
                break;

            case 0x60:
                printf("Controllers\n");
                break;

            default:
                printf("Unprocessed type 0x%02x\n", type);
                break;
        }
        bitOffset += SIGNED_BYTE_TO_BIT(count);
    }
    return EXIT_SUCCESS;
}

static int parse_patch_version(uint8_t * buff, int length) {
    if ((buff == NULL) || (length < 2)) {      // Ensure valid buffer and minimum required length
        return EXIT_FAILURE;
    }
    uint32_t bitPos  = 0;
    uint8_t  slot    = read_bit_stream(buff, &bitPos, 8);
    uint8_t  version = read_bit_stream(buff, &bitPos, 8);

    if (slot >= MAX_SLOTS) {     // Prevent out-of-bounds access
        return EXIT_FAILURE;
    }
    slotVersion[slot] = version;
    return EXIT_SUCCESS;
}

static void parse_param_change(uint8_t * buff, int length) {
    uint32_t   bitPos    = 0;
    tModule    module    = {0};
    tModuleKey key       = {0};
    uint32_t   param     = 0;
    uint32_t   variation = 0;
    uint32_t   value     = 0;

    key.location = read_bit_stream(buff, &bitPos, 8);
    key.index    = read_bit_stream(buff, &bitPos, 8);
    param        = read_bit_stream(buff, &bitPos, 8);
    value        = read_bit_stream(buff, &bitPos, 8);
    variation    = read_bit_stream(buff, &bitPos, 8);

    read_module(key, &module);
    module.param[variation][param].value = value;
    write_module(key, &module);

    printf("param = %u value = %u\n", param, value);
}

static int parse_command_response(uint8_t * buff, uint32_t * bitPos, uint8_t commandResponse, uint8_t subCommand, int length) {
    int i = 0;
    int j = 0;

    switch (commandResponse) {
        case 0x00: // slot!?
        case 0x01:
        case 0x02:
        case 0x03:

            switch (subCommand) {
                case SUB_COMMAND_VOLUME_INDICATOR:
                    tModule module = {0};

                    //printf("Vol ");
                    //for (i = 4; i < 40; i++)
                    //    printf("0x%02x ", buff[i]);
                    //printf("\n");

                    for (int k = 0; k <= 255; k++) {
                        module.key.location = gLocation;
                        module.key.index    = k;

                        if (read_module(module.key, &module) == true) {
                            switch (gModuleProperties[module.type].volumeType) {
                                case volumeTypeStereo:
                                {
                                    read_bit_stream(buff, bitPos, 8);
                                    module.volume.value1 = read_bit_stream(buff, bitPos, 8);
                                    read_bit_stream(buff, bitPos, 8);
                                    module.volume.value2 = read_bit_stream(buff, bitPos, 8);
                                    read_bit_stream(buff, bitPos, 8); // Unused / unknown!?
                                }
                                break;
                                case volumeTypeMono:
                                {
                                    module.volume.value1 = read_bit_stream(buff, bitPos, 8);
                                    module.volume.value2 = 0;
                                }
                                break;
                                case volumeTypeCompress:
                                {
                                    uint32_t val = 0;

                                    module.volume.value1  = read_bit_stream(buff, bitPos, 8);
                                    val                   = read_bit_stream(buff, bitPos, 8);
                                    module.volume.value2 |= (val << 8);
                                }
                                break;
                                default:
                                    break;
                            }

                            if (gModuleProperties[module.type].volumeType != volumeTypeNone) {
                                //printf("Module %u vol %u %u\n", module.key.index, module.volume[0], module.volume[1]);
                                write_module(module.key, &module);
                            }
                        }
                    }

                    call_wake_glfw();
                    return EXIT_SUCCESS;

                case SUB_COMMAND_LED_DATA:

                    for (i = 4; i < (length - 2); i++) {
                        buff[i] = reverse_bits_in_byte(buff[i]);
                    }

                    //printf("LED ");
                    //for (i = 4; i < (length-2); i++)
                    //    printf("0x%02x ", buff[i]);
                    //printf("\n");

                    read_bit_stream(buff, bitPos, 8); // Seems to be a byte of padding

                    for (int k = 0; k <= 255; k++) {
                        module.key.location = gLocation;
                        module.key.index    = k;

                        if (read_module(module.key, &module) == true) {
                            if (gModuleProperties[module.type].ledType == ledTypeYes) {
                                module.led.value = read_bit_stream(buff, bitPos, 1);
                                read_bit_stream(buff, bitPos, 1); // Not sure if this is used for anything yet, might just be padding

                                //printf("Module %u LED %u\n", module.key.index, module.led);
                                write_module(module.key, &module);
                            }
                        }
                    }

                    call_wake_glfw();
                    return EXIT_SUCCESS;

                case 0x7E:
                    printf("Got Error!!!\n");
                    return EXIT_FAILURE;

                case 0x72:
                    printf("Got resources in use slot %u\n", commandResponse);
                    return EXIT_SUCCESS;

                case 0x40:
                    parse_param_change(&buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
                    call_wake_glfw();
                    return EXIT_SUCCESS;

                default:
                    printf("Got 0x00 Unknown sub-command 0x%02x\n", subCommand);
                    return EXIT_SUCCESS;
            }
        case 0x0C:

            switch (subCommand) {
                case SUB_COMMAND_OK:
                    //printf("Got 0x0c OK\n");
                    return EXIT_SUCCESS;

                case SUB_COMMAND_GET_PATCH_VERSION:
                    printf("Got get patch version\n");
                    return parse_patch_version(&buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);

                case 0x03:
                    printf("Got synth settings\n");
                    return parse_synth_settings(&buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);

                case 0x80:
                    printf("Got response to deliberate unknown message 1\n");
                    return EXIT_SUCCESS;

                case 0x1E:
                    printf("Got response to deliberate unknown message 2\n");
                    return EXIT_SUCCESS;

                default:
                    printf("Got 0x0C Unknown sub-command 0x%02x\n", subCommand);
                    return EXIT_FAILURE;
            }
        case 0x04:

            switch (subCommand) {
                case 0x38:
                    printf("Got Patch load\n");
                    gotPatchChangeIndication = true;
                    return EXIT_SUCCESS;

                case 0x05:
                    printf("Got unknown sub-command 0x05 - possibly assigned voices\n");
                    return EXIT_SUCCESS;

                case 0x04:
                    printf("Got unknown sub-command 0x04\n");
                    return EXIT_SUCCESS;

                case 0x29:
                    printf("Got performance name\n");
                    return EXIT_SUCCESS;

                default:
                    printf("Got 0x04 Unknown sub-command 0x%02x\n", subCommand);
                    return EXIT_FAILURE;
            }
        case 0x08:

            switch (subCommand) {
                case 0x36:
                    printf("Got Patch Version\n");
                    printf("Val 1 0x%02x\n", read_bit_stream(buff, bitPos, 8));
                    printf("Val 2 0x%02x\n", read_bit_stream(buff, bitPos, 8));
                    printf("Val 3 0x%02x\n", read_bit_stream(buff, bitPos, 8));
                    return EXIT_SUCCESS;

                case 0x21:
                    printf("Got Patch info\n");
                    parse_patch(&buff[BIT_TO_BYTE(*bitPos) - 1], (length - BIT_TO_BYTE(*bitPos) - CRC_BYTES) + 1);
                    return EXIT_SUCCESS;

                case 0x27:
                    printf("Got Patch name (length %d)'", length);

                    for (int i = 0; i < (length - 6); i++) {
                        uint8_t ch = read_bit_stream(buff, bitPos, 8);

                        //printf("<0x%02x> ", ch);
                        if (ch <= 0x7F) {         // Only print valid ASCII characters - should be 16 chars max = length of 22
                            printf("%c", ch);
                        }
                    }

                    printf("'\n");
                    return EXIT_SUCCESS;

                case 0x7F:
                    //printf("Got 0x7f OK\n");
                    return EXIT_SUCCESS;

                default:
                    printf("Got 0x08 Unknown sub-command 0x%02x\n", subCommand);
                    return EXIT_FAILURE;
            }
        default:
            printf("Got Unknown command response 0x%02x\n", commandResponse);
            return EXIT_FAILURE;
    }
}

static int parse_incoming(uint8_t * buff, int length) {
    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }
    uint32_t bitPos       = 0;
    uint8_t  responseType = read_bit_stream(buff, &bitPos, 8);

    switch (responseType) {
        case RESPONSE_TYPE_INIT:
            printf("Got Response init\n");
            return EXIT_SUCCESS;

        case RESPONSE_TYPE_COMMAND:
        {
            uint8_t commandResponse = read_bit_stream(buff, &bitPos, 8);
            uint8_t version         = read_bit_stream(buff, &bitPos, 8);
            uint8_t subCommand      = read_bit_stream(buff, &bitPos, 8);

            return parse_command_response(buff, &bitPos, commandResponse, subCommand, length);
        }

        default:
            printf("Got Unknown response type 0x%02x\n", responseType);
            return EXIT_FAILURE;
    }
}

static int rcv_extended(int dataLength) {
    int      retVal                      = EXIT_FAILURE;
    uint32_t bitPos                      = 0;
    uint8_t  buff[EXTENDED_MESSAGE_SIZE] = {0};    // Could malloc this based on dataLength
    int      readLength                  = 0;
    int      tries                       = 0;
    uint8_t  responseType                = 0;

    for (tries = 0; tries < 5; tries++) {
        memset(buff, 0, sizeof(buff));
        readLength = read_usb_extended(buff, sizeof(buff));

        if (readLength > 0) {
            responseType = buff[0];

            if ((responseType == RESPONSE_TYPE_INIT) || (responseType == RESPONSE_TYPE_COMMAND)) {
                break;
            }
        }
    }

    if (readLength == dataLength) {
        bitPos = SIGNED_BYTE_TO_BIT(dataLength - 2);

        if (calc_crc16(buff, dataLength - 2) == read_bit_stream(buff, &bitPos, 16)) {
            retVal = parse_incoming(buff, dataLength);
        } else {
            printf("Bad CRC on ext message\n");
            retVal = -1;
        }
    } else {
        printf("Read ext problem! Read length = %d, data length = %d\n", readLength, dataLength);
    }
    return retVal;
}

static int int_rec(void) {
    int      retVal                       = EXIT_FAILURE;
    uint32_t bitPos                       = 0;
    uint8_t  buff[INTERRUPT_MESSAGE_SIZE] = {0};
    int      readLength                   = 0;
    int      dataLength                   = 0;
    int      type                         = 0;
    int      i                            = 0;
    bool     foundNoneZero                = false;


    memset(buff, 0, sizeof(buff));
    readLength = read_usb_interrupt(buff, sizeof(buff));

    if (readLength > 0) {
        dataLength = read_bit_stream(buff, &bitPos, 4);
        type       = read_bit_stream(buff, &bitPos, 4);

        if (type == RESPONSE_TYPE_EXTENDED) {
            // Work-around for what looks like a bug on Apple silicon only - int response looks like a
            // ext response, so check if anything other than 0s from 4th byte and don't process
            // if it's not all 0
            for (i = 3; i < readLength; i++) {
                if (buff[i] != 0) {
                    foundNoneZero = true;
                    break;
                }
            }

            if (foundNoneZero == false) {
                // Extended message expected
                dataLength = read_bit_stream(buff, &bitPos, 16);

                retVal = rcv_extended(dataLength);
            }
        } else if (type == RESPONSE_TYPE_EMBEDDED) {
            // If Embedded, can call the process incoming, but skip the first byte
            retVal = parse_incoming(buff + 1, dataLength);
        }
    } else if (readLength < 0) {
        gotBadConnectionIndication = true;
    }
    return retVal;
}

static int send_command(int state) {
    int      retVal                  = EXIT_FAILURE;
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint16_t crc                     = 0;
    int      msgLength               = 0;
    int      pos                     = COMMAND_OFFSET;
    uint32_t slot                    = 0;

    switch (state) {
        case eStateInit:
            buff[pos++] = 0x80;
            break;

        case eStateStop:
        case eStateStart:
        case eStateSelectSlot:
        case eStateGetPatchVersion:
        case eStateGetSynthSettings:
        case eStateGetUnknown1:
        case eStateGetUnknown2:
        case eStateGetPatchSlot:
        case eStateGetPatchNameSlot:
            buff[pos++] = 0x01;

            switch (state) {
                case eStateStop:
                case eStateStart:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x7d;

                    switch (state) {
                        case eStateStop:
                            buff[pos++] = 0x01;
                            break;

                        case eStateStart:
                            buff[pos++] = 0x00;
                            break;

                        default:
                            printf("Unknown state %d\n", state);
                            break;
                    }
                    break;

                case eStateSelectSlot:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x00;
                    buff[pos++] = 0x09;     // Sub command
                    buff[pos++] = slot;
                    break;

                case eStateGetPatchVersion:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x35;
                    buff[pos++] = slot;     // Slot 0=A
                    break;

                case eStateGetSynthSettings:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x02;
                    break;

                case eStateGetUnknown1:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = 0x81;
                    break;

                case eStateGetUnknown2:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x00;
                    buff[pos++] = 0x59;
                    break;

                case eStateGetPatchSlot:
                    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | 0;
                    buff[pos++] = slotVersion[slot];
                    buff[pos++] = 0x3c | slot;
                    break;

                case eStateGetPatchNameSlot:
                    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | 0;
                    buff[pos++] = slotVersion[slot];
                    buff[pos++] = 0x28 | slot;
                    break;

                default:
                    printf("Unknown state %d\n", state);
                    break;
            }
            break;

        default:
            printf("Unknown state %d\n", state);
            break;
    }
    msgLength = pos - COMMAND_OFFSET;

    if (msgLength > 0) {
        crc = calc_crc16(&buff[COMMAND_OFFSET], msgLength);
        write_uint16(&buff[msgLength + 2], crc);
        msgLength += 4;

        write_uint16(&buff[0], msgLength);

        if (write_usb(buff, msgLength) > 0) {
            retVal = EXIT_SUCCESS;
        }
    }
    return retVal;
}

static int send_write_data(tMessageContent * messageContent) {
    int      retVal                  = EXIT_FAILURE;
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint16_t crc                     = 0;
    int      msgLength               = 0;
    int      pos                     = COMMAND_OFFSET;
    uint32_t slot                    = 0;

    switch (messageContent->cmd) {
        case eMsgCmdSetValue:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | slot; //+slot
            buff[pos++] = slotVersion[slot];                           // needs to be slot ultimately
            buff[pos++] = SUB_COMMAND_SET_PARAM;
            buff[pos++] = messageContent->paramData.moduleKey.location;
            buff[pos++] = messageContent->paramData.moduleKey.index;
            buff[pos++] = messageContent->paramData.param;
            buff[pos++] = messageContent->paramData.value;
            buff[pos++] = messageContent->paramData.variation;     // variation
            break;

        case eMsgCmdSetMode:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot; //+slot
            buff[pos++] = slotVersion[slot];                 // needs to be slot ultimately
            buff[pos++] = SUB_COMMAND_SET_MODE;
            buff[pos++] = messageContent->modeData.moduleKey.location;
            buff[pos++] = messageContent->modeData.moduleKey.index;
            buff[pos++] = messageContent->modeData.mode;
            buff[pos++] = messageContent->modeData.value;
            printf("SET MODE %u %u\n", messageContent->modeData.mode, messageContent->modeData.value);
            break;

        case eMsgCmdWriteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;                                                          //+slot
            buff[pos++] = slotVersion[slot];
            buff[pos++] = SUB_COMMAND_WRITE_CABLE;
            buff[pos++] = 0x10 | (messageContent->cableData.location << 3) | 0x00;                                    // unknown, location so 0x00 = fx and 0x08 = va, then 3 bits for colour
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex; // top 2 bits = from type, 01 = output
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;                                               // top 2 bits = to type, 01 = output - always an input in our case if we've done things right!?

            // + extra data for potential module updates etc!?
            break;

        case eMsgCmdWriteModule:
            printf("Writing module!\n");
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot; //+slot
            buff[pos++] = slotVersion[slot];                 // needs to be slot ultimately
            buff[pos++] = SUB_COMMAND_WRITE_MODULE;
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
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot; //+slot
            buff[pos++] = slotVersion[slot];                 // needs to be slot ultimately
            buff[pos++] = SUB_COMMAND_MOVE_MODULE;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.location;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.index;
            buff[pos++] = messageContent->moduleMoveData.column;
            buff[pos++] = messageContent->moduleMoveData.row;
            break;

        case eMsgCmdDeleteModule:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot; //+slot
            buff[pos++] = slotVersion[slot];                 // needs to be slot ultimately
            buff[pos++] = SUB_COMMAND_DELETE_MODULE;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.location;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.index;
            break;

        case eMsgCmdSetModuleUpRate:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot; //+slot
            buff[pos++] = slotVersion[slot];                 // needs to be slot ultimately
            buff[pos++] = SUB_COMMAND_SET_MODULE_UPRATE;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.location;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.index;
            buff[pos++] = messageContent->moduleMoveData.upRate;
            break;

        case eMsgCmdDeleteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;                                                          //+slot
            buff[pos++] = slotVersion[slot];
            buff[pos++] = SUB_COMMAND_DELETE_CABLE;
            buff[pos++] = 0x2 | messageContent->cableData.location;                                                   // 1 = synth sectio / VA
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex; // top 2 bits = from type, 01 = output
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;                                               // top 2 bits = to type, 01 = output - always an input in our case if we've done things right!?
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

        if (write_usb(buff, msgLength) > 0) {
            retVal = EXIT_SUCCESS;
        }
    }
    return retVal;
}

static void state_handler(void) {
    static eState   state          = eStateFindDevice;
    tMessageContent messageContent = {0};

    switch (state) {
        case eStateFindDevice:

            if (open_usb() == EXIT_SUCCESS) {
                state = eStateInit;
            } else {
                usleep(100000);
            }
            break;

        case eStateInit:
        case eStateStop:
        case eStateSelectSlot:
        case eStateGetPatchVersion:
        case eStateGetPatchSlot:
        case eStateGetPatchNameSlot:
        case eStateGetSynthSettings:
        case eStateGetUnknown1:
        case eStateGetUnknown2:
        case eStateStart:

            if (send_command(state) == EXIT_SUCCESS) {
                if (int_rec() == EXIT_SUCCESS) {
                    if (state == eStateStart) {
                        call_full_patch_change_notify();
                        call_wake_glfw();
                    }
                    state += 1;
                }
            }
            break;

        case eStatePoll:

            // if got a command in queue, do that, otherwise do the int_rec();
            if (msg_receive(&gCommandQueue, eRcvPoll, &messageContent) == EXIT_SUCCESS) {
                send_write_data(&messageContent);
            } else {
                int_rec();
            }
            break;

        default:
            printf("State unknown %d\n", state);
            usleep(1000);
            break;
    }

    if (gotBadConnectionIndication == true) {
        state                      = eStateFindDevice;
        gotBadConnectionIndication = false;
    }

    if (gotPatchChangeIndication == true) {
        database_clear_cables();
        database_clear_modules();

        // Notify main graphics module that we've removed the module database
        call_full_patch_change_notify();
        call_wake_glfw();

        state = eStateGetPatchVersion;

        gotPatchChangeIndication = false;
    }
}

static void usb_comms_signal_handler(int sigraised) {
    // ToDo - deal with signals properly
    printf("\nUSBComms Sig Handler!!! %d\n", sigraised);

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

    reset_usb();

    for ( ; ;) {
        state_handler();
    }
}

void start_usb_thread(void) {
    if (pthread_create(&usbThread, NULL, usb_thread_loop, 0) != EXIT_SUCCESS) {
        exit(EXIT_FAILURE);
    }
}

#ifdef __cplusplus
}
#endif
