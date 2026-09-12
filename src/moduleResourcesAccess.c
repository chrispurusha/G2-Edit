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
// Notes: Docs/code-notes/moduleResourcesAccess.c.md - "// notes §k" refers there.

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
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

uint32_t array_size_label_location_list(void) {
    return ARRAY_SIZE(labelLocationList);
}

uint32_t array_size_display_location_list(void) {
    return ARRAY_SIZE(displayLocationList);
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

// notes §1
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

// notes §2
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

uint32_t radio_columns(uint32_t buttonCount) {
    return (buttonCount <= RADIO_MAX_COLUMNS) ? buttonCount : RADIO_MAX_COLUMNS;
}

uint32_t radio_rows(uint32_t buttonCount) {
    uint32_t columns = radio_columns(buttonCount);

    if (columns == 0) {
        return 0;
    }
    return (buttonCount + (columns - 1)) / columns;
}

const char * radio_caption(tModule * module, uint32_t paramIndex, uint32_t buttonIndex, const char ** strMap) {
    static const char * fallback = "?";

    if (  (module != NULL) && (paramIndex < MAX_NUM_PARAMETERS) && (buttonIndex < MAX_NUM_LABELS)
       && module->paramNameSet[paramIndex][buttonIndex] && (module->paramName[paramIndex][buttonIndex][0] != '\0')) {
        return module->paramName[paramIndex][buttonIndex];
    }

    if ((strMap != NULL) && (buttonIndex < array_size_str_map(strMap))) {
        return strMap[buttonIndex];
    }
    return fallback;
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

        // notes §3
        if (count > MAX_NUM_MODES) {
            LOG_ERROR("MAX_NUM_MODES needs increasing to >= %u for module type %u\n", count, moduleType);
            EXIT_IN_DEBUG();
            count = MAX_NUM_MODES;
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

// notes §4
uint32_t module_led_row_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_led_location_list(); i++) {
            if (ledLocationList[i].moduleType == moduleType) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

// How many LEDs a module drives from ONE multi-stream value, or 0 if it has no such group. The
// multi stream (0x3a) carries a 16-bit value per group; where the group holds several LEDs, the bits
// of that value are the LEDs — see CPanel::Blink in the reference, which spreads it a bit at a time.
uint32_t module_multibit_led_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        for (int i = 0; i < array_size_led_location_list(); i++) {
            if (ledLocationList[i].moduleType == moduleType && ledLocationList[i].ledType == ledTypeMultiBit) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

uint32_t module_led_count(tModuleType moduleType) {
    static uint32_t cache[moduleTypeMax]      = {0};
    static bool     validCache[moduleTypeMax] = {0};

    if (validCache[moduleType] == false) {
        uint32_t count = 0;

        // notes §5
        for (int i = 0; i < array_size_led_location_list(); i++) {
            if (ledLocationList[i].moduleType == moduleType && ledLocationList[i].ledType == ledTypeYes) {
                count++;
            }
        }

        cache[moduleType]      = count;
        validCache[moduleType] = true;
    }
    return cache[moduleType];
}

// notes §6
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

uint32_t array_size_module_colour_map(void) {
    return (uint32_t)(sizeof(gModuleColourMap) / sizeof(gModuleColourMap[0]));
}

// ── Palette groups ──────────────────────────────────────────────────────────

uint32_t array_size_palette_list(void) {
    return (uint32_t)(sizeof(gPaletteList) / sizeof(gPaletteList[0]));
}

// The modules in one palette group, in the order the group offers them. Returns how many were
// written. A module may appear in two groups (NoteDet is in In/Out and in MIDI), which is why the
// table is a list of pairs and this is a filter over it rather than a lookup.
uint32_t palette_group_modules(tPaletteGroup group, tModuleType * out, uint32_t max) {
    uint32_t count = 0;
    uint32_t i     = 0;

    if (out == NULL) {
        return 0;
    }

    for (i = 0; (i < array_size_palette_list()) && (count < max); i++) {
        if (gPaletteList[i].group == group) {
            out[count] = gPaletteList[i].moduleType;
            count++;
        }
    }

    return count;
}

const char * palette_group_name(tPaletteGroup group) {
    if (group >= palGroupCount) {
        return "";
    }
    return gPaletteGroupName[group];
}

// ── Module groups and replacement roles ─────────────────────────────────────

tModuleGroup module_group(tModuleType moduleType) {
    if (moduleType >= moduleTypeMax) {
        return moduleGroupNone;
    }
    return gModuleProperties[moduleType].group;
}

uint32_t array_size_module_role_list(void) {
    return (uint32_t)(sizeof(gModuleRoleList) / sizeof(gModuleRoleList[0]));
}

// The role a module's connector or parameter fills, or NULL if it fills none. `index` counts within
// the connector's own direction for the two connector kinds, matching tCableKey's io counts.
const char * module_role_for(tModuleType moduleType, tRoleKind kind, uint32_t index) {
    tModuleGroup group = module_group(moduleType);
    uint32_t     i     = 0;

    if (group == moduleGroupNone) {
        return NULL;
    }

    for (i = 0; i < array_size_module_role_list(); i++) {
        const tModuleRole * row = &gModuleRoleList[i];

        if (  (row->group == group) && (row->kind == kind)
           && (row->moduleType == moduleType) && (row->index == index)) {
            return row->role;
        }
    }

    return NULL;
}

// The reverse: which connector or parameter of `moduleType` fills `role`. MODULE_ROLE_NONE when the
// module has no counterpart for it — which is the case the replace has to drop a cable for.
uint32_t module_index_for_role(tModuleType moduleType, tRoleKind kind, const char * role) {
    tModuleGroup group = module_group(moduleType);
    uint32_t     i     = 0;

    if ((role == NULL) || (group == moduleGroupNone)) {
        return MODULE_ROLE_NONE;
    }

    for (i = 0; i < array_size_module_role_list(); i++) {
        const tModuleRole * row = &gModuleRoleList[i];

        if (  (row->group == group) && (row->kind == kind)
           && (row->moduleType == moduleType) && (strcmp(row->role, role) == 0)) {
            return row->index;
        }
    }

    return MODULE_ROLE_NONE;
}

// Whether this module can be replaced at all: it needs a group AND that group needs a role table,
// since without one a replace could only move cables by raw index, which is the mechanical
// behaviour the whole feature exists to avoid.
bool module_group_has_roles(tModuleGroup group) {
    uint32_t i = 0;

    if (group == moduleGroupNone) {
        return false;
    }

    for (i = 0; i < array_size_module_role_list(); i++) {
        if (gModuleRoleList[i].group == group) {
            return true;
        }
    }

    return false;
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
