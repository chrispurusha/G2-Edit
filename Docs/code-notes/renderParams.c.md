# renderParams.c notes

The longer comments from `renderParams.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `gParamRenderArea`

Which drawing area every param widget below renders into. The patch canvas draws through
moduleArea (canvas zoom + scroll applied); panels that reuse these same widgets - the
Parameter Pages panel - draw into mainArea instead. It's a mode set around the call rather
than a parameter, so that reusing the widgets elsewhere doesn't mean threading an extra
argument through all ~30 renderer signatures and their function-pointer types in
render_param_common().

## 2. `render_paramType1UniPolShort()`

0 to 64 units in WHOLE steps, which is a different control from paramTypeUniPol's half-steps
despite the similar name. The manual (p223, ValSw1-2) gives it as "Range: 0-64 units in steps of
1 unit", and the original editor prints the raw number straight out with the TOP STEP shown as
"64" — so the readout runs 0, 1, ... 62, 64 and the value 63 is never displayed. That gap is the
instrument's, not ours: the control has 64 raw positions and a range the manual calls 0 to 64.

NOT paramTypeUniPol, which halves the raw value and prints N.0/N.5 over a 128-step range. Using
that here would show 0.0 to 31.5 for a dial the panel reads as 0 to 64.

## 3. in `render_paramType1dB()`

ONE DECIMAL, UNSIGNED. Rounding to a whole dB quantised the whole dial to 37 distinct
readings where the synth shows 128, so a nudge of the Gain knob appeared to do nothing until
it crossed a decibel boundary - and it printed "+0dB" for every value from 62 to 66.

The top step is 128, not 127, which is what makes the dial reach exactly +18.0 rather than
the +17.72 that 127 gives. Confirmed for the Eq bands; LevScaler's smaller 8 dB range is
assumed to share the shape, not verified.

## 4. in `render_paramType1MixLevel()`

COMPUTED, not looked up - mix_level_db() reproduces the printed scale exactly except at
the three lowest steps, which the synth names rather than derives: silence, and two
values pinned just under -99. Computing the rest means a morphed level slides through the
decibels instead of stepping between whole dial positions.

## 5. in `render_paramType1ADRTime()`

COMPUTED, not looked up. adr_time_seconds() reproduces all 128 readings of the printed scale
exactly under the four print rules below, so nothing is lost by dropping the table - and a
morphed or smoothed envelope now sweeps instead of stepping, because an index truncated the
fractional dial values a morph produces and a curve does not.

The rules are what the scale itself asks for: milliseconds until the value reaches a second,
seconds after that, and one fewer decimal each time the number grows a digit, so the reading
stays three significant figures wide the whole way up the dial.

## 6. in `render_paramType1Partials()`

PartQuant Range — bipolar partial count, shown as value = raw - 64 (raw 64 = 0 centre, 127 = +63, 0 = -64),
with a leading '+' on positives. The manual (PARTQUANT / RANGE KNOB) adds a
'*' once the magnitude exceeds +/-32, flagging that the practical output
limit (the 32nd harmonic) has been passed.

## 7. `render_paramType1Phase()`

A Phase dial, in DEGREES - the whole 0..127 dial is one full turn, so the step is 360/128 and the
readout runs 0 to 357. The three dials that carry it (LfoShpA, LfoB, OscDual) were rendering the
raw number instead, so a phase of half a cycle read "64" rather than "180".

The synth prints no degree sign, and rounds to a whole degree, which the odd step size makes
visible: consecutive dial positions can differ by 2 or by 3.

## 8. `render_paramType1BipolarPinned()`

The OTHER bipolar 128-step dial: raw - 64 again, but with the top step pinned to a round 64 rather
than left at 63, and no '+' on positives. Both variants exist on the synth and they differ only at
that one step, which is exactly the sort of thing that cannot be inferred from the middle of the
scale - a pan knob reads +63 at the top where PShift's Fine reads 64.

## 9. `render_paramType1Bipolar()`

A bipolar 128-step dial, counted from the centre: raw 64 reads 0, the bottom of the dial reads
-64 and the top +63. Confirmed on the hardware against a MixStereo Pan knob.

The top step is +63 and NOT +64, so this is a plain raw - 64 with no correction at the end - the
opposite of the Gain and LevAmp dials, which do pin their top step to a round number. Worth
stating because the same shape appears in three places and only some of them do that.

Positives carry a '+' so that the centre reads "0" rather than "+0" and the two halves of the
dial are told apart at a glance. These were rendering as a 0-100% percentage, which put the
centre of a pan knob at "50%" and gave no sign at all.

## 10. in `render_paramType1Resonance()`

FltStatic's and DrumSynth's Res read as a PERCENTAGE, not as the Q that FltMulti, FltNord and
FltClassic show — two different controls that happen to share the label.

100/128 rather than 100/127 is what makes the scale land exactly on 25, 50 and 75 at raw 32,
64 and 96, and the synth prints those three — along with 0, and the clipped top — with no
decimal place at all, one decimal everywhere else. The curve already agreed; showing "50.0"
where the hardware shows "50" was the whole of the difference.

## 11. in `render_paramType1Slider()`

BOTH READINGS COME FROM THE HARDWARE, a SeqVal slider swept in each mode.

Bipolar: raw - 64, and the top step is 64 rather than 63 - the slider really does run
...60, 61, 62, 64, skipping 63 entirely. Same top-step pinning PShift's Fine has.

Unipolar: the 0 to 64 UNITS scale, in halves - 0.0, 0.5 ... 64.0 - not the raw 0..127 this
used to print.

"64.0" is four characters where a slider is only wide enough for two or three, so it is
SPLIT OVER TWO ROWS at the decimal point - the whole units above, the half below. That is
the same two-row treatment the old three-digit readings used, and the reason it is still
here now that a three-digit reading can no longer occur.

## 12. in `render_paramType1FreqShift()`

SUB IS THE FIRST RANGE, NOT THE LAST. The selector reads Sub / Lo / Hi in that order - checked
on the hardware - and both this switch and freqShiftRangeStrMap had it the other way round.
The two errors agreed with each other, so the module looked self-consistent while showing the
widest range's frequencies for the narrowest setting: a shifter set to Sub read up to 1568 Hz
where it actually reaches 8.78.

## 13. in `render_paramType1StandardToggle()`

BUTTON-ANCHORED, the same rule render_dial_with_text() follows: the rectangle IS the button,
and the label is drawn in the row ABOVE it. It used to be the other way round - the label at
the rectangle and the button pushed a row below it - which made the button's position depend
on whether the param happened to have a label, exactly the problem the dials had. It also
moved the button whenever a patch RENAMED the param, since that name arrives at runtime.

## 14. `render_paramType1RadioEdit()`

Channel Select radio buttons — see paramTypeRadioEdit in types.h. One button per channel, the
selected one lit, laid out four across and wrapping. The button width is the WIDEST CAPTION, so
the group grows to fit a renamed button instead of clipping it, exactly as the plain toggle does
with its string map.

## 15. in `render_paramType1RadioEdit()`

SIZED FOR THE LONGEST NAME THE G2 CAN HOLD, not for the names it happens to be holding. A
Channel Select button takes up to PROTOCOL_PARAM_NAME_SIZE characters — the manual: "the name
cannot be longer than 7 characters because of the size of the ASSIGNABLE DISPLAYS on the synth
front panel" — so the group is that wide always, whatever the buttons currently read.

It used to measure the widest CURRENT caption, which meant the group changed size, and its
click regions moved, the moment a button was renamed — and changed back if the new name was
shorter. A control that resizes under the cursor is worse than one wider than it needs to be.
eCache is safe on a string literal; the runtime captions are no longer measured at all.
What draw_button() adds around its text, asked of the library rather than named: the
DRAW_BUTTON_MARGIN constant sits in synthlibDefs.h's NON-G2_EDIT branch, so draw_button() sees
it and this file does not. draw_button_bounds() is the exposed answer — it returns the rect
draw_button() will actually draw for a given input, so a zero-sized probe yields 2 * margin.

## 16. in `render_paramType1RadioEdit()`

The cell PITCH, which has to include what draw_button() adds: it grows the rect it is handed by
DRAW_BUTTON_MARGIN on every side, so a cell of exactly textHeight drew a button four points
taller than its own row — which is why the second row of a 4x2 group overlapped the first. The
cell carries the margin, and the draw below hands over the cell MINUS the margin, so the button
lands back at exactly cell size.

## 17. in `render_paramType1RadioEdit()`

BUT NEVER WIDER THAN THE FACE. Seven of the widest glyph across four columns does not fit a
module — it ran off the right-hand edge and took two of the buttons with it. The row's own x in
the resource table says how much width is left (these rows are all anchored from the left), so
the cell is capped at what remains, shared between the columns. A caption too wide for the cell
it lands in is truncated when it is drawn, which is the price of a group that always fits and
never moves.

## 18. in `render_paramType1RadioEdit()`

THE RECTANGLE THE CALLER REGISTERS HAS TO BE THE ONE THE BUTTON WAS ACTUALLY DRAWN AT.
draw_button() returns the rect AFTER scale_scroll_adjust_rectangle(), which is the space
the click registry works in; the layout rect above is in unadjusted module coordinates.
Returning the latter registered the group somewhere else entirely — the buttons drew in
the right place and no click could ever reach them, at any zoom or scroll position.
