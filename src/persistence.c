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

// Window/zoom/dial-mode/last-browsed-folder settings persistence — goes through SynthLib's
// prefs.h (a plain "key=value" text file under a per-OS standard config directory) instead of
// NSUserDefaults, so none of this needs Objective-C/Cocoa any more.

#include "misc.h"
#include "defs.h"
#include "types.h"
#include "globalVars.h"
#include "graphics.h"
#include "utilsGraphics.h"
#include "fileBrowser.h"
#include "prefs.h"

void save_dial_mode(int mode) {
    prefs_set_int("dialMode", mode);
}

void save_zoom_factor(double zoom) {
    prefs_set_double("zoomFactor", zoom);
}

void save_window_size(int w) {
    prefs_set_int("windowWidth", w);
}

void save_window_pos(int x, int y) {
    prefs_set_int("windowX", x);
    prefs_set_int("windowY", y);
}

void save_file_browser_directory(const char * path) {
    if (path == NULL) {
        return;
    }
    prefs_set_string("fileBrowserLastDirectory", path);
}

// Restores window/zoom/dial-mode/last-browsed-folder state saved from a previous run. Called once
// at startup from setup_main_menu() (misc.mm) — prefs_init() must run before this (also there),
// so the settings file is loaded before any of these get_* calls.
void load_saved_settings(void) {
    gDialMode = (tDialMode)prefs_get_int("dialMode", (long)gDialMode);

    double       savedZoom       = prefs_get_double("zoomFactor", -1.0);

    if (savedZoom >= 0.24) {
        set_zoom_factor(savedZoom, (tCoord){0.0, 0.0});
    }
    long         savedW          = prefs_get_int("windowWidth", 0);

    if (savedW > 0) {
        int savedH = (int)savedW * TARGET_FRAME_BUFF_HEIGHT / TARGET_FRAME_BUFF_WIDTH;

        resize_window((int)savedW, savedH);
    }

    if (prefs_has_key("windowX") && prefs_has_key("windowY")) {
        int savedX = (int)prefs_get_int("windowX", 0);
        int savedY = (int)prefs_get_int("windowY", 0);

        reposition_window(savedX, savedY);
    }
    const char * savedBrowserDir = prefs_get_string("fileBrowserLastDirectory", NULL);

    if (savedBrowserDir != NULL) {
        set_file_browser_start_directory(savedBrowserDir);
    }
    set_file_browser_directory_changed_callback(save_file_browser_directory);
}
