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

#ifndef __MSG_QUEUE_H__
#define __MSG_QUEUE_H__

#include "sysIncludes.h"

typedef enum {
    eCommandInit,
    eCommandStart,
    eCommandStop,
    eCommandGetPatch
} eCommand;

typedef enum {
    eRcvPoll,
    eRcvWait
} eRcv;

typedef enum {
    eMsgCmdSetValue,
    eMsgCmdSetMode,
    eMsgCmdSetParamMorph,
    eMsgCmdWriteModule,
    eMsgCmdDeleteModule,
    eMsgCmdMoveModule,
    eMsgCmdSetModuleUpRate,
    eMsgCmdWriteCable,
    eMsgCmdDeleteCable,
    eMsgCmdSelectVariation
} eMsgCmd;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   variation;
    uint32_t   param;
    uint32_t   value;
} tParamData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   variation;
    uint32_t   param;
    uint32_t   paramMorph;
    uint32_t   value;
    uint32_t   negative;
} tParamMorphData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   mode;
    uint32_t   value;
} tModeData;

typedef struct {
    uint32_t variation;
} tVariationData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   type;
    uint32_t   row;
    uint32_t   column;
    uint32_t   colour;
    uint32_t   upRate;
    uint32_t   isLed;
    uint32_t   unknown1;
    uint32_t   modeCount;
    uint32_t   mode[MAX_NUM_MODES];
    char       name[MODULE_NAME_SIZE + 1];
} tModuleData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   row;
    uint32_t   column;
} tModuleMoveData;

typedef struct {
    uint32_t location;
    uint32_t colour;
    uint32_t moduleFromIndex;
    uint32_t connectorFromIoIndex;
    uint32_t linkType;
    uint32_t moduleToIndex;
    uint32_t connectorToIoIndex;
} tCableData;

typedef struct {
    uint32_t cmd;
    union {
        tParamData      paramData;
        tParamMorphData paramMorphData;
        tModeData       modeData;
        tModuleData     moduleData;
        tModuleData     moduleMoveData;
        tCableData      cableData;
        tVariationData  variationData;
    };
} tMessageContent;

typedef struct _message {
    tMessageContent   messageContent;
    struct _message * nextMessage;
} tMessage;

typedef struct {
    pthread_mutex_t mutex;
    sem_t *         semaphore;
    tMessage *      head;
    tMessage *      tail;
} tMessageQueue;

void msg_init(tMessageQueue * msgQueue, char * semName);
int msg_receive(tMessageQueue * msgQueue, eRcv rcv, tMessageContent * messageContent);
void msg_send(tMessageQueue * msgQueue, tMessageContent * messageContent);

#endif // __MSG_QUEUE_H__
