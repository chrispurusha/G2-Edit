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
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "mouseHandle.h"
#include "msgQueue.h"
#include "undo.h"
#include "mutatorUI.h"
#include "paramOverlay.h"
#include "utilsGraphics.h"
#include "synthlibGlobals.h"

// ── Mouse position ──────────────────────────────────────────────────────────────────────────────
//
// There is no pointer to report. When the input path is eventually built, THESE are the functions
// the plug-in's shell will implement for real, from the events the host delivers to the NSView —
// which is why they are grouped rather than scattered.

bool multi_select_modifier_held(void) {
    return false;
}

void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    if (coord != NULL) {
        coord->x = 0.0;
        coord->y = 0.0;
    }
}

void convert_mouse_coord_to_module_area_coord(tCoord * targetCoord, tCoord coord) {
    (void)coord;

    if (targetCoord != NULL) {
        targetCoord->x = 0.0;
        targetCoord->y = 0.0;
    }
}

void convert_mouse_coord_to_module_column_row(uint32_t * column, uint32_t * row, tCoord coord) {
    (void)coord;

    if (column != NULL) {
        *column = 0;
    }

    if (row != NULL) {
        *row = 0;
    }
}

void start_cursor_drag(void) {
}

// ── Editing ─────────────────────────────────────────────────────────────────────────────────────
//
// A plug-in has no G2 to send to and nothing to undo. msg_send() is the one worth noticing: it is
// the single point through which the whole UI reaches the hardware, so a plug-in that never calls
// it cannot accidentally write to a connected synth — which is the behaviour we want anyway.

void msg_send(tMessageQueue * msgQueue, const void * content) {
    (void)msgQueue;
    (void)content;
}

int32_t find_unique_module_id(uint32_t location) {
    (void)location;
    return -1;    // "no free id" — nothing here creates modules
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

// The drop-down a toggle or mode parameter opens when clicked. It is a SynthLib context menu, which
// needs the application's menu stack and its event loop to run — so this is not merely "nothing to
// do" like the undo stubs, but a real capability the plug-in does not have yet. It arrives with the
// input path, not before.
void open_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex, uint32_t paramRef) {
    (void)coord;
    (void)moduleKey;
    (void)paramIndex;
    (void)paramRef;
}

void open_mode_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t modeIndex, uint32_t modeRef) {
    (void)coord;
    (void)moduleKey;
    (void)modeIndex;
    (void)modeRef;
}

void param_overlay_note_param(tModule * module, uint32_t paramIndex, tRectangle rectangle, const char * displayValue) {
    (void)module;
    (void)paramIndex;
    (void)rectangle;
    (void)displayValue;
}

// ── Ambient state ───────────────────────────────────────────────────────────────────────────────

// The Mutator panel's state. Referenced by the canvas because a mutating module is drawn
// differently; zeroed here means "not mutating", which is the whole of what the plug-in needs.
tMutatorState gMutator = {0};

// synthlibGlobals.c is NOT linked in: its synthlib_request_redraw() calls glfwPostEmptyEvent(), so
// taking the file would take GLFW with it. Only these two are actually reached from the renderer.
//
// Redraw is where the plug-in and the application genuinely differ rather than merely stub out. The
// application posts an empty event to wake a blocked event loop; a plug-in marks its view dirty and
// lets AppKit schedule the frame. Wiring that through is the next real piece of work — for now the
// view redraws when the host tells it to.
void synthlib_request_redraw(void) {
}

tDialMode synthlib_dial_mode(void) {
    return eDialModeRotary;
}
