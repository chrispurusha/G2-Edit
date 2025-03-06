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
    eMsgCmdWriteModule,
    eMsgCmdWriteCable
} eMsgCmd;

typedef struct {
    uint32_t location;
    uint32_t index;
    uint32_t variation;
    uint32_t param;
    uint32_t value;
} tParamData;

typedef struct {
    uint32_t location;
    // cable count!?
    uint32_t colour;
    uint32_t moduleFromIndex;
    uint32_t connectorFromIndex;  // Might be IO count instead
    uint32_t linkType;
    uint32_t moduleToIndex;
    uint32_t connectorToIndex;
} tCableData;

typedef struct {
    uint32_t cmd;
    union {
        tParamData paramData;
        tCableData cableData;
    };
    //tMessageData data;
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
