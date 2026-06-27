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

#ifndef __MOUSE_PANELS_H__
#define __MOUSE_PANELS_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

bool handle_patch_notes_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_perf_settings_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_patch_params_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_patch_settings_mouse(tCoord coord, tMouseButton mouseButton);

#ifdef __cplusplus
}
#endif

#endif // __MOUSE_PANELS_H__
