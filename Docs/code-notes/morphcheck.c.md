# tools/morphcheck.c — notes

Offline proof that a Vel or Keyb morph reaches the sound PER VOICE. Written 2026-09-18, when a
DXRouter's Operators turned out to follow neither morph (reference §26.2.1) and there was no way to
demonstrate it but by ear.

## 1. file scope

THE TEST IS AN EQUIVALENCE, not a threshold. A morph is defined (reference §26.2) as the dial plus
range x amount, held to 0-127, where the amount is the voice's velocity or its note on the Keyb axis.
So the check is: play the note with the morph, then play the same note with the dial ALREADY THERE
and no morph at all. Those two must sound the same. Nothing about the parameter's own law has to be
known, which is what lets one tool check any parameter of any module.

THREE READINGS PER POINT, not two, and the third is what makes the verdict worth anything:

| | |
|---|---|
| `morphed` | the dial with the morph range on it, played at this velocity or note |
| `by hand` | the dial already moved to where the morph should put it, no morph |
| `no morph` | the dial left alone, no morph - what it sounds like if the morph does nothing |

`error` is morphed against by hand, and `separation` is no morph against by hand. A point where the
separation is small is one where moving the dial does not move the sound, so morphed and by hand
agree whatever the engine does: that point proves nothing and is reported INCONCLUSIVE rather than
counted as a pass. A sweep where EVERY point is inconclusive is VACUOUS and fails - the usual cause
is a parameter that this patch does not hear, or a range too small to matter. Without that column the
tool's favourite answer would be PASS.

The quantisation is the reference's, not the tool's. Both axes are tables (32 velocities, every other
note), so the morph lands on a row's amount and not on the exact one. The `by hand` dial is computed
from the ROW's amount, mirroring soundEngine.c's `velocity_row()`, `key_row()` and `axis_amount()` -
if those change, the three defines at the top of this file change with them. Otherwise every reading
would be out by up to one step and the tolerance would have to be loosened until the tool proved
nothing.

## 2. `undo_push_param_change()`

protocol.c calls it on every parameter it writes, and src/undo.c reaches into menus.c, selection.c
and the rest of the GUI. The stub here is the same idea as plugin/g2HostIo.c's null audio and MIDI
layer: the harness is not the application and has no undo stack. See do-morphcheck for why the file
list is deliberately this short.

## 3. `reading()`

A FRESH ENGINE FOR EVERY READING, and a PINNED START PHASE. Both are needed and neither is obvious.

Voice allocation round-robins, so playing the same note twice in one engine puts it on two different
voices; and every voice starts its oscillators at a random phase (sound-engine notes §63), so those
two voices sound different. Under FM the difference is large. The first version of this harness
played into one long-lived engine and its readings scattered by 40%, which reads exactly like a
half-working fix.

Restarting the engine is not enough by itself: the phase seed is deliberately never reset, so two
engines started in turn are as different as two power-ups of the instrument. `sound_engine_set_start_phase_seed()`
exists for this and for nothing else. With both in place three readings of the SAME dial agree to
every digit printed, which is the standard a comparison at 0.03 tolerance needs.

## 4-5. `sweep()` and its verdict

Seven points by default, spread across the axis: velocity 1 to 127, or note 36 to 96, which is C1 to
C6 - the Keyb axis's own 0 to 1. The first point of either axis sits at amount 0 by construction, so
it is always INCONCLUSIVE; that is correct and not a defect, and it is a useful control, since a
morph that wrongly applied at amount 0 would show up there as a separation.

## 6. the default range

A range of `-dial` from a dial at or above 64, `127 - dial` below it: the morph then lands exactly on
the far end of the dial's travel at full amount, sweeping the widest range that never clamps past it.
Clamping is not wrong - the engine clamps too - but a clamped point tests the clamp rather than the
mapping, and several in a row look like a pass.

## 7. the warm-up reading

One throwaway note before the sweep. The first note of a run measures a little low whatever the
settings - about 2% on SimpleLead - and without this the sweep's first point carries it. Cheap
insurance; the cause has not been chased, since it is gone once the first note has been played.
