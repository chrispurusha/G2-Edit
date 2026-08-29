# Getting the editor canvas into a plug-in window

Reference notes, not a plan. The question behind all of it: G2-Edit draws through GLFW, which
creates and owns its window, and a VST3 host instead hands the plug-in an `NSView` it owns. What
would it take for the application's renderer to draw there?

Mechanisms below were read out of [JUCE](https://github.com/juce-framework/JUCE) — as a reference
only; nothing is copied and no dependency is taken. JUCE is GPLv3-or-commercial and this project is
GPLv3, so borrowing code *would* be compatible, but none of this needs it.

---

## What the codebase actually looks like

Measured, not estimated — the file count is misleading in both directions.

**All OpenGL drawing goes through one file: `SynthLib/src/utilsGraphics.c`.** Nothing else in either
`src/` or `SynthLib/src/` makes a raw GL call. The renderer is, in effect, already separated.

**~300 GLFW references across 26 files in `src/`, but roughly 180 are `GLFW_KEY_*` / `GLFW_MOD_*` /
`GLFW_MOUSE_BUTTON_*` constants.** That is enum leakage, not coupling: define an internal key/button
enum and translate once at the boundary. The genuinely platform-bound API surface is small:

| Call | Count | Notes |
|---|---|---|
| `glfwGetKey` | 25 | A **pull** API. A plug-in only receives pushed events, so this needs an input-state object either way. The awkward one. |
| `glfwGetTime` | 15 | Not windowing at all — any monotonic clock replaces it. |
| `glfwWaitEvents` / `WaitEventsTimeout` | 13 | The run loop. See below. |
| window create / size / pos / hints / callbacks | ~20 | Effectively confined to `graphics.c`. |
| `glfwGetFramebufferSize`, content scale | ~8 | On macOS the view answers this; see the scaling note. |

**`synthlibScale.c` is nearly platform-free already.** `synthlib_scale_update(width, height)` takes
pixel dimensions and does the `glViewport` + `glOrtho` with no GLFW involvement. Only
`synthlib_scale_query_initial()` and `synthlib_scale_set_content_scale()` call GLFW, and both only to
*ask* for a scale factor. Splitting those two out is a concrete, small first refactor step — it is
the reason `g2GlDraw.c` currently repeats four lines of projection set-up rather than calling the
shared function.

**Fonts load from an absolute system path** (`/System/Library/Fonts/Supplemental/Arial.ttf`, in
`graphics.c`), not from the app bundle. That removes the usual plug-in resource-loading problem
entirely — a plug-in has a different bundle, but this path does not care.

---

## Mechanisms worth borrowing

### Attachment (macOS)

The whole story: the host calls `attached(void* parent, kPlatformTypeNSView)`; cast to `NSView *`,
retain it, add your own view as a subview. `removed()` reverses it. `g2Editor.mm` already does this
and has since the AppKit panel was written — **attachment was never the hard part.**

Host quirks JUCE has already paid for, worth knowing before hitting them:

- **WaveLab** hands over a zero-height parent view. JUCE detects `frame.size.height ≈ 0` and
  repositions to the origin, plus a compatibility timer.
- **Cubase 10** needs an async peer update inside `onSize()`.
- **Ableton** — our test host — fails `getSize()` if the system window is not yet set.

### The GL surface

JUCE subclasses **`NSOpenGLView`**. Not `CAOpenGLLayer`, not a layer-backed `NSView`. Those are what
people reach for after hitting trouble, and starting there is a poor way to discover whether there is
any. `setWantsBestResolutionOpenGLSurface: YES` gives a retina-resolution surface.

**Do not add an `NSViewGlobalFrameDidChangeNotification` observer.** JUCE has one because it attaches
a context to a *bare* `NSView`. Subclassing `NSOpenGLView` already covers it — the compiler says so
outright, deprecating that notification with "Use NSOpenGLView instead". Adding it back only doubles
the `-update` calls. If a host ever resizes the view without the surface following, *that* is when to
move to a bare `NSView` with a hand-managed `NSOpenGLContext` and this notification.

**Use the legacy (compatibility) profile** — no `NSOpenGLProfileVersion3_2Core` attribute. The whole
renderer is fixed-function (`glOrtho`, client-side vertex arrays, `GL_TEXTURE_2D` with no shader
behind it), so a core-profile context would draw nothing, silently. Immediate mode itself is gone —
`utilsGraphics.c` batches geometry into one `glDrawArrays` since 2026-08-28 — but batching is a step
towards a Metal backend, not away from the compatibility profile, and this attribute stays until
that backend exists.

### Scaling

VST3's `setContentScaleFactor()` **returns false on macOS** in JUCE — the platform has no host-supplied
scale to plumb through. `[view convertRectToBacking:]` is the entire answer, which is a meaningful
simplification versus the Windows path.

### Threading

The context is made current only on the thread that renders, wrapped in
`CGLLockContext`/`CGLUnlockContext`, with UI state read under a *separate* lock. That defined lock
order is the part worth imitating exactly, because G2-Edit already has two threads and the USB thread
already drives redraws through `register_glfw_wake_cb`.

### Resize negotiation

Host → plug-in is `onSize()`. Plug-in → host is `plugFrame->resizeView()`, gated by `canResize()` and
`checkSizeConstraint()` for min/max and aspect ratio. Our editor is currently fixed-size, so this is
genuinely new work rather than a port.

---

## The one thing NOT to copy: JUCE's render thread

JUCE drives repaints from a dedicated render thread, woken on macOS by `CVDisplayLink`
(`PerScreenDisplayLinks`) rather than letting `swapBuffers` block, with atomic flags coalescing
repaint requests and a `waitForWork()` idle.

**That machinery exists because JUCE must support continuously animating GL at display rate.
G2-Edit is event-driven** — it redraws when something changes, which is exactly why `graphics.c`
uses `glfwWaitEvents`/`WaitEventsTimeout` and why the USB thread pokes it awake. The natural
analogue here is far simpler: draw in `drawRect:`, and turn the wake callback into
`setNeedsDisplay:` marshalled to the main thread. No render thread, no display link, no coalescing
flags.

The temptation with a reference implementation is to copy its structure. JUCE's structure encodes a
requirement this project does not have.

---

## Shape of the eventual refactor

Not a symmetric "platform abstraction" with GLFW and Cocoa as peers — they are not peers. GLFW gives
you window + context + input + **the run loop**; VST3 gives you a view and **takes the run loop
away**. A symmetric interface forces the Cocoa side to fake things it has no business owning.

Three layers instead:

1. **Renderer** — already done, `utilsGraphics.c`, one file.
2. **Input and hit-test state** — mostly platform-free already, once the key constants are ours.
3. **Shell** — owns a GL context and a surface, and *pushes* events in.

The load-bearing change is that `do_graphics_loop()` becomes `render_one_frame()` that somebody else
calls. GLFW stays as the application's shell; it is doing real work (DPI, clipboard, input, window
management) that would otherwise have to be rewritten in Cocoa for no gain.

**A third beneficiary:** a shell that is not GLFW can also be an offscreen framebuffer, which would
make the renderer headlessly testable the way the sound engine now is. Given the connector-array bug
turned entirely on "the app draws, so it worked", that is not a small side benefit.

**A cost:** the boundary lands in SynthLib, shared by G2-Edit, SynthEdit and EmuUtility. That is
leverage, but the change gets reviewed and committed in that repo rather than this one.

---

## What has been built so far

The spike, in `g2GlView.m` (the surface) and `g2GlDraw.c` (the pixels), wired into `g2Editor.mm` as a
200pt strip below the existing controls. It answers only the question that decides everything else:
**will an OpenGL context attached to a host-owned NSView draw at all.**

Verified in a standalone test host that mimics a host's relationship to the view — it owns a window
and the view is added as a subview. Result: correct viewport, corner markers in the right corners,
diagonals meeting exactly at centre, and 1-pixel lines crisp on a 2× surface, confirming the
backing-scale path.

**Confirmed in Ableton Live**, 2026-08-08 — the strip renders correctly below the controls, alongside
Live's own drawing, with no host complaint.

**The application's renderer then replaced the raw GL**, same day. `g2GlDraw.c` now draws through
`render_rectangle_with_border()`, `render_text()`, `set_rgb_colour()` and `get_text_width()` — the
same SynthLib calls the editor canvas is built from. Text, borders, colours and text *measurement*
all work unchanged. Nothing in SynthLib had to be modified to achieve it.

What that took, and it was less than expected:

- `utilsGraphics.c` compiled into the plug-in as-is. It `#include`s `<GLFW/glfw3.h>` for the GL
  declarations but makes **no GLFW call**, so it needs the include path and not the library.
- `geometry.c` for `gGlobalGuiScale`, and the bundled static `libfreetype.a` for text. Nothing else.
- `synthlibScale.c` was avoided entirely by setting `set_render_width/height` and `gGlobalGuiScale`
  directly, so the split described below is still pending rather than done.
- `configure_synthlib_theme()` must be called before anything draws, and `preload_glyph_textures()`
  must be called **with the context current** — it builds GL textures, and without a context it
  fails silently into a font that draws nothing.
- `topBarHeight` is set to 0 here, deliberately, not to the application's value: it tells the
  renderer how much of the window the top bar and menu occupy, and this strip has neither.

**The actual patch canvas followed**, same day. `g2GlDraw.c` now calls `render_modules()` and
`render_cables()` — the application's own canvas functions — against the module database that
`g2Patch.c` fills when the plug-in loads its `.pch2`. Modules, dials, values, response curves,
envelope graphs, connectors and cables all draw correctly. The AppKit slider panel is gone; the
whole window is canvas.

What that took:

- **One seam in `src/`, and it was already duplicated.** `moduleGraphics.c`'s only GLFW use was a
  four-key "is Shift or Cmd held" test, written out twice there and twice more in `mouseHandle.c`.
  Extracted as `multi_select_modifier_held()` (declared in `mouseHandle.h`, answered from GLFW by
  the app and `false` by the plug-in). Four duplicate sites collapsed to one, and `moduleGraphics.c`
  now contains no GLFW call at all.
- **`vst3/g2AppStubs.c`** — thirteen functions, and worth reading as a measurement rather than a
  workaround. It is the complete list of what stands between the canvas renderer and a build with no
  application around it, in three groups: mouse position, editing, and two pieces of ambient state.
  They exist because `moduleGraphics.c` and `renderParams.c` hold the canvas CLICK HANDLERS in the
  same translation units as the drawing — the handlers never run here, but their references must
  resolve.
- Additional sources: `renderParams.c`, `selection.c`, `splitView.c`, `clickRegion.c`. No change to
  SynthLib.
- `clear_click_regions()` must be called each frame: the renderer registers a region per module,
  dial and connector as it draws, and nothing consumes them here.

Known limits of what is drawn:

- **Voice Area only, slot 0, no scroll or zoom.** A patch larger than the window is clipped rather
  than scrollable, and the FX area needs the pane machinery.
- **Nothing is interactive.** No mouse, so no dial drags, no selection, no context menus.
- **Nothing repaints on change.** `synthlib_request_redraw()` is a stub; the view redraws only when
  the host asks it to.

**Repaint-on-change and mouse CLICKS work**, same day. Verified by clicking a module in a host
window and watching it acquire the selection border and repaint.

- `synthlib_request_redraw()` is no longer a stub. It calls `g2_gl_view_request_redraw()`, which
  hops to the main thread (`dispatch_async`, never `_sync` — a sync hop from a thread the main
  thread waits on is a deadlock) and marks the view dirty. Every part of the editor that changes
  something already calls this, so one function made the whole canvas repaint on change.
- `vst3/g2Input.c` turns a position into a `dispatch_click_region()` call. **The hit-testing was not
  reimplemented** — every clickable thing already registers a click region as it is drawn, and
  SynthLib's dispatcher is already platform-free, so press-capture and layer priority came for free.
  The only thing missing had been somebody to say where the mouse is.
- `convert_mouse_coord_to_module_area_coord()` moved out of `mouseHandle.c` into
  `src/canvasCoords.c`. It was always pure arithmetic — `module_area()`, the scroll offsets, the
  zoom factor — with no window in it, so both the app and the plug-in now share one copy.

**DIAL DRAGS DO NOT WORK YET, and the reason is structural.** A press on a dial dispatches correctly
and sets up `gParamDragging`, but the code that turns subsequent motion into a value change lives in
`cursor_pos(GLFWwindow *, double, double)` in `mouseHandle.c` — a GLFW callback the plug-in cannot
link. So the drag begins and nothing consumes it.

That function is the next extraction, and it is a bigger one than the two done so far: it is long and
handles every drag the editor has (params, modes, modules, cables, tempo, vibrato, glide) mixed
together with GLFW's cursor warping and hiding. The param-drag arm is the piece worth pulling out
first, into something platform-free that takes a position rather than reading one.

**Module dragging and deselect-on-empty-click work**, 2026-08-08, via a third extraction:
`src/canvasDrag.c`. It holds the module-drag motion, the rubber band, the empty-canvas press and the
rubber-band release — all lifted out of `cursor_pos()` and `mouseHandle.c`'s press/release paths,
unchanged in behaviour. The only difference is that the coordinate arrives as an argument instead of
being read from GLFW; `cursor_pos()` never used its `GLFWwindow *` argument in the first place.

`convert_mouse_coord_to_module_column_row()` moved from `menus.c` to `canvasCoords.c` to join its
sibling — the module drag needs it, and like the other it is arithmetic with no window in it.

Note how "empty canvas" is detected: `dispatch_click_region()` returning false. Every occupied part
of the canvas registers a region as it draws, so a press nothing claims IS a press on bare canvas.
That is the application's own test, not a new one.

**Re-ordering on drop works too.** `shift_modules_down()` moved from `menus.c` into `selection.c`,
directly above `shift_selection_down()` — the code already described them as siblings, and
`selection.c` was already linked into the plug-in. `canvas_module_drag_release()` now calls whichever
applies, so a module dropped on another pushes it down its column exactly as the application does.
`send_module_move_msg()` inside them reaches the stubbed `msg_send()` and does nothing, which is
correct: there is no synth attached.

Dragging is restricted to the module's top name bar in the plug-in, as in the application — verified
by dragging from the body and confirming the module selects but does not move. That came for free:
`drag_area_click_handler` is registered on `module->dragArea` (the title strip) and is the only thing
that sets `gModuleDrag`, and the plug-in shares it.

Still not carried over: the move is not recorded for **undo**. The application builds a
`tUndoMoveEntry` on release; a plug-in has no undo stack, so that stayed behind.

**Dial dragging works**, 2026-08-08. The parameter-drag arm of `cursor_pos()` — 173 lines — moved
into `canvasDrag.c` as `canvas_param_drag_motion()`, unchanged in behaviour. It learns from arguments
what it used to read from GLFW: the canvas coordinate, the raw cursor position, and whether Alt is
held (Alt drags the morph OFFSET rather than the value).

The drag reference points (`gDragStartX/Y`, `gDragPrevX/Y`) moved to `globalVars.c` rather than into
`canvasDrag.c`, because `cursor_pos()`'s remaining arms — tempo, vibrato, glide — difference against
the same two points and stayed behind.

**~~ROTARY DIAL MODE ONLY in the plug-in~~ — WRONG, and superseded below.** The claim was that
vertical and horizontal modes need real cursor deltas and `start_cursor_drag()`'s hiding and warping,
so the plug-in had to report rotary. It does not: `cursor_raw_coord()` records the origin from the
same `gMouse` the motion path differences against, so the incremental modes are self-consistent in
canvas coordinates and work. All three modes are selectable from the plug-in's own Controls menu and
persist in its own prefs; rotary is only the FALLBACK DEFAULT in `g2Prefs.c` when the pref is absent.
What is actually missing without `cursor_capture()` is pointer hiding and confinement — see the
`View` paragraph further down, which had this right all along. This note is left standing rather than
deleted because the wrong version was quoted back at the owner more than once.

A BUG WORTH REMEMBERING, found when module re-ordering silently did nothing: **a press CAPTURES its
click region** (clickRegion.h), so `dispatch_click_region()` returns true on the matching RELEASE
too. The plug-in's input path was returning early on that, so the drag-end path — which does the
re-ordering — was never reached, and `gModuleDrag` stayed active into the next gesture. The release
must run both the captured handler and the drag-end work; only the press may return early.

**Menu bar, File → Open Patch File…, and reserved topbar space**, 2026-08-08.

SynthLib's `contextMenu.c`, `menuBar.c` and `synthlibHost.c` now link into the plug-in. That needed
almost nothing: `wake_glfw()` turned out to be the application's own one-line wrapper around
`synthlib_request_redraw()` (all 16 call sites resolve from one line here), and `glfwGetTime()` in
`contextMenu.c` — its ONLY real GLFW reference, a submenu hover delay — became
`clock_gettime(CLOCK_MONOTONIC)`. **That is a SynthLib change**, internal with no API impact, and it
removes a GLFW dependency, so SynthEdit and EmuUtility only gain by it.

`vst3/g2Menu.c` is a SMALLER menu than `appMenuBar.c`, deliberately. Five of the application's eight
menus (Settings, Backup, Restore, Controls, Experimental) exist to talk to a G2 over USB; porting
them would produce entries that either do nothing or look as though they might. The machinery is
shared; only the contents are the plug-in's.

`File → Open Patch File…` is the one that changes what the plug-in IS — until now it played a patch
compiled into the binary at build time. `vst3/g2FileDialog.m` is a small NSOpenPanel returning a
PATH; `misc.mm`'s `file_menu_open_patch()` was not reused because it also loads, through a loader
that carries the online branch and pulls in GLFW, and `g2Patch.c` already has the plug-in's loader.

A BUG THAT ONLY THE INCREMENTAL MODES COULD SHOW: `start_cursor_drag()` was left as an empty stub,
so `canvas_drag_set_origin()` never ran and `gDragPrevY` stayed at 0. Vertical and horizontal compute
`value + (previousY - currentY) * range / 200`, so the first movement evaluated `(0 - currentY)` — a
large negative — and drove every dial straight to zero. Rotary hid it completely by reading an
absolute angle and never touching those variables. The plug-in now implements `start_cursor_drag()`
in `g2Input.c` (it needs the mouse); it still cannot hide or warp the cursor, but recording the
origin was never the optional part.

`View` offers all three dial-drag modes. Vertical and horizontal work here because they difference
the pointer against its previous position, and canvas coordinates serve for that — they are simply
scaled in points rather than backing pixels, so a little less sensitive than the application's on a
Retina display. Rotary is the fallback default, but the choice persists in the plug-in's own prefs.

**THE POINTER IS HIDDEN FOR A DRAG NOW** (2026-08-09), which this paragraph previously said was
missing. `cursor_capture()`/`cursor_release()` in `g2GlView.m` hide it on a capturing drag and warp it
back to where the drag started on release — without the warp it would reappear wherever the physical
mouse had wandered to, which is most of the way across a screen after a long drag.

`[NSCursor hide]` is process-wide and reference-counted, so an unbalanced hide leaves the HOST without
a pointer. Hence a flag rather than trusting call pairing, a direct release on mouse-up, a poll in
`g2_input_drag_tick()` for the mouse-up that never arrives, and a release in `-removeFromSuperview`
for an editor closed mid-drag.

STILL NOT CONFINED: a long drag can run the physical mouse off the edge of the screen and the value
stops following. That needs `CGAssociateMouseAndMouseCursorPosition(false)` **and** feeding the drag
from `-deltaX`/`-deltaY` rather than absolute positions, since decoupling freezes the absolute values
the incremental modes currently difference. A change to how motion reaches the canvas, not one more
line in the view.

**Topbar space is RESERVED but empty** — `G2_PLUGIN_TOPBAR_HEIGHT`, a strip below the menu bar
showing the loaded patch name. Reserved now so the canvas is laid out around it from the start;
adding the controls later would otherwise shift the whole patch down. It is 44 rather than the
application's 80 because the slot, performance and clock controls that fill much of the app's bar
have no meaning in a plug-in — the host owns tempo, and a plug-in instance is one patch, not four
slots. Worth having when built: variations ×8 + Init, patch name, mono/poly, voice count, patch
volume, and the cable hide/transparent/colour toggles.

**Module drop-downs work**, 2026-08-08 — Wave selectors, Curve, Pad, slope menus and every other
toggle/menu parameter. `menus.c` (2577 lines) linked in as-is: it has **zero** GLFW calls, and once
SynthLib's context-menu system was linkable it needed only eleven more `undo_*` stubs plus
`midi_input_last_cc()`. `open_toggle_menu()`, `open_mode_toggle_menu()` and `find_unique_module_id()`
stopped being stubs at that point — `menus.c` defines all three, so linking it replaced them.

**Right-click menus and menu hover highlighting**, 2026-08-08 — two unrelated causes that presented
together.

*Right-click* was simply never handled: the view implemented only the left button. The canvas half of
`mouseHandle.c`'s `mouseButtonRightUp` case moved into `canvasDrag.c` as `canvas_right_click()`. Its
hit-test ORDER matters and is preserved — connectors, then parameters, then module body, then morph
labels — because a connector sits inside its module's rectangle, so testing the body first would
swallow every connector right-click.

*Hover highlighting* needed *two* things, and either alone would have looked like a failure:

1. **An `NSTrackingArea`.** AppKit does not deliver `-mouseMoved:` to a view without one, so the
   pointer position was only ever updated on a click, and `render_context_menu()` draws its highlight
   from that position. The area must be rebuilt on resize (`-updateTrackingAreas`).
2. **Polling the hover updaters.** `update_context_menu_hover()` and `update_menu_bar_hover()` are
   called every tick in the application (`graphics.c:2886`) — the dwell timer that opens a submenu
   flyout only advances when something asks it to. The plug-in calls them on pointer movement, which
   is enough given it redraws on demand rather than continuously.

**Cable dragging works**, 2026-08-08 — the last big piece of `cursor_pos()` the canvas needed.

The press already worked: `connector_click_handler` in `moduleGraphics.c` sets `gCableDrag`, and the
plug-in links that file. What moved out of `mouseHandle.c` into `canvasDrag.c` was
`handle_cable_connect()` and its three helpers (`set_up_cable_key`, `swap_cable_to_from_if_needed`,
`input_connector_has_cable`), plus the loose-end motion added to `canvas_drag_motion()`. The in-flight
cable is drawn in `g2GlDraw.c` after the settled ones, as `render_frame()` does — without it a drag is
invisible until it lands, which reads as nothing happening.

`msg_send()` inside the connect tells the G2 about the new cable; here it reaches a stub and does
nothing, which is correct — the cable exists locally, the engine picks it up from the database, and no
hardware is written to.

**Overall scaling and a resizable window**, 2026-08-08 — and the scaling was a BUG, not a feature.

The application lays its whole UI out in a fixed logical canvas `TARGET_FRAME_BUFF_WIDTH / 2` = 1280
units wide, with `gGlobalGuiScale` mapping that onto however many physical pixels exist. The plug-in
was setting `gGlobalGuiScale = backingScale`, which quietly redefined the logical canvas as 900 units
— about 70% of the application's field of view. That is why patches ran off the edge: not because the
window was small, but because the canvas was mis-sized. Adopting the application's formula makes the
whole of SimpleLead fit in the same window.

TWO CONSEQUENCES worth knowing:

- **Canvas coordinates are LOGICAL UNITS, not points** (~1.42 per point at a 900pt window). Everything
  the renderer draws is in them, including `MENU_BAR_HEIGHT`, so the chrome scales with the canvas
  exactly as it does in the application.
- **Mouse input had to follow.** `g2GlView.m` now hands over PHYSICAL PIXELS — it is authoritative
  about Cocoa's origin and its own backing scale, and nothing else — and `g2Input.c` divides by
  `gGlobalGuiScale`, which is the same conversion `get_global_gui_scaled_mouse_coord()` performs in
  the application. The two were only equal while the logical canvas was mis-sized.

The editor is now resizable, **with the ASPECT RATIO LOCKED to 2560:1440** — the same ratio the
application pins its window to via `glfwSetWindowAspectRatio(TARGET_FRAME_BUFF_WIDTH,
TARGET_FRAME_BUFF_HEIGHT)` in `init_graphics()`.

THE LOCK IS WHAT COMPLETES THE SCALING, and missing it was a real bug. `gGlobalGuiScale` comes from
WIDTH alone, so a taller window on its own merely uncovers more rows rather than drawing the patch
larger. The application never exhibits that because its window cannot be made taller without also
becoming wider. Free-resizing the plug-in let it do exactly what the application is prevented from
doing. `checkSizeConstraint()` now treats width as the authority and derives height from it.

CONSEQUENCE WORTH KNOWING: the visible canvas is now exactly the application's 1280x720 logical
space at every window size. A patch taller than that needs SCROLLING to reach the rest — which the
plug-in does not have. Before the lock, a tall window could reveal those rows by accident. So
scrollbars/zoom move back up the list of things worth having.

`onSize()` itself is still **UNTESTED locally** — the test host drives the view directly rather than
through `IPlugView`, so it is never exercised there.

An alternative, NOT implemented and noted only so the option is on record: scale from whichever axis
is tighter (uniform fit, letterboxing a wide-short window), or scale to the bounding box of the
modules that actually exist (zoom-to-fit, so any patch fills the window). Both would diverge from the
application, which is why neither was chosen.

**VA/FX panes, the split bar and scrollbars**, 2026-08-08 — all three at once, because they are all
in `splitView.c`, which the plug-in was ALREADY linking. This was wiring, not porting.

The draw now mirrors `render_frame()`'s pane loop exactly: `split_view_apply()`, then per pane
`set_module_pane()` / set `gLocation` / `module_pane_clip_begin()` / `render_modules()` /
`render_cables()` / `module_pane_clip_end()`, then `render_split_bar()` and
`render_pane_scrollbars()`. The FX area was entirely invisible before this.

Input goes to the chrome BEFORE the canvas, in both press and drag: a drag that began on a scrollbar
or the split bar must not be handed to whatever module lies underneath. `split_view_focus_at()` on
press makes the clicked half the focused one. Scroll wheel scrolls the pane UNDER THE POINTER rather
than the focused one, via `split_view_pane_at()` — which is what makes a two-pane view feel right.

Verified: both panes and the split bar render; dragging the vertical scrollbar scrolls the pane.
NOT verified — the scroll WHEEL (cliclick has no scroll command) and the split-bar DRAG.

**Auto-scroll while dragging**, 2026-08-08. `adjust_scroll_for_drag()` moved from `mouseHandle.c`
into `canvasDrag.c` — it was left behind on the first pass precisely because a plug-in with no
scrollbars had nothing to scroll, and that is no longer true. Nothing in it was ever platform-bound:
`get_time_ms()` is SynthLib's and the rest is the pane machinery.

The rate is the application's and needs no tuning: it RAMPS from `DRAG_SCROLL_MIN_RATE` (120 content
px/sec, just past the edge) to `DRAG_SCROLL_MAX_RATE` (1200) across `DRAG_SCROLL_RAMP_DIST` of
overshoot, so easing over the boundary creeps and pushing well past it moves quickly.

THE TICK MATTERS AS MUCH AS THE FUNCTION. Auto-scroll only advances when something calls it, so a
pointer held still just past the edge would stop scrolling. The application synthesises a
`cursor_pos()` call from its main loop while a drag is active — `graphics.c`, commented "Artificially
do cursor_pos call for drag scrolling when cursor not moving". The plug-in has no main loop, so the
view runs a 60 Hz `NSTimer` for the duration of a drag, started on press and invalidated on release
(and on `removeFromSuperview`, since the block retains the view). Only module and cable drags get it:
a rubber band selects what is already visible and a dial drag is not going anywhere.

**Remembered settings: dial mode and editor size**, 2026-08-08, through the same SynthLib prefs
store the application uses (`prefs.cpp` — a plain file under Application Support, and zero GLFW).

`prefs_init("G2 Alike")`, NOT `"G2-Edit"`. A shared dial-mode preference between app and plug-in
would be a nice touch, but `prefs.cpp` rewrites the whole file on a change and the standalone editor
and a hosted plug-in are very likely to be open at once — last-writer-wins would quietly lose
settings. Separate files are worth more than the shared setting.

Only the editor WIDTH is stored; height is derived from the locked aspect ratio, so storing both
would record the same fact twice and invite them to disagree. It is restored in the view's
CONSTRUCTOR because `getSize()` is asked before `attached()`, and written on every `onSize()` rather
than at close — a host is under no obligation to announce teardown in any particular order.

**WINDOW POSITION CANNOT BE REMEMBERED, and this is not a gap to fill later.** VST3 gives a plug-in
no way to place its own editor window: the host creates and positions it, and `IPlugView` only ever
negotiates SIZE (`getSize`/`onSize`/`checkSizeConstraint`). The application can call
`glfwSetWindowPos()` because it owns its window; a plug-in does not own its own. Whether the editor
reopens where it was is the host's behaviour to get right, not ours.

**The application's OWN top bar**, 2026-08-08 — `render_top_bar()` moved out of `graphics.c` into
`src/topbarRender.c` and is now drawn by both, along with `render_morph_groups()` (already in
`moduleGraphics.c`), `mouseTopbar.c` and `topbarResourcesAccess.c`.

A HAND-WRITTEN SUBSET WAS TRIED FIRST AND WAS THE WRONG CALL. It carried patch name, variations and
cable toggles, and looked nothing like the editor — which defeats the point of reusing the renderer
at all. The reasoning that led there was that `render_top_bar()` reads USB comms state; but "Offline"
is the TRUTHFUL state for a plug-in, and the TX/RX lamps simply stay dark. Nothing had to be hidden
to be honest, and a second implementation of the same bar could only drift from it.

`topbar_init_controls()` MUST BE CALLED. It copies each control's `defaultColour` out of the resource
table; without it every control draws with a zeroed `tRgb` — which is black — so Undo/Redo and the
A-D slot buttons came out as black rectangles.

**AN INCLUDE-ORDER BUG THAT WAS WORSE THAN IT LOOKED.** `defs.h` defines `G2_EDIT`, and
`synthlibDefs.h` gates `TOP_BAR_HEIGHT`, the colour palette and several layout constants on it. Four
plug-in files included `synthlibDefs.h` FIRST, so it expanded with `G2_EDIT` undefined and
`TOP_BAR_HEIGHT` became 0.0. That made `gTheme.topBarHeight` 24 instead of 104 — so the module band
started 80 units too high, most of it hidden behind the top bar, and the `MODULE_MARGIN` gap between
bar and canvas was swallowed. It presented only as "the gap is missing"; the patch was actually
positioned wrongly the whole time. Verified fixed by sampling the pixel column at the boundary
against the running application: background band now 384-396, matching the application's 385-398.

**The built-in patch is gone.** It was scaffolding from before the plug-in had an editor: with no way
to choose a file, embedding one removed a whole class of "why is it silent". `File > Open Patch
File...` replaced it, and a plug-in that quietly plays somebody else's lead patch is worse than one
that starts empty. `load_patch()` now uses the path chain that was always in `g2Patch.c`, and
`do-vst3` no longer generates `g2BuiltInPatch.h`.

**Empty-plug-in defaults**, 2026-08-08. `init_patch()` moved from `mouseHandle.c` to `dataBase.c` —
its own comment asked where it really belonged, nothing in it touches a window, and `clear_slot_data()`
came here for the same reason.

THE PANE DIVIDER'S POSITION IS PATCH DATA (`gPatchDescr[].barPosition`, see `splitView.h`). With no
patch loaded, `gPatchDescr` was all zeroes, and zero means "Voice Area takes no height" — so the
divider sat hard against the top of an empty window. The plug-in now calls `init_patch(0)` at
start-up, which sets the same 300 the application uses for a new patch, deliberately showing both
areas. A patch loaded afterwards carries its own value and overwrites it, as it does in the
application. It also fixes the empty plug-in showing a blank patch name and zero voice count.

**The application's menus**, 2026-08-08 — File, Settings, Controls, Tools, View. The plug-in composes
its bar from `appMenuBar.c`'s own menu functions, now exposed individually, rather than defining a
second set. Backup, Restore and Experimental are simply not in its bar.

`app_menu_set_device_capable(false)` OMITS the bank and device entries rather than greying them. The
application greys them while offline because going online is possible and a greyed row says "this
exists"; here it never is, and a permanently greyed row is worse than no row. Default true, so the
application is unchanged.

Getting it to LINK was the work, and the shape of it is worth recording — 41 undefined symbols,
resolved in four groups:

- **23 were `audio_output_*` / `midi_input_*`, ALL inside `open_experimental_menu()` and its private
  helpers.** Those are compiled out with `#ifndef G2_VST3_BUILD` — a compile-time guard rather than
  a runtime flag, because the linker needs every symbol in an object whether or not it runs.
- **Three real GLFW calls across every panel**, and no more: `glfwGetKey` in `mutatorUI.c` (its
  private `shift_held`/`cmd_held`, now the shared `shift_modifier_held()`/`cmd_modifier_held()` seam)
  and `glfwGetTime` in `virtualKeyboard.c`, `bankBrowser.cpp` and `fileBrowser.cpp` (now SynthLib's
  own `get_time_ms()`, which `contextMenu.c` was converted to as well so there is one clock).
  Everything else only used GLFW's key CONSTANTS, which need the header and not the library.
- **`draw_dialog_background_overlay()`** moved from `graphics.c` to `utilsGraphics.c`: six lines of
  drawing with no window in them.
- **The rest are genuinely absent capabilities** and are stubbed: `device_op_begin/end`,
  `save_zoom_factor`, and `is_cursor_hidden_dragging()` returning false (the plug-in cannot hide a
  cursor). `gMutator` and `param_overlay_note_param()` STOPPED being stubs — `mutatorUI.c` and
  `paramOverlay.c` are linked now, so the Mutator panel and parameter overlay are real.

**The custom file browser, and a bug it exposed**, 2026-08-08.

`File > Open Patch File...` did nothing at first, and the reason is worth knowing:
`file_menu_open_patch()` DOES NOT OPEN A BROWSER. It posts `eRspShowOpenRead` to `gToGuiThread` so
the browser opens from the render loop rather than from inside a menu callback (menuActions.c says
so). The plug-in's `msg_send()` was a no-op stub, so the message vanished.

`msg_send()` now DISCRIMINATES: `gToUsbThread` is still discarded — there is no synth, and a plug-in
must never write to one — while `gToGuiThread` is queued for the editor to drain in its draw, for
exactly the reason the application drains it there. One slot is enough; these are user actions.

That done, `open_file_browser_read()` is SynthLib's own browser, already linked, so the plug-in gets
the editor's file browser rather than an `NSOpenPanel`. `vst3/g2FileDialog.m` has been deleted. The
browser is modal: `file_browser_active()` is checked before anything else in the press path, as the
application checks it.

**A REAL BUG FOUND BY TESTING ZOOM, unrelated to zoom.** `g2_gl_draw_init()` runs when the editor
view is first created — AFTER the processor has loaded its patch. It called `init_patch(0)`
unconditionally, so OPENING THE EDITOR WIPED THE LOADED PATCH. Now guarded with
`slot_has_modules(0) == false`: defaults are for an empty plug-in, not for one that already has
something.

**Zoom already works** via View > Zoom In/Out/Reset — `set_zoom_factor()` is in `utilsGraphics.c` and
was linked all along. What is missing is the other two ways the application offers it: Cmd +/- (the
plug-in has no key handling) and Ctrl-scroll zoom-at-cursor (`scroll_event()` in mouseHandle.c),
plus persistence, since `save_zoom_factor()` is still a stub here.

**Topbar input**, 2026-08-08. The bar drew correctly but nothing on it responded, and the exception
was the giveaway: the MORPH DIALS worked while every button and dial beside them did not.

Morph dials register CLICK REGIONS (moduleGraphics.c), so `dispatch_click_region()` found them for
free. The rest of the topbar does not: `mouseTopbar.c` hit-tests those controls itself against the
rectangles `topbarResourcesAccess.c` holds, and nothing reaches them unless they are asked directly.
The file was linked but never called.

`handle_topbar_left_down/left_up/right_up()` are now called from the plug-in's press, release and
right-click paths, in the application's own order — straight after the menu bar and before the
scrollbars. Verified: Hide hides the cables, and Variation 3 switches to genuinely different
parameter values.

WORTH GENERALISING: "is it a click region, or does its own file hit-test it?" is the question to ask
of any chrome that draws but does not respond. Click regions come for free; everything else needs its
handler called explicitly.

**Submenu dwell**, 2026-08-08. A flyout opens on a HOVER TIMER, and that timer only advances when
`update_context_menu_hover()` is called. The plug-in was calling it on pointer MOVEMENT only, so a
submenu would not appear unless the mouse was kept jiggling on its parent item. The application polls
it every frame.

The drag tick timer now covers menus too: it runs while a drag OR an open menu needs advancing, and
stops itself when neither does. It is no longer stopped on mouse-up — a menu opened by that very
click stays open and still needs ticking.

THE SAME SHAPE AS THE AUTO-SCROLL FIX, and worth stating as a rule: anything the application drives
from its per-frame loop needs a tick here, because a plug-in has no loop. So far that is auto-scroll
and menu dwell; the next one that behaves oddly "only while the mouse is still" will be a third.

**Submenu dwell, second attempt** — the first fix was incomplete. The tick timer was only STARTED on
`mouseDown`, but a right-click context menu opens from `-rightMouseUp:`, and any menu opened after the
timer had already stopped itself had nothing driving it. `ensureTickTimer` is now called after EVERY
mouse event; the tick still stops itself once neither a drag nor an open menu needs it. Verified: the
"Set colour" flyout opens one second after a single move, with no further movement.

**Persistence via the application's own `persistence.c`**, which has ZERO GLFW calls and was simply
never linked. That gives the plug-in zoom, dial mode and the file browser's last folder on the same
keys the application uses (`zoomFactor`, `dialMode`, `fileBrowserLastDirectory`) rather than a second
set that could drift. `save_zoom_factor()` stopped being a stub as a result.

The complete set anything persists is: `zoomFactor` and `fileBrowserLastDirectory` (persistence.c),
plus `dialMode`, `windowWidth`, `windowX`, `windowY` (synthlibPersistence.c). The plug-in takes the
first three. `synthlibPersistence.c` itself is NOT linked — its only other job is restoring the window
with `glfwSetWindowSize()`/`glfwSetWindowPos()`, and a plug-in owns neither — so `g2Prefs.c` supplies
`synthlib_load_window_and_dial_mode()` doing the dial-mode half and dropping the window half. Editor
WIDTH is remembered separately through `getSize()`; position is not ours to control.

Remaining, in order:
2. Scroll and zoom, then the FX pane — needed before a patch bigger than the window is usable.
3. Right-click context menus: `open_toggle_menu()`/`open_mode_toggle_menu()` are stubs, and those
   need SynthLib's menu stack, which needs an event loop.
4. Split the two GLFW-calling functions out of `synthlibScale.c` (`synthlib_scale_query_initial`,
   `synthlib_scale_set_content_scale`) so the real `synthlib_scale_update()` can be called instead
   of repeating its four lines. Lands in the SynthLib repo and affects SynthEdit and EmuUtility when
   they next advance their pin — which is why it has been deferred rather than done in passing.

The language split in this folder is deliberate: `g2GlDraw.c` is plain C because pixels need no
runtime, `g2GlView.m` is plain Objective-C because `NSOpenGLView` needs one but nothing needs C++,
and `g2Editor.mm` is Objective-C++ only because `IPlugView` is a C++ vtable that has to hand over a
Cocoa view.

---

## What sharing this code has revealed about the application's mouse handling

Observations, NOT a plan — nothing here is proposed for implementation yet. They are recorded because
each one was learned by hitting it, and the evidence will otherwise be forgotten.

**1. A gesture's three phases live in three files, with nothing connecting them.**
A dial drag presses in `moduleGraphics.c` (sets `gParamDragging`, calls `start_cursor_drag()`), moves
in `cursor_pos()` in `mouseHandle.c`, and releases in `finish_param_drag()` reached from
`mouseButtonLeftUp`. Nothing declares that all three exist.

THIS CAUSED THREE BUGS IN ONE SESSION, all in the plug-in and all the same shape: module re-ordering
did nothing (release never reached), a dial stayed held after mouse-up (its release never reached),
and vertical dials slammed to zero (the press half of `start_cursor_drag()` was stubbed away). Each
was invisible until the feature was used. A gesture owning press/motion/release together would make
an unwired phase a compile-time or at least an obvious omission.

**2. `cursor_pos()` is a dispatch table written as an if/else chain.**
~450 lines over scrollbars, tempo, perf tempo, vibrato rate/amount, glide time, param drag, module
drag, cable drag, rubber band, context menu and connector hover — each arm independent, selected by
whichever `gXxxDragging` flag is set. With four arms now extracted, the rest reads as a list wanting
to be a table keyed on an active-gesture enum.

DONE IN PART 2026-08-09 — `cursor_pos()` is **207 → 104 lines**. Five arms turned out to be two
gestures written out repeatedly: tempo and perf-tempo, identical but for a rectangle; and vibrato
amount, vibrato rate and glide time, identical but for a module, a parameter index and a range. Those
are now `sTempoDragTargets` / `sPatchParamDragTargets` plus two helpers, so a sixth such dial is a
table row. Each entry points AT its rectangle, because those are filled at render time and a copy
taken at start-up would be stale. Verified live in rotary mode (the only mode whose drags survive
synthetic input, since the others hide the pointer): tempo 127→240 BPM, vibrato amount 26→69,
vibrato rate 6.02→7.97 Hz, glide 80→6270 ms, with the untouched dials staying put.

THE REST OF THE CHAIN IS NOT THIS. Scrollbar, module, cable and rubber-band drags do genuinely
different work; collapsing them needs observation 1, not another table.

A BUG THIS REFACTOR FLUSHED OUT, worth knowing because it was not caused by it: removing two
now-unused locals from `cursor_pos()` changed the stack frame, and `canvas_param_drag_motion()`
contained `bool altHeld = (altHeld);` — a local shadowing the parameter and initialised from itself,
present since eb26908. The garbage byte it read had been zero and became non-zero, so every plain dial
drag started writing the morph offset. `-Wuninitialized` had reported it in every build for months.
`OTHER_CFLAGS = "-Werror=uninitialized"` is now set in both configurations and verified to reject that
exact line; see todo.txt for what stands between here and a project-wide `-Werror`.

**3. `cursor_pos()` never used its `GLFWwindow *` argument.**
It read the pointer through `get_global_gui_scaled_mouse_coord()`. The signature implied a GLFW
dependency the body did not have, which is the main reason the extraction looked daunting and turned
out to be mechanical. Worth checking other signatures for the same lie.

DONE 2026-08-09, and the audit found exactly one more. Of the twelve functions taking a
`GLFWwindow *`, eleven are GLFW callbacks whose signature GLFW dictates — including `cursor_pos()`
itself, which now genuinely never reads it since the Alt poll became `alt_modifier_held()`. The
twelfth was `render_scrollbars(GLFWwindow *)`: not a callback, never read the window, called once as
`render_scrollbars((GLFWwindow *)synthlib_window())`, and the plug-in already draws its own bars.
Parameter removed.

**4. `start_cursor_drag()` conflates logic with platform.**
It records the drag origin AND hides/warps the cursor. Splitting it — `canvas_drag_set_origin()`
plus an optional `cursor_capture()`/`cursor_release()` — makes the platform half genuinely optional
instead of something a port can drop by accident, which is exactly what happened here.

DONE 2026-08-09, in the shape this note proposed. `start_cursor_drag()` is gone; the shared
`canvas_drag_begin()` (canvasDrag.c) records the origin and THEN calls the shell's `cursor_capture()`.
The shell supplies three functions: `cursor_raw_coord()` (GLFW's raw window coordinates in the
application, canvas coordinates in the plug-in — each only ever differenced against later positions
from the same source), `cursor_capture()` and `cursor_release()`. The plug-in's capture pair are now
DELIBERATE no-ops that cost it only pointer hiding, where before declining the platform half silently
discarded the origin as well. All eleven call sites go through `canvas_drag_begin()`, three of them in
`moduleGraphics.c`, which is the shared file that used to depend on each shell remembering the logic
half. Both targets build; the ordering is mechanical and equivalent, but a vertical-mode dial drag
(pointer hides, value moves, pointer returns) still wants an eye, since synthetic input cannot reach a
drag that hides the cursor.

**5. Modifier predicates are duplicated.**
The four-key Shift-or-Cmd multi-select test appeared four times (now one:
`multi_select_modifier_held()`); a Shift-only variant appears twice more; `mutatorUI.c` keeps private
`shift_held()`/`cmd_held()`. Three predicates, seven-plus copies.

DONE, and the last two copies went with lesson 6 below: the Shift-only variant survived as a written-out
`glfwGetKey()` pair at two sites in `mouse_button()` (empty-canvas press and rubber-band release) long
after the predicate existed. Both now call `shift_modifier_held()`.

**6. `glfwGetKey` is a PULL api, used 25 times.**
A plug-in only receives pushed events. An input-state struct updated by the shell would serve both,
and is what the plug-in needs before vertical/horizontal dial modes can behave exactly as they do in
the application.

DONE 2026-08-09 — `SynthLib/src/inputState.c`, and it turned out to be a DELETION rather than an
addition. **GLFW was already handing the application the answer and nobody read it:** `key_callback()`
and `mouse_button()` both take an `int mods` argument giving the modifier state at the moment of the
event, while three predicates polled `glfwGetKey()` for the same thing slightly later. Pushing the
argument is not just tidier, it is more correct — the poll answered "now", and "now" is after the
event was queued.

One writer, many readers. Each shell translates its own toolkit (`modifier_bits_from_glfw()` in
`mouseHandle.c`, `modifier_bits_from_ns()` in `g2GlView.m`) into `tModifierBits` and pushes; the
predicates read state and know nothing about windows. Results:
- `glfwGetKey` call sites in `src/`: **13 → 0.**
- The three plug-in stubs in `g2AppStubs.c` are **gone, not reimplemented** — Shift and Command now
  genuinely work in the plug-in (multi-select, Shift-drag on a mutator slider), where they had
  silently done nothing.
- Alt for morph dragging came off `glfwGetKey(window, ...)` too, which is what leaves `cursor_pos()`
  with a `GLFWwindow *` it now provably never uses (lesson 3).
- One correctness fix the poll could not have: `window_focus_callback()` clears the state on focus
  loss, because a modifier released while another application owns the keyboard is a release this
  process is never told about. A stuck modifier is worse than a missed one.

NOT done, and deliberately: `mutatorUI.c` still includes GLFW for `GLFW_KEY_*` in its keyboard
shortcuts. That is decoding a real key event, not polling for state, and it needs a shared key-code
enum rather than this seam. `moduleGraphics.c`'s GLFW include is now its only one and is load-bearing
for three raw `glEnable`/`glBlendFunc` calls (whose own TODO says they belong in the graphics
routines), not for anything GLFW.

**7. What is already right, and should not be "improved".**
The click-region registry. Every widget registers as it is drawn and `dispatch_click_region()`
resolves a coordinate against it, with press-capture and layer priority. That design is why
hit-testing needed NO work whatsoever to serve the plug-in — the single largest saving in this whole
exercise.

THE FALLBACKS ARE GONE, 2026-08-09 — `handle_morph_press()`, `handle_module_press()`,
`handle_morph_release()`, `handle_module_release()` and their two per-module helpers, 360 lines out of
`mouseHandle.c`. Removed on evidence rather than on the "should be nothing today" hunch that had kept
them:
- Every rectangle they hit-tested is registered as a click region by the same render pass that
  computes it — params (moduleGraphics.c:593), modes (622/664), connectors (820), body (1532), drag
  handle (1569). The census is complete, not a sample.
- Instrumented to log whenever either pair claimed a click: nothing, across 14 clicks over module
  bodies, dials, toggles, mode selectors, connectors, morph labels and the patch-settings panel.
- They were worse than dead. They tested the STORED rectangles, and for a module scrolled out of view
  those are stale ones left from wherever it was last drawn — so the only case in which they could
  still have fired is a case where firing would have been wrong. That is the same fault as the FX-pane
  hover bug fixed the same week.
`nudge_param_for_module()` and `nudge_param_under_cursor()` sat between them in the file and are live
keyboard-nudge code, which is why this was a brace-matched removal rather than a line range.

**Shape a shared API might take**, if it is ever worth doing:

    canvas_press(coord, button, modifiers)     // returns true if consumed
    canvas_motion(coord, rawCoord)
    canvas_release(coord, button, modifiers)
    canvas_modifiers()                          // replaces glfwGetKey polling, set by the shell
    cursor_capture() / cursor_release()         // optional; absent in a plug-in

with the application adding its topbar, scrollbar and panel handling around those calls, exactly as
it already wraps the extracted pieces today.
