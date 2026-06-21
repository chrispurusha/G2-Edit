/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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
    patch_name_set(slot, "Init");
}

void handle_button(tTopbarControlId controlId) {
    uint32_t slot      = atomic_load(&gSlot);
    uint32_t location  = atomic_load(&gLocation);
    uint32_t variation = gPatchDescr[slot].activeVariation;

    switch (controlId) {
        case topbarVaId:
        {
            atomic_store(&gLocation, locationVa);
            set_exclusive_button_highlight(topbarVaId, topbarFxId, controlId);
            set_x_scroll_bar(0); // or different scroll positions for va and fx!?
            set_y_scroll_bar(0);
            break;
        }
        case topbarFxId:
        {
            atomic_store(&gLocation, locationFx);
            set_exclusive_button_highlight(topbarVaId, topbarFxId, controlId);
            set_x_scroll_bar(0);
            set_y_scroll_bar(0);
            break;
        }
        case topbarOpenReadFileId:
        {
            gShowOpenFileReadDialogue = true;
            break;
        }
        case topbarOpenWriteFileId:
        {
            gShowOpenFileWriteDialogue = true;
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

            atomic_store(&gSlot, slot);

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

int32_t find_knob_for_param(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex) {
    for (int i = 0; i < MAX_NUM_KNOBS; i++) {
        if (  gKnobArray[slot].knob[i].assigned
           && gKnobArray[slot].knob[i].location == location
           && gKnobArray[slot].knob[i].moduleIndex == moduleIndex
           && gKnobArray[slot].knob[i].paramIndex == paramIndex) {
            return i;
        }
    }

    return -1;
}

static void action_assign_knob(int index) {
    uint32_t        slot        = atomic_load(&gSlot);
    uint32_t        targetKnob  = (uint32_t)gContextMenu.items[index].param;
    uint32_t        location    = gContextMenu.moduleKey.location;
    uint32_t        moduleIndex = gContextMenu.moduleKey.index;
    uint32_t        paramIndex  = gContextMenu.paramIndex;
    int32_t         existingKnob;
    tMessageContent msg         = {0};

    if (gKnobArray[slot].knob[targetKnob].assigned) {
        gKnobArray[slot].knob[targetKnob].assigned = false;
        msg.cmd                                    = eMsgCmdDeassignKnob;
        msg.slot                                   = slot;
        msg.knobDeassignData.knobIndex             = targetKnob;
        msg_send(&gCommandQueue, &msg);
        memset(&msg, 0, sizeof(msg));
    }
    existingKnob                                  = find_knob_for_param(slot, location, moduleIndex, paramIndex);

    if (existingKnob >= 0 && (uint32_t)existingKnob != targetKnob) {
        gKnobArray[slot].knob[existingKnob].assigned = false;
        msg.cmd                                      = eMsgCmdDeassignKnob;
        msg.slot                                     = slot;
        msg.knobDeassignData.knobIndex               = (uint32_t)existingKnob;
        msg_send(&gCommandQueue, &msg);
        memset(&msg, 0, sizeof(msg));
    }
    gKnobArray[slot].knob[targetKnob].assigned    = true;
    gKnobArray[slot].knob[targetKnob].location    = location;
    gKnobArray[slot].knob[targetKnob].moduleIndex = moduleIndex;
    gKnobArray[slot].knob[targetKnob].isLed       = 0;
    gKnobArray[slot].knob[targetKnob].paramIndex  = paramIndex;

    msg.cmd                                       = eMsgCmdAssignKnob;
    msg.slot                                      = slot;
    msg.knobAssignData.moduleKey                  = gContextMenu.moduleKey;
    msg.knobAssignData.paramIndex                 = paramIndex;
    msg.knobAssignData.knobIndex                  = targetKnob;
    msg_send(&gCommandQueue, &msg);

    gContextMenu.active                           = false;
    atomic_store(&gReDraw, true);
}

static void action_deassign_knob(int index) {
    uint32_t        slot        = atomic_load(&gSlot);
    uint32_t        location    = gContextMenu.moduleKey.location;
    uint32_t        moduleIndex = gContextMenu.moduleKey.index;
    uint32_t        paramIndex  = gContextMenu.paramIndex;
    int32_t         knobIndex   = find_knob_for_param(slot, location, moduleIndex, paramIndex);
    tMessageContent msg         = {0};

    if (knobIndex >= 0) {
        gKnobArray[slot].knob[knobIndex].assigned = false;
        msg.cmd                                   = eMsgCmdDeassignKnob;
        msg.slot                                  = slot;
        msg.knobDeassignData.knobIndex            = (uint32_t)knobIndex;
        msg_send(&gCommandQueue, &msg);
    }
    gContextMenu.active = false;
    atomic_store(&gReDraw, true);
}

static void action_set_toggle_value(int index) {
    uint32_t slot      = atomic_load(&gSlot);
    uint32_t variation = gPatchDescr[slot].activeVariation;
    tModule  module    = {0};

    if (read_module(gContextMenu.moduleKey, &module)) {
        uint32_t paramIdx = gContextMenu.paramIndex;
        module.param[variation][paramIdx].value = (uint32_t)gContextMenu.items[index].param;
        write_module(gContextMenu.moduleKey, &module);
        send_param_value(slot, gContextMenu.moduleKey, paramIdx, variation, module.param[variation][paramIdx].value);
    }
    gContextMenu.active = false;
    atomic_store(&gReDraw, true);
}

static void open_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex, uint32_t paramRef) {
    static tMenuItem menuItems[33];
    static char      labels[32][32];

    const char **    strMap = paramLocationList[paramRef].strMap;
    uint32_t         range  = paramLocationList[paramRef].range;

    for (uint32_t v = 0; v < range && v < 32; v++) {
        snprintf(labels[v], sizeof(labels[v]), "%s", (strMap && strMap[v]) ? strMap[v] : "");
        menuItems[v] = (tMenuItem){
            labels[v], RGB_GREY_3, action_set_toggle_value, v, NULL
        };
    }

    menuItems[range < 32 ? range : 32] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    gContextMenu.moduleKey             = moduleKey;
    gContextMenu.paramIndex            = paramIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
}

static void action_set_mode_value(int index) {
    uint32_t slot   = atomic_load(&gSlot);
    tModule  module = {0};

    if (read_module(gContextMenu.moduleKey, &module)) {
        uint32_t modeIdx = gContextMenu.paramIndex;
        module.mode[modeIdx].value = (uint32_t)gContextMenu.items[index].param;
        write_module(gContextMenu.moduleKey, &module);
        send_mode_value(slot, gContextMenu.moduleKey, modeIdx, module.mode[modeIdx].value);
    }
    gContextMenu.active = false;
    atomic_store(&gReDraw, true);
}

static void open_mode_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t modeIndex, uint32_t modeRef) {
    static tMenuItem menuItems[33];
    static char      labels[32][32];

    const char **    strMap = modeLocationList[modeRef].strMap;
    uint32_t         range  = modeLocationList[modeRef].range;

    for (uint32_t v = 0; v < range && v < 32; v++) {
        snprintf(labels[v], sizeof(labels[v]), "%s", (strMap && strMap[v]) ? strMap[v] : "");
        menuItems[v] = (tMenuItem){
            labels[v], RGB_GREY_3, action_set_mode_value, v, NULL
        };
    }

    menuItems[range < 32 ? range : 32] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    gContextMenu.moduleKey             = moduleKey;
    gContextMenu.paramIndex            = modeIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
}

static void action_rename_param_label(int index) {
    tModule  module = {0};
    uint32_t pi     = gContextMenu.paramIndex;

    if (read_module(gContextMenu.moduleKey, &module)) {
        gParamNameEdit.active     = true;
        gParamNameEdit.moduleKey  = gContextMenu.moduleKey;
        gParamNameEdit.paramIndex = pi;
        memset(gParamNameEdit.buffer, 0, sizeof(gParamNameEdit.buffer));

        if (module.paramNameSet[pi][0]) {
            strncpy(gParamNameEdit.buffer, module.paramName[pi][0], PROTOCOL_PARAM_NAME_SIZE);
        }
    }
    gContextMenu.active = false;
    atomic_store(&gReDraw, true);
}

void open_param_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex) {
    static tMenuItem pageMenuItems[NUM_PARAM_PAGES + 1];
    static char      pageLabels[NUM_PARAM_PAGES][10];
    static tMenuItem bankMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE + 1];
    static char      bankLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][24];
    static tMenuItem slotMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK + 1];
    static char      slotLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK][64];
    static tMenuItem menuItems[4];

    uint32_t         slot     = atomic_load(&gSlot);
    int32_t          assigned = find_knob_for_param(slot, moduleKey.location, moduleKey.index, paramIndex);
    int              count    = 0;

    for (int pg = 0; pg < NUM_PARAM_PAGES; pg++) {
        snprintf(pageLabels[pg], sizeof(pageLabels[pg]), "Page %c", 'A' + pg);

        for (int bk = 0; bk < NUM_BANKS_PER_PAGE; bk++) {
            snprintf(bankLabels[pg][bk], sizeof(bankLabels[pg][bk]), "%c - Bank %d", 'A' + pg, bk + 1);

            for (int k = 0; k < NUM_KNOBS_PER_BANK; k++) {
                uint32_t knobIdx = (uint32_t)((pg * NUM_BANKS_PER_PAGE + bk) * NUM_KNOBS_PER_BANK + k);
                bool     inUse   = gKnobArray[slot].knob[knobIdx].assigned;

                if (inUse) {
                    tModule      mod     = {0};
                    tModuleKey   modKey  = {
                        slot,
                        gKnobArray[slot].knob[knobIdx].location,
                        gKnobArray[slot].knob[knobIdx].moduleIndex
                    };
                    uint32_t     pi      = gKnobArray[slot].knob[knobIdx].paramIndex;
                    const char * modName = "";
                    const char * parName = "";

                    if (read_module(modKey, &mod)) {
                        uint32_t variation = gPatchDescr[slot].activeVariation;

                        modName = (mod.name[0] != '\0') ? mod.name : gModuleProperties[mod.type].name;

                        if ((pi < MAX_NUM_PARAMETERS) && mod.paramNameSet[pi][0]) {
                            parName = mod.paramName[pi][0];
                        } else if (pi < MAX_NUM_PARAMETERS) {
                            const char * label = paramLocationList[mod.param[variation][pi].paramRef].label;

                            if (label != NULL && label[0] != '\0') {
                                parName = label;
                            }
                        }
                    }
                    snprintf(slotLabels[pg][bk][k], sizeof(slotLabels[pg][bk][k]),
                             "%c %d - %d Used - %s %s", 'A' + pg, bk + 1, k + 1, modName, parName);
                } else {
                    snprintf(slotLabels[pg][bk][k], sizeof(slotLabels[pg][bk][k]),
                             "%c %d - %d ---", 'A' + pg, bk + 1, k + 1);
                }
                slotMenuItems[pg][bk][k] = (tMenuItem){
                    slotLabels[pg][bk][k], RGB_GREY_3, action_assign_knob, knobIdx, NULL
                };
            }

            slotMenuItems[pg][bk][NUM_KNOBS_PER_BANK] = (tMenuItem){
                NULL, RGB_BLACK, NULL, 0, NULL
            };

            bankMenuItems[pg][bk]                     = (tMenuItem){
                bankLabels[pg][bk], RGB_GREY_3, NULL, 0, slotMenuItems[pg][bk]
            };
        }

        bankMenuItems[pg][NUM_BANKS_PER_PAGE] = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };

        pageMenuItems[pg]                     = (tMenuItem){
            pageLabels[pg], RGB_GREY_3, NULL, 0, bankMenuItems[pg]
        };
    }

    pageMenuItems[NUM_PARAM_PAGES] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    menuItems[count++]             = (tMenuItem){
        "Assign knob...", RGB_GREY_3, NULL, 0, pageMenuItems
    };

    if (assigned >= 0) {
        menuItems[count++] = (tMenuItem){
            "Deassign knob", RGB_GREY_3, action_deassign_knob, 0, NULL
        };
    }
    {
        tModule  mod       = {0};
        uint32_t variation = gPatchDescr[slot].activeVariation;

        if (read_module(moduleKey, &mod) && (paramIndex < MAX_NUM_PARAMETERS)) {
            if (paramLocationList[mod.param[variation][paramIndex].paramRef].type1 == paramType1Enable) {
                menuItems[count++] = (tMenuItem){
                    "Rename", RGB_GREY_3, action_rename_param_label, 0, NULL
                };
            }
        }
    }

    menuItems[count]        = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    gContextMenu.moduleKey  = moduleKey;
    gContextMenu.paramIndex = paramIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
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

bool handle_module_press(tCoord coord, tMouseButton mouseButton) {
    bool        retVal     = false;
    uint32_t    paramCount = 0;
    tParamType2 paramType2 = paramType2Dial;
    uint32_t    slot       = atomic_load(&gSlot);
    uint32_t    location   = atomic_load(&gLocation);
    uint32_t    variation  = gPatchDescr[slot].activeVariation;
    tModule     module     = {0};

    // Since morph parameters are in top banner area, no longer need to check if (!within_rectangle(coord, module_area()))
    reset_walk_module();

    while (walk_next_module(&module) && (retVal == false)) {
        if (module.key.slot == slot && (module.key.location == location || module.key.location == locationMorph)) {
            if (module.key.location == locationMorph) {
                if (module.key.index == 1) {  // TODO: See if we can roll count into standard mechanism and pre-create the morph modules - maybe create new types at end of list?
                    paramCount = NUM_MORPHS * 2;
                } else {
                    paramCount = 1;  // TODO: correct value for index, probably through a function which returns correct count
                }
            } else {
                paramCount = module_param_count(module.type);
            }

            // Deal with click on param
            for (int i = 0; (i < paramCount) && (retVal == false); i++) {
                tParam * param = &module.param[variation][i];

                if (within_rectangle(coord, param->rectangle) && mouseButton == mouseButtonLeftDown) {
                    if (module.key.location == locationMorph) {      // TODO: See if we can roll count into standard mechanism and pre-create the morph modules - maybe create new types at end of list?
                        if (i < NUM_MORPHS) {
                            paramType2 = paramType2Dial;
                        } else {
                            paramType2 = paramType2Toggle;
                        }
                    } else {
                        paramType2 = (paramLocationList[param->paramRef].type2);
                    }

                    if (paramType2 == paramType2Dial) {
                        gParamDragging.moduleKey = module.key;
                        gParamDragging.type3     = paramType3Param;
                        gParamDragging.param     = i;
                        gParamDragging.active    = true;

                        if (module.key.location == locationMorph) {
                            gMorphGroupFocus = i;
                        }
                        retVal                   = true;
                    }
                    // Toggle: retVal stays false, handled on release
                }
            }

            // Modes — dials only on press
            if (retVal == false) {
                for (int i = 0; (i < module.modeCount) && (retVal == false); i++) {
                    tMode * mode = &module.mode[i];

                    if (within_rectangle(coord, module.mode[i].rectangle) && mouseButton == mouseButtonLeftDown) {
                        if ((modeLocationList[mode->modeRef].type2) == paramType2Dial) {
                            memset(&gParamDragging, 0, sizeof(gParamDragging));
                            gParamDragging.moduleKey = module.key;
                            gParamDragging.type3     = paramType3Mode;
                            gParamDragging.mode      = i;
                            gParamDragging.active    = true;
                            retVal                   = true;
                        }
                        // Mode toggle: handled on release
                    }
                }
            }

            // Connectors — start cable drag on press
            if (retVal == false) {
                for (int i = 0; (i < module_connector_count(module.type)) && (retVal == false); i++) {
                    if (within_rectangle(coord, module.connector[i].rectangle)) {
                        gCableDrag.fromModuleKey = module.key;

                        if (mouseButton == mouseButtonLeftDown) {
                            gCableDrag.fromConnectorIndex = i;
                            convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, coord);
                            gCableDrag.active             = true;
                            retVal                        = true;
                        }
                    }
                }
            }

            // Module drag area — start drag on press
            if (retVal == false) {
                if (within_rectangle(coord, module.dragArea) && mouseButton == mouseButtonLeftDown) {
                    delete_module(module.key);
                    write_module(module.key, &module);
                    gModuleDrag.moduleKey = module.key;
                    gModuleDrag.active    = true;
                    retVal                = true;
                }
            }
        }
    }
    finish_walk_module();
    return retVal;
}

bool handle_module_release(tCoord coord, tMouseButton mouseButton) {
    bool        retVal     = false;
    uint32_t    paramCount = 0;
    tParamType2 paramType2 = paramType2Dial;
    uint32_t    range      = 0;

    uint32_t    slot       = atomic_load(&gSlot);
    uint32_t    location   = atomic_load(&gLocation);
    uint32_t    variation  = gPatchDescr[slot].activeVariation;

    // Only fire if we weren't dragging — dial drags are handled in cursor_pos
    if (gParamDragging.active || gModuleDrag.active || gCableDrag.active) {
        return false;
    }
    reset_walk_module();
    tModule     module     = {0};

    while (walk_next_module(&module) && (retVal == false)) {
        if (module.key.slot == slot && (module.key.location == location || module.key.location == locationMorph)) {
            if (module.key.location == locationMorph) {
                if (module.key.index == 1) {  // TODO: See if we can roll count into standard mechanism and pre-create the morph modules - maybe create new types at end of list?
                    paramCount = NUM_MORPHS * 2;
                } else {
                    paramCount = 1;  // TODO: correct value for index, probably through a function which returns correct count
                }
            } else {
                paramCount = module_param_count(module.type);
            }

            // Params — toggles fire on release
            for (int i = 0; (i < paramCount) && (retVal == false); i++) {
                tParam * param = &module.param[variation][i];

                if (within_rectangle(coord, param->rectangle) && mouseButton == mouseButtonLeftUp) {
                    if (module.key.location == locationMorph) {  // TODO: See if we can roll count into standard mechanism and pre-create the morph modules - maybe create new types at end of list?
                        if (i < NUM_MORPHS) {
                            paramType2 = paramType2Dial;
                        } else {
                            paramType2 = paramType2Toggle;
                        }
                    } else {
                        paramType2 = (paramLocationList[param->paramRef].type2);
                    }

                    if (paramType2 == paramType2Menu) {
                        open_toggle_menu(coord, module.key, (uint32_t)i, param->paramRef);
                        retVal = true;
                    } else if (paramType2 == paramType2Toggle) {
                        if (module.key.location == locationMorph) {   // TODO: See if we can roll count into standard mechanism and pre-create the morph modules - maybe create new types at end of list?
                            range = 2;
                        } else {
                            range = paramLocationList[param->paramRef].range;
                        }
                        param->value = (param->value + 1) % range;
                        write_module(module.key, &module);
                        send_param_value(slot, module.key, (uint32_t)i, variation, param->value);
                        retVal       = true;
                    }
                    // Dials: already handled in cursor_pos via gParamDragging
                }
            }

            // Modes — toggle fires on release
            if (retVal == false) {
                for (int i = 0; (i < module.modeCount) && (retVal == false); i++) {
                    tMode * mode = &module.mode[i];

                    if (within_rectangle(coord, module.mode[i].rectangle) && mouseButton == mouseButtonLeftUp) {
                        if (modeLocationList[mode->modeRef].type2 == paramType2Menu) {
                            open_mode_toggle_menu(coord, module.key, (uint32_t)i, mode->modeRef);
                            retVal = true;
                        } else if (modeLocationList[mode->modeRef].type2 == paramType2Toggle) {
                            mode->value = (mode->value + 1) % modeLocationList[mode->modeRef].range;
                            write_module(module.key, &module);
                            send_mode_value(slot, module.key, (uint32_t)i, mode->value);
                            retVal      = true;
                        }
                    }
                }
            }
        }
    }
    finish_walk_module();
    return retVal;
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
    gScrollState.yBarDragging = false;
    gScrollState.xBarDragging = false;
    memset(&gModuleDrag, 0, sizeof(gModuleDrag));
    memset(&gParamDragging, 0, sizeof(gParamDragging));
    memset(&gCableDrag, 0, sizeof(gCableDrag));
    gTempoDragging            = false;
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

static bool input_connector_has_cable(uint32_t slot, uint32_t location,
                                      uint32_t moduleIndex, uint32_t ioCount) {
    tCable cable = {0};
    bool   found = false;

    reset_walk_cable();

    while (walk_next_cable(&cable) && !found) {
        if (  cable.key.slot == slot
           && cable.key.location == location) {
            // Check the TO end (always an input)
            if (  cable.key.moduleToIndex == moduleIndex
               && cable.key.connectorToIoCount == ioCount) {
                found = true;
            }
        }
    }
    finish_walk_cable();

    return found;
}

bool handle_cable_connect(tCoord coord, uint32_t slot, uint32_t location) {
    bool      found      = false;
    int32_t   i          = 0;
    tModule   fromModule = {0};
    tModule   toModule   = {0};
    tCableKey cableKey   = {0};
    tCable    cable      = {0};

    reset_walk_module();

    while (found == false && walk_next_module(&toModule)) {
        if (toModule.key.slot == slot && toModule.key.location == location) {
            for (i = 0; i < module_connector_count(toModule.type); i++) {
                if (within_rectangle(coord, toModule.connector[i].rectangle) == true) {
                    found                                         = true;
                    read_module(gCableDrag.fromModuleKey, &fromModule);
                    set_up_cable_key(&cableKey, &fromModule, &toModule, i);

                    swap_cable_to_from_if_needed(&cableKey, &fromModule, &toModule, i);

                    // Prevent self-connections and invalid connections
                    if (  (cableKey.moduleFromIndex == cableKey.moduleToIndex && gCableDrag.fromConnectorIndex == i)
                       || (  fromModule.connector[gCableDrag.fromConnectorIndex].dir == connectorDirOut
                          && toModule.connector[i].dir == connectorDirOut)) {
                        break;
                    }

                    // Note that this call will walk the cables, which we can't nest
                    if (input_connector_has_cable(slot, location,
                                                  cableKey.moduleToIndex,
                                                  cableKey.connectorToIoCount)) {
                        break;
                    }
                    cable.colour                                  = gCableColour;
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
    }
    finish_walk_module();
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
    tModule      module            = {0};
    int32_t      i                 = 0;
    uint32_t     slot              = atomic_load(&gSlot);
    uint32_t     location          = atomic_load(&gLocation);

    mouseButton = convert_to_mouse_button(button, action);

    get_global_gui_scaled_mouse_coord(&coord);

    if (noteEditDismissed) {
        if (mouseButton == mouseButtonLeftUp) {
            noteEditDismissed = false;
        }
        atomic_store(&gReDraw, true);
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
        atomic_store(&gReDraw, true);
        return;
    }

    if (gPatchSettingsEdit.active) {
        if (mouseButton == mouseButtonLeftUp) {
            if (gContextMenu.active) {
                if (!handle_context_menu_click(coord)) {
                    gContextMenu.active = false;
                }
                atomic_store(&gReDraw, true);
                return;
            }

            if (within_rectangle(coord, gSettingsPanelRects.close)) {
                gPatchSettingsEdit.active = false;
            } else {
                int s = 0;

                for (s = 0; s < 4; s++) {
                    if (within_rectangle(coord, gSettingsPanelRects.midiChan[s])) {
                        open_midi_chan_dropdown(below_rect(gSettingsPanelRects.midiChan[s]), &gSynthSettings.midiChanSlot[s]);
                    }
                }

                if (within_rectangle(coord, gSettingsPanelRects.globalChan)) {
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
        atomic_store(&gReDraw, true);
        return;
    }
    stop_patch_name_editing();
    stop_module_name_editing();
    stop_param_name_editing();

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
                    module.key.location      = locationMorph;
                    module.key.slot          = slot;
                    module.key.index         = PATCH_VOLUME;
                    gParamDragging.moduleKey = module.key;
                    gParamDragging.type3     = paramType3Param;
                    gParamDragging.param     = VOLUME_LEVEL;
                    gParamDragging.active    = true;
                    found                    = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarTempoDialId].rectangle)) {
                    gTempoDragging = true;
                    found          = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarClockRunStopId].rectangle)) {
                    running = !atomic_load(&gMasterClockRunning);
                    atomic_store(&gMasterClockRunning, (uint8_t)running);
                    send_master_clock_run((uint32_t)running);
                    atomic_store(&gReDraw, true);
                    found   = true;
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

                        atomic_store(&gReDraw, true);
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
                for (i = 0; i < cableColourMax; i++) {
                    if (within_rectangle(coord, gTopbarControls[(int)topbarCableColourSelect0Id + i].rectangle)) {
                        gCableColour = i;
                        atomic_store(&gReDraw, true);
                        found        = true;
                        break;
                    }
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarHideAllCablesId].rectangle)) {
                    int             colour         = 0;
                    int             visibleCount   = 0;

                    for (int colour = 0; colour < cableColourMax; colour++) {
                        if (gPatchDescr[slot].visible[colour]) {
                            visibleCount++;
                        }
                    }

                    if (visibleCount == 0) {
                        for (colour = 0; colour < cableColourMax; colour++) {
                            gPatchDescr[slot].visible[colour] = 1;
                        }
                    } else {
                        for (colour = 0; colour < cableColourMax; colour++) {
                            gPatchDescr[slot].visible[colour] = 0;
                        }
                    }
                    atomic_store(&gReDraw, true);
                    tMessageContent messageContent = {0};
                    messageContent.cmd  = eMsgCmdWritePatchDescr;
                    messageContent.slot = slot;
                    msg_send(&gCommandQueue, &messageContent);
                    found               = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarTransparentCablesId].rectangle)) {
                    bool current = atomic_load(&gCablesTransparent);
                    atomic_store(&gCablesTransparent, !current);
                    atomic_store(&gReDraw, true);
                    found = true;
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
                if (within_rectangle(coord, gTopbarControls[topbarSettingsId].rectangle)) {
                    gPatchSettingsEdit.active = true;
                    gPatchSettingsEdit.slot   = slot;
                    found                     = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gTopbarControls[topbarPatchNameId].rectangle)) {
                    gPatchNameEdit.active = true;
                    gPatchNameEdit.slot   = slot;
                    patch_name_get(slot, gPatchNameEdit.buffer, sizeof(gPatchNameEdit.buffer));
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
            reset_walk_module();

            while (found == false && walk_next_module(&module)) {
                if (module.key.slot == slot && module.key.location == location) {
                    for (i = 0; i < module_connector_count(module.type); i++) {
                        if (within_rectangle(coord, module.connector[i].rectangle)) {
                            open_connector_context_menu(coord, module.key, i);
                            found = true;
                            break;
                        }
                    }

                    if (found == false) {
                        uint32_t paramCount = module_param_count(module.type);
                        uint32_t variation  = gPatchDescr[slot].activeVariation;

                        for (uint32_t p = 0; p < paramCount && !found; p++) {
                            if (within_rectangle(coord, module.param[variation][p].rectangle)) {
                                open_param_context_menu(coord, module.key, p);
                                found = true;
                            }
                        }
                    }

                    if (found == false) {
                        if (within_rectangle(coord, module.rectangle)) {
                            open_module_context_menu(coord, module.key);
                            found = true;
                        }
                    }
                }
            }
            finish_walk_module();

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
    atomic_store(&gReDraw, true);
}

void cursor_pos(GLFWwindow * window, double xCoord, double yCoord) {
    tCoord          coord          = {0};
    double          angle          = 0.0;
    uint32_t        range          = 0;
    uint32_t        value          = 0;
    tModule         module         = {0};
    tMessageContent messageContent = {0};
    bool            noAction       = false; // unused — kept to avoid restructuring the else-chain below
    tParamType2     paramType2     = paramType2Dial;
    tParamType1     paramType1     = paramType1CommonDial;
    uint32_t        slot           = atomic_load(&gSlot);
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
        angle = calculate_mouse_angle((tCoord){x, y}, gTopbarControls[topbarTempoDialId].rectangle);
        value = angle_to_value(angle, 241);

        if (atomic_load(&gMasterClock) != value) {
            atomic_store(&gMasterClock, (uint8_t)value);
            send_master_clock_bpm(value);
        }
    } else if (gParamDragging.active == true) {
        read_module(gParamDragging.moduleKey, &module);

        // TODO: Think about if we need to check key's slot
        switch (gParamDragging.type3) {
            case paramType3Param:

                if (module.key.location == locationMorph) {
                    paramType2 = paramType2Dial;
                } else {
                    paramType2 = paramLocationList[module.param[variation][gParamDragging.param].paramRef].type2;
                }

                if (paramType2 == paramType2Dial) {
                    if (module.key.location == locationMorph) {
                        range      = 128;
                        paramType1 = paramType1CommonDial;
                    } else {
                        range      = paramLocationList[module.param[variation][gParamDragging.param].paramRef].range;
                        paramType1 = paramLocationList[module.param[variation][gParamDragging.param].paramRef].type1;
                    }

                    if (paramType1 == paramType1Slider) {
                        tRectangle rect     = module.param[variation][gParamDragging.param].rectangle;
                        double     fraction = (rect.coord.y + rect.size.h - y) / rect.size.h;

                        if (fraction < 0.0) {
                            fraction = 0.0;
                        }

                        if (fraction > 1.0) {
                            fraction = 1.0;
                        }
                        value = (uint32_t)round(fraction * (double)(range - 1));
                    } else {
                        angle = calculate_mouse_angle((tCoord){x, y}, module.param[variation][gParamDragging.param].rectangle);                                                            // possible add half size
                        value = angle_to_value(angle, range);
                    }

                    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) != GLFW_PRESS) {
                        if (module.param[variation][gParamDragging.param].value != value) {
                            module.param[variation][gParamDragging.param].value = value;

                            write_module(gParamDragging.moduleKey, &module);         // Write new value into parameter

                            send_param_value(slot, gParamDragging.moduleKey, gParamDragging.param, variation, value);
                        }
                    } else {
                        if (module.param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] != value) {
                            if (value >= module.param[variation][gParamDragging.param].value) {
                                module.param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] = value - module.param[variation][gParamDragging.param].value;
                            } else {
                                module.param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] = 256 - (module.param[variation][gParamDragging.param].value - value);
                            }
                            write_module(gParamDragging.moduleKey, &module);         // Write new value into parameter
                            LOG_DEBUG("Write to module %u variation %u\n", module.key.index, variation);

                            messageContent.cmd                       = eMsgCmdSetParamMorph;
                            messageContent.slot                      = slot;
                            messageContent.paramMorphData.moduleKey  = module.key;
                            messageContent.paramMorphData.param      = gParamDragging.param;
                            messageContent.paramMorphData.paramMorph = gMorphGroupFocus;
                            messageContent.paramMorphData.value      = module.param[variation][gParamDragging.param].morphRange[gMorphGroupFocus];
                            messageContent.paramMorphData.negative   = 0;
                            messageContent.paramMorphData.variation  = variation;
                            msg_send(&gCommandQueue, &messageContent);
                        }
                    }
                }
                break;
            case paramType3Mode:

                // Not currently being used for anything, but could ultimately be. In some cases, could be better as a drop down menu
                if (modeLocationList[module.mode[gParamDragging.mode].modeRef].type2 == paramType2Dial) {
                    angle = calculate_mouse_angle((tCoord){x, y}, module.mode[gParamDragging.mode].rectangle);                                                            // possible add half size
                    value = angle_to_value(angle, modeLocationList[module.mode[gParamDragging.mode].modeRef].range);

                    if (module.mode[gParamDragging.mode].value != value) {
                        module.mode[gParamDragging.mode].value = value;

                        write_module(gParamDragging.moduleKey, &module);         // Write new value into parameter

                        send_mode_value(slot, gParamDragging.moduleKey, gParamDragging.mode, value);
                    }
                }
                break;
        }
    } else if (gModuleDrag.active == true) {
        read_module(gModuleDrag.moduleKey, &module);

        convert_mouse_coord_to_module_column_row(&module.column, &module.row, (tCoord){x, y});

        if (module.row > 127) {
            module.row = 127;
        }

        if (module.column > 127) {
            module.column = 127;
        }
        write_module(gModuleDrag.moduleKey, &module);
        adjust_scroll_for_drag();
    } else if (gCableDrag.active == true) {
        convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, (tCoord){x - scale_from_percent(CONNECTOR_SIZE / 2.0), y - scale_from_percent(CONNECTOR_SIZE / 2.0)});  // SOMETHING NOT RIGHT HERE
        adjust_scroll_for_drag();
    } else if (gContextMenu.active == true) {
        // Dummy
    } else {
        tModule  hoverModule = {0};
        bool     hoverValid  = false;
        uint32_t hoverSlot   = atomic_load(&gSlot);
        uint32_t hoverLoc    = atomic_load(&gLocation);

        reset_walk_module();

        do {
            hoverValid = walk_next_module(&hoverModule);

            if (hoverValid && hoverModule.key.slot == hoverSlot && hoverModule.key.location == hoverLoc) {
                for (int i = 0; i < (int)module_connector_count(hoverModule.type); i++) {
                    if (within_rectangle(coord, hoverModule.connector[i].rectangle)) {
                        gHoverConnector.active      = true;
                        gHoverConnector.slot        = hoverSlot;
                        gHoverConnector.location    = hoverLoc;
                        gHoverConnector.moduleIndex = hoverModule.key.index;
                        gHoverConnector.ioCount     = (uint32_t)find_io_count_from_index(&hoverModule, hoverModule.connector[i].dir, i);
                        gHoverConnector.dir         = hoverModule.connector[i].dir;
                        break;
                    }
                }
            }

            if (gHoverConnector.active) {
                break;
            }
        } while (hoverValid);

        finish_walk_module();
        noAction = true;
    }
    // Limit re-draw/render if nothing's happened
    // if (noAction == false) {   // Used to have this check, TODO - see if there's a way to not redraw on every move
    atomic_store(&gReDraw, true);
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

    atomic_store(&gReDraw, true);
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
    LOG_DEBUG("char=%d\n", value);
    atomic_store(&gReDraw, true);
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
        atomic_store(&gReDraw, true);
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
                patch_name_set(gPatchNameEdit.slot, gPatchNameEdit.buffer);

                tMessageContent messageContent = {0};
                messageContent.cmd    = eMsgCmdSetPatchName;
                messageContent.slot   = gPatchNameEdit.slot;
                patch_name_get(gPatchNameEdit.slot, messageContent.patchName.name, sizeof(messageContent.patchName.name));

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
                tModule module = {0};

                gModuleNameEdit.active = false;
                module.key             = gModuleNameEdit.moduleKey;

                if (read_module(module.key, &module) == true) {
                    tMessageContent msg = {0};

                    strncpy(module.name, gModuleNameEdit.buffer, sizeof(module.name));
                    module.name[sizeof(module.name) - 1] = '\0';
                    write_module(module.key, &module);

                    msg.cmd                              = eMsgCmdSetModuleLabel;
                    msg.slot                             = gModuleNameEdit.moduleKey.slot;
                    msg.moduleLabelData.moduleKey        = gModuleNameEdit.moduleKey;
                    strncpy(msg.moduleLabelData.name, gModuleNameEdit.buffer, CLAVIA_NAME_SIZE);
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
                tModule module = {0};

                gParamNameEdit.active = false;

                if (read_module(gParamNameEdit.moduleKey, &module) == true) {
                    tMessageContent msg = {0};
                    uint32_t        pi  = gParamNameEdit.paramIndex;

                    module.paramNameSet[pi][0]                        = true;
                    strncpy(module.paramName[pi][0], gParamNameEdit.buffer, PROTOCOL_PARAM_NAME_SIZE);
                    module.paramName[pi][0][PROTOCOL_PARAM_NAME_SIZE] = '\0';
                    module.paramNumLabels[pi]                         = 1;
                    write_module(gParamNameEdit.moduleKey, &module);

                    msg.cmd                                           = eMsgCmdSetParamLabel;
                    msg.slot                                          = gParamNameEdit.moduleKey.slot;
                    msg.paramLabelData.moduleKey                      = gParamNameEdit.moduleKey;
                    msg.paramLabelData.paramIndex                     = pi;
                    strncpy(msg.paramLabelData.name, gParamNameEdit.buffer, PROTOCOL_PARAM_NAME_SIZE);
                    msg_send(&gCommandQueue, &msg);
                }
            } else if (key == GLFW_KEY_ESCAPE) {
                gParamNameEdit.active = false;  // discard
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
    atomic_store(&gReDraw, true);
}

#ifdef __cplusplus
}
#endif
