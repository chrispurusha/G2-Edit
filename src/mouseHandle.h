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
// Notes: Docs/code-notes/mouseHandle.h.md - "// notes §k" refers there.

#ifndef __MOUSE_HANDLE_H__
#define __MOUSE_HANDLE_H__

// GLFWwindow appears in the signatures below, so this header includes GLFW itself rather than
// relying on globalVars.h to have pulled it in.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>
#pragma clang diagnostic pop


#include "sysIncludes.h"
// convert_mouse_coord_to_module_area_coord() moved out to canvasCoords.c — it is arithmetic, not
// window handling. Included here so the callers that already include this header still see it.
#include "canvasCoords.h"

#ifdef __cplusplus
extern "C" {
#endif

void get_global_gui_scaled_mouse_coord(tCoord * coord);

// notes §1

// start_cursor_drag() is gone: call canvas_drag_begin() (canvasDrag.h). The application's own
// cursor_raw_coord()/cursor_capture()/cursor_release() are implemented in mouseHandle.c.
void stop_dragging(void);

// stop_dragging() preceded by the undo push for a param/mode dial drag. Anything that starts a
// drag by filling in gParamDragging must end it through here, not through stop_dragging(), or
// the drag won't be undoable.
void finish_param_drag(void);

// notes §2
void recover_lost_cursor(void);

bool is_cursor_hidden_dragging(void);
void stop_synth_name_editing(void);
bool handle_scrollbar_click(tCoord coord);
void set_x_scroll_bar(double x);
void set_y_scroll_bar(double y);
void char_event(unsigned int value);
void key_callback(int key, int scancode, int action, int mods);
void cursor_pos(tCoord coord);
// Normalised input handlers, registered with SynthLib via tSynthLibInputHandlers — the coordinate
// arrives already scaled, the button already decoded, the modifier state already updated.
void mouse_button(tCoord coord, tMouseButton mouseButton, int mods);
void scroll_event(double x, double y);
void window_focus_callback(bool focused);

#ifdef __cplusplus
}
#endif

#endif // __MOUSE_HANDLE_H__
