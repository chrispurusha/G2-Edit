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
// Notes: Docs/code-notes/graphics.h.md - "// notes §k" refers there.

#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "sysIncludes.h"
#include "geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// notes §1
void notify_full_patch_change(void);
void apply_top_bar_height(void);
void init_graphics(void);
void do_graphics_loop(void);
void clean_up_graphics(void);
void wake_glfw(void);

// One full frame, drawn and swapped. Public only for the backdoor's SCREENSHOT command, which has to
// force a frame synchronously rather than wait for the render loop to notice a redraw request — see
// backdoor.c. Nothing else should call it; ask for a redraw with synthlib_request_redraw() instead.
void render_frame(void);

// Reads a .pch2/.prf2 from disk into the patch database and redraws. Works offline, which is what
// makes it the backdoor's LOADFILE as well as the File menu's Open.
void read_file_into_memory_and_process(const char * filepath);

#ifdef ENABLE_MOUSE_CROSSHAIR
void toggle_mouse_crosshair(void); // TEMPORARY debug aid — F9, Debug builds only
#endif

// Patch DB <-> file helpers. Serialising/naming a slot touches the shared patch database, so when
// online these run on the USB thread (via eMsgCmdSavePatchFile / eMsgCmdSavePerfFile / eMsgCmdLoadFile)
// to stay atomic against the USB thread's own DB writes — see their handlers in usbComms.c.

// Asks the user whose copy wins after edits were made while the G2 was disconnected. Called from
// the reverse-queue drain on eRspOfflineConflict; writes recovery files before it asks.
void show_offline_conflict_dialog(uint32_t slotMask);
// write_database_to_file() / write_perf_to_file() moved to patchWrite.h on 2026-09-09 so the
// plug-in, which does not compile graphics.c, can save too.

// Busy state for in-flight whole-slot device ops. device_op_begin() is called when the op is enqueued
// (label e.g. "Loading…"/"Saving…"); device_op_end() when its completion response is drained.
void device_op_begin(const char * label);
void device_op_end(void);
void resize_window(int w, int h);
void reposition_window(int x, int y);

// The notes editor's cursor arithmetic moved to settingsPanels.h on 2026-09-16, with the four
// settings-family panels themselves; the floating-panel coordinator (floating_panel_is_frontmost(),
// floating_panels_under(), floating_panels_drag() and the four popup callbacks) moved to
// floatingPanels.h at the same time. Both so the plug-in, which does not compile graphics.c, gets
// them — see the note at the top of floatingPanels.c.

#ifdef __cplusplus
}
#endif

#endif // __GRAPHICS_H__
