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
#include "splitView.h"
#include "mutatorUI.h"
#include "paramPages.h"
#include "paramOverview.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"
#include "paramOverlay.h"
#include "soundEngine.h"
#include "alertDialog.h"
#include "menus.h"
#include "appMenuBar.h"
#include "synthlibPersistence.h"

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

static void action_save_patch_current(int index) {
    (void)index;
    file_menu_save_patch_to_current_path();
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
    static tMenuItem items[10];  // 9 entries + the NULL terminator
    bool             online       = gCommsState == eCommsOnLine;
    bool             isPerf       = gGlobalSettings.perfMode == 1;
    int              i            = 0;

    items[i++] = (tMenuItem){
        "Open Patch/Perf File...", (tRgb)RGB_GREY_3, action_open_patch, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Load Patch from Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_load_patch_location : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Load Performance from Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_load_perf_location : NULL, 0, NULL, 0, 0.0
    };
    // Save writes back to wherever this patch/perf came from; it only appears live once there IS
    // such a place, which is why Save As sits below it rather than being the only option.
    bool             haveSavePath = file_menu_have_saved_path();

    items[i++] = (tMenuItem){
        isPerf ? "Save Perf" : "Save Patch",
        haveSavePath ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5,
        haveSavePath ? action_save_patch_current : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        isPerf ? "Save Perf As..." : "Save Patch As...", (tRgb)RGB_GREY_3, action_save_patch, 0, NULL, 0, 0.0
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

static void action_open_param_pages(int index) {
    (void)index;
    settings_menu_open_param_pages();
}

static void action_open_param_overview(int index) {
    (void)index;
    settings_menu_open_param_overview();
}

static void action_open_virtual_keyboard(int index) {
    (void)index;
    settings_menu_open_virtual_keyboard();
}

static void action_open_patch_adjuster(int index) {
    (void)index;
    settings_menu_open_patch_adjuster();
}

static void action_send_ctrl_snapshot(int index) {
    tMessageContent msg = {0};

    (void)index;
    msg.cmd  = eMsgCmdSendCtrlSnapshot;
    msg.slot = gSlot;
    msg_send(&gToUsbThread, &msg);
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
        {"Synth",              (tRgb)RGB_GREY_3, action_open_synth,          0, NULL, 0, 0.0},
        {"Patch",              (tRgb)RGB_GREY_3, action_open_patch_settings, 0, NULL, 0, 0.0},
        {"Perf",               (tRgb)RGB_GREY_3, action_open_perf_settings,  0, NULL, 0, 0.0},
        {"Notes",              (tRgb)RGB_GREY_3, action_open_notes,          0, NULL, 0, 0.0},
        {"Parameter Pages",    (tRgb)RGB_GREY_3, action_open_param_pages,    0, NULL, 0, 0.0},
        {"Parameter Overview", (tRgb)RGB_GREY_3, action_open_param_overview, 0, NULL, 0, 0.0},
        {NULL,                 (tRgb)RGB_BLACK,  NULL,                       0, NULL, 0, 0.0},
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
    synthlib_set_dial_mode(eDialModeRotary);
    synthlib_save_dial_mode(synthlib_dial_mode());
}

static void action_dial_mode_vertical(int index) {
    (void)index;
    synthlib_set_dial_mode(eDialModeVertical);
    synthlib_save_dial_mode(synthlib_dial_mode());
}

static void action_dial_mode_horizontal(int index) {
    (void)index;
    synthlib_set_dial_mode(eDialModeHorizontal);
    synthlib_save_dial_mode(synthlib_dial_mode());
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
    // on the current dial mode, rather than mutating the string in place. Plain "*" rather than a
    // Unicode checkmark glyph: the app's glyph atlas only preloads ASCII (MAX_GLYPH_CHAR == 127 in
    // synthlibDefs.h), so anything above that silently fails to render.
    static char *    checked[3]   = {"* Rotary", "* Vertical", "* Horizontal"};
    static char *    unchecked[3] = {"Rotary", "Vertical", "Horizontal"};
    int              i;

    for (i = 0; i < 3; i++) {
        items[i].label = ((int)synthlib_dial_mode() == i) ? checked[i] : unchecked[i];
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

// The overlay views. Selecting the mode already showing turns it off again, so the entries behave
// as a radio group with a toggle on the active one.
//
// NOTE the argument is the item's POSITION in the menu, not the payload - contextMenu.c calls
// action(index) and leaves the action to fetch its own value out of
// gContextMenu.items[index].param, the same way every action in menus.c does.
static void action_overlay_mode(int index) {
    tParamOverlayMode mode = (tParamOverlayMode)gContextMenu.items[index].param;

    param_overlay_set_mode((param_overlay_mode() == mode) ? overlayModeNone : mode);
}

static void open_view_menu(tCoord anchor) {
    static tMenuItem items[9]                         = {0};
    static char      overlayLabel[overlayModeMax][40] = {0};
    int              i                                = 0;

    items[i++] = (tMenuItem){
        "Zoom In (Cmd +)", (tRgb)RGB_GREY_3, action_zoom_in, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Zoom Out (Cmd -)", (tRgb)RGB_GREY_3, action_zoom_out, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Zoom Reset", (tRgb)RGB_GREY_3, action_zoom_reset, 0, NULL, 0, 0.0
    };

    // The five overlay views, the active one ticked. overlayModeNone isn't offered as an entry of
    // its own - re-picking the active view is how you turn it off.
    for (tParamOverlayMode mode = overlayModeValues; mode < overlayModeMax; mode++) {
        bool active = (param_overlay_mode() == mode);

        snprintf(overlayLabel[mode], sizeof(overlayLabel[mode]), "%s View %s",
                 active ? "*" : " ", param_overlay_mode_name(mode));
        items[i++] = (tMenuItem){
            overlayLabel[mode], active ? (tRgb)RGB_CONTEXT_MENU_GREEN : (tRgb)RGB_GREY_3,
            action_overlay_mode, (uint32_t)mode, NULL, 0, 0.0
        };
    }

    items[i]   = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

static void action_toggle_mutator(int index) {
    (void)index;

    if (gMutator.active) {
        close_mutator_panel();
    } else {
        open_mutator_panel(gSlot);
    }
}

// The engine follows whatever single oscillator is selected at the time, so it is deliberately not
// greyed out when the selection is unsuitable — it simply makes no sound until one is selected, and
// can be left switched on while clicking around the patch.
static void action_toggle_sound_engine(int index) {
    (void)index;

    if (sound_engine_active()) {
        sound_engine_stop();
    } else if (sound_engine_start() == false) {
        // Starting can fail with no output device, or one another app holds exclusively. Say so
        // rather than leaving a menu that claims the engine is on while it is silent.
        show_alert("Sound Engine", "Could not open the audio output device.");
    } else {
        sound_engine_update_from_patch();    // don't wait for the next redraw to pick up the selection
    }
}

static void action_assign_midi_cc_all(int index) {
    (void)index;
    midi_cc_assign_all_knobs(gSlot);
}

static void action_clear_midi_cc_all(int index) {
    (void)index;
    midi_cc_clear_all(gSlot);
}

static void action_assign_midi_cc_selection(int index) {
    (void)index;
    midi_cc_assign_selection();
}

static void action_deassign_midi_cc_selection(int index) {
    (void)index;
    midi_cc_deassign_selection();
}

static void open_tools_menu(tCoord anchor) {
    static tMenuItem items[10];  // 8 entries + the NULL terminator, with room to grow
    // The two selection entries have nothing to act on without one, and the original greys them
    // the same way rather than letting the click be a silent no-op.
    bool             haveSelection = gSelection.count > 0;
    bool             online        = gCommsState == eCommsOnLine;
    int              i             = 0;

    // Label reflects current state the same way Controls' dial-mode items do (checkmark-style
    // "* " prefix — see open_controls_menu — isn't used here since the item's own name already
    // says what it does; a "Close Mutator" vs "Open Mutator" label reads clearer for a single
    // toggle than a checkmark would).
    items[i++] = (tMenuItem){
        gMutator.active ? "Close Mutator" : "Open Mutator", (tRgb)RGB_GREY_3, action_toggle_mutator, 0, NULL, 0, 0.0
    };
    // Tools is where the original keeps the Virtual Keyboard (manual p.128), alongside its
    // Parameter Pages/Overview — those two sit under Settings here by the owner's earlier call.
    items[i++] = (tMenuItem){
        "Virtual Keyboard", (tRgb)RGB_GREY_3, action_open_virtual_keyboard, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Patch Adjuster", (tRgb)RGB_GREY_3, action_open_patch_adjuster, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Assign MIDI CC to Knobs", (tRgb)RGB_GREY_3, action_assign_midi_cc_all, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Assign MIDI CC to Selection",
        haveSelection ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5,
        haveSelection ? action_assign_midi_cc_selection : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Deassign MIDI CC from Selection",
        haveSelection ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5,
        haveSelection ? action_deassign_midi_cc_selection : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        "Clear All MIDI CC", (tRgb)RGB_GREY_3, action_clear_midi_cc_all, 0, NULL, 0, 0.0
    };
    // Only means anything with the synth attached — it asks the G2 to transmit, so offline there
    // is nothing to transmit from.
    items[i++] = (tMenuItem){
        "Send Controller Snapshot",
        online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5,
        online ? action_send_ctrl_snapshot : NULL, 0, NULL, 0, 0.0
    };
    items[i]   = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

// Work that is being tried out rather than relied on. Kept as its own menu so that what is
// finished and what is an experiment are not sitting side by side under Tools — anything here may
// change or disappear, and graduates into one of the other menus once it has settled.
static void open_experimental_menu(tCoord anchor) {
    static tMenuItem items[4];   // 1 entry + the NULL terminator, with room to grow
    int              i = 0;

    // The engine follows whichever single oscillator is selected, so this is deliberately never
    // greyed out — it simply makes no sound until one is. Same state-in-the-label idiom as the
    // Mutator entry under Tools.
    items[i++] = (tMenuItem){
        sound_engine_active() ? "Disable Sound Engine" : "Enable Sound Engine",
        (tRgb)RGB_GREY_3, action_toggle_sound_engine, 0, NULL, 0, 0.0
    };

    // A status line while it is on. Greyed and with no action, so it reads as information rather
    // than something to click. Without it an engine that is on but silent gives no clue why — the
    // usual reason is simply that no OscB is selected.
    if (sound_engine_active()) {
        items[i++] = (tMenuItem){
            (char *)sound_engine_status_text(), (tRgb)RGB_GREY_5, NULL, 0, NULL, 0, 0.0
        };
    }
    items[i]   = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}

tMenuBarItem gAppMenuBar[] = {
    {"File",         open_file_menu        },
    {"Settings",     open_settings_menu    },
    {"Backup",       open_backup_menu      },
    {"Restore",      open_restore_menu     },
    {"Controls",     open_controls_menu    },
    {"Tools",        open_tools_menu       },
    {"View",         open_view_menu        },
    {"Experimental", open_experimental_menu},
    {NULL,           NULL                  },
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
