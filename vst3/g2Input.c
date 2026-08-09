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
#include "inputState.h"    // multi_select_modifier_held() — real modifiers now, see g2GlView.m
#include "canvasCoords.h"  // canvas_zoom_step() — shared with the application's Cmd +/-
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
//
// STARTS OFF-CANVAS, and {0,0} would be a bug: it is not a neutral "unknown", it is the top-left
// corner, which is exactly where the menu bar's first item sits. render_menu_bar() highlights
// whatever the pointer is inside, so a freshly-opened editor drew "File" lit up before the host had
// said anything about the pointer at all — and it stayed lit, because the view's tracking area is
// NSTrackingActiveInKeyWindow and delivers no movement until the plug-in window becomes key. The
// application never had this: GLFW reports a real cursor position from the first frame.
static tCoord gMouse = {-1.0, -1.0};

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
        // The raw coordinates are the canvas ones, and that is SELF-CONSISTENT rather than a
        // compromise: cursor_raw_coord() records the drag origin from this same gMouse, so the
        // incremental dial modes difference two values in one space. All three dial modes therefore
        // work here — the mode comes from this plug-in's own prefs and its own Controls menu, and
        // eDialModeRotary is only the fallback default in g2Prefs.c when that pref is absent.
        //
        // What is missing without a real cursor_capture() is pointer HIDING and confinement, not the
        // arithmetic: the pointer visibly travels away from the dial, and a long drag can run out of
        // screen. An earlier comment here claimed the plug-in "reports eDialModeRotary" and that the
        // other modes could not work at all, which was simply untrue.
        {
            // All four gestures through the shared table, in the one order that is written down —
            // see canvasDrag.h.
            //
            // ALT IS REAL NOW, so an Alt-drag on a dial adjusts its MORPH OFFSET rather than its value,
            // as it does in the application. It works in every dial mode: Alt only changes which field
            // the resulting value is written to, and the incremental modes are sound here for the
            // reason given above.
            tCanvasGesture took = canvas_gesture_motion(&(tCanvasGestureEvent){
                                                            .coord = gMouse, .rawX = gMouse.x, .rawY = gMouse.y,
                                                            .slot = gSlot, .location = gLocation,
                                                            .altHeld = alt_modifier_held(), .additive = multi_select_modifier_held()
                                                        });

            // Dragging a module or a cable past a pane's edge scrolls that pane to follow, at the
            // ramped rate the application uses. Only for those two: a rubber band selects what is
            // already visible, and a dial drag is not going anywhere.
            if ((took == canvasGestureModule) || (took == canvasGestureCable)) {
                adjust_scroll_for_drag();
            }
            return took != canvasGestureNone;
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

    // EVERY GESTURE'S RELEASE, IN ONE CALL. This was four hand-written blocks in a different order
    // from the application's four, which is the asymmetry that let this shell quietly miss phases: the
    // module drag went un-re-ordered and dials stayed held after mouse-up, each until someone noticed.
    // canvasGestureAll is the simple case — the application passes masks only because it interleaves
    // dispatch_click_region() partway through. See canvasDrag.h.
    //
    // Undo is still the one thing not carried over here: a plug-in has no undo stack, which is exactly
    // why the application wraps the param release in finish_param_drag() rather than the table doing it.
    if (canvas_gesture_release(&(tCanvasGestureEvent){
                                   .coord = gMouse, .rawX = gMouse.x, .rawY = gMouse.y,
                                   .slot = gSlot, .location = gLocation,
                                   .altHeld = alt_modifier_held(), .additive = multi_select_modifier_held()
                               }, canvasGestureAll) != canvasGestureNone) {
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

// The pointer has left the plug-in's view. Parks it back off-canvas so nothing hit-tests true — the
// menu bar item and the connector the pointer was last over both stop being highlighted, rather than
// staying lit until it comes back. Same sentinel the position starts at, and for the same reason.
//
// The caller ignores an exit that arrives with a button still down: a drag deliberately continues
// outside the view, and AppKit keeps delivering its movement.
void g2_input_pointer_left(void) {
    gMouse = (tCoord){
        -1.0, -1.0
    };
    canvas_hover_update(gMouse);   // clears gHoverConnector: (-1, -1) is in no pane, so it returns early
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

    // CMD + WHEEL ZOOMS, as it does in the application, around the pointer rather than the corner.
    //
    // ONE STEP PER EVENT rather than scaling by the delta: the deltas arriving here are PIXELS (see
    // the caller in g2GlView.m, which multiplies by the backing scale), so feeding them to a zoom
    // factor that moves in 0.1 steps would fling the canvas from one limit to the other on a single
    // flick. A notch is a step, which is what Cmd +/- does too.
    if (cmd_modifier_held() == true) {
        if (deltaY != 0.0) {
            uint32_t prevPane = module_pane();

            set_module_pane((uint32_t)pane);
            canvas_zoom_step_at((deltaY > 0.0) ? ZOOM_DELTA : -ZOOM_DELTA, gMouse);
            set_module_pane(prevPane);
        }
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
// ── The plug-in's half of the drag-begin seam (canvasDrag.h) ────────────────────────────────────
//
// This WAS start_cursor_drag(), a per-shell function that had to remember to record the drag origin.
// It is now two named jobs, and the one the plug-in cannot do is the one it is allowed to decline.
//
// The plug-in reports motion in canvas coordinates, so the origin is recorded in those — it is only
// ever differenced against later positions from this same source, so the space just has to agree with
// itself. Recording it in raw screen coordinates while reporting motion in canvas ones is precisely
// the kind of mismatch this seam's comment warns about.
void cursor_raw_coord(double * rawX, double * rawY) {
    if (rawX != NULL) {
        *rawX = gMouse.x;
    }

    if (rawY != NULL) {
        *rawY = gMouse.y;
    }
}

// DELIBERATE NO-OPS, and no longer silently damaging. An NSView owned by the host is not ours to
// confine the pointer inside, and the previous arrangement meant declining that also discarded the
// drag origin — vertical and horizontal dial drags collapsed to zero while rotary looked perfect,
// because rotary reads an absolute angle and never touches the origin.
//
// WHAT IS LOST IS HIDING, NOT FUNCTION. All three dial modes work — see g2_input_mouse_event() — and
// what a real implementation would add is NSCursor hide/unhide plus
// CGAssociateMouseAndMouseCursorPosition (or CGDisplayHideCursor with warping), so that the pointer
// stays put on the dial instead of travelling away from it and eventually running out of screen.
void cursor_capture(void) {
}

void cursor_release(void) {
}

// The application reads this from GLFW; the plug-in reads it from whatever the host last delivered.
// This is the function whose absence made every canvas interaction impossible, and it is four lines.
void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    if (coord != NULL) {
        *coord = gMouse;
    }
}

// ── Keyboard ────────────────────────────────────────────────────────────────────────────────────
//
// The shell decodes, the shared code acts — the same split as the modifier seam. g2GlView.m hands over
// a character it took from -charactersIgnoringModifiers (so this sees the key's unshifted meaning, and
// '+' and '=' are both worth accepting) plus whether Command was down.
//
// UNTIL NOW THIS VIEW RECEIVED NO KEY EVENTS AT ALL: NSView's -acceptsFirstResponder is NO by default
// and nothing had overridden it, so neither -keyDown: nor -flagsChanged: was ever called. Both are
// live now, which is also what makes a modifier pressed mid-drag — with no mouse movement to carry it
// — register at all.
//
// Returns true if the key was used, so the view can leave it alone otherwise and let the host have it.
// That matters: a host owns shortcuts like the space bar for transport, and a plug-in that swallowed
// everything would be worse than one that swallowed nothing.
bool g2_input_key(int character, bool cmdHeld) {
    if (cmdHeld == true) {
        // Cmd +/- is the canvas zoom, the same pair and the same step the application uses.
        if ((character == '=') || (character == '+')) {
            canvas_zoom_step(ZOOM_DELTA);
            return true;
        }

        if (character == '-') {
            canvas_zoom_step(-ZOOM_DELTA);
            return true;
        }
        return false;
    }

    // Bare +/- steps the parameter under the pointer by one raw unit.
    if ((character == '=') || (character == '+')) {
        return canvas_nudge_param_under_cursor(1);
    }

    if (character == '-') {
        return canvas_nudge_param_under_cursor(-1);
    }
    return false;
}
