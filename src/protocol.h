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

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "sysIncludes.h"

void read_clavia_string(uint8_t * buff, uint32_t * bitPos, char * name, int nameSize);
void write_clavia_string(uint8_t * buff, uint32_t * bitPos, const char * name);
void parse_patch_descr(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_patch_descr(uint32_t slot, uint8_t * buff, uint32_t * bitPos);
void parse_module_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_module_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos);
void parse_cable_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_cable_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos);
void parse_param_list(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_param_list(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos, uint32_t numVariations);
void parse_morph_params(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_morph_params(uint32_t slot, uint8_t * buff, uint32_t * bitPos, uint32_t numVariations);
void parse_knobs(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void parse_global_knobs(uint8_t * buff, uint32_t * bitPos);
void write_knobs(uint32_t slot, uint8_t * buff, uint32_t * bitPos);
void parse_controllers(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_controllers(uint32_t slot, uint8_t * buff, uint32_t * bitPos);
void parse_param_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_param_names(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos);
void parse_module_names(uint32_t slot, uint8_t * buff, uint32_t * subOffset);
void write_module_names(uint32_t slot, tLocation location, uint8_t * buff, uint32_t * bitPos);
void write_patch_notes(uint32_t slot, uint8_t * buff, uint32_t * bitPos);
void write_current_note_2(uint32_t slot, uint8_t * buff, uint32_t * bitPos);
void send_module_move_msg(tModule * module);
void send_param_value(uint32_t slot, tModuleKey moduleKey, uint32_t paramIdx, uint32_t variation, uint32_t value);
void send_mode_value(uint32_t slot, tModuleKey moduleKey, uint32_t modeIdx, uint32_t value);
void update_module_up_rates(void);

#endif // __PROTOCOL_H__
