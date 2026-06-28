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

#include "defs.h"
#include "types.h"
#include "utils.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "msgQueue.h"
#include "globalVars.h"

static pthread_mutex_t patchNameMutex = PTHREAD_MUTEX_INITIALIZER;

void read_clavia_string(uint8_t * buff, uint32_t * bitPos, char * name, int nameSize) {
    int i = 0;

    if (nameSize != CLAVIA_NAME_SIZE + 1) {
        LOG_ERROR("Called with invalid size of %d\n", nameSize);
        exit(1);
    }
    pthread_mutex_lock(&patchNameMutex);

    memset(name, 0, nameSize);

    for (i = 0; i < CLAVIA_NAME_SIZE; i++) {
        name[i] = (uint8_t)read_bit_stream(buff, bitPos, 8);

        if (name[i] == '\0') {
            break;
        }
    }

    pthread_mutex_unlock(&patchNameMutex);
}

void write_clavia_string(uint8_t * buff, uint32_t * bitPos, const char * name) {
    int i = 0;

    pthread_mutex_lock(&patchNameMutex);

    for (i = 0; i < CLAVIA_NAME_SIZE; i++) {
        write_bit_stream(buff, bitPos, 8, (uint8_t)name[i]);

        if (name[i] == '\0') {
            break;
        }
    }

    pthread_mutex_unlock(&patchNameMutex);
}

void parse_patch_descr(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    gPatchDescr[slot].unknown1        = read_bit_stream(buff, subOffset, 32);
    gPatchDescr[slot].unknown2        = read_bit_stream(buff, subOffset, 29);
    gPatchDescr[slot].voiceCount      = read_bit_stream(buff, subOffset, 5);
    gPatchDescr[slot].barPosition     = read_bit_stream(buff, subOffset, 14);
    gPatchDescr[slot].unknown3        = read_bit_stream(buff, subOffset, 3);
    gPatchDescr[slot].visible[0]      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[slot].visible[1]      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[slot].visible[2]      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[slot].visible[3]      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[slot].visible[4]      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[slot].visible[5]      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[slot].visible[6]      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[slot].monoPoly        = read_bit_stream(buff, subOffset, 2);
    gPatchDescr[slot].activeVariation = read_bit_stream(buff, subOffset, 8);
    gPatchDescr[slot].category        = read_bit_stream(buff, subOffset, 8);
    gPatchDescr[slot].unknown4        = read_bit_stream(buff, subOffset, 12);

    LOG_MODULE_DATA("  Voice Count %u\n", gPatchDescr[slot].voiceCount);
    LOG_MODULE_DATA("  Bar Position %u\n", gPatchDescr[slot].barPosition);
    LOG_MODULE_DATA("  Red %u\n", gPatchDescr[slot].visible[0]);
    LOG_MODULE_DATA("  Blue %u\n", gPatchDescr[slot].visible[1]);
    LOG_MODULE_DATA("  Yellow %u\n", gPatchDescr[slot].visible[2]);
    LOG_MODULE_DATA("  Orange %u\n", gPatchDescr[slot].visible[3]);
    LOG_MODULE_DATA("  Green %u\n", gPatchDescr[slot].visible[4]);
    LOG_MODULE_DATA("  Purple %u\n", gPatchDescr[slot].visible[5]);
    LOG_MODULE_DATA("  White %u\n", gPatchDescr[slot].visible[6]);
    LOG_MODULE_DATA("  Mono Poly %u\n", gPatchDescr[slot].monoPoly);
    LOG_MODULE_DATA("  Active Variation %u\n", gPatchDescr[slot].activeVariation);
    LOG_MODULE_DATA("  Category %u\n", gPatchDescr[slot].category);

    if (slot == gSlot) {
        set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId, (tTopbarControlId)(topbarVariation1Id + gPatchDescr[slot].activeVariation));
    }
}

void write_patch_descr(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PATCH_DESCRIPTION);
    write_bit_stream(buff, bitPos, 16, 15); // Length of following in bytes
    write_bit_stream(buff, bitPos, 32, gPatchDescr[slot].unknown1);
    write_bit_stream(buff, bitPos, 29, gPatchDescr[slot].unknown2);
    write_bit_stream(buff, bitPos, 5, gPatchDescr[slot].voiceCount);
    write_bit_stream(buff, bitPos, 14, gPatchDescr[slot].barPosition);
    write_bit_stream(buff, bitPos, 3, gPatchDescr[slot].unknown3);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[slot].visible[0]);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[slot].visible[1]);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[slot].visible[2]);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[slot].visible[3]);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[slot].visible[4]);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[slot].visible[5]);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[slot].visible[6]);
    write_bit_stream(buff, bitPos, 2, gPatchDescr[slot].monoPoly);
    write_bit_stream(buff, bitPos, 8, gPatchDescr[slot].activeVariation);
    write_bit_stream(buff, bitPos, 8, gPatchDescr[slot].category);
    write_bit_stream(buff, bitPos, 12, gPatchDescr[slot].unknown4);

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
}

void parse_module_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    uint32_t   i           = 0;
    uint32_t   j           = 0;
    uint32_t   type        = 0;
    tModuleKey key         = {0};
    tModule *  module      = NULL;

    LOG_MODULE_DATA("Module list\n");

    key.slot     = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_MODULE_DATA("Location       0x%x\n", key.location);
    uint32_t   moduleCount = read_bit_stream(buff, subOffset, 8);
    LOG_MODULE_DATA("Module Count   %d\n", moduleCount);

    for (i = 0; i < moduleCount; i++) {
        type              = read_bit_stream(buff, subOffset, 8);
        key.index         = read_bit_stream(buff, subOffset, 8);

        if ((key.location >= (uint32_t)locationMax) || (key.index >= MAX_NUM_MODULES)) {
            LOG_ERROR("Module key out of bounds location=%u index=%u\n", key.location, key.index);
            break;
        }
        module            = get_module_slot(key.slot, key.location, key.index);
        module->active    = true;
        module->key       = key;
        module->type      = type;
        module->column    = read_bit_stream(buff, subOffset, 7);
        module->row       = read_bit_stream(buff, subOffset, 7);
        module->colour    = read_bit_stream(buff, subOffset, 8);
        module->upRate    = read_bit_stream(buff, subOffset, 1);
        module->isLed     = read_bit_stream(buff, subOffset, 1);
        module->unknown1  = read_bit_stream(buff, subOffset, 6);
        module->modeCount = read_bit_stream(buff, subOffset, 4);

        LOG_MODULE_DATA("Module type %u\n", module->type);
        LOG_MODULE_DATA("Module column %u\n", module->column);
        LOG_MODULE_DATA("Module row %u\n", module->row);

        for (j = 0; j < module->modeCount; j++) {
            module->mode[j].value = read_bit_stream(buff, subOffset, 6);
            LOG_MODULE_DATA("Mode index %u = %u\n", j, module->mode[j].value);
        }

        LOG_MODULE_DATA("Number connectors for module %u\n", module_connector_count(type));
    }
}

void write_module_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    uint32_t moduleCount       = 0;
    uint32_t sizeBitPos        = 0;
    uint32_t moduleCountBitPos = 0;
    uint32_t j                 = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_MODULE_LIST);

    sizeBitPos        = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);

    moduleCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);   // Populated later

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (!module->active) {
            continue;
        }
        moduleCount++;
        write_bit_stream(buff, bitPos, 8, module->type);
        write_bit_stream(buff, bitPos, 8, module->key.index);
        write_bit_stream(buff, bitPos, 7, module->column);
        write_bit_stream(buff, bitPos, 7, module->row);
        write_bit_stream(buff, bitPos, 8, module->colour);
        write_bit_stream(buff, bitPos, 1, module->upRate);
        write_bit_stream(buff, bitPos, 1, module->isLed);
        write_bit_stream(buff, bitPos, 6, module->unknown1);
        write_bit_stream(buff, bitPos, 4, module->modeCount);

        for (j = 0; j < module->modeCount; j++) {
            write_bit_stream(buff, bitPos, 6, module->mode[j].value);
        }
    }

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
    write_bit_stream(buff, &moduleCountBitPos, 8, moduleCount);
}

void parse_cable_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tCableKey key        = {0};
    tCable    cable      = {0};

    LOG_MODULE_DATA("Cable list\n");

    key.slot     = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_MODULE_DATA("Location       0x%x\n", key.location);
    read_bit_stream(buff, subOffset, 6);                         // byte-align before GetUWord
    uint32_t  cableCount = read_bit_stream(buff, subOffset, 16); // high byte then low byte
    LOG_MODULE_DATA("Cable Count    %d\n", cableCount);

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
    uint32_t cableCount       = 0;
    uint32_t sizeBitPos       = 0;
    uint32_t cableCountBitPos = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_CABLE_LIST);

    sizeBitPos       = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later
    write_bit_stream(buff, bitPos, 2, location);
    write_bit_stream(buff, bitPos, 6, 0);   // byte-align before PutUWord

    cableCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    cableCount       = 0;

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, (uint32_t)location, i);

        if (cable == NULL || !cable->active) {
            continue;
        }
        cableCount++;
        write_bit_stream(buff, bitPos, 3, cable->colour);
        write_bit_stream(buff, bitPos, 8, cable->key.moduleFromIndex);
        write_bit_stream(buff, bitPos, 6, cable->key.connectorFromIoCount);
        write_bit_stream(buff, bitPos, 1, cable->key.linkType);
        write_bit_stream(buff, bitPos, 8, cable->key.moduleToIndex);
        write_bit_stream(buff, bitPos, 6, cable->key.connectorToIoCount);
    }

    *bitPos          = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);

    write_bit_stream(buff, &cableCountBitPos, 16, cableCount);
}

void parse_param_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    uint32_t   numVariations = 0;
    uint32_t   paramCount    = 0;
    uint32_t   moduleCount   = 0;
    uint32_t   paramValue    = 0;
    tModuleKey key           = {0};
    int        i             = 0;
    int        j             = 0;
    int        k             = 0;

    LOG_MODULE_DATA("Param list\n");
    key.slot      = slot;
    key.location  = read_bit_stream(buff, subOffset, 2);
    LOG_MODULE_DATA("Location       0x%x\n", key.location);     // 0..1 = param list, 2 = patch settings!?
    // SWITCH ON LOC BEING 0..1 or 2 2 = line 4112 in file.pas
    moduleCount   = read_bit_stream(buff, subOffset, 8);
    LOG_MODULE_DATA("Module Count      %u\n", moduleCount);
    numVariations = read_bit_stream(buff, subOffset, 8);     // Should always be 10 on USB or 9 in a file - TODO: sanity check
    LOG_MODULE_DATA("Variation Count      %u\n", numVariations);

    if (numVariations > 10) {
        LOG_MODULE_DATA("Variation Count > 10\n");
        return;
    }

    for (i = 0; i < moduleCount; i++) {
        key.index                = read_bit_stream(buff, subOffset, 8);

        paramCount               = read_bit_stream(buff, subOffset, 7);
        LOG_MODULE_DATA("  variation list param count = %u\n", paramCount);

        if (paramCount >= MAX_NUM_PARAMETERS) {
            LOG_ERROR("MAX_NUM_PARAMETERS needs increasing to >= %u\n", paramCount + 1);
            exit(1);
        }

        if ((key.location >= (uint32_t)locationMax) || (key.index >= MAX_NUM_MODULES)) {
            LOG_ERROR("parse_param_list: key out of bounds location=%u index=%u\n", key.location, key.index);
            break;
        }
        tModule * module = get_module_slot(key.slot, key.location, key.index);
        module->key              = key;
        module->active           = true;
        module->actualParamCount = paramCount;

        if ((module->type != moduleTypeUnknown0) && (module_param_count(module->type) > 0)) {
            if (paramCount != module_param_count(module->type)) {
                LOG_ERROR("Incorrect number of parameters on module %u %s count from G2 = %u, our structures = %u\n", module->type, gModuleProperties[module->type].name, paramCount, module_param_count(module->type));
            }

            if (paramCount > module_param_count(module->type)) {
                exit(1);
            }
        }

        for (j = 0; j < numVariations; j++) {
            uint32_t variation = read_bit_stream(buff, subOffset, 8);

            if (variation == 0) {
                LOG_MODULE_DATA("  Variation %u\n", variation);
            }

            if (j != variation) {
                LOG_WARNING("loop var %u != variation %u\n", j, variation);
            }

            for (k = 0; k < paramCount; k++) {
                paramValue                = read_bit_stream(buff, subOffset, 7);

                if (variation == 0) {
                    LOG_MODULE_DATA("   Param number %02d param value %02d\n", k, paramValue);
                }

                if (variation == 9) {
                    if (module->param[8][k].value != paramValue) {
                        LOG_MODULE_DATA("   Difference on init value in 8 = %02d 9 = %02d\n", module->param[8][k].value, paramValue);
                    }
                }
                module->param[j][k].value = paramValue;
            }
        }
    }
}

void write_param_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos, uint32_t numVariations) {
    uint32_t moduleCount       = 0;
    uint32_t sizeBitPos        = 0;
    uint32_t moduleCountBitPos = 0;
    uint32_t variationsBitPos  = 0;
    uint32_t paramCount        = 0;
    uint32_t variations        = 0;
    uint32_t i                 = 0;
    uint32_t j                 = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PARAM_LIST);

    sizeBitPos        = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);

    moduleCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);   // Populated later

    variationsBitPos  = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);   // Write 9 for files, not 10!

    for (i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (!module->active) {
            continue;
        }
        paramCount = module->actualParamCount;

        if (paramCount > 0) {
            variations = numVariations;
            moduleCount++;

            write_bit_stream(buff, bitPos, 8, module->key.index);
            write_bit_stream(buff, bitPos, 7, paramCount);

            for (uint32_t v = 0; v < numVariations; v++) {
                write_bit_stream(buff, bitPos, 8, v);

                for (j = 0; j < paramCount; j++) {
                    write_bit_stream(buff, bitPos, 7, module->param[v][j].value);
                }
            }
        }
    }

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &variationsBitPos, 8, variations);
    write_bit_stream(buff, &moduleCountBitPos, 8, moduleCount);
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void parse_morph_params(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tModuleKey key             = {0};
    uint32_t   numVariations   = 0;
    uint32_t   variation       = 0;
    uint32_t   morphParamCount = 0;
    uint32_t   paramIndex      = 0;
    uint32_t   morph           = 0;
    uint32_t   range           = 0;
    int        j               = 0;
    int        k               = 0;

    numVariations     = read_bit_stream(buff, subOffset, 8);
    gMorphCount[slot] = read_bit_stream(buff, subOffset, 4);

    for (j = 0; j < (int)gMorphCount[slot]; j++) {
        read_bit_stream(buff, subOffset, 2);  // keyboard morph assign (discard)
    }

    LOG_MODULE_DATA("Variations %u Morph Count %u\n", numVariations, gMorphCount[slot]);

    for (j = 0; j < numVariations; j++) {
        variation       = read_bit_stream(buff, subOffset, 8);

        for (k = 0; k < (int)gMorphCount[slot]; k++) {
            read_bit_stream(buff, subOffset, 7);  // morph value per morph (discard)
        }

        morphParamCount = read_bit_stream(buff, subOffset, 8);
        LOG_MODULE_DATA("Variation %u Morph param count %u\n", variation, morphParamCount);

        for (k = 0; k < morphParamCount; k++) {
            key.slot     = slot;
            key.location = read_bit_stream(buff, subOffset, 2);
            key.index    = read_bit_stream(buff, subOffset, 8);
            paramIndex   = read_bit_stream(buff, subOffset, 7);
            morph        = read_bit_stream(buff, subOffset, 4);
            range        = read_bit_stream(buff, subOffset, 8);

            LOG_MODULE_DATA("  Location %u\n", key.location);
            LOG_MODULE_DATA("  Module index %u\n", key.index);
            LOG_MODULE_DATA("  Param index %u\n", paramIndex);
            LOG_MODULE_DATA("  Morph %u\n", morph);
            LOG_MODULE_DATA("  Range %u\n", range);

            if ((key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
                tModule * module = get_module_slot(key.slot, key.location, key.index);

                if (morph < NUM_MORPHS) {
                    module->param[variation][paramIndex].morphRange[morph] = (uint8_t)range;
                } else {
                    LOG_ERROR("morph index %u out of range\n", morph);
                }
            }
        }
    }
}

void write_morph_params(uint32_t slot, uint8_t * buff, uint32_t * bitPos, uint32_t numVariations) {
    uint32_t sizeBitPos            = 0;
    uint32_t morphParamCountBitPos = 0;
    uint32_t morphParamCount       = 0;
    uint32_t paramCount            = 0;
    uint32_t i                     = 0;
    uint32_t j                     = 0;
    uint32_t m                     = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_MORPH_PARAMS);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);            // Populated later

    write_bit_stream(buff, bitPos, 8, numVariations); // Variation count (9)
    write_bit_stream(buff, bitPos, 4, gMorphCount[slot]);

    for (m = 0; m < gMorphCount[slot]; m++) {
        write_bit_stream(buff, bitPos, 2, 0);  // keyboard morph assign (zero)
    }

    for (i = 0; i < numVariations; i++) {
        write_bit_stream(buff, bitPos, 8, i);  // Variation number

        for (m = 0; m < gMorphCount[slot]; m++) {
            write_bit_stream(buff, bitPos, 7, 0);  // morph value (zero)
        }

        morphParamCountBitPos = *bitPos;
        write_bit_stream(buff, bitPos, 8, 0);  // Populated later

        morphParamCount       = 0;

        for (uint32_t l = 0; l < (uint32_t)locationMax; l++) {
            for (uint32_t idx = 0; idx < MAX_NUM_MODULES; idx++) {
                tModule * module = get_module_slot(slot, l, idx);

                if (!module->active) {
                    continue;
                }
                paramCount = module->actualParamCount;

                for (j = 0; j < paramCount; j++) {
                    for (m = 0; m < gMorphCount[slot]; m++) {
                        if (module->param[i][j].morphRange[m] != 0) {
                            morphParamCount++;
                            write_bit_stream(buff, bitPos, 2, module->key.location);
                            write_bit_stream(buff, bitPos, 8, module->key.index);
                            write_bit_stream(buff, bitPos, 7, j);
                            write_bit_stream(buff, bitPos, 4, m);
                            write_bit_stream(buff, bitPos, 8, module->param[i][j].morphRange[m]);
                        }
                    }
                }
            }
        }

        write_bit_stream(buff, &morphParamCountBitPos, 8, morphParamCount);
    }

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

static void parse_knob_common_fields(uint8_t * buff, uint32_t * bitPos, tKnob * knob) {
    knob->assigned = read_bit_stream(buff, bitPos, 1);

    if (knob->assigned) {
        knob->location    = read_bit_stream(buff, bitPos, 2);
        knob->moduleIndex = read_bit_stream(buff, bitPos, 8);
        knob->isLed       = read_bit_stream(buff, bitPos, 2);
        knob->paramIndex  = read_bit_stream(buff, bitPos, 7);
    }
}

void parse_knobs(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    uint32_t knobCount = 0;
    int      i         = 0;

    if (slot >= MAX_SLOTS) {
        return;
    }
    knobCount = read_bit_stream(buff, subOffset, 16);
    LOG_MODULE_DATA("  Knob Count %u\n", knobCount);

    // The G2 always sends exactly KNOB_COUNT (120) entries; guard against
    // malformed data sending more than we have storage for.
    if (knobCount > MAX_NUM_KNOBS) {
        LOG_ERROR("parse_knobs: knobCount %u exceeds KNOB_COUNT %u\n", knobCount, MAX_NUM_KNOBS);
        knobCount = MAX_NUM_KNOBS;
    }
    // Clear the list before repopulating
    memset(&gKnobArray[slot], 0, sizeof(tKnobArray));

    for (i = 0; i < (int)knobCount; i++) {
        tKnob * knob = &gKnobArray[slot].knob[i];

        parse_knob_common_fields(buff, subOffset, knob);

        if (knob->assigned) {
            LOG_MODULE_DATA("  Knob %d: location %u module %u isLed %u param %u\n",
                            i, knob->location, knob->moduleIndex, knob->isLed, knob->paramIndex);
        }
    }
}

void parse_global_knobs(uint8_t * buff, uint32_t * bitPos) {
    uint32_t knobCount = read_bit_stream(buff, bitPos, 16);

    LOG_MODULE_DATA("  Global knob count %u\n", knobCount);

    if (knobCount > MAX_NUM_KNOBS) {
        LOG_ERROR("parse_global_knobs: count %u exceeds %u\n", knobCount, MAX_NUM_KNOBS);
        knobCount = MAX_NUM_KNOBS;
    }
    memset(gGlobalKnobArray, 0, sizeof(gGlobalKnobArray));

    for (uint32_t i = 0; i < knobCount; i++) {
        tKnob         knob  = {0};
        tGlobalKnob * gKnob = &gGlobalKnobArray[i];

        parse_knob_common_fields(buff, bitPos, &knob);
        gKnob->assigned    = knob.assigned;
        gKnob->location    = knob.location;
        gKnob->moduleIndex = knob.moduleIndex;
        gKnob->isLed       = knob.isLed;
        gKnob->paramIndex  = knob.paramIndex;

        if (gKnob->assigned) {
            gKnob->slotIndex = read_bit_stream(buff, bitPos, 2);
            LOG_MODULE_DATA("  Global knob %u: slot %u location %u module %u isLed %u param %u\n",
                            i, gKnob->slotIndex, gKnob->location, gKnob->moduleIndex, gKnob->isLed, gKnob->paramIndex);
        }
    }
}

void write_knobs(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    int      i          = 0;
    uint32_t sizeBitPos = 0;

    if (slot >= MAX_SLOTS) {
        return;
    }
    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_KNOBS);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 16, MAX_NUM_KNOBS);

    for (i = 0; i < MAX_NUM_KNOBS; i++) {
        write_bit_stream(buff, bitPos, 1, gKnobArray[slot].knob[i].assigned ? 1 : 0);

        if (gKnobArray[slot].knob[i].assigned) {
            write_bit_stream(buff, bitPos, 2, gKnobArray[slot].knob[i].location);
            write_bit_stream(buff, bitPos, 8, gKnobArray[slot].knob[i].moduleIndex);
            write_bit_stream(buff, bitPos, 2, gKnobArray[slot].knob[i].isLed);
            write_bit_stream(buff, bitPos, 7, gKnobArray[slot].knob[i].paramIndex);
        }
    }

    *bitPos    = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void parse_controllers(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tModuleKey key             = {0};
    uint32_t   controllerCount = 0;
    uint32_t   paramIndex      = 0;
    int        i               = 0;

    LOG_MODULE_DATA("Controllers\n");

    controllerCount = read_bit_stream(buff, subOffset, 7);
    LOG_MODULE_DATA("  Controller Count %u\n", controllerCount);

    if (controllerCount > MAX_NUM_CONTROLLERS) {
        LOG_ERROR("Controller count %u exceeds MAX_NUM_CONTROLLERS %u\n", controllerCount, MAX_NUM_CONTROLLERS);
        controllerCount = MAX_NUM_CONTROLLERS;
    }

    for (i = 0; i < controllerCount; i++) {
        gControllerArray[slot].controller[i].midiCC      = read_bit_stream(buff, subOffset, 7);
        key.slot                                         = slot;
        key.location                                     = read_bit_stream(buff, subOffset, 2);
        key.index                                        = read_bit_stream(buff, subOffset, 8);
        paramIndex                                       = read_bit_stream(buff, subOffset, 7);
        gControllerArray[slot].controller[i].location    = key.location;
        gControllerArray[slot].controller[i].moduleIndex = key.index;
        gControllerArray[slot].controller[i].paramIndex  = paramIndex;

        LOG_MODULE_DATA("  Controller %d: MidiCC %u Location %u ModuleIndex %u ParamIndex %u\n",
                        i, gControllerArray[slot].controller[i].midiCC, key.location, key.index, paramIndex);

        // Shadow onto the module param for convenient per-param lookup
        tModule * module = get_module(key);

        if (module != NULL) {
            if (paramIndex < MAX_NUM_PARAMETERS) {
                module->param[0][paramIndex].midiCC    = gControllerArray[slot].controller[i].midiCC;
                module->param[0][paramIndex].hasMidiCC = true;
            } else {
                LOG_ERROR("Controller paramIndex %u out of range for module %u\n", paramIndex, key.index);
            }
        }
    }

    gControllerCount[slot] = controllerCount;
}

void write_controllers(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    uint32_t sizeBitPos            = 0;
    uint32_t controllerCountBitPos = 0;
    uint32_t i                     = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_CONTROLLERS);

    sizeBitPos            = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    controllerCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 7, 0);   // Populated later

    for (i = 0; i < gControllerCount[slot]; i++) {
        write_bit_stream(buff, bitPos, 7, gControllerArray[slot].controller[i].midiCC);
        write_bit_stream(buff, bitPos, 2, gControllerArray[slot].controller[i].location);
        write_bit_stream(buff, bitPos, 8, gControllerArray[slot].controller[i].moduleIndex);
        write_bit_stream(buff, bitPos, 7, gControllerArray[slot].controller[i].paramIndex);
    }

    *bitPos               = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &controllerCountBitPos, 7, gControllerCount[slot]);
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void parse_param_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    uint32_t   nameCount    = 0;
    uint32_t   paramLength  = 0;
    uint32_t   moduleLength = 0;
    tModuleKey key          = {0};
    int        i            = 0;
    int        j            = 0;
    int        labelIndex   = 0;
    int        k            = 0;
    uint32_t   isString     = 0;
    uint32_t   paramIndex   = 0;
    uint32_t   numLabels    = 0;

    LOG_MODULE_DATA("Param names\n");

    key.slot     = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    LOG_MODULE_DATA("Location       0x%x\n", key.location);
    nameCount    = read_bit_stream(buff, subOffset, 8);
    LOG_MODULE_DATA("NameCount      %d\n", nameCount);

    for (i = 0; i < nameCount; i++) {
        key.index    = read_bit_stream(buff, subOffset, 8);
        LOG_MODULE_DATA("Module index      %d\n", key.index);

        if ((key.location >= (uint32_t)locationMax) || (key.index >= MAX_NUM_MODULES)) {
            LOG_ERROR("parse_param_names: key out of bounds location=%u index=%u\n", key.location, key.index);
            break;
        }
        tModule * module = get_module_slot(key.slot, key.location, key.index);

        moduleLength = read_bit_stream(buff, subOffset, 8);
        LOG_MODULE_DATA("Module length     %d\n\n", moduleLength);

        for (j = 0; j < moduleLength;) {
            isString    = read_bit_stream(buff, subOffset, 8);
            LOG_MODULE_DATA("IsString     %d\n", isString);
            paramLength = read_bit_stream(buff, subOffset, 8);
            LOG_MODULE_DATA("ParamLen     %d\n", paramLength);
            paramIndex  = read_bit_stream(buff, subOffset, 8);
            LOG_MODULE_DATA("Param Index  %d\n", paramIndex);
            j          += 3;
            LOG_MODULE_DATA("Param name: ");

            if (paramLength > 0) {
                numLabels = (paramLength - 1) / PROTOCOL_PARAM_NAME_SIZE;

                if (numLabels > MAX_NUM_LABELS) {
                    LOG_ERROR("numLabels %u exceeds maximum %u for param %u\n", numLabels, MAX_NUM_LABELS, paramIndex);
                    exit(1);
                }

                if (paramIndex >= MAX_NUM_PARAMETERS) {
                    LOG_WARNING("paramIndex %u exceeds maximum %u, skipping\n", paramIndex, MAX_NUM_PARAMETERS);

                    for (k = 0; k < (int)(paramLength - 1); k++) {
                        read_bit_stream(buff, subOffset, 8);
                    }

                    j += paramLength - 1;
                    continue;
                }

                if (sizeof(module->paramName[0]) < (numLabels * PROTOCOL_PARAM_NAME_SIZE)) {
                    LOG_ERROR("paramName array too small for %u labels\n", numLabels);
                    exit(1);
                }
                memset(&module->paramName[paramIndex], 0, sizeof(module->paramName[0]));

                module->paramNumLabels[paramIndex] = numLabels;

                for (labelIndex = 0; labelIndex < numLabels; labelIndex++) {
                    module->paramNameSet[paramIndex][labelIndex] = true;

                    for (k = 0; k < PROTOCOL_PARAM_NAME_SIZE; k++) {
                        uint8_t ch = read_bit_stream(buff, subOffset, 8);

                        if ((ch >= 0x20) && (ch <= 0x7f)) {
                            LOG_MODULE_DATA_DIRECT("%c", ch);
                        } else {
                            LOG_MODULE_DATA_DIRECT(" ");
                        }
                        module->paramName[paramIndex][labelIndex][k] = ch;
                    }
                }

                j                                 += paramLength - 1;
            }
            LOG_MODULE_DATA_DIRECT(";\n");
        }
    }
}

void write_param_names(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    uint32_t nameCount          = 0;
    uint32_t sizeBitPos         = 0;
    uint32_t nameCountBitPos    = 0;
    uint32_t moduleLengthBitPos = 0;
    uint32_t moduleLength       = 0;
    bool     moduleHasNames     = false;
    uint32_t paramCount         = 0;
    uint32_t j                  = 0;
    uint32_t k                  = 0;
    uint32_t labelIndex         = 0;
    uint32_t numLabels          = 0;
    uint32_t paramLength        = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PARAM_NAMES);

    sizeBitPos      = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);

    nameCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);  // Populated later

    LOG_MODULE_DATA("Write param names for location %d\n", location);

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (!module->active) {
            continue;
        }
        paramCount = module->actualParamCount;

        if (paramCount > 0) {
            moduleHasNames = false;

            for (j = 0; j < paramCount; j++) {
                for (labelIndex = 0; labelIndex < MAX_NUM_LABELS; labelIndex++) {
                    if (module->paramNameSet[j][labelIndex] == true) {
                        moduleHasNames = true;
                        break;
                    }
                }

                if (moduleHasNames) {
                    break;
                }
            }

            if (moduleHasNames) {
                nameCount++;
                write_bit_stream(buff, bitPos, 8, module->key.index);

                moduleLengthBitPos = *bitPos;
                write_bit_stream(buff, bitPos, 8, 0);  // Populated later

                moduleLength       = 0;

                for (j = 0; j < paramCount; j++) {
                    numLabels = module->paramNumLabels[j];

                    if (numLabels > 0) {
                        paramLength   = 1 + (numLabels * PROTOCOL_PARAM_NAME_SIZE);

                        write_bit_stream(buff, bitPos, 8, 1);  // isString
                        write_bit_stream(buff, bitPos, 8, paramLength);
                        write_bit_stream(buff, bitPos, 8, j);  // paramIndex

                        moduleLength += 3;

                        for (labelIndex = 0; labelIndex < numLabels; labelIndex++) {
                            LOG_MODULE_DATA("Write param Name: ");

                            for (k = 0; k < PROTOCOL_PARAM_NAME_SIZE; k++) {
                                if ((module->paramName[j][labelIndex][k] >= 0x20) && (module->paramName[j][labelIndex][k] <= 0x7f)) {
                                    LOG_MODULE_DATA_DIRECT("%c", module->paramName[j][labelIndex][k]);
                                } else {
                                    LOG_MODULE_DATA_DIRECT(" ");
                                }
                                write_bit_stream(buff, bitPos, 8, module->paramName[j][labelIndex][k]);
                            }

                            LOG_MODULE_DATA_DIRECT(";\n");
                        }

                        moduleLength += paramLength - 1;
                    }
                }

                write_bit_stream(buff, &moduleLengthBitPos, 8, moduleLength);
            }
        }
    }

    *bitPos = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &nameCountBitPos, 8, nameCount);
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void parse_module_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset) {
    tModuleKey key   = {0};
    uint32_t   i     = 0;
    char       name[CLAVIA_NAME_SIZE + 1];

    LOG_MODULE_DATA("Module names\n");

    key.slot     = slot;
    key.location = read_bit_stream(buff, subOffset, 2);
    read_bit_stream(buff, subOffset, 6);
    LOG_MODULE_DATA("Location 0x%x\n", key.location);
    uint32_t   items = read_bit_stream(buff, subOffset, 8);
    LOG_MODULE_DATA("Items %u\n", items);

    for (i = 0; i < items; i++) {
        key.index = read_bit_stream(buff, subOffset, 8);
        read_clavia_string(buff, subOffset, name, sizeof(name));

        tModule * module = get_module(key);

        if (module != NULL) {
            COPY_STRING(module->name, name);
        }
    }
}

void write_module_names(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos) {
    uint32_t moduleCount     = 0;
    uint32_t sizeBitPos      = 0;
    uint32_t itemCountBitPos = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_MODULE_NAMES);

    sizeBitPos      = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    write_bit_stream(buff, bitPos, 2, location);
    write_bit_stream(buff, bitPos, 6, 0);  // Unknown/reserved

    itemCountBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 8, 0);  // Populated later

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (!module->active) {
            continue;
        }

        if (module->name[0] != '\0') {
            moduleCount++;
            write_bit_stream(buff, bitPos, 8, module->key.index);
            write_clavia_string(buff, bitPos, module->name);
        }
    }

    *bitPos         = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &itemCountBitPos, 8, moduleCount);
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void write_patch_notes(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    uint32_t sizeBitPos = 0;
    uint32_t i          = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PATCH_NOTES);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);  // Populated later

    for (i = 0; i < gPatchNotesSize[slot]; i++) {
        write_bit_stream(buff, bitPos, 8, gPatchNotes[slot][i]);
    }

    *bitPos    = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void write_current_note_2(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_CURRENT_NOTE_2);
    write_bit_stream(buff, bitPos, 16, 6);
    write_bit_stream(buff, bitPos, 8, 0x80);
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, 0x20);
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, 0x00);
}

void write_current_note_2_perf(uint32_t slot, uint8_t * buff, uint32_t * bitPos) {
    uint32_t voiceCount = gPatchDescr[slot].voiceCount;
    uint32_t n          = voiceCount + 1;   // voice list entries = voiceCount + 1
    uint32_t sizeBitPos = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_CURRENT_NOTE_2);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);

    write_bit_stream(buff, bitPos, 7, 64);  // last note: MIDI 64, no attack/release
    write_bit_stream(buff, bitPos, 7, 0);
    write_bit_stream(buff, bitPos, 7, 0);

    write_bit_stream(buff, bitPos, 5, voiceCount);  // count-1 field = voiceCount

    for (uint32_t i = 0; i < n; i++) {
        write_bit_stream(buff, bitPos, 7, 64);
        write_bit_stream(buff, bitPos, 7, 0);
        write_bit_stream(buff, bitPos, 7, 0);
    }

    *bitPos    = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));

    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void store_note2(uint32_t slot, uint8_t * buff, uint32_t * bitPos, uint32_t count) {
    uint32_t safeCount = 0;
    uint32_t i         = 0;

    if (slot >= MAX_SLOTS) {
        return;
    }
    safeCount        = (count < sizeof(gNote2[slot])) ? count : (uint32_t)sizeof(gNote2[slot]);

    for (i = 0; i < safeCount; i++) {
        gNote2[slot][i] = read_bit_stream(buff, bitPos, 8);
    }

    gNote2Size[slot] = safeCount;
}

void store_patch_notes(uint32_t slot, uint8_t * buff, uint32_t * bitPos, uint32_t count) {
    uint32_t notesSize = 0;
    uint32_t i         = 0;

    if (slot >= MAX_SLOTS) {
        return;
    }
    notesSize             = count;

    if (notesSize > sizeof(gPatchNotes[0]) - 1) {
        LOG_ERROR("Patch notes size %u exceeds limit\n", notesSize);
        notesSize = (uint32_t)(sizeof(gPatchNotes[0]) - 1);
    }
    gPatchNotesSize[slot] = 0;
    memset(gPatchNotes[slot], 0, sizeof(gPatchNotes[0]));

    for (i = 0; i < notesSize; i++) {
        gPatchNotes[slot][i] = read_bit_stream(buff, bitPos, 8);
    }

    gPatchNotesSize[slot] = notesSize;
}

int parse_synth_settings(uint8_t * buff, int length) {
    uint32_t bitPos = 0;
    int      i      = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }
    read_clavia_string(buff, &bitPos, gSynthSettings.name, sizeof(gSynthSettings.name));

    gGlobalSettings.perfMode         = read_bit_stream(buff, &bitPos, 1);
    read_bit_stream(buff, &bitPos, 5);  // Unused
    gSynthSettings.patchSortMode     = read_bit_stream(buff, &bitPos, 2);
    read_bit_stream(buff, &bitPos, 6);  // perfSortMode - unused
    gSynthSettings.perfSortMode      = read_bit_stream(buff, &bitPos, 2);
    gSynthSettings.perfBank          = read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.perfLocation      = read_bit_stream(buff, &bitPos, 8);
    gSynthSettings.memoryProtect     = read_bit_stream(buff, &bitPos, 1);
    read_bit_stream(buff, &bitPos, 7);

    for (i = 0; i < 4; i++) {
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
              gGlobalSettings.perfMode, gSynthSettings.perfBank, gSynthSettings.perfLocation);
    LOG_DEBUG("Patch Sort Mode=%u Perf Sort Mode=%u\n",
              gSynthSettings.patchSortMode, gSynthSettings.perfSortMode);

    return EXIT_SUCCESS;
}

int parse_performance_settings(uint8_t * buff, int length) {
    uint32_t bitPos = 0;
    int      i      = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }
    memset(gGlobalSettings.perfName, 0, sizeof(gGlobalSettings.perfName));
    read_clavia_string(buff, &bitPos, gGlobalSettings.perfName, sizeof(gGlobalSettings.perfName));
    LOG_DEBUG("Performance Name     = '%s'\n", gGlobalSettings.perfName);

    read_bit_stream(buff, &bitPos, 8);                                           // Regular val of 17?
    read_bit_stream(buff, &bitPos, 8);
    LOG_DEBUG("Keyboard Range Enab  = %u\n", read_bit_stream(buff, &bitPos, 8)); // Regular val of 82 for standard mode and 87 for performance mode? Seen 0x74 for standard an 0x80 for perf too. Seems to be keyboard range enabled!
    read_bit_stream(buff, &bitPos, 8);
    read_bit_stream(buff, &bitPos, 4);
    gGlobalSettings.selectedSlot       = read_bit_stream(buff, &bitPos, 2);
    LOG_DEBUG("SelectedSlot         = %u\n", gGlobalSettings.selectedSlot);
    read_bit_stream(buff, &bitPos, 2);
    LOG_DEBUG("RangeEnable          = %u\n", read_bit_stream(buff, &bitPos, 8));
    gGlobalSettings.masterClock        = read_bit_stream(buff, &bitPos, 8);  // This is used whether we're in performance mode or not. It's shared with non-perf mode
    LOG_DEBUG("MasterClock          = %u\n", gGlobalSettings.masterClock);
    LOG_DEBUG("KeyboardSplit        = %u\n", read_bit_stream(buff, &bitPos, 8));
    gGlobalSettings.masterClockRunning = read_bit_stream(buff, &bitPos, 8);
    LOG_DEBUG("MasterClockRun       = %u\n", gGlobalSettings.masterClockRunning);
    read_bit_stream(buff, &bitPos, 8);
    read_bit_stream(buff, &bitPos, 8);

    for (i = 0; i < MAX_SLOTS; i++) {
        read_clavia_string(buff, &bitPos, gGlobalSettings.slot[i].patchName, sizeof(gGlobalSettings.slot[i].patchName));
        gGlobalSettings.slot[i].enabled       = read_bit_stream(buff, &bitPos, 8);
        gPerfSettings.slot[i].keyboardEnabled = (uint8_t)read_bit_stream(buff, &bitPos, 8);
        gPerfSettings.slot[i].holdEnabled     = (uint8_t)read_bit_stream(buff, &bitPos, 8);
        read_bit_stream(buff, &bitPos, 8); // Bank index
        read_bit_stream(buff, &bitPos, 8); // Patch index
        gPerfSettings.slot[i].rangeLower      = (uint8_t)read_bit_stream(buff, &bitPos, 8);
        gPerfSettings.slot[i].rangeUpper      = (uint8_t)read_bit_stream(buff, &bitPos, 8);
        LOG_DEBUG("Slot %d:\n", i);
        LOG_DEBUG("  PatchName         = '%s'\n", gGlobalSettings.slot[i].patchName);
        LOG_DEBUG("  Active            = %u\n", gGlobalSettings.slot[i].enabled);
        LOG_DEBUG("  Key               = %u\n", gPerfSettings.slot[i].keyboardEnabled);
        LOG_DEBUG("  Hold              = %u\n", gPerfSettings.slot[i].holdEnabled);
        LOG_DEBUG("  BankIndex         = %u\n", 0);
        LOG_DEBUG("  PatchIndex        = %u\n", 0);
        LOG_DEBUG("  RangeLower        = %u\n", gPerfSettings.slot[i].rangeLower);
        LOG_DEBUG("  RangeUpper        = %u\n", gPerfSettings.slot[i].rangeUpper);

        read_bit_stream(buff, &bitPos, 8);
        read_bit_stream(buff, &bitPos, 8);
        read_bit_stream(buff, &bitPos, 8);
    }

    return EXIT_SUCCESS;
}

int parse_midi_cc(uint8_t * buff, int length) {
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

int parse_patch_version(uint8_t * buff, int length) {
    uint32_t bitPos  = 0;
    uint8_t  slot    = 0;
    uint8_t  version = 0;

    if ((buff == NULL) || (length < 2)) {
        return EXIT_FAILURE;
    }
    slot    = read_bit_stream(buff, &bitPos, 8);
    version = read_bit_stream(buff, &bitPos, 8);

    if (slot < MAX_SLOTS) {
        if (version != gGlobalSettings.slot[slot].patchVersion) {
            gGlobalSettings.slot[slot].patchVersion = version;
        }
    } else if (slot == MAX_SLOTS) {
        if (version != gGlobalSettings.perfVersion) {
            gGlobalSettings.perfVersion = version;
        }
    } else {
        return EXIT_FAILURE;
    }
    LOG_DEBUG("Parsed patch version slot %u = 0x%02x or %u\n", slot, version, version);
    return EXIT_SUCCESS;
}

int parse_patch(uint32_t slot, uint8_t * buff, int length) {
    uint32_t bitOffset = 0;
    uint8_t  type      = 0;
    int16_t  count     = 0;
    uint32_t subOffset = 0;

    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }

    while (BIT_TO_BYTE(bitOffset) < length) {
        type      = read_bit_stream(buff, &bitOffset, 8);
        count     = 0;
        subOffset = 0;

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

        LOG_MODULE_DATA("Type = 0x%x, Count = %d\n", type, count);

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

int parse_perf(uint8_t * buff, int length) {
    uint32_t bitOffset   = 0;
    uint32_t currentSlot = 0;
    uint8_t  headerType  = 0;
    uint8_t  selSlotByte = 0;
    uint32_t selSlot     = 0;
    uint8_t  type        = 0;
    int16_t  count       = 0;
    uint32_t subOffset   = 0;
    uint32_t slot        = 0;
    uint8_t  enabled     = 0;
    int      j           = 0;

    if ((buff == NULL) || (length <= 0)) {
        return EXIT_FAILURE;
    }
    headerType                         = (uint8_t)read_bit_stream(buff, &bitOffset, 8);

    if (headerType != 0x11) {
        LOG_ERROR("parse_perf: expected perf header 0x11, got 0x%02x\n", headerType);
        return EXIT_FAILURE;
    }
    read_bit_stream(buff, &bitOffset, 8);                                    // unknown (0x00)
    LOG_DEBUG("MasterVolume        = %u\n", read_bit_stream(buff, &bitOffset, 8));
    read_bit_stream(buff, &bitOffset, 8);                                    // unknown (0x00)
    selSlotByte                        = (uint8_t)read_bit_stream(buff, &bitOffset, 8);
    selSlot                            = (uint32_t)(selSlotByte / 4);

    if (selSlot < MAX_SLOTS) {
        gSlot = selSlot;
    }
    read_bit_stream(buff, &bitOffset, 8);                                    // unknown (0x00)
    gGlobalSettings.masterClock        = (uint8_t)read_bit_stream(buff, &bitOffset, 8);
    read_bit_stream(buff, &bitOffset, 8);                                    // unknown (0x00)
    gGlobalSettings.masterClockRunning = (uint8_t)read_bit_stream(buff, &bitOffset, 8);
    read_bit_stream(buff, &bitOffset, 8);                                    // 0x00
    read_bit_stream(buff, &bitOffset, 8);                                    // 0x00

    for (slot = 0; slot < MAX_SLOTS; slot++) {
        read_clavia_string(buff, &bitOffset, gGlobalSettings.slot[slot].patchName, sizeof(gGlobalSettings.slot[slot].patchName));

        LOG_DEBUG("Slot %u name '%s'\n", slot, gGlobalSettings.slot[slot].patchName);
        enabled                            = (uint8_t)read_bit_stream(buff, &bitOffset, 8); // IsSlotEnabled
        gGlobalSettings.slot[slot].enabled = enabled;

        for (j = 1; j < 10; j++) {
            read_bit_stream(buff, &bitOffset, 8);                            // remaining suffix bytes
        }
    }

    for (slot = 0; slot < MAX_SLOTS; slot++) {
        database_delete_cables_by_slot(slot);
        database_delete_modules_by_slot(slot);
        gMorphCount[slot]      = 0;
        gNote2Size[slot]       = 0;
        gControllerCount[slot] = 0;
        gPatchNotesSize[slot]  = 0;
        memset(&gPatchDescr[slot], 0, sizeof(gPatchDescr[0]));
        memset(&gKnobArray[slot], 0, sizeof(gKnobArray[0]));
        memset(gNote2[slot], 0, sizeof(gNote2[0]));
        memset(&gControllerArray[slot], 0, sizeof(gControllerArray[0]));
        memset(gPatchNotes[slot], 0, sizeof(gPatchNotes[0]));
    }

    while (BIT_TO_BYTE(bitOffset) < length) {
        type      = (uint8_t)read_bit_stream(buff, &bitOffset, 8);
        count     = 0;
        subOffset = 0;

        if (type != SUB_RESPONSE_SEL_PARAM_PAGE) {
            count = (int16_t)read_bit_stream(buff, &bitOffset, 16);

            if (count < 0) {
                LOG_ERROR("parse_perf: negative count %d for type 0x%02x, aborting\n", count, type);
                return EXIT_FAILURE;
            }

            if (BIT_TO_BYTE(bitOffset) + count > length) {
                LOG_ERROR("parse_perf: count %d for type 0x%02x exceeds buffer, aborting\n", count, type);
                return EXIT_FAILURE;
            }
        }
        subOffset = bitOffset;

        if ((type == SUB_RESPONSE_PATCH_NOTES) && (count == 0)) {
            currentSlot++;
            bitOffset += SIGNED_BYTE_TO_BIT(count);
            continue;
        }

        switch (type) {
            case SUB_RESPONSE_MODULE_LIST:
                parse_module_list(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CABLE_LIST:
                parse_cable_list(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_PARAM_LIST:
                parse_param_list(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_MODULE_NAMES:
                parse_module_names(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_PARAM_NAMES:
                parse_param_names(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_SEL_PARAM_PAGE:
                read_bit_stream(buff, &bitOffset, 8);
                break;

            case SUB_RESPONSE_PATCH_DESCRIPTION:
                parse_patch_descr(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_MORPH_PARAMS:
                parse_morph_params(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_KNOBS:
                parse_knobs(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CONTROLLERS:
                parse_controllers(currentSlot, buff, &subOffset);
                break;

            case SUB_RESPONSE_CURRENT_NOTE_2:
                store_note2(currentSlot, buff, &subOffset, (uint32_t)count);
                break;

            case SUB_RESPONSE_PATCH_NOTES:
                store_patch_notes(currentSlot, buff, &subOffset, (uint32_t)count);
                break;

            case SUB_RESPONSE_GLOBAL_KNOBS:
                parse_global_knobs(buff, &subOffset);
                break;

            default:
                LOG_DEBUG("parse_perf: unprocessed type 0x%02x slot %u\n", type, currentSlot);
                break;
        }
        bitOffset += SIGNED_BYTE_TO_BIT(count);
    }
    return EXIT_SUCCESS;
}

void send_module_move_msg(tModule * module) {
    tMessageContent messageContent = {0};
    uint32_t        slot           = gSlot;

    messageContent.cmd                  = eMsgCmdMoveModule;
    messageContent.slot                 = slot;
    messageContent.moduleData.moduleKey = module->key;
    messageContent.moduleData.row       = module->row;
    messageContent.moduleData.column    = module->column;
    msg_send(&gCommandQueue, &messageContent);
}

void send_param_value(uint32_t slot, tModuleKey moduleKey, uint32_t paramIdx, uint32_t variation, uint32_t value) {
    tMessageContent msg = {0};

    msg.cmd                 = eMsgCmdSetValue;
    msg.slot                = slot;
    msg.paramData.moduleKey = moduleKey;
    msg.paramData.param     = paramIdx;
    msg.paramData.variation = variation;
    msg.paramData.value     = value;
    msg_send(&gCommandQueue, &msg);
}

void send_mode_value(uint32_t slot, tModuleKey moduleKey, uint32_t modeIdx, uint32_t value) {
    tMessageContent msg = {0};

    msg.cmd                = eMsgCmdSetMode;
    msg.slot               = slot;
    msg.modeData.moduleKey = moduleKey;
    msg.modeData.mode      = modeIdx;
    msg.modeData.value     = value;
    msg_send(&gCommandQueue, &msg);
}

void update_module_up_rates(void) {
    uint32_t slot        = gSlot;
    uint32_t location    = gLocation;
    bool     changesMade = false;

    // Reset newUpRate for all modules in this slot/location
    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active) {
            module->newUpRate = 0;
        }
    }

    do {
        changesMade = false;

        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable *   cable         = get_cable_slot(slot, location, i);

            if (cable == NULL || !cable->active) {
                continue;
            }
            tModuleKey fromModuleKey = {cable->key.slot, cable->key.location, cable->key.moduleFromIndex};
            tModuleKey toModuleKey   = {cable->key.slot, cable->key.location, cable->key.moduleToIndex};
            tModule *  fromModule    = get_module(fromModuleKey);
            tModule *  toModule      = get_module(toModuleKey);

            if ((fromModule != NULL) && (toModule != NULL)) {
                tConnectorDir fromConnector = (cable->key.linkType == cableLinkTypeFromInput) ? connectorDirIn : connectorDirOut;
                int           fromConnIndex = find_index_from_io_count(fromModule, fromConnector, cable->key.connectorFromIoCount);
                int           toConnIndex   = find_index_from_io_count(toModule, connectorDirIn, cable->key.connectorToIoCount);

                if ((fromConnIndex != -1) && (toConnIndex != -1) && (toModule->newUpRate == 0)) {
                    if (fromModule->newUpRate == 1 || ((fromModule->connector[fromConnIndex].type == connectorTypeAudio) && (toModule->connector[toConnIndex].type != connectorTypeAudio))) {
                        toModule->newUpRate = 1;
                        changesMade         = true;
                    }
                }
            }
        }
    } while (changesMade);

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && (module->newUpRate != module->upRate)) {
            tMessageContent messageContent = {0};

            module->upRate                      = module->newUpRate;
            messageContent.cmd                  = eMsgCmdSetModuleUpRate;
            messageContent.slot                 = slot;
            messageContent.moduleData.moduleKey = module->key;
            messageContent.moduleData.upRate    = module->upRate;
            msg_send(&gCommandQueue, &messageContent);
        }
    }
}

void write_global_knobs(uint8_t * buff, uint32_t * bitPos) {
    uint32_t sizeBitPos = 0;

    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_GLOBAL_KNOBS);

    sizeBitPos = *bitPos;
    write_bit_stream(buff, bitPos, 16, 0);   // Populated later

    write_bit_stream(buff, bitPos, 16, MAX_NUM_KNOBS);

    for (int i = 0; i < MAX_NUM_KNOBS; i++) {
        tGlobalKnob * gKnob = &gGlobalKnobArray[i];

        write_bit_stream(buff, bitPos, 1, gKnob->assigned ? 1 : 0);

        if (gKnob->assigned) {
            write_bit_stream(buff, bitPos, 2, gKnob->location);
            write_bit_stream(buff, bitPos, 8, gKnob->moduleIndex);
            write_bit_stream(buff, bitPos, 2, gKnob->isLed);
            write_bit_stream(buff, bitPos, 7, gKnob->paramIndex);
            write_bit_stream(buff, bitPos, 2, gKnob->slotIndex);
        }
    }

    *bitPos    = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(*bitPos));
    write_bit_stream(buff, &sizeBitPos, 16, BIT_TO_BYTE(*bitPos - sizeBitPos) - 2);
}

void write_slot_separator(uint8_t * buff, uint32_t * bitPos) {
    write_bit_stream(buff, bitPos, 8, 0x6f);
    write_bit_stream(buff, bitPos, 16, 0);
}

void write_perf_header(uint8_t * buff, uint32_t * bitPos) {
    //char slotName[CLAVIA_NAME_SIZE + 1] = {0};

    write_bit_stream(buff, bitPos, 8, 0x11);                 // constant observed in all perf files
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, 0x50);                 // TODO: use real gMasterVolume once stored; 0x50 is the G2 default
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, (uint8_t)(gSlot * 4)); // selected slot encoding
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, gGlobalSettings.masterClock);
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, gGlobalSettings.masterClockRunning);
    write_bit_stream(buff, bitPos, 8, 0x00);
    write_bit_stream(buff, bitPos, 8, 0x00);

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if (gGlobalSettings.slot[slot].patchName[0] == '\0') {
            write_clavia_string(buff, bitPos, "No name");
        } else {
            write_clavia_string(buff, bitPos, gGlobalSettings.slot[slot].patchName);
        }
        bool has_content = slot_has_modules(slot);

        write_bit_stream(buff, bitPos, 8, has_content ? 0x01 : 0x00);
        write_bit_stream(buff, bitPos, 8, (slot == 0 || has_content) ? 0x01 : 0x00);
        write_bit_stream(buff, bitPos, 8, 0x00);
        write_bit_stream(buff, bitPos, 8, has_content ? 0x01 : 0x00);
        write_bit_stream(buff, bitPos, 8, has_content ? 0x01 : 0x00);
        write_bit_stream(buff, bitPos, 8, 0x00);
        write_bit_stream(buff, bitPos, 8, 0x7f);                              // slot volume (default 127)
        write_bit_stream(buff, bitPos, 8, slot == 0 ? 0x02 : 0x10);
        write_bit_stream(buff, bitPos, 8, 0x00);
        write_bit_stream(buff, bitPos, 8, 0x00);  // TODO - this all needs fixing, based on the parsing in usbscomms..c
    }
}

#ifdef __cplusplus
}
#endif

