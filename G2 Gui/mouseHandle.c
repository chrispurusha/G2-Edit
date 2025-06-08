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
#include "globalVars.h"

static void get_global_gui_scaled_mouse_coord(tCoord * coord) {
    int fbWidth  = 0;
    int fbHeight = 0;

    glfwGetCursorPos(gWindow, &(coord->x), &(coord->y));

    glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);

    coord->x /= fbWidth;
    coord->y /= fbHeight;
    coord->x *= TARGET_FRAME_BUFF_WIDTH;
    coord->y *= TARGET_FRAME_BUFF_HEIGHT;
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
    x = coord.x;
    y = coord.y;


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
    tModule  module   = {0};
    uint32_t location = gLocation;


    // Step 1 - initialise the old and new fields
    reset_walk_module();

    while (walk_next_module(&module)) {
        if (module.key.location == location) {
            module.newUpRate = 0;

            write_module(module.key, &module);
        }
    }
    finish_walk_module();

    // Step - run through cables and see if to module needs up-rating

    bool changesMade = false;

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
            tModuleKey    fromModuleKey = {cable.key.location, cable.key.moduleFromIndex};
            tModuleKey    toModuleKey   = {cable.key.location, cable.key.moduleToIndex};

            if ((fromModuleKey.location == location) && (toModuleKey.location == location)) {
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
                                changesMade = true;
                            } else if ((fromModule.connector[fromConnIndex].type == connectorTypeAudio) && (toModule.connector[toConnIndex].type != connectorTypeAudio)) {
                                //LOG_DEBUG("From module from is audio and to is control\n");
                                toModule.newUpRate = 1;
                                write_module(toModuleKey, &toModule);
                                changesMade = true;
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
        if (module.key.location == location) {
            if (module.newUpRate != module.upRate) {
                module.upRate = module.newUpRate;

                write_module(module.key, &module);

                tMessageContent messageContent = {0};
                messageContent.cmd                  = eMsgCmdSetModuleUpRate;
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

    messageContent.cmd                  = eMsgCmdMoveModule;
    messageContent.moduleData.moduleKey = module->key;
    messageContent.moduleData.row       = module->row;
    messageContent.moduleData.column    = module->column;
    msg_send(&gCommandQueue, &messageContent);
}

void init_params_on_module(tModule * module, uint32_t location, uint32_t variation) {
    uint32_t        locationListIndex = 0;
    uint32_t        paramIndex        = 0;
    uint32_t        numParams         = module_param_count(module->type);
    tMessageContent messageContent    = {0};

    if (location != gLocation) {
        return;
    }

    for (locationListIndex = 0; locationListIndex < array_size_param_location_list(); locationListIndex++) {
        if (paramLocationList[locationListIndex].moduleType == module->type) {
            module->param[variation][paramIndex].value = paramLocationList[locationListIndex].defaultValue;

            write_module(module->key, module);

            for (int i = 0; i < NUM_VARIATIONS; i++) {
                messageContent.cmd                 = eMsgCmdSetValue;
                messageContent.paramData.moduleKey = module->key;
                messageContent.paramData.param     = paramIndex;
                messageContent.paramData.variation = i;
                messageContent.paramData.value     = module->param[variation][paramIndex].value;

                msg_send(&gCommandQueue, &messageContent);
            }

            paramIndex++;

            if (paramIndex >= numParams) {
                break;
            }
        }
    }
}

void init_params_on_module_all_variations(tModule * module, uint32_t location) {
    if (location != gLocation) {
        return;
    }

    for (uint32_t variation = 0; variation < NUM_VARIATIONS; variation++) {
        init_params_on_module(module, location, variation);
    }
}

void handle_button(tButtonId buttonId) {
    switch (buttonId) {
        case vaButtonId:
        {
            gLocation                                     = locationVa;
            gMainButtonArray[buttonId].backgroundColour   = (tRgb)RGB_GREEN_ON;
            gMainButtonArray[fxButtonId].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
            set_x_scroll_bar(0); // or different scroll positions for va and fx!?
            set_y_scroll_bar(0);
            break;
        }
        case fxButtonId:
        {
            gLocation                                     = locationFx;
            gMainButtonArray[buttonId].backgroundColour   = (tRgb)RGB_GREEN_ON;
            gMainButtonArray[vaButtonId].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
            set_x_scroll_bar(0);
            set_y_scroll_bar(0);
            break;
        }
        case openReadFileButtonId:
        {
            gShowOpenFileReadDialogue = true;
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
        {
            uint32_t variation = (uint32_t)buttonId - (uint32_t)variation1ButtonId;

            gVariation = variation;

            for (uint32_t variation = 0; variation < NUM_GUI_VARIATIONS; variation++) {
                gMainButtonArray[(uint32_t)variation1ButtonId + variation].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
            }

            gMainButtonArray[buttonId].backgroundColour = (tRgb)RGB_GREEN_ON;

            tMessageContent messageContent = {0};
            messageContent.cmd                     = eMsgCmdSelectVariation;
            messageContent.variationData.variation = variation;
            msg_send(&gCommandQueue, &messageContent);

            break;
        }
        case initParamsButtonId:
        {
            tModule module      = {0};
            bool    validModule = false;

            reset_walk_module();

            do {
                validModule = walk_next_module(&module);

                if (validModule) {
                    init_params_on_module(&module, gLocation, gVariation); // TODO: take init value from the 9th (init) variation, or at least check our init values are the same
                }
            } while (validModule);

            finish_walk_module();

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
        if ((walk.column == module.column) && (walk.key.location == key.location)) {
            if (walk.key.index != key.index) {
                if ((module.row > walk.row) && (module.row < walk.row + gModuleProperties[walk.type].height)) {
                    module.row = walk.row + gModuleProperties[walk.type].height;
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
        if ((walk.column == module.column) && (walk.key.location == key.location)) {
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
            if ((walk.column == module.column) && (walk.key.location == key.location)) {
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
    tModule module     = {0};
    tCable  walk       = {0};
    int     outIndex   = -1;
    int     inIndex    = -1;
    bool    deleteWalk = false;

    if (gContextMenu.moduleKey.location == gLocation) {
        read_module(gContextMenu.moduleKey, &module);

        reset_walk_cable();

        while (walk_next_cable(&walk)) {
            deleteWalk = false;

            if (walk.key.location == gContextMenu.moduleKey.location) {
                switch (module.connector[gContextMenu.connectorIndex].dir) {
                    case connectorDirOut:
                        outIndex = find_io_count_from_index(&module, connectorDirOut, gContextMenu.connectorIndex);
                        break;
                    case connectorDirIn:
                        inIndex = find_io_count_from_index(&module, connectorDirIn, gContextMenu.connectorIndex);
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
                    messageContent.cableData.location             = gLocation;
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
    tModule module     = {0};
    tCable  walk       = {0};
    bool    deleteWalk = false;

    if (gContextMenu.moduleKey.location == gLocation) {
        read_module(gContextMenu.moduleKey, &module);

        reset_walk_cable();

        while (walk_next_cable(&walk)) {
            deleteWalk = false;

            if (walk.key.location == gContextMenu.moduleKey.location) {
                if (walk.key.moduleFromIndex == gContextMenu.moduleKey.index) {
                    deleteWalk = true;
                } else if (walk.key.moduleToIndex == gContextMenu.moduleKey.index) {
                    deleteWalk = true;
                }

                if (deleteWalk == true) {
                    tMessageContent messageContent = {0};

                    messageContent.cmd                            = eMsgCmdDeleteCable;
                    messageContent.cableData.location             = gLocation;
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
        tMessageContent messageContent = {0};

        messageContent.cmd                  = eMsgCmdDeleteModule;
        messageContent.moduleData.moduleKey = module.key;

        msg_send(&gCommandQueue, &messageContent);

        delete_module(gContextMenu.moduleKey, doFreeYes);

        update_module_up_rates();
    }
}

uint32_t find_unique_module_id(uint32_t location) {
    tModuleKey key    = {0};
    tModule    module = {0};
    uint32_t   i      = 0;

    key.location = location;

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
        val  = coord.x - area.coord.x;
        val += calc_scroll_x();
        val /= MODULE_X_SPAN;
        val /= get_zoom_factor();

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

void menu_action_create(int index) {
    if (gContextMenu.items[index].param != 0) {
        tModule         module         = {0};
        tMessageContent messageContent = {0};
        int32_t         uniqueIndex    = 0;

        module.key.location = gLocation;
        uniqueIndex         = find_unique_module_id(module.key.location);

        if (uniqueIndex > 0) {
            module.key.index = (uint32_t)uniqueIndex;
            module.type      = (tModuleType)gContextMenu.items[index].param;
            convert_mouse_coord_to_module_column_row(&module.column, &module.row, gContextMenu.coord);
            allocate_module_parameters(&module, module_param_count(module.type));
            allocate_module_connectors(&module, module_connector_count(module.type));

            strncpy(module.name, gModuleProperties[module.type].name, sizeof(module.name));
            module.name[sizeof(module.name) - 1] = '\0';

            messageContent.cmd                  = eMsgCmdWriteModule;
            messageContent.moduleData.moduleKey = module.key;
            messageContent.moduleData.type      = module.type;
            messageContent.moduleData.row       = module.row;
            messageContent.moduleData.column    = module.column;
            messageContent.moduleData.colour    = module.colour;
            messageContent.moduleData.upRate    = module.upRate;
            messageContent.moduleData.isLed     = module.isLed;
            messageContent.moduleData.unknown1  = module.unknown1;
            messageContent.moduleData.modeCount = module_mode_count(module.type);

            for (int i = 0; i < module_mode_count(module.type); i++) {
                messageContent.moduleData.mode[i] = module.mode[i].value;
            }

            strncpy(messageContent.moduleData.name, module.name, sizeof(messageContent.moduleData.name));
            messageContent.moduleData.name[sizeof(messageContent.moduleData.name) - 1] = '\0';

            msg_send(&gCommandQueue, &messageContent);

            write_module(module.key, &module);

            init_params_on_module_all_variations(&module, gLocation);

            shift_modules_down(module.key);
        }
    } else {
        gContextMenu.items = gContextMenu.items[index].subMenu;

        if (gContextMenu.items != NULL) {
            gContextMenu.active = true;
        }
    }
}

void open_module_area_context_menu(tCoord coord) {
    static tMenuItem ioMenuItems[] = {
        {"Create Keyboard", menu_action_create, moduleTypeKeyboard, NULL},
        {"Create 2-Out",    menu_action_create, moduleType2toOut,   NULL},
        {"Create 4-Out",    menu_action_create, moduleType4toOut,   NULL},
        {"Create Device",    menu_action_create, moduleTypeDevice,   NULL},
        {"Create Name Bar", menu_action_create, moduleTypeName,     NULL},
        {NULL,              NULL,                                0, NULL}         // End of menu
    };
    static tMenuItem noteMenuItems[] = {
        {"Create NoteQuant", menu_action_create, moduleTypeNoteQuant, NULL},
        {"Create NoteScaler", menu_action_create, moduleTypeNoteScaler, NULL},
        {NULL,                 NULL,                               0, NULL}       // End of menu
    };
    static tMenuItem oscMenuItems[] = {
        {"Create Osc B",       menu_action_create, moduleTypeOscB,    NULL},
        {"Create Osc C",       menu_action_create, moduleTypeOscC,    NULL},
        {"Create Osc Shape B", menu_action_create, moduleTypeOscShpB, NULL},
        {"Create Osc String",  menu_action_create, moduleTypeOscString, NULL},
        {"Create Noise",  menu_action_create, moduleTypeNoise, NULL},
        {"Create Met Noise",  menu_action_create, moduleTypeMetNoise, NULL},
        {"Create DrumSynth",  menu_action_create, moduleTypeDrumSynth, NULL},
        {"Create Osc Master",       menu_action_create, moduleTypeOscMaster,    NULL},
        {NULL,                 NULL,                               0, NULL}       // End of menu
    };
    static tMenuItem lfoMenuItems[] = {
        {"Create LFO A", menu_action_create, moduleTypeLfoA, NULL},
        {"Create LFO C", menu_action_create, moduleTypeLfoC, NULL},
        {"Create LFO Shp A", menu_action_create, moduleTypeLfoShpA, NULL},
        {"Create ClkGen", menu_action_create, moduleTypeClkGen, NULL},
        {NULL,              NULL,                               0, NULL}          // End of menu
    };
    static tMenuItem envMenuItems[] = {
        {"Create Env ADSR", menu_action_create, moduleTypeEnvADSR, NULL},
        {"Create Env AHD",  menu_action_create, moduleTypeEnvAHD,  NULL},
        {"Create Env H",    menu_action_create, moduleTypeEnvH,    NULL},
        {"Create Env D",    menu_action_create, moduleTypeEnvD,    NULL},
        {"Create Env Multi",menu_action_create, moduleTypeEnvMulti,NULL},
        {NULL,              NULL,                               0, NULL}          // End of menu
    };
    static tMenuItem filterMenuItems[] = {
        {"Create LP Filter",      menu_action_create,                    0, NULL},
        {"Create Nord Filter",    menu_action_create, moduleTypeFltNord,    NULL},
        {"Create Classic Filter", menu_action_create, moduleTypeFltClassic, NULL},
        {"Create Multi Filter",   menu_action_create, moduleTypeFltMulti,   NULL},
        {"Create Static Filter",   menu_action_create, moduleTypeFltStatic,   NULL},
        {"Create FltVoice",       menu_action_create, moduleTypeFltVoice,   NULL},
        {"Create Eq 2-band",      menu_action_create, moduleTypeEq2Band, NULL},
        {"Create Eq 3-band",      menu_action_create, moduleTypeEq3band, NULL},
        {NULL,                    NULL,                                  0, NULL} // End of menu
    };
    static tMenuItem levelMenuItems[] = {
        {"Create Constant",    menu_action_create,  moduleTypeConstant, NULL},
        {"Create ConstSwT",    menu_action_create,  moduleTypeConstSwT, NULL},
        {"Create CompLev",    menu_action_create,  moduleTypeCompLev, NULL},
        {"Create LevAmp",     menu_action_create,  moduleTypeLevAmp,  NULL},
        {"Create LevMult",     menu_action_create,  moduleTypeLevMult,  NULL},
        {"Create EnvFollow",     menu_action_create,  moduleTypeEnvFollow,  NULL},
        {NULL,                 NULL,                                 0, NULL}       // End of menu
    };

    static tMenuItem switchMenuItems[] = {
        {"Create SwOnOffM",   menu_action_create,  moduleTypeSwOnOffM, NULL},
        {"Create SwOnOffT",   menu_action_create,  moduleTypeSwOnOffT, NULL},
        {"Create Sw4-1",   menu_action_create,  moduleTypeSw4to1, NULL},
        {"Create Sw1-8",   menu_action_create,  moduleTypeSw1to8, NULL},
        {"Create Mux8-1X",   menu_action_create,  moduleTypeMux8to1X, NULL},
        {"Create S&H",   menu_action_create,  moduleTypeSandH, NULL},
        {NULL,                 NULL,                               0, NULL}       // End of menu
    };
    static tMenuItem shaperMenuItems[] = {
        {"Create Saturate",   menu_action_create,  moduleTypeSaturate, NULL},
        {"Create Clip",   menu_action_create,  moduleTypeClip, NULL},
        {"Create OverDrive",   menu_action_create,  moduleTypeOverdrive, NULL},
        {"Create ShpExp",     menu_action_create,  moduleTypeShpExp,   NULL},
        {"Create WaveWrap",     menu_action_create,  moduleTypeWaveWrap,   NULL},
        {"Create ShpStatic",     menu_action_create,  moduleTypeShpStatic,   NULL},
        {"Create Rect",     menu_action_create,  moduleTypeRect,   NULL},
        {NULL,                 NULL,                               0, NULL}       // End of menu
    };
    static tMenuItem mixerMenuItems[] = {
        {"Create Mixer 4-1 C", menu_action_create, moduleTypeMix4to1C, NULL},
        {"Create Mixer 2-1 B", menu_action_create, moduleTypeMix2to1B, NULL},
        {"Create Mixer 8-1 B", menu_action_create, moduleTypeMix8to1B, NULL},
        {"Create MixStereo",   menu_action_create, moduleTypeMixStereo,NULL},
        {"Create X-Fade",      menu_action_create, moduleTypeXtoFade,  NULL},
        {"Create Pan",         menu_action_create, moduleTypePan,      NULL},
        {NULL,                 NULL,                                0, NULL}      // End of menu
    };
    static tMenuItem logicMenuItems[] = {
        {"Create Invert",   menu_action_create,  moduleTypeInvert, NULL},
        {"Create Pulse",   menu_action_create,  moduleTypePulse, NULL},
        {"Create Delay",   menu_action_create,  moduleTypeDelay, NULL},
        {"Create Gate",   menu_action_create,  moduleTypeGate, NULL},
        {"Create ClkDiv",   menu_action_create,  moduleTypeClkDiv, NULL},
        {NULL,                 NULL,                               0, NULL}       // End of menu
    };
    static tMenuItem fxMenuItems[] = {
        {"Create Compressor", menu_action_create,  moduleTypeCompress, NULL},
        {"Create Reverb",     menu_action_create,  moduleTypeReverb,   NULL},
        {"Create Scratch",     menu_action_create,  moduleTypeScratch,   NULL},
        {NULL,                NULL,                               0,   NULL}       // End of menu
    };
    static tMenuItem moduleMenuItems[] = {
        {"Create In/Out",   menu_action_create, 0, ioMenuItems    },
        {"Create Osc",      menu_action_create, 0, oscMenuItems   },
        {"Create Random",   menu_action_create, 0, NULL           },
        {"Create Filter",   menu_action_create, 0, filterMenuItems},
        {"Create Delay",    menu_action_create, 0, NULL           },
        {"Create Level",    menu_action_create, 0, levelMenuItems },
        {"Create Switch",   menu_action_create, 0, switchMenuItems},
        {"Create Sequence", menu_action_create, 0, NULL           },
        {"Create Note",     menu_action_create, 0, noteMenuItems  },
        {"Create LFO",      menu_action_create, 0, lfoMenuItems   },
        {"Create Env",      menu_action_create, 0, envMenuItems   },
        {"Create FX",       menu_action_create, 0, fxMenuItems    },
        {"Create Shaper",   menu_action_create, 0, shaperMenuItems},
        {"Create Mixer",    menu_action_create, 0, mixerMenuItems },
        {"Create Logic",    menu_action_create, 0, logicMenuItems },
        {"Create Midi",     menu_action_create, 0, NULL           },
        {NULL,              NULL,               0, NULL           } // End of menu
    };
    static tMenuItem menuItems[] = {
        {"Create module", menu_action_create, 0, moduleMenuItems},
        {NULL,            NULL,               0, NULL           }  // End of menu
    };

    // Store menu position
    gContextMenu.coord  = coord;
    gContextMenu.items  = menuItems;
    gContextMenu.active = true;
}

void open_connector_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t connectorIndex) {
    static tMenuItem menuItems[] = {
        {"Delete cable", menu_action_delete_cable, 0, NULL},
        {NULL,           NULL,                     0, NULL}         // End of menu
    };

    // Store menu position
    gContextMenu.coord          = coord;
    gContextMenu.items          = menuItems;
    gContextMenu.moduleKey      = moduleKey;
    gContextMenu.connectorIndex = connectorIndex;
    gContextMenu.active         = true;
}

void open_module_context_menu(tCoord coord, tModuleKey moduleKey) {
    static tMenuItem menuItems[] = {
        {"Delete module", menu_action_delete_module, 0, NULL},
        {NULL,            NULL,                      0, NULL}       // End of menu
    };

    // Store menu position
    gContextMenu.coord     = coord;
    gContextMenu.items     = menuItems;
    gContextMenu.moduleKey = moduleKey;
    gContextMenu.active    = true;
}

bool handle_module_click(tCoord coord, int button) {
    bool        retVal     = false;
    uint32_t    paramCount = 0;
    tParamType2 paramType2 = paramType2Dial;
    uint32_t    range      = 0;

    // Since morph parameters are in top banner area, no longer need to check if (!within_rectangle(coord, module_area()))

    reset_walk_module();
    tModule module = {0};

    while (walk_next_module(&module) && (retVal == false)) {
        if (module.key.location == gLocation || module.key.location == locationMorph) {
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
                tParam * param = &module.param[gVariation][i];

                if (within_rectangle(coord, param->rectangle)) {
                    if (button == GLFW_MOUSE_BUTTON_LEFT) {
                        if (module.key.location == locationMorph) {  // TODO: See if we can roll count into standard mechanism and pre-create the morph modules - maybe create new types at end of list?
                            if (i < NUM_MORPHS) {
                                paramType2 = paramType2Dial;
                            } else {
                                paramType2 = paramType2Toggle;
                            }
                        } else {
                            paramType2 = (paramLocationList[param->paramRef].type2);
                        }

                        if (paramType2 == paramType2Dial) {
                            gParamDragging.moduleKey.index    = module.key.index;
                            gParamDragging.moduleKey.location = module.key.location;
                            gParamDragging.type3              = paramType3Param;
                            gParamDragging.param              = i;
                            gParamDragging.active             = true;

                            if (module.key.location == locationMorph) {
                                gMorphGroupFocus = i;
                            }
                            retVal = true;
                        } else {
                            if (module.key.location == locationMorph) {   // TODO: See if we can roll count into standard mechanism and pre-create the morph modules - maybe create new types at end of list?
                                range = 2;
                            } else {
                                range = paramLocationList[param->paramRef].range;
                            }
                            param->value = (param->value + 1) % range;
                            write_module(module.key, &module);

                            tMessageContent messageContent = {0};
                            messageContent.cmd                 = eMsgCmdSetValue;
                            messageContent.paramData.moduleKey = module.key;
                            messageContent.paramData.param     = i;
                            messageContent.paramData.variation = gVariation;
                            messageContent.paramData.value     = param->value;

                            msg_send(&gCommandQueue, &messageContent);
                            retVal = true;
                        }
                    }
                }
            }

            if (retVal == false) {
                // Deal with click on mode
                for (int i = 0; (i < module.modeCount) && (retVal == false); i++) {
                    tMode * mode = &module.mode[i];

                    if (within_rectangle(coord, module.mode[i].rectangle)) {
                        if (button == GLFW_MOUSE_BUTTON_LEFT) {
                            if ((modeLocationList[mode->modeRef].type2) == paramType2Dial) {
                                gParamDragging.moduleKey.index    = module.key.index;
                                gParamDragging.moduleKey.location = module.key.location;
                                gParamDragging.type3              = paramType3Mode;
                                gParamDragging.mode               = i;
                                gParamDragging.active             = true;
                                retVal                            = true;
                            } else {
                                /*param->value = (param->value + 1) % paramLocationList[param->paramRef].range;
                                 * write_module(module.key, &module);
                                 *
                                 * tMessageContent messageContent = {0};
                                 * messageContent.cmd                 = eMsgCmdSetValue;
                                 * messageContent.paramData.moduleKey = module.key;
                                 * messageContent.paramData.mode     = i;
                                 * messageContent.paramData.variation = 0;
                                 * messageContent.paramData.value     = param->value;
                                 *
                                 * msg_send(&gCommandQueue, &messageContent);
                                 * retVal = true;*/
                            }
                        }
                    }
                }
            }

            if (retVal == false) {
                // Deal with click on connector
                for (int i = 0; (i < module_connector_count(module.type)) && (retVal == false); i++) {
                    if (within_rectangle(coord, module.connector[i].rectangle)) {
                        gCableDrag.fromModuleKey = module.key;

                        if (button == GLFW_MOUSE_BUTTON_LEFT) {
                            gCableDrag.fromConnectorIndex = i;   // Index into array of connectors

                            convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, coord);

                            gCableDrag.active = true;
                            retVal            = true;
                        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                            open_connector_context_menu(coord, module.key, i);
                            finish_walk_module();
                            retVal = true;
                        }
                    }
                }
            }

            // Deal with click on module
            if (retVal == false) {
                if (within_rectangle(coord, module.dragArea)) {
                    if (button == GLFW_MOUSE_BUTTON_LEFT) {
                        tModule tmpModule = {0};

                        read_module(module.key, &tmpModule);
                        delete_module(module.key, doFreeNo);
                        write_module(tmpModule.key, &tmpModule);
                        gModuleDrag.moduleKey = tmpModule.key;
                        gModuleDrag.active    = true;
                        retVal                = true;
                    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                        open_module_context_menu(coord, module.key);
                        finish_walk_module();
                        retVal = true;
                    }
                }
            }
        }
    }
    finish_walk_module();
    return retVal;
}

bool handle_module_area_click(tCoord coord, int button) {
    if (!within_rectangle(coord, module_area())) {
        return false;
    }
    open_module_area_context_menu(coord);

    return true;
}

void set_x_scroll_bar(double x) {
    gScrollState.xBar = clamp_scroll_bar(x, get_render_width());
    set_x_scroll_percent(get_scroll_bar_percent(gScrollState.xBar, get_render_width() / GLOBAL_GUI_SCALE));
}

void set_y_scroll_bar(double y) {
    gScrollState.yBar = clamp_scroll_bar(y, get_render_height());
    set_y_scroll_percent(get_scroll_bar_percent(gScrollState.yBar, get_render_height() / GLOBAL_GUI_SCALE));
}

bool handle_context_menu_click(tCoord coord) {
    if ((gContextMenu.active == false) || (gContextMenu.items == NULL)) {
        return false;
    }
    double size        = 0.0;
    double largestSize = 0.0;
    double itemHeight  = STANDARD_TEXT_HEIGHT;

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        size = get_text_width(gContextMenu.items[i].label, itemHeight);

        if (size > largestSize) {
            largestSize = size;
        }
    }

    int yOffset = 0;

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        tRectangle itemRect = {
            {gContextMenu.coord.x, gContextMenu.coord.y + yOffset},
            {
                largestSize + (5 * 2), itemHeight + (5 * 2)
            }
        };

        if (within_rectangle(coord, itemRect)) {
            gContextMenu.active = false; // Close the current menu

            if (gContextMenu.items[i].action != NULL) {
                gContextMenu.items[i].action(i); // Call the selected action
            }
            return true;
        }
        yOffset += itemHeight + (5 * 2);
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
}

void mouse_button(GLFWwindow * window, int button, int action, int mods) {
    int    width    = 0;
    int    height   = 0;
    tCoord coord    = {0};
    bool   quitLoop = false;

    glfwGetWindowSize(window, &width, &height);

    get_global_gui_scaled_mouse_coord(&coord);

    //LOG_DEBUG("button=%d action=%d mods=%d\n", button, action, mods);

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (gContextMenu.active == false) { // Don't check left button press on non-menu items
                if (!handle_scrollbar_click(coord)) {
                    handle_module_click(coord, button);
                }
            }
        }
    } else if (action == GLFW_RELEASE) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            bool found = false;

            for (uint32_t i = 0; i < array_size_main_button_array(); i++) {
                if (within_rectangle(coord, gMainButtonArray[i].rectangle)) {
                    handle_button((tButtonId)i);
                    found = true;
                }
            }

            if (found == false) {
                if (gContextMenu.active) {
                    if (!handle_context_menu_click(coord)) {
                        gContextMenu.active = false;  // Close if clicked outside - TODO: think if this is the right thing to do here
                    }
                } else if (gModuleDrag.active == true) {
                    shift_modules_down(gModuleDrag.moduleKey);
                } else if (gCableDrag.active) {
                    tModule   fromModule = {0};
                    tModule   toModule   = {0};
                    tCableKey cableKey   = {0};
                    tCable    cable      = {0};

                    reset_walk_module();

                    while (walk_next_module(&toModule) && !quitLoop) {
                        if (toModule.key.location == gLocation) {
                            for (int i = 0; i < module_connector_count(toModule.type); i++) {
                                if (!within_rectangle(coord, toModule.connector[i].rectangle)) {
                                    continue;
                                }
                                read_module(gCableDrag.fromModuleKey, &fromModule);
                                set_up_cable_key(&cableKey, &fromModule, &toModule, i);

                                swap_cable_to_from_if_needed(&cableKey, &fromModule, &toModule, i);

                                // Prevent self-connections and invalid connections
                                if (  (cableKey.moduleFromIndex == cableKey.moduleToIndex && gCableDrag.fromConnectorIndex == i)
                                   || (  fromModule.connector[gCableDrag.fromConnectorIndex].dir == connectorDirOut
                                      && toModule.connector[i].dir == connectorDirOut)) {
                                    quitLoop = true;
                                    break;
                                }
                                cable.colour = 0; // TODO: choose colour from menu or calculate
                                write_cable(cableKey, &cable);

                                tMessageContent messageContent = {0};

                                messageContent.cmd                            = eMsgCmdWriteCable;
                                messageContent.cableData.location             = gLocation;
                                messageContent.cableData.moduleFromIndex      = cableKey.moduleFromIndex;
                                messageContent.cableData.connectorFromIoIndex = cableKey.connectorFromIoCount;
                                messageContent.cableData.moduleToIndex        = cableKey.moduleToIndex;
                                messageContent.cableData.connectorToIoIndex   = cableKey.connectorToIoCount;
                                messageContent.cableData.linkType             = cableKey.linkType;
                                messageContent.cableData.colour               = cable.colour;
                                msg_send(&gCommandQueue, &messageContent);

                                quitLoop = true;
                                break;
                            }
                        }
                    }
                    finish_walk_module();
                    update_module_up_rates();
                }
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (!handle_module_click(coord, button)) {
                handle_module_area_click(coord, button);
            }
        }
        stop_dragging(); // Could do on dragging complete events, but arguably safer here
    }
    gReDraw = true;
}

void cursor_pos(GLFWwindow * window, double x, double y) {
    tCoord          coord          = {0};
    int             width          = 0;
    int             height         = 0;
    double          angle          = 0.0;
    uint32_t        range          = 0;
    uint32_t        value          = 0;
    tModule         module         = {0};
    tMessageContent messageContent = {0};
    bool            noAction       = false;
    tParamType2     paramType2     = paramType2Dial;

    get_global_gui_scaled_mouse_coord(&coord);
    x = coord.x;
    y = coord.y;

    // Scale x and y to match intended rendering window
    //glfwGetWindowSize(window, &width, &height);
    //x = (x * (double)get_render_width()) / (double)width;
    //y = (y * (double)get_render_height()) / (double)height;

    if (gScrollState.yBarDragging == true) {
        set_y_scroll_bar(y);
    } else if (gScrollState.xBarDragging == true) {
        set_x_scroll_bar(x);
    } else if (gParamDragging.active == true) {
        read_module(gParamDragging.moduleKey, &module);

        switch (gParamDragging.type3) {
            case paramType3Param:

                if (module.key.location == locationMorph) {
                    paramType2 = paramType2Dial;
                } else {
                    paramType2 = paramLocationList[module.param[gVariation][gParamDragging.param].paramRef].type2;
                }

                if (paramType2 == paramType2Dial) {
                    if (module.key.location == locationMorph) {
                        range = 128;
                    } else {
                        range = paramLocationList[module.param[gVariation][gParamDragging.param].paramRef].range;
                    }
                    angle = calculate_mouse_angle((tCoord){x, y}, module.param[gVariation][gParamDragging.param].rectangle);                                                            // possible add half size
                    value = angle_to_value(angle, range);

                    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) != GLFW_PRESS) {
                        if (module.param[gVariation][gParamDragging.param].value != value) {
                            module.param[gVariation][gParamDragging.param].value = value;

                            write_module(gParamDragging.moduleKey, &module);         // Write new value into parameter

                            messageContent.cmd                 = eMsgCmdSetValue;
                            messageContent.paramData.moduleKey = gParamDragging.moduleKey;
                            messageContent.paramData.param     = gParamDragging.param;
                            messageContent.paramData.variation = gVariation;
                            messageContent.paramData.value     = value;
                            msg_send(&gCommandQueue, &messageContent);
                        }
                    } else {
                        if (module.param[gVariation][gParamDragging.param].morphRange[gMorphGroupFocus] != value) {
                            if (value >= module.param[gVariation][gParamDragging.param].value) {
                                module.param[gVariation][gParamDragging.param].morphRange[gMorphGroupFocus] = value - module.param[gVariation][gParamDragging.param].value;
                            } else {
                                module.param[gVariation][gParamDragging.param].morphRange[gMorphGroupFocus] = 256 - (module.param[gVariation][gParamDragging.param].value - value);
                            }
                            write_module(gParamDragging.moduleKey, &module);         // Write new value into parameter
                            printf("Write to module %u variation %u\n", module.key.index, gVariation);

                            messageContent.cmd                       = eMsgCmdSetParamMorph;
                            messageContent.paramMorphData.moduleKey  = module.key;
                            messageContent.paramMorphData.param      = gParamDragging.param;
                            messageContent.paramMorphData.paramMorph = gMorphGroupFocus;
                            messageContent.paramMorphData.value      = module.param[gVariation][gParamDragging.param].morphRange[gMorphGroupFocus];
                            messageContent.paramMorphData.negative   = 0;
                            messageContent.paramMorphData.variation  = gVariation;
                            msg_send(&gCommandQueue, &messageContent);
                        }
                    }
                }
                break;
            case paramType3Mode:

                if (modeLocationList[module.mode[gParamDragging.param].modeRef].type2 == paramType2Dial) {
                    angle = calculate_mouse_angle((tCoord){x, y}, module.mode[gParamDragging.param].rectangle);                                                            // possible add half size
                    value = angle_to_value(angle, modeLocationList[module.mode[gParamDragging.param].modeRef].range);

                    if (module.mode[gParamDragging.mode].value != value) {
                        module.mode[gParamDragging.mode].value = value;

                        write_module(gParamDragging.moduleKey, &module);         // Write new value into parameter

                        messageContent.cmd                = eMsgCmdSetMode;
                        messageContent.modeData.moduleKey = gParamDragging.moduleKey;
                        messageContent.modeData.mode      = gParamDragging.mode;
                        messageContent.modeData.value     = value;
                        msg_send(&gCommandQueue, &messageContent);
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
        noAction = true;
    }

    // Limit re-draw/render if nothing's happened
    if (noAction == false) {
        gReDraw = true;
    }
}

void scroll_event(GLFWwindow * window, double x, double y) {
    const double zoomIncrement = 0.025;  // Zoom sensitivity
    double       zoomFactor    = 0.0;
    tCoord       mouseCoord    = {0};
    tRectangle   moduleArea    = module_area(); // Get the module display area
    int          width         = 0;
    int          height        = 0;

    glfwGetWindowSize(window, &width, &height);

    get_global_gui_scaled_mouse_coord(&mouseCoord);

    //mouseCoord.x = (mouseCoord.x * (double)get_render_width()) / (double)width;
    //mouseCoord.y = (mouseCoord.y * (double)get_render_height()) / (double)height;

    //LOG_DEBUG("Zoom = %f yEndMax = %f module area size = %f percent = %f\n", gZoomFactor, yEndMax, moduleArea.size.h, get_scroll_bar_percent(gScrollState.yBar, gRenderHeight));

    if (within_rectangle(mouseCoord, moduleArea)) {
        zoomFactor  = get_zoom_factor();
        zoomFactor += y * zoomIncrement;

        if (zoomFactor < 0.25) {
            zoomFactor = 0.25;
        } else if (zoomFactor > 2.0) {
            zoomFactor = 2.0;
        }
        set_zoom_factor(zoomFactor);
    }
    gReDraw = true;
}

void char_event(GLFWwindow * window, unsigned int value) {
    LOG_DEBUG("char=%d\n", value);
    gReDraw = true;
}

void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    LOG_DEBUG("key=%d scancode=%d action=%d mods=%d\n", key, scancode, action, mods);
    gReDraw = true;
}

#ifdef __cplusplus
}
#endif
