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

// Windows stand-in for src/midiInput.c (Docs/windows-port-plan.md, step 3). No sources yet; the
// settings are kept under the Mac's keys so the winmm version inherits them.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "prefs.h"
#include "midiInput.h"

#define PREF_KEY_SOURCE      "midiInputSourceId"
#define PREF_KEY_CHANNEL     "midiInputChannel"
#define PREF_KEY_TO_SYNTH    "midiInputSendsToSynth"

static bool             gEnabled      = true;
static _Atomic uint32_t gChannel      = MIDI_CHANNEL_OMNI;
static _Atomic bool     gSendsToSynth = true;

bool midi_input_start(void) {
    return true;
}

void midi_input_stop(void) {
}

uint32_t midi_input_source_count(void) {
    return 0;
}

const char * midi_input_source_name(uint32_t index) {
    (void)index;
    return "";
}

bool midi_input_source_is_selected(uint32_t index) {
    (void)index;
    return false;
}

void midi_input_select_source(int32_t index) {
    if (index == MIDI_INPUT_NONE) {
        gEnabled = false;
        prefs_set_int(PREF_KEY_SOURCE, 0);
    } else if (index == MIDI_INPUT_ALL) {
        gEnabled = true;
        prefs_set_int(PREF_KEY_SOURCE, -1);
    }
}

bool midi_input_all_sources_selected(void) {
    return gEnabled;
}

bool midi_input_is_enabled(void) {
    return gEnabled;
}

uint32_t midi_input_channel(void) {
    return atomic_load(&gChannel);
}

void midi_input_select_channel(uint32_t channel) {
    atomic_store(&gChannel, (channel > 16) ? MIDI_CHANNEL_OMNI : channel);
    prefs_set_int(PREF_KEY_CHANNEL, (long)atomic_load(&gChannel));
}

uint32_t midi_input_connected_count(void) {
    return 0;
}

uint32_t midi_input_pressure_count(void) {
    return 0;
}

int32_t midi_input_last_cc(void) {
    return -1;
}

bool midi_input_sends_to_synth(void) {
    return atomic_load(&gSendsToSynth);
}

void midi_input_set_sends_to_synth(bool enable) {
    atomic_store(&gSendsToSynth, enable);
    prefs_set_int(PREF_KEY_TO_SYNTH, enable ? 1 : 0);
}

void midi_input_load_settings(void) {
    gEnabled = (prefs_get_int(PREF_KEY_SOURCE, -1) != 0);
    atomic_store(&gChannel, (uint32_t)prefs_get_int(PREF_KEY_CHANNEL, MIDI_CHANNEL_OMNI));
    atomic_store(&gSendsToSynth, prefs_get_int(PREF_KEY_TO_SYNTH, 1) != 0);
}

#ifdef __cplusplus
}
#endif
