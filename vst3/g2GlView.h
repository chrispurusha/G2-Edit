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

#ifndef __G2_GL_VIEW_H__
#define __G2_GL_VIEW_H__

// An OpenGL surface that lives inside a window somebody else owns.
//
// This is the experiment described in vst3/plugin-gui-notes.md: the application's renderer draws
// through GLFW, which insists on creating its own window, and a plug-in is handed an NSView by the
// host instead. The question this answers is the only one that decides whether the editor canvas
// can ever appear in a plug-in — will an OpenGL context attached to a host-provided NSView draw at
// all, inside a real host, alongside that host's own rendering.
//
// It deliberately does NOT use the application's renderer yet. See the notes file for why: the
// drawing code is reachable (all of it funnels through SynthLib's utilsGraphics.c) but pulling it in
// drags GLFW along through synthlibScale.c, and that untangling is only worth doing once the surface
// itself is known to work.

// extern "C" because one caller is g2Editor.mm, which is Objective-C++ — the implementation is plain
// Objective-C, so without this the C++ side asks the linker for a mangled name that the C side never
// emitted.
#ifdef __cplusplus
extern "C" {
#endif

// Mark the surface as needing to be redrawn. Safe from ANY thread: the view must be touched on the
// main thread, and this hops there itself rather than making every caller remember to.
//
// Declared outside the Objective-C section deliberately. This is what plain C reaches for — it is
// how synthlib_request_redraw() is answered in the plug-in (g2AppStubs.c), which is the whole
// mechanism by which a change anywhere in the editor causes a repaint. The application posts an
// empty event to wake a blocked GLFW loop; here, AppKit schedules the frame.
void g2_gl_view_request_redraw(void);

#ifdef __cplusplus
}
#endif

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>

#ifdef __cplusplus
extern "C" {
#endif

// Creates the view, retained by the caller's autorelease pool as usual for ARC. Add it as a subview
// of whatever the host handed over; it needs no further setup.
NSView * g2_create_gl_view(NSRect frame);

#ifdef __cplusplus
}
#endif

#endif // __OBJC__

#endif // __G2_GL_VIEW_H__
