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

#ifndef __G2_PREFS_H__
#define __G2_PREFS_H__

#ifdef __cplusplus
extern "C" {
#endif

// Settings the plug-in remembers between sessions, through the same SynthLib prefs store the
// application uses (prefs.h) — but under its OWN name, so it gets its own file.
//
// NOT SHARED WITH THE APPLICATION'S, deliberately. prefs.cpp rewrites the whole file on a change, so
// two processes writing the same one — and the standalone editor and a hosted plug-in are very
// likely to be open together — would let a last-writer-wins clobber quietly lose settings. A shared
// dial-mode preference would be a nice touch; it is not worth that.
#define G2_PREFS_APP_NAME    "G2 Alike"

#define G2_PREF_DIAL_MODE    "dialMode"
#define G2_PREF_EDITOR_WIDTH "editorWidth"

// Call once before anything reads a preference. Safe to call more than once.
void g2_plugin_prefs_init(void);

#ifdef __cplusplus
}
#endif

#endif // __G2_PREFS_H__
