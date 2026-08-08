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

// Ends a module drag without the application's follow-up (overlap shuffling and the move undo entry,
// both of which need menus.c). Intended for the plug-in; the application has its own richer path.
bool canvas_module_drag_clear(void);

#endif // __CANVAS_DRAG_H__
