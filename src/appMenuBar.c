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
#include "midiCcList.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"
#include "paramOverlay.h"
#include "soundEngine.h"
#include "audioOutput.h"
#include "midiInput.h"
#include "alertDialog.h"
#include "prefs.h"
#include "renderBackend.h"
#include "menus.h"
#include <unistd.h>

#include "appMenuBar.h"
#include "helpPanel.h"
#include "synthlibPersistence.h"

// Real actions land in these six open_*_menu() functions as misc.mm's Cocoa
// menu items get ported over (File first, then
// Settings/Backup/Restore/Controls/View). The bar itself, its layout, and
// click/hover routing are already real and final.
static void action_open_patch(int index) {
    (void)index;
    file_menu_open_patch();
}

// One entry of File > Open Recent. The item's param carries the index into the recent list rather
// than the menu position, because the list and the menu are not the same length — the menu has a
// Clear Menu row on the end of it.
static void action_open_recent(int index) {
    const char * path = recent_files_path(gContextMenu.items[index].param);

    if (path != NULL) {
        file_menu_open_path(path);
    }
}

static void action_clear_recent(int index) {
    (void)index;
    recent_files_clear();
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

// WHETHER A G2 CAN EVER BE ATTACHED, as distinct from whether one is attached right now.
//
// The application greys its bank and device entries while offline, because going online is a thing
// that can happen and a greyed entry says "this exists". The VST3 plug-in has no USB layer at all,
// so for it those entries are not disabled — they are meaningless, and a permanently greyed row is
// worse than no row. Default true, so the application is unchanged.
static bool sDeviceCapable = true;

void app_menu_set_device_capable(bool capable) {
    sDeviceCapable = capable;
}

void open_file_menu(tCoord anchor) {
    static tMenuItem items[11];  // 10 entries + the NULL terminator
    bool             online      = gCommsState == eCommsOnLine;
    bool             isPerf      = gGlobalSettings.perfMode == 1;
    int              i           = 0;

    items[i++] = (tMenuItem){
        "Open Patch/Perf File...", (tRgb)RGB_GREY_3, action_open_patch, 0, NULL, 0, 0.0
    };

    // OPEN RECENT, as a flyout. Both arrays are static because the menu engine keeps the pointers it
    // is given and reads them while the menu is open — a stack array would be gone by then. So are
    // the labels: tMenuItem.label is a borrowed const char *, and a basename points into the stored
    // path, which outlives the menu.
    //
    // A MISSING FILE IS SHOWN GREYED, not hidden. Removing it silently would make a patch on an
    // unplugged drive vanish from the list for good; greying says "this is still yours, it is just
    // not reachable right now", which is what the platform menus do.
    static tMenuItem recentItems[RECENT_FILES_MAX + 2];
    uint32_t         recentCount = recent_files_count();
    uint32_t         r           = 0;

    for (uint32_t n = 0; n < recentCount; n++) {
        const char * path      = recent_files_path(n);
        bool         reachable = (path != NULL) && (access(path, R_OK) == 0);

        recentItems[r++] = (tMenuItem){
            recent_files_display_name(n),
            reachable ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5,
            reachable ? action_open_recent : NULL,
            n, NULL, 0, 0.0
        };
    }

    if (recentCount > 0) {
        // No separator above it: the menu engine has no separator item and nothing else in the app
        // fakes one with a dead row, so Clear Menu simply sits last.
        recentItems[r++] = (tMenuItem){
            "Clear Menu", (tRgb)RGB_GREY_3, action_clear_recent, 0, NULL, 0, 0.0
        };
    }
    recentItems[r] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    // Greyed with no flyout when there is nothing in it, rather than opening an empty one.
    items[i++]     = (tMenuItem){
        "Open Recent",
        (recentCount > 0) ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5,
        NULL, 0,
        (recentCount > 0) ? recentItems : NULL, 0, 0.0
    };

    if (sDeviceCapable) {
        items[i++] = (tMenuItem){
            "Load Patch from Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_load_patch_location : NULL, 0, NULL, 0, 0.0
        };
    }

    if (sDeviceCapable) {
        items[i++] = (tMenuItem){
            "Load Performance from Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_load_perf_location : NULL, 0, NULL, 0, 0.0
        };
    }
    // Save writes back to wherever this patch/perf came from; it only appears live once there IS
    // such a place, which is why Save As sits below it rather than being the only option.
    bool haveSavePath = file_menu_have_saved_path();

    items[i++] = (tMenuItem){
        isPerf ? "Save Perf" : "Save Patch",
        haveSavePath ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5,
        haveSavePath ? action_save_patch_current : NULL, 0, NULL, 0, 0.0
    };
    items[i++] = (tMenuItem){
        isPerf ? "Save Perf As..." : "Save Patch As...", (tRgb)RGB_GREY_3, action_save_patch, 0, NULL, 0, 0.0
    };

    if (sDeviceCapable) {
        items[i++] = (tMenuItem){
            isPerf ? "Store Perf to Bank..." : "Store Patch to Bank...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_store_to_bank : NULL, 0, NULL, 0, 0.0
        };
    }

    if (sDeviceCapable) {
        items[i++] = (tMenuItem){
            "Delete Patch...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_delete_patch_location : NULL, 0, NULL, 0, 0.0
        };
    }

    if (sDeviceCapable) {
        items[i++] = (tMenuItem){
            "Delete Performance...", online ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5, online ? action_delete_perf_location : NULL, 0, NULL, 0, 0.0
        };
    }
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

static void action_open_midi_cc_list(int index) {
    (void)index;
    open_midi_cc_list_panel(gSlot);
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

void open_settings_menu(tCoord anchor) {
    static tMenuItem items[] = {
        {"Synth",                (tRgb)RGB_GREY_3, action_open_synth,          0, NULL, 0, 0.0},
        {"Patch",                (tRgb)RGB_GREY_3, action_open_patch_settings, 0, NULL, 0, 0.0},
        {"Perf",                 (tRgb)RGB_GREY_3, action_open_perf_settings,  0, NULL, 0, 0.0},
        {"Notes",                (tRgb)RGB_GREY_3, action_open_notes,          0, NULL, 0, 0.0},
        {"Parameter Pages",      (tRgb)RGB_GREY_3, action_open_param_pages,    0, NULL, 0, 0.0},
        {"Parameter Overview",   (tRgb)RGB_GREY_3, action_open_param_overview, 0, NULL, 0, 0.0},
        // The original reaches this from a right-click menu and the M key; menu-only here, matching
        // the standing decision for this family of panels.
        {"MIDI Controller List", (tRgb)RGB_GREY_3, action_open_midi_cc_list,   0, NULL, 0, 0.0},
        {NULL,                   (tRgb)RGB_BLACK,  NULL,                       0, NULL, 0, 0.0},
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

void open_backup_menu(tCoord anchor) {
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

void open_restore_menu(tCoord anchor) {
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

void open_controls_menu(tCoord anchor) {
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

// Pulls back far enough to see the whole patch — what a large patch needs before anything else is
// worth doing to it. The scroll is half the job and has to come second: see
// split_view_scroll_to_content().
static void action_zoom_to_fit(int index) {
    (void)index;
    set_zoom_factor(split_view_zoom_to_fit(), (tCoord){0.0, 0.0});
    split_view_scroll_to_content();
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

static void action_open_help(int index) {
    (void)index;
    open_help_panel();
}

// The keyboard and mouse reference. Its own top-level menu rather than an entry buried in Tools:
// a shortcut list nobody can find is a shortcut list nobody reads.
void open_help_menu(tCoord anchor) {
    static tMenuItem items[2] = {0};

    items[0] = (tMenuItem){
        "Keyboard and Mouse...", (tRgb)RGB_GREY_3, action_open_help, 0, NULL, 0, 0.0
    };
    items[1] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };
    open_context_menu(anchor, items, 0, 0.0);
}

void open_view_menu(tCoord anchor) {
    // 3 zoom entries + Zoom to Fit + one per overlay view + the NULL terminator. It was exactly full
    // at 9 before Zoom to Fit was added; overflowing one of these arrays does not fail visibly, it
    // quietly writes over whatever static follows it (see the Experimental menu's note in todo.txt).
    static tMenuItem items[10]                        = {0};
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
    items[i++] = (tMenuItem){
        "Zoom to Fit", (tRgb)RGB_GREY_3, action_zoom_to_fit, 0, NULL, 0, 0.0
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
#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

// A 32-output interface is 16 pairs, and a machine can easily have half a dozen devices. At file
// scope rather than inside open_experimental_menu(), because the UID snapshot below is sized by the
// first of them and the action that reads it is not inside that function.
#define MAX_AUDIO_DEVICE_ITEMS      (33)
#define MAX_OUTPUT_CHANNEL_ITEMS    (65)
#define MAX_MIDI_SOURCE_ITEMS       (34)

// THE UID SNAPSHOT TAKEN WHEN THE MENU WAS BUILT, one per row. The row a user clicks means "the
// device whose name I just read", and only the UID can still say which device that was: the list is
// re-enumerated on every audio_output_device_count(), so by the time this action runs, row 3 may be a
// different device — or the list may be shorter than the menu still on screen.
static char          sDeviceUid[MAX_AUDIO_DEVICE_ITEMS][AUDIO_DEVICE_UID_MAX] = {0};

static void action_select_audio_device(int index) {
    if ((index < 0) || (index >= (int)MAX_AUDIO_DEVICE_ITEMS)) {
        return;
    }

    if (audio_output_select_device_by_uid(sDeviceUid[index]) == false) {
        // Said out loud rather than swallowed. A device can be listed and still refuse to open — in
        // exclusive use by another application, or unable to offer the rate asked of it — and the
        // previous version discarded that, leaving a ticked device making no sound and no way to tell
        // why. It can also have been unplugged while this very menu was open.
        show_alert("Audio device", "That output could not be opened. It may be in use by another application, or no longer connected.");
    }
}
#endif // G2_VST3_BUILD


// Output level. Attenuation only — see sound_engine_set_output_level_db() for why boosting into the
// limiter is not offered.
static const int32_t kOutputLevels[]                                          = {0, -3, -6, -9, -12, -18, -24};

#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

static void action_select_output_level(int index) {
    if ((index >= 0) && (index < (int)(sizeof(kOutputLevels) / sizeof(kOutputLevels[0])))) {
        audio_output_select_level_db(kOutputLevels[index]);
    }
}
#endif // G2_VST3_BUILD


#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

static void action_select_buffer_frames(int index) {
    // Item 0 is "Device default"; the rest are the powers of two below.
    static const uint32_t sizes[] = {0, 32, 64, 128, 256, 512, 1024, 2048};

    audio_output_select_buffer_frames(sizes[((size_t)index < (sizeof(sizes) / sizeof(sizes[0]))) ? index : 0]);
}
#endif // G2_VST3_BUILD


#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

static void action_select_left_output(int index) {
    audio_output_select_left_channel((uint32_t)index);
}
#endif // G2_VST3_BUILD


#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

static void action_select_right_output(int index) {
    audio_output_select_right_channel((uint32_t)index);
}
#endif // G2_VST3_BUILD


#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

static void action_select_midi_source(int index) {
    // The list is offered with "None" first and "All sources" second, so the real sources start at
    // row 2 and are shifted by that much.
    if (index == 0) {
        midi_input_select_source(MIDI_INPUT_NONE);
    } else if (index == 1) {
        midi_input_select_source(MIDI_INPUT_ALL);
    } else {
        midi_input_select_source(index - 2);
    }
}
#endif // G2_VST3_BUILD


#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

static void action_select_midi_channel(int index) {
    midi_input_select_channel((uint32_t)index);   // 0 is Omni, 1..16 a channel
}
#endif // G2_VST3_BUILD


#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

static void action_toggle_midi_to_synth(int index) {
    (void)index;
    midi_input_set_sends_to_synth(!midi_input_sends_to_synth());
}
#endif // G2_VST3_BUILD

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

// SWITCHING TAKES EFFECT ON THE NEXT LAUNCH, and the alert says so rather than leaving the user to
// wonder why nothing changed. It cannot be done live: the window itself is built differently for
// each backend — OpenGL has GLFW create a context alongside it, Metal has GLFW create none — so
// changing it means destroying and rebuilding the window, its context, every glyph atlas and
// texture, and all the callbacks established around it. See renderBackend.h.
static void action_toggle_render_backend(int index) {
    (void)index;

    tRenderBackendId wanted = (gfx_backend_current() == eRenderBackendOpenGL)
                              ? eRenderBackendMetal : eRenderBackendOpenGL;

    if (!gfx_backend_available(wanted)) {
        show_alert("Renderer", "That renderer is not available in this build.");
        return;
    }
    // Written, not applied. gfx_backend_choose() is deliberately NOT called here: the running
    // window belongs to the current backend and would be left talking to the wrong one.
    prefs_set_int("renderBackend", (long)wanted);

    static char      message[160];

    snprintf(message, sizeof(message),
             "The %s renderer will be used the next time G2-Edit starts.\n\nCurrently running: %s.",
             gfx_backend_name(wanted), gfx_backend_name(gfx_backend_current()));
    show_alert("Renderer", message);
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

void open_tools_menu(tCoord anchor) {
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
#ifndef G2_VST3_BUILD    // needs the application's audio-device and MIDI-input layers

void open_experimental_menu(tCoord anchor) {
    // Sized with room to spare, and deliberately generous: the entries here are conditional — the
    // status line only appears with the engine running, the output lists only on a multi-channel
    // device — so the real count varies, and overrunning this array corrupts whatever static
    // follows it. It did exactly that, blanking the device flyout only while the engine was on.
    // One slot per entry with room to spare. This overflowed once already, at 8, and the symptom
    // was another menu's array being quietly overwritten rather than anything obviously wrong here.
    static tMenuItem items[20];
    int              i = 0;

    // WHICH RENDERER, offered here because Metal is exactly what this menu is for — new, worth
    // trying, not yet what anything relies on. The label names the backend it will switch TO, the
    // same state-in-the-label idiom as the Mutator and the sound engine, and says outright that it
    // needs a restart so nobody clicks it twice wondering why the screen looks the same.
    if (gfx_backend_available(eRenderBackendMetal)) {
        items[i++] = (tMenuItem){
            (gfx_backend_current() == eRenderBackendOpenGL)
            ? "Use Metal Renderer (on restart)" : "Use OpenGL Renderer (on restart)",
            (tRgb)RGB_GREY_3, action_toggle_render_backend, 0, NULL, 0, 0.0
        };
        // What is running now, greyed so it reads as information rather than a control. Without it
        // there is no way to tell which backend drew the window you are looking at.
        static char rendererLine[48];

        snprintf(rendererLine, sizeof(rendererLine), "Renderer: %s",
                 gfx_backend_name(gfx_backend_current()));
        items[i++] = (tMenuItem){
            rendererLine, (tRgb)RGB_GREY_5, NULL, 0, NULL, 0, 0.0
        };
    }
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
        // A second information line for modulation. "No vibrato" has three quite different causes —
        // the keyboard not sending pressure, the morph not moving, or no LFO in the resolved chain —
        // and they are indistinguishable by ear.
        items[i++] = (tMenuItem){
            (char *)sound_engine_modulation_text(), (tRgb)RGB_GREY_5, NULL, 0, NULL, 0, 0.0
        };
    }
    // Which device the engine plays through, and which pair of its outputs. Both are flyouts off
    // this menu, and both remember the choice — see audioOutput.h for why the device is stored by
    // UID rather than by position in the list.
    {
        static tMenuItem devices[MAX_AUDIO_DEVICE_ITEMS];
        static tMenuItem lefts[MAX_OUTPUT_CHANNEL_ITEMS];
        static tMenuItem rights[MAX_OUTPUT_CHANNEL_ITEMS];
        static char      deviceLabel[MAX_AUDIO_DEVICE_ITEMS][80];
        static char      leftLabel[MAX_OUTPUT_CHANNEL_ITEMS][24];
        static char      rightLabel[MAX_OUTPUT_CHANNEL_ITEMS][24];
        uint32_t         count    = audio_output_device_count();
        uint32_t         channels = audio_output_selected_device_channels();
        uint32_t         d        = 0;
        uint32_t         c        = 0;

        for (d = 0; (d < count) && (d < (MAX_AUDIO_DEVICE_ITEMS - 1)); d++) {
            // A leading marker rather than a checkmark, matching how Controls marks its dial modes.
            snprintf(deviceLabel[d], sizeof(deviceLabel[d]), "%s%s (%u ch)",
                     audio_output_device_is_selected(d) ? "* " : "  ",
                     audio_output_device_name(d), (unsigned)audio_output_device_channels(d));
            // Pin what this row NAMES, not where it sits — see sDeviceUid.
            strncpy(sDeviceUid[d], audio_output_device_uid(d), sizeof(sDeviceUid[d]) - 1);
            sDeviceUid[d][sizeof(sDeviceUid[d]) - 1] = '\0';
            devices[d]                               = (tMenuItem){
                deviceLabel[d], (tRgb)RGB_GREY_3, action_select_audio_device, d, NULL, 0, 0.0
            };
        }

        devices[d] = (tMenuItem){
            NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
        };

        // Left and right are separate lists, not pairs: the two legs of a monitor path on a desk are
        // not necessarily neighbouring outputs, and forcing 29/30 when the wiring wants 29/31 would
        // mean repatching the desk to suit the software.
        for (c = 0; (c < channels) && (c < (MAX_OUTPUT_CHANNEL_ITEMS - 1)); c++) {
            snprintf(leftLabel[c], sizeof(leftLabel[c]), "%sOut %u",
                     (audio_output_left_channel() == c) ? "* " : "  ", (unsigned)c + 1);
            snprintf(rightLabel[c], sizeof(rightLabel[c]), "%sOut %u",
                     (audio_output_right_channel() == c) ? "* " : "  ", (unsigned)c + 1);
            lefts[c]  = (tMenuItem){
                leftLabel[c], (tRgb)RGB_GREY_3, action_select_left_output, c, NULL, 0, 0.0
            };
            rights[c] = (tMenuItem){
                rightLabel[c], (tRgb)RGB_GREY_3, action_select_right_output, c, NULL, 0, 0.0
            };
        }

        lefts[c]   = (tMenuItem){
            NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
        };
        rights[c]  = (tMenuItem){
            NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
        };

        items[i++] = (tMenuItem){
            "Audio Device", (tRgb)RGB_GREY_3, NULL, 0, devices, 0, 0.0
        };

        // Buffer size. Fewer frames means a keypress lands sooner, since the buffer length is the
        // floor on how late a note can take effect; more means the audio thread is woken less often.
        {
            static const uint32_t sizes[] = {0, 32, 64, 128, 256, 512, 1024, 2048};
            static tMenuItem      buffers[9];
            static char           bufferLabel[9][28];
            uint32_t              n       = 0;

            for (n = 0; n < (sizeof(sizes) / sizeof(sizes[0])); n++) {
                if (sizes[n] == 0) {
                    snprintf(bufferLabel[n], sizeof(bufferLabel[n]), "%sDevice default",
                             (audio_output_buffer_frames() == 0) ? "* " : "  ");
                } else {
                    snprintf(bufferLabel[n], sizeof(bufferLabel[n]), "%s%u frames",
                             (audio_output_buffer_frames() == sizes[n]) ? "* " : "  ",
                             (unsigned)sizes[n]);
                }
                buffers[n] = (tMenuItem){
                    bufferLabel[n], (tRgb)RGB_GREY_3, action_select_buffer_frames, n, NULL, 0, 0.0
                };
            }

            buffers[n] = (tMenuItem){
                NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
            };
            items[i++] = (tMenuItem){
                "Buffer Size", (tRgb)RGB_GREY_3, NULL, 0, buffers, 0, 0.0
            };
        }

        // Output level. A patch can arrive hot — several oscillators through two mixers is well
        // within the G2's own headroom but not within a DAC's — and a fixed trim is easier to
        // reason about than leaving the output limiter to decide.
        {
            static tMenuItem levels[(sizeof(kOutputLevels) / sizeof(kOutputLevels[0])) + 1];
            static char      levelLabel[(sizeof(kOutputLevels) / sizeof(kOutputLevels[0]))][24];
            uint32_t         n = 0;

            for (n = 0; n < (sizeof(kOutputLevels) / sizeof(kOutputLevels[0])); n++) {
                if (kOutputLevels[n] == 0) {
                    snprintf(levelLabel[n], sizeof(levelLabel[n]), "%s0 dB (full)",
                             (audio_output_level_db() == 0) ? "* " : "  ");
                } else {
                    snprintf(levelLabel[n], sizeof(levelLabel[n]), "%s%d dB",
                             (audio_output_level_db() == kOutputLevels[n]) ? "* " : "  ",
                             (int)kOutputLevels[n]);
                }
                levels[n] = (tMenuItem){
                    levelLabel[n], (tRgb)RGB_GREY_3, action_select_output_level, n, NULL, 0, 0.0
                };
            }

            levels[n]  = (tMenuItem){
                NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
            };
            items[i++] = (tMenuItem){
                "Output Level", (tRgb)RGB_GREY_3, NULL, 0, levels, 0, 0.0
            };
        }

        // A stereo device has nothing to choose. Multi-column past eight, or a 32-output interface
        // runs off the bottom of the screen.
        if (channels > 2) {
            items[i++] = (tMenuItem){
                "Left Output", (tRgb)RGB_GREY_3, NULL, 0, lefts, (channels > 8) ? 2 : 1, 0.0
            };
            items[i++] = (tMenuItem){
                "Right Output", (tRgb)RGB_GREY_3, NULL, 0, rights, (channels > 8) ? 2 : 1, 0.0
            };
        }
    }

    // Where MIDI comes from, on which channel, and whether it also plays the G2. The same input
    // serves the sound engine and the hardware, which is the point: one keyboard, both instruments,
    // the same notes — that is how the two get compared.
    {
        static tMenuItem sources[MAX_MIDI_SOURCE_ITEMS];
        static tMenuItem chans[18];
        static char      sourceLabel[MAX_MIDI_SOURCE_ITEMS][80];
        static char      chanLabel[18][20];
        uint32_t         count = midi_input_source_count();
        uint32_t         m     = 0;
        uint32_t         ch    = 0;

        snprintf(sourceLabel[0], sizeof(sourceLabel[0]), "%sNone",
                 midi_input_is_enabled() ? "  " : "* ");
        sources[0]     = (tMenuItem){
            sourceLabel[0], (tRgb)RGB_GREY_3, action_select_midi_source, 0, NULL, 0, 0.0
        };

        // ALL SOURCES AT ONCE, which is what a desk with a keyboard, a control surface and a DAW port
        // on it actually wants, and it takes anything plugged in LATER too — the setup-changed
        // notification reconnects. It was already the startup state, with no way back to it once a
        // single source had been chosen.
        snprintf(sourceLabel[1], sizeof(sourceLabel[1]), "%sAll sources",
                 midi_input_all_sources_selected() ? "* " : "  ");
        sources[1]     = (tMenuItem){
            sourceLabel[1], (tRgb)RGB_GREY_3, action_select_midi_source, 1, NULL, 0, 0.0
        };

        for (m = 0; (m < count) && ((m + 2) < (MAX_MIDI_SOURCE_ITEMS - 1)); m++) {
            snprintf(sourceLabel[m + 2], sizeof(sourceLabel[m + 2]), "%s%s",
                     midi_input_source_is_selected(m) ? "* " : "  ", midi_input_source_name(m));
            sources[m + 2] = (tMenuItem){
                sourceLabel[m + 2], (tRgb)RGB_GREY_3, action_select_midi_source, m + 2, NULL, 0, 0.0
            };
        }

        sources[m + 2] = (tMenuItem){
            NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
        };

        for (ch = 0; ch <= 16; ch++) {
            if (ch == 0) {
                snprintf(chanLabel[0], sizeof(chanLabel[0]), "%sAll (Omni)",
                         (midi_input_channel() == 0) ? "* " : "  ");
            } else {
                snprintf(chanLabel[ch], sizeof(chanLabel[ch]), "%sChannel %u",
                         (midi_input_channel() == ch) ? "* " : "  ", (unsigned)ch);
            }
            chans[ch] = (tMenuItem){
                chanLabel[ch], (tRgb)RGB_GREY_3, action_select_midi_channel, ch, NULL, 0, 0.0
            };
        }

        chans[17]      = (tMenuItem){
            NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
        };

        items[i++]     = (tMenuItem){
            "MIDI Input", (tRgb)RGB_GREY_3, NULL, 0, sources, 0, 0.0
        };
        items[i++]     = (tMenuItem){
            "MIDI Channel", (tRgb)RGB_GREY_3, NULL, 0, chans, 2, 0.0
        };
        items[i++]     = (tMenuItem){
            midi_input_sends_to_synth() ? "MIDI Plays the G2: On" : "MIDI Plays the G2: Off",
            (tRgb)RGB_GREY_3, action_toggle_midi_to_synth, 0, NULL, 0, 0.0
        };
    }
    items[i] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    open_context_menu(anchor, items, 0, 0.0);
}
#endif // G2_VST3_BUILD


tMenuBarItem gAppMenuBar[] = {
    {"File",         open_file_menu        },
    {"Settings",     open_settings_menu    },
    {"Backup",       open_backup_menu      },
    {"Restore",      open_restore_menu     },
    {"Controls",     open_controls_menu    },
    {"Tools",        open_tools_menu       },
    {"View",         open_view_menu        },
#ifndef G2_VST3_BUILD
    {"Experimental", open_experimental_menu},
    {"Help",         open_help_menu        },
#endif
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
