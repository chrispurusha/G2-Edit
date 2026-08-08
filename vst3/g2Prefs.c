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

// synthlibPersistence.c is NOT linked: its only other job is restoring the WINDOW, and it does that
// with glfwSetWindowSize()/glfwSetWindowPos(). A plug-in owns neither — the host places and sizes
// the editor, and VST3 offers no way to ask otherwise (the width it reopens at is handled in
// g2Editor.mm through getSize()). So the dial-mode half is done here and the window half dropped.
void synthlib_load_window_and_dial_mode(int targetFrameBuffWidth, int targetFrameBuffHeight) {
    (void)targetFrameBuffWidth;
    (void)targetFrameBuffHeight;

    synthlib_set_dial_mode((tDialMode)prefs_get_int(G2_PREF_DIAL_MODE, (long)eDialModeRotary));
}
