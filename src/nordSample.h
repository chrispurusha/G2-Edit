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

#ifndef __NORD_SAMPLE_H__
#define __NORD_SAMPLE_H__

#include <stdbool.h>
#include <stdint.h>

#define NORD_SAMPLE_MAX_ZONES    (64)

typedef struct {
    float *  data;          // -1..1, `channels` interleaved
    uint32_t channels;      // notes §9 - 1 or 2
    uint32_t length;        // frames
    double   rootNote;      // MIDI note the zone sounds at its own rate (notes §3)
    double   sampleRate;    // the zone's own (notes §3)
    bool     looped;        // notes §5 - loopStart to the zone's end repeats, a whole number of cycles
    uint32_t loopStart;
    uint32_t id;            // notes §6 - how the key map names the zone
    int32_t  keyLow;        // notes §6 - the keys it plays, from the key map
    int32_t  keyHigh;
    int32_t  velLow;        // notes §10 - the velocities it plays, 0-127 unless the map says otherwise
    int32_t  velHigh;
    double   gain;          // notes §6 - linear, the file's level times the zone's
    double   detune;        // notes §6 - semitones
} tNordZone;

typedef struct {
    bool      mapped;       // notes §6 - zones chosen by the key map, not by the nearest root
    uint32_t  zoneCount;
    tNordZone zone[NORD_SAMPLE_MAX_ZONES];
} tNordSample;

bool nord_sample_load(const char * path, tNordSample * sample);
void nord_sample_free(tNordSample * sample);
const tNordSample * nord_sample_get(const char * path);

typedef enum {
    eNordSampleNotLoaded,   // not asked for yet: the engine loads a file when it builds the patch
    eNordSampleLoaded,
    eNordSampleFailed,      // missing, unreadable, or not a Nord sample file this decoder knows
} tNordSampleStatus;

tNordSampleStatus nord_sample_status(const char * path);   // never loads

#endif
