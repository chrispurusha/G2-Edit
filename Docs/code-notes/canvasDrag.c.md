# canvasDrag.c notes

The longer comments from `canvasDrag.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `cable_drag_set_end()`

Where a cable's loose end goes for a pointer at coord. The single definition of it: the press, the
motion and the plug-in's own motion path all reach it here, and they did not previously agree.

THE ORDER MATTERS AND WAS WRONG. The half-connector offset centres the cable on the pointer rather
than hanging it from the connector square's top-left corner, and it is in MODULE SPACE — 8.75
units, off a fixed MODULE_WIDTH of 350. Subtracting it from the SCREEN coordinate first meant
convert_mouse_coord_to_module_area_coord() then divided it by the zoom factor along with
everything else, while render_cable_from_to() adds the same offset back unscaled. The two only
cancel at 100%: the end lands 8.75 * (zoom - 1) logical pixels from the pointer, trailing it when
zoomed out and leading it when zoomed in. Converting first and offsetting after keeps the offset
in the space it is expressed in.

## 2. `drag_whole_units()`

Whole parameter units for a pointer movement of `pixels`, carrying the sub-unit remainder over to
the next call — see tParamDragging::unitAccum for why discarding it made slow drags do nothing.

ONLY FOR THE INCREMENTAL FORM, where the reference point advances every event. An Alt (morph) drag
measures from the drag's fixed start instead, so its truncation loses nothing and it must NOT feed
this accumulator: adding an absolute displacement to a running total every event would race away.

## 3. in `module_drag_motion()`

Everything selected moves by the SAME delta, rather than each module jumping to the
pointer — otherwise a multiple selection collapses onto one square as soon as it moves.

AND THE DELTA IS CLAMPED ONCE FOR THE WHOLE GROUP, not each module against the grid
separately. Clamping per module was the bug (CT, 2026-08-30: "relative position of the
group to each other should remain the same. currently, individuals can reposition vs
the rest"): drag a selection at the left edge and the members already in column 0 stay
put while the rest keep moving, so the shape of the selection is permanently deformed.
The clamp belongs on the movement, not on the destination.

## 4. in `module_drag_motion()`

BY THE DELTA, exactly as the multi path above does, so the module keeps the offset
it was grabbed by. This used to assign the cursor's cell straight into
column/row, which put the module's TOP-LEFT under the pointer and made it jump the
moment a drag started anywhere but the title strip.

## 5. in `canvas_module_drag_release()`

RE-ORDER FIRST. A module dropped on top of another must push it down its column, exactly as
the application does on release — without this a drag leaves two modules occupying the same
grid squares, drawn over each other. Selected modules are transparent to one another, so a
multiple selection shuffles only what it lands on.

A column packed to MAX_ROWS has nowhere to put the drop, and the shift says so rather than
piling modules onto the last row. The whole location goes back to where it was at drag start -
the snapshot canvas_module_drag_begin() already takes for undo serves as the rollback, which is
why it covers the location and not just the dragged keys.

## 6. `canvas_param_drag_motion()`

── Parameter (dial) dragging ───────────────────────────────────────────────────────────────────

Lifted whole from cursor_pos()'s gParamDragging arm. Unchanged in behaviour; what changed is only
how it learns three things it used to read from GLFW directly:

```
  coord        the pointer in canvas logical units, as before
  rawX/rawY    the RAW cursor position, which the vertical and horizontal dial modes difference
               against their previous value. Rotary does not use them — it reads an absolute
               angle each event — which is why the plug-in works today reporting rotary while
               cursor_capture()'s pointer hiding remains application-only — see canvasDrag.h.
  altHeld      Alt drags the MORPH OFFSET rather than the value.

```
Returns true if a parameter drag consumed the motion.

## 7. in `canvas_param_drag_motion()`

Read and write through the dragged module's own Slot rather than the on-screen
gSlot the rest of this function uses. Identical for a drag on the patch canvas,
but a drag started in the Parameter Pages panel can be on a Global page knob
assigned to a module in one of the other three Slots, each with its own active
Variation.

## 8. in `canvas_param_drag_motion()`

Alt held = setting the morph offset rather than the value itself
(see below): module->param[...].value deliberately stays fixed
while dragging like that, so the delta must be measured from the
drag's fixed start point (gDragStartX/Y), not the continuously-
reset gDragPrevX/Y — otherwise each event only reflects the tiny
motion since the *previous* event against an unmoving base, which
collapses to ~0 (and the morph amount flickers back to 0) the
instant the mouse isn't actively moving between two polls. Rotary
doesn't have this problem since it reads an absolute angle each
event instead of an incremental delta.
NO LOCAL altHeld HERE. There used to be `bool altHeld = (altHeld);` — a local
shadowing the parameter and initialised FROM ITSELF, so it read an
uninitialised stack slot and the caller's answer was thrown away. Undefined
behaviour that looks stable: the garbage byte happens to be whatever the
previous call left at that stack offset, so it can read false for months and
then turn true when an unrelated change alters the frame above it. That is
exactly what happened — removing two now-unused locals from cursor_pos() made
every plain dial drag start writing the morph offset. Use the parameter.

## 9. in `canvas_param_drag_motion()`

SHIFT SLOWS THE DRAG DOWN — dial_drag_pixels_for_full_range() (SynthLib) is the
shared policy, the same one SynthEdit's dials use, so "finer" means the same
thing in both editors. Modes are left out below: a 2-to-4 position selector
gains nothing from a finer drag.

## 10. `canvas_param_drag_release()`

Ends a parameter drag: records it for undo, then clears the drag state.

Extracted from finish_param_drag() so the plug-in can end a drag too. Without this the plug-in
never cleared gParamDragging, so releasing the mouse left the dial "held" — and the next click
anywhere carried on dragging it. The application still calls stop_dragging() afterwards, which
clears the other drag kinds and restores the cursor; neither is meaningful here.

## 11. `canvas_right_click()`

── Right-click menus ───────────────────────────────────────────────────────────────────────────

The canvas half of mouseHandle.c's mouseButtonRightUp handler, lifted out so the plug-in gets the
same menus. Hit-tests in the application's order — connectors, then parameters, then the module
body, then the morph labels — which matters: a connector sits inside its module's rectangle, so
testing the body first would swallow every connector right-click.

The application keeps its own topbar and module-area right-click handling after calling this.

## 12. in `canvas_right_click()`

ONE QUERY, not a walk over every module and every parameter. The click-region registry already
holds each widget's rectangle and its identity, front to back, so "what is under the cursor"
is a lookup rather than a re-derivation — and it is the SAME lookup a left-click makes, which
is the property the old nested loops could not offer: they were a second opinion about z-order
that happened to agree.

The precedence this replaces is preserved without being restated. Connectors and parameters
register AFTER the module body (render_module registers the body, then the drag strip, then
calls render_module_common), and the registry resolves ties by taking the most recently
registered — so a connector still wins over the body it sits inside, exactly as the old
"connectors, then params, then body" order spelled out.

THE SLOT AND LOCATION FILTER IS NOT OPTIONAL, and dropping it was a real regression: with the
split view showing the Voice Area and the FX area at once, both panes have widgets registered,
and a right-click in the FX pane came back with a VA module — the owner saw an "Assign knob"
menu for a module underneath. The old nested walk was implicitly scoped because it iterated
only the modules of the location it was given; a registry query is scoped to the whole screen,
so the scope has to be stated. Ask the registry WHAT is there, then confirm it is something
this pane owns.

## 13. `set_up_cable_key()`

── Cable dragging ──────────────────────────────────────────────────────────────────────────────

The press already lives in a click-region handler (connector_click_handler in moduleGraphics.c),
which the plug-in shares; what follows is the motion's destination and the connect on release,
moved out of mouseHandle.c so the plug-in can patch as well as look.

msg_send() inside the connect tells the G2 about the new cable. In a plug-in that reaches a stub
and does nothing, which is right — the cable exists locally, the sound engine picks it up from the
database, and no hardware is written to.

## 14. `find_cable_at_connector()`

The cable attached to a given connector, and where its OTHER end is — what Ctrl-click needs in
order to pick a cable up and drag its free end. Either end can be the one clicked: linkType says
which direction the FROM end points, which is what makes an input-to-input link (two input ends,
see the backdoor-duplicate note in Docs) readable here rather than guessed at.

An output can carry several cables. This takes the FIRST it finds, which is deterministic but
arbitrary; the original editor has the same ambiguity and the manual does not say how it resolves
it. For an input there is only ever one, which is the case that matters.

## 15. `handle_cable_reroute()`

Moving a picked-up hole and everything plugged into it. See tCableDragging: Ctrl-click grabs a
CONNECTOR, not a cable, and the original moves the lot as one operation.

ALL OR NOTHING when the drop lands on a connector. Every cable is validated before any is deleted,
and if one of them cannot be made — most obviously three cables dropped on an input, which accepts
exactly one — nothing changes at all. A partial move would leave the patch in a state nobody asked
for and would have to be unpicked by hand. Dropped on empty canvas, they are all disconnected,
which is the manual's "pull out the connector and release".

## 16. in `handle_cable_connect()`

Inherits the FROM connector's CURRENT (upRate-promoted, if applicable) colour, not
just its declared base type — matches the manual's "cables connected to this
output will inherit this colour" (g2manual.txt p.71); see effective_connector_type()'s
own comment (moduleResourcesAccess.h) for why the promotion itself lives there,
not in the stored connector type.

## 17. in `handle_cable_connect()`

A connect is a topology change, so the chain's colour is re-derived across the WHOLE tree,
as the original editor does — the WHOLE tree, where the branch-scoped commands (COLOR,
DELETE) cover only the branch below the clicked connector.

This is what maintains the invariant that the colour above only guesses at: every cable in
a chain carries ONE colour, the source output's signal colour, or WHITE when the chain has
no source at all. Joining two inputs together produces a sourceless chain and so comes out
white, which is the manual's "non-functional input-to-input connections". Attaching a
source later repaints the whole tree, discarding any colour the user had chosen — which is
the original's behaviour, and the reason recolouring is tied to topology changes only.

## 18. `drag_scroll_step()`

── Auto-scroll while dragging ──────────────────────────────────────────────────────────────────

Dragging a module or a cable past the edge of a pane scrolls that pane to follow. Moved out of
mouseHandle.c once the plug-in gained scrollbars of its own — it was left behind on the first pass
precisely because a plug-in with no scrollbars had nothing to scroll.

Nothing in it was ever platform-bound: get_time_ms() is SynthLib's, and the rest is the pane
machinery. The rate RAMPS from DRAG_SCROLL_MIN_RATE to DRAG_SCROLL_MAX_RATE across
DRAG_SCROLL_RAMP_DIST of overshoot, so easing just past the edge creeps and pushing well beyond it
moves quickly — which is what stops it feeling like a runaway.

## 19. `canvas_hover_update()`

Which connector the pointer is over, if any. Lifted from cursor_pos()'s final branch.

The canvas dims every cable NOT touching the hovered connector, so without this the plug-in drew
the hover state it was never given — every cable stayed lit.

Clears gHoverConnector first, so "over nothing" is as much an answer as "over this one".

The pane UNDER THE CURSOR decides which Location to search — NOT gLocation, which follows the
FOCUSED pane and so only changes on a click. Two things went wrong when this read gLocation:
hovering the unfocused half searched the other half's modules, and because a module scrolled past
its pane's foot still registers its connector rectangles (render_modules() needs them for cable
geometry even when the module itself is clipped away), those rectangles land on screen inside the
pane BELOW. Hovering the FX area therefore lit up Voice Area connectors sitting invisibly
underneath it. Matching the pane fixes both: a connector can only be hit in the pane it was drawn
in, where the scissor guarantees it is really visible.

module_area_for_pane() is exactly the canvas, top bar and scrollbars already excluded, so the
pane lookup subsumes the bounds check this used to make by hand. Returns -1 on the split bar.

## 20. `nudge_one_param()`

── Nudging the parameter under the pointer ─────────────────────────────────────────────────────

Bare +/- steps the parameter under the pointer by one raw unit. Moved here from mouseHandle.c, where
both of these were statics and therefore application-only: the plug-in had no keyboard at all, so
the question never came up. It has one now, and this is the action behind it — the KEY DECODING
stays in each shell, because a GLFW key code and an NSEvent's characters are not the same thing,
and translating once at the boundary is the same split the modifier seam uses.

## 21. `scroll_module_into_view()`

Bring a module fully into view, scrolling the pane that shows its Location by the smallest amount
that does it. Without this, Shift+arrows happily move the focus to a module that is scrolled off
the canvas: the marks are drawn correctly, on something nobody can see.

MINIMUM MOVEMENT, not centring - a keyboard walk down a column should creep the view along rather
than jumping the focused module to the middle each step, which makes the surrounding modules leap
about. A module already fully visible scrolls not at all.

## 22. `canvas_move_module_focus()`

Shift+arrows walk the focus from module to module. Manual p84: "To move the focus to another
module in the Patch, press the Shift key on the computer keyboard together with the
Up/Down/Left/Right arrow buttons. The modules in a Patch are accessed depending on how they were
visually placed in the Patch window" - so this navigates by COLUMN AND ROW, not by module index.
Index order is creation order, which after a few edits bears no relation to what is on screen.

Up/Down stay in the column and take the nearest module above or below. Left/Right cross to the
nearest column that HAS a module in that direction - skipping empty columns rather than stopping
dead at one - and within it take the module whose row is closest to where the focus already was,
which is what keeps a sideways move feeling horizontal.

## 23. in `canvas_nudge_param_under_cursor()`

ONE QUERY. This used to walk every module in the Morph location and then every module in the
current one, testing each parameter's rectangle and then each mode's — morph first, because
morph knobs are drawn over the canvas and had to win a hit test against whatever sits beneath
them. The registry already knows that: the morph dials register at eClickLayerPanel and the
canvas widgets at eClickLayerCanvas, and the walk goes front to back. The old code's own
comment said as much — "the click path gets this ordering from the click-region layers
instead" — which is the duplication this removes rather than a difference to preserve.

## 24. in `canvas_nudge_param_under_cursor()`

SCOPED, for the same reason canvas_right_click() is: the registry covers the whole screen, and
with both panes visible a widget belonging to the other one can be found under the pointer.
The walk this replaced was implicitly scoped — it iterated the Morph location and then
gLocation, and nothing else — so the scope has to be stated rather than assumed.

## 25. in `canvas_nudge_param_under_cursor()`

STEPPING A PARAMETER ALSO FOCUSES IT (CT, 2026-08-24), so the arrow keys carry on from
wherever +/- left off instead of from some older click - the two ways of nudging one
value stay on the same value.

Only on a real step: delta 0 is the "is there anything here?" probe (mouseHandle.c's
alt-hover), and answering it must not move the focus. And only for a canvas parameter -
a morph dial never registers a param click either (see param_click_handler's note), and
a mode is not a parameter, so neither is something MIDI Learn could then act on.

## 26. `tCanvasGestureRow`

── The gesture table ───────────────────────────────────────────────────────────────────────────

See canvasDrag.h for why this exists. One row per gesture, one column per phase: a gesture whose
release was never wired up is now a NULL sitting in plain sight rather than a phase that silently
never runs, which is how the plug-in came to leave dials held and modules un-re-ordered.

PRESS IS NOT A COLUMN HERE, and that is not an oversight. A press is a hit test, and the click-region
registry already owns hit testing for the whole canvas (moduleGraphics.c registers every widget as it
draws it); a press column would mean a second, competing answer to "what is under the pointer". What
the press does have to do is call canvas_drag_begin(), and that is the one line each handler shares.
