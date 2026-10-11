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

#define NSMP_SAMPLE_RATE    (44100.0)
#define NSMP_FULL_SCALE     (8192.0)        // notes §2 - 14-bit samples
// notes §8 - the two layouts: the original, and the later one with 12-byte chunk headers and 32-bit words
typedef struct {
    uint32_t chunksStart;
    uint32_t chunkHeader;   // tag, version, length
    uint32_t tagAt;         // where the three letters sit in the tag
    uint32_t lengthAt;
    uint32_t wordBytes;
    bool     wordPerChannel;    // notes §9 - each channel packs its own words, which alternate
} tFormat;

static const tFormat kFormatOriginal                                = {0x18, 9, 0, 5, 3, false};
static const tFormat kFormatLater                                   = {0x2c, 12, 1, 8, 4, true};
#define NSMP_MAX_ORDER           (7)
#define NSMP_MAX_RETUNE_CENTS    (50.0)     // notes §7 - beyond this the loop is not a whole-cycle one
#define NSMP_HEADER_SEARCH       (120)      // notes §1 - where a zone's coded blocks may begin
#define NSMP_MAX_CHANNELS        (2)

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

static uint32_t read_word(const uint8_t * p, uint32_t bytes) {
    uint32_t word = 0;

    for (uint32_t i = 0; i < bytes; i++) {
        word = (word << 8) | p[i];
    }

    return word;
}

static tBlockHeader block_header(const uint8_t * p, const tFormat * f) {
    uint32_t     word   = read_word(p, f->wordBytes);
    tBlockHeader header = {0};

    header.bitWidth = ((word >> 19) & 0xFu) + 1u;
    header.order    = (word >> 14) & 0xFu;
    header.count    = word & 0x3FFFu;
    header.stop     = (((word >> 23) & 1u) == 1u) && (header.bitWidth == 1u) && (header.order == 0u);
    return header;
}

static uint32_t packed_words(uint32_t count, uint32_t bitWidth, const tFormat * f) {
    return ((count * bitWidth) + ((f->wordBytes * 8u) - 1u)) / (f->wordBytes * 8u);
}

// notes §9 - the words after a block's header
static uint32_t block_words(const tBlockHeader * header, uint32_t channels, const tFormat * f) {
    return f->wordPerChannel ? (channels * packed_words(header->count / channels, header->bitWidth, f))
           : packed_words(header->count, header->bitWidth, f);
}

// notes §1 - the blocks chain from here to a stop block that ends the zone exactly
static bool chains_to_end(const uint8_t * data, uint32_t length, uint32_t start, uint32_t * samples, uint32_t channels,
                          const tFormat * f) {
    uint32_t p = start;

    *samples = 0;

    while ((p + f->wordBytes) <= length) {
        tBlockHeader header = block_header(data + p, f);

        p        += f->wordBytes;

        if (header.stop) {
            return p == length;
        }

        if (header.order > NSMP_MAX_ORDER) {
            return false;
        }
        p        += block_words(&header, channels, f) * f->wordBytes;
        *samples += header.count;
    }
    return false;
}

// notes §3 - the zone header's fields, at these offsets into the zone's chunk
#define ZONE_ROOT_KEY        (0x05)
#define ZONE_RATE            (0x06)
#define ZONE_CHANNELS        (0x08)   // notes §9
#define ZONE_LEVEL_DB        (0x39)   // notes §10 - the later layout: a float, the zone's level in dB
#define ZONE_FIRST_BLOCK     (0x12)
#define ZONE_LOOP_START      (0x24)   // notes §5 - the loop runs from here to the zone's end
#define ZONE_HEADER_BYTES    (0x31)

// notes §3 - a header position names the coded block at this file offset
static uint32_t block_offset(uint32_t position, const tFormat * f) {
    return f->chunksStart + (position * f->wordBytes);
}

// notes §1-§3 - decode one zone; `at` is the zone chunk's offset in the file, which the header's positions
// count from
static bool decode_zone(const uint8_t * data, uint32_t length, uint32_t at, tNordZone * zone, const tFormat * f) {
    uint32_t start                                      = 0;
    uint32_t samples                                    = 0;
    bool     headed                                     = false;
    uint32_t channels                                   = (  (length > ZONE_CHANNELS) && (data[ZONE_CHANNELS] >= 1u)
                                                          && (data[ZONE_CHANNELS] <= NSMP_MAX_CHANNELS)) ? data[ZONE_CHANNELS] : 1u;

    if (length >= ZONE_HEADER_BYTES) {
        uint32_t first = block_offset(read_u32(data + ZONE_FIRST_BLOCK), f);

        headed = (first > at) && ((first - at) < length) && chains_to_end(data, length, first - at, &samples, channels, f);
        start  = headed ? (first - at) : 0u;
    }

    while (!headed && (start < NSMP_HEADER_SEARCH) && !chains_to_end(data, length, start, &samples, channels, f)) {
        start++;
    }

    if ((!headed && (start >= NSMP_HEADER_SEARCH)) || (samples == 0u)) {
        return false;
    }
    uint32_t loopStartAt                                = headed ? (block_offset(read_u32(data + ZONE_LOOP_START), f) - at) : 0u;
    bool     sawStart                                   = false;
    float *  out                                        = malloc(sizeof(float) * samples);
    int32_t  history[NSMP_MAX_CHANNELS][NSMP_MAX_ORDER] = {{0}};
    uint32_t written                                    = 0; // samples, all channels
    uint32_t p                                          = start;

    if (out == NULL) {
        return false;
    }

    for ( ; ;) {
        tBlockHeader header   = block_header(data + p, f);

        // notes §5 - the loop start is the frame where this block begins
        if (headed && (p == loopStartAt)) {
            zone->loopStart = written / channels;
            sawStart        = true;
        }
        p += f->wordBytes;

        if (header.stop) {
            break;
        }
        uint32_t     streams  = f->wordPerChannel ? channels : 1u;
        uint32_t     perWords = packed_words(header.count / streams, header.bitWidth, f);

        for (uint32_t i = 0; i < header.count; i++) {
            // notes §9 - sample i belongs to channel i % channels; with words per channel, that channel's j-th
            // sample is in its own bit stream, whose k-th word is the block's (k * channels + channel)-th
            uint32_t  channel  = i % channels;
            uint32_t  stream   = f->wordPerChannel ? channel : 0u;
            uint32_t  index    = f->wordPerChannel ? (i / channels) : i;
            uint32_t  bit      = index * header.bitWidth;
            uint32_t  bits     = f->wordBytes * 8u;
            uint64_t  window   = 0;

            for (uint32_t w = 0; w < 2u; w++) {
                uint32_t k    = (bit / bits) + w;
                uint32_t word = (k < perWords) ? read_word(data + p + (((k * streams) + stream) * f->wordBytes), f->wordBytes) : 0u;

                window |= (uint64_t)word << (64u - (bits * (w + 1u)));
            }

            uint32_t  width    = ((header.bitWidth - 1u) & 0xFu) + 1u;  // 1..16 by construction
            int64_t   residual = (int64_t)(window << (bit % bits)) >> (64u - width);
            int64_t   predict  = 0;
            int32_t * h        = history[channel];

            for (uint32_t k = 0; k < NSMP_MAX_ORDER; k++) {
                predict += (int64_t)kPredictor[header.order][k] * h[k];
            }

            int32_t   y        = (int32_t)(residual + predict);

            memmove(&h[1], &h[0], sizeof(h[0]) * (NSMP_MAX_ORDER - 1));
            h[0]           = y;
            out[written++] = (float)((double)y / NSMP_FULL_SCALE);
        }

        p += block_words(&header, channels, f) * f->wordBytes;
    }

    zone->channels   = channels;
    zone->data       = out;
    zone->length     = written / channels;
    zone->looped     = sawStart && (zone->loopStart + 1u < zone->length);
    zone->loopEnd    = zone->length;

    uint32_t rate = headed ? (((uint32_t)data[ZONE_RATE] << 8) | data[ZONE_RATE + 1]) : 0u;

    zone->sampleRate = ((rate >= 8000u) && (rate <= 96000u)) ? (double)rate : NSMP_SAMPLE_RATE;
    zone->rootNote   = (headed && (data[ZONE_ROOT_KEY] < 128u)) ? (double)data[ZONE_ROOT_KEY] : -1.0;

    // notes §7 - the loop is a whole number of cycles: its length over that number is the zone's true period,
    // and the rate that puts the root exactly in tune follows from it
    if (zone->looped && (zone->rootNote >= 0.0)) {
        double rootHz = 440.0 * exp2((zone->rootNote - 69.0) / 12.0);
        double span   = (double)(zone->length - zone->loopStart);
        double cycles = round((span * rootHz) / zone->sampleRate);
        double tuned  = (cycles >= 1.0) ? (rootHz * (span / cycles)) : 0.0;

        if ((tuned > 0.0) && (fabs(1200.0 * log2(tuned / zone->sampleRate)) < NSMP_MAX_RETUNE_CENTS)) {
            zone->sampleRate = tuned;
        }
    }
    zone->id         = read_u32(data);
    zone->gain       = 1.0;
    zone->velLow     = 0;
    zone->velHigh    = 127;

    if (f->wordPerChannel && (length >= (ZONE_LEVEL_DB + 4u))) {
        union {
            uint32_t word;
            float    value;
        } level = {.word = read_u32(data + ZONE_LEVEL_DB)};

        if (isfinite(level.value) && (fabsf(level.value) < 48.0f)) {
            zone->gain = pow(10.0, level.value / 20.0);
        }
    }
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
            sum += (double)zone->data[(from + i) * zone->channels] * zone->data[(from + i + lag) * zone->channels];
        }

        if (sum > best) {
            best    = sum;
            bestLag = lag;
        }
    }

    return (bestLag == 0) ? 60.0 : (69.0 + (12.0 * log2((sampleRate / (double)bestLag) / 440.0)));
}

// notes §6 - the key map: levels and tunings, 128 per-key entries, then one entry per zone with its top key
#define MAP_HEADER_BYTES    (15)
#define MAP_KEY_BYTES       (6)
#define MAP_KEYS            (128)
#define MAP_ZONE_BYTES      (15)
#define MAP_UNITY           (1048576.0)     // 2^20: a level of 0 dB
#define MAP_DETUNE_STEPS    (256.0)         // a semitone

static int32_t read_s24(const uint8_t * p) {
    int32_t v = (int32_t)(((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2]);

    return (v & 0x800000) ? (v - 0x1000000) : v;
}

static uint32_t read_u24(const uint8_t * p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static int compare_top_key(const void * a, const void * b) {
    return ((const tNordZone *)a)->keyHigh - ((const tNordZone *)b)->keyHigh;
}

static void apply_key_map(const uint8_t * map, uint32_t length, tNordSample * sample) {
    uint32_t        fixed   = MAP_HEADER_BYTES + (MAP_KEYS * MAP_KEY_BYTES) + 3u;
    uint32_t        entries = (length > fixed) ? ((length - fixed) / MAP_ZONE_BYTES) : 0u;

    // Only the layout seen in these files: anything else keeps the nearest-root choice
    if ((entries == 0u) || (entries != sample->zoneCount) || ((fixed + (entries * MAP_ZONE_BYTES)) != length)) {
        return;
    }
    double          global  = (double)read_u24(map) / MAP_UNITY;
    const uint8_t * entry   = map + fixed;
    uint32_t        found   = 0;

    for (uint32_t e = 0; e < entries; e++, entry += MAP_ZONE_BYTES) {
        for (uint32_t z = 0; z < sample->zoneCount; z++) {
            tNordZone * zone = &sample->zone[z];

            if (zone->id == read_u24(entry)) {
                zone->gain    = global * ((double)read_u24(entry + 3) / MAP_UNITY);
                zone->detune  = (double)read_s24(entry + 6) / MAP_DETUNE_STEPS;
                zone->keyHigh = entry[9];
                found++;
            }
        }
    }

    if (found != sample->zoneCount) {
        for (uint32_t z = 0; z < sample->zoneCount; z++) {
            sample->zone[z].gain   = 1.0;   // a map that does not name every zone is not trusted at all
            sample->zone[z].detune = 0.0;
        }

        return;
    }
    qsort(sample->zone, sample->zoneCount, sizeof(sample->zone[0]), compare_top_key);

    for (uint32_t z = 0; z < sample->zoneCount; z++) {
        sample->zone[z].keyLow = (z == 0u) ? 0 : (sample->zone[z - 1u].keyHigh + 1);
    }

    sample->zone[sample->zoneCount - 1u].keyHigh = 127;
    sample->mapped                               = true;
}

// notes §10 - the later layout's key map: the same per-key table with 10-byte entries, then one 16-byte
// record per zone - root, top key, bottom key, the zone id, the velocity range
#define MAP2_KEY_BYTES        (10)
#define MAP2_COUNT_AT         (6u + (MAP_KEYS * MAP2_KEY_BYTES) + 29u)
#define MAP2_ZONE_BYTES       (16)
#define MAP2_ZONE_HIGH        (1)
#define MAP2_ZONE_LOW         (2)
#define MAP2_ZONE_ID          (8)
#define MAP2_ZONE_VEL_LOW     (14)
#define MAP2_ZONE_VEL_HIGH    (15)

static void apply_key_map_later(const uint8_t * map, uint32_t length, tNordSample * sample) {
    if (length < (MAP2_COUNT_AT + 3u)) {
        return;
    }
    uint32_t        entries = read_u24(map + MAP2_COUNT_AT);
    const uint8_t * entry   = map + MAP2_COUNT_AT + 3u;
    double          global  = (double)read_u24(map) / MAP_UNITY;
    uint32_t        found   = 0;

    if ((entries != sample->zoneCount) || ((MAP2_COUNT_AT + 3u + (entries * MAP2_ZONE_BYTES)) > length)) {
        return;   // only the layout seen in these files: anything else keeps the nearest-root choice
    }

    for (uint32_t e = 0; e < entries; e++, entry += MAP2_ZONE_BYTES) {
        for (uint32_t z = 0; z < sample->zoneCount; z++) {
            tNordZone * zone = &sample->zone[z];

            if (zone->id == read_u32(entry + MAP2_ZONE_ID)) {
                zone->keyLow  = entry[MAP2_ZONE_LOW];
                zone->keyHigh = entry[MAP2_ZONE_HIGH];
                zone->velLow  = entry[MAP2_ZONE_VEL_LOW];
                zone->velHigh = entry[MAP2_ZONE_VEL_HIGH];
                zone->gain   *= global;
                found++;
            }
        }
    }

    if (found != sample->zoneCount) {
        return;
    }
    int32_t top = -1;

    for (uint32_t z = 0; z < sample->zoneCount; z++) {
        top = (sample->zone[z].keyHigh > top) ? sample->zone[z].keyHigh : top;
    }

    for (uint32_t z = 0; z < sample->zoneCount; z++) {
        if (sample->zone[z].keyHigh == top) {
            sample->zone[z].keyHigh = 127;   // the top zone goes on up, as the bottom one's range reaches 0
        }
    }

    sample->mapped = true;
}

static uint8_t * read_whole_file(const char * path, long * size) {
    FILE *    file = fopen(path, "rb");

    *size = 0;

    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long      n    = ftell(file);
    uint8_t * d    = (n > 0) ? malloc((size_t)n) : NULL;

    fseek(file, 0, SEEK_SET);

    if ((d == NULL) || (fread(d, 1, (size_t)n, file) != (size_t)n)) {
        fclose(file);
        free(d);
        return NULL;
    }
    fclose(file);
    *size = n;
    return d;
}

static void own(tNordSample * sample, float * data) {
    if (sample->ownedCount < NORD_SAMPLE_MAX_ZONES) {
        sample->owned[sample->ownedCount++] = data;
    } else {
        free(data);   // cannot happen: a sample is owned once, and there are no more samples than zones
    }
}

// A Nord .nsmp / .nsmp4; banks are read by the cache instead (notes §11)
static bool nord_sample_load(const char * path, tNordSample * sample) {
    long            size      = 0;
    uint8_t *       d         = read_whole_file(path, &size);

    memset(sample, 0, sizeof(*sample));

    if (d == NULL) {
        return false;
    }
    const tFormat * f         = ((size > 4) && (d[4] == 1u)) ? &kFormatLater : &kFormatOriginal; // notes §8

    if ((size < (long)f->chunksStart) || (memcmp(d, "CBIN", 4) != 0) || (memcmp(d + 8, "nsmp", 4) != 0)) {
        free(d);
        return false;
    }
    const uint8_t * map       = NULL;
    uint32_t        mapLength = 0;

    // notes §1 - tagged chunks: tag, type, length, data
    for (uint32_t p = f->chunksStart; (p + f->chunkHeader) <= (uint32_t)size;) {
        uint32_t length = read_u32(d + p + f->lengthAt);

        if ((p + f->chunkHeader + length) > (uint32_t)size) {
            break;
        }

        if (memcmp(d + p + f->tagAt, "map", 3) == 0) {
            map       = d + p + f->chunkHeader;
            mapLength = length;
        }

        if ((memcmp(d + p + f->tagAt, "stk", 3) == 0) && (sample->zoneCount < NORD_SAMPLE_MAX_ZONES)) {
            tNordZone * zone = &sample->zone[sample->zoneCount];

            if (decode_zone(d + p + f->chunkHeader, length, p + f->chunkHeader, zone, f)) {
                own(sample, zone->data);

                if (zone->rootNote < 0.0) {
                    zone->rootNote = zone_root_note(zone, zone->sampleRate);   // notes §3 - no header to read it from
                }
                sample->zoneCount++;
            }
        }
        p += f->chunkHeader + length;
    }

    if ((map != NULL) && f->wordPerChannel) {
        apply_key_map_later(map, mapLength, sample);
    } else if (map != NULL) {
        apply_key_map(map, mapLength, sample);
    }
    free(d);
    return sample->zoneCount > 0;
}

// notes §11 - E-mu Emulator X: a bank (.exb) of presets, each a list of voices whose zones name samples kept
// one to a file (.ebl) in the SamplePool folder beside it. IFF chunks: a big-endian tag and length, with
// LISTs and voices holding chunks of their own.
#define EMU_MAX_SAMPLES    (1000)
#define EBL_HEADER_BASE    (2)         // the sample header's offsets count from here in the E5S1 chunk
#define EBL_START_L        (0x48)      // then start R, end L, end R, loop start L and R, loop end L and R
#define EBL_RATE           (0x68)
#define EBL_OPTIONS        (0x6e)
#define EBL_OPTION_LOOP    (0x0001)
#define ZHDR_SAMPLE        (4)
#define ZHDR_ROOT          (10)
#define ETW_LOW            (4)
#define ETW_HIGH           (7)
#define PHDR_NAME          (12)        // UTF-16, from the Phdr tag

static uint32_t read_u32_le(const uint8_t * p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// The first chunk with this tag between `from` and `to`, at the top level; its body and length
static bool iff_find(const uint8_t * d, uint32_t from, uint32_t to, const char * tag, uint32_t * body, uint32_t * length) {
    for (uint32_t p = from; (p + 8u) <= to;) {
        uint32_t n = read_u32(d + p + 4);

        if ((n > (to - p - 8u))) {
            return false;
        }

        if (memcmp(d + p, tag, 4) == 0) {
            *body   = p + 8u;
            *length = n;
            return true;
        }
        p += 8u + n + (n & 1u);
    }

    return false;
}

// One .ebl: 16-bit little-endian frames, a stereo file's left channel whole and then its right
static bool ebl_load(const char * path, tNordZone * zone) {
    long            size   = 0;
    uint8_t *       d      = read_whole_file(path, &size);
    uint32_t        body   = 0;
    uint32_t        length = 0;

    if (d == NULL) {
        return false;
    }

    if (  (size < 12) || (memcmp(d, "FORM", 4) != 0) || !iff_find(d, 12, (uint32_t)size, "E5S1", &body, &length)
       || (length < (EBL_HEADER_BASE + EBL_OPTIONS + 2u))) {
        free(d);
        return false;
    }
    const uint8_t * h      = d + body + EBL_HEADER_BASE;
    uint32_t        limit  = length - EBL_HEADER_BASE;
    uint32_t        f[8];

    for (uint32_t i = 0; i < 8u; i++) {
        f[i] = read_u32_le(h + EBL_START_L + (4u * i));   // start L, start R, end L, end R, loop start L, R, loop end L, R
    }

    uint32_t        startL = f[0], startR = f[1], endL = f[2], endR = f[3];
    uint32_t        rate   = read_u32_le(h + EBL_RATE);
    uint32_t        frames = ((endL >= startL) && (endL < limit)) ? (((endL - startL) / 2u) + 1u) : 0u;
    bool            stereo = (frames > 0u) && (startR > endL) && (endR < limit) && ((endR - startR) == (endL - startL));
    uint32_t        ch     = stereo ? 2u : 1u;
    float *         out    = (frames > 0u) ? malloc(sizeof(float) * frames * ch) : NULL;

    if ((out == NULL) || (rate < 1000u) || (rate > 192000u)) {
        free(out);
        free(d);
        return false;
    }

    for (uint32_t i = 0; i < frames; i++) {
        for (uint32_t c = 0; c < ch; c++) {
            const uint8_t * q = h + ((c == 0u) ? startL : startR) + (2u * i);

            out[(i * ch) + c] = (float)(int16_t)((uint16_t)q[0] | ((uint16_t)q[1] << 8)) / 32768.0f;
        }
    }

    // notes §11 - the stored loop end is the frame before the loop's last
    uint32_t        loopS  = (f[4] >= startL) ? ((f[4] - startL) / 2u) : 0u;
    uint32_t        loopE  = (f[6] >= startL) ? (((f[6] - startL) / 2u) + 2u) : 0u;

    memset(zone, 0, sizeof(*zone));
    zone->data       = out;
    zone->channels   = ch;
    zone->length     = frames;
    zone->sampleRate = (double)rate;   // notes §11 - the rate the file plays at, its fine tuning included
    zone->loopEnd    = (loopE <= frames) ? loopE : frames;
    zone->loopStart  = loopS;
    zone->looped     = ((((uint32_t)h[EBL_OPTIONS] | ((uint32_t)h[EBL_OPTIONS + 1] << 8)) & EBL_OPTION_LOOP) != 0u)
                       && ((loopS + 1u) < zone->loopEnd);
    zone->gain       = 1.0;
    zone->velHigh    = 127;
    free(d);
    return true;
}

// notes §11 - a bank once read: its presets found and named, and its samples read on first use and shared by
// every preset that names them, so choosing another preset never reads a file twice
typedef struct {
    uint8_t *   data;
    uint32_t    size;
    uint32_t    presetCount;
    uint32_t    presetBody[NORD_SAMPLE_MAX_PRESETS];
    uint32_t    presetLength[NORD_SAMPLE_MAX_PRESETS];
    char        presetName[NORD_SAMPLE_MAX_PRESETS][NORD_SAMPLE_PRESET_NAME_SIZE];
    tNordZone * pool[EMU_MAX_SAMPLES + 1];   // sample number -> its data, NULL until read (or unreadable)
    bool        poolTried[EMU_MAX_SAMPLES + 1];
} tEmuBank;

typedef struct {
    const char *  path;                        // the bank's, for finding its SamplePool
    tEmuBank *    bank;
    tNordSample * sample;
    int32_t       voiceKey[2];
    int32_t       voiceVel[2];
    uint32_t      windows;            // ETW chunks seen since the voice or zone began
    bool          inZones;
    int32_t       zone;               // the zone the windows after a Zhdr narrow, or -1
} tEmuWalk;

// notes §11 - <bank folder>/SamplePool/<bank name>SL<nnn>.ebl
static bool emu_sample_path(const char * bank, uint32_t number, char * out, size_t size) {
    const char * slash = strrchr(bank, '/');
    const char * leaf  = (slash != NULL) ? (slash + 1) : bank;
    const char * dot   = strrchr(leaf, '.');
    int          dir   = (int)(leaf - bank);
    int          stem  = (dot != NULL) ? (int)(dot - leaf) : (int)strlen(leaf);

    return snprintf(out, size, "%.*sSamplePool/%.*sSL%03u.ebl", dir, bank, stem, leaf, number) < (int)size;
}

static void emu_zone(tEmuWalk * w, const uint8_t * z, uint32_t length) {
    tNordSample * sample = w->sample;
    tEmuBank *    bank   = w->bank;
    uint32_t      number = (length > (ZHDR_ROOT)) ? (((uint32_t)z[ZHDR_SAMPLE] << 8) | z[ZHDR_SAMPLE + 1]) : 0u;

    w->zone = -1;

    if ((number == 0u) || (number > EMU_MAX_SAMPLES) || (sample->zoneCount >= NORD_SAMPLE_MAX_ZONES)) {
        return;
    }

    if (!bank->poolTried[number]) {
        char        file[1100];
        tNordZone * read = calloc(1, sizeof(tNordZone));

        bank->poolTried[number] = true;

        if ((read != NULL) && emu_sample_path(w->path, number, file, sizeof(file)) && ebl_load(file, read)) {
            bank->pool[number] = read;
        } else {
            free(read);
        }
    }

    if (bank->pool[number] == NULL) {
        return;
    }
    tNordZone * zone = &sample->zone[sample->zoneCount++];

    *zone          = *bank->pool[number];   // the pool's data, this zone's map
    zone->id       = number;
    zone->rootNote = z[ZHDR_ROOT];
    zone->keyLow   = w->voiceKey[0];
    zone->keyHigh  = w->voiceKey[1];
    zone->velLow   = w->voiceVel[0];
    zone->velHigh  = w->voiceVel[1];
    w->zone        = (int32_t)(sample->zoneCount - 1u);
    w->windows     = 0;
}

static void emu_walk(tEmuWalk * w, const uint8_t * d, uint32_t from, uint32_t to) {
    for (uint32_t p = from; (p + 8u) <= to;) {
        uint32_t        n    = read_u32(d + p + 4);
        const uint8_t * body = d + p + 8;

        if (n > (to - p - 8u)) {
            return;
        }

        if ((memcmp(d + p, "LIST", 4) == 0) && (n >= 4u)) {
            bool zones = (memcmp(body, "E5ZL", 4) == 0);

            w->inZones = w->inZones || zones;
            emu_walk(w, d, p + 12u, p + 8u + n);
            w->inZones = w->inZones && !zones;
        } else if (memcmp(d + p, "E5V1", 4) == 0) {
            w->voiceKey[0] = 0;
            w->voiceKey[1] = 127;
            w->voiceVel[0] = 0;
            w->voiceVel[1] = 127;
            w->windows     = 0;
            w->zone        = -1;
            emu_walk(w, d, p + 8u, p + 8u + n);
        } else if (memcmp(d + p, "Zhdr", 4) == 0) {
            emu_zone(w, body, n);
        } else if ((memcmp(d + p, "ETW ", 4) == 0) && (n > ETW_HIGH)) {
            // a voice's first two windows are its keys and velocities; a zone's narrow them
            int32_t lo = body[ETW_LOW];
            int32_t hi = body[ETW_HIGH];

            if (!w->inZones && (w->windows < 2u)) {
                int32_t * range = (w->windows == 0u) ? w->voiceKey : w->voiceVel;

                range[0] = lo;
                range[1] = hi;
            } else if (w->inZones && (w->zone >= 0) && (w->windows < 2u)) {
                tNordZone * zone = &w->sample->zone[w->zone];
                int32_t *   low  = (w->windows == 0u) ? &zone->keyLow : &zone->velLow;
                int32_t *   high = (w->windows == 0u) ? &zone->keyHigh : &zone->velHigh;

                *low  = (lo > *low) ? lo : *low;
                *high = (hi < *high) ? hi : *high;
            }
            w->windows++;
        }
        p += 8u + n + (n & 1u);
    }
}

static bool bytes_contain(const uint8_t * d, uint32_t length, const char * tag) {
    for (uint32_t i = 0; (i + 4u) <= length; i++) {
        if (memcmp(d + i, tag, 4) == 0) {
            return true;
        }
    }

    return false;
}

// notes §11 - a preset's name: UTF-16 in its Phdr, kept as ASCII for the glyph atlas
static void emu_preset_name(const uint8_t * body, uint32_t length, uint32_t number, char * out, size_t size) {
    uint32_t at = 0;
    size_t   n  = 0;

    while (((at + 4u) <= length) && (memcmp(body + at, "Phdr", 4) != 0)) {
        at++;
    }

    for (uint32_t p = at + PHDR_NAME; ((at + 4u) <= length) && ((p + 1u) < length) && ((n + 1u) < size); p += 2u) {
        uint32_t c = (uint32_t)body[p] | ((uint32_t)body[p + 1] << 8);

        if (c == 0u) {
            break;
        }
        out[n++] = ((c >= 32u) && (c < 127u)) ? (char)c : '?';
    }

    while ((n > 0u) && (out[n - 1u] == ' ')) {
        n--;
    }
    out[n] = '\0';

    if (n == 0u) {
        snprintf(out, size, "Preset %u", (unsigned)(number + 1u));
    }
}

// notes §11 - the presets that hold zones; the empty ones a bank carries are passed over
static bool emu_bank_open(uint8_t * d, uint32_t size, tEmuBank * bank) {
    memset(bank, 0, sizeof(*bank));

    for (uint32_t p = 12; ((p + 8u) <= size) && (bank->presetCount < NORD_SAMPLE_MAX_PRESETS);) {
        uint32_t n = read_u32(d + p + 4);

        if (n > (size - p - 8u)) {
            break;
        }

        if ((memcmp(d + p, "E5P1", 4) == 0) && (n >= 2u) && bytes_contain(d + p + 8u, n, "Zhdr")) {
            uint32_t i = bank->presetCount++;

            bank->presetBody[i]   = p + 8u;
            bank->presetLength[i] = n;
            emu_preset_name(d + p + 8u, n, i, bank->presetName[i], sizeof(bank->presetName[i]));
        }
        p += 8u + n + (n & 1u);
    }

    bank->data = d;
    bank->size = size;
    return bank->presetCount > 0u;
}

static void emu_preset_load(const char * path, tEmuBank * bank, uint32_t preset, tNordSample * sample) {
    tEmuWalk walk;
    uint32_t body = bank->presetBody[preset];

    memset(sample, 0, sizeof(*sample));
    memset(&walk, 0, sizeof(walk));
    walk.path      = path;
    walk.bank      = bank;
    walk.sample    = sample;
    walk.zone      = -1;
    emu_walk(&walk, bank->data, body + 2u, body + bank->presetLength[preset]);
    sample->mapped = (sample->zoneCount > 0u);
}

void nord_sample_free(tNordSample * sample) {
    for (uint32_t i = 0; i < sample->ownedCount; i++) {
        free(sample->owned[i]);
    }

    memset(sample, 0, sizeof(*sample));
}

// notes §4 - every file loaded once and kept: a patch rebuild must never wait on the disk twice
#define NSMP_CACHE_SIZE    (32)

typedef enum {
    ePresetNotTried,
    ePresetLoaded,
    ePresetFailed,
} tPresetState;

typedef struct {
    char          path[1024];
    bool          loaded;       // a Nord file decoded, or a bank's presets found
    tNordSample   sample;       // a Nord file's one map
    tEmuBank *    bank;         // notes §11 - a bank's, with its presets' maps made on first use
    tNordSample * preset[NORD_SAMPLE_MAX_PRESETS];
    uint8_t       presetState[NORD_SAMPLE_MAX_PRESETS];
} tCacheEntry;

static tCacheEntry     sCache[NSMP_CACHE_SIZE];
static uint32_t        sCacheCount;
static pthread_mutex_t sCacheLock = PTHREAD_MUTEX_INITIALIZER;

static bool cache_entry_load(tCacheEntry * entry) {
    long      size     = 0;
    uint8_t * d        = NULL;
    uint8_t   head[12] = {0};
    FILE *    file     = fopen(entry->path, "rb");
    bool      bank     = (file != NULL) && (fread(head, 1, sizeof(head), file) == sizeof(head))
                         && (memcmp(head, "FORM", 4) == 0) && (memcmp(head + 8, "E5B0", 4) == 0);

    if (file != NULL) {
        fclose(file);
    }

    if (!bank) {
        return nord_sample_load(entry->path, &entry->sample);
    }
    d           = read_whole_file(entry->path, &size);
    entry->bank = calloc(1, sizeof(tEmuBank));

    if ((d != NULL) && (entry->bank != NULL) && emu_bank_open(d, (uint32_t)size, entry->bank)) {
        return true;    // the bank keeps `d`: its presets are read from it on first use
    }
    free(d);
    free(entry->bank);
    entry->bank = NULL;
    return false;
}

// Under the lock: the entry for this file, read now if it never was; NULL when the cache is full
static tCacheEntry * cache_entry(const char * path, bool load) {
    for (uint32_t i = 0; i < sCacheCount; i++) {
        if (strcmp(sCache[i].path, path) == 0) {
            return &sCache[i];
        }
    }

    if (!load || (sCacheCount >= NSMP_CACHE_SIZE)) {
        return NULL;
    }
    tCacheEntry * entry = &sCache[sCacheCount++];

    snprintf(entry->path, sizeof(entry->path), "%s", path);
    entry->loaded = cache_entry_load(entry);   // a failure is remembered too
    return entry;
}

static uint32_t clamp_preset(const tCacheEntry * entry, uint32_t preset) {
    return (preset < entry->bank->presetCount) ? preset : (entry->bank->presetCount - 1u);
}

const tNordSample * nord_sample_get(const char * path, uint32_t preset) {
    const tNordSample * found = NULL;

    if ((path == NULL) || (path[0] == '\0')) {
        return NULL;
    }
    pthread_mutex_lock(&sCacheLock);

    tCacheEntry *       entry = cache_entry(path, true);

    if ((entry != NULL) && entry->loaded && (entry->bank == NULL)) {
        found = &entry->sample;
    } else if ((entry != NULL) && entry->loaded) {
        uint32_t p = clamp_preset(entry, preset);

        if (entry->presetState[p] == ePresetNotTried) {
            entry->preset[p] = calloc(1, sizeof(tNordSample));

            if (entry->preset[p] != NULL) {
                emu_preset_load(path, entry->bank, p, entry->preset[p]);
            }

            if ((entry->preset[p] != NULL) && (entry->preset[p]->zoneCount > 0u)) {
                entry->presetState[p] = ePresetLoaded;
            } else {
                free(entry->preset[p]);
                entry->preset[p]      = NULL;
                entry->presetState[p] = ePresetFailed;
            }
        }
        found = entry->preset[p];
    }
    pthread_mutex_unlock(&sCacheLock);
    return found;
}

tNordSampleStatus nord_sample_status(const char * path, uint32_t preset) {
    tNordSampleStatus status = eNordSampleNotLoaded;

    if ((path == NULL) || (path[0] == '\0')) {
        return status;
    }
    pthread_mutex_lock(&sCacheLock);

    tCacheEntry *     entry  = cache_entry(path, false);

    if ((entry != NULL) && !entry->loaded) {
        status = eNordSampleFailed;
    } else if ((entry != NULL) && (entry->bank == NULL)) {
        status = eNordSampleLoaded;
    } else if (entry != NULL) {
        uint8_t state = entry->presetState[clamp_preset(entry, preset)];

        status = (state == ePresetLoaded) ? eNordSampleLoaded : (state == ePresetFailed) ? eNordSampleFailed : eNordSampleNotLoaded;
    }
    pthread_mutex_unlock(&sCacheLock);
    return status;
}

uint32_t nord_sample_preset_count(const char * path) {
    uint32_t      count = 0;

    if ((path == NULL) || (path[0] == '\0')) {
        return 0;
    }
    pthread_mutex_lock(&sCacheLock);

    tCacheEntry * entry = cache_entry(path, true);

    if ((entry != NULL) && entry->loaded) {
        count = (entry->bank != NULL) ? entry->bank->presetCount : 1u;
    }
    pthread_mutex_unlock(&sCacheLock);
    return count;
}

bool nord_sample_preset_name(const char * path, uint32_t preset, char * out, size_t size) {
    bool          named = false;

    if ((path == NULL) || (path[0] == '\0') || (size == 0u)) {
        return false;
    }
    pthread_mutex_lock(&sCacheLock);

    tCacheEntry * entry = cache_entry(path, false);

    if ((entry != NULL) && entry->loaded && (entry->bank != NULL)) {
        snprintf(out, size, "%s", entry->bank->presetName[clamp_preset(entry, preset)]);
        named = true;
    }
    pthread_mutex_unlock(&sCacheLock);
    return named;
}
