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
// Notes: Docs/code-notes/g2Patch.c.md - "// notes §k" refers there.

// notes §1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "dataBase.h"
#include "globalVars.h"
#include "protocol.h"
#include "utils.h"
#include "soundEngine.h"
#include "g2Patch.h"

// Parse a .pch2 image already in memory. Split out from the file loader so the built-in patch and a
// file on disk go through exactly one implementation of the CRC check and the header walk.
bool g2_plugin_parse_patch(const uint8_t * buff, int64_t fileSize, uint32_t slot) {
    int64_t  byteOffset = 0;
    uint32_t readCrc    = 0;
    uint32_t calcCrc    = 0;
    uint8_t  type       = 0;

    // A patch carries a header, a version and type byte, and a trailing CRC, so anything this short
    // cannot be one — and the offsets below would run off the end working it out.
    if ((buff == NULL) || (fileSize < 8) || (slot >= MAX_SLOTS)) {
        return false;
    }

    // The binary section starts after the ASCII header, which is terminated by the first NUL.
    for (int64_t i = 0; i < fileSize; i++) {
        if (buff[i] == 0x00) {
            byteOffset = i + 1;
            break;
        }
    }

    if ((byteOffset == 0) || (byteOffset + 2 >= fileSize)) {
        return false;
    }
    readCrc = (uint32_t)(buff[fileSize - 2] << 8 | buff[fileSize - 1]);
    calcCrc = calc_crc16(buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));

    if (readCrc != calcCrc) {
        return false;
    }
    byteOffset++;                   // version, not needed here
    type = buff[byteOffset++];

    if (type != 0) {                // 0 is a patch; a performance (1) has four slots and no meaning here
        return false;
    }
    clear_slot_data(slot);
    // notes §2
    parse_patch(slot, (uint8_t *)(buff + byteOffset), (uint32_t)((fileSize - byteOffset) - 2));
    return true;
}

bool g2_plugin_load_patch(const char * filepath, uint32_t slot) {
    FILE *    file       = NULL;
    uint8_t * buff       = NULL;
    int64_t   fileSize   = 0;
    size_t    readSize   = 0;
    bool      loaded     = false;

    if ((filepath == NULL) || (filepath[0] == '\0') || (slot >= MAX_SLOTS)) {
        return false;
    }
    file = fopen(filepath, "rb");

    if (file == NULL) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    clearerr(file);

    // A patch carries a header, a version and type byte, and a trailing CRC, so anything this short
    // cannot be one — and the offsets below would run off the end of the buffer working it out.
    if (fileSize < 8) {
        fclose(file);
        return false;
    }
    buff = (uint8_t *)malloc((size_t)fileSize);

    if (buff == NULL) {
        fclose(file);
        return false;
    }
    readSize = fread(buff, 1, (size_t)fileSize, file);
    fclose(file);

    if (readSize != (size_t)fileSize) {
        free(buff);
        return false;
    }

    loaded = g2_plugin_parse_patch(buff, fileSize, slot);
    free(buff);

    return loaded;
}

// The whole file, or NULL. The caller frees it.
static uint8_t * read_whole_file(const char * filepath, int64_t * size) {
    FILE *    file = NULL;
    uint8_t * buff = NULL;
    int64_t   len  = 0;

    if ((filepath == NULL) || (filepath[0] == '\0')) {
        return NULL;
    }
    file = fopen(filepath, "rb");

    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    len = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (len < 8) {
        fclose(file);
        return NULL;
    }
    buff = (uint8_t *)malloc((size_t)len);

    if ((buff != NULL) && (fread(buff, 1, (size_t)len, file) != (size_t)len)) {
        free(buff);
        buff = NULL;
    }
    fclose(file);
    *size = len;
    return buff;
}

// Copies `path` into a remembered-path buffer unless it already IS that buffer - File > Save hands
// the loader's own buffer back, and copying onto itself is undefined (feedback: COPY_STRING trap).
static void remember(char * dest, const char * path) {
    if (dest != path) {
        snprintf(dest, FILE_PATH_SIZE, "%s", path);
    }
}

tG2FileKind g2_plugin_open_file(const char * filepath, uint32_t slot) {
    int64_t     fileSize   = 0;
    int64_t     byteOffset = 0;
    uint8_t *   buff       = read_whole_file(filepath, &fileSize);
    uint8_t     type       = 0;
    tG2FileKind kind       = eG2FileFailed;

    if (buff == NULL) {
        return eG2FileFailed;
    }

    // The same header walk and CRC check g2_plugin_parse_patch() makes, then a branch on the type.
    for (int64_t i = 0; i < fileSize; i++) {
        if (buff[i] == 0x00) {
            byteOffset = i + 1;
            break;
        }
    }

    if ((byteOffset == 0) || ((byteOffset + 2) >= fileSize)
       || ((uint32_t)((buff[fileSize - 2] << 8) | buff[fileSize - 1])
           != calc_crc16(buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2)))) {
        free(buff);
        return eG2FileFailed;
    }
    byteOffset++;                   // version
    type = buff[byteOffset++];

    if ((type == 0) && (slot < MAX_SLOTS)) {
        clear_slot_data(slot);
        parse_patch(slot, buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));
        set_patch_name_from_filename(slot, filepath);
        remember(gSavedPatchPath[slot], filepath);
        kind = eG2FilePatch;
    } else if (type == 1) {
        // notes §3
        char * dot = NULL;

        for (uint32_t i = 0; i < MAX_SLOTS; i++) {
            clear_slot_data(i);
        }
        {
            const char * slash = strrchr(filepath, '/');

            snprintf(gGlobalSettings.perfName, sizeof(gGlobalSettings.perfName), "%s",
                     (slash != NULL) ? (slash + 1) : filepath);
        }
        dot = strrchr(gGlobalSettings.perfName, '.');

        if (dot != NULL) {
            *dot = '\0';
        }
        gGlobalSettings.perfMode = 1;
        parse_perf(buff + byteOffset, (int)((fileSize - byteOffset) - 2));
        remember(gSavedPerfPath, filepath);
        kind = eG2FilePerformance;
    }
    free(buff);
    return kind;
}
