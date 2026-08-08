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
#include "contextMenu.h"
#include "menuBar.h"
#include "utilsGraphics.h"
#include "g2Menu.h"

#include "g2Input.h"

// The last position the host told us about, in the canvas's logical units with a top-left origin —
// the same space get_global_gui_scaled_mouse_coord() produces in the application, so everything
// downstream is unchanged.
static tCoord gMouse = {0.0, 0.0};

// Takes PHYSICAL PIXELS and stores LOGICAL UNITS — the canvas's own space, which is what every
// hit-test and every click region is expressed in.
//
// This is the same conversion get_global_gui_scaled_mouse_coord() performs in the application, and
// it has to track gGlobalGuiScale rather than the backing scale: the two were equal only while the
// plug-in mis-sized its logical canvas.
void g2_input_set_mouse(double x, double y) {
    double scale = (gGlobalGuiScale > 0.0) ? gGlobalGuiScale : 1.0;

    gMouse.x = x / scale;
    gMouse.y = y / scale;
}

bool g2_input_mouse_event(double x, double y, eClickPhase phase) {
    bool handled = false;

    g2_input_set_mouse(x, y);

    // A drag in progress owns the motion outright — the click regions are not consulted, exactly as
    // the application's cursor_pos() does not consult them while something is being dragged.
    if (phase == eClickDrag) {
        // Dials first: a parameter drag and a module drag can never both be active, but the
        // parameter one is the common case and reads an absolute angle, so it costs nothing to ask.
        //
        // The raw coordinates are the canvas ones here. That is correct ONLY because the plug-in
        // reports eDialModeRotary, which reads an absolute angle rather than differencing against
        // the previous event. Vertical and horizontal dial modes would need genuine raw cursor
        // deltas AND start_cursor_drag()'s cursor hiding, neither of which a host view gives us yet.
        if (canvas_param_drag_motion(gMouse, gMouse.x, gMouse.y, false) == true) {
            return true;
        }
        return canvas_drag_motion(gMouse);
    }

    if (phase == eClickPress) {
        // MENUS FIRST, exactly as mouseHandle.c orders it. An open popup must swallow the click that
        // dismisses it, and the menu bar must win over whatever the canvas has drawn underneath.
        if (handle_context_menu_click(gMouse) == true) {
            return true;
        }

            if (handle_menu_bar_click(gPluginMenuBar, g2_menu_bar_rect(get_render_width() / gGlobalGuiScale), gMouse) == true) {
            return true;
        }

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

    // Ends a dial drag. Without this gParamDragging stays set, the dial remains "held" after the
    // button is up, and the next click anywhere carries on turning it.
    if (canvas_param_drag_release() == true) {
        handled = true;
    }

    // Completes a cable drag — creating the cable if the pointer came to rest on a connector, and
    // clearing the drag either way so a released-into-nothing cable does not stay attached to the
    // pointer.
    if (gCableDrag.active == true) {
        if (handle_cable_connect(gMouse, gSlot, gLocation) == true) {
            handled = true;
        }
        memset(&gCableDrag, 0, sizeof(gCableDrag));
        handled = true;
    }
    return handled;
}

// Pointer moved with no button down. Updates the position AND advances the hover state: the menu
// highlight is drawn from the pointer, and a submenu flyout opens on a dwell timer that only ticks
// when something polls it. The application polls both of these every frame (graphics.c); doing it on
// movement is enough here, since the plug-in redraws on demand rather than continuously.
void g2_input_hover(double x, double y) {
    g2_input_set_mouse(x, y);
    update_context_menu_hover();
    update_menu_bar_hover(gPluginMenuBar, g2_menu_bar_rect(get_render_width() / gGlobalGuiScale));
}

// Right button. The application opens its canvas menus on release, and the hit-test order matters —
// see canvas_right_click().
bool g2_input_right_click(double x, double y) {
    g2_input_set_mouse(x, y);

    // An open menu takes the click, as it does for the left button.
    if (handle_context_menu_click(gMouse) == true) {
        return true;
    }
    return canvas_right_click(gMouse, gSlot, gLocation);
}

// Called by the canvas when a dial drag begins (moduleGraphics.c). In the application this ALSO
// hides the cursor and warps it, which is what lets a vertical drag run past the screen edge; here
// it cannot, so the pointer stays visible and travels.
//
// SETTING THE ORIGIN IS NOT OPTIONAL, though, and leaving this empty was a real bug. The vertical and
// horizontal dial modes compute the new value as
//
//     value + (previousY - currentY) * range / 200
//
// so with no origin recorded, previousY was still 0 and the very first movement evaluated
// (0 - currentY), a large negative number that drove every dial straight to zero. Rotary hid it by
// reading an absolute angle and never touching these.
void start_cursor_drag(void) {
    canvas_drag_set_origin(gMouse.x, gMouse.y);
}

// The application reads this from GLFW; the plug-in reads it from whatever the host last delivered.
// This is the function whose absence made every canvas interaction impossible, and it is four lines.
void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    if (coord != NULL) {
        *coord = gMouse;
    }
}
