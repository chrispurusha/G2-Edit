# paramOverlay.c notes

The longer comments from `paramOverlay.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `MAX_PARAM_OVERLAYS`

One row per parameter, and a patch can hold MAX_NUM_MODULES modules of MAX_NUM_PARAMETERS each -
but only what is on screen is ever queued, and a parameter contributes at most three rows (it
can carry a patch knob, a global knob and a MIDI CC at once). This is sized for a full screen of
dense modules rather than for the theoretical patch; overflow simply stops queueing, which loses
labels off the bottom of a very crowded canvas rather than misdrawing anything.

## 2. `OVERLAY_BOX_ALPHA`

The label's backing box is translucent so the control stays readable underneath - the point of
these views is to annotate the patch, not to hide it, and a dial with an opaque chip over its
centre is just a blank square. Opaque enough that the text stays legible against whatever the
module's colour happens to be.

## 3. in `param_overlay_render_pane()`

gOverlayRect is the TEXT rect. draw_button() would pad it and put the text back at
+margin inside; the backing is drawn by hand here to get an alpha on it, so recover the
same padding from draw_button_bounds() and inset the box around the text rather than
letting it hang off to one side.

## 4. in `param_overlay_render_pane()`

Translucent, and nothing is ever blanked underneath: these views annotate the patch, so
the dial, button or menu box being annotated has to stay visible. That is affordable
only because the label never repeats what the widget already shows - see
param_overlay_note_param() - so the chip stays small enough to sit in a corner of a
button rather than across its face.
No blend enable/disable: blending is on for the whole session (render_backend_init()).

## 5. `queue_row()`

Queues one row over the given rectangle, the way the original editor's popup boxes sit on the
parameter rather than under it - there is no room underneath, where in a dense module the next
row is already the next widget's label.

textAnchor lines the chip up with the widget's OWN text instead of centring it. That is for the
widgets that carry text - a button or a menu box - where centring would land the chip across the
middle of the word and leave neither readable ("Semi" under a "0" reads as "S0mi"). Starting it
exactly where the widget's first character starts means the chip reads as a prefix to the word
rather than something dropped on top of it. A dial has nothing inside its circle to collide
with, so it gets the centre.
Further rows for the same parameter stack downwards from the first.

## 6. in `queue_row()`

eNoCache, NOT eCache: that cache is keyed on the text POINTER, so it is only safe for string
literals, whose address and contents travel together. Every label here is built into a caller's
stack buffer, which is the same address on every call — so the first label measured through
that buffer had its width returned for every label after it, whatever the new contents were.
The chip is sized from this, so a value that reached three digits kept the box it was given as
one and overflowed its own backing.

## 7. in `param_overlay_note_param()`

A widget that draws TEXT of its own inside the rect - a toggle, menu, enable or bypass
button. Those get the chip lined up with that text so the word stays readable. Dials and
sliders put their text in the rows ABOVE the rect, which the chip never reaches, so they
take it centred; the caller handing us a non-empty display string is what distinguishes them.

No allowance is needed for a toggle's own label row: render_paramType1StandardToggle() is
button-anchored now, so the rect IS the button whether or not the param carries a label.

## 8. in `param_overlay_note_param()`

The long-standing hover behaviour: assignment labels for the one parameter under the
mouse. Skipped outright during a cursor-hiding drag, when the reported pointer
position is a relative-delta accumulator rather than a real point and can drift over
an unrelated parameter.

## 9. in `param_overlay_note_param()`

The raw wire value ONLY. Nothing is blanked any more, so whatever the widget renders
for itself is still on screen - a dial's "554.4Hz" in the row above it, a button's
own "Semi" alongside the chip - and printing it again here would be the same string
twice. Raw and formatted still end up next to each other, which is what the original
shows; the difference is that the formatted half comes from the control itself.

## 10. in `param_overlay_note_param()`

What the parameter's current value becomes on the wire. A CC carries 0-127, so a
parameter whose range is not 128 is scaled into that.
NEEDS A HARDWARE CHECK: this assumes a plain proportional scale. The G2 may well
round differently, and the manual only says the view shows "how each knob is
actually sent and received over MIDI" without stating the mapping.
