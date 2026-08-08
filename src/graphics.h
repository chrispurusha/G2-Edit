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

#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "sysIncludes.h"
#include "geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_graphics(void);
void do_graphics_loop(void);
void clean_up_graphics(void);
void wake_glfw(void);

#ifdef ENABLE_MOUSE_CROSSHAIR
void toggle_mouse_crosshair(void); // TEMPORARY debug aid — F9, Debug builds only
#endif

// Patch DB <-> file helpers. Serialising/naming a slot touches the shared patch database, so when
// online these run on the USB thread (via eMsgCmdSavePatchFile / eMsgCmdSavePerfFile / eMsgCmdLoadFile)
// to stay atomic against the USB thread's own DB writes — see their handlers in usbComms.c.
int write_database_to_file(const char * filepath, uint32_t slot);  // EXIT_SUCCESS / EXIT_FAILURE

// Asks the user whose copy wins after edits were made while the G2 was disconnected. Called from
// the reverse-queue drain on eRspOfflineConflict; writes recovery files before it asks.
void show_offline_conflict_dialog(uint32_t slotMask);
int write_perf_to_file(const char * filepath);                     // EXIT_SUCCESS / EXIT_FAILURE

// Busy state for in-flight whole-slot device ops. device_op_begin() is called when the op is enqueued
// (label e.g. "Loading…"/"Saving…"); device_op_end() when its completion response is drained.
void device_op_begin(const char * label);
void device_op_end(void);
void resize_window(int w, int h);
void reposition_window(int x, int y);

int note_editor_cursor_move_line(int cursorPos, int delta);
int note_editor_cursor_line_home(int cursorPos);
int note_editor_cursor_line_end(int cursorPos);
int note_editor_cursor_from_click(double logicalX, double logicalY);

// Shared popup-panel chrome (bordered box, inset title bar, Close button) — see graphics.cpp.

#ifdef __cplusplus
}
#endif

#endif // __GRAPHICS_H__
