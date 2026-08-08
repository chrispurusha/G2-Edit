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
// defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
// colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
// silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
// behind the top bar and with the margin between them swallowed.
#include "defs.h"
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
#include "splitView.h"
#include "fileBrowser.h"
#include "mouseTopbar.h"
#include "topbarResourcesAccess.h"

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
        // Scrollbar and split-bar drags come first: both are chrome drawn over the canvas, and a
        // drag that began on one must not be handed to whatever module lies underneath.
        if (pane_scrollbar_dragging() == true) {
            handle_pane_scrollbar_drag(gMouse);
            return true;
        }
        handle_split_bar_cursor_pos(gMouse);

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

        {
            bool moved = canvas_drag_motion(gMouse);

            // Dragging a module or a cable past a pane's edge scrolls that pane to follow, at the
            // ramped rate the application uses. Only for those two: a rubber band selects what is
            // already visible, and a dial drag is not going anywhere.
            if ((gModuleDrag.active == true) || (gCableDrag.active == true)) {
                adjust_scroll_for_drag();
            }
            return moved;
        }
    }

    // THE BROWSER IS MODAL. While it is open nothing behind it may act on a click — the application
    // makes the same check before anything else in its own press handler.
    if (file_browser_active() == true) {
        if (phase == eClickPress) {
            handle_file_browser_mouse_down(gMouse);
            (void)handle_file_browser_click(gMouse);
        }
        return true;
    }

    if (phase == eClickPress) {
        // ANY click ends an in-progress name edit, as it does in the application — which calls these
        // before it interprets the click as anything else. Clicking the patch name again simply
        // ends the edit and the topbar starts a new one.
        stop_patch_name_editing();
        stop_module_name_editing();
        stop_param_name_editing();
        stop_perf_name_editing();

        // MENUS FIRST, exactly as mouseHandle.c orders it. An open popup must swallow the click that
        // dismisses it, and the menu bar must win over whatever the canvas has drawn underneath.
        if (handle_context_menu_click(gMouse) == true) {
            return true;
        }

            if (handle_menu_bar_click(gPluginMenuBar, g2_menu_bar_rect(get_render_width() / gGlobalGuiScale), gMouse) == true) {
            return true;
        }

        // THE TOPBAR. Its controls are NOT click regions — mouseTopbar.c hit-tests them against the
        // rectangles topbarResourcesAccess.c holds — so nothing reaches them unless they are asked
        // directly. That is why the morph dials worked (they DO register click regions, in
        // moduleGraphics.c) while every button and dial beside them did nothing.
        //
        // Straight after the menu bar and before the scrollbars, which is the application's order.
        if (handle_topbar_left_down(gMouse, gSlot) == true) {
            return true;
        }

        // Then the canvas chrome, above the modules for the same reason as during a drag.
        if (handle_split_bar_mouse(gMouse, mouseButtonLeftDown) == true) {
            return true;
        }

        if (handle_pane_scrollbar_click(gMouse) == true) {
            return true;
        }

        // Which pane the click landed in becomes the focused one, so a subsequent scroll or
        // rubber band acts on the half being worked in.
        (void)split_view_focus_at(gMouse);

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
    // EVERY TOPBAR BUTTON RELEASES, whether or not the release landed on one — the application does
    // exactly this at the top of its own mouse-up. Without it isPressed stayed set, so buttons kept
    // their pressed grey after the mouse came up, and that grey also masked the green a slot or a
    // Hide/Dim toggle had just been given by set_exclusive_button_highlight().
    for (int i = 0; i < (int)topbarControlMax; i++) {
        gTopbarControls[i].isPressed = false;
    }

    if (handle_topbar_left_up(gMouse, gSlot) == true) {
        handled = true;
    }

    (void)handle_split_bar_mouse(gMouse, mouseButtonLeftUp);
    pane_scrollbar_release();

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

    if (handle_file_browser_mouse_move(gMouse) == true) {
        return;
    }

    // Which connector the pointer is over. The canvas dims every cable not touching it, so without
    // this the plug-in never dimmed anything.
    canvas_hover_update(gMouse);
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
    if (handle_topbar_right_up(gMouse) == true) {
        return true;
    }
    if (canvas_right_click(gMouse, gSlot, gLocation) == true) {
        return true;
    }

    // Nothing on a module was hit, so this is bare canvas — the create-module menu, exactly as the
    // application falls through to it.
    return handle_module_area_click(gMouse);
}

// Wheel or trackpad. Scrolls whichever pane the pointer is over rather than the focused one, which
// is what makes a two-pane view feel right — you scroll what you are looking at.
void g2_input_scroll(double x, double y, double deltaX, double deltaY) {
    int32_t pane = 0;

    g2_input_set_mouse(x, y);
    pane = split_view_pane_at(gMouse);

    if (pane < 0) {
        return;
    }
    pane_scroll_by((uint32_t)pane, -deltaX, -deltaY);
}

// A drag tick with no new mouse event behind it.
//
// Auto-scroll only advances when something asks it to, so holding the pointer still just past a
// pane's edge would stop the scrolling dead. The application solves this by synthesising a
// cursor_pos() call from its main loop while a drag is active (graphics.c: "Artificially do
// cursor_pos call for drag scrolling when cursor not moving"); this is the same trick, driven by a
// timer in the view.
//
// Returns true if anything moved, so the caller only redraws when there is a reason to.
bool g2_input_drag_tick(void) {
    bool busy = false;

    // A DRAG needs ticking so auto-scroll keeps running with the pointer held still past a pane
    // edge; see adjust_scroll_for_drag().
    if ((gModuleDrag.active == true) || (gCableDrag.active == true)) {
        (void)canvas_drag_motion(gMouse);
        adjust_scroll_for_drag();
        busy = true;
    }

    // AN OPEN MENU needs ticking for the same kind of reason: a submenu opens on a DWELL timer, and
    // that timer only advances when something asks it to. Driving it from pointer movement alone
    // meant a flyout would not appear unless the mouse was kept jiggling on the parent item. The
    // application polls this every frame.
    if (gContextMenu.active == true) {
        update_context_menu_hover();
        update_menu_bar_hover(gPluginMenuBar, g2_menu_bar_rect(get_render_width() / gGlobalGuiScale));
        busy = true;
    }
    return busy;
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
