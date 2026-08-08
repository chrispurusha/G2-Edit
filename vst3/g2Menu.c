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
#include "synthlibDefs.h"
#include "synthlibTypes.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "contextMenu.h"
#include "menuBar.h"
#include "globalVars.h"
#include "soundEngine.h"
#include "synthlibGlobals.h"

#include "g2Patch.h"
#include "g2FileDialog.h"
#include "g2GlView.h"
#include "g2Menu.h"

// The patch currently loaded, for the topbar to show once there is one. Also what makes "the plug-in
// is playing the built-in patch" distinguishable from "the plug-in is playing what you chose".
static char gLoadedPatchName[256] = {0};

const char * g2_menu_loaded_patch_name(void) {
    return (gLoadedPatchName[0] != '\0') ? gLoadedPatchName : NULL;
}

static void action_open_patch_file(int index) {
    char path[1024] = {0};

    (void)index;

    if (g2_choose_patch_file(path, sizeof(path)) == false) {
        return;
    }

    // Slot 0 always: a plug-in instance is one patch, and the G2's four-slot performance layout is a
    // hardware notion with nothing to map onto here.
    if (g2_plugin_load_patch(path, 0) == false) {
        return;
    }

    // The engine reads a parameter snapshot, not the database — see the note in g2GlDraw.c. The
    // redraw below rebuilds it, but do it here too so a patch loaded with the editor somehow not
    // redrawing is still heard.
    sound_engine_update_from_patch();

    {
        const char * leaf = strrchr(path, '/');

        leaf = (leaf != NULL) ? (leaf + 1) : path;
        strncpy(gLoadedPatchName, leaf, sizeof(gLoadedPatchName) - 1);
        gLoadedPatchName[sizeof(gLoadedPatchName) - 1] = '\0';
    }
    g2_gl_view_request_redraw();
}

static void open_file_menu(tCoord anchor) {
    static tMenuItem items[2];
    int              i = 0;

    items[i++] = (tMenuItem){
        "Open Patch File...", (tRgb)RGB_GREY_3, action_open_patch_file, 0, NULL, 0, 0.0
    };
    // NULL label terminates the list, as the application's menus do.
    items[i++] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

// ── View ────────────────────────────────────────────────────────────────────────────────────────

static void action_dial_mode(int index) {
    synthlib_set_dial_mode((tDialMode)index);
    g2_gl_view_request_redraw();
}

static void open_view_menu(tCoord anchor) {
    static tMenuItem items[4];
    tDialMode        mode = synthlib_dial_mode();
    int              i    = 0;

    // ALL THREE DIAL MODES, including the two that want a hidden cursor.
    //
    // Vertical and horizontal difference the pointer against its previous position, and that works
    // here — the plug-in feeds canvas coordinates rather than raw device pixels, so a drag is simply
    // scaled differently (points, not backing pixels) and therefore a little less sensitive than the
    // application's on a Retina display.
    //
    // What is genuinely missing is start_cursor_drag()'s cursor hiding and warping: the pointer
    // visibly travels away from the dial instead of staying put, and stops at the screen edge. That
    // makes them worth OFFERING rather than withholding — someone who prefers vertical dials would
    // rather have them imperfect than not at all — but it is why rotary remains the default.
    items[i++] = (tMenuItem){
        (mode == eDialModeRotary) ? "Dial Drag: Rotary  *" : "Dial Drag: Rotary",
        (tRgb)RGB_GREY_3, action_dial_mode, (uint32_t)eDialModeRotary, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        (mode == eDialModeVertical) ? "Dial Drag: Vertical  *" : "Dial Drag: Vertical",
        (tRgb)RGB_GREY_3, action_dial_mode, (uint32_t)eDialModeVertical, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        (mode == eDialModeHorizontal) ? "Dial Drag: Horizontal  *" : "Dial Drag: Horizontal",
        (tRgb)RGB_GREY_3, action_dial_mode, (uint32_t)eDialModeHorizontal, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

tMenuBarItem gPluginMenuBar[] = {
    {"File", open_file_menu},
    {"View", open_view_menu},
    {NULL,   NULL          },
};

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
