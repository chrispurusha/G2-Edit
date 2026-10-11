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
// Notes: Docs/code-notes/slotSets.c.md - "// notes §k" refers there.

#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "globalVars.h"
#include "protocol.h"
#include "patchWrite.h"
#include "soundEngine.h"
#include "slotSets.h"

// notes §2 - where a .pch2/.prf2 image's data starts, after its text header and the version and type bytes; false
// if the header or the CRC is wrong. The data ends 2 bytes before `size`.
bool file_image_body(const uint8_t * buff, int64_t size, int64_t * body, uint8_t * type) {
    int64_t at = 0;

    if ((buff == NULL) || (size < 8)) {
        return false;
    }

    for (int64_t i = 0; i < size; i++) {
        if (buff[i] == 0x00) {
            at = i + 1;
            break;
        }
    }

    if (  (at == 0) || ((at + 2) >= size)
       || ((uint32_t)((buff[size - 2] << 8) | buff[size - 1]) != calc_crc16(buff + at, (uint32_t)((size - at) - 2)))) {
        return false;
    }
    *type = buff[at + 1];
    *body = at + 2;
    return true;
}

// notes §2
bool perf_image_parse(const uint8_t * buff, int64_t size) {
    int64_t body = 0;
    uint8_t type = 0;

    if ((file_image_body(buff, size, &body, &type) == false) || (type != 1)) {
        return false;
    }
    database_write_lock();                             // parse_perf() does not take it
    parse_perf((uint8_t *)(buff + body), (int)((size - body) - 2));
    perf_engine_only_header_apply(buff, (size_t)body); // patchWrite.c notes §3
    database_write_unlock();
    return true;
}

// notes §3
void slot_set_other_take(const uint8_t * image, size_t length, const char * perfName) {
    free(gOtherSlotSet);
    gOtherSlotSet    = NULL;
    gOtherSlotSetLen = 0;

    if ((image != NULL) && (length > 0u)) {
        gOtherSlotSet = (uint8_t *)malloc(length);

        if (gOtherSlotSet != NULL) {
            memcpy(gOtherSlotSet, image, length);
            gOtherSlotSetLen = length;
        }
    }
    snprintf(gOtherPerfName, sizeof(gOtherPerfName), "%s", (perfName != NULL) ? perfName : "");
}

// notes §1
void perf_mode_set(bool perf) {
    if (device_ready()) {
        // The G2 swaps its own sets; the editor reads the new one back
        tMessageContent msg = {0};

        msg.cmd                  = perf ? eMsgCmdWriteModePerf : eMsgCmdWriteModePatch;
        gGlobalSettings.perfMode = perf ? 1 : 0;
        msg_send(&gToUsbThread, &msg);
        return;
    }

    if (perf == (gGlobalSettings.perfMode == 1)) {
        return;
    }
    size_t    length  = 0;
    uint8_t * leaving = NULL;
    char      name[CLAVIA_NAME_SIZE + 1];

    database_read_lock();
    leaving                  = write_perf_to_memory(&length, true); // patchWrite.c notes §3 - everything, engine-only too
    database_read_unlock();
    snprintf(name, sizeof(name), "%s", gGlobalSettings.perfName);

    database_write_lock();

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        clear_slot_data(slot);
        init_patch(slot);
    }

    database_write_unlock();

    // The set this mode had when it was last left; a mode never used yet starts empty, as the G2's first does
    if ((gOtherSlotSet != NULL) && (perf_image_parse(gOtherSlotSet, (int64_t)gOtherSlotSetLen) == true)) {
        snprintf(gGlobalSettings.perfName, sizeof(gGlobalSettings.perfName), "%s", gOtherPerfName);
    }
    slot_set_other_take(leaving, length, name);
    free(leaving);

    gGlobalSettings.perfMode = perf ? 1 : 0;

    database_read_lock();
    sound_engine_update_from_patch();
    database_read_unlock();

    if (perf) {
        sound_engine_start_performance();   // soundEngine notes §216
    }
}
