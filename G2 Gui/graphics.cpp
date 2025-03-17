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

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include "defs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "usbComms.h"
#include "graphics.h"
#include "utilsGraphics.h"
#include "dataBase.h"
#include "moduleGraphics.h"

static tScrollState      gScrollState  = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false};
static tContextMenu      gContextMenu  = {0};
static FT_Library        gLibrary      = {0};
static FT_Face           gFace         = {0};
static GLFWwindow *      gWindow       = NULL;
static tDialDragging     gDialDragging = {0};
static tModuleDragging   gModuleDrag   = {0};
static tCableDragging    gCableDrag    = {0};

extern tMessageQueue     gCommandQueue;
extern tModuleProperties gModuleProperties[];

void framebuffer_size_callback(GLFWwindow * window, int width, int height) {
    glViewport(0, 0, width, height);
}

void error_callback(int error, const char * description) {
    fprintf(stderr, "Error [%d]: %s\n", error, description);
}

void menu_action_delete_cable(void) {
    tModule module = {0};
    tCable walk = {0};
    int outIndex = -1;
    int inIndex = -1;
    bool deleteWalk = false;
    
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
                messageContent.cableData.moduleFromIndex      = walk.key.moduleFromIndex;
                messageContent.cableData.connectorFromIoIndex = walk.key.connectorFromIoCount;
                messageContent.cableData.moduleToIndex        = walk.key.moduleToIndex;
                messageContent.cableData.connectorToIoIndex   = walk.key.connectorToIoCount;
                messageContent.cableData.linkType             = walk.key.linkType;

                msg_send(&gCommandQueue, &messageContent);
                
                delete_cable(walk.key);
                // Todo: assess uprate for modules we were connected to. Maybe even run through all of them!?
            }
        }
    }
}

void menu_action_delete_module(void) {
    tModule module = {0};
    tCable walk = {0};
    int outIndex = -1;
    int inIndex = -1;
    bool deleteWalk = false;
    
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
                messageContent.cableData.moduleFromIndex      = walk.key.moduleFromIndex;
                messageContent.cableData.connectorFromIoIndex = walk.key.connectorFromIoCount;
                messageContent.cableData.moduleToIndex        = walk.key.moduleToIndex;
                messageContent.cableData.connectorToIoIndex   = walk.key.connectorToIoCount;
                messageContent.cableData.linkType             = walk.key.linkType;

                msg_send(&gCommandQueue, &messageContent);
                
                delete_cable(walk.key);
                // Todo: assess uprate for modules we were connected to. Maybe even run through all of them!?
            }
        }
    }
 
    tMessageContent messageContent = {0};

    messageContent.cmd                  = eMsgCmdDeleteModule;
    messageContent.moduleData.moduleKey = module.key;

    msg_send(&gCommandQueue, &messageContent);
    
    delete_module(gContextMenu.moduleKey, doFreeYes);
}

void open_connector_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t connectorIndex) {
    static tMenuItem menuItems[] = {
        {"Delete cable", menu_action_delete_cable},
        {"Dummy option", NULL                    },
        {NULL,           NULL                    }         // End of menu
    };

    // Store menu position
    gContextMenu.active = true;
    gContextMenu.coord  = coord;
    gContextMenu.items  = menuItems;
    gContextMenu.moduleKey  = moduleKey;
    gContextMenu.connectorIndex  = connectorIndex;
}

void open_module_context_menu(tCoord coord, tModuleKey moduleKey) {
    static tMenuItem menuItems[] = {
        {"Delete module", menu_action_delete_module},
        {"Dummy option", NULL                    },
        {NULL,           NULL                    }         // End of menu
    };

    // Store menu position
    gContextMenu.active = true;
    gContextMenu.coord  = coord;
    gContextMenu.items  = menuItems;
    gContextMenu.moduleKey  = moduleKey;
}

void render_context_menu(void) {
    if (!gContextMenu.active) {
        return;
    }
    double     size        = 0.0;
    double     largestSize = 0.0;
    tCoord     mouseCoord  = {0};
    tRectangle menuItem    = {0};
    double     itemHeight  = STANDARD_TEXT_HEIGHT;

    int        width, height;
    glfwGetWindowSize(gWindow, &width, &height);
    glfwGetCursorPos(gWindow, &mouseCoord.x, &mouseCoord.y);

    mouseCoord.x = (mouseCoord.x * (double)get_render_width()) / (double)width;
    mouseCoord.y = (mouseCoord.y * (double)get_render_height()) / (double)height;

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        size = get_text_width(gContextMenu.items[i].label, itemHeight);

        if (size > largestSize) {
            largestSize = size;
        }
    }

    int yOffset = 0;

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        menuItem = {{gContextMenu.coord.x, gContextMenu.coord.y + yOffset}, {largestSize + (5 * 2), itemHeight + (5 * 2)}};

        if (within_rectangle(mouseCoord, menuItem)) {
            set_rbg_colour({0.2, 0.6, 0.2});
        } else {
            set_rbg_colour({0.3, 0.3, 0.3});    // Background
        }
        render_rectangle(mainArea, menuItem);

        set_rbg_colour({0.9, 0.9, 0.9});    // White text
        render_text(mainArea, {{gContextMenu.coord.x + 5, gContextMenu.coord.y + 5 + yOffset}, {BLANK_SIZE, itemHeight}}, gContextMenu.items[i].label);
        yOffset += itemHeight + (5 * 2);
    }
}

bool handle_context_menu_click(tCoord coord) {
    if (!gContextMenu.active) {
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
            {largestSize,          itemHeight + 5                }};

        if (within_rectangle(coord, itemRect)) {
            if (gContextMenu.items[i].action != NULL) {
                gContextMenu.items[i].action(); // Call the selected action
            }
            gContextMenu.active = false;        // Close the menu
            return true;
        }
        yOffset += itemHeight + (5 * 2);
    }

    //gContextMenu.active = false;  // Close if clicked outside
    memset(&gContextMenu, 0, sizeof(gContextMenu)); // Clear everything (including active flag) to zero
    
    return false;
}

void set_x_scroll_bar(double x) {
    gScrollState.xBar = clamp_scroll_bar(x, get_render_width());
    set_x_scroll_percent(get_scroll_bar_percent(gScrollState.xBar, get_render_width()));
}

void set_y_scroll_bar(double y) {
    gScrollState.yBar = clamp_scroll_bar(y, get_render_height());
    set_y_scroll_percent(get_scroll_bar_percent(gScrollState.yBar, get_render_height()));
}

bool handle_scrollbar_click(tCoord coord) {
    tRectangle yScrollBar = {
        {(double)get_render_width() - SCROLLBAR_WIDTH,                         0.0},
        {SCROLLBAR_WIDTH,                              (double)get_render_height()}};

    tRectangle xScrollBar = {
        {                       0.0, (double)get_render_height() - SCROLLBAR_WIDTH},
        {(double)get_render_width(), SCROLLBAR_WIDTH                              }};

    if (within_rectangle(coord, yScrollBar)) {
        set_y_scroll_bar(coord.y);
        gScrollState.yBarDragging = true;
        return true;
    }

    if (within_rectangle(coord, xScrollBar)) {
        set_x_scroll_bar(coord.x);
        gScrollState.xBarDragging = true;
        return true;
    }
    return false;
}

void send_module_move_msg(tModule * module) {
    tMessageContent messageContent = {0};

    messageContent.cmd                  = eMsgCmdMoveModule;
    messageContent.moduleData.moduleKey = module->key;
    messageContent.moduleData.row       = module->row;
    messageContent.moduleData.column    = module->column;
    msg_send(&gCommandQueue, &messageContent);
}

bool param_type_is_dial(tParamType type) {
    switch (type) {
        case paramTypeFreq:
        case paramTypeResonance:
        case paramTypePitch:
        case paramTypeCommonDial:
            return true;

        default:
            return false;
    }
}

bool param_type_is_toggle(tParamType type) {
    switch (type) {
        case paramTypeKeyboardTrack:
        case paramTypeGainControl:
        case paramTypeBypass:
        case paramTypeFltClassicDb:
        case paramTypeFltMultiDb:
            return true;

        default:
            return false;
    }
}

bool handle_module_click(tCoord coord, int button) {
    if (!within_rectangle(coord, module_area())) {
        return false;
    }
    reset_walk_module();
    tModule module = {0};

    while (walk_next_module(&module)) {
        
        // Deal with click on param
        for (int i = 0; i < gModuleProperties[module.type].numParameters; i++) {
            tParam * param = &module.param[0][i];

            if (within_rectangle(coord, param->rectangle)) {
                if (button == GLFW_MOUSE_BUTTON_LEFT) {
                    if (param_type_is_dial(param->type) == true) {
                        gDialDragging.moduleKey.index    = module.key.index;
                        gDialDragging.moduleKey.location = module.key.location;
                        gDialDragging.variation          = 0;
                        gDialDragging.param              = i;
                        gDialDragging.active             = true;
                        return true;
                    } else if (param_type_is_toggle(param->type) == true) {
                        param->value = (param->value + 1) % param->range;
                        write_module(module.key, &module);
                        
                        tMessageContent messageContent = {0};
                        messageContent.cmd                 = eMsgCmdSetValue;
                        messageContent.paramData.moduleKey = module.key;
                        messageContent.paramData.param     = i;
                        messageContent.paramData.variation = 0;
                        messageContent.paramData.value     = param->value;
                        
                        msg_send(&gCommandQueue, &messageContent);
                        return true;
                    }
                }
            }
        }

        // Deal with click on connector
        for (int i = 0; i < gModuleProperties[module.type].numConnectors; i++) {
            if (within_rectangle(coord, module.connector[i].rectangle)) {
                tRectangle area = module_area();

                gCableDrag.fromModuleKey      = module.key;
                
                if (button == GLFW_MOUSE_BUTTON_LEFT) {
                    gCableDrag.fromConnectorIndex = i;   // Index into array of connectors
                    double     val  = 0;

                    // Todo: Use a function for this scaling etc.
                    val                            = coord.x - area.coord.x;
                    val                           += calc_scroll_x();
                    val                           /= get_zoom_factor();
                    gCableDrag.toConnector.coord.x = val;
                    val                            = coord.y - area.coord.y;
                    val                           += calc_scroll_y();
                    val                           /= get_zoom_factor();
                    gCableDrag.toConnector.coord.y = val;

                    gCableDrag.active = true;
                    return true;
                } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                    open_connector_context_menu(coord, module.key, i);
                    return true;
                }
            }
        }

        // Deal with click on module
        if (within_rectangle(coord, module.rectangle)) {
            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                // Take the module off the linked list and put on the end, which makes it render last and so render on the top
                tModule tmpModule = {0};
                read_module(module.key, &tmpModule);
                delete_module(module.key, doFreeNo);
                write_module(tmpModule.key, &tmpModule);
                gModuleDrag.moduleKey = tmpModule.key;
                gModuleDrag.active    = true;
                return true;
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                open_module_context_menu(coord, module.key);
                return true;
            }
        }
    }
    return false;
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
                            // ToDo - flag up the clash!
                        }
                        
                        write_module(walk.key, &walk);
                        send_module_move_msg(&module);
                    }
                }
            }
        }
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
    if (fromModule->connector[gCableDrag.fromConnectorIndex].dir == connectorDirIn
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

void mouse_button(GLFWwindow * window, int button, int action, int mods) {
    int    width, height;
    tCoord coord    = {0};
    bool   quitLoop = false;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &coord.x, &coord.y);

    coord.x = (coord.x * (double)get_render_width()) / (double)width;
    coord.y = (coord.y * (double)get_render_height()) / (double)height;

    //printf("button=%d action=%d mods=%d\n", button, action, mods);

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (gContextMenu.active) {
                if (!handle_context_menu_click(coord)) {
                    gContextMenu.active = false;  // Close if clicked outside
                }
            } else {
                if (!handle_scrollbar_click(coord)) {
                    handle_module_click(coord, button);
                }
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            handle_module_click(coord, button);
        }
    } else if (action == GLFW_RELEASE) {
        if (gModuleDrag.active == true) {
            shift_modules_down(gModuleDrag.moduleKey);
        }

        if (gCableDrag.active) {
            tModule   fromModule = {0};
            tModule   toModule   = {0};
            tCableKey cableKey   = {0};
            tCable    cable      = {0};

            reset_walk_module();

            while (walk_next_module(&toModule) && !quitLoop) {
                for (int i = 0; i < gModuleProperties[toModule.type].numConnectors; i++) {
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
                    cable.colour = 0; // Todo: choose colour from menu
                    write_cable(cableKey, &cable);

                    tMessageContent messageContent = {0};

                    messageContent.cmd                            = eMsgCmdWriteCable;
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
        gScrollState.yBarDragging = false;
        gScrollState.xBarDragging = false;
        memset(&gModuleDrag, 0, sizeof(gModuleDrag));     // Reset dragging state
        memset(&gDialDragging, 0, sizeof(gDialDragging)); // Reset dragging state
        memset(&gCableDrag, 0, sizeof(gCableDrag));       // Reset dragging state
    }
}

void adjust_scroll_for_drag(void) {
    double x = 0.0;
    double y = 0.0;
    double xAdjustAmount = 0.1;
    double yAdjustAmount = 0.2;
    double timeDelta = get_time_delta();
    tRectangle      area = module_area();
    
    glfwGetCursorPos(gWindow, &x, &y);
        
    xAdjustAmount *= timeDelta;
    yAdjustAmount *= timeDelta;
    
    if (x > (area.coord.x+area.size.w)) {
        gScrollState.xBar+=xAdjustAmount;
        set_x_scroll_bar(gScrollState.xBar);
    }
    if (x < (area.coord.x)) {
        gScrollState.xBar-=xAdjustAmount;
        set_x_scroll_bar(gScrollState.xBar);
    }
    if (y > (area.coord.y+area.size.h)) {
        gScrollState.yBar+=yAdjustAmount;
        set_y_scroll_bar(gScrollState.yBar);
    }
    if (y < (area.coord.y)) {
        gScrollState.yBar-=yAdjustAmount;
        set_y_scroll_bar(gScrollState.yBar);
    }
}

void cursor_pos(GLFWwindow * window, double x, double y) {
    int             width          = 0;
    int             height         = 0;
    double          angle          = 0.0;
    uint32_t        value          = 0;
    tModule         module         = {0};
    tMessageContent messageContent = {0};
    double          val            = 0;

    tRectangle      area = module_area();

    // Scale x and y to match intended rendering window
    glfwGetWindowSize(window, &width, &height);
    x = (x * (double)get_render_width()) / (double)width;
    y = (y * (double)get_render_height()) / (double)height;

    if (gScrollState.yBarDragging == true) {
        set_y_scroll_bar(y);
    } else if (gScrollState.xBarDragging == true) {
        set_x_scroll_bar(x);
    } else if (gDialDragging.active == true) {
        read_module(gDialDragging.moduleKey, &module);

        if (param_type_is_dial(module.param[gDialDragging.variation][gDialDragging.param].type) == true) {
            angle                                                            = calculate_mouse_angle({x, y}, module.param[gDialDragging.variation][gDialDragging.param].rectangle); // possible add half size
            value                                                            = angle_to_value(angle);
            module.param[gDialDragging.variation][gDialDragging.param].value = value;
        } else {
            printf("Unknown module type %u\n", module.param[gDialDragging.variation][gDialDragging.param].type);
            exit(1);
        }
        write_module(gDialDragging.moduleKey, &module);         // Write new value into parameter

        messageContent.cmd                 = eMsgCmdSetValue;
        messageContent.paramData.moduleKey = gDialDragging.moduleKey;
        messageContent.paramData.param     = gDialDragging.param;
        messageContent.paramData.variation = gDialDragging.variation;
        messageContent.paramData.value     = value;
        msg_send(&gCommandQueue, &messageContent);
    } else if (gModuleDrag.active == true) {
        read_module(gModuleDrag.moduleKey, &module);

        // Todo: Use a function for this scaling etc.

        val           = x - area.coord.x;
        val          += calc_scroll_x();
        val          /= MODULE_X_SPAN;
        val          /= get_zoom_factor();
        module.column = floor(val);
        val           = y - area.coord.y;
        val          += calc_scroll_y();
        val          /= MODULE_Y_SPAN;
        val          /= get_zoom_factor();
        module.row    = floor(val);
        if (module.row > 127) {
            module.row = 127;
        }
        if (module.column > 127) {
            module.column = 127;
        }
        write_module(gModuleDrag.moduleKey, &module);
        adjust_scroll_for_drag();
    } else if (gCableDrag.active == true) {
        
        // Todo: Use a function for this scaling etc.
        val                            = x - area.coord.x;
        val                           += calc_scroll_x();
        val                           /= get_zoom_factor();
        gCableDrag.toConnector.coord.x = val;
        val                            = y - area.coord.y;
        val                           += calc_scroll_y();
        val                           /= get_zoom_factor();
        gCableDrag.toConnector.coord.y = val;
        adjust_scroll_for_drag();
    }
}

void scroll_event(GLFWwindow * window, double x, double y) {
    const double zoomIncrement = 0.025;  // Zoom sensitivity
    double       zoomFactor    = 0.0;

    tCoord     mouseCoord = {0};
    tRectangle moduleArea = module_area(); // Get the module display area

    int        width, height;

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &mouseCoord.x, &mouseCoord.y);
    mouseCoord.x = (mouseCoord.x * (double)get_render_width()) / (double)width;
    mouseCoord.y = (mouseCoord.y * (double)get_render_height()) / (double)height;

    //printf("Zoom = %f yEndMax = %f module area size = %f percent = %f\n", gZoomFactor, yEndMax, moduleArea.size.h, get_scroll_bar_percent(gScrollState.yBar, gRenderHeight));

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
}

void char_event(GLFWwindow * window, unsigned int value) {
    printf("char=%d\n", value);
}

void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    printf("key=%d scancode=%d action=%d mods=%d\n", key, scancode, action, mods);
}

void render_scrollbars(GLFWwindow * window) {
    // Scrollbar background
    set_rbg_colour({0.7, 0.7, 0.7});
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, 0.0}, {SCROLLBAR_WIDTH, (double)get_render_height() - SCROLLBAR_MARGIN}});
    render_rectangle(mainArea, {{0.0, (double)get_render_height() - SCROLLBAR_WIDTH}, {(double)get_render_width() - SCROLLBAR_MARGIN, SCROLLBAR_WIDTH}});

    // Bottom right box
    set_rbg_colour(RGB_BACKGROUND_GREY);
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, (double)get_render_height() - SCROLLBAR_WIDTH}, {SCROLLBAR_WIDTH, SCROLLBAR_WIDTH}});

    // Scroll indicator blocks
    set_rbg_colour({0.9, 0.9, 0.9});
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, gScrollState.yBar - (SCROLLBAR_LENGTH / 2.0)}, {SCROLLBAR_WIDTH, SCROLLBAR_LENGTH}});
    render_rectangle(mainArea, {{gScrollState.xBar - (SCROLLBAR_LENGTH / 2.0), (double)get_render_height() - SCROLLBAR_WIDTH}, {SCROLLBAR_LENGTH, SCROLLBAR_WIDTH}});
}

void render_top_bar(void) {
    set_rbg_colour({0.5, 0.5, 0.5});
    render_rectangle_with_border(mainArea, {{0.0, 0.0}, {get_render_width() - SCROLLBAR_MARGIN, TOP_BAR_HEIGHT}});
}

void wake_glfw(void) {
    glfwPostEmptyEvent();
}

void setup_render_context(void) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, get_render_width(), get_render_height(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

void init_graphics(void) {
    tIntRectangle monitor      = {0};
    int           fbWidth      = 0;
    int           fbHeight     = 0;
    int           screenWidth  = 0;
    int           screenHeight = 0;
    
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    register_glfw_wake_cb(wake_glfw);   //TODO - possible also register for callback on new module load, then reset scrollbars etc.

    glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &monitor.coord.x, &monitor.coord.y, &monitor.size.w, &monitor.size.h);
    screenWidth  = monitor.coord.x + monitor.size.w;
    screenHeight = monitor.coord.y + monitor.size.h;

    gWindow = glfwCreateWindow((screenWidth * 3) / 4, (screenHeight * 3) / 4, "G2 Editor", NULL, NULL);

    if (!gWindow) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetWindowSizeLimits(gWindow, (screenWidth * 1) / 4, (screenHeight * 1) / 4, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowAspectRatio(gWindow, screenWidth, screenHeight);

    glfwMakeContextCurrent(gWindow);

    glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);
    glfwSwapInterval(1);

    glfwSetKeyCallback(gWindow, key_callback);
    glfwSetCharCallback(gWindow, char_event);
    glfwSetCursorPosCallback(gWindow, cursor_pos);
    glfwSetMouseButtonCallback(gWindow, mouse_button);
    glfwSetScrollCallback(gWindow, scroll_event);

    FT_Init_FreeType(&gLibrary);
    FT_New_Face(gLibrary, "/System/Library/Fonts/Supplemental/Arial.ttf", 0, &gFace);
    FT_Set_Char_Size(gFace, 0, 48 * 64, 300, 300);

    // Preload glyph textures
    if (!preload_glyph_textures("/System/Library/Fonts/Supplemental/Arial.ttf", 72.0f)) {
        printf("Failed to preload glyph textures.\n");
    }
    int renderWidth  = 0;
    int renderHeight = 0;
    glfwGetWindowSize(gWindow, &renderWidth, &renderHeight);
    set_render_width(renderWidth);
    set_render_height(renderHeight);

    setup_render_context();
}

void do_graphics_loop(void) {
    while (!glfwWindowShouldClose(gWindow)) {
        glClearColor(0.8, 0.8, 0.8, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_modules();
        render_cables();

        if (gCableDrag.active == true) {
            tModule module = {0};
            read_module(gCableDrag.fromModuleKey, &module);
            set_rbg_colour(RGB_WHITE);
            render_cable_from_to(module.connector[gCableDrag.fromConnectorIndex], gCableDrag.toConnector);
        }
        render_top_bar();
        render_scrollbars(gWindow);
        render_context_menu();
        
        // Swap buffers and look for events
        glfwSwapBuffers(gWindow);

        if ((gModuleDrag.active == true) || (gCableDrag.active == true)) {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(gWindow, &x, &y);
            cursor_pos(gWindow, x, y);  // Artificially do cursor_pos call for drag scrolling
            glfwWaitEventsTimeout(0.016);
        } else {
            glfwWaitEvents();
        }
    }
}

void clean_up_graphics(void) {
    // Clean up
    FT_Done_Face(gFace);
    FT_Done_FreeType(gLibrary);
    free_textures();

    glfwDestroyWindow(gWindow);
    glfwTerminate();
}

#ifdef __cplusplus
}
#endif
