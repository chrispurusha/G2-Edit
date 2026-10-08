# Sound engine reference

How the local sound engine (`src/soundEngine.c`) models each module, and the measurements behind it.
Code comments refer here by section number (`// §3.2`). The dated history of how each result was
reached - including what went wrong on the way - is in `findings.md`.

Measured on the instrument unless marked **(from the reference model)**; those are to be confirmed on the
hardware. "Dial" means the raw 0-127 value.

---

## 1. Level meters

**1.1 Law.** The G2's meters read the PEAK, one value per octave. Below full scale the value is
7 + the binary exponent of the peak (0.5-1 reads 7, 0.25-0.5 reads 6, under 2^-7 reads 0). At and
above full scale it skips: 1-2 reads 9, 2-4 reads 11, 4 and over reads 12 with the clip bit (0x40).
Boundaries within 0.6 dB of -6.02 dB × n; sine, saw and square agree to 0.3 dB.
The 2-In's meter follows the same law (2026-10-08, §37) and sends 2-4 as 43, i.e. 11 with 0x20 set; the
canvas draws the low nibble and the clip bit only, so the bit changes nothing on screen. What it means is unknown.

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
resolution - the level a mixer sends the DSP is the morph accumulator's, not the integer dial, and the module looks the curve up. Read back at every integer dial value it agrees with the formula to **0.006 dB
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

**4.3 Modulation (from the reference model).** Position += 4 × attenuator × input: a quarter of full scale
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

**6.5 Not modelled.** Nothing on the inputs since 2026-09-28: Shape Mod is §6.7, FM §6.8.

**6.6 Sync (2026-09-28, from the sync stage).** One stage serves OscB, OscC, OscShpA, OscShpB and OscDual;
OscPM has its own variant. A rising crossing of the Sync input - the last sample at or below zero, this
one above - restarts the phase at the module's reset word, -0.9 of the phase range, plus that sample's step,
so the new cycle keeps its time. In the engine's phase (the word / 2, wrapped) that is 0.55: just past
the saw's step, the ramp restarting near its foot. OscPM keeps its phase over 0..1 from -1, so there the
same word is 0.05. Checked: an OscB a fifth above an OscA, synced to it, repeats exactly at the OscA's
period (12.135 ms at 82.4 Hz, correlation 1.000). The stage patches 01 (two OscC), 02 (an OscB) and 14
(an OscShpA) cable a Sync input and change with it (revert record row 89).

**6.7 Shape Mod (2026-09-28, from the reference model).** OscB, OscShpA and OscShpB share one control-rate stage: shape word = Shape word + 4 x input word x Shape M word, saturated to the 24-bit range. Shape and
Shape M are both v/128 with 127 counting as full. A unit is a quarter word, so in the engine's terms the
shape is Shape + input x Shape M. The Shape Mod jack is the fifth input on all three; Shape M is
parameter 7 on OscB and OscShpB, 8 on OscShpA. OscB's pulse takes the word below zero too: its part
outputs +-1 plus the shape word, the width (1 - y)/2 of the cycle, so a negative shape widens the pulse
up to a steady level at -1. The shape oscillators' waves run below zero too, each wave's own law
continued (2026-10-02, the wave stages run with the word swept -1..+1):
- Sine1, Sine2, TriSaw: the mirror of +y - the same wave reversed in time and inverted, so the steep
  segment moves to the other side of the peak. Their shortest segments (two, four, two samples) hold
  whichever side is steep. Sine2's gain is 1 + |y|.
- DblSaw: the second saw's offset y/2 wraps, so -y sounds as +y.
- Pulse: OscB's law - +-1 plus y, high for (1 - y)/2 of the cycle - silent at -1.
- SymPulse: exactly the wave at +y.
- Sine3, Sine4: the module saturates the ratio at zero; below zero they are the Shape 0 sine.
All eight are the reference model, checked sample by sample. Checked on the G2 2026-10-02 (OscShpB at E4, Shape Mod
from a Constant, y = +-1, +-0.75, +-0.5, +-0.25, 0, every wave): each -y take matches its +y twin, and the
engine matches the G2 within 0.16 dB on harmonics 1-16 at every setting. The one gap is Pulse at exactly
+-1, where only a one-sample click remains: the G2's is -41 dB per harmonic at both ends, the engine's
-33 dB at +1 and -43 dB at -1.
Eight stage patches cable Shape Mod; 18 Unreal Dreams' pad is two OscB pulses width-modulated by LFOs.

**6.8 FM (2026-09-28, from the reference model).** OscB, OscC, OscShpA and OscShpB (OscD has none: its
phase stage only adds the increment). Per sample the phase advances by the increment plus

    clip(2 x FM x input x ((1 - Trk) + 32 x Trk x k))

words, where FM is the FM dial through the cube + 1% attenuator curve (the instrument's, which is
`type_ii_attenuator()` exactly), Trk the FM Lin/Trk menu (0 or 1), and k the Pitch stage's first output word: the
key's increment WITHOUT the Coarse and Fine factors, which the Pitch stage multiplies in only for the
second word, the oscillator's own increment. At unity Coarse is 0x1c20d/2^23 and Fine 1/2, so the
oscillator's increment is 32 x 0x1c20d/2^23 x 1/2 x k. With a word of increment = 48 kHz and a unit a
quarter word:
- FM Lin: a deviation of 24 kHz x FM x input, whatever the pitch.
- FM Trk: a deviation of 2^23/0x1c20d x FM x input x (frequency / 2^(Tune offset/12)) - a constant
  index across the keyboard, measured from the key's pitch rather than the tuned one.
The deviation saturates at one word (48 kHz). The frequency can go below zero - through-zero FM, the
phase then running backwards. FM jacks: input 3 on OscB, OscShpA and OscShpB, 2 on OscC; FM dial 5
(OscC 4), menu 10 on OscB, 6 on OscShpA and OscC, 9 on OscShpB - the patch's own parameter order,
which for OscC is not the order the module lists them in. Only the Semi Tune Mode is checked; the others use the same
Tune offset.

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

**OscD's parameter 3 IS its Pitch Type (corrected 2026-09-28).** The instrument's own parameter list
for OscD reads Coarse, Fine, KBT, Tune Mode, On/Off; until 2026-09-28 the engine took parameter 3 for a
Pitch mod dial and played every OscD as Semi (revert record row 92). 08 Ice Pad's four OscDs are on
Partial at Coarse 75/73/71/69 - the 12th, 10th, 8th and 6th harmonics - and were playing 11, 9, 7 and 5
semitones up instead.

**OscShpA and OscShpB read it too (2026-10-07).** Both keep the drop-down at parameter 4, and the
engine read their Tune as Semi until this date. AnalogClassic's three OscShpBs are on Partial: the
middle one at Tune 63 (1:2) played a semitone below the others instead of an octave below, which was
the reported "two oscillators diverging in pitch".

## 7. Noise

**7.1 Model.** White noise through a one-pole low-pass whose corner the Color dial sets; each voice has
its own generator. Every setting's spectrum fits a one-pole within 0.5-0.7 dB.

**7.2 Table - SUPERSEDED 2026-09-25 by 7.2a, which the engine now plays.** The 2026-09-12 fit below
was taken on desk inputs 5/6, whose low shelf (findings.md 2026-09-25) is what flattened its dark-end
corners near 130 Hz and pulled its dark-end levels down.

| Color | 0 | 16 | 32 | 48 | 64 | 80 | 96 | 112 | 127 |
|---|---|---|---|---|---|---|---|---|---|
| corner Hz | 18306 | 7664 | 3164 | 1353 | 615 | 323 | 194 | 143 | 129 |
| level dB RMS re FS | -7.7 | -9.6 | -10.2 | -9.4 | -8.6 | -9.1 | -10.7 | -12.8 | -14.8 |

**7.2a The instrument's module, exactly (2026-09-18).** Read from the reference model - the whole module, not a fit:

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
scaled, not a share of the dry mixed back. The control word decides: one value outputs zero (the
module off), another applies the compensation, anything else passes `y` through unscaled.

**The corner** is a clean geometric run from **exactly 20000.0 Hz at dial 0 to exactly 12.000 Hz at
dial 127** (ratio 1.0602 a step) - the round endpoints are what confirm 96 kHz is the right rate.

| dial | 0 | 16 | 32 | 48 | 64 | 80 | 96 | 112 | 127 |
|---|---|---|---|---|---|---|---|---|---|
| instrument's pole, Hz | 20000 | 7855 | 3085 | 1212 | 476 | 187 | 73 | 29 | 12 |
| measured (7.2), Hz | 18306 | 7664 | 3164 | 1353 | 615 | 323 | 194 | 143 | 129 |

**ADOPTED 2026-09-25.** The dark-half disagreement below was the capture chain: desk inputs 5/6 carry
a first-order low shelf (zero 69 Hz, pole 197 Hz; inputs 19/20 and the Fireface are flat). Put through
that shelf and the 48 kHz band, this model reproduces all 17 measured levels to a constant -1.5 dB
(+-0.15; the level reference), and the "12 dB offset" is the engine's unit being four DSP words. The
engine now computes the pole as `exp(-2 pi f / rate)` with `f = 20000 (12/20000)^(dial/127)` - the
table to 1 LSB - and the gain as `1 + dial^3/65536`, the one-pole's 1/4 cancelling the x4. So the dark
end is now 3-6 dB louder and many octaves darker than the fitted table played. What follows is the
2026-09-18 reasoning for waiting, kept as the record.

The two agree to a few per cent while the noise is bright and diverge to a factor of TEN by the top of
the dial. THE ENGINE STILL USED THE MEASURED TABLE, deliberately. Putting the whole model above through
the same arithmetic gives a level curve whose SHAPE follows the measured one to about 1 dB over dials
0-64 and then drifts apart, reaching 6 dB by 127 - the same half of the dial the corners disagree on -
over a constant 12 dB offset that is B's own 1/4 and is presumably the output stage the capture was
referred to. So the bright half is confirmed and the dark half is not, and the measured table is what
currently reproduces the captured levels. Adopting the model wholesale would move the noise by that
12 dB and change the dark end by more, on an explanation nobody has heard yet: it wants a listening
check against the G2 first (todo.md).

**7.3 Level compensation.** The G2 adds back a share of the filtered signal growing as the dial cubed
(from the reference model), which is why the level barely falls as the noise darkens - but see 7.2a, which
casts doubt on whether it is the filtered signal that is added back.

**7.4 Check.** Engine and G2 meters agree at 6 of 9 settings; the brightest read one value higher in
the engine (a crest difference).

## 8. OscNoise

**8.1 Parameters.** On the instrument: Coarse 0, Fine 1, KBT 2, Pitch M 3, Tune Md 4, WidthMod 5,
**Width 6**, On 7. The module tables name 5 and 6 the other way round.

**8.2 The module, from the reference model (2026-09-27; replaces the measured model below).** A 24-bit
LFSR (shift left, XOR 0xd71d87 when the bit shifted out is 1) through a one-pole
tilt - pole 0x7bd7db (0.9675, about 500 Hz), input gain a quarter of 1 - pole - then THREE identical
Chamberlin band-pass sections in series at the pitch, each taking the one before it:

    pre  = g (in + in') / 2              g  = 2 (d^2 + min(d, 0.15625) / 16)
    low  = low' + h band'                fb = d^2 (1 - h)
    high = pre - low - fb band'
    band = band' + 4 h high              out = band (1 - h); the third section's out x 4

every stored value saturating at the word's +-1 (four engine units). `oscnoise_step()`.

**8.3 Pitch.** h = pi f / 96 kHz - LINEAR in f, not the sine the filters use - and it stops at
0x518368 / 2: the band centre goes no higher than 9.73 kHz, whatever the pitch asks. Near the top the
linear h lands the peak a little above the note (8.38 kHz for 8 kHz at Width 127), as on the instrument.

**8.4 Width.** w = (Width + Width M x the input) / 128, 64 units at Width M 127 moving it across the whole
dial, held to 0..1. Then d = 1 - 0.99 (0.9453 - 0.3984 w^2): 0.459 at Width 127, 0.064 at 0. Per
section that is a Q of about 3 at 127, 25 at 64 and 130 at 0 (measured off a 4 kHz band). Until
2026-09-27 the input moved Width four times as far (revert record row 83).

**8.5 Level.** Not normalised: the tilt makes the band's level peak near 500 Hz and fall about 3 dB an
octave above it - at Width 127, -5.3 dB RMS re 64 units at 125 Hz, -2.4 at 500 Hz, -3.3 at 1 kHz, -8.8
at 4 kHz, -12.5 at 8 kHz; narrower Widths a dB or two lower. The 2026-09-12 capture below found it flat
to +-1.5 dB, which it is between about 110 Hz and 1 kHz; the engine's old flat -4.5 dB was right there
and up to 8 dB loud above it.

**8.6 Checked** (2026-09-27): the model against the reference model, sample by sample on an
impulse and on noise, 250 Hz-12 kHz, Width 0-127: equal to the stages' fixed-point rounding (0.1% of
the peak on noise). The engine's rendered OscNoise against the reference model at C3-C8 and Width 127/64/16: the
same centre and Q, and the same level shape to +-0.5 dB (one constant apart - the engine's output
chain).

**8.7 The measured model this replaced (2026-09-12).** Two band-passes, Q = 3.34 x e^(0.032 (127 -
Width)) each, level normalised to about -4.5 dB RMS; see findings. It agreed with the parts within
about 10% on Q from Width 16 to 112, which is why it sounded right in the middle of the dial.

## 9. Node structure

**9.1 Inputs.** Up to `MAX_NODE_INPUTS` (10) - Mix4-1S's eight legs and Chain pair.

**9.2 Gains.** Up to `MAX_NODE_LEVELS` (12) smoothed gains per node; only nodes that use them smooth
them.

**9.3 Output legs.** `NODE_OUTPUTS` (9 - Sw1-8, §45). Each input reads the leg its cable's output maps to
(`srcLeg`, set when the chain is built); a source filling only two legs maps anything past its first
output to leg 1, as before.

**9.4 Sources.** `chain_has_source()` counts oscillators, Pulse, Noise and OscNoise; a chain without one is
reported as having nothing patched into it.

## 10. FltMulti

**10.1 Parameters and connections.** Freq 0, FreqM 1 (the PitchVar attenuator), KBT 2 (Off, 25, 50,
75, 100%), GComp 3, Res 4, dB/Oct 5 (0 = 6 dB, 1 = 12 dB), On 6. Inputs In, PitchVar, Pitch; outputs
LP, BP, HP - the node's three legs (§9.3).

**10.1a FreqM is twice the Pitch input** (2026-10-08, from the reference model): the module forms 4 x (PitchVar x FreqM + Pitch x 0.5), FreqM being v/128, so at full FreqM a unit on PitchVar moves the
cutoff two semitones where a unit on Pitch moves it one. Measured on the G2 with +8 units: FreqM 32, 64,
96 and 127 give +3.7, +7.8, +12.0 and +15.8 semitones; the engine had half of each and now agrees within
1%. FltLP and FltHP are built on the same coefficient stage, and their FM input was already
at two semitones a unit (notes §160).

**10.2 Filter (from the reference model, confirmed by 10.3).** A Chamberlin state-variable filter, one per
voice, run at the engine rate:

    low  = low' + F·band'          F = 2 sin(π fc / fs),  fc = flt_cutoff_hz(Freq)
    high = drive - low - q·band'   q = 2d²(1 - F/2),      d  = 1 - 0.99·Res/128
    band = band' + F·high          drive = input, × d with GComp on

(' is the previous sample.) **The cutoff stops at 20.8 kHz** (settled 2026-09-27 from the coefficient stage): the module clamps the half-coefficient F/2 at 0.6368 before it computes q, so a Freq, a key track or a
Pitch input that asks for more gets 20.8 kHz - the same ceiling as FltNord (§23.2). Before, the engine went
on to 0.45 of the graph rate (revert record row 77). **GComp** is the module's drive word: d with GComp on,
1 with it off - the module holds both and GComp chooses.

The (1 - F/2) in q is what keeps it stable at every cutoff and
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

**10.4 FltStatic (from the reference model, adopted 2026-09-13).** The filter of 10.2 with one output, chosen by
FilterType, and its own damping and drive:

- **Damping** d = 1 - Res/128, zero at 127 on the instrument; the engine stops at 0.01, FltMulti's top
  (`fltstatic_damping()`). The acoustic Q is 0.5/d², as the peaks measured in 2026-08 show (paramCurves.c
  notes §12).
- **Outputs** LP and BP are 10.2's 12 dB outputs; HP is `high`. The drive is 1, except BP, which is held to
  a unity peak while d² >= 1/2 (drive 2d², up to Res 37), and HP, whose drive is × (1 - F/2 - F²/4).
- **GC** multiplies the drive by d, as FltMulti's GComp does - except BP while d² >= 1/2.
- **Checked** against the reference model's arithmetic run sample by sample (Freq 30-100, Res 0-110, GC off and
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

**11.2 Shelves (from the reference model, confirmed by 11.6, EXACT since 2026-09-18).** Low shelf
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
editor's face had 6 and 11 the other way round until 2026-10-03. Inputs Pitch, PitchVar, Sync,
PW, Phase; Sync as §6.6.

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

**12.4 Modulation (from the reference model, 2026-10-03).** PW Mod and Phase Mod are v/128 (127 = 1);
PW is v/512 and Phase (v - 64)/256 of a word. the module adds 4 x (dial + amount x input) to its pulse threshold and
to its saw phase, with the input as a word (a quarter of an engine unit). So an engine input of 1.0 at full amount
moves PW by one whole dial range and the saw phase by half a cycle (OSCDUAL_PW_DEPTH 1, OSCDUAL_PHASE_DEPTH 0.5).
§12.5's "4x / 2x" are the same law per WORD of input; the engine had applied them per engine unit, four times
too strong (revert record row 127). CHECKED ON THE G2 2026-10-03 (OscDual at E4, a bipolar Constant into each
input at full amount, Fireface capture): PW at +8 and +16 units moved the duty 6.24% and 12.54% (law: 6.25%, 12.5%;
the old factor would have given 25% and 50%); Phase at +16 units, square and saw both full, matched the engine's
harmonics 2-8 within 0.15 dB, where the old factor was 9 dB out.

**12.5 Measurement.** 2026-09-12 - OscDual alone against an OscA sine at the same pitch, harmonics
read at each setting; the sub at three pitches, Soft off and on.
By meter the engine reads one value above the G2 at 9 of 10 settings - the pattern of §1.2: the
instrument's waveforms sit just under full scale, the engine's band-limited edges just over. Both
order the settings the same way (Soft above plain, the 180° mix below the 0° one). The sub is fitted on
its harmonic LEVELS only; its phase, and so its peak, is not pinned - the G2 meters it below full scale
where the model peaks near 1.9. Captured peaks cannot settle it: the output path rings on hard edges
(the plain square reaches 1.69 × the sine's peak in the capture while metering below full scale).

**12.5 On the instrument's laws (2026-09-17, supersedes 12.2-12.3 where they differ).** From the reference model: the square is LOW from phase 0 to 0.5 - PW/256 (dial 127 = silent), DC-free; the saw
RISES and steps at phase Phase/128 (Phase dial through dial/128, 127 = 1); the sub is a square an octave down,
low first; all three with the two-sample edge of §6.3; Soft = one-pole, coefficient 8 inc96, times 2. NO shelf on
the sub: the measured 190 Hz shelf was very likely the capture chain's own high-pass (§6.3 found -4 dB at 110 Hz on
that chain). The inputs' depths are §12.4's; over-range PW wraps.
Runs at the engine rate. NOT YET compared sample for sample with the harness - see todo.md.

## 13. FltComb

**13.1 Parameters and connections.** Freq 0, Pitch 1 (the PitchVar attenuator), Kbt 2 (Off, 25-100%),
FB 3, FB Mod 4, Type 5 (Notch, Peak, Deep), Level 6, On 7. Inputs In, Pitch, PitchVar, FB Mod.

**13.1a The Pitch attenuator is the mixer's Exp taper** (2026-10-08, from the reference model):
the Pitch dial reaches the part through the same curve as the mixer levels (§3.2, `type_ii_attenuator()`),
not linearly. Measured on the G2 with +63 units into PitchVar: Pitch 17 moves the teeth +0.2 semitone and
64 moves them +8.4; the engine had +8.5 and +32. GlassCathedral's LFO at Pitch 17 had swept its comb by
eight semitones - the "slow phasing" - where the G2's barely moves.

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

**13.4 The comb (the reference model, 2026-10-02).** One structure for every Type. With L the
Level word (half the mixer's Exp taper, §13.5), g = (FB - 64)/64 + 2 x FB Mod x input (saturated to
+-1), c = g x Y8 and b = g x Y9:

    line[n] = L x[n] + c tap[n-1]        tap[n] = line[n - D]        y[n] = 2 (b L x[n] + tap[n])

| Type | Y8 (c per g) | Y9 (b per g) |
|---|---|---|
| Notch | 0 | 1.0 |
| Peak | 0.9 | -0.4 |
| Deep | 0.9 | 0.6 |

D is one period less a sample (§13.2), read through a four-point Lagrange interpolator whose coefficients
are a 512-step table - the fraction is taken to 1/512. Every stored word saturates. So the feedforward
path is D long and the loop D + 1, and at FB 64 the output is the input delayed by D. As one section,
Peak is (-0.4g + (1 + 0.36g^2) z^-D) over (1 - 0.9g z^-(D+1)) and Deep (0.6g + (1 - 0.54g^2) z^-D) over
the same - which is why the 2026-09-12 fits found b = -0.30g, +2.45 dB g^2 and an extra sample for Peak,
and could not fit Deep above |g| = 0.5 (at full FB its gain is -6.7 dB, not -4.1). Checked against the reference model run sample by sample: within 2e-5 for all three Types, FB 0-127, delays 3-584 samples. On/Off off
passes the input.

**13.5 Level.** The mixer's Exp taper (§3.2), as the EQs' (§11.1).

**13.6 Measurement.** 2026-09-12 - noise through FltComb against the unfiltered noise, on a LINEAR
frequency axis (a comb is periodic in linear frequency): four Freq, feedback across the dial, all three
Types, and again at Level 64 so the resonant Types could not clip. Fitted per setting with the delay
free, then jointly per Type.


## 14. Operator and DXRouter

The instrument's own laws throughout (settled 2026-10-03), checked against the G2 through the Fireface
the same day - the confirmations are with each part. Revert record rows 128-133.

**14.1 One node.** A DXRouter and the Operators patched into its six inputs are played as ONE node
(`eNodeDx`). Their FM runs through the router in both directions - Operator out to router in, router
out to Operator FM - which a chain of separate nodes cannot evaluate: `add_node()` would recurse round
the loop until its depth guard stopped it. `dx_build()` finds the Operator feeding each router input
and copies its dials into the snapshot's `dxOp[]`, in the instrument's words (six per router,
`MAX_DX_OPERATORS` 24 = four routers). Each sample the operators run 6 down to 1: every modulation goes
from a higher-numbered operator to a lower one, so each operator's modulators are already computed
when it runs. An Operator NOT patched into a router is not played. The algorithm table is the DX7
chart, shared with the DXRouter graph (paramCurves.c `dx_algorithm()`), and it is what the instrument
routes, with the one exception in 14.5.

What the node reads from the voice rather than from cables: gate, Note and Vel - the Keyboard's Gate,
Note and Lin, which is how every DX patch on file cables all six Operators. The instrument reads the
Operator's own Gate, Note and Vel inputs, so a patch putting something else into them is NOT YET
MODELLED, nor are the Freq, Pitch and AMod inputs and the router's Out1-Out6 used anywhere else. An
unpatched AMod reads full scale on the instrument and changes nothing, as EnvADSR's AM does (17.5).

**14.2 The envelope.** It moves a LOG level, 0 to 0x7fffff, once a 24 kHz tick. The amplitude is
`kDxAmpWords[level >> 16]`, read linearly between whole steps by the low 16 bits: 129 points, 0x200000
(one unit) at the top, 0.70 dB a step. L1-L4 and Level reach it through `kDxLevelWords`.

Six segments, in order: R1 to L1, R2 to L2, R3 to L3, a hold, R4 to L4, and a final hold. A segment's
target is its L plus the note's offset (14.4), never below 0. Each tick the level steps towards the
target and, landing on it, moves to the next segment; a hold never lands. A gate rising (or a voice
retrigger) puts it back on R1 FROM WHERE IT IS; a gate down puts it on R4, every tick; Sync restarts
the phase as the gate rises. A step is a whole number of words:

- falling: `kDxDecayWords[r]` x 0xbd567
- rising: `kDxAttackWords[r]` x (0x1a9fc, plus 0x28f5c below 0x75c28f, plus 0x7ae14 below 0x600000),
  plus a jump of 0xa3d7 below 0x34b5dd - quick off the floor, slowing towards the top

where r is the rate plus the rate scaling (14.4), at most 99; a rate of 0 is not scaled, and the
rate dials never go past 99 in effect. Because a rate is a step on the log level, an envelope with
lower levels is over sooner - the manual's "lower envelope levels means that the entire envelope
cycle becomes faster".

The engine's level moves bit for bit with the instrument's: 400 random settings of every dial,
velocity, note and gate length, 11.9 million ticks, no difference; the amplitude within one word.
CONFIRMED ON THE G2: attack, decay to L2 and L3 and the release land within one 50 ms window of the
G2's, and the drop from peak to sustain matches to the decibel.

**Levels above 99.** The G2 accepts 100-127 for L1-L4 and Level, and `kDxLevelWords` carries the
instrument's own words for them, read by their low 24 bits. Measured: Level 100 plays exactly as 99;
Level 110 and 127, and L1-L3 at 127, are at the noise floor. The table reproduces all of that.

**14.3 FM and feedback.** A unit at an Operator's FM input moves its phase `DX_FM_CYCLES_PER_UNIT`
= 3.2706 cycles: 0x345487 x 64 over a two-word cycle, at a quarter word a unit. Modulators reach the
router's outputs at unity. Feedback is its source's previous sample times
`kDxFeedbackWords[group][Feedback]`, the groups being algorithms 6 and 32, algorithm 4, algorithm 18,
and every other; at 7 that is 0.348 for most algorithms. There is no averaging of the last two
samples, which is the DX7's arrangement and not the G2's. CONFIRMED ON THE G2 (2->1 at algorithm 1,
modulator Level 60-90, then algorithm 2 at Feedback 3, 5 and 7): the carrier's harmonics 1-10 within
0.2 dB, and within 0.4 dB with feedback, down to the capture's -60 dB floor.

**14.4 The note's offset.** Every segment's target is shifted by one offset, kept at the
accumulator's precision (a word is 1 << 24) as the instrument carries it:

- Level: `kDxLevelWords[Level]` less the top
- keyboard level scaling: the distance from BrPt in two-semitone steps, at most 63; Lin is
  d x 162263/40 and Exp a quarter of it (on a log level the "exponential" curve is the gentler line),
  times the side's depth (v x 0xffff) and 128; -Lin and -Exp subtract, +Exp and +Lin add. Level and
  the scaling together never go above the top
- then a fixed -0x77660, which puts an Operator at Level 99 with no velocity 5.2 dB below full
- velocity: `kDxVelocityWords[v]` x Vel/7, the index being the Keyboard's Lin as a word >> 14

The Note is the Keyboard's, 0x8000 a semitone from E4; BrPt v is the key v + 17 (E4 at 47). Rate
scaling adds RateScale x 0x84210 x (note word + 0x158000) >> 39 rate steps, counting up from key 21:
0 at the bottom, 15 at key 102 for RateScale 3. CONFIRMED ON THE G2: level scaling at keys 40, 64 and
100 (L-Depth 60 -Exp, R-Depth 60 -Lin) within 0.12 dB; rate scaling at RateScale 7, keys 40 and 88,
within one 10 ms window; velocity at Vel 7, velocities 127, 100, 64 and 30 through MIDI, within
0.03 dB.

**14.5 Main and pitch.** Main is twice `kDxMainWords[algorithm]` times the carriers' sum: 1.0 for
most two-carrier algorithms, 0.75 for three, down to 0.375 for algorithm 32's six. Algorithms 16-18
store the word 0x800000, which is -1.0, so their Main is twice the sum INVERTED. Algorithm 28 sends
Operator 2 to Main as well as into Operator 1 - the instrument's routing, where the DX7 has it as a
modulator only (`alsoMain`). CONFIRMED ON THE G2: one Operator through Main at algorithm 1 sits
8.8 dB below a full-scale OscDual square on the G2 and 8.45 dB below it in the engine.

Pitch is the instrument's: Ratio is the played note (Kbt on) or E4 (off) times Coarse (0 is a half)
x (1 + Fine/100); Fixed is 1, 10, 100 or 1000 Hz x 10^(Fine/100); Detune is 1 cent a step.

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

**16.2a No key in the FX area (2026-09-28).** The FX area has no keyboard, so anything there that tracks
the key sits at E4, the instrument's pitch zero: an FX oscillator with KBT on plays its Tune, an FX LFO
or filter with KBT moves nothing. The engine had passed the post-mix nodes a key of 0.0 - MIDI note 0 -
so 15 Randee dz's FX-area bass (OscDual and OscD, KBT on, played by an FX SeqNote) sounded five octaves
and four semitones low, below hearing. Only 15 of the stage patches changes.

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

**17.3 Integer steps, from where it is (adopted 2026-09-14, from the reference model, run tick by tick).**
Each stage is one recurrence on the current level, in the instrument's own integers, once per 24 kHz
tick, the level holding between ticks:

    level' = target + max(0, add + floor(2 · half · (level - target) / 2^23))

with half, add and target 24-bit words from its tables - each our time law rounded DOWN. The attack
aims at 0 with add the table's step (Log: half = e^(-ln16/n)/2, add = (16/15)(1 - e^(-ln16/n)); Exp:
e^(+ln16/n)/2 and (e^(ln16/n) - 1)/15; linear: 1/2 and 1/n, n = time × 24000) and ends when it passes
full scale. Decay aims at Sustain and release at zero, with half = floor(e^(-ln100/n))/2, or for LinLin
a whole-increment fall. `env_rates_build()`, `env_segment()`. The engine reproduces the reference model exactly at every tick (the shortest settings to 1e-5 of full scale, where our closed form and its
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
units (1.0 here) and Sustain v/128 of it - checked against the reference model.

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

KB and Reset are read on every envelope since 2026-09-27, at each module's own parameter number: KB is
0 on EnvADDSR, 6 on EnvADSR, EnvADR and EnvAHD, 7 on ModAHD, 9 on ModADSR and 11 on EnvMulti. Reset
is 2 on EnvADR, 3 on EnvAHD, 7 on EnvADSR, 8 on EnvMulti and 10 on EnvADDSR. **EnvD and EnvH have no
KB at all**: only their Trig jack starts them. Before this, every envelope except EnvADSR fired from
the keys, so 14 CS80project72's untriggered EnvD, wired straight to an Out, put a 1-unit step on every
note, which the G2 does not play (Fireface capture 2026-09-27).

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
BipInv pass audio while no key is held. `env_output()`; matches the reference model to 5e-7 at every
type, three Sustain settings and two AM levels.

**17.7 Normal/Reset (2026-09-16).** Reset puts the level to zero in the tick the gate rises, and the
attack runs from there; Normal (17.3) runs it from where it is. Matches the reference model tick for
tick through a retrigger during the release.

**17.8 How long 45 s is.** The dial says 45.0 s at 127 and the table's rate falls 40 dB in 44.7 s, but
each step rounds down, which matters most near silence: the reference model reaches -40 dB in 37.0 s,
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
| EnvAHD, ModAHD | A, H, D - no hold, a one-shot (EnvAHD is played by 17.11 since 2026-10-08, 17.11a) |
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
modules number theirs differently, so they gate from the key and never reset. EnvMulti is no longer played
by this walker but by the reference model's arithmetic (§17.11). Neither the stages nor the curves have been
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

**17.11 EnvMulti, from the reference model (2026-10-02).** Not the stage walker of §17.9: each
of the four segments carries a PROGRESS word p that starts at 0 when the segment is entered and steps by
the attack recurrence of §17.3 for the module's Shape and the segment's Time (p' = add + 2 half p, the
same tables as EnvADSR's attack). The segment's time is up when p passes full scale. The level is

- rising (target above the level the segment started from), and every segment under LinLin:
  start + p x (target - start) - the full attack curve SCALED to the step, so a rise to half scale
  takes exactly as long as a rise to full;
- falling, under the other three Shapes: target + decay x (level - target), the decay multiplier of
  §17.2 for the same Time dial - an exponential approach that is cut off when p's time is up, so the
  next segment starts from wherever it got to.

The held segment (Sustain L1, L2 or L3) and segment 4 do not advance when their time is up; the level
then stays where the formula leaves it. A rising gate (or a new key with KB on) starts segment 1 - from
L4 when Reset is on, from the level it is at when it is off - and a falling gate starts the segment after
the held one. With Sustain "none" nothing is held but segment 4 and the gate's fall is ignored. Levels
are v/128 (127 full). All four steps happen in the tick the gate changes. Checked against the reference model run
tick by tick with the instrument's own time tables: 0 difference over 128 runs (four Shapes, four
Sustain places, Reset on and off, rising and falling segments, short and long gates).

**17.11a EnvAHD is EnvMulti with three segments (2026-10-08).** The instrument builds EnvAHD from the
very stages EnvMulti uses - the same envelope stage and the same segment stage, three of them - so the engine
now plays it through 17.11 rather than the stage walker of 17.9: Attack to full, Hold at full, Decay to
nothing, the Decay segment held at its end and nothing held for the gate, whose fall is ignored. The HOLD
is a segment like the others: its progress word steps by the attack recurrence for its own Time dial and
the Shape, and it ends when that passes full scale, the level staying put meanwhile. Under the walker it
had lasted no time at all - a stage from full to full passed its level test on the first tick - so every
EnvAHD played Attack straight into Decay (Flows_DZ's notes were a quarter of their length). Checked
against the G2 (OscA > EnvAHD > 2-Out, Attack and Decay 0, C4): Hold 16, 32, 40, 48, 56, 64, 72, 80 sound
10, 58, 130, 272, 544, 1030, 1880, 3350 ms on the G2 and 10, 56, 128, 272, 544, 1034, 1892, 3348 in the
engine.

**17.9a A flat stage in the walker is timed (2026-10-08).** For the envelopes the walker still plays
(ModAHD's Hold, EnvH's H), a stage that neither rises nor falls lasts as long as the instrument's EnvH
part holds: a counter loaded with full scale on the trigger loses the Lin attack word for the dial each
envelope tick, and the output stays on while it is above zero (`env_hold_ticks()`). Checked against the G2
(an OscA at 6.6 kHz through the module, six notes a take): EnvH Hold 12, 16, 20, 24, 32, 48, 64 sound 4.1,
7.3, 12.6, 21.2, 54.2, 269.5, 1025.0 ms on the G2 and 4.1, 7.3, 12.7, 21.3, 54.3, 269.5, 1025.1 in the
engine. ModAHD, which has a part of its own, is within 0.3% (Hold 32: 54.7 against 54.6; Hold 64: 1028.4
against 1025.3), the G2 about a millisecond longer at short settings.

## 18. Pulse

**18.1 Width.** The Sub range's width in 96 kHz samples is the dial's displayed time (the Lo display,
over ten) less two samples: a cubic in ln over dial/127 (`pulse_time_seconds()`, notes §73). Within two
samples of all 17 widths measured on the instrument (8 at dial 0 to 96083 at 127). Lo and Hi are ten
and a hundred times Sub (manual). The old fit was 4% out at worst and could not reach dial 0.

**18.3 Time Mod (2026-09-27, from the time stage both modules share).** The Time M input moves the Time
dial by Mod x TimeMod steps - 64 units of Mod with TimeMod at 127 is the whole dial - and the sum is held
to the dial's range, then read through the same time law, so a modulated time is the law at a moved dial
position (the module interpolates its own table between dial steps). Pulse compares the time since its
rising edge with the current time, so a Mod that moves during a pulse moves where it ends. The Logic
Delay (§46) runs its time through the same part. Unpatched, both are exactly as before.

## 19. StChorus

The instrument's own law, adopted 2026-09-14 (from the reference model, run sample by sample). The engine
reproduces that model to 64-76 dB below the signal across the whole of both dials - the rest is the
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

The instrument's own network, adopted 2026-09-14 (from the reference model, run sample by sample). At the
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

The instrument's own filter, adopted 2026-09-14 (from the reference model, run sample by sample). The engine
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

The instrument's own filters, adopted 2026-09-14 (from the reference model, run sample by sample). The engine
matches that model to 0.01 dB across every slope and cutoff tried.

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

The instrument's own filter, adopted 2026-09-14 (from the reference model, run sample by sample). It is not a
ladder. It is FltMulti's Chamberlin state-variable filter (§10.2): one stage for 12 dB, and two of the
same type for 24 dB. The engine matches that model to 0.1 dB over every type, both slopes, GC on and
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

**23.5 The inputs** (2026-09-27, from the reference model; checked against it to −45…−89 dB, driven and self-fed):
- **Pitch** (the unscaled one) adds to PitchVar × Pitch M exactly as on FltClassic (§21.3). It was not
  connected before.
- **FM lin** adds FM × input straight to h, after the exponential law: h′ = |h + FM·x|. A result of
  full scale or more gives h = 0; then the 20.8 kHz cap applies. h from the pitch stage has already
  saturated just below 1, so only FM itself can overflow.
- **Res** adds Res M × input to Res/128, saturating to ±1, before d = 1 − 0.99·r. d cannot exceed 1,
  so a negative swing only opens the filter to no damping at all. GC follows the modulated d.

Both amounts are dial/128, with 127 counting as 1. x is in engine units, so a ±1 (±64 unit) signal
swings r or h by the full amount. A filter feeding its own output into Res at Res M 127 (09 Antarktis)
is a self-oscillator whose level the loop sets.

## 24. DelayA and DelayB

The instrument's own delay, adopted 2026-09-14 (from the reference model, run sample by sample). The engine
reproduces that model exactly: every output sample is identical for a click and a noise burst,
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

**24.2 Filters.** Both sets of words come from the instrument's own 16-bit-half arithmetic.
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
modulation stage and routes FB and DryWet through it:
- **FB** = max(0, FB + 4 × FB-mod input × FB-mod amount).
- **DryWet** = max(0, DryWet + 4 × input × amount), mixed LINEARLY: wet = min(1, 2x) and
  dry = min(1, 2(1 − x)), not the squared ramps of 24.4.

Both are worked out after each tap and take effect on the next sample. In the engine they are exact
exactly, including a Constant held against the FB-mod input, which can take the feedback to
nothing.

## 25. Compressor

The instrument's own compressor, adopted 2026-09-14 (from the reference model, run sample by sample). The
engine reproduces its output exactly across Threshold, Ratio, Attack, Release and Level.

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
1. **Level.** The larger of |L| and |R| - or, with the SideChain switch on, the side-chain input. One
   detector and one gain serve both channels (2026-09-28; the engine had been mono, reading In L only
   and copying it to both outputs - revert record row 90). The inputs are In L, In R and the side-chain,
   an unpatched one reading 0; the module's FIRST output jack (the right-hand one) is R and its second
   L - which is why every stereo stage patch crosses them into the next module's inputs. Nine of the
   nineteen stage patches drive it in stereo. It follows a peak at once and releases
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
Keyb amount is 0 at C1 and 1 at C6, and goes past both (-0.6 at note 0, 1.52 at 127). The octave shift
is the patch's Octave Shift (§63a), which the engine counts into the Keyb row since 2026-09-28.

The engine prepares these, since the audio thread cannot build nodes. Whenever the chain or a morph
range changes, it builds the chain at each morph's full amount as well, takes the nodes that differ
from the base build (at most eight per morph), and rebuilds just those modules along that morph's
axis: every velocity 0-127 and every note 0-127 (`build_axis_table()`, `build_module_node()`), so the
amount a voice plays is the law's exactly. Each voice picks its rows at note-on (and a Mono voice
returning to a held key its new Keyb row). **Until 2026-09-28 the axes were 32 velocities and every
other note**, rounding a morph by up to range/62 dial units on velocity and range/60 on the key: with
FltClassic Freq on a Vel range of 22, velocity 100 played 0.29 semitone flat - CT heard the resonance
low against the G2 (findings 2026-09-28, revert record row 101). Knob
smoothing stays per node, and a voice adds each table's offset from the base node to the smoothed
Freq, Res, gain, shape and mixer levels. FX Area nodes take the latest note's rows.

LIMITS (closed 2026-09-18 - see §26.2.2 and §26.2.3). A node both morphs move plays a merge, each
word from whichever axis moves it, and the words BOTH move come from a build at the pair of amounts -
so the two are summed before the conversion and the clamp, which is the law in §26.2.0. What is left
is a limit of count rather than resolution: the pair is tabulated at the same 128 velocities and
128 notes as the per-axis tables, and only the FIRST node a patch morphs on both axes gets one
(MAX_PAIR_NODES). Building the tables takes about
3.6 ms in a Debug build when every row is used; the plug-in does that on a thread of its own since
2026-09-18 (`rebuild_worker()`, g2Plugin.c notes §14) rather than on the audio thread.

**26.2.0 The law, confirmed against the reference model (2026-09-18).** Every morph
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

Checked 2026-09-17 against the reference model, at 187.5 Hz and Shape 0, 32, 64, 96
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
at the engine rate with a scaled to it (`oscillator_step()`); the model matched the reference model to
1.8e-4 of full scale sample by sample, and the engine's output matches its level, DC and harmonics. The
four-sample floor is the hardware's 0.013-cycle lobe at full Shape (329 Hz).

**27.3 Sine3 and Sine4 - the reference model (2026-09-25).** From the instrument's own stages, run
sample by sample. With g the Shape word and inc96 the phase step per 96 kHz sample:

    r    = g x (Y0 - 8 inc96),  Y0 = 8279556/2^23 = 0.98699, clamped at 0
    q    = DIV16( sin(theta)/16 , (1 - 2r cos theta + r^2)/4 )        Sine4: cos 2theta
    Sine3/Sine4 = 4q (1 - 0.703125 g)                                  in engine units

DIV16 is the division of 27.3a. 0.703125 is a fraction, 90/128. At Shape 64 this gives Sine3 at 0.648 of a pure sine and Sine4 at 0.648/(1 + r).

**27.3a The division.** Sixteen steps of restoring division on 24-bit words - of the dividend's magnitude, the sign put back afterwards - so the quotient is N'/D' to 16
bits. Once N'/D' reaches 1 it wraps. That wrap is Sine3's "ceiling": its harmonic ratio stops at 0.909 at
full Shape (E4) where the ratio law says 0.958. Sine4 never gets there. `dsf_divide()` does the sixteen
steps. Before 2026-09-25 the engine fitted this as a flat cap of 0.905 on r, and the
level slope as 0.642 against 0.703125.

**27.3b Checked on the G2.** The reference model and the engine both match the 2026-09-17 sweep
(G2Captures/oscshpb/sweep-2026-09-17) to 0.001 in level and ratio at all eleven Shapes at E4, for both
waves, and within 0.002 in ratio at E2 and E6. The sweep was taken on desk inputs 5/6, so each harmonic
is corrected for that path's shelf (zero 69 Hz, pole 197 Hz); that shelf is also the "1.10-1.13 lift of
harmonics 2 and up" the sweep's README describes.

**Two details matter**, and missing them once made the engine read 12 dB low: the coefficient is the fraction 0.703125, not the integer 90 (which makes the level term vanish), and the quotient is not shifted after
the divide.

**27.5 On the instrument's phase, at the engine rate (2026-09-17).** OscShpB runs once per engine sample, like the
basic oscillators (§6.3), against the reference model sample for sample (a test harness; 96 kHz):
- Phase p = 2 x phase wrapped to -1..1; x = 2 inc96 is its step per sample; y the Shape word (dial/128, 127 = 1).
- Sine1 peaks at half a cycle: argument phase + 0.5 + rise/2, rise = max((1 - y)/2, 2 inc96). -82 dB.
- Sine2's positive lobe ends at half a cycle: phase + 0.5 + lobe, lobe = max((1 - y)/2, 4 inc96). -62 dB.
- Sine3/Sine4 start 0.75 of a cycle on. -80 dB, except Shape 120-127 below ~1.5 kHz where the engine keeps the
  hardware-measured ratio cap (§27.3), which the harness (missing its level stage) does not have.
- TriSaw: FALLS from +1 at p = -y to -1, rises over max(1 - y, 2x); corners rounded by
  turn x (2 - |d|)^3 / 24 (turn = 2/rise + 2/(2 - rise)) at the peak (down) and at the phase wrap p = 1 (up; skipped
  at y = 1, where the peak is the wrap). -43 to -77 dB below 1.5 kHz, -25 to -32 dB at 6 kHz: the reference model's own division flips the sign of the samples beside the peak with tiny pitch changes, so those two samples
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
beat at 127, which at the reference 120 BPM is 0.0078 Hz to 32 Hz. It follows the G2's master clock,
as the delay's Clk and a Master-sourced ClkGen do (notes §200); 120 BPM only when no G2 has reported one.
Until 2026-10-01 the LFO alone stayed at 120 BPM, so on any other tempo it drifted against the patch's
own clocked sequencers (15 Randee dz).

**The LFO reads the table at dial/4 straight; the delay does not.** The delay's Clk goes through its own
slot map (`clk_sync_index()`, notes §21), reversed and compressed. Until 2026-10-01 the LFO went through
that map too, so dial 32 (4/1, 16 beats) ran at 1/16 of a beat: 8 Hz instead of 0.125 Hz. The dial's
text was always right - it already read clkSyncStrMap at dial/4, as the instrument's text does.

**28.3 RndSt and Rnd** (2026-09-27, from the reference model). The random waves are drawn from a 24-bit
linear congruential generator: seed' = the low word of seed × 0xb2d9d + 0x361963, arranged as the DSP
accumulator arranges it. The seed starts at 0 when the patch loads and is never reseeded by a note, so
the sequence repeats from load.
- A draw happens as the phase rises through the middle of its cycle, once per cycle. The output then
  moves **halfway** to the draw, not onto it: step′ = step + ⌊(draw − step)/2⌋. Before the first draw
  the wave sits at 0.
- **RndSt** outputs the step.
- **Rnd** follows it through a two-pole smoother clocked by the LFO's own phase increment, with
  x = 16 f/fs (at most 1):

      y += x·v        v += x·(step − y − (2 − x)·v)

  So the glide takes a fixed fraction of a cycle at any rate.

The old model drew from `rand()` on the wrap and jumped straight to the value. An LFO feeding its own
Rate input (09 Antarktis) then parked at −1 with its rate at the floor, and never drew again.

**28.4 The counter's inputs** (2026-09-28, from the reference model). LfoB and LfoShpA have a
counter stage with inputs that LfoA's and LfoC's simpler counter lacks:
- **Rst**: a rising edge (the last reading at or below zero, this one above) clears the counter to its
  word 0 - the engine's phase 0.5, where the Snc output goes high (§54).
- **Phase** and **Phase M**: the waves read the counter plus the Phase word plus 4 x input x Phase M. The
  Phase word is a per-waveform offset plus v/64 (the dial spans a cycle); Phase M is v/128, 127 full. In
  cycles: Phase v/128, Phase M input x amount / 2. LfoB's offset puts its Sine and Tri half a word
  (a quarter cycle) behind LfoA's reading of the same counter, its Saw and Sqr where LfoA's are; taken
  relative to LfoA, whose mapping the engine already uses. LfoShpA's per-waveform offsets are in §28.6.
- **Shape M** (LfoShpA): shape word + 8 x input x Shape M (v/128), saturated; the Shape dial is the
  bipolar word (v - 64)/64, so in the engine's 0..1 shape it adds input x Shape M.
- **Dir** (LfoShpA, input 5, 2026-10-02): the counter's step is multiplied by the input (4 x the word, so
  1.0 is unity). Unpatched it reads that constant, so the LFO runs forward; a negative input runs it
  backwards, 0 stops it, and a value above 1 speeds it up - it is a rate multiplier, not a switch.
The random waves still draw on the counter itself (§28.3), not the offset read.

**28.5 The sine is the oscillator's polynomial (2026-09-28).** The LFO's sine stage carries the same words as the oscillator's sine stage (0x800000, 0x648035, 0xadd4d5, 0x92aa9), so the LFO sine is `wave_sine_polynomial()` on
the folded phase, not libm `sin()` - within 1.5e-4 (-77 dB) of it, read a quarter cycle on to keep the
phase the engine already had. Also far cheaper: six LFOs a voice at the 96 kHz graph rate were a tenth
of 18 Unreal Dreams' render time. The rate skips its `exp2()` when nothing modulates it (bit-exact).

**28.6 LfoShpA's waves (2026-10-01, from the reference model).** Six waves, each a formula in the
read phase a (the counter plus the Phase word, a cycle running -1..1) and the shape s = 0.97 x the
Shape word (v - 64)/64, 127 pinned to full; Shape M adds as §28.4. The wave stage's own setting comes
from the Wave menu through {0, 5, 6, 2, 3, 4}, and each setting adds its own phase before the read:

| Wave | Phase added | Law |
|---|---|---|
| Sine | half a cycle | the sine polynomial (§28.5) of the skewed triangle r = 2(a s + \|a - s\| - 1)/(s^2 - 1) - 1 |
| CosBell | a quarter | the sine polynomial of the bell b: a' = a + s/2 (wrapped); b = -1 where a' >= s, else 1 - 2\|r(a')\| |
| TriBell | a quarter | the bell b itself |
| Saw>Tri | half a cycle | r itself |
| Tri>Sqr | a quarter | the triangle 2\|a\| - 1 times (1 + 2(s + 1)), clipped at full scale |
| Pulse | half a cycle | +1 while a < s, else -1 |

At Shape 64 (s = 0) the skewed triangle is the plain one, so Sine is a pure sine; Shape 1 makes it a
falling saw and 127 a rising one, as the manual says. Checked against the reference model run at every
Shape dial and 4096 phases: within 1.4e-5 for TriBell, Saw>Tri, Tri>Sqr and Pulse, and 1.0e-4 for Sine
and CosBell (the shared polynomial, §28.5). The engine's earlier waves were guesses: Sine and CosBell
ignored Shape, TriBell was a plain triangle, Tri>Sqr a tanh (notes §158), and none had its phase.
The Dir input is §28.4's.

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

**Depth x Mod is held to +-1** (64 units) before it multiplies In: the module forms it in a data
register, which saturates. So a Mod input above 64 units cannot push the gain past one. Confirmed
2026-09-27 by running the reference model against these formulas in all four Enable x m/1-m
combinations, Mod -64 to +192 units: equal to one LSB (revert record row 82).

**29.3 Depth rides on the node's `gain`**, which is what gives it the per-sample smoothing and the
per-voice morph offset every other level dial gets; the three drop-downs are read raw, because a
drop-down cannot carry a morph (manual p.20).

**29.4 Enable off, SETTLED 2026-09-27 from the reference model (and run, as 29.2).** Enable changes the module's last step, and what it swaps in depends on m/1-m: with m/1-m ON the output becomes In (the module is
bypassed), with it OFF the output is cleared (silent). Enable on runs the module. So "off" is a bypass
only for the crossfade form - the plain multiplier with Enable off makes nothing, which is also what it
makes at Depth 0. Until 2026-09-27 the engine passed In in both (revert record row 79). The stage
patches are unaffected: 02 Big Pad's two ModAmts with Enable off both have m/1-m on.

## 30. SwOnOffT

Added 2026-09-19. One parameter, 0 On.

Closed, the output is the input; open, it is nothing. With **nothing patched to In** a closed switch
sends 64 units, which is 1.0 in the engine (§16), so the module doubles as a manual constant (the
unpatched In reads a 64-unit constant).

**30.1 Ctrl is the switch's POSITION, 4 units a step (settled 2026-09-27)** - 4 units closed, 0 open -
the same code every switch's Ctrl carries (§33.1) and the Mux modules read (§68.3), so a switch can
drive a Mux to the matching position. The reference model writes the module's word as
0x20000 closed and 0 open, through the same path whose Range words FreqShift confirms (§57), stored
unshifted; the module copies it to Ctrl. SwOnOffM is the same
(its host writes the same word). A logic input reads 4 units as high, so only a Ctrl feeding a level
hears the change. Until 2026-09-27 the engine sent 64 units (revert record row 87).

## 31. LevConv

Added 2026-09-19. Two drop-downs, no dial: the range it READS (`levConvStrMap` {Bip, Pos, Neg}) and
the one it WRITES (`posStrMap` {Pos, PosInv, Neg, NegInv, Bip, BipInv}). Both are drop-downs, so
neither can be morphed and both are read raw.

**31.1 A straight line between the two ranges, and then SATURATED.** In engine terms (1.0 is 64
units, §16) Bip is -1..+1, Pos 0..+1 and Neg -1..0; the Inv output forms are the same range with
its ends swapped. The output is `outLo + (In - inLo) x (outHi - outLo) / (inHi - inLo)`. Bip to Bip
is therefore unity, which is how 01 Mini Emulator uses five of them.

The reference model computes `offset + 2k x In` and clamps the result to full scale, so the
affine form is confirmed and the saturation is not optional - an over-range input stops at the rail
rather than carrying on past it. The offset and the gain come from the instrument, which is where
the two drop-downs land.

## 32. LevAdd

Added 2026-09-19. Adds its dial to the input, and the dial is a Constant's: Bipolar (value - 64)
units, Unipolar value / 2 units, 127 reading exactly 64 in both (§16.1). It shares
`constant_level()` with the Constant module, so the two cannot drift apart.

## 33. Sw2-1 and Sw8-1

Added 2026-09-19, one node kind for both. **Out is the selected input, passed through untouched** -
the reference model reads the chosen connector and writes it, with no arithmetic on the way. The
selector is a radio button, read raw.

**33.1 The Ctrl output** is 0 units for In 1, 4 for In 2, and so on to 28 for In 8 (manual, Common
Switch parameters) - `select x 4 / 64` in engine terms. 01 Mini Emulator drives a ValSw2-1 from one.

## 34. ValSw2-1

Added 2026-09-19. In 1 normally, In 2 (the On input, and the lamp lit) while Ctrl EQUALS the value -
within half a unit either side. The value dial counts whole units 0 to 64, and its top step reads 64
rather than 63 - the same law the face prints (`render_paramType1UniPolShort`).

**SETTLED 2026-09-27 from the reference model**, which is unambiguous: it subtracts the value
from Ctrl, compares the magnitude with a word of half a unit, and selects On only when
it is not larger. The manual's "lower limit" describes a threshold; the instrument has none. Until
2026-09-27 the engine followed the manual (revert record row 74): In 2 from the value upward. With a
stepped Ctrl the two agree only at the value itself - above it the manual's switch stays on and the
instrument's goes back off. The value word is v x 2^15 - one unit a step - with 63 standing for 64 units, which is
the engine's reading of the dial.

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
module has no time law of its own. Its update writes two words into the module, both
indexed by the Time dial and both taken from the ENVELOPE's own tables (17.3):

- **Log**: a one-pole whose coefficient is `2 x (1 - envelope decay multiplier[Time])`. The factor
  of two is what makes the dial's printed Time the time to close the gap to **1%** of it, rather
  than the envelope's own reading of the same table entry.
- **Lin**: a constant step of **a tenth** of `envelope linear attack step[Time]` per tick. The step
  is a fraction of the 24-bit word's full scale, which is four of the engine's units (256 units), so
  in the engine's terms it is 0.4 / (Time x 24 kHz): 64 units take 2.5 x the dial's Time, an octave
  of pitch (12 units) 0.47 x. SETTLED 2026-09-27 from the reference model, run step by step: the step is multiplied by a word the instrument sets from Shape, -1.0 for Log and -0.1
  for Lin. Before, the engine stepped full scale in the dial's Time, 2.5 x too fast (revert record
  row 78).

The Log step is c x |gap| plus one least significant bit, so it lands exactly rather than
approaching forever; and both shapes stop ON the target once the gap is no larger than a step. Both
CONFIRMED by running the reference model tick by tick (2026-09-27): a Lin glide of 64 units lands in
61 501 ticks at Time 64, 3 258 at 32 and 524 288 at the top - 2.5 x the dial's Time each, as above - and
Log reaches 99% in the one-pole's time.

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

**36.2 Glide On is read a step late (2026-09-28).** The Glide's end stage decides whether to glide from
the Glide On it stored last time, then stores the new one. So a note whose own Gate switches the glide
on - 07 Unstable Lead's Keyboard Gate into Glide On through a Logic Delay - lands on its pitch at once,
and only the notes after it glide; the engine had been gliding into the first note too (revert record
row 93).

## 37. 2-In and 4-In from the jacks

Added 2026-09-19 as a silent node; PLAYS since 2026-10-08. The jacks on the back of the instrument are
whatever the caller hands `sound_engine_set_input()` before a render: four channels at the device
rate, In 1-4. G2 Alike's side-chain ("In 1/2", an aux bus off by default) is In 1/2; the application
has no input device yet, so In 1-4 are silent there.

- **2-In** reads In 1/2 (In from 0) or In 3/4 (1) on its two outputs; **4-In** from In reads all four.
  Both through the module's On and Pad (db12PadStrMap, +6/0/-6/-12 dB, as the FX input).
- **Level: the converter's full scale is a word's full scale, 4.0** (DSP_FULL_SCALE, notes §196; an
  oscillator is 1.0), before the Pad, and the input clips there. MEASURED 2026-10-08 off the G2's own
  2-In meter (law §1.1; 43 is 0x20 | 11, a flag seen only on this meter, and 76 is 12 with the clip
  bit), a 1 kHz sine from the Fireface into In 1, 1 dB steps:
  - Pad 0 dB: -48 dBFS reads 3, -40 4, -30 6, -24 7, -18 9, -15 and -12 11, -9 to 0 12 + clip. One gain
    fits every step, the 2-In carrying 11.3-12.5 x the Fireface's amplitude.
  - Pad -6 dB holds at 9 (just under 2.0) from -15 dBFS to 0; Pad -12 dB holds at 7 (just under 1.0)
    from -12 to 0. Unclipped, both would have reached 12 + clip: the input saturates before the Pad,
    at word full scale. Pad +6 reads one octave above Pad 0 throughout (db12PadStrMap order confirmed).
  - The converter's full scale sits between -12 and -9 dBFS of THAT Fireface output - a property of
    the interface's output level, not of the G2.
  So a host's full scale is the converter's, and a 2-In wired straight to a 2-Out plays 4 x VOICE_GAIN
  (-4.4 dB) against its input, the output trim every source goes through (notes §17). Two earlier
  versions the same day read full scale as 1/VOICE_GAIN (unity In to Out) and then 1.0; the second was
  12 dB short of the instrument.
- **Rate.** The input is brought up to the graph rate a block at a time, zero-stuffed and filtered by
  the output decimator's own 64-tap filter run as an interpolator: 32 graph samples (0.33 ms at 96 kHz)
  of delay, flat to within 0.03 dB at 18 kHz. A graph at the device's rate takes it as it comes.
- **Both passes read it** (notes §202): the block is filled before the voice thread starts, and the voice
  pass and the pass after the mix each keep their own position in it. A 2-In in the Voice area is a
  source, so a patch with one drones voice 0 at rest (§206).
- Only a patch with a 2-In or 4-In switched on pays for the conversion. A call longer than 1024 frames
  is rendered in pieces, because the block buffer holds that many.

CHECKED 2026-10-08, offline (a 2-In or 4-In wired to SimpleLead's 2-Out, a 1 kHz sine at -12 dBFS on
In 1/2, at the first version's unity gain): unity in the FX area at 48 and 96 kHz, serial and threaded; In 3/4 silent with only two
channels fed; in the Voice area through the patch's FX chain, serial and threaded within 0.01 dB. In
G2 Alike, tools/vst3host --bus-test feeds its side-chain: Out 1/2 reads -19.4 dBFS RMS at 4.0 (-15.0 at
the first version's gain). The engine's 2-In and 4-In meters follow the input (2026-10-08).

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
(`the formatter`), and `divModeStrMap` chooses the mode.

- **Gated**: every nth clock pulse is passed with its shape unaltered, so at a divider of 1 the
  train passes through untouched.
- **Toggled**: the output flips on every nth EDGE, and **both the rising and the falling edge
  count** - so an odd divider halves the frequency again. A divider of 3 divides by one and a half
  (manual p.236).
- **Rst is the barred arrow**: the reset does not act at once but waits for the next positive edge
  of Clk. It is the **rising edge** of Rst that arms it (2026-10-07): until that date a Rst held high
  reset the count on every clock, so 14 pattern seq's ClkDiv, reset from ClkGen's ClkActive, passed
  every pulse and clocked its SeqEvents sixteen times too fast. On the G2 the patch steps once a bar.

## 38.5 S&H

Added 2026-09-25 from the reference model (11 words). On each rising edge of
Ctrl - above zero now and not above it the sample before, the same test OscPerc's Trig uses - the output
takes In; otherwise it holds. Inputs In 0, Ctrl 1; per 96 kHz sample; an unpatched Ctrl reads constant 0,
so it never samples and holds 0. In the stage patches it mostly feeds KeyQuant, NoteQuant, CompLev and
OscB's Shape mod input, so most of its effect arrives with those.

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
| Noise Filter Freq | the instrument's cutoff table read FOUR entries higher than the filter modules' table: exactly `2 sin(pi f / 192000)` with `f = 16.35 x 2^((dial+4)/12)`, used directly as the filter coefficient, so the filter centres on f/2 - see 39.4 |
| Noise Filter Res | `dial/512`, capped at a quarter - see 39.9 |
| Noise Filter Sweep | `dial/128`; one semitone a dial step at full velocity and envelope - see 39.4 |
| Master and Slave Level, Bend Amount, Click, Noise | an exponential level curve - see 39.3 |

**The inputs are Trig, VEL, PITCH in that order (fixed 2026-09-25)** - the original face puts input 1
at the bottom beside "Vel" and input 2 in the middle beside "Pitch", and the G2 agrees (a Constant
into input 1 at 0 units silences it; into input 2 it moves the pitch a semitone a unit). The engine
and moduleResources.h had the two the other way round.

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
Read out of the reference model with everything but the click switched off, and confirmed
against the instrument.

| | the instrument | the engine, before |
|---|---|---|
| shape | held one envelope tick, then a ONE-POLE decay | a linear ramp |
| decay | x0.780851 every sample at 96 kHz (tau = 42 us, -20 dB in 0.1 ms) | linear to zero over 2 ms |
| peak | HALF the dialled level at 64 units of Vel, HELD one 24 kHz envelope tick (corrected 2026-09-25: "a quarter" was read at a guessed strike of 0x100000) | the full dialled level |
| level curve | the shared exponential (39.3) | the shared exponential - already right |

The decay coefficient is not fitted: it is a constant sitting in the reference model, and the
engine now uses it directly, rate-corrected as `0.780851^(96000/rate)` so an engine running at any
sample rate decays in the same TIME. Measured after the change, the engine's click decays at
0.78085 per sample and its level curve matches the reference's at every dial to a constant.

So the engine was roughly twenty times too long, four times too loud, and the wrong curve shape.
On a preset like Kick 1, where Click sits at 79, that is a large part of what CT heard as "more
noise than the G2" - a 2 ms full-scale DC ramp is a broadband thump.

**Hardware note.** The instrument measurement that prompted this (peaks 0.00163 / 0.01039 / 0.03845
/ 0.05824 at dials 32/64/96/127, all -20 dB within 1 ms) looked like a curve STEEPER than the
shared exponential. It is not - the reference model gives the shared curve exactly. A 0.1 ms
event at 48 kHz is about five samples, so the capture could not resolve its peak. **Do not fit a
level law to an event shorter than the capture can resolve.**

**39.9 The noise filter's RESONANCE - CORRECTED 2026-09-25.** The damping is

    d = 1 - 4 x 0.99 x resWord,      resWord = dial/512, capped at a quarter

so it runs down to 0.01 at 127. The 0.99 is A.Y[6], which the Noise Type action writes on every
Noise Type change (and so on every patch load); the frame image boots it to 0.8, and the 2026-09-21
reading below took that boot value. The G2 settles it: at Res 127 a noise hit rings for ~200 ms
before falling 20 dB, which a floor of 0.2 cannot do. What follows is the superseded reading:

    damping = 1 - 3.2 x resWord   - which FLOORS AT 0.2
The engine had `1 - 0.98 x dial/128`, which runs down to 0.028: far more resonant at the top than
the instrument ever gets, which is what the hardware saw (+14.4 dB of resonance at full Res
against the instrument's +10.9). Now taken from the instrument's law.

An earlier note in 39.4 guessed the engine was "four times too resonant because the instrument sends a
quarter scale". The quarter scale is real, but the relation is not a simple scaling - it is this
affine law with a floor, and the guess would have given far too LITTLE resonance. Reading the
filter beat guessing at it.

**39.10 SETTLED 2026-09-25: the noise path, from the reference model A.** Per 96 kHz sample, in DSP words
(the engine plays four times these - see 39.6):

    noise  = 24-bit LFSR, taps 0xD71D87, as a signed fraction
    colour = ((1+p)/2)(noise - lastNoise) + p colour,      p = 0.967525   (a one-pole high-pass)
    in     = colour x env x Amount,      env = Vel word x noise envelope (0.25 at 64 units)
    f      = min(1, cutoffWord x 2^(512 env sweepWord / 12)),      q = 0.9 d (1 - f/2)
             cutoffWord = 2 sin(pi F / 192000), F = 16.35 x 2^((dial+4)/12) - the table exactly (2026-09-25;
             read earlier as sin(pi F/96000), which drifts to 15% low at the top of the dial)
    stage  : low += f band;  high = in - low - 2q band;  band += f high     (each clamped to +-1)
    two stages in cascade sharing f and q; stage 2 is fed d x (stage 1's tap)
    taps   : Noise Type 0 = LP, 1 = BP, 2 = HP, the same tap on both stages

So the sweep is `512 x env x sweepWord` semitones - one a dial step at full envelope, which is the
hardware's 2.65 octaves at 32 - and the "10.3 Hz" resonant-peak law measured in 39.4 is the 20.6 Hz
table base read as a Chamberlin coefficient without the `2 sin`. Checked on the G2 2026-09-25 (rig
in findings.md): noise, click and oscillator energies all land within 0.8 dB of one constant, for
all three filter types.

**39.6 The oscillators and the bend, 2026-09-25, from the reference model B.** Each is a two-state
resonator on the 24 kHz tick, struck from rest on the trigger with `Vel << 1` - so every hit starts
at phase 0 - and its output goes through a one-pole low-pass whose coefficient is 8x its own
increment (clamped to 1). The master and slave decay words are the envelope's multiplier SQUARED
(the Decay actions square it), because only one of the resonator's states is damped; the
amplitude therefore falls at the envelope's own rate. The Pitch input and the bend envelope share
one accumulator:

    semitones = (Pitch + bendEnv) / 2^15, saturating at +-64;   bendEnv = Vel x BendAmount at the trigger, x BendDecay per tick

so full Bend at 64 units is 64 semitones.

**The engine's scale:** a DSP word is a quarter of an engine unit (a full-scale oscillator is 0.25 of
the word, 1.0 in the engine), so every DrumSynth output is its word x 4 - the strike's `Vel << 1` puts
the oscillators at 2 x Level. Checked on the G2 2026-09-25: the master alone peaks 3.5 dB above a
full-scale OscA sine on the same path (x4 predicts +4.5). The first port that day used x2, 6 dB low. What first read as a STALE REGISTER in the
pitch-index line is this bend term: the accumulator already held it. **Velocity: an unpatched Vel is
64 units**, not the key velocity - the G2 plays identically at MIDI velocities 32, 64 and 127 -
and a DSP signal unit is 2^15 (64 units = 0x200000).

**The resonator's own frequency law (closed 2026-09-25).** The increment is `k = 2 pi f / 24000`
for the nominal pitch, SATURATING AT 1.0; a two-state resonator given k plays `24000 asin(k/2) / pi`
- a little above nominal at kilohertz pitches - at `1/sqrt(1 - k^2/4)` of its strike, and the cap
holds every oscillator at or below 4 kHz (the slave's k is the master's times the ratio, capped
again). The engine plays exactly that, so full Bend and high Master Freq track the reference model to within the
measurement's own scatter (+-0.26 semitone at Bend 127) and stop at 4 kHz as it does, instead of
running on to the engine's Nyquist.

**39.5 The panel lamp, added 2026-09-20.** DrumSynth's face has an LED by its Trig and nothing lit
it: the engine published a lamp for the LFO alone, and every other module's LED stayed dark unless a
real G2 was attached to send one (CT). It now follows the MASTER ENVELOPE, which is what the
instrument shows - its lamp reads a level word of the reference model, the same way an
envelope's does, not the Trig input. So it comes on with the hit and fades out with it rather than
following the key: offline it lights at the note-on and goes out 263 ms later on the default preset,
with the key released at 80 ms. The face has one lamp and a poly patch has one of these per voice,
so voice 0 publishes and the rest do not - the rule the LFO already used, now in one place
(notes §194).

**39.4 The noise path - SETTLED 2026-09-25, see 39.10; kept as the record of how.** The
module is two stages of its own, one running at 96 kHz and one at 24 kHz, so its noise source,
its filter and their gains are all inside that model. **Nothing here may be fitted to a capture**:
the standing rule is that the instrument's own arithmetic decides, as it did for §§21-25.

What the instrument already gives, read off the parameter conversion (2026-09-20), so the reference model starts from a known input:

| dial | word the instrument sends |
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
  not used the way the filter modules use theirs - and that is a question only the reference model can answer.
- **The Sweep dial measures one semitone a step**: 0, 2.65 and 5.30 octaves at dials 0, 32 and 64,
  exactly linear. The engine's five-octaves-over-the-dial comes from the manual, not from the
  instrument, and is 2.1x too shallow - but the reference model has to confirm the law before it changes.
- **The resonance curve is the wrong shape** (SETTLED since - §39.9), quite apart from the level:
  peak gain relative to Res 0 runs 0, +0.6, +1.2, +5.0, +10.9 dB on the G2 at dials
  0/32/64/96/127, against the engine's 0, +1.2, +3.7, +7.6, +14.4. **The earlier guess in this section - that the engine is four times
  too resonant because the instrument sends a quarter scale - is DISPROVED**: quartering the dial gives
  far too little resonance, not too much.
- **The noise decays too slowly**: to -20 dB in 96 ms against the G2's 76 ms, on Kick 1's Noise
  Decay of 49.
- **The CLICK is a second, separate suspect and has never been isolated** (CT, 2026-09-21; SETTLED
  since - §39.4a). The 12 dB noise figure above is click-free - Click was zeroed on BOTH sides for
  it, as were the oscillators for the noise take and the noise for the oscillator take - so that number stands.
  But Kick 1 runs Click at 79, and the engine's click is invented from end to end: a LINEAR DC RAMP
  from full scale to zero over a hard-coded 2 ms, scaled by the shared level curve and velocity,
  added straight to the output. Nothing about it is measured. A 2 ms DC ramp is broadband, which is
  where the residual +1.4 to +2.0 dB in the top three bands over the first 30 ms could easily be
  coming from once the noise is right. **Isolate it**: Master, Slave and Noise at 0, Click swept,
  on both the instrument and the engine.

**THE REFERENCE MODEL PLAYS (2026-09-21).** The reference model now runs offline and produces a decaying
drum hit. What it took, beyond translating the two stages: the built-in tables at their right addresses
AND in the right memories, the pointers the patch set-up installs between the two stages, the four dials
that reach the DSP through custom actions rather than the plain parameter path, and - the thing
that took four rounds to find - **the PITCH input.**

DrumSynth is a VOICE module, and its oscillators are a two-state sine RESONATOR damped by the
decay multiplier, not a phase accumulator. A resonator has to be struck, and what strikes it is a
word derived from the Pitch input. An unpatched Pitch on the instrument carries the voice's own
pitch, never zero - so with Pitch left at zero the module is silent and every other hypothesis
about the silence tests plausible and negative in turn. Measured by removing one input at a time
from the working reference model: no Pitch gives nothing, no Trig gives nothing, and no velocity is worth
about half a dB.

It does NOT yet reproduce the hardware. Three inputs are still approximations - the pitch action's
fixed-point multiply, the Slave Ratio conversion, and what the voice supplies as a resting Pitch
(which sets the strike amplitude, and so the module's whole level). Until those are exact the
measurements above are the target, not something to compare against.

A change was drafted from these numbers and REVERTED the same day - the instrument's own logic is
the reference and a capture is only its check - which is why they are recorded here as targets
rather than as constants.

## 40. OscPerc

Added 2026-09-25 from the reference model (37 words, run sample by sample) and checked
on the G2. Dials: Coarse 0, Fine 1, Tune Mode 2, KBT 3, Pitch mod 4 through the shared oscillator pitch
path (§6); Decay 5, Click 6, Punch 7, Mute 8. Inputs Pitch, PitchVar, Trig.

**40.1 The voice.** Per 96 kHz sample, in DSP words (the engine plays four times these):

    k      = pi x inc,  inc = 2f/96000;  Punch: k doubled while the phase has not saturated (below)
    low    = low + k high
    high   = Decay x high - k low                                    (the new low)
    Trig edge (new > 0, old <= 0): high = 2 x Trig, phase = 0
    out    = out + min(1, 8k)(Click^2 high + (1 - Click^2) low - out)   - 0 when muted
    phase  = min(phase + inc, 1); every stored word limited to +-1

A two-state resonator struck by the Trig's own level (the Keyboard's Gate, 64 units, strikes at 0.5 of a
word), crossfaded between its two states by Click, through the same 8k-tracking low-pass as the
DrumSynth's oscillators (§39.6). **An unpatched Trig never strikes** - its edge input is wired to the
constant 0 - which the G2 confirms (silent).

**40.2 The dials.** Decay is the instrument's own decay table, a per-sample multiplier (notes §195;
0.681 at 0 to 0.99999 at 127). Click is SQUARED by the host (its Click action: Y3 = c^2, Y4 = 1 - c^2).
Punch changes one step: k doubles while the phase's add has not overflowed. The phase
restarts at each hit and saturates at 1.0 after half a cycle, so Punch is an octave-up first half cycle.

**40.3 Checked on the G2 (2026-09-25, outputs 3/4).** Against eight takes at E4 - Decay 32/64/100, Click
0/64/127, Punch on and off, velocity 32 and 127 - the reference model and the engine match the G2's relative
levels to 0.1 dB, its -20 dB times to 2 ms, its pitch exactly and its envelope to 0.6 dB down to -30 dB.
The absolute level is 3.36 dB over a full-scale sine on the G2 against 3.45 predicted. Velocity changes
nothing (the Gate is a fixed 64 units). Below about -45 dB the G2's tail decays more slowly: that is its
analogue output's AC coupling, about 3 Hz, turning the hit's DC area into a slow tail - reproduced
within 1-2 dB by a 3-4 Hz high-pass on the reference model's output, so it is not the synthesis.

## 41. KeyQuant

Added 2026-09-25 from the reference model (control rate, stateless) and the instrument's key update,
bit-identical to the reference model over 231,120 cases (six key sets, both Capture modes, four Ranges).

- **Range** scales the input first: `dial x 2^16`, 127 = 1.0.
- The scaled pitch is split into octaves and a 15-bit place within the octave (an octave is 0x8000, a
  semitone 2730.67), and the place is compared against twelve thresholds; the last one reached picks
  that key's value, and octave + value comes back in pitch units (1 semitone = 2^15).
- **Capture Closest**: each threshold is halfway between neighbouring keys that are on, and the octave's
  seam moves into the middle of the gap above the last key, so a note just under the octave goes to
  whichever key is nearer. **Evenly**: the octave is shared out equally among the keys that are on.
- A key that is off never matches; with **no keys on**, the scaled input passes straight through.

The thresholds use the instrument's key table (`k x 2730`), the values the module's own (`k x 32768/12`,
rounded): they differ by up to a word and both are kept.

## 42. LFO Mono

Added 2026-09-25. Every LFO's Mono/Poly selector (LfoA 1, LfoB 5, LfoC 1, LfoShpA 9) was ignored, so each
voice ran its own LFO from its own random start phase (notes §63) and the notes of a chord swept out of
step. **Mono is one LFO for the whole patch**: one phase, advanced once a sample outside the voice loop
and read by every voice, starting at zero when the patch is built. Poly keeps a phase per voice. It
changes 02 Big Pad and 17 Mighty Nord; a one-voice patch (05 SelfOsc LFO) plays as before, whose single
LFO already kept exact time across notes and gaps.

## 43. MinMax

Added 2026-09-25 from the reference model. Outputs in the module's order are **Min** then **Max** of
inputs A and B, read as they are, each saturated at the signal limit (4.0 in the engine, the 24-bit
word). An unpatched input reads zero.

## 44. ConstSwT

Added 2026-09-25. A Constant (§16.1: the same Value law and Bip/Uni reading, Bip/Uni here at index 2)
whose switch (index 1) clears the output when off - the instrument swaps a clear into the reference model rather than scaling.

## 45. Sw1-8

Added 2026-09-25. In goes to the selected output (0-7) and every other output is zero - the host
writes a gain of one to the selected output's word and zero to the rest. **Ctrl** is the ninth output,
4 units a step as on Sw8-1 (§33.1). Unpatched In reads zero. Nine outputs is why `NODE_OUTPUTS` is 9.

## 46. Logic Delay

Added 2026-09-25 from the reference model. Time uses Pulse's law and ranges (§18).

- **Pos**: a counter runs while In is high and resets while it is low; Out goes high once it has run
  the time out, so the rising edge is delayed and the falling edge is not.
- **Neg**: the mirror - the counter runs while In is low, and Out stays high until it has run out.
  At load In is low, so Out is high for one delay time, as on the instrument.
- **Cycle**: one whole pulse is shifted by the time; a pulse arriving while one is being delayed is
  ignored ("can only delay one single pulse", manual).
- The time Mod input moves the time as Pulse's does (§18.3).

## 47. RandomA

Added 2026-09-25 from the reference model; the draw sequence is identical to the reference model
over 200,000 draws at every Step setting.

- **Rate** and **Range** are the LFO laws (§28). A new value is drawn once a cycle, when the phase
  crosses the middle of its cycle.
- **The draw**: a 24-bit linear congruential generator, `seed = seed x 0xB2D9D + 0x361963` (mod 2^24),
  read as a signed fraction r. A one-pole moves towards it, `acc += p (r s - acc)`, and the value is
  `acc x 8192`, reflected once about +-1 and saturated.
- **Step** sets both p and s: p = 0x80200, 0x200200, 0x480200, 0x7FFFFF (/2^23) for 25-100%, and
  s = min(sqrt(2^23/p), 8192) x 2048 / 2^23, so a small Step is a slow reflected walk and 100% a fresh
  value every cycle, all with the same spread.
- **Edge** is a two-stage glide towards the value: with c = min(1, 256 x (2 x rate / fs) x e),
  `pos += c v; v += c (target - pos - (2 - c) v)`, output pos. e = 0x10000, 0x20000, 0x40000,
  0x100000 for 0-75%; 100% steps (c = 1).
- **OutType** Bip passes the value; Pos is 0.5 + 0.5x, Neg -0.5 + 0.5x. **Active** off gives zero.
- **Mono** is one generator for every voice, from seed 0; Poly gives each voice its own seed.
- The Pitch input is not read yet.

## 48. CompLev

Added 2026-09-25 from the reference model. Out is a logic HIGH while A >= C, where C is (value - 64) units and
127 reads just under 64 (0x1FFFFF). A equal to C is HIGH. Unpatched A reads zero.

## 49. NoteQuant

Added 2026-09-25 from the reference model; bit-identical to it over 13.7 million cases (every seventh Range,
every third Notes, inputs across the whole word). In words, with a semitone = 2^15:
- Range scales the input by v/128 (127 = exactly 1).
- Notes 0 (Off) passes the scaled input. Otherwise it counts steps of n semitones with the reciprocal
  as 0x7FFFFF / n, rounds half up, and multiplies back by n semitones; the result saturates.

## 50. LFO rate inputs and KBT

Added 2026-09-25 (manual, Common LFO parameters; the rate stage is the same exponential pitch law as an
oscillator's). The rate is the dial's rate x 2^(P/12), P in semitones (1 unit = 1 semitone):
P = fixed input + second input x Rate M (the Type II attenuator, notes §15) + KBT x (key - E4), KBT
Off/25/50/75/100%. LfoC has only the fixed input and no KBT; RandomA's Pitch input is a fixed one. A
Mono LFO (§42) steps at the rate a voice last computed from its inputs. Until now every LFO input was
ignored.

## 51. OscMaster

Added 2026-09-25 from the reference model. No sound: its output is the pitch an oscillator would play -
(key - E4) when KBT is on + (Coarse - 64) semitones (whatever the tune mode shows) + (Cent - 64)/128
semitone + Pitch + PitchVar x Pitch M - saturated. It drives the Pitch inputs of oscillators and LFOs
(08 Ice Pad drives an LfoC with it).

## 52. DlySingleA and DlySingleB

Added 2026-09-25. A bare delay line, no feedback or mix: Out is In, Time x step samples later, with
step = round(range x 96 kHz / 127) over the seven ranges 5 ms to 2.7 s - the law the Time readout
shows and DelayA/B follow (§24.1). DlySingleB's Time M adds In x TimeMod dial steps (TimeMod read
raw, clamped to 0-127), from the tap stage's words: full TimeMod at 64 units sweeps the whole range,
positive longer. Takes a line from DelayA/B's pool of four.

**52.1 The taps, from the reference model (2026-09-27).** DlySingleA's tap is the same program as DelayA's
(§24): a whole sample, Time x step back - the engine is exact there. DlySingleB's, and every tap of
DelayDual and DelayQuad, is the MODULATED tap: it adds the Time M word x the input to the Time word,
clamps to the dial's range, and reads between samples with a four-point Lagrange interpolator (the
fraction's top nine bits pick one of 512 coefficient sets, which are Lagrange's to the word). The
engine now reads those taps with Lagrange (`delay_ring_lagrange()`); DlySingle used a Hermite cubic and
Dual/Quad a linear read until then (revert record row 80). DlyEight's taps are whole samples at k x Time/8
for k = 1..8 (its Time word is the dial / 8), so tap 8 sits at the dialled Time; DelayQuad's Main output
is the far end of the line, the full Range.

## 53. OscPM

Added 2026-09-26 from the reference model. Pitch as the other oscillators (Coarse, Cent, KBT, tune
mode, PitchVar x Pitch M). **Phase M** is added to the phase the wave is read at, not accumulated:
In x PhM x 64 in phase words, which is **8 cycles per full-scale (64-unit) input at full PhM**; PhM is
the Type II attenuator, capped at 1. The **Sine** is the module's fifth-order odd polynomial on the
folded phase, x = 2|p| - 1 with p the phase over -1..1: x (1.5704 - 0.6459 x^2 + 0.0716 x^4), about
-cos(pi p), peaking at 0.996. **Tri** is the folded phase itself, with its two corners rounded by the
increment: OscPM's triangle is the same part OscC and OscD use, so it takes their correction and limit
(§6.3, `OSC_CORNER_LIMIT_PARTS`), applied 2026-09-27 (revert record row 86). Sync as the other oscillators' (§6.6).

The DX operators use the same phase-modulation stage, which puts their FM depth (§14.3, a guess of one
cycle per full-scale input) in question - todo.md.

## 54. LFO Snc output

Added 2026-09-26 from the LFO's counter stage. LfoB's and LfoShpA's Snc is not a pulse: it is a logic
HIGH through the second half of the counter's cycle (the counter at or past its midpoint), a 50%
square that ignores the Phase dial. Before this it carried a copy of the waveform. 13 Dist Activity
clocks four S&Hs from one.

## 55. Phaser

Added 2026-09-26 from the reference model. An audio-rate chain of second-order allpass sections
sharing two coefficients, swept by an LFO at the 24 kHz control rate:
- **LFO**: a counter stepping floor(Rate^2/2) + 32 per tick (0.046 Hz at Rate 0, 11.6 Hz at 127) - the
  readout's own law, which the engine calls.
- **Type I**: 2 sections; the sweep is the counter's sine (the oscillators' polynomial);
  g = 0.1155 + 0.0723 x sweep, c = 0.742; out = 0.5625 dry + 0.5405 wet.
- **Type II**: 3 sections; the sweep is 2v^2 - 1 with v = (1 + triangle)/2; g = 0.0297 + 0.0214 x sweep,
  c = 0.953; out = 0.5007 dry + 0.6169 wet.
- A section, with states s, t: w = u - t - 2gs - 2cs; s' = s + 2gw; t' = t + 2gs; out = t + 2gs + w - 2cs'.
- **FB** is floor(v x 113/127)/128 (0.883 at 127), from the chain's output back into its input.
- Checked: with FB 0 the chain passes white noise at exactly unity (1.0000) for both types, at both
  ends of the Rate range - it is allpass, which the decode had to get right in every sign.

## 56. FltVoice

Added 2026-09-26 from the reference model. A control stage places the vowel - Vowel dial
(v - 64)/64 plus VowelMod In x mod/64, clamped to +-1: -1 is Vowel 1, 0 Vowel 2, +1 Vowel 3, blended
linearly on each side - and shifts every formant by Freq: (v - 64)/2 semitones plus FreqMod In x
mod x 64 (a semitone a unit), clamped to +-32, as 2^(s/12). It hands four (damping, frequency, gain)
triples to the audio part: four state-variable resonators on the one input, summing the FIRST one's
low-pass and the other three's band-pass. Each vowel is four (frequency, gain) words from the
instrument's table (A, E, I, O, U, Y, AA, AE, OE); gains are scaled by Level (Type II attenuator)/2.
Res is the instrument's squared-linear word, 0.132 at 0 falling to 0 at 127. Bypass passes the input.

**56.1 The Freq shift is half a semitone flat (settled 2026-09-27).** The control stage turns the shift
into a multiplier with two of the DSP's built-in tables: the semitone table (entry 188 is 1.0) by the whole
semitones, and the cent table by the remaining fraction in 128 steps of 50/64 cent. The cent table's
first entry is -50 cents, and the part indexes it from there, so every shift comes out 50 cents below
(v - 64)/2: the formants sit a quarter tone under the vowel table's words at Freq 64, and reach them at
Freq 65. The engine took the table's zero as 0 cents until 2026-09-27 (revert record row 84).

## 57. FreqShift

Added 2026-09-26 from the reference model. A Bode shifter: two chains of four allpass sections in
z^-2, (c + z^-2)/(1 + c z^-2), the second hearing the input a sample late, make a Hilbert pair; two
phases a quarter cycle apart drive the sine polynomial. **Down = cos H1 + sin H2, Up = cos H1 - sin
H2.** The shift is x^3 x the range's word at 96 kHz, x = FreqShift/128 (127 = 1) + Mod In x mod,
clamped 0..1: Hi 0x42E40 (1568 Hz at full) and Lo 0x42C0 (97.8 Hz) match the readout exactly; **Sub's
word 0x80 gives 0.73 Hz at full where the readout says 8.78 Hz** - SETTLED 2026-09-27 by the reference model: its Range handler writes 0x80, 0x42C0 and 0x42E40, the same three
words, so the instrument shifts by 0.73 Hz at full on Sub and the 8.78 is the readout's own constant.
The engine follows the word; the dial reads what the G2 displays. Checked: a 1 kHz tone shifted 196 Hz comes out at 1196 Hz on Up at full level, the other sideband
37 dB down. Up to two per patch; more pass through unshifted.

## 58. Step sequencers (SeqVal, SeqNote, SeqEvent)

Added 2026-09-26. The three run the instrument's one 16-step stage; the engine runs that part exactly (identical to the reference model over 1.2 million samples of random clocks, resets, loops and parks at
every length, cycle, gate mode and polarity). Checked on the G2 at 192 kHz (findings 2026-09-26):
- **Steps jump; there is no smoothing** - a step change has the converters' own edge.
- **The new value lands two of the module's ticks after the clock's rising edge**, and the module ticks at
  its clock's rate: 96 kHz when clocked from audio, 24 kHz from control signals (21 us and 83 us on the
  G2). Which one is the module's rate: the module sits in the audio list when the module is up-rated and
  in the 24 kHz list when it is not. Since 2026-10-02 the engine does the same - a sequencer that is not
  up-rated reads its inputs and steps once per 24 kHz tick and holds its outputs between, so its value
  lands two 24 kHz ticks after the edge as on the G2, and a clock pulse shorter than a tick can be missed
  as it can there.
- Inputs Clk, Rst, Loop, Park, then an input added to each row's output; outputs Link, the first row,
  the second row. A clock already high at load counts as an edge, so the first step heard is step 2.
- Parameters: 0-15 the first row, 16-31 the second, 32 Cycle, 33 Length (value + 1 steps); SeqVal 34
  Bip/Uni, 35 the second row's Trig/Gate; SeqNote 34 its Trig/Gate; SeqEvent 34-35 each row's.
- Values: SeqVal v x 2^14 (v/2 units, 127 = 64), Bipolar (v - 64) units; SeqNote is always Bipolar, so
  (v - 64) is semitones from E4; on/off steps are 0 or 64 units. A Trig row passes its step only while
  the clock is high (two ticks late); a Gate row holds it.
- Up to 8 sequencers per patch.

**58.1 SeqNote's record stage** (2026-10-01, from the instrument's own record stage and the code that links
it). SeqNote runs a second stage after the 16-step one. It takes the value row (the step plus the Note
input), RecVal (input 6) and RecEnable (input 7), and is what drives the Note output:
- **While RecEnable is above zero the output IS RecVal**, raw and unquantised - the monitor the manual
  describes. Otherwise the output is the value row.
- **Recording** writes into the 16-step stage's own step words, so the stored sequence changes and keeps
  playing changed. Each tick while armed, the current step (the module's word 43 points at it) takes RecVal
  as a note: (RecVal + 0x200000) >> 15, convergent-rounded, negative read as 0 and 127 or more read as
  128; stored `note << 14`. Only the 16 steps are writable (the patch set-up sets the limit to step 16's word).
- **Arming**: a counter climbs by TWICE its delay word per tick while RecEnable is high and clears when
  it falls; it is armed once the counter passes full scale. The delay word is 0x7FFFFF (armed at once)
  for a part the patch set-up places as audio-rate, 0x10CC otherwise (about 41 ms at the 24 kHz control tick).
  Read from the module's link code: the delay word follows the record stage's list - 0x7FFFFF in the
  audio list, 0x10CC in the 24 kHz one - and the list follows the module's up-rate, which is what the
  engine keys it on. At control rate the engine now ticks the record stage at 24 kHz too, so the word is
  used as it stands.
- On the instrument the host also copies recorded steps back into the patch's step dials. The engine
  keeps them in its own state instead: a step's dial is copied in only when it moves, so a recorded step
  plays until that step is edited, and the whole sequence starts afresh when the patch's wiring changes.

## 59. ClkGen

Added 2026-09-26 from the reference model, exactly (identical over 12 million ticks with
random tempos, sync settings, swing and resets). It ticks at 24 kHz. A phase covers one "Sync every"
period of 2^n beats and steps by tempo x 0x55555 >> n; the tempo word is floor(BPM x 279.625), with
Tempo v reading 24 + 2v BPM below 32, 56 + v to 95 and 2v - 40 above (24-214 BPM). At 120 BPM: 1/96
gives 24 pulses a beat (30% duty), 1/16 four (swing moves every second one - 3380/2629 ticks at 32,
4495/1514 at 127), Sync a short pulse at the start of each period, ClkActive high while on. Rst
restarts the phase. **Master** follows the instrument's global clock (notes §200; 120 BPM when none is known).

## 60. NoteScaler

Added 2026-09-26 from the reference model: Out = In x Range, saturated, Range v x 2^16 (127 = full scale).

## 61. 2-In from a bus

2026-09-26. A 2-In set to Bus 1/2 or Bus 3/4 reads the Voice area's 2-Outs sent there, through the same
bridge as the FX input (18 Unreal Dreams sends its voices through Bus 1/2 into its FX area). From In
1/2 or In 3/4 it is the input (§37).

## 62. NoteSend

Added 2026-09-26 from the reference model. A rising Gate sends a note; falling releases the note it sent. Note =
dial + Note In in semitones (the dial word carries half a semitone, so it rounds), velocity = dial +
Vel In x 2 per unit, both clipped to 0-127. A NoteSend to "This" or to its own slot plays the engine's
own voices - which is how 18 Unreal Dreams plays itself from its sequencers; other channels would
leave by MIDI and are dropped.

**62.1 A NoteSend is a root of the graph (2026-09-28).** The chain is built backwards from the Out
modules, and a NoteSend feeds none, so until this date it and everything driving it (18's two
SeqNotes, their ClkGen) were pruned: 18 never played its sequences, and the 2026-09-26 "plays
itself" was the Voice area's own sequencer, not the NoteSends. `add_note_senders()` now adds every
NoteSend after the Outs. Checked against a G2 capture of 18 (capture-inventory): the same pitches
(140, 174, 207, 262, 415, 693, 931 Hz) and octave-band balance within 1-2 dB to 8 kHz.

A voice in 18 never finishes: Status's Patch Active keeps each voice's ClkGen running and its
SeqEvent re-gates Env3/Env4 after the key is up (KB OR the Gate jack, §17.4), so released voices keep
sounding and 18 fills all 32 voices within about 25 s of loading - on the instrument too, whose
queue allocator (§15.1a) the engine already follows.

## 63. Patch Volume

Added 2026-09-26 (CT: the slot volume in the top bar did nothing in engine mode). The patch's
Volume (Morph-area settings, per variation) scales the slot's output by the same exp curve as a
mixer level, 0.01x + 0.99x^3 with x = v/127 (the instrument's host sends that curve's word), and its
on switch silences the slot. Glided over ~10 ms. **Measured on the G2 at 192 kHz** (OscA sine,
outs 3/4, re Volume 127): 100 -6.18 dB, 64 -17.60, 32 -34.60 - the curve gives -6.18, -17.60,
-34.73. The dB the top bar prints (paramCurves notes §28: -7.7 dB at 100, -23.3 at 64) reproduces the
instrument's own printed scale but is NOT the gain it applies. A new or empty patch (the plug-in's
start, File > New) now has a Volume of 100, on, so the top bar shows it in engine mode too.

**63a Octave Shift (2026-09-28).** The patch setting (the Sustain settings module's first parameter)
transposes the keyboard by whole octaves. It is stored 0..4 with 2 as no shift, as every patch off the
instrument holds it; the engine ignored it until 2026-09-28, which put 11, 14 and 17 an octave high and
05, 06 and 16 an octave low. The editor's own Patch Settings panel read and wrote it as a signed -2..+2
and so showed "+2" for no shift; both now use the stored form.

**The settings are per variation (2026-10-07).** Octave Shift, Glide, Vibrato and Bend are stored in
each variation like Volume, and the engine reads the ACTIVE one. Until this date it read variation 1's
whatever was playing: ALARM DX plays variation 8, whose Octave Shift is -1, and sounded an octave high.

**Every module sees the shifted key (2026-10-07).** The shift transposes the keyboard, so the Keyboard
module's Note output and the Operators' key (their level and rate scaling, §14.4) carry it, as its
Pitch output and the oscillators always did. Until this date those two read the key as played, so a
DX patch with an Octave Shift scaled its levels an octave away from where it sounds.

## 64. RndClkA and RndTrig

Added 2026-09-26 from the reference model. Both use RandomA's generator (§47): the 24-bit LCG
x' = x.0xB2D9D + 0x361963.
- **RndClkA** draws on each rising Clk edge, with no glide: the one-pole towards the scaled draw and the
  fold, as RandomA. Step's word is 512 (v^2 + 1), saturating at 127 (the instrument's square law). A rising Rst
  reloads the generator from the Seed input and clears the one-pole. Parameters: 0 Step, 1 Mono, 2 (the
  display), 3 Bip/Pos/Neg, 4 on. Mono starts every voice from seed 0, so they run one sequence.
- **RndTrig** draws on each rising Clk edge and passes that pulse when the draw is at or below the
  threshold (Density v - 64) x 2^17 + Prob In x StepM (v x 2^16, 127 full), so Density gives the
  probability v/128. A passed pulse stays high while the clock does. Parameters: 0 Density, 1 StepM, 2
  on, 3 Mono. Inputs Clk, Rst, Seed, Prob.

## 65. DlyStereo

Added 2026-09-27 from the reference model: two of DelayB's lines (§24) fed from the one input, with
DelayB's LP, HP and dry/wet laws and its tap exactly. Each line's feedback is FB x its own
filtered tap plus X-FB x the other's, (y.FB + y'.XFB) >> 23, with all four amounts as DelayB's FB dial
word. Time L and Time R follow the tap's time law over the Range (mode 0: 500 ms, 1 s, 1.35 s), or
with Clk on the clock-sync law as DelayA's (notes §85-86). Out1 is the left line, Out2 the right.
Parameters: 0 Time L, 1 Time R, 2 FB L, 3 FB R, 4 X-FB L, 5 X-FB R, 6 Time/Clk, 7 LP, 8 Dry/Wet,
9 on, 10 HP. It takes two lines from the delay pool (§24).

## 66. MetNoise

Added 2026-09-27 from the reference model; no capture yet.
- **Frequency.** f = Freq v x 2^14 (127 = 2^21) plus FreqMod In x Mod (v x 2^16, 127 full), shifted up
  two and floored at zero; then w = (0.41421 + 0.58579 f)^2 (from 0.1716 to 1, the square root of 2's
  own constants).
- **Oscillator.** Six 24-bit phases, randomised at the start, step by w x ratio each 96 kHz sample,
  ratios 0x1e354, 0x28b44, 0x2d7b9, 0x362fd, 0x46666, 0x4aec3 (1 : 1.34 : 1.50 : 1.79 : 2.32 : 2.47).
  At full Freq that is 708-1756 Hz; at zero 121-301 Hz. Each phase that is negative after its step
  adds 1/8 to a sum that starts at -3/8, so the output is six squares centred on zero. Off (param 2)
  silences it before the filter.
- **Colour.** u = (2^21 - Colour v x 2^14) - ColourMod In x Mod, shifted up two and floored at zero
  (Colour is inverted, so ColourMod is too). Pole c0 = 0.67563 + 0.29011 u, a = 0.5 - 0.09375 u, and
  gain c1 = a (1 + c0).
- **Filter.** Eight one-pole high-passes in series, each t = s + c1.x, s' = c0.t - c1.x, which is
  H = c1 (1 - z^-1) / (1 - c0 z^-1). The output is the last stage shifted up two (x4), saturated.
  Colour 0 puts the corners near 500 Hz, and Colour 127 near 5 kHz with a lower gain.
- Parameters: 0 Colour, 1 Freq, 2 on, 3 Freq Mod, 4 Colour Mod. Inputs FreqMod, ColourMod. Up to four
  per patch; more are silent.

## 67. FltPhase

Added 2026-09-27 from the reference model. The engine is identical to them exactly (six
million samples across random dials, notch counts, types and modulation; KBT off).
- **Pitch.** p = (Pitch + PitchVar x PitchM) x 4 as a DSP word, the pitch domain of 2^15 a semitone.
  The multiplier is semitone table x cent table, both closed forms that round exactly:
  2^17 x 2^(s/12) and 2^22 x 2^(c/1536). That gives 2^16 x 2^(p/1536) with p's top bits in semitones
  and seven bits of cents.
- **Frequency word** B = Freq x KBT (>> 18) x that (>> 16), floored at zero. Freq is 2 sin(pi fc / 96 kHz)
  for fc = 100 x 160^(v/127) Hz: 100 Hz to 16 kHz, 127 full scale. It is within 3 LSB of the instrument's
  own table, which the engine does not carry. KBT is 2^18 at E4 and follows the key by Off/25/50/75/100%.
- **Spread word** A = (Spread v x 2^14 + Spr x SpreadM) x 4, floored at 0x30000. The damping is
  k = A - AB/2.
- **Filter.** The input x Level (the mixer's exp curve) enters at 1/8, plus the feedback word. Six
  state-variable allpass stages run in the 48-bit accumulator: s1' = s1 + B.s2, t = x - 2k.s2 - s1',
  s2' = s2 + B.t, y = s1' + t - 2k.s2'. Notches n (1-6) taps the output after stage n; all six always run.
- **Mix.** The amount is FB ((v - 64) x 2^17) + FM x FBM x 8. Type sets two words: Notch (loop 0,
  dry 1), Peak (loop 0.9995, dry -0.2) and Deep (loop 0.9995, dry 0.6). The next sample's feedback is
  tap x amount x loop. The output is tap x 8 + input x Level x amount x dry, so with Notch and FB at
  64 it is a pure allpass (no audible notches) and FB sets their depth and sign.
- Off passes input x Level. Parameters: 0 PitchM, 1 Freq, 2 SpreadM, 3 FB, 4 Notches, 5 Spread, 6 on,
  7 Level, 8 FBM, 9 Type, 10 Kbt. Inputs In, PitchVar, Spr, FM, Pitch. Up to four per patch; more pass
  their input through.

## 68. The remaining switches, counters and converters

Added 2026-09-27 from the reference model, each checked in a small patch built in code.
- **68.1 Momentary and 2/4-way switches.** SwOnOffM, Sw2-1M, Sw1-2M and ConstSwM run the same parts as
  SwOnOffT (§30), Sw2-1 (§33), Sw1-2 and ConstSwT (§44). The button is held rather than latched, and
  the engine plays the state the patch stores. Sw4-1 is Sw8-1 with four inputs. Sw1-2 and Sw1-4 are
  Sw1-8 (§45) with two or four outputs, Ctrl coming after them. Every Ctrl is 4 units a step (the
  host writes 0x20000 a step), and on a momentary switch 4 units when held.
- **68.2 ValSw1-2** sends In to Out 2 once Ctrl reaches the value, and to Out 1 below it, as ValSw2-1
  does (§34). **It switches at EQUALITY** (settled 2026-09-27, as §34): the module tests |Ctrl - value| <= 1/2
  unit, not the manual's "lower limit" (revert record row 75).
- **68.3 Mux8-1 / Mux1-8.** The step is Ctrl's word >> 17 (4 units a step), clamped to 0..7. Mux8-1
  passes the chosen input and Mux1-8 puts In on the chosen output, both at a gain of exactly one.
  **Mux8-1X is not built**: the reference model shifts the crossfade weights in ways reading it does not
  show, so it needs the reference model run rather than read.
- **68.4 T&H** follows In while Ctrl is above zero and holds the last value while it is not.
- **68.5 WindSw** passes In, and sends Gate high, while From <= Ctrl <= To. From and To are v x 2^14,
  half a unit a step with 127 = 64 units. An unpatched In reads 0.
- **68.6 8Counter / BinCounter** step on a rising Clk (above zero now, not before). A high Rst holds
  them at zero, and it is read after the step. The 8Counter holds one of its eight outputs high,
  wrapping after the eighth. BinCounter's outputs 001..128 are the count's bits.
- **68.7 ADConv / DAConv.** ADConv's code is (In word x 4, saturated) >> 16, two units a unit, so -128 to
  +127 over +-64 units in half-unit steps. D0..D7 are its bits, and D7 is the sign. DAConv reverses
  it, an input counting as a 1 when above zero. The pair round-trips a constant exactly, except +64
  units, which comes back at the top code, 63.5.
- **68.8 Red2Blue / Blue2Red** pass their input through. On the instrument they cross between the
  audio and control rates (Blue2Red holds each control value for four samples); the engine runs every
  signal at one rate.
- Logic outputs are 1.0 high and 0 low (§38).

## 69. The single-part modules

Added 2026-09-27 from the reference model, each checked in a patch built in code. WahWah is also
identical exactly to its part.
- **69.1 SeqLev** is SeqVal (§58): the same part and the same laws. Only its Bip/Uni default differs.
- **69.2 CompSig** sends a logic high while A >= B.
- **69.3 LevMod.** v = (Balance + ModDepth in x Depth) x 8, with Balance (v - 64) x 2^14 and Depth
  v x 2^16. Out = In x (1/2 - v/2) + (In x Mod x 4) x (1/2 + v/2), so Balance 0 is the clean input,
  64 is In(1 + Mod)/2 (AM) and 127 is In x Mod (ring modulation). Checked: 0.5, 0.375 and 0.25 for
  In = Mod = 0.5.
- **69.4 EnvFollow.** |In| lifts a peak word at once, and it falls by 0x2746/2^23 of the gap a sample.
  The output follows the peak with the Attack coefficient while rising and the Release one while
  falling. Each dial's coefficient is 1 - exp(-ln 100 / (T x 96 kHz)), falling to 1% in T. Attack T
  runs 0.53 ms (1) to 1 s (127), with 0 instant; Release runs 10 ms to 3 s. That is within 0.2% of the
  instrument's tables (release to 1 LSB); the engine does not carry the tables.
- **69.5 PartQuant.** Range is (v & ~1) x 2^16. |In x Range|, rounded to whole units, picks partial
  k <= 31, and the output is 12 log2(k + 1) semitones with In's sign (the table rounds exactly to this).
- **69.6 DlyShiftReg.** On a rising Clk the eight values shift along and In enters at Out 1, so Out k
  is In k - 1 clocks ago. The outputs hold between clocks.
- **69.7 DlyClock.** On a rising Clk it writes In into a 128-entry ring and outputs the value written
  N clocks before (N the dial, 0 = the value just written). It holds between clocks. Up to four a
  patch.
- **69.8 Digitizer.** The rate is 32.70 Hz x 2^(v/12) (a semitone a step, 50.2 kHz at 127) plus
  Rate mod. It goes through the semitone and cent tables (§67) times 0x1c20c; the phase gains twice
  that each sample, and each overflow of +-1 takes the input. The output is the held word ANDed with
  the Bits mask, which keeps the top v + 1 bits (12 = Off). Off passes In. Checked: 330, 1318 and
  10548 changes a second at Rate 40, 64 and 100.
- **69.9 WahWah.** s = (Sweep v x 2^14 + Sweep in x mod) x 4, floored at 0, and q = s^2. Three
  words run from bottom to top along q: damping 0x66666 + q x 0x15c28f, frequency 0x1374c + q x 0x7ced9
  and gain 0x11eb85 + q x 0x333333. A state-variable filter takes In at 1/4 (lp' = lp + 2f.bp,
  hp = In/4 - lp' - 2d.bp, bp' = bp + 2f.hp), and Out = bp' x gain x 16. Off passes In.
- **69.10 RandomB** runs RandomA's six stages (§47): Rate 0, Mono 1, Kbt 2 (Off..100%), Rate mod 3
  (the exp attenuator, as an LFO's, §50), Step 4 (a dial, 512 (v^2 + 1), of which RandomA's Step menu
  is the points 32/64/96/127), on 5, OutType 6, Range 7, Edge 8. Inputs Rate and RateVar move the rate
  as an LFO's do. At RandomA's settings it renders identically to RandomA.
- **69.11 NoteDet** gives Gate (the key held), Vel and RVel from the engine's held-key table, for every
  voice alike. The instrument writes Vel at note-on and RVel at note-off, each as v x 2^14 - v / 128, so 127
  reads 0.992 - and RVel holds until that key's next release (2026-09-27; before, RVel read 0 and Vel
  was v / 127, revert record row 85).
- **69.12 4-In** from the jacks is In 1-4 (§37). From Bus (2026-10-02) it is both buses, bridged as
  2-In's are: outputs 1-2 are the Voice area's 2-Outs sent to Bus 1/2, outputs 3-4 those sent to Bus 3/4,
  through the same On and Pad.
- **Not yet: Rnd Clock B and Rnd Pattern.** Their RndState and RndLoop parts hand values to each other
  through shared registers and read a host word not yet identified, so they need the whole module run
  in the harness, not part by part.

## 70. Basic versions of the remaining modules

Added 2026-09-27 at CT's request, breadth first: every remaining module at a basic level, to be refined
one by one later. Each follows the manual's description, and uses the instrument's laws where they
were cheap to take; engine-module-status.md lists each as Partial with what is basic about it. None has
been compared with the instrument yet.
- **70.1 DelayDual / DelayQuad / DlyEight** read taps off one shared delay line, with the seven-way
  Range and the delay time law (paramCurves notes §19-20). Dual and Quad move each tap's dial by its
  mod input x its amount (64 = one engine unit a full dial). Quad's Time/Clk uses the clock-sync law,
  and its Main output reads the Range's full time. Eight's taps are at 1..8 x the Time spacing, and the
  Range is the total to tap 8. The taps are the instrument's (§52.1, 2026-09-27).
- **70.2 Flanger** (the reference model, 2026-10-03). A sweep stage on the 24 kHz tick and a
  delay stage every 96 kHz sample:
  - **Sweep.** A signed 24-bit phase steps by Rate x 16 a tick (8 at Rate 0): a saw at
    v x 384000/2^24 Hz, 0.01 to 2.91 Hz (the manual's 24.4 Hz is wrong). Its absolute value is a
    triangle, starting from a random phase drawn at load. The delay word is
    floor(Range x 0xdbec x |phase|) + 0x128000, and the read sits (word >> 14) + 1 + t samples back, with
    t = ((word >> 9) & 31)/32: 75 samples at the floor, 511.4 at Range 127.
  - **Ring.** 512 samples. A 4-point Lagrange read at 1/32-sample steps, with the four coefficients
    halved, so the read is half the delayed signal d. A delay past the ring wraps onto the newest
    samples, as the module's mask does; only Range near 127 at the top of the sweep reaches it.
  - **Sums, every word saturated.** The ring takes 0.8 In + 2 x FB x o, where FB = floor(v x 7000000/127)
    of a word (0.834 at 127). The second output o = 0.6 In + 0.3 d is the feedback, so the loop gain
    tops out at 0.5. Out = 0.6 In + 0.8 d, or In with the module off; the ring keeps running while it is
    off.
- **70.3 PShift / Scratch** (the reference model, 2026-10-03). The same five stages in both: a
  100 ms line, two taps, a crossfade, and a control stage on the 24 kHz tick:
  - **Taps.** One signed 24-bit phase p, with the second tap at p plus half a cycle. Each tap reads
    9728 x X1 x (1 + p) samples back (a 4-point Lagrange read), where X1 = 127/2048 x 2^Delay gives
    windows of 12.6, 25.1, 50.3 and 100.5 ms. Each tap is weighted 1 - p^2, so the two gains sum to 1
    at a jump and 1.5 between. **The output is the NEGATED sum.** Off passes In.
  - **Rate.** The phase steps by 6990.67 x (1 - ratio) x 8/4/2/1 a tick, for Delay 0..3, so the delay
    moves 4 x (1 - ratio) samples a tick.
  - **PShift ratio.** Coarse is a quarter semitone a step (+-16 semitones), interpolated linearly between
    the semitone table's entries. Fine is +-50 cents (127 = +49.2). Pitch M is v/128 (127 = 1) at **half
    a semitone a unit**.
  - **Scratch ratio.** A word R = (v - 64)/512 (127 = 1/8) passes a one-pole s = 0.01 R + 0.99 s +
    Ratio M x input (the input's word, NOT through the 0.01: a few hundredths of a unit move it as far as
    the whole dial), saturated to +-1. The speed is 4 x clamp(8s), +-4, backwards below 64. Both gains
    are scaled by clamp(64 x (8|s| - 1/128), 0, 1), which is silent at 64 and full from 66.
  - **What a capture would check.** With a short window the jump is a large fraction of a cycle, so the
    output's carrier can cancel into sidebands at the crossfade rate (440 Hz at Coarse 112 in 12.5 ms
    reads 880 +- 40). That is the structure itself, not the engine.
- **70.4 OscString** (the reference model, 2026-10-03). A 7000-sample line (13.7 Hz at the
  lowest), pitched by the oscillators' Pitch stage (§6):
  - **Period.** 2^25 / increment, which is exactly 96000/f samples. It is read with a 4-point Lagrange
    at 1/512 sample. the module adds two samples and the read sits two samples in, so the loop is exactly
    one period with Damp at 0.
  - **Loop.** The read passes a one-pole s += Damp x (read - s), with Damp = (127 - v)/128 (v = 0 gives
    1, no damping; 127 freezes it). The line takes In + Decay x s, and Out is s. With the module off the
    loop runs on and Out is 0. Every word saturates.
  - **Decay** is the loop gain per period: 1 - t(127 - v), interpolated as the dial is.
    t(i) = exp(2.070135 - 11.050795 x 0.9869^i) for i > 0 and t(0) = 0, which reproduces the instrument's
    128-entry table within 0.1%. Decay 127 never decays; 64 keeps 0.936 a period, 0 keeps 0.0004.
  - **Damp moves the pitch.** The one-pole adds its own delay and nothing compensates for it: at Damp 64
    the string is about 1 sample long, +8 cents flat at A4 and +34 cents at A7.
- **70.4a Resonator** (the reference model, 2026-10-03). Two delay lines meeting at a junction, every
  96 kHz sample, in words (a quarter of an engine unit); every stored word saturates.
  - **Lengths.** The period is OscString's (96000/f samples, the oscillators' pitch dials); less six samples, it
    is split Pos : 1 - Pos (Pos v/128, 127 = 1) between line 1 and line 2. Each line reads through a 4-point
    Lagrange; with the averager's two samples the loop is exactly one period.
  - **Junction.** Line 1 takes 0.97 (a L1 + b L2 + c Exc); line 2 takes Decay x four half-sample averages
    ((1 + z^-1)/2 each) of the Damp one-pole of (d L2 + e L1 + f Exc). Decay and Damp are OscString's laws
    (§70.4). The Alg sets a..f and the Out1 mix:

    | Alg | a, b, c | d, e, f | Out1 |
    |---|---|---|---|
    | String1 | 0, 1, -1 | 0, 1, 1 | 0 |
    | String2 | 0, -1, 1 | 0, -1, 1 | -(L1 + L2) |
    | Tube1 | 0, -1, 0 | 0, 0, 1 | L1 |
    | Tube2 | 0, 1, 0 | 1, 0, 1 | L1 - L2 |
    | Tube3 | 1, 0, 1 | 1, 0, 1 | -(L1 + L2) |

  - **Outputs.** Out1 is that mix of the two lines; Out2 is what enters line 2. Off zeroes both junction
    outputs. In1 is the excitation; In2 and In3 are Pitch and PitchVar.
  - **What it means.** The strings are one loop through both lines, with the excitation entering at the Pos
    point in opposite senses - a pluck position: at Pos 64 the fundamental leads, nearer an end the higher
    harmonics do. The tubes do NOT close their own loop (Tube1 has none): they are made to be driven by a
    Driver whose return comes from Out1, which closes it.
- **70.5 Driver** (the reference model, 2026-10-03). One stage per Type, all every 96 kHz sample, in
  words (a quarter of an engine unit). In1 is the excitation (breath pressure, bow velocity) and In2 the
  return from the resonator. Stiffness is v/128 and Embouchure v/128 (127 = 1 for both); Bow uses
  Embouchure / 16.
  - **Reed** (and -Lip- and -Mallet-, which share its part): r = Emb - 4 Stiff (In2 - In1), saturated to
    +-1; out = In1 + r (In2 - In1) - the classic reed table, Embouchure its offset and Stiffness its slope.
  - **Bow**: v = 8 Stiff |In1 - In2 + Emb|, saturated at 1; out = min(3 (1 - v)^3, 1) (In1 - In2) - a
    friction curve, full stick near zero relative velocity.
  - Every stored word saturates, so out stays within a word.
- **70.6 NoiseGate** (the reference model, 2026-10-02): a follower, a gate, an attack-hold-release
  envelope and a VCA. Out is In x the envelope; Env is the envelope.
  - The follower, every 96 kHz sample, in words: stage 1 jumps up to |In| and falls towards it by
    0x2746 / 2^23 a sample; stage 2 jumps up to stage 1 and falls towards it by 0x1A10 / 2^23.
  - The gate opens when stage 2 is above Threshold (v/128) and shuts when it is below three quarters of
    it - hysteresis. Switched off, the gate is held open: the module still passes In through its
    envelope, which then sits at full.
  - The envelope is the ADSR's arithmetic (§17.3) at the 24 kHz tick, Log shape: attack to full in
    0.25 ms x (1 + 19 v / 127)^2 (the attack words this gives are the instrument's, bit for bit), held
    while the gate is open, then a fall with the decay law over ((v + 35.72) / 162.72)^5 s (Release 0:
    0.5 ms) - within 0.1% of the instrument's release times.
  Checked against the reference model run together: the gate opens and shuts on the same sample, Out within 1e-5
  and Env within 1e-4 over 40 000 samples of bursts at five settings.

- **70.7 PitchTrack** (the reference model, 2026-10-03). Pitch 0 is E2 (82.41 Hz), 12 units an
  octave, as ZeroCnt's (§70.7a). Every part runs on every 96 kHz sample; see the end of this section.
  - **Gate.** |In| passes a two-stage follower: the first stage attacks instantly and releases by
    0x2746/2^23 a sample, the second follows it up and releases by 0x1a10/2^23. Gate goes high when the
    second stage exceeds Threshold (v/128 units of a full-scale signal, 127 = 1) and low below 3/4 of it.
  - **Positive peaks.** In passes a one-pole low-pass (pole 0x7d6103, about 316 Hz) and then a DC blocker
    (pole 0x7fe645, gain (1 + pole)/2, about 12 Hz). The positive half of that passes the same kind of
    follower (second-stage release 0x270a/2^23). A sample counts as a peak when it reaches 0.9 of the
    follower.
  - **Negative peaks** come the same way from the negative half of the RAW In: the module wires its input
    there directly, not through the filters.
  - **Flip-flop.** A positive peak sets it and a negative peak resets it (reset wins). The flip-flop is
    the Period output: a square at the tracked pitch, not the "very short pulse" the manual describes.
    The counter measures the samples between its rising edges.
  - **Outputs** are Period, Pitch and Gate, in that order, which is the manual's order. The editor's
    face had Pitch and Gate labelled the other way round until 2026-10-03.
  - **The rate.** These parts may be placed in the every-sample list or the 24 kHz one, and the counter's
    offset word is 0 in the first case. Offline, the parts at 24 kHz miss periods of a 440 Hz saw (360
    edges a second); at 96 kHz they track sines and saws from 55 to 880 Hz exactly. So the engine runs
    them at 96 kHz.
- **70.7a ZeroCnt, exactly (2026-09-27):** the Track stage reads its input once a 24 kHz tick, counts the
  ticks between rising crossings (the last reading at or below 0, this one above) WHOLE, and the Calc stage
  turns the count into 12 log2((24 000 / count) / 82.41 Hz) units - the table's law to 0.01 units. So the
  reading steps: 440 Hz, 54.5 ticks, reads 28.9 or 29.4 by turns, not 29.0 (revert record row 88).
- **70.8 Vocoder** (the reference model, 2026-10-03). Two stages: converters every 96 kHz
  sample, and a band bank on the 24 kHz tick. Words are a quarter of an engine unit.
  - **Section.** Every filter is a chain of one form: w = 2(u - k1 w2 - k2 w1), y = w/2 + k3 w2 + k4 w1,
    i.e. (1 + 2k4 z^-1 + 2k3 z^-2)/(1 + 2k2 z^-1 + 2k1 z^-2). The first section takes k0 x the input;
    each later one takes the previous output shifted by a fixed power of two. Stored words saturate.
  - **Converters.** Ctrl passes one section (a gentle low-pass near 11 kHz) and In four (a steep
    low-pass, 0.94 to 8 kHz, -20 dB at 10 kHz). Each is doubled and held every fourth sample. The bank's
    output is held, passes the same four sections, and is scaled by 0x651eb8 x 8 (2.97 overall in the
    passband).
  - **Bands.** Sixteen contiguous bands with edges at about 205, 325, 461, 606, 778, 974, 1200, 1460,
    1775, 2152, 2601, 3159, 3876, 4890, 6430 and 8566 Hz. Band 0 is a two-section low-pass below
    205 Hz; the rest are four-section band-passes. Passband gain is 0.5. Analysis (Ctrl) and synthesis
    (In) use the same coefficient words, which the engine holds as the instrument's data: the edges are
    a designed set, not a curve.
  - **Followers.** Each analysis band is rectified into p = max(r, R r + (1 - R) p), then
    e = max(R p + (1 - R) e, A p + (1 - A) e): release R and attack A per band, from 0.0021 and 0.0165
    (band 0) up to 0.0179 and 0.218 (band 15), a tick.
  - **Routing and output.** Synthesis band k is scaled by the envelope its BandSel names (Off: none). The
    sum x 16 is the output.
  - **Emphasis** replaces the analysis input with 8 (e0 x - e1 e0 x[n-1]), e0 = 0x5061f1, e1 = 0x4bd344.
    **Monitor** outputs Ctrl unchanged.
  - **Checked.** The bank was checked against the instrument's program run sample by sample: 53 dB below
    the signal, with emphasis off and on, under a scrambled routing. The converters are read from the reference model and are not yet run.
- **70.9 RndClkB** (the reference model, 2026-10-02) is RndClkA (§64) with a Step M stage in front.
  Parameters: 0 Step, 1 OutType, 2 on, 3 Mode (Mono, as RndClkA's), 4 Step M; inputs Clk, Rst, Seed,
  Step M. With the Step M jack unpatched that part is not linked in, and the host gives Step's words
  exactly as RndClkA's - B is A. With it patched, the module computes them each tick from
  s = Step + 4 x input x Step M (both v/128, s held to 0..full): the one-pole's word q = s^2 + 0x200, and
  the draw's pre-scale from the module's own approximation to 1/sqrt(q) - a linear seed 0.5833 - 0.3333 m
  on the normalised mantissa, the exponent halved - within about 0.2% of the instrument's exact root. The
  engine reproduces both words for every one of the 2^23 step words. The Character mode picks the
  generator stage: Rnd1 is RndClkA's linear congruence, Rnd2 a 24-bit shift register - shift left one,
  and XOR 0x872B41 when a bit falls off the top - with the same pre-scale and one-pole after it. Mono
  clears the generator's word to 0, where the shift register stays: Rnd2 in Mono holds still until a
  Rst reloads it from Seed. Poly starts each voice from a random word. **RndPattern** (the reference model, 2026-10-02): a loop stage, then - by the Wave mode - Val's
  generator (RndClkA's, Step's square law) or State's gate, then the level shift. Parameters 0 Pattern,
  1 Bank, 2 Step, 3 Loop, 4 Step M, 5 OutType, 6 on; inputs Clk, Rst, Seed, Seed (fine), Step.
  - The loop stage reseeds the generator's word with Pattern (v - 64) x 2^15 + Bank (v - 64) x 2^8 + Seed
    + Seed fine / 128 (in words, saturated), and clears the one-pole, on a rising Rst - unpatched, Rst
    reads high, so that happens once at the start - or once its counter, restarted at -Loop, has counted
    past zero. Each rising Clk counts and draws, so the pattern repeats every Loop + 1 clocks (1-16).
  - Val: the draw is RndClkA's value.
  - State: each rising Clk draws; while the clock stays high the output is +1 if the draw (a signed
    24-bit word) is at or below Step's (v - 64) x 2^17 + 8 x Step input x Step M (v/128), otherwise it
    stays -1; whenever the clock is low it is -1. Before its first tick the level is 0.25.
  Checked against the four stages run together, sample by sample: within the output's truncation (6e-7)
  over 30 000 samples of clocks, resets and moving seeds, in both modes.

- **70.10 SeqCtr** (the reference model, 2026-10-02). Steps are words v x 2^14 (v/2 units,
  127 = 64), events 0 or 64 units. Parameters 0-15 steps, 16-31 events, 32 Pulse (0 trigger, 1 gate), 33
  Pol, 34 XFade (0-3); inputs Ctrl, Val, Trig, added to the outputs Val and Trig.
  - Ctrl / 4 units picks the step. At or above 64 units, or below 0, it picks a rest pair instead: 0, or
    the centre when bipolar - so the output rests outside the dial's range.
  - Within the step, w = (1 - the position) x 1, 2 or 4 (XFade 3, 2, 1), saturated at 1, and XFade 0 makes
    w = 1: out = step x w + next step x (1 - w), so XFade 3 fades across the whole step and 1 across its
    last quarter. Step 16's next is step 1. Bipolar takes the centre off and doubles.
  - Trig is the step's event - as a gate, or (Pulse 0) for four ticks after the step word changes (the module's ticks: 96 kHz up-rated, 24 kHz otherwise, as §58's sequencers; the engine does the same).
    the module tests that by comparing the whole accumulator, so ANY Ctrl whose low 16 bits are not zero
    counts as a change: a stepped Ctrl landing on exact multiples gives four-tick pulses, a moving one
    keeps Trig up for as long as the step's event is set.
  Checked against the reference model run as such, sample by sample: identical at every XFade, Pulse and
  Pol, over 20 000 samples of ramps, wobbles and out-of-range Ctrl.

- **70.11 Mux8-1X** (the reference model, 2026-10-02): a control stage turns Ctrl into eight gains
  and a mixer sums gain x input. Ctrl is held to 0..63 units and input k sits at 9k (not Mux8-1's 4-unit
  step). With d = (0x7fff - 256 X-Fade) >> 1, each gain is

      clamp((512 (8d + 0x4000) - 112 (8d + 0x2000) |Ctrl - 9k|) / 2^23, 0, 1)

  - a flat top, then a linear fall. At X-Fade 0 an input holds full gain to 4.3 units from its place and
  is gone by 4.8, so midway between two inputs each plays at 0.63; at 127 the top is 0.5 units wide and
  the fall reaches 8.6, a near-linear crossfade (0.51 + 0.51 midway). The sum saturates at the word.
  Checked against the reference model run sample by sample: within 5e-5 at X-Fade 0-127, Ctrl -2..66.
- **70.12 LevScaler** (the reference model, 2026-10-03). A 24 kHz stage makes the gain and a second
  multiplies:
  - **Key and breakpoint.** The key is the Note input plus, with Kbt on, the keyboard (both in keys from E4).
    BrkPnt is v - 64 keys (127 = +64).
  - **Slope.** Below the breakpoint the distance is multiplied by L.Gain's word, -341 (v - 64)/63 (+341 at
    v = 0); above it by R.Gain's, +341 (v - 64)/63 (-341 at 0).
  - **Gain.** Distance x word / 256 is a step of the instrument's semitone gain table (2^(step/12), 0.502 dB
    a step, interpolated linearly between whole steps), clamped to -128..+95 (-64 dB .. +47.7 dB). So a
    Gain dial at 127 is 8.02 dB an octave, and a positive dial raises the keys on its side.
  - **Outputs.** Level is the gain (1.0 = 0 dB), saturated at 4; Out is In x Level.
- **70.13 The MIDI and panel modules.** Status (the reference model, 2026-10-02): Patch Active
  is LOW - read off the G2 2026-10-04 (an Invert on it lit, held note, the patch sent from the editor and
  loaded from a bank alike), though the manual says it goes high on load. SeqOscExp depends on it: its two
  sequencers restart each other through Invert(Patch Active) and an AND Gate, and with Patch Active high
  the chain stops and the patch is silent; Var Active is high, and low for one 24 kHz
  tick after the variation changes - a trigger, not a level; Voice No. is the voice's index (its low five
  bits) x 4 units, 0 in the FX area.
  Device gives the wheel, aftertouch, control pedal (morph group 5), sustain, pitch stick, and the G2X
  global wheels 1 and 2, which arrive as MIDI CC 96 and 97 on the slot's channel (the manual's fixed CC
  list).
  CtrlRcv and NoteRcv (the reference model, 2026-10-02) take MIDI as it arrives, before any channel
  filter. Channel 1-16 is that channel, This the slot's own (the editor's MIDI input channel, or any when
  it is set to all), Keyb the keys that play the voices. CtrlRcv: each arrival of its controller sets Val
  to v/128 (127 full) and raises Rcv for one 24 kHz tick; Val is 0 until the first. NoteRcv: Rcv is high
  while its note is held, Vel the last note-on's velocity / 128, RVel the last note-off's; on Keyb it is
  NoteDet (§69.11). In the plug-in only notes reach them - a VST3 host gives a plug-in no MIDI controller
  stream beyond the ones it maps to parameters. CtrlSend, PCSend, Automate and NoteZone render nothing.

## 71. OverDrive

The instrument's own (2026-10-04), replacing a fitted soft-knee curve (revert record row 134). Two stages: a
coefficient stage on the 24 kHz tick, and the shaper on every 96 kHz sample. The engine follows the reference model to within 7e-4 of a unit sample by sample, over all four Types, Sym and Asym, Drive 16-127
and inputs of 0.3-3 units.

**71.1 Per sample.** Words as fractions, each saturating at a word's full scale; the input is a word, a
quarter of an engine unit.
- Feedback (Heavy only): v = f x the shaped sample last time + (1 - f) x in.
- Asym's square term: t = -k + v + k v^2 (k 0 for Sym).
- Gain: u = 16 (drive x the type's share + Y2) t.
- The polynomial, twice: p(z) = 8 (Y3 z + Y4 z^3 + Y5 z^5); s = p(p(u)).
- A 2-pole high-pass on s/4 at about 7 Hz (coefficient 0xfa0/2^23, damping 1), which takes out Asym's DC.
- Out = dry x in + wet x the high-passed s.

**71.2 Type and Shape.** Soft, Hard, Fat and Heavy set the polynomial's words (Y2-Y5, from one setting each:
0, 0x40, 0x7f, 0x30), the drive's share of the gain (1/4, 1/2, 1, 0) and Heavy's feedback (0.75 of the
drive, halved). Asym sets k = 0x7f x 0x1cca (0x40 for Heavy); Sym, 0. `overdrive_words()` in paramCurves.c
does this in the instrument's own integer arithmetic, so the module face's curve and the engine share it.

**71.3 The coefficients, per tick.** d = Drive + Mod x Drive Mod, both dial/128 with 127 counting as 1,
clamped to 0..1. Dry is (1 - d)^2 and wet 1 - (1 - d)^2, so Drive 0 is transparent.

**71.4 Checked.** Against the G2 inside 14 CS80project72 (OverDrive Soft, Sym, Drive 32): the energy above
6 kHz after it is -41.5 dB in the engine and -41.6 on the G2; the fitted curve gave -40.2. At 0.5 units the
old curve made the 5th and 7th harmonics 18 dB too strong.
