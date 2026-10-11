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

#ifndef __SLOT_SETS_H__
#define __SLOT_SETS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool file_image_body(const uint8_t * buff, int64_t size, int64_t * body, uint8_t * type); // notes §2
bool perf_image_parse(const uint8_t * buff, int64_t size);                                // notes §2
void perf_mode_set(bool perf);                                                            // notes §1
void slot_set_other_take(const uint8_t * image, size_t length, const char * perfName);    // notes §3

#ifdef __cplusplus
}
#endif

#endif // __SLOT_SETS_H__
