# g2View.m notes

The longer comments from `g2View.m`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

The plug-in's drawing surface, and nothing else.

METAL ONLY, SINCE 2026-09-09, and this file used to be both. It carried an NSOpenGLView subclass
beside the NSView one, chosen by G2_VST3_METAL at build time, because a superclass is fixed when
the file is compiled and a runtime choice would have meant two copies of the four hundred lines of
input handling below. That was the right trade while OpenGL was the path with the hours in it.

It is not the right trade now. The Metal path is what ships, the sibling plug-ins have only ever
had Metal, and the OpenGL half was an unreachable second surface implementation that still had to
keep compiling - the deprecated NSOpenGLView, a pixel format, a context lock, prepareOpenGL and
reshape. WHAT WENT IS macOS-SPECIFIC OPENGL and nothing more: SynthLib's renderBackendGL.c is
untouched and is still the application's default, still OpenGL 1.1, and will be the ONLY backend
when there are Windows and Linux versions - where a plug-in view is an HWND or an X11 window and
could not have used a line of what was removed here anyway.

Plain Objective-C, not Objective-C++: a view subclass genuinely needs the runtime, but nothing
here needs C++, and the drawing itself needs neither (g2Draw.c). The three languages in this
folder each earn their place - g2Editor.mm is Objective-C++ only because IPlugView is a C++
interface that has to hand a Cocoa view to the host.

A LAYER-HOSTING NSView. The CAMetalLayer on it is the surface; there is no context to own, so
there is nothing to prepare and nothing to -update on a resize. gfx_set_surface() resizes the
layer, called every frame with the view's backing size.

## 2. `modifier_bits_from_ns()`

THE PLUG-IN'S HALF OF SynthLib's MODIFIER SEAM. The application translates GLFW's `mods`; this
translates an NSEvent's flags, and everything downstream reads the same predicates without knowing
which shell it is running in. Before the seam existed these were stubbed to false in
g2AppStubs.c — Shift and Command simply did nothing in the plug-in.

Cmd is COMMAND, matching the application's GLFW_MOD_SUPER, and Alt is OPTION. NSEvent reports
several bits this UI has no use for (Caps Lock, the function key, the numeric-keypad flag), so the
translation is a whitelist rather than a cast — a stray bit must not read as a held modifier.

## 3. `gCursorHidden`

── Hiding the pointer for a drag (canvasDrag.h's cursor_capture/cursor_release) ────────────────

These were no-ops, on the reasoning that a host-owned view has no business confining the pointer.
Hiding it is a different question from confining it, and hiding is what an incremental dial drag
actually wants: the application hides the pointer for the same gesture, and a visible cursor
wandering off the dial it is turning looks broken.

[NSCursor hide] IS PROCESS-WIDE AND REFERENCE-COUNTED. Process-wide means the HOST loses its pointer
too, so an unbalanced hide is not a cosmetic bug — it is a DAW with no cursor. Hence the flag rather
than trusting call pairing, the poll in g2_input_drag_tick(), and the release in
-removeFromSuperview below for an editor closed mid-drag.

THE POINTER IS PUT BACK WHERE THE DRAG STARTED. Without confinement it keeps moving while hidden, so
on release it would otherwise reappear somewhere across the screen from the dial the user was just
turning. Warping it back is what makes this feel like the application, whose GLFW cursor mode does
the same thing by decoupling the pointer entirely.

NOT CONFINED, still: a long drag can run the physical mouse off the edge of the screen and the value
stops following. Fixing that needs CGAssociateMouseAndMouseCursorPosition(false) and feeding the
drag from -deltaX/-deltaY instead of absolute positions, which is a change to how motion reaches the
canvas rather than one more line here.

## 4. in `cursor_capture()`

DECOUPLED FROM THE HARDWARE, which is what makes the drag unbounded: the pointer stops moving
while movement still arrives as deltas, so a dial can be turned further than the screen is wide.
Its absolute position is frozen from here, which is why -mouseDragged: switches to
g2_input_drag_by() while this is in force — differencing a frozen position reports no movement.
The same pair of calls GLFW's disabled-cursor mode uses (ThirdParty/glfw cocoa_window.m).

## 5. `gViews`

EVERY OPEN EDITOR, for g2_view_request_redraw(). A host can have several instances' editors open at
once, and a redraw request carries no instance - synthlib_request_redraw() is called from all over
the shared code - so all of them are asked. It used to be only the most recently opened one, which
left every other editor frozen until the mouse crossed it. Weak, so a closed editor simply drops out.
Main thread only, which is where every caller reaches it (g2_view_request_redraw() hops there).

## 6. `gFrameStats`

FRAME STATISTICS, for "the editor does not refresh as quickly as the application". With
G2_PLUGIN_FRAME_STATS=1 in the host's environment, one line a second on stderr: how many frames
were drawn, the mean and worst time spent building one and presenting it, and the longest gap
between two. The first two answer "is a frame expensive"; the gap answers "is the host asking for
frames at all" - the two causes look identical from the outside and want opposite fixes.

## 7. in `frame_stats_record()`

The same, for an EVENT, which is hit-tested against the click regions the last frame registered.
If another editor drew since this one did, those regions are that editor's - so this one draws
itself first, synchronously, which refills them with its own. Costs a frame only when two editors
are in use at once, and only on the event that switches between them.

## 8. in `frame_stats_record()`

LAYER-HOSTING, and the order matters: gfx_attach_window() assigns the layer and only then
sets wantsLayer, which is what tells AppKit the contents are the layer's and that it must not
draw over them. Handing it the VIEW rather than a window is the whole difference from the
application — in a plug-in the window belongs to the host.

## 9. in `frame_stats_record()`

WITHOUT A TRACKING AREA, -mouseMoved: IS NEVER CALLED. That is why menu items did not highlight:
the highlight is drawn from the pointer position (render_context_menu reads it), and the position
was only ever updated on a click. AppKit delivers mouse-moved events to a view solely on the
strength of a tracking area covering it, and the area has to be rebuilt whenever the view resizes.

## 10. in `frame_stats_record()`

THE BACKSTOP, for a host that releases the view without ever taking it out of its superview.

THIS IS THE ONE THAT CRASHED LIVE. gfx_attach_window() is called from -initWithFrame: above and
there was no matching detach anywhere in this plug-in — GenBridge and MidiSyncTool have had one
since their per-window slots were written; only this one went without. Every editor the host
closed left its slot occupied, holding that dead view's address and its CAMetalLayer.

THAT CRASHED LIVE. The backend finds a window's slot by comparing the raw view POINTER, and the
allocator hands the same address back for a new NSView all the time — so reopening the editor
matched the dead view's slot, took the "already known, just select it" path, and drew and
presented into a layer belonging to a view that no longer existed. The crash is inside
-nextDrawable, on a layer that is no longer in any live layer tree.

It is also a slow leak of the eight available slots, which is the same fault arriving by a
different route: past the eighth open the backend has no slot to give and leaves the previous
window selected, so a new editor draws into an old one's layer.

BOTH HERE AND IN -removeFromSuperview. That one is the deterministic path and is where the two
sibling plug-ins do it; this catches the host that skips it. mtl_detach_window() clears the
slot's `native`, so whichever runs second finds nothing and does nothing.

## 11. in `frame_stats_record()`

── Mouse ───────────────────────────────────────────────────────────────────────────────────────

Cocoa's origin is bottom-left and the canvas's is top-left, so y is flipped here — at the
boundary, in the only file that knows Cocoa's convention. Everything past this point is in the
canvas's own coordinates and the application's existing hit-testing takes over (g2Input.c).

The LEFT button goes through the click regions, which do not distinguish buttons; the RIGHT button
has its own path, because the application's context menus hit-test connectors, parameters and
module bodies in a specific order of their own.

## 12. in `frame_stats_record()`

PHYSICAL PIXELS, y-flipped. Not points: the canvas works in its own logical units, and only
g2Input.c knows the scale that converts between them. Handing over pixels keeps this file's job to
the two things it is actually authoritative about — Cocoa's bottom-left origin, and the backing
scale of the surface it owns.

## 13. in `frame_stats_record()`

THE MOUSE-UP THAT NEVER ARRIVED. A captured drag has the pointer hidden AND decoupled from the
hardware, so losing the release does not just leave a dial held — it leaves the user with no cursor
and a mouse that does not move, inside somebody else's DAW. Hosts do run their own event routing, and
-mouseUp: is not guaranteed to reach a plug-in's view.

[NSEvent pressedMouseButtons] IS THE AUTHORITY. It reports the hardware, so it is true whether or not
we were told; every other candidate — our own drag flags, the last event we saw — is derived from the
thing that went missing. If the button is up while we still hold the pointer, the release is
synthesised through the ordinary path so the drag ends exactly as it would have, undo included.

## 14. in `frame_stats_record()`

Started when there is something to advance and stopped by the tick itself once there is not:
60 Hz of doing nothing is a poor thing to leave running inside somebody else's host.

CALLED AFTER EVERY MOUSE EVENT, not just on press. A right-click context menu opens from
-rightMouseUp:, and a menu opened once the timer had already stopped itself would otherwise have
nothing driving its dwell timer at all — which is why submenus still needed a jiggle after the
first fix.

## 15. in `frame_stats_record()`

THE METERS AND LEDs, WHICH NOTHING ELSE ASKS TO BE DRAWN.

The sound engine publishes what a module's meter and LED should show from the AUDIO thread, into
atomic arrays, and it cannot ask for a frame itself without a syscall per block. So it raises a
flag when a published value actually CHANGES - not every block, or the panel would run flat out
over a silent patch - and something has to consume it.

In the application that consumer is the render loop (graphics.c, 2026-09-08). A plug-in has no
loop: it draws when AppKit is told to, and everything else that changes the canvas says so through
synthlib_request_redraw(). The engine is the one thing that cannot, so this timer does it for it,
and the symptom without it is exactly the application's was - the compressor's LED and the volume
meters moving only while the mouse did.

50 ms, which is the application's cadence, and NOT the drag timer's 60 Hz: a meter is read by eye
and twenty frames a second is more than enough for one, where a drag is followed by the hand and
is not. It runs for as long as the editor is open, because in the PLUG-IN the engine is always
running - it is what the plug-in is for, started in setupProcessing() rather than switched on from
a menu as it is in the application. An idle tick costs one atomic exchange.

## 16. in `frame_stats_record()`

+timerWithTimeInterval:, NOT +scheduledTimerWithTimeInterval:. The scheduled one is already on
the run loop in NSDefaultRunLoopMode, so adding it again below registered the same timer with
the same run loop twice - which the documentation says not to do, and which is the first thing
to suspect if a timer stops firing. Created unscheduled and added ONCE, in common modes.

WEAK self. The block is retained by the timer and the timer by the view, so capturing self
strongly made a cycle the view could never escape - and -dealloc, the documented backstop for
a host that never calls -removeFromSuperview, could then never run. That backstop is what
stops a reopened editor drawing into a dead view's Metal layer, so the cycle was not merely a
leak: it disarmed the fix for the crash in project_g2alike_metal_layer_crash.

## 17. in `frame_stats_record()`

ASKED, BUT NOT OBEYED, and the flag is consumed rather than tested. The application
redraws only when this says something changed, because its loop would otherwise spin at
the display's rate; here the rate is already fixed at 20 Hz by the timer, so the gate
saves one redraw of an idle canvas and buys a whole class of confusion - a meter that has
stopped and a flag that says nothing changed look identical from the outside. It has to
be consumed either way, or the application's own render loop would find it permanently
set the moment the two ever share a build.

## 18. in `frame_stats_record()`

WITHOUT THIS THE VIEW RECEIVES NO KEY EVENTS AT ALL, and NSView's default is NO. An earlier comment
here claimed it was "already YES", which was simply wrong: -keyDown: and -flagsChanged: were both
dead code, so the plug-in had no keyboard shortcuts and could not see a modifier pressed while the
pointer sat still.

## 19. in `frame_stats_record()`

+/- steps the parameter under the pointer; Cmd +/- zooms the canvas. Both live in shared code — see
g2_input_key(), which returns false for anything it does not want.

ANYTHING UNCLAIMED GOES TO super, and that is deliberate: the host owns shortcuts of its own (the
space bar for transport, most obviously) and a plug-in editor that swallowed every key would be a
worse neighbour than one that swallowed none.

## 20. in `frame_stats_record()`

The tracking area asked for enter/exit alongside movement, and this is the half that was missing:
without it the last hovered menu item or connector stayed highlighted after the pointer had left
the plug-in window entirely, since nothing else ever moves the recorded position back off it.

A drag that leaves the view is not a departure — AppKit keeps delivering its movement, and the
canvas auto-scrolls precisely because the pointer is outside. So an exit with a button still down
is ignored, and the release that ends the drag puts the position where the pointer really is.

## 21. in `frame_stats_record()`

The application opens its context menus on right button UP, not down, so the same here.
Trackpad and wheel both arrive here. Deltas are in points and the canvas scrolls in pixels, so
they are handed over as-is and g2Input.c applies the scale — the same division every other
coordinate goes through.

EXCEPT THAT A WHEEL DOES NOT SEND POINTS. AppKit only reports scrollingDelta in points when
hasPreciseScrollingDeltas is YES, which means a trackpad or a Magic Mouse; a traditional wheel
reports LINES, and one notch is 1.0. Passed on unconverted that became about two pixels of canvas
per notch — the deltas were being scaled as though they were points when they were not.
WHEEL_SCROLL_STEP is the application's own notch-to-pixel figure, so a notch here now moves the
canvas exactly as far as a notch there, and a trackpad's points still pass through untouched.

## 22. in `frame_stats_record()`

SELECT THIS VIEW'S CONTEXT FIRST. The backend keeps one CURRENT window, so with two editors
open whichever drew last left it pointing at its own layer, and drawing without claiming ours
would paint into the other one's window. Attaching an already-known view is a pointer
assignment, so doing it every frame is cheap and removes any need to track whose turn it is.
Both sibling plug-ins have always done this; this one did not, which is a second way for one
editor to end up drawing into another's layer.

## 23. in `frame_stats_record()`

A Metal command buffer is built and committed here and nowhere else, and the backend owns its
own state. The frame is drawn into the backend's offscreen target and gfx_present() blits it
to this view's layer — the same two calls the application's render_present() makes, spelled
out because a plug-in's frame is driven by AppKit rather than by a render loop.

ONE AUTORELEASE POOL PER FRAME, drained before this returns. The backend's command buffer,
encoder and drawable are autoreleased, and the command buffer keeps every vertex buffer the
frame drew with alive for as long as it lives - so without a pool of our own, freeing them
waits on whichever pool the HOST drains, whenever it drains it. Hygiene, and NOT the cure for
the editor slowing down the longer it was open: that was the Metal backend compiled without
ARC, and this pool made no measurable difference to it (see do-plugin's OBJC_SOURCES).

## 24. `g2_view_request_redraw()`

Called from plain C — and, in the application's design, potentially from the USB thread, which is
why the main-thread hop is here rather than being every caller's problem. dispatch_async and not
_sync: a synchronous hop from a thread the main thread is waiting on is a deadlock, and redrawing
a frame later is never worth that risk.

## 25. `g2_view_create()`

The plain-C door into the above, for the wrapper - see g2View.h.

__bridge_retained IS THE POINT OF IT. The view crosses to the caller as a void *, where ARC can
see nothing at all, so the +1 has to be handed over explicitly; the wrapper's __bridge_transfer
takes it back. Returning an autoreleased object through a void * would have it freed out from
under the host at the end of the run loop's next pass.
