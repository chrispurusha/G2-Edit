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

**11.2 Shelves (from the DSP code, confirmed by 11.6).** Low shelf y = x + (G - 1)·lp, lp a one-pole
low-pass at the Lo Freq corner. High shelf y = x + (G - 1)·hp, hp a one-pole high-pass with unity
gain at Nyquist. Lo Freq: 80, 110 and 160 Hz, as named. Hi Freq: the FIRST setting sounds at 8 kHz and
the SECOND at 6 kHz (fitted 8.1 and 6.0) - the reverse of the names the editor shows; the third
fitted 13.3 kHz, where the capture thins out, and is taken as its name, 12 kHz.

**11.3 Peak.** EqPeak and Eq3band's mid band: y = x + (G - 1)·q·bp, bp a band-pass of damping q
(peak 1/q). Centre: EqPeak `flt_cutoff_hz(Freq)`, Eq3band 100 × 80^(Freq/127) Hz. For a boost
EqPeak's q = 2√2 × (1 - BW/128), whatever the gain - the instrument's own law, adopted 2026-09-13
(`eq_peak_bw_damping()`). The measured fit it replaced, 2(2^N - 1)/√(2^N) with N = (128 - BW)/64
octaves (twice the damping of a band-pass N octaves wide), agrees at BW 64 and within 4% elsewhere.
EqPeak's CENTRE IS OPEN: the instrument's own table reads 20 × 800^(Freq/127) Hz, 20 Hz to 16 kHz,
against the displayed curve used here; they meet only near Freq 73 (to-test.md). Eq3band's mid has no BW dial: it fits q = 1.36, and the engine uses the formula's
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

**15.3 Poly stealing.** A new note takes a free voice first: one doing nothing, then the longest
released. With every voice held it steals the oldest, unless that voice has the lowest note held and
the new note is higher, when the next oldest goes instead - the manual's "it will try to keep the lowest
note sounding" (Voice allocation and polyphony). A repeated note-on for a key whose voice is still
releasing takes a fresh voice and lets that release ring on; a note-off closes every voice on its key.

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
