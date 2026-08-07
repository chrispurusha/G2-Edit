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

// The plug-in's stand-in for the two platform I/O layers.
//
// In the application the sound engine opens its own CoreAudio device (audioOutput.c) and its own
// CoreMIDI ports (midiInput.c). In a plug-in the host owns both: it hands us a buffer to fill and
// an event list to read, so neither layer exists. These are the few entry points the engine still
// references, given null implementations so the link resolves.
//
// This IS the "a wrapper replaces audioOutput.c and nothing else" plan, arrived at literally.
//
// sound_engine_start()/stop() are the only callers of the audio ones, and the plug-in calls
// sound_engine_start_hosted()/stop_hosted() instead - so these are compiled in but never reached.
// They return failure rather than success on purpose: if a future change ever routes the plug-in
// through sound_engine_start(), it will fail loudly and visibly rather than appear to open a device
// that is not there.

#include "sysIncludes.h"

bool audio_output_start(void) {
    return false;
}

void audio_output_stop(void) {
}

// The engine asks these when deciding whether anything is playing it. With the host as the only
// source of notes, there is exactly one notional connection and no channel pressure.
uint32_t midi_input_connected_count(void) {
    return 1;
}

uint32_t midi_input_pressure_count(void) {
    return 0;
}
