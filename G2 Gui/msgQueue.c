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

#include "defs.h"
#include "types.h"
#include "msgQueue.h"

void msg_init(tMessageQueue * msgQueue, char * semName) {
    pthread_mutex_init(&msgQueue->mutex, NULL);
    msgQueue->head = NULL;
    msgQueue->tail = NULL;

    // Try opening semaphore if it exists, or create a new one
    msgQueue->semaphore = sem_open(semName, O_CREAT | O_EXCL, 0, 0);

    if (msgQueue->semaphore == SEM_FAILED) {
        // Handle existing semaphore (retry without O_EXCL)
        msgQueue->semaphore = sem_open(semName, 0);

        if (msgQueue->semaphore == SEM_FAILED) {
            fprintf(stderr, "%s() Semaphore '%s' create/open failed\n", __FUNCTION__, semName);
            return;
        }
    }
    // Remove semaphore on exit
    sem_unlink(semName);
}

int msg_receive(tMessageQueue * msgQueue, eRcv rcv, tMessageContent * messageContent) {
    int        retVal      = EXIT_FAILURE;
    tMessage * oldMsgQueue = NULL;

    if (msgQueue == NULL) {
        fprintf(stderr, "%s() msgQueue==NULL\n", __FUNCTION__);
        return retVal;
    }

    if (messageContent == NULL) {
        fprintf(stderr, "%s() messageContent==NULL\n", __FUNCTION__);
        return retVal;
    }

    switch (rcv) {
        case eRcvWait:
            sem_wait(msgQueue->semaphore);     // Block until message is available
            break;

        case eRcvPoll:

            if (sem_trywait(msgQueue->semaphore) == -1) {
                if (errno == EAGAIN) {       // Semaphore is not available, no messages
                    return retVal;
                }
                // Handle other errors (e.g., ENOMEM)
                perror("sem_trywait failed");
                return retVal;
            }
            break;
    }
    pthread_mutex_lock(&msgQueue->mutex);

    if (msgQueue->head != NULL) {
        // Copy the content from the head message to the provided messageContent
        memcpy(messageContent, &(msgQueue->head->messageContent), sizeof(*messageContent));

        oldMsgQueue    = msgQueue->head;
        msgQueue->head = msgQueue->head->nextMessage;

        // If the queue becomes empty, reset the tail pointer as well
        if (msgQueue->head == NULL) {
            msgQueue->tail = NULL;
        }
        retVal = EXIT_SUCCESS;         // Return success when message is processed
    }
    pthread_mutex_unlock(&msgQueue->mutex);

    if (oldMsgQueue != NULL) {
        memset(oldMsgQueue, 0, sizeof(*oldMsgQueue));     // Clear the memory
        free(oldMsgQueue);                                // Free the message
    }
    return retVal;
}

void msg_send(tMessageQueue * msgQueue, tMessageContent * messageContent) {
    tMessage * message = NULL;

    if (msgQueue == NULL) {
        fprintf(stderr, "%s() msgQueue==NULL\n", __FUNCTION__);
        return;
    }
    // Allocate memory for the new message
    message = malloc(sizeof(tMessage));

    if (message == NULL) {
        fprintf(stderr, "%s() Memory allocation failed for message\n", __FUNCTION__);
        return;         // Memory allocation failure
    }
    // Copy the message content to the new message
    memcpy(&(message->messageContent), messageContent, sizeof(message->messageContent));

    pthread_mutex_lock(&msgQueue->mutex);

    // If the queue is empty, set both head and tail to the new message
    if (msgQueue->tail == NULL) {
        msgQueue->head = message;
        msgQueue->tail = message;
    } else {
        msgQueue->tail->nextMessage = message;
        msgQueue->tail              = message; // Move the tail pointer to the new last message
    }
    pthread_mutex_unlock(&msgQueue->mutex);

    // Signal that a new message is available
    sem_post(msgQueue->semaphore);
}

#ifdef __cplusplus
}
#endif