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

#ifndef __UTILS_GRAPHICS_H__
#define __UTILS_GRAPHICS_H__

#include "sysIncludes.h"
#include "types.h"

#define NO_ZOOM    (1.0)

void set_rbg_colour(tRgb rgb);
void set_rbga_colour(tRgba rgba);

tRectangle module_area(void);
tRectangle render_line(tArea area, tCoord start, tCoord end, double thickness);
tRectangle render_rectangle(tArea area, tRectangle rectangle);
tRectangle render_rectangle_with_border(tArea area, tRectangle rectangle);
tRectangle render_triangle(tArea area, tTriangle triangle);
tRectangle render_circle_line(tArea area, tCoord coord, double radius, int segments, double thickness);
tRectangle render_circle_part(tArea area, tCoord coord, double radius, int segments, int startSeg, int numSegs);
tRectangle render_circle_part_angle(tArea area, tCoord coord, double radius, double startAngle, double endAngle, int numSteps);
tRectangle render_radial_line(tArea area, tCoord coord, double radius, double angleDegrees, double thickness);
tRectangle draw_power_button(tArea area, tRectangle rectangle, bool active);
tRectangle draw_toggle_button(tArea area, tRectangle rectangle, char * text);
tRectangle render_bezier_curve(tArea area, tCoord start, tCoord control, tCoord end, double thickness, int segments);
tRectangle render_text(tArea area, tRectangle rectangle, char * text);
bool preload_glyph_textures(const char * fontPath, double fontSize);
double get_text_width(char * text, double targetHeight);
double largest_text_width(int numItems, char ** text, double targetHeight);
void free_textures(void);
double value_to_angle(uint32_t value);
double get_scroll_bar_percent(double scrollBar, double renderSize);
double set_scroll_bar_percent(double percent, double renderSize);
uint32_t angle_to_value(double angle);
double calculate_mouse_angle(tCoord mouseCoord, tRectangle rectangle);
bool within_rectangle(tCoord coord, tRectangle rectangle);
double clamp_scroll_bar(double value, double max_value);
void set_x_scroll_percent(double percent);
void set_y_scroll_percent(double percent);
void set_zoom_factor(double zoomFactor);
void set_x_end_max(double xEndMax);
void set_y_end_max(double yEndMax);
double get_char_width(char ch, double targetHeight);
void set_render_width(int width);
void set_render_height(int height);
double get_zoom_factor(void);
int get_render_width(void);
int get_render_height(void);
double calc_scroll_x(void);
double calc_scroll_y(void);

#endif // __UTILS_GRAPHICS_H__