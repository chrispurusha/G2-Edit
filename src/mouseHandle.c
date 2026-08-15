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

#ifdef __cplusplus
extern "C" {
#endif

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop


#include <math.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "topbarResourcesAccess.h"
#include "utilsGraphics.h"
#include "mouseHandle.h"
#include "canvasDrag.h"
#include "graphics.h"
#include "splitView.h"
#include "globalVars.h"
#include "protocol.h"
#include "menus.h"
#include "mousePanels.h"
#include "mouseTopbar.h"
#include "selection.h"
#include "undo.h"
#include "mutatorUI.h"
#include "paramPages.h"
#include "paramOverview.h"
#include "midiCcList.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"
#include "helpPanel.h"
#include "misc.h"
#include "appMenuBar.h"
#include "fileBrowser.h"
#include "bankBrowser.h"
#include "alertDialog.h"
#include "clickRegion.h"
#include "cableChain.h"
#include "inputState.h"

// Drag-start state moved to canvasDrag.c along with the parameter-drag arm that uses it.
static int gDragSkipCount = 0;      // skip first N cursor_pos events after CURSOR_DISABLED — covers stale NORMAL-mode events + transition event

void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    int winWidth  = 0;
    int winHeight = 0;

    glfwGetCursorPos(synthlib_window(), &(coord->x), &(coord->y));
    glfwGetWindowSize(synthlib_window(), &winWidth, &winHeight);

    coord->x = (coord->x / (double)winWidth) * (get_render_width() / gGlobalGuiScale);
    coord->y = (coord->y / (double)winHeight) * (get_render_height() / gGlobalGuiScale);
}

static void send_master_clock_bpm(uint32_t bpm) {
    tMessageContent messageContent = {0};

    messageContent.cmd                    = eMsgCmdSetMasterClockBPM;
    messageContent.masterClockBPMData.bpm = bpm;
    msg_send(&gToUsbThread, &messageContent);
}

// THE MODIFIER SEAM'S APPLICATION END IS ONE CALL PER EVENT, and both the translation and the
// predicates are SynthLib's — set_modifier_state_from_glfw() in inputStateGlfw.c, shared with
// SynthEdit, and shift_modifier_held() and friends in inputState.c, shared with the plug-in, which
// pushes the same bits from an NSEvent.
//
// GLFW ALREADY HANDED US THIS ON EVERY EVENT and nobody read it. Both key_callback() and
// mouse_button() take an `int mods` argument describing the modifier state AT THE MOMENT OF THE
// EVENT, while three separate predicates polled glfwGetKey() for the same answer a little later.
// Pushing the argument is not merely tidier, it is more correct: the poll answered "now", and "now"
// is after the event has been queued.

// Registered with GLFW so a modifier released while another application has the keyboard cannot
// leave one stuck on here — see set_modifier_state()'s note. There is nothing to restore on the way
// back in: the next key or button event carries the truth with it.
void window_focus_callback(GLFWwindow * window, int focused) {
    (void)window;

    if (focused == 0) {
        set_modifier_state((uint32_t)eModifierNone);
    }
}

// WHETHER WE ACTUALLY HID THE CURSOR, tracked explicitly rather than inferred from the drag flags.
//
// stop_dragging() used to decide by asking is_cursor_hidden_dragging(), i.e. by re-reading the very
// state it was about to clear. That is fragile in two ways, and both have bitten:
//
//   - Anything that clears a drag flag BEFORE stop_dragging() runs leaves the cursor hidden for good.
//     finish_param_drag() started doing exactly that when its undo push moved out to
//     canvas_param_drag_release(), which memsets gParamDragging — so dragging the slot volume, or any
//     dial, could strand the pointer with no way to get it back.
//   - A spurious or duplicated mouse-up has the same effect for the same reason.
//
// A flag set where the cursor is hidden and cleared where it is restored cannot disagree with itself.
static bool sCursorHidden = false;

// ── The application's half of the drag-begin seam (canvasDrag.h) ────────────────────────────────
//
// start_cursor_drag() USED TO BE HERE and did both jobs at once. It is now canvas_drag_begin() in
// canvasDrag.c, which records the origin and then calls these — so the shared canvas code no longer
// depends on a per-shell function remembering to do the logic half. See canvasDrag.h for the bug that
// argues for the split.

// GLFW reports motion in raw window coordinates, so the origin is recorded in those.
void cursor_raw_coord(double * rawX, double * rawY) {
    double x = 0.0;
    double y = 0.0;

    glfwGetCursorPos(synthlib_window(), &x, &y);

    if (rawX != NULL) {
        *rawX = x;
    }

    if (rawY != NULL) {
        *rawY = y;
    }
}

// Is any in-window text field taking keystrokes? Note entry and any other bare-letter shortcut has
// to stand aside while one is: the letter belongs to the name being typed.
static bool any_text_edit_active(void) {
    return gPatchNameEdit.active
           || gPatchNotesEdit.active
           || gModuleNameEdit.active
           || gParamNameEdit.active
           || gSynthNameEdit.active
           || gPerfNameEdit.active;
}

void cursor_capture(void) {
    gDragSkipCount = 3;
    glfwSetInputMode(synthlib_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    sCursorHidden  = true;
}

// Unconditional on the flag rather than on any drag state — see sCursorHidden above. Restoring a
// pointer that is already visible costs nothing; failing to restore a hidden one costs the user their
// pointer, which is why every restore funnels through here rather than calling GLFW directly.
void cursor_release(void) {
    if (sCursorHidden == true) {
        glfwSetInputMode(synthlib_window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        sCursorHidden = false;
    }
}

// Pure legacy fallback now — morph group dials register their own click
// region (eClickLayerPanel, moduleGraphics.cpp's morph_param_click_handler)
// which dispatch_click_region() already checks, and wins over regular
// modules, before this is ever reached. See mouse_button().
// Step the parameter under the cursor by one raw wire unit. A dial maps its whole range across a
// few tens of pixels, so a drag cannot reliably land on a chosen value, let alone move by exactly
// one - which is what identifying a parameter's real quantisation needs (where does the synth's own
// display actually change?). The step is written to the device exactly as a drag's is, so the G2
// reacts to each one. Pair it with View > Parameter Values to see the raw number being stepped.

void set_x_scroll_bar(double x) {
    gScrollState.xBar = clamp_scroll_bar(x, get_render_width());
    set_x_scroll_percent(get_scroll_bar_percent(gScrollState.xBar, get_render_width() / gGlobalGuiScale));
}

void set_y_scroll_bar(double y) {
    gScrollState.yBar = clamp_scroll_bar(y, get_render_height());
    set_y_scroll_percent(get_scroll_bar_percent(gScrollState.yBar, get_render_height() / gGlobalGuiScale));
}

bool handle_scrollbar_click(tCoord coord) {
    // Both axes belong to the split view now — one vertical bar per pane and one horizontal per
    // pane, all proportional. gScrollState's own thumbs are no longer drawn or hit-tested.
    return handle_pane_scrollbar_click(coord);
}

// Last resort: the cursor is hidden but nothing is being dragged any more.
//
// The explicit flag handles a spurious or duplicated mouse-up — those still reach stop_dragging(),
// which restores. What it cannot handle is a mouse-up that never ARRIVES at all: no stop_dragging,
// no restore, pointer gone. Polled from the render loop so that state cannot persist for more than a
// frame, which is a better answer than debouncing the button — a debounce delays every real release
// to guard against a rare bad one, and still loses if the event is dropped rather than repeated.
//
// THE FIRST HALF OF THIS WAS UNREACHABLE IN THE CASE IT WAS WRITTEN FOR, which is why the pointer
// still went missing occasionally. It asked is_cursor_hidden_dragging(), i.e. our own drag flags — and
// those are set by the press and cleared by the release. Lose the release and gParamDragging.active
// stays set for ever, so the condition below is never true and the poll does nothing, every frame,
// for as long as the application runs. A poll whose trigger depends on the event that went missing
// cannot recover from the event going missing.
//
// So the hardware is asked instead. If no button is physically down while we still hold the pointer,
// the release is synthesised through the ORDINARY path — mouse_button() with a GLFW_RELEASE, exactly
// what the callback would have delivered — so the drag ends the way it should have, undo entry
// included, rather than being torn down by hand here. Same authority and same approach as the
// plug-in shell's recoverLostRelease (vst3/g2GlView.m).
void recover_lost_cursor(void) {
    if ((sCursorHidden == true) && (platform_any_mouse_button_down() == false)) {
        // The modifier state is saved and put back around the synthetic release. mouse_button() begins by
        // pushing set_modifier_state_from_glfw(mods) so its handlers can read the predicates, and there
        // are no real mods to pass here — a literal 0 would report every modifier as released. Alt in
        // particular is what tells a dial drag to move the morph offset rather than the value, so
        // clearing it while still holding a key would leave the next gesture reading the wrong one until
        // some real event happened to refresh it.
        uint32_t modifiers = modifier_state();

        mouse_button(synthlib_window(), GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
        set_modifier_state(modifiers);
    }

    if (is_cursor_hidden_dragging() == false) {
        cursor_release();
    }
}

bool is_cursor_hidden_dragging(void) {
    return gParamDragging.active || gTempoDragging || gPerfTempoDragging || gVibRateDragging || gVibAmountDragging || gGlideTimeDragging;
}

// Ends a param/mode dial drag: records the undo entry for the whole drag as one old->new pair,
// then clears every drag state. Shared with the Parameter Pages panel, which drives the same
// gParamDragging machinery but swallows the mouse-up before the canvas handler below ever sees
// it - so without this the panel's drags would be silently missing from Ctrl-Z.
void finish_param_drag(void) {
    // Undo push and clear are shared with the plug-in — see canvasDrag.h. stop_dragging() below
    // still clears the other drag kinds and restores the cursor.
    // Through the table like the other three, so the param gesture is not the one exception that
    // reaches its release by a private path. The undo push inside canvas_param_drag_release() and the
    // cursor restore in stop_dragging() are what keep this wrapper application-side.
    {
        tCoord coord = {0};

        get_global_gui_scaled_mouse_coord(&coord);
        (void)canvas_gesture_release(&(tCanvasGestureEvent){
            .coord = coord, .slot = gSlot, .location = gLocation
        }, canvasGestureParam);
    }
    stop_dragging();
}

void stop_dragging(void) {
    gScrollState.yBarDragging = false;
    gScrollState.xBarDragging = false;
    pane_scrollbar_release();
    memset(&gModuleDrag, 0, sizeof(gModuleDrag));
    memset(&gParamDragging, 0, sizeof(gParamDragging));
    memset(&gCableDrag, 0, sizeof(gCableDrag));
    gTempoDragging            = false;
    gPerfTempoDragging        = false;
    gVibRateDragging          = false;
    gVibAmountDragging        = false;
    gGlideTimeDragging        = false;
    gRubberBand.active        = false;
    gDragSkipCount            = 0;

    // No explicit glfwSetCursorPos(gDragStartX, gDragStartY) here — GLFW's
    // cocoa backend already restores the cursor to wherever it was when
    // CURSOR_DISABLED was entered, as soon as we switch back to NORMAL (see
    // updateCursorMode() in cocoa_window.m). An extra explicit warp on top
    // of that was redundant, and two independent warps in a row can land a
    // pixel or two off from each other — enough, in SynthEdit's tightly
    // packed filter dials, to spill onto a neighbouring control.
    cursor_release();
}

tMouseButton convert_to_mouse_button(int button, int action) {
    tMouseButton mouseButton = mouseButtonNone;

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            mouseButton = mouseButtonLeftDown;
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            mouseButton = mouseButtonRightDown;
        }
    } else if (action == GLFW_RELEASE) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            mouseButton = mouseButtonLeftUp;
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            mouseButton = mouseButtonRightUp;
        }
    }
    return mouseButton;
}

void mouse_button(GLFWwindow * window, int button, int action, int mods) {
    tCoord       coord       = {0};
    tMouseButton mouseButton = mouseButtonNone;
    bool         found       = false;
    int32_t      i           = 0;
    uint32_t     slot        = gSlot;
    uint32_t     location    = gLocation;

    set_modifier_state_from_glfw(mods);   // before any handler runs: they read the predicates

    mouseButton = convert_to_mouse_button(button, action);

    get_global_gui_scaled_mouse_coord(&coord);

    if (gDeviceOpInProgress > 0) {
        return; // a whole-slot device op is in flight — swallow canvas interaction until it completes
    }

    if (file_browser_active()) {
        // Modal: swallow mouse-down entirely so nothing underneath (module press, scrollbar
        // drag, rubber-band select) starts while it's open — handle_file_browser_click() only
        // needs to see mouse-up, but mouse-down was previously falling straight through to the
        // module-area/topbar/scrollbar handling below, so the down-half of a click on e.g. the
        // browser's Cancel button could also press a module control underneath it.
        if (mouseButton == mouseButtonLeftDown) {
            handle_file_browser_mouse_down(coord);
        } else if (mouseButton == mouseButtonLeftUp) {
            handle_file_browser_click(coord);
        }
        synthlib_request_redraw();
        return;
    }

    if (bank_browser_active()) {
        // Same modal mouse-down/up gating as file_browser_active() above.
        if (mouseButton == mouseButtonLeftDown) {
            handle_bank_browser_mouse_down(coord);
        } else if (mouseButton == mouseButtonLeftUp) {
            handle_bank_browser_click(coord);
        }
        synthlib_request_redraw();
        return;
    }

    if (alert_dialog_active()) {
        // Same modal gating as file_browser_active()/bank_browser_active() above, plus routing
        // around the bank-picker's own dropdown: that flyout is opened (from
        // handle_alert_dialog_click()) using the app's shared context-menu system, so once it's
        // open, clicks must go to handle_context_menu_click() — exactly how the main window's own
        // menu bar already defers to it — rather than being swallowed here as if they'd landed on
        // the dialog panel itself.
        if (mouseButton == mouseButtonLeftDown) {
            if (!gContextMenu.active) {
                handle_alert_dialog_mouse_down(coord);
            }
        } else if (mouseButton == mouseButtonLeftUp) {
            if (gContextMenu.active) {
                if (!handle_context_menu_click(coord)) {
                    gContextMenu.active = false;
                }
            } else {
                handle_alert_dialog_click(coord);
            }
        }
        synthlib_request_redraw();
        return;
    }

    if (handle_patch_notes_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_perf_settings_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_param_pages_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_midi_cc_list_mouse(coord, mouseButton)) {
        return;
    }

    if (handle_param_overview_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }
    // Floating panels are hit-tested FRONT TO BACK, matching the order they are drawn in (see
    // render_panels() in graphics.c, which draws them back to front). Fixed call order was wrong the
    // moment two of them could overlap: whichever was tested first swallowed the press, even when it
    // was the one underneath.
    {
        tFloatingPanelEntry panels[] = {
            {&gVirtualKeyboard.panel, NULL, handle_virtual_keyboard_mouse, NULL},
            {&gPatchAdjuster.panel,   NULL, handle_patch_adjuster_mouse,   NULL},
            {&gHelpPanel.panel,       NULL, handle_help_panel_mouse,       NULL},
            {&gMutator.panel,         NULL, handle_mutator_mouse,          NULL}
        };
        uint32_t            count    = (uint32_t)(sizeof(panels) / sizeof(panels[0]));

        floating_panel_sort(panels, count);

        // Reversed: sorted back-to-front for drawing, so front-to-back is the hit-test order.
        for (uint32_t i = count; i > 0; i--) {
            if (panels[i - 1].mouse(coord, mouseButton)) {
                synthlib_request_redraw();
                return;
            }
        }
    }

    if (handle_patch_params_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_patch_settings_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }
    stop_patch_name_editing();
    stop_module_name_editing();
    stop_param_name_editing();
    stop_perf_name_editing();

    // The split bar owns its own strip, and it sits between the panes rather than inside either, so
    // it gets first refusal before anything tries to interpret the click as a canvas click.
    if (handle_split_bar_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }

    // Focus follows the pane a press lands in, BEFORE the click is interpreted: everything below
    // reads gLocation, and in a split view that has to mean "the half you just clicked in" or a
    // module in the FX pane would be looked up in the Voice Area.
    //
    // A LEFT press while a context menu is open is exempt, and that exemption is why "create module"
    // in the FX area used to create it in the Voice Area. The menu is raised on right-UP at the click
    // position, but clamp_menu_to_screen() (SynthLib's contextMenu.c) slides a frame back UP the
    // window when it would overrun the bottom — and the module menu is tall. Right-click low in the
    // FX pane, which is the LOWER of the two, and most of its items are drawn over the Voice Area.
    // Selecting one is a left press landing in pane 0, which re-pointed gLocation at the Voice Area
    // before menu_action_create() read it. The ROW came out wrong with it: the position does come
    // from gContextMenu.originCoord, the unclamped right-click, but
    // convert_mouse_coord_to_module_column_row() resolves it against whichever pane is current, and
    // the FX pane starts further down the window. Measured, one right-click at the same point: FX
    // col 3 row 2 with this guard, VA col 3 row 14 without it. Every other menu action reading
    // gLocation had the same hole — the cable-chain commands test it against the location the menu
    // was RAISED against (cable_menu_node()) and so silently did nothing at all.
    //
    // Nothing is lost by skipping it: every left-press handler below is already gated on
    // !gContextMenu.active, so while a menu is open the press has no other effect to be focused for.
    // A RIGHT press is NOT exempt — it always begins a fresh menu, replacing any open one on
    // right-up, so it must point focus at the pane it landed in.
    if (  (mouseButton == mouseButtonRightDown)
       || ((mouseButton == mouseButtonLeftDown) && !gContextMenu.active)) {
        split_view_focus_at(coord);
        location = gLocation;
    }

    switch (mouseButton) {
        case mouseButtonLeftDown:
        {
            if (!found) {
                found = handle_menu_bar_click(gAppMenuBar, app_menu_bar_rect(), coord);
            }

            if (!found) {
                found = handle_topbar_left_down(coord, slot);
            }

            if (!found) {
                found = handle_scrollbar_click(coord);
            }

            // Every clickable widget — module params/modes/connectors/body/drag-handle AND the morph
            // group overlay — registers a click region at render time (see moduleGraphics.c). Morph
            // registers at eClickLayerPanel, everything else at eClickLayerCanvas, so dispatch itself
            // (not call order) guarantees morph wins over a scrolled regular module that happens to sit
            // visually underneath it — see moduleGraphics.c's own top-of-file comment.
            //
            // handle_morph_press()/handle_module_press() USED TO FOLLOW THIS AS A FALLBACK, "kept for
            // anything dispatch doesn't match (should be nothing today)". Deleted 2026-08-09, on
            // evidence rather than on that hunch: every rectangle they hit-tested — params, modes,
            // connectors, the body and the drag handle — is registered as a click region by the same
            // render pass that computes it, and instrumenting both of them to log when they claimed a
            // click produced nothing across the canvas widgets and the patch-settings panel. Worse than
            // dead: they tested the STORED rectangles, which for a module scrolled out of view are the
            // stale ones left from wherever it was last drawn, so the one case where they could still
            // have fired is a case where firing would have been wrong (the same fault as the FX-pane
            // hover bug). 318 lines, in git history if ever needed.
            if (!found && !gContextMenu.active) {
                found = dispatch_click_region(coord, eClickPress);
            }

            // Click on empty module-area space: clear selection and start rubber-band. Shared with
            // the plug-in — see canvasDrag.h.
            if (!found && !gContextMenu.active) {
                found = canvas_empty_press(coord, shift_modifier_held());
            }
        }
        break;

        case mouseButtonLeftUp:
        {
            // One event for every gesture release below. Raw coordinates are the canvas ones here: no
            // release phase differences against them — only the incremental MOTION does that.
            tCanvasGestureEvent releaseEvent = {
                .coord    = coord,
                .rawX     = coord.x,
                .rawY     = coord.y,
                .slot     = slot,
                .location = location,
                .altHeld  = false,
                .additive = shift_modifier_held()
            };

            for (i = 0; i < (int)topbarControlMax; i++) {
                gTopbarControls[i].isPressed = false;
            }

            if (  gParamDragging.active
               || gTempoDragging
               || gPerfTempoDragging
               || gVibRateDragging
               || gVibAmountDragging
               || gGlideTimeDragging) {
                found = true;
            }

            if (found == false) {
                if (gContextMenu.active == true) {
                    if (within_rectangle(coord, app_menu_bar_rect())) {
                        // Same click's mouse-down just opened/switched/closed this dropdown via
                        // handle_menu_bar_click() — landing back on the bar itself on mouse-up is
                        // not a dropdown-item selection, so leave the state exactly as mouse-down
                        // left it. Must be checked before handle_context_menu_click(): that call
                        // has the side effect of closing the menu itself whenever coord doesn't
                        // land on any open item, which a bar click never does.
                        found = true;
                    } else if (handle_context_menu_click(coord)) {
                        found = true;
                    } else {
                        gContextMenu.active = false;  // Close if clicked outside - TODO: think if this is the right thing to do here
                    }
                }
            }

            if (!found) {
                found = handle_topbar_left_up(coord, slot);
            }

            if (found == false) {
                // THE RE-ORDER ITSELF IS canvas_module_drag_release() NOW — it was written out again
                // here, in a second copy the plug-in never used and this shell never shared. The undo
                // push below stays application-side (a plug-in has no undo stack) and still works
                // because that function clears only gModuleDrag.active, leaving the drag's snapshot
                // intact for exactly this comparison.
                if (canvas_gesture_release(&releaseEvent, canvasGestureModule) != canvasGestureNone) {
                    // Push move undo: compare snapshot (before) with current (after)
                    tUndoMoveEntry entries[MAX_NUM_MODULES];
                    uint32_t       entryCount = 0;
                    bool           anyMoved   = false;

                    for (uint32_t si = 0; si < gModuleDrag.snapshotCount; si++) {
                        tModule * mod = get_module(gModuleDrag.snapshotKeys[si]);

                        if (!mod) {
                            continue;
                        }
                        entries[entryCount].key       = gModuleDrag.snapshotKeys[si];
                        entries[entryCount].oldColumn = gModuleDrag.snapshotColumn[si];
                        entries[entryCount].oldRow    = gModuleDrag.snapshotRow[si];
                        entries[entryCount].newColumn = mod->column;
                        entries[entryCount].newRow    = mod->row;

                        if (  entries[entryCount].oldColumn != entries[entryCount].newColumn
                           || entries[entryCount].oldRow != entries[entryCount].newRow) {
                            anyMoved = true;
                        }
                        entryCount++;
                    }

                    if (anyMoved && entryCount > 0) {
                        undo_push_move((uint32_t)gSlot, (uint32_t)gLocation,
                                       entries, entryCount);
                    }
                    found = true;
                }
            }

            if (found == false) {
                if (canvas_gesture_release(&releaseEvent, canvasGestureCable) != canvasGestureNone) {
                    found = true;
                }
            }

            // See the matching mouseButtonLeftDown case: dispatch is layer-ordered (morph's
            // eClickLayerPanel beats everything else's eClickLayerCanvas). Its two legacy fallbacks
            // went the same way, and for the same reasons.
            if (!found) {
                found = dispatch_click_region(coord, eClickRelease);
            }

            // AFTER dispatch, exactly as before: a press-captured widget owns its own release, and
            // the rubber band must not pre-empt it. That ordering is why canvas_gesture_release()
            // takes a mask instead of always sweeping all four — see canvasDrag.h.
            if (canvas_gesture_release(&releaseEvent, canvasGestureRubberBand) != canvasGestureNone) {
                found = true;
            }
            finish_param_drag();   // the param gesture's release plus this shell's undo and cursor restore
        }
        break;

        case mouseButtonRightDown:
        {
            // Currently no use for right button down

            stop_dragging();
        }
        break;

        case mouseButtonRightUp:
        {
            found = canvas_right_click(coord, slot, location);

            if (!found) {
                found = handle_module_area_click(coord);
            }

            if (handle_topbar_right_up(coord)) {
                found = true;
            }
            stop_dragging();
        }
        break;

        default:
            break;
    }
    synthlib_request_redraw();
}

static uint32_t calc_tempo_drag_value(double xCoord, double yCoord, double x, double y, tRectangle rotaryRect) {
    int      newVal = 0;
    double   angle  = 0.0;
    uint32_t value  = 0;

    if (synthlib_dial_mode() == eDialModeVertical) {
        newVal     = (int)gGlobalSettings.masterClock + (int)((gDragPrevY - yCoord) * 241.0 / 200.0);
        gDragPrevY = yCoord;

        if (newVal < 30) {
            newVal = 30;
        }

        if (newVal > 240) {
            newVal = 240;
        }
        value      = (uint32_t)newVal;
    } else if (synthlib_dial_mode() == eDialModeHorizontal) {
        newVal     = (int)gGlobalSettings.masterClock + (int)((xCoord - gDragPrevX) * 241.0 / 200.0);
        gDragPrevX = xCoord;

        if (newVal < 30) {
            newVal = 30;
        }

        if (newVal > 240) {
            newVal = 240;
        }
        value      = (uint32_t)newVal;
    } else {
        angle = calculate_mouse_angle((tCoord){x, y}, rotaryRect);
        value = angle_to_value(angle, 211) + 30;
    }
    return value;
}

// ── The named-rect dial drags, as data ──────────────────────────────────────────────────────────
//
// FIVE ARMS OF cursor_pos()'s IF/ELSE CHAIN WERE TWO GESTURES WRITTEN OUT REPEATEDLY: the tempo dial
// and the performance-settings tempo dial, identical but for which rectangle they sit in; and the
// vibrato amount, vibrato rate and glide time dials, identical but for a module, a parameter index
// and a range. About 100 lines in which the only things that varied were the four values now in the
// tables below. That is the whole of what vst3/plugin-gui-notes.md's second observation asks for —
// the chain reads as a list wanting to be a table — applied to the part of it that is genuinely
// repetition rather than genuinely different work.
//
// Each entry points AT its rectangle rather than copying it: these rectangles are filled in at
// render time, so a copy taken here would be a stale one from start-up. The addresses are constant
// because the rectangles are globals, which is what lets the tables be static.
//
// The remaining arms of the chain are NOT candidates for this. A scrollbar drag, a module drag, a
// cable drag and a rubber band each do genuinely different work; collapsing those would need a
// gesture object, which is observation 1 in that file and a much larger change.

typedef struct {
    bool *       active;       // which gXxxDragging flag arms this drag
    tRectangle * rotaryRect;   // dial position, for rotary mode's absolute angle
} tTempoDragTarget;

static const tTempoDragTarget      sTempoDragTargets[]      = {
    {&gTempoDragging,     &gTopbarControls[topbarTempoDialId].rectangle},
    {&gPerfTempoDragging, &gPerfSettingsPanelRects.masterClock         },
};

typedef struct {
    bool *       active;
    uint32_t     moduleIndex;  // the patchModule* inside the Morph location
    uint32_t     param;
    uint32_t     range;        // number of values the dial has; the clamp is 0 .. range - 1
    tRectangle * rotaryRect;
} tPatchParamDragTarget;

static const tPatchParamDragTarget sPatchParamDragTargets[] = {
    {&gVibAmountDragging, patchModuleVibrato, VIBRATO_DEPTH, 101, &gPatchParamRects[pPVibratoAmount]},
    {&gVibRateDragging,   patchModuleVibrato, VIBRATO_RATE,  128, &gPatchParamRects[pPVibratoRate]  },
    {&gGlideTimeDragging, patchModuleGlide,   GLIDE_SPEED,   128, &gPatchParamRects[pPGlideTime]    },
};

// Returns true if one of these drags was active and consumed the motion, so the caller's chain can
// carry on to the next gesture exactly as the separate arms did.
static bool handle_tempo_drag_motion(double xCoord, double yCoord, double x, double y) {
    for (size_t i = 0; i < (sizeof(sTempoDragTargets) / sizeof(sTempoDragTargets[0])); i++) {
        if (*sTempoDragTargets[i].active == false) {
            continue;
        }
        uint32_t value = calc_tempo_drag_value(xCoord, yCoord, x, y, *sTempoDragTargets[i].rotaryRect);

        if (gGlobalSettings.masterClock != value) {
            gGlobalSettings.masterClock = (uint8_t)value;
            send_master_clock_bpm(value);
        }
        return true;
    }

    return false;
}

static bool handle_patch_param_drag_motion(uint32_t slot, double xCoord, double yCoord, double x, double y) {
    for (size_t i = 0; i < (sizeof(sPatchParamDragTargets) / sizeof(sPatchParamDragTargets[0])); i++) {
        const tPatchParamDragTarget * target = &sPatchParamDragTargets[i];

        if (*target->active == false) {
            continue;
        }
        // The KEY names gPatchParamsEdit.slot while the message is addressed to the slot the caller
        // passes, which is gSlot. Both of the arms this replaces did exactly that, so it is preserved
        // rather than tidied — the two are kept in step whenever the slot changes (see the patch
        // screen's own slot handling), and making them agree here would hide it rather than settle it.
        tModuleKey                    key    = {(uint32_t)gPatchParamsEdit.slot, (uint32_t)locationMorph, target->moduleIndex};
        tModule *                     module = get_module(key);

        if (module == NULL) {
            return true;   // armed on a module that is not there: consumed, as the original's NULL check was
        }
        uint8_t *                     param  = &module->param[0][target->param].value;
        int                           newVal = (int)*param;

        if (synthlib_dial_mode() == eDialModeHorizontal) {
            newVal    += (int)((xCoord - gDragPrevX) * (double)target->range / 200.0);
            gDragPrevX = xCoord;
        } else if (synthlib_dial_mode() == eDialModeRotary) {
            // Absolute angle, so no previous position to update — the incremental modes own gDragPrev*.
            newVal = (int)angle_to_value(calculate_mouse_angle((tCoord){x, y}, *target->rotaryRect), target->range);
        } else {
            newVal    += (int)((gDragPrevY - yCoord) * (double)target->range / 200.0);
            gDragPrevY = yCoord;
        }

        if (newVal < 0) {
            newVal = 0;
        }

        if (newVal > (int)(target->range - 1)) {
            newVal = (int)(target->range - 1);
        }

        if (*param != (uint8_t)newVal) {
            *param = (uint8_t)newVal;
            send_param_value(slot, key, target->param, 0, (uint32_t)newVal);
        }
        return true;
    }

    return false;
}

void cursor_pos(GLFWwindow * window, double xCoord, double yCoord) {
    // Several locals went with the parameter-drag arm when it moved to canvasDrag.c; what is left is
    // what the remaining arms actually use.
    tCoord coord = {0};
    double x     = 0;
    double y     = 0;


    get_global_gui_scaled_mouse_coord(&coord);
    x                      = coord.x;
    y                      = coord.y;

    // Scale x and y to match intended rendering window
    //glfwGetWindowSize(window, &width, &height);
    //x = (x * (double)get_render_width()) / (double)width;
    //y = (y * (double)get_render_height()) / (double)height;

    gHoverConnector.active = false;

    if (gSplitView.dragging) {
        handle_split_bar_cursor_pos(coord);
        return;
    }

    if (pane_scrollbar_dragging()) {
        handle_pane_scrollbar_drag(coord);
        return;
    }

    if (gPatchAdjuster.active && (gPatchAdjuster.dragKnob >= 0)) {
        handle_patch_adjuster_cursor_pos(coord);
        synthlib_request_redraw();
        return;
    }

    if (gMutator.active && (gMutator.panel.dragging || (gMutator.draggingSlider >= 0))) {
        handle_mutator_cursor_pos(coord);
        synthlib_request_redraw();
        return;
    }

    // Floating panels being moved (floatingPanel.h). Ahead of the canvas gestures below for the same
    // reason the Mutator is: a panel drag owns the pointer until it is released, and the panel is
    // over the canvas, so letting the canvas also act on the motion would rubber-band underneath it.
    // Every floating panel has to be listed here or its title bar drags nothing — the Help panel was
    // added to the draw and hit-test registries but missed off this one, so it could be raised and
    // closed but never moved. The Mutator is absent deliberately: handle_mutator_cursor_pos() above
    // already runs its move, because it has sliders to drag as well.
    if (  floating_panel_drag(&gVirtualKeyboard.panel, coord)
       || floating_panel_drag(&gPatchAdjuster.panel, coord)
       || floating_panel_drag(&gHelpPanel.panel, coord)) {
        return;
    }

    // Mouse is just hovering over the (non-dragging) Mutator panel - skip all the hover detection
    // below (cable highlight, knob/CC overlays, etc.) so nothing hidden behind the floater lights
    // up. gHoverConnector.active is already false from just above.
    if (gMutator.active && within_rectangle(coord, gMutator.panel.rect)) {
        synthlib_request_redraw();
        return;
    }

    // Bank/file browser scrollbar-thumb drag - both no-op and return false unless a drag started
    // on that popup's own scrollbar (list_scrollbar_mouse_down(), called from their respective
    // mouse-down handlers) is actually in progress.
    if (handle_bank_browser_mouse_move(coord) || handle_file_browser_mouse_move(coord)) {
        return;
    }

    if (gDragSkipCount > 0) {
        gDragPrevX = xCoord;
        gDragPrevY = yCoord;
        gDragSkipCount--;
        synthlib_request_redraw();
        return;
    }

    if (gScrollState.yBarDragging == true) {
        set_y_scroll_bar(y - gScrollState.yGrabOffset);
    } else if (gScrollState.xBarDragging == true) {
        set_x_scroll_bar(x - gScrollState.xGrabOffset);
        //} else if (gVoiceDialDragging == true) {  // Use this for patch level...?
        //
        //angle                        = calculate_mouse_angle((tCoord){x, y}, gVoiceDialRect);
        //value                        = angle_to_value(angle, 31);
        //gPatchDescr[slot].voiceCount = value + 1; // Note G2 won't let me set less than a value of 1, can't set to zero for zero based
        //messageContent.cmd           = eMsgCmdWritePatchDescr;
        // messageContent.slot          = slot;
        // msg_send(&gToUsbThread, &messageContent);
    } else if (handle_tempo_drag_motion(xCoord, yCoord, x, y)) {
        // The tempo dial and the performance-settings tempo dial — see sTempoDragTargets.
    } else if (handle_patch_param_drag_motion(gSlot, xCoord, yCoord, x, y)) {
        // Vibrato amount, vibrato rate and glide time — see sPatchParamDragTargets.
    } else if (canvas_gesture_motion(&(tCanvasGestureEvent){
        .coord = coord, .rawX = xCoord, .rawY = yCoord,
        .slot = gSlot, .location = gLocation,
        .altHeld = alt_modifier_held(), .additive = shift_modifier_held()
    }) != canvasGestureNone) {
        // All four canvas gestures — dial, module, cable, rubber band — through the one table in
        // canvasDrag.c. These were four hand-maintained arms here and a differently-ordered ladder in
        // the plug-in; see canvasDrag.h for what that cost.
        //
        // THE AUTO-SCROLL STAYS HERE, and only for the gestures that travel: it belongs to whoever owns
        // the scrollbars, which a plug-in canvas does not. A dial drag is not going anywhere, so it is
        // the one gesture left out.
        //
        // THE RUBBER BAND TRAVELS TOO. It was excluded on the grounds that it "selects what is already
        // visible", which is only true of a selection that fits on screen — drag towards the edge to
        // gather a whole column of modules and the band simply stopped at the boundary, with no way to
        // reach the rest. Including it is safe because the band's anchor is stored in MODULE-AREA
        // coordinates (convert_mouse_coord_to_module_area_coord adds the scroll offset and divides out
        // the zoom), so it is absolute canvas space: scrolling moves the view underneath a fixed anchor
        // rather than dragging the anchor along with it. Had the anchor been in window coordinates this
        // would have skewed the rectangle instead, which is presumably why it was avoided.
        if ((gModuleDrag.active == true) || (gCableDrag.active == true) || (gRubberBand.active == true)) {
            adjust_scroll_for_drag();
        }
    } else if (gContextMenu.active == true) {
        // Dummy
    } else {
        // Bounds-checks the coordinate itself now — see canvasDrag.c.
        canvas_hover_update(coord);
    }
    // Limit re-draw/render if nothing's happened
    // if (noAction == false) {   // Used to have this check, TODO - see if there's a way to not redraw on every move
    synthlib_request_redraw();
    // }
}

void scroll_event(GLFWwindow * window, double x, double y) {
    tCoord  coord   = {0};

    if (file_browser_active()) {
        handle_file_browser_scroll(y);
        return;
    }

    if (bank_browser_active()) {
        handle_bank_browser_scroll(y);
        return;
    }
    // The wheel acts on the pane UNDER THE CURSOR, not the focused one — hovering the FX half and
    // scrolling should move the FX half, without first having to click into it.
    get_global_gui_scaled_mouse_coord(&coord);
    int32_t hovered = split_view_pane_at(coord);

    if (hovered < 0) {
        hovered = (int32_t)split_view_focused_pane();
    }

    // ALT + WHEEL over a parameter adjusts it, rather than scrolling the pane under it — the wheel
    // equivalent of the bare +/- keys, and it goes through the same canvas_nudge_param_under_cursor()
    // so the two cannot drift on what counts as a step or which widgets refuse one.
    //
    // The accumulator is not decoration. A mouse wheel delivers whole notches, but a trackpad
    // delivers a stream of fractions, and stepping one unit per EVENT would make a trackpad race
    // through the range while truncating each fraction to nothing would make it do nothing at all.
    // Same reasoning as the sub-unit remainder carried between drag events — see tParamDragging.
    // A delta of 0 is a pure QUERY — nudge_param_for_module() reports whether the cursor is over a
    // steppable parameter and only sends when the value actually changes. Asking first is what lets
    // Alt + wheel away from any parameter still scroll the pane normally, instead of being swallowed.
    if (alt_modifier_held() && canvas_nudge_param_under_cursor(0)) {
        static double wheelAccum = 0.0;

        wheelAccum += y;

        int           steps      = (int)wheelAccum;

        if (steps != 0) {
            wheelAccum -= (double)steps;
            canvas_nudge_param_under_cursor(steps);
            synthlib_request_redraw();
        }
        return;   // while Alt is held over a parameter, the wheel belongs to it
    }

    if (gCommandKeyPressed == true) {
        uint32_t prevPane = module_pane();

        set_module_pane((uint32_t)hovered);
        canvas_zoom_step_at(y * ZOOM_DELTA, coord);
        set_module_pane(prevPane);
    } else {
        // Content pixels per notch, relative to THAT PANE's own position — see pane_scroll_by().
        pane_scroll_by((uint32_t)hovered, -x * WHEEL_SCROLL_STEP, -y * WHEEL_SCROLL_STEP);
    }
//    LOG_DEBUG("Area: %f %f - size: %i %i - barY %f %f %f \n", moduleArea.size.w,moduleArea.size.h, width,height, gScrollState.yBar, gScrollState.yRectangle.size.h,gScrollState.yRectangle.coord.y);

    synthlib_request_redraw();
}

void char_event(GLFWwindow * window, unsigned int value) {
    if (file_browser_active()) {
        handle_file_browser_char(value);
        return;
    }

    if (gPatchNameEdit.active) {
        size_t   len       = strlen(gPatchNameEdit.buffer);
        uint32_t cursorPos = gPatchNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gPatchNameEdit.buffer[cursorPos + 1],
                    &gPatchNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gPatchNameEdit.buffer[cursorPos] = (char)value;
            gPatchNameEdit.cursorPos++;
        }
    }

    if (gPatchNotesEdit.active) {
        size_t   len       = strlen(gPatchNotesEdit.buffer);
        uint32_t cursorPos = gPatchNotesEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < PATCH_NOTES_SIZE)) {
            memmove(&gPatchNotesEdit.buffer[cursorPos + 1],
                    &gPatchNotesEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gPatchNotesEdit.buffer[cursorPos] = (char)value;
            gPatchNotesEdit.cursorPos++;
        }
    }

    if (gModuleNameEdit.active) {
        size_t   len       = strlen(gModuleNameEdit.buffer);
        uint32_t cursorPos = gModuleNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gModuleNameEdit.buffer[cursorPos + 1],
                    &gModuleNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gModuleNameEdit.buffer[cursorPos] = (char)value;
            gModuleNameEdit.cursorPos++;
        }
    }

    if (gParamNameEdit.active) {
        size_t   len       = strlen(gParamNameEdit.buffer);
        uint32_t cursorPos = gParamNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < PROTOCOL_PARAM_NAME_SIZE)) {
            memmove(&gParamNameEdit.buffer[cursorPos + 1],
                    &gParamNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gParamNameEdit.buffer[cursorPos] = (char)value;
            gParamNameEdit.cursorPos++;
        }
    }

    if (gSynthNameEdit.active) {
        size_t   len       = strlen(gSynthNameEdit.buffer);
        uint32_t cursorPos = gSynthNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gSynthNameEdit.buffer[cursorPos + 1],
                    &gSynthNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gSynthNameEdit.buffer[cursorPos] = (char)value;
            gSynthNameEdit.cursorPos++;
        }
    }

    if (gPerfNameEdit.active) {
        size_t   len       = strlen(gPerfNameEdit.buffer);
        uint32_t cursorPos = gPerfNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gPerfNameEdit.buffer[cursorPos + 1],
                    &gPerfNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gPerfNameEdit.buffer[cursorPos] = (char)value;
            gPerfNameEdit.cursorPos++;
        }
    }
    LOG_DEBUG("char=%d\n", value);
    synthlib_request_redraw();
}

void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
    set_modifier_state_from_glfw(mods);   // a modifier PRESS is a key event like any other

    LOG_DEBUG("key=%d scancode=%d action=%d mods=%d\n", key, scancode, action, mods);

    if ((key == GLFW_KEY_L) && (action == GLFW_PRESS)) {
        LOG_INFO("L key: mods=0x%x cmdFlag=%d | fileBrowser=%d bankBrowser=%d alert=%d menu=%d "
                 "notesEdit=%d perfNameEdit=%d\n",
                 mods, (int)gCommandKeyPressed, (int)file_browser_active(), (int)bank_browser_active(),
                 (int)alert_dialog_active(), (int)gContextMenu.active,
                 (int)gPatchNotesEdit.active, (int)gPerfNameEdit.active);
    }

    if (file_browser_active()) {
        handle_file_browser_key(key, action);
        synthlib_request_redraw();
        return;
    }

    if (bank_browser_active()) {
        handle_bank_browser_key(key, action);
        synthlib_request_redraw();
        return;
    }

    if (alert_dialog_active()) {
        // Escape closes just the bank-picker dropdown first, if it's open, rather than the whole
        // dialog underneath it — same precedence the main window's own menu-vs-Escape handling uses.
        if (gContextMenu.active && (key == GLFW_KEY_ESCAPE) && (action == GLFW_PRESS)) {
            gContextMenu.active = false;
        } else {
            handle_alert_dialog_key(key, action);
        }
        synthlib_request_redraw();
        return;
    }

    if (handle_param_pages_key(key, mods, action)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_midi_cc_list_key(key, mods, action)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_param_overview_key(key, mods, action)) {
        synthlib_request_redraw();
        return;
    }
    // Floating-panel keys, FRONT TO BACK for the same reason their clicks are: Escape has to close
    // the panel you are looking at. Fixed call order closed whichever handler happened to be first —
    // with the Help panel in front and the Virtual Keyboard behind it, Escape shut the keyboard.
    {
        tFloatingPanelEntry panels[] = {
            {&gVirtualKeyboard.panel, NULL, NULL, handle_virtual_keyboard_key},
            {&gPatchAdjuster.panel,   NULL, NULL, handle_patch_adjuster_key  },
            {&gHelpPanel.panel,       NULL, NULL, handle_help_panel_key      },
            {&gMutator.panel,         NULL, NULL, handle_mutator_key         }
        };
        uint32_t            count    = (uint32_t)(sizeof(panels) / sizeof(panels[0]));

        floating_panel_sort(panels, count);

        for (uint32_t i = count; i > 0; i--) {
            if (panels[i - 1].key(key, mods, action)) {
                synthlib_request_redraw();
                return;
            }
        }
    }

    // NOTE ENTRY, deliberately global — it does not need the Virtual Keyboard panel open, because the
    // panel is a view of that state rather than a precondition for it.
    //
    // The text-edit guard is EXPLICIT rather than positional. key_event()'s own name-editing block is
    // further down this function (the char_event() handlers near the top of the file are a different
    // path), so relying on "everything above has returned" would have let every letter typed into a
    // patch name play a note as well as being inserted. Stating the condition also means a future
    // edit field cannot quietly reintroduce it by being handled somewhere new.
    if (!any_text_edit_active() && handle_note_entry_key(key, mods, action)) {
        return;
    }

    if (gPatchNotesEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gPatchNotesEdit.buffer);
            uint32_t cursorPos = gPatchNotesEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gPatchNotesEdit.buffer[cursorPos - 1],
                            &gPatchNotesEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPatchNotesEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gPatchNotesEdit.buffer[cursorPos],
                            &gPatchNotesEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (  (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
                      && (mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER))) {
                uint32_t        newSize = (uint32_t)len;

                if (newSize > PATCH_NOTES_SIZE) {
                    newSize = PATCH_NOTES_SIZE;
                }
                memcpy(gPatchNotes[gPatchNotesEdit.slot], gPatchNotesEdit.buffer, newSize);
                gPatchNotes[gPatchNotesEdit.slot][newSize] = '\0';
                gPatchNotesSize[gPatchNotesEdit.slot]      = newSize;
                gPatchNotesEdit.active                     = false;
                tMessageContent msg     = {0};
                msg.cmd                                    = eMsgCmdWritePatch;
                msg.slot                                   = gPatchNotesEdit.slot;
                msg_send(&gToUsbThread, &msg);
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                if (len < PATCH_NOTES_SIZE) {
                    memmove(&gPatchNotesEdit.buffer[cursorPos + 1],
                            &gPatchNotesEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPatchNotesEdit.buffer[cursorPos] = '\r';
                    gPatchNotesEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gPatchNotesEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gPatchNotesEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_UP) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_move_line((int)cursorPos, -1);
            } else if (key == GLFW_KEY_DOWN) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_move_line((int)cursorPos, 1);
            } else if (key == GLFW_KEY_HOME) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_line_home((int)cursorPos);
            } else if (key == GLFW_KEY_END) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_line_end((int)cursorPos);
            } else if (key == GLFW_KEY_ESCAPE) {
                gPatchNotesEdit.active = false;
            }
        }
        synthlib_request_redraw();
        return;
    } else if (gPatchNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gPatchNameEdit.buffer);
            uint32_t cursorPos = gPatchNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gPatchNameEdit.buffer[cursorPos - 1],
                            &gPatchNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPatchNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gPatchNameEdit.buffer[cursorPos],
                            &gPatchNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gPatchNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gPatchNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gPatchNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gPatchNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                // Commit
                gPatchNameEdit.active = false;
                uint32_t        pnSlot         = gPatchNameEdit.slot;
                char            oldPatchName[CLAVIA_NAME_SIZE + 1];
                COPY_STRING(oldPatchName, gGlobalSettings.slot[pnSlot].patchName);
                COPY_STRING(gGlobalSettings.slot[pnSlot].patchName, gPatchNameEdit.buffer);
                tMessageContent messageContent = {0};
                messageContent.cmd    = eMsgCmdSetPatchName;
                messageContent.slot   = pnSlot;
                COPY_STRING(messageContent.patchName.name, gGlobalSettings.slot[pnSlot].patchName);
                msg_send(&gToUsbThread, &messageContent);
                undo_push_patch_name(pnSlot, oldPatchName, gPatchNameEdit.buffer);
            } else if (key == GLFW_KEY_ESCAPE) {
                // Cancel — discard edits
                gPatchNameEdit.active = false;
            }
        }
    } else if (gModuleNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gModuleNameEdit.buffer);
            uint32_t cursorPos = gModuleNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gModuleNameEdit.buffer[cursorPos - 1],
                            &gModuleNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gModuleNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gModuleNameEdit.buffer[cursorPos],
                            &gModuleNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gModuleNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gModuleNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gModuleNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gModuleNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gModuleNameEdit.active = false;

                tModule * module = get_module(gModuleNameEdit.moduleKey);

                if (module != NULL) {
                    tMessageContent msg = {0};
                    char            oldName[CLAVIA_NAME_SIZE + 1];
                    COPY_STRING(oldName, module->name);
                    COPY_STRING(module->name, gModuleNameEdit.buffer);
                    msg.cmd                       = eMsgCmdSetModuleLabel;
                    msg.slot                      = gModuleNameEdit.moduleKey.slot;
                    msg.moduleLabelData.moduleKey = gModuleNameEdit.moduleKey;
                    COPY_STRING(msg.moduleLabelData.name, gModuleNameEdit.buffer);
                    msg_send(&gToUsbThread, &msg);
                    undo_push_module_name(gModuleNameEdit.moduleKey, oldName, gModuleNameEdit.buffer);
                }
            } else if (key == GLFW_KEY_ESCAPE) {
                gModuleNameEdit.active = false;  // discard
            }
        }
    } else if (gParamNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gParamNameEdit.buffer);
            uint32_t cursorPos = gParamNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gParamNameEdit.buffer[cursorPos - 1],
                            &gParamNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gParamNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gParamNameEdit.buffer[cursorPos],
                            &gParamNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gParamNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gParamNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gParamNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gParamNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gParamNameEdit.active = false;

                tModule * module = get_module(gParamNameEdit.moduleKey);

                if (module != NULL) {
                    tMessageContent msg    = {0};
                    uint32_t        pi     = gParamNameEdit.paramIndex;
                    bool            oldSet = module->paramNameSet[pi][0];
                    char            oldName[PROTOCOL_PARAM_NAME_SIZE + 1];
                    COPY_STRING(oldName, module->paramName[pi][0]);

                    module->paramNameSet[pi][0]   = true;
                    COPY_STRING(module->paramName[pi][0], gParamNameEdit.buffer);
                    module->paramNumLabels[pi]    = 1;

                    msg.cmd                       = eMsgCmdSetParamLabel;
                    msg.slot                      = gParamNameEdit.moduleKey.slot;
                    msg.paramLabelData.moduleKey  = gParamNameEdit.moduleKey;
                    msg.paramLabelData.paramIndex = pi;
                    COPY_STRING(msg.paramLabelData.name, gParamNameEdit.buffer);
                    msg_send(&gToUsbThread, &msg);
                    undo_push_param_name(gParamNameEdit.moduleKey, pi,
                                         oldName, oldSet,
                                         gParamNameEdit.buffer, true);
                }
            } else if (key == GLFW_KEY_ESCAPE) {
                gParamNameEdit.active = false;  // discard
            }
        }
    } else if (gSynthNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gSynthNameEdit.buffer);
            uint32_t cursorPos = gSynthNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gSynthNameEdit.buffer[cursorPos - 1],
                            &gSynthNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gSynthNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gSynthNameEdit.buffer[cursorPos],
                            &gSynthNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gSynthNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gSynthNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gSynthNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gSynthNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gSynthNameEdit.active = false;
                COPY_STRING(gSynthSettings.name, gSynthNameEdit.buffer);
                send_synth_settings_msg();
            } else if (key == GLFW_KEY_ESCAPE) {
                gSynthNameEdit.active = false;
            }
        }
    } else if (gPerfNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gPerfNameEdit.buffer);
            uint32_t cursorPos = gPerfNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gPerfNameEdit.buffer[cursorPos - 1],
                            &gPerfNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPerfNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gPerfNameEdit.buffer[cursorPos],
                            &gPerfNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gPerfNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gPerfNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gPerfNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gPerfNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gPerfNameEdit.active = false;
                char            oldPerfName[CLAVIA_NAME_SIZE + 1];
                COPY_STRING(oldPerfName, gGlobalSettings.perfName);
                COPY_STRING(gGlobalSettings.perfName, gPerfNameEdit.buffer);
                tMessageContent messageContent = {0};
                messageContent.cmd   = eMsgCmdWritePerfName;
                msg_send(&gToUsbThread, &messageContent);
                undo_push_perf_name(oldPerfName, gPerfNameEdit.buffer);
            } else if (key == GLFW_KEY_ESCAPE) {
                gPerfNameEdit.active = false;
            }
        }
    } else if (gContextMenu.active && key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        close_context_menu();
        synthlib_request_redraw();
#ifdef ENABLE_MOUSE_CROSSHAIR
    } else if (key == GLFW_KEY_F9 && action == GLFW_PRESS) {
        toggle_mouse_crosshair(); // TEMPORARY debug aid — Debug builds only
        synthlib_request_redraw();
#endif
    } else if ((key == GLFW_KEY_DELETE || key == GLFW_KEY_BACKSPACE) && action == GLFW_PRESS) {
        if (gSelection.count > 0) {
            undo_push_delete_selection();
            delete_selection();
            update_module_up_rates();
            synthlib_request_redraw();
        }
    } else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    } else if (key == GLFW_KEY_LEFT_SUPER && action == GLFW_PRESS) {
        gCommandKeyPressed = true;
    } else if (key == GLFW_KEY_LEFT_SUPER && action == GLFW_RELEASE) {
        gCommandKeyPressed = false;
    } else if (  (action == GLFW_PRESS) && (key == GLFW_KEY_L)
              && ((mods & (GLFW_MOD_SUPER | GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == 0)) {
        // MIDI Learn, and the original editor's only BARE-key shortcut — everything else here is
        // Command-modified. Deliberately silent when it cannot act: it is a fast alternative to the
        // right-click assign menu, and a dialog every time a stray L is typed would defeat that.
        LOG_INFO("L pressed - MIDI Learn\n");
        midi_learn_focused_param();
    } else if (  (  (key == GLFW_KEY_EQUAL) || (key == GLFW_KEY_KP_ADD)
                 || (key == GLFW_KEY_MINUS) || (key == GLFW_KEY_KP_SUBTRACT))
              && ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
              && (gCommandKeyPressed == false)
              && ((mods & (GLFW_MOD_SUPER | GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == 0)) {
        // Bare +/- step the hovered parameter up/down by one raw unit. GLFW reports the physical
        // key, so '+' arrives as GLFW_KEY_EQUAL with Shift held - which is why Shift is deliberately
        // NOT in the modifier guard below, and why the keypad's own pair is accepted too.
        //
        // The other two guards are both needed: mods covers the real keyboard, gCommandKeyPressed is
        // the flag the Cmd branch below runs on, and Cmd -/+ (canvas zoom) must keep reaching it.
        // GLFW_REPEAT is honoured so holding a key walks the range instead of one press per unit.
        bool increment = (key == GLFW_KEY_EQUAL) || (key == GLFW_KEY_KP_ADD);

        if (canvas_nudge_param_under_cursor(increment ? 1 : -1)) {
            synthlib_request_redraw();
        }
    } else if (action == GLFW_PRESS && gCommandKeyPressed == true) {
        // Cmd +/- is the canvas zoom, through canvas_zoom_step() so this shell and the plug-in cannot
        // drift on the step size, the anchor or whether the choice is remembered.
        if (key == GLFW_KEY_MINUS) {
            LOG_DEBUG("ZOOM OUT\n");
            canvas_zoom_step(-ZOOM_DELTA);
        }

        if (key == GLFW_KEY_EQUAL) {
            LOG_DEBUG("ZOOM IN\n");
            canvas_zoom_step(ZOOM_DELTA);
        }

        if (key == GLFW_KEY_C) {
            copy_selection();
        }

        if (key == GLFW_KEY_X) {
            cut_selection();
        }

        if (key == GLFW_KEY_V) {
            paste_clipboard();
        }

        // Select All joins the Cut/Copy/Paste/Undo shortcuts rather than being menu-only: those
        // four already carry theirs, and it is an edit command rather than one of the panels the
        // owner has kept shortcut-free.
        if (key == GLFW_KEY_A) {
            selection_select_all();
        }

        if (key == GLFW_KEY_Z) {
            if (mods & GLFW_MOD_SHIFT) {
                undo_redo();
            } else {
                undo_undo();
            }
        }

        if (key == GLFW_KEY_2) {
            if (gMutator.active) {
                close_mutator_panel();
            } else {
                open_mutator_panel(gSlot);
            }
        }

        // File/Settings menu shortcuts — same keys as the old Cocoa menu's keyEquivalents
        // ("o"/"s"/"n"/","), now dispatched straight to the plain-C action functions the
        // in-window menu bar (src/appMenuBar.c) also calls.
        if (key == GLFW_KEY_O) {
            file_menu_open_patch();
        }

        if (key == GLFW_KEY_S) {
            file_menu_save_patch();
        }

        if (key == GLFW_KEY_N) {
            file_menu_new_patch();
        }

        if (key == GLFW_KEY_COMMA) {
            settings_menu_open_synth();
        }
    }
    synthlib_request_redraw();
}

#ifdef __cplusplus
}
#endif
