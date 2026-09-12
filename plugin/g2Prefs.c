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
// Notes: Docs/code-notes/g2Prefs.c.md - "// notes §k" refers there.

#include "sysIncludes.h"
#include "synthlibTypes.h"
#include "synthlibGlobals.h"
#include "prefs.h"
#include "misc.h"   // load_saved_settings() and friends are declared here, not in a persistence.h

#include "g2Prefs.h"

void g2_plugin_prefs_init(void) {
    static bool done = false;

    if (done == true) {
        return;
    }
    done = true;

    prefs_init(G2_PREFS_APP_NAME);

    // The application's own restore: zoom, dial mode and the file browser's last folder, plus
    // registering the callback that keeps that folder up to date. persistence.c has no GLFW in it,
    // so the plug-in uses it rather than keeping a second set of keys that could drift.
    load_saved_settings();
}

// notes §1
void synthlib_load_window_and_dial_mode(int targetFrameBuffWidth, int targetFrameBuffHeight) {
    (void)targetFrameBuffWidth;
    (void)targetFrameBuffHeight;

    synthlib_set_dial_mode((tDialMode)prefs_get_int(G2_PREF_DIAL_MODE, (long)eDialModeRotary));
}
