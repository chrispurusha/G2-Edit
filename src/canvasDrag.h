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
// Notes: Docs/code-notes/canvasDrag.h.md - "// notes §k" refers there.

#ifndef __CANVAS_DRAG_H__
#define __CANVAS_DRAG_H__

#include "sysIncludes.h"
#include "types.h"

// notes §1

// Motion during a drag. Coordinate is in the canvas's logical units, top-left origin. Returns true
// if a drag consumed it, so a caller can tell whether to look at hover instead. Auto-scroll at the
// canvas edge is deliberately NOT done here — it belongs to whoever owns the scrollbars.
bool canvas_drag_motion(tCoord coord);

// A press that landed on empty canvas: clears the selection unless `additive`, and starts a
// rubber band. Returns true if the coordinate was inside the module area at all.
bool canvas_empty_press(tCoord coord, bool additive);

// Finishes a rubber band, selecting what it enclosed. Returns true if one was in progress.
bool canvas_rubber_band_release(tCoord coord, uint32_t slot, uint32_t location, bool additive);

// Ends a module drag, pushing aside anything the module was dropped on top of — the same
// re-ordering the application performs. It does NOT record the move for undo; the application does
// that itself, and a plug-in has no undo stack.
bool canvas_module_drag_release(void);

// Records where a drag began, in RAW cursor coordinates. The incremental dial modes difference
// against it; Alt-held morph dragging measures from it rather than from the previous event.
void canvas_drag_set_origin(double rawX, double rawY);

// notes §2
typedef enum {
    canvasGestureNone       = 0,
    canvasGestureParam      = 1u << 0,   // a dial or slider, including an Alt morph-offset drag
    canvasGestureModule     = 1u << 1,   // moving one module or a whole selection
    canvasGestureCable      = 1u << 2,   // dragging a cable end towards a connector
    canvasGestureRubberBand = 1u << 3,   // sweeping out a selection over empty canvas
    canvasGestureAll        = 0x0Fu
} tCanvasGesture;

// Everything a phase might need, so the table's rows can share one signature. A shell fills in what
// it has: `additive` is Shift (add to the selection rather than replace it) and `altHeld` is Alt (drag
// the morph offset rather than the value), both already answered by SynthLib's pushed modifier state.
typedef struct {
    tCoord   coord;      // pointer in canvas logical units
    double   rawX;       // pointer in the space THIS shell reports motion in — see cursor_raw_coord()
    double   rawY;
    uint32_t slot;
    uint32_t location;
    bool     altHeld;
    bool     additive;
} tCanvasGestureEvent;

// notes §3
tCanvasGesture canvas_gesture_motion(const tCanvasGestureEvent * event);

// notes §4
tCanvasGesture canvas_gesture_release(const tCanvasGestureEvent * event, tCanvasGesture wanted);

// Steps the parameter under the pointer by `delta` raw units and sends the change, returning true if
// one was found there. This is what a shell's bare +/- key should call; the shell decodes the key.
bool canvas_nudge_param_under_cursor(int delta);

// The same step, but on the FOCUSED parameter - what the Up/Down arrow keys act on. Both false when
// there is nothing focused, or when what was focused has since gone.
bool canvas_nudge_focused_param(int delta);
bool canvas_move_param_focus(int delta);

// Shift+arrows: move the focus to the neighbouring MODULE, by where it sits on the canvas rather
// than by index. False at the edge of the patch in that direction.
bool canvas_move_module_focus(int dx, int dy);

// notes §5
void canvas_drag_begin(void);

// notes §6
void cursor_raw_coord(double * rawX, double * rawY);
void cursor_capture(void);
void cursor_release(void);

// Dial dragging. See the long note above the definition for what each argument replaces.
bool canvas_param_drag_motion(tCoord coord, double rawX, double rawY, bool altHeld);

// Ends a dial drag: records it for undo and clears the drag state. MUST be called on mouse release
// or the dial stays held and the next click anywhere keeps dragging it.
bool canvas_param_drag_release(void);

// Scrolls the focused pane when a drag has gone past its edge. Call once per motion event while a
// module or cable drag is in progress; it times itself, so calling it more often does not scroll
// faster.
void adjust_scroll_for_drag(void);

// Right-click on the canvas: opens the connector, parameter, module or morph-label menu under the
// pointer, in that order of priority. Returns true if one was opened.
bool canvas_right_click(tCoord coord, uint32_t slot, uint32_t location);

// Right-click on empty canvas: opens the create-module menu. Call after canvas_right_click().
bool handle_module_area_click(tCoord coord);

// Updates gHoverConnector from a pointer position. Call on every move with no button down.
void canvas_hover_update(tCoord coord);

// Places a cable drag's loose end for a pointer at coord, converting to module-area coordinates and
// applying the half-connector centring offset in that order — see the definition for why the order
// is not a detail. Every site that moves the loose end must go through this.
void cable_drag_set_end(tCoord coord);

// Cable dragging. The PRESS is a click-region handler in moduleGraphics.c; motion is carried by
// canvas_drag_motion() above. This completes the drag: if the pointer is over a connector, the cable
// is created. Returns true if one was.
bool handle_cable_connect(tCoord coord, uint32_t slot, uint32_t location);
// notes §7
bool cable_touches_connector(const tCable * cable, uint32_t moduleIndex, uint32_t ioCount, tConnectorDir dir);
// Where a cable's OTHER end is, given which end is plugged into the hole being moved. Shared with the
// renderer, which draws one dragged line per cable on that hole.
void cable_far_end(const tCable * cable, uint32_t moduleIndex, uint32_t ioCount, uint32_t * farModuleIndex, uint32_t * farIoCount, tConnectorDir * farDir);
bool find_cable_at_connector(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t ioCount, tConnectorDir dir, tCableKey * key, uint32_t * otherModuleIndex, uint32_t * otherIoCount, tConnectorDir * otherDir);

// Cable-key helpers, used by the connect and by the cable popup commands.
void set_up_cable_key(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex);
bool swap_cable_to_from_if_needed(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex);

#endif // __CANVAS_DRAG_H__
