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

#include <math.h>

#include "defs.h"
#include "types.h"
#include "utils.h"
#include "utilsGraphics.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
#include "protocol.h"
#include "menus.h"

// ── Synth settings action targets ──────────────────────────────────────────

static uint8_t * gSettingU8Target = NULL;
static int8_t *  gSettingI8Target = NULL;

void send_synth_settings_msg(void) {
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdWriteSynthSettings;
    msg_send(&gCommandQueue, &msg);
}

static void action_setting_u8(int index) {
    *gSettingU8Target = (uint8_t)gContextMenu.items[index].param;
    send_synth_settings_msg();
}

static void action_setting_i8(int index) {
    *gSettingI8Target = (int8_t)(int32_t)gContextMenu.items[index].param;
    send_synth_settings_msg();
}

// ── Patch descriptor action targets ────────────────────────────────────────

static void send_patch_descr_update(uint32_t slot) {
    tMessageContent messageContent = {0};

    messageContent.cmd  = eMsgCmdWritePatchDescr;
    messageContent.slot = slot;
    msg_send(&gCommandQueue, &messageContent);
}

static void action_set_patch_type(int index) {
    uint32_t slot = gSlot;

    gPatchDescr[slot].category = gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active        = false;
}

static void action_set_mono_poly(int index) {
    uint32_t slot = gSlot;

    gPatchDescr[slot].monoPoly = (uint8_t)gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active        = false;
}

static void action_set_voice_count(int index) {
    uint32_t slot = gSlot;

    gPatchDescr[slot].voiceCount = (uint8_t)gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active          = false;
}

static void action_copy_variation(int index) {
    uint32_t        sourceVariation = (uint32_t)(gContextMenu.items[index].param >> 4) & 0xF;
    uint32_t        targetVariation = (uint32_t)(gContextMenu.items[index].param) & 0xF;
    uint32_t        slot            = gSlot;
    uint32_t        numParams       = 0;
    uint32_t        paramIndex      = 0;
    uint32_t        morphIndex      = 0;
    tMessageContent msg             = {0};

    LOG_DEBUG("Copy variation %u to %u\n", sourceVariation, targetVariation);

    for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * module = get_module_slot(slot, loc, i);

            if (!module->active) {
                continue;
            }
            numParams = module_param_count(module->type);

            for (paramIndex = 0; paramIndex < numParams; paramIndex++) {
                module->param[targetVariation][paramIndex].value = module->param[sourceVariation][paramIndex].value;

                for (morphIndex = 0; morphIndex < NUM_MORPHS; morphIndex++) {
                    module->param[targetVariation][paramIndex].morphRange[morphIndex] = module->param[sourceVariation][paramIndex].morphRange[morphIndex];
                }
            }
        }
    }

    msg.cmd                             = eMsgCmdCopyVariation;
    msg.slot                            = slot;
    msg.copyVariationData.fromVariation = sourceVariation;
    msg.copyVariationData.toVariation   = targetVariation;
    msg_send(&gCommandQueue, &msg);

    gContextMenu.active                 = false;
    gReDraw = true;
}

// ── Module / cable / morph actions ─────────────────────────────────────────

static void menu_action_set_cable_colour(int index) {
    uint32_t  newColour = gContextMenu.items[index].param;
    uint32_t  slot      = gSlot;
    uint32_t  location  = &gLocation;
    int       outIndex  = -1;
    int       inIndex   = -1;

    gContextMenu.active = false;

    if ((gContextMenu.moduleKey.slot != slot) || (gContextMenu.moduleKey.location != location)) {
        return;
    }
    tModule * module    = get_module(gContextMenu.moduleKey);

    if (module == NULL) {
        return;
    }

    switch (module->connector[gContextMenu.connectorIndex].dir) {
        case connectorDirOut:
            outIndex = find_io_count_from_index(module, connectorDirOut, gContextMenu.connectorIndex);
            break;
        case connectorDirIn:
            inIndex  = find_io_count_from_index(module, connectorDirIn, gContextMenu.connectorIndex);
            break;
    }

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable    = get_cable_slot(slot, location, i);
        bool     doUpdate = false;

        if (cable == NULL || !cable->active) {
            continue;
        }

        if (cable->key.moduleFromIndex == gContextMenu.moduleKey.index) {
            if (cable->key.linkType == cableLinkTypeFromInput) {
                if ((int)cable->key.connectorFromIoCount == inIndex) {
                    doUpdate = true;
                }
            } else if (cable->key.linkType == cableLinkTypeFromOutput) {
                if ((int)cable->key.connectorFromIoCount == outIndex) {
                    doUpdate = true;
                }
            }
        }

        if (cable->key.moduleToIndex == gContextMenu.moduleKey.index) {
            if ((int)cable->key.connectorToIoCount == inIndex) {
                doUpdate = true;
            }
        }

        if (doUpdate) {
            cable->colour                                 = newColour;

            tMessageContent messageContent = {0};
            messageContent.cmd                            = eMsgCmdWriteCable;
            messageContent.slot                           = slot;
            messageContent.cableData.location             = location;
            messageContent.cableData.moduleFromIndex      = cable->key.moduleFromIndex;
            messageContent.cableData.connectorFromIoIndex = cable->key.connectorFromIoCount;
            messageContent.cableData.moduleToIndex        = cable->key.moduleToIndex;
            messageContent.cableData.connectorToIoIndex   = cable->key.connectorToIoCount;
            messageContent.cableData.linkType             = cable->key.linkType;
            messageContent.cableData.colour               = newColour;
            msg_send(&gCommandQueue, &messageContent);
        }
    }

    gReDraw = true;
}

static void menu_action_delete_cable(int index) {
    int      outIndex = -1;
    int      inIndex  = -1;
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    if ((gContextMenu.moduleKey.slot == slot) && (gContextMenu.moduleKey.location == location)) {
        tModule * module = get_module(gContextMenu.moduleKey);

        if (module == NULL) {
            return;
        }

        switch (module->connector[gContextMenu.connectorIndex].dir) {
            case connectorDirOut:
                outIndex = find_io_count_from_index(module, connectorDirOut, gContextMenu.connectorIndex);
                break;
            case connectorDirIn:
                inIndex  = find_io_count_from_index(module, connectorDirIn, gContextMenu.connectorIndex);
                break;
        }

        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable * cable    = get_cable_slot(slot, location, i);
            bool     doDelete = false;

            if (cable == NULL || !cable->active) {
                continue;
            }

            if (cable->key.moduleFromIndex == gContextMenu.moduleKey.index) {
                if (cable->key.linkType == cableLinkTypeFromInput) {
                    if ((int)cable->key.connectorFromIoCount == inIndex) {
                        doDelete = true;
                    }
                } else if (cable->key.linkType == cableLinkTypeFromOutput) {
                    if ((int)cable->key.connectorFromIoCount == outIndex) {
                        doDelete = true;
                    }
                }
            }

            if (cable->key.moduleToIndex == gContextMenu.moduleKey.index) {
                if ((int)cable->key.connectorToIoCount == inIndex) {
                    doDelete = true;
                }
            }

            if (doDelete) {
                tMessageContent messageContent = {0};

                messageContent.cmd                            = eMsgCmdDeleteCable;
                messageContent.slot                           = slot;
                messageContent.cableData.location             = location;
                messageContent.cableData.moduleFromIndex      = cable->key.moduleFromIndex;
                messageContent.cableData.connectorFromIoIndex = cable->key.connectorFromIoCount;
                messageContent.cableData.moduleToIndex        = cable->key.moduleToIndex;
                messageContent.cableData.connectorToIoIndex   = cable->key.connectorToIoCount;
                messageContent.cableData.linkType             = cable->key.linkType;

                msg_send(&gCommandQueue, &messageContent);

                delete_cable(cable->key);
            }
        }

        update_module_up_rates();
    }
}

static void menu_action_delete_module(int index) {
    uint32_t        slot           = gSlot;
    uint32_t        location       = gLocation;
    tMessageContent messageContent = {0};

    if (gContextMenu.moduleKey.slot == slot && gContextMenu.moduleKey.location == location) {
        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable * cable = get_cable_slot(slot, location, i);

            if (cable == NULL || !cable->active) {
                continue;
            }

            if (cable->key.moduleFromIndex == gContextMenu.moduleKey.index || cable->key.moduleToIndex == gContextMenu.moduleKey.index) {
                memset(&messageContent, 0, sizeof(messageContent));
                messageContent.cmd                            = eMsgCmdDeleteCable;
                messageContent.slot                           = slot;
                messageContent.cableData.location             = location;
                messageContent.cableData.moduleFromIndex      = cable->key.moduleFromIndex;
                messageContent.cableData.connectorFromIoIndex = cable->key.connectorFromIoCount;
                messageContent.cableData.moduleToIndex        = cable->key.moduleToIndex;
                messageContent.cableData.connectorToIoIndex   = cable->key.connectorToIoCount;
                messageContent.cableData.linkType             = cable->key.linkType;

                msg_send(&gCommandQueue, &messageContent);

                delete_cable(cable->key);
            }
        }

        memset(&messageContent, 0, sizeof(messageContent));
        messageContent.cmd                  = eMsgCmdDeleteModule;
        messageContent.slot                 = slot;
        messageContent.moduleData.moduleKey = gContextMenu.moduleKey;

        msg_send(&gCommandQueue, &messageContent);

        delete_module(gContextMenu.moduleKey);

        update_module_up_rates();
    }
}

static void action_rename_module(int index) {
    tModule * module = get_module(gContextMenu.moduleKey);

    if (module != NULL) {
        gModuleNameEdit.active                   = true;
        gModuleNameEdit.moduleKey                = gContextMenu.moduleKey;
        COPY_STRING(gModuleNameEdit.buffer, module->name);
    }
    gContextMenu.active = false;
    gReDraw = true;
}

static void action_set_module_colour(int index) {
    if (gContextMenu.items[index].subMenu == NULL) {
        tMessageContent messageContent = {0};
        tModule *       module         = get_module(gContextMenu.moduleKey);

        if (module == NULL) {
            return;
        }
        module->colour                            = gContextMenu.items[index].param;

        messageContent.cmd                        = eMsgCmdSetModuleColour;
        messageContent.slot                       = module->key.slot;
        messageContent.moduleColourData.moduleKey = module->key;
        messageContent.moduleColourData.colour    = module->colour;

        msg_send(&gCommandQueue, &messageContent);
    } else {
        tMenuItem * subMenu = gContextMenu.items[index].subMenu;

        if (subMenu != NULL) {
            open_context_menu(gContextMenu.coord, subMenu, 6, STANDARD_TEXT_HEIGHT * 2);
        }
    }
}

static void action_rename_morph_label(int index) {
    uint32_t  morphIndex = (uint32_t)gContextMenu.items[index].param;
    uint32_t  slot       = gSlot;

    gContextMenu.moduleKey = (tModuleKey){
        slot, locationMorph, 1
    };

    tModule * module     = get_module(gContextMenu.moduleKey);

    if (module != NULL) {
        uint32_t pi = morphIndex;

        gParamNameEdit.active     = true;
        gParamNameEdit.moduleKey  = gContextMenu.moduleKey;
        gParamNameEdit.paramIndex = pi;
        COPY_STRING(gParamNameEdit.buffer, module->paramName[pi][0]);
    }
    gContextMenu.active    = false;
    gReDraw = true;
}

// ── Module creation helpers ─────────────────────────────────────────────────

static void init_params_on_module(tModule * module, uint32_t location, uint32_t variation) {
    uint32_t paramListIndex = 0;
    uint32_t paramIndex     = 0;
    uint32_t numParams      = module_param_count(module->type);
    uint32_t slot           = gSlot;

    if (location != gLocation) {
        return;
    }

    for (paramListIndex = 0; paramListIndex < array_size_param_location_list(); paramListIndex++) {
        if (paramLocationList[paramListIndex].moduleType == module->type) {
            module->param[variation][paramIndex].value = paramLocationList[paramListIndex].defaultValue;
            send_param_value(slot, module->key, paramIndex, variation, module->param[variation][paramIndex].value);
            paramIndex++;

            if (paramIndex >= numParams) {
                break;
            }
        }
    }
}

static void init_params_on_module_all_variations(tModule * module, uint32_t location) {
    if (location != gLocation) {
        return;
    }

    for (uint32_t variation = 0; variation < NUM_VARIATIONS; variation++) {
        init_params_on_module(module, location, variation);
    }
}

static int32_t find_unique_module_id(uint32_t location) {
    uint32_t slot = gSlot;

    for (uint32_t i = 1; i < MAX_NUM_MODULES; i++) {
        tModule * candidate = get_module_slot(slot, location, i);

        if ((candidate == NULL) || !candidate->active) {
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

void shift_modules_down(tModuleKey key) {
    tModule * module            = get_module(key);

    if (module == NULL) {
        return;
    }
    bool      moduleRePosition  = false;
    bool      doDrop            = false;
    uint32_t  rowAndBelowToDrop = 0;
    uint32_t  dropAmount        = 0;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * walk = get_module_slot(key.slot, key.location, i);

        if (!walk->active || walk->key.index == key.index) {
            continue;
        }

        if ((walk->column == module->column) && (module->row > walk->row) && (module->row < walk->row + gModuleProperties[walk->type].height)) {
            module->row      = walk->row + gModuleProperties[walk->type].height;
            send_module_move_msg(module);
            moduleRePosition = true;
            break;
        }
    }

    if (moduleRePosition == false) {
        send_module_move_msg(module);
    }

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * walk = get_module_slot(key.slot, key.location, i);

        if (!walk->active || walk->key.index == key.index) {
            continue;
        }

        if ((walk->column == module->column) && (walk->row >= module->row) && (walk->row < module->row + gModuleProperties[module->type].height)) {
            rowAndBelowToDrop = walk->row;
            dropAmount        = (module->row + gModuleProperties[module->type].height) - walk->row;
            doDrop            = true;
            break;
        }
    }

    if (doDrop == true) {
        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * walk = get_module_slot(key.slot, key.location, i);

            if (!walk->active || walk->key.index == key.index) {
                continue;
            }

            if ((walk->column == module->column) && (walk->row >= rowAndBelowToDrop)) {
                walk->row += dropAmount;

                if (walk->row > MAX_ROWS) {
                    walk->row = MAX_ROWS;
                }
                send_module_move_msg(walk);
            }
        }
    }
}

static void menu_action_create(int index) {
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    if (gContextMenu.items[index].param != 0) {
        tModule         module         = {0};
        tMessageContent messageContent = {0};
        int32_t         uniqueIndex    = 0;

        module.key.slot     = slot;
        module.key.location = location;
        uniqueIndex         = find_unique_module_id(module.key.location);

        if (uniqueIndex >= 0) {
            module.key.index                                                           = (uint32_t)uniqueIndex;
            module.type                                                                = (tModuleType)gContextMenu.items[index].param;
            convert_mouse_coord_to_module_column_row(&module.column, &module.row, gContextMenu.originCoord);

            COPY_STRING(module.name, gModuleProperties[module.type].name);

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

            COPY_STRING(messageContent.moduleData.name, module.name);

            msg_send(&gCommandQueue, &messageContent);

            write_module(module.key, &module);

            init_params_on_module_all_variations(get_module(module.key), location);

            shift_modules_down(module.key);
        }
    } else {
        tMenuItem * subMenu = gContextMenu.items[index].subMenu;

        if (subMenu != NULL) {
            open_context_menu(gContextMenu.coord, subMenu, 0, 0.0);
        }
    }
}

// ── Parameter / knob actions ────────────────────────────────────────────────

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
    uint32_t        slot        = gSlot;
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
    gReDraw = true;
}

static void action_deassign_knob(int index) {
    uint32_t        slot        = gSlot;
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
    gReDraw = true;
}

static void action_set_toggle_value(int index) {
    uint32_t  slot      = gSlot;
    uint32_t  variation = gPatchDescr[slot].activeVariation;
    tModule * module    = get_module(gContextMenu.moduleKey);

    if (module != NULL) {
        uint32_t paramIdx = gContextMenu.paramIndex;
        module->param[variation][paramIdx].value = (uint32_t)gContextMenu.items[index].param;
        send_param_value(slot, gContextMenu.moduleKey, paramIdx, variation, module->param[variation][paramIdx].value);
    }
    gContextMenu.active = false;
    gReDraw = true;
}

void open_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex, uint32_t paramRef) {
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
    uint32_t  slot   = gSlot;
    tModule * module = get_module(gContextMenu.moduleKey);

    if (module != NULL) {
        uint32_t modeIdx = gContextMenu.paramIndex;
        module->mode[modeIdx].value = (uint32_t)gContextMenu.items[index].param;
        send_mode_value(slot, gContextMenu.moduleKey, modeIdx, module->mode[modeIdx].value);
    }
    gContextMenu.active = false;
    gReDraw = true;
}

void open_mode_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t modeIndex, uint32_t modeRef) {
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
    uint32_t  pi     = gContextMenu.paramIndex;
    tModule * module = get_module(gContextMenu.moduleKey);

    if (module != NULL) {
        gParamNameEdit.active     = true;
        gParamNameEdit.moduleKey  = gContextMenu.moduleKey;
        gParamNameEdit.paramIndex = pi;
        memset(gParamNameEdit.buffer, 0, sizeof(gParamNameEdit.buffer));

        if (module->paramNameSet[pi][0]) {
            COPY_STRING(gParamNameEdit.buffer, module->paramName[pi][0]);
        }
    }
    gContextMenu.active = false;
    gReDraw = true;
}

void open_param_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex) {
    static tMenuItem pageMenuItems[NUM_PARAM_PAGES + 1];
    static char      pageLabels[NUM_PARAM_PAGES][10];
    static tMenuItem bankMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE + 1];
    static char      bankLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][24];
    static tMenuItem slotMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK + 1];
    static char      slotLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK][64];
    static tMenuItem menuItems[4];

    uint32_t         slot     = gSlot;
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
                    tModuleKey   modKey  = {slot, gKnobArray[slot].knob[knobIdx].location, gKnobArray[slot].knob[knobIdx].moduleIndex};
                    uint32_t     pi      = gKnobArray[slot].knob[knobIdx].paramIndex;
                    const char * modName = "";
                    const char * parName = "";
                    tModule *    mod     = get_module(modKey);

                    if (mod != NULL) {
                        uint32_t variation = gPatchDescr[slot].activeVariation;

                        modName = (mod->name[0] != '\0') ? mod->name : gModuleProperties[mod->type].name;

                        if ((pi < MAX_NUM_PARAMETERS) && mod->paramNameSet[pi][0]) {
                            parName = mod->paramName[pi][0];
                        } else if (pi < MAX_NUM_PARAMETERS) {
                            const char * label = paramLocationList[mod->param[variation][pi].paramRef].label;

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
        uint32_t  variation = gPatchDescr[slot].activeVariation;
        tModule * mod       = get_module(moduleKey);

        if ((mod != NULL) && (paramIndex < MAX_NUM_PARAMETERS)) {
            if (paramLocationList[mod->param[variation][paramIndex].paramRef].type1 == paramType1Enable) {
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

// ── Static menu item arrays (menuResources.h uses the actions above) ───────

#include "menuResources.h"

// ── Core context menu mechanism ─────────────────────────────────────────────

static void clamp_menu_to_screen(tMenuItem * items, uint32_t columns) {
    int      count        = 0;
    double   renderWidth  = get_render_width() / gGlobalGuiScale;
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

    if (gContextMenu.coord.y < 0.0) {
        gContextMenu.coord.y = 0.0;
    }

    if (gContextMenu.cellWidth > 0.0) {
        double menuWidth = gContextMenu.cellWidth * (double)cols;

        if (gContextMenu.coord.x + menuWidth > renderWidth - SCROLLBAR_WIDTH) {
            gContextMenu.coord.x = renderWidth - SCROLLBAR_WIDTH - menuWidth;
        }

        if (gContextMenu.coord.x < 0.0) {
            gContextMenu.coord.x = 0.0;
        }
    }
}

void open_context_menu(tCoord coord, tMenuItem * items, uint32_t columns, double cellWidth) {
    gContextMenu.coord     = coord;
    gContextMenu.items     = items;
    gContextMenu.columns   = columns;
    gContextMenu.cellWidth = cellWidth;
    gContextMenu.active    = true;
    clamp_menu_to_screen(items, (columns > 1) ? columns : 1);
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
        size = get_text_width(gContextMenu.items[i].label, itemHeight, eNoCache);

        if (size > largestSize) {
            largestSize = size;
        }
    }

    double   cellW       = (largestSize + (5 * 2) > itemHeight) ? largestSize + (5 * 2) : itemHeight;
    double   cellH       = itemHeight + (5 * 2);

    cellW = (gContextMenu.cellWidth > 0.0) ? gContextMenu.cellWidth : cellW;

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

    memset(&gContextMenu, 0, sizeof(gContextMenu));

    return false;
}

// ── Helper ──────────────────────────────────────────────────────────────────

tCoord below_rect(tRectangle r) {
    tCoord c = {r.coord.x, r.coord.y + r.size.h};

    return c;
}

// ── Synth settings dropdowns ────────────────────────────────────────────────

void open_midi_chan_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gMidiChanItems, 0, 0.0);
}

void open_sysex_id_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gSysexIdItems, 0, 0.0);
}

void open_tune_semi_dropdown(tCoord coord, int8_t * target) {
    static tMenuItem items[26];
    static char      labels[25][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 25; i++) {
            int val = i - 12;
            snprintf(labels[i], sizeof(labels[i]), "%+d", val);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_i8;
            items[i].param   = (uint32_t)(int32_t)val;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingI8Target = target;
    open_context_menu(coord, items, 5, 0.0);
}

void open_tune_cent_dropdown(tCoord coord, int8_t * target) {
    static tMenuItem items[102];
    static char      labels[101][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 101; i++) {
            int val = i - 50;
            snprintf(labels[i], sizeof(labels[i]), "%+d", val);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_i8;
            items[i].param   = (uint32_t)(int32_t)val;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingI8Target = target;
    open_context_menu(coord, items, 10, 0.0);
}

void open_octave_shift_dropdown(tCoord coord, int8_t * target) {
    gSettingI8Target = target;
    open_context_menu(coord, gOctaveShiftItems, 0, 0.0);
}

void open_pedal_gain_dropdown(tCoord coord, uint8_t * target) {
    static tMenuItem items[34];
    static char      labels[33][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 33; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%.2f", 1.0 + i / 64.0);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_u8;
            items[i].param   = (uint32_t)i;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingU8Target = target;
    open_context_menu(coord, items, 4, 0.0);
}

void open_patch_sort_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPatchSortItems, 0, 0.0);
}

void open_perf_sort_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPerfSortItems, 0, 0.0);
}

void open_on_off_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gOnOffItems, 0, 0.0);
}

void open_active_off_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gActiveOffItems, 0, 0.0);
}

void open_pedal_polarity_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPedalPolarityItems, 0, 0.0);
}

// ── Module / cable / morph menus ────────────────────────────────────────────

void open_connector_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t connectorIndex) {
    static tMenuItem cableColourItems[] = {
        {"",   {0.7, 0.1, 0.1}, menu_action_set_cable_colour, cableColourRed,    NULL},
        {"",   {0.3, 0.3, 0.7}, menu_action_set_cable_colour, cableColourBlue,   NULL},
        {"",   {0.7, 0.7, 0.1}, menu_action_set_cable_colour, cableColourYellow, NULL},
        {"",   {0.8, 0.3, 0.2}, menu_action_set_cable_colour, cableColourOrange, NULL},
        {"",   {0.1, 0.7, 0.1}, menu_action_set_cable_colour, cableColourGreen,  NULL},
        {"",   {0.7, 0.1, 0.7}, menu_action_set_cable_colour, cableColourPurple, NULL},
        {NULL, RGB_BLACK, NULL, 0, NULL}
    };

    static tMenuItem menuItems[]        = {
        {"Delete cable", RGB_GREY_3, menu_action_delete_cable, 0, NULL            },
        {"Cable colour", RGB_GREY_3, NULL,                     0, cableColourItems},
        {NULL,           RGB_BLACK,  NULL,                     0, NULL            }
    };

    gContextMenu.moduleKey      = moduleKey;
    gContextMenu.connectorIndex = connectorIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
}

void open_module_context_menu(tCoord coord, tModuleKey moduleKey) {
    static tMenuItem colourMenuItems[] = {
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
        {"Rename",        RGB_GREY_3, action_rename_module,      0, NULL           },
        {"Set colour",    RGB_GREY_3, action_set_module_colour,  0, colourMenuItems},
        {"Delete module", RGB_GREY_3, menu_action_delete_module, 0, NULL           },
        {NULL,            RGB_BLACK,  NULL,                      0, NULL           }
    };

    gContextMenu.moduleKey = moduleKey;
    open_context_menu(coord, menuItems, 0, 0.0);
}

void open_module_area_context_menu(tCoord coord) {
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
        {NULL,                  RGB_BLACK,  NULL,                                0, NULL},
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
        {NULL,                    RGB_BLACK,  NULL,                                  0, NULL},
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
        {NULL,             RGB_BLACK,  NULL,                                 0, NULL},
    };
    static tMenuItem randomMenuItems[] = {
        {"Random A", RGB_GREY_3, menu_action_create, moduleTypeRandomA, NULL},
        {"Random B", RGB_GREY_3, menu_action_create, moduleTypeRandomB, NULL},
        {NULL,       RGB_BLACK,  NULL,                               0, NULL},
    };
    static tMenuItem lfoMenuItems[]    = {
        {"LFO A",           RGB_GREY_3, menu_action_create, moduleTypeLfoA,    NULL},
        {"LFO B",           RGB_GREY_3, menu_action_create, moduleTypeLfoB,    NULL},
        {"LFO C",           RGB_GREY_3, menu_action_create, moduleTypeLfoC,    NULL},
        {"LFO Shp A",       RGB_GREY_3, menu_action_create, moduleTypeLfoShpA, NULL},
        {"Clock Generator", RGB_GREY_3, menu_action_create, moduleTypeClkGen,  NULL},
        {NULL,              RGB_BLACK,  NULL,                               0, NULL},
    };
    static tMenuItem envMenuItems[]    = {
        {"Envelope ADSR",     RGB_GREY_3, menu_action_create, moduleTypeEnvADSR,  NULL},
        {"Envelope AHD",      RGB_GREY_3, menu_action_create, moduleTypeEnvAHD,   NULL},
        {"Envelope ADR",      RGB_GREY_3, menu_action_create, moduleTypeEnvADR,   NULL},
        {"Envelop ADDSR",     RGB_GREY_3, menu_action_create, moduleTypeEnvADDSR, NULL},
        {"Envelope H",        RGB_GREY_3, menu_action_create, moduleTypeEnvH,     NULL},
        {"Envelope D",        RGB_GREY_3, menu_action_create, moduleTypeEnvD,     NULL},
        {"Envelope Multi",    RGB_GREY_3, menu_action_create, moduleTypeEnvMulti, NULL},
        {"Envelope Mod AHD",  RGB_GREY_3, menu_action_create, moduleTypeModAHD,   NULL},
        {"Envelope Mod ADSR", RGB_GREY_3, menu_action_create, moduleTypeModADSR,  NULL},
        {NULL,                RGB_BLACK,  NULL,                                0, NULL},
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
        {NULL,             RGB_BLACK,  NULL,                                  0, NULL},
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
        {NULL,             RGB_BLACK,  NULL,                                   0, NULL},
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
        {NULL,        RGB_BLACK,  NULL,                                 0, NULL},
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
        {NULL,       RGB_BLACK,  NULL,                                 0, NULL},
    };
    static tMenuItem seqMenuItems[]    = {
        {"Sequencer Event",      RGB_GREY_3, menu_action_create, moduleTypeSeqEvent, NULL},
        {"Sequencer Values",     RGB_GREY_3, menu_action_create, moduleTypeSeqVal,   NULL},
        {"Sequencer Level",      RGB_GREY_3, menu_action_create, moduleTypeSeqLev,   NULL},
        {"Sequencer Note",       RGB_GREY_3, menu_action_create, moduleTypeSeqNote,  NULL},
        {"Sequencer Controlled", RGB_GREY_3, menu_action_create, moduleTypeSeqCtr,   NULL},
        {NULL,                   RGB_BLACK,  NULL,                                0, NULL},
    };
    static tMenuItem shaperMenuItems[] = {
        {"Saturate",  RGB_GREY_3, menu_action_create, moduleTypeSaturate,  NULL},
        {"Clip",      RGB_GREY_3, menu_action_create, moduleTypeClip,      NULL},
        {"OverDrive", RGB_GREY_3, menu_action_create, moduleTypeOverdrive, NULL},
        {"ShpExp",    RGB_GREY_3, menu_action_create, moduleTypeShpExp,    NULL},
        {"WaveWrap",  RGB_GREY_3, menu_action_create, moduleTypeWaveWrap,  NULL},
        {"ShpStatic", RGB_GREY_3, menu_action_create, moduleTypeShpStatic, NULL},
        {"Rect",      RGB_GREY_3, menu_action_create, moduleTypeRect,      NULL},
        {NULL,        RGB_BLACK,  NULL,                                 0, NULL},
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
        {NULL,          RGB_BLACK,  NULL,                                 0, NULL},
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
        {NULL,         RGB_BLACK,  NULL,                                  0, NULL},
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
        {NULL,         RGB_BLACK,  NULL,                                 0, NULL},
    };
    static tMenuItem midiMenuItems[]   = {
        {"CtrlSend", RGB_GREY_3, menu_action_create, moduleTypeCtrlSend, NULL},
        {"PCSend",   RGB_GREY_3, menu_action_create, moduleTypePCSend,   NULL},
        {"NoteSend", RGB_GREY_3, menu_action_create, moduleTypeNoteSend, NULL},
        {"CtrlRcv",  RGB_GREY_3, menu_action_create, moduleTypeCtrlRcv,  NULL},
        {"NoteRcv",  RGB_GREY_3, menu_action_create, moduleTypeNoteRcv,  NULL},
        {"NoteDet",  RGB_GREY_3, menu_action_create, moduleTypeNoteDet,  NULL},
        {"NoteZone", RGB_GREY_3, menu_action_create, moduleTypeNoteZone, NULL},
        {NULL,       RGB_BLACK,  NULL,                                0, NULL},
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
        {NULL,       RGB_BLACK,  NULL,               0, NULL           },
    };
    static tMenuItem menuItems[]       = {
        {"Create module", RGB_GREY_3, menu_action_create, 0, moduleMenuItems},
        {NULL,            RGB_BLACK,  NULL,               0, NULL           },
    };

    gContextMenu.originCoord = coord;
    open_context_menu(coord, menuItems, 0, 0.0);
}

void open_morph_label_context_menu(tCoord coord, uint32_t morphIndex) {
    static tMenuItem menuItems[2];

    menuItems[0] = (tMenuItem){
        "Rename", RGB_GREY_3, action_rename_morph_label, morphIndex + NUM_MORPHS, NULL
    };
    menuItems[1] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };
    open_context_menu(coord, menuItems, 0, 0.0);
}

// ── Patch settings menus ────────────────────────────────────────────────────

void open_patch_type_context_menu(tCoord coord) {
    open_context_menu(coord, gPatchTypeItems, 0, 0.0);
}

void open_mono_poly_context_menu(tCoord coord) {
    open_context_menu(coord, gMonoPolyItems, 0, 0.0);
}

void open_voice_count_context_menu(tCoord coord) {
    static tMenuItem menuItems[33];
    static char      labels[32][4];
    static bool      labelsInitialised = false;
    uint32_t         slot              = gSlot;
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

void open_variation_copy_menu(tCoord coord, uint32_t sourceVariation) {
    static tMenuItem menuItems[NUM_VARIATIONS + 1];
    static char      labels[NUM_VARIATIONS][32];
    int              count           = 0;
    uint32_t         targetVariation = 0;

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

#ifdef __cplusplus
}
#endif
