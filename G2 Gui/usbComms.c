/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
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

#include "iokit.h"
#include "utils.h"
#include "defs.h"
#include "msgQueue.h"
#include "usbComms.h"
#include "main.h"
#include "dataBase.h"

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

static bool      gotBadConnectionIndication = false;
static bool      gotPatchChangeIndication   = false;
static uint8_t   slotVersion[MAX_SLOTS]     = {0};
static pthread_t usbThread = NULL;

tMessageQueue gCommandQueue = {0};

unsigned int volData[1024] = {0};    //Temporary for testing. Will ultimately have a better mechanism for passing data
static void  (* wake_glfw_func_ptr)(void) = NULL;
    
extern tModuleProperties gModuleProperties[];

void register_glfw_wake_cb(void (* func_ptr)(void)) {
    wake_glfw_func_ptr = func_ptr;
}

static void call_wake_glfw(void) {
    if (wake_glfw_func_ptr == NULL) {
        printf("Wake GLFW callback function not registered\n");
        exit(1);
    }

    wake_glfw_func_ptr();
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
            mode = read_bit_stream(buff, subOffset, 6);             // Not sure what to do with this yet
        }
        
        // Ultimately do through a database function, so we're not mallocing here.
        if (gModuleProperties[type].numConnectors > 0) {
            //module.numConnectors = gModuleProperties[type].numConnectors;  //
            printf("Type = %s connectors %u\n", gModuleProperties[type].name, gModuleProperties[type].numConnectors);
            module.connector = malloc(gModuleProperties[type].numConnectors * sizeof(tConnector));
            if (module.connector != NULL) {
                memset(module.connector, 0, gModuleProperties[type].numConnectors * sizeof(tConnector));
            }
        }

        printf("Number connectors for module %u\n", gModuleProperties[type].numConnectors);
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
        printf(" Colour         0x%x\n", cable.colour);
        key.moduleFrom = read_bit_stream(buff, subOffset, 8);         // key will get written into struct on write
        printf(" Module From    %d\n", key.moduleFrom);
        key.connectorFrom = read_bit_stream(buff, subOffset, 6);
        printf(" Connector From %d\n", key.connectorFrom);
        cable.linkType = read_bit_stream(buff, subOffset, 1);   // 1 = output to input, 0 = input to input
        printf(" Link Type      0x%x\n", cable.linkType);
        key.moduleTo = read_bit_stream(buff, subOffset, 8);
        printf(" Module To      %d\n", key.moduleTo);
        key.connectorTo = read_bit_stream(buff, subOffset, 6);
        printf(" Connector To   %d\n", key.connectorTo);

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
    int        i = 0;
    int        j = 0;
    int        k = 0;

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

        // See if we can do the mallocing and freeing in the db routines
        module.numParams = paramCount;

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
    uint32_t nameCount    = 0;
    uint32_t paramLength  = 0;
    uint32_t moduleLength = 0;
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

    //reset_walk_module();

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
            memcpy(module.name, name, sizeof(module.name));
            write_module(key, &module);
        }
    }
}

static int parse_patch(uint8_t * buff, int length) {
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
                    //printf("Got Volume ");
                    j = 0;
                    for (i = 4; i < (length - 6); i += 2) {     // Exclude header/footer
                        volData[j] = read_bit_stream(buff, bitPos, 16);
                        //printf("%u ", volData[j]);
                        j++;
                    }
                    //printf("\n");
                    call_wake_glfw();
                    return EXIT_SUCCESS;

                case SUB_COMMAND_LED_DATA:
                    //printf("Got LED data ");
                    //for (i = 4; i < (length - 6); i += 2)
                    //    printf("%u ", read_bit_stream(buff, bitPos, 16));
                    //printf("\n");
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
                    //printf("Got OK\n");
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
                    printf("Got OK\n");
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

        case RESPONSE_TYPE_COMMAND: {
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
    int      retVal = EXIT_FAILURE;
    uint32_t bitPos = 0;
    uint8_t  buff[EXTENDED_MESSAGE_SIZE] = {0};    // Could malloc this based on dataLength
    int      readLength   = 0;
    int      tries        = 0;
    uint8_t  responseType = 0;

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
        }
        else {
            printf("Bad CRC on ext message\n");
            retVal = -1;
        }
    }
    else {
        printf("Read ext problem! Read length = %d, data length = %d\n", readLength, dataLength);
    }

    return retVal;
}

static int int_rec(void) {
    int      retVal = EXIT_FAILURE;
    uint32_t bitPos = 0;
    uint8_t  buff[INTERRUPT_MESSAGE_SIZE] = {0};
    int      readLength    = 0;
    int      dataLength    = 0;
    int      type          = 0;
    int      i             = 0;
    int      tries         = 0;
    bool     foundNoneZero = false;

    for (tries = 0; tries < 5; tries++) {
        memset(buff, 0, sizeof(buff));
        readLength = read_usb_interrupt(buff, sizeof(buff));
        if (readLength > 0) {
            break;
        }
    }

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
        }
        else if (type == RESPONSE_TYPE_EMBEDDED) {
            // If Embedded, can call the process incoming, but skip the first byte
            retVal = parse_incoming(buff + 1, dataLength);
        }
    }
    else if (readLength < 0) {
        gotBadConnectionIndication = true;
    }

    return retVal;
}

static int send_command(int state) {
    int      retVal = EXIT_FAILURE;
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint16_t crc       = 0;
    int      msgLength = 0;
    int      pos       = COMMAND_OFFSET;
    uint32_t slot      = 0;

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

static int send_write_value_command(tMessageContent * messageContent) {
    int      retVal = EXIT_FAILURE;
    uint8_t  buff[SEND_MESSAGE_SIZE] = {0};
    uint16_t crc       = 0;
    int      msgLength = 0;
    int      pos       = COMMAND_OFFSET;
    uint32_t slot      = 0;

    buff[pos++] = 0x01;
    buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | slot; //+slot
    buff[pos++] = slotVersion[slot];                           // needs to be slot ultimately
    buff[pos++] = SUB_COMMAND_SET_PARAM;
    buff[pos++] = messageContent->location;
    buff[pos++] = messageContent->index;
    buff[pos++] = messageContent->param;
    buff[pos++] = messageContent->value;
    buff[pos++] = messageContent->variation;     // variation

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
            }
            else {
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
                    state += 1;
                }
            }
            call_wake_glfw();
            break;

        case eStatePoll:
            // if got a command in queue, do that, otherwise do the int_rec();
            if (msg_receive(&gCommandQueue, eRcvPoll, &messageContent) == EXIT_SUCCESS) {
                send_write_value_command(&messageContent);
            }
            else {
                int_rec();
            }
            break;

        default:
            printf("State unknown %d\n", state);
            usleep(1000);
            break;
    }

    if (gotBadConnectionIndication == true) {
        state = eStateFindDevice;
        gotBadConnectionIndication = false;
    }

    if (gotPatchChangeIndication == true) {
        database_clear_cables();
        database_clear_modules();

        state = eStateGetPatchVersion;
        gotPatchChangeIndication = false;
    }
}

static void * usb_thread_loop(void * arg) {
    msg_init(&gCommandQueue, "command");

    reset_usb();

    for (;;) {
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
