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
// Notes: Docs/code-notes/noteStack.h.md - "// notes §k" refers there.

#ifndef __NOTE_STACK_H__
#define __NOTE_STACK_H__

#include "sysIncludes.h"

// notes §1

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
