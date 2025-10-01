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

static pthread_mutex_t dbMutex     = {0};
static tModule *       firstModule = NULL;
static tModule *       walkModule  = NULL;
static tCable *        firstCable  = NULL;
static tCable *        walkCable   = NULL;

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
    tModule * module = NULL;
    uint32_t  count  = 0;

    database_mutex_lock();

    module = firstModule;

    LOG_DEBUG("\n\nDump modules\n");

    while (module != NULL) {
        LOG_DEBUG("Slot %u\n", module->key.slot);
        LOG_DEBUG("Location %u\n", module->key.location);
        LOG_DEBUG("Index %u\n", module->key.index);
        LOG_DEBUG(" Type %u\n", module->type);
        LOG_DEBUG(" Name %s\n", module->name);
        LOG_DEBUG(" Row %d\n", module->row);
        LOG_DEBUG(" Column %d\n", module->column);
        LOG_DEBUG(" Prev %p\n", module->prev);
        LOG_DEBUG(" Next %p\n", module->next);

        count++;
        module = module->next;
    }
    LOG_DEBUG("\nModule Count=%u\n", count);
    LOG_DEBUG("\n\n");

    database_mutex_unlock();
}

static tModule * find_module(tModuleKey key) {
    tModule * module = NULL;

    database_mutex_lock();

    module = firstModule;

    while (module != NULL) {
        if ((module->key.slot == key.slot) && (module->key.location == key.location) && (module->key.index == key.index)) {
            break;
        }
        module = module->next;
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
    tModule * dbModule      = NULL;
    tModule * iterateModule = NULL;

    module->key = key;  // Ensure key is set

    database_mutex_lock();

    dbModule = find_module(key);

    if (dbModule == NULL) {
        dbModule = (tModule *)malloc(sizeof(tModule));

        if (dbModule == NULL) {
            LOG_ERROR("Malloc fail\n");
            exit(1);
        }
        memset(dbModule, 0, sizeof(*dbModule));

        if (firstModule == NULL) {
            firstModule = dbModule;
        } else {
            iterateModule = firstModule;

            while (iterateModule->next != NULL) {
                iterateModule = iterateModule->next;
            }
            iterateModule->next = dbModule;
            dbModule->prev      = iterateModule;
        }
    }

    if (dbModule != NULL) {
        // Preserve prev and next pointers before memcpy
        tModule * prev = dbModule->prev;
        tModule * next = dbModule->next;
        memcpy(dbModule, module, sizeof(*dbModule));
        // Restore prev and next pointers
        dbModule->prev = prev;
        dbModule->next = next;
    } else {
        LOG_ERROR("Module generation or update failed\n");
        exit(1);
    }
    database_mutex_unlock();
}

void delete_module(tModuleKey key) {
    tModule * dbModule = NULL;

    database_mutex_lock();

    dbModule = find_module(key);

    if (dbModule != NULL) {
        walkModule = dbModule->prev;  // Trick to point to previous item if we're deleting

        if (dbModule->prev != NULL) {
            dbModule->prev->next = dbModule->next;
        } else {
            firstModule = dbModule->next;
        }

        if (dbModule->next != NULL) {
            dbModule->next->prev = dbModule->prev;
        }
        memset(dbModule, 0, sizeof(*dbModule));  // Protection against using stale data
        free(dbModule);
    }
    database_mutex_unlock();
}

void reset_walk_module(void) {
    database_mutex_lock();
    walkModule = NULL;
}

void finish_walk_module(void) {
    database_mutex_unlock();
}

bool walk_next_module(tModule * module) {
    bool validModule = false;

    memset(module, 0, sizeof(*module));

    database_mutex_lock();

    if (walkModule == NULL) {
        walkModule = firstModule;
    } else {
        walkModule = walkModule->next;
    }

    if (walkModule != NULL) {
        memcpy(module, walkModule, sizeof(*module));
        validModule = true;
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
    tCable * dbCable      = NULL;
    tCable * iterateCable = NULL;

    cable->key = key;

    database_mutex_lock();

    dbCable = find_cable(key);

    if (dbCable == NULL) {
        dbCable = (tCable *)malloc(sizeof(tCable));

        if (dbCable == NULL) {
            LOG_DEBUG("Malloc fail\n");
            exit(1);
        }
        memset(dbCable, 0, sizeof(*dbCable));

        if (firstCable == NULL) {
            firstCable = dbCable;
        } else {
            iterateCable = firstCable;

            while (iterateCable->next != NULL) {
                iterateCable = iterateCable->next;
            }
            iterateCable->next = dbCable;
            dbCable->prev      = iterateCable;
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
        }
        memset(dbCable, 0, sizeof(*dbCable));  // Protection against using stale data
        free(dbCable);
    }
    database_mutex_unlock();
}

void reset_walk_cable(void) {
    database_mutex_lock();
    walkCable = NULL;
}

void finish_walk_cable(void) {
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

void database_clear_modules(void) {
    tModule * module     = NULL;
    tModule * nextModule = NULL;

    database_mutex_lock();

    module = firstModule;

    while (module != NULL) {
        nextModule = module->next;
        free(module);
        module = nextModule;
    }
    firstModule = NULL;

    database_mutex_unlock();
}

void database_clear_cables(void) {
    tCable * cable     = NULL;
    tCable * nextCable = NULL;

    database_mutex_lock();

    cable = firstCable;

    while (cable != NULL) {
        nextCable = cable->next;
        free(cable);
        cable = nextCable;
    }
    firstCable = NULL;

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
