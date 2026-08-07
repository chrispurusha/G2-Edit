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

#ifndef __G2_PATCH_H__
#define __G2_PATCH_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Read a .pch2 from disk into the database at `slot`. Offline only, no device involved.
// Returns false for a missing file, a failed CRC, or a performance file rather than a patch.
bool g2_plugin_load_patch(const char * filepath, uint32_t slot);

#ifdef __cplusplus
}
#endif

#endif
