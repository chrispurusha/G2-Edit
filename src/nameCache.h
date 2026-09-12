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
// Notes: Docs/code-notes/nameCache.h.md - "// notes §k" refers there.

#ifndef NAME_CACHE_H
#define NAME_CACHE_H

#include <stdbool.h>

// notes §1

// Writes both name tables to cache.txt. Call after a sweep that ran to completion.
void name_cache_save(void);

// Fills both name tables from cache.txt. True if anything was restored.
bool name_cache_load(void);

// Whether the last saved sweep finished. A sweep interrupted by a disconnect leaves this false, so
// the next run knows the tables it just loaded are partial and re-reads them rather than trusting
// a list with holes in it.
bool name_cache_is_complete(void);
void name_cache_set_complete(bool complete);

#endif // NAME_CACHE_H
