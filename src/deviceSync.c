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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "msgQueue.h"
#include "globalVars.h"
#include "graphics.h"
#include "prefs.h"
#include "deviceSync.h"

// Does this queued command change the patch the G2 holds? Queries, view state and whole-file
// operations do not: replaying them would be pointless rather than wrong, and counting them as
// divergence would fire the conflict dialog on a reconnect where nothing was actually edited.
static bool command_changes_patch(uint32_t cmd) {
    switch (cmd) {
        case eMsgCmdSetValue:
        case eMsgCmdSetMode:
        case eMsgCmdSetParamMorph:
        case eMsgCmdWriteModule:
        case eMsgCmdDeleteModule:
        case eMsgCmdMoveModule:
        case eMsgCmdSetModuleUpRate:
        case eMsgCmdWriteCable:
        case eMsgCmdSetCableColour:
        case eMsgCmdDeleteCable:
        case eMsgCmdSetModuleLabel:
        case eMsgCmdSetPatchName:
        case eMsgCmdSetModuleColour:
        case eMsgCmdWritePatchDescr:
        case eMsgCmdAssignKnob:
        case eMsgCmdDeassignKnob:
        case eMsgCmdAssignMidiCC:
        case eMsgCmdDeassignMidiCC:
        case eMsgCmdCopyVariation:
        case eMsgCmdSetParamLabel:
        case eMsgCmdSetCustomData:
        case eMsgCmdSetMutationLock:
            return true;

        default:
            return false;
    }
}

uint32_t device_sync_drain_offline_edits(void) {
    tMessageContent messageContent = {0};
    uint32_t        slotMask       = 0;
    uint32_t        discarded      = 0;

    while (msg_receive(&gToUsbThread, eRcvPoll, &messageContent) == EXIT_SUCCESS) {
        discarded++;

        if (command_changes_patch(messageContent.cmd) && (messageContent.slot < MAX_SLOTS)) {
            slotMask |= (1u << messageContent.slot);
        }
        memset(&messageContent, 0, sizeof(messageContent));
    }

    if (discarded > 0) {
        LOG_DEBUG("Discarded %u queued command(s) from the offline period, dirty slot mask 0x%x\n",
                  discarded, slotMask);
    }
    return slotMask;
}

// Recovery files are the app's own, not the user's, so they live in the app's folder rather than
// among real patches in whatever directory was last browsed. The dialog quotes the path, which is
// the only time anyone needs to know where it is.
#define RECOVERY_KEEP    (10u)  // Newest N kept; older ones pruned on each write

// What the last write produced, so a subsequent explicit Save As can retire the automatic copy it
// has just made redundant. Only ever holds one conflict's worth — the dialog is modal.
static char     sRecoveryPath[MAX_SLOTS][FILE_PATH_SIZE] = {0};
static uint32_t sRecoveryCount                           = 0;

// Builds (and creates) <app support>/G2-Edit/Recovery. Mirrors prefs.cpp's platform choice.
static bool recovery_folder(char * out, size_t outSize) {
    const char * home                   = getenv("HOME");

    if ((home == NULL) || (home[0] == '\0')) {
        return false;
    }
    char         parent[FILE_PATH_SIZE] = {0};

    snprintf(parent, sizeof(parent), "%s/Library/Application Support/G2-Edit", home);
    snprintf(out, outSize, "%s/Recovery", parent);

    // Both levels, ignoring "already there" — anything else is a real failure and the caller's
    // write will report it.
    if ((mkdir(parent, 0755) != 0) && (errno != EEXIST)) {
        LOG_ERROR("Could not create %s (errno %d)\n", parent, errno);
        return false;
    }

    if ((mkdir(out, 0755) != 0) && (errno != EEXIST)) {
        LOG_ERROR("Could not create %s (errno %d)\n", out, errno);
        return false;
    }
    return true;
}

// Keeps the newest RECOVERY_KEEP .pch2 files and deletes the rest. Called after writing, so a
// conflict's own files are always among the newest and can never be the ones pruned.
static void prune_recovery_folder(const char * folder) {
    struct dirent * entry                     = NULL;
    DIR *           dir                       = opendir(folder);

    if (dir == NULL) {
        return;
    }
    char            paths[64][FILE_PATH_SIZE] = {0};
    time_t          times[64]                 = {0};
    size_t          count                     = 0;

    while (((entry = readdir(dir)) != NULL) && (count < 64)) {
        const char * dot = strrchr(entry->d_name, '.');

        if ((dot == NULL) || (strcmp(dot, ".pch2") != 0)) {
            continue;
        }
        struct stat  st;

        snprintf(paths[count], FILE_PATH_SIZE, "%s/%s", folder, entry->d_name);

        if (stat(paths[count], &st) == 0) {
            times[count] = st.st_mtime;
            count++;
        }
    }
    closedir(dir);

    if (count <= RECOVERY_KEEP) {
        return;
    }

    // Selection sort by mtime, oldest first, deleting until only RECOVERY_KEEP remain. count is
    // capped at 64 so the quadratic pass costs nothing worth optimising.
    for (size_t removed = 0; removed < (count - RECOVERY_KEEP); removed++) {
        size_t oldest = SIZE_MAX;

        for (size_t i = 0; i < count; i++) {
            if ((paths[i][0] != '\0') && ((oldest == SIZE_MAX) || (times[i] < times[oldest]))) {
                oldest = i;
            }
        }

        if (oldest == SIZE_MAX) {
            break;
        }
        LOG_DEBUG("Pruning old recovery file %s\n", paths[oldest]);
        remove(paths[oldest]);
        paths[oldest][0] = '\0';
    }
}

uint32_t device_sync_write_recovery_files(uint32_t slotMask, char * outLocation, size_t outLocationSize) {
    char     folder[FILE_PATH_SIZE] = {0};
    uint32_t written                = 0;

    sRecoveryCount = 0;

    if (!recovery_folder(folder, sizeof(folder))) {
        if ((outLocation != NULL) && (outLocationSize > 0)) {
            outLocation[0] = '\0';
        }
        return 0;
    }

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if ((slotMask & (1u << slot)) == 0) {
            continue;
        }
        const char * name                 = gGlobalSettings.slot[slot].patchName;

        if ((name == NULL) || (name[0] == '\0')) {
            name = "Untitled";
        }
        char         path[FILE_PATH_SIZE] = {0};

        // Slot letter keeps two dirty slots with the same patch name from overwriting each other.
        snprintf(path, sizeof(path), "%s/%s (recovered %c).pch2", folder, name, (char)('A' + slot));

        if (write_database_to_file(path, slot) == EXIT_SUCCESS) {
            COPY_STRING(sRecoveryPath[sRecoveryCount], path);
            sRecoveryCount++;
            written++;

            if (outLocation != NULL) {
                // One file: quote it exactly. More than one: the folder they share is more useful
                // than an arbitrary one of them.
                snprintf(outLocation, outLocationSize, "%s", (written == 1) ? path : folder);
            }
        } else {
            LOG_ERROR("Recovery write failed for slot %u: %s\n", slot, path);
        }
    }

    if ((written == 0) && (outLocation != NULL) && (outLocationSize > 0)) {
        outLocation[0] = '\0';
    }
    prune_recovery_folder(folder);

    return written;
}

void device_sync_discard_recovery_files(void) {
    for (uint32_t i = 0; i < sRecoveryCount; i++) {
        LOG_DEBUG("Save As made recovery file redundant, removing %s\n", sRecoveryPath[i]);
        remove(sRecoveryPath[i]);
        sRecoveryPath[i][0] = '\0';
    }

    sRecoveryCount = 0;
}

#ifdef __cplusplus
}
#endif
