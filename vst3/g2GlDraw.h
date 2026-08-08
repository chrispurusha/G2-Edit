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

#ifndef __G2_GL_DRAW_H__
#define __G2_GL_DRAW_H__

#ifdef __cplusplus
extern "C" {
#endif

// One-off GL state and the font atlas. Must be called with the context CURRENT — building the glyph
// textures is a GL operation, and doing it without a context silently produces a font that draws
// nothing.
void g2_gl_draw_init(void);

// Draw one frame into the current context.
//
// Dimensions are PHYSICAL pixels and backingScale is how many of them make a point; the caller has
// already resolved both, because asking for them is a platform question and this file is
// deliberately not part of the platform. The renderer works in logical points, so the scale is what
// connects the two.
void g2_gl_draw_frame(int pixelWidth, int pixelHeight, double backingScale);

#ifdef __cplusplus
}
#endif

#endif // __G2_GL_DRAW_H__
