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

static pthread_mutex_t   dbMutex     = {0};
static tModule *         firstModule = NULL;
static tModule *         walkModule  = NULL;
static tCable *          firstCable  = NULL;
static tCable *          walkCable   = NULL;

extern tModuleProperties gModuleProperties[];

static void mutex_lock(void) {
    if (pthread_mutex_lock(&dbMutex) != 0) {
        pthread_mutexattr_t attr = {0};

        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&dbMutex, &attr);
        pthread_mutexattr_destroy(&attr);

        pthread_mutex_lock(&dbMutex);
    }
}

static void mutex_unlock(void) {
    pthread_mutex_unlock(&dbMutex);
}

void dump_modules(void) {
    tModule * module = NULL;
    uint32_t  count  = 0;

    mutex_lock();

    module = firstModule;

    printf("\n\nDump modules\n");

    while (module != NULL) {
        printf("Location %u\n", module->key.location);
        printf("Index %u\n", module->key.index);
        printf(" Type %u\n", module->type);
        printf(" Name %s\n", module->name);
        printf(" Row %d\n", module->row);
        printf(" Column %d\n", module->column);
        printf(" Prev %p\n", module->prev);
        printf(" Next %p\n", module->next);

        count++;
        module = module->next;
    }
    printf("\nModule Count=%u\n", count);
    printf("\n\n");

    mutex_unlock();
}

static tModule * find_module(tModuleKey key) { // todo - use tModuleKey instead of location and index
    tModule * module = NULL;

    mutex_lock();

    module = firstModule;

    while (module != NULL) {
        if ((module->key.location == key.location) && (module->key.index == key.index)) {
            break;
        }
        module = module->next;
    }
    mutex_unlock();

    return module;
}

bool read_module(tModuleKey key, tModule * module) {
    bool      retVal      = false;
    tModule * foundModule = NULL;

    memset(module, 0, sizeof(*module));

    mutex_lock();

    foundModule = find_module(key);

    if (foundModule != NULL) {
        memcpy(module, foundModule, sizeof(*module));
        retVal = true;
    }
    mutex_unlock();

    return retVal;
}

void write_module(tModuleKey key, tModule * module) {
    tModule * dbModule      = NULL;
    tModule * iterateModule = NULL;

    module->key = key;  // Ensure key is set

    mutex_lock();

    dbModule = find_module(key);

    if (dbModule == NULL) {
        dbModule = (tModule *)malloc(sizeof(tModule));

        if (dbModule == NULL) {
            printf("Malloc fail\n");
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
    // Preserve prev and next pointers before memcpy
    tModule * prev = dbModule->prev;
    tModule * next = dbModule->next;
    memcpy(dbModule, module, sizeof(*dbModule));
    // Restore prev and next pointers
    dbModule->prev = prev;
    dbModule->next = next;

    mutex_unlock();
}

void delete_module(tModuleKey key, tDoFree doFree) {
    tModule * dbModule = NULL;

    mutex_lock();

    dbModule = find_module(key);

    if (dbModule != NULL) {
        if (dbModule->prev != NULL) {
            dbModule->prev->next = dbModule->next;
        } else {
            firstModule = dbModule->next;
        }

        if (dbModule->next != NULL) {
            dbModule->next->prev = dbModule->prev;
        }

        
        if (doFree == doFreeYes) {
            for (uint32_t i = 0; i < VARIATIONS; i++) {
                free(dbModule->param[i]);
            }
            
            if (dbModule->connector != NULL) {
                free(dbModule->connector);
            }
        }
        memset(dbModule, 0, sizeof(*dbModule));  // Protection against using stale data
        free(dbModule);
    }
    mutex_unlock();
}

void reset_walk_module(void) {
    walkModule = NULL;
}

bool walk_next_module(tModule * module) {
    bool validModule = false;

    memset(module, 0, sizeof(*module));

    mutex_lock();

    if (walkModule == NULL) {
        walkModule = firstModule;
    } else {
        walkModule = walkModule->next;
    }

    if (walkModule != NULL) {
        memcpy(module, walkModule, sizeof(*module));
        validModule = true;
    }
    mutex_unlock();

    return validModule;
}

void dump_cables(void) {
    tCable * cable = NULL;

    mutex_lock();

    cable = firstCable;

    printf("\n\n\nDump cables\n");

    while (cable != NULL) {
        printf("Cable\n");
        printf("Location %u\n", cable->key.location);
        printf(" Colour %u\n", cable->colour);
        printf(" Module from %u\n", cable->key.moduleFromIndex);
        printf(" Connector from %u\n", cable->key.connectorFromIoCount);
        printf(" Link type %u\n", cable->key.linkType);
        printf(" Module to %u\n", cable->key.moduleToIndex);
        printf(" Connector to %u (depends on link type)\n", cable->key.connectorToIoCount);
        cable = cable->next;
    }
    printf("\n\n\n");

    mutex_unlock();
}

static tCable * find_cable(tCableKey key) {
    tCable * cable = NULL;

    mutex_lock();

    cable = firstCable;

    while (cable != NULL) {
        if (memcmp(&cable->key, &key, sizeof(tCableKey)) == 0) {
            break;
        }
        cable = cable->next;
    }
    mutex_unlock();

    return cable;
}

bool read_cable(tCableKey key, tCable * cable) {
    bool     retVal     = false;
    tCable * foundCable = NULL;

    memset(cable, 0, sizeof(*cable));

    mutex_lock();

    foundCable = find_cable(key);

    if (foundCable != NULL) {
        memcpy(cable, foundCable, sizeof(*cable));
        retVal = true;
    }
    mutex_unlock();

    return retVal;
}

void write_cable(tCableKey key, tCable * cable) {
    tCable * dbCable      = NULL;
    tCable * iterateCable = NULL;

    cable->key = key;

    mutex_lock();

    dbCable = find_cable(key);

    if (dbCable == NULL) {
        dbCable = (tCable *)malloc(sizeof(tCable));

        if (dbCable == NULL) {
            printf("Malloc fail\n");
            exit(1);
        }

        if (firstCable == NULL) {
            firstCable = dbCable;
        } else {
            iterateCable = firstCable;

            while (iterateCable->next != NULL) {
                iterateCable = iterateCable->next;
            }
            iterateCable->next = dbCable;
        }
    }

    if (dbCable != NULL) {
        memcpy(dbCable, cable, sizeof(*dbCable));
    } else {
        printf("Cable generation or update failed\n");
        exit(1);
    }
    //TODO - possibly have local copy of the db's model of next,rather than using incoming

    mutex_unlock();
}

void reset_walk_cable(void) {
    walkCable = NULL;
}

bool walk_next_cable(tCable * cable) {
    bool validCable = false;

    memset(cable, 0, sizeof(*cable));

    mutex_lock();

    if (walkCable == NULL) {
        walkCable = firstCable;
    } else {
        walkCable = walkCable->next;
    }

    if (walkCable != NULL) {
        memcpy(cable, walkCable, sizeof(*cable));
        validCable = true;
    }
    mutex_unlock();

    return validCable;
}

void database_clear_modules(void) {
    tModule * module     = NULL;
    tModule * nextModule = NULL;

    mutex_lock();

    module = firstModule;

    while (module != NULL) {
        nextModule = module->next;

        for (uint32_t i = 0; i < VARIATIONS; i++) {
            free(module->param[i]);
        }
        
        if (module->connector != NULL) {
            free(module->connector);
        }
        free(module);
        module = nextModule;
    }
    firstModule = NULL;

    mutex_unlock();
}

void database_clear_cables(void) {
    tCable * cable     = NULL;
    tCable * nextCable = NULL;

    mutex_lock();

    cable = firstCable;

    while (cable != NULL) {
        nextCable = cable->next;
        free(cable);
        cable = nextCable;
    }
    firstCable = NULL;

    mutex_unlock();
}

int find_io_count_from_index(tModule * module, tConnectorDir dir, int index) {
    int ioCount = -1;

    for (int i = 0; i <= index; i++) {
        //printf("%d is type %d\n", i, module->connector[i].dir);
        if (module->connector[i].dir == dir) {
            ioCount++;
        }
    }

    return ioCount;  // Index does not match the direction
}

int find_index_from_io_count(tModule * module, tConnectorDir dir, int targetCount) {   // Todo: have an instance of this in graphics too! Needs moving to one place
    int count = 0;

    //printf("%s find index num connectors %u\n", gModuleProperties[module->type].name, gModuleProperties[module->type].numConnectors);
    for (int index = 0; index < gModuleProperties[module->type].numConnectors; index++) {
        if (module->connector[index].dir == dir) {
            if (count == targetCount) {
                return index;
            }
            count++;
        }
    }

    return -1;  // Not found
}

void allocate_module_parameters(tModule * module, uint32_t paramCount) {
    if (paramCount > 0) {
        printf("Type = %s parameters %u\n", gModuleProperties[module->type].name, paramCount);
        for (uint32_t i = 0; i < VARIATIONS; i++) {  // Might need to check if already allocated!?
            if (module->param[i] == NULL) {
                module->param[i] = malloc(paramCount * sizeof(tParam));
            }
            if (module->param[i] != NULL) {
                memset(module->param[i], 0, paramCount * sizeof(tParam));
            } else {
                perror("Failed to allocate memory for param array");
                exit(1);
            }
        }
        module->allocatedParams = paramCount;
    }
}
    
void allocate_module_connectors(tModule * module, uint32_t connectorCount) {
    // Ultimately do through a database function, so we're not mallocing here.
    if (connectorCount > 0) {
        printf("Type = %s connectors %u\n", gModuleProperties[module->type].name, gModuleProperties[module->type].numConnectors);
        if (module->connector == NULL) {
            module->connector = malloc(gModuleProperties[module->type].numConnectors * sizeof(tConnector)); // Might need to check if already allocated!?
        }
        if (module->connector != NULL) {
            memset(module->connector, 0, gModuleProperties[module->type].numConnectors * sizeof(tConnector));
        } else {
            perror("Failed to allocate memory for connector array");
            exit(1);
        }
        module->allocatedConnectors = connectorCount;
    }
}
    
#ifdef __cplusplus
}
#endif
