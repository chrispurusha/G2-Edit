# moduleGraphics.c notes

The longer comments from `moduleGraphics.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tParamClickCtx`

── Click-region registration ────────────────────────────────────────────────

Every clickable widget on the canvas — module params, mode toggles,
connectors, the module body/drag-handle strip, and the morph group
overlay — registers a clickable rect each frame right where its render
function already computes it, instead of mouseHandle.c re-deriving
hit-testing over every active module. Morph group dials register at
eClickLayerPanel (a fixed on-screen overlay, unlike everything else here,
which is eClickLayerCanvas and scrolls with the module area) — dispatch
checks Panel before Canvas unconditionally, which is what lets a scrolled
regular module sit visually underneath the morph overlay without stealing
its clicks (see mouse_button()'s own comment on this).

## 2. `param_click_handler()`

Mirrors the params loop previously in mouseHandle.c's
handle_module_press_for_module()/handle_module_release_for_module() — see
git history for that code. Only reachable for non-morph modules (this
handler is only ever registered from render_param_common(), which morph
groups don't go through), so the paramType-by-location branch those
functions needed is gone here.

## 3. in `param_click_handler()`

The parameter MIDI Learn will act on. Every param type, not just the draggable ones — a
button can carry a CC too. This has to live here rather than in mouseHandle.c's
handle_module_press_for_module(), which looks like the click path but is legacy fallback:
every widget registers a click region at render time and dispatch_click_region() gets
there first, so that function no longer runs for a canvas parameter.

## 4. in `param_click_handler()`

From the registry's own capture, which dispatch armed with this region before calling
this handler — so the drag holds the exact rectangle that was clicked, rather than
looking the same thing up again in an array that a later frame may have rewritten.
See click_region_capture_rect() and tParamDragging.

## 5. in `param_click_handler()`

THE GROUP'S RECTANGLE, TAKEN NOW. click_region_capture_rect() answers only while a
press is in flight — dispatch_click_region() clears the capture BEFORE it calls the
release handler, deliberately, so a handler that re-enters dispatch cannot find a
stale one. Asking for it on release returns false and leaves the rect zeroed, and the
hit test then finds no button under any coordinate at all.

## 6. in `param_click_handler()`

A push is MOMENTARY, and it fires on the way DOWN — the mouse button being held is what
gives the pulse its width. This used to be the other way round: 0 on press and 1 on
release, which left the device holding the parameter at 1 for good. That value is
stored in the patch, so the G2 acted on it again every time it recompiled.

SeqVal's "Rnd" is where that bit: press it once, then add or delete ANY module, and the
instrument re-randomised all 16 steps and reported the new values back — which is the
"sequencer data updates randomly when I add a module" report. The editor was showing
the truth; the latched button was the cause. Confirmed both ways on hardware: clearing
param 36 by hand stopped it dead, and setting it back to 1 randomised the sequence
within two seconds without any other traffic.

The 0 has to be a separate event rather than the next line down: sent back-to-back in
the same millisecond, the device sees the release before it acts on the trigger and
nothing happens at all.

## 7. in `param_click_handler()`

Radio: the value IS the button, so there is no cycling — whichever box the cursor is
over becomes the selection. One click region covers the whole group and the button
falls out of the geometry, which keeps the group a single widget everywhere else in
the app (hit tests, knob assignment, the parameter pages) and needs no second table.

## 8. `mode_click_handler()`

Mirrors the modes loop previously in mouseHandle.c's
handle_module_press_for_module()/handle_module_release_for_module() — see
git history for that code. Modes only ever have two release-relevant
types (Menu, Toggle); anything else (e.g. paramTypeOscWave) is a plain
drag, armed on press.

## 9. `connector_click_handler()`

Mirrors the connectors loop previously in mouseHandle.c's
handle_module_press_for_module() — see git history for that code. Press
only: connector release (completing a cable) is handled entirely
separately, by handle_cable_connect() re-scanning every connector against
the release coord directly — it doesn't care which connector (if any) was
originally pressed, so it isn't a per-widget dispatch target.

## 10. in `connector_click_handler()`

CTRL-CLICK PICKS UP THE CABLE THAT IS ALREADY THERE instead of starting a new one, and drags
its FAR end — so the end under the cursor is the one that moves, which is what "pull out the
connector" means. Everything after this point is the ordinary drag: the release either lands
on a connector, and the cable is re-routed, or it does not, and the cable is simply gone.
Manual p65. With no cable on the connector there is nothing to pick up, so it falls through
and draws a new one as an unmodified click would.

## 11. `drag_area_click_handler()`

Mirrors the "module->dragArea" branch previously in mouseHandle.c's
handle_module_press_for_module() — see git history for that code. Registered
on module->dragArea *after* module_body_click_handler is registered on the
(larger, overlapping) module->rectangle, so this wins for clicks landing in
the drag-handle strip, exactly like the old first-match-wins loop order.

## 12. in `drag_area_click_handler()`

THE CURSOR'S CELL AT THE MOMENT OF THE GRAB, not the module's own. The drag moves everything
by the DELTA between this and where the cursor is now, so a module keeps the offset it was
picked up by instead of jumping to put its top-left corner under the pointer.

It used to record module->column/row, which was survivable only while a drag could start
nowhere but the title strip - the pointer was within a few pixels of the top-left anyway. Now
that the whole face drags (2026-08-30) that assumption is gone: grab a four-row module by its
bottom edge and it leapt up under the cursor. CT: "handle seems to snap/anchor mouse hold to
top." The multi-module path already moved by a delta for the same reason, and says so.

## 13. in `drag_area_click_handler()`

THE WHOLE LOCATION, not just what is being dragged. The release calls shift_modules_down() /
shift_selection_down(), which move modules the user never touched — everything below whatever
the drop landed on gets pushed down its column. Undo has to put those back too, and it can
only reverse what the snapshot recorded: with just the dragged keys in here, undo returned the
dragged module to where it came from and left the displaced ones sitting where the shift had
shoved them. Cheap — the walk is bounded by MAX_NUM_MODULES and runs once per drag start —
and the release side keeps only the entries that actually moved. Same before/after pairing
that module_positions_snapshot()/module_positions_changed() give paste and module creation.

## 14. `morph_click_ctx()`

Mirrors the morph-specific branch of the params loop previously in
mouseHandle.c's handle_module_press_for_module()/
handle_module_release_for_module() (location == locationMorph) — see git
history for that code. Registered at eClickLayerPanel by
render_morph_groups() below, not eClickLayerCanvas like every other
handler in this file — see this file's own top-of-file comment for why.
userData carries the param index (0..NUM_MORPHS*2-1) as a plain integer,
not a pointer — the morph module is a fixed singleton ({gSlot,
locationMorph, 1}), so there's no per-instance context to point to the way
a regular module's tModuleKey needs.

Unlike a regular module param, morph's own paramType is derived purely
from which half of the index range i falls in (i < NUM_MORPHS = the dial
itself, always paramTypeCommonDial; i >= NUM_MORPHS = the knob/morph-name
label underneath it, always paramTypeToggle) — never from
paramLocationList[param->paramRef].type the way param_click_handler reads
it. That collapses the original 3-way paramType branch down to: the dial
half only ever arms a drag (on press), the label half only ever toggles
(on release, flipping the isKnob flag render_morph_groups() reads).
A TAGGED CONTEXT, NOT AN INTEGER CAST TO A POINTER. These regions used to carry (void *)(intptr_t)i
as their user data, which works for a handler that knows what it registered and is a landmine for
anything that asks the registry what is under the cursor: reading a tag off it dereferences a small
integer. The morph module's own slots in sParamClickCtx are free — render_param_common() is never
called for a morph module, which is what fills them for everything else — so the context lives
there, tagged eCanvasWidgetMorph.

## 15. `render_param_focus_marks()`

This might be too generic and won't be able to use, or we add extra params!
TODO: possibly move all the type cases into functions in a new source file, references by function pointer?
The focused parameter - the one the arrow keys would act on, and the one MIDI Learn targets.
Marked with four corner brackets rather than a full outline, because a full one is already taken:
a SELECTED MODULE draws a complete yellow box (see below), and a second complete box inside it
would read as the same idea at a smaller size. Corners are also the only frame that fits a widget
whose rectangle is mostly text - a dial's rect carries its label and value, so a full border would
box up words that are not part of the control.

## 16. in `render_param_focus_marks()`

BOTH SIZES COME FROM THE RECT, and that is what makes them zoom. widgetRect is already in
screen space, so it grows with the canvas zoom; anything derived from it grows with it, while
a constant does not. An earlier version capped the arm at 3.5 and set the thickness to a flat
1.5 - the cap was already biting at 100% zoom (a dial rect is about 24.5 across, so 0.175 of
it is 4.3), which left the marks a fixed size while the dial they bracket grew underneath.

Short on purpose: a third of the side crowds the dial, so this is half the length that first
looked right.

## 17. in `render_param_focus_marks()`

mainArea, NOT moduleArea, and that is the whole trick. render_line() applies the canvas zoom
and scroll for moduleArea - but widgetRect has ALREADY had them applied: it is the rectangle
handed to register_click_region() and hit-tested against raw mouse coordinates, so it is in
screen space before it gets here. Passing moduleArea transforms it a second time and the marks
land off-canvas, drawn perfectly and nowhere near the dial.

## 18. in `render_param_common()`

WHERE THE WIDGET ACTUALLY WENT. A local, because that is all it ever was: the value is written
here, handed to register_click_region() a few lines down and returned to the caller, and
nothing reads it afterwards. It used to be a slot in a 6MB [slot][location][module][param]
global that every hit test in the app then re-read — see the migration note in Docs/todo.md.

## 19. in `render_param_common()`

The module's own Slot, not gSlot: identical while rendering the canvas (which only ever
draws the selected Slot), but the Parameter Pages panel reuses these widgets to draw a
Global page's knobs, and those can point at a module in any of the four Slots - each with
its own active Variation. Same reason the renderParams.c widgets read module->key.slot.

## 20. in `render_param_common()`

A WAVEFORM PICKER IS STILL AN ORDINARY paramTypeMenu — only its FACE differs. It was
briefly given a param type of its own, and that broke the drop-down: the type is read
in a dozen places (canvasDrag, paramPages, mutator, the click routing below) and every
one of them has to learn a new value. Deciding here instead leaves all of that code
seeing exactly the menu it saw before, so dragging, the popup and the name list keep
working with no changes anywhere else.

## 21. in `render_param_common()`

Drawn at a FIXED shape, not the module's current one: measured 2026-08-23, all four
of the shape oscillators' sines are identical pure sines at Shape 0, so a live icon
would draw the same picture for every entry there. Three quarters develops every
oscillator wave without going so far that SymPulse falls silent (it does, at the
top of its dial) or Pulse narrows to a sliver; LfoShpA's dial is bipolar with its
neutral wave at the CENTRE, so it takes 0.5, which is also where its Sqr2Tri reads
as a trapezoid rather than a second square beside Sqr.

## 22. `canvas_widget_at_any_layer()`

EVERY registered region in this application now carries a tagged context — the morph dials were
the last holdout, and they carried a bare integer. That is what makes this variant safe: it walks
all layers, so the fixed morph overlay is found before the scrolling canvas underneath it, exactly
as a click resolves.

## 23. `param_is_under_cursor()`

Is this parameter the thing under the cursor RIGHT NOW?

Answered from the click-region registry rather than by testing the app's own rectangle array, so
the answer cannot disagree with where a click would actually land: same front-to-back walk, same
data, one description of where the widget is. The comparison is against this parameter's own click
context, which is its identity in the registry — no tag, no lookup table, and sParamClickCtx stays
private to this file.

Returns false while the registry is empty (before the first frame) or when something is drawn over
the canvas, which is the correct answer in both cases.

## 24. in `render_mode_common()`

The caller's loop is bounded by module_mode_count(), which counts ROWS IN modeLocationList for
this type — not by MAX_NUM_MODES, which is how many a tModule can hold. Those were the same
thing while MAX_NUM_MODES was 16; at 2 they are one added table row apart, and the writes below
would run off the end of module->mode[] and off sModeClickCtx's last dimension.

## 25. in `render_mode_common()`

Per MODE, not per module: mode[0] took every mode's modeRef, so the LAST one rendered won and
every other mode kept 0 — modeLocationList[0], which is OscShpB's waveform. That is why the
Gate's second drop-down opened a menu of Sine1/Sine2/..., and why picking from it could write a
value the Gate has no meaning for (that list is 8 long, gateTypeStrMap is 6).

## 26. in `render_mode_common()`

render_dial_with_text() is dial-anchored and draws its text upwards, so shift the
rect down by the rows this mode will use to keep the block where it was. No entry
in modeLocationList is currently an OscWave, so this path is unexercised - it is
converted for correctness rather than because anything renders through it today.

## 27. in `render_mode_common()`

The label sits ABOVE the button, and the button does not move to make room. The dial
branch above does the opposite — it offsets the dial downwards — but every position in
modeLocationList was laid out against a renderer that drew no label at all, so pushing
the buttons down would shift every mode dropdown in the app. Drawing upwards into the
space the module already leaves keeps those positions meaning what they always did.

## 28. in `render_mode_common()`

A waveform picker shows the WAVE, not its name — see module_wave_picker_mode(). The
caption is blanked so the picture has the button to itself, but the width still comes
from text, as everywhere else: WAVE_MENU_CAPTION reserves a face wide enough to read a
waveform in, where an empty string would collapse the button to a sliver.

## 29. `volume_source()`

WHICH SOURCE A METER READS. While the sound engine is playing it knows what a module is actually
doing, and that is more useful than the last value the instrument sent over USB - especially for the
compressor, where watching the meter is how you tell a leveller is working. The engine publishes the
SAME 8-bit value the USB stream carries, so nothing below needed changing to draw it.

Falls back to the database whenever the engine is off or has nothing for this module, so a patch
viewed without the engine looks exactly as it always did.

## 30. in `render_led_common()`

Bit 0 green, bit 1 red — the pair to parse_led_data()'s extraction, and swapped from
what this used to say. The two were mirror images of each other and cancelled exactly
(a value of 1 or 2 was transposed on the way in and transposed back here; 0 and 3 are
symmetric), so this draws the same colours it always did. Which of the two bits is
really green is still an assumption — but it is now ONE assumption, in one place,
instead of two that only worked together.
Prefer the engine's LED while it is playing - see volume_source() for the reasoning
and sound_engine_module_led() for which modules publish one.

## 31. in `render_connector_common()`

.dir and .type are deliberately NOT written here any more. They are static per module type
and are now filled by populate_module_connectors() when the module enters the database, so
they are correct for code that never draws — the sound engine's cable lookups above all.
Setting them here as well would restore two owners for one fact, which is how they came to be
available only after rendering in the first place. Geometry stays ours; the rest is the
resource list's.

## 32. in `render_connector_common()`

PAST THE CONNECTOR, not by a text height. This used to add STANDARD_TEXT_HEIGHT, which
is a measurement of the LABEL and says nothing about how tall the thing it has to clear
is — so the glyphs landed on the bottom of the connector circle. labelLocLeft and
labelLocRight already step over the connector by its own size.w; this is the same rule
on the other axis, and it stays correct whatever CONNECTOR_SIZE or the zoom become.

labelLocUp is left as it is: going UP, the distance to clear is the label's own height,
because coord.y is the top of the text box.

## 33. in `render_connector_common()`

Per the G2 manual ("Control signals, blue connectors" / "Logic or gate signals, yellow and
orange connectors", g2manual.txt p.135) and matching the original editor's display, where both colours depend on the connector's
bandwidth (the fast logic orange is RGB (1.0, 0.75, 0.31)): a module running at the
higher (audio) bandwidth promotes its blue (control) connectors to red/Audio, but a yellow
(logic) connector instead becomes orange/TurboLogic — still a logic signal, just the
higher-bandwidth variant, never plain red. NOT stored back into
module->connector[connectorIndex].type above — that field is the connector's permanent
declared type, used by protocol.c's own upRate-propagation walk (see its own comment), and
must never reflect this purely-cosmetic promotion.

## 34. in `render_connector_common()`

The stored rectangle is the HIT area, not the drawn one — see CONNECTOR_HIT_PADDING. It is the
only thing .rectangle is used for (this registration, handle_cable_connect()'s release target
and the hover test), so padding it here makes all three agree; the cable end itself is drawn
from .coord, which is untouched.

## 35. `adjust_rectangle()`

THE FACE IS POSITIONED WITHIN THE BODY, NOT THE WHOLE MODULE — y = 0 is just below the title
bar, not the module's outer top edge.

It used to be the outer edge: render_module() drew the title bar INSIDE the module rectangle and
then handed that same whole rectangle to the face renderers, so every top-anchored row had to
leave room for the bar by hand. They did, and they all settled on y = 5% — which is why the inset
is 5% and not the bar's own 4.857%: it is the figure the tables were already written against, so
the change is exact and leaves every coordinate a whole number. The bar itself is 17 units
(3 + STANDARD_TEXT_HEIGHT + 2) and the body starts at 17.5, half a unit clear of it.

Bottom-anchored rows are unaffected — the bottom edge does not move — which is most of them.
THE Y PERCENTAGE COVERS THE WHOLE MODULE AGAIN, as of 2026-08-30. y = 0 is the module's outer top
edge, not the top of a body inset below a title bar - because there is no title bar any more.

This reverses the inset introduced on 2026-08-29, and the 346 top-anchored and 23 middle-anchored
rows in moduleResources.h moved back with it (+5 and +2.5 respectively; the 1483 bottom-anchored
rows measure up from an edge that did not move and were untouched). VERIFIED TWO WAYS: the regex
matched 1852 rows against 1852 anchor tokens in the file - the 2026-08-29 batch's own trap was
silently skipping the 759 connector rows, which use a macro for their size - and 329 of the 330
transformed rows come out EXACTLY equal to their values in the commit before the inset existed.
The 40 that differ are the rows edited since: Operator, re-laid from the original's resource
file, and the filter response graphs added on 2026-08-30.

The gain is real estate: a two-row module's usable body goes from 63.5px to 81px, a quarter more,
and "100% of the module" now means what it says.

## 36. `centre_on_drawn_height()`

A Middle anchor has to centre what is actually DRAWN, and for several control types that is not
the height its table row carries. render_paramType1StandardToggle() and render_paramType1Enable()
draw one text row tall, and a mode's button is drawn at HALF its row's height, while both rows
carry a square 7% box - so centring on the row's own height left the button sitting high by half
the difference. A dial needs no correction: it is drawn w x w from the rectangle's top and every
dial row in the table is square, so its centre already lands on the module's. Nor does a bypass -
draw_power_button() uses the rectangle exactly as given.

The correction is applied to the OFFSET, never to size.h. render_mode_common() derives its text
height from the rectangle it is handed (size.h / 2), so shrinking the rectangle to the drawn
height would halve the button it was meant to centre - which is exactly what a first attempt did
to Gate's G1/G2.

ONLY the Middle anchors are corrected. A Top anchor never reads the height at all, and the Bottom
anchors' offsets were laid out by hand against the row's own height, so changing what they mean
would move every bottom-anchored button in the app.

## 37. `render_module_connectors()`

Registers module->connector[i].coord (logical, moduleArea-local — the same space cables
read it back in, applying scale/scroll themselves at draw time) and draws the small
connector glyphs. Split out of render_module_common() so it can run on its own for a
module that's currently scrolled off-screen: cables reference connector positions on
BOTH their endpoint modules regardless of which one (if either) is actually visible right
now, so this must stay up to date even when the rest of that module's rendering is skipped.

## 38. `skewed_ramp_zero_start()`

── OscShpB waveform preview ─────────────────────────────────────────────────

The manual describes "Waveform Drop-Down Selectors With Graphs" on the G2's
Shape Oscillators. This was originally built against OscB, with one
graph per waveform - but
on real hardware, Shape turned out to make no audible difference to OscB's
sin/tri/saw at all (only squ and sup). The original editor's Shape graphs
show dual sine, DSF, tweaked triangle and pulse shapes, which fit OscShpB's 8-option waveform
mode (Sine1-4, TriSaw, DblSaw, Pulse, SymPulse - oscShpBStrMap) far
better. Moved here on that basis; still an approximation distilled into
simple closed-form functions rather than a byte-exact port, so treat this
as a starting point to verify against real hardware, same as before.
A single cycle that starts at a rising zero-crossing and runs -1..+1..-1..(back to 0), with
"peak" (0..1) setting where the top of the ramp falls: 0.5 is a symmetric triangle, near 1.0 a
near-full sawtooth ramp. This is exactly TriSaw's math below, factored out so DblSaw can reuse
it rather than duplicate a subtly different version of the same thing - both the phase-shift
(landing a rising, not falling, zero-crossing at phase 0) and the sign flip (matching the real
editor's orientation) were only worked out and confirmed via TriSaw.

## 39. `pulse_edge_width()`

Ramp width for the Pulse/SymPulse zero-crossing ramps below: half a sample-to-sample step at
the render loop's 200-sample resolution (must stay narrower than that step or the ramp isn't
actually sampled at all), capped further so it never eats more than half of whatever room is
actually available on either side of it (relevant once High/Low get thin near Shape's
extremes - see SymPulse below).

## 40. `oscshpb_waveform_sample()`

Shape is always the raw 0-127 param value normalised to 0..1 - but the dial itself only
displays* 50%..99% of that (render_paramType1Shape), so Shape 0 is the dial's displayed
minimum (50%) and Shape 1 its displayed maximum (99%), not "no shaping"/"full shaping" in the
usual 0-100% sense. Sine1-4/TriSaw/Pulse/SymPulse are all taken directly from the manual's
"WAVEFORMS AND SHAPES" section for OscShpB (g2manual.txt), which spells out each one's exact
Shape 50%/75%/99% appearance - not guesses. DblSaw's manual description hasn't been
reconciled with what's actually implemented below yet (see its own comment).

## 41. in `oscshpb_waveform_sample()`

THE LAWS LIVE IN waveModels.c, shared with the sound engine so the wave that is drawn and the
wave that is heard cannot drift apart — which they had, badly, before it existed. What stays
here is how a law is turned into a DRAWN curve, and that is genuinely this file's business:
the render loop takes 200 samples, so a hard step between two of them is simply missed and the
drawn line never passes through zero where the real wave does. Hence the explicit ramps below,
which the engine neither has nor wants — it band-limits instead.

## 42. `lfoshpa_waveform_sample()`

LfoShpA's six waves are a DIFFERENT FAMILY from the two shape oscillators' - Sine, CosBell,
TriBell, Saw2Tri, Sqr2Tri, Sqr - so none of OscShpB's laws carry over wholesale. MEASURED
2026-08-23 by running the LFO at audio rate (Range = Rate Hi) so the same capture rig applies,
then fitting candidate families against the captured cycle; every wave landed on one cleanly,
at 0.98 to 1.000.

ITS SHAPE DIAL IS BIPOLAR AND DEFAULTS TO 64, unlike the oscillators' Shape which starts at 0 and
only opens. At 64 each wave is its neutral self - a pure sine, a symmetric triangle, a square -
and Shape skews it either way from there. paramLocationList already carries that default.

## 43. in `lfoshpa_waveform_sample()`

Sine - THE SAME three-segment symmetric phase warp as OscShpB's Sine1, which is a
pleasing result: one model now serves OscShpA, OscShpB and this. Only the breakpoint
law differs, and here it is linear and passes through the identity (0.25) at the
dial's centre: measured 0.02, 0.13, 0.25, 0.37, 0.48 across the sweep.

## 44. in `lfoshpa_waveform_sample()`

CosBell and TriBell - one bell per cycle, silent for the rest of it, and the bell simply
WIDENS with Shape: measured width 0.05, 0.26, 0.50, 0.74, 0.98, i.e. the dial itself.
The two differ only in the bell's own profile, a raised cosine against a triangle.
The output is AC coupled, so the bell's mean is removed - exactly 0.5 * width for both
profiles - and what is left is renormalised, which is what puts the bell above the line
and a shallow negative shelf below it rather than a bell sitting on zero.
Neither bell closes completely, and they do not stop at the same place: at Shape 0
CosBell measures about 0.08 wide and TriBell about 0.05. Forcing both to one figure
costs the other one visibly, so the floor is per wave.

## 45. in `lfoshpa_waveform_sample()`

Sqr - a plain pulse width, and the width IS the dial: measured 0.26, 0.50, 0.74, 0.96.
It correlates a little lower than the others (0.90 to 0.99) because the instrument's
edges are not vertical, which a drawn icon has no way to show at this size anyway.
Measured 0.08, 0.26, 0.50, 0.74, 0.96 - very nearly the dial itself, but it neither
closes nor fills completely, so the fitted line and its floor are used rather than the
bare dial value.

## 46. `basic_sine()`

LfoB has NO Shape parameter — it picks one of four fixed waveforms, so its graph is one static
shape per selection rather than a morphing one. Captured 2026-08-23 by running the LFO at audio
rate (Range = Rate Hi), the same trick that makes any of these measurable with the audio rig.

ITS WAVE PARAMETER STOPS AT 3, even though lfoWaveStrMap carries six names: setting 4 or 5 gives a
cycle indistinguishable from Squ, so the device clamps there. paramLocationList's declared range of
4 is right, and the map's trailing "RndSt"/"Rnd" belong to a different LFO. Random waves could not
be drawn as a single cycle in any case.
THE FOUR TEXTBOOK WAVES, as four primitives rather than four copies. LfoB's measured cycle and the
OscA family's first four are the same shapes, and every one of the G2's plain (non-shapable)
waveform selectors is built out of these — so they are written once here and the per-module sample
functions below choose among them. The bodies came from LfoB's capture and are unchanged by being
named: this is the same code it always ran.

## 47. `shaper_transfer_sample()`

RECT AND SHPSTATIC ARE NOT WAVES AT ALL — they are TRANSFER CURVES, so their icon plots output
against INPUT rather than against phase, and the horizontal axis runs -1 to +1 instead of round a
cycle. Everything else about the picker is the same, which is why they arrive through the same
plumbing: module_wave_sample() is handed a position across the box either way.

MEASURED 2026-08-24 with a dry/wet rig — a triangle sent BOTH straight to one output and through
the shaper to the other, so pairing the two channels sample by sample gives the transfer curve
directly, with the source cancelling out. The fit leaves the gain free, because the two channels
have unknown relative gain and an input that never reaches full scale would otherwise masquerade
as curvature (it did: peak-normalising first pulled every exponent toward 1).

RECT IS EXACTLY WHAT THE MANUAL SAYS (p207), which is worth recording given how often it is not:
discard negatives, discard positives, mirror negatives up, mirror positives down.

SHPSTATIC'S ICON DRAWS THE ENGINE'S OWN CURVES since 2026-09-13 (shaper_transfer(), paramCurves.c
notes §31): x2 and x3 are s^2 and s^3, Inv x2 and Inv x3 are 1 - (1 - s)^2 and 1 - (1 - s)^3. The
capture above had fitted a pure power law to each:
```
    x2      p = 1.98   rms 0.00002      x3      p = 2.97   rms 0.00002
    Inv x2  p = 0.65   rms 0.00079      Inv x3  p = 0.49   rms 0.00122
```
and the two Inv curves fitted forty times worse because they are not power laws at all - a power law
fitted to 1 - (1 - s)^n lands on about those exponents. So the capture and the curves agree; only the
model fitted to it was wrong.

## 48. `module_wave_is_transfer()`

A transfer curve has ENDS, not a seam. render_wave_icon() closes a cycle by drawing the step
between its last sample and its first, which is right for a saw and nonsense here — the two ends of
a rectifier's curve are simply its extremes, and joining them would draw a vertical through the
middle of the picture.

## 49. `LFO_RANDOM_STEPS`

LfoA's and LfoC's set — lfoWaveStrMap, the SAME map LfoB uses, but all six entries rather than the
four LfoB clamps to. The first four are LfoB's measured cycle and are simply delegated; only the
two random ones are new, and they are a different kind of thing entirely.

A RANDOM WAVE HAS NO CYCLE TO MEASURE, so this is the one wave icon in the app that is a
REPRESENTATION rather than a fitted law, and it should not pretend otherwise. What it has to convey
is the single distinction between the two entries: RndSt holds each new value until the next step
(a sample-and-hold staircase) while Rnd glides between them. Everything else about it — how many
steps, which levels — is a drawing decision.

THE LEVELS ARE A FIXED TABLE, deliberately. Drawing from an actual random source would make the
icon flicker on every redraw, which is worse than useless on a picker: two entries that never look
the same twice cannot be compared. Six steps reads clearly at the ~30 pixels a button gives.

MEASURED 2026-08-24, and the direction is confirmed. LfoA at audio rate (Range = Rate Hi, the trick
that made LfoShpA measurable), each wave captured to ITS OWN file so there are no sweep boundaries
to mis-segment. Read from the distribution of sample-to-sample differences, since a staircase is
bimodal — nearly all differences zero, a few very large — where a glide is not:
```
               p50/p99      differences at zero    longest hold
    RndSt        0.057            43.4%             19 samples
    Rnd          0.123            21.1%              8 samples
```
So RndSt holds twice as often and twice as long, which is the distinction the two icons draw.

DO NOT READ THESE OFF A PLOT. Eyeballing 10 ms windows of the two gave the OPPOSITE answer at one
point, because the two captures autoscale to different vertical ranges and the eye compares shapes
rather than hold times. The statistic is trustworthy where the picture is not.

Rnd IS NOT A PURE GLIDE, though — it still holds 21% of the time, so it is more likely a slewed or
smoothed random than the linear interpolation drawn here. That refinement is open; the direction it
differs from RndSt in is not.

## 50. `osc_a_waveform_sample()`

OscA's family — shapeOscATypeStrMap: Sine, Tri, Saw, Sqr50, Sqr25, Sqr10 — shared by OscA, OscC
and OscD, which is why one function serves three modules. None of the three has a Shape dial, so
like LfoB each selection is one static shape.

THE THREE SQUARES ARE PULSE WIDTHS AND THE NAMES SAY SO: Sqr50 is the square, Sqr25 a quarter-cycle
pulse, Sqr10 a tenth. That is the one part of this set that needs no interpretation at all.

MEASURED ON THE HARDWARE 2026-08-24, on OscA at E4 and again three octaves down, and both of the
things that were transferred from other modules on the first pass turned out to need correcting:
```
  - THE SAW RISES. Drawn falling (carried over from LfoB, whose saw genuinely does fall) it scored
    0.516; rising, 0.988.
  - "SQR10" IS NOT A 10% PULSE. Fitting ideal pulses of every width against the captured cycle puts
    it at 1/16 — 6.25% — where 10% scores 0.79 and 6.25% scores 0.99. Measured twice, at 145 and at
    1165 samples per cycle, agreeing to within the fit's own resolution. THE MANUAL SAYS 10% (p174,
    "Sine, Triangle, Sawtooth, Square, 25% Pulse or 10% Pulse") AND IS WRONG, which by now is the
    expected outcome rather than a surprise. Its 25% is exact, though, and so is the square's 50%,
    so the name is only wrong on the one entry — and 1/2, 1/4, 1/16 are all binary fractions, which
    is what a DSP would be expected to produce.
```
The pulses need NO edge ramps: drawn as ideal steps they correlate 0.992 and 0.989, so whatever
band-limiting OscShpB's Pulse needed (pulse_edge_width() above) does not show here.
Correlations, rotation-free, against the cycle the instrument produced:
```
  Sine 1.0000   Tri 0.9981   Saw 0.9880   Sqr50 0.9917   Sqr25 0.9888   Sqr10 0.9903
```

## 51. `osc_b_waveform_sample()`

OscB's five — shapeTypeStrMap: Sine, Tri, Saw, Sqr, DualSaw. MEASURED 2026-08-24, and every one of
them turned out to be a law we already had, so this is an index remap and not a new family.

SHAPE ONLY REACHES TWO OF THE FIVE. Sweeping it across the whole dial leaves Sine a pure sine (no
harmonic above the first rises above 0.01 at any setting), Tri a triangle and Saw a sawtooth whose
harmonics stay at 1/n to two decimals. Only Sqr and DualSaw respond to it at all — which is worth
knowing before anyone models a Shape law for the other three.
```
  - SINE, TRI: the plain primitives.
  - SAW RISES, like OscA's and unlike LfoB's: 0.988 against the rising form, 0.516 against the
    falling one.
  - SQR IS OscShpB's PULSE, law and all. Measured duty runs 0.500, 0.375, 0.250, 0.125 at Shape
    0/32/64/96 — a straight 0.5 - 0.49*shape, which is exactly what oscshpb_waveform_sample() case
    6 already draws. At the top of the dial it lands on 0.055 rather than continuing to 0.010, and
    0.055 of a 145.6-sample cycle is EIGHT SAMPLES: the same floor OscShpB's Pulse was measured to
    have. So the floor belongs to the instrument's pulse generator rather than to one module.
  - DUALSAW IS OscShpB's DBLSAW, at 0.999 across the dial and clearly separated from every runner-up
    (0.87 at best). Its Shape is the same detune.
```
THE MANUAL MISCOUNTS THIS MODULE (p174): it says "one of five waveforms" and then lists six, adding
a symmetric pulse. There is no sixth — writing 5 to the waveform parameter gives a cycle identical
to DualSaw at 1.000, so the instrument clamps and paramLocationList's declared range of 5 is right.

## 52. `module_wave_picker_mode()`

Which parameter, on which module, is the waveform picker whose button should show a PICTURE. Kept
as a list in one place rather than spread through the render code, so adding a module is one line.
OscShpB keeps its waveform in a MODE rather than a parameter, so it reaches the button through
render_mode_common() instead of the parameter path — which is why it was the last picker still
showing words after the other three were converted.

## 53. `module_wave_value()`

WHERE EACH MODULE KEEPS ITS WAVEFORM, AND THE SHAPE THAT GOES WITH IT. The picker predicates above
name the INDEX; these return the VALUE, so the small icon and the big graph ask the same question in
the same way. This used to be written out twice — once for the icon and once inside the graph
renderer, as a chain of isLfoB / isLfoShpA / isShpA tests — and adding OscB would have made it a
third copy of the same knowledge.

## 54. `module_wave_icon_shape()`

THE ORIGINAL EDITOR PICKS WAVEFORMS WITH PICTURES, NOT WORDS — its selector buttons carry little
line drawings of the wave. This draws the same idea in our own style rather than lifting its
bitmaps: the pixels are in its resource file and decode cleanly, but they are Clavia's artwork, and
we can do better than copy them anyway. Every one of these waves now has a MEASURED model behind
it, so the icon is generated from the same function that draws the module's big graph — which makes
it resolution independent, themed like everything else, and automatically correct if a law is ever
refined. (CT agreed this approach 2026-08-23.)

## 55. in `render_wave_icon()`

mainArea, NOT moduleArea, and that is the whole point. render_line() applies the
canvas's zoom and scroll itself when it is given moduleArea — but the rectangle this
draws into came back from the button renderer with that transform ALREADY applied, so
asking for it again placed the icon at neither the right size nor the right position
and left it standing still while the canvas moved under it. mainArea skips the second
adjustment and keeps the global scaling both paths share. (Same trap as the radio
buttons' click regions earlier: draw_button returns ADJUSTED coordinates.)

## 56. in `render_wave_icon()`

CLOSE THE SEAM ONLY WHERE THE WAVE ACTUALLY JUMPS. A single cycle drawn on its own leaves its
two ends unjoined, so a saw came out as a bare diagonal with nothing to show the flyback that
makes it a saw (CT). The first attempt dropped a vertical to the ZERO LINE at each end, which
fixed the saw and broke the triangle: Saw2Tri starts and ends at -1, so it grew a half-height
stub at both ends that is not part of the wave (CT again).

The seam is a WRAP, so the honest thing to draw is the step across it — from where the cycle
ends to where it begins — and only when there is a step to draw. A triangle ends where it
started, so nothing is drawn and it closes on its own; a saw or a square ends a full swing
away from its start, and gets the single vertical edge that identifies it.

## 57. in `render_wave_icon()`

DRAWN AT BOTH ENDS, because the wave repeats: the step across the seam is the same edge
whether you meet it leaving one cycle or entering the next, and showing it only on the
right left the saw and the square looking like they began in mid-air (CT). With both, one
cycle reads as a complete, bounded waveform.

## 58. in `render_oscshpb_waveform_graph()`

ONE PERIOD OF WHATEVER WAVE THIS MODULE IS SET TO, green on grey, in the box graphLocationList
gives it. Four modules share this renderer — OscShpB, OscShpA, LfoB and OscB — and they keep
their waveform in three different places and their Shape in three more. None of that is decided
here any more: module_wave_value(), module_shape_value() and module_wave_sample() answer those
three questions for every module, so this function is only the drawing.
OSCSHPA SHARES THIS RENDERER, AND ITS WAVES ARE THE SAME WAVES. Measured 2026-08-23: each of
OscShpA's six correlates 0.990-0.999 with one of OscShpB's laws and is clearly separated from
the runner-up, so the same sample function serves both. What differs is only how the module
says which wave and how shaped:
```
  - OscShpB keeps the waveform in a MODE (its only one) and Shape at param 6.
  - OSCSHPA HAS NO MODES AT ALL — the instrument answers "module has 0" when asked — so its
    waveform is a genuine parameter, index 9, with Shape at 7. That difference was the open
    question when this was planned; the hardware settled it, and paramLocationList was right.
  - OscShpA offers SIX waves, not eight: it drops DblSaw and Pulse, so its index 5 is
    OscShpB's SymPulse (7) and the first five map straight across.
```

## 59. in `render_oscshpb_waveform_graph()`

THE RIGHT-HAND EDGE DRAWS ITSELF, THE LEFT-HAND ONE DOES NOT. The sweep runs to xFraction 1.0,
where fmod() wraps the phase back to 0 — so the final segment leaps from the end of the cycle
to its start and paints the flyback at the right of the box. Nothing precedes the first sample,
so the same edge is missing on the left, and a saw sat in its box bounded on one side only
(CT). Mirroring it there completes the shape.

Gated on a real step, exactly as the picker's icon is: a wave that ends where it began — a
triangle, a sine, a bell — must NOT be given a vertical, or it grows a stub that is not part of
the wave.

## 60. `envadsr_decay_level()`

── EnvADSR envelope preview ─────────────────────────────────────────────────

A small live graph of the classic Attack/Decay/Sustain/Release envelope shape, same spirit
and roughly the same size as the OscShpB waveform preview above. Segment WIDTHS are drawn
from each knob's own raw value independently (not real time - Attack/Decay/Release span
0.5ms to 45s each, far too wide a dynamic range to draw to scale in a
small box; and not normalised against each other either, since dividing by the sum of all
three made each segment's width depend on the OTHER two as well as its own knob, saturating
quickly enough that sweeping one knob only looked like it had two states). The Sustain
plateau's HEIGHT reflects its level directly (it's a level, not a timed phase, so it gets a
fixed display width just to show the hold clearly).

Both Env Shape and Output Type are taken from the manual's "COMMON ENVELOPE GENERATOR
PARAMETERS" section (g2manual.txt), not guesses:

- SHAPE SCROLL BUTTON (envShapeStrMap - "there are four alternatives: Logarithmic Attack &
```
  Exponential Decay/Release, Linear Attack & Exponential Decay/Release, Exponential Attack &
  Decay/Release and Linear Attack & Decay/Release") - so the first word of each is Attack's
  curve, the second is Decay+Release's. Log (the default) is concave (fast then levelling -
  confirmed against the real original editor); Exp attack is its mirror, convex (slow then a
  fast final approach); Decay/Release's Exp is concave, matching a capacitor discharging.
```
- OUTPUT TYPE SCROLL BUTTON ("Pos: 0 up to +64 then down to 0. PosInv: +64 down to 0 then up
```
  to +64, i.e. inverted. Neg/NegInv mirror Pos/PosInv into negative range. Bip/BipInv: bipolar,
  sustain level fixed at 0 (ignoring the Sustain knob)"). Implemented as one shape computed in
  Pos's own convention, then optionally reflected (1-x, for PosInv/Neg) and/or negated (for
  Neg/NegInv/BipInv) - Bip/BipInv additionally swap in a fixed 0 sustain and let Release run
  on to -1 instead of stopping at 0.
```
- GRAPHS ("Any sustain level is indicated with an orange line; the rest...are green. There is
```
  also a yellow horizontal line which indicates the zero level") - colours below match this
  directly.
```
The attack and decay/release curves themselves are paramCurves.c's, shared with the sound engine
so the envelope drawn here is the one that is actually played. This file keeps only the part that
is a DRAWING concern: a segment on the face runs between two arbitrary levels - full down to
Sustain, Sustain down to the release target - where the engine's own segments always run 1 to 0.

Attack curve types (envShapeStrMap's first word): 0=Log (default), 1=Lin, 2=Exp, 3=Lin.
Decay/Release curve types (its second word): 0-2=Exp, 3=Lin - which env_fall_level() applies.

## 61. `env_graph_time_width()` (was in `render_envadsr_graph()`)

Each segment's width comes from its OWN knob only, independent of the other two - 0.04
(raw minimum, still clearly visible rather than a zero-width vertical line) up to 0.24
(raw maximum). Whatever's left of the box after Attack+Decay+Release+the fixed Sustain
width is just background.

## 62. in `render_envelope_graph()` (was `render_envadsr_graph()`)

Where "envelope output = 0" sits, and how much of the box height its full swing covers -
Pos/PosInv only ever go 0..+1 (manual: "0 units...up to +64"), so zero sits at the
bottom; Neg/NegInv only ever go 0..-1, so zero sits at the top; only Bip/BipInv are
genuinely bipolar, spanning -1..+1 around a centred zero.

## 63. `tFilterGraph`

── FltClassic response preview ──────────────────────────────────────────────

A small live graph of the classic lowpass filter's frequency response, using the real 2-pole
resonant lowpass magnitude formula |H(f)|^2 = 1 / [(1-(f/fc)^2)^2 + (f/(fc*Q))^2] rather than
stitching together a flat line, a Gaussian "bump" and a separate linear rolloff - passband,
resonant peak (taller and narrower as Q increases, per the manual's "narrow resonance
peak...similar to...analog ladder filters") and rolloff all fall out of the one formula as a
single smooth curve. 18/24 dB/octave are modelled as extra plain one-pole rolloff stages
cascaded onto the base 2-pole (12dB) response, which also naturally narrows the peak further
at higher slopes, matching real higher-order filter behaviour. Q itself uses the real 0.5..50
range from flt_resonance_q(). The
X axis is ~10 octaves of relative frequency (roughly matching Freq's own real ~14Hz..21kHz
exponential range) rather than a literal Hz-calibrated Bode plot - the box is too small to be
literal about that regardless of curve shape. The cutoff is inset into a band across the box
(not mapped edge-to-edge) so the resonance peak always keeps headroom either side - see the
cutoff calc below.
WHERE EACH FILTER KEEPS THE THREE THINGS A CURVE NEEDS. They are not laid out alike, and two of
them keep the slope in a MODE rather than a parameter, so this cannot be positional. -1 means the
module does not have that control at all.

## 64. in `filter_graph_map()`

Five of the seven filters draw a response curve. The original editor draws one on six -
FltClassic, FltNord, FltLP, FltHP, FltComb and FltPhase, but NOT FltStatic - and we differ
from it twice, deliberately: FltStatic gains one because its face has the room and its
response is worth seeing, while FltComb and FltPhase have none yet because a comb and a
phaser want their own renderer rather than this response curve. See todo.md.

## 65. in `render_filter_response_graph()`

Inset the cutoff's on-screen position into a band rather than mapping the knob edge-to-edge, so
the resonance peak (and the passband/left flank below it) always keeps horizontal headroom. At
min Freq the peak used to sit hard against the left edge with its lower half off-screen. This is
a small horizontal zoom-out: the box now shows a slightly wider window than the knob's own
~14Hz..21kHz range so the curve never runs off either edge.

## 66. in `render_filter_response_graph()`

Shared with the dial text and the sound engine — see renderParams.h. What the curve draws and
what the engine plays come from one definition.

MEASURED, NOT ASSUMED (2026-08-24). This used to draw a BIQUAD with extra one-poles bolted on,
and a biquad's gain at DC is 1 whatever its Q — so the drawn passband stayed pinned at 0 dB and
only the peak grew. The instrument does the opposite: winding Res up pulls the whole passband
DOWN, about 14 dB by the top of the dial, because the resonance is feedback around the poles
rather than a Q inside a biquad. See flt_ladder_feedback() in paramCurves.c for the capture and
for the part that took the longest to see — the loop is always four poles long and the dB
switch only moves the output tap.

## 67. in `render_filter_response_graph()`

FLTNORD IS NOT FLTCLASSIC'S LADDER. Measured 2026-08-30: FltClassic's passband droops with
resonance, from -1.3 dB to -12.7 dB, which is the feedback the comment above describes and
which this curve draws correctly. FltNord's does NOT - with its GC off the passband stays flat
at about +1 dB across the whole Res range and only the peak grows. So borrowing the ladder
draws a droop the instrument does not have. A four-pole ladder's DC gain is 1/(1 + k), so
(1 + k) cancels it; FltNord's GC, which IS a measured broadband attenuation, then goes on top.
Same expression as the sound engine uses, for the same reason - see the note beside fltGain
there. Every other filter gets 1.0 and is unchanged.

## 68. in `render_filter_response_graph()`

Scale must stay within min(baseY's own fraction, 1-that fraction) - 0.6/0.55 didn't
(0.6+0.55 = 1.15), which is exactly why the rolloff (level -> -1) was plotting below
the box's bottom edge; 0.38 fits both the 0.6-above and 0.4-below headroom baseY
leaves either side of it.

## 69. in `render_filter_response_graph()`

NEVER TRACE ALONG THE FLOOR, at either end. A low-pass that has fully rolled off used to
stop early so it did not trail a flat line to the right edge; a high-pass STARTS fully
rolled off, so the same box grew a flat line along the bottom from the left edge up to
where the curve lifts off (CT). One rule covers both, and it needs no test for which
topology is being drawn: draw only once the curve has been on the page, and stop once it
has left it again.

## 70. in `render_module()`

THE WHOLE FACE DRAGS, not just the title strip (CT, 2026-08-30). Every parameter, mode,
connector and graph registers its own region on this same layer AFTER this one, so each of
them still wins over it - this is the fall-through for the parts of a module that carry no
control, which is exactly the "click anywhere there is nothing adjustable" behaviour the
original editor has. It needed no new hit-testing: the registry already resolved overlaps
this way, and the title strip was only ever a smaller rectangle registered later.

## 71. in `render_module()`

COPY_STRING, NOT snprintf: the USB thread writes module names as patch data arrives, and
this read is on the render thread. Both go through gStringCopyMutex, so what is drawn is
one name rather than a mixture of the old and the new. See defs.h.
CENTRED ON THE Name MODULE ONLY (CT, 2026-08-30). That module IS a caption - a name bar
dropped on the canvas to label a section of a patch - so its text belongs in the middle of
it. Every other module keeps its name in the top-left corner where it has always been:
there the name identifies the module and sits above its controls, and centring it would
move a fixed landmark on 200-odd faces for no gain.
EDITING is left-aligned in both cases, so the caret starts at the same x wherever you
rename, and a centred field cannot shift under the caret as the name grows.

## 72. in `render_module()`

THE TYPE NAME AND THE INDEX ARE NOT DRAWN ON THE FACE. They used to be - the type in brackets
across the middle of the title bar, the index in its top-right corner (that one Debug-only) -
and both are gone as of 2026-08-30 (CT). Neither is patch data, the original editor shows
neither, and the title row is the scarcest space on a module. open_module_context_menu()
heads the module's own right-click menu with both, which is a gesture away when they matter.
This is the first step towards dropping the title bar altogether, as the original has none.

## 73. in `render_modules()`

Skip the (relatively expensive, many-sub-element) render for modules currently
scrolled entirely outside the visible canvas. Rect here mirrors render_module()'s
own moduleRectangle exactly; rectangle_visible_in_module_area() applies the real
scale/scroll transform so a module straddling the viewport edge still renders.

## 74. in `render_cable_from_to()`

A quadratic bezier curve is always fully contained within the bounding box of its 3
control points, so this bbox — built from the endpoints AND the sag point, not just
the endpoints — is a correct (if slightly loose) bound on where the curve can actually
fall. Skip the draw if none of that box is visible.

## 75. `cable_is_being_rerouted()`

The cable a Ctrl-drag has picked up is NOT DRAWN while the drag is in flight. It still exists —
the delete happens at release, so that the whole re-route is one undo step — but leaving it on
screen showed the cable in its old position while the rubber band drew the new one, which reads as
though a second cable is being made rather than this one being moved. The manual's "pull out the
connector" is what the user should see: the cable leaves its socket and follows the cursor.

## 76. `MORPH_GROUP_WITH_THIRD_SOURCE`

Which source a morph group is taking, given its selector value. Every group offers the knob at 0
and one named alternative at 1 - except the FIFTH, which has a third: the first global wheel.
Treating any non-zero value as "the" named source showed a group 5 morph assigned to G.Wh 1 as
Sust.Pd instead.

## 77. in `render_morph_groups()`

The per-frame "nullify all the rectangles so stale ones cannot be clicked" loop that used
to be here is gone with the array: hit-testing comes from the click-region registry, and
clear_click_regions() empties that at the top of every frame. A widget that is not drawn
this frame is not registered this frame, which is the same guarantee without the bookkeeping.

## 78. `render_shaper_transfer_graph()`

The Shaper group's transfer curve: output against input across -1..+1, which is exactly what the
manual says the original's graph shows (p.204, "The Y-axis represents the output signal values, and
the X-axis the input signal values"). It evaluates `shaper_transfer()` - the engine's own function,
see `paramCurves.c.md` §30 - so what is drawn is what the engine plays. That cuts both ways: apart
from Rect those laws are the manual's shapes with GUESSED depths, and the graph is only as right as
they are. When the ramp captures refit them, the graph follows with no edit here.

The original draws one on Clip, Overdrive, Saturate, ShpExp and WaveWrap, and NOT on ShpStatic or
Rect, whose pickers already carry their curves as icons (§47) - so those two get none here either.

The amount drawn is the dial alone: the Mod input adds a live signal the face cannot know. Bypass
does not flatten the curve, for the same reason the filter graphs ignore it - the graph shows what
the settings do, and the Bypass button shows whether they are in use.

PLACEMENT. The original's box is 34x22 at XPos 173-174 on a two-row face: through the transform in
module-layout-rules.md that is 67.8% in and 13.3% wide. The width is kept; the position is not. At
67.8% the box sat squarely over Saturate's and ShpExp's "Curve" label, and at the 0.59 zoom the owner
works at - where text stops shrinking with the face - the label covered a third of it. The top-right
corner is empty on all five faces, so all five boxes sit there, 4% in from the edge, rather than
two of them being squashed to clear a label. Nor is the height the transform's 17%, which would run
into the dials and jacks our faces keep along the bottom edge.

## 79. `render_eq_response_graph()`

Gain against frequency, 20 Hz to 20.5 kHz on a log axis and +-20 dB with 0 dB across the middle - the
gain dials reach +-18. It evaluates `eq_magnitude()` on the bands `eq_bands_build()` builds for the
engine (`paramCurves.c.md` §37, §38), so a cut is drawn exactly as wide as it is played: a cut mirrors
the boost of the same size and is therefore far WIDER than it (§11.4 of the engine reference), which
is the least intuitive thing about these three modules and worth being able to see.

PLACEMENT. The original draws a 52x28 box on all three, at XPos 182 on EqPeak and Eq3band and 173 on
Eq2Band. EqPeak takes the transformed position outright - 71.4% in and 20.4% wide, the filters' box.
Eq2Band and Eq3band keep the width but end 15% from the right edge rather than 8%: our faces put their
level meter at 86-89%, where the original had its input jack.

## 80. `render_comb_response_graph()`

FltComb's response on a LINEAR frequency axis spanning four teeth. A comb is periodic in linear
frequency, and on the log axis the other filters use its teeth crowd together at the top: at the
default Freq there are about sixty of them below 20 kHz, several to a pixel. So this graph shows the
tooth SHAPE - which is what FB and Type change - and does not move with Freq, whose value the dial
already reads out. Evaluates `flt_comb_magnitude()` (`paramCurves.c.md` §39), the law the engine
plays; +-24 dB, since Peak at full FB reaches about +22.

PLACEMENT. The original's box (52x28 at XPos 182) is where our face has its level meter, so the box
is inset 16% from the right, as the EQs' are for the same reason. FltPhase, underneath it in most
patches, takes the same position so the two line up.

## 81. `render_phaser_response_graph()`

FltPhase's response, 20 Hz to 20 kHz on a log axis, +-24 dB. It evaluates `flt_phase_magnitude()`,
which is a MODEL the engine does not play (`paramCurves.c.md` §40): read the notch COUNT and the FB
depth as measured, the notch POSITIONS as a fit made at one Freq, and what Spread does as a
placeholder. Worth keeping in mind before trusting a fine detail of this graph over the ear.

## 82. `render_vocoder_routing_graph()`

The Vocoder's band routing - what the manual says the original's graph shows. Each synthesis band
(along the bottom) gets a line up to the analysis band its BandSel names (along the top); Off draws
none. The default routing is sixteen verticals, as in the original. Nothing is modelled: it is the
sixteen parameters, drawn.

The box runs from 10% to 90% of the width so its sixteen columns sit exactly over the sixteen BandSel
buttons below it (each 5% wide, from 10%). The original's second Vocoder graph - a 154x12 strip at the
height of its band buttons - is not reproduced; what it draws is not established.

## 83. `env_graph_segments()`

Every envelope module as a list of segments - a width, the level it ends at, and whether it is the
sustain - which `render_envelope_graph()` draws the way the EnvADSR graph always drew itself: a rise
along the Shape's attack curve, a fall along its decay curve, anything flat as a straight line, the
sustain orange. EnvADSR became the first case of this on 2026-09-13 and draws exactly as before; the
widths never add past 0.96 for it, so the scale-to-fit never touches it. The stages are the manual's
(p.200-203):

    EnvADSR, ModADSR    A to full, D to S, sustain at S, R to zero
    EnvADR              A, then D to zero - or, in Release mode AND gated, a sustain at full then R.
                        The manual: "When in [Rel] mode the Gate/Trig button must be set to Gate,
                        else the module will still work as when in [Dcy] mode"
    EnvAHD, ModAHD      A to full, held at full for H (green: a timed hold is not a sustain), D
    EnvD                an instant rise, then D - "the attack time ... is immediate"
    EnvH                instant on, H, instant off - "immediate attack and decay times"
    EnvADDSR            A, D1 to L1, D2 to L2, R; the sustain at L1 or L2 as its Sustain button says
    EnvMulti            T1-T4 to L1-L4, sustaining at L1, L2 or L3 as selected; "Trg" is none

ModADSR and ModAHD have no Shape button: the manual fixes them at "Linear Attack & Exponential
Decay/Release", which is LinExp. EnvD's decay is exponential for the same reason ("the release is
exponential"), and EnvH has no curve to draw.

WHERE AN ENVELOPE STARTS. At zero, except EnvMulti in Normal mode, which the manual says restarts
from the L4 it last ended at; with Reset it restarts from zero. So the graph starts where a retrigger
would.

BIP AND BIPINV, on the modules that have them, keep the EnvADSR graph's reading of the manual: the
sustaining level sits at the centre (zero) and the envelope ends at the far extreme. The segment
leading into the sustain goes to zero too. EnvMulti is the exception the manual names - "full range
(-64 to 64 units) in Bipolar" - so its levels are read as bipolar and drawn as set.

WHEN THE SEGMENTS DO NOT FIT. EnvADDSR and EnvMulti can reach 1.2 of the box at full dials, so the
whole envelope is scaled to fit when it overflows. That makes one segment's width depend on the
others only then, which is the price of five timed segments in one box.

PLACEMENT FOLLOWS EnvADSR, whose face is the one the owner is happy with (2026-09-13: "well spaced,
roughly common and intuitive locations"); the rest are not finished. So every box starts 20% in, as
EnvADSR's does, in the empty band across the top, and is only as short or as low as the face in its
CURRENT state forces: EnvADR's three rows leave a strip 8.5% tall above its selectors, and EnvMulti's
Gate and AM jacks push its box down to 20%. When a face is re-laid out on EnvADSR's pattern, its row
in graphLocationList should become EnvADSR's {{20, 8}, {60, 16}}. EnvD and EnvH have two rows and no
band to use; they take the shapers' top-right corner.

## 84. `render_keyquant_keyboard()`

KeyQuant's twelve note parameters as one octave of keyboard (CT asked for it to "look a little more
like a keyboard"). They stay ordinary on/off parameters in paramLocationList - a new parameter type
is read in a dozen places, which is what broke the wave-menu attempt (§47's neighbours) - so the
Parameter Pages, the mutator and the menus still see plain switches. Only the module face changes:
`param_drawn_by_graph()` stops `render_param_common()` drawing, registering or focus-marking them
(it still records their paramRef, which other code reads), and the keyboard is their only widget.
Their rows' positions are therefore never drawn.

WHY IT HAS TO BE ITS OWN DRAWER: black keys sit over white keys, but parameters are drawn in their
own order (E, F, F#, G ... D, D#), so a white key drawn later would paint over the black key before
it. Here the whites go down first and the blacks on top.

CLICKS: within a layer the most recently registered click region wins (clickRegion.c), which is
paint order. So registering the whites and then the blacks puts every black key over the white
beneath it. Each key fills in and hands the click to its note parameter's own tParamClickCtx - the
one `render_param_common()` would have filled - so it toggles exactly as an on/off button does,
focus, undo and the wire command included.

SELECTED NOTES keep their key colour's family: a lit white key goes bright green, a lit black key
dark green. A single green for both would lose which note is sharp; keeping the whites light and the
blacks dark keeps the keyboard readable with any selection.

## 85. `render_dxrouter_algorithm_graph()`

The DXRouter's 32 algorithms drawn as the DX7's own algorithm chart: six numbered operator boxes,
carriers on the bottom row joined by the output line, each modulator above what it modulates, and
the one feedback loop in orange (G2 manual p.185: "An internal feedback loop is indicated with
orange lines in the DXRouter graph"; the algorithms "correspond to the factory algorithms of the
DX7 synthesizer"). The loop is drawn grey while Feedback is 0, so the graph also says whether the
loop is doing anything.

THE TABLE IS ONLY WHO MODULATES WHOM - one target bitmask per operator and the loop's two ends,
taken from the published DX7 algorithm chart. The picture is calculated from that rather than
stored as 32 drawings:
- level: a carrier is 0, a modulator one above the highest thing it modulates. Every DX7
  modulation runs from a higher-numbered operator to a lower one, so one ascending pass settles it.
- width: an operator with one target is that target's child; a subtree is as wide as its
  children together, at least one column. One descending pass.
- position: carriers left to right in operator order, each subtree centred over its root, children
  in operator order. An operator with several targets (6 on algorithm 19, 3 on 20) is not a child
  of any of them and sits over the middle of its targets.
That reproduces the familiar chart shapes; the worst case is six columns (algorithm 32) by four
levels (algorithms 1, 2 and 18).

THE GRID IS FIXED at six columns by four levels whatever the algorithm, with the algorithm centred
across it, so stepping through the algorithms moves boxes rather than resizing them.

THE FEEDBACK LOOP leaves the bottom of its source operator, runs right past both ends of the loop
and comes back into the top of its destination. Algorithms 4 and 6 are the two whose loop spans more
than one operator (4 feeds back from 4 to 6, 6 from 5 to 6); the rest feed an operator back to
itself.

## 86. Operator's two graphs - `operator_env_width()`, `render_operator_kbscale_graph()`

Operator is a DX7 operator (G2 manual p.184), so its graphs follow the DX7's controls rather than
the G2 envelopes'. Operator is the one module with TWO graph rows: the envelope's comes first,
because render_envelope_graph() takes a module's first row, and the level scaling's second, found
with find_graph_location_nth().

THE ENVELOPE is the rate/level kind: it starts at L4, travels to L1, L2 and L3 at rates R1-R3,
holds at L3 (the orange plateau, as on every envelope graph) and returns to L4 at R4. It goes
through env_graph_segments() like the G2 envelopes, as one more case. Two things differ from them:
- A DX rate runs the other way - 99 is the fastest - so a stage's width falls as its rate rises.
- A stage's time grows with the distance it travels (manual: "The greater the difference between
  two adjacent Level settings, the longer it will take"; equal levels make the rate irrelevant).
  So the width is that distance times a rate term, and a stage between equal levels has no width.
Levels and rates are read as 0-127. The defaults (90 99 80 99 70 99 70 0) look like DX values, but
the G2's own panel reads L1 and Level as 127 at raw 127 (read 2026-08-10), so the module is NOT
capped at the DX's 99 and a graph that clamped there flattened every setting above it. The widths show the shape and which stage is slow;
they are not measured times.

THE LEVEL SCALING draws the manual's graph: "the two gain slopes and the break point ... The Y-axis
represents the output level and the X-axis the entire note range (A-1 to C8). The horizontal line
represents the 0 dB output level." The break point is BrPt read as a note number (the dial's own
reading, C-1 at 0), placed on an A-1..C8 axis. Each side takes its curve menu (-Lin, -Exp, +Exp,
+Lin) and its depth, the depth as a fraction of that menu's top value. Linear is a straight slope
from the break point; exponential starts flat and steepens (e^4x, normalised) - the DX7's
characteristic shapes, not measured gains.

A depth reaches its full offset half the axis (about four octaves) from the break point and holds
there: measured over the whole keyboard, a side that is only half of it never got near its depth and
every curve drew nearly flat. It is a display scale, like the rest of this graph.

## 87. `render_compress_graph()` and graph handles

Compress drawn as a compressor's usual static curve - output level against input level, both -36 to
+12 dB - from compress_out_db(), the engine's own gain law with the detector settled (paramCurves.c
notes §42). The grey diagonal is unity, what no compression would give; the yellow line is RefLvl,
the level the module pulls towards (it is a leveller). Thr Off draws the unity line and greys the
two handles that have nothing to hold.

THE HANDLES ARE THE DIALS (CT: "Even better if I could click on key parts of the graph and drag to
adjust"). Three squares - the threshold knee, the curve at the right-hand edge (the ratio) and the
RefLvl line's left end - each drags its own parameter:
- Each registers a click region whose context BEGINS with that parameter's tParamClickCtx, so to the
  click registry, the right-click menu, MIDI learn and focus a handle simply is the parameter.
- A press starts the parameter's ordinary drag (param_drag_start(), shared with the widget) plus a
  pointer-to-value function and the graph's on-screen box in tParamDragging. canvas_param_drag_motion()
  calls that function instead of the dial laws, so the value follows the pointer: the threshold
  under it on the input axis (past +11 dB it goes Off), RefLvl on the output axis, and the ratio
  that would put the curve's right-hand end where the pointer is. Undo, linked variations, the wire
  command and Alt for morph are all the drag's own, at its end.
- The press does NOT capture the cursor, as the dials' vertical/horizontal modes do (canvas_drag_begin()):
  a value that follows the pointer has to see where the pointer is. The drag's end clears
  tParamDragging whole, so the function never outlives its drag.
The same mechanism is meant for the envelope graphs later (CT: "Similar could later apply to
envelopes etc."): a handle per breakpoint, a function per handle.

THE LIVE POINT (CT: "We could also use the compressor LED data"): the gain-reduction meter shows the
reduction the settings call for, dB over the threshold x (1 - 1/ratio) (sound-engine-notes §122,
measured on the G2), so its lit LEDs, turned back into reduction by compress_meter_reduction_db() and
divided by (1 - 1/ratio), give how far over the threshold the input is - an orange dot on the curve at
that input level. At 1:1 the meter shows nothing and there is no dot - where the
compressor is working now. Nothing is drawn while no LED is lit, i.e. below the threshold. It follows
whatever feeds the meter: the G2 when connected, the local engine when that is playing.

THE dB MARKS beside the gain-reduction LEDs (CT: "squeeze the dB indications onto the compressor
too ... Text indication is on original editor") are the original editor's own: 1, 4, 9, 15, 24 and
30 dB, top to bottom, each against the LED row it prints them at - drawn in render_volume_common(),
in a smaller text size, in the gap between the graph and the meter. The original's "Gain Reduction"
heading does not fit and is left off. The meter's top LED is bit 0 of its value, which is why the
scale reads downwards.

CHECKED BY A REAL DRAG (2026-09-13, cliclick on an offline instance): the threshold handle dragged
60 points right took Thr from raw 18 (-12 dB) to 35 (+5 dB), exactly what the pointer mapping
predicts for that distance across the axis.

THE ENVELOPES HAVE HANDLES TOO (2026-09-13, CT: "Similar could later apply to envelopes"). Every
segment now records the parameter that sets its width and, where there is one, the parameter that
sets its end level, and each segment with a time gets a handle at its breakpoint: sideways sets the
time, and where the level is a parameter too (EnvADSR/ModADSR's D, EnvADDSR's D1 and D2, EnvMulti's
four stages, Operator's R/L pairs) up and down sets the level in the same drag.
- NO CLOSED-FORM INVERSE: the widths are scaled to fit the box, so every other segment moves when one
  changes. env_handle_value() instead runs every value of the parameter through env_graph_segments()
  on a COPY of the parameters (which is why that function takes a parameter array, not the module)
  and keeps the one whose breakpoint lands nearest the pointer - the current value on a tie. It is
  exact whatever the law, which is how Operator's reversed DX rates need nothing special.
- THE SECOND AXIS is a second parameter in tParamDragging (graphValue2/param2), committed after the
  first so it is found against the first's new value, with its own undo step at the drag's end. It
  takes no morph; Alt-drag morphs the time only.
- Where Bip/BipInv pins the levels (sustain at the centre), the level axis has nothing to move and
  the search keeps the value.

## 88. `guard_graph_areas()`

A press on a graph but off its handles does NOTHING (CT: "Graphical representation area probably
shouldn't be recognised for module dragging. Currently too easy to miss the drag buttons and move the
whole module by accident"). Every graph row of a module registers a click region with a no-op press,
after the module body - so it wins over the body's drag - and before the graphs' own handles and
keys - which therefore win over it. Its context is kind None with the module's key, so a right-click
over a graph still opens the module's menu (canvas_right_click() falls back to it). The screen box is
mapped from the module's own drawn rectangle, the transform every other region on the face went
through.

## 89. in `render_module()`

While the sound engine runs, a module it does not play is drawn under a grey veil, last, so the whole
face - name, dials, jacks - dims. The test is `sound_engine_models_module()`: the engine's own list of
node kinds, plus Operator (played through the DXRouter it is patched into) and Name (no sound at all).
The veil registers no click region, so the module still edits and drags as before; a patch that uses
one of these still loads and plays, with that module silent. In the plug-in the engine always runs, so
the veil is always there; in the application only while Experimental > Enable Sound Engine is on.

