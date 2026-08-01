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

#ifndef UNDO_H
#define UNDO_H

#include <stdbool.h>
#include <stdint.h>
#include "types.h"

typedef struct {
    tModuleKey key;
    uint32_t   oldColumn;
    uint32_t   oldRow;
    uint32_t   newColumn;
    uint32_t   newRow;
} tUndoMoveEntry;

// Snapshot gSelection + all touching cables, push as a delete command.
void undo_push_delete_selection(void);

// Record module position changes (called after drop).
void undo_push_move(uint32_t slot, uint32_t location, tUndoMoveEntry * entries, uint32_t count);

// Record a paste operation so it can be undone and redone.
void undo_push_paste(uint32_t slot, uint32_t location, uint32_t anchorCol, uint32_t anchorRow, tModuleKey * pastedKeys, uint32_t pastedCount, tClipboardModule * clipModules, uint32_t clipModuleCount, tClipboardCable * clipCables, uint32_t clipCableCount);

// Cable-chain edits are recorded as a before/after snapshot of every cable in one location
// rather than as a list of individual changes: a single command (Disconnect especially) can
// delete, re-create AND recolour cables at once, and the set it touches is only known by
// walking the chain. Snapshotting the location sidesteps all of that, and a patch's cable
// count is small enough that it costs nothing worth counting.
//
// Bracket the edit with begin/commit. Commit pushes nothing if the cables came out unchanged,
// so a command that turns out to be a no-op leaves no undo entry behind. Nesting is not
// supported — a begin while one is already open is ignored, which keeps the outermost
// bracket authoritative when one cable operation is built from others.
void undo_begin_cable_edit(uint32_t slot, uint32_t location);
void undo_commit_cable_edit(void);

// Record an Add Module, AFTER the module exists — the snapshot is what redo puts back.
void undo_push_create_module(tModuleKey key);

// Record a module colour change (old → new).
void undo_push_module_colour(tModuleKey key, uint32_t oldColour, uint32_t newColour);

// MIDI CC assignment changes, bracketed like cable edits: assigning a CC another parameter owns
// is two changes at once (steal, then assign), so the slot's whole controller table is snapshotted
// rather than the individual entries. Commit pushes nothing if the table came out unchanged.
void undo_begin_midi_cc_edit(uint32_t slot);
void undo_commit_midi_cc_edit(void);

// Global knob assignments, bracketed like the MIDI CC pair and for the same reason. Performance-
// wide, so no slot argument.
void undo_begin_global_knob_edit(void);
void undo_commit_global_knob_edit(void);

// Record a single param value change (old → new). variation ignored for modes.
void undo_push_param_change(tModuleKey key, uint32_t paramIndex, uint32_t variation, uint32_t oldValue, uint32_t newValue);
void undo_push_mode_change(tModuleKey key, uint32_t modeIndex, uint32_t oldValue, uint32_t newValue);

// Record knob assignment change. idx2 == -1 if only one knob changed.
void undo_push_knob(uint32_t slot, uint32_t idx1, const tKnob * before1, const tKnob * after1, int32_t idx2, const tKnob * before2, const tKnob * after2);

// Record a module name change (old → new).
void undo_push_module_name(tModuleKey key, const char * oldName, const char * newName);

// Record a Patch Mutator "Exclude From Mutation" toggle (old → new). Applying (undo/redo) also
// sends the change live via send_mutation_lock_value() - see its comment in protocol.h/defs.h.
void undo_push_module_exclude(tModuleKey key, uint8_t oldValue, uint8_t newValue);

// Record a param label change (old → new). oldSet/newSet indicate whether
// a custom label existed before/after.
void undo_push_param_name(tModuleKey key, uint32_t paramIndex, const char * oldName, bool oldSet, const char * newName, bool newSet);

// Patch-descriptor field identifiers for undo_push_patch_descr.
#define UNDO_PATCH_DESCR_VOICE_COUNT    0u
#define UNDO_PATCH_DESCR_MONO_POLY      1u
#define UNDO_PATCH_DESCR_CATEGORY       2u

// Record a change to one field of gPatchDescr[slot]. Use the constants above.
void undo_push_patch_descr(uint32_t slot, uint8_t which, uint8_t oldValue, uint8_t newValue);

// Record a patch name change (old → new).
void undo_push_patch_name(uint32_t slot, const char * oldName, const char * newName);

// Record a perf name change (old → new).
void undo_push_perf_name(const char * oldName, const char * newName);

// Perf-setting field identifiers for undo_push_perf_setting.
#define UNDO_PERF_SLOT_ENABLED      0u
#define UNDO_PERF_SLOT_KEYBOARD     1u
#define UNDO_PERF_SLOT_HOLD         2u
#define UNDO_PERF_KEYBOARD_RANGE    3u

// Record a change to one perf-settings boolean. slot is the slot index for
// per-slot fields; ignored for UNDO_PERF_KEYBOARD_RANGE.
void undo_push_perf_setting(uint8_t which, int32_t slot, uint8_t oldValue, uint8_t newValue);

bool undo_can_undo(void);
bool undo_can_redo(void);
void undo_undo(void);
void undo_redo(void);
void undo_clear(void);

#endif /* UNDO_H */
