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

#ifndef __G2_INPUT_H__
#define __G2_INPUT_H__

#include "sysIncludes.h"
#include "types.h"
#include "clickRegion.h"

#ifdef __cplusplus
extern "C" {
#endif

// Coordinates are in the canvas's LOGICAL units with a TOP-LEFT origin. Cocoa's are points with a
// bottom-left origin, so the view flips y before calling — doing it at the boundary keeps the
// flip in one place, next to the only code that knows Cocoa's convention.

// Record the pointer position without dispatching anything. For move/hover, and so that a handler
// which reads the mouse mid-drag sees where it actually is.
void g2_input_set_mouse(double x, double y);

// Record the position and dispatch it to whatever click region is under it. Returns true if a region
// handled it. Press captures, so the matching release goes to the same region wherever the pointer
// has travelled — see clickRegion.h.
bool g2_input_mouse_event(double x, double y, eClickPhase phase);

// Pointer moved with no button down: updates position and advances menu hover/dwell state.
void g2_input_hover(double x, double y);

// Right button released: opens the connector/parameter/module/morph menu under the pointer.
bool g2_input_right_click(double x, double y);

// Wheel/trackpad scroll, in physical pixels. Scrolls the pane under the pointer.
void g2_input_scroll(double x, double y, double deltaX, double deltaY);

// Advances a module/cable drag with no new mouse event — what keeps auto-scroll running while the
// pointer is held still past a pane edge. Returns true if anything moved.
bool g2_input_drag_tick(void);

#ifdef __cplusplus
}
#endif

#endif // __G2_INPUT_H__
