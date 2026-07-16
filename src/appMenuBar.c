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

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "utilsGraphics.h"
#include "contextMenu.h"
#include "globalVars.h"
#include "misc.h"
#include "graphics.h"
#include "appMenuBar.h"

// Real actions land in these six open_*_menu() functions as misc.mm's Cocoa
// menu items get ported over (File first, then
// Settings/Backup/Restore/Controls/View). The bar itself, its layout, and
// click/hover routing are already real and final.
static void action_open_patch(int index) {
    (void)index;
    file_menu_open_patch();
}

static void action_save_patch(int index) {
    (void)index;
    file_menu_save_patch();
}

static void action_new_patch(int index) {
    (void)index;
    file_menu_new_patch();
}

static void action_load_patch_location(int index) {
    (void)index;
    file_menu_load_patch_location();
}

static void action_load_perf_location(int index) {
    (void)index;
    file_menu_load_perf_location();
}

static void action_delete_patch_location(int index) {
    (void)index;
    file_menu_delete_patch_location();
}

static void action_delete_perf_location(int index) {
    (void)index;
    file_menu_delete_perf_location();
}

static void action_store_to_bank(int index) {
    (void)index;
    file_menu_store_to_bank();
}

static void open_file_menu(tCoord anchor) {
    static tMenuItem items[9];
    bool             online = gCommsState == eCommsOnLine;
    bool             isPerf = gGlobalSettings.perfMode == 1;
    int              i      = 0;

    items[i++] = (tMenuItem){
        "Open Patch/Perf File...", (tRgb)RGB_GREY_3, action_open_patch, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Load Patch from Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_load_patch_location : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Load Performance from Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_load_perf_location : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        isPerf ? "Save Perf to File..." : "Save Patch to File...", (tRgb)RGB_GREY_3, action_save_patch, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        isPerf ? "Store Perf to Bank..." : "Store Patch to Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_store_to_bank : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Delete Patch...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_delete_patch_location : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Delete Performance...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_delete_perf_location : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "New Patch", (tRgb)RGB_GREY_3, action_new_patch, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

static void action_open_synth(int index) {
    (void)index;
    settings_menu_open_synth();
}

static void action_open_patch_settings(int index) {
    (void)index;
    settings_menu_open_patch();
}

static void action_open_perf_settings(int index) {
    (void)index;
    settings_menu_open_perf();
}

static void action_open_notes(int index) {
    (void)index;
    settings_menu_open_notes();
}

static void open_settings_menu(tCoord anchor) {
    static tMenuItem items[] = {
        {"Synth", (tRgb)RGB_GREY_3, action_open_synth,          0, NULL, 0, 0.0},
        {"Patch", (tRgb)RGB_GREY_3, action_open_patch_settings, 0, NULL, 0, 0.0},
        {"Perf",  (tRgb)RGB_GREY_3, action_open_perf_settings,  0, NULL, 0, 0.0},
        {"Notes", (tRgb)RGB_GREY_3, action_open_notes,          0, NULL, 0, 0.0},
        {NULL,    (tRgb)RGB_BLACK,  NULL,                       0, NULL, 0, 0.0},
    };

    open_context_menu(anchor, items, 0, 0.0);
}

static void action_backup_patch_bank(int index) {
    (void)index;
    backup_menu_patch_bank();
}

static void action_backup_perf_bank(int index) {
    (void)index;
    backup_menu_perf_bank();
}

static void action_backup_synth_settings(int index) {
    (void)index;
    backup_menu_synth_settings();
}

static void action_backup_everything(int index) {
    (void)index;
    backup_menu_everything();
}

static void open_backup_menu(tCoord anchor) {
    static tMenuItem items[6];
    bool             online = gCommsState == eCommsOnLine;
    int              i      = 0;

    items[i++] = (tMenuItem){
        "Patch Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_backup_patch_bank : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Performance Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_backup_perf_bank : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Backup Synth Settings...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_backup_synth_settings : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Everything...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_backup_everything : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

static void action_restore_patch_bank(int index) {
    (void)index;
    restore_menu_patch_bank();
}

static void action_restore_perf_bank(int index) {
    (void)index;
    restore_menu_perf_bank();
}

static void action_restore_synth_settings(int index) {
    (void)index;
    restore_menu_synth_settings();
}

static void action_restore_everything(int index) {
    (void)index;
    restore_menu_everything();
}

static void open_restore_menu(tCoord anchor) {
    static tMenuItem items[6];
    bool             online = gCommsState == eCommsOnLine;
    int              i      = 0;

    items[i++] = (tMenuItem){
        "Patch Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_restore_patch_bank : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Performance Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_restore_perf_bank : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Synth Settings...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_restore_synth_settings : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Everything...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_restore_everything : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

static void action_dial_mode_rotary(int index) {
    (void)index;
    gDialMode = eDialModeRotary;
    save_dial_mode(gDialMode);
}

static void action_dial_mode_vertical(int index) {
    (void)index;
    gDialMode = eDialModeVertical;
    save_dial_mode(gDialMode);
}

static void action_dial_mode_horizontal(int index) {
    (void)index;
    gDialMode = eDialModeHorizontal;
    save_dial_mode(gDialMode);
}

static void open_controls_menu(tCoord anchor) {
    static tMenuItem items[]      = {
        {"* Rotary",     (tRgb)RGB_GREY_3, action_dial_mode_rotary,     0, NULL, 0, 0.0},
        {"* Vertical",   (tRgb)RGB_GREY_3, action_dial_mode_vertical,   0, NULL, 0, 0.0},
        {"* Horizontal", (tRgb)RGB_GREY_3, action_dial_mode_horizontal, 0, NULL, 0, 0.0},
        {NULL,           (tRgb)RGB_BLACK,  NULL,                        0, NULL, 0, 0.0},
    };

    // Labels are fixed strings with a checkmark prefix baked in (tMenuItem has no separate
    // "checked" flag) — point each entry's label at the checked or unchecked variant depending
    // on gDialMode, rather than mutating the string in place. Plain "*" rather than a Unicode
    // checkmark glyph: the app's glyph atlas only preloads ASCII (MAX_GLYPH_CHAR == 127 in
    // synthlibDefs.h), so anything above that silently fails to render.
    static char *    checked[3]   = {"* Rotary", "* Vertical", "* Horizontal"};
    static char *    unchecked[3] = {"Rotary", "Vertical", "Horizontal"};
    int              i;

    for (i = 0; i < 3; i++) {
        items[i].label = ((int)gDialMode == i) ? checked[i] : unchecked[i];
    }

    open_context_menu(anchor, items, 0, 0.0);
}

static void action_zoom_in(int index) {
    (void)index;
    set_zoom_factor(get_zoom_factor() + ZOOM_DELTA, (tCoord){0.0, 0.0});
    save_zoom_factor(get_zoom_factor());
    wake_glfw();
}

static void action_zoom_out(int index) {
    (void)index;
    set_zoom_factor(get_zoom_factor() - ZOOM_DELTA, (tCoord){0.0, 0.0});
    save_zoom_factor(get_zoom_factor());
    wake_glfw();
}

static void action_zoom_reset(int index) {
    (void)index;
    set_zoom_factor(NO_ZOOM, (tCoord){0.0, 0.0});
    save_zoom_factor(get_zoom_factor());
    wake_glfw();
}

static void open_view_menu(tCoord anchor) {
    static tMenuItem items[] = {
        {"Zoom In (Cmd +)",  (tRgb)RGB_GREY_3, action_zoom_in,    0, NULL, 0, 0.0},
        {"Zoom Out (Cmd -)", (tRgb)RGB_GREY_3, action_zoom_out,   0, NULL, 0, 0.0},
        {"Zoom Reset",       (tRgb)RGB_GREY_3, action_zoom_reset, 0, NULL, 0, 0.0},
        {NULL,               (tRgb)RGB_BLACK,  NULL,              0, NULL, 0, 0.0},
    };

    open_context_menu(anchor, items, 0, 0.0);
}

tMenuBarItem gAppMenuBar[] = {
    {"File",     open_file_menu    },
    {"Settings", open_settings_menu},
    {"Backup",   open_backup_menu  },
    {"Restore",  open_restore_menu },
    {"Controls", open_controls_menu},
    {"View",     open_view_menu    },
    {NULL,       NULL              },
};

tRectangle app_menu_bar_rect(void) {
    return (tRectangle){
        {
            0.0, 0.0
        }, {
            (get_render_width() / gGlobalGuiScale), MENU_BAR_HEIGHT
        }
    };
}

#ifdef __cplusplus
}
#endif
