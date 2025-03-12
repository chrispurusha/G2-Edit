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

#ifndef __MODULE_GRAPHICS_H__
#define __MODULE_GRAPHICS_H__

#include "sysIncludes.h"
#include "types.h"

void render_module(tModule * module);
void render_modules(void);
void render_cables(void);
void calculate_module_bounds(double * xEndMax, double * yEndMax, tRectangle moduleArea);
void render_cable_from_to(tConnector from, tConnector to);
tRectangle module_area(void);

#endif // __MODULE_GRAPHICS_H__