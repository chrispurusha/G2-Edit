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

#include <string.h>
#include <stdio.h>

#include "misc.h"
#include "defs.h"
#include "types.h"
#include "globalVars.h"
#include "graphics.h"
#include "utilsGraphics.h"
#include "fileBrowser.h"
#include "prefs.h"
#include "synthlibPersistence.h"

// ── RECENT FILES ──────────────────────────────────────────────────────────────────────────────
//
// The File > Open Recent list. Most-recent first, capped at RECENT_FILES_MAX, persisted one prefs
// key per slot so the list survives a restart the way every other editor's does.
//
// PATHS, NOT NAMES. The menu shows each file's basename because that is what a menu of files should
// read like, but what is stored and what is opened is the full path — two patches called "Lead" in
// different folders are different files, and a list keyed on the name would conflate them.
static char     sRecent[RECENT_FILES_MAX][FILE_PATH_SIZE];
static uint32_t sRecentCount;

static void recent_files_save(void) {
    char key[32] = {0};

    for (uint32_t i = 0; i < RECENT_FILES_MAX; i++) {
        snprintf(key, sizeof(key), "recentFile%u", i);
        // The empty string for slots past the end, so a list that SHRANK (cleared, or an entry
        // dropped) does not leave older longer entries behind to be read back on the next run.
        prefs_set_string(key, (i < sRecentCount) ? sRecent[i] : "");
    }
}

void recent_files_load(void) {
    char key[32] = {0};

    sRecentCount = 0;

    for (uint32_t i = 0; i < RECENT_FILES_MAX; i++) {
        snprintf(key, sizeof(key), "recentFile%u", i);
        const char * path = prefs_get_string(key, NULL);

        if ((path == NULL) || (path[0] == '\0')) {
            continue;
        }
        COPY_STRING(sRecent[sRecentCount], path);
        sRecentCount++;
    }
}

// Newly opened goes to the front. An entry already in the list MOVES rather than duplicating, which
// is what makes the list read as "what I have been working on" instead of a raw open log.
void recent_files_add(const char * path) {
    uint32_t existing = RECENT_FILES_MAX;

    if ((path == NULL) || (path[0] == '\0')) {
        return;
    }

    for (uint32_t i = 0; i < sRecentCount; i++) {
        if (strcmp(sRecent[i], path) == 0) {
            existing = i;
            break;
        }
    }

    if (existing == 0) {
        return;                                  // already at the front: nothing to do, nothing to write
    }

    if (existing < sRecentCount) {
        for (uint32_t i = existing; i > 0; i--) {
            COPY_STRING(sRecent[i], sRecent[i - 1]);
        }
    } else {
        if (sRecentCount < RECENT_FILES_MAX) {
            sRecentCount++;
        }

        for (uint32_t i = sRecentCount - 1; i > 0; i--) {
            COPY_STRING(sRecent[i], sRecent[i - 1]);
        }
    }
    COPY_STRING(sRecent[0], path);
    recent_files_save();
}

uint32_t recent_files_count(void) {
    return sRecentCount;
}

const char * recent_files_path(uint32_t index) {
    return (index < sRecentCount) ? sRecent[index] : NULL;
}

// Just the filename, for the menu label. Returns a pointer INTO the stored path, so it stays valid
// exactly as long as the entry does — which is longer than the menu is open.
const char * recent_files_display_name(uint32_t index) {
    const char * path  = recent_files_path(index);

    if (path == NULL) {
        return NULL;
    }
    const char * slash = strrchr(path, '/');

    return (slash != NULL) ? (slash + 1) : path;
}

void recent_files_clear(void) {
    sRecentCount = 0;
    memset(sRecent, 0, sizeof(sRecent));
    recent_files_save();
}

void save_zoom_factor(double zoom) {
    prefs_set_double("zoomFactor", zoom);
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
    synthlib_load_window_and_dial_mode(TARGET_FRAME_BUFF_WIDTH, TARGET_FRAME_BUFF_HEIGHT);

    double       savedZoom       = prefs_get_double("zoomFactor", -1.0);

    if (savedZoom >= 0.24) {
        set_zoom_factor(savedZoom, (tCoord){0.0, 0.0});
    }
    const char * savedBrowserDir = prefs_get_string("fileBrowserLastDirectory", NULL);

    if (savedBrowserDir != NULL) {
        set_file_browser_start_directory(savedBrowserDir);
    }
    set_file_browser_directory_changed_callback(save_file_browser_directory);
    recent_files_load();
}
