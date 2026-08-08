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

#ifndef __NOTE_STACK_H__
#define __NOTE_STACK_H__

#include "sysIncludes.h"

// LAST-NOTE PRIORITY for a monophonic engine — what makes legato work.
//
// Hold D, play F, release F, and the note should fall back to the D still under your finger rather
// than stopping. That needs a record of what is held, which is what this is. Without it, releasing
// any note simply silences the engine and legato playing falls apart.
//
// Split out of midiInput.c so the VST3 plug-in gets the same behaviour from the same code. It could
// not be reused where it was: midiInput.c interleaves the stack with send_note_to_synth(), which
// talks to the G2 over USB — something a plug-in must never do. Only the STACK is shared; who else
// hears about a note stays with the caller.
//
// The G2 itself is told about every note as played, not the stack's view: the hardware does its own
// voice allocation and wants them all. The stack exists purely for the local monophonic engine.
//
// NOT THREAD SAFE, and does not need to be: each host drives it from one thread — the MIDI thread in
// the application, the audio thread in the plug-in.

#define NOTE_STACK_MAX    (16)

// Note on/off, driving the sound engine as a side effect. note_stack_note_off() is where legato
// happens: it falls back to the newest note still held, or releases if there is none.
void note_stack_note_on(uint8_t note);
void note_stack_note_off(uint8_t note);

// Panic. Clears the stack and releases the engine. The caller is responsible for telling anything
// else that needs to know — walk the stack with the accessors below BEFORE calling this.
void note_stack_all_off(void);

uint32_t note_stack_count(void);
uint8_t note_stack_at(uint32_t index);

// The note currently sounding, or -1 if none. Used to decide whether a polyphonic key pressure
// message applies to the note being played rather than one still held underneath it.
int32_t note_stack_top(void);

#endif // __NOTE_STACK_H__
