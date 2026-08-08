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

// Dial dragging. See the long note above the definition for what each argument replaces.
bool canvas_param_drag_motion(tCoord coord, double rawX, double rawY, bool altHeld);

// Ends a dial drag: records it for undo and clears the drag state. MUST be called on mouse release
// or the dial stays held and the next click anywhere keeps dragging it.
bool canvas_param_drag_release(void);

// Right-click on the canvas: opens the connector, parameter, module or morph-label menu under the
// pointer, in that order of priority. Returns true if one was opened.
bool canvas_right_click(tCoord coord, uint32_t slot, uint32_t location);

// Cable dragging. The PRESS is a click-region handler in moduleGraphics.c; motion is carried by
// canvas_drag_motion() above. This completes the drag: if the pointer is over a connector, the cable
// is created. Returns true if one was.
bool handle_cable_connect(tCoord coord, uint32_t slot, uint32_t location);

// Cable-key helpers, used by the connect and by the cable popup commands.
void set_up_cable_key(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex);
bool swap_cable_to_from_if_needed(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex);

#endif // __CANVAS_DRAG_H__
