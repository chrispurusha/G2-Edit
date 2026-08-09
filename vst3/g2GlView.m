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
#include "inputState.h"

@interface G2GlView : NSOpenGLView
@property (strong) NSTimer * dragTimer;
@end

// THE PLUG-IN'S HALF OF SynthLib's MODIFIER SEAM. The application translates GLFW's `mods`; this
// translates an NSEvent's flags, and everything downstream reads the same predicates without knowing
// which shell it is running in. Before the seam existed these were stubbed to false in
// g2AppStubs.c — Shift and Command simply did nothing in the plug-in.
//
// Cmd is COMMAND, matching the application's GLFW_MOD_SUPER, and Alt is OPTION. NSEvent reports
// several bits this UI has no use for (Caps Lock, the function key, the numeric-keypad flag), so the
// translation is a whitelist rather than a cast — a stray bit must not read as a held modifier.
static uint32_t modifier_bits_from_ns(NSEventModifierFlags flags) {
    uint32_t bits = (uint32_t)eModifierNone;

    if ((flags & NSEventModifierFlagShift) != 0) {
        bits |= (uint32_t)eModifierShift;
    }

    if ((flags & NSEventModifierFlagCommand) != 0) {
        bits |= (uint32_t)eModifierCmd;
    }

    if ((flags & NSEventModifierFlagOption) != 0) {
        bits |= (uint32_t)eModifierAlt;
    }

    if ((flags & NSEventModifierFlagControl) != 0) {
        bits |= (uint32_t)eModifierCtrl;
    }
    return bits;
}

// ── Hiding the pointer for a drag (canvasDrag.h's cursor_capture/cursor_release) ────────────────
//
// These were no-ops, on the reasoning that a host-owned view has no business confining the pointer.
// Hiding it is a different question from confining it, and hiding is what an incremental dial drag
// actually wants: the application hides the pointer for the same gesture, and a visible cursor
// wandering off the dial it is turning looks broken.
//
// [NSCursor hide] IS PROCESS-WIDE AND REFERENCE-COUNTED. Process-wide means the HOST loses its pointer
// too, so an unbalanced hide is not a cosmetic bug — it is a DAW with no cursor. Hence the flag rather
// than trusting call pairing, the poll in g2_input_drag_tick(), and the release in
// -removeFromSuperview below for an editor closed mid-drag.
//
// THE POINTER IS PUT BACK WHERE THE DRAG STARTED. Without confinement it keeps moving while hidden, so
// on release it would otherwise reappear somewhere across the screen from the dial the user was just
// turning. Warping it back is what makes this feel like the application, whose GLFW cursor mode does
// the same thing by decoupling the pointer entirely.
//
// NOT CONFINED, still: a long drag can run the physical mouse off the edge of the screen and the value
// stops following. Fixing that needs CGAssociateMouseAndMouseCursorPosition(false) and feeding the
// drag from -deltaX/-deltaY instead of absolute positions, which is a change to how motion reaches the
// canvas rather than one more line here.
static BOOL     gCursorHidden  = NO;
static CGPoint  gCursorRestore = {0.0, 0.0};

bool cursor_is_captured(void) {
    return gCursorHidden == YES;
}

void cursor_capture(void) {
    if (gCursorHidden == YES) {
        return;   // already hidden; a second hide would need a second unhide
    }
    // Cocoa reports the pointer with the origin at the BOTTOM left of the main screen, and
    // CGWarpMouseCursorPosition() wants it at the TOP left. Flip against the MAIN screen's height —
    // the one CG's global space is anchored to, which is not necessarily the screen the window is on.
    NSPoint here   = [NSEvent mouseLocation];
    NSArray * all  = [NSScreen screens];
    CGFloat  mainH = ([all count] > 0) ? NSMaxY([[all objectAtIndex:0] frame]) : 0.0;

    gCursorRestore = CGPointMake(here.x, mainH - here.y);
    gCursorHidden  = YES;
    [NSCursor hide];

    // DECOUPLED FROM THE HARDWARE, which is what makes the drag unbounded: the pointer stops moving
    // while movement still arrives as deltas, so a dial can be turned further than the screen is wide.
    // Its absolute position is frozen from here, which is why -mouseDragged: switches to
    // g2_input_drag_by() while this is in force — differencing a frozen position reports no movement.
    // The same pair of calls GLFW's disabled-cursor mode uses (ThirdParty/glfw cocoa_window.m).
    CGAssociateMouseAndMouseCursorPosition(false);
}

void cursor_release(void) {
    if (gCursorHidden == NO) {
        return;
    }
    gCursorHidden = NO;
    CGWarpMouseCursorPosition(gCursorRestore);
    // The documented companion to warping: it resynchronises the window server's idea of where the
    // pointer is with the hardware, so the jump is not followed by a delta that undoes it.
    CGAssociateMouseAndMouseCursorPosition(true);
    [NSCursor unhide];
}

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
    cursor_release();        // an editor closed mid-drag must not leave the host without a pointer
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

// The same conversion as canvasPointFor: but from a SCREEN point, for the recovery below — which has
// no event to take a location from, only wherever the pointer actually is.
- (NSPoint)canvasPointForScreenPoint:(NSPoint)screenPoint {
    NSRect  screenRect = NSMakeRect(screenPoint.x, screenPoint.y, 1.0, 1.0);
    NSPoint inWindow   = [[self window] convertRectFromScreen:screenRect].origin;
    NSPoint p          = [self convertPoint:inWindow fromView:nil];
    NSRect  bounds     = [self bounds];
    NSRect  backing    = [self convertRectToBacking:bounds];
    double  scale      = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;

    return NSMakePoint(p.x * scale, (bounds.size.height - p.y) * scale);
}

// THE MOUSE-UP THAT NEVER ARRIVED. A captured drag has the pointer hidden AND decoupled from the
// hardware, so losing the release does not just leave a dial held — it leaves the user with no cursor
// and a mouse that does not move, inside somebody else's DAW. Hosts do run their own event routing, and
// -mouseUp: is not guaranteed to reach a plug-in's view.
//
// [NSEvent pressedMouseButtons] IS THE AUTHORITY. It reports the hardware, so it is true whether or not
// we were told; every other candidate — our own drag flags, the last event we saw — is derived from the
// thing that went missing. If the button is up while we still hold the pointer, the release is
// synthesised through the ordinary path so the drag ends exactly as it would have, undo included.
- (void)recoverLostRelease {
    if (cursor_is_captured() == false) {
        return;
    }

    if ([NSEvent pressedMouseButtons] != 0) {
        return;
    }
    NSPoint c = [self canvasPointForScreenPoint:[NSEvent mouseLocation]];

    g2_input_mouse_event(c.x, c.y, eClickRelease);
    [self setNeedsDisplay:YES];
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

        // BEFORE the tick, so a lost release is dealt with in the same frame it becomes detectable
        // rather than one later.
        [self recoverLostRelease];

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

// Pushed from EVERY event that carries flags, before the event is acted on — a press must be judged
// by the modifiers held when it happened. AppKit puts the flags on all of them, mouse events
// included, so there is no polling to do and nothing to keep in step.
- (void)pushModifiersFor:(NSEvent *)event {
    set_modifier_state(modifier_bits_from_ns([event modifierFlags]));
}

// WITHOUT THIS THE VIEW RECEIVES NO KEY EVENTS AT ALL, and NSView's default is NO. An earlier comment
// here claimed it was "already YES", which was simply wrong: -keyDown: and -flagsChanged: were both
// dead code, so the plug-in had no keyboard shortcuts and could not see a modifier pressed while the
// pointer sat still.
- (BOOL)acceptsFirstResponder {
    return YES;
}

// Shift or Alt pressed with the pointer still: no mouse event, so without this the state would not
// change until the next click — which is precisely what an Alt-drag on a dial needs, since the Alt
// often goes down after the button.
- (void)flagsChanged:(NSEvent *)event {
    [self pushModifiersFor:event];
    [super flagsChanged:event];
}

// +/- steps the parameter under the pointer; Cmd +/- zooms the canvas. Both live in shared code — see
// g2_input_key(), which returns false for anything it does not want.
//
// ANYTHING UNCLAIMED GOES TO super, and that is deliberate: the host owns shortcuts of its own (the
// space bar for transport, most obviously) and a plug-in editor that swallowed every key would be a
// worse neighbour than one that swallowed none.
- (void)keyDown:(NSEvent *)event {
    NSString * chars = [event charactersIgnoringModifiers];

    [self pushModifiersFor:event];

    if ([chars length] == 1) {
        unichar character = [chars characterAtIndex:0];
        BOOL    cmdHeld   = ([event modifierFlags] & NSEventModifierFlagCommand) != 0;

        if (g2_input_key((int)character, cmdHeld == YES) == true) {
            [self setNeedsDisplay:YES];
            return;
        }
    }
    [super keyDown:event];
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    // Claim the keyboard on a click IN the view rather than when the editor opens. Taking it at open
    // time would pull the host's own key handling out from under the user for a window they may only
    // have glanced at.
    [[self window] makeFirstResponder:self];
    [self pushModifiersFor:event];
    g2_input_mouse_event(c.x, c.y, eClickPress);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event {
    [self pushModifiersFor:event];

    if (cursor_is_captured() == YES) {
        // The pointer is frozen, so there is no position to report — only movement. Deltas arrive in
        // POINTS; canvasPointFor: works in backing pixels, so they are scaled the same way to keep one
        // space. deltaY is positive DOWNWARD, matching the canvas, which is how GLFW uses it too.
        NSRect bounds  = [self bounds];
        NSRect backing = [self convertRectToBacking:bounds];
        double scale   = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;

        g2_input_drag_by([event deltaX] * scale, [event deltaY] * scale);
    } else {
        NSPoint c = [self canvasPointFor:event];

        g2_input_mouse_event(c.x, c.y, eClickDrag);
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    // NOT stopped here any more. The button coming up does not mean nothing needs ticking: a menu
    // opened by that very click stays open and its submenu dwell timer still has to run. The tick
    // itself stops the timer once nothing is left to do.
    [self pushModifiersFor:event];
    g2_input_mouse_event(c.x, c.y, eClickRelease);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

- (void)mouseMoved:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    // No dispatch — a click nobody made. But the hover state must be advanced: menu highlighting
    // and the dwell timer that opens a submenu flyout both work off the pointer.
    [self pushModifiersFor:event];
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

    [self pushModifiersFor:event];
    g2_input_scroll(c.x, c.y, [event scrollingDeltaX] * scale, [event scrollingDeltaY] * scale);
    [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent *)event {
    (void)event;    // the application has no use for right-down either; the menu opens on release
}

- (void)rightMouseUp:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    [self pushModifiersFor:event];
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
