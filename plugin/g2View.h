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
// Notes: Docs/code-notes/g2View.h.md - "// notes §k" refers there.

#ifndef __G2_GL_VIEW_H__
#define __G2_GL_VIEW_H__

#include <stdint.h>
#include <stdbool.h>

// notes §1

// extern "C" because one caller is g2Editor.mm, which is Objective-C++ — the implementation is plain
// Objective-C, so without this the C++ side asks the linker for a mangled name that the C side never
// emitted.
#ifdef __cplusplus
extern "C" {
#endif

// notes §2
void g2_view_request_redraw(void);

// True while cursor_capture() has the pointer hidden. Polled by the drag tick so a release that never
// arrives cannot leave the host without a pointer — see cursor_capture() in g2View.m.
bool cursor_is_captured(void);

// notes §3
void * g2_view_create(void * doc, double width, double height);

// Counterpart to g2_view_create(). Does NOT release the view: the wrapper owns that reference and
// hands it to ARC. This is for whatever the editor hung off it — timers, in this case, which would
// otherwise keep firing at a view the host has taken out of its window.
void g2_view_destroy(void * view);

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
NSView * g2_create_gl_view(NSRect frame, void * doc);

#ifdef __cplusplus
}
#endif

#endif // __OBJC__

#endif // __G2_GL_VIEW_H__
