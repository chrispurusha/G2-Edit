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

#include <string.h>

#include "defs.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "msgQueue.h"
#include "moduleResourcesAccess.h"
#include "protocol.h"
#include "selection.h"

bool is_selected(tModuleKey key) {
    for (uint32_t i = 0; i < gSelection.count; i++) {
        tModuleKey k = gSelection.keys[i];

        if (k.slot == key.slot && k.location == key.location && k.index == key.index) {
            return true;
        }
    }

    return false;
}

void selection_clear(void) {
    memset(&gSelection, 0, sizeof(gSelection));
}

static void selection_add(tModuleKey key) {
    if (gSelection.count >= MAX_NUM_MODULES) {
        return;
    }

    if (is_selected(key)) {
        return;
    }
    gSelection.keys[gSelection.count++] = key;
}

static void selection_remove(tModuleKey key) {
    for (uint32_t i = 0; i < gSelection.count; i++) {
        tModuleKey k = gSelection.keys[i];

        if (k.slot == key.slot && k.location == key.location && k.index == key.index) {
            gSelection.keys[i] = gSelection.keys[--gSelection.count];
            return;
        }
    }
}

void selection_set_single(tModuleKey key) {
    selection_clear();
    selection_add(key);
}

void selection_toggle(tModuleKey key) {
    if (is_selected(key)) {
        selection_remove(key);
    } else {
        selection_add(key);
    }
}

// Add all active modules in the given slot/location whose rectangle overlaps rect.
// rect and module positions are both in module-space (pre-zoom, pre-scroll).
// mod->rectangle is screen-space and is NOT used here.
void selection_add_rect(tRectangle rect, uint32_t slot, uint32_t location) {
    double r1x2 = rect.coord.x + rect.size.w;
    double r1y2 = rect.coord.y + rect.size.h;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * mod      = get_module_slot(slot, location, i);

        if (!mod->active) {
            continue;
        }
        double    h        = (double)gModuleProperties[mod->type].height;
        double    mx       = mod->column * MODULE_X_SPAN;
        double    my       = mod->row * MODULE_Y_SPAN;
        double    mw       = MODULE_WIDTH;
        double    mh       = (h * MODULE_Y_SPAN) - MODULE_Y_GAP;
        bool      overlaps = rect.coord.x < mx + mw && r1x2 > mx
                             && rect.coord.y < my + mh && r1y2 > my;

        if (overlaps) {
            selection_add(mod->key);
        }
    }
}

// Send cables-then-module delete to the G2 and remove from local DB.
void delete_module_and_cables(tModuleKey key) {
    uint32_t        slot     = key.slot;
    uint32_t        location = key.location;
    tMessageContent msg      = {0};

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if (cable == NULL || !cable->active) {
            continue;
        }

        if (cable->key.moduleFromIndex != key.index && cable->key.moduleToIndex != key.index) {
            continue;
        }
        msg                                = (tMessageContent){
            0
        };
        msg.cmd                            = eMsgCmdDeleteCable;
        msg.slot                           = slot;
        msg.cableData.location             = location;
        msg.cableData.moduleFromIndex      = cable->key.moduleFromIndex;
        msg.cableData.connectorFromIoIndex = cable->key.connectorFromIoCount;
        msg.cableData.moduleToIndex        = cable->key.moduleToIndex;
        msg.cableData.connectorToIoIndex   = cable->key.connectorToIoCount;
        msg.cableData.linkType             = cable->key.linkType;
        msg_send(&gCommandQueue, &msg);
        delete_cable(cable->key);
    }

    msg                      = (tMessageContent){
        0
    };
    msg.cmd                  = eMsgCmdDeleteModule;
    msg.slot                 = slot;
    msg.moduleData.moduleKey = key;
    msg_send(&gCommandQueue, &msg);
    delete_module(key);
}

void delete_selection(void) {
    for (uint32_t i = 0; i < gSelection.count; i++) {
        delete_module_and_cables(gSelection.keys[i]);
    }

    selection_clear();
}

// Like shift_modules_down but applied to every selected module. Selected modules
// are transparent to each other — only conflicts with non-selected modules are resolved.
void shift_selection_down(void) {
    uint32_t slot     = (uint32_t)gSlot;
    uint32_t location = (uint32_t)gLocation;

    for (uint32_t si = 0; si < gSelection.count; si++) {
        tModuleKey key               = gSelection.keys[si];
        tModule *  module            = get_module(key);

        if (module == NULL) {
            continue;
        }
        bool       moduleRePosition  = false;
        bool       doDrop            = false;
        uint32_t   rowAndBelowToDrop = 0;
        uint32_t   dropAmount        = 0;

        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * walk = get_module_slot(slot, location, i);

            if (!walk->active || walk->key.index == key.index || is_selected(walk->key)) {
                continue;
            }

            if ((walk->column == module->column) && (module->row > walk->row) && (module->row < walk->row + gModuleProperties[walk->type].height)) {
                module->row      = walk->row + gModuleProperties[walk->type].height;
                send_module_move_msg(module);
                moduleRePosition = true;
                break;
            }
        }

        if (!moduleRePosition) {
            send_module_move_msg(module);
        }

        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * walk = get_module_slot(slot, location, i);

            if (!walk->active || walk->key.index == key.index || is_selected(walk->key)) {
                continue;
            }

            if ((walk->column == module->column) && (walk->row >= module->row) && (walk->row < module->row + gModuleProperties[module->type].height)) {
                rowAndBelowToDrop = walk->row;
                dropAmount        = (module->row + gModuleProperties[module->type].height) - walk->row;
                doDrop            = true;
                break;
            }
        }

        if (doDrop) {
            for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
                tModule * walk = get_module_slot(slot, location, i);

                if (!walk->active || walk->key.index == key.index || is_selected(walk->key)) {
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
}
