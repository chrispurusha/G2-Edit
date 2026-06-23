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

#ifndef __DATABASE_H__
#define __DATABASE_H__

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"

tModule * get_module(tModuleKey key);
tModule * get_module_slot(uint32_t slot, uint32_t location, uint32_t index);
void write_module(tModuleKey key, tModule * module);
void delete_module(tModuleKey key);
void dump_modules(void);
void database_clear_modules(void);
void database_delete_modules_by_slot(uint32_t slot);
tCable * get_cable_slot(uint32_t slot, uint32_t location, uint32_t index);
tCable * get_cable(tCableKey key);
void write_cable(tCableKey key, tCable * cable);
void delete_cable(tCableKey key);
void dump_cables(void);
void database_clear_cables(void);
int find_io_count_from_index(tModule * module, tConnectorDir dir, int index);
int find_index_from_io_count(tModule * module, tConnectorDir dir, int targetCount);
void database_delete_cables_by_slot(uint32_t slot);
void init_database(void);

#endif // __DATABASE_H__
