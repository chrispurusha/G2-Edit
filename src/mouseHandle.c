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
#include "utilsGraphics.h"
#include "mouseHandle.h"
#include "graphics.h"
#include "globalVars.h"

void open_patch_type_context_menu(tCoord coord);

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

void update_module_up_rates(void) {
    tModule  module      = {0};
    uint32_t slot        = atomic_load(&gSlot); // TODO: Might need to pass this in as a parameter
    uint32_t location    = atomic_load(&gLocation);

    // Step 1 - initialise the old and new fields
    reset_walk_module();

    while (walk_next_module(&module)) {
        if (module.key.slot == slot && module.key.location == location) {
            module.newUpRate = 0;

            write_module(module.key, &module);
        }
    }
    finish_walk_module();

    // Step - run through cables and see if to module needs up-rating

    bool     changesMade = false;

    do {
        changesMade = false;
        tCable  cable      = {0};
        tModule fromModule = {0};
        tModule toModule   = {0};

        reset_walk_cable();

        while (walk_next_cable(&cable)) {
            tConnectorDir fromConnector = connectorDirOut;
            int           fromConnIndex = -1;
            int           toConnIndex   = -1;
            tModuleKey    fromModuleKey = {cable.key.slot, cable.key.location, cable.key.moduleFromIndex};
            tModuleKey    toModuleKey   = {cable.key.slot, cable.key.location, cable.key.moduleToIndex};

            if ((fromModuleKey.slot == slot) && (toModuleKey.slot == slot) && (fromModuleKey.location == location) && (toModuleKey.location == location)) {
                if (read_module(fromModuleKey, &fromModule) && read_module(toModuleKey, &toModule)) {
                    if (cable.key.linkType == cableLinkTypeFromInput) {
                        fromConnector = connectorDirIn;
                    } else {
                        fromConnector = connectorDirOut;
                    }
                    fromConnIndex = find_index_from_io_count(&fromModule, fromConnector, cable.key.connectorFromIoCount);
                    toConnIndex   = find_index_from_io_count(&toModule, connectorDirIn, cable.key.connectorToIoCount);

                    if ((fromConnIndex != -1) && (toConnIndex != -1)) {
                        if (toModule.newUpRate == 0) {
                            if (fromModule.newUpRate == 1) {
                                //LOG_DEBUG("From module is uprated from = %u %u\n", fromModule.key.location, fromModule.key.index);
                                toModule.newUpRate = 1;
                                write_module(toModuleKey, &toModule);
                                changesMade        = true;
                            } else if ((fromModule.connector[fromConnIndex].type == connectorTypeAudio) && (toModule.connector[toConnIndex].type != connectorTypeAudio)) {
                                //LOG_DEBUG("From module from is audio and to is control\n");
                                toModule.newUpRate = 1;
                                write_module(toModuleKey, &toModule);
                                changesMade        = true;
                            }
                        }
                    }
                }
            }
        }
        finish_walk_cable();
    } while (changesMade);

    // Step 3 - write any changes to database and device
    reset_walk_module();

    while (walk_next_module(&module)) {
        if ((module.key.slot == slot) && (module.key.location == location)) {
            if (module.newUpRate != module.upRate) {
                module.upRate                       = module.newUpRate;

                write_module(module.key, &module);

                tMessageContent messageContent = {0};
                messageContent.cmd                  = eMsgCmdSetModuleUpRate;
                messageContent.slot                 = slot;
                messageContent.moduleData.moduleKey = module.key;
                messageContent.moduleData.upRate    = module.upRate;
                msg_send(&gCommandQueue, &messageContent);
            }
        }
    }
    finish_walk_module();
}

void send_module_move_msg(tModule * module) {
    tMessageContent messageContent = {0};
    uint32_t        slot           = atomic_load(&gSlot);

    messageContent.cmd                  = eMsgCmdMoveModule;
    messageContent.slot                 = slot;
    messageContent.moduleData.moduleKey = module->key;
    messageContent.moduleData.row       = module->row;
    messageContent.moduleData.column    = module->column;
    msg_send(&gCommandQueue, &messageContent);
}

static void send_param_value(uint32_t slot, tModuleKey moduleKey, uint32_t paramIdx, uint32_t variation, uint32_t value) {
    tMessageContent msg = {0};

    msg.cmd                 = eMsgCmdSetValue;
    msg.slot                = slot;
    msg.paramData.moduleKey = moduleKey;
    msg.paramData.param     = paramIdx;
    msg.paramData.variation = variation;
    msg.paramData.value     = value;
    msg_send(&gCommandQueue, &msg);
}

static void send_mode_value(uint32_t slot, tModuleKey moduleKey, uint32_t modeIdx, uint32_t value) {
    tMessageContent msg = {0};

    msg.cmd                = eMsgCmdSetMode;
    msg.slot               = slot;
    msg.modeData.moduleKey = moduleKey;
    msg.modeData.mode      = modeIdx;
    msg.modeData.value     = value;
    msg_send(&gCommandQueue, &msg);
}

void init_params_on_module(tModule * module, uint32_t location, uint32_t variation) {
    uint32_t paramListIndex = 0;
    uint32_t paramIndex     = 0;
    uint32_t numParams      = module_param_count(module->type);
    bool     anyParamSet    = false;
    uint32_t slot           = atomic_load(&gSlot);

    if (location != atomic_load(&gLocation)) {
        return;
    }

    for (paramListIndex = 0; paramListIndex < array_size_param_location_list(); paramListIndex++) {
        if (paramLocationList[paramListIndex].moduleType == module->type) {
            module->param[variation][paramIndex].value = paramLocationList[paramListIndex].defaultValue;
            anyParamSet                                = true;

            send_param_value(slot, module->key, paramIndex, variation, module->param[variation][paramIndex].value);

            paramIndex++;

            if (paramIndex >= numParams) {
                break;
            }
        }
    }

    // Write the module once after all params are updated, not once per param
    if (anyParamSet) {
        write_module(module->key, module);
    }
}

void init_params_on_module_all_variations(tModule * module, uint32_t location) {
    // Happens on module creation
    if (location != atomic_load(&gLocation)) {
        return;
    }

    for (uint32_t variation = 0; variation < NUM_VARIATIONS; variation++) {
        init_params_on_module(module, location, variation);
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

void handle_button(tButtonId buttonId) {
    uint32_t slot      = atomic_load(&gSlot);
    uint32_t location  = atomic_load(&gLocation);
    uint32_t variation = gPatchDescr[slot].activeVariation;

    switch (buttonId) {
        case vaButtonId:
        {
            atomic_store(&gLocation, locationVa);
            set_exclusive_button_highlight(vaButtonId, fxButtonId, buttonId);
            set_x_scroll_bar(0); // or different scroll positions for va and fx!?
            set_y_scroll_bar(0);
            break;
        }
        case fxButtonId:
        {
            atomic_store(&gLocation, locationFx);
            set_exclusive_button_highlight(vaButtonId, fxButtonId, buttonId);
            set_x_scroll_bar(0);
            set_y_scroll_bar(0);
            break;
        }
        case openReadFileButtonId:
        {
            gShowOpenFileReadDialogue = true;
            break;
        }
        case openWriteFileButtonId:
        {
            gShowOpenFileWriteDialogue = true;
            break;
        }
        case variation1ButtonId:
        case variation2ButtonId:
        case variation3ButtonId:
        case variation4ButtonId:
        case variation5ButtonId:
        case variation6ButtonId:
        case variation7ButtonId:
        case variation8ButtonId:
        case variationInitButtonId:
        {
            uint32_t        variation      = (uint32_t)buttonId - (uint32_t)variation1ButtonId;

            gPatchDescr[slot].activeVariation      = variation;

            set_exclusive_button_highlight(variation1ButtonId, variationInitButtonId, buttonId);

            tMessageContent messageContent = {0};
            messageContent.cmd                     = eMsgCmdSelectVariation;
            messageContent.slot                    = slot;
            messageContent.variationData.variation = variation;
            msg_send(&gCommandQueue, &messageContent);

            break;
        }
        //case initParamsButtonId:   // TODO - might add a set to defaults button
        //{
        //    tModule module      = {0};
        //    bool    validModule = false;
//
        //    reset_walk_module();
//
        //    do {
        //        validModule = walk_next_module(&module);

        //       if (validModule) {
        //           init_params_on_module(&module, location, variation); // TODO: take init value from the 9th (init) variation, or at least check our init values are the same
        //       }
        //   } while (validModule);
//
        //    finish_walk_module();

        //    break;
        //}
        case slotAButtonId:
        case slotBButtonId:
        case slotCButtonId:
        case slotDButtonId:
        {
            uint32_t        slot           = (uint32_t)buttonId - (uint32_t)slotAButtonId;

            atomic_store(&gSlot, slot);

            tMessageContent messageContent = {0};
            messageContent.cmd           = eMsgCmdSelectSlot;
            messageContent.slot          = slot;
            messageContent.slotData.slot = slot;
            msg_send(&gCommandQueue, &messageContent);

            set_exclusive_button_highlight(slotAButtonId, slotDButtonId, buttonId);
            set_exclusive_button_highlight(variation1ButtonId, variationInitButtonId,
                                           (tButtonId)((uint32_t)variation1ButtonId + gPatchDescr[slot].activeVariation));
            break;
        }
        case initPatchId:
        {
            init_patch(slot);

            //gMainButtonArray[buttonId].backgroundColour   = (tRgb)RGB_GREEN_ON;
            tMessageContent messageContent = {0};
            messageContent.cmd  = eMsgCmdWritePatch;
            messageContent.slot = slot;
            msg_send(&gCommandQueue, &messageContent);
            break;
        }
    }
}

void shift_modules_down(tModuleKey key) {   // TODO: Deal with modules already on last row!
    tModule  module            = {0};
    tModule  walk              = {0};
    bool     doDrop            = false;
    uint32_t rowAndBelowToDrop = 0;
    uint32_t dropAmount        = 0;
    bool     moduleRePosition  = false;

    if (read_module(key, &module) == false) {
        return;
    }
    // First step - if moved module lands on exisitng module after existing module's first row, drop it down
    reset_walk_module();

    while (walk_next_module(&walk)) {
        if ((walk.column == module.column) && (walk.key.slot == key.slot) && (walk.key.location == key.location)) {
            if (walk.key.index != key.index) {
                if ((module.row > walk.row) && (module.row < walk.row + gModuleProperties[walk.type].height)) {
                    module.row       = walk.row + gModuleProperties[walk.type].height;
                    write_module(module.key, &module);
                    send_module_move_msg(&module);
                    moduleRePosition = true;
                    break;
                }
            }
        }
    }
    finish_walk_module();

    if (moduleRePosition == false) {
        send_module_move_msg(&module);
    }
    // If a module is on-top of the new module (within it's area), drop it
    reset_walk_module();

    while (walk_next_module(&walk)) {
        if ((walk.column == module.column) && (walk.key.slot == key.slot) && (walk.key.location == key.location)) {
            if (walk.key.index != key.index) {
                if ((walk.row >= module.row) && (walk.row < module.row + gModuleProperties[module.type].height)) {
                    rowAndBelowToDrop = walk.row;
                    dropAmount        = (module.row + gModuleProperties[module.type].height) - walk.row;
                    doDrop            = true;
                    break;
                }
            }
        }
    }
    finish_walk_module();

    // Drop subsequent modules by the same amount used in walk section 2
    if (doDrop == true) {
        reset_walk_module();

        while (walk_next_module(&walk)) {
            if ((walk.column == module.column) && (walk.key.slot == key.slot) && (walk.key.location == key.location)) {
                if (walk.key.index != key.index) {
                    if (walk.row >= rowAndBelowToDrop) {
                        walk.row += dropAmount;

                        if (walk.row > MAX_ROWS) {
                            walk.row = MAX_ROWS;
                            // TODO: flag up the clash!
                        }
                        write_module(walk.key, &walk);
                        send_module_move_msg(&walk);
                    }
                }
            }
        }
        finish_walk_module();
    }
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

void menu_action_delete_cable(int index) {
    tModule  module     = {0};
    tCable   walk       = {0};
    int      outIndex   = -1;
    int      inIndex    = -1;
    bool     deleteWalk = false;
    uint32_t slot       = atomic_load(&gSlot);
    uint32_t location   = atomic_load(&gLocation);

    if ((gContextMenu.moduleKey.slot == slot) && (gContextMenu.moduleKey.location == location)) { // TODO - probably don't need to do this check?
        read_module(gContextMenu.moduleKey, &module);

        reset_walk_cable();

        while (walk_next_cable(&walk)) {
            deleteWalk = false;

            if (walk.key.slot == slot && walk.key.location == gContextMenu.moduleKey.location) {
                switch (module.connector[gContextMenu.connectorIndex].dir) {
                    case connectorDirOut:
                        outIndex = find_io_count_from_index(&module, connectorDirOut, gContextMenu.connectorIndex);
                        break;
                    case connectorDirIn:
                        inIndex  = find_io_count_from_index(&module, connectorDirIn, gContextMenu.connectorIndex);
                        break;
                }

                if (walk.key.moduleFromIndex == gContextMenu.moduleKey.index) {
                    if (walk.key.linkType == cableLinkTypeFromInput) {
                        if (walk.key.connectorFromIoCount == inIndex) {
                            deleteWalk = true;
                        }
                    } else if (walk.key.linkType == cableLinkTypeFromOutput) {
                        if (walk.key.connectorFromIoCount == outIndex) {
                            deleteWalk = true;
                        }
                    }
                }

                if (walk.key.moduleToIndex == gContextMenu.moduleKey.index) {
                    if (walk.key.connectorToIoCount == inIndex) {
                        deleteWalk = true;
                    }
                }

                if (deleteWalk == true) {
                    tMessageContent messageContent = {0};

                    messageContent.cmd                            = eMsgCmdDeleteCable;
                    messageContent.slot                           = slot;
                    messageContent.cableData.location             = location;
                    messageContent.cableData.moduleFromIndex      = walk.key.moduleFromIndex;
                    messageContent.cableData.connectorFromIoIndex = walk.key.connectorFromIoCount;
                    messageContent.cableData.moduleToIndex        = walk.key.moduleToIndex;
                    messageContent.cableData.connectorToIoIndex   = walk.key.connectorToIoCount;
                    messageContent.cableData.linkType             = walk.key.linkType;

                    msg_send(&gCommandQueue, &messageContent);

                    delete_cable(walk.key);
                }
            }
        }
        finish_walk_cable();
        update_module_up_rates();
    }
}

void menu_action_delete_module(int index) {
    tModule         module         = {0};
    tCable          walk           = {0};
    bool            deleteWalk     = false;
    uint32_t        slot           = atomic_load(&gSlot);
    uint32_t        location       = atomic_load(&gLocation);
    tMessageContent messageContent = {0};

    if (gContextMenu.moduleKey.slot == slot && gContextMenu.moduleKey.location == location) { // TODO - probably don't need to do this check?
        read_module(gContextMenu.moduleKey, &module);

        reset_walk_cable();

        while (walk_next_cable(&walk)) {
            deleteWalk = false;

            if (walk.key.slot == slot && walk.key.location == gContextMenu.moduleKey.location) {
                if (walk.key.moduleFromIndex == gContextMenu.moduleKey.index) {
                    deleteWalk = true;
                } else if (walk.key.moduleToIndex == gContextMenu.moduleKey.index) {
                    deleteWalk = true;
                }

                if (deleteWalk == true) {
                    memset(&messageContent, 0, sizeof(messageContent));
                    messageContent.cmd                            = eMsgCmdDeleteCable;
                    messageContent.slot                           = slot;
                    messageContent.cableData.location             = location;
                    messageContent.cableData.moduleFromIndex      = walk.key.moduleFromIndex;
                    messageContent.cableData.connectorFromIoIndex = walk.key.connectorFromIoCount;
                    messageContent.cableData.moduleToIndex        = walk.key.moduleToIndex;
                    messageContent.cableData.connectorToIoIndex   = walk.key.connectorToIoCount;
                    messageContent.cableData.linkType             = walk.key.linkType;

                    msg_send(&gCommandQueue, &messageContent);

                    delete_cable(walk.key);
                }
            }
        }
        finish_walk_cable();

        memset(&messageContent, 0, sizeof(messageContent));
        messageContent.cmd                  = eMsgCmdDeleteModule;
        messageContent.slot                 = slot;
        messageContent.moduleData.moduleKey = module.key;

        msg_send(&gCommandQueue, &messageContent);

        delete_module(gContextMenu.moduleKey);

        update_module_up_rates();
    }
}

static void action_rename_module(int index) {
    tModule module = {0};

    if (read_module(gContextMenu.moduleKey, &module)) {
        gModuleNameEdit.active                   = true;
        gModuleNameEdit.moduleKey                = gContextMenu.moduleKey;
        strncpy(gModuleNameEdit.buffer, module.name, CLAVIA_NAME_SIZE);
        gModuleNameEdit.buffer[CLAVIA_NAME_SIZE] = '\0';
    }
    gContextMenu.active = false;
    atomic_store(&gReDraw, true);
}

int32_t find_unique_module_id(uint32_t location) {
    tModuleKey key    = {0};
    tModule    module = {0};
    int32_t    i      = 0;
    uint32_t   slot   = atomic_load(&gSlot);

    key.slot     = slot; // TODO: Might need to pass this in as a parameter?
    key.location = location;

    // Note that index 0 is *not* valid
    for (i = 1; i <= 255; i++) {
        key.index = i;

        if (read_module(key, &module) == false) {
            return (int32_t)i;
        }
    }

    return -1;
}

void convert_mouse_coord_to_module_column_row(uint32_t * column, uint32_t * row, tCoord coord) {
    double     val  = 0.0;
    tRectangle area = module_area();

    if (column != NULL) {
        val     = coord.x - area.coord.x;
        val    += calc_scroll_x();
        val    /= MODULE_X_SPAN;
        val    /= get_zoom_factor();

        if (val < 0.0) {
            val = 0.0;
        }
        *column = floor(val);
    }

    if (row != NULL) {
        val  = coord.y - area.coord.y;
        val += calc_scroll_y();
        val /= MODULE_Y_SPAN;
        val /= get_zoom_factor();

        if (val < 0.0) {
            val = 0.0;
        }
        *row = floor(val);
    }
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

static void clamp_menu_to_screen(tMenuItem * items, uint32_t columns) {
    int      count        = 0;
    double   renderHeight = get_render_height() / gGlobalGuiScale;
    double   cellH        = STANDARD_TEXT_HEIGHT + (5 * 2);
    uint32_t cols         = (columns > 1) ? columns : 1;

    while (items[count].label != NULL) {
        count++;
    }
    int      rows         = (count + (int)cols - 1) / (int)cols;
    double   menuHeight   = rows * cellH;

    if (gContextMenu.coord.y + menuHeight > (renderHeight - SCROLLBAR_WIDTH)) {
        gContextMenu.coord.y = (renderHeight - SCROLLBAR_WIDTH) - menuHeight;
    }
}

static void open_context_menu(tCoord coord, tMenuItem * items, uint32_t columns, double cellWidth) {
    gContextMenu.coord     = coord;
    gContextMenu.items     = items;
    gContextMenu.columns   = columns;
    gContextMenu.cellWidth = cellWidth;
    gContextMenu.active    = true;
    clamp_menu_to_screen(items, (columns > 1) ? columns : 1);
}

void menu_action_create(int index) {
    uint32_t slot     = atomic_load(&gSlot);
    uint32_t location = atomic_load(&gLocation);

    if (gContextMenu.items[index].param != 0) { //TODO - should this be based on gContextMenu.items[index].subMenu being NULL or not?
        tModule         module         = {0};
        tMessageContent messageContent = {0};
        int32_t         uniqueIndex    = 0;

        module.key.slot     = slot; // TODO: Possibly pass this into find_unique...
        module.key.location = location;
        uniqueIndex         = find_unique_module_id(module.key.location);

        if (uniqueIndex >= 0) {
            module.key.index                                                           = (uint32_t)uniqueIndex;
            module.type                                                                = (tModuleType)gContextMenu.items[index].param;
            convert_mouse_coord_to_module_column_row(&module.column, &module.row, gContextMenu.originCoord);

            strncpy(module.name, gModuleProperties[module.type].name, sizeof(module.name));
            module.name[sizeof(module.name) - 1]                                       = '\0';

            messageContent.cmd                                                         = eMsgCmdWriteModule;
            messageContent.slot                                                        = slot;
            messageContent.moduleData.moduleKey                                        = module.key;
            messageContent.moduleData.type                                             = module.type;
            messageContent.moduleData.row                                              = module.row;
            messageContent.moduleData.column                                           = module.column;
            messageContent.moduleData.colour                                           = module.colour;
            messageContent.moduleData.upRate                                           = module.upRate;
            messageContent.moduleData.isLed                                            = module.isLed;
            messageContent.moduleData.unknown1                                         = module.unknown1;
            messageContent.moduleData.modeCount                                        = module_mode_count(module.type);

            for (int i = 0; i < module_mode_count(module.type); i++) {
                messageContent.moduleData.mode[i] = module.mode[i].value;
            }

            strncpy(messageContent.moduleData.name, module.name, sizeof(messageContent.moduleData.name));
            messageContent.moduleData.name[sizeof(messageContent.moduleData.name) - 1] = '\0';

            msg_send(&gCommandQueue, &messageContent);

            write_module(module.key, &module);

            init_params_on_module_all_variations(&module, location);

            shift_modules_down(module.key);
        }
    } else {
        tMenuItem * subMenu = gContextMenu.items[index].subMenu;

        if (subMenu != NULL) {
            open_context_menu(gContextMenu.coord, subMenu, 0, 0.0);
        }
    }
}

void action_set_module_colour(int index) {
    if (gContextMenu.items[index].subMenu == NULL) {
        tModule         module         = {0};
        tMessageContent messageContent = {0};

        read_module(gContextMenu.moduleKey, &module);

        module.colour                             = gContextMenu.items[index].param;

        write_module(module.key, &module);

        messageContent.cmd                        = eMsgCmdSetModuleColour;
        messageContent.slot                       = module.key.slot; // TODO - possible better method for other functions, since slot is part of module key - might not even need this in the type
        messageContent.moduleColourData.moduleKey = module.key;
        messageContent.moduleColourData.colour    = module.colour;

        msg_send(&gCommandQueue, &messageContent);
    } else {
        tMenuItem * subMenu = gContextMenu.items[index].subMenu;

        if (subMenu != NULL) {
            open_context_menu(gContextMenu.coord, subMenu, 6, STANDARD_TEXT_HEIGHT * 2);
        }
    }
}

void open_module_area_context_menu(tCoord coord) {  // TODO: Move these static structures into a module like globalVars.c
    static tMenuItem ioMenuItems[]   = {
        {"2 Outputs",           RGB_GREY_3, menu_action_create, moduleType2toOut,   NULL},
        {"4 Outputs",           RGB_GREY_3, menu_action_create, moduleType4toOut,   NULL},
        {"2 Inputs",            RGB_GREY_3, menu_action_create, moduleType2toIn,    NULL},
        {"4 Inputs",            RGB_GREY_3, menu_action_create, moduleType4toIn,    NULL},
        {"FX Input",            RGB_GREY_3, menu_action_create, moduleTypeFxtoIn,   NULL},
        {"Keyboard",            RGB_GREY_3, menu_action_create, moduleTypeKeyboard, NULL},
        {"Monophonic Keyboard", RGB_GREY_3, menu_action_create, moduleTypeMonoKey,  NULL},
        {"Device",              RGB_GREY_3, menu_action_create, moduleTypeDevice,   NULL},
        {"Status",              RGB_GREY_3, menu_action_create, moduleTypeStatus,   NULL},
        {"Note Detector",       RGB_GREY_3, menu_action_create, moduleTypeNoteDet,  NULL},
        {"Name Bar",            RGB_GREY_3, menu_action_create, moduleTypeName,     NULL},
        {NULL,                  RGB_BLACK,  NULL,                                0, NULL}    // End of menu
    };
    static tMenuItem noteMenuItems[] = {
        {"Note Quantiser",        RGB_GREY_3, menu_action_create, moduleTypeNoteQuant,  NULL},
        {"Key Quantiser",         RGB_GREY_3, menu_action_create, moduleTypeKeyQuant,   NULL},
        {"Partial Quantiser",     RGB_GREY_3, menu_action_create, moduleTypePartQuant,  NULL},
        {"Note Scaler",           RGB_GREY_3, menu_action_create, moduleTypeNoteScaler, NULL},
        {"Glide",                 RGB_GREY_3, menu_action_create, moduleTypeGlide,      NULL},
        {"Pitch Tracker",         RGB_GREY_3, menu_action_create, moduleTypePitchTrack, NULL},
        {"Zero Crossing Counter", RGB_GREY_3, menu_action_create, moduleTypeZeroCnt,    NULL},
        {"Level Scaler",          RGB_GREY_3, menu_action_create, moduleTypeLevScaler,  NULL},
        {NULL,                    RGB_BLACK,  NULL,                                  0, NULL} // End of menu
    };
    static tMenuItem oscMenuItems[]  = {
        {"Osc A",          RGB_GREY_3, menu_action_create, moduleTypeOscA,      NULL},
        {"Osc B",          RGB_GREY_3, menu_action_create, moduleTypeOscB,      NULL},
        {"Osc C",          RGB_GREY_3, menu_action_create, moduleTypeOscC,      NULL},
        {"Osc D",          RGB_GREY_3, menu_action_create, moduleTypeOscD,      NULL},
        {"Osc Phase Mod",  RGB_GREY_3, menu_action_create, moduleTypeOscPM,     NULL},
        {"Osc Shape A",    RGB_GREY_3, menu_action_create, moduleTypeOscShpA,   NULL},
        {"Osc Shape B",    RGB_GREY_3, menu_action_create, moduleTypeOscShpB,   NULL},
        {"Osc Dual",       RGB_GREY_3, menu_action_create, moduleTypeOscDual,   NULL},
        {"Noise Osc",      RGB_GREY_3, menu_action_create, moduleTypeOscNoise,  NULL},
        {"Noise",          RGB_GREY_3, menu_action_create, moduleTypeNoise,     NULL},
        {"Metallic Noise", RGB_GREY_3, menu_action_create, moduleTypeMetNoise,  NULL},
        {"Osc Percussion", RGB_GREY_3, menu_action_create, moduleTypeOscPerc,   NULL},
        {"Drum Synth",     RGB_GREY_3, menu_action_create, moduleTypeDrumSynth, NULL},
        {"Osc String",     RGB_GREY_3, menu_action_create, moduleTypeOscString, NULL},
        {"FM Operator",    RGB_GREY_3, menu_action_create, moduleTypeOperator,  NULL},
        {"DX Router",      RGB_GREY_3, menu_action_create, moduleTypeDXRouter,  NULL},
        {"Driver",         RGB_GREY_3, menu_action_create, moduleTypeDriver,    NULL},
        {"Resonator",      RGB_GREY_3, menu_action_create, moduleTypeResonator, NULL},
        {"Osc Master",     RGB_GREY_3, menu_action_create, moduleTypeOscMaster, NULL},
        {NULL,             RGB_BLACK,  NULL,                                 0, NULL}        // End of menu
    };
    static tMenuItem randomMenuItems[] = {
        {"Random A", RGB_GREY_3, menu_action_create, moduleTypeRandomA, NULL},
        {"Random B", RGB_GREY_3, menu_action_create, moduleTypeRandomB, NULL},
        //{"Random Clock A", RGB_GREY_3, menu_action_create, NULL, NULL},
        //{"Random Clock B", RGB_GREY_3, menu_action_create, NULL, NULL},
        //{"Random Trig", RGB_GREY_3, menu_action_create, NULL, NULL},
        //{"Random Pattern", RGB_GREY_3, menu_action_create, NULL, NULL},
        {NULL,       RGB_BLACK,  NULL,                               0, NULL}               // End of menu
    };

    static tMenuItem lfoMenuItems[]    = {
        {"LFO A",           RGB_GREY_3, menu_action_create, moduleTypeLfoA,    NULL},
        {"LFO B",           RGB_GREY_3, menu_action_create, moduleTypeLfoB,    NULL},
        {"LFO C",           RGB_GREY_3, menu_action_create, moduleTypeLfoC,    NULL},
        {"LFO Shp A",       RGB_GREY_3, menu_action_create, moduleTypeLfoShpA, NULL},
        {"Clock Generator", RGB_GREY_3, menu_action_create, moduleTypeClkGen,  NULL},
        {NULL,              RGB_BLACK,  NULL,                               0, NULL}         // End of menu
    };
    static tMenuItem envMenuItems[]    = {
        {"Envelope ADSR",     RGB_GREY_3, menu_action_create, moduleTypeEnvADSR,  NULL}, // TODO - Match these up with OG edirot
        {"Envelope AHD",      RGB_GREY_3, menu_action_create, moduleTypeEnvAHD,   NULL},
        {"Envelope ADR",      RGB_GREY_3, menu_action_create, moduleTypeEnvADR,   NULL},
        {"Envelop ADDSR",     RGB_GREY_3, menu_action_create, moduleTypeEnvADDSR, NULL},
        {"Envelope H",        RGB_GREY_3, menu_action_create, moduleTypeEnvH,     NULL},
        {"Envelope D",        RGB_GREY_3, menu_action_create, moduleTypeEnvD,     NULL},
        {"Envelope Multi",    RGB_GREY_3, menu_action_create, moduleTypeEnvMulti, NULL},
        {"Envelope Mod AHD",  RGB_GREY_3, menu_action_create, moduleTypeModAHD,   NULL},
        {"Envelope Mod ADSR", RGB_GREY_3, menu_action_create, moduleTypeModADSR,  NULL},
        {NULL,                RGB_BLACK,  NULL,                                0, NULL}      // End of menu
    };
    static tMenuItem filterMenuItems[] = {
        {"LP Filter",      RGB_GREY_3, menu_action_create, moduleTypeFltLP,      NULL},
        {"HP Filter",      RGB_GREY_3, menu_action_create, moduleTypeFltHP,      NULL},
        {"Nord Filter",    RGB_GREY_3, menu_action_create, moduleTypeFltNord,    NULL},
        {"Classic Filter", RGB_GREY_3, menu_action_create, moduleTypeFltClassic, NULL},
        {"Multi Filter",   RGB_GREY_3, menu_action_create, moduleTypeFltMulti,   NULL},
        {"Phase Filter",   RGB_GREY_3, menu_action_create, moduleTypeFltPhase,   NULL},
        {"Comb Filter",    RGB_GREY_3, menu_action_create, moduleTypeFltComb,    NULL},
        {"Static Filter",  RGB_GREY_3, menu_action_create, moduleTypeFltStatic,  NULL},
        {"FltVoice",       RGB_GREY_3, menu_action_create, moduleTypeFltVoice,   NULL},
        {"WahWah",         RGB_GREY_3, menu_action_create, moduleTypeWahWah,     NULL},
        {"Vocoder",        RGB_GREY_3, menu_action_create, moduleTypeVocoder,    NULL},
        {"Eq 2-band",      RGB_GREY_3, menu_action_create, moduleTypeEq2Band,    NULL},
        {"Eq 3-band",      RGB_GREY_3, menu_action_create, moduleTypeEq3band,    NULL},
        {"Eq Peak",        RGB_GREY_3, menu_action_create, moduleTypeEqPeak,     NULL},
        {NULL,             RGB_BLACK,  NULL,                                  0, NULL}        // End of menu
    };
    static tMenuItem delayMenuItems[]  = {
        {"Delay Single A", RGB_GREY_3, menu_action_create, moduleTypeDlySingleA,  NULL},
        {"Delay Single B", RGB_GREY_3, menu_action_create, moduleTypeDlySingleB,  NULL},
        {"Delay Dual",     RGB_GREY_3, menu_action_create, moduleTypeDelayDual,   NULL},
        {"Delay Quad",     RGB_GREY_3, menu_action_create, moduleTypeDelayQuad,   NULL},
        {"Delay A",        RGB_GREY_3, menu_action_create, moduleTypeDelayA,      NULL},
        {"Delay B",        RGB_GREY_3, menu_action_create, moduleTypeDelayB,      NULL},
        {"Delay Stereo",   RGB_GREY_3, menu_action_create, moduleTypeDlyStereo,   NULL},
        {"Delay Clock",    RGB_GREY_3, menu_action_create, moduleTypeDlyClock,    NULL},
        {"Delay Eight",    RGB_GREY_3, menu_action_create, moduleTypeDlyEight,    NULL},
        {"DlyShiftReg",    RGB_GREY_3, menu_action_create, moduleTypeDlyShiftReg, NULL},
        {NULL,             RGB_BLACK,  NULL,                                   0, NULL}        // End of menu
    };


    static tMenuItem levelMenuItems[]  = {
        {"Constant",  RGB_GREY_3, menu_action_create, moduleTypeConstant,  NULL},
        {"ConstSwM",  RGB_GREY_3, menu_action_create, moduleTypeConstSwM,  NULL},
        {"ConstSwT",  RGB_GREY_3, menu_action_create, moduleTypeConstSwT,  NULL},
        {"CompLev",   RGB_GREY_3, menu_action_create, moduleTypeCompLev,   NULL},
        {"CompSig",   RGB_GREY_3, menu_action_create, moduleTypeCompSig,   NULL},
        {"LevAdd",    RGB_GREY_3, menu_action_create, moduleTypeLevAdd,    NULL},
        {"LevAmp",    RGB_GREY_3, menu_action_create, moduleTypeLevAmp,    NULL},
        {"LevConv",   RGB_GREY_3, menu_action_create, moduleTypeLevConv,   NULL},
        {"LevMod",    RGB_GREY_3, menu_action_create, moduleTypeLevMod,    NULL},
        {"LevMult",   RGB_GREY_3, menu_action_create, moduleTypeLevMult,   NULL},
        {"MinMax",    RGB_GREY_3, menu_action_create, moduleTypeMinMax,    NULL},
        {"ModAmt",    RGB_GREY_3, menu_action_create, moduleTypeModAmt,    NULL},
        {"NoiseGate", RGB_GREY_3, menu_action_create, moduleTypeNoiseGate, NULL},
        {"EnvFollow", RGB_GREY_3, menu_action_create, moduleTypeEnvFollow, NULL},
        {NULL,        RGB_BLACK,  NULL,                                 0, NULL}               // End of menu
    };

    static tMenuItem switchMenuItems[] = {
        {"SwOnOffM", RGB_GREY_3, menu_action_create, moduleTypeSwOnOffM,  NULL},
        {"SwOnOffT", RGB_GREY_3, menu_action_create, moduleTypeSwOnOffT,  NULL},
        {"Sw2-1",    RGB_GREY_3, menu_action_create, moduleTypeSw2to1,    NULL},
        {"Sw2-1M",   RGB_GREY_3, menu_action_create, moduleTypeSw2to1,    NULL},
        {"Sw4-1",    RGB_GREY_3, menu_action_create, moduleTypeSw4to1,    NULL},
        {"Sw8-1",    RGB_GREY_3, menu_action_create, moduleTypeSw8to1,    NULL},
        {"Sw1-2",    RGB_GREY_3, menu_action_create, moduleTypeSw1to2,    NULL},
        {"Sw1-2M",   RGB_GREY_3, menu_action_create, moduleTypeSw1to2M,   NULL},
        {"Sw1-4",    RGB_GREY_3, menu_action_create, moduleTypeSw1to4,    NULL},
        {"Sw1-8",    RGB_GREY_3, menu_action_create, moduleTypeSw1to8,    NULL},
        {"ValSw2-1", RGB_GREY_3, menu_action_create, moduleTypeValSw2to1, NULL},
        {"ValSw1-2", RGB_GREY_3, menu_action_create, moduleTypeValSw1to2, NULL},
        {"Mux8-1",   RGB_GREY_3, menu_action_create, moduleTypeMux8to1,   NULL},
        {"Mux1-8",   RGB_GREY_3, menu_action_create, moduleTypeMux1to8,   NULL},
        {"Mux8-1X",  RGB_GREY_3, menu_action_create, moduleTypeMux8to1X,  NULL},
        {"S&H",      RGB_GREY_3, menu_action_create, moduleTypeSandH,     NULL},
        {"T&H",      RGB_GREY_3, menu_action_create, moduleTypeTandH,     NULL},
        {"WindSw",   RGB_GREY_3, menu_action_create, moduleTypeWindSw,    NULL},
        {NULL,       RGB_BLACK,  NULL,                                 0, NULL}              // End of menu
    };
    static tMenuItem seqMenuItems[]    = {
        {"Sequencer Event",      RGB_GREY_3, menu_action_create, moduleTypeSeqEvent, NULL},
        {"Sequencer Values",     RGB_GREY_3, menu_action_create, moduleTypeSeqVal,   NULL},
        {"Sequencer Level",      RGB_GREY_3, menu_action_create, moduleTypeSeqLev,   NULL},
        {"Sequencer Note",       RGB_GREY_3, menu_action_create, moduleTypeSeqNote,  NULL},
        {"Sequencer Controlled", RGB_GREY_3, menu_action_create, moduleTypeSeqCtr,   NULL},
        {NULL,                   RGB_BLACK,  NULL,                                0, NULL}   // End of menu
    };
    static tMenuItem shaperMenuItems[] = {
        {"Saturate",  RGB_GREY_3, menu_action_create, moduleTypeSaturate,  NULL},
        {"Clip",      RGB_GREY_3, menu_action_create, moduleTypeClip,      NULL},
        {"OverDrive", RGB_GREY_3, menu_action_create, moduleTypeOverdrive, NULL},
        {"ShpExp",    RGB_GREY_3, menu_action_create, moduleTypeShpExp,    NULL},
        {"WaveWrap",  RGB_GREY_3, menu_action_create, moduleTypeWaveWrap,  NULL},
        {"ShpStatic", RGB_GREY_3, menu_action_create, moduleTypeShpStatic, NULL},
        {"Rect",      RGB_GREY_3, menu_action_create, moduleTypeRect,      NULL},
        {NULL,        RGB_BLACK,  NULL,                                 0, NULL}             // End of menu
    };
    static tMenuItem mixerMenuItems[]  = {
        {"Mixer 1-1 A", RGB_GREY_3, menu_action_create, moduleTypeMix1to1A,  NULL},
        {"Mixer 1-1 S", RGB_GREY_3, menu_action_create, moduleTypeMix1to1S,  NULL},
        {"Mixer 2-1 A", RGB_GREY_3, menu_action_create, moduleTypeMix2to1A,  NULL},
        {"Mixer 4-1 A", RGB_GREY_3, menu_action_create, moduleTypeMix4to1A,  NULL},
        {"Mixer 4-1 B", RGB_GREY_3, menu_action_create, moduleTypeMix4to1B,  NULL},
        {"Mixer 4-1 C", RGB_GREY_3, menu_action_create, moduleTypeMix4to1C,  NULL},
        {"Mixer 4-1 S", RGB_GREY_3, menu_action_create, moduleTypeMix4to1S,  NULL},
        {"Mixer 2-1 B", RGB_GREY_3, menu_action_create, moduleTypeMix2to1B,  NULL},
        {"Mixer 8-1 A", RGB_GREY_3, menu_action_create, moduleTypeMix8to1A,  NULL},
        {"Mixer 8-1 B", RGB_GREY_3, menu_action_create, moduleTypeMix8to1B,  NULL},
        {"MixStereo",   RGB_GREY_3, menu_action_create, moduleTypeMixStereo, NULL},
        {"Fade 1-2",    RGB_GREY_3, menu_action_create, moduleTypeFade1to2,  NULL},
        {"Fade 2-1",    RGB_GREY_3, menu_action_create, moduleTypeFade2to1,  NULL},
        {"X-Fade",      RGB_GREY_3, menu_action_create, moduleTypeXtoFade,   NULL},
        {"Pan",         RGB_GREY_3, menu_action_create, moduleTypePan,       NULL},
        {NULL,          RGB_BLACK,  NULL,                                 0, NULL}           // End of menu
    };
    static tMenuItem logicMenuItems[]  = {
        {"Invert",     RGB_GREY_3, menu_action_create, moduleTypeInvert,     NULL},
        {"Pulse",      RGB_GREY_3, menu_action_create, moduleTypePulse,      NULL},
        {"Delay",      RGB_GREY_3, menu_action_create, moduleTypeDelay,      NULL},
        {"Gate",       RGB_GREY_3, menu_action_create, moduleTypeGate,       NULL},
        {"FlipFlop",   RGB_GREY_3, menu_action_create, moduleTypeFlipFlop,   NULL},
        {"ClkDiv",     RGB_GREY_3, menu_action_create, moduleTypeClkDiv,     NULL},
        {"8Counter",   RGB_GREY_3, menu_action_create, moduleType8Counter,   NULL},
        {"BinCounter", RGB_GREY_3, menu_action_create, moduleTypeBinCounter, NULL},
        {"ADConv",     RGB_GREY_3, menu_action_create, moduleTypeADConv,     NULL},
        {"DAConv",     RGB_GREY_3, menu_action_create, moduleTypeDAConv,     NULL},
        {NULL,         RGB_BLACK,  NULL,                                  0, NULL}           // End of menu
    };
    static tMenuItem fxMenuItems[]     = {
        {"Compressor", RGB_GREY_3, menu_action_create, moduleTypeCompress,  NULL},
        {"Digitizer",  RGB_GREY_3, menu_action_create, moduleTypeDigitizer, NULL},
        {"FreqShift",  RGB_GREY_3, menu_action_create, moduleTypeFreqShift, NULL},
        {"Flanger",    RGB_GREY_3, menu_action_create, moduleTypeFlanger,   NULL},
        {"Chorus",     RGB_GREY_3, menu_action_create, moduleTypeStChorus,  NULL},
        {"Phaser",     RGB_GREY_3, menu_action_create, moduleTypePhaser,    NULL},
        {"PShift",     RGB_GREY_3, menu_action_create, moduleTypePShift,    NULL},
        {"Reverb",     RGB_GREY_3, menu_action_create, moduleTypeReverb,    NULL},
        {"Scratch",    RGB_GREY_3, menu_action_create, moduleTypeScratch,   NULL},
        {NULL,         RGB_BLACK,  NULL,                                 0, NULL}             // End of menu
    };
    static tMenuItem midiMenuItems[]   = {
        {"CtrlSend", RGB_GREY_3, menu_action_create, moduleTypeCtrlSend, NULL},
        {"PCSend",   RGB_GREY_3, menu_action_create, moduleTypePCSend,   NULL},
        {"NoteSend", RGB_GREY_3, menu_action_create, moduleTypeNoteSend, NULL},
        {"CtrlRcv",  RGB_GREY_3, menu_action_create, moduleTypeCtrlRcv,  NULL},
        {"NoteRcv",  RGB_GREY_3, menu_action_create, moduleTypeNoteRcv,  NULL},
        {"NoteDet",  RGB_GREY_3, menu_action_create, moduleTypeNoteDet,  NULL},
        {"NoteZone", RGB_GREY_3, menu_action_create, moduleTypeNoteZone, NULL},
        {NULL,       RGB_BLACK,  NULL,                                0, NULL}               // End of menu
    };

    static tMenuItem moduleMenuItems[] = {
        {"In/Out",   RGB_GREY_3, menu_action_create, 0, ioMenuItems    },
        {"Osc",      RGB_GREY_3, menu_action_create, 0, oscMenuItems   },
        {"Random",   RGB_GREY_3, menu_action_create, 0, randomMenuItems},
        {"Filter",   RGB_GREY_3, menu_action_create, 0, filterMenuItems},
        {"Delay",    RGB_GREY_3, menu_action_create, 0, delayMenuItems },
        {"Level",    RGB_GREY_3, menu_action_create, 0, levelMenuItems },
        {"Switch",   RGB_GREY_3, menu_action_create, 0, switchMenuItems},
        {"Sequence", RGB_GREY_3, menu_action_create, 0, seqMenuItems   },
        {"Note",     RGB_GREY_3, menu_action_create, 0, noteMenuItems  },
        {"LFO",      RGB_GREY_3, menu_action_create, 0, lfoMenuItems   },
        {"Env",      RGB_GREY_3, menu_action_create, 0, envMenuItems   },
        {"FX",       RGB_GREY_3, menu_action_create, 0, fxMenuItems    },
        {"Shaper",   RGB_GREY_3, menu_action_create, 0, shaperMenuItems},
        {"Mixer",    RGB_GREY_3, menu_action_create, 0, mixerMenuItems },
        {"Logic",    RGB_GREY_3, menu_action_create, 0, logicMenuItems },
        {"Midi",     RGB_GREY_3, menu_action_create, 0, midiMenuItems  },
        {NULL,       RGB_BLACK,  NULL,               0, NULL           }        // End of menu
    };
    static tMenuItem menuItems[]       = {
        {"Create module", RGB_GREY_3, menu_action_create, 0, moduleMenuItems},
        {NULL,            RGB_BLACK,  NULL,               0, NULL           } // End of menu
    };

    // Store menu position
    gContextMenu.originCoord = coord;
    open_context_menu(coord, menuItems, 0, 0.0);
}

void open_connector_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t connectorIndex) {
    static tMenuItem menuItems[] = {
        {"Delete cable", RGB_GREY_3, menu_action_delete_cable, 0, NULL},
        {NULL,           RGB_BLACK,  NULL,                     0, NULL}        // End of menu
    };

    // Store menu position
    gContextMenu.moduleKey      = moduleKey;
    gContextMenu.connectorIndex = connectorIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
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

void open_module_context_menu(tCoord coord, tModuleKey moduleKey) {
    static tMenuItem colourMenuItems[] = {
        // 6 columns (R,G,B,Y,Pu,Cy) x 4 rows of shades, then Grey
        {"",   MODULE_RED_1,         action_set_module_colour,  6, NULL},
        {"",   MODULE_GREEN_1,       action_set_module_colour, 10, NULL},
        {"",   MODULE_BLUE_1,        action_set_module_colour,  5, NULL},
        {"",   MODULE_YELLOW_1,      action_set_module_colour,  9, NULL},
        {"",   MODULE_PURPLE_1,      action_set_module_colour, 21, NULL},
        {"",   MODULE_CYAN_1,        action_set_module_colour, 17, NULL},
        {"",   MODULE_RED_2,         action_set_module_colour, 13, NULL},
        {"",   MODULE_GREEN_2,       action_set_module_colour,  8, NULL},
        {"",   MODULE_BLUE_2,        action_set_module_colour, 20, NULL},
        {"",   MODULE_YELLOW_2,      action_set_module_colour, 11, NULL},
        {"",   MODULE_PURPLE_2,      action_set_module_colour, 22, NULL},
        {"",   MODULE_CYAN_2,        action_set_module_colour,  7, NULL},
        {"",   MODULE_RED_3,         action_set_module_colour, 14, NULL},
        {"",   MODULE_GREEN_3,       action_set_module_colour, 16, NULL},
        {"",   MODULE_BLUE_3,        action_set_module_colour, 12, NULL},
        {"",   MODULE_YELLOW_3,      action_set_module_colour, 15, NULL},
        {"",   MODULE_PURPLE_3,      action_set_module_colour, 23, NULL},
        {"",   MODULE_CYAN_3,        action_set_module_colour, 18, NULL},
        {"",   MODULE_RED_4,         action_set_module_colour,  1, NULL},
        {"",   MODULE_GREEN_4,       action_set_module_colour,  2, NULL},
        {"",   MODULE_BLUE_4,        action_set_module_colour,  3, NULL},
        {"",   MODULE_YELLOW_4,      action_set_module_colour,  4, NULL},
        {"",   MODULE_PURPLE_4,      action_set_module_colour, 24, NULL},
        {"",   MODULE_CYAN_4,        action_set_module_colour, 19, NULL},
        {"",   MODULE_STANDARD_GREY, action_set_module_colour,  0, NULL},
        {NULL, RGB_BLACK,            NULL,                      0, NULL}
    };

    static tMenuItem menuItems[] = {
        {"Rename",        RGB_GREY_3, action_rename_module,      0, NULL},
        {"Set colour",    RGB_GREY_3, action_set_module_colour,  0, colourMenuItems,},
        {"Delete module", RGB_GREY_3, menu_action_delete_module, 0, NULL},
        {NULL,            RGB_BLACK,  NULL,                      0, NULL}      // End of menu
    };

    // Store menu position
    gContextMenu.moduleKey = moduleKey;
    open_context_menu(coord, menuItems, 0, 0.0);
}

static void action_rename_morph_label(int index) {
    tModule  module     = {0};
    uint32_t morphIndex = (uint32_t)gContextMenu.items[index].param;
    uint32_t slot       = atomic_load(&gSlot);

    gContextMenu.moduleKey = (tModuleKey){
        slot, locationMorph, 1
    };

    if (read_module(gContextMenu.moduleKey, &module)) {
        uint32_t pi = morphIndex + NUM_MORPHS;

        gParamNameEdit.active     = true;
        gParamNameEdit.moduleKey  = gContextMenu.moduleKey;
        gParamNameEdit.paramIndex = pi;
        memset(gParamNameEdit.buffer, 0, sizeof(gParamNameEdit.buffer));
        strncpy(gParamNameEdit.buffer, module.paramName[pi][0], PROTOCOL_PARAM_NAME_SIZE);
    }
    gContextMenu.active    = false;
    atomic_store(&gReDraw, true);
}

static void open_morph_label_context_menu(tCoord coord, uint32_t morphIndex) {
    static tMenuItem menuItems[2];

    menuItems[0] = (tMenuItem){
        "Rename", RGB_GREY_3, action_rename_morph_label, morphIndex, NULL
    };
    menuItems[1] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };
    open_context_menu(coord, menuItems, 0, 0.0);
}

static void send_patch_descr_update(uint32_t slot) {
    tMessageContent messageContent = {0};

    messageContent.cmd  = eMsgCmdWritePatchDescr;
    messageContent.slot = slot;
    msg_send(&gCommandQueue, &messageContent);
}

static void action_set_patch_type(int index) {
    uint32_t slot = atomic_load(&gSlot);

    gPatchDescr[slot].category = gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active        = false;
}

void open_patch_type_context_menu(tCoord coord) {
    static tMenuItem menuItems[] = {
        {"No Cat",    RGB_GREY_3, action_set_patch_type, patchTypeNoCat,     NULL},
        {"Acoustic",  RGB_GREY_3, action_set_patch_type, patchTypeAcoustic,  NULL},
        {"Sequencer", RGB_GREY_3, action_set_patch_type, patchTypeSequencer, NULL},
        {"Bass",      RGB_GREY_3, action_set_patch_type, patchTypeBass,      NULL},
        {"Classic",   RGB_GREY_3, action_set_patch_type, patchTypeClassic,   NULL},
        {"Drum",      RGB_GREY_3, action_set_patch_type, patchTypeDrum,      NULL},
        {"Fantasy",   RGB_GREY_3, action_set_patch_type, patchTypeFantasy,   NULL},
        {"Fx",        RGB_GREY_3, action_set_patch_type, patchTypeFx,        NULL},
        {"Lead",      RGB_GREY_3, action_set_patch_type, patchTypeLead,      NULL},
        {"Organ",     RGB_GREY_3, action_set_patch_type, patchTypeOrgan,     NULL},
        {"Pad",       RGB_GREY_3, action_set_patch_type, patchTypePad,       NULL},
        {"Piano",     RGB_GREY_3, action_set_patch_type, patchTypePiano,     NULL},
        {"Synth",     RGB_GREY_3, action_set_patch_type, patchTypeSynth,     NULL},
        {"Audio In",  RGB_GREY_3, action_set_patch_type, patchTypeAudioIn,   NULL},
        {"User 1",    RGB_GREY_3, action_set_patch_type, patchTypeUser1,     NULL},
        {"User 2",    RGB_GREY_3, action_set_patch_type, patchTypeUser2,     NULL},
        {NULL,        RGB_BLACK,  NULL,                                   0, NULL}
    };

    open_context_menu(coord, menuItems, 0, 0.0);
}

static void action_set_mono_poly(int index) {
    uint32_t slot = atomic_load(&gSlot);

    gPatchDescr[slot].monoPoly = (uint8_t)gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active        = false;
}

static void open_mono_poly_context_menu(tCoord coord) {
    static tMenuItem menuItems[] = {
        {"Poly",   RGB_GREY_3, action_set_mono_poly, monoPolyPoly,   NULL},
        {"Mono",   RGB_GREY_3, action_set_mono_poly, monoPolyMono,   NULL},
        {"Legato", RGB_GREY_3, action_set_mono_poly, monoPolyLegato, NULL},
        {NULL,     RGB_BLACK,  NULL,                              0, NULL}
    };

    open_context_menu(coord, menuItems, 0, 0.0);
}

static void action_set_voice_count(int index) {
    uint32_t slot = atomic_load(&gSlot);

    gPatchDescr[slot].voiceCount = (uint8_t)gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active          = false;
}

static void open_voice_count_context_menu(tCoord coord) {
    static tMenuItem menuItems[33];
    static char      labels[32][4];
    static bool      labelsInitialised = false;
    uint32_t         slot              = atomic_load(&gSlot);
    uint32_t         assignedVoices    = gAssignedVoices[slot];

    if (!labelsInitialised) {
        for (int i = 0; i < 32; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%d", i + 1);
            menuItems[i].label   = labels[i];
            menuItems[i].action  = action_set_voice_count;
            menuItems[i].param   = (uint32_t)i;
            menuItems[i].subMenu = NULL;
        }

        menuItems[32]     = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        labelsInitialised = true;
    }

    for (int i = 0; i < 32; i++) {
        bool invalid = (assignedVoices > 0) && ((uint32_t)(i + 1) > assignedVoices);
        menuItems[i].colour = invalid ? (tRgb)RGB_RED_5 : (tRgb)RGB_GREY_3;
    }

    open_context_menu(coord, menuItems, 4, 0.0);
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

bool handle_context_menu_click(tCoord coord) {
    if ((gContextMenu.active == false) || (gContextMenu.items == NULL)) {
        return false;
    }
    double   size        = 0.0;
    double   largestSize = 0.0;
    double   itemHeight  = STANDARD_TEXT_HEIGHT;
    uint32_t columns     = (gContextMenu.columns > 1) ? gContextMenu.columns : 1;

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        size = get_text_width(gContextMenu.items[i].label, itemHeight);

        if (size > largestSize) {
            largestSize = size;
        }
    }

    double   computed    = (largestSize + (5 * 2) > itemHeight) ? largestSize + (5 * 2) : itemHeight;
    double   cellW       = (gContextMenu.cellWidth > 0.0) ? gContextMenu.cellWidth : computed;
    double   cellH       = itemHeight + (5 * 2);

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        int        col      = (int)(i % columns);
        int        row      = (int)(i / columns);
        tRectangle itemRect = {
            {gContextMenu.coord.x + col * cellW, gContextMenu.coord.y + row * cellH},
            {cellW,                              cellH                             }
        };

        if (within_rectangle(coord, itemRect)) {
            if (gContextMenu.items[i].action != NULL) {
                if (gContextMenu.items[i].subMenu == NULL) {
                    gContextMenu.active = false;
                }
                gContextMenu.items[i].action(i);
            } else if (gContextMenu.items[i].subMenu != NULL) {
                open_context_menu(gContextMenu.coord, gContextMenu.items[i].subMenu, 0, 0.0);
            } else {
                gContextMenu.active = false;
            }
            return true;
        }
    }

    memset(&gContextMenu, 0, sizeof(gContextMenu)); // Clear everything (including active flag) to zero

    return false;
}

bool handle_scrollbar_click(tCoord coord) {
    //printf("Mouse %f %f bar x %f %f %f %f\n", coord.x, coord.y, gScrollState.xRectangle.coord.x,  gScrollState.xRectangle.coord.y,  gScrollState.xRectangle.size.w,  gScrollState.xRectangle.size.h);

    if (within_rectangle(coord, gScrollState.xRectangle)) {
        set_x_scroll_bar(coord.x);
        gScrollState.xBarDragging = true;
        return true;
    }

    if (within_rectangle(coord, gScrollState.yRectangle)) {
        set_y_scroll_bar(coord.y);
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
    //gPatchVolumeDragging      = false;
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

static void action_copy_variation(int index) {
    uint32_t        sourceVariation = (uint32_t)(gContextMenu.items[index].param >> 4) & 0xF;
    uint32_t        targetVariation = (uint32_t)(gContextMenu.items[index].param) & 0xF;
    uint32_t        slot            = atomic_load(&gSlot);
    uint32_t        numParams       = 0;
    uint32_t        paramIndex      = 0;
    uint32_t        morphIndex      = 0;
    tModule         module          = {0};
    tMessageContent msg             = {0};

    LOG_DEBUG("Copy variation %u to %u\n", sourceVariation, targetVariation);

    // Update local state for both VA and FX locations
    reset_walk_module();

    while (walk_next_module(&module)) {
        if (module.key.slot == slot) {
            numParams = module_param_count(module.type);

            for (paramIndex = 0; paramIndex < numParams; paramIndex++) {
                module.param[targetVariation][paramIndex].value = module.param[sourceVariation][paramIndex].value;

                for (morphIndex = 0; morphIndex < NUM_MORPHS; morphIndex++) {
                    module.param[targetVariation][paramIndex].morphRange[morphIndex] = module.param[sourceVariation][paramIndex].morphRange[morphIndex];
                }
            }

            write_module(module.key, &module);
        }
    }
    finish_walk_module();

    msg.cmd                             = eMsgCmdCopyVariation;
    msg.slot                            = slot;
    msg.copyVariationData.fromVariation = sourceVariation;
    msg.copyVariationData.toVariation   = targetVariation;
    msg_send(&gCommandQueue, &msg);

    gContextMenu.active                 = false;
    atomic_store(&gReDraw, true);
}

void open_variation_copy_menu(tCoord coord, uint32_t sourceVariation) {
    static tMenuItem menuItems[NUM_VARIATIONS + 1]; // +1 for terminator
    int              count                      = 0;
    uint32_t         targetVariation            = 0;
    static char      labels[NUM_VARIATIONS][32] = {0};

    memset(&labels, 0, sizeof(labels));

    for (targetVariation = 0; targetVariation < NUM_VARIATIONS; targetVariation++) {
        if (targetVariation != sourceVariation) {
            if (targetVariation == VARIATION_INIT) {
                snprintf(labels[targetVariation], sizeof(labels[targetVariation]), "Copy to Init");
            } else {
                snprintf(labels[targetVariation], sizeof(labels[targetVariation]), "Copy to variation %u", targetVariation + 1);
            }
            menuItems[count].label   = labels[targetVariation];
            menuItems[count].colour  = (tRgb)RGB_GREY_3;
            menuItems[count].action  = action_copy_variation;
            menuItems[count].param   = (int)((sourceVariation << 4) | targetVariation);
            menuItems[count].subMenu = NULL;
            count++;
        }
    }

    menuItems[count] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    open_context_menu(coord, menuItems, 0, 0.0);
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
    tModule      module      = {0};
    int32_t      i           = 0;
    uint32_t     slot        = atomic_load(&gSlot);
    uint32_t     location    = atomic_load(&gLocation);

    mouseButton = convert_to_mouse_button(button, action);

    get_global_gui_scaled_mouse_coord(&coord);

    if (gPatchNotesEdit.active) {
        if (mouseButton == mouseButtonLeftDown) {
            int newPos = note_editor_cursor_from_click(coord.x, coord.y);
            if (newPos >= 0) {
                gPatchNotesEdit.cursorPos = (uint32_t)newPos;
            } else {
                gPatchNotesEdit.active = false;
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

            for (i = 0; i < array_size_main_button_array(); i++) {
                if (within_rectangle(coord, gMainButtonArray[i].rectangle)) {
                    found                         = true;
                    gMainButtonArray[i].isPressed = true; // Used only to change button colour
                    break;
                }
            }

            if (found == false) {
                if (handle_scrollbar_click(coord)) {
                    found = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gPatchVolumeRectangle)) {
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
                if (handle_module_press(coord, mouseButton) == true) {
                    found = true;
                }
            }
        }
        break;

        case mouseButtonLeftUp:
        {
            for (i = 0; i < array_size_main_button_array(); i++) {
                gMainButtonArray[i].isPressed = false;
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
                for (i = 0; i < array_size_main_button_array(); i++) {
                    if (within_rectangle(coord, gMainButtonArray[i].rectangle)) {
                        handle_button((tButtonId)i);
                        found = true;
                        break;
                    }
                }
            }

            if (found == false) {
                for (i = 0; i < cableColourMax; i++) {
                    if (within_rectangle(coord, gCableColourToggleRect[i])) {
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
                    if (within_rectangle(coord, gCableColourSelectRect[i])) {
                        gCableColour = i;
                        atomic_store(&gReDraw, true);
                        found        = true;
                        break;
                    }
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gHideAllCablesRect)) {
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
                if (within_rectangle(coord, gTransparentCablesRect)) {
                    bool current = atomic_load(&gCablesTransparent);
                    atomic_store(&gCablesTransparent, !current);
                    atomic_store(&gReDraw, true);
                    found = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gPatchNotesButtonRect)) {
                    gPatchNotesEdit.active    = true;
                    gPatchNotesEdit.slot      = slot;
                    gPatchNotesEdit.cursorPos = gPatchNotesSize[slot];
                    memset(gPatchNotesEdit.buffer, 0, sizeof(gPatchNotesEdit.buffer));
                    memcpy(gPatchNotesEdit.buffer, gPatchNotes[slot], gPatchNotesSize[slot]);
                    found                     = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gPatchNameRectangle)) {
                    gPatchNameEdit.active = true;
                    gPatchNameEdit.slot   = slot;
                    patch_name_get(slot, gPatchNameEdit.buffer, sizeof(gPatchNameEdit.buffer));
                    found                 = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gPatchTypeRectangle)) {
                    open_patch_type_context_menu(gPatchTypeRectangle.coord);
                    found = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gMonoPolyRectangle)) {
                    open_mono_poly_context_menu(coord);
                    found = true;
                }
            }

            if (found == false) {
                if (within_rectangle(coord, gVoiceCountRectangle)) {
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

            for (i = (int)variation1ButtonId; i <= (int)variationInitButtonId; i++) {
                if (within_rectangle(coord, gMainButtonArray[i].rectangle)) {
                    uint32_t sourceVariation = (uint32_t)i - (uint32_t)variation1ButtonId;
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
        set_y_scroll_bar(y);
    } else if (gScrollState.xBarDragging == true) {
        set_x_scroll_bar(x);
        //} else if (gVoiceDialDragging == true) {  // Use this for patch level...?
        //
        //angle                        = calculate_mouse_angle((tCoord){x, y}, gVoiceDialRect);
        //value                        = angle_to_value(angle, 31);
        //gPatchDescr[slot].voiceCount = value + 1; // Note G2 won't let me set less than a value of 1, can't set to zero for zero based
        //messageContent.cmd           = eMsgCmdWritePatchDescr;
        // messageContent.slot          = slot;
        // msg_send(&gCommandQueue, &messageContent);
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
                uint32_t newSize = (uint32_t)len;
                if (newSize > PATCH_NOTES_SIZE) newSize = PATCH_NOTES_SIZE;
                memcpy(gPatchNotes[gPatchNotesEdit.slot], gPatchNotesEdit.buffer, newSize);
                gPatchNotes[gPatchNotesEdit.slot][newSize] = '\0';
                gPatchNotesSize[gPatchNotesEdit.slot]      = newSize;
                gPatchNotesEdit.active                     = false;
                tMessageContent msg                        = {0};
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
                if (cursorPos > 0) gPatchNotesEdit.cursorPos--;
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) gPatchNotesEdit.cursorPos++;
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
