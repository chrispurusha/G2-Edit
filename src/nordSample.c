/*
 * The G2 Editor application.
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
// Notes: Docs/code-notes/nordSample.c.md - "// notes §k" refers there.

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nordSample.h"

#define NSMP_SAMPLE_RATE      (44100.0)
#define NSMP_FULL_SCALE       (8192.0)      // notes §2 - 14-bit samples
#define NSMP_CHUNKS_START     (0x18)
#define NSMP_CHUNK_HEADER     (9)           // tag (4), type (1), length (4)
#define NSMP_WORD_BYTES       (3)
#define NSMP_MAX_ORDER        (7)
#define NSMP_HEADER_SEARCH    (120)         // notes §1 - where a zone's coded blocks may begin

// notes §2 - the fixed predictors, (1 - z^-1)^order
static const int32_t kPredictor[NSMP_MAX_ORDER + 1][NSMP_MAX_ORDER] = {
    {0,   0,  0,   0,  0,  0, 0},
    {1,   0,  0,   0,  0,  0, 0},
    {2,  -1,  0,   0,  0,  0, 0},
    {3,  -3,  1,   0,  0,  0, 0},
    {4,  -6,  4,  -1,  0,  0, 0},
    {5, -10, 10,  -5,  1,  0, 0},
    {6, -15, 20, -15,  6, -1, 0},
    {7, -21, 35, -35, 21, -7, 1},
};

typedef struct {
    bool     stop;
    uint32_t bitWidth;
    uint32_t order;
    uint32_t count;
} tBlockHeader;

static uint32_t read_u32(const uint8_t * p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static tBlockHeader block_header(const uint8_t * p) {
    uint32_t     word   = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    tBlockHeader header = {0};

    header.bitWidth = ((word >> 19) & 0xFu) + 1u;
    header.order    = (word >> 14) & 0xFu;
    header.count    = word & 0x3FFFu;
    header.stop     = (((word >> 23) & 1u) == 1u) && (header.bitWidth == 1u) && (header.order == 0u);
    return header;
}

static uint32_t block_words(const tBlockHeader * header) {
    return ((header->count * header->bitWidth) + ((NSMP_WORD_BYTES * 8u) - 1u)) / (NSMP_WORD_BYTES * 8u);
}

// notes §1 - the blocks chain from here to a stop block that ends the zone exactly
static bool chains_to_end(const uint8_t * data, uint32_t length, uint32_t start, uint32_t * samples) {
    uint32_t p = start;

    *samples = 0;

    while ((p + NSMP_WORD_BYTES) <= length) {
        tBlockHeader header = block_header(data + p);

        p        += NSMP_WORD_BYTES;

        if (header.stop) {
            return p == length;
        }

        if (header.order > NSMP_MAX_ORDER) {
            return false;
        }
        p        += block_words(&header) * NSMP_WORD_BYTES;
        *samples += header.count;
    }
    return false;
}

// notes §3 - the zone header's fields, at these offsets into the zone's chunk
#define ZONE_ROOT_KEY        (0x05)
#define ZONE_RATE            (0x06)
#define ZONE_FIRST_BLOCK     (0x12)
#define ZONE_LOOP_START      (0x1b)
#define ZONE_LOOP_END        (0x24)
#define ZONE_HEADER_BYTES    (0x31)

// notes §3 - a header position names the coded block at this file offset
static uint32_t block_offset(uint32_t position) {
    return NSMP_CHUNKS_START + (position * NSMP_WORD_BYTES);
}

// notes §1-§3 - decode one zone; `at` is the zone chunk's offset in the file, which the header's positions
// count from
static bool decode_zone(const uint8_t * data, uint32_t length, uint32_t at, tNordZone * zone) {
    uint32_t start                   = 0;
    uint32_t samples                 = 0;
    bool     headed                  = false;

    if (length >= ZONE_HEADER_BYTES) {
        uint32_t first = block_offset(read_u32(data + ZONE_FIRST_BLOCK));

        headed = (first > at) && ((first - at) < length) && chains_to_end(data, length, first - at, &samples);
        start  = headed ? (first - at) : 0u;
    }

    while (!headed && (start < NSMP_HEADER_SEARCH) && !chains_to_end(data, length, start, &samples)) {
        start++;
    }

    if ((!headed && (start >= NSMP_HEADER_SEARCH)) || (samples == 0u)) {
        return false;
    }
    uint32_t loopStartAt             = headed ? (block_offset(read_u32(data + ZONE_LOOP_START)) - at) : 0u;
    uint32_t loopEndAt               = headed ? (block_offset(read_u32(data + ZONE_LOOP_END)) - at) : 0u;
    bool     sawStart                = false;
    bool     sawEnd                  = false;
    float *  out                     = malloc(sizeof(float) * samples);
    int32_t  history[NSMP_MAX_ORDER] = {0};
    uint32_t written                 = 0;
    uint32_t p                       = start;

    if (out == NULL) {
        return false;
    }

    for ( ; ;) {
        tBlockHeader header = block_header(data + p);

        // notes §5 - the loop points are the sample counts where these two blocks begin
        if (headed && (p == loopStartAt)) {
            zone->loopStart = written;
            sawStart        = true;
        }

        if (headed && (p == loopEndAt)) {
            zone->loopEnd = written;
            sawEnd        = true;
        }
        p += NSMP_WORD_BYTES;

        if (header.stop) {
            break;
        }
        // Residuals, signed, most significant bit first, through a 64-bit window
        uint64_t     window = 0;
        uint32_t     held   = 0;
        uint32_t     words  = block_words(&header);

        for (uint32_t i = 0; i < header.count; i++) {
            while (held < header.bitWidth) {
                uint32_t word = ((uint32_t)data[p] << 16) | ((uint32_t)data[p + 1] << 8) | (uint32_t)data[p + 2];

                window |= (uint64_t)word << (40u - held);
                held   += 24u;
                p      += NSMP_WORD_BYTES;
                words--;
            }
            uint32_t width    = ((header.bitWidth - 1u) & 0xFu) + 1u;   // 1..16 by construction
            int64_t  residual = (int64_t)window >> (64u - width);
            int64_t  predict  = 0;

            window       <<= header.bitWidth;
            held          -= header.bitWidth;

            for (uint32_t k = 0; k < NSMP_MAX_ORDER; k++) {
                predict += (int64_t)kPredictor[header.order][k] * history[k];
            }

            int32_t  y        = (int32_t)(residual + predict);

            memmove(&history[1], &history[0], sizeof(history[0]) * (NSMP_MAX_ORDER - 1));
            history[0]     = y;
            out[written++] = (float)((double)y / NSMP_FULL_SCALE);
        }

        p += words * NSMP_WORD_BYTES;   // the block's padding, if the last word was not used up
    }

    zone->data       = out;
    zone->length     = written;
    zone->looped     = sawStart && sawEnd && (zone->loopStart < zone->loopEnd) && (zone->loopEnd < written);

    uint32_t rate = headed ? (((uint32_t)data[ZONE_RATE] << 8) | data[ZONE_RATE + 1]) : 0u;

    zone->sampleRate = ((rate >= 8000u) && (rate <= 96000u)) ? (double)rate : NSMP_SAMPLE_RATE;
    zone->rootNote   = (headed && (data[ZONE_ROOT_KEY] < 128u)) ? (double)data[ZONE_ROOT_KEY] : -1.0;
    return true;
}

// notes §3 - the zone's pitch, from its period, until the key map is read
static double zone_root_note(const tNordZone * zone, double sampleRate) {
    const uint32_t window  = 4096;
    uint32_t       from    = zone->length / 3;

    if ((from + window + (uint32_t)(sampleRate / 40.0)) > zone->length) {
        return 60.0;
    }
    double         best    = 0.0;
    uint32_t       bestLag = 0;

    for (uint32_t lag = (uint32_t)(sampleRate / 2000.0); lag < (uint32_t)(sampleRate / 40.0); lag++) {
        double sum = 0.0;

        for (uint32_t i = 0; i < window; i += 2) {
            sum += (double)zone->data[from + i] * zone->data[from + i + lag];
        }

        if (sum > best) {
            best    = sum;
            bestLag = lag;
        }
    }

    return (bestLag == 0) ? 60.0 : (69.0 + (12.0 * log2((sampleRate / (double)bestLag) / 440.0)));
}

bool nord_sample_load(const char * path, tNordSample * sample) {
    FILE *    file = fopen(path, "rb");

    memset(sample, 0, sizeof(*sample));

    if (file == NULL) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    long      size = ftell(file);
    uint8_t * d    = (size > 0) ? malloc((size_t)size) : NULL;

    fseek(file, 0, SEEK_SET);

    if ((d == NULL) || (fread(d, 1, (size_t)size, file) != (size_t)size)) {
        fclose(file);
        free(d);
        return false;
    }
    fclose(file);

    if ((size < NSMP_CHUNKS_START) || (memcmp(d, "CBIN", 4) != 0) || (memcmp(d + 8, "nsmp", 4) != 0)) {
        free(d);
        return false;
    }

    // notes §1 - tagged chunks: tag, type, length, data
    for (uint32_t p = NSMP_CHUNKS_START; (p + NSMP_CHUNK_HEADER) <= (uint32_t)size;) {
        uint32_t length = read_u32(d + p + 5);

        if ((p + NSMP_CHUNK_HEADER + length) > (uint32_t)size) {
            break;
        }

        if ((memcmp(d + p, "stk", 3) == 0) && (sample->zoneCount < NORD_SAMPLE_MAX_ZONES)) {
            tNordZone * zone = &sample->zone[sample->zoneCount];

            if (decode_zone(d + p + NSMP_CHUNK_HEADER, length, p + NSMP_CHUNK_HEADER, zone)) {
                if (zone->rootNote < 0.0) {
                    zone->rootNote = zone_root_note(zone, zone->sampleRate);   // notes §3 - no header to read it from
                }
                sample->zoneCount++;
            }
        }
        p += NSMP_CHUNK_HEADER + length;
    }

    free(d);
    return sample->zoneCount > 0;
}

void nord_sample_free(tNordSample * sample) {
    for (uint32_t z = 0; z < sample->zoneCount; z++) {
        free(sample->zone[z].data);
    }

    memset(sample, 0, sizeof(*sample));
}

// notes §4 - every file loaded once and kept: a patch rebuild must never wait on the disk twice
#define NSMP_CACHE_SIZE    (32)

typedef struct {
    char        path[1024];
    bool        loaded;
    tNordSample sample;
} tCacheEntry;

static tCacheEntry     sCache[NSMP_CACHE_SIZE];
static uint32_t        sCacheCount;
static pthread_mutex_t sCacheLock = PTHREAD_MUTEX_INITIALIZER;

const tNordSample * nord_sample_get(const char * path) {
    const tNordSample * found = NULL;

    if ((path == NULL) || (path[0] == '\0')) {
        return NULL;
    }
    pthread_mutex_lock(&sCacheLock);

    for (uint32_t i = 0; i < sCacheCount; i++) {
        if (strcmp(sCache[i].path, path) == 0) {
            found = sCache[i].loaded ? &sCache[i].sample : NULL;
            pthread_mutex_unlock(&sCacheLock);
            return found;
        }
    }

    if (sCacheCount < NSMP_CACHE_SIZE) {
        tCacheEntry * entry = &sCache[sCacheCount++];

        snprintf(entry->path, sizeof(entry->path), "%s", path);
        entry->loaded = nord_sample_load(path, &entry->sample);   // a failure is remembered too
        found         = entry->loaded ? &entry->sample : NULL;
    }
    pthread_mutex_unlock(&sCacheLock);
    return found;
}
