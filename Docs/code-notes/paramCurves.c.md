# paramCurves.c notes

The longer comments from `paramCurves.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

What a dial's raw 0..127 value MEANS, in hertz, seconds, multiples or semitones - split out from
renderParams.c, which draws those dials, so that this half carries no GLFW or OpenGL dependency.

That split is what lets the sound engine be built into something with no user interface at all -
a VST3 plug-in - because every other file the engine needs (soundEngine, dataBase, globalVars,
cableChain, moduleResourcesAccess, protocol) is already free of the GUI, and this was the one
that was not.

The sharing itself is the point and predates the split: the dial text and the sound engine derive
their numbers from these same functions, so a curve cannot be corrected in one place and left
wrong in the other. Nothing here touches global state, so the UI thread and the audio thread's
parameter snapshot can both call them freely.

## 2. `OSC_SUB_OCTAVES_DOWN`

A sub-oscillator's frequency, for the Pitch Type setting named "Sub".

It is the ordinary note scale ELEVEN OCTAVES DOWN: the dial value is a MIDI note number, exactly
as it is for Semi, and the result is that note's frequency divided by 2048. That is where the
0.21484375 in the arithmetic comes from - it is 440/2048, the A eleven octaves below concert
pitch - so nothing here is a new curve, only the familiar one shifted.

The bottom of the dial is silence rather than a very low note, which is why 0 returns 0 rather
than 0.0001 Hz.

fineSemitones is the Cent dial's contribution, (cent - 64) / 128 of a semitone. Callers that
cannot reach that parameter pass 0.0, which is exactly right at the dial's default of 64 and
wrong by at most half a semitone at either extreme.

## 3. `lfo_shape_percent()`

THE LFO'S SHAPE DIAL IS NOT THE OSCILLATORS'. OscShpA/OscShpB run 50%..99%, so their Shape only
ever opens from a neutral wave; LfoShpA's runs the FULL 1%..99% with the neutral wave at its
CENTRE, and skews either way from there — which is why its default is 64 rather than 0. The manual
pins all three points for the Sine wave: "At 50% Shape, the signal is a pure sine wave. At 1%
Shape, the signal is a down sawtooth and at 99% Shape, an up sawtooth", and the same 1%/99% ends
are quoted for CosBell and TriBell. Measurement agrees: the recovered phase-warp breakpoint runs
0.02 to 0.48 and passes through the identity at the dial's centre.

## 4. `ADR_TIME_OFFSET`

An envelope segment's length in seconds — the 0.5 ms to 45 s scale the manual quotes for the Decay
knobs (p.196).

COMPUTED, not looked up. The scale is an exact EIGHTH POWER of the dial value shifted by a
constant, and pinning the exponent at a whole 8 fits better than letting it float (which lands on
8.01) — which is what says 8 is the real number rather than an artefact. It also matches how the
hardware would evaluate it: powers there are built by repeated squaring, so an integer exponent is
what the machine wants, and 8 is three squarings.

REPRODUCES ALL 128 PRINTED ENTRIES EXACTLY, digit for digit, once formatted the way the synth
formats them (see render_paramType1ADRTime). That is what retired the lookup table: a curve that
agrees with every entry is not an approximation of the table, it IS the table, and it keeps
working between the entries.

Only ONE constant is fitted. The scale is not free — it is pinned so that the top of the dial
lands on exactly the 45 s the manual quotes, which leaves the offset as the single unknown. The
window of offsets that reproduces all 128 entries is only 0.0018 wide (40.1664 to 40.1682), so
there is very little room left to be wrong in; 40.167 sits in the middle of it.

Computing rather than tabulating also handles the fractional dial values that morphs and the
engine's parameter smoothing produce. A table index truncates those, so a morphed attack would
step between whole dial positions instead of sweeping; a curve just passes through them.

Supersedes two earlier fits of the same shape: offset 39.55 with exponent 7.9421, and offset
40.150, which reproduced 108 of the 128 printed entries — every miss being the last digit low
by one, which is what said the constant was fractionally small rather than the shape wrong.

## 5. `ENV_ATTACK_SHARPNESS`

How sharply the exponential envelope segments curve. MEASURED ON THE HARDWARE 2026-08-24, and it
takes TWO constants, not one: the rise and the fall are not equally curved.

```
    shape    dial   attack k   decay k
    LogExp    80      2.90       4.25
    ExpExp    80      2.81       4.51
    ExpExp    64      2.78       4.20

```
Method: an EnvADSR modulating a LevMod on a steady 523 Hz OscB into 2-Out, captured off the
instrument's own outputs and fitted. The envelope output is NOT measured directly - it is a slow
control signal and the audio output is AC coupled, which would bend the very curve being measured -
so what is fitted is the amplitude of a tone the envelope opens.

TWO THINGS THE CONTROL SETTING PROVED, and neither was assumed:
```
  - LinLin fits a straight line to rms 0.014, so the VCA and the capture chain are linear and the
    tone's amplitude really does track the envelope. Without that, every k here would be the
    product of the envelope and whatever the VCA does.
  - the fitted segment length came out 3.20 s against adr_time_seconds(80)'s 3.208 s, which checks
    the time law at the same time.
```
k CAME OUT THE SAME AT TWO DIFFERENT TIME DIALS (2.78 at 1.02 s segments, 2.81 at 3.20 s), which is
what says the shape is applied to a NORMALISED progress rather than being a fixed time constant.
Fit the segment length freely and it trades against k - an asymptotic tail sitting in the noise
looks equally like a longer, shallower curve - so the length is pinned to what the control measured.

Both figures replace guesses: the engine used 5.0 for both and the drawn envelope used 4.0 for both.
The fall was nearly right at 4.0; the rise was wrong in both copies, and noticeably so.

Normalised at both ends below, because exp() is asymptotic: a raw exponential would neither leave
0 nor arrive at 1 within the segment, so the segment would not land on its own endpoints.

## 6. `FLT_RESONANCE_DAMPING_SPAN`

A filter's resonance, as Q - 0.5 at the bottom of the dial up to 50 at the top, the range the
Res dial prints.
THE DAMPING FALLS LINEARLY AND Q IS ITS INVERSE SQUARE. Write d for the damping,

```
    d = 1 - 0.9 * value / 127        1 at the bottom of the dial, 0.1 at the top
    Q = 0.5 / d²                     0.5 at the bottom, 50 at the top

```
which is the resonance of a two-pole section whose damping term the dial sets directly - the
knob moves the pole towards the unit circle in a straight line, and the Q that results is not
linear at all. That is why neither an exponential nor a straight line between 0.5 and 50 ever
fitted: an earlier exponential matched only at the two endpoints and was out by as much as 245%
in between, reading Q 10.9 where the dial showed 3.16.

This replaces reading a 128-entry table through atof(). The table was right - it agreed
with this to the printed precision at 126 of its 128 entries - but an index truncates, so a
morphed or smoothed Res swept in whole dial steps instead of gliding, and every lookup cost a
string parse on the audio thread.

The two entries that disagree are raw 17 and 33, where the table reads 0.64 and 0.84 against
this formula's 0.65 and 0.85. Both sit far from a rounding boundary, so one of the two is
genuinely wrong rather than differently rounded; that is a question for the hardware, and one
step of Q at the very bottom of the knob is not worth holding up the change for.

## 7. `FLT_LADDER_K_MAX`

Four one-poles in a loop reach 180 degrees of phase exactly at the corner, where each has
contributed 45, and |G|^4 is then 1/4 - so the loop sustains itself at a feedback of 4 and the
response there is infinite. The Res dial runs linearly up to JUST SHORT of that.

4.0 EXACTLY, AND THE DIAL REACHES SELF-OSCILLATION AT ITS TOP. Four one-poles in a loop reach
180 degrees of phase at the corner where |G|^4 is 1/4, so the loop sustains itself at k = 4.

THIS WAS 3.914 UNTIL 2026-08-30, solved from a measured maximum-resonance peak of +24.4 dB on the
reasoning that a FINITE peak requires k below 4. THE PEAK WAS NOT THE FILTER. CT spotted the
output sitting in the clipping red, and re-measuring showed the +24.4 dB was the LIMITED
AMPLITUDE OF AN OSCILLATION, not a linear response - the loop was already at unity gain and
something downstream was setting the level.

TWO INDEPENDENT MEASUREMENTS NOW AGREE, and neither can see a peak at all:
```
  - IT SUSTAINS. At Res 127 with the input cable DELETED, the module holds a 1054.7 Hz tone at
    -45.7 dBFS indefinitely - unchanged over three successive captures. It does not SELF-START
    from silence (never excited, it reads the -101.6 dBFS noise floor), which is precisely what
    unity loop gain looks like: marginally stable, sustaining whatever starts it. Stepping the
    dial up from 121 to 127 with no input therefore finds nothing, and stepping down from an
    excited 127 finds a tone at every setting - the same filter, two answers, and only the
    excitation history separates them.
  - THE PASSBAND DROOP GIVES k WITHOUT GOING NEAR THE PEAK. DC gain is 1/(1+k), so k falls out of
    the low-frequency shelf, which is 60 dB below the resonance and cannot be driven into
    limiting. Measured with the rig's own noise floor subtracted, input level chosen per setting
    to keep the output below -20 dBFS, and each point checked against a quieter drive:
        Res            0     32     64     96    110    120
        droop dB   -0.31  -5.87  -9.81 -12.39 -12.95 -13.59
        k           0.036  0.967  2.094  3.165  3.442  3.778
        4 * v/127   0.000  1.008  2.016  3.024  3.465  3.780
    Least squares through the origin gives k = 4.045 * v/127. The dial is linear in feedback and
    lands on the self-oscillation point at the top, which is the musically obvious design and
    what the sustained tone independently confirms.

```
WHY THE OLD READING SURVIVED SO LONG: the six numbers it was checked against (three peaks, three
passbands) were all taken through the same limiting, so they agreed with each other. Only the
droop, measured on its own, could tell them apart.

flt_ladder_magnitude() already floors its denominator, and the renderer clamps to the box, so
k = 4 draws a curve that reaches the top of the graph at maximum Res rather than dividing by zero.
That is the honest picture of a filter that oscillates.

## 8. `flt_ladder_feedback()`

FLTCLASSIC IS A LADDER, AND ITS FEEDBACK ALWAYS GOES ROUND ALL FOUR POLES. The dB switch does not
change the loop — it only chooses which stage the output is TAPPED from, stage 2, 3 or 4. Measured
2026-08-24 (noise through the filter, every setting divided by the same patch bypassed) and it is
what settles a contradiction that had held this up: the passband droop said the feedback amount was
the same in all three slope modes, while the peak heights said it had to be normalised per mode.
Both are true of THIS topology and of no simpler one. Fitting each slope setting separately gives
the SAME k to within 0.011 — 4.079 / 4.087 / 4.090 at Res 127 for the three taps, all at a fitted
corner of 1040 Hz — where a loop that matched the tap gives 1.25 / 1.62 / 2.01 and fits three to
six times worse (rms 0.5-0.9 dB against 3-6 dB).

MASK THE NOISE FLOOR BEFORE FITTING. The 24 dB tap first fitted three times worse than the other
two (rms 2.9 dB, k 3.83) purely because its stopband falls below the G2's own noise inside the
window, so the fit was being asked to match the INSTRUMENT'S NOISE. Dropping every point below
-45 dB took it to rms 0.91 dB and k 4.090. The 12 dB tap never reaches the floor in that window and
does not move at all when the same mask is applied, which is what proves the cause.

IT IS ALSO THE MUSICAL CHOICE, which is presumably why it was built this way: with the loop fixed
at four poles, self-oscillation arrives at k = 4 whatever slope is selected, so the Res dial means
the same thing in all three modes and a patch keeps its character when the slope is changed.

The dial is LINEAR in k, k = 4 x Res/127: fitted 0.000, 0.987, 2.157, 3.169, 4.079 at Res 0, 32,
64, 96, 127, a straight line through the origin to within +/-0.08. The top of the dial therefore
lands exactly on the self-oscillation threshold rather than short of it or past it.

NOT flt_resonance_q() ABOVE, WHICH STAYS: that one is a biquad's Q, and the numbers it produces
still match the values the synth DISPLAYS for the Res dial. This is the feedback amount the
response actually has. Two different questions about the same knob.

## 9. `flt_ladder_magnitude()`

The magnitude of G^tap / (1 + k.G^4) at f/fc = ratio, where G = 1/(1 + j.ratio) is one pole.
Written out in real arithmetic so it carries no complex.h dependency into either caller.

A CONTINUOUS-TIME MODEL, FOR DRAWING. The shape is shared with the engine — same topology, same
linear-in-Res feedback, same tap — but the CONSTANT is not, and unifying them would be a mistake:
```
  - this one is the ideal analogue ladder, where four one-poles reach 180 degrees exactly at the
    corner and sustain at k = 4, which is exactly where the dial's top now lands (measured
    2026-08-30: the hardware sustains an oscillation at Res 127);
  - soundEngine.c's LADDER_K_MAX is 4.3, and is right to be different: its loop carries a sample of
    delay whose phase depends on the sample rate, and its stages saturate, both of which move the k
    at which the loop actually oscillates. Its figure is measured against the instrument too, by a
    different method (saw harmonics rather than noise), and the two agree on everything that IS
    shared.
```
So: take the topology from here, never the number.

## 10. `flt_cascade_poles()`

── The filters that are NOT ladders ─────────────────────────────────────────
Three topologies cover the seven filter modules, and they are genuinely different - anything that
draws them from one model is wrong for four of the six. All measured 2026-08-29/30 by putting
noise through the module and dividing by the same patch bypassed. See findings.md.

## 11. `flt_cascade_poles()`

FltLP and FltHP: N IDENTICAL ONE-POLES AT A COMMON CORNER, and the slope mode IS the pole count.
Fitting N and fc freely returned N = 1,2,3,4,5,6 for the six slope names with fc within 4% of the
dial's nominal frequency every time, at a residual of 0.4 to 0.6 dB - the measurement's own noise.

THE DIAL IS THE PER-POLE CORNER, NOT THE COMPOSITE -3 dB POINT, which is the thing a naive drawing
gets wrong: the composite point falls to fc * sqrt(2^(1/N) - 1) as poles are added, so 36db is 3 dB
down near 366 Hz where the dial reads 1047.

## 12. `flt_static_q()`

FltStatic: A PLAIN RESONANT BIQUAD, and the only one of the seven that is. 12 dB/octave measured
(11.6 over three octaves), and THE PASSBAND DOES NOT MOVE WITH RESONANCE - +0.15, +0.56, +0.19 dB
at Res 0, 64 and 127 - which is exactly what separates it from FltClassic and FltNord, whose
passbands drop away as the feedback rises.

ITS Q IS NOT THE Q THE DIAL PRINTS, and both numbers are right. flt_resonance_q() reproduces what
the G2 shows on its own panel and must keep doing so; what a RESPONSE CURVE needs is the resonance
the filter actually has, which is far higher. Measured peak gain (~= Q once Q >= 2), at levels
chosen per setting to keep the filter out of saturation and checked against a quieter drive:
```
    Res      0     32     64     80     96    110    120
    peak  +2.2   +5.4  +10.0  +14.3  +21.1  +31.0  +43.4 dB
    Q      ...    1.9    3.2    5.2   11.4   35.5  148
```
Damping falls linearly to zero at the top of the dial, the same shape FltClassic's feedback has,
so Q = 0.5/d^2 with d = 1 - v/127. That lands the top of the dial on self-oscillation and is
within about 25% through the middle, which is as much as a 30-pixel curve can show.
DO NOT read a Q off a spectrum without checking the resolution: at 2048 points the Res 127 peak
reads +17 dB and at 8192 it reads +36, because a Q=50 peak at 1 kHz is narrower than one bin.

## 13. `flt_nord_gc_gain()`

FltNord's GC (Gain Control), parameter 3, DEFAULT ON. MEASURED 2026-08-30 against the instrument,
eight Res settings with GC on and off through the same path (Noise -> LevAmp -> FltNord -> 2-Out,
LP at 24 dB, input backed off 12 dB so every point is verified linear).

WHAT GC IS: a broadband attenuation that grows with resonance. It does NOT change the filter's
shape - the peak measured above the passband is the same either way (27.9 dB with GC on against
28.2 dB with it off, at Res 110). What it does is pull the whole signal down as the resonance
rises, so the peak grows about 12 dB across the dial instead of 29 dB. With GC off the module
audibly distorts at high Res and self-oscillates at the top; with it on it stays linear.

```
    Res      0     16     32     48     64     80     96    110
    dB    0.00  -0.88  -2.28  -3.81  -5.84  -8.33 -11.61 -17.09

```
The cubic below fits those to +/-0.6 dB. Measured on the 24 dB low-pass; whether GC follows the
same law on the other slopes and types is not yet established - see Docs/findings.md, where the
12 dB band-pass is recorded as self-oscillating at Res 127 even with GC on, which this does not
model.

## 14. `flt_nord_tap()`

FltNord: FLTCLASSIC'S TOPOLOGY, and the passband droop is what proves it. Low-frequency gain
against Res, LP at 24 dB/oct: -0.4, -2.6, -5.8, -11.5, -38.2 dB at 0/32/64/96/127. That is
feedback around a cascade, DC gain 1/(1+k); a biquad's passband would not move, and FltStatic's
does not. THE DROOP IS THE SAME IN BOTH SLOPE MODES (-11.61 dB at 12 dB/oct against -11.49 at 24,
both at Res 96), which is the FltClassic signature exactly: one four-pole loop, the dB switch
moving only the tap.

It resonates a great deal harder than FltClassic - -38 dB of droop implies 1+k near 80 - but that
figure was captured before the clipping was found and is NOT yet trustworthy above Res 96. Until
it is re-measured this uses FltClassic's own feedback law, which is measured and safe, and the
difference will show up as FltNord drawing less resonant than it sounds at the top of its dial.

## 15. `LFO_SEMITONE_RATIO`

An LFO's speed in Hz, for a given Range setting. Shared with the sound engine so the rate heard
and the rate shown cannot drift apart.

THE TWO FAST RANGES ARE SEMITONE SCALES: a rate of base * 2^(value/12), i.e. the dial is a pitch,
twelve steps to a doubling, the same shape as an oscillator's Tune. That is why 127 steps spans
2^(127/12) = 1535: the whole dial is ten and a half octaves of rate. These were previously written
as an exponential fitted between the two endpoints the manual quotes, which lands on the same
curve to within a percent — the fit and the real law agree because the range IS 127 semitones —
but stating the base and the semitone directly says what the control actually is, and removes a
1.8 % error at the bottom of Rate Hi where the fitted endpoint was rounded to 0.26.

ClkSync needs the patch's master clock, which the engine has no notion of, so it falls back to the
slow end of Rate Lo rather than pretending to be in time with something.

## 16. in `lfo_rate_hz()`

LINEAR IN FREQUENCY, not a semitone scale like the two below — the period is simply
699.05 s divided by (value + 1), which is why the top of the dial lands on 699/128 =
5.46 s, exactly the figure the manual quotes (p.148). Reading the two endpoints and
assuming the usual exponential sweep between them was out by up to 90% through the
middle: at raw 25 it gave a 269 s period where the hardware's divider gives 26.9 s.

## 17. in `lfo_rate_hz()`

RATE LO IS RATE HI FOUR OCTAVES DOWN, measured 2026-09-07, not an independent constant.
The base was 0.0159, which is the manual's rounded figure and 0.43% low; three settings
imply 0.0159666, 0.0159668 and 0.0159685 against LFO_HI_BASE_HZ / 16 = 0.01596875.
The clincher is that Lo 96 and Hi 48 measure the IDENTICAL period, 0.24462 s - exactly
what a 48-semitone offset predicts, so the two ranges are one scale with an octave
offset rather than two constants that happen to be near a factor of sixteen.

## 18. `delay_time_clk_param_index()`

Which parameter carries a delay's Time/Clk selector. It differs per module — DelayB puts it at 4,
DelayA at 5, DlyStereo at 6, DelayQuad at 8 where it governs all four of its Time dials — and
getting this wrong is the mistake that left DelayA permanently bypassed in the sound engine.
Returns -1 for a module that has no such selector.

## 19. `delay_range_max_seconds()`

The maximum a delay's Time dial reaches, for a given Range setting. THERE ARE THREE DIFFERENT
RANGE TABLES and the modules do not share one:

```
  delayRangeStrMap     7 entries  5ms/25ms/100ms/500ms/1.0s/2.0s/2.7s  DlySingleA/B, DelayDual,
                                                                       DelayQuad, DlyEight
  delayABRangeStrMap   4 entries  500ms/1.0s/2.0s/2.7s                 DelayA, DelayB
  dlyStereoRangeStrMap 3 entries  500ms/1.0s/1.35s                     DlyStereo

```
This was one switch using the 7-entry maxima for all of them, so a DelayB set to its second Range
showed 25 ms where the synth showed 1 s — the tables agree on neither length nor order.

## 20. `delay_time_seconds()`

A delay's Time dial (in Time mode, not Clk) as seconds.

LINEAR, and counted in SAMPLES at the G2's 96 kHz engine rate rather than in seconds:

```
    time = ((raw * step) + 1) / 96000

```
The +1 is why raw 0 is a delay of one sample - 0.01 ms - rather than none at all, and every step
is a whole number of samples, so the scale cannot be reproduced by interpolating between two
times in seconds. The Range selector chooses `step`, and 127 of them land a hair OVER the time it
advertises: Range 1.0s reaches 1.000135 s, which is exactly why the top of that dial is the one
value on it reading in seconds rather than milliseconds.

Hardware-confirmed on a DelayB at Range 1.0s, where step works out at 756 - raw 0, 1, 2, 3, 4,
13, 24, 126, 127 read 0.01m, 7.89m, 15.8m, 23.6m, 31.5m, 102m, 189m, 992m and 1.000s, and all
nine agree. Only that one Range is confirmed; the rest derive their step the same way. The plain
lerp between DELAY_TIME_MIN and the range maximum that this replaced was close but not equal - it
gave 7.87m where the synth says 7.89m.

Shared with the sound engine so the delay that is heard cannot drift from the one displayed.

## 21. `kClkSyncSlot`

Raw dial value to an index into clkSyncStrMap, for a delay in Clk mode.

The value's top five bits select the division, so every slot is exactly 4 raw values wide and the
dial has 32 of them. A delay only offers 22 of the divisions clkSyncStrMap can name — the manual
gives its Clk range as 1/64T to 2/1 (p.182), against the LFO's full 64:1 to 1:64T (p.148), which
makes sense of a module whose Range caps at 2.7s: 2/1 is already past that at any sane tempo. The
ten divisions through the middle of the delay's range simply occupy two slots each. THAT is why
the dial's buckets are uneven — 4 raw values per division at each end of the travel, 8 through
the middle — and why no single scale factor reproduces it.

Hardware-confirmed on a DelayB by stepping Time one raw unit at a time and recording where the
synth's own display changed. Sweeping up and down agreed exactly (the down readings sit one
value lower throughout, which is just the boundary being read from the other side), and the
change points came out at 4, 8, 12, 16, 20, 24, 28, then every 8 to 108, then every 4 to 124.

This was previously a proportional stretch of 22 divisions across 0-127, which agreed with the
hardware at the two endpoints and nowhere else — 118 of the 128 raw values named the wrong
division. clk_sync_beats() reads the same index, so the sound engine was mistimed to match.

The four modules that offer Clk at all (DelayA, DelayB, DelayQuad, DlyStereo — see
delay_time_clk_param_index(), and the manual names exactly those four) share one formatter in the
reference, so they are assumed to share this table too; only DelayB is confirmed. The LFO's
ClkSync is a DIFFERENT table and is not handled here.

## 22. `PSHIFT_SEMI_STEPS_PER_SEMITONE`

The curves below were read off the hardware directly - the dial set to raw 0, 64 and 127 in turn
and the synth's own panel display recorded - so each is anchored at three points rather than
inferred. Where the top step is pinned to a round number that is noted, because the dials are not
consistent about it and it is invisible from the middle of the scale.

## 23. `SCRATCH_STEPS_PER_MULTIPLE`

Scratch's Ratio: playback speed as a signed multiple, -4.00 through 0 to +4.00, sixteen dial steps
to each whole multiple. Zero is a standstill and the negative half plays backwards, which is what
the control is for. Read -x4.00 / x0 / x4.00, and the top step IS pinned - the curve alone would
give 3.94 at raw 127.

## 24. `pitchtrack_threshold_db()`

PitchTrack's Threshold, in decibels: a plain amplitude ratio against full scale, 20*log10(raw/127).
Silence at the bottom of the dial and 0 dB at the top. Read as "- Infinity" / -6.0 dB / -0 dB.

NOT the cubic-blended curve the mixer levels use - that one gives -17.6 dB at the middle of the
dial where this reads -6.0, so the two are nothing like each other despite both being decibels.

## 25. `FLANGER_RATE_STEP`

A flanger's sweep rate in hertz. COUNTED IN A 24-BIT FRACTION, not in hertz: the step is
384000/2^24, an exact binary fraction, so the whole scale is a straight line of 2^-24 units and
the top of the dial lands on 2.91 Hz. The bottom step is HALF a step rather than nothing, which
is why raw 0 reads 0.01 Hz and not 0.00.

## 26. `PHASER_RATE_STEP`

A phaser's sweep rate in hertz. SQUARE IN THE DIAL VALUE, and counted in the same 24-bit fraction
as the flanger: half of raw² whole steps of 24000, offset by 768000 so the bottom of the dial sits
at 0.05 Hz rather than at nothing. The square is why the top of the dial moves so much faster per
step than the bottom - 0.05 Hz to 11.6 Hz, with most of that in the last quarter of the travel.

The halving truncates, so this must floor rather than round: at odd dial values the two differ by
a whole 24000-unit step.

## 27. `MIX_LEVEL_CUBIC_MIX`

A mixer level in decibels, for the channels whose Curve is set to dB.

A CUBIC BLENDED WITH A LINE, not a plain logarithm: with x the dial as a fraction of full scale,
the amplitude is x³ with 1% of a straight line mixed in, and the reading is 20·log10 of that. The
1% is what keeps the very bottom of the dial from diving to minus infinity as fast as a pure cube
would, and it is why the scale cannot be written as so many dB per step.

The three lowest steps are named rather than computed - silence, then two values the curve itself
would place slightly differently - and naming them is the renderer's job, so this returns a plain
-infinity at the bottom and leaves the wording alone. Reproduces every other reading of the
printed scale exactly.

## 28. `PATCH_VOLUME_BASE`

The patch's master volume in decibels: -78 dB at the bottom of the dial up to 0 at the top.

The dial is exponential in the ATTENUATION rather than in the gain - (16/3) raised to how far
DOWN the dial you are, scaled to 18 and shifted so the top reads exactly 0 - which is why the
numbers crowd together at the quiet end and spread out near unity. Reproduces all 128 readings
of the printed scale exactly.

## 29. `LEV_AMP_LINEAR_TOP`

LevAmp's amplification, as a multiplier. MEASURED ON THE HARDWARE 2026-08-30, and the manual's
"0.25 to 4.0 times the input level" (p.227) describes only the TOP THREE QUARTERS of the dial.
Shared with the sound engine so what is heard and what the dial reads cannot drift apart.

THE BOTTOM OF THE DIAL IS LINEAR AND REACHES SILENCE. The previous reading of this control -
one exponential, 0.25 * 2^(v/32) across the whole range - put 0.25x at dial 0, where the module
is in fact SILENT, and 0.5x at dial 32 where it really gives 0.33x. Anything below 64 was wrong,
by as much as 6 dB, in the dial text and in the engine alike.

FOUR SEGMENTS, and every corner of them is an exact round number:
```
    dial   0 -  24    gain = v / 96          linear, 0 to 0.25x
    dial  24 -  64    0.25 * 2^((v-24)/20)   0.25x to 1x, twenty steps to a doubling
    dial  64 -  96    2^((v-64)/32)          1x to 2x, thirty-two steps
    dial  96 - 127    2 * 2^((v-96)/31)      2x to 4x, thirty-one steps
```
The 32-then-31 split across unity is not rounding on our side: 64->96 measured exactly 2.000x and
96->127 exactly 4.000x, so the instrument spends one fewer step on its top octave.

METHOD, and it matters because the bottom of the dial is 60 dB down: a SINE from OscA rather than
the noise source every other measurement here used, so the level could be read from one FFT bin
and stayed clear of the noise floor to the bottom of the dial. Measured at 33 dial positions,
every one within 0.3% of the four segments above.

ITS Type SELECTOR DOES NOT CHANGE THE GAIN. Lin and dB were swept separately and agree to four
decimal places at every one of the 33 positions, so this function is right to ignore the
parameter. Whatever Type does, it is not this.
