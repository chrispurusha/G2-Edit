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

// The plug-in's mouse input, in C. g2GlView.m turns Cocoa events into calls on this; nothing below
// knows what an NSEvent is.
//
// THE HIT-TESTING IS NOT REIMPLEMENTED HERE, and that is the point. Every clickable thing on the
// canvas — module bodies, dials, toggles, connectors — registers a click region as it is DRAWN
// (moduleGraphics.c, renderParams.c), and SynthLib's dispatch_click_region() resolves a coordinate
// against them. That machinery is already platform-free and the plug-in already runs it, so all the
// application's hit-testing behaviour, including press-capture and layer priority, comes for free.
// What was missing was only somebody to say where the mouse is.
//
// WHERE A CLICK ENDS UP: the handlers write to the module database and then call msg_send() to tell
// the G2. In the plug-in msg_send() is a no-op (g2AppStubs.c) because there is no G2 — but the
// database write still happens, and sound_engine_update_from_patch() reads exactly that. So a dial
// drag here changes the patch and the sound with no hardware in the path at all, which is the
// behaviour a plug-in wants rather than a limitation of it.

#include "sysIncludes.h"
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "clickRegion.h"
#include "globalVars.h"
#include "canvasDrag.h"

#include "g2Input.h"

// The last position the host told us about, in the canvas's logical units with a top-left origin —
// the same space get_global_gui_scaled_mouse_coord() produces in the application, so everything
// downstream is unchanged.
static tCoord gMouse = {0.0, 0.0};

void g2_input_set_mouse(double x, double y) {
    gMouse.x = x;
    gMouse.y = y;
}

bool g2_input_mouse_event(double x, double y, eClickPhase phase) {
    bool handled = false;

    g2_input_set_mouse(x, y);

    // A drag in progress owns the motion outright — the click regions are not consulted, exactly as
    // the application's cursor_pos() does not consult them while something is being dragged.
    if (phase == eClickDrag) {
        return canvas_drag_motion(gMouse);
    }

    if (phase == eClickPress) {
        if (dispatch_click_region(gMouse, phase) == true) {
            return true;
        }

        // NOTHING WAS UNDER THE POINTER. In the application this is where a click on bare canvas
        // clears the selection and starts a rubber band; dispatch_click_region() returning false is
        // the whole of how "empty space" is detected, since every occupied part of the canvas
        // registers a region. Without this the plug-in could select a module but never deselect one.
        //
        // No modifier plumbing yet, so a press always replaces the selection rather than adding to
        // it — multi_select_modifier_held() is still false in the plug-in.
        return canvas_empty_press(gMouse, false);
    }

    // ---- release ---------------------------------------------------------------------------------
    //
    // DO NOT RETURN EARLY WHEN dispatch_click_region() CLAIMS THE RELEASE. A press CAPTURES its
    // region (see clickRegion.h), so the handler that began a module drag owns the matching release
    // and dispatch always reports it handled. Returning there was a real bug: the drag was never
    // ended and never re-ordered, so a module dropped on another simply overlapped it, and
    // gModuleDrag stayed active into the next gesture.
    //
    // Both must run: the captured handler needs its release, and the drag needs finishing.
    handled = dispatch_click_region(gMouse, phase);

    if (canvas_rubber_band_release(gMouse, gSlot, gLocation, false) == true) {
        handled = true;
    }

    // Ends the module drag and re-orders around it, as the application does. Undo is the one thing
    // not carried over — a plug-in has no undo stack.
    if (canvas_module_drag_release() == true) {
        handled = true;
    }
    return handled;
}

// The application reads this from GLFW; the plug-in reads it from whatever the host last delivered.
// This is the function whose absence made every canvas interaction impossible, and it is four lines.
void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    if (coord != NULL) {
        *coord = gMouse;
    }
}
