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
#include "utils.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
    
uint32_t get_param_count(tLocation location, uint32_t index, tModuleType type) {
    uint32_t paramCount = 0;
    
    if (location == locationMorph) {
        switch (index) {
            case 1: {
                paramCount = 16;
                break;
            }
            case 2:
            case 3:
            case 4:
            case 7: {
                paramCount = 2;
                break;
            }
            case 5: {
                paramCount = 3;
                break;
            }
            case 6: {
                paramCount = 4;
                break;
            }
            default: {
                break;
            }
        }
    } else {
        paramCount = module_param_count(type);
    }
    
    return paramCount;
}
    
void parse_patch_descr(uint8_t * buff, uint32_t * subOffset) {
    gPatchDescr[gSlot].unknown1        = read_bit_stream(buff, subOffset, 32);
    gPatchDescr[gSlot].unknown2        = read_bit_stream(buff, subOffset, 29);
    gPatchDescr[gSlot].voiceCount      = read_bit_stream(buff, subOffset, 5);
    gPatchDescr[gSlot].barPosition     = read_bit_stream(buff, subOffset, 14);
    gPatchDescr[gSlot].unknown3        = read_bit_stream(buff, subOffset, 3);
    gPatchDescr[gSlot].redVisible      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].blueVisible     = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].yellowVisible   = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].orangeVisible   = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].greenVisible    = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].purpleVisible   = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].whiteVisible    = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].monoPoly        = read_bit_stream(buff, subOffset, 2);
    gPatchDescr[gSlot].activeVariation = read_bit_stream(buff, subOffset, 8);
    gPatchDescr[gSlot].category        = read_bit_stream(buff, subOffset, 8);
    gPatchDescr[gSlot].unknown4        = read_bit_stream(buff, subOffset, 12);

    LOG_DEBUG("  Voice Count %u\n", gPatchDescr[gSlot].voiceCount);
    LOG_DEBUG("  Bar Position %u\n", gPatchDescr[gSlot].barPosition);
    LOG_DEBUG("  Red %u\n", gPatchDescr[gSlot].redVisible);
    LOG_DEBUG("  Blue %u\n", gPatchDescr[gSlot].blueVisible);
    LOG_DEBUG("  Yellow %u\n", gPatchDescr[gSlot].yellowVisible);
    LOG_DEBUG("  Orange %u\n", gPatchDescr[gSlot].orangeVisible);
    LOG_DEBUG("  Green %u\n", gPatchDescr[gSlot].greenVisible);
    LOG_DEBUG("  Purple %u\n", gPatchDescr[gSlot].purpleVisible);
    LOG_DEBUG("  White %u\n", gPatchDescr[gSlot].whiteVisible);
    LOG_DEBUG("  Mono Poly %u\n", gPatchDescr[gSlot].monoPoly);
    LOG_DEBUG("  Active Variation %u\n", gPatchDescr[gSlot].activeVariation);
    LOG_DEBUG("  Category %u\n", gPatchDescr[gSlot].category);

    // TODO - Might want to reconsider how we do this, since there's multiple cases of this setting of button colour
    for (uint32_t i = 0; i < NUM_GUI_VARIATIONS; i++) {
        gMainButtonArray[(uint32_t)variation1ButtonId + i].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
    }

    gMainButtonArray[gPatchDescr[gSlot].activeVariation + (uint32_t)variation1ButtonId].backgroundColour = (tRgb)RGB_GREEN_ON;
}

void write_patch_descr(uint8_t * buff, uint32_t * bitPos) {
    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PATCH_DESCRIPTION);
    write_bit_stream(buff, bitPos, 16, 15); // Length of following in bytes
    write_bit_stream(buff, bitPos, 32, gPatchDescr[gSlot].unknown1);
    write_bit_stream(buff, bitPos, 29, gPatchDescr[gSlot].unknown2);
    write_bit_stream(buff, bitPos, 5, gPatchDescr[gSlot].voiceCount);
    write_bit_stream(buff, bitPos, 14, gPatchDescr[gSlot].barPosition);
    write_bit_stream(buff, bitPos, 3, gPatchDescr[gSlot].unknown3);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].redVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].blueVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].yellowVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].orangeVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].greenVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].purpleVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].whiteVisible);
    write_bit_stream(buff, bitPos, 2, gPatchDescr[gSlot].monoPoly);
    write_bit_stream(buff, bitPos, 8, gPatchDescr[gSlot].activeVariation);
    write_bit_stream(buff, bitPos, 8, gPatchDescr[gSlot].category);
    write_bit_stream(buff, bitPos, 12, gPatchDescr[gSlot].unknown4);

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
}

void parse_module_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    uint32_t   i      = 0;
    uint32_t   j      = 0;
    uint32_t   type   = 0;
    tModuleKey key    = {0};
    tModule    module = {0};

    LOG_DEBUG("Module list\n");

    key.slot     = slot;
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

void write_module_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    tModule  module      = {0};
    uint32_t moduleCount = 0;
    bool     validModule = false;
    //int32_t   location = 0;
    uint32_t sizeBitPos        = 0;
    uint32_t moduleCountBitPos = 0;
    uint32_t j                 = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_MODULE_LIST);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);

    moduleCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);  // Populated later

    moduleCount = 0;
    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if (validModule == true) {
            if ((module.key.slot == slot) && (module.key.location == location)) {
                moduleCount++;
                write_bit_stream(buff, bitPos, 8, module.type);
                write_bit_stream(buff, bitPos, 8, module.key.index);
                write_bit_stream(buff, bitPos, 7, module.column);
                write_bit_stream(buff, bitPos, 7, module.row);
                write_bit_stream(buff, bitPos, 8, module.colour);
                write_bit_stream(buff, bitPos, 1, module.upRate);
                write_bit_stream(buff, bitPos, 1, module.isLed);
                write_bit_stream(buff, bitPos, 6, module.unknown1);
                write_bit_stream(buff, bitPos, 4, module.modeCount);

                for (j = 0; j < module.modeCount; j++) {
                    write_bit_stream(buff, bitPos, 6, module.mode[j].value);
                }
            }
        }
    } while (validModule);

    finish_walk_module();

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
    
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
    write_bit_stream(buff, &moduleCountBitPos, 8, moduleCount);
}

void parse_cable_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tCableKey key   = {0};
    tCable    cable = {0};

    LOG_DEBUG("Cable list\n");

    key.slot     = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_DEBUG("Location       0x%x\n", key.location);
    LOG_DEBUG("Unknown        0x%x\n", read_bit_stream(buff, subOffset, 12));  // TODO, store
    uint32_t cableCount = read_bit_stream(buff, subOffset, 10);
    LOG_DEBUG("Cable Count    %d\n", cableCount);

    for (uint32_t i = 0; i < cableCount; i++) {
        cable.colour             = read_bit_stream(buff, subOffset, 3);
        key.moduleFromIndex      = read_bit_stream(buff, subOffset, 8); // key will get written into struct on write
        key.connectorFromIoCount = read_bit_stream(buff, subOffset, 6);
        key.linkType             = read_bit_stream(buff, subOffset, 1); // 1 = output to input, 0 = input to input
        key.moduleToIndex        = read_bit_stream(buff, subOffset, 8);
        key.connectorToIoCount   = read_bit_stream(buff, subOffset, 6);

        write_cable(key, &cable);
    }
}

void write_cable_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    tCable   cable            = {0};
    uint32_t cableCount       = 0;
    bool     validCable       = false;
    uint32_t sizeBitPos       = 0;
    uint32_t cableCountBitPos = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_CABLE_LIST);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later
    write_bit_stream(buff, bitPos, 2, location);
    write_bit_stream(buff, bitPos, 12, 0);  // Unknown - TODO, store

    cableCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 10, 0);  // Populated later

    cableCount = 0;
    reset_walk_cable();

    do {
        validCable = walk_next_cable(&cable);

        if (validCable == true) {
            if ((cable.key.slot == slot) && (cable.key.location == location)) {
                cableCount++;
                write_bit_stream(buff, bitPos, 3, cable.colour);
                write_bit_stream(buff, bitPos, 8, cable.key.moduleFromIndex);
                write_bit_stream(buff, bitPos, 6, cable.key.connectorFromIoCount);
                write_bit_stream(buff, bitPos, 1, cable.key.linkType);
                write_bit_stream(buff, bitPos, 8, cable.key.moduleToIndex);
                write_bit_stream(buff, bitPos, 6, cable.key.connectorToIoCount);
            }
        }
    } while (validCable);

    finish_walk_cable();
    
    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
    
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
    
    write_bit_stream(buff, &cableCountBitPos, 10, cableCount);
}

void parse_param_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
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
    key.slot     = slot;
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
            LOG_ERROR("Incorrect number of parameters on module %u %s count from G2 = %u, our structures = %u\n", module.type, gModuleProperties[module.type].name, paramCount, module_param_count(module.type));
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

void write_param_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    tModule  module      = {0};
    uint32_t moduleCount = 0;
    bool     validModule = false;
    //int32_t   location = 0;
    uint32_t sizeBitPos        = 0;
    uint32_t moduleCountBitPos = 0;
    uint32_t paramCount        = 0;
    uint32_t variations        = 0;
    uint32_t i                 = 0;
    uint32_t j                 = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PARAM_LIST);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);

    moduleCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);  // Populated later

    // See if there's at least one valid module on the location and set variations to non-zero if so
    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if (validModule == true) {
            if ((module.key.slot == slot) && (module.key.location == location)) {
                // We found a valid module on that location, so can set variations to non-zero
                variations = NUM_VARIATIONS - 1;
                break;
            }
        }
    } while (validModule);

    finish_walk_module();

    write_bit_stream(buff, bitPos, 8, variations);  // Variation count - always 10 for synth, 9 for file?

    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if (validModule == true) {
            if ((module.key.slot == slot) && (module.key.location == location)) {
                paramCount = get_param_count(location, module.key.index, module.type);

                if (paramCount > 0) {
                    moduleCount++;
                    write_bit_stream(buff, bitPos, 8, module.key.index);

                    write_bit_stream(buff, bitPos, 7, paramCount);

                    for (i = 0; i < (NUM_VARIATIONS - 1); i++) {
                        write_bit_stream(buff, bitPos, 8, i);

                        for (j = 0; j < paramCount; j++) {
                            write_bit_stream(buff, bitPos, 7, module.param[i][j].value);
                        }
                    }
                }
            }
        }
    } while (validModule);

    finish_walk_module();
    
    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
    
    write_bit_stream(buff, &moduleCountBitPos, 8, moduleCount);
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void parse_morph_params(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    // line 3754 in file.pas
    tModule    module          = {0};
    tModuleKey key             = {0};
    uint32_t   variationCount  = 0;
    uint32_t   variation       = 0;
    uint32_t   morphParamCount = 0;
    uint32_t   paramIndex      = 0;
    uint32_t   morph           = 0;
    uint32_t   range           = 0;
    int        j               = 0;
    int        k               = 0;

    variationCount    = read_bit_stream(buff, subOffset, 8);
    gMorphCount[slot] = read_bit_stream(buff, subOffset, 4);
    read_bit_stream(buff, subOffset, 20);  // Reserved data

    LOG_DEBUG("Variations %u Morph Count %u\n", variationCount, gMorphCount[slot]);

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
            key.slot     = slot;
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
    }
}

void write_morph_params(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    tModule  module                = {0};
    bool     validModule           = false;
    uint32_t sizeBitPos            = 0;
    uint32_t morphParamCountBitPos = 0;
    uint32_t morphParamCount       = 0;
    uint32_t paramCount            = 0;
    uint32_t i                     = 0;
    uint32_t j                     = 0;
    uint32_t m                     = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_MORPH_PARAMS);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);                 // Populated later

    write_bit_stream(buff, bitPos, 8, NUM_VARIATIONS - 1); // Variation count (9)
    write_bit_stream(buff, bitPos, 4, gMorphCount[slot]);  // Morph count (typically 4)
    write_bit_stream(buff, bitPos, 20, 0);                 // Reserved data

    // For each variation
    for (i = 0; i < (NUM_VARIATIONS - 1); i++) {
        write_bit_stream(buff, bitPos, 4, i);  // Variation number
        write_bit_stream(buff, bitPos, 4, 0);  // Unknown
        write_bit_stream(buff, bitPos, 8, 0);  // Unknown
        write_bit_stream(buff, bitPos, 8, 0);  // Unknown
        write_bit_stream(buff, bitPos, 8, 0);  // Unknown
        write_bit_stream(buff, bitPos, 8, 0);  // Unknown
        write_bit_stream(buff, bitPos, 8, 0);  // Unknown
        write_bit_stream(buff, bitPos, 8, 0);  // Unknown
        write_bit_stream(buff, bitPos, 4, 0);  // Unknown

        morphParamCountBitPos = *bitPos;
        write_bit_stream(buff, bitPos, 8, 0);  // Populated later

        morphParamCount = 0;
        reset_walk_module();

        // Walk through all modules to find parameters with morph ranges
        do {
            validModule = walk_next_module(&module);

            if (validModule == true) {
                // Only filter by slot, not by location - morph params are for ALL locations
                if (module.key.slot == slot) {
                    paramCount = get_param_count(location, module.key.index, module.type);

                    // Check each parameter for morph assignments
                    for (j = 0; j < paramCount; j++) {
                        // Write ALL morphs assigned to this parameter
                        for (m = 0; m < gMorphCount[slot]; m++) {
                            if (module.param[i][j].morphRange[m] != 0) {
                                morphParamCount++;
                                write_bit_stream(buff, bitPos, 2, module.key.location);
                                write_bit_stream(buff, bitPos, 8, module.key.index);
                                write_bit_stream(buff, bitPos, 7, j); // Parameter index
                                write_bit_stream(buff, bitPos, 4, m); // Morph index
                                write_bit_stream(buff, bitPos, 8, module.param[i][j].morphRange[m]);
                            }
                        }
                    }
                }
            }
        } while (validModule);

        finish_walk_module();

        write_bit_stream(buff, &morphParamCountBitPos, 8, morphParamCount);
        write_bit_stream(buff, bitPos, 4, 0);  // Trailing unknown bits
    }

    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
    
    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE(*bitPos)); // Round down
}
    
void parse_knobs(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
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
            key.slot     = slot;
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

void write_knobs(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
}

void parse_param_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset, int count) {
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

    key.slot     = slot;
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

void write_param_names(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    tModule  module             = {0};
    uint32_t moduleCount        = 0;
    bool     validModule        = false;
    uint32_t sizeBitPos         = 0;
    uint32_t nameCountBitPos    = 0;
    uint32_t moduleLengthBitPos = 0;
    uint32_t moduleLength       = 0;
    uint32_t paramCount         = 0;
    uint32_t j                  = 0;
    uint32_t k                  = 0;
    uint32_t labelIndex         = 0;
    uint32_t numLabels          = 0;
    uint32_t paramLength        = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PARAM_NAMES);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);

    nameCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);  // Populated later

    moduleCount = 0;
    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if (validModule == true) {
            if ((module.key.slot == slot) && (module.key.location == location)) {
                paramCount = get_param_count(location, module.key.index, module.type);

                if (paramCount > 0) {
                    // Check if this module has any named parameters
                    bool moduleHasNames = false;

                    for (j = 0; j < paramCount; j++) {
                        for (k = 0; k < PROTOCOL_PARAM_NAME_SIZE; k++) {
                            if (module.paramName[j][k] != 0) {
                                moduleHasNames = true;
                                break;
                            }
                        }

                        if (moduleHasNames) {
                            break;
                        }
                    }

                    if (moduleHasNames) {
                        moduleCount++;
                        write_bit_stream(buff, bitPos, 8, module.key.index);

                        moduleLengthBitPos = *bitPos;
                        write_bit_stream(buff, bitPos, 8, 0);  // Populated later

                        moduleLength = 0;

                        for (j = 0; j < paramCount; j++) {
                            bool hasName = false;

                            for (k = 0; k < PROTOCOL_PARAM_NAME_SIZE && !hasName; k++) {
                                if (module.paramName[j][k] != 0) {
                                    hasName = true;
                                }
                            }

                            if (hasName) {
                                numLabels   = 1;
                                paramLength = 1 + (numLabels * PROTOCOL_PARAM_NAME_SIZE);

                                write_bit_stream(buff, bitPos, 8, 1);  // isString
                                write_bit_stream(buff, bitPos, 8, paramLength);
                                write_bit_stream(buff, bitPos, 8, j);  // paramIndex

                                moduleLength += 3;

                                for (labelIndex = 0; labelIndex < numLabels; labelIndex++) {
                                    for (k = 0; k < PROTOCOL_PARAM_NAME_SIZE; k++) {
                                        write_bit_stream(buff, bitPos, 8, module.paramName[j][k]);
                                    }
                                }

                                moduleLength += paramLength - 1;
                            }
                        }

                        write_bit_stream(buff, &moduleLengthBitPos, 8, moduleLength);
                    }
                }
            }
        }
    } while (validModule);

    finish_walk_module();

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
    
    write_bit_stream(buff, &nameCountBitPos, 8, moduleCount);
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void parse_module_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tModule    module = {0};
    tModuleKey key    = {0};
    uint32_t   i      = 0;
    char       name[MODULE_NAME_SIZE + 1];


    LOG_DEBUG("Module names\n");

    key.slot     = slot;
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

void write_module_names(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    tModule  module          = {0};
    uint32_t moduleCount     = 0;
    bool     validModule     = false;
    uint32_t sizeBitPos      = 0;
    uint32_t itemCountBitPos = 0;
    uint32_t k               = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_MODULE_NAMES);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);
    write_bit_stream(buff, bitPos, 6, 0);  // Unknown/reserved

    itemCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);  // Populated later

    moduleCount = 0;
    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if (validModule == true) {
            if ((module.key.slot == slot) && (module.key.location == location)) {
                if (module.name[0] != '\0') {
                    moduleCount++;
                    write_bit_stream(buff, bitPos, 8, module.key.index);

                    for (k = 0; k < MODULE_NAME_SIZE; k++) {
                        write_bit_stream(buff, bitPos, 8, module.name[k]);
                        if (module.name[k] == '\0') {
                            break;
                        }
                    }
                }
            }
        }
    } while (validModule);

    finish_walk_module();

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
    
    write_bit_stream(buff, &itemCountBitPos, 8, moduleCount);
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

#ifdef __cplusplus
}
#endif

