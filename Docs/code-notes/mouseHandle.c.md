# mouseHandle.c notes

The longer comments from `mouseHandle.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `window_focus_callback()`

THE MODIFIER SEAM'S APPLICATION END IS ONE CALL PER EVENT, and both the translation and the
predicates are SynthLib's — set_modifier_state_from_glfw() in inputStateGlfw.c, shared with
SynthEdit, and shift_modifier_held() and friends in inputState.c, shared with the plug-in, which
pushes the same bits from an NSEvent.

GLFW ALREADY HANDED US THIS ON EVERY EVENT and nobody read it. Both key_callback() and
mouse_button() take an `int mods` argument describing the modifier state AT THE MOMENT OF THE
EVENT, while three separate predicates polled glfwGetKey() for the same answer a little later.
Pushing the argument is not merely tidier, it is more correct: the poll answered "now", and "now"
is after the event has been queued.

## 2. in `window_focus_callback()`

WHETHER WE ACTUALLY HID THE CURSOR, tracked explicitly rather than inferred from the drag flags.

stop_dragging() used to decide by asking is_cursor_hidden_dragging(), i.e. by re-reading the very
state it was about to clear. That is fragile in two ways, and both have bitten:

```
  - Anything that clears a drag flag BEFORE stop_dragging() runs leaves the cursor hidden for good.
    finish_param_drag() started doing exactly that when its undo push moved out to
    canvas_param_drag_release(), which memsets gParamDragging — so dragging the slot volume, or any
    dial, could strand the pointer with no way to get it back.
  - A spurious or duplicated mouse-up has the same effect for the same reason.

```
A flag set where the cursor is hidden and cleared where it is restored cannot disagree with itself.

## 3. `cursor_raw_coord()`

── The application's half of the drag-begin seam (canvasDrag.h) ────────────────────────────────

start_cursor_drag() USED TO BE HERE and did both jobs at once. It is now canvas_drag_begin() in
canvasDrag.c, which records the origin and then calls these — so the shared canvas code no longer
depends on a per-shell function remembering to do the logic half. See canvasDrag.h for the bug that
argues for the split.

## 4. `notes_own_keyboard()`

WHO OWNS THE KEYBOARD, asked once, instead of six independent editors each helping themselves.

char_event() is a row of sequential ifs with no else between them, so every ACTIVE editor used to
receive every character. Two of them being active at once is easy: opening the notes editor ends
no name edit, and starting a name edit does not close the notes editor. The results were a
character landing in the patch name AND the notes buffer at the same time, and — reported — the
synth name being untypeable whenever the notes editor sat open behind the Synth Settings panel.

Two rules settle it, in this order:

```
  * A NAME EDIT WINS. You entered it by clicking that exact field, which is the most specific
    intent available, and it is dismissed by clicking away.
  * OTHERWISE THE FRONTMOST PANEL WINS, which is what you are looking at. The notes editor is the
    only panel whose typing is handled here rather than through the panel key walk — the walk is
    already ordered front to back, so the other panels have always had this for free.
```

## 5. `set_x_scroll_bar()`

Pure legacy fallback now — morph group dials register their own click
region (eClickLayerPanel, moduleGraphics.cpp's morph_param_click_handler)
which dispatch_click_region() already checks, and wins over regular
modules, before this is ever reached. See mouse_button().
Step the parameter under the cursor by one raw wire unit. A dial maps its whole range across a
few tens of pixels, so a drag cannot reliably land on a chosen value, let alone move by exactly
one - which is what identifying a parameter's real quantisation needs (where does the synth's own
display actually change?). The step is written to the device exactly as a drag's is, so the G2
reacts to each one. Pair it with View > Parameter Values to see the raw number being stepped.

## 6. `recover_lost_cursor()`

Last resort: the cursor is hidden but nothing is being dragged any more.

The explicit flag handles a spurious or duplicated mouse-up — those still reach stop_dragging(),
which restores. What it cannot handle is a mouse-up that never ARRIVES at all: no stop_dragging,
no restore, pointer gone. Polled from the render loop so that state cannot persist for more than a
frame, which is a better answer than debouncing the button — a debounce delays every real release
to guard against a rare bad one, and still loses if the event is dropped rather than repeated.

THE FIRST HALF OF THIS WAS UNREACHABLE IN THE CASE IT WAS WRITTEN FOR, which is why the pointer
still went missing occasionally. It asked is_cursor_hidden_dragging(), i.e. our own drag flags — and
those are set by the press and cleared by the release. Lose the release and gParamDragging.active
stays set for ever, so the condition below is never true and the poll does nothing, every frame,
for as long as the application runs. A poll whose trigger depends on the event that went missing
cannot recover from the event going missing.

So the hardware is asked instead. If no button is physically down while we still hold the pointer,
the release is synthesised through the ORDINARY path — mouse_button() with a GLFW_RELEASE, exactly
what the callback would have delivered — so the drag ends the way it should have, undo entry
included, rather than being torn down by hand here. Same authority and same approach as the
plug-in shell's recoverLostRelease (vst3/g2View.m).

## 7. in `recover_lost_cursor()`

A CANVAS GESTURE WITH NO BUTTON BEHIND IT LOST ITS RELEASE. Every one of them is torn down by
the left-up handler, so if one is still running while nothing is pressed, that event never
arrived — the window lost focus mid-drag, or something else swallowed it. The rubber band is
the visible symptom: its rectangle stays drawn on the canvas until the next click.

SYNTHESISING THE RELEASE rather than clearing the flags is what makes this safe. The release
handlers are where a rubber band applies its selection and a module drag pushes its undo
entry; zeroing the state instead would drop both, turning a stuck rectangle into a lost edit.

## 8. in `recover_lost_cursor()`

A SYNTHETIC RELEASE, so it goes straight to the handler rather than through SynthLib's shim
— there is no GLFW event behind it. That also means the shim's set_modifier_state_from_glfw()
does not run, which is exactly what is wanted here: there are no real mods to pass, and a
literal 0 would report every modifier as released. Alt in particular is what tells a dial
drag to move the morph offset rather than the value, so clearing it while a key is still
held would leave the next gesture reading the wrong one until some real event refreshed it.

## 9. `finish_param_drag()`

Ends a param/mode dial drag: records the undo entry for the whole drag as one old->new pair,
then clears every drag state. Shared with the Parameter Pages panel, which drives the same
gParamDragging machinery but swallows the mouse-up before the canvas handler below ever sees
it - so without this the panel's drags would be silently missing from Ctrl-Z.

## 10. in `finish_param_drag()`

Undo push and clear are shared with the plug-in — see canvasDrag.h. stop_dragging() below
still clears the other drag kinds and restores the cursor.
Through the table like the other three, so the param gesture is not the one exception that
reaches its release by a private path. The undo push inside canvas_param_drag_release() and the
cursor restore in stop_dragging() are what keep this wrapper application-side.

## 11. in `stop_dragging()`

No explicit glfwSetCursorPos(gDragStartX, gDragStartY) here — GLFW's
cocoa backend already restores the cursor to wherever it was when
CURSOR_DISABLED was entered, as soon as we switch back to NORMAL (see
updateCursorMode() in cocoa_window.m). An extra explicit warp on top
of that was redundant, and two independent warps in a row can land a
pixel or two off from each other — enough, in SynthEdit's tightly
packed filter dials, to spill onto a neighbouring control.

## 12. in `mouse_button()`

THE SYNTH NAME EDIT ENDS ON A PRESS ANYWHERE BUT ITS OWN FIELD, and this is stated here, ahead
of the popup dispatch, because that dispatch RETURNS for anything it consumes. Doing it below
with the other name edits covers the canvas and the chrome but not the menu bar or a context
menu, both of which are the coordinator's and neither of which is "the synth name field".

Its own field is exempt so that clicking into the text you are already editing keeps the edit
alive; the Synth Settings panel restarts the edit on the release when the click lands there.

## 13. in `mouse_button()`

The modal cascade — file browser, bank browser, alert dialog, each with its own early return
and its own mouse-down/mouse-up gating, plus the alert's routing around its bank-picker
dropdown — moved into SynthLib. See synthlibPopups.h. The gating is the part worth not losing:
a modal popup swallows the PRESS as well as the click, because the browsers act only on the
release and the press used to fall straight through to the canvas underneath.
ONE QUESTION, ASKED ONCE: is this click any popup's? Thirteen ifs used to stand here — the
modal cascade, the four fixed panels, then a hand-sorted walk of the seven floating ones — and
their SEQUENCE was the z-order, restated (differently, and so wrongly in two places) by
render_frame(). All of them are rows in gAppPopups now, ranked by a layer that decides drawing
and hit-testing together. See graphics.c and synthlibPopups.h.

The gating is the part worth not losing: a modal popup swallows the PRESS as well as the
click, because the browsers act only on the release and the press used to fall straight
through to the canvas underneath.

## 14. in `mouse_button()`

A CLICK THAT REACHES HERE LANDED ON NOTHING THAT CLAIMED IT — the canvas, the chrome, empty
space — and that ends every name edit. Abandoning rather than committing is the established
meaning: stop_*_name_editing() memsets the edit, so the half-typed buffer goes and the real
name is untouched.

THE SYNTH NAME IS NOT IN THIS LIST, and belongs where it now is, at the top of this function:
it was missing here for a reason that stopped being true. The Synth Settings panel was MODAL,
returning true for every click anywhere on screen, so no click could reach this line while a
synth name was being edited, and nothing needed to end it. Making that panel float — which is
what let the canvas stay live behind it — turned an impossible case into an ordinary one, and
the edit survived a click on the modules area. Reported 2026-08-20.

Adding it here would have fixed only the half of "anywhere" that reaches this far. The popup
dispatch above returns for whatever it consumes, so the menu bar and the context menus never
get here — and neither of them is the synth name field either.

## 15. in `mouse_button()`

Focus follows the pane a press lands in, BEFORE the click is interpreted: everything below
reads gLocation, and in a split view that has to mean "the half you just clicked in" or a
module in the FX pane would be looked up in the Voice Area.

A LEFT press while a context menu is open is exempt, and that exemption is why "create module"
in the FX area used to create it in the Voice Area. The menu is raised on right-UP at the click
position, but clamp_menu_to_screen() (SynthLib's contextMenu.c) slides a frame back UP the
window when it would overrun the bottom — and the module menu is tall. Right-click low in the
FX pane, which is the LOWER of the two, and most of its items are drawn over the Voice Area.
Selecting one is a left press landing in pane 0, which re-pointed gLocation at the Voice Area
before menu_action_create() read it. The ROW came out wrong with it: the position does come
from gContextMenu.originCoord, the unclamped right-click, but
convert_mouse_coord_to_module_column_row() resolves it against whichever pane is current, and
the FX pane starts further down the window. Measured, one right-click at the same point: FX
col 3 row 2 with this guard, VA col 3 row 14 without it. Every other menu action reading
gLocation had the same hole — the cable-chain commands test it against the location the menu
was RAISED against (cable_menu_node()) and so silently did nothing at all.

Nothing is lost by skipping it: every left-press handler below is already gated on
!gContextMenu.active, so while a menu is open the press has no other effect to be focused for.
A RIGHT press is NOT exempt — it always begins a fresh menu, replacing any open one on
right-up, so it must point focus at the pane it landed in.

## 16. in `mouse_button()`

THE MENU BAR USED TO BE TESTED HERE, first of the left-down chain. It is the
coordinator's now, at the lowest layer there is — which is what this position meant,
since everything ahead of it in the old sequence (the panels, the context menu) is
ranked above it there. It could only move once the floating panels were ranked too:
a panel may overlap the bar, so dispatching the bar from a coordinator that ran
BEFORE the panels would have silently put it in front of them.

Nothing is lost by it happening before split_view_focus_at() below: the bar sits above
both panes, so split_view_pane_at() returns -1 for any coordinate on it and the call
was already a no-op.

## 17. in `mouse_button()`

Every clickable widget — module params/modes/connectors/body/drag-handle AND the morph
group overlay — registers a click region at render time (see moduleGraphics.c). Morph
registers at eClickLayerPanel, everything else at eClickLayerCanvas, so dispatch itself
(not call order) guarantees morph wins over a scrolled regular module that happens to sit
visually underneath it — see moduleGraphics.c's own top-of-file comment.

handle_morph_press()/handle_module_press() USED TO FOLLOW THIS AS A FALLBACK, "kept for
anything dispatch doesn't match (should be nothing today)". Deleted 2026-08-09, on
evidence rather than on that hunch: every rectangle they hit-tested — params, modes,
connectors, the body and the drag handle — is registered as a click region by the same
render pass that computes it, and instrumenting both of them to log when they claimed a
click produced nothing across the canvas widgets and the patch-settings panel. Worse than
dead: they tested the STORED rectangles, which for a module scrolled out of view are the
stale ones left from wherever it was last drawn, so the one case where they could still
have fired is a case where firing would have been wrong (the same fault as the FX-pane
hover bug). 318 lines, in git history if ever needed.

## 18. in `mouse_button()`

Same click's mouse-down just opened/switched/closed this dropdown via
handle_menu_bar_click() — landing back on the bar itself on mouse-up is
not a dropdown-item selection, so leave the state exactly as mouse-down
left it. Must be checked before handle_context_menu_click(): that call
has the side effect of closing the menu itself whenever coord doesn't
land on any open item, which a bar click never does.

## 19. in `mouse_button()`

BEFORE the topbar, and before the canvas: a palette drag RELEASES over the canvas,
which is the whole point of it, so the release cannot be claimed by whatever is under
the cursor at the time. palette_left_up() returns false unless a tile was actually
pressed, so it costs nothing when the palette is idle or closed.

## 20. in `mouse_button()`

THE RE-ORDER ITSELF IS canvas_module_drag_release() NOW — it was written out again
here, in a second copy the plug-in never used and this shell never shared. The undo
push below stays application-side (a plug-in has no undo stack) and still works
because that function clears only gModuleDrag.active, leaving the drag's snapshot
intact for exactly this comparison.

## 21. in `mouse_button()`

Push move undo: compare snapshot (before) with current (after)
The snapshot is the whole location, so most of it did not move — keep only
what did. Recording the rest would have undo re-issue a move message per
untouched module, telling the G2 to put each one back where it already is.

## 22. `tTempoDragTarget`

── The named-rect dial drags, as data ──────────────────────────────────────────────────────────

FIVE ARMS OF cursor_pos()'s IF/ELSE CHAIN WERE TWO GESTURES WRITTEN OUT REPEATEDLY: the tempo dial
and the performance-settings tempo dial, identical but for which rectangle they sit in; and the
vibrato amount, vibrato rate and glide time dials, identical but for a module, a parameter index
and a range. About 100 lines in which the only things that varied were the four values now in the
tables below. That is the whole of what vst3/plugin-gui-notes.md's second observation asks for —
the chain reads as a list wanting to be a table — applied to the part of it that is genuinely
repetition rather than genuinely different work.

Each entry points AT its rectangle rather than copying it: these rectangles are filled in at
render time, so a copy taken here would be a stale one from start-up. The addresses are constant
because the rectangles are globals, which is what lets the tables be static.

The remaining arms of the chain are NOT candidates for this. A scrollbar drag, a module drag, a
cable drag and a rubber band each do genuinely different work; collapsing those would need a
gesture object, which is observation 1 in that file and a much larger change.

## 23. in `handle_patch_param_drag_motion()`

The KEY names gPatchParamsEdit.slot while the message is addressed to the slot the caller
passes, which is gSlot. Both of the arms this replaces did exactly that, so it is preserved
rather than tidied — the two are kept in step whenever the slot changes (see the patch
screen's own slot handling), and making them agree here would hide it rather than settle it.

## 24. `cursor_pos()`

The coordinate is the EVENT's own, handed over by SynthLib's shim already scaled. This used to
ignore its GLFW x/y parameters and poll glfwGetCursorPos instead — the same value in practice,
since both come from the same place, but the event's is the position the event actually happened
at rather than wherever the pointer has reached by the time the handler runs.

## 25. in `cursor_pos()`

THE RAW CURSOR POSITION IS STILL NEEDED HERE, and is fetched rather than passed. The vertical
and horizontal dial modes difference raw window coordinates against their previous value (see
canvas_param_drag_motion), and this app's drag arithmetic — Alt morph offsets, the sub-unit
accumulator, the Shift-fine divisor — is tuned around that. The other two editors moved their
drags to logical units when they took the shared shim; this one deliberately did not, because
there was no need to disturb working maths to change a function signature.

## 26. in `cursor_pos()`

Hovering over a panel: skip all the hover detection below — cable highlight, connector
hover, knob/CC overlays — so nothing UNDERNEATH the panel lights up in response to a pointer
that never touched it. gHoverConnector.active is already false from just above.

THIS USED TO NAME THE MUTATOR AND ONLY THE MUTATOR, which meant it was not a rule but a
patch applied to the one panel somebody noticed it on. The other six had the bug the comment
described: hovering Synth Settings ran the canvas hover under it, lighting connectors it was
covering and hiding the cables that go with them. See floating_panels_under() in graphics.c.

## 27. in `cursor_pos()`

All four canvas gestures — dial, module, cable, rubber band — through the one table in
canvasDrag.c. These were four hand-maintained arms here and a differently-ordered ladder in
the plug-in; see canvasDrag.h for what that cost.

THE AUTO-SCROLL STAYS HERE, and only for the gestures that travel: it belongs to whoever owns
the scrollbars, which a plug-in canvas does not. A dial drag is not going anywhere, so it is
the one gesture left out.

THE RUBBER BAND TRAVELS TOO. It was excluded on the grounds that it "selects what is already
visible", which is only true of a selection that fits on screen — drag towards the edge to
gather a whole column of modules and the band simply stopped at the boundary, with no way to
reach the rest. Including it is safe because the band's anchor is stored in MODULE-AREA
coordinates (convert_mouse_coord_to_module_area_coord adds the scroll offset and divides out
the zoom), so it is absolute canvas space: scrolling moves the view underneath a fixed anchor
rather than dragging the anchor along with it. Had the anchor been in window coordinates this
would have skewed the rectangle instead, which is presumably why it was avoided.

## 28. in `scroll_event()`

ALT + WHEEL over a parameter adjusts it, rather than scrolling the pane under it — the wheel
equivalent of the bare +/- keys, and it goes through the same canvas_nudge_param_under_cursor()
so the two cannot drift on what counts as a step or which widgets refuse one.

The accumulator is not decoration. A mouse wheel delivers whole notches, but a trackpad
delivers a stream of fractions, and stepping one unit per EVENT would make a trackpad race
through the range while truncating each fraction to nothing would make it do nothing at all.
Same reasoning as the sub-unit remainder carried between drag events — see tParamDragging.
A delta of 0 is a pure QUERY — nudge_param_for_module() reports whether the cursor is over a
steppable parameter and only sends when the value actually changes. Asking first is what lets
Alt + wheel away from any parameter still scroll the pane normally, instead of being swallowed.

## 29. `key_step_direction()`

Which way a '+'/'-' key press steps, resolved against the USER'S KEYBOARD LAYOUT rather than the
physical slot. GLFW's Cocoa backend fills its keycode table from hardcoded Apple virtual keycodes
(cocoa_init.m: 0x18 -> GLFW_KEY_EQUAL, 0x1B -> GLFW_KEY_MINUS), which are POSITIONS on the board
and take no notice of the active input source. Matching those tokens therefore means "the two keys
left of Backspace on a US board", and on a Finnish/Swedish layout those two slots carry '+/?' and
the '´/`' dead key - so '+' zoomed out, '´' zoomed in, and the real '-' (bottom row, beside '.')
did nothing at all. Reported by a user, fixed here.

glfwGetKeyName() runs the scancode through UCKeyTranslate against the live input source and GLFW
refreshes that cache when the layout changes, so this follows a layout switched mid-session. It
reports the UNSHIFTED character, which is why '=' counts as increment: on a US board '+' is
Shift-'='. The keypad pair stays positional - those caps are printed '+' and '-' everywhere - and
the old tokens remain as the fallback for a layout whose name comes back NULL or non-Latin.

## 30. in `key_callback()`

The modal cascade that used to be written out here — file browser, bank browser, alert dialog,
each with its own early return, and the alert's routing around its bank-picker dropdown — is
SynthLib's now. See synthlibPopups.h: the order is a layer on each popup rather than the order
of the ifs in this function, and the quirks live in one copy instead of one per application.
The same one question the clicks ask, on the key channel: Escape has to close the panel you are
LOOKING at, which is a statement about z-order and so has exactly one right answer for both.
mods is carried through the coordinator for this — the floating panels' key handlers need it,
and a dispatcher that owned keys while dropping a field of a key event was a trap waiting.

## 31. in `key_callback()`

NOTE ENTRY, deliberately global — it does not need the Virtual Keyboard panel open, because the
panel is a view of that state rather than a precondition for it.

The text-edit guard is EXPLICIT rather than positional. key_event()'s own name-editing block is
further down this function (the char_event() handlers near the top of the file are a different
path), so relying on "everything above has returned" would have let every letter typed into a
patch name play a note as well as being inserted. Stating the condition also means a future
edit field cannot quietly reintroduce it by being handled somewhere new.

## 32. in `key_callback()`

A Channel Select group is named as a SET: the parameter carries one name
per button and the wire format sends them together, so renaming one button
has to fill in the others from what they already read. Leave them out and
the instrument is told the group has fewer buttons than it does.

## 33. in `key_callback()`

NO BARE-ESCAPE BRANCH HERE, AND THERE MUST NOT BE ONE AGAIN. Escape used to reach this point
and call glfwSetWindowShouldClose() — the GLFW sample-code idiom, which is fine for a demo and
wrong for an editor: it quit the application, with unsaved patch edits and no confirmation, on
a key whose whole meaning everywhere else in this app is "close the thing in front of me".
Every panel, page, browser and menu above handles its own Escape and returns before this point,
so what fell through to it was precisely the case where the user meant "never mind" — most
easily hit by pressing Escape once to dismiss a popup and once more out of habit.

Escape now does nothing when there is nothing to dismiss. Quitting is Cmd-Q, the Quit item in
the application menu (GLFW's Cocoa backend populates both) or the window's close button — all
three go through window_close_callback and the normal shutdown.

## 34. in `key_callback()`

THE FOCUSED PARAMETER, not the hovered one. Manual p84: "You can also use the computer
keyboard's Up/Down arrow keys to increase and decrease the focused parameter value", and
"To move the focus to another parameter in the module, press the Left/Right arrow buttons".
Focus is set by clicking a parameter, and now also by stepping one with bare +/-.

Reached only after the text editors above have had the key: every one of them returns or is
guarded by any_text_edit_active(), which is what stops an arrow in the patch-notes field
also moving a dial. GLFW_REPEAT is honoured so holding an arrow walks the range, matching
the +/- branch below.

## 35. in `key_callback()`

Bare +/- step the hovered parameter up/down by one raw unit, on whichever keys the user's
layout prints them on - see key_step_direction(). Shift is deliberately NOT in the modifier
guard: on a US board '+' is Shift-'=', and holding Shift must not stop the step.

The other two guards are both needed: mods covers the real keyboard, gCommandKeyPressed is
the flag the Cmd branch below runs on, and Cmd -/+ (canvas zoom) must keep reaching it.
GLFW_REPEAT is honoured so holding a key walks the range instead of one press per unit.
