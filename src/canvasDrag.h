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

#ifndef __CANVAS_DRAG_H__
#define __CANVAS_DRAG_H__

#include "sysIncludes.h"
#include "types.h"

// Dragging on the module canvas, with no window system in it.
//
// The press that STARTS a drag already lives in a click-region handler (moduleGraphics.c), which the
// plug-in shares. What did not was the motion that carries it: that was buried in cursor_pos() in
// mouseHandle.c, a GLFW callback. So a module drag in the plug-in began and then nothing moved,
// because the code that moves it could not be linked.
//
// These take a coordinate rather than reading one, which is the whole of what made them
// unshareable — cursor_pos() itself never used its GLFWwindow argument.
//
// NOT the whole of cursor_pos(): param dragging, cable dragging, tempo/vibrato/glide and connector
// hover remain there. Those are the next pieces to move if the plug-in is to edit values as well as
// move modules.

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

// ── The canvas gestures, declared as a set ──────────────────────────────────────────────────────
//
// A GESTURE HAS THREE PHASES AND NOTHING USED TO SAY SO. Press lives in a click-region handler, motion
// and release live here, and each shell wired the phases up by hand — the application in
// mouseHandle.c, the plug-in in g2Input.c. Two hand-maintained ladders for the same four gestures,
// and the consequences were not theoretical (vst3/plugin-gui-notes.md, observation 1):
//
//   - The plug-in never reached the module drag's release, so a dropped module was never re-ordered.
//   - It never reached the dial drag's release, so a dial stayed held after mouse-up and the next
//     click anywhere carried on turning it.
//   - The application grew its OWN copy of the module-drag release, inline in its mouse-up handler,
//     while canvas_module_drag_release() sat here used only by the plug-in — two implementations of
//     one phase, free to drift.
//   - The two shells even ran their release phases in different orders, and only one order could have
//     been the considered one.
//
// The table in canvasDrag.c now names all four gestures and their phases in one place, so a gesture
// with a phase left unwired is a visible hole in a table row rather than a silence. A shell asks for
// motion or release and the table decides who wants it.
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

// Offers the motion to each gesture in turn and returns the one that took it, or canvasGestureNone.
// The identity is returned rather than a bool because a shell may have work of its own to add — the
// application auto-scrolls the canvas for a module or cable drag, which belongs to whoever owns the
// scrollbars.
tCanvasGesture canvas_gesture_motion(const tCanvasGestureEvent * event);

// Releases every gesture in `wanted` that is in progress, and returns the set that acted.
//
// WHY A MASK RATHER THAN A SWEEP OF ALL FOUR: the application interleaves dispatch_click_region() in
// the middle of its release handling — module and cable first, then the click regions, then the rubber
// band — and that ordering is load-bearing for press-captured widgets. Rather than quietly changing
// it, a shell says which gestures it wants released at this point. Passing canvasGestureAll is the
// simple case and is what the plug-in does.
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

// ── Starting a drag: the logic half is shared, the platform half is optional ─────────────────────
//
// CALL THIS TO BEGIN ANY CURSOR-CAPTURING DRAG. It records the origin — which every incremental dial
// mode depends on — and THEN asks the shell to capture the pointer. It replaced start_cursor_drag(),
// which did both jobs in one function per shell, and that is the point rather than tidiness:
//
// The application's version recorded the origin and hid the pointer. The plug-in's version was
// briefly an empty stub, because "hide the pointer" is not something a host-owned NSView can simply
// do — and stubbing it out silently took the ORIGIN with it, so vertical and horizontal dial drags
// slammed to zero while rotary (which reads an absolute angle) looked perfect. See
// vst3/plugin-gui-notes.md, observation 4. Splitting them means a shell can decline the platform half
// and cannot drop the logic half by accident.
void canvas_drag_begin(void);

// Implemented by the SHELL, not here.
//
// cursor_raw_coord() reports the pointer in whatever space that shell reports MOTION in — raw window
// coordinates for GLFW, canvas coordinates for the plug-in. It only has to agree with itself: the
// origin is only ever differenced against later positions from the same source.
//
// cursor_capture()/cursor_release() hide and confine the pointer for the duration of a drag, so an
// incremental drag is not limited by the edge of the screen. BOTH MAY BE NO-OPS — a plug-in in a host
// window is entitled to decline, and declining now costs it only the pointer hiding.
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
// The cable attached to a connector, and where its other end is — see the definition. Used by the
// Ctrl-click pick-up, which needs to start a drag from the far end of an existing cable.
// Is either end of this cable plugged into that hole? Shared with the renderer, which hides every
// cable on a hole being dragged.
bool cable_touches_connector(const tCable * cable, uint32_t moduleIndex, uint32_t ioCount, tConnectorDir dir);
// Where a cable's OTHER end is, given which end is plugged into the hole being moved. Shared with the
// renderer, which draws one dragged line per cable on that hole.
void cable_far_end(const tCable * cable, uint32_t moduleIndex, uint32_t ioCount, uint32_t * farModuleIndex, uint32_t * farIoCount, tConnectorDir * farDir);
bool find_cable_at_connector(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t ioCount, tConnectorDir dir, tCableKey * key, uint32_t * otherModuleIndex, uint32_t * otherIoCount, tConnectorDir * otherDir);

// Cable-key helpers, used by the connect and by the cable popup commands.
void set_up_cable_key(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex);
bool swap_cable_to_from_if_needed(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex);

#endif // __CANVAS_DRAG_H__
