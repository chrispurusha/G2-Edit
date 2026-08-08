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
renderer is fixed-function (`glOrtho`, `glBegin`, `glVertex`), so a core-profile context would draw
nothing, silently.

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

**ROTARY DIAL MODE ONLY in the plug-in, and this is load-bearing.** Rotary reads an absolute angle
each event, so passing the canvas coordinate as the "raw" one is harmless. Vertical and horizontal
modes difference raw cursor deltas AND depend on `start_cursor_drag()` hiding and warping the
cursor — a platform capability the plug-in does not have, which is why `synthlib_dial_mode()`
reports rotary there. Wiring the other two modes means giving the plug-in real cursor control first.

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
Retina display. What is missing is `start_cursor_drag()`'s cursor hiding and warping: the pointer
visibly travels away from the dial rather than staying put. Offered anyway, with rotary the default.

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

**3. `cursor_pos()` never used its `GLFWwindow *` argument.**
It read the pointer through `get_global_gui_scaled_mouse_coord()`. The signature implied a GLFW
dependency the body did not have, which is the main reason the extraction looked daunting and turned
out to be mechanical. Worth checking other signatures for the same lie.

**4. `start_cursor_drag()` conflates logic with platform.**
It records the drag origin AND hides/warps the cursor. Splitting it — `canvas_drag_set_origin()`
plus an optional `cursor_capture()`/`cursor_release()` — makes the platform half genuinely optional
instead of something a port can drop by accident, which is exactly what happened here.

**5. Modifier predicates are duplicated.**
The four-key Shift-or-Cmd multi-select test appeared four times (now one:
`multi_select_modifier_held()`); a Shift-only variant appears twice more; `mutatorUI.c` keeps private
`shift_held()`/`cmd_held()`. Three predicates, seven-plus copies.

**6. `glfwGetKey` is a PULL api, used 25 times.**
A plug-in only receives pushed events. An input-state struct updated by the shell would serve both,
and is what the plug-in needs before vertical/horizontal dial modes can behave exactly as they do in
the application.

**7. What is already right, and should not be "improved".**
The click-region registry. Every widget registers as it is drawn and `dispatch_click_region()`
resolves a coordinate against it, with press-capture and layer priority. That design is why
hit-testing needed NO work whatsoever to serve the plug-in — the single largest saving in this whole
exercise. Note the application's own comment that `handle_morph_press()`/`handle_module_press()` are
"pure legacy fallback... should be nothing today"; those are candidates for removal.

**Shape a shared API might take**, if it is ever worth doing:

    canvas_press(coord, button, modifiers)     // returns true if consumed
    canvas_motion(coord, rawCoord)
    canvas_release(coord, button, modifiers)
    canvas_modifiers()                          // replaces glfwGetKey polling, set by the shell
    cursor_capture() / cursor_release()         // optional; absent in a plug-in

with the application adding its topbar, scrollbar and panel handling around those calls, exactly as
it already wraps the extracted pieces today.
