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

#ifndef __DATABASE_H__
#define __DATABASE_H__

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"

bool read_module(tModuleKey key, tModule * module);
void write_module(tModuleKey key, tModule * module);
void delete_module(tModuleKey key, tDoFree doFree);
void dump_modules(void);
void reset_walk_module(void);
bool walk_next_module(tModule * module);
uint32_t module_height(uint32_t type);
char * module_type_name(uint32_t type);
bool read_cable(tCableKey key, tCable * cable);
void write_cable(tCableKey key, tCable * cable);
void delete_cable(tCableKey key);
void dump_cables(void);
void reset_walk_cable(void);
bool walk_next_cable(tCable * cable);
void database_clear_modules(void);
void database_clear_cables(void);
int find_io_count_from_index(tModule * module, tConnectorDir dir, int index);
int find_index_from_io_count(tModule * module, tConnectorDir dir, int targetCount);
void allocate_module_parameters(tModule * module, uint32_t paramCount);
void allocate_module_connectors(tModule * module, uint32_t connectorCount);

#endif // __DATABASE_H__
