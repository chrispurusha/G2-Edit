# g2Input.c notes

The longer comments from `g2Input.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

The plug-in's mouse input, in C. g2View.m turns Cocoa events into calls on this; nothing below
knows what an NSEvent is.

THE HIT-TESTING IS NOT REIMPLEMENTED HERE, and that is the point. Every clickable thing on the
canvas — module bodies, dials, toggles, connectors — registers a click region as it is DRAWN
(moduleGraphics.c, renderParams.c), and SynthLib's dispatch_click_region() resolves a coordinate
against them. That machinery is already platform-free and the plug-in already runs it, so all the
application's hit-testing behaviour, including press-capture and layer priority, comes for free.
What was missing was only somebody to say where the mouse is.

WHERE A CLICK ENDS UP: the handlers write to the module database and then call msg_send() to tell
the G2. In the plug-in msg_send() is a no-op (g2AppStubs.c) because there is no G2 — but the
database write still happens, and sound_engine_update_from_patch() reads exactly that. So a dial
drag here changes the patch and the sound with no hardware in the path at all, which is the
behaviour a plug-in wants rather than a limitation of it.

## 2. file scope

defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
behind the top bar and with the margin between them swallowed.

## 3. `gMouse`

The last position the host told us about, in the canvas's logical units with a top-left origin —
the same space get_global_gui_scaled_mouse_coord() produces in the application, so everything
downstream is unchanged.

STARTS OFF-CANVAS, and {0,0} would be a bug: it is not a neutral "unknown", it is the top-left
corner, which is exactly where the menu bar's first item sits. render_menu_bar() highlights
whatever the pointer is inside, so a freshly-opened editor drew "File" lit up before the host had
said anything about the pointer at all — and it stayed lit, because the view's tracking area is
NSTrackingActiveInKeyWindow and delivers no movement until the plug-in window becomes key. The
application never had this: GLFW reports a real cursor position from the first frame.

## 4. `g2_input_set_mouse()`

Takes PHYSICAL PIXELS and stores LOGICAL UNITS — the canvas's own space, which is what every
hit-test and every click region is expressed in.

This is the same conversion get_global_gui_scaled_mouse_coord() performs in the application, and
it has to track gGlobalGuiScale rather than the backing scale: the two were equal only while the
plug-in mis-sized its logical canvas.

## 5. in `dispatch_drag()`

Dials first: a parameter drag and a module drag can never both be active, but the
parameter one is the common case and reads an absolute angle, so it costs nothing to ask.

The raw coordinates are the canvas ones, and that is SELF-CONSISTENT rather than a
compromise: cursor_raw_coord() records the drag origin from this same gMouse, so the
incremental dial modes difference two values in one space. All three dial modes therefore
work here — the mode comes from this plug-in's own prefs and its own Controls menu, and
eDialModeRotary is only the fallback default in g2Prefs.c when that pref is absent.

What is missing without a real cursor_capture() is pointer HIDING and confinement, not the
arithmetic: the pointer visibly travels away from the dial, and a long drag can run out of
screen. An earlier comment here claimed the plug-in "reports eDialModeRotary" and that the
other modes could not work at all, which was simply untrue.

## 6. in `dispatch_drag()`

All four gestures through the shared table, in the one order that is written down —
see canvasDrag.h.

ALT IS REAL NOW, so an Alt-drag on a dial adjusts its MORPH OFFSET rather than its value,
as it does in the application. It works in every dial mode: Alt only changes which field
the resulting value is written to, and the incremental modes are sound here for the
reason given above.

## 7. `g2_input_drag_by()`

MOTION FROM DELTAS, for a drag whose pointer is confined. dx/dy are in the same pixel space
g2_input_set_mouse() takes, so the conversion to logical units is identical — see there.

This is what makes an incremental dial drag unbounded: with the pointer decoupled from the hardware
(cursor_capture() in g2View.m), its absolute position is frozen, so differencing absolute positions
would report no movement at all. Accumulating the deltas into gMouse gives the same VIRTUAL pointer
GLFW's disabled-cursor mode hands the application — see cocoa_window.m in ThirdParty, which adds
[event deltaY] to a top-left-origin position exactly as this does, and is where the sign came from.

## 8. in `g2_input_mouse_event()`

A MODAL POPUP OWNS THE CLICK. While one is open nothing behind it may act on a click — the
application makes the same check before anything else in its own press handler.

Through SynthLib's coordinator rather than by hand. This tested file_browser_active() alone
and called the browser's two handlers directly, which left the OTHER modal popups — the alert
dialog above all — with no way to be clicked, and so no way to be dismissed. The coordinator
knows the layer order and which popups are modal, and dispatches to the frontmost active one.
BOTH PHASES, and the release is not optional: the alert dialog arms its button on the press
and ACTS on the release, so dispatching the press alone drew a pressed OK that never did
anything. The file browser acts on the press, which is why sending only that had looked
sufficient.

## 9. in `g2_input_mouse_event()`

THE TOPBAR. Its controls are NOT click regions — mouseTopbar.c hit-tests them against the
rectangles topbarResourcesAccess.c holds — so nothing reaches them unless they are asked
directly. That is why the morph dials worked (they DO register click regions, in
moduleGraphics.c) while every button and dial beside them did nothing.

Straight after the menu bar and before the scrollbars, which is the application's order.

## 10. in `g2_input_mouse_event()`

NOTHING WAS UNDER THE POINTER. In the application this is where a click on bare canvas
clears the selection and starts a rubber band; dispatch_click_region() returning false is
the whole of how "empty space" is detected, since every occupied part of the canvas
registers a region. Without this the plug-in could select a module but never deselect one.

No modifier plumbing yet, so a press always replaces the selection rather than adding to
it — multi_select_modifier_held() is still false in the plug-in.

## 11. in `g2_input_mouse_event()`

---- release ---------------------------------------------------------------------------------

DO NOT RETURN EARLY WHEN dispatch_click_region() CLAIMS THE RELEASE. A press CAPTURES its
region (see clickRegion.h), so the handler that began a module drag owns the matching release
and dispatch always reports it handled. Returning there was a real bug: the drag was never
ended and never re-ordered, so a module dropped on another simply overlapped it, and
gModuleDrag stayed active into the next gesture.

Both must run: the captured handler needs its release, and the drag needs finishing.
EVERY TOPBAR BUTTON RELEASES, whether or not the release landed on one — the application does
exactly this at the top of its own mouse-up. Without it isPressed stayed set, so buttons kept
their pressed grey after the mouse came up, and that grey also masked the green a slot or a
Hide/Dim toggle had just been given by set_exclusive_button_highlight().

## 12. in `g2_input_mouse_event()`

BEFORE the topbar and before the canvas: a palette drag RELEASES over the canvas, which is
the whole point of it, so the release cannot be claimed by whatever happens to be under the
cursor. palette_left_up() returns false unless a tile was actually pressed, so it costs
nothing while the palette is idle or closed.

## 13. in `g2_input_mouse_event()`

EVERY GESTURE'S RELEASE, IN ONE CALL. This was four hand-written blocks in a different order
from the application's four, which is the asymmetry that let this shell quietly miss phases: the
module drag went un-re-ordered and dials stayed held after mouse-up, each until someone noticed.
canvasGestureAll is the simple case — the application passes masks only because it interleaves
dispatch_click_region() partway through. See canvasDrag.h.

Undo is still the one thing not carried over here: a plug-in has no undo stack, which is exactly
why the application wraps the param release in finish_param_drag() rather than the table doing it.

## 14. `g2_input_hover()`

Pointer moved with no button down. Updates the position AND advances the hover state: the menu
highlight is drawn from the pointer, and a submenu flyout opens on a dwell timer that only ticks
when something polls it. The application polls both of these every frame (graphics.c); doing it on
movement is enough here, since the plug-in redraws on demand rather than continuously.

## 15. `g2_input_pointer_left()`

The pointer has left the plug-in's view. Parks it back off-canvas so nothing hit-tests true — the
menu bar item and the connector the pointer was last over both stop being highlighted, rather than
staying lit until it comes back. Same sentinel the position starts at, and for the same reason.

The caller ignores an exit that arrives with a button still down: a drag deliberately continues
outside the view, and AppKit keeps delivering its movement.

## 16. in `g2_input_scroll()`

THE POPUPS GET THE WHEEL FIRST, which is the application's very first line in scroll_event()
and was the one input this function never forwarded. The file browser could be dragged by its
scrollbar thumb but not scrolled, and the notch fell through to the canvas hidden behind it.

The plug-in registers no popups of its own — synthlib_popups_register() is called only from
the application — but it does not need to: SynthLib's own table carries the file browser, the
bank browser and the alert dialog, so dispatching here covers all three exactly as it does in
the application, rather than hand-rolling a file_browser_active() check that would have to be
extended for every popup added later.

ROWS, NOT PIXELS. A popup's scroll handler counts LIST ROWS, and the application hands it raw
GLFW notches at one notch per row. What arrives here is pixels, because the canvas panes below
want pixels. Dividing by the same WHEEL_SCROLL_STEP the view multiplied by puts a notch back
on one row instead of inventing a second constant that could drift from it.

## 17. in `g2_input_scroll()`

Over the palette band the wheel scrolls the TILES sideways — a group wider than the window is
otherwise unreachable, and the band is above both panes so no pane wants this event anyway.
NOTCHES, not pixels: palette_scroll() advances by one tile per unit, so the same
WHEEL_SCROLL_STEP division the popups get applies here for the same reason.

## 18. in `g2_input_scroll()`

CMD + WHEEL ZOOMS, as it does in the application, around the pointer rather than the corner.

ONE STEP PER EVENT rather than scaling by the delta: the deltas arriving here are PIXELS (see
the caller in g2View.m, which multiplies by the backing scale), so feeding them to a zoom
factor that moves in 0.1 steps would fling the canvas from one limit to the other on a single
flick. A notch is a step, which is what Cmd +/- does too.

## 19. `g2_input_drag_tick()`

A drag tick with no new mouse event behind it.

Auto-scroll only advances when something asks it to, so holding the pointer still just past a
pane's edge would stop the scrolling dead. The application solves this by synthesising a
cursor_pos() call from its main loop while a drag is active (graphics.c: "Artificially do
cursor_pos call for drag scrolling when cursor not moving"); this is the same trick, driven by a
timer in the view.

Returns true if anything moved, so the caller only redraws when there is a reason to.

## 20. in `g2_input_drag_tick()`

A CAPTURED POINTER KEEPS THIS TICKING, and that is the whole point of the line. cursor_capture()
hides the pointer for the entire HOST process and decouples it from the hardware, so a mouse-up
that never arrives leaves the user with no cursor and a frozen mouse in their DAW. The recovery
that catches that lives in g2View.m (-recoverLostRelease), because the authority on whether the
button is still down is [NSEvent pressedMouseButtons] and not anything visible from here — but it
can only run while this timer is alive, and a parameter drag on its own never made it busy.

The previous version of this net tested (captured && !gParamDragging.active) and was useless
twice over: the timer had already stopped itself mid-drag, and only the release that went missing
clears gParamDragging, so the condition could never come true in the case it was written for.

## 21. in `g2_input_drag_tick()`

AN OPEN MENU needs ticking for the same kind of reason: a submenu opens on a DWELL timer, and
that timer only advances when something asks it to. Driving it from pointer movement alone
meant a flyout would not appear unless the mouse was kept jiggling on the parent item. The
application polls this every frame.

## 22. `cursor_raw_coord()`

Called by the canvas when a dial drag begins (moduleGraphics.c). In the application this ALSO
hides the cursor and warps it, which is what lets a vertical drag run past the screen edge; here
it cannot, so the pointer stays visible and travels.

SETTING THE ORIGIN IS NOT OPTIONAL, though, and leaving this empty was a real bug. The vertical and
horizontal dial modes compute the new value as

```
    value + (previousY - currentY) * range / 200

```
so with no origin recorded, previousY was still 0 and the very first movement evaluated
(0 - currentY), a large negative number that drove every dial straight to zero. Rotary hid it by
reading an absolute angle and never touching these.
── The plug-in's half of the drag-begin seam (canvasDrag.h) ────────────────────────────────────

This WAS start_cursor_drag(), a per-shell function that had to remember to record the drag origin.
It is now two named jobs, and the one the plug-in cannot do is the one it is allowed to decline.

The plug-in reports motion in canvas coordinates, so the origin is recorded in those — it is only
ever differenced against later positions from this same source, so the space just has to agree with
itself. Recording it in raw screen coordinates while reporting motion in canvas ones is precisely
the kind of mismatch this seam's comment warns about.

## 23. `get_global_gui_scaled_mouse_coord()`

DELIBERATE NO-OPS, and no longer silently damaging. An NSView owned by the host is not ours to
confine the pointer inside, and the previous arrangement meant declining that also discarded the
drag origin — vertical and horizontal dial drags collapsed to zero while rotary looked perfect,
because rotary reads an absolute angle and never touches the origin.

WHAT IS LOST IS HIDING, NOT FUNCTION. All three dial modes work — see g2_input_mouse_event() — and
what a real implementation would add is NSCursor hide/unhide plus
CGAssociateMouseAndMouseCursorPosition (or CGDisplayHideCursor with warping), so that the pointer
stays put on the dial instead of travelling away from it and eventually running out of screen.
cursor_capture()/cursor_release() are NOT here — they need NSCursor, so they live in g2View.m
with the rest of this shell's Cocoa. See there for how the pointer is hidden and put back.

## 24. `g2_input_key()`

── Keyboard ────────────────────────────────────────────────────────────────────────────────────

The shell decodes, the shared code acts — the same split as the modifier seam. g2View.m hands over
a character it took from -charactersIgnoringModifiers (so this sees the key's unshifted meaning, and
'+' and '=' are both worth accepting) plus whether Command was down.

UNTIL NOW THIS VIEW RECEIVED NO KEY EVENTS AT ALL: NSView's -acceptsFirstResponder is NO by default
and nothing had overridden it, so neither -keyDown: nor -flagsChanged: was ever called. Both are
live now, which is also what makes a modifier pressed mid-drag — with no mouse movement to carry it
— register at all.

Returns true if the key was used, so the view can leave it alone otherwise and let the host have it.
That matters: a host owns shortcuts like the space bar for transport, and a plug-in that swallowed
everything would be worse than one that swallowed nothing.

## 25. `glfw_key_for_mac()`

── Keys for SynthLib's popups ──────────────────────────────────────────────────────────────────

THE FILE BROWSER COULD NOT BE TYPED INTO and Escape closed nothing: the application hands every
key and character to the popup coordinator, and the plug-in never did, so File > Save As had a
filename box that ignored the keyboard. The popups speak GLFW's key codes, so the handful they
use are translated from macOS's virtual key codes (Carbon's kVK_* values, written out so this
file needs no Carbon header).
