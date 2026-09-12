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
// Notes: Docs/code-notes/mouseHandle.c.md - "// notes §k" refers there.

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
#include "palette.h"
#include "utilsGraphics.h"
#include "synthlibPopups.h"
#include "synthlibWindow.h"
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

// The transform itself is SynthLib's now (synthlibWindow.h) — it was written out in all three apps,
// and this copy was the one that had lost the divide-by-zero guard. The NAME stays because it is
// what gets injected into synthlib_host_init() and what nine files here call.
void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    synthlib_mouse_coord(coord);
}

static void send_master_clock_bpm(uint32_t bpm) {
    tMessageContent messageContent = {0};

    messageContent.cmd                    = eMsgCmdSetMasterClockBPM;
    messageContent.masterClockBPMData.bpm = bpm;
    msg_send(&gToUsbThread, &messageContent);
}

// notes §1

// Registered with GLFW so a modifier released while another application has the keyboard cannot
// leave one stuck on here — see set_modifier_state()'s note. There is nothing to restore on the way
// back in: the next key or button event carries the truth with it.
void window_focus_callback(bool focused) {
    if (!focused) {
        set_modifier_state((uint32_t)eModifierNone);
    }
}

// notes §2
static bool sCursorHidden = false;

// notes §3

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
// The NAME fields only — every one of them entered deliberately, by clicking the field itself.
static bool any_name_edit_active(void) {
    return gPatchNameEdit.active
           || gModuleNameEdit.active
           || gParamNameEdit.active
           || gSynthNameEdit.active
           || gPerfNameEdit.active;
}

static bool any_text_edit_active(void) {
    return any_name_edit_active() || gPatchNotesEdit.active;
}

// notes §4
static bool notes_own_keyboard(void) {
    return gPatchNotesEdit.active
           && !any_name_edit_active()
           && floating_panel_is_frontmost(&gPatchNotesEdit.panel);
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

// notes §5

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

// notes §6
void recover_lost_cursor(void) {
    // notes §7
    bool staleGesture = (gRubberBand.active == true)
                        || (gModuleDrag.active == true)
                        || (gCableDrag.active == true);

    if (((sCursorHidden == true) || (staleGesture == true)) && (platform_any_mouse_button_down() == false)) {
        // notes §8
        tCoord at = {0};

        get_global_gui_scaled_mouse_coord(&at);
        mouse_button(at, mouseButtonLeftUp, 0);
    }

    if (is_cursor_hidden_dragging() == false) {
        cursor_release();
    }
}

bool is_cursor_hidden_dragging(void) {
    return gParamDragging.active || gTempoDragging || gPerfTempoDragging || gVibRateDragging || gVibAmountDragging || gGlideTimeDragging;
}

// notes §9
void finish_param_drag(void) {
    // notes §10
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

    // notes §11
    cursor_release();
}

tMouseButton convert_to_mouse_button(int button, int action) {
    return synthlib_mouse_button(button, action);   // pure decode, shared — see synthlibWindow.h
}

// The coordinate arrives already scaled and the button already decoded, and the modifier state has
// been updated before this runs — SynthLib's shim does all three, for all three editors. See
// tSynthLibInputHandlers in synthlibWindow.h.
void mouse_button(tCoord coord, tMouseButton mouseButton, int mods) {
    (void)mods;
    bool     found    = false;
    int32_t  i        = 0;
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    if (gDeviceOpInProgress > 0) {
        return; // a whole-slot device op is in flight — swallow canvas interaction until it completes
    }

    // notes §12
    if (  ((mouseButton == mouseButtonLeftDown) || (mouseButton == mouseButtonRightDown))
       && !within_rectangle(coord, gSettingsPanelRects.synthName)) {
        stop_synth_name_editing();
    }

    // notes §13
    if (synthlib_popups_dispatch_click(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }
    // notes §14
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

    // notes §15
    if (  (mouseButton == mouseButtonRightDown)
       || ((mouseButton == mouseButtonLeftDown) && !gContextMenu.active)) {
        split_view_focus_at(coord);
        location = gLocation;
    }

    switch (mouseButton) {
        case mouseButtonLeftDown:
        {
            // notes §16
            if (!found) {
                found = handle_topbar_left_down(coord, slot);
            }

            // The palette band sits directly under the topbar, above everything on the canvas, so
            // it is tested here rather than with the panels: a press on a tile must not also reach
            // the canvas underneath and start a rubber-band selection.
            if (!found) {
                found = palette_left_down(coord);
            }

            if (!found) {
                found = handle_scrollbar_click(coord);
            }

            // notes §17
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
                        // notes §18
                        found = true;
                    } else if (handle_context_menu_click(coord)) {
                        found = true;
                    } else {
                        gContextMenu.active = false;  // Close if clicked outside - TODO: think if this is the right thing to do here
                    }
                }
            }

            // notes §19
            if (!found) {
                found = palette_left_up(coord);
            }

            if (!found) {
                found = handle_topbar_left_up(coord, slot);
            }

            if (found == false) {
                // notes §20
                if (canvas_gesture_release(&releaseEvent, canvasGestureModule) != canvasGestureNone) {
                    // notes §21
                    tUndoMoveEntry entries[MAX_NUM_MODULES];
                    uint32_t       entryCount = 0;

                    for (uint32_t si = 0; si < gModuleDrag.snapshotCount; si++) {
                        tModule * mod = get_module(gModuleDrag.snapshotKeys[si]);

                        if (!mod) {
                            continue;
                        }

                        if (  (mod->column == gModuleDrag.snapshotColumn[si])
                           && (mod->row == gModuleDrag.snapshotRow[si])) {
                            continue;
                        }
                        entries[entryCount].key       = gModuleDrag.snapshotKeys[si];
                        entries[entryCount].oldColumn = gModuleDrag.snapshotColumn[si];
                        entries[entryCount].oldRow    = gModuleDrag.snapshotRow[si];
                        entries[entryCount].newColumn = mod->column;
                        entries[entryCount].newRow    = mod->row;
                        entryCount++;
                    }

                    if (entryCount > 0) {
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

// notes §22

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
        // notes §23
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

// notes §24
void cursor_pos(tCoord coord) {
    double x      = coord.x;
    double y      = coord.y;

    // notes §25
    double xCoord = 0.0;
    double yCoord = 0.0;

    cursor_raw_coord(&xCoord, &yCoord);

    // Scale x and y to match intended rendering window
    //glfwGetWindowSize(window, &width, &height);
    //x = (x * (double)get_render_width()) / (double)width;
    //y = (y * (double)get_render_height()) / (double)height;

    gHoverConnector.active = false;

    // Ahead of every other gesture: while a palette tile is being dragged the pointer belongs to
    // the palette, and the tile hover highlight has to keep up when it is not.
    palette_cursor_moved(coord);

    if (palette_drag_active()) {
        return;
    }

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

    // Floating panels being moved. Ahead of the canvas gestures below for the same reason the Mutator
    // is: a panel drag owns the pointer until it is released, and the panel is over the canvas, so
    // letting the canvas also act on the motion would rubber-band underneath it.
    if (floating_panels_drag(coord)) {
        return;
    }

    // notes §26
    if (floating_panels_under(coord)) {
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
        // notes §27
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

void scroll_event(double x, double y) {
    tCoord  coord   = {0};

    if (synthlib_popups_dispatch_scroll(y)) {
        return;
    }
    // The wheel acts on the pane UNDER THE CURSOR, not the focused one — hovering the FX half and
    // scrolling should move the FX half, without first having to click into it.
    get_global_gui_scaled_mouse_coord(&coord);

    // Over the palette band the wheel scrolls the TILES sideways - a group wider than the window is
    // otherwise unreachable, and the band is above both panes so no pane wants this event anyway.
    if (palette_scroll(y, coord)) {
        return;
    }
    int32_t hovered = split_view_pane_at(coord);

    if (hovered < 0) {
        hovered = (int32_t)split_view_focused_pane();
    }

    // notes §28
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

void char_event(unsigned int value) {
    if (synthlib_popups_dispatch_char(value)) {
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

    if (notes_own_keyboard()) {
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

// notes §29
static int key_step_direction(int key, int scancode) {
    if ((key == GLFW_KEY_KP_ADD) || (key == GLFW_KEY_KP_SUBTRACT)) {
        return (key == GLFW_KEY_KP_ADD) ? 1 : -1;
    }
    const char * name = glfwGetKeyName(key, scancode);

    if ((name != NULL) && (name[0] != '\0') && (name[1] == '\0')) {
        if ((name[0] == '+') || (name[0] == '=')) {
            return 1;
        }

        if (name[0] == '-') {
            return -1;
        }
        return 0;    // A named key that is neither: on FI/SWE this is how '´' stops zooming in.
    }

    if (key == GLFW_KEY_EQUAL) {
        return 1;
    }

    if (key == GLFW_KEY_MINUS) {
        return -1;
    }
    return 0;
}

void key_callback(int key, int scancode, int action, int mods) {
    LOG_DEBUG("key=%d scancode=%d action=%d mods=%d\n", key, scancode, action, mods);

    if ((key == GLFW_KEY_L) && (action == GLFW_PRESS)) {
        LOG_INFO("L key: mods=0x%x cmdFlag=%d | fileBrowser=%d bankBrowser=%d alert=%d menu=%d "
                 "notesEdit=%d perfNameEdit=%d\n",
                 mods, (int)gCommandKeyPressed, (int)file_browser_active(), (int)bank_browser_active(),
                 (int)alert_dialog_active(), (int)gContextMenu.active,
                 (int)gPatchNotesEdit.active, (int)gPerfNameEdit.active);
    }

    // notes §30
    if (synthlib_popups_dispatch_key(key, mods, action)) {
        synthlib_request_redraw();
        return;
    }

    // notes §31
    if (!any_text_edit_active() && handle_note_entry_key(key, mods, action)) {
        return;
    }

    if (notes_own_keyboard()) {
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
                    tMessageContent msg       = {0};
                    uint32_t        pi        = gParamNameEdit.paramIndex;
                    uint32_t        li        = gParamNameEdit.labelIndex;
                    uint32_t        variation = gPatchDescr[module->key.slot].activeVariation;
                    uint32_t        paramRef  = module->param[variation][pi].paramRef;
                    bool            oldSet    = module->paramNameSet[pi][li];
                    char            oldName[PROTOCOL_PARAM_NAME_SIZE + 1];
                    COPY_STRING(oldName, module->paramName[pi][li]);

                    module->paramNameSet[pi][li]  = true;
                    COPY_STRING(module->paramName[pi][li], gParamNameEdit.buffer);

                    if (paramLocationList[paramRef].type == paramTypeRadioEdit) {
                        // notes §32
                        uint32_t buttons = paramLocationList[paramRef].range;

                        for (uint32_t b = 0; (b < buttons) && (b < MAX_NUM_LABELS); b++) {
                            if (!module->paramNameSet[pi][b]) {
                                module->paramNameSet[pi][b] = true;
                                COPY_STRING(module->paramName[pi][b],
                                            radio_caption(module, pi, b, paramLocationList[paramRef].strMap));
                            }
                        }

                        module->paramNumLabels[pi] = (buttons < MAX_NUM_LABELS) ? buttons : MAX_NUM_LABELS;
                    } else {
                        module->paramNumLabels[pi] = 1;
                    }
                    msg.cmd                       = eMsgCmdSetParamLabel;
                    msg.slot                      = gParamNameEdit.moduleKey.slot;
                    msg.paramLabelData.moduleKey  = gParamNameEdit.moduleKey;
                    msg.paramLabelData.paramIndex = pi;
                    COPY_STRING(msg.paramLabelData.name, gParamNameEdit.buffer);
                    msg_send(&gToUsbThread, &msg);
                    undo_push_param_name(gParamNameEdit.moduleKey, pi, li,
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
        // notes §33
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
    } else if (  (  (key == GLFW_KEY_UP) || (key == GLFW_KEY_DOWN)
                 || (key == GLFW_KEY_LEFT) || (key == GLFW_KEY_RIGHT))
              && ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
              && (gCommandKeyPressed == false)
              && ((mods & (GLFW_MOD_SUPER | GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == 0)) {
        // notes §34
        bool acted = false;

        if ((mods & GLFW_MOD_SHIFT) != 0) {
            // SHIFT MOVES BETWEEN MODULES, all four directions - manual p84. Without shift, Up/Down
            // are the value and Left/Right walk the parameters of the module already focused.
            int dx = (key == GLFW_KEY_LEFT) ? -1 : ((key == GLFW_KEY_RIGHT) ? 1 : 0);
            int dy = (key == GLFW_KEY_UP) ? -1 : ((key == GLFW_KEY_DOWN) ? 1 : 0);

            acted = canvas_move_module_focus(dx, dy);
        } else if ((key == GLFW_KEY_UP) || (key == GLFW_KEY_DOWN)) {
            acted = canvas_nudge_focused_param((key == GLFW_KEY_UP) ? 1 : -1);
        } else {
            acted = canvas_move_param_focus((key == GLFW_KEY_RIGHT) ? 1 : -1);
        }

        if (acted) {
            synthlib_request_redraw();
        }
    } else if (  (key_step_direction(key, scancode) != 0)
              && ((action == GLFW_PRESS) || (action == GLFW_REPEAT))
              && (gCommandKeyPressed == false)
              && ((mods & (GLFW_MOD_SUPER | GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == 0)) {
        // notes §35
        if (canvas_nudge_param_under_cursor(key_step_direction(key, scancode))) {
            synthlib_request_redraw();
        }
    } else if (action == GLFW_PRESS && gCommandKeyPressed == true) {
        // Cmd +/- is the canvas zoom, through canvas_zoom_step() so this shell and the plug-in cannot
        // drift on the step size, the anchor or whether the choice is remembered. Layout-aware for
        // the same reason the bare pair above is.
        int zoomDir = key_step_direction(key, scancode);

        if (zoomDir != 0) {
            LOG_DEBUG("ZOOM %s\n", (zoomDir > 0) ? "IN" : "OUT");
            canvas_zoom_step(zoomDir * ZOOM_DELTA);
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
