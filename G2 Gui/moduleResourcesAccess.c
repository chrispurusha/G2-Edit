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

#ifdef __cplusplus
extern "C" {
#endif

#include "moduleResources.h"
#include "moduleResourcesAccess.h"

uint32_t array_size_param_location_list(void) { // Todo: move to a module resources source file when it's created
    return ARRAY_SIZE(paramLocationList);
}

uint32_t array_size_mode_location_list(void) { // Todo: move to a module resources source file when it's created
    return ARRAY_SIZE(modeLocationList);
}

uint32_t array_size_connector_location_list(void) { // Todo: move to a module resources source file when it's created
    return ARRAY_SIZE(connectorLocationList);
}

#ifdef __cplusplus
}
#endif
