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
#include "misc.h"
#include "appMenuBar.h"
#include "fileBrowser.h"
#include "bankBrowser.h"
#include "alertDialog.h"
#include "clickRegion.h"
#include "cableChain.h"

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

bool shift_modifier_held(void) {
    return (glfwGetKey(synthlib_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
           || (glfwGetKey(synthlib_window(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
}

bool cmd_modifier_held(void) {
    return (glfwGetKey(synthlib_window(), GLFW_KEY_LEFT_SUPER) == GLFW_PRESS)
           || (glfwGetKey(synthlib_window(), GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS);
}

// The application's answer: ask GLFW directly. See mouseHandle.h for why this is a function.
bool multi_select_modifier_held(void) {
    return (glfwGetKey(synthlib_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
           || (glfwGetKey(synthlib_window(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
           || (glfwGetKey(synthlib_window(), GLFW_KEY_LEFT_SUPER) == GLFW_PRESS)
           || (glfwGetKey(synthlib_window(), GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS);
}

void start_cursor_drag(void) {
    {
        double startX = 0.0;
        double startY = 0.0;

        glfwGetCursorPos(synthlib_window(), &startX, &startY);
        canvas_drag_set_origin(startX, startY);
    }
    gDragSkipCount = 3;
    glfwSetInputMode(synthlib_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

static bool handle_module_press_for_module(tModule * module, tCoord coord, tMouseButton mouseButton, uint32_t variation) {
    bool       retVal     = false;
    uint32_t   paramCount = 0;
    tParamType paramType  = paramTypeCommonDial;
    int        i          = 0;
    bool       isSlider   = false;
    tParam *   param      = NULL;
    tMode *    mode       = NULL;

    if (module->key.location == locationMorph) {
        if (module->key.index == 1) {
            paramCount = NUM_MORPHS * 2;
        } else {
            paramCount = 1;
        }
    } else {
        paramCount = module_param_count(module->type);
    }

    for (i = 0; (i < (int)paramCount) && (retVal == false); i++) {
        param = &module->param[variation][i];

        if (within_rectangle(coord, gParamRectangle[module->key.slot][module->key.location][module->key.index][i]) && mouseButton == mouseButtonLeftDown) {
            if (module->key.location == locationMorph) {
                paramType = (i < NUM_MORPHS) ? paramTypeCommonDial : paramTypeToggle;
            } else {
                paramType = paramLocationList[param->paramRef].type;
            }

            if (  paramType != paramTypeToggle && paramType != paramTypeMenu
               && paramType != paramTypeBypass && paramType != paramTypeEnable
               && paramType != paramTypePush && paramType != paramTypeCustomData) {
                gParamDragging.moduleKey       = module->key;
                gParamDragging.type3           = paramType3Param;
                gParamDragging.param           = i;
                gParamDragging.startValue      = param->value;
                gParamDragging.active          = true;

                if (module->key.location == locationMorph) {
                    gMorphGroupFocus = i;
                }
                gParamDragging.startMorphRange = param->morphRange[gMorphGroupFocus];
                isSlider                       = (module->key.location != locationMorph)
                                                 && (paramType == paramTypeSlider);

                if (synthlib_dial_mode() != eDialModeRotary || isSlider) {
                    start_cursor_drag();
                }
            } else if (paramType == paramTypePush) {
                send_param_value(module->key.slot, module->key, (uint32_t)i, variation, 0);
                param->value = 0;
            }
            retVal = true;
        }
    }

    if (retVal == false) {
        for (i = 0; (i < (int)module->modeCount) && (retVal == false); i++) {
            mode = &module->mode[i];

            if (within_rectangle(coord, module->mode[i].rectangle) && mouseButton == mouseButtonLeftDown) {
                if (  modeLocationList[mode->modeRef].type != paramTypeToggle
                   && modeLocationList[mode->modeRef].type != paramTypeMenu) {
                    memset(&gParamDragging, 0, sizeof(gParamDragging));
                    gParamDragging.moduleKey  = module->key;
                    gParamDragging.type3      = paramType3Mode;
                    gParamDragging.mode       = i;
                    gParamDragging.startValue = mode->value;
                    gParamDragging.active     = true;

                    if (synthlib_dial_mode() != eDialModeRotary) {
                        start_cursor_drag();
                    }
                    retVal                    = true;
                }
            }
        }
    }

    if (retVal == false) {
        for (i = 0; (i < (int)module_connector_count(module->type)) && (retVal == false); i++) {
            if (within_rectangle(coord, module->connector[i].rectangle)) {
                gCableDrag.fromModuleKey = module->key;

                if (mouseButton == mouseButtonLeftDown) {
                    gCableDrag.fromConnectorIndex = i;
                    convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, coord);
                    gCableDrag.active             = true;
                    retVal                        = true;
                }
            }
        }
    }

    if (retVal == false) {
        if (within_rectangle(coord, module->dragArea) && mouseButton == mouseButtonLeftDown) {
            bool multiSelectHeld = multi_select_modifier_held();

            if (multiSelectHeld) {
                selection_toggle(module->key);
            } else if (!is_selected(module->key)) {
                selection_set_single(module->key);
            }
            gModuleDrag.moduleKey     = module->key;
            gModuleDrag.isMulti       = is_selected(module->key) && gSelection.count > 1;
            gModuleDrag.prevColumn    = module->column;
            gModuleDrag.prevRow       = module->row;
            gModuleDrag.active        = true;

            // Snapshot starting positions for move undo
            gModuleDrag.snapshotCount = 0;

            if (gModuleDrag.isMulti) {
                for (uint32_t si = 0; si < gSelection.count && si < MAX_NUM_MODULES; si++) {
                    tModule * sel = get_module(gSelection.keys[si]);

                    if (!sel) {
                        continue;
                    }
                    gModuleDrag.snapshotKeys[gModuleDrag.snapshotCount]   = gSelection.keys[si];
                    gModuleDrag.snapshotColumn[gModuleDrag.snapshotCount] = sel->column;
                    gModuleDrag.snapshotRow[gModuleDrag.snapshotCount]    = sel->row;
                    gModuleDrag.snapshotCount++;
                }
            } else {
                gModuleDrag.snapshotKeys[0]   = module->key;
                gModuleDrag.snapshotColumn[0] = module->column;
                gModuleDrag.snapshotRow[0]    = module->row;
                gModuleDrag.snapshotCount     = 1;
            }
            retVal                    = true;
        }
    }

    // Clicking anywhere else on the module body selects without starting a drag
    if (retVal == false) {
        if (within_rectangle(coord, module->rectangle) && mouseButton == mouseButtonLeftDown) {
            bool multiSelectHeld = multi_select_modifier_held();

            if (multiSelectHeld) {
                selection_toggle(module->key);
            } else {
                selection_set_single(module->key);
            }
            retVal = true;
        }
    }
    return retVal;
}

// Pure legacy fallback now — morph group dials register their own click
// region (eClickLayerPanel, moduleGraphics.cpp's morph_param_click_handler)
// which dispatch_click_region() already checks, and wins over regular
// modules, before this is ever reached. See mouse_button().
static bool handle_morph_press(tCoord coord, tMouseButton mouseButton) {
    uint32_t slot      = gSlot;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationMorph, i);

        if (module->active && handle_module_press_for_module(module, coord, mouseButton, variation)) {
            return true;
        }
    }

    return false;
}

bool handle_module_press(tCoord coord, tMouseButton mouseButton) {
    uint32_t slot      = gSlot;
    uint32_t location  = gLocation;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    // Morph group knobs are drawn on top of the scrollable module canvas
    // (render_morph_groups() runs after render_modules() each frame), so
    // they must also win hit-testing first — otherwise a regular module
    // param that happens to sit at the same screen location intercepts the
    // click instead.
    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationMorph, i);

        if (module->active && handle_module_press_for_module(module, coord, mouseButton, variation)) {
            return true;
        }
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && handle_module_press_for_module(module, coord, mouseButton, variation)) {
            return true;
        }
    }

    return false;
}

// Step the parameter under the cursor by one raw wire unit. A dial maps its whole range across a
// few tens of pixels, so a drag cannot reliably land on a chosen value, let alone move by exactly
// one - which is what identifying a parameter's real quantisation needs (where does the synth's own
// display actually change?). The step is written to the device exactly as a drag's is, so the G2
// reacts to each one. Pair it with View > Parameter Values to see the raw number being stepped.
static bool nudge_param_for_module(tModule * module, tCoord coord, uint32_t variation, int delta) {
    uint32_t paramCount = 0;

    if (module->key.location == locationMorph) {
        paramCount = (module->key.index == 1) ? (NUM_MORPHS * 2) : 1;
    } else {
        paramCount = module_param_count(module->type);
    }

    for (uint32_t i = 0; i < paramCount; i++) {
        tParam *   param = &module->param[variation][i];
        tParamType type  = paramTypeCommonDial;
        uint32_t   range = 128;
        int        newValue;

        if (!within_rectangle(coord, gParamRectangle[module->key.slot][module->key.location][module->key.index][i])) {
            continue;
        }

        if (module->key.location != locationMorph) {
            type  = paramLocationList[param->paramRef].type;
            range = paramLocationList[param->paramRef].range;
        }

        // Push is momentary and CustomData is not a scalar, so neither has a value to step. Return
        // true regardless: the cursor IS over this parameter, and falling through to keep looking
        // would let a widget behind it take the keypress instead.
        if ((type == paramTypePush) || (type == paramTypeCustomData) || (range == 0)) {
            return true;
        }
        newValue = (int)param->value + delta;

        if (newValue < 0) {
            newValue = 0;
        } else if (newValue >= (int)range) {
            newValue = (int)range - 1;
        }

        if ((uint32_t)newValue != param->value) {
            param->value = (uint32_t)newValue;
            send_param_value(module->key.slot, module->key, i, variation, (uint32_t)newValue);
        }
        return true;
    }

    // Mode selectors render as dials too, so hovering one and getting nothing would be the
    // surprise. They carry their own range table and their own write command.
    for (uint32_t i = 0; i < module->modeCount; i++) {
        tMode *  mode  = &module->mode[i];
        uint32_t range = modeLocationList[mode->modeRef].range;
        int      newValue;

        if (!within_rectangle(coord, mode->rectangle)) {
            continue;
        }

        if (range == 0) {
            return true;
        }
        newValue = (int)mode->value + delta;

        if (newValue < 0) {
            newValue = 0;
        } else if (newValue >= (int)range) {
            newValue = (int)range - 1;
        }

        if ((uint32_t)newValue != mode->value) {
            mode->value = (uint32_t)newValue;
            send_mode_value(module->key.slot, module->key, i, (uint32_t)newValue);
        }
        return true;
    }

    return false;
}

static bool nudge_param_under_cursor(int delta) {
    tCoord   coord     = {0};
    uint32_t slot      = gSlot;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    get_global_gui_scaled_mouse_coord(&coord);

    // Same precedence as handle_module_press(): morph knobs are drawn on top of the canvas, so they
    // have to win the hit test against a module param that happens to sit at the same place.
    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationMorph, i);

        if (module->active && nudge_param_for_module(module, coord, variation, delta)) {
            return true;
        }
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, gLocation, i);

        if (module->active && nudge_param_for_module(module, coord, variation, delta)) {
            return true;
        }
    }

    return false;
}

static bool handle_module_release_for_module(tModule * module, tCoord coord, tMouseButton mouseButton, uint32_t slot, uint32_t variation) {
    bool       retVal     = false;
    uint32_t   paramCount = 0;
    tParamType paramType  = paramTypeCommonDial;
    uint32_t   range      = 0;
    int        i          = 0;
    tParam *   param      = NULL;
    tMode *    mode       = NULL;

    if (module->key.location == locationMorph) {
        paramCount = (module->key.index == 1) ? (NUM_MORPHS * 2) : 1;
    } else {
        paramCount = module_param_count(module->type);
    }

    for (i = 0; (i < (int)paramCount) && (retVal == false); i++) {
        param = &module->param[variation][i];

        if (within_rectangle(coord, gParamRectangle[module->key.slot][module->key.location][module->key.index][i]) && mouseButton == mouseButtonLeftUp) {
            if (module->key.location == locationMorph) {
                paramType = (i < NUM_MORPHS) ? paramTypeCommonDial : paramTypeToggle;
            } else {
                paramType = paramLocationList[param->paramRef].type;
            }

            if (paramType == paramTypeMenu || paramType == paramTypeCustomData) {
                open_toggle_menu(coord, module->key, (uint32_t)i, param->paramRef);
                retVal = true;
            } else if (paramType == paramTypeToggle || paramType == paramTypeBypass || paramType == paramTypeEnable) {
                range        = (module->key.location == locationMorph) ? 2 : paramLocationList[param->paramRef].range;
                uint32_t oldParamVal = param->value;
                param->value = (param->value + 1) % range;
                send_param_value(slot, module->key, (uint32_t)i, variation, param->value);
                undo_push_param_change(module->key, (uint32_t)i, variation, oldParamVal, param->value);
                retVal       = true;
            } else if (paramType == paramTypePush) {
                uint32_t listSize = array_size_param_location_list();

                for (uint32_t ref = 0; ref < listSize; ref++) {
                    if (  paramLocationList[ref].moduleType == module->type
                       && paramLocationList[ref].type == paramTypeCustomData) {
                        send_custom_data_value(slot, module->key);
                        break;
                    }
                }

                send_param_value(slot, module->key, (uint32_t)i, variation, 1);
                param->value = 0;
                retVal       = true;
            }
        }
    }

    if (retVal == false) {
        for (i = 0; (i < (int)module->modeCount) && (retVal == false); i++) {
            mode = &module->mode[i];

            if (within_rectangle(coord, module->mode[i].rectangle) && mouseButton == mouseButtonLeftUp) {
                if (modeLocationList[mode->modeRef].type == paramTypeMenu) {
                    open_mode_toggle_menu(coord, module->key, (uint32_t)i, mode->modeRef);
                    retVal = true;
                } else if (modeLocationList[mode->modeRef].type == paramTypeToggle) {
                    uint32_t oldModeVal = mode->value;
                    mode->value = (mode->value + 1) % modeLocationList[mode->modeRef].range;
                    send_mode_value(slot, module->key, (uint32_t)i, mode->value);
                    undo_push_mode_change(module->key, (uint32_t)i, oldModeVal, mode->value);
                    retVal      = true;
                }
            }
        }
    }
    return retVal;
}

// See handle_morph_press() — same reasoning, for the release side.
static bool handle_morph_release(tCoord coord, tMouseButton mouseButton) {
    uint32_t slot      = gSlot;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    if (gParamDragging.active || gModuleDrag.active || gCableDrag.active) {
        return false;
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationMorph, i);

        if (module->active && handle_module_release_for_module(module, coord, mouseButton, slot, variation)) {
            return true;
        }
    }

    return false;
}

bool handle_module_release(tCoord coord, tMouseButton mouseButton) {
    uint32_t slot      = gSlot;
    uint32_t location  = gLocation;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    // Only fire if we weren't dragging — dial drags are handled in cursor_pos
    if (gParamDragging.active || gModuleDrag.active || gCableDrag.active) {
        return false;
    }

    // Same priority as handle_module_press: morph knobs are drawn on top of
    // the module canvas, so they must be hit-tested first.
    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationMorph, i);

        if (module->active && handle_module_release_for_module(module, coord, mouseButton, slot, variation)) {
            return true;
        }
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && handle_module_release_for_module(module, coord, mouseButton, slot, variation)) {
            return true;
        }
    }

    return false;
}

bool handle_module_area_click(tCoord coord) {
    if (within_rectangle(coord, module_area())) {
        open_module_area_context_menu(coord);
        return true;
    }
    return false;
}

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
    (void)canvas_param_drag_release();
    stop_dragging();
}

void stop_dragging(void) {
    bool wasCursorDragging = is_cursor_hidden_dragging();

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
    if (wasCursorDragging) {
        glfwSetInputMode(synthlib_window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void stop_patch_name_editing(void) {
    memset(&gPatchNameEdit, 0, sizeof(gPatchNameEdit));
}

void stop_module_name_editing(void) {
    memset(&gModuleNameEdit, 0, sizeof(gModuleNameEdit));
}

void stop_param_name_editing(void) {
    memset(&gParamNameEdit, 0, sizeof(gParamNameEdit));
}

void stop_patch_notes_editing(void) {
    memset(&gPatchNotesEdit, 0, sizeof(gPatchNotesEdit));
}

void stop_synth_name_editing(void) {
    memset(&gSynthNameEdit, 0, sizeof(gSynthNameEdit));
}

void stop_perf_name_editing(void) {
    memset(&gPerfNameEdit, 0, sizeof(gPerfNameEdit));
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

    if (handle_mutator_mouse(coord, mouseButton)) {
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

    if (handle_virtual_keyboard_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_patch_adjuster_mouse(coord, mouseButton)) {
        synthlib_request_redraw();
        return;
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
    if (mouseButton == mouseButtonLeftDown || mouseButton == mouseButtonRightDown) {
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

            // Every clickable widget — module params/modes/connectors/body/drag-handle
            // AND the morph group overlay — registers a click region at render time (see
            // moduleGraphics.cpp). Morph registers at eClickLayerPanel, everything else at
            // eClickLayerCanvas, so dispatch itself (not call order) guarantees morph wins
            // over a scrolled regular module that happens to sit visually underneath it —
            // see moduleGraphics.cpp's own top-of-file comment. handle_morph_press() and
            // handle_module_press() below are now pure legacy fallback, kept for anything
            // dispatch doesn't match (should be nothing today).
            if (!found && !gContextMenu.active) {
                found = dispatch_click_region(coord, eClickPress);
            }

            if (!found && !gContextMenu.active) {
                found = handle_morph_press(coord, mouseButton);
            }

            if (!found && !gContextMenu.active) {
                found = handle_module_press(coord, mouseButton);
            }

            // Click on empty module-area space: clear selection and start rubber-band. Shared with
            // the plug-in — see canvasDrag.h.
            if (!found && !gContextMenu.active) {
                bool shiftHeld = (glfwGetKey(synthlib_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                                 || (glfwGetKey(synthlib_window(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                found = canvas_empty_press(coord, shiftHeld);
            }
        }
        break;

        case mouseButtonLeftUp:
        {
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
                if (gModuleDrag.active == true) {
                    if (gModuleDrag.isMulti) {
                        shift_selection_down();
                    } else {
                        shift_modules_down(gModuleDrag.moduleKey);
                    }
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
                if (gCableDrag.active) {
                    if (handle_cable_connect(coord, slot, location) == true) {
                        found = true;
                    }
                }
            }

            // See the matching mouseButtonLeftDown case: dispatch is layer-ordered
            // (morph's eClickLayerPanel beats everything else's eClickLayerCanvas), so
            // call order here no longer matters — handle_morph_release()/
            // handle_module_release() are pure legacy fallback.
            if (!found) {
                found = dispatch_click_region(coord, eClickRelease);
            }

            if (!found) {
                found = handle_morph_release(coord, mouseButton);
            }

            if (!found) {
                found = handle_module_release(coord, mouseButton);
            }
            {
                bool shiftHeld = (glfwGetKey(synthlib_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                                 || (glfwGetKey(synthlib_window(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                if (canvas_rubber_band_release(coord, slot, location, shiftHeld)) {
                    found = true;
                }
            }
            finish_param_drag();
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

void cursor_pos(GLFWwindow * window, double xCoord, double yCoord) {
    tCoord          coord          = {0};
    double          angle          = 0.0;
    uint32_t        range          = 0;
    uint32_t        value          = 0;
    tMessageContent messageContent = {0};
    tParamType      paramType      = paramTypeCommonDial;
    uint32_t        slot           = gSlot;
    uint32_t        variation      = gPatchDescr[slot].activeVariation;
    double          x              = 0;
    double          y              = 0;


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

    if (gMutator.active && (gMutator.draggingPanel || (gMutator.draggingSlider >= 0))) {
        handle_mutator_cursor_pos(coord);
        synthlib_request_redraw();
        return;
    }

    // Mouse is just hovering over the (non-dragging) Mutator panel - skip all the hover detection
    // below (cable highlight, knob/CC overlays, etc.) so nothing hidden behind the floater lights
    // up. gHoverConnector.active is already false from just above.
    if (gMutator.active && within_rectangle(coord, gMutator.panelRect)) {
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
    } else if (gTempoDragging == true) {
        value = calc_tempo_drag_value(xCoord, yCoord, x, y, gTopbarControls[topbarTempoDialId].rectangle);

        if (gGlobalSettings.masterClock != value) {
            gGlobalSettings.masterClock = (uint8_t)value;
            send_master_clock_bpm(value);
        }
    } else if (gPerfTempoDragging == true) {
        value = calc_tempo_drag_value(xCoord, yCoord, x, y, gPerfSettingsPanelRects.masterClock);

        if (gGlobalSettings.masterClock != value) {
            gGlobalSettings.masterClock = (uint8_t)value;
            send_master_clock_bpm(value);
        }
    } else if (gVibAmountDragging == true) {
        tModuleKey vibKey    = {(uint32_t)gPatchParamsEdit.slot, (uint32_t)locationMorph, patchModuleVibrato};
        tModule *  vibModule = get_module(vibKey);
        int        newVal    = 0;

        if (vibModule != NULL) {
            if (synthlib_dial_mode() == eDialModeHorizontal) {
                newVal     = (int)vibModule->param[0][VIBRATO_DEPTH].value + (int)((xCoord - gDragPrevX) * 101.0 / 200.0);
                gDragPrevX = xCoord;
            } else if (synthlib_dial_mode() == eDialModeRotary) {
                newVal = (int)angle_to_value(calculate_mouse_angle((tCoord){x, y}, gPatchParamRects[pPVibratoAmount]), 101);
            } else {
                newVal     = (int)vibModule->param[0][VIBRATO_DEPTH].value + (int)((gDragPrevY - yCoord) * 101.0 / 200.0);
                gDragPrevY = yCoord;
            }

            if (newVal < 0) {
                newVal = 0;
            }

            if (newVal > 100) {
                newVal = 100;
            }
            value = (uint32_t)newVal;

            if (vibModule->param[0][VIBRATO_DEPTH].value != value) {
                vibModule->param[0][VIBRATO_DEPTH].value = (uint8_t)value;
                send_param_value(slot, vibKey, VIBRATO_DEPTH, 0, value);
            }
        }
    } else if (gVibRateDragging == true) {
        tModuleKey vibKey    = {(uint32_t)gPatchParamsEdit.slot, (uint32_t)locationMorph, patchModuleVibrato};
        tModule *  vibModule = get_module(vibKey);
        int        newVal    = 0;

        if (vibModule != NULL) {
            if (synthlib_dial_mode() == eDialModeHorizontal) {
                newVal     = (int)vibModule->param[0][VIBRATO_RATE].value + (int)((xCoord - gDragPrevX) * 128.0 / 200.0);
                gDragPrevX = xCoord;
            } else if (synthlib_dial_mode() == eDialModeRotary) {
                newVal = (int)angle_to_value(calculate_mouse_angle((tCoord){x, y}, gPatchParamRects[pPVibratoRate]), 128);
            } else {
                newVal     = (int)vibModule->param[0][VIBRATO_RATE].value + (int)((gDragPrevY - yCoord) * 128.0 / 200.0);
                gDragPrevY = yCoord;
            }

            if (newVal < 0) {
                newVal = 0;
            }

            if (newVal > 127) {
                newVal = 127;
            }
            value = (uint32_t)newVal;

            if (vibModule->param[0][VIBRATO_RATE].value != value) {
                vibModule->param[0][VIBRATO_RATE].value = (uint8_t)value;
                send_param_value(slot, vibKey, VIBRATO_RATE, 0, value);
            }
        }
    } else if (gGlideTimeDragging == true) {
        tModuleKey glideKey    = {(uint32_t)gPatchParamsEdit.slot, (uint32_t)locationMorph, patchModuleGlide};
        tModule *  glideModule = get_module(glideKey);
        int        newVal      = 0;

        if (glideModule != NULL) {
            if (synthlib_dial_mode() == eDialModeHorizontal) {
                newVal     = (int)glideModule->param[0][GLIDE_SPEED].value + (int)((xCoord - gDragPrevX) * 128.0 / 200.0);
                gDragPrevX = xCoord;
            } else if (synthlib_dial_mode() == eDialModeRotary) {
                newVal = (int)angle_to_value(calculate_mouse_angle((tCoord){x, y}, gPatchParamRects[pPGlideTime]), 128);
            } else {
                newVal     = (int)glideModule->param[0][GLIDE_SPEED].value + (int)((gDragPrevY - yCoord) * 128.0 / 200.0);
                gDragPrevY = yCoord;
            }

            if (newVal < 0) {
                newVal = 0;
            }

            if (newVal > 127) {
                newVal = 127;
            }
            value = (uint32_t)newVal;

            if (glideModule->param[0][GLIDE_SPEED].value != value) {
                glideModule->param[0][GLIDE_SPEED].value = (uint8_t)value;
                send_param_value(slot, glideKey, GLIDE_SPEED, 0, value);
            }
        }
    } else if (canvas_param_drag_motion(coord, xCoord, yCoord,
                                        glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS)) {
        // Parameter dragging moved to canvasDrag.c so the plug-in shares it — see canvasDrag.h.
    } else if (gModuleDrag.active == true) {
        // Module drag motion moved to canvasDrag.c so the plug-in can share it — see canvasDrag.h.
        // The auto-scroll stays here: it belongs to whoever owns the scrollbars, which a plug-in
        // canvas does not yet.
        (void)canvas_drag_motion(coord);
        adjust_scroll_for_drag();
    } else if (gCableDrag.active == true) {
        convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, (tCoord){x - scale_from_percent(CONNECTOR_SIZE / 2.0), y - scale_from_percent(CONNECTOR_SIZE / 2.0)});  // SOMETHING NOT RIGHT HERE
        adjust_scroll_for_drag();
    } else if (gRubberBand.active == true) {
        (void)canvas_drag_motion(coord);   // shared with the plug-in — see canvasDrag.h
    } else if (gContextMenu.active == true) {
        // Dummy
    } else if (  (coord.x >= 0.0)
              && (coord.y >= TOP_BAR_HEIGHT + MENU_BAR_HEIGHT)
              && (coord.x < (get_render_width() / gGlobalGuiScale) - MODULE_SCROLLBAR_WIDTH)
              && (coord.y < (get_render_height() / gGlobalGuiScale) - MODULE_SCROLLBAR_WIDTH)) {
        uint32_t hoverSlot = gSlot;
        uint32_t hoverLoc  = gLocation;

        for (uint32_t idx = 0; idx < MAX_NUM_MODULES; idx++) {
            tModule * hoverModule = get_module_slot(hoverSlot, hoverLoc, idx);

            if (!hoverModule->active) {
                continue;
            }

            for (int i = 0; i < (int)module_connector_count(hoverModule->type); i++) {
                if (within_rectangle(coord, hoverModule->connector[i].rectangle)) {
                    gHoverConnector.active      = true;
                    gHoverConnector.slot        = hoverSlot;
                    gHoverConnector.location    = hoverLoc;
                    gHoverConnector.moduleIndex = hoverModule->key.index;
                    gHoverConnector.ioCount     = (uint32_t)find_io_count_from_index(hoverModule, hoverModule->connector[i].dir, i);
                    gHoverConnector.dir         = hoverModule->connector[i].dir;
                    break;
                }
            }

            if (gHoverConnector.active) {
                break;
            }
        }
    }
    // Limit re-draw/render if nothing's happened
    // if (noAction == false) {   // Used to have this check, TODO - see if there's a way to not redraw on every move
    synthlib_request_redraw();
    // }
}

void scroll_event(GLFWwindow * window, double x, double y) {
    tCoord  coord      = {0};
    double  zoomFactor = 0.0;

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
    int32_t hovered    = split_view_pane_at(coord);

    if (hovered < 0) {
        hovered = (int32_t)split_view_focused_pane();
    }

    if (gCommandKeyPressed == true) {
        uint32_t prevPane = module_pane();

        set_module_pane((uint32_t)hovered);
        zoomFactor  = get_zoom_factor();
        zoomFactor += y * ZOOM_DELTA;
        set_zoom_factor(zoomFactor, coord);
        save_zoom_factor(get_zoom_factor());
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
    double zoomFactor = 0.0;

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

    if (handle_mutator_key(key, mods, action)) {
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

    if (handle_virtual_keyboard_key(key, mods, action)) {
        synthlib_request_redraw();
        return;
    }

    if (handle_patch_adjuster_key(key, mods, action)) {
        synthlib_request_redraw();
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

        if (nudge_param_under_cursor(increment ? 1 : -1)) {
            synthlib_request_redraw();
        }
    } else if (action == GLFW_PRESS && gCommandKeyPressed == true) {
        tRectangle area  = {0};
        tCoord     coord = {0};

        area    = module_area();
        coord.x = area.coord.x;
        coord.y = area.coord.y;

        // React on command key with - + keys for zooming
        if (key == GLFW_KEY_MINUS) {
            LOG_DEBUG("ZOOM OUT\n");
            zoomFactor  = get_zoom_factor();
            zoomFactor -= ZOOM_DELTA;
            set_zoom_factor(zoomFactor, coord);
            save_zoom_factor(get_zoom_factor());
        }

        if (key == GLFW_KEY_EQUAL) {
            LOG_DEBUG("ZOOM IN\n");
            zoomFactor  = get_zoom_factor();
            zoomFactor += ZOOM_DELTA;
            set_zoom_factor(zoomFactor, coord);
            save_zoom_factor(get_zoom_factor());
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
