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

// System header files
#include <math.h>

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include "defs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "topbarResourcesAccess.h"
#include "utilsGraphics.h"
#include "mouseHandle.h"
#include "graphics.h"
#include "globalVars.h"
#include "protocol.h"
#include "menus.h"

// Drag-start state for vertical/horizontal dial modes
static double gDragStartX    = 0.0;   // cursor position at press — used for restore on release
static double gDragStartY    = 0.0;
static double gDragPrevX     = 0.0;   // cursor position at previous cursor_pos call — used for incremental delta
static double gDragPrevY     = 0.0;
static bool   gDragFirstMove = false; // absorbs the first cursor_pos after CURSOR_DISABLED is set (coordinate discontinuity on macOS)

void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    int winWidth  = 0;
    int winHeight = 0;

    glfwGetCursorPos(gWindow, &(coord->x), &(coord->y));
    glfwGetWindowSize(gWindow, &winWidth, &winHeight);

    coord->x = (coord->x / (double)winWidth) * (get_render_width() / gGlobalGuiScale);
    coord->y = (coord->y / (double)winHeight) * (get_render_height() / gGlobalGuiScale);
}

void adjust_scroll_for_drag(void) {
    tCoord     coord         = {0};
    double     x             = 0.0;
    double     y             = 0.0;
    double     xAdjustAmount = 0.1;
    double     yAdjustAmount = 0.2;
    double     timeDelta     = get_time_delta();
    tRectangle area          = module_area();

    get_global_gui_scaled_mouse_coord(&coord);
    x              = coord.x;
    y              = coord.y;


    xAdjustAmount *= timeDelta;
    yAdjustAmount *= timeDelta;

    if (x > (area.coord.x + area.size.w)) {
        gScrollState.xBar += xAdjustAmount;
        set_x_scroll_bar(gScrollState.xBar);
    }

    if (x < (area.coord.x)) {
        gScrollState.xBar -= xAdjustAmount;
        set_x_scroll_bar(gScrollState.xBar);
    }

    if (y > (area.coord.y + area.size.h)) {
        gScrollState.yBar += yAdjustAmount;
        set_y_scroll_bar(gScrollState.yBar);
    }

    if (y < (area.coord.y)) {
        gScrollState.yBar -= yAdjustAmount;
        set_y_scroll_bar(gScrollState.yBar);
    }
}

void init_patch(uint32_t slot) {  // Todo - think where this should really go
    memset(&gPatchDescr[slot], 0, sizeof(gPatchDescr[0]));
    gPatchDescr[slot].voiceCount      = 1;
    gPatchDescr[slot].barPosition     = 600;
    gPatchDescr[slot].unknown3        = 2;   // unknown9 in Delphi
    gPatchDescr[slot].visible[0]      = 1;
    gPatchDescr[slot].visible[1]      = 1;
    gPatchDescr[slot].visible[2]      = 1;
    gPatchDescr[slot].visible[3]      = 1;
    gPatchDescr[slot].visible[4]      = 1;
    gPatchDescr[slot].visible[5]      = 1;
    gPatchDescr[slot].visible[6]      = 1;
    gPatchDescr[slot].monoPoly        = 1;
    gPatchDescr[slot].activeVariation = 0;
    gPatchDescr[slot].category        = 0;

    database_delete_cables_by_slot(slot);
    database_delete_modules_by_slot(slot);
    gMorphCount[slot]                 = 8; // Check default!?
    gNote2Size[slot]                  = 0;
    gControllerCount[slot]            = 0; // Seems to default to 2, so might need to set up defaults
    gPatchNotesSize[slot]             = 0;
    memset(&(gKnobArray[slot]), 0, sizeof(gKnobArray[0]));
    memset(gNote2[slot], 0, sizeof(gNote2[0]));
    memset(&(gControllerArray[slot]), 0, sizeof(gControllerArray[0]));
    memset(gPatchNotes[slot], 0, sizeof(gPatchNotes[0]));

    COPY_STRING(gGlobalSettings.slot[slot].patchName, "Init");
}

void handle_button(tTopbarControlId controlId) {
    uint32_t slot      = gSlot;
    uint32_t location  = gLocation;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    switch (controlId) {
        case topbarVaId:
        {
            gLocation = locationVa;
            set_exclusive_button_highlight(topbarVaId, topbarFxId, controlId);
            set_x_scroll_bar(0); // or different scroll positions for va and fx!?
            set_y_scroll_bar(0);
            break;
        }
        case topbarFxId:
        {
            gLocation = locationFx;
            set_exclusive_button_highlight(topbarVaId, topbarFxId, controlId);
            set_x_scroll_bar(0);
            set_y_scroll_bar(0);
            break;
        }
        case topbarVariation1Id:
        case topbarVariation2Id:
        case topbarVariation3Id:
        case topbarVariation4Id:
        case topbarVariation5Id:
        case topbarVariation6Id:
        case topbarVariation7Id:
        case topbarVariation8Id:
        case topbarVariationInitId:
        {
            uint32_t        variation      = (uint32_t)controlId - (uint32_t)topbarVariation1Id;

            gPatchDescr[slot].activeVariation      = variation;

            set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId, controlId);

            tMessageContent messageContent = {0};
            messageContent.cmd                     = eMsgCmdSelectVariation;
            messageContent.slot                    = slot;
            messageContent.variationData.variation = variation;
            msg_send(&gCommandQueue, &messageContent);

            break;
        }
        case topbarSlotAId:
        case topbarSlotBId:
        case topbarSlotCId:
        case topbarSlotDId:
        {
            uint32_t        slot           = (uint32_t)controlId - (uint32_t)topbarSlotAId;

            gSlot                        = slot;

            tMessageContent messageContent = {0};
            messageContent.cmd           = eMsgCmdSelectSlot;
            messageContent.slot          = slot;
            messageContent.slotData.slot = slot;
            msg_send(&gCommandQueue, &messageContent);

            set_exclusive_button_highlight(topbarSlotAId, topbarSlotDId, controlId);
            set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId,
                                           (tTopbarControlId)((uint32_t)topbarVariation1Id + gPatchDescr[slot].activeVariation));
            break;
        }
        case topbarNewPatchId:
        {
            init_patch(slot);

            tMessageContent messageContent = {0};
            messageContent.cmd  = eMsgCmdWritePatch;
            messageContent.slot = slot;
            msg_send(&gCommandQueue, &messageContent);
            break;
        }
        default:
            break;
    }
    (void)location;
    (void)variation;
}

void set_up_cable_key(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex) {
    // This logic is pretty horrible - sorry
    cableKey->slot                 = fromModule->key.slot;
    cableKey->location             = fromModule->key.location;
    cableKey->moduleFromIndex      = fromModule->key.index;
    cableKey->connectorFromIoCount = find_io_count_from_index(fromModule, fromModule->connector[gCableDrag.fromConnectorIndex].dir, gCableDrag.fromConnectorIndex);
    cableKey->moduleToIndex        = toModule->key.index;
    cableKey->connectorToIoCount   = find_io_count_from_index(toModule, toModule->connector[toConnectorIndex].dir, toConnectorIndex);
    cableKey->linkType             = fromModule->connector[gCableDrag.fromConnectorIndex].dir;
}

bool swap_cable_to_from_if_needed(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex) {
    if (  fromModule->connector[gCableDrag.fromConnectorIndex].dir == connectorDirIn
       && toModule->connector[toConnectorIndex].dir == connectorDirOut) {
        uint32_t tmpModuleIndex    = cableKey->moduleFromIndex;
        uint32_t tmpConnectorIndex = cableKey->connectorFromIoCount;

        cableKey->moduleFromIndex      = cableKey->moduleToIndex;
        cableKey->connectorFromIoCount = cableKey->connectorToIoCount;
        cableKey->moduleToIndex        = tmpModuleIndex;
        cableKey->connectorToIoCount   = tmpConnectorIndex;
        cableKey->linkType             = toModule->connector[toConnectorIndex].dir;

        return true; // Indicates swap occurred
    }
    return false;
}

void convert_mouse_coord_to_module_area_coord(tCoord * targetCoord, tCoord coord) {
    if (targetCoord == NULL) {
        return;
    }
    double     val  = 0.0;
    tRectangle area = module_area();

    val            = coord.x - area.coord.x;
    val           += calc_scroll_x();
    val           /= get_zoom_factor();
    targetCoord->x = val;

    val            = coord.y - area.coord.y;
    val           += calc_scroll_y();
    val           /= get_zoom_factor();
    targetCoord->y = val;
}

static void send_master_clock_bpm(uint32_t bpm) {
    tMessageContent messageContent = {0};

    messageContent.cmd                    = eMsgCmdSetMasterClockBPM;
    messageContent.masterClockBPMData.bpm = bpm;
    msg_send(&gCommandQueue, &messageContent);
}

static void send_master_clock_run(uint32_t running) {
    tMessageContent messageContent = {0};

    messageContent.cmd                        = eMsgCmdSetMasterClockRun;
    messageContent.masterClockRunData.running = running;
    msg_send(&gCommandQueue, &messageContent);
}

static bool handle_module_press_for_module(tModule * module, tCoord coord, tMouseButton mouseButton, uint32_t variation) {
    bool       retVal     = false;
    uint32_t   paramCount = 0;
    tParamType paramType  = paramTypeCommonDial;

    if (module->key.location == locationMorph) {
        if (module->key.index == 1) {
            paramCount = NUM_MORPHS * 2;
        } else {
            paramCount = 1;
        }
    } else {
        paramCount = module_param_count(module->type);
    }

    for (int i = 0; (i < (int)paramCount) && (retVal == false); i++) {
        tParam * param = &module->param[variation][i];

        if (within_rectangle(coord, gParamRectangle[module->key.slot][module->key.location][module->key.index][i]) && mouseButton == mouseButtonLeftDown) {
            if (module->key.location == locationMorph) {
                paramType = (i < NUM_MORPHS) ? paramTypeCommonDial : paramTypeToggle;
            } else {
                paramType = paramLocationList[param->paramRef].type;
            }

            if (  paramType != paramTypeToggle && paramType != paramTypeMenu
               && paramType != paramTypeBypass && paramType != paramTypeEnable) {
                gParamDragging.moduleKey = module->key;
                gParamDragging.type3     = paramType3Param;
                gParamDragging.param     = i;
                gParamDragging.active    = true;

                if (module->key.location == locationMorph) {
                    gMorphGroupFocus = i;
                }
                bool isSlider = (module->key.location != locationMorph)
                                && (paramType == paramTypeSlider);

                if (gDialMode != eDialModeRotary || isSlider) {
                    glfwGetCursorPos(gWindow, &gDragStartX, &gDragStartY);
                    gDragPrevX     = gDragStartX;
                    gDragPrevY     = gDragStartY;
                    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    gDragFirstMove = true;
                }
                retVal                   = true;
            }
        }
    }

    if (retVal == false) {
        for (int i = 0; (i < (int)module->modeCount) && (retVal == false); i++) {
            tMode * mode = &module->mode[i];

            if (within_rectangle(coord, module->mode[i].rectangle) && mouseButton == mouseButtonLeftDown) {
                if (  modeLocationList[mode->modeRef].type != paramTypeToggle
                   && modeLocationList[mode->modeRef].type != paramTypeMenu) {
                    memset(&gParamDragging, 0, sizeof(gParamDragging));
                    gParamDragging.moduleKey = module->key;
                    gParamDragging.type3     = paramType3Mode;
                    gParamDragging.mode      = i;
                    gParamDragging.active    = true;

                    if (gDialMode != eDialModeRotary) {
                        glfwGetCursorPos(gWindow, &gDragStartX, &gDragStartY);
                        gDragPrevX     = gDragStartX;
                        gDragPrevY     = gDragStartY;
                        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        gDragFirstMove = true;
                    }
                    retVal                   = true;
                }
            }
        }
    }

    if (retVal == false) {
        for (int i = 0; (i < (int)module_connector_count(module->type)) && (retVal == false); i++) {
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
            gModuleDrag.moduleKey = module->key;
            gModuleDrag.active    = true;
            retVal                = true;
        }
    }
    return retVal;
}

bool handle_module_press(tCoord coord, tMouseButton mouseButton) {
    uint32_t slot      = gSlot;
    uint32_t location  = gLocation;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && handle_module_press_for_module(module, coord, mouseButton, variation)) {
            return true;
        }
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationMorph, i);

        if (module->active && handle_module_press_for_module(module, coord, mouseButton, variation)) {
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

    if (module->key.location == locationMorph) {
        paramCount = (module->key.index == 1) ? (NUM_MORPHS * 2) : 1;
    } else {
        paramCount = module_param_count(module->type);
    }

    for (int i = 0; (i < (int)paramCount) && (retVal == false); i++) {
        tParam * param = &module->param[variation][i];

        if (within_rectangle(coord, gParamRectangle[module->key.slot][module->key.location][module->key.index][i]) && mouseButton == mouseButtonLeftUp) {
            if (module->key.location == locationMorph) {
                paramType = (i < NUM_MORPHS) ? paramTypeCommonDial : paramTypeToggle;
            } else {
                paramType = paramLocationList[param->paramRef].type;
            }

            if (paramType == paramTypeMenu) {
                open_toggle_menu(coord, module->key, (uint32_t)i, param->paramRef);
                retVal = true;
            } else if (paramType == paramTypeToggle || paramType == paramTypeBypass || paramType == paramTypeEnable) {
                range        = (module->key.location == locationMorph) ? 2 : paramLocationList[param->paramRef].range;
                param->value = (param->value + 1) % range;
                send_param_value(slot, module->key, (uint32_t)i, variation, param->value);
                retVal       = true;
            }
        }
    }

    if (retVal == false) {
        for (int i = 0; (i < (int)module->modeCount) && (retVal == false); i++) {
            tMode * mode = &module->mode[i];

            if (within_rectangle(coord, module->mode[i].rectangle) && mouseButton == mouseButtonLeftUp) {
                if (modeLocationList[mode->modeRef].type == paramTypeMenu) {
                    open_mode_toggle_menu(coord, module->key, (uint32_t)i, mode->modeRef);
                    retVal = true;
                } else if (modeLocationList[mode->modeRef].type == paramTypeToggle) {
                    mode->value = (mode->value + 1) % modeLocationList[mode->modeRef].range;
                    send_mode_value(slot, module->key, (uint32_t)i, mode->value);
                    retVal      = true;
                }
            }
        }
    }
    return retVal;
}

bool handle_module_release(tCoord coord, tMouseButton mouseButton) {
    uint32_t slot      = gSlot;
    uint32_t location  = gLocation;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    // Only fire if we weren't dragging — dial drags are handled in cursor_pos
    if (gParamDragging.active || gModuleDrag.active || gCableDrag.active) {
        return false;
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && handle_module_release_for_module(module, coord, mouseButton, slot, variation)) {
            return true;
        }
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationMorph, i);

        if (module->active && handle_module_release_for_module(module, coord, mouseButton, slot, variation)) {
            return true;
        }
    }

    return false;
}

bool handle_module_area_click(tCoord coord, int button) {
    bool found = false;

    if (within_rectangle(coord, module_area())) {
        open_module_area_context_menu(coord);
        found = true;
    }
    return found;
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
    //printf("Mouse %f %f bar x %f %f %f %f\n", coord.x, coord.y, gScrollState.xRectangle.coord.x,  gScrollState.xRectangle.coord.y,  gScrollState.xRectangle.size.w,  gScrollState.xRectangle.size.h);

    if (within_rectangle(coord, gScrollState.xThumb)) {
        gScrollState.xGrabOffset  = coord.x - gScrollState.xBar;
        gScrollState.xBarDragging = true;
        return true;
    }

    if (within_rectangle(coord, gScrollState.yThumb)) {
        gScrollState.yGrabOffset  = coord.y - gScrollState.yBar;
        gScrollState.yBarDragging = true;
        return true;
    }
    return false;
}

void stop_dragging(void) {
    if (gParamDragging.active || gTempoDragging || gPerfTempoDragging) {
        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (gDialMode != eDialModeRotary) {
            glfwSetCursorPos(gWindow, gDragStartX, gDragStartY);
        }
    }
    gScrollState.yBarDragging = false;
    gScrollState.xBarDragging = false;
    memset(&gModuleDrag, 0, sizeof(gModuleDrag));
    memset(&gParamDragging, 0, sizeof(gParamDragging));
    memset(&gCableDrag, 0, sizeof(gCableDrag));
    gTempoDragging            = false;
    gPerfTempoDragging        = false;
    gDragFirstMove            = false;
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

static bool input_connector_has_cable(uint32_t slot, uint32_t location,
                                      uint32_t moduleIndex, uint32_t ioCount) {
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if (cable == NULL || !cable->active) {
            continue;
        }

        if (cable->key.moduleToIndex == moduleIndex && cable->key.connectorToIoCount == ioCount) {
            return true;
        }
    }

    return false;
}

bool handle_cable_connect(tCoord coord, uint32_t slot, uint32_t location) {
    bool      found      = false;
    int32_t   i          = 0;
    tCableKey cableKey   = {0};
    tCable    cable      = {0};

    tModule * fromModule = get_module(gCableDrag.fromModuleKey);

    if (fromModule == NULL) {
        return false;
    }

    for (uint32_t idx = 0; idx < MAX_NUM_MODULES && !found; idx++) {
        tModule * toModule = get_module_slot(slot, location, idx);

        if (!toModule->active) {
            continue;
        }

        for (i = 0; i < (int32_t)module_connector_count(toModule->type); i++) {
            if (within_rectangle(coord, toModule->connector[i].rectangle) == true) {
                found                                         = true;
                set_up_cable_key(&cableKey, fromModule, toModule, i);

                swap_cable_to_from_if_needed(&cableKey, fromModule, toModule, i);

                // Prevent self-connections and invalid connections
                if (  (cableKey.moduleFromIndex == cableKey.moduleToIndex && gCableDrag.fromConnectorIndex == i)
                   || (  fromModule->connector[gCableDrag.fromConnectorIndex].dir == connectorDirOut
                      && toModule->connector[i].dir == connectorDirOut)) {
                    break;
                }

                // Note that this call will walk the cables, which we can't nest
                if (input_connector_has_cable(slot, location,
                                              cableKey.moduleToIndex,
                                              cableKey.connectorToIoCount)) {
                    break;
                }
                cable.colour                                  = (uint32_t)cable_colour_for_connector_type(fromModule->connector[gCableDrag.fromConnectorIndex].type);
                write_cable(cableKey, &cable);

                tMessageContent messageContent = {0};

                messageContent.cmd                            = eMsgCmdWriteCable;
                messageContent.slot                           = slot;
                messageContent.cableData.location             = location;
                messageContent.cableData.moduleFromIndex      = cableKey.moduleFromIndex;
                messageContent.cableData.connectorFromIoIndex = cableKey.connectorFromIoCount;
                messageContent.cableData.moduleToIndex        = cableKey.moduleToIndex;
                messageContent.cableData.connectorToIoIndex   = cableKey.connectorToIoCount;
                messageContent.cableData.linkType             = cableKey.linkType;
                messageContent.cableData.colour               = cable.colour;
                msg_send(&gCommandQueue, &messageContent);

                break;
            }
        }
    }

    update_module_up_rates();

    return found;
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
    static bool  noteEditDismissed = false;
    tCoord       coord             = {0};
    tMouseButton mouseButton       = mouseButtonNone;
    bool         found             = false;
    bool         running           = false;
    int32_t      i                 = 0;
    uint32_t     slot              = gSlot;
    uint32_t     location          = gLocation;

    mouseButton = convert_to_mouse_button(button, action);

    get_global_gui_scaled_mouse_coord(&coord);

    if (noteEditDismissed) {
        if (mouseButton == mouseButtonLeftUp) {
            noteEditDismissed = false;
        }
        gReDraw = true;
        return;
    }

    if (gPatchNotesEdit.active) {
        if (mouseButton == mouseButtonLeftDown) {
            if (within_rectangle(coord, gPatchNotesCloseRect)) {
                size_t          len     = strlen(gPatchNotesEdit.buffer);
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
                msg_send(&gCommandQueue, &msg);
            } else if (within_rectangle(coord, gPatchNotesDiscardRect)) {
                size_t origLen = strlen(gPatchNotesEdit.original);
                memset(gPatchNotesEdit.buffer, 0, sizeof(gPatchNotesEdit.buffer));
                memcpy(gPatchNotesEdit.buffer, gPatchNotesEdit.original, origLen);
                gPatchNotesEdit.cursorPos = (uint32_t)origLen;
            } else {
                int newPos = note_editor_cursor_from_click(coord.x, coord.y);

                if (newPos >= 0) {
                    gPatchNotesEdit.cursorPos = (uint32_t)newPos;
                } else {
                    gPatchNotesEdit.active = false;
                    noteEditDismissed      = true;
                }
            }
        }
        gReDraw = true;
        return;
    }

    if (gPerfSettingsEdit.active) {
        if (mouseButton == mouseButtonLeftDown) {
            if (within_rectangle(coord, gPerfSettingsPanelRects.masterClock)) {
                gPerfTempoDragging = true;

                if (gDialMode != eDialModeRotary) {
                    glfwGetCursorPos(gWindow, &gDragStartX, &gDragStartY);
                    gDragPrevX     = gDragStartX;
                    gDragPrevY     = gDragStartY;
                    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    gDragFirstMove = true;
                }
            }
        }

        if (mouseButton == mouseButtonLeftUp) {
            stop_dragging();

            if (gContextMenu.active) {
                if (!handle_context_menu_click(coord)) {
                    gContextMenu.active = false;
                }
                gReDraw = true;
                return;
            }

            if (within_rectangle(coord, gPerfSettingsPanelRects.close)) {
                gPerfSettingsEdit.active = false;
            } else {
                int s = 0;

                if (within_rectangle(coord, gPerfSettingsPanelRects.masterClockRunning)) {
                    open_stop_run_dropdown(below_rect(gPerfSettingsPanelRects.masterClockRunning), &gGlobalSettings.masterClockRunning);
                } else if (within_rectangle(coord, gPerfSettingsPanelRects.keyboardRange)) {
                    open_perf_on_off_dropdown(below_rect(gPerfSettingsPanelRects.keyboardRange), &gPerfSettings.keyboardRange);
                } else {
                    for (s = 0; s < MAX_SLOTS; s++) {
                        if (within_rectangle(coord, gPerfSettingsPanelRects.slotEnabled[s])) {
                            open_perf_on_off_dropdown(below_rect(gPerfSettingsPanelRects.slotEnabled[s]), &gGlobalSettings.slot[s].enabled);
                        } else if (within_rectangle(coord, gPerfSettingsPanelRects.slotKeyboard[s])) {
                            open_perf_on_off_dropdown(below_rect(gPerfSettingsPanelRects.slotKeyboard[s]), &gPerfSettings.slot[s].keyboardEnabled);
                        } else if (within_rectangle(coord, gPerfSettingsPanelRects.slotHold[s])) {
                            open_perf_on_off_dropdown(below_rect(gPerfSettingsPanelRects.slotHold[s]), &gPerfSettings.slot[s].holdEnabled);
                        } else if (within_rectangle(coord, gPerfSettingsPanelRects.rangeLower[s])) {
                            open_midi_note_dropdown(below_rect(gPerfSettingsPanelRects.rangeLower[s]), &gPerfSettings.slot[s].rangeLower);
                        } else if (within_rectangle(coord, gPerfSettingsPanelRects.rangeUpper[s])) {
                            open_midi_note_dropdown(below_rect(gPerfSettingsPanelRects.rangeUpper[s]), &gPerfSettings.slot[s].rangeUpper);
                        }
                    }
                }
            }
        }
        gReDraw = true;
        return;
    }

    if (gPatchParamsEdit.active) {
        if (mouseButton == mouseButtonLeftUp) {
            if (gContextMenu.active) {
                if (!handle_context_menu_click(coord)) {
                    gContextMenu.active = false;
                }
                gReDraw = true;
                return;
            }

            if (within_rectangle(coord, gPatchParamClose)) {
                gPatchParamsEdit.active = false;
            } else {
                bool switched = false;

                for (uint32_t s = 0; s < MAX_SLOTS; s++) {
                    if (within_rectangle(coord, gPatchParamSlots[s])) {
                        gPatchParamsEdit.slot = s;
                        switched              = true;
                        break;
                    }
                }

                if (!switched) {
                    static const struct {
                        tPatchParamRectId id;
                        void              (*open_fn)(tCoord);
                    } kPatchParamTable[] = {
                        {pPSustainPedal,  NULL                            },
                        {pPOctaveShift,   open_patch_octave_shift_dropdown},
                        {pPArpEnabled,    NULL                            },
                        {pPArpRate,       open_arp_rate_dropdown          },
                        {pPArpDirection,  open_arp_direction_dropdown     },
                        {pPArpOctaves,    open_arp_octave_dropdown        },
                        {pPVibratoAmount, open_vibrato_amount_dropdown    },
                        {pPVibratoSource, open_vibrato_source_dropdown    },
                        {pPVibratoRate,   NULL                            },
                        {pPGlideTime,     open_glide_time_dropdown        },
                        {pPGlideMode,     open_glide_mode_dropdown        },
                        {pPBendRange,     open_bend_range_dropdown        },
                        {pPBendEnabled,   NULL                            },
                    };
                    static const struct {
                        tPatchParamRectId id;
                        uint32_t          moduleIndex;
                        uint32_t          paramIndex;
                    } kPatchOnOffTable[] = {
                        {pPSustainPedal, PATCH_SUSTAIN,     SUSTAIN_PEDAL},
                        {pPArpEnabled,   PATCH_ARPEGGIATOR, ARP_ON_OFF   },
                        {pPBendEnabled,  PATCH_BEND,        BEND_ON_OFF  },
                    };

                    for (int i = 0; i < (int)(sizeof(kPatchParamTable) / sizeof(*kPatchParamTable)); i++) {
                        tPatchParamRectId rid = kPatchParamTable[i].id;

                        if (within_rectangle(coord, gPatchParamRects[rid])) {
                            if (kPatchParamTable[i].open_fn != NULL) {
                                kPatchParamTable[i].open_fn(below_rect(gPatchParamRects[rid]));
                            } else {
                                for (int j = 0; j < (int)(sizeof(kPatchOnOffTable) / sizeof(*kPatchOnOffTable)); j++) {
                                    if (kPatchOnOffTable[j].id == rid) {
                                        open_patch_on_off_dropdown(below_rect(gPatchParamRects[rid]),
                                                                   kPatchOnOffTable[j].moduleIndex,
                                                                   kPatchOnOffTable[j].paramIndex);
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        gReDraw = true;
        return;
    }

    if (gPatchSettingsEdit.active) {
        if (mouseButton == mouseButtonLeftUp) {
            if (gContextMenu.active) {
                if (!handle_context_menu_click(coord)) {
                    gContextMenu.active = false;
                }
                gReDraw = true;
                return;
            }

            if (within_rectangle(coord, gSettingsPanelRects.close)) {
                gPatchSettingsEdit.active = false;
                stop_synth_name_editing();
            } else {
                int s = 0;

                stop_synth_name_editing();

                for (s = 0; s < 4; s++) {
                    if (within_rectangle(coord, gSettingsPanelRects.midiChan[s])) {
                        open_midi_chan_dropdown(below_rect(gSettingsPanelRects.midiChan[s]), &gSynthSettings.midiChanSlot[s]);
                    }
                }

                if (within_rectangle(coord, gSettingsPanelRects.synthName)) {
                    gSynthNameEdit.active = true;
                    COPY_STRING(gSynthNameEdit.buffer, gSynthSettings.name);
                } else if (within_rectangle(coord, gSettingsPanelRects.globalChan)) {
                    open_midi_chan_dropdown(below_rect(gSettingsPanelRects.globalChan), &gSynthSettings.globalChan);
                } else if (within_rectangle(coord, gSettingsPanelRects.sysexId)) {
                    open_sysex_id_dropdown(below_rect(gSettingsPanelRects.sysexId), &gSynthSettings.sysexId);
                } else if (within_rectangle(coord, gSettingsPanelRects.localOn)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.localOn), &gSynthSettings.localOn);
                } else if (within_rectangle(coord, gSettingsPanelRects.memoryProtect)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.memoryProtect), &gSynthSettings.memoryProtect);
                } else if (within_rectangle(coord, gSettingsPanelRects.progChangeRcv)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.progChangeRcv), &gSynthSettings.progChangeRcv);
                } else if (within_rectangle(coord, gSettingsPanelRects.progChangeSnd)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.progChangeSnd), &gSynthSettings.progChangeSnd);
                } else if (within_rectangle(coord, gSettingsPanelRects.controllersRcv)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.controllersRcv), &gSynthSettings.controllersRcv);
                } else if (within_rectangle(coord, gSettingsPanelRects.controllersSnd)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.controllersSnd), &gSynthSettings.controllersSnd);
                } else if (within_rectangle(coord, gSettingsPanelRects.sendClock)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.sendClock), &gSynthSettings.sendClock);
                } else if (within_rectangle(coord, gSettingsPanelRects.receiveClock)) {
                    open_on_off_dropdown(below_rect(gSettingsPanelRects.receiveClock), &gSynthSettings.receiveClock);
                } else if (within_rectangle(coord, gSettingsPanelRects.tuneSemi)) {
                    open_tune_semi_dropdown(below_rect(gSettingsPanelRects.tuneSemi), &gSynthSettings.tuneSemi);
                } else if (within_rectangle(coord, gSettingsPanelRects.tuneCent)) {
                    open_tune_cent_dropdown(below_rect(gSettingsPanelRects.tuneCent), &gSynthSettings.tuneCent);
                } else if (within_rectangle(coord, gSettingsPanelRects.globalShiftActive)) {
                    open_active_off_dropdown(below_rect(gSettingsPanelRects.globalShiftActive), &gSynthSettings.globalShiftActive);
                } else if (within_rectangle(coord, gSettingsPanelRects.globalOctaveShift)) {
                    open_octave_shift_dropdown(below_rect(gSettingsPanelRects.globalOctaveShift), &gSynthSettings.globalOctaveShift);
                } else if (within_rectangle(coord, gSettingsPanelRects.pedalPolarity)) {
                    open_pedal_polarity_dropdown(below_rect(gSettingsPanelRects.pedalPolarity), &gSynthSettings.pedalPolarity);
                } else if (within_rectangle(coord, gSettingsPanelRects.pedalGain)) {
                    open_pedal_gain_dropdown(below_rect(gSettingsPanelRects.pedalGain), &gSynthSettings.pedalGain);
                } else if (within_rectangle(coord, gSettingsPanelRects.patchSortMode)) {
                    open_patch_sort_dropdown(below_rect(gSettingsPanelRects.patchSortMode), &gSynthSettings.patchSortMode);
                } else if (within_rectangle(coord, gSettingsPanelRects.perfSortMode)) {
                    open_perf_sort_dropdown(below_rect(gSettingsPanelRects.perfSortMode), &gSynthSettings.perfSortMode);
                }
            }
        }
        gReDraw = true;
        return;
    }
    stop_patch_name_editing();
    stop_module_name_editing();
    stop_param_name_editing();
    stop_perf_name_editing();

    switch (mouseButton) {
        case mouseButtonLeftDown:
        {
            // Button down should only ever deal with start of drag, or graphical indication of button down

            for (i = 0; i < TOPBAR_STANDARD_BUTTON_COUNT; i++) {
                if (within_rectangle(coord, gTopbarControls[i].rectangle)) {
                    found                        = true;
                    gTopbarControls[i].isPressed = true;
                    break;
                }
            }

            if (found == false) {
                if (handle_scrollbar_click(coord)) {
                    found = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPatchVolumeId].rectangle)) {
                    tModuleKey volKey = {slot, (uint32_t)locationMorph, PATCH_VOLUME};
                    gParamDragging.moduleKey = volKey;
                    gParamDragging.type3     = paramType3Param;
                    gParamDragging.param     = VOLUME_LEVEL;
                    gParamDragging.active    = true;

                    if (gDialMode != eDialModeRotary) {
                        glfwGetCursorPos(gWindow, &gDragStartX, &gDragStartY);
                        gDragPrevX     = gDragStartX;
                        gDragPrevY     = gDragStartY;
                        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        gDragFirstMove = true;
                    }
                    found                    = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarTempoDialId].rectangle)) {
                    gTempoDragging = true;

                    if (gDialMode != eDialModeRotary) {
                        glfwGetCursorPos(gWindow, &gDragStartX, &gDragStartY);
                        gDragPrevX     = gDragStartX;
                        gDragPrevY     = gDragStartY;
                        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        gDragFirstMove = true;
                    }
                    found          = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarClockRunStopId].rectangle)) {
                    running                            = !gGlobalSettings.masterClockRunning;
                    gGlobalSettings.masterClockRunning = (uint8_t)running;
                    send_master_clock_run((uint32_t)running);
                    gReDraw                            = true;
                    found                              = true;
                }
            }

            if (found == false) {
                if (handle_module_press(coord, mouseButton) == true) {
                    found = true;
                }
            }
        }
        break;

        case mouseButtonLeftUp:
        {
            for (i = 0; i < TOPBAR_STANDARD_BUTTON_COUNT; i++) {
                gTopbarControls[i].isPressed = false;
            }

            if (found == false) {
                if (gContextMenu.active == true) {
                    if (handle_context_menu_click(coord)) {
                        found = true;
                    } else {
                        gContextMenu.active = false;  // Close if clicked outside - TODO: think if this is the right thing to do here
                    }
                }
            }

            if (found == false) {
                for (i = 0; i < TOPBAR_STANDARD_BUTTON_COUNT; i++) {
                    if (within_rectangle(coord, gTopbarControls[i].rectangle)) {
                        handle_button((tTopbarControlId)i);
                        found = true;
                        break;
                    }
                }
            }

            if (found == false) {
                for (i = 0; i < cableColourMax; i++) {
                    if (within_rectangle(coord, gTopbarControls[(int)topbarCableColourToggle0Id + i].rectangle)) {
                        for (int colour = 0; colour < cableColourMax; colour++) {
                            gPatchDescr[slot].visible[i] = !gPatchDescr[slot].visible[i];
                        }

                        gReDraw             = true;
                        tMessageContent messageContent = {0};
                        messageContent.cmd  = eMsgCmdWritePatchDescr;
                        messageContent.slot = slot;
                        msg_send(&gCommandQueue, &messageContent);
                        found               = true;
                        break;
                    }
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarHideAllCablesId].rectangle)) {
                    bool current = gCablesHideAll;
                    gCablesHideAll = !current;
                    gReDraw        = true;
                    found          = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarTransparentCablesId].rectangle)) {
                    bool current = gCablesTransparent;
                    gCablesTransparent = !current;
                    gReDraw            = true;
                    found              = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPatchNotesId].rectangle)) {
                    gPatchNotesEdit.active    = true;
                    gPatchNotesEdit.slot      = slot;
                    gPatchNotesEdit.cursorPos = gPatchNotesSize[slot];
                    memset(gPatchNotesEdit.buffer, 0, sizeof(gPatchNotesEdit.buffer));
                    memcpy(gPatchNotesEdit.buffer, gPatchNotes[slot], gPatchNotesSize[slot]);
                    memset(gPatchNotesEdit.original, 0, sizeof(gPatchNotesEdit.original));
                    memcpy(gPatchNotesEdit.original, gPatchNotes[slot], gPatchNotesSize[slot]);
                    found                     = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPerfModeId].rectangle)) {
                    tMessageContent msg = {0};

                    if (gGlobalSettings.perfMode == 0) {
                        msg.cmd                  = eMsgCmdWriteModePerf;
                        gGlobalSettings.perfMode = 1;
                    } else {
                        msg.cmd                  = eMsgCmdWriteModePatch;
                        gGlobalSettings.perfMode = 0;
                    }
                    msg_send(&gCommandQueue, &msg);
                    found = true;
                }
            }

            if (found == false) {
                if (gGlobalSettings.perfMode == 1 && within_rectangle(coord, gTopbarControls[topbarPerfNameId].rectangle)) {
                    gPerfNameEdit.active = true;
                    COPY_STRING(gPerfNameEdit.buffer, gGlobalSettings.perfName);
                    found                = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarSettingsId].rectangle)) {
                    gPatchSettingsEdit.active = true;
                    gPatchSettingsEdit.slot   = slot;
                    found                     = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPerfSettingsId].rectangle)) {
                    gPerfSettingsEdit.active = true;
                    found                    = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPatchSettingsId].rectangle)) {
                    gPatchParamsEdit.active = true;
                    gPatchParamsEdit.slot   = slot;
                    found                   = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPatchNameId].rectangle)) {
                    gPatchNameEdit.active = true;
                    gPatchNameEdit.slot   = slot;
                    COPY_STRING(gPatchNameEdit.buffer, gGlobalSettings.slot[slot].patchName);
                    found                 = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPatchTypeId].rectangle)) {
                    open_patch_type_context_menu(gTopbarControls[topbarPatchTypeId].rectangle.coord);
                    found = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarMonoPolyId].rectangle)) {
                    open_mono_poly_context_menu(coord);
                    found = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarVoiceCountId].rectangle)) {
                    open_voice_count_context_menu(coord);
                    found = true;
                }
            }

            if (found == false) {
                if (gModuleDrag.active == true) {
                    shift_modules_down(gModuleDrag.moduleKey);
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

            if (found == false) {
                if (handle_module_release(coord, mouseButton) == true) {
                    found = true;
                }
            }
            stop_dragging();
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
            for (uint32_t idx = 0; idx < MAX_NUM_MODULES && !found; idx++) {
                tModule * module = get_module_slot(slot, location, idx);

                if (!module->active) {
                    continue;
                }

                for (i = 0; i < (int)module_connector_count(module->type); i++) {
                    if (within_rectangle(coord, module->connector[i].rectangle)) {
                        open_connector_context_menu(coord, module->key, i);
                        found = true;
                        break;
                    }
                }

                if (found == false) {
                    uint32_t paramCount = module_param_count(module->type);
                    uint32_t variation  = gPatchDescr[slot].activeVariation;

                    for (uint32_t p = 0; p < paramCount && !found; p++) {
                        if (within_rectangle(coord, gParamRectangle[module->key.slot][module->key.location][module->key.index][p])) {
                            open_param_context_menu(coord, module->key, p);
                            found = true;
                        }
                    }
                }

                if (found == false) {
                    if (within_rectangle(coord, module->rectangle)) {
                        open_module_context_menu(coord, module->key);
                        found = true;
                    }
                }
            }

            if (found == false) {
                for (int mi = 0; mi < NUM_MORPHS && !found; mi++) {
                    if (within_rectangle(coord, gMorphLabelRect[mi])) {
                        open_morph_label_context_menu(coord, (uint32_t)mi);
                        found = true;
                    }
                }
            }

            if (found == false) {
                if (handle_module_area_click(coord, button)) {
                    found = true;
                }
            }

            for (i = (int)topbarVariation1Id; i <= (int)topbarVariationInitId; i++) {
                if (within_rectangle(coord, gTopbarControls[i].rectangle)) {
                    uint32_t sourceVariation = (uint32_t)i - (uint32_t)topbarVariation1Id;
                    open_variation_copy_menu(coord, sourceVariation);
                    found = true;
                    break;
                }
            }

            stop_dragging();
        }
        break;

        default:
            break;
    }
    gReDraw = true;
}

void cursor_pos(GLFWwindow * window, double xCoord, double yCoord) {
    tCoord          coord          = {0};
    double          angle          = 0.0;
    uint32_t        range          = 0;
    uint32_t        value          = 0;
    tMessageContent messageContent = {0};
    bool            noAction       = false; // unused — kept to avoid restructuring the else-chain below
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

    if (gDragFirstMove) {
        gDragPrevX     = xCoord;
        gDragPrevY     = yCoord;
        gDragFirstMove = false;
        gReDraw        = true;
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
        // msg_send(&gCommandQueue, &messageContent);
    } else if (gTempoDragging == true) {
        if (gDialMode == eDialModeVertical) {
            int newVal = (int)gGlobalSettings.masterClock + (int)((gDragPrevY - yCoord) * 241.0 / 200.0);
            gDragPrevY = yCoord;

            if (newVal < 0) {
                newVal = 0;
            }

            if (newVal > 240) {
                newVal = 240;
            }
            value      = (uint32_t)newVal;
        } else if (gDialMode == eDialModeHorizontal) {
            int newVal = (int)gGlobalSettings.masterClock + (int)((xCoord - gDragPrevX) * 241.0 / 200.0);
            gDragPrevX = xCoord;

            if (newVal < 0) {
                newVal = 0;
            }

            if (newVal > 240) {
                newVal = 240;
            }
            value      = (uint32_t)newVal;
        } else {
            angle = calculate_mouse_angle((tCoord){x, y}, gTopbarControls[topbarTempoDialId].rectangle);
            value = angle_to_value(angle, 241);
        }

        if (gGlobalSettings.masterClock != value) {
            gGlobalSettings.masterClock = (uint8_t)value;
            send_master_clock_bpm(value);
        }
    } else if (gPerfTempoDragging == true) {
        if (gDialMode == eDialModeVertical) {
            int newVal = (int)gGlobalSettings.masterClock + (int)((gDragPrevY - yCoord) * 241.0 / 200.0);
            gDragPrevY = yCoord;

            if (newVal < 0) {
                newVal = 0;
            }

            if (newVal > 240) {
                newVal = 240;
            }
            value      = (uint32_t)newVal;
        } else if (gDialMode == eDialModeHorizontal) {
            int newVal = (int)gGlobalSettings.masterClock + (int)((xCoord - gDragPrevX) * 241.0 / 200.0);
            gDragPrevX = xCoord;

            if (newVal < 0) {
                newVal = 0;
            }

            if (newVal > 240) {
                newVal = 240;
            }
            value      = (uint32_t)newVal;
        } else {
            angle = calculate_mouse_angle((tCoord){x, y}, gPerfSettingsPanelRects.masterClock);
            value = angle_to_value(angle, 241);
        }

        if (gGlobalSettings.masterClock != value) {
            gGlobalSettings.masterClock = (uint8_t)value;
            send_master_clock_bpm(value);
        }
    } else if (gParamDragging.active == true) {
        tModule * module = get_module(gParamDragging.moduleKey);

        if (module != NULL) {
            switch (gParamDragging.type3) {
                case paramType3Param:

                    if (module->key.location == locationMorph) {
                        paramType = paramTypeCommonDial;
                    } else {
                        paramType = paramLocationList[module->param[variation][gParamDragging.param].paramRef].type;
                    }

                    if (  paramType != paramTypeToggle && paramType != paramTypeMenu
                       && paramType != paramTypeBypass && paramType != paramTypeEnable) {
                        if (module->key.location == locationMorph) {
                            range     = 128;
                            paramType = paramTypeCommonDial;
                        } else {
                            range     = paramLocationList[module->param[variation][gParamDragging.param].paramRef].range;
                            paramType = paramLocationList[module->param[variation][gParamDragging.param].paramRef].type;
                        }

                        if (paramType == paramTypeSlider) {
                            int newVal = (int)module->param[variation][gParamDragging.param].value + (int)((gDragPrevY - yCoord) * (double)range / 200.0);
                            gDragPrevY = yCoord;

                            if (newVal < 0) {
                                newVal = 0;
                            }

                            if (newVal >= (int)range) {
                                newVal = (int)range - 1;
                            }
                            value      = (uint32_t)newVal;
                        } else if (gDialMode == eDialModeVertical) {
                            int newVal = (int)module->param[variation][gParamDragging.param].value + (int)((gDragPrevY - yCoord) * (double)range / 200.0);
                            gDragPrevY = yCoord;

                            if (newVal < 0) {
                                newVal = 0;
                            }

                            if (newVal >= (int)range) {
                                newVal = (int)range - 1;
                            }
                            value      = (uint32_t)newVal;
                        } else if (gDialMode == eDialModeHorizontal) {
                            int newVal = (int)module->param[variation][gParamDragging.param].value + (int)((xCoord - gDragPrevX) * (double)range / 200.0);
                            gDragPrevX = xCoord;

                            if (newVal < 0) {
                                newVal = 0;
                            }

                            if (newVal >= (int)range) {
                                newVal = (int)range - 1;
                            }
                            value      = (uint32_t)newVal;
                        } else {
                            angle = calculate_mouse_angle((tCoord){x, y}, gParamRectangle[module->key.slot][module->key.location][module->key.index][gParamDragging.param]);
                            value = angle_to_value(angle, range);
                        }

                        if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) != GLFW_PRESS) {
                            if (module->param[variation][gParamDragging.param].value != value) {
                                module->param[variation][gParamDragging.param].value = value;
                                send_param_value(slot, gParamDragging.moduleKey, gParamDragging.param, variation, value);
                            }
                        } else {
                            if (module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] != value) {
                                if (value >= module->param[variation][gParamDragging.param].value) {
                                    module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] = value - module->param[variation][gParamDragging.param].value;
                                } else {
                                    module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] = 256 - (module->param[variation][gParamDragging.param].value - value);
                                }
                                LOG_DEBUG("Write to module %u variation %u\n", module->key.index, variation);

                                messageContent.cmd                       = eMsgCmdSetParamMorph;
                                messageContent.slot                      = slot;
                                messageContent.paramMorphData.moduleKey  = module->key;
                                messageContent.paramMorphData.param      = gParamDragging.param;
                                messageContent.paramMorphData.paramMorph = gMorphGroupFocus;
                                messageContent.paramMorphData.value      = module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus];
                                messageContent.paramMorphData.negative   = 0;
                                messageContent.paramMorphData.variation  = variation;
                                msg_send(&gCommandQueue, &messageContent);
                            }
                        }
                    }
                    break;
                case paramType3Mode:

                    if (  modeLocationList[module->mode[gParamDragging.mode].modeRef].type != paramTypeToggle
                       && modeLocationList[module->mode[gParamDragging.mode].modeRef].type != paramTypeMenu) {
                        uint32_t modeRange = modeLocationList[module->mode[gParamDragging.mode].modeRef].range;

                        if (gDialMode == eDialModeVertical) {
                            int newVal = (int)module->mode[gParamDragging.mode].value + (int)((gDragPrevY - yCoord) * (double)modeRange / 200.0);
                            gDragPrevY = yCoord;

                            if (newVal < 0) {
                                newVal = 0;
                            }

                            if (newVal >= (int)modeRange) {
                                newVal = (int)modeRange - 1;
                            }
                            value      = (uint32_t)newVal;
                        } else if (gDialMode == eDialModeHorizontal) {
                            int newVal = (int)module->mode[gParamDragging.mode].value + (int)((xCoord - gDragPrevX) * (double)modeRange / 200.0);
                            gDragPrevX = xCoord;

                            if (newVal < 0) {
                                newVal = 0;
                            }

                            if (newVal >= (int)modeRange) {
                                newVal = (int)modeRange - 1;
                            }
                            value      = (uint32_t)newVal;
                        } else {
                            angle = calculate_mouse_angle((tCoord){x, y}, module->mode[gParamDragging.mode].rectangle);
                            value = angle_to_value(angle, modeRange);
                        }

                        if (module->mode[gParamDragging.mode].value != value) {
                            module->mode[gParamDragging.mode].value = value;
                            send_mode_value(slot, gParamDragging.moduleKey, gParamDragging.mode, value);
                        }
                    }
                    break;
            }
        }
    } else if (gModuleDrag.active == true) {
        tModule * module = get_module(gModuleDrag.moduleKey);

        if (module != NULL) {
            convert_mouse_coord_to_module_column_row(&module->column, &module->row, (tCoord){x, y});

            if (module->row > 127) {
                module->row = 127;
            }

            if (module->column > 127) {
                module->column = 127;
            }
        }
        adjust_scroll_for_drag();
    } else if (gCableDrag.active == true) {
        convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, (tCoord){x - scale_from_percent(CONNECTOR_SIZE / 2.0), y - scale_from_percent(CONNECTOR_SIZE / 2.0)});  // SOMETHING NOT RIGHT HERE
        adjust_scroll_for_drag();
    } else if (gContextMenu.active == true) {
        // Dummy
    } else if (  (coord.x >= 0.0)
              && (coord.y >= TOP_BAR_HEIGHT)
              && (coord.x < (get_render_width() / gGlobalGuiScale) - SCROLLBAR_WIDTH)
              && (coord.y < (get_render_height() / gGlobalGuiScale) - SCROLLBAR_WIDTH)) {
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

        noAction = true;
    }
    // Limit re-draw/render if nothing's happened
    // if (noAction == false) {   // Used to have this check, TODO - see if there's a way to not redraw on every move
    gReDraw = true;
    // }
}

void scroll_event(GLFWwindow * window, double x, double y) {
    tCoord coord      = {0};
    double zoomFactor = 0.0;

    if (gCommandKeyPressed == true) {
        get_global_gui_scaled_mouse_coord(&coord);
        zoomFactor  = get_zoom_factor();
        zoomFactor += y * ZOOM_DELTA;
        set_zoom_factor(zoomFactor, coord);
    } else {
        if (x != 0) {
            gScrollState.xBar -= x / 2;
            set_x_scroll_bar(gScrollState.xBar);
        }

        if (y != 0) {
            gScrollState.yBar -= y;
            set_y_scroll_bar(gScrollState.yBar);
        }
    }
//    LOG_DEBUG("Area: %f %f - size: %i %i - barY %f %f %f \n", moduleArea.size.w,moduleArea.size.h, width,height, gScrollState.yBar, gScrollState.yRectangle.size.h,gScrollState.yRectangle.coord.y);

    gReDraw = true;
}

void char_event(GLFWwindow * window, unsigned int value) {
    if (gPatchNameEdit.active) {
        size_t len = strlen(gPatchNameEdit.buffer);

        // Accept printable ASCII only, up to PATCH_NAME_SIZE
        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            gPatchNameEdit.buffer[len]     = (char)value;
            gPatchNameEdit.buffer[len + 1] = '\0';
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
        size_t len = strlen(gModuleNameEdit.buffer);

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            gModuleNameEdit.buffer[len]     = (char)value;
            gModuleNameEdit.buffer[len + 1] = '\0';
        }
    }

    if (gParamNameEdit.active) {
        size_t len = strlen(gParamNameEdit.buffer);

        if ((value >= 0x20) && (value <= 0x7e) && (len < PROTOCOL_PARAM_NAME_SIZE)) {
            gParamNameEdit.buffer[len]     = (char)value;
            gParamNameEdit.buffer[len + 1] = '\0';
        }
    }

    if (gSynthNameEdit.active) {
        size_t len = strlen(gSynthNameEdit.buffer);

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            gSynthNameEdit.buffer[len]     = (char)value;
            gSynthNameEdit.buffer[len + 1] = '\0';
        }
    }

    if (gPerfNameEdit.active) {
        size_t len = strlen(gPerfNameEdit.buffer);

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            gPerfNameEdit.buffer[len]     = (char)value;
            gPerfNameEdit.buffer[len + 1] = '\0';
        }
    }
    LOG_DEBUG("char=%d\n", value);
    gReDraw = true;
}

void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
    double zoomFactor = 0.0;

    LOG_DEBUG("key=%d scancode=%d action=%d mods=%d\n", key, scancode, action, mods);

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
                msg_send(&gCommandQueue, &msg);
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
        gReDraw = true;
        return;
    } else if (gPatchNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            if (key == GLFW_KEY_BACKSPACE) {
                size_t len = strlen(gPatchNameEdit.buffer);

                if (len > 0) {
                    gPatchNameEdit.buffer[len - 1] = '\0';
                }
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                // Commit
                gPatchNameEdit.active = false;
                COPY_STRING(gGlobalSettings.slot[gPatchNameEdit.slot].patchName, gPatchNameEdit.buffer);
                tMessageContent messageContent = {0};
                messageContent.cmd    = eMsgCmdSetPatchName;
                messageContent.slot   = gPatchNameEdit.slot;
                COPY_STRING(messageContent.patchName.name, gGlobalSettings.slot[gPatchNameEdit.slot].patchName);
                msg_send(&gCommandQueue, &messageContent);
            } else if (key == GLFW_KEY_ESCAPE) {
                // Cancel — discard edits
                gPatchNameEdit.active = false;
            }
        }
    } else if (gModuleNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            if (key == GLFW_KEY_BACKSPACE) {
                size_t len = strlen(gModuleNameEdit.buffer);

                if (len > 0) {
                    gModuleNameEdit.buffer[len - 1] = '\0';
                }
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gModuleNameEdit.active = false;

                tModule * module = get_module(gModuleNameEdit.moduleKey);

                if (module != NULL) {
                    tMessageContent msg = {0};
                    COPY_STRING(module->name, gModuleNameEdit.buffer);
                    msg.cmd                       = eMsgCmdSetModuleLabel;
                    msg.slot                      = gModuleNameEdit.moduleKey.slot;
                    msg.moduleLabelData.moduleKey = gModuleNameEdit.moduleKey;
                    COPY_STRING(msg.moduleLabelData.name, gModuleNameEdit.buffer);
                    msg_send(&gCommandQueue, &msg);
                }
            } else if (key == GLFW_KEY_ESCAPE) {
                gModuleNameEdit.active = false;  // discard
            }
        }
    } else if (gParamNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            if (key == GLFW_KEY_BACKSPACE) {
                size_t len = strlen(gParamNameEdit.buffer);

                if (len > 0) {
                    gParamNameEdit.buffer[len - 1] = '\0';
                }
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gParamNameEdit.active = false;

                tModule * module = get_module(gParamNameEdit.moduleKey);

                if (module != NULL) {
                    tMessageContent msg = {0};
                    uint32_t        pi  = gParamNameEdit.paramIndex;

                    module->paramNameSet[pi][0]   = true;
                    COPY_STRING(module->paramName[pi][0], gParamNameEdit.buffer);
                    module->paramNumLabels[pi]    = 1;

                    msg.cmd                       = eMsgCmdSetParamLabel;
                    msg.slot                      = gParamNameEdit.moduleKey.slot;
                    msg.paramLabelData.moduleKey  = gParamNameEdit.moduleKey;
                    msg.paramLabelData.paramIndex = pi;
                    COPY_STRING(msg.paramLabelData.name, gParamNameEdit.buffer);
                    msg_send(&gCommandQueue, &msg);
                }
            } else if (key == GLFW_KEY_ESCAPE) {
                gParamNameEdit.active = false;  // discard
            }
        }
    } else if (gSynthNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            if (key == GLFW_KEY_BACKSPACE) {
                size_t len = strlen(gSynthNameEdit.buffer);

                if (len > 0) {
                    gSynthNameEdit.buffer[len - 1] = '\0';
                }
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
            if (key == GLFW_KEY_BACKSPACE) {
                size_t len = strlen(gPerfNameEdit.buffer);

                if (len > 0) {
                    gPerfNameEdit.buffer[len - 1] = '\0';
                }
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gPerfNameEdit.active = false;
                COPY_STRING(gGlobalSettings.perfName, gPerfNameEdit.buffer);
                tMessageContent messageContent = {0};
                messageContent.cmd   = eMsgCmdWritePerfName;
                msg_send(&gCommandQueue, &messageContent);
            } else if (key == GLFW_KEY_ESCAPE) {
                gPerfNameEdit.active = false;
            }
        }
    } else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    } else if (key == GLFW_KEY_LEFT_SUPER && action == GLFW_PRESS) {
        gCommandKeyPressed = true;
    } else if (key == GLFW_KEY_LEFT_SUPER && action == GLFW_RELEASE) {
        gCommandKeyPressed = false;
    } else if (action == GLFW_PRESS && gCommandKeyPressed == true) {
        tRectangle area  = {0};
        tCoord     coord = {0};

        area    = module_area();
        coord.x = area.coord.x + area.size.w / 2.0;
        coord.y = area.coord.y + area.size.h / 2.0;

        // React on command key with - + keys for zooming
        if (key == GLFW_KEY_MINUS) {
            LOG_DEBUG("ZOOM OUT\n");
            zoomFactor  = get_zoom_factor();
            zoomFactor -= ZOOM_DELTA;
            set_zoom_factor(zoomFactor, coord);
        }

        if (key == GLFW_KEY_EQUAL) {
            LOG_DEBUG("ZOOM IN\n");
            zoomFactor  = get_zoom_factor();
            zoomFactor += ZOOM_DELTA;
            set_zoom_factor(zoomFactor, coord);
        }
    }
    gReDraw = true;
}

#ifdef __cplusplus
}
#endif
