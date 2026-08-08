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

void init_patch(uint32_t slot);
void get_global_gui_scaled_mouse_coord(tCoord * coord);
// Is a multi-select modifier — either Shift or either Command — held right now?
//
// One function rather than the four-way glfwGetKey() expression written out at each site, which it
// was in four places. It is also the ONLY reason moduleGraphics.c reached for GLFW: extracting it
// leaves that file able to link into a build with no GLFW in it at all, which is what lets the
// canvas renderer be reused by the VST3 plug-in. See vst3/plugin-gui-notes.md.
//
// The implementation is therefore part of the PLATFORM, not of the drawing: the application answers
// it from GLFW (below), and the plug-in answers it from whatever its host gives it — currently
// nothing, so false.
bool multi_select_modifier_held(void);

void start_cursor_drag(void);
void stop_dragging(void);

// stop_dragging() preceded by the undo push for a param/mode dial drag. Anything that starts a
// drag by filling in gParamDragging must end it through here, not through stop_dragging(), or
// the drag won't be undoable.
void finish_param_drag(void);

// True while any drag that hides the cursor (CURSOR_DISABLED) is active —
// param/tempo/perf-tempo/vibrato-rate/vibrato-amount/glide-time dragging.
// During these, the reported cursor position is a virtual/relative-delta
// accumulator, not a real on-screen point — it can drift over an unrelated
// control, so anything that hover-highlights "what's under the mouse"
// (e.g. render_knob_assignment_overlay()'s per-param hover check) needs to
// suppress itself while this is true, or it'll highlight the wrong control.
bool is_cursor_hidden_dragging(void);
void stop_synth_name_editing(void);
bool handle_scrollbar_click(tCoord coord);
void set_x_scroll_bar(double x);
void set_y_scroll_bar(double y);
void char_event(GLFWwindow * window, unsigned int value);
void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods);
void cursor_pos(GLFWwindow * window, double x, double y);
void mouse_button(GLFWwindow * window, int button, int action, int mods);
void scroll_event(GLFWwindow * window, double x, double y);

#ifdef __cplusplus
}
#endif

#endif // __MOUSE_HANDLE_H__
