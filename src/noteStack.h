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

// Note on/off, passed to the sound engine as played. What sounds after a release in Mono or Legato
// is the engine's decision, not the stack's - see §15 of the sound engine reference.
// velocity as sound_engine_note() takes it: 1-127 on, the release velocity off.
void note_stack_note_on(uint8_t note, uint8_t velocity);
void note_stack_note_off(uint8_t note, uint8_t velocity);

// Panic. Clears the stack and releases the engine. The caller is responsible for telling anything
// else that needs to know — walk the stack with the accessors below BEFORE calling this.
void note_stack_all_off(void);

uint32_t note_stack_count(void);
uint8_t note_stack_at(uint32_t index);

#endif // __NOTE_STACK_H__
