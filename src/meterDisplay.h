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
#ifndef __METER_DISPLAY_H__
#define __METER_DISPLAY_H__

#include "sysIncludes.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// What the canvas draws for a level meter and a lamp, smoothed from the value the G2 or the engine sent
uint32_t meter_display_level(tModuleKey key, uint32_t leg, uint32_t value);
uint32_t meter_display_lamp(tModuleKey key, uint32_t lamp, uint32_t value);
bool meter_display_take_settling(void);    // a meter or lamp is still catching up: draw again

#ifdef __cplusplus
}
#endif

#endif // __METER_DISPLAY_H__
