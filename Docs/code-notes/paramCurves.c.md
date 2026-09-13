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

## 5. `env_attack_level()`, `env_fall_level()`

The shapes of the envelope segments as the instrument makes them (sound engine reference §17). The
constants are in paramCurves.h because the engine's recurrences use them too:

```
    ENV_RISE_SHARPNESS   ln 16  = 2.7726   the Log and Exp attacks cover a factor of 16 (24 dB)
    ENV_FALL_SHARPNESS   ln 100 = 4.6052   decay and release fall 40 dB in the dial's time
    ENV_LOG_RISE_TARGET  16/15             the Log attack is a one-pole aimed here, reaching 1 on time
```

With those, the normalised forms below ARE the instrument's attacks from zero: (1 - e^-kp)/(1 - e^-k)
is a one-pole aimed at 16/15 that arrives at 1 at p = 1, and (e^kp - 1)/(e^k - 1) grows sixteen-fold
over the segment. The FALL is a pure exponential on the instrument - it never reaches zero on time,
it is 40 dB down - so the normalised fall here is for DRAWING only, closing the last 1% so a drawn
segment lands on its end point. The engine does not use these any more; it runs the stages as
recurrences (soundEngine.c `envelope_step()`).

THE 2026-08-24 CAPTURE AGREES, read correctly. It fitted k = 2.78-2.90 for the rise and 4.20-4.51
for the fall to the amplitude of a tone an EnvADSR opened (LogExp and ExpExp at dials 64 and 80),
with the segment length pinned to the time law. The rise is ln 16 within the fit's spread. The fall
reads low because the fit used the NORMALISED form, which forces the curve to zero at the dial time;
fitted to a pure exponential that is 1% at that point, it has to be shallower to follow it. That
capture also confirmed LinLin is straight and the time law itself (3.20 s against 3.208 s at 80).

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
so Q = 0.5/d^2 with d = 1 - v/128, the instrument's own damping (adopted 2026-09-13; was v/127). That
lands the top of the dial on self-oscillation and is within about 25% of these peaks through the middle, which is as much as a 30-pixel curve can show.
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

## 30. `CLIP_PARAM_LEVEL_MOD`

SHAPER GROUP - Clip, Overdrive, Saturate, ShpExp, WaveWrap, ShpStatic and Rect (manual p.204-207).
Moved here from `soundEngine.c` on 2026-09-13 (its notes §8, §106-§112) so that the graph each
module draws and the engine that plays it evaluate ONE function: when these laws are measured and
refitted, the picture follows the sound without a second edit.

Every one of these is MEMORYLESS: the output depends only on the present input sample, through
what the manual calls a transfer function and draws as a graph. That is why they arrive as one
node kind carrying a mode rather than as seven, and why they cost nothing to run at audio rate -
which is exactly what the G2 means by a control module promoted to audio. Full scale is +-1.0
here, which is the +-64 units the manual quotes for the instrument's headroom.

THE ORDERS ARE NOT UNIFORM AND ARE NOT GUESSES WORTH REPEATING FROM MEMORY. WaveWrap lists its
modulation depth BEFORE its amount and its Mod jack BEFORE its In jack; Overdrive and Clip list
the mod dial first but the In jack first; Saturate and ShpExp list the amount first. Every one of
these came from the layout tables in moduleResources.h, and none is confirmed against the
instrument yet.

HOW MUCH OF THIS IS KNOWN. Rect is EXACT: the manual states all four operations in words, and
there is no dial to get wrong. ShpStatic's four labels - Inv x3, Inv x2, x2, x3 - name their own
curves (but see §31). Everything else here is structurally right and numerically a guess: the
manual describes the family (a logarithmic curve for Saturate, an exponential one for ShpExp,
four named overdrive characters, a fold rather than a clip for WaveWrap) but names no constant
anywhere.

THESE ARE THE CHEAPEST MEASUREMENTS LEFT. A memoryless module gives up its ENTIRE transfer
function to one capture: send a slow full-scale ramp - or simply a low sine, which sweeps every
input level twice per cycle - through it and plot output against input. One capture per mode,
no impulse, no windowing, no decay fitting. See to-test.md.

## 31. in `shaper_transfer()`

SHPSTATIC'S FOUR CURVES, the instrument's (2026-09-13), on s = |x| over full scale with the sign
carried through: Inv x3 is 1 - (1 - s)^3, Inv x2 is 1 - (1 - s)^2, x2 is s^2 and x3 is s^3. "Inv" is
the curve turned over - a fast rise that flattens into full scale - NOT a root, which is what this
played until then (s^(1/3) and s^(1/2)). The two Inv curves hold at full scale beyond it; x2 and x3
carry on up to the headroom (notes §46).

The 2026-08-24 capture had fitted power laws to these and read 0.49 and 0.65 for the Inv pair: a power
law cannot follow 1 - (1 - s)^n, and the fit landed wherever the test signal's level put it.

## 32. in `shaper_transfer()`

SHPEXP CROSSFADES THE DRY SIGNAL AGAINST A POWER CURVE (2026-09-13): y = (1 - a)s + a s^n with the
sign carried through, n = 2, 3, 4, 5 for shpExpCurveStrMap's x2..x5, a the Amount (plus its mod
input) over 128. Until then Amount bent the EXPONENT from 1 towards n instead - the same end points,
a different path between them. Nothing clamps at full scale; a hot input rises as s^n to the headroom.

## 33. in `shaper_transfer()`

SATURATE (2026-09-13): y = (1 - a)s + a(1 - (1 - s)^n) up to full scale, sign carried through, with
n = 3, 5, 7, 9 for Curve 1-4 - an odd-order curve that meets full scale flat, blended against the dry
signal by the Amount. Beyond full scale it carries on straight at slope (1 - a), so at full Amount it
holds and at none it passes. Until then this was a normalised log curve with an unmeasured k range.

## 34. in `shaper_transfer()`

Amplify, then fold. Up to 19 dB of drive, which is four folds on a full-scale input -
the "deep distortion and FM-like characteristics" of the manual.

THE MAXIMUM DRIVE IS ODD ON PURPOSE. shaper_fold() returns exactly zero at every EVEN
integer, so an even maximum - 16 was the first thing written here - sends full scale
to silence at the top of the dial, and a full-scale input then vanishes exactly where
the module should be at its most extreme. Nine folds full scale back to full scale.

## 35. in `shaper_transfer()`

Drive into a soft limiter whose KNEE is what the four type names select:
y = x / (1 + |x|^n)^(1/n) reaches +-1 asymptotically, gently for a small n and
almost squarely for a large one. odTypeStrMap is {Soft, Hard, Fat, Heavy}, so Fat
takes the most drive and Hard the sharpest knee.

AMOUNT BOTH DRIVES AND MIXES, and the mix is what makes zero mean zero. The limiter
bends the curve at every drive setting, unity included - x/(1+x^2)^(1/2) is already
3 dB down at full scale with no drive at all - so a dial that only fed the drive
would leave the module audibly distorting with its depth control shut. Crossfading
the shaped signal against the dry one by the same dial is the only construction here
that reaches genuine transparency at 0 and full character at 127. Which of the two
the instrument actually does is UNMEASURED; that it is transparent at 0 is not in
doubt, since the module has no separate bypass reading of its own dial.

## 36. in `shaper_transfer()`

CLIP'S LEVEL LOWERS THE THRESHOLD IN A STRAIGHT LINE (2026-09-13): t = (128 - Level)/128 of full scale,
so Level 64 clips at half scale and 127 at 1/128 - it never reaches zero. The mod input lowers it
further, by its attenuator (over 128) times the input, and it stops at zero. Sym clips both halves,
Asym only the positive one (asymSymStrMap: 0 is Asym). Until then the threshold fell 36 dB
exponentially across the dial, so the middle of the dial clipped four times harder than it should.

## 46. `SHAPER_HEADROOM`

The instrument carries a signal of four times full scale before its fixed point saturates - full scale
is 64 units of a 256-unit range - so a shaper sees a hot input as it is and can pass up to that much.
This clamped to full scale until 2026-09-13, which flattened every overdriven input to the same thing.

## 37. `kEqLowShelfHz`

EqPeak, Eq2Band and Eq3band's laws - every constant here is §11 of the engine reference, where the
measurements behind them are. Moved from `soundEngine.c` on 2026-09-13 for the same reason as the
shapers (§30): the response each module draws comes from the same bands the engine plays.

`eq_bands_build()` takes a `tParamReader` so there is ONE list of which parameter is which: the
engine passes its morph-following reader and the graph the raw dial, and both read the shelf
selectors raw, as a drop-down carries no morph.

## 38. `eq_magnitude()`

The ANALOGUE PROTOTYPES of the engine's three sections, multiplied: each section adds (G - 1) times
its filter to what passes through it, so each is 1 + (G - 1).F and the three are in series. Written
in real arithmetic, as flt_ladder_magnitude() is, since neither caller has complex.h.

WHY NOT THE ENGINE'S DISCRETE FORMS: those depend on the engine's sample rate, which a graph drawn
on the face has no business knowing, and they match these prototypes closely below a few kHz. The
peak is a topology-preserving SVF, exact at its centre by construction; the one-pole shelves drift
from their prototypes only in the top octave, where a box a few dozen pixels wide cannot show it.

THE INPUT LEVEL IS LEFT OUT. It is a broadband gain with its own dial and readout; folded in, a
Level of 64 (-17.6 dB) would sink the whole curve out of a +-20 dB box and hide the shape the graph
is there to show.

## 39. `FLTCOMB_TUNING_SEMITONES`

FltComb's laws, moved from `soundEngine.c` on 2026-09-13 so the graph on the module draws what the
engine plays. Every constant is §13 of the engine reference, where the measurements are.

`flt_comb_delay_samples()` keeps the engine's expression exactly, sample rate and all, so the move
changed nothing the engine computes; the graph calls it at FLTCOMB_REFERENCE_RATE, the rate the
extra-delay figures were measured at. `flt_comb_magnitude()` is the section's own transfer function,
k (1 + b.g.z^-D) / (1 - c.g.z^-D) - frequency-flat, so it cannot show the four-point read's high-end
loss (§13.4), which a box a few dozen pixels wide would not resolve anyway.

## 40. `FLTPHASE_PARAM_FREQ`

A MODEL OF FltPhase FOR THE GRAPH. The engine does not play this module, and the captures behind
what follows were not kept - only the figures in findings.md ("FltPhase - A PHASER", 2026-08-29).

WHAT WAS MEASURED, all at Freq 75:
    first notch for Notch settings 0..5    2098, 1254, 1020, 750, 656, 539 Hz
    notch depth at FB 96 / 112 / 127       -7, -13, -28 dB; FB 64 exactly flat
    Notch against Deep                     about 5 dB of notch depth; Peak inverts the sense

THE MODEL, and what each piece rests on:
  - N IDENTICAL SECOND-ORDER ALLPASS SECTIONS, N the Notch setting plus one - the manual's "six
    allpass filters which displace the phase 180 degrees each". Their chain A has unit gain and a
    phase of -2N.atan(r/Q / (1 - r^2)); a notch falls where A = -1. With the centre at
    flt_cutoff_hz(Freq + 12) - 2093 Hz here - and Q 1.04 this lands all six recorded first notches
    at 0.27 dB rms, the worst 6% out. ONE free parameter for six points is what makes it credible.
  - FB IS A MIX, g = (FB - 64)/64, not a feedback: Notch is 1 + g.A. Its notch is 1 - g deep, which
    is -6.0 and -12.0 dB at FB 96 and 112 against -7 and -13 measured; -36 dB at 127 against -28 is
    the capture's floor. Its peaks reach 1 + g, +6 dB at full FB - findings.md reports "up to +8 dB
    of its own gain", close but not checked.
  - PEAK IS 1 / (1 - g.A), which puts peaks where Notch put notches - "inverts the sense". DEEP IS
    (1 + g.A) / (1 - g.A), notches and peaks both, deeper than Notch by 20.log(1 + g): 4.9 dB at FB
    112, the "about 5 dB" measured. The structure is a guess; that both figures fall out of it is
    the evidence for it.

ASSUMED, NOT MEASURED:
  - that the centre follows Freq a semitone per step - true of every other G2 filter dial, but seen
    here at one Freq only;
  - SPREAD ENTIRELY. The capture was at the default, 64, which is where Q 1.04 belongs. Letting
    Spread move Q - lower Q spreads the notches apart - by an octave per 32 steps is a placeholder
    so that the dial does something plausible, with nothing behind the number.
  - Peak and Deep's exact forms beyond the two figures above.
What would settle it is in todo.md: a Freq sweep, a Spread sweep, and each Type at three FB values.

## 41. `operator_ratio()`, `operator_fixed_hz()`

Operator is a DX7 operator and "all the parameters and controls behave like on the DX7" (G2 manual
p.184), so its frequency follows the DX7's laws rather than a G2 frequency dial's:
- RATIO: Coarse 0 is 0.50 and 1-31 are themselves; Fine 0-99 adds that many hundredths of it
  (x1.00 to x1.99 at Coarse 1).
- FIXED: Coarse picks the decade - 1, 10, 100, 1000 Hz, repeating every four steps - and Fine
  multiplies it by 10^(Fine/100), so each decade is covered in 100 logarithmic steps.
Values past the DX ranges (Coarse 31, Fine 99) are clamped. The Coarse dial shows the result, the way
the DX7 shows one frequency for the two controls. UNCONFIRMED ON THE G2: these are the DX7's laws,
which the manual says the module copies; the G2's own reading has not been compared.

## 42. `compress_ratio()`, `compress_out_db()`

Compress's three level dials, as the instrument reads them (measured 2026-08-10 from its own dial
displays; the manual agrees on the ranges): Thr and RefLvl are raw - 30 dB, and Thr's top position
(raw 42) reads "Off"; Ratio runs in three straight stretches that repeat a decade higher above raw
34, 1.0:1 to about 95:1 - transcribed from the instrument's formatter, not fitted. compress_ratio()
was the engine's own compressor_ratio() until 2026-09-13, moved here unchanged so the dial text and
the graph read the same law the engine plays.

THE STATIC CURVE is the engine's gain law with the detector settled on a steady input: the module is
a LEVELLER (measured 2026-09-07) - out = in + (1 - 1/ratio) x (target - max(in, Thr)), with target
the higher of RefLvl and Thr. Above the threshold that pulls the level towards the target by the
ratio; below it the same makeup is applied as at the threshold. That below-threshold part is the
engine's choice, NOT yet measured on the instrument (sound-engine-reference, Compress).
compress_ratio_raw() is the inverse for a graph handle: the dial position whose ratio is nearest,
by log distance.

The meter table (compress_meter_lit(), compress_meter_reduction_db()) takes GAIN REDUCTION - dB over
Thr x (1 - 1/ratio) - and lights whole LEDs rounded down: established on the G2 2026-09-13
(sound-engine-notes §122). Its inverse returns the least reduction that lights a given count.
The table is per-LED thresholds, not points to interpolate between (refined the same day - see
§122 for the window each threshold was fitted inside); compress_meter_reduction_db() returns the
threshold of the highest lit LED.

## 43. `kDxAlgorithms`, `dx_algorithm()`

The 32 DX7 algorithms as who-modulates-whom: one target bitmask per operator and the feedback loop's
two ends, from the published DX7 algorithm chart. Moved here from moduleGraphics.c on 2026-09-13 so the
DXRouter graph (moduleGraphics.c notes §85) and the sound engine (sound-engine-reference §14) read the
same table - the engine cannot include the graph code.

## 44. `constant_level()`

A Constant's output as the engine carries it, 1.0 being 64 units. Bipolar is (value - 64) units and
Unipolar value / 2, with 127 reading exactly 64 in both - the top step is the one exception to either
straight line, as on every G2 level dial. The same law the dial displays (renderParams.c
`render_paramType1BipLevel()`). LevAdd's offset follows it too, when that module is added
(Docs/mini-emulator-engine-plan.md). Sound engine reference §16.1.

## 45. `vibrato_rate_hz()`

The patch Vibrato's rate, as the instrument makes it: a phase increment of 255 + 256 x dial/127 on a
16-bit phase, stepped at the instrument's pitch tick of 96000/94 Hz - the tick patch glide steps at
too, which is why glide's time per octave sits 2% off its own display. That is 3.97 Hz at dial 0,
5.98 Hz at 64 and 7.96 Hz at 127. Shared by the engine and the Patch Settings dial so the two agree.
It replaced "4 + dial/127 x 4 Hz", which was within 0.7% everywhere (engine law revert record).

The depth needed no change: the dial reads cents, 1 cent a step, full at full controller - which is
the instrument's to within 1%.
