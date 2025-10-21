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

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "sysIncludes.h"

void parse_patch_descr(uint8_t * buff, uint32_t * subOffset);
void write_patch_descr(uint8_t * buff, uint32_t * bitPos);
void parse_module_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_module_list(uint32_t slot, uint8_t * buff, uint32_t * bitPos);
void parse_cable_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_cable_list(uint32_t slot, uint8_t * buff, uint32_t * bitPos);
    
#endif // __PROTOCOL_H__
