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

#include "dataBase.h"
#include "moduleResourcesAccess.h"

static pthread_mutex_t dbMutex                                          = {0};
static tModule         modules[MAX_SLOTS][locationMax][MAX_NUM_MODULES] = {0};
static uint32_t        walkSlot                                         = 0;
static uint32_t        walkLocation                                     = 0;
static uint32_t        walkIndex                                        = 0;
static tCable *        firstCable                                       = NULL;
static tCable *        lastCable                                        = NULL;
static tCable *        walkCable                                        = NULL;
static int32_t         moduleWalkNestCount                              = 0;
static int32_t         cableWalkNestCount                               = 0;

static void database_mutex_init(void) {
    pthread_mutexattr_t attr = {0};

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&dbMutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void database_mutex_lock(void) {
    pthread_mutex_lock(&dbMutex);
}

static void database_mutex_unlock(void) {
    pthread_mutex_unlock(&dbMutex);
}

void dump_modules(void) {
    uint32_t count = 0;

    database_mutex_lock();
    LOG_DEBUG("\n\nDump modules\n");

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        for (uint32_t location = 0; location < (uint32_t)locationMax; location++) {
            for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
                if (modules[slot][location][index].active) {
                    tModule * module = &modules[slot][location][index];
                    LOG_DEBUG("Slot %u\n", module->key.slot);
                    LOG_DEBUG("Location %u\n", module->key.location);
                    LOG_DEBUG("Index %u\n", module->key.index);
                    LOG_DEBUG(" Type %u\n", module->type);
                    LOG_DEBUG(" Name %s\n", module->name);
                    LOG_DEBUG(" Row %d\n", module->row);
                    LOG_DEBUG(" Column %d\n", module->column);
                    count++;
                }
            }
        }
    }

    LOG_DEBUG("\nModule Count=%u\n", count);
    LOG_DEBUG("\n\n");
    database_mutex_unlock();
}

static tModule * find_module(tModuleKey key) {
    tModule * module = NULL;

    database_mutex_lock();

    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
        if (modules[key.slot][key.location][key.index].active) {
            module = &modules[key.slot][key.location][key.index];
        }
    }
    database_mutex_unlock();

    return module;
}

bool read_module(tModuleKey key, tModule * module) {
    bool      retVal      = false;
    tModule * foundModule = NULL;

    memset(module, 0, sizeof(*module));

    database_mutex_lock();

    foundModule = find_module(key);

    if (foundModule != NULL) {
        memcpy(module, foundModule, sizeof(*module));
        retVal = true;
    }
    database_mutex_unlock();

    return retVal;
}

void write_module(tModuleKey key, tModule * module) {
    module->key    = key;
    module->active = true;

    database_mutex_lock();

    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
        modules[key.slot][key.location][key.index] = *module;
    } else {
        LOG_ERROR("Module key out of bounds slot=%u location=%u index=%u\n", key.slot, key.location, key.index);
    }
    database_mutex_unlock();
}

void delete_module(tModuleKey key) {
    database_mutex_lock();

    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
        memset(&modules[key.slot][key.location][key.index], 0, sizeof(tModule));
    }
    database_mutex_unlock();
}

void reset_walk_module(void) {
    database_mutex_lock();
    moduleWalkNestCount++;

    if (moduleWalkNestCount > 1) {
        LOG_ERROR("Attempted to nest module walk - can't do that\n");
        exit(1);
    }
    walkSlot     = 0;
    walkLocation = 0;
    walkIndex    = 0;
}

void finish_walk_module(void) {
    moduleWalkNestCount--;
    database_mutex_unlock();
}

bool walk_next_module(tModule * module) {
    bool validModule = false;

    memset(module, 0, sizeof(*module));
    database_mutex_lock();

    while (walkSlot < MAX_SLOTS) {
        uint32_t s = walkSlot;
        uint32_t l = walkLocation;
        uint32_t i = walkIndex;

        walkIndex++;

        if (walkIndex >= MAX_NUM_MODULES) {
            walkIndex = 0;
            walkLocation++;

            if (walkLocation >= (uint32_t)locationMax) {
                walkLocation = 0;
                walkSlot++;
            }
        }

        if (modules[s][l][i].active) {
            *module     = modules[s][l][i];
            validModule = true;
            break;
        }
    }
    database_mutex_unlock();

    return validModule;
}

void dump_cables(void) {
    tCable * cable = NULL;

    database_mutex_lock();

    cable = firstCable;

    LOG_DEBUG("\n\n\nDump cables\n");

    while (cable != NULL) {
        LOG_DEBUG("Cable\n");
        LOG_DEBUG("Slot %u\n", cable->key.slot);
        LOG_DEBUG("Location %u\n", cable->key.location);
        LOG_DEBUG(" Colour %u\n", cable->colour);
        LOG_DEBUG(" Module from %u\n", cable->key.moduleFromIndex);
        LOG_DEBUG(" Connector from %u\n", cable->key.connectorFromIoCount);
        LOG_DEBUG(" Link type %u\n", cable->key.linkType);
        LOG_DEBUG(" Module to %u\n", cable->key.moduleToIndex);
        LOG_DEBUG(" Connector to %u (depends on link type)\n", cable->key.connectorToIoCount);
        cable = cable->next;
    }
    LOG_DEBUG("\n\n\n");

    database_mutex_unlock();
}

static tCable * find_cable(tCableKey key) {
    tCable * cable = NULL;

    database_mutex_lock();

    cable = firstCable;

    while (cable != NULL) {
        if (memcmp(&cable->key, &key, sizeof(tCableKey)) == 0) {
            break;
        }
        cable = cable->next;
    }
    database_mutex_unlock();

    return cable;
}

bool read_cable(tCableKey key, tCable * cable) {
    bool     retVal     = false;
    tCable * foundCable = NULL;

    memset(cable, 0, sizeof(*cable));

    database_mutex_lock();

    foundCable = find_cable(key);

    if (foundCable != NULL) {
        memcpy(cable, foundCable, sizeof(*cable));
        retVal = true;
    }
    database_mutex_unlock();

    return retVal;
}

void write_cable(tCableKey key, tCable * cable) {
    tCable * dbCable = NULL;

    cable->key = key;

    database_mutex_lock();

    dbCable    = find_cable(key);

    if (dbCable == NULL) {
        dbCable = (tCable *)malloc(sizeof(tCable));

        if (dbCable == NULL) {
            LOG_DEBUG("Malloc fail\n");
            exit(1);
        }
        memset(dbCable, 0, sizeof(*dbCable));

        if (firstCable == NULL) {
            firstCable = dbCable;
            lastCable  = dbCable;
        } else {
            lastCable->next = dbCable;
            dbCable->prev   = lastCable;
            lastCable       = dbCable;
        }
    }

    if (dbCable != NULL) {
        tCable * prev = dbCable->prev;
        tCable * next = dbCable->next;
        memcpy(dbCable, cable, sizeof(*dbCable));
        dbCable->prev = prev;
        dbCable->next = next;
    } else {
        LOG_DEBUG("Cable generation or update failed\n");
        exit(1);
    }
    database_mutex_unlock();
}

void delete_cable(tCableKey key) {
    tCable * dbCable = NULL;

    database_mutex_lock();

    dbCable = find_cable(key);

    if (dbCable != NULL) {
        walkCable = dbCable->prev;  // Trick to point to previous item if we're deleting

        if (dbCable->prev != NULL) {
            dbCable->prev->next = dbCable->next;
        } else {
            firstCable = dbCable->next;
        }

        if (dbCable->next != NULL) {
            dbCable->next->prev = dbCable->prev;
        } else {
            lastCable = dbCable->prev;
        }
        memset(dbCable, 0, sizeof(*dbCable));  // Protection against using stale data
        free(dbCable);
    }
    database_mutex_unlock();
}

void reset_walk_cable(void) {
    database_mutex_lock();
    cableWalkNestCount++;

    if (cableWalkNestCount > 1) {
        LOG_ERROR("Attempted to nest cable walk - can't do that\n");
        exit(1);
    }
    walkCable = NULL;
}

void finish_walk_cable(void) {
    cableWalkNestCount--;
    database_mutex_unlock();
}

bool walk_next_cable(tCable * cable) {
    bool validCable = false;

    memset(cable, 0, sizeof(*cable));

    database_mutex_lock();

    if (walkCable == NULL) {
        walkCable = firstCable;
    } else {
        walkCable = walkCable->next;
    }

    if (walkCable != NULL) {
        memcpy(cable, walkCable, sizeof(*cable));
        validCable = true;
    }
    database_mutex_unlock();

    return validCable;
}

void database_delete_modules_by_slot(uint32_t slot) {
    database_mutex_lock();

    if (slot < MAX_SLOTS) {
        for (uint32_t location = 0; location < (uint32_t)locationMax; location++) {
            for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
                memset(&modules[slot][location][index], 0, sizeof(tModule));
            }
        }
    }
    database_mutex_unlock();
}

void database_delete_cables_by_slot(uint32_t slot) {
    tCable * cable     = NULL;
    tCable * nextCable = NULL;

    database_mutex_lock();

    cable = firstCable;

    while (cable != NULL) {
        nextCable = cable->next;

        if (cable->key.slot == slot) {
            delete_cable(cable->key);
        }
        cable     = nextCable;
    }
    database_mutex_unlock();
}

void database_clear_modules(void) {
    database_mutex_lock();
    memset(modules, 0, sizeof(modules));
    database_mutex_unlock();
}

void database_clear_cables(void) {
    tCable * cable     = NULL;
    tCable * nextCable = NULL;

    database_mutex_lock();

    cable      = firstCable;

    while (cable != NULL) {
        nextCable = cable->next;
        free(cable);
        cable     = nextCable;
    }
    firstCable = NULL;
    lastCable  = NULL;

    database_mutex_unlock();
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
    database_mutex_init();
}

#ifdef __cplusplus
}
#endif
