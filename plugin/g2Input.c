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
// Notes: Docs/code-notes/g2Input.c.md - "// notes §k" refers there.

// notes §1

#include "sysIncludes.h"
// notes §2
#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "clickRegion.h"
#include "globalVars.h"
#include "canvasDrag.h"
#include "inputState.h"    // multi_select_modifier_held() — real modifiers now, see g2View.m
#include "canvasCoords.h"  // canvas_zoom_step() — shared with the application's Cmd +/-
#include "contextMenu.h"
#include "menuBar.h"
#include "utilsGraphics.h"
#include "g2Menu.h"
#include "splitView.h"
#include "fileBrowser.h"
#include "synthlibPopups.h"    // synthlib_popups_dispatch_scroll() — the wheel, as the app routes it
#include "mouseTopbar.h"
#include "topbarResourcesAccess.h"
#include "palette.h"          // the module palette band, which the topbar opens under itself

#include "synthlibPopups.h"     // synthlib_popups_dispatch_key/_char() - see g2_input_popup_key()
#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>         // the key CONSTANTS only, which is what SynthLib's popups speak

#include "g2View.h"      // cursor_is_captured() — the hidden-pointer safety net
#include "g2Input.h"

// notes §3
static tCoord gMouse = {-1.0, -1.0};

// notes §4
void g2_input_set_mouse(double x, double y) {
    double scale = (gGlobalGuiScale > 0.0) ? gGlobalGuiScale : 1.0;

    gMouse.x = x / scale;
    gMouse.y = y / scale;
}

// The drag half of g2_input_mouse_event(), reading gMouse rather than being handed a position. Split
// out so a CONFINED drag can use it: while the pointer is captured it does not move, so there is no
// absolute position to pass — gMouse is advanced by the event's deltas instead (g2_input_drag_by()).
static bool dispatch_drag(void) {
    {
        // AHEAD OF EVERY OTHER GESTURE, as cursor_pos() orders it in the application: while a
        // palette tile is being dragged the pointer belongs to the palette, and the tile it is over
        // has to keep highlighting when it is not.
        palette_cursor_moved(gMouse);

        if (palette_drag_active() == true) {
            return true;
        }

        // Scrollbar and split-bar drags come first: both are chrome drawn over the canvas, and a
        // drag that began on one must not be handed to whatever module lies underneath.
        if (pane_scrollbar_dragging() == true) {
            handle_pane_scrollbar_drag(gMouse);
            return true;
        }
        handle_split_bar_cursor_pos(gMouse);

        // notes §5
        {
            // notes §6
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
}

// notes §7
bool g2_input_drag_by(double dxPixels, double dyPixels) {
    double scale = (gGlobalGuiScale > 0.0) ? gGlobalGuiScale : 1.0;

    gMouse.x += dxPixels / scale;
    gMouse.y += dyPixels / scale;
    return dispatch_drag();
}

bool g2_input_mouse_event(double x, double y, eClickPhase phase) {
    bool handled = false;

    g2_input_set_mouse(x, y);

    // A drag in progress owns the motion outright — the click regions are not consulted, exactly as
    // the application's cursor_pos() does not consult them while something is being dragged.
    if (phase == eClickDrag) {
        return dispatch_drag();
    }

    // notes §8
    if (synthlib_popups_modal_active() == true) {
        // Anything that is not a press is an up here: eClickDrag has already returned above, which
        // leaves eClickRelease and eClickReleaseOutside, and the popups treat both as the release.
        (void)synthlib_popups_dispatch_click(gMouse, (phase == eClickPress) ? mouseButtonLeftDown
                                                                           : mouseButtonLeftUp);
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

        // notes §9
        if (handle_topbar_left_down(gMouse, gSlot) == true) {
            return true;
        }

        // The palette band sits directly under the topbar and above the whole canvas, so it is
        // tested here rather than with the canvas: a press on a tile must not also reach the modules
        // underneath and start a rubber band. Returns false whenever the band is closed.
        if (palette_left_down(gMouse) == true) {
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

        // notes §10
        return canvas_empty_press(gMouse, false);
    }

    // notes §11
    for (int i = 0; i < (int)topbarControlMax; i++) {
        gTopbarControls[i].isPressed = false;
    }

    // notes §12
    if (palette_left_up(gMouse) == true) {
        handled = true;
    } else if (handle_topbar_left_up(gMouse, gSlot) == true) {
        handled = true;
    }

    (void)handle_split_bar_mouse(gMouse, mouseButtonLeftUp);
    pane_scrollbar_release();

    handled = dispatch_click_region(gMouse, phase);

    // notes §13
    if (canvas_gesture_release(&(tCanvasGestureEvent){
                                   .coord = gMouse, .rawX = gMouse.x, .rawY = gMouse.y,
                                   .slot = gSlot, .location = gLocation,
                                   .altHeld = alt_modifier_held(), .additive = multi_select_modifier_held()
                               }, canvasGestureAll) != canvasGestureNone) {
        handled = true;
    }

    // PAIRED WITH cursor_capture() HERE, not left to the tick's safety poll. The poll exists for the
    // release that never arrives; this is the one that does, and going through it directly means the
    // pointer comes back the instant the button does rather than up to a tick later.
    cursor_release();
    return handled;
}

// notes §14
void g2_input_hover(double x, double y) {
    g2_input_set_mouse(x, y);

    if (handle_file_browser_mouse_move(gMouse) == true) {
        return;
    }

    // The tile hover highlight. The application drives this from cursor_pos(), which covers both a
    // bare move and a drag; here the two are separate entry points, so both call it.
    palette_cursor_moved(gMouse);

    // Which connector the pointer is over. The canvas dims every cable not touching it, so without
    // this the plug-in never dimmed anything.
    canvas_hover_update(gMouse);
    update_context_menu_hover();
    update_menu_bar_hover(gPluginMenuBar, g2_menu_bar_rect(get_render_width() / gGlobalGuiScale));
}

// notes §15
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

    // notes §16
    if (synthlib_popups_dispatch_scroll(deltaY / WHEEL_SCROLL_STEP)) {
        return;
    }

    // notes §17
    if (palette_scroll(deltaY / WHEEL_SCROLL_STEP, gMouse) == true) {
        return;
    }
    pane = split_view_pane_at(gMouse);

    if (pane < 0) {
        return;
    }

    // notes §18
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

// notes §19
bool g2_input_drag_tick(void) {
    bool busy = false;

    // notes §20
    if (cursor_is_captured() == true) {
        busy = true;
    }

    // A DRAG needs ticking so auto-scroll keeps running with the pointer held still past a pane
    // edge; see adjust_scroll_for_drag().
    if ((gModuleDrag.active == true) || (gCableDrag.active == true)) {
        (void)canvas_drag_motion(gMouse);
        adjust_scroll_for_drag();
        busy = true;
    }

    // notes §21
    if (gContextMenu.active == true) {
        update_context_menu_hover();
        update_menu_bar_hover(gPluginMenuBar, g2_menu_bar_rect(get_render_width() / gGlobalGuiScale));
        busy = true;
    }
    return busy;
}

// notes §22
void cursor_raw_coord(double * rawX, double * rawY) {
    if (rawX != NULL) {
        *rawX = gMouse.x;
    }

    if (rawY != NULL) {
        *rawY = gMouse.y;
    }
}

// notes §23

// The application reads this from GLFW; the plug-in reads it from whatever the host last delivered.
// This is the function whose absence made every canvas interaction impossible, and it is four lines.
void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    if (coord != NULL) {
        *coord = gMouse;
    }
}

// notes §24
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

// notes §25
static int glfw_key_for_mac(unsigned short keyCode) {
    switch (keyCode) {
        case 53:  return GLFW_KEY_ESCAPE;
        case 36:  return GLFW_KEY_ENTER;
        case 76:  return GLFW_KEY_KP_ENTER;
        case 51:  return GLFW_KEY_BACKSPACE;
        case 117: return GLFW_KEY_DELETE;
        case 48:  return GLFW_KEY_TAB;
        case 123: return GLFW_KEY_LEFT;
        case 124: return GLFW_KEY_RIGHT;
        case 125: return GLFW_KEY_DOWN;
        case 126: return GLFW_KEY_UP;
        case 115: return GLFW_KEY_HOME;
        case 119: return GLFW_KEY_END;
        case 116: return GLFW_KEY_PAGE_UP;
        case 121: return GLFW_KEY_PAGE_DOWN;
        default:  return GLFW_KEY_UNKNOWN;
    }
}

// The first code point of a UTF-8 string, or 0. Only what a filename box could be sent.
static unsigned int first_codepoint(const char * utf8) {
    const unsigned char * s = (const unsigned char *)utf8;

    if ((s == NULL) || (s[0] == 0u)) {
        return 0u;
    }

    if (s[0] < 0x80u) {
        return s[0];
    }

    if (((s[0] & 0xE0u) == 0xC0u) && (s[1] != 0u)) {
        return ((s[0] & 0x1Fu) << 6) | (s[1] & 0x3Fu);
    }

    if (((s[0] & 0xF0u) == 0xE0u) && (s[1] != 0u) && (s[2] != 0u)) {
        return ((s[0] & 0x0Fu) << 12) | ((s[1] & 0x3Fu) << 6) | (s[2] & 0x3Fu);
    }
    return 0u;
}

bool g2_input_popup_key(unsigned short macKeyCode, const char * characters, bool isRepeat) {
    int  key  = glfw_key_for_mac(macKeyCode);
    bool used = false;

    if (key != GLFW_KEY_UNKNOWN) {
        used = synthlib_popups_dispatch_key(key, 0, isRepeat ? GLFW_REPEAT : GLFW_PRESS);
    } else {
        unsigned int codepoint = first_codepoint(characters);

        // Printable only: a control character here is a key that has no GLFW name above.
        if ((codepoint >= 0x20u) && (codepoint != 0x7Fu)) {
            used = synthlib_popups_dispatch_char(codepoint);
        }
    }

    if (used) {
        synthlib_request_redraw();
    }
    return used;
}
