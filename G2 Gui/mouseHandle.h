/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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

#include "sysIncludes.h"

bool handle_context_menu_click(tCoord coord);
bool handle_scrollbar_click(tCoord coord);
void set_x_scroll_bar(double x);
void set_y_scroll_bar(double y);
void char_event(GLFWwindow * window, unsigned int value);
void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods);
void cursor_pos(GLFWwindow * window, double x, double y);
void mouse_button(GLFWwindow * window, int button, int action, int mods);
void scroll_event(GLFWwindow * window, double x, double y);
void init_params_on_new_module(tModule * module);

#endif // __MOUSE_HANDLE_H__
