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

// What the canvas renderer refers to but a plug-in has no answer for.
//
// The plug-in draws the patch; it does not edit it, does not own a mouse, and is not connected to a
// G2. Everything below exists because moduleGraphics.c and renderParams.c contain the CLICK HANDLERS
// as well as the drawing, in the same translation units — the handlers are never called here, but
// their references still have to resolve.
//
// THIS FILE IS A MEASUREMENT, NOT JUST A CONVENIENCE. It is the complete list of what stands between
// the editor's renderer and a build with no application around it, and it is thirteen functions in
// three groups: mouse position, editing, and two pieces of ambient state. That is the real size of
// the coupling, and it is small enough to be worth reading as an argument for the shell/renderer
// split described in plugin-gui-notes.md.
//
// Each stub is inert rather than merely empty where that distinction matters — a coordinate is
// zeroed, a "find a free slot" returns "none" — so that if one ever IS called, the result is
// something harmless and obvious rather than uninitialised memory.

#include "sysIncludes.h"
// defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
// colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
// silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
// behind the top bar and with the margin between them swallowed.
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
#include "g2GlView.h"
#include "prefs.h"
#include "g2Prefs.h"
#include "canvasDrag.h"

// ── Mouse position ──────────────────────────────────────────────────────────────────────────────
//
// There is no pointer to report. When the input path is eventually built, THESE are the functions
// the plug-in's shell will implement for real, from the events the host delivers to the NSView —
// which is why they are grouped rather than scattered.

bool multi_select_modifier_held(void) {
    return false;
}

// No modifier state from the host view yet. Shift-drag on a mutator slider and Cmd-click behave as
// unmodified clicks until there is.
bool shift_modifier_held(void) {
    return false;
}

bool cmd_modifier_held(void) {
    return false;
}

// NONE OF THE MOUSE-POSITION FUNCTIONS ARE STUBS ANY MORE, which is why the group is empty but for
// the modifier below. get_global_gui_scaled_mouse_coord() is answered by g2Input.c from the host's
// events, and both coordinate conversions moved into src/canvasCoords.c — out of mouseHandle.c and
// menus.c respectively — because the arithmetic never needed a window in the first place.

// start_cursor_drag() is NOT here any more — it is real, in g2Input.c, because it needs the mouse.

// ── Editing ─────────────────────────────────────────────────────────────────────────────────────
//
// A plug-in has no G2 to send to and nothing to undo. msg_send() is the one worth noticing: it is
// the single point through which the whole UI reaches the hardware, so a plug-in that never calls
// it cannot accidentally write to a connected synth — which is the behaviour we want anyway.

// MESSAGES TO THE G2 ARE DROPPED; MESSAGES TO THE GUI ARE NOT.
//
// Both go through this one function in the application. Dropping everything was right while the
// plug-in only drew — but File > Open Patch File... does not open a browser itself: it POSTS
// eRspShowOpenRead to the GUI queue so the browser opens from the render loop rather than from
// inside a menu callback (see menuActions.c). Dropping that made the menu item do nothing at all.
//
// So gToUsbThread is still discarded — there is no synth, and a plug-in must never write to one —
// and gToGuiThread is queued for the editor to drain. One slot is enough: these are user actions,
// arriving one click at a time.
static tMessageContent gGuiMsg      = {0};
static bool            gGuiMsgValid = false;

void msg_send(tMessageQueue * msgQueue, const void * content) {
    if ((msgQueue != &gToGuiThread) || (content == NULL)) {
        return;
    }
    gGuiMsg      = *(const tMessageContent *)content;
    gGuiMsgValid = true;
}

bool g2_take_gui_message(tMessageContent * out) {
    if ((gGuiMsgValid == false) || (out == NULL)) {
        return false;
    }
    *out         = gGuiMsg;
    gGuiMsgValid = false;
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
void undo_push_create_module(tModuleKey key) {
    (void)key;
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
                     tClipboardCable * clipCables, uint32_t clipCableCount) {
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
}

// open_toggle_menu(), open_mode_toggle_menu() and find_unique_module_id() are NO LONGER STUBS —
// menus.c is linked in now, so the drop-down a toggle or menu parameter opens is the application's
// own. It was stubbed only while SynthLib's context-menu system could not be linked.

// ── Ambient state ───────────────────────────────────────────────────────────────────────────────
//
// gMutator and param_overlay_note_param() used to be faked here. They are REAL now — mutatorUI.c and
// paramOverlay.c are linked, so the Mutator panel and the parameter overlay work rather than being
// pretended at.

// synthlibGlobals.c is NOT linked in: its synthlib_request_redraw() calls glfwPostEmptyEvent(), so
// taking the file would take GLFW with it. Only these two are actually reached from the renderer.
//
// Redraw is where the plug-in and the application genuinely differ rather than merely stub out, and
// it is NOT a stub — it is the real thing, routed differently. The application posts an empty event
// to wake a blocked GLFW loop; here the view is marked dirty and AppKit schedules the frame. Every
// part of the editor that changes something already calls this, so wiring this one function is what
// makes the whole canvas repaint on change.
void synthlib_request_redraw(void) {
    g2_gl_view_request_redraw();
}

// The application's wake-the-render-loop wrapper (graphics.c). It is one line there too — this is
// not a stub so much as the same function, since synthlib_request_redraw() above already does the
// right thing in a plug-in. Every menu action calls it.
void wake_glfw(void) {
    synthlib_request_redraw();
}

// Settable from the View menu, and REMEMBERED between sessions the way the application remembers
// it — through the same SynthLib prefs store, though under the plug-in's own name. See
// g2_plugin_prefs_init() for why the file is not shared with the application's.
//
// Rotary is the default because the other two want a hidden, warped cursor that a host view does not
// give us — see the note in g2Menu.c's View menu.
static tDialMode gDialMode = eDialModeRotary;

tDialMode synthlib_dial_mode(void) {
    return gDialMode;
}

void synthlib_set_dial_mode(tDialMode mode) {
    gDialMode = mode;
    prefs_set_int(G2_PREF_DIAL_MODE, (long)mode);
}
