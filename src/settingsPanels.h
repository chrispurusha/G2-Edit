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
// Notes: Docs/code-notes/settingsPanels.c.md - "// notes §k" refers there.

#ifndef __SETTINGS_PANELS_H__
#define __SETTINGS_PANELS_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

// The four panels that used to be drawn from graphics.c, which is the application's render loop and
// is not compiled into the plug-in - so in the plug-in these opened and never appeared. Moved out
// 2026-09-16 for the same reason patchWrite.c was: nothing here needs a window.
void render_patch_settings_panel(void);
void render_patch_params_panel(void);
void render_perf_settings_panel(void);
void render_patch_notes_edit(void);

// The notes editor's cursor arithmetic, which mouseHandle.c and mousePanels.c reach for. Declared in
// graphics.h until the move.
int note_editor_cursor_move_line(int cursorPos, int delta);
int note_editor_cursor_line_home(int cursorPos);
int note_editor_cursor_line_end(int cursorPos);
int note_editor_cursor_from_click(double logicalX, double logicalY);

#ifdef __cplusplus
}
#endif

#endif // __SETTINGS_PANELS_H__
