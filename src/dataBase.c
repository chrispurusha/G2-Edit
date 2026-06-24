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

#include "dataBase.h"
#include "moduleResourcesAccess.h"

tModule gModule[MAX_SLOTS][locationMax][MAX_NUM_MODULES] = {0};

tCable  gCable[MAX_SLOTS][locationMax][MAX_NUM_CABLES]   = {0};

tModule * get_module_slot(uint32_t slot, uint32_t location, uint32_t index) {
    if ((slot < MAX_SLOTS) && (location < (uint32_t)locationMax) && (index < MAX_NUM_MODULES)) {
        return &gModule[slot][location][index];
    }
    return NULL;
}

tModule * get_module(tModuleKey key) {
    tModule * module = get_module_slot(key.slot, key.location, key.index);

    if ((module != NULL) && module->active) {
        return module;
    }
    return NULL;
}

void write_module(tModuleKey key, tModule * module) {
    module->key    = key;
    module->active = true;

    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
        gModule[key.slot][key.location][key.index] = *module;
    } else {
        LOG_ERROR("Module key out of bounds slot=%u location=%u index=%u\n", key.slot, key.location, key.index);
    }
}

void delete_module(tModuleKey key) {
    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
        memset(&gModule[key.slot][key.location][key.index], 0, sizeof(tModule));
    }
}

void database_delete_modules_by_slot(uint32_t slot) {
    if (slot < MAX_SLOTS) {
        for (uint32_t location = 0; location < (uint32_t)locationMax; location++) {
            for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
                memset(&gModule[slot][location][index], 0, sizeof(tModule));
            }
        }
    }
}

uint32_t count_active_modules(uint32_t slot) {
    uint32_t count = 0;

    if (slot >= MAX_SLOTS) {
        return 0;
    }
    // Only count VA and FX — morph-location modules exist on all slots (sent by G2 via USB)
    // but are not written to patch/perf files and don't indicate real patch content.
    for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
        if (gModule[slot][locationVa][index].active) {
            count++;
        }
        if (gModule[slot][locationFx][index].active) {
            count++;
        }
    }
    return count;
}

bool slot_has_modules(uint32_t slot) {
    return count_active_modules(slot) > 0;
}

void database_clear_modules(void) {
    memset(gModule, 0, sizeof(gModule));
}

void dump_modules(void) {
    uint32_t count = 0;

    LOG_DEBUG("\n\nDump modules\n");

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        for (uint32_t location = 0; location < (uint32_t)locationMax; location++) {
            for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
                if (gModule[slot][location][index].active) {
                    tModule * module = &gModule[slot][location][index];
                    LOG_DEBUG("Slot %u Location %u Index %u Type %u Name %s Row %u Column %u\n",
                              module->key.slot, module->key.location, module->key.index,
                              module->type, module->name, module->row, module->column);
                    count++;
                }
            }
        }
    }

    LOG_DEBUG("\nModule Count=%u\n\n", count);
}

tCable * get_cable_slot(uint32_t slot, uint32_t location, uint32_t index) {
    if ((slot < MAX_SLOTS) && (location < (uint32_t)locationMax) && (index < MAX_NUM_CABLES)) {
        return &gCable[slot][location][index];
    }
    return NULL;
}

tCable * get_cable(tCableKey key) {
    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax)) {
        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable * cable = &gCable[key.slot][key.location][i];

            if (cable->active && (memcmp(&cable->key, &key, sizeof(tCableKey)) == 0)) {
                return cable;
            }
        }
    }
    return NULL;
}

void write_cable(tCableKey key, tCable * cable) {
    tCable * existing = get_cable(key);

    if (existing != NULL) {
        existing->colour = cable->colour;
        return;
    }

    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax)) {
        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            if (!gCable[key.slot][key.location][i].active) {
                gCable[key.slot][key.location][i]        = *cable;
                gCable[key.slot][key.location][i].key    = key;
                gCable[key.slot][key.location][i].active = true;
                return;
            }
        }

        LOG_ERROR("write_cable: no free slot slot=%u location=%u\n", key.slot, key.location);
    }
}

void delete_cable(tCableKey key) {
    tCable * cable = get_cable(key);

    if (cable != NULL) {
        memset(cable, 0, sizeof(tCable));
    }
}

void dump_cables(void) {
    LOG_DEBUG("\n\nDump cables\n");

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
            for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
                tCable * cable = &gCable[slot][loc][i];

                if (cable->active) {
                    LOG_DEBUG("Slot %u Location %u Colour %u From %u/%u LinkType %u To %u/%u\n",
                              cable->key.slot, cable->key.location, cable->colour,
                              cable->key.moduleFromIndex, cable->key.connectorFromIoCount,
                              cable->key.linkType, cable->key.moduleToIndex,
                              cable->key.connectorToIoCount);
                }
            }
        }
    }

    LOG_DEBUG("\n\n");
}

void database_delete_cables_by_slot(uint32_t slot) {
    if (slot < MAX_SLOTS) {
        for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
            memset(gCable[slot][loc], 0, sizeof(gCable[slot][loc]));
        }
    }
}

void database_clear_cables(void) {
    memset(gCable, 0, sizeof(gCable));
}

int find_io_count_from_index(tModule * module, tConnectorDir dir, int index) {
    int ioCount = -1;

    for (int i = 0; i <= index; i++) {
        //LOG_DEBUG("%d is type %d\n", i, module->connector[i].dir);
        if (module->connector[i].dir == dir) {
            ioCount++;
        }
    }

    return ioCount;  // Index does not match the direction
}

int find_index_from_io_count(tModule * module, tConnectorDir dir, int targetCount) {
    int count = 0;

    //LOG_DEBUG("%s find index num connectors %u\n", gModuleProperties[module->type].name, gModuleProperties[module->type].numConnectors);
    for (uint32_t index = 0; index < module_connector_count(module->type); index++) {
        if (module->connector[index].dir == dir) {
            if (count == targetCount) {
                return index;
            }
            count++;
        }
    }

    return -1;  // Not found
}

void init_database(void) {
}

#ifdef __cplusplus
}
#endif
