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

bool undo_can_undo(void);
bool undo_can_redo(void);
void undo_undo(void);
void undo_redo(void);
void undo_clear(void);

#endif /* UNDO_H */
