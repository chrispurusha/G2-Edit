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

// MIDI note input for the local sound engine, so a real keyboard can play it instead of the on
// screen one. Like audioOutput.c this is the platform half — CoreMIDI lives here and nowhere else.
//
// It listens to EVERY MIDI source on the system, on every channel, and takes note on/note off (plus
// All Notes Off). There is no device or channel selection: with one monophonic voice there is
// nothing to route, and "whatever is plugged in plays it" is the useful behaviour. Sources that
// appear after startup are picked up automatically, so plugging a keyboard in while the engine runs
// works without a restart.
//
// Started and stopped by the sound engine — there is nothing for MIDI to play when it is off, so
// nothing calls these directly.

bool midi_input_start(void);
void midi_input_stop(void);

// How many sources are currently connected. Informational, for the log and any future UI.
uint32_t midi_input_source_count(void);

#ifdef __cplusplus
}
#endif

#endif // __MIDI_INPUT_H__
