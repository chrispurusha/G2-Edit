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
@property (strong) NSTimer * dragTimer;
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

// WITHOUT A TRACKING AREA, -mouseMoved: IS NEVER CALLED. That is why menu items did not highlight:
// the highlight is drawn from the pointer position (render_context_menu reads it), and the position
// was only ever updated on a click. AppKit delivers mouse-moved events to a view solely on the
// strength of a tracking area covering it, and the area has to be rebuilt whenever the view resizes.
- (void)removeFromSuperview {
    [self stopDragTimer];    // the timer retains self through its block; leaving it running leaks the view
    [super removeFromSuperview];
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];

    for (NSTrackingArea * area in [self trackingAreas]) {
        [self removeTrackingArea:area];
    }

    NSTrackingArea * area = [[NSTrackingArea alloc]
                             initWithRect:[self bounds]
                                  options:(NSTrackingMouseMoved | NSTrackingActiveInKeyWindow |
                                           NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited)
                                    owner:self
                                 userInfo:nil];

    [self addTrackingArea:area];
}

// ── Mouse ───────────────────────────────────────────────────────────────────────────────────────
//
// Cocoa's origin is bottom-left and the canvas's is top-left, so y is flipped here — at the
// boundary, in the only file that knows Cocoa's convention. Everything past this point is in the
// canvas's own coordinates and the application's existing hit-testing takes over (g2Input.c).
//
// The LEFT button goes through the click regions, which do not distinguish buttons; the RIGHT button
// has its own path, because the application's context menus hit-test connectors, parameters and
// module bodies in a specific order of their own.

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    (void)event;
    return YES;    // act on the click that focuses the window too, rather than swallowing it
}

// PHYSICAL PIXELS, y-flipped. Not points: the canvas works in its own logical units, and only
// g2Input.c knows the scale that converts between them. Handing over pixels keeps this file's job to
// the two things it is actually authoritative about — Cocoa's bottom-left origin, and the backing
// scale of the surface it owns.
- (NSPoint)canvasPointFor:(NSEvent *)event {
    NSPoint p      = [self convertPoint:[event locationInWindow] fromView:nil];
    NSRect  bounds = [self bounds];
    NSRect  backing = [self convertRectToBacking:bounds];
    double  scale  = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;

    return NSMakePoint(p.x * scale, (bounds.size.height - p.y) * scale);
}

// Started when there is something to advance and stopped by the tick itself once there is not:
// 60 Hz of doing nothing is a poor thing to leave running inside somebody else's host.
//
// CALLED AFTER EVERY MOUSE EVENT, not just on press. A right-click context menu opens from
// -rightMouseUp:, and a menu opened once the timer had already stopped itself would otherwise have
// nothing driving its dwell timer at all — which is why submenus still needed a jiggle after the
// first fix.
- (void)ensureTickTimer {
    if (self.dragTimer == nil) {
        [self startDragTimer];
    }
}

- (void)startDragTimer {
    [self stopDragTimer];
    self.dragTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                                     repeats:YES
                                                       block:^(NSTimer * t) {
        (void)t;

        if (g2_input_drag_tick() == true) {
            [self setNeedsDisplay:YES];
        } else {
            [self stopDragTimer];    // nothing left to advance
        }
    }];
}

- (void)stopDragTimer {
    [self.dragTimer invalidate];
    self.dragTimer = nil;
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    g2_input_mouse_event(c.x, c.y, eClickPress);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    g2_input_mouse_event(c.x, c.y, eClickDrag);
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    // NOT stopped here any more. The button coming up does not mean nothing needs ticking: a menu
    // opened by that very click stays open and its submenu dwell timer still has to run. The tick
    // itself stops the timer once nothing is left to do.
    g2_input_mouse_event(c.x, c.y, eClickRelease);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    // No dispatch — a click nobody made. But the hover state must be advanced: menu highlighting
    // and the dwell timer that opens a submenu flyout both work off the pointer.
    g2_input_hover(c.x, c.y);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

// The tracking area asked for enter/exit alongside movement, and this is the half that was missing:
// without it the last hovered menu item or connector stayed highlighted after the pointer had left
// the plug-in window entirely, since nothing else ever moves the recorded position back off it.
//
// A drag that leaves the view is not a departure — AppKit keeps delivering its movement, and the
// canvas auto-scrolls precisely because the pointer is outside. So an exit with a button still down
// is ignored, and the release that ends the drag puts the position where the pointer really is.
- (void)mouseExited:(NSEvent *)event {
    (void)event;

    if ([NSEvent pressedMouseButtons] != 0) {
        return;
    }
    g2_input_pointer_left();
    [self setNeedsDisplay:YES];
}

// The application opens its context menus on right button UP, not down, so the same here.
// Trackpad and wheel both arrive here. Deltas are in points and the canvas scrolls in pixels, so
// they are handed over as-is and g2Input.c applies the scale — the same division every other
// coordinate goes through.
- (void)scrollWheel:(NSEvent *)event {
    NSPoint c      = [self canvasPointFor:event];
    NSRect  bounds = [self bounds];
    NSRect  backing = [self convertRectToBacking:bounds];
    double  scale  = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;

    g2_input_scroll(c.x, c.y, [event scrollingDeltaX] * scale, [event scrollingDeltaY] * scale);
    [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent *)event {
    (void)event;    // the application has no use for right-down either; the menu opens on release
}

- (void)rightMouseUp:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    g2_input_right_click(c.x, c.y);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
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
