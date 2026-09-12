# patchAdjuster.c notes

The longer comments from `patchAdjuster.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `name_is()`

─── Classification ──────────────────────────────────────────────────────────

Which of the eight knobs owns a parameter. The original does this by the G2's own
parameter-TYPE id, each knob claiming a set — Attack 0x0b; Decay 0x0c,0x10; Sustain 0x0d; Release 0x0e; Mod Rate
0x16,0x41,0x88; Resonance 0x12,0x87; Timbre 0x05,0x06,0x11,0x22,0x8b; Effects 0x29,0x83).
G2-Edit's paramLocationList does NOT carry those ids — it has its own tParamType plus a label —
so the ids can't be used directly and this classifies on (module type, param type, label)
instead, the same way mutator.c's classify_param() already does.

THAT MAKES THIS AN APPROXIMATION OF THE ORIGINAL'S COVERAGE, NOT A REPRODUCTION OF IT. The
manual's own descriptions are loose in the same places ("various waveshape parameters", "some
Time parameters in the multi-stage envelopes"), so exact parity was never reachable from the
documentation either. Where a parameter's role is unambiguous from its type — a filter cutoff,
a resonance, an LFO rate, an envelope stage — it is claimed. Where it would take a guess, it is
left alone: a knob that misses a parameter is a smaller sin than one that mangles an unrelated
one, since every move here is invisible until you hear it.

Known gaps, all of the same kind — a parameter whose module gives it meaning but whose type is
the catch-all paramTypeCommonDial:
```
  * LfoB's rate (two undistinguished CommonDials; which is the rate isn't derivable here)
  * FM amounts and shaper amounts, which Timbre should claim per the manual
  * dry/wet on FX modules that don't label it "Dry/Wet"
```
Extending this is a matter of adding module-name cases below, exactly as the Mutator does.

## 2. in `adjuster_classify_param()`

── Envelope stages ───────────────────────────────────────────────────
The stage is carried by the LABEL, not the type: paramTypeADRTime is every envelope time
there is. The multi-stage envelopes use T1..T4 / D1 / D2, which the manual folds into Decay
("some Time parameters in the multi-stage envelopes").

## 3. in `adjuster_classify_param()`

── Timbre ────────────────────────────────────────────────────────────
"Filter cutoff frequencies, FM modulation amounts, various waveshape parameters in
oscillators and shapers, as well as various FX parameters affecting the timbre of the
sound." Cutoff is the unambiguous part and the one that carries the effect; the FM and
shaper amounts are the gap noted at the top of this section.

## 4. `adjuster_apply()`

Recomputes every classified parameter from the baseline and the current knob positions, and
sends only those whose value actually changes. The "only what changed" test is the original's
too, and it matters: a Timbre sweep over a dense patch touches a lot of parameters, and without
it every mouse-move would re-send all of them.

Sent one parameter at a time rather than as a whole-patch write, deliberately — the opposite of
the bulk MIDI CC tools. send_param_value() is a COMMAND_WRITE_NO_RESP write that expects no
acknowledgement, which is what the canvas's own dial drags have always used, so there is no ack
to be left in the pipe and no patch-version race to lose (see the note in menus.c).

## 5. in `render_patch_adjuster_panel()`

Always legible, never greyed. This doubles as the manual's clickable centre marker
("just turn the knob back to its middle position, or click the centre marker"), so
drawing it faintly at centre — which is exactly when you want to aim at it — would
hide the control at the moment it matters. Off-centre it goes black to stand out.
