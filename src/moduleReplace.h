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

#ifndef MODULE_REPLACE_H
#define MODULE_REPLACE_H

#include <stdbool.h>
#include <stdint.h>
#include "types.h"

// Replace one module with another from the same group, keeping the cables that have somewhere to
// go. The manual describes the feature on p.82; Docs/module-replace-design.md carries the design.

// Whether this module has a group with a role table, i.e. whether a replacement can be offered.
bool module_can_replace(tModuleType moduleType);

// The other members of this module's group, in the group's own order, EXCLUDING the module itself.
// Returns how many were written.
uint32_t module_replace_candidates(tModuleType moduleType, tModuleType * out, uint32_t max);

// Do it. Returns false and changes nothing if the types are not swappable or the column has no room
// for a taller module.
bool module_replace(tModuleKey key, tModuleType newType);

#endif // MODULE_REPLACE_H
