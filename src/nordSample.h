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
    float *  data;          // -1..1, mono
    uint32_t length;
    double   rootNote;      // MIDI note the zone sounds at its own rate (notes §3)
    double   sampleRate;    // the zone's own (notes §3)
    bool     looped;        // notes §5 - loopStart..loopEnd repeats, crossfaded over length - loopEnd
    uint32_t loopStart;
    uint32_t loopEnd;
} tNordZone;

typedef struct {
    uint32_t  zoneCount;
    tNordZone zone[NORD_SAMPLE_MAX_ZONES];
} tNordSample;

bool nord_sample_load(const char * path, tNordSample * sample);
void nord_sample_free(tNordSample * sample);
const tNordSample * nord_sample_get(const char * path);

#endif
