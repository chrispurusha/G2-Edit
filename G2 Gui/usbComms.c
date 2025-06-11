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
#include "globalVars.h"

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

static bool      gotBadConnectionIndication = false;
static bool      gotPatchChangeIndication   = false;
static uint8_t   slotVersion[MAX_SLOTS]     = {0};
static pthread_t usbThread                  = NULL;
static void      (*wake_glfw_func_ptr)(void) = NULL;
static void      (*full_patch_change_notify_func_ptr)(void) = NULL;

void register_glfw_wake_cb(void ( *func_ptr )(void)) {
    wake_glfw_func_ptr = func_ptr;
}

void register_full_patch_change_notify_cb(void ( *func_ptr )(void)) {
    full_patch_change_notify_func_ptr = func_ptr;
}

static void call_wake_glfw(void) {
    if (wake_glfw_func_ptr == NULL) {
        LOG_ERROR("Wake GLFW callback function not registered\n");
        exit(1);
    }
    wake_glfw_func_ptr();
}

static void call_full_patch_change_notify(void) {
    if (full_patch_change_notify_func_ptr == NULL) {
        LOG_ERROR("Full patch change callback function not registered\n");
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
    LOG_DEBUG("Local on (bit 0)0x%x\n", read_bit_stream(buff, &bitPos, 1));     //Actually top bit = 0x80
    read_bit_stream(buff, &bitPos, 7);
    read_bit_stream(buff, &bitPos, 6);
    LOG_DEBUG("Prog Change Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    LOG_DEBUG("Prog Change Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));
    read_bit_stream(buff, &bitPos, 6);
    LOG_DEBUG("Controllers Rcv 0x%x\n", read_bit_stream(buff, &bitPos, 1));     // send actually bit 0
    LOG_DEBUG("Controllers Snd 0x%x\n", read_bit_stream(buff, &bitPos, 1));     // send actually bit 0
    LOG_DEBUG("Send Clock (bit 1), ignore ext clock (bit 2) , 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Tune cent 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global Shift Active (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Global Octave Shift 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Tune semi 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Filler 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    LOG_DEBUG("Pedal Polarity (bit 0) 0x%x\n", read_bit_stream(buff, &bitPos, 8));     //0xC0 (1100) = closed, 0x40 (0100) = open therefore bit 7
    LOG_DEBUG("Control Pedal Gain 0x%x\n", read_bit_stream(buff, &bitPos, 8));
    //17 other unknown bytes to go

    retVal = EXIT_SUCCESS;

    return retVal;
}

static void parse_module_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    uint32_t   i      = 0;
    uint32_t   j      = 0;
    uint32_t   type   = 0;
    tModuleKey key    = {0};
    tModule    module = {0};

    LOG_DEBUG("Module list\n");

    key.slot = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_DEBUG("Location       0x%x\n", key.location);     // Discerns between FX and main, could put in the module itself
    uint32_t moduleCount = read_bit_stream(buff, subOffset, 8);
    LOG_DEBUG("Module Count   %d\n", moduleCount);

    for (i = 0; i < moduleCount; i++) {
        type      = read_bit_stream(buff, subOffset, 8);
        key.index = read_bit_stream(buff, subOffset, 8);

        if (read_module(key, &module) == true) {
            LOG_DEBUG("Module already created\n");
        }
        module.type      = type;
        module.column    = read_bit_stream(buff, subOffset, 7);        // 7
        module.row       = read_bit_stream(buff, subOffset, 7);        // 7
        module.colour    = read_bit_stream(buff, subOffset, 8);        // 8
        module.upRate    = read_bit_stream(buff, subOffset, 1);        // 1
        module.isLed     = read_bit_stream(buff, subOffset, 1);        // 1
        module.unknown1  = read_bit_stream(buff, subOffset, 6);        // 6
        module.modeCount = read_bit_stream(buff, subOffset, 4);        // 4

        LOG_DEBUG("Module type %u\n", module.type);
        LOG_DEBUG("Module column %u\n", module.column);
        LOG_DEBUG("Module row %u\n", module.row);

        for (j = 0; j < module.modeCount; j++) {
            module.mode[j].value = read_bit_stream(buff, subOffset, 6);
            LOG_DEBUG("Mode index %u = %u\n", j, module.mode[j].value);
            LOG_DEBUG("MODE %u %u\n", j, module.mode[j].value);
        }

        LOG_DEBUG("Number connectors for module %u\n", module_connector_count(type));
        write_module(key, &module);
    }
}

static void parse_cable_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tCableKey key   = {0};
    tCable    cable = {0};

    LOG_DEBUG("Cable list\n");

    key.slot = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_DEBUG("Location       0x%x\n", key.location);
    LOG_DEBUG("Unknown        0x%x\n", read_bit_stream(buff, subOffset, 12));
    uint32_t cableCount = read_bit_stream(buff, subOffset, 10);
    LOG_DEBUG("Cable Count    %d\n", cableCount);

    for (uint32_t i = 0; i < cableCount; i++) {
        cable.colour = read_bit_stream(buff, subOffset, 3);
        //LOG_DEBUG(" Colour         0x%x\n", cable.colour);
        key.moduleFromIndex = read_bit_stream(buff, subOffset, 8);         // key will get written into struct on write
        //LOG_DEBUG(" Module From    %d\n", key.moduleFromIndex);
        key.connectorFromIoCount = read_bit_stream(buff, subOffset, 6);
        //LOG_DEBUG(" Connector From %d\n", key.connectorFromIoCount);
        key.linkType = read_bit_stream(buff, subOffset, 1);   // 1 = output to input, 0 = input to input
        //LOG_DEBUG(" Link Type      0x%x\n", key.linkType);
        key.moduleToIndex = read_bit_stream(buff, subOffset, 8);
        //LOG_DEBUG(" Module To      %d\n", key.moduleToIndex);
        key.connectorToIoCount = read_bit_stream(buff, subOffset, 6);
        //LOG_DEBUG(" Connector To   %d\n", key.connectorToIoCount);

        write_cable(key, &cable);
    }
}

static void parse_param_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    uint32_t   variationCount = 0;
    uint32_t   paramCount     = 0;
    uint32_t   moduleCount    = 0;
    uint32_t   paramValue     = 0;
    tModuleKey key            = {0};
    tModule    module         = {0};
    int        i              = 0;
    int        j              = 0;
    int        k              = 0;

    LOG_DEBUG("Param list\n");
    key.slot = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_DEBUG("Location       0x%x\n", key.location);     // 0..1 = param list, 2 = patch settings!?
    // SWITCH ON LOC BEING 0..1 or 2 2 = line 4112 in file.pas
    moduleCount = read_bit_stream(buff, subOffset, 8);
    LOG_DEBUG("Module Count      %u\n", moduleCount);
    variationCount = read_bit_stream(buff, subOffset, 8);     // Should always be 10 (VARIATIONS) - TODO: sanity check
    LOG_DEBUG("Variation Count      %u\n", variationCount);

    for (i = 0; i < moduleCount; i++) {
        key.index = read_bit_stream(buff, subOffset, 8);
        LOG_DEBUG(" Module Index        %u\n", key.index);

        paramCount = read_bit_stream(buff, subOffset, 7);
        LOG_DEBUG("  variation list param count = %u\n", paramCount);

        if (paramCount >= MAX_NUM_PARAMETERS) {
            LOG_ERROR("MAX_NUM_PARAMETERS needs increasing to >= %u\n", paramCount + 1);
            exit(1);
        }

        if (read_module(key, &module) == false) {
            module.key = key;
        }
        
        if ((module.type != moduleTypeUnknown0) && (module_param_count(module.type) > 0) && (paramCount != module_param_count(module.type))) {
            LOG_ERROR("Incorrect number of parameters on module %u %s count from G2 = %u, our structures = %u\n",  module.type, gModuleProperties[module.type].name, paramCount, module_param_count(module.type));
            exit(1);
        }

        for (j = 0; j < variationCount; j++) {                                                          // 0 to 9, but last 2 not available on old editor. Possibly/probably init values?
            uint32_t variation = read_bit_stream(buff, subOffset, 8);

            if (variation == 0) { // Limit to just 1st variation for now
                LOG_DEBUG("  Variation %u\n", variation);
            }

            if (j != variation) {
                LOG_WARNING("loop var %u != variation %u\n", j, variation);
            }

            for (k = 0; k < paramCount; k++) {
                paramValue = read_bit_stream(buff, subOffset, 7);

                if (variation == 0) { // Limit to just 1st variation for now
                    LOG_DEBUG("   Param number %02d param value %02d\n", k, paramValue);
                }
                module.param[j][k].value = paramValue;
            }
        }

        write_module(key, &module);         // Careful with type 2, morphs!
    }
}

static void parse_param_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset, int count) {
    //uint32_t location = 0;
    uint32_t   nameCount    = 0;
    uint32_t   paramLength  = 0;
    uint32_t   moduleLength = 0;
    tModule    module       = {0};
    tModuleKey key          = {0};
    int        i            = 0;
    int        j            = 0;
    int        labelIndex   = 0;
    int        k            = 0;
    int        variation    = 0;
    uint32_t   isString     = 0;
    uint32_t   paramIndex   = 0;
    uint32_t   numLabels    = 0;

    LOG_DEBUG("Param names\n");

    key.slot = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_DEBUG("Location       0x%x\n", key.location);
    //LOG_DEBUG("Unknown        %d\n", read_bit_stream(buff, subOffset, 6));
    nameCount = read_bit_stream(buff, subOffset, 8);
    LOG_DEBUG("NameCount      %d\n", nameCount);
    //LOG_DEBUG("Module count      %d\n", read_bit_stream(buff, subOffset, 8));

    // SWITCH ON LOC BEING 0..1 or 2
    for (i = 0; i < nameCount; i++) {
        key.index = read_bit_stream(buff, subOffset, 8);
        LOG_DEBUG("Module index      %d\n", key.index);

        if (read_module(key, &module) == false) {
            write_module(key, &module);
        }
        moduleLength = read_bit_stream(buff, subOffset, 8);
        LOG_DEBUG("Module length     %d\n", moduleLength);         // 5004

        for (j = 0; j < moduleLength;) {
            isString = read_bit_stream(buff, subOffset, 8);
            LOG_DEBUG("IsString     %d\n", isString);
            paramLength = read_bit_stream(buff, subOffset, 8);
            LOG_DEBUG("ParamLen     %d\n", paramLength);
            paramIndex = read_bit_stream(buff, subOffset, 8);
            LOG_DEBUG("Param Index  %d\n", paramIndex);
            j += 3;
            LOG_DEBUG("Param name: ");

            if (paramLength > 0) {
                numLabels = (paramLength - 1) / PROTOCOL_PARAM_NAME_SIZE;


                memset(&module.paramName[paramIndex], 0, sizeof(module.paramName[paramIndex]));

                for (labelIndex = 0; labelIndex < numLabels; labelIndex++) {
                    for (k = 0; k < PROTOCOL_PARAM_NAME_SIZE; k++) {
                        uint8_t ch = read_bit_stream(buff, subOffset, 8);

                        if ((ch >= 0x20) && (ch <= 0x7f)) {
                            LOG_DEBUG_DIRECT("%c", ch);
                        }

                        for (variation = 0; variation < NUM_VARIATIONS; variation++) {
                            module.paramName[paramIndex][k] = ch;
                        }
                    }
                }

                LOG_DEBUG_DIRECT("\n");
                j += paramLength - 1;
            }
        }

        write_module(key, &module);
    }
}

static void parse_module_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tModule    module = {0};
    tModuleKey key    = {0};
    uint32_t   i      = 0;
    char       name[MODULE_NAME_SIZE + 1];


    LOG_DEBUG("Module names\n");

    key.slot = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    read_bit_stream(buff, subOffset, 6);
    LOG_DEBUG("Location 0x%x\n", key.location);
    uint32_t items = read_bit_stream(buff, subOffset, 8);
    LOG_DEBUG("Items %u\n", items);

    for (i = 0; i < items; i++) {
        key.index = read_bit_stream(buff, subOffset, 8);
        LOG_DEBUG(" Module Name Index %u\n", key.index);

        LOG_DEBUG(" Module loc %u index %u\n", module.key.location, module.key.index);

        memset(&name, 0, sizeof(name));

        for (int k = 0; k < MODULE_NAME_SIZE; k++) {
            name[k] = read_bit_stream(buff, subOffset, 8);

            if (name[k] == '\0') {
                break;
            }
        }

        LOG_DEBUG("%s\n", name);

        if (read_module(key, &module) == true) {
            strncpy(module.name, name, sizeof(module.name));
            module.name[sizeof(module.name) - 1] = '\0';
            write_module(key, &module);
        }
    }
}

static void parse_morph_params(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    // line 3754 in file.pas
    tModule    module          = {0};
    tModuleKey key             = {0};
    uint32_t   variationCount  = 0;
    uint32_t   variation       = 0;
    uint32_t   morphCount      = 0;
    uint32_t   morphParamCount = 0;
    uint32_t   paramIndex      = 0;
    uint32_t   morph           = 0;
    uint32_t   range           = 0;
    int        j               = 0;
    int        k               = 0;
    uint32_t   bitsLeft        = 0;

    variationCount = read_bit_stream(buff, subOffset, 8);
    morphCount     = read_bit_stream(buff, subOffset, 4);
    read_bit_stream(buff, subOffset, 20);  // Reserved data

    LOG_DEBUG("Variations %u Morphs %u\n", variationCount, morphCount);

    for (j = 0; j < variationCount; j++) {    // 0 to 9, but last 2 not available on old editor. Possibly/probably init values?
        variation = read_bit_stream(buff, subOffset, 4);
        read_bit_stream(buff, subOffset, 4);  // Lots of unknown stuff
        read_bit_stream(buff, subOffset, 8);
        read_bit_stream(buff, subOffset, 8);
        read_bit_stream(buff, subOffset, 8);
        read_bit_stream(buff, subOffset, 8);
        read_bit_stream(buff, subOffset, 8);
        read_bit_stream(buff, subOffset, 8);
        read_bit_stream(buff, subOffset, 4);
        morphParamCount = read_bit_stream(buff, subOffset, 8);
        LOG_DEBUG("Variation %u Morph param count %u\n", variation, morphParamCount);

        for (k = 0; k < morphParamCount; k++) {
            key.slot = slot;
            key.location = read_bit_stream(buff, subOffset, 2);
            key.index    = read_bit_stream(buff, subOffset, 8);
            paramIndex   = read_bit_stream(buff, subOffset, 7);
            morph        = read_bit_stream(buff, subOffset, 4);
            range        = read_bit_stream(buff, subOffset, 8);

            LOG_DEBUG("  Location %u\n", key.location);
            LOG_DEBUG("  Module index %u\n", key.index);
            LOG_DEBUG("  Param index %u\n", paramIndex);
            LOG_DEBUG("  Morph %u\n", morph);
            LOG_DEBUG("  Range %u\n", range);

            if (read_module(key, &module) == false) {
                write_module(key, &module);
            }
            module.param[j][paramIndex].morphRange[morph] = range;

            write_module(key, &module);
        }

        read_bit_stream(buff, subOffset, 4);

        // Supposedly, if what's left is < 4 bits, we should read the remaining bits only. Shouldn't happen on this system since we step over block
    }
}

static void parse_knobs(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    // line 4268 in file.pas

    tModuleKey key        = {0};
    uint32_t   knobCount  = 0;
    uint32_t   isLed      = 0;
    uint32_t   paramIndex = 0;
    uint32_t   assigned   = 0;
    int        i          = 0;


    knobCount = read_bit_stream(buff, subOffset, 16);
    LOG_DEBUG("  Knob Count %u\n", knobCount);

    for (i = 0; i < knobCount; i++) {
        assigned = read_bit_stream(buff, subOffset, 1);

        if (assigned == 1) {
            key.slot = slot;
            key.location = read_bit_stream(buff, subOffset, 2);
            key.index    = read_bit_stream(buff, subOffset, 8);
            isLed        = read_bit_stream(buff, subOffset, 2);
            paramIndex   = read_bit_stream(buff, subOffset, 7);

            LOG_DEBUG("Knob %d\n", i);
            LOG_DEBUG("  Module Location %u\n", key.location);
            LOG_DEBUG("  Module Index %u\n", key.index);
            LOG_DEBUG("  IsLed %u\n", isLed);
            LOG_DEBUG("  Param Index %u\n", paramIndex);
        }
    }
}

static void parse_patch_descr(uint8_t * buff, uint32_t * subOffset) {
    uint32_t barPosition     = 0;
    uint32_t voiceCount      = 0;
    uint32_t redVisible      = 0;
    uint32_t blueVisible     = 0;
    uint32_t yellowVisible   = 0;
    uint32_t orangeVisible   = 0;
    uint32_t greenVisible    = 0;
    uint32_t purpleVisible   = 0;
    uint32_t whiteVisible    = 0;
    uint32_t monoPoly        = 0;
    uint32_t activeVariation = 0;
    uint32_t category        = 0;

    read_bit_stream(buff, subOffset, 61);  // Unknown - may need to write to file or back to synth?
    voiceCount  = read_bit_stream(buff, subOffset, 5);
    barPosition = read_bit_stream(buff, subOffset, 14);
    read_bit_stream(buff, subOffset, 3);   // Unknown - may need to write to file or back to synth?
    redVisible      = read_bit_stream(buff, subOffset, 1);
    blueVisible     = read_bit_stream(buff, subOffset, 1);
    yellowVisible   = read_bit_stream(buff, subOffset, 1);
    orangeVisible   = read_bit_stream(buff, subOffset, 1);
    greenVisible    = read_bit_stream(buff, subOffset, 1);
    purpleVisible   = read_bit_stream(buff, subOffset, 1);
    whiteVisible    = read_bit_stream(buff, subOffset, 1);
    monoPoly        = read_bit_stream(buff, subOffset, 2);
    activeVariation = read_bit_stream(buff, subOffset, 8);
    category        = read_bit_stream(buff, subOffset, 8);
    read_bit_stream(buff, subOffset, 12);  // Unknown or padding - may need to write to file or back to synth?

    LOG_DEBUG("  Voice Count %u\n", voiceCount);
    LOG_DEBUG("  Bar Position %u\n", barPosition);
    LOG_DEBUG("  Red %u\n", redVisible);
    LOG_DEBUG("  Blue %u\n", blueVisible);
    LOG_DEBUG("  Yellow %u\n", yellowVisible);
    LOG_DEBUG("  Orange %u\n", orangeVisible);
    LOG_DEBUG("  Green %u\n", greenVisible);
    LOG_DEBUG("  Purple %u\n", purpleVisible);
    LOG_DEBUG("  White %u\n", whiteVisible);
    LOG_DEBUG("  Mono Poly %u\n", monoPoly);
    LOG_DEBUG("  Active Variation %u\n", activeVariation);
    LOG_DEBUG("  Category %u\n", category);

    gVariation = activeVariation;
}

int parse_patch(uint32_t slot, uint8_t * buff, int length) { // TODO: also accessed from file, so need to decide how to access from USB and file
    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }
    uint32_t bitOffset = 0;

    while (BIT_TO_BYTE(bitOffset) < length) {
        uint8_t  type      = read_bit_stream(buff, &bitOffset, 8);
        int16_t  count     = read_bit_stream(buff, &bitOffset, 16);
        uint32_t subOffset = bitOffset;

        LOG_DEBUG("Type = 0x%x, Count = %d\n", type, count);

        switch (type) {
            case SUB_RESPONSE_MODULE_LIST:     // Module list
            {
                parse_module_list(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_CABLE_LIST:     // Cable list
            {
                parse_cable_list(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_PARAM_LIST:     // Param list
            {
                parse_param_list(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_PARAM_NAMES:     // Param names
            {
                parse_param_names(slot, buff, &subOffset, count);
                break;
            }

            case SUB_RESPONSE_MODULE_NAMES:     // Module names
            {
                parse_module_names(slot, buff, &subOffset);
                break;
            }

            case 0x2d:     // Special case (ignore)
            {
                // This 0x2d 0x00 sequence reportedly only appears on USB comms, not in files
                // so ignore as a size by moving back a byte
                count = -1;
                break;
            }

            case SUB_RESPONSE_PATCH_DESCRIPTION: // Not sure we should be getting this, since we're already processing patch description in this function!?
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
                parse_morph_params(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_KNOBS:
            {
                LOG_DEBUG("Knobs\n");

                uint32_t tmpSubOffset = subOffset;

                for (int i = 0; i < 32; i++) {
                    LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, &subOffset, 8));
                }

                LOG_DEBUG_DIRECT("\n");
                subOffset = tmpSubOffset;
                parse_knobs(slot, buff, &subOffset);
                break;
            }

            case SUB_RESPONSE_CONTROLLERS:
            {
                LOG_DEBUG("Controllers\n");

                uint32_t tmpSubOffset = subOffset;

                for (int i = 0; i < 32; i++) {
                    LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, &subOffset, 8));
                }

                LOG_DEBUG_DIRECT("\n");
                subOffset = tmpSubOffset;
                break;
            }
                
            case SUB_RESPONSE_CURRENT_NOTE_2:
            {
                LOG_DEBUG("Current note 2\n");
                break;
            }

            case SUB_RESPONSE_PATCH_NOTES:
            {
                LOG_DEBUG("Patch notes\n");
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

    LOG_DEBUG("Param change - module %u:%u param = %u value = %u\n", key.location, key.index, param, value);
}

static int parse_command_response(uint8_t * buff, uint32_t * bitPos, uint8_t commandResponse, uint8_t subCommand, int length) {
    tModule  module = {0};
    uint32_t slot   = commandResponse & 0x03;

    // Not sure higher bits in commandResponse byte have much practical effect
    //LOG_DEBUG("Slot %u\n", slot);

    switch (subCommand) {
        case SUB_RESPONSE_VOLUME_INDICATOR:
        {
            //{
            //    uint32_t tmpBitPos = *bitPos;
            //
            //    for (int i = 4; i < (length - 2); i++) {
            //        LOG_DEBUG_DIRECT("0x%02x ", read_bit_stream(buff, bitPos, 8));
            //    }
            //    LOG_DEBUG_DIRECT("\n");
            //    *bitPos = tmpBitPos;
            //}

            read_bit_stream(buff, bitPos, 8); // dummy - not sure what it does

            for (int32_t location = 1; location >= 0; location--) {
                for (int k = 0; k <= 255; k++) {
                    module.key.slot = slot;
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
                            //LOG_DEBUG("Module loc %u index %u vol %u %u\n", module.key.location, module.key.index, module.volume.value1, module.volume.value2);
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

            //LOG_DEBUG("LED ");
            //for (i = 4; i < (length-2); i++)
            //    LOG_DEBUG("0x%02x ", buff[i]);
            //LOG_DEBUG("\n");

            read_bit_stream(buff, bitPos, 8); // Seems to be a byte of padding

            for (int k = 0; k <= 255; k++) {
                module.key.slot = slot;
                module.key.location = gLocation;
                module.key.index    = k;

                if (read_module(module.key, &module) == true) {
                    if (gModuleProperties[module.type].ledType == ledTypeYes) {
                        module.led.value = read_bit_stream(buff, bitPos, 1);
                        read_bit_stream(buff, bitPos, 1); // Not sure if this is used for anything yet, might just be padding

                        //LOG_DEBUG("Module %u LED %u\n", module.key.index, module.led);
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
            parse_param_change(&buff[BIT_TO_BYTE(*bitPos)], length - BIT_TO_BYTE(*bitPos) - CRC_BYTES);
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
            LOG_DEBUG("Got MIDI cc response\n");
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_GLOBAL_PAGE:
        {
            LOG_DEBUG("Got Global page\n");
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_PATCH_VERSION_CHANGE:
        {
            LOG_DEBUG("Got Patch load\n");
            gotPatchChangeIndication = true;
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
            uint32_t slot = 0;
            
            LOG_DEBUG("Got slot select\n");
            slot = read_bit_stream(buff, bitPos, 8);
            
            gSlot = slot;
            
            for (uint32_t i = 0; i < MAX_SLOTS; i++) {
                gMainButtonArray[(uint32_t)slotAButtonId + i].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
            }

            gMainButtonArray[slotAButtonId + slot].backgroundColour = (tRgb)RGB_GREEN_ON;
            
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
            LOG_DEBUG("Got Patch name (length %d)'", length);

            for (int i = 0; i < (length - 6); i++) {
                uint8_t ch = read_bit_stream(buff, bitPos, 8);

                //LOG_DEBUG("<0x%02x> ", ch);
                if (ch <= 0x7F) {         // Only print valid ASCII characters - should be 16 chars max = length of 22
                    LOG_DEBUG_DIRECT("%c", ch);
                }
            }

            LOG_DEBUG_DIRECT("'\n");
            return EXIT_SUCCESS;
        }

        case SUB_RESPONSE_OK:
        {
            //LOG_DEBUG("Got 0x7f OK\n");
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
    call_wake_glfw(); // Assume something happened, so we want to re-draw
    return ret;
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
            LOG_DEBUG("Bad CRC on ext message\n");
            retVal = -1;
        }
    } else {
        LOG_DEBUG("Read ext problem! Read length = %d, data length = %d\n", readLength, dataLength);
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
    
    switch (state) {
        case eStateInit: // Apparently resets the version numbers
            buff[pos++] = 0x80;
            break;

        case eStateStop:
        case eStateStart:
        case eStateSelectSlot:
        case eStateGetPatchVersion:
        case eStateGetSynthSettings:
        case eStateGetUnknown1:
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
                    buff[pos++] = 0; // Slot
                    break;

                case eStateGetPatchVersion:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = SUB_COMMAND_GET_PATCH_VERSION;  // TODO: each slot!?
                    buff[pos++] = 0;     // Slot 0=A
                    break;

                case eStateGetSynthSettings:
                    buff[pos++] = COMMAND_REQ | COMMAND_SYS;
                    buff[pos++] = 0x41;
                    buff[pos++] = SUB_COMMAND_GET_SYNTH_SETTINGS;
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

                case eStateGetPatchSlotA:
                case eStateGetPatchSlotB:
                case eStateGetPatchSlotC:
                case eStateGetPatchSlotD:
                {
                    uint32_t slot = state - eStateGetPatchSlotA;
                    
                    buff[pos++] = COMMAND_REQ | COMMAND_SLOT | slot;
                    buff[pos++] = slotVersion[slot];
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
                    buff[pos++] = slotVersion[slot];
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
    //uint32_t slot                    = 0;   // TODO: pass this via the message structure

    switch (messageContent->cmd) {
        case eMsgCmdSetValue:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_PARAM;
            buff[pos++] = messageContent->paramData.moduleKey.location;
            buff[pos++] = messageContent->paramData.moduleKey.index;
            buff[pos++] = messageContent->paramData.param;
            buff[pos++] = messageContent->paramData.value;
            buff[pos++] = messageContent->paramData.variation;     // variation
            break;

        case eMsgCmdSetMode:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion[messageContent->slot];
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
            buff[pos++] = slotVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_WRITE_CABLE;
            buff[pos++] = 0x10 | (messageContent->cableData.location << 3) | 0x00;                                    // unknown, location so 0x00 = fx and 0x08 = va, then 3 bits for colour
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex; // top 2 bits = from type, 01 = output
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;                                               // top 2 bits = to type, 01 = output - always an input in our case if we've done things right!?

            // + extra data for potential module updates etc!?
            break;

        case eMsgCmdWriteModule:
            LOG_DEBUG("Writing module!\n");
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion[messageContent->slot];
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
            buff[pos++] = slotVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_MOVE_MODULE;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.location;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.index;
            buff[pos++] = messageContent->moduleMoveData.column;
            buff[pos++] = messageContent->moduleMoveData.row;
            break;

        case eMsgCmdDeleteModule:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_DELETE_MODULE;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.location;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.index;
            break;

        case eMsgCmdSetModuleUpRate:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_SET_MODULE_UPRATE;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.location;
            buff[pos++] = messageContent->moduleMoveData.moduleKey.index;
            buff[pos++] = messageContent->moduleMoveData.upRate;
            break;

        case eMsgCmdDeleteCable:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_REQ | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion[messageContent->slot];
            buff[pos++] = SUB_COMMAND_DELETE_CABLE;
            buff[pos++] = 0x2 | messageContent->cableData.location;
            buff[pos++] = messageContent->cableData.moduleFromIndex;
            buff[pos++] = (messageContent->cableData.linkType << 6) | messageContent->cableData.connectorFromIoIndex; // top 2 bits = from type, 01 = output
            buff[pos++] = messageContent->cableData.moduleToIndex;
            buff[pos++] = messageContent->cableData.connectorToIoIndex;                                               // top 2 bits = to type, 01 = output - always an input in our case if we've done things right!?
            break;

        case eMsgCmdSetParamMorph:
            buff[pos++] = 0x01;
            buff[pos++] = COMMAND_WRITE_NO_RESP | COMMAND_SLOT | messageContent->slot;
            buff[pos++] = slotVersion[messageContent->slot];
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
            buff[pos++] = slotVersion[messageContent->slot];
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

        // 4418 mess.pas for assign/deassign knob
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
        case eStateGetPatchSlotA:
        case eStateGetPatchSlotB:
        case eStateGetPatchSlotC:
        case eStateGetPatchSlotD:
        case eStateGetPatchNameSlotA:
        case eStateGetPatchNameSlotB:
        case eStateGetPatchNameSlotC:
        case eStateGetPatchNameSlotD:
        case eStateGetSynthSettings:
        case eStateGetUnknown1:
        case eStateGetUnknown2:
        case eStateStart:

            if (send_command(state) == EXIT_SUCCESS) {
                if (int_rec() == EXIT_SUCCESS) {
                    if (state == eStateStop) {
                        database_clear_cables();
                        database_clear_modules();

                        // Notify main graphics module that we've removed the module database
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

            // if got a command in queue, do that, otherwise do the int_rec();
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

    if (gotBadConnectionIndication == true) {
        LOG_DEBUG("Bad connection indication\n");
        state                      = eStateFindDevice;
        gotBadConnectionIndication = false;
    }

    if (gotPatchChangeIndication == true) {
        LOG_DEBUG("Patch change indication\n");
        state                    = eStateStop;
        gotPatchChangeIndication = false;
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

    init_usb();

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
