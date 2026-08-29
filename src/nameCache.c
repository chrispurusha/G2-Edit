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

// See nameCache.h for why this exists and, more importantly, for the one thing it must never be
// used for.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "types.h"
#include "globalVars.h"
#include "prefs.h"
#include "nameCache.h"

#define NAME_CACHE_KEY         "g2NameTable"
#define NAME_CACHE_DONE_KEY    "g2NameTableComplete"

// ONE FIXED-WIDTH RECORD PER POPULATED LOCATION, and no separators at all. cache.txt is a
// key=value file read a line at a time, so a value cannot contain a newline — and a patch name can
// contain very nearly anything else, which rules out every obvious delimiter. Fixed width sidesteps
// the question: a name is stored space-padded to its full length and trimmed on the way back.
//
//   [0]    'P' patch or 'F' performance
//   [1..2] bank     (2 hex)
//   [3..4] location (2 hex)
//   [5]    category (1 hex, 0-15)
//   [6..]  name, space-padded to CLAVIA_NAME_SIZE
#define REC_LEN    (6 + CLAVIA_NAME_SIZE)

static void append_table(char * out, size_t outMax, size_t * used, char tag,
                         tNameTableEntry * table, uint32_t banks) {
    for (uint32_t bank = 0; bank < banks; bank++) {
        for (uint32_t loc = 0; loc < NUM_LOCATIONS_PER_BANK; loc++) {
            tNameTableEntry * entry                      = &table[(bank * NUM_LOCATIONS_PER_BANK) + loc];

            if (!entry->populated || (*used + REC_LEN + 1 > outMax)) {
                continue;
            }
            // A newline in a name would split the value across two lines and take the rest of the
            // cache with it, since cache.txt is read a line at a time. Nothing else in the file's
            // format is positional, so every other control character is mapped out too rather than
            // reasoning about which ones survive a round trip.
            char              safe[CLAVIA_NAME_SIZE + 1] = {0};

            for (int i = 0; i < CLAVIA_NAME_SIZE; i++) {
                char ch = entry->name[i];

                if (ch == '\0') {
                    break;
                }
                safe[i] = ((ch >= 0x20) && (ch < 0x7F)) ? ch : ' ';
            }

            *used += (size_t)snprintf(out + *used, outMax - *used, "%c%02X%02X%X%-*s",
                                      tag, (unsigned)bank, (unsigned)loc,
                                      (unsigned)(entry->category & 0x0F),
                                      CLAVIA_NAME_SIZE, safe);
        }
    }
}

void name_cache_save(void) {
    // 40 banks * 128 locations at 22 bytes each, plus room to spare. Big for a preferences value,
    // but it is one string written once per sweep and it lives in cache.txt, not prefs.txt.
    size_t max  = (size_t)((NUM_PATCH_BANKS + NUM_PERF_BANKS) * NUM_LOCATIONS_PER_BANK * REC_LEN) + 64;
    char * blob = (char *)calloc(1, max);

    if (blob == NULL) {
        return;
    }
    size_t used = 0;

    append_table(blob, max, &used, 'P', &gPatchNameTable[0][0], NUM_PATCH_BANKS);
    append_table(blob, max, &used, 'F', &gPerfNameTable[0][0], NUM_PERF_BANKS);

    cache_set_string(NAME_CACHE_KEY, blob);
    free(blob);
}

bool name_cache_load(void) {
    const char * blob     = cache_get_string(NAME_CACHE_KEY, "");

    if ((blob == NULL) || (strlen(blob) < REC_LEN)) {
        return false;
    }
    size_t       length   = strlen(blob);
    uint32_t     restored = 0;

    for (size_t at = 0; (at + REC_LEN) <= length; at += REC_LEN) {
        const char *      rec    = blob + at;
        char              hex[3] = {0};
        unsigned          bank   = 0;
        unsigned          loc    = 0;
        tNameTableEntry * table  = NULL;
        uint32_t          banks  = 0;

        if (rec[0] == 'P') {
            table = &gPatchNameTable[0][0];
            banks = NUM_PATCH_BANKS;
        } else if (rec[0] == 'F') {
            table = &gPerfNameTable[0][0];
            banks = NUM_PERF_BANKS;
        } else {
            return restored != 0;    // not our format, or truncated — keep whatever came before it
        }
        hex[0]                        = rec[1];
        hex[1]                        = rec[2];
        bank                          = (unsigned)strtoul(hex, NULL, 16);
        hex[0]                        = rec[3];
        hex[1]                        = rec[4];
        loc                           = (unsigned)strtoul(hex, NULL, 16);

        if ((bank >= banks) || (loc >= NUM_LOCATIONS_PER_BANK)) {
            continue;
        }
        hex[0]                        = rec[5];
        hex[1]                        = '\0';

        tNameTableEntry * entry = &table[(bank * NUM_LOCATIONS_PER_BANK) + loc];

        entry->category               = (uint8_t)strtoul(hex, NULL, 16);
        memcpy(entry->name, rec + 6, CLAVIA_NAME_SIZE);
        entry->name[CLAVIA_NAME_SIZE] = '\0';

        // The record was space-padded to a fixed width; a real trailing space in a patch name is
        // indistinguishable from padding and is not worth preserving.
        for (int i = CLAVIA_NAME_SIZE - 1; (i >= 0) && (entry->name[i] == ' '); i--) {
            entry->name[i] = '\0';
        }

        entry->populated              = true;
        restored++;
    }

    return restored != 0;
}

bool name_cache_is_complete(void) {
    // Absent key means a cache written before this flag existed, or none at all. Treat that as
    // INCOMPLETE: re-sweeping costs eight seconds once, where trusting a list with holes in it
    // shows the user a bank location that looks empty and is not.
    return strcmp(cache_get_string(NAME_CACHE_DONE_KEY, "0"), "1") == 0;
}

void name_cache_set_complete(bool complete) {
    cache_set_string(NAME_CACHE_DONE_KEY, complete ? "1" : "0");
}
