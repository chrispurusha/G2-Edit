# Analogue filter capture - design

Living design note, started 2026-09-12. The aim: a plug-in that reproduces a synth's ANALOGUE filter,
built the way the G2 engine's modules have been - capture the real thing, fit a model, check it against
the captures. First subject: the Minimoog Voyager's filter. Nothing is built yet.

Lives under G2-Edit for now because the capture tools, the analysis and the plug-in framework are here;
it may move to a project of its own once there is code.

---

## 1. Why this is within reach

**1.1 Control.** SynthEdit already drives the Voyager's filter over MIDI (its `voyager.txt` layout), and
has a backdoor for scripted sweeps - the same shape as G2-Edit's backdoor that the G2 measurements ran on.

**1.2 Resolution.** The Voyager's filter controls are finer than a standard 7-bit MIDI controller: Cutoff and
Resonance are 14-bit MSB/LSB pairs (CC 19/51 and CC 21/53, 0..16383), as are its other continuous controls,
and the Panel Dump carries the same value shifted left 2. Hardware-confirmed 2026-07 and already how
SynthEdit sends them. So a sweep can step the cutoff law finely enough to fit it - unlike the G2's 128 steps.
A physical pot does not quite reach either end of its range; set values over MIDI, never by hand.

**1.3 Capture and analysis.** `tools/capture.c` (AUHAL, chosen channels, a comment in the file), windows
aligned on a signal edge rather than the clock, noise-floor masking, clip checks, and per-setting model
fitting - all used for the G2 today (sound engine reference §10-§13, `findings.md`).

**1.4 A plug-in framework.** SynthLib's VST3 and AUv2 wrappers take a format-free description; GenBridge
is already an effect built on them.

**1.5 A starting model.** The engine's FltClassic is a 4-pole ladder whose topology and feedback range
were settled from captures - the same family as the Voyager's filter.

## 2. What is harder than the G2

**2.1 It is not linear.** The G2 modules measured so far were fitted small-signal. A ladder filter
saturates in every stage; its resonance, its loss of bass as resonance rises, and its self-oscillation
level all depend on how hard it is driven. The model has to be fitted at several drive levels.

**2.2 The unit drifts.** A model describes one Voyager, at one temperature, on one day. Repeat a
reference measurement through each session (§5.7) to see how much.

**2.3 The path around the filter.** The interface's converters, the Voyager's external input and mixer
(which can overload before the filter does), and the VCA and output stage all colour what is captured.
§3.2 is how they are separated out.

**2.4 Fully open is not flat (to confirm).** The Voyager's filter may not fully open even with cutoff at
maximum, so "filter wide open" cannot serve as the unfiltered reference. The reference has to be a signal
that bypasses the filter altogether (§3.2), and the model has to reproduce whatever roll-off remains at the
top of the cutoff range rather than treat it as the chain's.

**First measurement, 2026-09-17** (saw of §3.1 at 64.33 Hz, cutoff 16383 AND filter envelope amount 16383
with full sustain - the widest the filter could be opened, see §2.5; resonance 0, Dual LP, Spacing 8192,
pole setting not yet read off the panel). Filtered / unfiltered at every harmonic, relative to the
fundamental, median per band (spread within a band about ±0.3 dB below 8 kHz):

| Band (Hz) | 120-250 | 250-500 | 0.5-1k | 1-2k | 2-4k | 4-8k | 8-12k | 12-16k | 16-21k |
|---|---|---|---|---|---|---|---|---|---|
| dB | -0.1 | -0.5 | -0.8 | -1.0 | -1.2 | -1.4 | -2.3 | -3.1 | -4.8 |

So fully open is NOT flat: a gentle tilt of about -1 dB by 1 kHz that no low-pass near 20 kHz would make,
then a steeper fall above 8 kHz. What is filter and what is the VCA and output stage (which the unfiltered
tap skips) is not yet separated. The filtered output sits +1.4 dB above the unfiltered at the fundamental.

**2.5 Cutoff over MIDI barely moved the filter (2026-09-17, open).** With the envelope amount at 0, cutoff
0..16383 over CC 19/51 changed the response by under 1 dB, monotonically, and it stayed a low-pass near
100 Hz falling 12 dB/octave (a 2-pole slope - the pole setting is to be checked). Resonance over CC 21/53
did act (a peak near 110 Hz), and the envelope opened the filter fully, so the filter works and the CCs
arrive. To check at the panel: the physical Cutoff knob and whether it overrides the CC, a filter CV or
pedal input, and any modulation routed to cutoff.

## 3. The rig

**3.1 Signal path - phase 1, the Voyager's own sources (owner, 2026-09-17).** No external input yet: a
MIDI note plays the Voyager's own oscillators or noise through its mixer, the filter, the VCA and out.
The unfiltered output (§3.2a) is the filter's input, captured at the same moment, so the filter's
response is filtered / unfiltered per capture and no test signal has to be sent or calibrated. The VCA is
held fully open with the note held, the envelope amount to the filter at zero and keyboard tracking off,
so the filter's controls are the only thing that changes.

- **Noise** is the linear-response source: a continuous spectrum, so filtered/unfiltered gives the whole
  magnitude curve at once (§4.3), averaged over enough of it to settle.
- **One sawtooth** gives harmonics at known frequencies and a known slope - the response sampled at the
  harmonics, and a check on the noise result. Pick the pitch so they are dense where the cutoff is.
  Oscillator 1 is set to saw, but the Voyager's Wave control is continuous, so its "saw" point may not be a
  pure one. Sweep Wave around it and pick the setting whose UNFILTERED spectrum is closest to a saw's
  (every harmonic present, falling as 1/k, even ones as strong as odd). Purity matters less than it seems -
  the ratio divides the actual input out - but a missing or weak harmonic is a hole in the measurement.
  **Measured 2026-09-17** (C3 held, Osc 1 sounding at 64.3 Hz, unfiltered channel only, harmonics 2-32
  against 1/k): Osc 1 Wave is the 14-bit pair **CC 9/41**, as `voyager.txt` has it (CC 95 not tried). Below
  about 5700 the saw is blended with triangle - every harmonic low by the same amount, -9 dB at 2048; above
  about 5800 the pulse blends in and notches walk through the harmonics. **Best saw: 5728** (MSB 44, LSB 96),
  1.0 dB rms from ideal, repeatable to 0.03 dB; a flat -1.3 dB offset (the fundamental slightly strong)
  remains at every setting and is the instrument's. On the panel's 0-127 scale 5728 is 44.75 (value / 128);
  limited to whole steps, 45 (1.33 dB) beats 44 (1.98 dB). Settings for the good saw: Osc 1 on and saw at
  Wave 5728, Osc 2 and 3 off, note C3 (MIDI 48) on channel 8 (sounds at 64.3 Hz).

  **The unfiltered output runs without a note** - the oscillators free-run at the LAST note's pitch - so a
  capture must start its analysis after the note's pitch has arrived. A "second tone" at 101 Hz on the
  first analysis was exactly that: the previous note, until the new one came in. No reply to a Panel Dump
  Request came back through the Cirklon port.

**3.1a Phase 2, external input (later).** The Mac plays a designed test signal (§4.1-§4.2) into the
Voyager's external audio input instead, for exponential sweeps and stepped sines at exact levels.

**3.2 Two captures at once - before and after the filter.** Besides the Voyager's normal output, take its
UNFILTERED signal: the mixer's output ahead of the filter. Captured together on two channels:

- after / before = the filter alone, with the interface, the external input and the mixer divided out;
- before / what was sent = the input stage alone - its gain, its frequency response and where it starts
  to saturate, which tells us how much drive the filter can actually be given;
- the interface's own loopback, out to in, captured once, calibrates the rest.

Which jack carries the pre-filter signal, and whether taking it changes the path, to be settled at the
instrument before the first capture. That separate output, bypassing the filter, is also the only honest
comparison for §2.4 - capture it alongside the filtered output at maximum cutoff to see how far the filter
really opens.

**3.2a The rig as wired (owner, 2026-09-17).** Already in place:

- Voyager UNFILTERED output -> QU-24 input 16 (channel 15 as `tools/capture` numbers them, 0-indexed);
- Voyager main output, FILTERED -> QU-24 input 24 (channel 23, 0-indexed);
- MIDI from the Mac through the Cirklon, port "Mirror" (MIDI 1), Voyager on channel 8.

So phase 1 (§3.1) needs no new cabling. Phase 2 (§3.1a) adds the "what was sent" leg - a QU-24 output
into the Voyager's external audio input, and a loopback of that output for §5.1. Before the first capture,
scan all channels before trusting the map (the G2 captures found other QU-24 channels carrying loud bleed).
The exact CoreMIDI destination name for the Cirklon port is to be read off the system, not guessed.

**3.3 Levels.** Per the G2 lesson (memory: capture drive and clipping): small-signal sweeps well below
the point where the input stage or the filter saturates, and a clip check on every capture before a fit
is trusted or distrusted.

**3.4 Alignment.** Every capture opens on silence and aligns on the first signal edge, as today's G2
captures do - capture start latency varies by a large fraction of a second.

## 4. Test signals

Phase 1 uses the Voyager's own noise and sawtooth (§3.1); §4.1-§4.2 are phase 2.

**4.1 Exponential sine sweeps.** One sweep yields the linear response AND each harmonic's response
separately (the harmonics fall at known times ahead of the fundamental), so mild nonlinearity is measured
rather than being noise in the fit. Several levels per setting.

**4.2 Stepped sines** at a few frequencies and many levels, for the compression curve directly.

**4.3 Noise**, as used for the G2, for a fast linear check on a linear frequency axis where needed.

**4.4 Silence** at high resonance, for self-oscillation.

## 5. Measurements

**5.1 Chain calibration** - loopback, and the pre-filter tap's own response (§3.2).

**5.2 Small-signal response** over a grid of cutoff and resonance settings: the poles, cutoff against the
control's value (the law, its octave slope and any offset), feedback against resonance, and the passband
loss as resonance rises.

**5.3 Large-signal behaviour** - the same grid at rising levels: the harmonic responses and the
compression give the saturation curve per stage and where in the loop it sits.

**5.4 Self-oscillation** - frequency against cutoff, amplitude, and waveform (how far from a sine).

**5.5 Modulation** - cutoff swept by the envelope at several speeds: does the filter follow at once, or
lag.

**5.6 Both filter modes** - the Voyager's dual low-pass (with Spacing) and high-pass/low-pass.

**5.7 Repeatability** - one reference setting captured at the start, middle and end of every session.

## 6. The model

**6.1 Structure, not a black box.** A topology-preserving 4-pole ladder with a saturating stage per pole
and the fitted laws of §5 - cheap to run, sweepable, and every parameter means something. A neural model
would need far more data and would hide what it had learned.

**6.2 The input stage** as its own optional block, from §3.2's before/sent measurement, so the plug-in's
Drive can reach the same overload the instrument does.

**6.3 Oversampling** - saturation creates harmonics, so the nonlinear part runs oversampled, as the G2
engine's oscillators do.

## 7. The plug-in

**7.1** An audio effect on SynthLib's wrappers: Cutoff, Resonance, Drive, the filter mode and Spacing, and
a profile per captured instrument. The AU wrapper's effect path is still unproven with real samples
(CLAUDE.md) - this would be its first real use.

**7.2** Profile names describe the filter, not a manufacturer's trademark.

## 8. Checking it

**8.1** Replay the captured test signals through the model and compare: responses, harmonics, the
compression curve, self-oscillation.

**8.2** Null tests on sweeps, and A/B by ear on real material through both.

## 9. Open questions

- The QU-24 output used to send the test signal (§3.2a); before and after are already on inputs 16 and 24.
- Whether the filter fully opens at maximum cutoff (§2.4) - first capture: bypass output against filtered
  output, cutoff at 16383, resonance at 0.
- How the VCA is held open for long captures without the envelope intruding.
- Whether a second profile (the Minitaur) follows the same model with different laws.
- Separate plug-in, or a filter type inside G2 Alike.

## 10. First steps

1. Scan the QU-24 channels with a note held; confirm unfiltered on 15 and filtered on 23 (0-indexed), and
   the Cirklon port's CoreMIDI name ("Cirklon2+Mirror MIDI 1"). Done 2026-09-17, with the saw sweep (§3.1). Then the §2.4 check: noise, cutoff 16383, resonance 0, filtered
   against unfiltered.
2. A capture script driving SynthEdit's backdoor (cutoff, resonance, mixer levels) and sending the note,
   the way `tools/`'s G2 scripts drive G2-Edit's.
3. Small-signal grid (§5.2) on noise, cross-checked on a sawtooth, and the model's linear part.
4. Large-signal (§5.3) by raising the mixer level, and self-oscillation (§5.4).
5. A first plug-in, and §8.
6. Later: phase 2 (§3.1a), external input and designed test signals.
