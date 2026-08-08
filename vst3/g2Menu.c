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

// The plug-in's menu bar.
//
// A SEPARATE, SMALLER MENU THAN THE APPLICATION'S — not a port of appMenuBar.c, and deliberately so.
// Of that file's eight menus, five (Settings, Backup, Restore, Controls, Experimental) exist to talk
// to a G2 over USB, which a plug-in has no connection to and should not pretend to. Porting them
// would produce a menu full of entries that either do nothing or, worse, look as though they might.
//
// What IS shared is everything underneath: SynthLib's context-menu and menu-bar rendering and
// interaction, unchanged. Those needed only two things to link into a plug-in — see
// plugin-gui-notes.md — so the machinery is the application's; only the contents are ours.

#include "sysIncludes.h"
// defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
// colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
// silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
// behind the top bar and with the margin between them swallowed.
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibTypes.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "contextMenu.h"
#include "menuBar.h"
#include "globalVars.h"
#include "soundEngine.h"
#include "synthlibGlobals.h"

#include "appMenuBar.h"
#include "g2Patch.h"
#include "g2FileDialog.h"
#include "g2GlView.h"
#include "g2Menu.h"

// THE APPLICATION'S OWN MENUS, minus the ones that describe hardware.
//
// An earlier version here defined its own File and View menus. That was the same mistake the topbar
// made: a plug-in that resembles the editor is the point, and a second set of menu definitions can
// only drift from the first. appMenuBar.c's menus are now exposed individually so a bar can be
// composed from a subset of them.
//
// DROPPED ENTIRELY: Backup, Restore and Experimental. All three exist to talk to a G2 over USB or to
// toggle the sound engine the plug-in IS. The bank entries inside File and Tools are dropped too,
// via app_menu_set_device_capable(false) — the application GREYS those while offline because going
// online is possible; here it never is, and a permanently greyed row is worse than no row.
tMenuBarItem gPluginMenuBar[] = {
    {"File",     open_file_menu    },
    {"Settings", open_settings_menu},
    {"Controls", open_controls_menu},
    {"Tools",    open_tools_menu   },
    {"View",     open_view_menu    },
    {NULL,       NULL              },
};

void g2_menu_init(void) {
    app_menu_set_device_capable(false);
}

// The patch the File menu last opened, for the topbar to show. Set by the plug-in's own
// file_menu_open_patch() in g2FileMenu.c.
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
    // Reserved, not yet drawn into. Sits directly below the menu bar and above the canvas, which is
    // where the application puts it — so when the variation buttons, patch name and cable toggles
    // arrive they land in the space the canvas has already been laid out around, rather than
    // shifting everything down at that point.
    return (tRectangle){{0.0, MENU_BAR_HEIGHT}, {pointWidth, G2_PLUGIN_TOPBAR_HEIGHT}};
}
