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

#include "defs.h"
#include "types.h"
#include "msgQueue.h"

int msgqueue_sem_trywait(tMessageQueue * queue) {
    int retVal = 0;

    errno = 0;
    pthread_mutex_lock(&queue->semMutex);

    if (queue->semCount <= 0) {
        errno  = EAGAIN;
        retVal = -1;
    } else {
        queue->semCount--;
    }
    pthread_mutex_unlock(&queue->semMutex);

    return retVal;
}

void msgqueue_sem_wait(tMessageQueue * queue) {
    pthread_mutex_lock(&queue->semMutex);

    while (queue->semCount <= 0) {
        pthread_cond_wait(&queue->semCond, &queue->semMutex);
    }
    queue->semCount--;
    pthread_mutex_unlock(&queue->semMutex);
}

void msgqueue_sem_send(tMessageQueue * queue) {
    pthread_mutex_lock(&queue->semMutex);
    queue->semCount++;
    pthread_cond_signal(&queue->semCond);
    pthread_mutex_unlock(&queue->semMutex);
}

void msg_init(tMessageQueue * msgQueue, char * semName) {
    pthread_mutexattr_t attr = {0};

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&msgQueue->mutex, &attr);
    pthread_mutex_init(&msgQueue->semMutex, &attr);
    pthread_cond_init(&msgQueue->semCond, NULL);
    msgQueue->semCount = 0;
    pthread_mutexattr_destroy(&attr);

    msgQueue->head     = NULL;
    msgQueue->tail     = NULL;
}

int msg_receive(tMessageQueue * msgQueue, eRcv rcv, tMessageContent * messageContent) {
    int        retVal      = EXIT_FAILURE;
    tMessage * oldMsgQueue = NULL;
    bool       proceed     = true;

    if (msgQueue == NULL) {
        LOG_DEBUG("msgQueue==NULL\n");
        return retVal;
    }

    if (messageContent == NULL) {
        LOG_DEBUG("messageContent==NULL\n");
        return retVal;
    }

    switch (rcv) {
        case eRcvWait:
            msgqueue_sem_wait(msgQueue);
            break;

        case eRcvPoll:

            if (msgqueue_sem_trywait(msgQueue) == -1) {
                if (errno != EAGAIN) {
                    perror("sem_trywait failed");
                }
                proceed = false;
            }
            break;
    }

    if (proceed) {
        pthread_mutex_lock(&msgQueue->mutex);

        if (msgQueue->head != NULL) {
            memcpy(messageContent, &(msgQueue->head->messageContent), sizeof(*messageContent));
            oldMsgQueue    = msgQueue->head;
            msgQueue->head = msgQueue->head->nextMessage;

            if (msgQueue->head == NULL) {
                msgQueue->tail = NULL;
            }
            retVal         = EXIT_SUCCESS;
        }
        pthread_mutex_unlock(&msgQueue->mutex);

        if (oldMsgQueue != NULL) {
            memset(oldMsgQueue, 0, sizeof(*oldMsgQueue));
            free(oldMsgQueue);
        }
    }
    return retVal;
}

void msg_send(tMessageQueue * msgQueue, tMessageContent * messageContent) {
    tMessage * message = NULL;

    if (msgQueue == NULL) {
        LOG_DEBUG("msgQueue==NULL\n");
        return;
    }
    message = malloc(sizeof(tMessage));

    if (message != NULL) {
        memset(message, 0, sizeof(tMessage));
        memcpy(&(message->messageContent), messageContent, sizeof(message->messageContent));

        pthread_mutex_lock(&msgQueue->mutex);

        if (msgQueue->tail == NULL) {
            msgQueue->head = message;
            msgQueue->tail = message;
        } else {
            msgQueue->tail->nextMessage = message;
            msgQueue->tail              = message;
        }
        pthread_mutex_unlock(&msgQueue->mutex);

        msgqueue_sem_send(msgQueue);
    } else {
        LOG_DEBUG("Memory allocation failed for message\n");
    }
}

int msg_count(tMessageQueue * msgQueue) {
    int count = 0;

    if (msgQueue == NULL) {
        LOG_DEBUG("msgQueue==NULL\n");
        return count;
    }
    pthread_mutex_lock(&msgQueue->mutex);

    count = msgQueue->semCount;

    pthread_mutex_unlock(&msgQueue->mutex);

    return count;
}

#ifdef __cplusplus
}
#endif
