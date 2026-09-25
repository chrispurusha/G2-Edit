# waveModels.c notes

The longer comments from `waveModels.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

The measured wave laws. See waveModels.h for why this file exists and why four of the eight waves
are parameters rather than samples. Every constant below was fitted to a capture of the real
instrument, not taken from the manual - several of the manual's descriptions are wrong, and where
they are, the comment says so.

## 2. `wave_sine1()`

-- Sine1 -------------------------------------------------------------------

The manual: "a phase modulated sine wave. At 50% Shape setting, the signal is a perfect sine wave
and at 99% similar to a sawtooth wave". MEASURED 2026-08-23 over a nine-point Shape sweep,
recovered by inverting sin() on the captured cycle to read the phase warp w(p) off the hardware:
```
  - Shape 0 gives w(p) = p to three decimals, i.e. a pure sine. The harmonics grow from Shape 16
    upwards, so the dial acts over its whole range and not just above half.
  - w(0.5) = 0.500 at every Shape, so the descending zero crossing never moves and the duty stays
    at 0.500. An earlier model let the duty collapse to 0.10, which drew a sine's top lobe crammed
    into a tenth of the cycle with a full-width bottom lobe hung off it.
  - w(1 - p) = 1 - w(p): the warp is odd-symmetric about the centre.
  - The slope is piecewise constant with ONE breakpoint per half, sitting exactly at the peak,
    where w = 0.25.
```
So it is a one-parameter warp. b is the breakpoint and b = 0.25 is the identity, which is why
Shape 0 is a clean sine; as b shrinks the rise compresses into the front of the cycle and the
remainder becomes a long linear sweep from w = 0.25 to w = 0.75 - a slow fall from peak to
trough, which is the sawtooth the manual describes.

REPLACED 2026-09-17 BY THE INSTRUMENT'S OWN LAW (reference §27). The rising half takes (1 - g)/2 of
the cycle and the falling half the rest, each linear in angle, with g the Shape word (dial/128, 127
counting as 1) - so the breakpoint above is b = (1 - g)/4, which the measured table matches to 0.0005
at every setting but full Shape. There the instrument holds the rise to two samples
(`wave_sine1_limited()`, the engine passes that; the drawing does not), which is 0.0034 at 329 Hz against
the fitted 0.008 - a difference only far above the harmonics the capture could read. The fitted
quadratic is kept below as the record of how the form was found.

## 3. in `wave_sine1()`

RE-MEASURED 2026-08-30. The LINEAR term was already right; the QUADRATIC one was about five
times too large, which lifted b at the top of the dial and made our Sine1 duller than the
instrument - reported by ear as "hardware definitely sounds brighter than our version".

```
    raw        0      16      32      64      96     127
    b     0.2500  0.2185  0.1870  0.1250  0.0630  0.0080     <- measured
    b     0.2500  0.2188  0.1896  0.1367  0.0915  0.0550     <- previous law

```
Fitted per point against the harmonic series and anchored at the identity warp, residuals
+/-0.0013. The model FORM is confirmed by the same fit: it reproduces the measured harmonics
to 0.21 dB or better at every setting, and to 0.02 dB at raw 127 (-8.0/-11.9/-14.5/-16.5/
-18.2/-19.6 against a measured -8.1/-11.9/-14.6/-16.6/-18.2/-19.5). At full Shape the wave is
about 2 dB under an ideal sawtooth across the series, which is the manual's "similar to a
sawtooth wave" and is what it sounds like.

HOW TO MEASURE THIS, because getting it wrong is easy and cost several wrong answers here.
Do NOT read harmonics off a period-synchronously averaged cycle: the period comes from an
INTEGER sample lag, so it is quantised, and over hundreds of averaged cycles the accumulated
phase error smears the upper harmonics away. That looks exactly like a capture-chain lowpass
- it produced an apparent 25 dB roll-off by the sixth harmonic and a whole set of plausible,
wrong constants. Refine f0 on the harmonic sum and read each harmonic with a windowed DFT
instead (tools has the working version). Validate the method on OscB's own sawtooth and
square first: the saw must come back at -6.0/-9.5/-12.0 and the square must show no even
harmonics at all. Ours now do, to 0.2 dB.

## 4. `wave_sine2()`

-- Sine2 -------------------------------------------------------------------

The manual: "a Sine -> Double Sine signal". A two-segment phase warp is the right FORM - it
correlates 0.999+ with the capture - but an earlier model ran it the wrong way, opening the
breakpoint out and widening the positive lobe. The hardware CLOSES it: the breakpoint runs 0.495
down to 0.030, narrowing the positive lobe to a spike.

The mean of sin() over this warp is exactly 2(2d - 1)/pi. Removing it is what lifts the spike
above the trough, and the hardware's output is AC coupled, so removing it is also what the
instrument does.

REPLACED 2026-09-17 BY THE INSTRUMENT'S OWN SINE2 (reference §27.2). The positive half takes (1 - s)/2 of
the cycle (the measured d above), never under four samples - which at 329 Hz is 0.0137, the 0.013 the
hardware showed at full Shape - through a fifth-order odd polynomial close to sin(pi x/2). The engine then
multiplies by 1 + Shape and runs the instrument's DC blocker; wave_sine2() itself is the unit shape the
editor draws. The subtracted mean below was a static stand-in for that blocker, and was 6 dB quiet.

## 5. in `wave_sine2()`

MEASURED 2026-08-30: this clamp was the error, not the law. d wants 0.016 at full Shape and
the instrument measures 0.013, but the floor of 0.03 held it well short - worth 4.5 dB of
missing harmonics at Shape 127, where every other setting on the dial matches to 0.15 dB.
Unclamped the law lands within 1.0 dB, which is inside its own scatter; a refit over seven
captured points suggests 0.5 - 0.5054*s + 0.0146*s^2 if it is ever worth the last 0.8 dB.
The floor now only guards the divide.

## 6. `wave_sine3()`

-- Sine3 -------------------------------------------------------------------

The manual calls this "a Sine -> Even harmonics signal", AND THE MANUAL IS WRONG: the measured
spectrum at full Shape is 1, 0.90, 0.81, 0.73, 0.65 - a FULL harmonic series, with the 3rd and 5th
as strong as the 2nd and 4th. Nothing that adds only even harmonics can produce that.

What does produce it is the Poisson kernel: a geometric harmonic series, every harmonic present
with amplitude ratio^n. This is its closed form.

REPLACED 2026-09-17 and again 2026-09-25 (reference §27.3): the engine sounds the part's own program through
wave_sine3_instrument() - r = g x (0.98699 - 8 x inc96), the level (1 - 0.703125g), and the part's 16-step
division, whose wrap is what once looked like a ratio "held under 0.905". This function is the unit-peak
shape the editor draws, with r uncapped. The "0.90" above was that wrap read as a ratio.

## 7. `wave_sine4()`

-- Sine4 -------------------------------------------------------------------

"a Sine -> Odd harmonics signal", and this time the manual is right: measured 1, 0.02, 0.95, 0.02,
0.90 for harmonics 1..5 at full Shape - odd only. (An early reading of an ASCII plot called it
"double-humped, so the odd-harmonic model must be wrong". That was the wrong conclusion: two humps
per cycle is exactly what strong odd harmonics look like.)

The same kernel as Sine3 but in 2*theta, which is what makes the series odd-only, normalised by
its own peak so the amplitude does not run away as ratio approaches 1.

## 8. `wave_trisaw_peak()`

THE INSTRUMENT'S SYMMETRY IS RAW/128, adopted 2026-09-14 from the DSP code: the peak sits at
(1 + raw/128)/2 - 0.5 at Shape 0, 0.75 at 64, 0.996 at 127. It was 0.5 + 0.47 x raw/127, fitted to a
capture that read 0.97 at the top: that was the instrument's OTHER rule showing, not its peak.

THE FALL NEVER SHORTENS PAST TWO SAMPLES AT THE NOTE'S PITCH (the instrument clamps its symmetry to
1 - 4f/fs), so at Shape 127 the peak is 0.996 up to about 200 Hz, 0.978 at C6 and 0.956 at C7 - a
capture near C6 reads close to 0.97, which is where the old fit came from. Below that the instrument's
saw is far brighter than 0.97 made it: at C2 its fall is 0.4% of the cycle, where ours was 3%. The
drawn wave uses the unclamped peak; the engine applies the clamp in `osc_shp_wave()`. The mid-dial
gap the 2026-08-23 check left (about 2 dB, "the largest thing left") is the 0.47 against 0.496 slope.
