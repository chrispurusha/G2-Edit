/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
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


void set_rbg_colour(tRgb rgb);
void set_rbga_colour(tRgba rgba);

void render_line(tCoord start, tCoord end, double thickness);
void render_rectangle(tRectangle rectangle);
void render_rectangle_with_border(tRectangle rectangle, double zoomFactor);
void render_triangle(tTriangle triangle);
void render_circle_line(tCoord coord, double radius, int segments, double thickness);
void render_circle_part(tCoord coord, double radius, int segments, int startSeg, int numSegs);
void render_circle_part_angle(tCoord coord, double radius, double startAngle, double endAngle, int numSteps);
void render_radial_line(tCoord coord, double radius, double angleDegrees, double thickness);
void draw_power_button(tRectangle rectangle, bool active);
void draw_toggle_button(tRectangle rectangle, char * text);
void render_bezier_curve(tCoord start, tCoord control, tCoord end, double thickness, int segments);
bool preload_glyph_textures(const char * fontPath, double fontSize);
void render_text(tRectangle rectangle, char * text);
double get_text_width_scaled(char *text, double target_height, double zoomFactor);
double largest_text_width(int numItems, char ** text, double target_height, double zoomFactor);
void free_textures(void);
double value_to_angle(uint32_t value);
double get_scroll_bar_percent(double scrollBar, double renderSize);
double set_scroll_bar_percent(double percent, double renderSize);
uint32_t angle_to_value(double angle);
double calculate_mouse_angle(tCoord mouseCoord, tRectangle rectangle);
bool within_rectangle(tCoord coord, tRectangle rectangle);
double clamp_scroll_bar(double value, double max_value);

#endif // __UTILS_GRAPHICS_H__
