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

// The plug-in's drawing surface, and nothing else.
//
// METAL ONLY, SINCE 2026-09-09, and this file used to be both. It carried an NSOpenGLView subclass
// beside the NSView one, chosen by G2_VST3_METAL at build time, because a superclass is fixed when
// the file is compiled and a runtime choice would have meant two copies of the four hundred lines of
// input handling below. That was the right trade while OpenGL was the path with the hours in it.
//
// It is not the right trade now. The Metal path is what ships, the sibling plug-ins have only ever
// had Metal, and the OpenGL half was an unreachable second surface implementation that still had to
// keep compiling - the deprecated NSOpenGLView, a pixel format, a context lock, prepareOpenGL and
// reshape. WHAT WENT IS macOS-SPECIFIC OPENGL and nothing more: SynthLib's renderBackendGL.c is
// untouched and is still the application's default, still OpenGL 1.1, and will be the ONLY backend
// when there are Windows and Linux versions - where a plug-in view is an HWND or an X11 window and
// could not have used a line of what was removed here anyway.
//
// Plain Objective-C, not Objective-C++: a view subclass genuinely needs the runtime, but nothing
// here needs C++, and the drawing itself needs neither (g2Draw.c). The three languages in this
// folder each earn their place - g2Editor.mm is Objective-C++ only because IPlugView is a C++
// interface that has to hand a Cocoa view to the host.
//
// A LAYER-HOSTING NSView. The CAMetalLayer on it is the surface; there is no context to own, so
// there is nothing to prepare and nothing to -update on a resize. gfx_set_surface() resizes the
// layer, called every frame with the view's backing size.

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

// The view currently attached, for g2_view_request_redraw() to find. A host can open more than
// one instance of the plug-in, so this is "the most recently attached" rather than "the" view —
// enough while the editor is a single window per instance, and the thing to revisit when a redraw
// needs to reach a specific instance. Weak so a closed editor leaves nil here rather than a dangling
// pointer.
static __weak G2View * gCurrentView = nil;

// FRAME STATISTICS, for "the editor does not refresh as quickly as the application". With
// G2_PLUGIN_FRAME_STATS=1 in the host's environment, one line a second on stderr: how many frames
// were drawn, the mean and worst time spent building one and presenting it, and the longest gap
// between two. The first two answer "is a frame expensive"; the gap answers "is the host asking for
// frames at all" - the two causes look identical from the outside and want opposite fixes.
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

    // LAYER-HOSTING, and the order matters: gfx_attach_window() assigns the layer and only then
    // sets wantsLayer, which is what tells AppKit the contents are the layer's and that it must not
    // draw over them. Handing it the VIEW rather than a window is the whole difference from the
    // application — in a plug-in the window belongs to the host.
    gfx_attach_window((__bridge void *)self);
    g2_draw_init();
    return self;
}


// WITHOUT A TRACKING AREA, -mouseMoved: IS NEVER CALLED. That is why menu items did not highlight:
// the highlight is drawn from the pointer position (render_context_menu reads it), and the position
// was only ever updated on a click. AppKit delivers mouse-moved events to a view solely on the
// strength of a tracking area covering it, and the area has to be rebuilt whenever the view resizes.
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

// THE BACKSTOP, for a host that releases the view without ever taking it out of its superview.
//
// THIS IS THE ONE THAT CRASHED LIVE. gfx_attach_window() is called from -initWithFrame: above and
// there was no matching detach anywhere in this plug-in — GenBridge and MidiSyncTool have had one
// since their per-window slots were written; only this one went without. Every editor the host
// closed left its slot occupied, holding that dead view's address and its CAMetalLayer.
//
// THAT CRASHED LIVE. The backend finds a window's slot by comparing the raw view POINTER, and the
// allocator hands the same address back for a new NSView all the time — so reopening the editor
// matched the dead view's slot, took the "already known, just select it" path, and drew and
// presented into a layer belonging to a view that no longer existed. The crash is inside
// -nextDrawable, on a layer that is no longer in any live layer tree.
//
// It is also a slow leak of the eight available slots, which is the same fault arriving by a
// different route: past the eighth open the backend has no slot to give and leaves the previous
// window selected, so a new editor draws into an old one's layer.
//
// BOTH HERE AND IN -removeFromSuperview. That one is the deterministic path and is where the two
// sibling plug-ins do it; this catches the host that skips it. mtl_detach_window() clears the
// slot's `native`, so whichever runs second finds nothing and does nothing.
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

// THE METERS AND LEDs, WHICH NOTHING ELSE ASKS TO BE DRAWN.
//
// The sound engine publishes what a module's meter and LED should show from the AUDIO thread, into
// atomic arrays, and it cannot ask for a frame itself without a syscall per block. So it raises a
// flag when a published value actually CHANGES - not every block, or the panel would run flat out
// over a silent patch - and something has to consume it.
//
// In the application that consumer is the render loop (graphics.c, 2026-09-08). A plug-in has no
// loop: it draws when AppKit is told to, and everything else that changes the canvas says so through
// synthlib_request_redraw(). The engine is the one thing that cannot, so this timer does it for it,
// and the symptom without it is exactly the application's was - the compressor's LED and the volume
// meters moving only while the mouse did.
//
// 50 ms, which is the application's cadence, and NOT the drag timer's 60 Hz: a meter is read by eye
// and twenty frames a second is more than enough for one, where a drag is followed by the hand and
// is not. It runs for as long as the editor is open, because in the PLUG-IN the engine is always
// running - it is what the plug-in is for, started in setupProcessing() rather than switched on from
// a menu as it is in the application. An idle tick costs one atomic exchange.
- (void)startMeterTimer {
    __weak G2View * weakSelf = self;

    [self stopMeterTimer];

    // +timerWithTimeInterval:, NOT +scheduledTimerWithTimeInterval:. The scheduled one is already on
    // the run loop in NSDefaultRunLoopMode, so adding it again below registered the same timer with
    // the same run loop twice - which the documentation says not to do, and which is the first thing
    // to suspect if a timer stops firing. Created unscheduled and added ONCE, in common modes.
    //
    // WEAK self. The block is retained by the timer and the timer by the view, so capturing self
    // strongly made a cycle the view could never escape - and -dealloc, the documented backstop for
    // a host that never calls -removeFromSuperview, could then never run. That backstop is what
    // stops a reopened editor drawing into a dead view's Metal layer, so the cycle was not merely a
    // leak: it disarmed the fix for the crash in project_g2alike_metal_layer_crash.
    self.meterTimer = [NSTimer timerWithTimeInterval:0.05
                                             repeats:YES
                                               block:^(NSTimer * t) {
        G2View * strongSelf = weakSelf;

        if (strongSelf == nil) {
            [t invalidate];
            return;
        }
        // ASKED, BUT NOT OBEYED, and the flag is consumed rather than tested. The application
        // redraws only when this says something changed, because its loop would otherwise spin at
        // the display's rate; here the rate is already fixed at 20 Hz by the timer, so the gate
        // saves one redraw of an idle canvas and buys a whole class of confusion - a meter that has
        // stopped and a flag that says nothing changed look identical from the outside. It has to
        // be consumed either way, or the application's own render loop would find it permanently
        // set the moment the two ever share a build.
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
//
// EXCEPT THAT A WHEEL DOES NOT SEND POINTS. AppKit only reports scrollingDelta in points when
// hasPreciseScrollingDeltas is YES, which means a trackpad or a Magic Mouse; a traditional wheel
// reports LINES, and one notch is 1.0. Passed on unconverted that became about two pixels of canvas
// per notch — the deltas were being scaled as though they were points when they were not.
// WHEEL_SCROLL_STEP is the application's own notch-to-pixel figure, so a notch here now moves the
// canvas exactly as far as a notch there, and a trackpad's points still pass through untouched.
- (void)scrollWheel:(NSEvent *)event {
    NSPoint c      = [self canvasPointFor:event];
    NSRect  bounds = [self bounds];
    NSRect  backing = [self convertRectToBacking:bounds];
    double  scale  = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;
    double  lines  = [event hasPreciseScrollingDeltas] ? 1.0 : WHEEL_SCROLL_STEP;

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

    [self pushModifiersFor:event];
    g2_input_right_click(c.x, c.y);
    [self ensureTickTimer];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;

    // Physical pixels, not points. Asking the view to convert is the whole of the backing-scale
    // question on macOS — there is no host-supplied scale factor to plumb through, because VST3's
    // setContentScaleFactor() is not used on this platform.
    NSRect bounds  = [self bounds];
    NSRect backing = [self convertRectToBacking:bounds];
    double scale   = (bounds.size.width > 0.0) ? (backing.size.width / bounds.size.width) : 1.0;

    // SELECT THIS VIEW'S CONTEXT FIRST. The backend keeps one CURRENT window, so with two editors
    // open whichever drew last left it pointing at its own layer, and drawing without claiming ours
    // would paint into the other one's window. Attaching an already-known view is a pointer
    // assignment, so doing it every frame is cheap and removes any need to track whose turn it is.
    // Both sibling plug-ins have always done this; this one did not, which is a second way for one
    // editor to end up drawing into another's layer.
    gfx_attach_window((__bridge void *)self);

    // A Metal command buffer is built and committed here and nowhere else, and the backend owns its
    // own state. The frame is drawn into the backend's offscreen target and gfx_present() blits it
    // to this view's layer — the same two calls the application's render_present() makes, spelled
    // out because a plug-in's frame is driven by AppKit rather than by a render loop.
    //
    // ONE AUTORELEASE POOL PER FRAME, drained before this returns. The backend's command buffer,
    // encoder and drawable are autoreleased, and the command buffer keeps every vertex buffer the
    // frame drew with alive for as long as it lives - so without a pool of our own, freeing them
    // waits on whichever pool the HOST drains, whenever it drains it. Hygiene, and NOT the cure for
    // the editor slowing down the longer it was open: that was the Metal backend compiled without
    // ARC, and this pool made no measurable difference to it (see do-plugin's OBJC_SOURCES).
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

// Called from plain C — and, in the application's design, potentially from the USB thread, which is
// why the main-thread hop is here rather than being every caller's problem. dispatch_async and not
// _sync: a synchronous hop from a thread the main thread is waiting on is a deadlock, and redrawing
// a frame later is never worth that risk.
void g2_view_request_redraw(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gCurrentView setNeedsDisplay:YES];    // nil-safe; a closed editor simply does nothing
    });
}

NSView * g2_create_gl_view(NSRect frame) {
    G2View * view = [[G2View alloc] initWithFrame:frame];

    gCurrentView = view;
    return view;
}

// The plain-C door into the above, for the wrapper - see g2View.h.
//
// __bridge_retained IS THE POINT OF IT. The view crosses to the caller as a void *, where ARC can
// see nothing at all, so the +1 has to be handed over explicitly; the wrapper's __bridge_transfer
// takes it back. Returning an autoreleased object through a void * would have it freed out from
// under the host at the end of the run loop's next pass.
void * g2_view_create(double width, double height) {
    NSView * view = g2_create_gl_view(NSMakeRect(0.0, 0.0, width, height));

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
