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

#ifndef NAME_CACHE_H
#define NAME_CACHE_H

#include <stdbool.h>

// ── The bank name tables, remembered between runs ───────────────────────────────────────────────
//
// WHY: send_list_names_sweep() reads every populated patch and performance name off the instrument,
// and that takes EIGHT SECONDS — measured 2026-08-29 at 8,012 ms against 116 ms for the patch data
// itself, so it is 98% of the wait before the editor considers itself ready. The names feed the
// Load/Store/Delete pickers and nothing else. Remembering them means only the first run pays.
//
// The same idea SynthEdit already uses for the Voyager and Z1, and the same store: SynthLib's
// prefs.cpp keeps cache.txt beside prefs.txt precisely so that regenerable data never sits in the
// file holding real settings.
//
// WHAT THE CACHE IS NOT ALLOWED TO DO. It populates the PICKER and nothing else. Every destructive
// operation — store, delete, load — already asks the instrument what is really at that location
// first (peek_store_target/peek_delete_target/peek_load_target in usbComms.c), and that must stay
// true: the G2's contents change from its own front panel and from other editors, so a remembered
// name is a display convenience and never evidence that a location is free or occupied.

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
