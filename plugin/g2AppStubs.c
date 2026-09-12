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
// Notes: Docs/code-notes/g2AppStubs.c.md - "// notes §k" refers there.

// notes §1

#include "sysIncludes.h"
// notes §2
#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "mouseHandle.h"
#include "msgQueue.h"
#include "globalVars.h"   // gToGuiThread / gToUsbThread
#include "g2AppStubs.h"
#include "undo.h"
#include "mutatorUI.h"
#include "paramOverlay.h"
#include "utilsGraphics.h"
#include "synthlibGlobals.h"
#include "g2View.h"
#include "prefs.h"
#include "g2Prefs.h"
#include "canvasDrag.h"

// notes §3

// notes §4

// notes §5
void msg_send(tMessageQueue * msgQueue, const void * content) {
    if ((msgQueue != &gToGuiThread) || (content == NULL)) {
        return;
    }
    gDoc->hostedGuiMsg      = *(const tMessageContent *)content;
    gDoc->hostedGuiMsgValid = true;
}

bool g2_take_gui_message(tMessageContent * out) {
    if ((gDoc->hostedGuiMsgValid == false) || (out == NULL)) {
        return false;
    }
    *out                    = gDoc->hostedGuiMsg;
    gDoc->hostedGuiMsgValid = false;
    return true;
}

void undo_push_param_change(tModuleKey key, uint32_t paramIndex, uint32_t variation, uint32_t oldValue, uint32_t newValue) {
    (void)key;
    (void)paramIndex;
    (void)variation;
    (void)oldValue;
    (void)newValue;
}

void undo_push_mode_change(tModuleKey key, uint32_t modeIndex, uint32_t oldValue, uint32_t newValue) {
    (void)key;
    (void)modeIndex;
    (void)oldValue;
    (void)newValue;
}

void undo_push_delete_selection(void) {
}

// The Undo/Redo buttons on the top bar call these. There is no undo stack in the plug-in, so they
// draw and click but do nothing — which is at least honest, and the buttons are part of the
// application's bar rather than something added here.
void undo_undo(void) {
}

void undo_redo(void) {
}

// The rest of the undo surface, reached from menus.c. Same reasoning as the pushes above: there is
// no undo stack here, and the begin/commit pairs bracket edits that simply are not recorded.
void undo_push_create_module(tModuleKey key, tUndoMoveEntry * displaced, uint32_t displacedCount) {
    (void)key;
    (void)displaced;
    (void)displacedCount;
}

void undo_push_module_colour(tModuleKey key, uint32_t oldColour, uint32_t newColour) {
    (void)key;
    (void)oldColour;
    (void)newColour;
}

void undo_push_module_exclude(tModuleKey key, uint8_t oldValue, uint8_t newValue) {
    (void)key;
    (void)oldValue;
    (void)newValue;
}

void undo_push_patch_descr(uint32_t slot, uint8_t which, uint8_t oldValue, uint8_t newValue) {
    (void)slot;
    (void)which;
    (void)oldValue;
    (void)newValue;
}

void undo_push_knob(uint32_t slot, uint32_t idx1, const tKnob * before1, const tKnob * after1,
                    int32_t idx2, const tKnob * before2, const tKnob * after2) {
    (void)slot;
    (void)idx1;
    (void)before1;
    (void)after1;
    (void)idx2;
    (void)before2;
    (void)after2;
}

void undo_begin_cable_edit(uint32_t slot, uint32_t location) {
    (void)slot;
    (void)location;
}

void undo_commit_cable_edit(void) {
}

void undo_begin_global_knob_edit(void) {
}

void undo_commit_global_knob_edit(void) {
}

void undo_begin_midi_cc_edit(uint32_t slot) {
    (void)slot;
}

void undo_commit_midi_cc_edit(void) {
}

// A device operation starting or finishing. The plug-in never performs one — there is no G2 — but
// menuActions.c references these unconditionally.
void device_op_begin(const char * label) {
    (void)label;
}

void device_op_end(void) {
}

// save_zoom_factor() is REAL now — persistence.c is linked, so zoom persists here as it does in the
// application.

// The application's Controls menu calls this when the dial mode changes. Route it to the plug-in's
// own prefs so the setting persists here too — synthlib_set_dial_mode() already writes it, so this
// only has to not throw the value away.
void synthlib_save_dial_mode(tDialMode mode) {
    prefs_set_int(G2_PREF_DIAL_MODE, (long)mode);
}

// Ends a dial drag. The application also restores the cursor it hid; there is nothing hidden here.
void finish_param_drag(void) {
    (void)canvas_param_drag_release();
}

// True while a drag that hides the cursor is running. The plug-in cannot hide the cursor, so the
// only honest answer is no — which is also what makes hover highlighting behave during a dial drag.
bool is_cursor_hidden_dragging(void) {
    return false;
}

// MIDI Learn's "what controller arrived last". No MIDI input layer here — the host delivers events
// straight to the processor — so there is nothing to report.
int32_t midi_input_last_cc(void) {
    return -1;
}

void undo_push_paste(uint32_t slot, uint32_t location, uint32_t anchorCol, uint32_t anchorRow,
                     tModuleKey * pastedKeys, uint32_t pastedCount,
                     tClipboardModule * clipModules, uint32_t clipModuleCount,
                     tClipboardCable * clipCables, uint32_t clipCableCount,
                     tUndoMoveEntry * displaced, uint32_t displacedCount) {
    (void)slot;
    (void)location;
    (void)anchorCol;
    (void)anchorRow;
    (void)pastedKeys;
    (void)pastedCount;
    (void)clipModules;
    (void)clipModuleCount;
    (void)clipCables;
    (void)clipCableCount;
    (void)displaced;
    (void)displacedCount;
}

// open_toggle_menu(), open_mode_toggle_menu() and find_unique_module_id() are NO LONGER STUBS —
// menus.c is linked in now, so the drop-down a toggle or menu parameter opens is the application's
// own. It was stubbed only while SynthLib's context-menu system could not be linked.

// notes §6

// notes §7
void synthlib_request_redraw(void) {
    g2_view_request_redraw();
}

// The application's wake-the-render-loop wrapper (graphics.c). It is one line there too — this is
// not a stub so much as the same function, since synthlib_request_redraw() above already does the
// right thing in a plug-in. Every menu action calls it.
void wake_glfw(void) {
    synthlib_request_redraw();
}

// notes §8
void notify_full_patch_change(void) {
    gLocation = locationVa;
    synthlib_request_redraw();
}

// notes §9
void undo_push_module_replace(tModuleKey key, const tClipboardModule * before,
                              const tClipboardModule * after) {
    (void)key;
    (void)before;
    (void)after;
}

// notes §10
static tDialMode gDialMode = eDialModeRotary;

tDialMode synthlib_dial_mode(void) {
    return gDialMode;
}

void synthlib_set_dial_mode(tDialMode mode) {
    gDialMode = mode;
    prefs_set_int(G2_PREF_DIAL_MODE, (long)mode);
}
