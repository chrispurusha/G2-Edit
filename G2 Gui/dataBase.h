/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
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


#ifndef __DATABASE_H__
#define __DATABASE_H__

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"


typedef enum {
    paramTypeNone,
    paramTypeDial,
    paramTypeToggle,
} tParamType;

typedef struct _struct_param {
    tParamType type;
    tRectangle rectangle;
    uint32_t   range;
    uint32_t   value;
} tParam;

typedef struct {
    uint32_t location;
    uint32_t moduleFrom;
    uint32_t connectorFrom;
    uint32_t moduleTo;
    uint32_t connectorTo;
} tCableKey;

typedef struct _struct_cable {
    tCableKey              key;
    uint32_t               colour;
    uint32_t               linkType;
    struct _struct_cable * next;       // This can go, when we attach to modules rather than separate linked list
} tCable;

typedef enum {
    connectorTypeAudioIn,   // Todo: Needs splitting into 2 types: input/output and control/audio
    connectorTypeAudioOut,
    connectorTypeControlIn,
    connectorTypeControlOut,
    connectorTypeMax
} tConnectorType;

typedef struct {
    uint32_t location;
    uint32_t index;
} tModuleKey;

typedef struct _struct_module {
    tModuleKey              key;
    uint32_t                type;              // Review this. Is it used!?
    uint32_t                row;
    uint32_t                column;
    uint32_t                colour;
    uint32_t                upRate;            // Probably needs to be permanent in tModuleProperties
    uint32_t                isLed;             // Probably needs to be permanent in tModuleProperties
    uint32_t                unknown1;                                         // Guess we should store this, to write back if necessary. Might not be needed
    uint32_t                modeCount;                                        // Don't yet know what this is for. Might need modes array adding
    char                    name[MODULE_NAME_SIZE + 1];
    uint32_t                numParams;                                              // Relates to tComp array below - Todo: only refer to this in the permanent tModuleProperties
    tParam                  param[VARIATIONS][MAX_PARAMS_PER_MODULE];               // Going to need to allocate this per module, depending on numParams to save memory
    uint32_t                numConnectors;                                         // Todo: only refer to this in the permanent tModuleProperties - todo: might need to be num inputs connectors and num output connectors, but that would make direct array referencing not possible. Better to add up input and output connectors to check against protocol?
    tCoord                  connector[MAX_CONNECTORS_PER_MODULE][connectorTypeMax]; // Going to need to allocate this per module to save memory - Todo: this should be array of input connectors and output connectors or one array of connectors, marked as input or output
    struct _struct_module * next;
} tModule;

bool read_module(tModuleKey key, tModule * module);
void write_module(tModuleKey key, tModule * module);
void dump_modules(void);
void reset_walk_module(void);
bool walk_next_module(tModule * module);
uint32_t module_height(uint32_t type);

//tRgb module_colour_map_to_rbg(uint32_t colour);
char * module_type_name(uint32_t type);
bool read_cable(tCableKey key, tCable * cable);
void write_cable(tCableKey key, tCable * cable);
void dump_cables(void);
void reset_walk_cable(void);
bool walk_next_cable(tCable * cable);
void database_clear_modules(void);
void database_clear_cables(void);

#endif // __DATABASE_H__
