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

#ifndef __MODULE_RESOURCE_ACCESS_H__
#define __RESOURCE_ACCESS_H__

#include "sysIncludes.h"
#include "types.h"

extern const tRgb               cableColourMap[];
extern const tRgb               gModuleColourMap[];
extern const tRgb               connectorColourMap[];
extern const tModuleProperties  gModuleProperties[];
extern const tParamLocation     paramLocationList[];
extern const tParamLocation     modeLocationList[];
extern const tConnectorLocation connectorLocationList[];

uint32_t array_size_param_location_list(void);
uint32_t array_size_mode_location_list(void);
uint32_t array_size_connector_location_list(void);
uint32_t module_param_count(tModuleType moduleType);
uint32_t module_mode_count(tModuleType moduleType);
uint32_t module_connector_count(tModuleType moduleType);

#endif // __MODULE_RESOURCE_ACCESS_H__
