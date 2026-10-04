/*
 * SynthLib - common library for synthesizer editor applications.
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

// Windows stand-in for SynthLib/src/synthlibMidi.c: MIDI out, not yet - every send fails, as it does
// on the Mac when no destination is chosen.

#ifdef __cplusplus
extern "C" {
#endif

#include "synthlibMidi.h"

void synthlib_midi_set_out_port(MIDIPortRef port) {
    (void)port;
}

bool synthlib_midi_send_to(const uint8_t * data, uint32_t length, MIDIEndpointRef dest) {
    (void)data;
    (void)length;
    (void)dest;
    return false;
}

#ifdef __cplusplus
}
#endif
