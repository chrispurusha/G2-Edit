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
// Notes: Docs/code-notes/g2Prefs.h.md - "// notes §k" refers there.

#ifndef __G2_PREFS_H__
#define __G2_PREFS_H__

#ifdef __cplusplus
extern "C" {
#endif

// notes §1
#define G2_PREFS_APP_NAME    "G2 Alike"

#define G2_PREF_DIAL_MODE    "dialMode"
#define G2_PREF_EDITOR_WIDTH "editorWidth"

// Call once before anything reads a preference. Safe to call more than once.
void g2_plugin_prefs_init(void);

#ifdef __cplusplus
}
#endif

#endif // __G2_PREFS_H__
