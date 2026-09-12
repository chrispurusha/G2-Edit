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
// Notes: Docs/code-notes/noteStack.c.md - "// notes §k" refers there.

// See noteStack.h for why this is not in midiInput.c any more.

#include "sysIncludes.h"
#include "soundEngine.h"
#include "noteStack.h"

// One stack per engine, for the reason the engine's own state is banked (soundEngine.c): each plug-in
// instance plays its own notes, and the stack is what decides which note its engine is sounding.
static uint8_t  gHeldBank[SOUND_ENGINE_MAX_ENGINES][NOTE_STACK_MAX];
static uint32_t gHeldCountBank[SOUND_ENGINE_MAX_ENGINES];
#define gHeld         (gHeldBank[sound_engine_index()])
#define gHeldCount    (gHeldCountBank[sound_engine_index()])

static void held_remove(uint8_t note) {
    uint32_t i = 0;

    for (i = 0; i < gHeldCount; i++) {
        if (gHeld[i] == note) {
            uint32_t j = 0;

            for (j = i + 1; j < gHeldCount; j++) {
                gHeld[j - 1] = gHeld[j];
            }

            gHeldCount--;
            return;
        }
    }
}

void note_stack_note_on(uint8_t note) {
    held_remove(note);   // a repeat without its note off must not occupy two slots

    if (gHeldCount < NOTE_STACK_MAX) {
        gHeld[gHeldCount++] = note;
    }
    sound_engine_note((int32_t)note, true);
}

void note_stack_note_off(uint8_t note) {
    held_remove(note);

    // notes §1
    if (sound_engine_is_polyphonic() == true) {
        sound_engine_note((int32_t)note, false);
        return;
    }

    if (gHeldCount > 0) {
        // notes §2
        sound_engine_note((int32_t)gHeld[gHeldCount - 1], true);
    } else {
        sound_engine_note(-1, false);
    }
}

void note_stack_all_off(void) {
    gHeldCount = 0;
    sound_engine_note(-1, false);
}

uint32_t note_stack_count(void) {
    return gHeldCount;
}

uint8_t note_stack_at(uint32_t index) {
    return (index < gHeldCount) ? gHeld[index] : 0;
}

int32_t note_stack_top(void) {
    return (gHeldCount > 0) ? (int32_t)gHeld[gHeldCount - 1] : -1;
}
