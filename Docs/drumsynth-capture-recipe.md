# DrumSynth capture recipe

What to capture off the hardware to settle DrumSynth, what each capture answers, and the traps.
Written 2026-09-19 when the module went in (reference §39); **the level sweeps were run on
2026-09-20 and the answer did not come from them** - read the next section before planning another
session.

Follows the pattern of `reverb-capture-recipe.md`; read `capture-inventory.md` first, as ever.

## THE LEVEL CURVE IS SETTLED, AND NOT BY THIS RECIPE

The whole first version of this note was one measurement: peak against dial for each level dial, to
decode the exponential curve behind them. It was run. It gave three different power laws - 3.71,
2.75 and 2.99 - each fitting under 0.9 dB rms, and all three are wrong. The curve is one curve,
`0.01x + 0.99x^3`, which is what the engine already had; the instrument's own parameter conversion
says so plainly and §39.3 now records it.

**Why the measurement lost.** Peak output of a decaying hit is the whole voice - both oscillators
summed, the bend still falling, the output stage - not the gain word, and the two dials that
disagreed are the two that interact. Noise, the one contributor that reaches the output more or
less alone, came out at 2.99.

Keep the shape of that lesson for the next module: **before designing a capture, ask what the
measurement is a function of.** If the quantity you can measure is downstream of more than the
thing you want, a good fit is not evidence.

## 2026-09-25: SETTLED from G2Demo, and two rig traps

Everything this recipe was waiting on is settled (reference §39.6, §39.9, §39.10; findings.md
2026-09-25). Two things about the rig that any further capture must allow for:

- **Desk inputs 5/6 (G2 outs 1/2) are a low shelf**, first order, zero 69 Hz, pole 197 Hz: -6 dB at
  the drum's 67-83 Hz. Inputs 19/20 (outs 3/4) are flat - capture there, or undo the shelf.
- **DEVNOTE sends no velocity**, and an unpatched drum Vel ignores the key velocity anyway (it is a
  fixed 64 units). For velocity, patch a Constant into the BOTTOM input (Vel) or use MIDI.

## What was left: the noise filter (§39.4) - SETTLED 2026-09-25

Two host-side facts disagree with the engine and neither can be captured out:

- Res is sent at `dial/512`, capped at a quarter of full scale, against the engine's 0..1.
- Noise Filter Freq reads a different cutoff table from the one §22 and §23 use.

Both words mean whatever DrumSynth's own DSP code says they mean - the module is two parts of its
own, one at 96 kHz and one at 24 kHz, so the filter is hand-written rather than one of the filter
modules. **That is a native harness, not a capture session**, of the kind §§21-25 each got, and it
settles the filter, the cutoff scale, the click and the four decays in one go.

A capture is still worth having as the CHECK on that harness once it runs - which is what the rig
below is for, and it is already built and proven.

## Before anything: what has to be true

- The G2 powered on and on USB. `system_profiler SPUSBDataType | grep -i clavia` finds it, and the
  backdoor's `COMMS` says the editor is talking to it.
- **Its OUTPUTS 3/4 into the QU-24, inputs 19/20 — zero-based 18/19, `--channels 18,19`** (2-Out Destination 1).
  Not outputs 1/2: that path (inputs 5/6) has a low shelf, and nothing on the desk's 5/6 shows why.
- `G2_EDIT_BACKDOOR=1` on the editor, so the sweeps can be driven rather than knob-twiddled.
- **Check the two input pairs against each other first.** The reverb session found 5/6 sitting
  2.6 dB below 19/20 on this desk; free to exploit, and free to be caught out by.

## The rig

Everything in the VOICE area, because DrumSynth is a voice module:

```
    Keyboard --Gate--> DrumSynth --> LevAmp x2 --> 2-Out
```

The Keyboard's Gate drives Trig, so `DEVNOTE <note> <vel> on` fires one hit and the sweep can step
a dial between hits. The LevAmp is not optional for the same reason it was not for the reverb — see
that recipe's level section; gain on the OUTPUT only.

`DEVADDMODULE` builds it, `CABLE` wires it, `DEVSET VA <index> <param> <value>` moves a dial ON THE
HARDWARE (plain `SET` is local only and would measure nothing).

## Sweep 1 — the level curve. RUN 2026-09-20; kept as the method, not as the answer

Isolate one contributor, sweep it, read the peak of each hit. This works as a procedure and the rig
is proven - but see the top of this file for why its result was not the law.

1. Slave Level 0, Noise 0, Click 0, Bend Amount 0. Master Decay around 46 (its preset default) so
   each hit has a readable peak and dies well before the next.
2. Master Freq low enough to be unambiguous — its default 42 is fine.
3. For `v` in 0, 8, 16 … 120, 127: `DEVSET VA <drum> 4 <v>`, then `DEVNOTE 60 127 on`, wait past
   the decay, `DEVNOTE 60 0 off`.
4. One capture over the whole sweep. Peak per hit against `v` IS the curve.

Then repeat for **Slave Level** (param 5, with Master 0 instead), **Noise** (14) and **Click** (13).
If all four trace the same shape, one table serves all six and §39.3 is settled. If they do not,
that is a more interesting finding than the curve.

**Velocity must be constant across a sweep** — it scales five of these six dials (manual p.181), so
a varying velocity would multiply the very thing being measured. Use 127 throughout, which the
manual says reaches the dialled setting.

## Sweep 2 — Bend Amount, which is octaves not level. NOT RUN, and no longer needed for the curve

Bend is on the same curve but its effect is pitch, so it is measured differently and independently
confirms the curve.

Slave Level 0, Noise 0, Click 0, a LONG Master Decay so the pitch fall is audible over time. Sweep
Bend Amount and measure each hit's STARTING frequency against its settled frequency; the ratio in
octaves against the dial is the curve again, scaled by the manual's 5 octaves.

## Spot checks, cheap once the instrument is on

- **The four decays. ANSWERED without the instrument 2026-09-20** - the conversion reads the
  envelope's decay-multiplier table for all four. A capture would now only confirm it.
- **The four decays, as originally planned.** Master Decay at 32, 64 and 96, measuring time to −40 dB. The engine claims
  the envelope's own table via `adr_time_seconds()`; §17.3's figures say 0.054 s, 1.014 s and
  8.22 s. Agreement confirms the reading for all four decays at once.
- **The noise filter sweep.** Sweep at 0, 64 and 127 with Noise up and the oscillators down,
  measuring the filter's starting cutoff against its settled one. The manual says 0 to 5 octaves.
- **The filter type buttons**, HP/BP/LP, one hit each — a shape check, not a number.

## Traps

- **Space the hits beyond the DECAY, not beyond the note.** The reverb recipe learned this the hard
  way with tails; a long Master Decay here is seconds, and overlapping hits make every peak a sum.
- **Do not raise velocity to get level.** It scales the dial being measured. Raise the LevAmp.
- **A peak is not an amplitude if it clips.** The engine's own preset peaks run 0.8 to 2.9 in its
  units, so the instrument may be nearer its ceiling than expected on some presets. Check for flat
  tops before fitting anything — that trap has cost this project a session before.
- **One capture per sweep, with its `.json` sidecar.** A capture without one is nearly worthless
  later; the inventory says several early ones are only recoverable from `findings.md`.

## What a result looks like

A table of dial against peak, normalised to the dial's top, for each of the four level dials. If
they coincide, §39.3 becomes a fitted closed form and the engine stops guessing — and the same
curve is what the instrument uses for these dials everywhere else it appears.
