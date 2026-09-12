# canvasDrag.h notes

The longer comments from `canvasDrag.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `canvas_drag_motion()`

Dragging on the module canvas, with no window system in it.

The press that STARTS a drag already lives in a click-region handler (moduleGraphics.c), which the
plug-in shares. What did not was the motion that carries it: that was buried in cursor_pos() in
mouseHandle.c, a GLFW callback. So a module drag in the plug-in began and then nothing moved,
because the code that moves it could not be linked.

These take a coordinate rather than reading one, which is the whole of what made them
unshareable — cursor_pos() itself never used its GLFWwindow argument.

NOT the whole of cursor_pos(): param dragging, cable dragging, tempo/vibrato/glide and connector
hover remain there. Those are the next pieces to move if the plug-in is to edit values as well as
move modules.

## 2. `tCanvasGesture`

── The canvas gestures, declared as a set ──────────────────────────────────────────────────────

A GESTURE HAS THREE PHASES AND NOTHING USED TO SAY SO. Press lives in a click-region handler, motion
and release live here, and each shell wired the phases up by hand — the application in
mouseHandle.c, the plug-in in g2Input.c. Two hand-maintained ladders for the same four gestures,
and the consequences were not theoretical (vst3/plugin-gui-notes.md, observation 1):

```
  - The plug-in never reached the module drag's release, so a dropped module was never re-ordered.
  - It never reached the dial drag's release, so a dial stayed held after mouse-up and the next
    click anywhere carried on turning it.
  - The application grew its OWN copy of the module-drag release, inline in its mouse-up handler,
    while canvas_module_drag_release() sat here used only by the plug-in — two implementations of
    one phase, free to drift.
  - The two shells even ran their release phases in different orders, and only one order could have
    been the considered one.

```
The table in canvasDrag.c now names all four gestures and their phases in one place, so a gesture
with a phase left unwired is a visible hole in a table row rather than a silence. A shell asks for
motion or release and the table decides who wants it.

## 3. `canvas_gesture_motion()`

Offers the motion to each gesture in turn and returns the one that took it, or canvasGestureNone.
The identity is returned rather than a bool because a shell may have work of its own to add — the
application auto-scrolls the canvas for a module or cable drag, which belongs to whoever owns the
scrollbars.

## 4. `canvas_gesture_release()`

Releases every gesture in `wanted` that is in progress, and returns the set that acted.

WHY A MASK RATHER THAN A SWEEP OF ALL FOUR: the application interleaves dispatch_click_region() in
the middle of its release handling — module and cable first, then the click regions, then the rubber
band — and that ordering is load-bearing for press-captured widgets. Rather than quietly changing
it, a shell says which gestures it wants released at this point. Passing canvasGestureAll is the
simple case and is what the plug-in does.

## 5. `canvas_drag_begin()`

── Starting a drag: the logic half is shared, the platform half is optional ─────────────────────

CALL THIS TO BEGIN ANY CURSOR-CAPTURING DRAG. It records the origin — which every incremental dial
mode depends on — and THEN asks the shell to capture the pointer. It replaced start_cursor_drag(),
which did both jobs in one function per shell, and that is the point rather than tidiness:

The application's version recorded the origin and hid the pointer. The plug-in's version was
briefly an empty stub, because "hide the pointer" is not something a host-owned NSView can simply
do — and stubbing it out silently took the ORIGIN with it, so vertical and horizontal dial drags
slammed to zero while rotary (which reads an absolute angle) looked perfect. See
vst3/plugin-gui-notes.md, observation 4. Splitting them means a shell can decline the platform half
and cannot drop the logic half by accident.

## 6. `cursor_raw_coord()`

Implemented by the SHELL, not here.

cursor_raw_coord() reports the pointer in whatever space that shell reports MOTION in — raw window
coordinates for GLFW, canvas coordinates for the plug-in. It only has to agree with itself: the
origin is only ever differenced against later positions from the same source.

cursor_capture()/cursor_release() hide and confine the pointer for the duration of a drag, so an
incremental drag is not limited by the edge of the screen. BOTH MAY BE NO-OPS — a plug-in in a host
window is entitled to decline, and declining now costs it only the pointer hiding.

## 7. `cable_touches_connector()`

The cable attached to a connector, and where its other end is — see the definition. Used by the
Ctrl-click pick-up, which needs to start a drag from the far end of an existing cable.
Is either end of this cable plugged into that hole? Shared with the renderer, which hides every
cable on a hole being dragged.
