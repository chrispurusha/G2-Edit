# Sound engine reference

How the local sound engine (`src/soundEngine.c`) models each module, and the measurements behind it.
Code comments refer here by section number (`// §3.2`). The dated history of how each result was
reached - including what went wrong on the way - is in `findings.md`.

Measured on the instrument unless marked **(from the DSP code)**; those are to be confirmed on the
hardware. "Dial" means the raw 0-127 value.

---

## 1. Level meters

**1.1 Law.** The G2's meters read the PEAK, one value per octave. Below full scale the value is
7 + the binary exponent of the peak (0.5-1 reads 7, 0.25-0.5 reads 6, under 2^-7 reads 0). At and
above full scale it skips: 1-2 reads 9, 2-4 reads 11, 4 and over reads 12 with the clip bit (0x40).
Boundaries within 0.6 dB of -6.02 dB × n; sine, saw and square agree to 0.3 dB.

**1.2 Engine.** A 200 ms peak follower per metered module, then the law above via `frexp`. Engine and
G2 agree on 67 of 80 steps of a saw sweep; the rest are one value high at boundaries, where the
engine's band-limited saw peaks a fraction of a dB higher.

**1.3 Rendering.** Low nibble = level; 1-7 green, 8-11 yellow, red above 11 or with bit 0x40.

## 2. Dials

**2.1 The value/128 rule.** A dial reaches the DSP as value/128, with 127 pinned to exactly 1 - so 64
is exactly one half. Holds for mixer Lin levels, Pan, X-Fade, the faders and their mod attenuators.
`dial_fraction()`.

**2.2 Exception.** MixStereo's pan dials divide by 127 (§5.2).

## 3. Mixers

**3.1 One node.** Every summing mixer is `eNodeMix`, driven by `kMixSpecs`: channel count, stereo,
and where each type keeps its level dials, On buttons, Inv switches, curve and pad.

| Type | Channels | Controls |
|---|---|---|
| Mix1-1A, Mix1-1S | 1 (S: stereo) | Lev, On, curve |
| Mix2-1A | 2 | Lev/On interleaved, curve |
| Mix2-1B | 2 | Inv before each Lev, curve |
| Mix4-1A | 4 | none - unity |
| Mix4-1B | 4 | Lev ×4, curve |
| Mix4-1C | 4 | Lev ×4, On ×4, pad, curve |
| Mix4-1S | 4 stereo | Lev ×4, On ×4, curve |
| Mix8-1A | 8 | pad only |
| Mix8-1B | 8 | Lev ×8, curve, pad |
| MixFader | 8 | Lev ×8, On ×8, curve, pad |

**3.2 Exp (and dB) curve.** Gain = 0.99x³ + 0.01x, x = dial/127 - `mix_level_gain()`, the same
function the dial's dB text uses. Fits 218 measured steps to 0.01 dB RMS; a pure cube is 5.8 dB out
at dial 13. The manual (p.216): Exp and dB are the same curve.

**3.3 Lin curve.** §2.1 - dial/128, 127 = 1.

**3.4 Pad.** Three positions, 0 / -6.02 / -12.04 dB, on every channel together.

**3.5 Chain.** Sums at unity after the channels and is NOT padded.

**3.6 Check.** All eleven types, 106 configurations, within 0.07 dB.

## 4. Pan, X-Fade, Fade1-2, Fade2-1

**4.1 Node.** `eNodeFade`, weights from `fade_weights()`, position u = §2.1 plus modulation.

**4.2 Laws.**

| Module | Lin | Log |
|---|---|---|
| Pan, X-Fade | 1-u and u | 1-u² and 1-(1-u)² |

Fade1-2 and Fade2-1 STEER rather than crossfade: x = 2u-1, the first side carries -x left of centre,
the second x right of it, the other is silent - so the centre is silent. Every point fits to 0.001.

**4.3 Modulation (from the DSP code).** Position += 4 × attenuator × input: a quarter of full scale
at a full attenuator sweeps the whole dial. `MOD_INPUT_SCALE`, shared with OscNoise's Width input (§8.5).

## 5. MixStereo

**5.1 Levels.** Each channel's Lev and the master follow §3.2.

**5.2 Pan.** The Log law of §4.2 with u = dial/127. The coefficients are scaled to 127² rather than
full scale, so hard left is (127/128)² - the module sits 0.14 dB below unity.

**5.3 Node.** `eNodeMixStereo`: six mono inputs, an L and an R gain per channel (12 smoothed gains,
`MAX_NODE_LEVELS`).

## 6. Basic oscillators

**6.1 One table.** OscA, OscB, OscC and OscD share the oscillator node; `kOscParams` says where each
keeps its dials and its waveform.

**6.2 Waveforms.** OscA, OscC and OscD: Sine, Tri, Saw, Sqr50, Sqr25, Sqr10 - the three squares as
fixed duties via the shared Shape (0, 0.5102, 0.8163 -> 50%, 25%, 10%). OscA keeps its choice as a
parameter, OscC and OscD as a mode.

**6.3 Pitch inputs.** Input 0 direct, input 1 attenuated by Pitch M. OscC: connectors 3 and 0. OscD:
Pitch only.

**6.4 Check.** OscC and OscD match OscA to 0.02 dB in level and 0.25 dB over ten harmonics.

**6.5 Not modelled.** FM on OscB and OscC.

## 7. Noise

**7.1 Model.** White noise through a one-pole low-pass whose corner the Color dial sets; each voice has
its own generator. Every setting's spectrum fits a one-pole within 0.5-0.7 dB.

**7.2 Table.** `kNoiseColour`: corner and RMS level at 17 settings, corner interpolated geometrically
and level in dB. Tabulated because the G2 looks the pole up in a stored table.

| Color | 0 | 16 | 32 | 48 | 64 | 80 | 96 | 112 | 127 |
|---|---|---|---|---|---|---|---|---|---|
| corner Hz | 18306 | 7664 | 3164 | 1353 | 615 | 323 | 194 | 143 | 129 |
| level dB RMS re FS | -7.7 | -9.6 | -10.2 | -9.4 | -8.6 | -9.1 | -10.7 | -12.8 | -14.8 |

**7.3 Level compensation.** The G2 adds back a share of the filtered signal growing as the dial cubed
(from the DSP code), which is why the level barely falls as the noise darkens.

**7.4 Check.** Engine and G2 meters agree at 6 of 9 settings; the brightest read one value higher in
the engine (a crest difference).

## 8. OscNoise

**8.1 Parameters.** On the instrument: Coarse 0, Fine 1, KBT 2, Pitch M 3, Tune Md 4, WidthMod 5,
**Width 6**, On 7. The module tables name 5 and 6 the other way round.

**8.2 Model.** White noise through TWO identical two-pole band-passes in series at the pitch: the
-10 dB span is 2.2-2.3 times the -3 dB width at every setting, which is what two resonators give
(one would give 3).

**8.3 Width.** Q per resonator = 3.34 × e^(0.032 × (127 - Width)): 3.3 at 127, 4.9 at 112, 9 at 96
(measured), and about 190 at 0 by extrapolation - the manual's "lively fluctuating sine". Below 80 the
band is narrower than the analysis resolves (under ~0.06 octave).

**8.4 Level.** Normalised: about -4.5 dB RMS re full scale across width and pitch (±1.5 dB).

**8.5 Width input (from the DSP code).** Width position += 4 × WidthMod × input, as §4.3.

## 9. Node structure

**9.1 Inputs.** Up to `MAX_NODE_INPUTS` (10) - Mix4-1S's eight legs and Chain pair.

**9.2 Gains.** Up to `MAX_NODE_LEVELS` (12) smoothed gains per node; only nodes that use them smooth
them.

**9.3 Output legs.** `NODE_OUTPUTS` (3). Each input reads the leg its cable's output maps to
(`srcLeg`, set when the chain is built); a source filling only two legs maps anything past its first
output to leg 1, as before.

**9.4 Sources.** `chain_has_source()` counts oscillators, Pulse, Noise and OscNoise; a chain without one is
reported as having nothing patched into it.

## 10. FltMulti

**10.1 Parameters and connections.** Freq 0, FreqM 1 (the PitchVar attenuator), KBT 2 (Off, 25, 50,
75, 100%), GComp 3, Res 4, dB/Oct 5 (0 = 6 dB, 1 = 12 dB), On 6. Inputs In, PitchVar, Pitch; outputs
LP, BP, HP - the node's three legs (§9.3).

**10.2 Filter (from the DSP code, confirmed by 10.3).** A Chamberlin state-variable filter, one per
voice, run at the engine rate:

    low  = low' + F·band'          F = 2 sin(π fc / fs),  fc = flt_cutoff_hz(Freq)
    high = drive - low - q·band'   q = 2d²(1 - F/2),      d  = 1 - Res/127
    band = band' + F·high          drive = input, × d with GComp on

(' is the previous sample.) The (1 - F/2) in q is what keeps it stable at every cutoff and
resonance - the largest pole radius over the whole range is 0.999998. The outputs are taken with a
half-sample correction the instrument builds in: the input there is the mean of two samples, which
the engine moves to the outputs instead (identical response, no marginal pole). With b = 1 - F/2:

| | 12 dB | 6 dB |
|---|---|---|
| LP | (low + 2·low' + low'') / 4 | (low + band + low' + band') / 2 |
| BP | b·(band + band') / 2 | LP₁₂ - HP₁₂ |
| HP | b·high | HP₁₂ + BP₁₂ |

On off passes the input to all three outputs.

**10.3 Measurement.** 2026-09-12 - noise through FltMulti, LP, BP and HP at Freq 40/64/88,
Res 0/64/110 and both slopes, each divided by the unfiltered noise, against 10.2 at 96 kHz:

- **Shape** within 0.5-0.6 dB mean on every output, 1.1 dB worst - the 6 dB BP included.
- **Cutoff** is `flt_cutoff_hz()`.
- **Resonance.** Q = 0.5/d² at low cutoffs. The Q the dial DISPLAYS, `flt_resonance_q()`, is a different
  number (damping span 0.9): the acoustic Q reaches self-oscillation at Res 127. `fltmulti_damping()`
  floors d at 0.02 to keep 127 finite.
- **GComp** is the drive × d. Levels against Res 0: +0.1 dB at Res 64, +1.0 dB at Res 110. GComp off is
  not measured.
- **6 dB BP** is LP - HP: |1 + ω²| over the two-pole denominator, so it is flat at Res 0 and rises to a
  peak of 2Q at the cutoff - the "strong resonant peak" of the manual.

## 11. EQs

**11.1 Gain and level.** Every EQ gain dial - EqPeak's Gain, and Lo, MidGn and Hi on Eq2Band and
Eq3band - is the dB it displays, (dial - 64) × 18/64, to within 0.5 dB. `eq_dial_gain()`. Level is the
mixer's Exp taper (§3.2), `mix_level_gain()`: Level 64 is -17.6 dB, not -6.

**11.2 Shelves (from the DSP code, confirmed by 11.6).** Low shelf y = x + (G - 1)·lp, lp a one-pole
low-pass at the Lo Freq corner. High shelf y = x + (G - 1)·hp, hp a one-pole high-pass with unity
gain at Nyquist. Lo Freq: 80, 110 and 160 Hz, as named. Hi Freq: the FIRST setting sounds at 8 kHz and
the SECOND at 6 kHz (fitted 8.1 and 6.0) - the reverse of the names the editor shows; the third
fitted 13.3 kHz, where the capture thins out, and is taken as its name, 12 kHz.

**11.3 Peak.** EqPeak and Eq3band's mid band: y = x + (G - 1)·q·bp, bp a band-pass of damping q
(peak 1/q). Centre: EqPeak `flt_cutoff_hz(Freq)`, Eq3band 100 × 80^(Freq/127) Hz. For a boost
q = 2(2^N - 1)/√(2^N) with N = (128 - BW)/64 octaves - twice the damping of a band-pass N octaves wide -
whatever the gain. Eq3band's mid has no BW dial: it fits q = 1.36, and the engine uses the formula's
1 octave, 1.41 (`EQ_MID_OCTAVES`).

**11.4 Cuts mirror boosts.** A cut is the exact inverse of the boost of the same size: the peak's
damping becomes q/G, a low shelf's corner rises to fc/G, a high shelf's falls to fc × G. A cut is
therefore far wider than the boost it mirrors. `eq_mirror_cuts()`.

**11.5 Engine filter.** The instrument's peak is the Chamberlin filter of §10.2, which is unstable once
F × q passes 2 - a deep, wide cut above about 1 kHz. The engine uses a topology-preserving SVF instead,
stable at any damping and the same response below a few kHz. What the instrument does there is not
known; the one poorly fitting measurement (Eq3band mid at -13.5 dB and 8 kHz, 1.4 dB) is such a setting.

**11.6 Measurement.** 2026-09-12 - white noise through each EQ at 43 settings, divided by the
unfiltered noise. With 11.1-11.4: EqPeak shape 0.57 dB mean (0.82 worst), Eq2Band 0.53 (0.63), Eq3band
0.66 (1.42, the setting in 11.5); level within 0.1 dB mean, 1.0 dB worst. Bypass not checked.

## 12. OscDual

**12.1 Parameters and connections.** On the instrument: Coarse 0, Fine 1, Kbt 2, PitchM 3, TuneM 4,
SqrL 5, PW mod amount 6, SawL 7, Phase 8, SubL 9, On 10, **PW 11**, Phase mod amount 12, Soft 13. The
module tables have 6 and 11 the other way round (as OscNoise has 5 and 6). Inputs Pitch, PitchVar, Sync,
PW, Phase; Sync is not modelled.

**12.2 Waveforms.** Three at one pitch, summed:

- Pulse, full scale, DC removed; duty = (1 - PW/128)/2 with 127 pinned - 50, 37.5, 25, 12.6, 3.1% at
  0, 32, 64, 96, 120, silent at 127.
- Saw, full scale, falling like OscA's; its fundamental is in phase with the pulse's at Phase 0, and
  Phase rotates it by Phase/128 of a cycle (the dial's 360/128 degrees).
- Sub-octave (12.3).

Each level is linear, `dial_fraction()`: -2.50, -6.02, -12.04 dB at 96, 64, 32.

**12.3 Sub-octave.** A square an octave down, through a FIXED first-order shelf - gain 0.38 at DC,
1.12 at high frequency, corner about 190 Hz - which fits the sub's fundamental at 41, 165 and 659 Hz to
about 0.3 dB. Soft doubles it and adds a one-pole low-pass that tracks the pitch at 1.5 × the
oscillator's (3 × the sub's): +5.4 dB on the fundamental and a further -3 dB on the third harmonic, the
same at all three pitches.

**12.4 Modulation (not measured).** PW += PW mod × input and Phase += Phase mod × input (in cycles),
each at a scale of 1 until measured.

**12.5 Measurement.** 2026-09-12 - OscDual alone against an OscA sine at the same pitch, harmonics
read at each setting; the sub at three pitches, Soft off and on.
By meter the engine reads one value above the G2 at 9 of 10 settings - the pattern of §1.2: the
instrument's waveforms sit just under full scale, the engine's band-limited edges just over. Both
order the settings the same way (Soft above plain, the 180° mix below the 0° one). The sub is fitted on
its harmonic LEVELS only; its phase, and so its peak, is not pinned - the G2 meters it below full scale
where the model peaks near 1.9. Captured peaks cannot settle it: the output path rings on hard edges
(the plain square reaches 1.69 × the sine's peak in the capture while metering below full scale).

## 13. FltComb

**13.1 Parameters and connections.** Freq 0, Pitch 1 (the PitchVar attenuator), Kbt 2 (Off, 25-100%),
FB 3, FB Mod 4, Type 5 (Notch, Peak, Deep), Level 6, On 7. Inputs In, Pitch, PitchVar, FB Mod.

**13.2 Tuning.** The comb's delay is 96000/f - 1 samples at 96 kHz, where f is the Freq curve NINE
SEMITONES DOWN, `flt_cutoff_hz(Freq - 9)`: the teeth sit a major sixth below what the dial reads. Fits
the four Freq settings measured to 0.01 samples. (An earlier reading, "nominal / 1.67", was this law
seen through the one-sample offset, which is why it drifted at high Freq.)

**13.3 Feedback.** g = (FB - 64)/64: 64 is no comb, below 64 the comb inverts.

**13.4 Types.** One section, gain k × (1 + b·z^-D') / (1 - c·z^-D'):

| Type | b | c | D' | k |
|---|---|---|---|---|
| Notch | g | 0 | D | 1 |
| Peak | -0.30 g | 0.90 g | D + 1.1 | +2.45 g² dB |
| Deep | 0.60 g | 0.85 g | D + 0.5 | -4.1 g² dB |

Notch and Peak fit every setting to the capture's noise floor (about 2 dB rms per bin). Deep fits to
the floor for |g| up to 0.5 and grows to 5 dB rms at full feedback - a single section is not its whole
structure (the DSP code reads the delay through a four-point interpolator, which a frequency-flat model
cannot show). The extra delay of Peak and Deep is part of the same story.

**13.5 Level.** The mixer's Exp taper (§3.2), as the EQs' (§11.1).

**13.6 Measurement.** 2026-09-12 - noise through FltComb against the unfiltered noise, on a LINEAR
frequency axis (a comb is periodic in linear frequency): four Freq, feedback across the dial, all three
Types, and again at Level 64 so the resonant Types could not clip. Fitted per setting with the delay
free, then jointly per Type.

