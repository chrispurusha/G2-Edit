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

#ifndef __MIDI_INPUT_H__
#define __MIDI_INPUT_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

// MIDI note input, so a real keyboard can play the local sound engine, the G2 itself, or both at
// once — which is the point of "both": hearing the engine against the hardware on the same notes is
// how the two get compared. Like audioOutput.c this is the platform half; CoreMIDI lives here and
// nowhere else.
//
// Deliberately NOT owned by the sound engine. It runs from application startup, because sending MIDI
// on to the G2 is useful whether or not the engine is switched on.
//
// The source and channel are chosen rather than assumed. Listening to everything on every channel is
// a fine default for one keyboard on a desk, but it is wrong the moment a controller sends on a
// channel the patch is not using, or a DAW's echo port is also present. The source is remembered by
// its CoreMIDI unique ID rather than its name, since names repeat across identical interfaces.

bool midi_input_start(void);
void midi_input_stop(void);

// The sources currently on the system. Rebuilt by count(), so call that first and treat indices as
// valid only until the next call.
uint32_t midi_input_source_count(void);
const char * midi_input_source_name(uint32_t index);
bool midi_input_source_is_selected(uint32_t index);

// Pass MIDI_INPUT_NONE to take no input at all.
#define MIDI_INPUT_NONE    (-1)
void midi_input_select_source(int32_t index);
bool midi_input_is_enabled(void);

// 0 listens on every channel; 1..16 selects one.
#define MIDI_CHANNEL_OMNI    (0)
uint32_t midi_input_channel(void);
void midi_input_select_channel(uint32_t channel);

// Whether incoming notes are also played on the G2, over the same USB path the Virtual Keyboard
// uses. Independent of the sound engine, so MIDI can drive the hardware, the engine, or both.
// How many sources are feeding in right now, as opposed to how many exist to choose from.
uint32_t midi_input_connected_count(void);

// Pressure messages seen since startup, channel and polyphonic together. Answers "is this keyboard
// sending aftertouch at all" without logging on the MIDI thread, which must never do I/O.
uint32_t midi_input_pressure_count(void);

// The most recent MIDI CC number received, or -1 if none has arrived yet. Backs MIDI Learn (the L
// key), which assigns the focused parameter to whatever came in last.
int32_t midi_input_last_cc(void);

bool midi_input_sends_to_synth(void);
void midi_input_set_sends_to_synth(bool enable);

// Reads the remembered source, channel and destination. Call once at startup, after prefs_init().
void midi_input_load_settings(void);

#ifdef __cplusplus
}
#endif

#endif // __MIDI_INPUT_H__
