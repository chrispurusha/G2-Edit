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

**1.2 Capture and analysis.** `tools/capture.c` (AUHAL, chosen channels, a comment in the file), windows
aligned on a signal edge rather than the clock, noise-floor masking, clip checks, and per-setting model
fitting - all used for the G2 today (sound engine reference §10-§13, `findings.md`).

**1.3 A plug-in framework.** SynthLib's VST3 and AUv2 wrappers take a format-free description; GenBridge
is already an effect built on them.

**1.4 A starting model.** The engine's FltClassic is a 4-pole ladder whose topology and feedback range
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

## 3. The rig

**3.1 Signal path.** The Mac plays a test signal out of the audio interface into the Voyager's external
audio input; the Voyager's own oscillators and noise are off. The signal goes through its mixer, the
filter, the VCA and out. The VCA is held fully open with a note held, the envelope amount to the filter
at zero and keyboard tracking off, so the filter's controls are the only thing that changes.

**3.2 Two captures at once - before and after the filter.** Besides the Voyager's normal output, take its
UNFILTERED signal: the mixer's output ahead of the filter. Captured together on two channels:

- after / before = the filter alone, with the interface, the external input and the mixer divided out;
- before / what was sent = the input stage alone - its gain, its frequency response and where it starts
  to saturate, which tells us how much drive the filter can actually be given;
- the interface's own loopback, out to in, captured once, calibrates the rest.

Which jack carries the pre-filter signal, and whether taking it changes the path, to be settled at the
instrument before the first capture.

**3.3 Levels.** Per the G2 lesson (memory: capture drive and clipping): small-signal sweeps well below
the point where the input stage or the filter saturates, and a clip check on every capture before a fit
is trusted or distrusted.

**3.4 Alignment.** Every capture opens on silence and aligns on the first signal edge, as today's G2
captures do - capture start latency varies by a large fraction of a second.

## 4. Test signals

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

- Which Voyager output carries the pre-filter signal (§3.2), and the interface channels for send, before
  and after.
- The control resolution for cutoff over MIDI - whether 7 bits is fine enough to fit the law, or the
  sweep needs the finer form if the layout has one.
- How the VCA is held open for long captures without the envelope intruding.
- Whether a second profile (the Minitaur) follows the same model with different laws.
- Separate plug-in, or a filter type inside G2 Alike.

## 10. First steps

1. Settle the rig (§3.1-§3.2): cables, channels, levels; a loopback capture.
2. A capture script driving SynthEdit's backdoor the way `tools/`'s G2 scripts drive G2-Edit's.
3. Small-signal grid (§5.2) and the model's linear part.
4. Large-signal (§5.3) and self-oscillation (§5.4).
5. A first plug-in, and §8.
