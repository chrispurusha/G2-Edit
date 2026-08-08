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

// The OpenGL surface, and nothing else.
//
// Plain Objective-C, not Objective-C++: an NSOpenGLView subclass genuinely needs the runtime, but
// nothing here needs C++, and the drawing itself needs neither (g2GlDraw.c). The three languages in
// this folder each earn their place — g2Editor.mm is Objective-C++ only because IPlugView is a C++
// interface that has to hand a Cocoa view to the host.
//
// NSOpenGLView rather than a bare NSView with a CAOpenGLLayer, and not a layer-backed view either.
// That follows JUCE, which has shipped exactly this arrangement across a very large number of hosts
// for years; the layer-backed routes are what people reach for when they hit trouble, and starting
// at the trouble is a poor way to find out whether there is any.

#define GL_SILENCE_DEPRECATION    1

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

#include "g2GlDraw.h"
#include "g2GlView.h"
#include "g2Input.h"

@interface G2GlView : NSOpenGLView
@end

// The view currently attached, for g2_gl_view_request_redraw() to find. A host can open more than
// one instance of the plug-in, so this is "the most recently attached" rather than "the" view —
// enough while the editor is a single window per instance, and the thing to revisit when a redraw
// needs to reach a specific instance. Weak so a closed editor leaves nil here rather than a dangling
// pointer.
static __weak G2GlView * gCurrentView = nil;

@implementation G2GlView

- (instancetype)initWithFrame:(NSRect)frame {
    // No profile attribute, so this is the legacy (compatibility) profile. That is deliberate and
    // not laziness: the application's renderer is fixed-function throughout — glOrtho, glBegin,
    // glVertex — so a core-profile context here would draw nothing when the real renderer is
    // eventually pointed at it, and would do so silently.
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize,  8,
        0
    };

    NSOpenGLPixelFormat * format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

    if (format == nil) {
        return nil;
    }
    self = [super initWithFrame:frame pixelFormat:format];

    if (self == nil) {
        return nil;
    }

    // Retina. Without this the surface is allocated at point resolution and scaled up, which looks
    // soft rather than broken — the kind of fault that survives a long time because nobody can say
    // exactly what is wrong with it.
    [self setWantsBestResolutionOpenGLSurface:YES];
    return self;
}

- (void)prepareOpenGL {
    [super prepareOpenGL];

    NSOpenGLContext * context = [self openGLContext];
    GLint             swap    = 1;

    [context makeCurrentContext];
    [context setValues:&swap forParameter:NSOpenGLContextParameterSwapInterval];
    g2_gl_draw_init();
}

// NO NSViewGlobalFrameDidChangeNotification OBSERVER, though JUCE has one and copying it was the
// first thing tried here. The compiler rejects it in as many words: that notification is deprecated
// with "Use NSOpenGLView instead", because NSOpenGLView already watches it and calls -update for
// you. JUCE needs it because JUCE attaches a context to a bare NSView; subclassing NSOpenGLView buys
// exactly this, and adding the observer back would only double up the -update calls.
//
// If a host ever does resize the view without the surface following, THAT is the thing to reach for
// — a bare NSView plus a hand-managed NSOpenGLContext and this notification. It is a bigger change
// than it looks, so it is worth being sure the simple arrangement has actually failed first.
- (void)reshape {
    [super reshape];
    [[self openGLContext] update];
}

// ── Mouse ───────────────────────────────────────────────────────────────────────────────────────
//
// Cocoa's origin is bottom-left and the canvas's is top-left, so y is flipped here — at the
// boundary, in the only file that knows Cocoa's convention. Everything past this point is in the
// canvas's own coordinates and the application's existing hit-testing takes over (g2Input.c).
//
// Nothing is done about which BUTTON was pressed: the canvas's click regions do not distinguish
// them, and the right-button context menus are not available in the plug-in yet.

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    (void)event;
    return YES;    // act on the click that focuses the window too, rather than swallowing it
}

- (NSPoint)canvasPointFor:(NSEvent *)event {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    return NSMakePoint(p.x, [self bounds].size.height - p.y);
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    g2_input_mouse_event(c.x, c.y, eClickPress);
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    g2_input_mouse_event(c.x, c.y, eClickDrag);
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    g2_input_mouse_event(c.x, c.y, eClickRelease);
    [self setNeedsDisplay:YES];
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    // Position only, no dispatch: hover highlighting reads the mouse position, and a dispatch here
    // would deliver a click nobody made.
    g2_input_set_mouse(c.x, c.y);
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;

    NSOpenGLContext * context = [self openGLContext];
    CGLContextObj     cgl     = [context CGLContextObj];

    // The lock is not needed while only this method touches the context, but it is what makes it
    // safe for the USB thread to ever drive a redraw — which is how the real editor works, and the
    // reason the application has a wake callback at all. Establishing the discipline now costs one
    // pair of calls; retrofitting it after a threading bug costs considerably more.
    CGLLockContext(cgl);
    [context makeCurrentContext];

    // Physical pixels, not points. Asking the view to convert is the whole of the backing-scale
    // question on macOS — there is no host-supplied scale factor to plumb through, because VST3's
    // setContentScaleFactor() is not used on this platform.
    NSRect bounds  = [self bounds];
    NSRect backing = [self convertRectToBacking:bounds];
    double scale   = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;

    g2_gl_draw_frame((int)backing.size.width, (int)backing.size.height, scale);

    [context flushBuffer];
    CGLUnlockContext(cgl);
}

@end

// Called from plain C — and, in the application's design, potentially from the USB thread, which is
// why the main-thread hop is here rather than being every caller's problem. dispatch_async and not
// _sync: a synchronous hop from a thread the main thread is waiting on is a deadlock, and redrawing
// a frame later is never worth that risk.
void g2_gl_view_request_redraw(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gCurrentView setNeedsDisplay:YES];    // nil-safe; a closed editor simply does nothing
    });
}

NSView * g2_create_gl_view(NSRect frame) {
    G2GlView * view = [[G2GlView alloc] initWithFrame:frame];

    gCurrentView = view;
    return view;
}
