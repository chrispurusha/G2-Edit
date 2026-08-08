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

#ifndef __CANVAS_COORDS_H__
#define __CANVAS_COORDS_H__

#include "sysIncludes.h"
#include "types.h"

// Canvas coordinate arithmetic, with no window system in it.
//
// This lived in mouseHandle.c, which is the most GLFW-bound file in the project — but the maths
// itself only ever needed module_area(), the scroll offsets and the zoom factor, none of which know
// what a window is. Splitting it out lets the VST3 plug-in convert a mouse position the same way the
// application does, instead of keeping a second copy that could drift.
//
// Takes a coordinate already in the canvas's logical units (what get_global_gui_scaled_mouse_coord()
// produces) and returns the position within the scrolled, zoomed module area.
void convert_mouse_coord_to_module_area_coord(tCoord * targetCoord, tCoord coord);

// Which module grid square a coordinate falls in. Was in menus.c; moved for the same reason.
void convert_mouse_coord_to_module_column_row(uint32_t * column, uint32_t * row, tCoord coord);

#endif // __CANVAS_COORDS_H__
