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
// Notes: Docs/code-notes/g2Menu.c.md - "// notes §k" refers there.

// notes §1

#include "sysIncludes.h"
// notes §2
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibTypes.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "contextMenu.h"
#include "alertDialog.h"
#include "synthlibVersion.h"
#include "menuBar.h"
#include "globalVars.h"
#include "soundEngine.h"
#include "synthlibGlobals.h"

#include "appMenuBar.h"
#include "g2Patch.h"
#include "g2View.h"
#include "g2Menu.h"

// notes §3

// notes §4
static void action_about(int index) {
    static char text[1024] = {0};

    (void)index;

    // notes §5
    snprintf(text, sizeof(text), "%s\n\nEngine: %s",
             synthlib_about_text("G2 Alike"), sound_engine_status_text());
    show_alert("About", text);
}

// Named apart from the application's open_help_menu(), which appMenuBar.h declares and this
// file can see — the plug-in's Help menu holds only About, the application's holds more.
static void open_plugin_help_menu(tCoord anchor) {
    static tMenuItem items[2] = {0};

    items[0] = (tMenuItem){
        "About G2 Alike...", (tRgb)RGB_GREY_3, action_about, 0, NULL, 0, 0.0
    };
    items[1] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };
    open_context_menu(anchor, items, 0, 0.0);
}

tMenuBarItem gPluginMenuBar[] = {
    {"File",     open_file_menu    },
    {"Settings", open_settings_menu},
    {"Controls", open_controls_menu},
    {"Tools",    open_tools_menu   },
    {"View",     open_view_menu    },
    {"Help",     open_plugin_help_menu},
    {NULL,       NULL              },
};

void g2_menu_init(void) {
    app_menu_set_device_capable(false);
}

// The patch the File menu last opened, for the topbar to show. Set by file_menu_open_patch(), which
// is the application's own (src/menuActions.c) and is compiled into the plug-in.
static char gLoadedPatchName[256] = {0};

const char * g2_menu_loaded_patch_name(void) {
    return (gLoadedPatchName[0] != '\0') ? gLoadedPatchName : NULL;
}

void g2_menu_set_loaded_patch_name(const char * name) {
    if (name == NULL) {
        gLoadedPatchName[0] = '\0';
        return;
    }
    strncpy(gLoadedPatchName, name, sizeof(gLoadedPatchName) - 1);
    gLoadedPatchName[sizeof(gLoadedPatchName) - 1] = '\0';
}

tRectangle g2_menu_bar_rect(double pointWidth) {
    return (tRectangle){{0.0, 0.0}, {pointWidth, MENU_BAR_HEIGHT}};
}

tRectangle g2_topbar_rect(double pointWidth) {
    // notes §6
    return (tRectangle){{0.0, MENU_BAR_HEIGHT}, {pointWidth, G2_PLUGIN_TOPBAR_HEIGHT}};
}
