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

#ifdef __cplusplus
extern "C" {
#endif

#include "moduleResources.h"
#include "moduleResourcesAccess.h"

tCableColour cable_colour_for_connector_type(tConnectorType type) {
    switch (type) {
        case connectorTypeAudio:      return cableColourRed;

        case connectorTypeControl:    return cableColourBlue;

        case connectorTypeLogic:      return cableColourYellow;

        case connectorTypeTurboLogic: return cableColourOrange;

        default:                      return cableColourRed;
    }
}

tConnectorType effective_connector_type(tConnectorType baseType, bool upRate) {
    if (upRate) {
        if (baseType == connectorTypeControl) {
            return connectorTypeAudio;
        }

        if (baseType == connectorTypeLogic) {
            return connectorTypeTurboLogic;
        }
    }
    return baseType;
}

uint32_t array_size_param_location_list(void) {
    return ARRAY_SIZE(paramLocationList);
}

uint32_t array_size_connector_location_list(void) {
    return ARRAY_SIZE(connectorLocationList);
}

uint32_t array_size_mode_location_list(void) {
    return ARRAY_SIZE(modeLocationList);
}

uint32_t array_size_volume_location_list(void) {
    return ARRAY_SIZE(volumeLocationList);
}

const tVolumeMeterConfig * find_volume_meter_config(tVolumeType volumeType) {
    for (uint32_t i = 0; i < ARRAY_SIZE(volumeMeterConfigList); i++) {
        if (volumeMeterConfigList[i].volumeType == volumeType) {
            return &volumeMeterConfigList[i];
        }
    }

    return NULL;
}

const tGraphLocation * find_graph_location(tModuleType moduleType) {
    for (uint32_t i = 0; i < ARRAY_SIZE(graphLocationList); i++) {
        if (graphLocationList[i].moduleType == moduleType) {
            return &graphLocationList[i];
        }
    }

    return NULL;
}

uint32_t array_size_led_location_list(void) {
    return ARRAY_SIZE(ledLocationList);
}

uint32_t array_size_str_map(const char ** strMap) {
    uint32_t i = 0;

    if (strMap != NULL) {
        while (strMap[i]) {
            i++;
        }
    }
    return i;
}

// Called from both threads — caches are pre-warmed by init_module_resource_cache() before USB thread starts.
uint32_t module_param_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_param_location_list(); i++) {
            if (paramLocationList[i].moduleType == moduleType) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

// How many parameters the DEVICE sends for this module type, which is not always how many rows the
// table has. paramTypeCustomData rows are local storage, not wire parameters: SeqNote's Magnifier
// and Octave sit in param slots past the end of what the G2 transmits, and reach the device through
// their own eMsgCmdSetCustomData message instead (see send_custom_data_value() in protocol.c).
//
// Only the patch-parse count check wants this. Everywhere else — init, copy/paste, the sound engine —
// genuinely means "every slot this module uses", which is module_param_count() above.
//
// Called from both threads — caches are pre-warmed by init_module_resource_cache() before USB thread starts.
uint32_t module_device_param_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_param_location_list(); i++) {
            if ((paramLocationList[i].moduleType == moduleType) && (paramLocationList[i].type != paramTypeCustomData)) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

// Called from both threads — caches are pre-warmed by init_module_resource_cache() before USB thread starts.
uint32_t module_connector_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_connector_location_list(); i++) {
            if (connectorLocationList[i].moduleType == moduleType) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

// A connector's direction and type are STATIC per module type: they restate what
// connectorLocationList already holds, which is why tConnector's own fields carry the note
// "Should pull from the location list".
//
// They used to be written only as a side effect of DRAWING, in render_connector_common(). The
// application always draws, so the array was always filled and nothing looked wrong — but
// anything that renders no pixels (the VST3 plug-in, any headless harness) saw a zeroed array,
// and every cable lookup that reads .dir then failed silently. The sound engine's modulation
// inputs simply never connected, which presents as a filter that plays shut rather than as an
// error. Populating at load time makes the data available whether or not anything is drawn.
//
// Only .dir and .type are set here. .coord and .rectangle are genuine geometry — they depend on
// where the module is actually rendered — and remain the renderer's to fill in.
void populate_module_connectors(tModule * module) {
    uint32_t connector = 0;
    uint32_t count     = 0;
    uint32_t i         = 0;

    if (module == NULL) {
        return;
    }
    count = module_connector_count(module->type);

    if (count == 0) {
        return;
    }

    if (count > MAX_NUM_CONNECTORS) {
        LOG_ERROR("MAX_NUM_CONNECTORS needs increasing to >= %u for module type %u\n", count, module->type);
        count = MAX_NUM_CONNECTORS;
    }

    for (i = 0; i < array_size_connector_location_list(); i++) {
        if (connectorLocationList[i].moduleType == module->type) {
            // The renderer's own walk starts from this cached offset, so seeding it here saves it
            // rescanning the list from the top.
            if (module->gotConnectorIndexCache == false) {
                module->connectorIndexCache    = i;
                module->gotConnectorIndexCache = true;
            }
            module->connector[connector].dir  = connectorLocationList[i].direction;
            module->connector[connector].type = connectorLocationList[i].type;
            connector++;

            if (connector >= count) {
                break;
            }
        }
    }
}

uint32_t module_mode_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_mode_location_list(); i++) {
            if (modeLocationList[i].moduleType == moduleType) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

uint32_t module_volume_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_volume_location_list(); i++) {
            if (volumeLocationList[i].moduleType == moduleType) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

// Called from both threads — caches are pre-warmed by init_module_resource_cache() before USB thread starts.
uint32_t module_led_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_led_location_list(); i++) {
            if (ledLocationList[i].moduleType == moduleType && ledLocationList[i].ledType != ledTypePark) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

// Patch Mutator "Exclude From Mutation" default per module type, recovered from the original editor's own defaults,
// original editor's GetDefaultMutationLock() and confirmed against 395 real captured patches (see
// mutator.c). Applied to newly created modules. (Previously also reapplied on every patch reparse
// for old-format patches - removed 2026-07-15 once live writes via SUB_COMMAND_SET_MUTATION_LOCK
// were confirmed working on hardware, since that reapply was clobbering live-toggled bits on
// every resync. Old patches now simply trust whatever's on the wire, same as new ones.)
bool default_mutation_lock(tModuleType moduleType) {
    switch (moduleType) {
        case moduleType4toOut:
        case moduleType2toOut:
        case moduleTypeFxtoIn:
        case moduleType2toIn:
        case moduleType4toIn:
        case moduleTypeCtrlSend:
        case moduleTypePCSend:
        case moduleTypeNoteSend:
        case moduleTypeCtrlRcv:
        case moduleTypeNoteRcv:
        case moduleTypeNoteZone:
        case moduleTypeVocoder:
        case moduleTypeMixStereo:
        case moduleTypeCompLev:
        case moduleTypeCompress:
        case moduleTypePitchTrack:
        case moduleTypeNoteQuant:
        case moduleTypeNoteDet:
        case moduleTypeValSw1to2:
        case moduleTypeValSw2to1:
        case moduleTypePulse:
        case moduleTypeDelay:
        case moduleTypeDlyClock:
        case moduleTypeConstSwT:
        case moduleTypeConstSwM:
        case moduleTypeClkGen:
        case moduleTypeClkDiv:
        case moduleTypeLevAmp:
        case moduleTypeLevAdd:
        case moduleTypeLevScaler:
        case moduleTypeLevConv:
        case moduleTypeRect:
        case moduleTypeShpStatic:
        case moduleTypeWindSw:
        case moduleTypeSwOnOffT:
        case moduleTypeMux8to1X:
        case moduleTypeAutomate:
        case moduleTypeEnvFollow:
        case moduleTypeNoiseGate:
        case moduleTypeMonoKey:
        case moduleTypeRndPattern:
            return true;

        default:
            return false;
    }
}

void init_module_resource_cache(void) {
    tModuleType t = (tModuleType)0;

    for (t = (tModuleType)0; t < moduleTypeMax; t++) {
        module_param_count(t);
        module_device_param_count(t);
        module_connector_count(t);
        module_mode_count(t);
        module_volume_count(t);
        module_led_count(t);
    }
}

#ifdef __cplusplus
}
#endif
