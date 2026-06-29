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

bool is_selected(tModuleKey key);
void selection_clear(void);
void selection_set_single(tModuleKey key);
void selection_toggle(tModuleKey key);
void selection_add_rect(tRectangle rect, uint32_t slot, uint32_t location);
void delete_module_and_cables(tModuleKey key);
void delete_selection(void);
void shift_selection_down(void);
void copy_selection(void);
void cut_selection(void);
void paste_snapshot(uint32_t slot, uint32_t location, uint32_t anchorCol, uint32_t anchorRow, tClipboardModule * modules, uint32_t moduleCount, tClipboardCable * cables, uint32_t cableCount);
void paste_clipboard(void);

#endif /* SELECTION_H */
