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
// Notes: Docs/code-notes/g2View.m.md - "// notes §k" refers there.

// notes §1

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>    // CACurrentMediaTime(), for the frame statistics

#include "renderBackendSelect.h"

#include "renderBackend.h"


#include "defs.h"        // WHEEL_SCROLL_STEP — one wheel notch, shared with the application
#include "g2Draw.h"
#include "g2View.h"
#include "g2Input.h"
#include "inputState.h"
#include "soundEngine.h"    // sound_engine_meters_dirty() - see -startMeterTimer

@interface G2View : NSView
@property (strong) NSTimer * dragTimer;
@property (strong) NSTimer * meterTimer;
@property (assign) void *    doc;          // the instance's tG2Document; owned by g2Plugin.c
@end

// notes §2
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

// notes §3
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

    // notes §4
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

// notes §5
static NSHashTable *    gViews         = nil;

// Which editor drew last. The click-region registry (SynthLib's clickRegion.c) is still one per
// process and is refilled by every frame, so it holds whichever editor drew LAST; an event in any
// other editor would be hit-tested against that one's controls. See -enterForInput.
static __weak G2View *  gLastDrawnView = nil;

// notes §6
static bool   gFrameStats       = false;
static double gFrameWindowStart = 0.0;
static double gFrameLast        = 0.0;
static double gFrameGapMax      = 0.0;
static double gFrameDrawSum     = 0.0;
static double gFrameDrawMax     = 0.0;
static double gFramePresentSum  = 0.0;
static double gFramePresentMax  = 0.0;
static int    gFrameCount       = 0;

static void frame_stats_record(double start, double drawn, double presented) {
    double drawMs    = (drawn - start) * 1000.0;
    double presentMs = (presented - drawn) * 1000.0;

    if (gFrameWindowStart == 0.0) {
        gFrameWindowStart = start;
    } else if (((start - gFrameLast) * 1000.0) > gFrameGapMax) {
        gFrameGapMax = (start - gFrameLast) * 1000.0;
    }
    gFrameLast        = start;
    gFrameCount++;
    gFrameDrawSum    += drawMs;
    gFramePresentSum += presentMs;
    gFrameDrawMax     = (drawMs > gFrameDrawMax) ? drawMs : gFrameDrawMax;
    gFramePresentMax  = (presentMs > gFramePresentMax) ? presentMs : gFramePresentMax;

    if ((presented - gFrameWindowStart) >= 1.0) {
        fprintf(stderr, "G2 Alike frames: %d in %.2f s, draw %.2f/%.2f ms, present %.2f/%.2f ms "
                "(mean/worst), longest gap %.1f ms\n",
                gFrameCount, presented - gFrameWindowStart,
                gFrameDrawSum / gFrameCount, gFrameDrawMax,
                gFramePresentSum / gFrameCount, gFramePresentMax, gFrameGapMax);
        gFrameWindowStart = presented;
        gFrameGapMax      = 0.0;
        gFrameDrawSum     = 0.0;
        gFrameDrawMax     = 0.0;
        gFramePresentSum  = 0.0;
        gFramePresentMax  = 0.0;
        gFrameCount       = 0;
    }
}

@implementation G2View

// THIS EDITOR'S INSTANCE, made current before anything the shared code does on its behalf. AppKit
// calls these methods directly, not through g2Plugin.c, so the plug-in's own selection cannot be
// relied on: another instance's audio or UI callback may have run on this thread in between.
- (void)enter {
    g2_draw_enter(self.doc);
}

// notes §7
- (void)enterForInput {
    [self enter];

    if (gLastDrawnView != self) {
        [self display];
    }
}


- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];

    if (self == nil) {
        return nil;
    }
    // ASSERTED, not chosen. The application picks a backend at start-up from a saved setting; this
    // build has only one linked (SYNTHLIB_NO_GL_BACKEND), so this says out loud which one everything
    // below is talking to rather than leaving it to a default.
    gfx_backend_choose(eRenderBackendMetal);

    gFrameStats = (getenv("G2_PLUGIN_FRAME_STATS") != NULL);

    [self startMeterTimer];

    // notes §8
    gfx_attach_window((__bridge void *)self);
    g2_draw_init();
    return self;
}


// notes §9
- (void)removeFromSuperview {
    cursor_release();        // an editor closed mid-drag must not leave the host without a pointer
    [self stopDragTimer];    // the timer retains self through its block; leaving it running leaks the view
    [self stopMeterTimer];

    // Hand back the layer and render targets. Without this a host that opens and closes editors
    // would exhaust the backend's window slots, since every new view is a different pointer — and
    // worse, see -dealloc.
    gfx_detach_window((__bridge void *)self);

    [super removeFromSuperview];
}

// notes §10
- (void)dealloc {
    [self stopMeterTimer];    // the backstop for a host that never takes the view out of its superview
    gfx_detach_window((__bridge void *)self);
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

// notes §11

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    (void)event;
    return YES;    // act on the click that focuses the window too, rather than swallowing it
}

// notes §12
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

// notes §13
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

// notes §14
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

        [self enterForInput];

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

// notes §15
- (void)startMeterTimer {
    __weak G2View * weakSelf = self;

    [self stopMeterTimer];

    // notes §16
    self.meterTimer = [NSTimer timerWithTimeInterval:0.05
                                             repeats:YES
                                               block:^(NSTimer * t) {
        G2View * strongSelf = weakSelf;

        if (strongSelf == nil) {
            [t invalidate];
            return;
        }
        [strongSelf enter];    // the meters asked about below are this instance's engine's

        // notes §17
        (void)sound_engine_meters_dirty();

        if (sound_engine_active() == true) {
            [strongSelf setNeedsDisplay:YES];
        }
    }];

    // Without this the meters freeze while a menu is tracking or the window is being resized, which
    // is the same fault in a smaller window - the sibling plug-ins' panels carry the same line.
    [[NSRunLoop mainRunLoop] addTimer:self.meterTimer forMode:NSRunLoopCommonModes];
}

- (void)stopMeterTimer {
    [self.meterTimer invalidate];    // the timer retains self, so this is also what lets the view go
    self.meterTimer = nil;
}

// Pushed from EVERY event that carries flags, before the event is acted on — a press must be judged
// by the modifiers held when it happened. AppKit puts the flags on all of them, mouse events
// included, so there is no polling to do and nothing to keep in step.
- (void)pushModifiersFor:(NSEvent *)event {
    set_modifier_state(modifier_bits_from_ns([event modifierFlags]));
}

// notes §18
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

// notes §19
- (void)keyDown:(NSEvent *)event {
    NSString * chars = [event charactersIgnoringModifiers];

    [self enterForInput];
    [self pushModifiersFor:event];

    // An open popup's keys first - a filename being typed must not trigger the shortcuts below.
    if (g2_input_popup_key([event keyCode], [[event characters] UTF8String], [event isARepeat])) {
        [self setNeedsDisplay:YES];
        return;
    }

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

    [self enterForInput];

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
    [self enterForInput];
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

    [self enterForInput];

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

    [self enterForInput];

    // No dispatch — a click nobody made. But the hover state must be advanced: menu highlighting
    // and the dwell timer that opens a submenu flyout both work off the pointer.
    [self pushModifiersFor:event];
    g2_input_hover(c.x, c.y);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

// notes §20
- (void)mouseExited:(NSEvent *)event {
    (void)event;

    if ([NSEvent pressedMouseButtons] != 0) {
        return;
    }
    [self enter];
    g2_input_pointer_left();
    [self setNeedsDisplay:YES];
}

// notes §21
- (void)scrollWheel:(NSEvent *)event {
    NSPoint c      = [self canvasPointFor:event];
    NSRect  bounds = [self bounds];
    NSRect  backing = [self convertRectToBacking:bounds];
    double  scale  = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;
    double  lines  = [event hasPreciseScrollingDeltas] ? 1.0 : WHEEL_SCROLL_STEP;

    [self enterForInput];
    [self pushModifiersFor:event];
    g2_input_scroll(c.x, c.y,
                    [event scrollingDeltaX] * lines * scale,
                    [event scrollingDeltaY] * lines * scale);
    [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent *)event {
    (void)event;    // the application has no use for right-down either; the menu opens on release
}

- (void)rightMouseUp:(NSEvent *)event {
    NSPoint c = [self canvasPointFor:event];

    [self enterForInput];
    [self pushModifiersFor:event];
    g2_input_right_click(c.x, c.y);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;

    [self enter];
    gLastDrawnView = self;

    // Physical pixels, not points. Asking the view to convert is the whole of the backing-scale
    // question on macOS — there is no host-supplied scale factor to plumb through, because VST3's
    // setContentScaleFactor() is not used on this platform.
    NSRect bounds  = [self bounds];
    NSRect backing = [self convertRectToBacking:bounds];
    double scale   = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;

    // notes §22
    gfx_attach_window((__bridge void *)self);

    // notes §23
    @autoreleasepool {
        double started = gFrameStats ? CACurrentMediaTime() : 0.0;

        g2_draw_frame((int)backing.size.width, (int)backing.size.height, scale);

        double drawn = gFrameStats ? CACurrentMediaTime() : 0.0;

        gfx_present();

        if (gFrameStats) {
            frame_stats_record(started, drawn, CACurrentMediaTime());
        }
    }
}

@end

// notes §24
void g2_view_request_redraw(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        for (G2View * view in gViews) {
            [view setNeedsDisplay:YES];
        }
    });
}

NSView * g2_create_gl_view(NSRect frame, void * doc) {
    // SELECTED BEFORE -initWithFrame:, which runs g2_draw_init() - and that sets up the top bar and
    // the split view in whichever document is current.
    g2_draw_enter(doc);

    G2View * view = [[G2View alloc] initWithFrame:frame];

    view.doc = doc;

    if (gViews == nil) {
        gViews = [NSHashTable weakObjectsHashTable];
    }
    [gViews addObject:view];
    return view;
}

// notes §25
void * g2_view_create(void * doc, double width, double height) {
    NSView * view = g2_create_gl_view(NSMakeRect(0.0, 0.0, width, height), doc);

    return (__bridge_retained void *)view;
}

void g2_view_destroy(void * view) {
    G2View * v = (__bridge G2View *)view;

    // THE TIMERS, WHICH RETAIN THEIR TARGET. A repeating NSTimer holds a strong reference to the
    // view, so a view the host has removed would otherwise never be deallocated and would go on
    // waking the main thread for a drag and a meter refresh that no longer have a window.
    [v.dragTimer invalidate];
    v.dragTimer = nil;
    [v.meterTimer invalidate];
    v.meterTimer = nil;
}
