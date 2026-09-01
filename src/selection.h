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

#ifndef SELECTION_H
#define SELECTION_H

#include "types.h"
#include "undo.h"   // tUndoMoveEntry, for the position snapshot pair below

bool is_selected(tModuleKey key);
void selection_clear(void);
void selection_validate(void);
void selection_select_all(void);
void selection_set_single(tModuleKey key);
void selection_toggle(tModuleKey key);
void selection_add_rect(tRectangle rect, uint32_t slot, uint32_t location);
void delete_module_and_cables(tModuleKey key);
void delete_selection(void);
// Pushes anything a moved module now overlaps further down its column. Moved from menus.c.
// Return false when the column is full to the bottom and the gesture must be put back - see the
// comment on shift_fit_row() in selection.c.
bool shift_modules_down(tModuleKey key);
bool shift_selection_down(void);
// As shift_selection_down(), but for a location that is not necessarily the one on screen — a paste
// being redone can name a slot/location the user has since navigated away from.
bool shift_selection_down_in(uint32_t slot, uint32_t location);
// Before/after halves of the module-position record the shifts above make necessary for undo.
uint32_t module_positions_snapshot(uint32_t slot, uint32_t location, tUndoMoveEntry * out);
uint32_t module_positions_changed(tUndoMoveEntry * entries, uint32_t count);
void copy_selection(void);
void cut_selection(void);
void paste_snapshot(uint32_t slot, uint32_t location, uint32_t anchorCol, uint32_t anchorRow, tClipboardModule * modules, uint32_t moduleCount, tClipboardCable * cables, uint32_t cableCount);
void paste_clipboard(void);

#endif /* SELECTION_H */
