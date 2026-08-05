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

#ifndef __MODULE_RESOURCE_ACCESS_H__
#define __MODULE_RESOURCE_ACCESS_H__

#include "sysIncludes.h"
#include "types.h"

extern const char *             clkSyncStrMap[];
extern const char *             noteNameStrMap[];
extern const char *             morphStrMap[];
extern const tRgb               gCableColourMap[];
extern const tRgb               gModuleColourMap[];
extern const tRgb               connectorColourMap[];
extern const tModuleProperties  gModuleProperties[];
extern const tParamLocation     paramLocationList[];
extern const tModeLocation      modeLocationList[];
extern const tConnectorLocation connectorLocationList[];
extern const tVolumeLocation    volumeLocationList[];
extern const tVolumeMeterConfig volumeMeterConfigList[];
extern const tLedLocation       ledLocationList[];
extern const tGraphLocation     graphLocationList[];
extern const char *             patchVolumeStrMap[];
//extern const double             dbLvlMap[];
//extern const double             ADRTimeMap[];
//const double                    pulseLoTime[];
extern const char *             dbLvlStrMap[];
extern const char *             ADRTimeStrMap[];
extern const char *             filter_resonanceStrMap[];
extern const char *             pulseLoTimeStrMap[];
extern const char *             freq_shift_hiStrMap[];
extern const char *             freq_shift_loStrMap[];
extern const char *             freq_shift_subStrMap[];
extern const char *             patch_settings_glideStrMap[];

tCableColour cable_colour_for_connector_type(tConnectorType type);
// A module running at the higher (audio) bandwidth promotes its Control connectors to Audio and
// its Logic connectors to TurboLogic (orange) — see render_connector_common()'s own comment
// (moduleGraphics.cpp) for the manual/decompiled-source references this is confirmed against.
// Audio connectors are never affected; upRate has no effect when false. Shared by both the
// connector-hole rendering itself and cable-creation's "inherit the source connector's current
// colour" logic (mouseHandle.c), so the two can never disagree.
tConnectorType effective_connector_type(tConnectorType baseType, bool upRate);
const tVolumeMeterConfig * find_volume_meter_config(tVolumeType volumeType);
const tGraphLocation * find_graph_location(tModuleType moduleType);
uint32_t array_size_param_location_list(void);
uint32_t array_size_connector_location_list(void);
uint32_t array_size_mode_location_list(void);
uint32_t array_size_volume_location_list(void);
uint32_t array_size_led_location_list(void);
uint32_t array_size_str_map(const char ** strMap);
uint32_t module_param_count(tModuleType moduleType);
uint32_t module_connector_count(tModuleType moduleType);
uint32_t module_mode_count(tModuleType moduleType);
uint32_t module_volume_count(tModuleType moduleType);
uint32_t module_led_count(tModuleType moduleType);
bool default_mutation_lock(tModuleType moduleType);
void init_module_resource_cache(void);

#endif // __MODULE_RESOURCE_ACCESS_H__
