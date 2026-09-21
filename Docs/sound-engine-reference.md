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
engine's band-limited saw peaks a fraction of a dB higher. A Voice Area module is metered from the sum
of the voices after their fades, an FX Area one from its own output (notes §191) - for one note the two
are the same; what the G2 shows for a chord is not measured.

**1.3 Rendering.** Low nibble = level; 1-7 green, 8-11 yellow, red above 11 or with bit 0x40.

## 2. Dials

**2.1 The value/128 rule.** A dial reaches the DSP as value/128, with 127 pinned to exactly 1 - so 64
is exactly one half. Holds for mixer Lin levels, Pan, X-Fade, the faders and their mod attenuators.
`dial_fraction()`.

**2.2 Exception.** MixStereo's pan dials divide by 127 (§5.2).

**2.3 The filter Freq curve, and what it is nine semitones away from (2026-09-18).** `flt_cutoff_hz()`
is 13.75 × 2^(dial/12), and it is the instrument's own curve read off a SHIFTED index. The table every
filter's cutoff comes from holds one entry per semitone - the Chamberlin coefficient 2·sin(π·f/96000) -
and **entry 64 is E4, 329.628 Hz, exactly**. So it is a PITCH table
about E4, not a dial table - which is why the same E4 pivot turns up in filter key tracking (§21.3).

Written as a frequency, entry k is 13.75 × 2^((k - 9)/12) Hz, to 0.005% at every entry. So
`flt_cutoff_hz(dial)` is that table at `dial + 9`: the filters index it nine semitones above
the dial, and the measurements (§10.3, §21, §22, §23) are what say that offset is right for them.

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

**3.2 Exp (and dB) curve. EXACT - confirmed against the instrument's own table, 2026-09-18.**
Gain = 0.99x³ + 0.01x, x = dial/127 - `mix_level_gain()`, the same function the dial's dB text uses.

This began as a fit to 218 measured steps (0.01 dB RMS; a pure cube is 5.8 dB out at dial 13) and is
now known to be the instrument's law rather than a good approximation to it. Its level table is
4065 entries, reaching full scale at index 4064 = 127 × 32, so it is indexed by the dial at 1/32
resolution - the level a mixer sends the DSP is the morph accumulator's, not the integer dial, and the
part looks the curve up. Read back at every integer dial value it agrees with the formula to **0.006 dB
at worst**, at dial 1 where the table itself quantises, and to 0.000 dB everywhere else.

So nothing here needs changing, and the formula is if anything better than the table: a morphed level
moves through 4065 steps on the instrument and continuously here. The manual (p.216): Exp and dB are
the same curve.

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

**6.2 Waveforms.** OscA, OscC and OscD: Sine, Tri, Saw, Sqr50, Sqr25, Sqr10. OscB: Sine, Tri, Saw, Sqr,
DualSaw. OscA keeps its choice as a parameter, OscC and OscD as a mode. The node's `shape` is the PULSE
OFFSET y (0..1): Sqr50/25/10 are y = 0, 0.5, 0.875 (duties 1/2, 1/4 and 1/16 - the manual's "10%" is
1/16, measured 2026-08-24); OscB's is its Shape dial as a word, dial/128 with 127 = 1.

**These three numbers are the instrument's own** and are not to be "corrected" - CONFIRMED a second
way 2026-09-20. Selecting a waveform writes two words: which wave the oscillator runs, and, for the
pulses only, a threshold the phase is compared against - 0, half scale and 0.875 of full scale,
exactly the y above. The pulse is high while the phase is past that threshold, so the duty is
(1 - y)/2 and the third one really is 1/16. The manual's "10%" is a nominal label, the arithmetic
invites changing 0.875 to 0.8, and both the hardware measurement and the instrument's own constants
say not to. See the DO NOT RE-TRY list.

**6.3 The waves (measured 2026-09-17).** One cycle, phase 0..1, all peak 1:

| wave | law |
|---|---|
| Sine | `wave_sine_polynomial()` of the triangle below, uncorrected - the odd fifth-order polynomial Sine2 uses (§27.2), so it starts at -1 |
| Tri | -1 at phase 0, +1 at 0.5, each corner rounded by `osc_corner()` |
| Saw | RISES: 0 at phase 0, +1 just before 0.5, steps to -1, back to 0 at 1 |
| Sqr | +1 from y/2 to 0.5, -1 elsewhere, plus y - so it carries no DC, and is silent at y = 1 |
| DualSaw | Saw + Saw a further y/2 of a cycle on: twice a saw at y = 0, a saw an octave up at y = 1 |

Every step is spread over a triangle `OSC_EDGE_SAMPLES` (2) of the instrument's 96 kHz samples either side
- the polyBLEP form stretched to 2 x inc96 per side, inc96 being the phase step per 96 kHz sample. That is
the whole harmonic roll-off: each harmonic sits sinc^2(2F/96000) under 1/n. At 2 kHz the 10th harmonic is
5 dB down, where a one-sample edge at the engine rate had put it. Each triangle corner gets the same
triangle's rounding, x (2 - d) min((2 - d)^2, L) / 6 with x = 2 inc96 and d the distance in 96 kHz samples;
the limit L is the instrument's arithmetic saturating, and differs by module: 2 on OscA/OscB, 1 on OscC/OscD
(`OSC_CORNER_LIMIT_*`, each fitted to within 0.3 dB).

These waves run once per engine sample, with no oversampling of their own: the instrument draws them for
its 96 kHz sample and needs none, and the engine is at 96 kHz behind a 48 kHz device. There the output is the
instrument's own sample for sample (checked to its 24-bit rounding); the edges are set in time, so at other
rates the waves keep their shape. OscDual and OscShpB still run oversampled (notes §154).

Measured on the hardware with OscB into 2-Out at eight pitches (110 Hz-12.5 kHz), Shape swept 0-127 on Sqr
and DualSaw, and OscC's Saw, Sqr50/25/10 and Tri at four pitches; the chain was calibrated by the Sine at the
same eight pitches. Every harmonic below 16 kHz agrees within 0.1 dB, apart from the triangle limits above.
At Shape 127 the instrument's Sqr leaves a single-sample click at -38 dB per harmonic, which is not modelled.

**6.3a Start phase.** Each oscillator free-runs from a random phase drawn when the patch is built (and again
on every topology change); a note-on never resets it (notes §63).

**6.4 Pitch inputs.** Input 0 direct, input 1 attenuated by Pitch M. OscC: connectors 3 and 0. OscD:
Pitch only.

**6.5 Not modelled.** FM on OscB and OscC; OscB's Shape modulation input; Sync.

**6.1a WHAT THE TUNE DIAL MEANS: the Pitch Type drop-down (2026-09-19).** Four settings on OscA,
OscB, OscC, OscNoise and OscDual, and the engine read all of them as Semi - it logged "PitchType %d
not supported" and carried on, so any oscillator not set to Semi played at the wrong pitch. CT found
it on 02 Big Pad, whose two oscillators are both Partial.

All four end as a `basePitch` on the Semi scale where 64 is unity, so the keyboard tracking and
everything downstream are untouched - a frequency ratio is an offset in semitones. The laws are the
ones the dial itself prints (renderParams.c), shared rather than restated:

| | Tune means | basePitch |
|---|---|---|
| 0 Semi | semitones, 64 unity | `tune` |
| 1 Freq | 8.1758 Hz to 12.55 kHz absolute | from `osc_freq_hz()` |
| 2 Factor | 0.0248x to 38.072x of the note | `64 + 12 log2(factor)` |
| 3 Partial | 0 silent; 1-32 sub-audio hertz; 33-63 the ratio 1:(65-tune); 64-127 the ratio (tune-63):1 | `64 + 12 log2(ratio)` |

**Freq and Partial's sub-audio end are ABSOLUTE, so they ignore the key**: Kbt is forced off for
those rather than left to the button, which is what a fixed frequency means. Partial at 0 silences
the oscillator.

02 Big Pad's oscillators have Tune 63 in Partial, which is 1:2 - an octave below the note. They were
playing at 63.07 on the Semi scale, a semitone below unity; they now play at 52.07.

**OscD has no Pitch Type at all** - five parameters, and 3 is its "Pitch" mod dial. The engine's
table said its pitch type was parameter 3, which read that dial as the type. Harmless while anything
above Semi was refused and wrong the moment this section started acting on it, so that entry is -1
now, meaning always Semi. Whether the dial is OscD's PitchVar attenuator, and so belongs in the mod
slot, is still open - see todo.md.

## 7. Noise

**7.1 Model.** White noise through a one-pole low-pass whose corner the Color dial sets; each voice has
its own generator. Every setting's spectrum fits a one-pole within 0.5-0.7 dB.

**7.2 Table.** `kNoiseColour`: corner and RMS level at 17 settings, corner interpolated geometrically
and level in dB. Tabulated because the G2 looks the pole up in a stored table.

| Color | 0 | 16 | 32 | 48 | 64 | 80 | 96 | 112 | 127 |
|---|---|---|---|---|---|---|---|---|---|
| corner Hz | 18306 | 7664 | 3164 | 1353 | 615 | 323 | 194 | 143 | 129 |
| level dB RMS re FS | -7.7 | -9.6 | -10.2 | -9.4 | -8.6 | -9.1 | -10.7 | -12.8 | -14.8 |

**7.2a The instrument's module, exactly (2026-09-18).** Read from its own DSP part and the host code
that feeds it - the whole module, not a fit:

    x    = 24-bit LFSR, shift left, XOR the tap mask X[0] when the bit shifted out is 1,
           then sign-extended: white noise at full scale
    y    = clamp24(A·y' + B·x)                       the one-pole, Q23 throughout
    out  = clamp24(y · (1 + 32·C))                   the level compensation

with the three coefficients written by the host on every Color change:

| | value | |
|---|---|---|
| A | the colour table at `127 - dial` | the pole - note the REVERSED index |
| B | `(0x7fffff - A) / 4` | so the one-pole's DC gain is exactly 1/4, -12.04 dB |
| C | `dial³ × 4` as a Q23 word | so the compensation is `1 + dial³/65536`, 0 dB at dial 0 to +30.2 dB at 127 |

That last row is 7.3's "growing as the dial cubed", now exact - and it IS the filtered signal that is
scaled, not a share of the dry mixed back. The P-code word decides: one value outputs zero (the
module off), another applies the compensation, anything else passes `y` through unscaled.

**The corner** is a clean geometric run from **exactly 20000.0 Hz at dial 0 to exactly 12.000 Hz at
dial 127** (ratio 1.0602 a step) - the round endpoints are what confirm 96 kHz is the right rate.

| dial | 0 | 16 | 32 | 48 | 64 | 80 | 96 | 112 | 127 |
|---|---|---|---|---|---|---|---|---|---|
| instrument's pole, Hz | 20000 | 7855 | 3085 | 1212 | 476 | 187 | 73 | 29 | 12 |
| measured (7.2), Hz | 18306 | 7664 | 3164 | 1353 | 615 | 323 | 194 | 143 | 129 |

The two agree to a few per cent while the noise is bright and diverge to a factor of TEN by the top of
the dial. THE ENGINE STILL USES THE MEASURED TABLE, deliberately. Putting the whole model above through
the same arithmetic gives a level curve whose SHAPE follows the measured one to about 1 dB over dials
0-64 and then drifts apart, reaching 6 dB by 127 - the same half of the dial the corners disagree on -
over a constant 12 dB offset that is B's own 1/4 and is presumably the output stage the capture was
referred to. So the bright half is confirmed and the dark half is not, and the measured table is what
currently reproduces the captured levels. Adopting the model wholesale would move the noise by that
12 dB and change the dark end by more, on an explanation nobody has heard yet: it wants a listening
check against the G2 first (todo.md).

**7.3 Level compensation.** The G2 adds back a share of the filtered signal growing as the dial cubed
(from the DSP code), which is why the level barely falls as the noise darkens - but see 7.2a, which
casts doubt on whether it is the filtered signal that is added back.

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
    high = drive - low - q·band'   q = 2d²(1 - F/2),      d  = 1 - 0.99·Res/128
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
  number (damping span 0.9): the acoustic Q reaches self-oscillation at Res 127. `fltmulti_damping()` is
  the instrument's own: 0.01 exactly at 127 (adopted 2026-09-13; was 1 - Res/127 floored at 0.02 - the
  same to 0.02 through the middle, 1.9 dB more peak at Res 110).
- **GComp** is the drive × d. Levels against Res 0: +0.1 dB at Res 64, +1.0 dB at Res 110. GComp off is
  not measured.
- **6 dB BP** is LP - HP: |1 + ω²| over the two-pole denominator, so it is flat at Res 0 and rises to a
  peak of 2Q at the cutoff - the "strong resonant peak" of the manual.

**10.4 FltStatic (from the DSP code, adopted 2026-09-13).** The filter of 10.2 with one output, chosen by
FilterType, and its own damping and drive:

- **Damping** d = 1 - Res/128, zero at 127 on the instrument; the engine stops at 0.01, FltMulti's top
  (`fltstatic_damping()`). The acoustic Q is 0.5/d², as the peaks measured in 2026-08 show (paramCurves.c
  notes §12).
- **Outputs** LP and BP are 10.2's 12 dB outputs; HP is `high`. The drive is 1, except BP, which is held to
  a unity peak while d² >= 1/2 (drive 2d², up to Res 37), and HP, whose drive is × (1 - F/2 - F²/4).
- **GC** multiplies the drive by d, as FltMulti's GComp does - except BP while d² >= 1/2.
- **Checked** against the DSP code's arithmetic run sample by sample (Freq 30-100, Res 0-110, GC off and
  on): LP and BP within 0.1 dB, HP within 0.3 dB below 2 kHz and about 1 dB at Res 110 or 4 kHz - its HP
  tap has a term the engine does not model. Very low corners differ by the instrument's own fixed-point
  error, which is not modelled either.

Until 2026-09-13 the engine ran FltStatic as a separate state-variable section, tuned about π times
(1.65 octaves) above its dial, always low-pass whatever FilterType said, with d = 1 - Res/127 (revert
record 20).

## 11. EQs

**11.1 Gain and level.** Every EQ gain dial - EqPeak's Gain, and Lo, MidGn and Hi on Eq2Band and
Eq3band - is the dB it displays, (dial - 64) × 18/64 with 127 the full +18, to within 0.5 dB. `eq_dial_gain()`. Level is the
mixer's Exp taper (§3.2), `mix_level_gain()`: Level 64 is -17.6 dB, not -6.

**11.2 Shelves (from the DSP code, confirmed by 11.6, EXACT since 2026-09-18).** Low shelf
y = x + (G - 1)·lp, lp a one-pole low-pass at the Lo Freq corner. High shelf y = x + (G - 1)·hp, hp a
one-pole high-pass with unity gain at Nyquist. Both corner tables are now read from the instrument
rather than fitted, and both agree with the 2026-09-12 capture:

| setting | Lo Freq | Hi Freq |
|---|---|---|
| 0 | 80 Hz | **8 kHz** |
| 1 | 110 Hz | **6 kHz** |
| 2 | 160 Hz | 12 kHz |

THE HIGH SHELF'S FIRST TWO ARE SWAPPED IN THE INSTRUMENT ITSELF, and this is not ours to fix. Its
coefficient table holds 8000, 6000, 12000 Hz in that order; its own display text reads "6 kHz",
"8 kHz", "12 kHz". So a G2 set to the setting labelled 6 kHz filters at 8 kHz. Our engine follows the
table and our dial follows the text, which is exactly what the instrument does on both counts - do
not "correct" either side to match the other. The capture fitted 8.1, 6.0 and 13.3 kHz; the first two
were right and the third was the capture thinning out, the table saying 12 kHz as its name does.

**11.3 Peak.** EqPeak and Eq3band's mid band: y = x + (G - 1)·q·bp, bp a band-pass of damping q
(peak 1/q). Centre: EqPeak `flt_cutoff_hz(Freq)`, Eq3band 100 × 80^(Freq/127) Hz. For a boost
EqPeak's q = 2√2 × (1 - BW/128), whatever the gain - the instrument's own law, adopted 2026-09-13
(`eq_peak_bw_damping()`). The measured fit it replaced, 2(2^N - 1)/√(2^N) with N = (128 - BW)/64
octaves (twice the damping of a band-pass N octaves wide), agrees at BW 64 and within 4% elsewhere.
EqPeak's CENTRE, SETTLED 2026-09-18: 20 × 800^(Freq/127) Hz, 20 Hz at 0 to 16 kHz at 127
(`eq_peak_centre_hz()`), which the instrument's own coefficient table matches to 0.0074% across all
128 values. It is NOT `flt_cutoff_hz()`, the filter modules' curve, which the engine and the dial both
used before and which is up to 45% away - the two agree only near Freq 73, which is why the capture
fit could not separate them. Eq3band's mid has no BW dial: it fits q = 1.36, and the engine uses the
formula's 1 octave, 1.41 (`EQ_MID_OCTAVES`); its centre, 100 × 80^(Freq/127), was already the
instrument's.

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

**12.5 On the instrument's laws (2026-09-17, supersedes 12.2-12.3 where they differ).** From the instrument's
own part, run as a harness: the square is LOW from phase 0 to 0.5 - PW/256 (dial 127 = silent), DC-free; the saw
RISES and steps at phase Phase/128 (Phase dial through dial/128, 127 = 1); the sub is a square an octave down,
low first; all three with the two-sample edge of §6.3; Soft = one-pole, coefficient 8 inc96, times 2. NO shelf on
the sub: the measured 190 Hz shelf was very likely the capture chain's own high-pass (§6.3 found -4 dB at 110 Hz on
that chain). PW input reaches 4x the dial's range (OSCDUAL_PW_DEPTH), the phase input 2x; over-range PW wraps.
Runs at the engine rate. NOT YET compared sample for sample with the harness - see todo.md.

## 13. FltComb

**13.1 Parameters and connections.** Freq 0, Pitch 1 (the PitchVar attenuator), Kbt 2 (Off, 25-100%),
FB 3, FB Mod 4, Type 5 (Notch, Peak, Deep), Level 6, On 7. Inputs In, Pitch, PitchVar, FB Mod.

**13.2 Tuning.** The comb's delay is 96000/f - 1 samples at 96 kHz, where f is the Freq curve NINE
SEMITONES DOWN, `flt_cutoff_hz(Freq - 9)`: the teeth sit a major sixth below what the dial reads. Fits
the four Freq settings measured to 0.01 samples. (An earlier reading, "nominal / 1.67", was this law
seen through the one-sample offset, which is why it drifted at high Freq.)

**AND THE NINE SEMITONES ARE NOT THE COMB'S (2026-09-18).** `flt_cutoff_hz(Freq - 9)` is exactly
the instrument's own cutoff table (§2.3) read at the dial, with no offset at all
(§2.3, 0.000% at every dial from 0 to 122). The other filters read the same table at `dial + 9`. So
the comb is the module that indexes it straight, and the "major sixth below what the dial reads" is
the OTHER filters' offset seen from here, not a property of the comb. The magic number was measured
before the table was read; it is the same number either way, and now it has a reason.

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


## 14. Operator and DXRouter

**14.1 One node.** A DXRouter and the Operators patched into its six inputs are played as ONE node
(`eNodeDx`). Their FM runs through the router in both directions - Operator out to router in, router
out to Operator FM - which a chain of separate nodes cannot evaluate: `add_node()` would recurse round
the loop until its depth guard stopped it. `dx_build()` finds the Operator feeding each router input
and copies its settings into the snapshot's `dxOp[]` (six per router, `MAX_DX_OPERATORS` 24 = four
routers). Each sample the operators run 6 down to 1: every DX7 modulation goes from a higher-numbered
operator to a lower one, so each operator's modulators are already computed when it runs. The Main
output is the carriers' sum. An Operator NOT patched into a router is not played. The algorithm table is
the published DX7 chart, shared with the DXRouter graph (paramCurves.c `dx_algorithm()`).

What the node reads from the voice rather than from cables: gate and pitch (as `eNodeEnv` does - the
Keyboard module's outputs ARE the voice). NOT MODELLED: the Freq, Pitch and AMod inputs, Vel (the
engine has no velocity), KBEnv, and the router's Out1-Out6 as outputs anywhere else.

**14.2 Levels and the envelope - the DX7's laws, NOT MEASURED ON THE G2.** Level and L1-L4 are 0.75 dB
a step below 99 (99 is full scale; 0 is silence); values above 99, which the G2 accepts and displays,
are held at 99. The envelope moves in dB, linearly, at each stage's rate towards its level: L4 -> L1 at
R1, L2 at R2, L3 at R3, held there while the key is down, then L4 at R4. A rate is a full 96 dB sweep
taking 40 s at 0, halving every 6.5 steps to 1 ms at 99. RateScale speeds the rates up the keyboard,
by 2^(RateScale/7 x (note - 60)/24). Sync restarts the phase at each note.

**14.3 FM depth and feedback - UNMEASURED.** A full-scale signal at an FM input moves the phase by one
cycle (`DX_FM_CYCLES_PER_UNIT`). Feedback 1-7 feeds the operator's last two outputs, averaged, back
into its target at 2^(Feedback - 8) of that, so 7 is half a cycle (pi) - the DX7's arrangement; 0 is
off. Algorithms 4 and 6 feed back across operators (4 to 6, 5 to 6), the rest into the same operator.

**14.4 Keyboard level scaling - shape from the DX7, size UNMEASURED.** Each side of BrPt (read as a
note number) takes its curve (-Lin, -Exp, +Exp, +Lin) and depth; a full depth is 24 dB four octaves
from the break point. Linear is a straight slope, exponential e^4x normalised.

**14.5 Main output - UNMEASURED.** The carriers are summed and divided by how many there are, so a
six-carrier algorithm is no louder than a one-carrier one. The DX7 sums without dividing; how the G2's
DXRouter scales its Main output has not been measured, and this is the first thing to check against a
capture. Frequencies: Ratio mode is the played note (Kbt on) or E4 (off) times Coarse/Fine by the DX7's
law (paramCurves.c notes §41); Fixed is 1/10/100/1000 Hz times Fine; Detune is taken as 1 cent a step.

## 15. Voicing: Mono, Legato, stealing and glide

How the G2 decides which voice a key sounds on, and what sounds when a key comes up. The engine does
this itself: every note-on and note-off reaches it as played, from the MIDI keyboard (noteStack.c), the
computer keyboard and the plug-in alike, and none of them chooses a note on its behalf. The G2 behaves
as described here with its own keyboard; NOT YET CHECKED BY EAR against the engine (to-test.md).

**15.1 The keys held.** A count per key (`gKeyHeld`), kept on the audio thread by `voice_note_on()`
and `voice_note_off()`. A note-off clears its key whatever the count, and all-notes-off clears the lot.

**15.2 Mono and Legato: the newest key sounds, and a release goes back to the HIGHEST key held.** Not
the most recent: hold G, play C over it, then E, let E go and G sounds, not C. The return happens only
when the key let go is the one sounding - releasing a key held underneath changes nothing but 15.1.
Both modes play one voice (notes §68). In Mono every change of note restarts the envelopes: a key
played over a held one and a return to a held key alike (`trigger`, notes §71 and §189). Legato
restarts on neither - the note moves, gliding if Auto glide is on - and restarts only for a key played
with nothing held. A key played with nothing held restarts in both.

**15.1a Which free voice a note takes: ONE QUEUE (settled 2026-09-19 against the instrument's own
allocator).** The instrument holds its voices in a single linked queue with a count of how many are
in use. A note takes the one at the FRONT. A note-off unlinks that voice and relinks it at the BACK
there and then, and decrements the in-use count in the same breath - **whether or not its release is
still sounding**. There is no second list and no test anywhere for whether a voice is still making a
sound: released IS available, and the queue order alone decides, so the voice a new note takes is
the one released longest ago.

`queueOrder` is that position. It starts in voice order, and every release - a key up, the sustain
pedal lifting, an all-notes-off - sends the voice to the back.

**Two earlier versions, both wrong, both audible.** The first returned the first voice that was
neither sounding nor gated, which in a phrase of separated notes is voice 0 every time, so one voice
played the whole part. The second (the same morning) picked the least recently used of the SILENT
voices and only then fell back to the released-but-ringing ones. That reads the `sounding` flag,
which is a rendering flag rather than an allocation one - and 179 clears it on voice 0 the moment
that voice's key comes up, so voice 0 looked free while it was still audibly releasing. Once the
other thirteen had each been used, EVERY note landed on voice 0 and cut its own tail off, with
thirteen voices sitting idle. CT heard that on 02 Big Pad as stealing after a few notes; the voice
count was never the problem, and 02 Big Pad does ask for and get its 14.

Two things follow, and both are worth keeping in mind before adding a cleverer rule: the allocator
must not consult anything about audibility, and a flag that the render owns must not be read here.

**15.3 Poly stealing.** A new note takes a free voice first: one doing nothing, then the longest
released. With every voice held it steals the oldest, unless that voice has the lowest note held and
the new note is higher, when the next oldest goes instead - the manual's "it will try to keep the lowest
note sounding" (Voice allocation and polyphony). A repeated note-on for a key whose voice is still
releasing takes a fresh voice and lets that release ring on; a note-off closes every voice on its key.

**15.3a A STOLEN voice gets a GATE CYCLE, and nothing else (settled 2026-09-19 against the
instrument's own allocator, and CONFIRMED on the hardware the same day).** Where the free list is empty, the allocator takes the voice, writes a
**zero into that voice's gate word**, runs the DSP far enough that the gate has been seen down, and
only then writes the new note's pitch and velocity and raises the gate again. That is the whole of
it. No envelope is reset, no level is touched, and nothing is faded.

What the stolen note then does is the patch's own business, not the allocator's: the gate falling
puts every envelope into its release stage, and the gate rising restarts the attack - from the level
it is at (17.3, Normal) or from zero (17.7, Reset). So a stolen note attacks from zero only where
the envelope is set to Reset, and the instrument has no rule that says otherwise.

**Mono is not exempt.** A Mono patch has one voice, so a second key with the first still held finds
nothing free and goes down this same path - and in Mono the result is indistinguishable from an
ordinary retrigger, which is what it is.

**Legato skips it entirely.** The gate is not taken down for a steal, and it is not re-raised while
any key is still held; only a note arriving with nothing held raises it. So the voice simply changes
note with the gate never falling, and no envelope restarts. That is Legato, and it comes out of the
allocator rather than being a special case anywhere else.

`VOICE_STEAL_GATE_TICKS` is how long the engine holds the gate down: one envelope tick, which is the
least that guarantees every envelope has seen it. The instrument waits a whole DSP pass. Both are
tens of microseconds and neither is audible as a late note.

**Two wrong turns on the way here, both recorded because they are easy to take again.** The first
read "writes a zero" as zeroing the envelope LEVEL and reset every envelope, accumulator, tick and
stage on a steal - which takes a held voice's output to nothing between one sample and the next, and
is exactly the click CT then heard. The second tried to cure that click with a 5 ms fade before the
note trigged. Neither is anything the instrument does. On 02 Big Pad the reset shows as a slope
discontinuity about sixty times the local slope four hundred microseconds into the steal; the gate
cycle is continuous through the join, sample for sample.

**15.4 Patch glide is CONSTANT RATE.** The manual (Patch Settings, Glide): "the greater the distance
between two subsequent notes, the longer the glide time", 19 ms to 6.27 s per octave. So the voice moves
at 12 semitones per glide time, whatever the interval. Normal slides every note from wherever its
voice was; Auto only when the voice was taken from a held key or sent back to one (`glideActive`,
notes §70); Off jumps. Until 2026-09-13 this was an exponential approach, which covered a semitone and
two octaves in the same time.

**15.6 Patch Vibrato.** A sine shared by every voice, free-running. Rate: 3.97 to 7.96 Hz,
`vibrato_rate_hz()` (paramCurves.c notes §45). Depth: 1 cent a step of the Amount dial at the peak,
scaled by the chosen controller (aftertouch or wheel) - 100 is a semitone either way at full
controller. Checked 2026-09-13 against the instrument's own rate and depth law: the depth agreed
within 1% already; the rate was a straight 4 to 8 Hz, within 0.7%, and is now exact.

**15.5 Not modelled.** The sustain pedal holding keys (on the G2 a sustained key stays held until the
pedal lifts; here sustain is only its morph group), velocity, and the Hi and Lo keys a MonoKey module
reports (Docs/mini-emulator-engine-plan.md).

## 16. Signal units at the dials and inputs

Corrected 2026-09-13. A signal of 1.0 in the engine is 64 units on the G2.

**16.1 Constant.** Its Bip/Uni switch reads 0 for BIPOLAR (bipUniStrMap), which the engine had the wrong
way round: every Constant set to Bipolar played as Unipolar and the reverse. Bipolar is (value - 64)
units and Unipolar value / 2 units, 127 reading exactly 64 in both - the same as the dial's own display
(renderParams.c `render_paramType1BipLevel()`). The engine's old Bipolar formula, value/127 x 2 - 1,
was also half a unit high at the centre, which on a Pitch input is half a semitone. paramCurves.c
`constant_level()`.

**16.2 Pitch inputs are one unit a semitone** (manual, Definitions and Signal types): a full-scale
signal moves an oscillator 64 semitones, and a Keyboard Note output played through a Pitch input with
KBT off plays in tune. The engine took full scale as 12 semitones (notes §14). Oscillators' Pitch and
PitchVar inputs, and FltMulti's and FltComb's, all move five times as far as before for the same signal.

**16.3 EnvADSR Sustain** is the dial over 128, 127 reaching exactly full level (`dial_fraction()`), as
the other level dials are; it was over 127, a fraction of a percent high everywhere below the top.

## 17. Envelopes (EnvADSR)

The instrument's law, adopted 2026-09-13; the time law and the curve constants agree with the
2026-08-24 capture (paramCurves.c notes §5). Each stage runs on the level it is at.

**17.1 Times.** One law for all three dials, adr_time_seconds() (0.5 ms to 45 s). The instrument steps
its envelopes at 24 kHz and a LINEAR attack adds a whole increment of full scale per step, so its time
is the dial's rounded to the increment below: exact to 0.002% up the dial, and long at the top (1.025 s
at 64, 34.95 s at 120, 49.9 s at 127). The other stages stretch or shrink at long settings too - 17.3.

**17.2 Shapes.** LinExp and LinLin attacks rise in a straight line, full scale in the attack time.
LogExp's is a one-pole aimed at 16/15 of full, arriving at full on time; ExpExp's grows sixteen-fold
over it - both ENV_RISE_SHARPNESS, ln 16. Decay and release (all but LinLin) are pure exponentials,
40 dB in the dial's time (ENV_FALL_SHARPNESS, ln 100): decay towards Sustain, release towards zero.
LinLin falls in a straight line at full scale per dial time, so a decay to a high Sustain is quick.

**17.3 Integer steps, from where it is (adopted 2026-09-14, from the DSP code, run tick by tick).**
Each stage is one recurrence on the current level, in the instrument's own integers, once per 24 kHz
tick, the level holding between ticks:

    level' = target + max(0, add + floor(2 · half · (level - target) / 2^23))

with half, add and target 24-bit words from its tables - each our time law rounded DOWN. The attack
aims at 0 with add the table's step (Log: half = e^(-ln16/n)/2, add = (16/15)(1 - e^(-ln16/n)); Exp:
e^(+ln16/n)/2 and (e^(ln16/n) - 1)/15; linear: 1/2 and 1/n, n = time × 24000) and ends when it passes
full scale. Decay aims at Sustain and release at zero, with half = floor(e^(-ln100/n))/2, or for LinLin
a whole-increment fall. `env_rates_build()`, `env_segment()`. The engine reproduces the instrument's
code exactly at every tick (the shortest settings to 1e-5 of full scale, where our closed form and its
table differ by a few steps).

The rounding is audible only at long settings, and in both directions:

| Dial | 64 | 96 | 112 | 120 | 127 |
|---|---|---|---|---|---|
| LogExp attack | 1.032 s | 9.55 s | 26.1 s | stops at 0.969 | stops at 0.955 |
| ExpExp attack | 1.028 s | 9.17 s | 23.2 s | 35.3 s | 62.9 s |
| Decay / release to -40 dB | 1.014 s | 8.22 s | 19.1 s | 27.0 s | 37.0 s |
| the dial's time | 1.023 s | 8.72 s | 21.2 s | 32.0 s | 45.0 s |

A LogExp attack from about 118 up never reaches full scale, so its decay never starts while the key
is held; long falls speed up as they near zero, where rounding down is most of each step.

Every stage runs from the level it is at: a retrigger during a release rises from there and arrives
sooner (notes §150), and Sustain can move while a key is held and the level follows it. The gate is
read at the tick and takes effect from the next one. The output is the level itself: full scale is 64
units (1.0 here) and Sustain v/128 of it - checked against the instrument's code.

Until 2026-09-13 the stages were fixed-length ramps with a fall sharpness of 4.32, normalised to reach
zero at the dial's time - decay and release came out a constant 6% slow at every setting. From then
until 2026-09-14 they were floating-point recurrences with the exact law (revert record 27).

**17.4 Gate (2026-09-16).** The envelope is gated when KB is on and the voice's key is held, or when
its Gate jack is above 0 (manual p.197). KB is the keyboard gate, not key tracking. Only a keyboard
gate restarts the attack on a new note while the gate is already high; the jack restarts it on its own
rising edge. With KB off and nothing in the jack the envelope never fires, as on the instrument. The
one departure: a jack fed by a module the engine does not play yet (the Keyboard module's Gate, most
often) counts as the keys, so those patches keep sounding. The Gate jack reads its source per voice,
so an LFO gating it sounds only on a voice that is running - voice 0 at rest in drone mode.

**17.4a AN ENVELOPE'S THREE INPUTS ARE FOUND BY ROLE, not by position (2026-09-19).** The engine
took the first three input connectors as In, Gate and AM. That is EnvADSR's layout and **only**
EnvADSR's: every other envelope orders them differently, and ModADSR puts its four mod jacks in
between, so its audio In sits at connector 5 and its AM at 6.

| | In | Gate | AM | positional read |
|---|---|---|---|---|
| EnvADSR | 0 | 1 | 2 | correct |
| EnvADR, EnvMulti | 1 | 0 | 2 | In and Gate swapped |
| EnvAHD, EnvD, EnvH, EnvADDSR | 2 | 0 | 1 | all three wrong |
| ModADSR | 5 | 0 | 6 | In and AM never looked at |
| ModAHD | 4 | 0 | 5 | the same |

So the eight envelopes that started playing with 17.9 were all reading the wrong jacks, and the
consequence is bigger than a wrong modulation: the engine follows the chain BACKWARDS from the Out,
so an audio In it never looks at is a chain that stops dead at the envelope. That is why 01 Mini
Emulator built ten nodes - the FX area and the envelope - and reported "Nothing is patched into
it" with its whole voice area unbuilt. It builds 80 now.

The module role table already named all three for every type ("VCA Inputs", "Trig & Gate Inputs",
"Amp Inputs"), so this is a lookup rather than a new table. `env_input_connectors()`.

**17.5 AM (2026-09-16).** The envelope's level is multiplied by its AM jack, four times the jack's word
on the instrument, so 64 units (1.0 here) is full level; the product is held to ±1, and an unpatched jack
is full. The Env output carries the product and the VCA output is the audio times it. This is how a
patch makes an envelope velocity sensitive: a Keyboard velocity output into AM (manual p.197).

**17.6 Output Type (2026-09-16).** With L the level times AM (17.5) and S the Sustain level, the Env
output is:

| Type | Output | Idle | Peak (AM full) |
|---|---|---|---|
| Pos | L | 0 | +64 |
| PosInv | 1 - L | +64 | 0 |
| Neg | L - 1 | -64 | 0 |
| NegInv | -L | 0 | -64 |
| Bip | L - S | -S | 64 - S |
| BipInv | S - L | +S | S - 64 |

The bipolar pair are offset by Sustain, not by half scale: the sustain stage sits at 0 units, and AM
does not scale the offset. The VCA output is the audio times this output, so PosInv, Neg, Bip and
BipInv pass audio while no key is held. `env_output()`; matches the instrument's code to 5e-7 at every
type, three Sustain settings and two AM levels.

**17.7 Normal/Reset (2026-09-16).** Reset puts the level to zero in the tick the gate rises, and the
attack runs from there; Normal (17.3) runs it from where it is. Matches the instrument's code tick for
tick through a retrigger during the release.

**17.8 How long 45 s is.** The dial says 45.0 s at 127 and the table's rate falls 40 dB in 44.7 s, but
each step rounds down, which matters most near silence: the instrument's code reaches -40 dB in 37.0 s,
-60 dB in 40.2 s and exact silence in 40.5 s (at 96: 8.2, 10.6 and 10.9 s against the dial's 8.72 s).
The engine runs the same arithmetic, so it does too.

**17.9 All nine envelope modules, from one stage map (2026-09-19).** EnvADSR was the only envelope the
engine knew: every other envelope module failed `module_kind()`, so `add_node()` refused it and it fell
out of the chain entirely - whatever it fed got nothing. All nine now play.

THE STAGES COME FROM THE MAP THE FACE ALREADY USED. `env_stage_map()` (paramCurves.c) describes every
envelope module as a list of segments - which parameter sets each time, which sets each level, and
where the held one is - and it moved out of the drawing code so both can read it. One description, so
a face and the sound cannot disagree about what an envelope does.

| | stages |
|---|---|
| EnvADSR, ModADSR | A, D, hold, R |
| EnvADR | A, R - and a hold between them in Release mode while gated |
| EnvAHD, ModAHD | A, H, D - no hold, a one-shot |
| EnvD | to full at once, then D |
| EnvH | to full at once, H, then off at once |
| EnvADDSR | A, D1, D2, hold, R - the hold at L1 or L2 as its own switch says |
| EnvMulti | four segments to L1-L4, the hold wherever Sustain names |

A segment rises or falls depending on where the one before it left off, and takes the attack curves of
§17.1 or the decay of §17.2 accordingly. A held segment is not advanced INTO while the gate is up,
which is how the ADSR decay goes on running toward the sustain level and never finishes - exactly what
it did before. The gate falling jumps past the held segment; with no held segment there is nothing to
jump past and a one-shot runs to its end, which is what EnvAHD and EnvD want.

LEVELS COME FROM THE DIAL, not from the map: the map's own level is a DRAWING level (value/127) and
the dialled one is §16.3's value/128. A held segment sits at whatever the segment before it reached,
which is also where §17.6's bipolar types centre.

Checked: SimpleLead and Dx.pch2 render bit-identically to the engine before the change, so EnvADSR and
the DX envelopes are untouched. Re-typing SimpleLead's amp envelope to each module in turn, the eight
others all produced sound where every one of them had been silent.

NOT YET CHECKED. The KB gate and Reset are read at EnvADSR's own parameter numbers only; the other
modules number theirs differently, so they gate from the key and never reset. A segment that RISES to
an intermediate level - EnvMulti's alone - uses the attack curve toward that level, which is a
reasonable reading and not one taken from the instrument. Neither the stages nor the curves have been
heard against a G2.

**17.10 The Mod envelopes' time-mod jacks, added 2026-09-20.** ModADSR and ModAHD give each of
their time dials a mod AMOUNT and a jack of its own, and the engine had neither: the node took
only In, Gate and AM, so every one of those jacks was ignored and the dial alone set the time.
That is not a corner: 01 Mini Emulator sets its Filter Env's Decay and Release THROUGH them, from
a Constant and a ValSw - the Minimoog's decay switch - so the filter's sweep was wrong in every
variation while the envelope in isolation was exact (CT, 2026-09-20: "the filter modulation is
different engine vs. G2 ... seems to be a chain of modules driving it").

**The law.** A control signal of 1.0 is 64 units, and

    effective dial = dial + units x (amount / 64)

clamped to 0..127, with the time then read from the dial's own table as usual. At an amount of 64
one unit moves the dial one step; at 127 it moves it just under two. Measured on the hardware
(Constant -> Decay Mod, amount 64, decay to -20 dB with the note held):

| mod input | G2 | engine before | engine now |
|---|---|---|---|
| -32 units | 28 ms | 500 ms | 32 ms |
| -16 units | 136 ms | 500 ms | 136 ms |
| 0 | 500 ms | 500 ms | 500 ms |
| +16 units | 1536 ms | 500 ms | 1536 ms |
| +32 units | 3620 ms | 500 ms | 4104 ms |

The instrument's own conversion agrees: the A, D and R mod amounts reach the DSP as a word linear
in the amount dial, and the Sustain mod amount as the dial over 128. So the law is linear in the
amount, which is what the table above measures at one amount and what the engine implements.

**Where the jacks are.** One connector past the dial each modulates, on both modules - so the node
puts them at input `ENV_INPUT_MOD + p` for the module's parameter `p`, and `env_stage_map()` in
paramCurves.c records each segment's mod-amount parameter beside its time parameter.

**Rebuilt on the envelope's tick, not per sample**, and only while a jack is patched and actually
moving the dial off its own setting - so an unmodulated envelope costs nothing and pays exactly
what it did before. The stage's words are recomputed from the effective dial into a scratch
segment, which is why nothing per voice had to be stored.

**NOT yet checked on the hardware:** the Attack and Sustain mod jacks, and ModAHD's. They share
this one path, so the decay measurement exercises the mechanism, but neither the attack's curve
under a mod nor the sustain's own law has been measured. to-test.md.

## 18. Pulse

**18.1 Width.** The Sub range's width in 96 kHz samples is the dial's displayed time (the Lo display,
over ten) less two samples: a cubic in ln over dial/127 (`pulse_time_seconds()`, notes §73). Within two
samples of all 17 widths measured on the instrument (8 at dial 0 to 96083 at 127). Lo and Hi are ten
and a hundred times Sub (manual). The old fit was 4% out at worst and could not reach dial 0.

## 19. StChorus

The instrument's own law, adopted 2026-09-14 (from the DSP code, run sample by sample). The engine
reproduces that code to 64-76 dB below the signal across the whole of both dials - the rest is the
instrument's fixed-point rounding - and it agrees with every earlier measurement of the module.

**19.1 Taps.** Two per channel, swept in opposite directions by one triangle u (0 to 1 and back):
tap 1 = 505 - 504u and tap 2 = 65 + 378u, counted in 96 kHz samples (5.26 down to 0.01 ms, and 0.68
up to 4.61 ms) - tap 2 moves three quarters as far as tap 1. Each position resolves to 1/32 sample and
is read by 4-point Lagrange interpolation, the sample just written counting as 0 samples ago. The
right channel reads the LFO a quarter cycle on.

**19.2 Rate.** The LFO is a signed 24-bit phase (one cycle is 2^24) stepped at 24 kHz by
Detune × 8 × (1 + trim/4): 0.01144 Hz a step, 1.453 Hz at 127. trim is a random fraction in [-1, 1)
drawn for each instance when the patch loads, along with the starting phase, so two StChorus modules
in one patch run at different rates, up to 25% apart. The engine draws both from the node index
(`chorus_reset()`), so a render repeats.

**19.3 Mix.** Dry × (1 - a/2) plus each tap × a/2, a = Amount/128 with 127 = 1: unity at Amount 0,
dry and the two taps equal at the top.

**19.4 Against the measurements.** Tap spans measured 0.049-5.305 and 0.734-4.620 ms (within 1%); rate
1.3905 Hz at Detune 127, which is one instance's trim (× 0.957 of 19.2); the per-tap wet/dry ratio
within 1-4% of 19.3 at every Amount measured. The one disagreement was the overall level: the fitted
law it replaced sat +3 dB above the input at Amount 0, where 19.3 is exactly unity. CONFIRMED ON THE G2
2026-09-14: the level is the same with the module bypassed and at Amount 0 (CT; a first reading of
+1.8 dB was spoiled by transients), so the fitted level was the capture rig's reference.

## 20. Reverb

The instrument's own network, adopted 2026-09-14 (from the DSP code, run sample by sample). At the
engine's 96 kHz (a 48 kHz device) the engine is exact: every word of its delay memory and every output
word equals the instrument's, over every value of Time, Brightness and DryWet in all four rooms,
driven by an impulse and by a stereo noise burst. It replaces the fitted model (revert record row 29),
which matched the captures' decay and colour but could not match their stereo structure.

**20.1 Network.** One ring of 32768 words at 96 kHz. A cursor moves one word per sample, and every
read in a sample happens before any write. In order:
- **Input.** The network hears (L + R)/2, through Brightness's one-pole lowpass,
  out = y0 × in + y1 × previous out, which has unity gain at DC.
- **Pre-delay.** 1060 samples.
- **Diffusion.** Four allpasses in series, with gains 0.63, 0.63, 0.63 and y8 and alternating signs.
  Each allpass's output is a stored word before it is reused.
- **Tank.** A figure-8 of two halves. Each half starts with the diffused input plus y4 × what arrives
  from the other half. It then runs through:
  - an allpass (y8);
  - a delay read through a moving tap (20.4);
  - a second allpass (y9, opposite sign);
  - the damping one-pole, out = y5 × in + y6 × previous out. Its gain at DC is the decay gain d5, and
    Brightness sets its corner.
- **Outputs.** Seven taps per channel, at positions not shared between the channels. Each channel
  has one tap at y2 = 0.3 and six at ±y3 = ±0.3 d5. In tap order the signs are:
  - L: − + − (y2) + − +
  - R: (y2) + − + − + −

**20.2 Positions.** Each place in the network sits int(room × K + 1200) − 144 + step words ahead of
the cursor. The room factor is Small 0.78, Medium 0.98, Large 1.19 and Hall 1.31. `kRvPlace` in the
code holds the K and step values:

| place | K, step | place | K, step | place | K, step |
|---|---|---|---|---|---|
| pre-delay out | 0, +3 | tank A in | 1000, −1 | tank B in | 11651, 0 |
| AP1 out | 110, −1 | AP-A in | 1004, 0 | AP-B in | 11655, 0 |
| AP2 in | 110, 0 | AP-A out | 1677, −1 | AP-B out | 12393, 0 |
| AP2 out | 255, 0 | moving tap A | 1677, −127 | moving tap B | 12393, −127 |
| AP3 out | 532, −1 | AP-A2 in | 4425, 0 | AP-B2 in | 15080, 0 |
| AP4 in | 532, 0 | AP-A2 out | 6726, −1 | AP-B2 out | 17536, 0 |
| AP4 out | 921, 0 | damping A | 7300, 0 | damping B | 18600, 0 |
| | | | | tank B out | 22599, 0 |

The output taps have step 0 unless noted:
- L: K = 3589, 6307, 8992, 12398 (step +110), 15537, 18693, 21432.
- R: K = 1680, 5403, 7347, 10589, then 14470, 17021 and 19561 (each step −1).

The first output reaches R before L (Small: 11.75 against 12.89 ms), because R's taps sit earlier.

**20.3 Coefficients.** Time and Brightness enter as v/127.
- L = int(room × 22599 + 1200) − 1200 is the longest tap's length.
- t = 3 (room index + 1) × Time/127.
- d5 = 10^(−3 L / (6 t × 96000)), and 0 at Time 0.
- d8 = 1 + 99 × Brightness/127, and d6 = d8/100.

The coefficients are:

| | value | | value |
|---|---|---|---|
| y0 | 0.7 d6 | y5 | d6 × d5 |
| y1 | 1 − y0 | y6 | 1 − d6 |
| y2 | 0.3 | y7 | 0.63 |
| y3 | 0.3 d5 | y8 | 0.75 d5 + 0.4, clamped to 0.45-0.62 |
| y4 | d5² | y9 | 0.55 d5 + 0.25, clamped to 0.30-0.48 |

The arithmetic is single precision as the instrument does it: d8, t, d5, d6 and each coefficient
are rounded to a float, and y1 is subtracted in float. Each coefficient is then truncated to 23
fractional bits. Each of those roundings is one step of the coefficient grid, and each one mattered
(20.6).

**20.4 The moving taps.** A signed 24-bit phase gains 174 every sample, so one cycle is
2^24/174 = 96420 samples (0.996 Hz).
- The triangle is the size of the phase plus 174, taken before the phase wraps and held at 2^23 − 1.
  On the one sample where the phase wraps, the triangle stays at full scale rather than reading the
  wrapped value.
- Both taps read 76 × triangle/2^23 samples behind their place (up to 0.79 ms). They take the
  whole and 23-bit fractional parts of that, and interpolate linearly between the two neighbouring
  words.
- The two halves' taps move together, in phase.

**20.5 Mix.** x = DryWet × 65536, with 127 giving 2^23 − 1.
- wet = min(1, 2x)² and dry = min(1, 2(1 − x))², with x as a fraction of 2^23. Each is truncated to
  23 bits and held below 1.
- Both are full at 64; at 0 there is dry alone, and at 127 wet alone. The fitted law cubed both ramps.
- Each output is dry × that channel's own input + wet × that channel's tap sum. The network hears the
  average of the inputs, but the dry path does not.
- Bypassed, each input passes to its own output.
- The engine models the first Reverb in a chain only; a second one passes its inputs straight through.

**20.6 Quantisation.** A word is 24 bits: engine value v is stored as floor(v × 2^21)/2^21, held to
[−4, 4 − 2^−21] (full scale 1.0, headroom ×4).
- The inputs are words on arrival.
- Every ring store is a whole multiply-accumulate, rounded down once.
- The seven output taps are summed whole and rounded down once, and so is each output.

At Brightness 0 the tail is a few tens of words RMS, so a difference of one word is already 40 dB
down. An error of one coefficient step, or one rounding done the other way, spreads through the whole
tail. The engine is exact only at 96 kHz. At other rates the positions and the LFO step scale by
rate/96000, rounded, which is close but not identical.

**20.7 Against the captures.** The Time law is linear and the room ratios are exact. The onsets
agree (Small L 1237 samples measured, 12.89 ms). The captured decay times run 6-12% longer
throughout, which comes from their early-decay fits over about 15 dB of tail, not from the module.

## 21. FltClassic

The instrument's own filter, adopted 2026-09-14 (from the DSP code, run sample by sample). The engine
agrees with that code to 75-114 dB on a full-scale saw at every slope and resonance below
self-oscillation. It replaces the shared ladder (revert record row 30), which put the pole in the
right place but lacked the zeros, the clean input stage and the Pitch input.

**21.1 Loop.** Each sample at 96 kHz, with the pole p and the zero z from 21.2:
1. x = in − 8k × s4, clipped to ±4 (four times full scale).
2. u = (25/64)(x − x³/48), a clean cubic below a few times full scale.
3. The four stages:
   - s1 ← p s1 + (1 − p) u
   - s2 ← p s2 + (1 − p)(s1 + z × the previous s1)
   - s3 ← p s3 + (1 − p)(s2 + z × the previous s2)
   - s4 ← p s4 + (1 − p) s3

The resonance always comes from s4, and the slope picks the output: 24 dB s4, 18 dB s3, and 12 dB
s2 + z × the previous s2. The gain is unity at DC, since (25/64)(1 + z)² = 1 at z = 0.6. The two
zeros are what keep the top octave below the plain one-pole cascade the engine ran before.

**21.2 Coefficients.**
- a = π f/fs, held at 0.69 (21.1 kHz), where f is the dial's 13.75 × 2^(v/12) plus modulation.
- p = 1 − 2a + 2a² − (4/3)a³, a third-order e^(−2a).
- z = 0.47 + 0.13 × min(1, 2p). That is 0.6 up to about 10.6 kHz, then falls, so the passband dips by
  up to 1.5 dB at the very top.
- 8k = Res × 0.03345, which is 4.25 at 127 and self-oscillating near the top of the dial.

**21.3 Modulation.** Each input adds semitones to the Freq dial:
- the modulated input, × the Env amount's v (127 counts 128 on the instrument; the engine uses 127);
- the Pitch input, × 64, with no knob. The engine ignored this input before.

**KBT** adds (note − 64) × the KBT fraction (0, ¼, ½, ¾ or 1) in semitones. Note 64, E4, is the
instrument's pitch zero: each voice's pitch is counted from it, which is also why Coarse 64 plays the
key pressed. The engine pivoted on middle C until 2026-09-14, so at 100% every note was 4 semitones
bright, with its self-oscillation 4 semitones high. This applies to every filter with a KBT control.

**21.4 Arithmetic.** The engine runs the loop in the instrument's own numbers:
- one unit is a quarter of the engine's range;
- the coefficients are the instrument's words;
- the sum runs unbroken through all four stages, as the DSP's does, and each value it stores is
  rounded down to 23 bits.

A decaying tail therefore reaches exact silence. A filter at full Res with no input stays quiet until
something rings it, as on the hardware, rather than growing its own residue into oscillation.

## 22. FltLP and FltHP

The instrument's own filters, adopted 2026-09-14 (from the DSP code, run sample by sample). The engine
matches that code to 0.01 dB across every slope and cutoff tried.

**22.1 Coefficient.** h = sin(π f/fs) at the Freq dial, which is the Chamberlin half-coefficient table,
× 2^(modulation/12), held at 1. Modulation and KBT scale the dial's coefficient rather than moving the
dial, so at the top of the range they carry on past it until the coefficient saturates.

**22.2 FltLP.** One to six identical one-poles, y += 2h(x − y), with 2h held below 1. Near the top of the
dial the stages pass everything. There is no saturation short of the word (±4).

**22.3 FltHP.** One to six identical one-poles, y = p y′ + d(x − x′), with p = 1 − 2h and d = 1 − h. This
is unity at Nyquist.

**22.4 Against the old law.** The engine had g = 1 − e^(−ω), which put the corners low: 1.3 dB at the
cutoff for 12 dB/oct at 4.4 kHz, and 4.8 dB at 24 dB/oct at 7.9 kHz, where it was 10 dB low at 4× the
cutoff. Its FltLP also went through the ladder's soft knee, compressing a full-scale input.

## 23. FltNord

The instrument's own filter, adopted 2026-09-14 (from the DSP code, run sample by sample). It is not a
ladder. It is FltMulti's Chamberlin state-variable filter (§10.2): one stage for 12 dB, and two of the
same type for 24 dB. The engine matches that code to 0.1 dB over every type, both slopes, GC on and
off, and Res 0-116, from Freq 70 up. Below that, the instrument's own fixed-point noise is what differs.

**23.1 Stage.** x is the mean of this and the previous input sample:

    low = low′ + F·band′        high = x − low − q·band′        band = band′ + F·high,    F = 2h

The outputs:

| Type | Output |
|---|---|
| LP | (low + low′)/2 |
| BP | (1 − h)·band |
| HP | y = 2(1 − h)·high − 0.9·y′ |
| BR | y = 2(x − q·band′) − 0.9·y′ |

At 24 dB a second stage of the same type follows. Band-reject stays a single stage. The type order is
LP, BP, HP, BR.

**23.2 Coefficients.**
- h = sin(π f/fs) × 2^(modulation/12) (§22.1), held at 0.6368, i.e. 20.8 kHz.
- d = 1 − 0.99 × Res/128, with 127 counting as 1, so d = 0.01 at the top. Band-reject uses
  1 − 0.5 × Res/128.
- q = 2·qb·(1 − h), where qb = d² at 12 dB and max(d², 0.7071·d) at 24 dB. That floor keeps the
  cascaded pair from ringing twice as hard.

**23.3 The 0.9 on HP and BR.** Their output is 2(…)/(1 + 0.9 z⁻¹): 1.053 at DC and rising towards
Nyquist, a small built-in lift of the top.

**23.4 GC** is the drive × d, as FltMulti's GComp. As Res rises it lowers what goes in, so the resonant
peak stays level rather than the passband (−40 dB at Res 127).

Not modelled: the FM-lin and Res-mod inputs.

## 24. DelayA and DelayB

The instrument's own delay, adopted 2026-09-14 (from the DSP code, run sample by sample). The engine
reproduces that code word for word: every output sample is identical for a click and a noise burst,
over Time 0-127 in two ranges, FB 0-127, and LP, HP and DryWet across their dials.

**24.1 Loop.** Each sample:
1. Memory takes input/2 plus the previous sample's feedback.
2. The tap reads Time × step samples back. The step is 378, 756, 1512 or 2041 for the 500 ms, 1 s, 2 s
   and 2.7 s ranges, so 127 steps land a hair over the range. Time 0 is no delay at all.
3. The tap goes through the LP and the HP.
4. That filtered signal is both the wet output and, × FB, the next feedback.

So even the first repeat is filtered, and repeat n has been through the filters n times. FB is v/128
with 127 = 1, so the loop does not decay at all at the top. The Time readout's extra sample (0.01 ms
at raw 0) belongs to the display, not the audio.

**24.2 Filters.** Both sets of words come from the host's own 16-bit-half arithmetic.
- **LP:** a one-pole, y += c(x − y), with c = (0.1473 + 0.8527 v/127)³. That is about 50 Hz at LP 0,
  3.3 kHz at 64, and wide open (c = 1) at 127.
- **HP (DelayB only):** two states A and B, with F = (v/256)³:
  - t = B + F·A
  - high = x − 3B + F(B − A)
  - then A ← t, B ← B + F·high
  - out = (1 − F − F²)·high

  F = 0 passes the signal unchanged.

**24.3 Memory.** 16-bit: each 24-bit word's low byte is masked on the read. The loop runs at half
scale, so the memory resolves steps of 2^-12 of full scale.

**24.4 Mix.** wet = min(1, 2x)² and dry = min(1, 2(1 − x))², with x = DryWet/128 and 127 = 1. Both are
full at 64. The fitted law it replaces cubed both ramps.

**24.5 Arithmetic.** The engine runs the tap in the instrument's integer arithmetic, on half-scale
24-bit words. Every sum of products is rounded down once, as the DSP's accumulator does, and that
rounding is what sets the lowest HP settings and the long tails at high FB.

**24.6 Against the earlier measurements.**
- FB, measured at 0.497 / 0.741 / 1.000 for 64 / 96 / 127, is v/128.
- The repeat level is identical at DryWet 64 and 127.
- The LP knee near 3.5 kHz at 64 is this one-pole.
- "Only two repeats survive at LP 0" is the 50 Hz bottom, which the fitted law had at 660 Hz.

**24.6 DelayB's modulation inputs.** With either control input patched, the instrument adds a
modulation part and routes FB and DryWet through it:
- **FB** = max(0, FB + 4 × FB-mod input × FB-mod amount).
- **DryWet** = max(0, DryWet + 4 × input × amount), mixed LINEARLY: wet = min(1, 2x) and
  dry = min(1, 2(1 − x)), not the squared ramps of 24.4.

Both are worked out after each tap and take effect on the next sample. In the engine they are exact
word for word, including a Constant held against the FB-mod input, which can take the feedback to
nothing.

## 25. Compressor

The instrument's own compressor, adopted 2026-09-14 (from the DSP code, run sample by sample). The
engine reproduces its output word for word across Threshold, Ratio, Attack, Release and Level.

**25.1 Words.**
- Threshold t and Level l are in dB, dial − 30.
- The ratio comes from the dial in four runs:
  - 1.0 to 1.9 in tenths;
  - 2.0 to 4.8 in fifths;
  - 5.0 to 9.5 in halves;
  - above dial 34, the same again × 10.
- The make-up gain is (l − t)(1 − 1/r) dB when l is above t, at most 42 dB.
- Attack and Release are per-sample coefficients from the instrument's own tables, interpolated
  between dial steps. Attack 0 is instant.

**25.2 Each sample.**
1. **Level.** The larger of |L| and |R| (the engine's is mono). It follows a peak at once and releases
   at the release coefficient, and it never falls below −84 dB.
2. **Log.** A piecewise-linear log2 of that level: the exponent, then the mantissa taken as linear.
3. **Ratio's reduction.** (level − t)(1 − 1/r), rising at the attack coefficient and falling at the
   release.
4. **Level limiter.** Beside it, the excess over Level, rising at once and releasing at the release
   rate.
5. **Gain.** The larger of the two becomes a gain through a 2^(−k/4) table, interpolated, and then the
   make-up gain is applied.

So the module levels: below the threshold everything is lifted by the make-up, and above it the
output rises at 1/r.

**25.3 Against the old law.** The engine smoothed the signal rather than the gain, so with a slow attack
and a fast release it never saw a peak. In 03 Chris' Lead (−4 dB, 4:1, Attack 104, Release 0, Level 0 dB)
it did not compress at all. The instrument takes a 0 dB input down by 3 dB and a +6 dB input by 7.4 dB,
after lifting everything by 3 dB.

## 26. Keyboard module and velocity

Added 2026-09-16. Every note reaches the engine with its velocity, and a note-off with its release
velocity: MIDI as received (a note-on at 0 releases at 64), the plug-in's host velocity x 127, the
Virtual Keyboard its own Velocity setting, which is what it sends the G2.

The Keyboard module is a per-voice node with six outputs, in its connector order (manual p.158):

| Output | Signal |
|---|---|
| Pitch | the voice's pitch, glide, bend and vibrato included, in semitones about E4 (note 64 is 0 units) |
| Gate | full scale (+64 units) while the key is held |
| Lin | velocity/127 |
| Release | release velocity/127, 0 from the next note-on until the key comes up |
| Note | the bare note number about E4 |
| Exp | (velocity/127)³ |

Lin and Exp are the instrument's own velocity curves in closed form: Lin is exact and the cube agrees
with every entry of its table to half a count.

**26.2 The Vel and Keyb morphs, per voice (2026-09-17).** On the instrument each note-on gives every
parameter with a Vel or Keyb morph range its own value for that voice: the patch-wide value plus
range x velocity/127 plus range x (note - 36 + octave shift x 12)/60, in dial units, held to 0-127. The
Keyb amount is 0 at C1 and 1 at C6, and goes past both (-0.6 at note 0, 1.52 at 127). The engine has
no panel octave shift.

The engine prepares these, since the audio thread cannot build nodes. Whenever the chain or a morph
range changes, it builds the chain at each morph's full amount as well, takes the nodes that differ
from the base build (at most eight per morph), and rebuilds just those modules along that morph's
axis: 32 velocities, and every other note from 0 to 126 (`build_axis_table()`, `build_module_node()`).
Each voice picks its rows at note-on (and a Mono voice returning to a held key its new Keyb row), so
the worst step is range/31 dial units for velocity and range/30 for a two-semitone key step. Knob
smoothing stays per node, and a voice adds each table's offset from the base node to the smoothed
Freq, Res, gain, shape and mixer levels. FX Area nodes take the latest note's rows.

LIMITS (closed 2026-09-18 - see §26.2.2 and §26.2.3). A node both morphs move plays a merge, each
word from whichever axis moves it, and the words BOTH move come from a build at the pair of amounts -
so the two are summed before the conversion and the clamp, which is the law in §26.2.0. What is left
is a resolution limit rather than a modelling one: the pair is tabulated at the same 32 velocities and
64 notes as the per-axis tables, and only the FIRST node a patch morphs on both axes gets one
(MAX_PAIR_NODES). Building the tables takes about
3.6 ms in a Debug build when every row is used; the plug-in does that on a thread of its own since
2026-09-18 (`rebuild_worker()`, g2Plugin.c notes §14) rather than on the audio thread.

**26.2.0 The law, confirmed against the instrument's own code (2026-09-18).** Every morph
contribution - all eight groups and both per-voice axes - is summed into ONE value per parameter per
voice, in 1/256 dial units: the dial times 256, plus each group's range times its controller (scaled
by 4/127), plus range x velocity/127 and range x (note - 36 + octave shift)/60, both times 256. That
sum is clamped ONCE to 0..127 and only then converted by the module's own law. There is no ordering
between the axes and no notion of one winning: a parameter has a single value per voice.

`param_value()` already implements exactly this, clamp included, so a single build of the chain at a
given (velocity, key) pair is correct. What is not correct is how the engine SAMPLES it: it builds at
(velocity, 0) and (0, key) separately and recombines two whole nodes. Where the two axes move
different parameters of a node that recombination could be exact; where they move the SAME parameter
it cannot, because the sum has to happen before the conversion and before the clamp. That is the
limit above, and it is an artefact of the per-axis tables rather than of the law.

**26.2.1 A DXRouter's Operators, per voice (2026-09-18).** An Operator's parameters live on the
Operator module rather than on the router, so a Vel or Keyb morph on one moved nothing the router's
own node carries: the table rows held a node alone, and the six Operators came from the base build
whatever the voice. Each row now carries that router's six as well, built at the row's own amount
(`build_module_node()`'s opsOut, `dx_operators_differ()`, `voice_morph_ops()`), and `dx_step()` reads
the playing voice's set - while the per-voice state arrays stay keyed on dxBase, which is the base
build's and the same for every row. They follow whichever axis the router's own node follows, so the
limit above applies to them unchanged. Two morphed routers per axis (MAX_VOICE_DX_NODES); a third
follows the base build, as a ninth morphed node already does.

Checked offline on PatchTestFiles/DXTest.pch2 (Keyboard, DXRouter, six Operators, 2-Out): a -99 Vel
morph on an Operator's Level plays, at every velocity from 22 to 127, within 0.04% rms of the same
dial turned down by hand, and at velocity 1 within 2.6% - the axis's own step (row 0 is amount 0, so
dial 99 against the hand's 98). Before the change the morphed note measured the same at every
velocity, to five figures. The reading is only reproducible with a fresh engine per note: voice
allocation round-robins and each voice starts its oscillators at a random phase (notes §63), which
otherwise swamps the effect.

**26.2.2 A node both axes move, merged per voice (2026-09-18).** The tables are built one axis at a
time, so a node both axes move had two candidate builds and the engine simply played the Keyb one -
which threw away every Vel-morphed parameter of that node that was not one of the four smoothed
values. Since the law is a per-parameter sum (§26.2.0), building at (amount, 0) and at (0, amount)
gives the RIGHT answer for any parameter only one axis moves; only a parameter both move needs a
build at the pair. So the two builds are now merged rather than chosen between.

The merge is by 8-byte word. When a table is built, each column records which words of the node its
axis moves anywhere on that axis (`mark_moved_words()`); a `_Static_assert` holds tEngineNode and an
Operator set to a whole number of words, since the merge assumes no field straddles one. A voice
whose note-on changes its rows assembles its node from the base, the words the Vel axis moves and the
words the Keyb axis moves (`merge_moved_words()`, `merge_voice_nodes()`), and plays that. Post-mix
nodes get the same from the latest note's rows, as they did before. Words BOTH axes move take the
Keyb value, exactly as the whole node used to, and eval_node() still adds both offsets to the four
smoothed values - that is the limit above, unchanged.

The merge happens at note-on and when a new build arrives, never per sample: the cost is one node
copy per merged node per note. It holds at most MAX_VOICE_NODES merged nodes and MAX_VOICE_DX_NODES
merged Operator sets, and costs 346 KB per engine.

Measured with `tools/morphcheck --param2` on SimpleLead, a Vel morph of -80 on an EnvADSR's Sustain
and a Keyb morph of -80 on its Decay: before, the Sustain morph was lost entirely and the readings
were 55%, 436%, 2040% and 1687% away from the same two dials set by hand; after, 0.77%, 0.61%, 0.25%
and 0.00%. The single-axis cases and the same-parameter case are unchanged, the latter still 7.66%
and 3.87% out at its worst on a DXRouter Operator's Level.

**26.2.3 The same parameter on both axes (2026-09-18).** The merge in §26.2.2 settles every word only
one axis moves. A word BOTH move it cannot: the right answer is the node built at (velocity, key)
together, because §26.2.0's law sums the two offsets into one dial value and clamps it once, before
the module's own conversion. Adding two converted offsets instead is exact only where the conversion
is linear in dial units - Freq is, a gain on a curve is not, and a DXRouter Operator's Level was 7.7%
out at its worst.

WHAT MAKES A 2-D TABLE AFFORDABLE is that only the shared words are kept, not the node. The per-axis
masks already say which words each axis moves (§26.2.2); their AND is exactly the set that needs the
pair, and it is a handful of doubles rather than a node's 137 words. `build_pair_table()` walks
VEL_MORPH_LEVELS x KEY_MORPH_LEVELS setting BOTH entries of sBuildAxis - which `param_value()` has
always supported - and keeps those words alone, for the node and for a DXRouter's six Operators. 786
KB a table, 2048 builds, against the 768 the two per-axis tables already cost; the rebuild has been
off the audio thread since the same day, which is what makes that affordable.

The four smoothed values then take ONE offset, `spec - base`, from the node the voice actually plays,
where they used to take one per axis and add them. That is now correct in every case: only Vel moves
it and spec is the Vel node, only Keyb and spec is the Keyb node, both and spec's word came from the
pair build.

Measured with `tools/morphcheck --axis both` on DXTest, a -60 morph on both axes of one Operator's
Level: 7.66% and 3.87% out at the two mid points before, 0.14% and 0.01% after, the rest 0.00%. The
single-axis and disjoint-parameter cases are unchanged.

**26.3 Sustain pedal (2026-09-17).** Morph group 5 (Sust.Pd) is the pedal, down from 0.5 (CC64 at 64 and
above). A key released while it is down leaves its voice gated - the envelopes sustain and the
Keyboard module's Gate stays high - and the pedal coming up releases every voice it was holding. A
new note on such a voice clears the hold; All Notes Off releases them regardless.

## 27. OscShpB and OscShpA wave shapes

Checked 2026-09-17 against the instrument's own wave parts, run natively, at 187.5 Hz and Shape 0, 32, 64, 96
and 127. g is the Shape word, dial/128 with 127 counting as 1 (`wave_shape_word()`).

| Wave | Law | Engine vs instrument |
|---|---|---|
| Sine1 | a sine whose rising half takes (1 - g)/2 of the cycle, never under two samples, and its falling half the rest, each linear in angle | exact, every Shape |
| Sine2 | 27.2 | level, DC and harmonics match at 10 Hz, 187.5 Hz and 1 kHz, every Shape (within 0.2 dB) |
| Sine3, Sine4 | 27.3 | harmonic shape and level match the captures at Shape 64 |
| TriSaw | triangle, peak at 0.5 + g/2, fall never under two samples | within 0.1 dB to harmonic 20 |
| DblSaw | two full saws, the second Shape/256 of a cycle later, summed (peak 2) | within 0.1 dB; the engine halved it until now |
| Pulse | high for (1 - g)/2 of the cycle, never under one sample, with the DC taken out: the ±1 square minus (2d - 1), d the duty | within 0.2 dB; at Shape 127 the instrument's two one-sample edges leave a spike, which the one-sample floor reproduces to 2 dB |
| SymPulse | high, low, then silent | matches |

**27.2 Sine2.** With s the Shape word, limited so the positive lobe keeps at least four samples:
- the positive half-sine takes (1 - s)/2 of the cycle and the negative half the rest; within each, a
  linear phase x from 0 to 1 and back goes through the odd polynomial 1.5704x - 0.6419x^3 + 0.0716x^5
  (close to sin(pi x/2));
- times 1 + |Shape| - the UNLIMITED Shape, so the gain keeps rising where the lobe has stopped narrowing;
- then a DC blocker at 96 kHz, a = 4000/2^23: w = b + a*c, out = in - w - 2b, b += a*out, c = w (b and c
  its two states, per voice). It sits near 20 Hz, so a very low Sine2 is attenuated - 0.29 rms at 10 Hz
  against 0.70 at 187 Hz - and the narrow lobe's DC is taken out, which is what lifts it above the trough.

The engine renders the shape and gain with its oscillators and runs the blocker after their decimation,
at the engine rate with a scaled to it (`oscillator_step()`); the model matched the instrument's code to
1.8e-4 of full scale sample by sample, and the engine's output matches its level, DC and harmonics. The
four-sample floor is the hardware's 0.013-cycle lobe at full Shape (329 Hz).

**27.3 Sine3 and Sine4.** Measured 2026-09-17 on the G2 (Shape 16-127 at E4, and 64/96/127 at E2 and E6,
G2Captures/oscshpb/sweep-2026-09-17) and set against the instrument's code:
- the ratio r = g x (0.987 - 8 x inc96), inc96 the phase step per 96 kHz sample - the code's law, which the
  captures follow exactly to Shape 112 at all three pitches - held under 0.905, where the hardware stops
  (0.903 at E4 and 0.907 at E2 at full Shape, against the code's 0.96 and 0.98; E6 stays under it);
- Sine3 = sin theta/(1 - 2r cos theta + r^2) x (1 - 0.642g): the whole harmonic series, at a level falling linearly
  with Shape and not with pitch (0.642 is twice the part's own -0.321);
- Sine4 = sin theta (1 - 0.642g)/(1 - 2r cos 2theta + r^2): the odd series, which is Sine3's level over 1 + r.

`wave_sine3_instrument()` / `wave_sine4_instrument()`, `wave_dsf_ratio()`; the drawn shapes use the same r at
unit peak. Against the sweep: levels within 1% (0.1 dB) to Shape 112, the ratio within 0.002 everywhere
but Shape 120 (0.900 against 0.886); at Shape 120-127 the hardware is a further 0.2-0.65 dB down. The capture
chain lifts harmonics 2 and up by 1.10-1.13 against the fundamental, so ratios were read against the
instrument's Sine1 at the same setting. The code's translation lacks the level stage (it gives the series at
a quarter, and Sine4 over 1 + r), which is why it read 12 dB low; the 08-23 fit's 0.90/0.94 were the capped
ratio read from harmonics 2 and up.

**27.5 On the instrument's phase, at the engine rate (2026-09-17).** OscShpB runs once per engine sample, like the
basic oscillators (§6.3), against the instrument's own wave parts sample for sample (a test harness; 96 kHz):
- Phase p = 2 x phase wrapped to -1..1; x = 2 inc96 is its step per sample; y the Shape word (dial/128, 127 = 1).
- Sine1 peaks at half a cycle: argument phase + 0.5 + rise/2, rise = max((1 - y)/2, 2 inc96). -82 dB.
- Sine2's positive lobe ends at half a cycle: phase + 0.5 + lobe, lobe = max((1 - y)/2, 4 inc96). -62 dB.
- Sine3/Sine4 start 0.75 of a cycle on. -80 dB, except Shape 120-127 below ~1.5 kHz where the engine keeps the
  hardware-measured ratio cap (§27.3), which the harness (missing its level stage) does not have.
- TriSaw: FALLS from +1 at p = -y to -1, rises over max(1 - y, 2x); corners rounded by
  turn x (2 - |d|)^3 / 24 (turn = 2/rise + 2/(2 - rise)) at the peak (down) and at the phase wrap p = 1 (up; skipped
  at y = 1, where the peak is the wrap). -43 to -77 dB below 1.5 kHz, -25 to -32 dB at 6 kHz: the harness's own
  division emulation flips the sign of the samples beside the peak with tiny pitch changes, so those two samples
  are not settled by it.
- DblSaw: two RISING saws stepping at phase 0, the second y/2 on, two-sample edges. Exact (-150 dB).
- Pulse: high above p = y, each edge a straight line one sample either side (the later edge wins where they
  overlap), + y; the instrument's "1" is 0x7fffff, which is what leaves a spike at y = 1. Exact (-102 dB).
- SymPulse: -1 for the first (1 - y)/2, 0, +1 for the last (1 - y)/2, two-sample edges. Exact.


## 28. LFO rate

Checked 2026-09-18 against the instrument's own rate tables and the code that reads them. All four ranges
that were implemented agreed; the fifth, Clk, was not implemented at all.

**28.1 The ranges.** The Range selector picks between five laws, and the instrument reads the dial at
the morph accumulator's 1/256 resolution, not as an integer:

| Range | Instrument | Engine | State |
|---|---|---|---|
| Sub | `(dial + 1) x 16`, no table - linear | `(dial + 1) / 699.0507` Hz | EXACT: the ratio to Hi matches to 0.001% |
| Lo | its Lo table, 128 entries, interpolated | `(0.2555/16) x 2^(dial/12)` | EXACT |
| Hi | its Hi table, 128 entries, interpolated | `0.2555 x 2^(dial/12)` | EXACT, and separately hardware-measured |
| BPM | three straight runs | the same three runs | confirmed 2026-09-13 |
| Clk | its sync-ratio table at `dial/4`, 32 slots | §28.2 | ADDED 2026-09-18; was 1 Hz flat |

Both tables are pure geometric at exactly 12 steps per octave (0.001 dB from a fitted geometric for
Hi, 0.020 for Lo), which is where `2^(dial/12)` comes from.

**LO IS EXACTLY HI/16, and the tables appear to say otherwise at the bottom of the dial.** At dial 0
they read 2858 and 178, a ratio of 16.056; by dial 96 it is 16.0001 and at 120 it is 16.0000 exactly.
The discrepancy is the rounding of 178.6 to 178, not a law - do not "correct" the base from the first
entry.

**28.2 Clk, and the table the delay already had.** The instrument's LFO sync ratios are 1, 4/3, 2,
8/3, 4 ... 4096, 6144 - straight and triplet divisions over 32 slots, indexed by dial/4. Those are
`256 / beats` for the very table `clk_sync_beats()` already holds for the delay's Clk, entry for entry
(worst 0.024%, and only on the triplets where 1365/1024 is a rounded 4/3). So one beat table serves
both modules, and the delay's Clk table is confirmed as the instrument's own into the bargain.

The rate is therefore `(BPM/60) / clk_sync_beats(dial)`: 256 beats per cycle at dial 0 to 1/24 of a
beat at 127, which at the reference 120 BPM is 0.0078 Hz to 32 Hz. The engine has no live master clock
yet, so this uses the same fixed reference tempo the delay's Clk does - when one arrives, both follow
it together.

## 29. ModAmt

Added 2026-09-19. Parameters, validated against the G2 (param-validation.md): 0 Depth, 1 Enable,
2 Exp/Lin, 3 m/1-m.

**29.1 The Depth taper is the mixers' own law.** On **Exp** the Depth dial is exactly
`mix_level_gain()` - the cube-plus-1%-linear curve of §3.2 - to within 6e-8 over all 128 positions,
so the two share one function rather than carrying a table each. On **Lin** it is the plain fraction
`dial / 128`, with 127 reaching exactly 1.0 as the other level dials do (§16.3). The dial's own
display agrees: param-validation records Depth as `percent = raw*100/128`.

**29.2 m and 1-m.** With the m/1-m button OFF the module is a plain multiplier, so nothing comes out
at Depth 0: `Out = In x Depth x Mod`. With it ON the input stays at full level at Depth 0 and the
modulation is crossfaded in: `Out = In x ((1 - Depth) + (Depth x Mod))` (manual p.232).

**29.3 Depth rides on the node's `gain`**, which is what gives it the per-sample smoothing and the
per-voice morph offset every other level dial gets; the three drop-downs are read raw, because a
drop-down cannot carry a morph (manual p.20).

**29.4 Enable is UNSETTLED.** The engine treats Enable off as a bypass that passes In through
unchanged. That is the usual reading of the G2's Enable buttons but it has NOT been confirmed on the
instrument, and it matters: the 02 Big Pad test patch has Enable off on both of its ModAmts. The
alternative - Enable off silencing the output - would sound very different. See to-test.md.

## 30. SwOnOffT

Added 2026-09-19. One parameter, 0 On.

Closed, the output is the input; open, it is nothing. With **nothing patched to In** a closed switch
sends 64 units, which is 1.0 in the engine (§16), so the module doubles as a manual constant. The
Ctrl output carries the switch state as a logic signal on the same scale - 1.0 closed, 0 open
(manual p.222, and the Logic group's definition of a logic HIGH on p.233).

## 31. LevConv

Added 2026-09-19. Two drop-downs, no dial: the range it READS (`levConvStrMap` {Bip, Pos, Neg}) and
the one it WRITES (`posStrMap` {Pos, PosInv, Neg, NegInv, Bip, BipInv}). Both are drop-downs, so
neither can be morphed and both are read raw.

**31.1 A straight line between the two ranges, and then SATURATED.** In engine terms (1.0 is 64
units, §16) Bip is -1..+1, Pos 0..+1 and Neg -1..0; the Inv output forms are the same range with
its ends swapped. The output is `outLo + (In - inLo) x (outHi - outLo) / (inHi - inLo)`. Bip to Bip
is therefore unity, which is how 01 Mini Emulator uses five of them.

The instrument's own part computes `offset + 2k x In` and clamps the result to full scale, so the
affine form is confirmed and the saturation is not optional - an over-range input stops at the rail
rather than carrying on past it. The offset and the gain come from the host side, which is where
the two drop-downs land.

## 32. LevAdd

Added 2026-09-19. Adds its dial to the input, and the dial is a Constant's: Bipolar (value - 64)
units, Unipolar value / 2 units, 127 reading exactly 64 in both (§16.1). It shares
`constant_level()` with the Constant module, so the two cannot drift apart.

## 33. Sw2-1 and Sw8-1

Added 2026-09-19, one node kind for both. **Out is the selected input, passed through untouched** -
the instrument's part reads the chosen connector and writes it, with no arithmetic on the way. The
selector is a radio button, read raw.

**33.1 The Ctrl output** is 0 units for In 1, 4 for In 2, and so on to 28 for In 8 (manual, Common
Switch parameters) - `select x 4 / 64` in engine terms. 01 Mini Emulator drives a ValSw2-1 from one.

## 34. ValSw2-1

Added 2026-09-19. In 1 normally, In 2 once the Ctrl input REACHES the threshold. The threshold dial
counts whole units 0 to 64, and its top step reads 64 rather than 63 - the same law the face prints
(`render_paramType1UniPolShort`).

## 35. MonoKey

Added 2026-09-19. Three outputs and no inputs, and it belongs to the KEYBOARD rather than to a
voice: every voice sees the same values, so nothing in it reads the voice it is being evaluated for.

- **Pitch** is the chosen key on the Keyboard module's own scale - E4 is 0 units, one unit a
  semitone (§15.1, manual p.158).
- **Gate** is high from the first key down until the LAST key comes up, which is the single-trigger
  behaviour 01 Mini Emulator's two ModADSR depend on. It reads the engine's held-key table (§15.1).
- **Vel** is the velocity of the last key pressed.

**35.2 Pitch carries BEND and VIBRATO, added 2026-09-20.** It did not, and that made the pitch
wheel dead on any patch whose oscillators have KBT off and take their pitch from this module -
01 Mini Emulator is exactly that patch, three OscA with KBT off and MonoKey's Pitch routed in
through a Glide and a mixer per oscillator. The wheel worked everywhere else because every other
pitch path reads `voicePitch`, which has always had bend and vibrato in it; only MonoKey rebuilt a
pitch of its own from the raw key and so dropped them (CT, 2026-09-20: "pitch bend doesn't work
with Mini Emulator. It works with Chris' Lead").

They ride on the KEYBOARD, not on a voice's note, so what the module adds is `voicePitch` less
that voice's own note - bend plus vibrato and nothing else. That difference is identical for every
voice, which keeps 35's rule that all voices read the same value out of this module. The note
itself must NOT come in that way: Last is the mono voice's note by 35 below, and Lo and Hi are
keys that are still down.

**35.1 Priority** is `monoKeyStrMap` {Last, Lo, Hi}, and the instrument keeps all three for it.

Its note vector holds a HELD COUNT per key with that key's velocity, and the highest and lowest
held notes beside them. A note-on extends the range if it is outside it; a note-off **rescans** -
down from the old highest, up from the old lowest - for a key whose count is still non-zero. So Lo
and Hi always name keys that are STILL DOWN, which is what the engine's scan of `gKeyHeld` does.

**Last is the mono voice's own note**, not a separate record of the last key pressed. That matters
because 15.2 hands the voice back to a held key when the key above it comes up, so Last follows it
there. Reading a "last pressed" of its own instead is what stopped a held note returning on
01 Mini Emulator: hold a key, play a higher one, release it, and the first should sound again - it
did on the G2 and did not here (CT, 2026-09-19). Last still outlives the last key coming up,
because the voice's note does (15.2).

**Vel** is that key's velocity, kept per key as the instrument keeps it, so Lo and Hi report the
velocity of the key they name rather than of whatever was played most recently.

Still open: in a Poly patch all three read the voice being evaluated, which is a guess - MonoKey is
a monophonic module and the case may not arise. Lo and Hi are raw keys and so carry no glide.

## 36. Glide

Added 2026-09-19. A slew for control signals, with its own Time dial - **a different law from the
patch-wide glide of §15.4**, which runs 19 ms to 6.27 s per octave. This one runs 0.2 ms at 0 to
22.4 s at 127, read straight off the table the face prints rather than fitted, exactly as the patch
glide reads its own.

It glides while its button is on, or while the Glide On logic input is high where something is
patched into it; otherwise the input passes through. The first value a Glide ever sees arrives
whole rather than being slewed up from zero.

**36.1 THE SLEW IS AN ENVELOPE SEGMENT (settled 2026-09-19 against the instrument).** The Glide
module has no time law of its own. Its host update writes two words into the DSP frame, both
indexed by the Time dial and both taken from the ENVELOPE's own tables (17.3):

- **Log**: a one-pole whose coefficient is `2 x (1 - envelope decay multiplier[Time])`. The factor
  of two is what makes the dial's printed Time the time to close the gap to **1%** of it, rather
  than the envelope's own reading of the same table entry.
- **Lin**: a constant step of `envelope linear attack step[Time]` per tick - full scale in that
  time.

Both run at the envelope tick rate (24 kHz), because on the instrument this IS an envelope segment.
The engine builds both from `adr_time_seconds()`, which is where 17 already models those tables, so
there is no new table: it agrees with the instrument's own values to better than 1% across the
dial, and within a few steps at the very top where 17.3 already says the closed form parts from the
table.

Checked against what the dial prints, as time to 99%: dial 0 gives 0.19 ms against 0.2, dial 8
1.02 against 1.0, dial 32 27.1 against 27, dial 64 511.4 against 511, dial 127 22.5 s against 22.4.

An earlier version read the printed table and divided by ln(100) on the reasoning that 17.3 quotes
its times to -40 dB. That happened to be the right convention - which is why the numbers agreed -
but it took the coefficient from a display string rounded to three figures instead of from the law,
and it ran per sample rather than per tick.

## 37. 2-In

Added 2026-09-19. The jacks on the back of the instrument, which this engine does not have: two
outputs, both silent. It exists as a node so a patch containing one is not reported as unmodelled
and its face is not greyed out. 01 Mini Emulator has one, switched off in its mixer.

## 38. The Logic group

Added 2026-09-19: Invert, Gate, FlipFlop and ClkDiv. The rest of the group (8Counter, BinCounter,
ADConv, DAConv, Delay) is still silent.

**38.0 A logic input is HIGH above zero.** Not above a halfway threshold - the instrument's own
logic parts test the input as a signed value greater than zero, so the smallest positive signal is
already a HIGH. A logic HIGH OUTPUT is 64 units, which is 1.0 in the engine (§16, §30, manual
p.233).

## 38.1 Invert

Two independent inverters on one face, their jacks interleaved (In 1, Out 1, In 2, Out 2). Each
output is HIGH when its input is not. **An unpatched input reads low, so its output sits HIGH** -
which is what an inverter with nothing on it does.

## 38.2 Gate

Two independent two-input gates, each with its own type from `gateTypeStrMap`
{AND, NAND, OR, NOR, XOR, NXOR}. Gate 1 takes In1_1 and In1_2, gate 2 takes In2_1 and In2_2. The
types are drop-downs, so they are read raw and cannot be morphed.

## 38.3 FlipFlop

Clk, Rst and In; the outputs are **NotQ then Q**, in the module's own connector order, and NotQ is
always the inverse of Q (manual p.235).

- **D-type**: the state on In is clocked to Q on the POSITIVE EDGE of Clk. While Rst is HIGH, Q is
  held low and clocking is ignored until Rst goes low again.
- **Set-Reset**: In becomes S. A positive edge on S sets Q. **Rst has priority over S.** With S and
  Rst both low, a clock on Clk TOGGLES Q - and a constant HIGH on either stops the toggling, since
  both have priority over Clk.

## 38.4 ClkDiv

Clk and Rst in, one output. The Divider dial reads **one more than it holds**, so 1 to 128
(`ParamText::Enum`), and `divModeStrMap` chooses the mode.

- **Gated**: every nth clock pulse is passed with its shape unaltered, so at a divider of 1 the
  train passes through untouched.
- **Toggled**: the output flips on every nth EDGE, and **both the rising and the falling edge
  count** - so an odd divider halves the frequency again. A divider of 3 divides by one and a half
  (manual p.236).
- **Rst is the barred arrow**: the reset does not act at once but waits for the next positive edge
  of Clk.

## 39. DrumSynth

Added 2026-09-19. A master and a slave oscillator, a noise source through a sweeping multimode
filter, and a global bend plus a click - the classic analogue rhythmbox voice (manual p.181).
Sixteen parameters, three inputs (Trig, Pitch, Vel) and one audio output.

**39.1 What each dial becomes** is the instrument's own conversion, and the pattern is worth seeing
whole, because five of the sixteen reuse tables this engine already models:

| dial | conversion |
|---|---|
| Master Freq | its own pitch law, `20 x 2^(dial/24)` - 20 Hz at 0 to 784 Hz at 127. CONFIRMED ON THE G2 2026-09-21 at five dials, all within 0.2%. Measure it with the BEND AT ZERO: the bend is still falling for the first tenth of a second and reads as a much higher pitch |
| Slave Ratio | `2^(v/48)`, so 1 to 6.26 times the master |
| Master, Slave, Noise Filter and Bend Decay | the ENVELOPE's decay multiplier table - the same one §36.1's glide uses |
| Noise Filter Freq | the FILTER cutoff table |
| Noise Filter Res | linear in the dial, but a QUARTER of full scale at 127 - see 39.4 |
| Noise Filter Sweep | linear in the dial, over 5 octaves; full scale at 127 |
| Master and Slave Level, Bend Amount, Click, Noise | an exponential level curve - see 39.3 |

The two pitch laws are shared with the face (`drum_master_hz()`, `drum_slave_ratio()` in
paramCurves.c), so the dial's reading and the sound cannot disagree.

**39.2 The voice.** Trig fires on a transition from at or below zero to above it (manual), and
starts every envelope at once. Each then decays at its own per-tick multiplier, on the envelope's
own 24 kHz tick. The bend sweeps both oscillators DOWN from its octaves above their pitch, and the
noise filter sweeps DOWN from its octaves above its cutoff, each following its own decay - the
manual is explicit that both start high and fall. Velocity scales the two levels, the sweep, the
bend, the click and the noise, and full velocity reaches the dialled settings.

The noise filter is a Chamberlin, the same form §23.1 uses, and it needs §23.1's clamps: without
them the fifth Kick preset drove it unstable and the module produced 1e27 rather than a drum.

**39.3 The level curve, SETTLED 2026-09-20.** Five dials - Master Level, Slave Level, Bend
Amount, Click and Noise - share ONE curve, and it is the curve this engine already had:
`0.01x + 0.99x^3` with `x = dial/127`, i.e. `mix_level_gain()`, the same law the mixers' Exp/dB
taper and every mod amount use (§14). The instrument's parameter conversion dispatches all sixteen
dials by index and sends exactly those five through that one table; there is no separate drum
level law. Nothing in the engine changed.

A hardware sweep on 2026-09-20 fitted each dial's peak independently as a power law and got 3.71
(Master), 2.75 (Slave) and 2.99 (Noise) - so Noise landed on the cube and the other two did not.
Those exponents were briefly adopted and are now reverted. **The measurement is not the law**: peak
output of a decaying hit reads the whole voice - two oscillators summed, the bend still falling,
the output stage - not the gain word, and the two oscillators are the two that interact. The curve
stands on the instrument's own conversion; the capture is kept in findings.md as the record of what
peak-of-a-hit actually measures, which is not this.

**39.4a The click, SETTLED 2026-09-21 - the engine's was the wrong shape, length and level.**
Read out of the module's own code with everything but the click switched off, and confirmed
against the instrument.

| | the instrument | the engine, before |
|---|---|---|
| shape | held one envelope tick, then a ONE-POLE decay | a linear ramp |
| decay | x0.780851 every sample at 96 kHz (tau = 42 us, -20 dB in 0.1 ms) | linear to zero over 2 ms |
| peak | a QUARTER of the dialled level | the full dialled level |
| level curve | the shared exponential (39.3) | the shared exponential - already right |

The decay coefficient is not fitted: it is a constant sitting in the module's own frame, and the
engine now uses it directly, rate-corrected as `0.780851^(96000/rate)` so an engine running at any
sample rate decays in the same TIME. Measured after the change, the engine's click decays at
0.78085 per sample and its level curve matches the reference's at every dial to a constant.

So the engine was roughly twenty times too long, four times too loud, and the wrong curve shape.
On a preset like Kick 1, where Click sits at 79, that is a large part of what CT heard as "more
noise than the G2" - a 2 ms full-scale DC ramp is a broadband thump.

**Hardware note.** The instrument measurement that prompted this (peaks 0.00163 / 0.01039 / 0.03845
/ 0.05824 at dials 32/64/96/127, all -20 dB within 1 ms) looked like a curve STEEPER than the
shared exponential. It is not - the module's own code gives the shared curve exactly. A 0.1 ms
event at 48 kHz is about five samples, so the capture could not resolve its peak. **Do not fit a
level law to an event shorter than the capture can resolve.**

**39.5 The panel lamp, added 2026-09-20.** DrumSynth's face has an LED by its Trig and nothing lit
it: the engine published a lamp for the LFO alone, and every other module's LED stayed dark unless a
real G2 was attached to send one (CT). It now follows the MASTER ENVELOPE, which is what the
instrument shows - its lamp reads a level word of the module's own DSP state, the same way an
envelope's does, not the Trig input. So it comes on with the hit and fades out with it rather than
following the key: offline it lights at the note-on and goes out 263 ms later on the default preset,
with the key released at 80 ms. The face has one lamp and a poly patch has one of these per voice,
so voice 0 publishes and the rest do not - the rule the LFO already used, now in one place
(notes §194).

**39.4 STILL UNSETTLED: the noise path, and it needs the NATIVE HARNESS, not captures.** The
module is two DSP parts of its own, one running at 96 kHz and one at 24 kHz, so its noise source,
its filter and their gains are all inside that code. **Nothing here may be fitted to a capture**:
the standing rule is that the instrument's own arithmetic decides, as it did for §§21-25.

What the HOST side already gives, read off the parameter conversion (2026-09-20), so the harness
starts from a known input:

| dial | word the host sends |
|---|---|
| 0 Master Freq, 1 Slave Ratio, 10 Noise Type, 15 On | the raw dial |
| 2, 3, 9, 12 - the four decays | the ENVELOPE's decay-multiplier table |
| 4, 5, 11, 13, 14 - the five levels | one shared exponential curve (39.3) |
| 6 Noise Filter Freq | a cutoff table of its OWN: the same `2^23 sin(pi f / 96000)` form as the filter modules' but based three semitones higher, `f = 16.35 x 2^(v/12)`, and 132 entries long rather than 128 |
| 7 Noise Filter Res | `dial/512`, capped at a QUARTER of full scale |
| 8 Noise Filter Sweep | `dial/128`, full scale at 127 |

**Captures taken 2026-09-20 as EVIDENCE FOR the harness - what its output has to reproduce - and
explicitly not as laws to fit.** Rig: Keyboard -> DrumSynth -> LevAmp -> 2-Out in Slot A, Kick 1's
settings, contributors isolated by zeroing the others.

- **The noise is about 12 dB too loud relative to the two oscillators.** Measured as a ratio, so it
  carries no rig calibration in it: noise-only peak against oscillators-only peak is -21.3 dB on the
  G2 and -9.5 dB in the engine. About 11 dB of that is already there at zero resonance, so most of
  it is a fixed gain rather than the resonance law. **This is what CT hears** ("Drum synth engine
  has more noise than G2 on Kick 1").
- **The resonant peak tracks `10.3 x 2^(v/12)`**, dead straight over dials 32 to 112 (implied base
  10.24, 10.35, 10.23, 10.35, 10.29). The engine's own peak tracks its nominal cutoff to 13.8, so
  the peak is a faithful read of the cutoff and the difference is real: the engine sits five
  semitones high. **That contradicts the table above by eight semitones**, which means the word is
  not used the way the filter modules use theirs - and that is a question only the Compute can
  answer.
- **The Sweep dial measures one semitone a step**: 0, 2.65 and 5.30 octaves at dials 0, 32 and 64,
  exactly linear. The engine's five-octaves-over-the-dial comes from the manual, not from the
  instrument, and is 2.1x too shallow - but the reference has to confirm the law before it changes.
- **The resonance curve is the wrong shape**, quite apart from the level: peak gain relative to
  Res 0 runs 0, +0.6, +1.2, +5.0, +10.9 dB on the G2 at dials 0/32/64/96/127, against the engine's
  0, +1.2, +3.7, +7.6, +14.4. **The earlier guess in this section - that the engine is four times
  too resonant because the host sends a quarter scale - is DISPROVED**: quartering the dial gives
  far too little resonance, not too much.
- **The noise decays too slowly**: to -20 dB in 96 ms against the G2's 76 ms, on Kick 1's Noise
  Decay of 49.
- **The CLICK is a second, separate suspect and has never been isolated** (CT, 2026-09-21). The
  12 dB noise figure above is click-free - Click was zeroed on BOTH sides for it, as were the
  oscillators for the noise take and the noise for the oscillator take - so that number stands.
  But Kick 1 runs Click at 79, and the engine's click is invented from end to end: a LINEAR DC RAMP
  from full scale to zero over a hard-coded 2 ms, scaled by the shared level curve and velocity,
  added straight to the output. Nothing about it is measured. A 2 ms DC ramp is broadband, which is
  where the residual +1.4 to +2.0 dB in the top three bands over the first 30 ms could easily be
  coming from once the noise is right. **Isolate it**: Master, Slave and Noise at 0, Click swept,
  on both the instrument and the engine.

**THE HARNESS PLAYS (2026-09-21).** The module's own code now runs offline and produces a decaying
drum hit. What it took, beyond translating the two parts: the boot tables at their right addresses
AND in the right memories, the pointers the linker installs between the two parts, the four dials
that reach the DSP through custom actions rather than the plain parameter path, and - the thing
that took four rounds to find - **the PITCH input.**

DrumSynth is a VOICE module, and its oscillators are a two-state sine RESONATOR damped by the
decay multiplier, not a phase accumulator. A resonator has to be struck, and what strikes it is a
word derived from the Pitch input. An unpatched Pitch on the instrument carries the voice's own
pitch, never zero - so with Pitch left at zero the module is silent and every other hypothesis
about the silence tests plausible and negative in turn. Measured by removing one input at a time
from the working harness: no Pitch gives nothing, no Trig gives nothing, and no velocity is worth
about half a dB.

It does NOT yet reproduce the hardware. Three inputs are still approximations - the pitch action's
fixed-point multiply, the Slave Ratio conversion, and what the voice supplies as a resting Pitch
(which sets the strike amplitude, and so the module's whole level). Until those are exact the
measurements below are the target, not something to compare against.

A change was drafted from these numbers and REVERTED the same day - the instrument's own logic is
the reference and a capture is only its check - which is why they are recorded here as targets
rather than as constants.

**39.5 The panel lamp, added 2026-09-20.** DrumSynth's face has an LED by its Trig and nothing lit
it: the engine published a lamp for the LFO alone, and every other module's LED stayed dark unless a
real G2 was attached to send one (CT). It now follows the MASTER ENVELOPE, which is what the
instrument shows - its lamp reads a level word of the module's own DSP state, the same way an
envelope's does, not the Trig input. So it comes on with the hit and fades out with it rather than
following the key: offline it lights at the note-on and goes out 263 ms later on the default preset,
with the key released at 80 ms. The face has one lamp and a poly patch has one of these per voice,
so voice 0 publishes and the rest do not - the rule the LFO already used, now in one place
(notes §194).

**39.4 STILL UNSETTLED: the noise filter, and only the noise filter.** The whole module is two DSP
parts of its own - one at 96 kHz, one at 24 kHz - so its filter is hand-written rather than one of
the filter modules, and its coefficient law is inside that code. Two host-side facts are known and
disagree with what the engine does:

- **Res reaches only a quarter of full scale**: the instrument sends `dial/512`, capped at 0.25,
  where the engine sends `dial_fraction()` - 0 to 1 - and turns it into a damping of `1 - 0.98 res`.
  If that word is the Chamberlin damping directly, as the equivalent word is in §23, the engine is
  four times too resonant at the top of the dial and the drums ring where the instrument thumps.
- **Noise Filter Freq reads a different cutoff table** from the one §22 and §23 use.

Neither can be settled from the host side alone, because the meaning of both words is in the two
own DSP code. That is a native-harness job of the kind §21-§25 each were, and it would settle
the filter, the cutoff scale, the click and the four decays together - a better use of a session
than more captures. Until then the filter shape is the thing to listen to, and Res is the dial to
distrust.
