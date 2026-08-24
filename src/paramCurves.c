/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// What a dial's raw 0..127 value MEANS, in hertz, seconds, multiples or semitones - split out from
// renderParams.c, which draws those dials, so that this half carries no GLFW or OpenGL dependency.
//
// That split is what lets the sound engine be built into something with no user interface at all -
// a VST3 plug-in - because every other file the engine needs (soundEngine, dataBase, globalVars,
// cableChain, moduleResourcesAccess, protocol) is already free of the GUI, and this was the one
// that was not.
//
// The sharing itself is the point and predates the split: the dial text and the sound engine derive
// their numbers from these same functions, so a curve cannot be corrected in one place and left
// wrong in the other. Nothing here touches global state, so the UI thread and the audio thread's
// parameter snapshot can both call them freely.

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "paramCurves.h"

// The oscillator dial curves, kept apart from the renderers that print them so the sound engine
// can share them. See renderParams.h for why. Nothing here touches global state, so both the
// UI thread and the audio thread's parameter snapshot can call them freely.

int osc_pitch_type_param_index(tModule * module) {
    switch (module->type) {
        case moduleTypeOscB:
        case moduleTypeResonator:
        case moduleTypeOscShpB:
        case moduleTypeOscString:
        case moduleTypeOscNoise:
        case moduleTypeOscShpA:
        case moduleTypeOscDual:
        {
            return 4;
        }
        case moduleTypeOscMaster:
        case moduleTypeOscC:
        case moduleTypeOscPM:
        {
            return 3;
        }
        case moduleTypeOscPerc:
        {
            return 2;
        }
        case moduleTypeOscA:
        {
            return 6;
        }
        default:
        {
            return -1;
        }
    }
}

double osc_freq_semitones(double paramValue) {
    if (paramValue >= 127.0) {
        return 63.0;    // Clip - the dial's top step is +63, not +64
    }
    return paramValue - 64.0;
}

double osc_freq_hz(double paramValue) {
    double minFreq = 8.1758;    // A concert-pitch C-1
    double maxFreq = 12550.0;

    return exp(paramValue / 127.0 * log(maxFreq / minFreq)) * minFreq;
}

double osc_freq_factor(double paramValue) {
    double minFactor = 0.0248;
    double maxFactor = 38.072;

    return exp(paramValue / 127.0 * log(maxFactor / minFactor)) * minFactor;
}

double osc_fine_cents(double paramValue) {
    return (paramValue - 64.0) / 64.0 * 50.0;
}

// A sub-oscillator's frequency, for the Pitch Type setting named "Sub".
//
// It is the ordinary note scale ELEVEN OCTAVES DOWN: the dial value is a MIDI note number, exactly
// as it is for Semi, and the result is that note's frequency divided by 2048. That is where the
// 0.21484375 in the arithmetic comes from - it is 440/2048, the A eleven octaves below concert
// pitch - so nothing here is a new curve, only the familiar one shifted.
//
// The bottom of the dial is silence rather than a very low note, which is why 0 returns 0 rather
// than 0.0001 Hz.
//
// fineSemitones is the Cent dial's contribution, (cent - 64) / 128 of a semitone. Callers that
// cannot reach that parameter pass 0.0, which is exactly right at the dial's default of 64 and
// wrong by at most half a semitone at either extreme.
#define OSC_SUB_OCTAVES_DOWN    (11.0)

double osc_sub_freq_hz(double paramValue, double fineSemitones) {
    if (paramValue <= 0.0) {
        return 0.0;
    }
    return 440.0 * exp2(((paramValue + fineSemitones - 69.0) / 12.0) - OSC_SUB_OCTAVES_DOWN);
}

double osc_shape_percent(double paramValue) {
    return paramValue * 49.0 / 127.0 + 50.0;
}

// THE LFO'S SHAPE DIAL IS NOT THE OSCILLATORS'. OscShpA/OscShpB run 50%..99%, so their Shape only
// ever opens from a neutral wave; LfoShpA's runs the FULL 1%..99% with the neutral wave at its
// CENTRE, and skews either way from there — which is why its default is 64 rather than 0. The manual
// pins all three points for the Sine wave: "At 50% Shape, the signal is a pure sine wave. At 1%
// Shape, the signal is a down sawtooth and at 99% Shape, an up sawtooth", and the same 1%/99% ends
// are quoted for CosBell and TriBell. Measurement agrees: the recovered phase-warp breakpoint runs
// 0.02 to 0.48 and passes through the identity at the dial's centre.
double lfo_shape_percent(double paramValue) {
    return paramValue * 98.0 / 127.0 + 1.0;
}

// 13.75 Hz is A-1, so like the oscillators' Tune the filter's Freq dial is really a pitch — its
// value counts semitones up from there, reaching about 21 kHz at 127 (manual: "13.76 Hz to 21.1 kHz").
double flt_cutoff_hz(double paramValue) {
    // exp2 rather than pow(2, x): the same value, but this is called once per filter per voice
    // per oversampled sample, where a general pow() is several times the cost of the base-2 form.
    return 13.75 * exp2(paramValue / 12.0);
}

// An envelope segment's length in seconds — the 0.5 ms to 45 s scale the manual quotes for the Decay
// knobs (p.196).
//
// COMPUTED, not looked up. The scale is an exact EIGHTH POWER of the dial value shifted by a
// constant, and pinning the exponent at a whole 8 fits better than letting it float (which lands on
// 8.01) — which is what says 8 is the real number rather than an artefact. It also matches how the
// hardware would evaluate it: powers there are built by repeated squaring, so an integer exponent is
// what the machine wants, and 8 is three squarings.
//
// REPRODUCES ALL 128 PRINTED ENTRIES EXACTLY, digit for digit, once formatted the way the synth
// formats them (see render_paramType1ADRTime). That is what retired the lookup table: a curve that
// agrees with every entry is not an approximation of the table, it IS the table, and it keeps
// working between the entries.
//
// Only ONE constant is fitted. The scale is not free — it is pinned so that the top of the dial
// lands on exactly the 45 s the manual quotes, which leaves the offset as the single unknown. The
// window of offsets that reproduces all 128 entries is only 0.0018 wide (40.1664 to 40.1682), so
// there is very little room left to be wrong in; 40.167 sits in the middle of it.
//
// Computing rather than tabulating also handles the fractional dial values that morphs and the
// engine's parameter smoothing produce. A table index truncates those, so a morphed attack would
// step between whole dial positions instead of sweeping; a curve just passes through them.
//
// Supersedes two earlier fits of the same shape: offset 39.55 with exponent 7.9421, and offset
// 40.150, which reproduced 108 of the 128 printed entries — every miss being the last digit low
// by one, which is what said the constant was fractionally small rather than the shape wrong.
#define ADR_TIME_OFFSET    (40.167)
#define ADR_TIME_MAX_S     (45.0)      // The manual's stated maximum, and what raw 127 must give
#define ADR_TIME_SCALE     (ADR_TIME_MAX_S / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET) \
                            / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET)                \
                            / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET)                \
                            / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET))

double adr_time_seconds(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return ADR_TIME_SCALE * pow(value + ADR_TIME_OFFSET, 8.0);
}

// How sharply the exponential envelope segments curve. MEASURED ON THE HARDWARE 2026-08-24, and it
// takes TWO constants, not one: the rise and the fall are not equally curved.
//
//     shape    dial   attack k   decay k
//     LogExp    80      2.90       4.25
//     ExpExp    80      2.81       4.51
//     ExpExp    64      2.78       4.20
//
// Method: an EnvADSR modulating a LevMod on a steady 523 Hz OscB into 2-Out, captured off the
// instrument's own outputs and fitted. The envelope output is NOT measured directly - it is a slow
// control signal and the audio output is AC coupled, which would bend the very curve being measured -
// so what is fitted is the amplitude of a tone the envelope opens.
//
// TWO THINGS THE CONTROL SETTING PROVED, and neither was assumed:
//   - LinLin fits a straight line to rms 0.014, so the VCA and the capture chain are linear and the
//     tone's amplitude really does track the envelope. Without that, every k here would be the
//     product of the envelope and whatever the VCA does.
//   - the fitted segment length came out 3.20 s against adr_time_seconds(80)'s 3.208 s, which checks
//     the time law at the same time.
// k CAME OUT THE SAME AT TWO DIFFERENT TIME DIALS (2.78 at 1.02 s segments, 2.81 at 3.20 s), which is
// what says the shape is applied to a NORMALISED progress rather than being a fixed time constant.
// Fit the segment length freely and it trades against k - an asymptotic tail sitting in the noise
// looks equally like a longer, shallower curve - so the length is pinned to what the control measured.
//
// Both figures replace guesses: the engine used 5.0 for both and the drawn envelope used 4.0 for both.
// The fall was nearly right at 4.0; the rise was wrong in both copies, and noticeably so.
//
// Normalised at both ends below, because exp() is asymptotic: a raw exponential would neither leave
// 0 nor arrive at 1 within the segment, so the segment would not land on its own endpoints.
#define ENV_ATTACK_SHARPNESS    (2.83)
#define ENV_FALL_SHARPNESS      (4.32)

double env_attack_level(uint32_t envShape, double progress) {
    switch (envShape) {
        case eEnvShapeLogExp:
        {
            // Log - rises fast, then flattens towards the peak.
            return (1.0 - exp(-ENV_ATTACK_SHARPNESS * progress)) / (1.0 - exp(-ENV_ATTACK_SHARPNESS));
        }
        case eEnvShapeExpExp:
        {
            // Exp - starts slowly, then accelerates into the peak.
            return (exp(ENV_ATTACK_SHARPNESS * progress) - 1.0) / (exp(ENV_ATTACK_SHARPNESS) - 1.0);
        }
        default:
        {
            return progress;   // LinExp and LinLin both rise in a straight line
        }
    }
}

// The falling segments - decay and release - are exponential for every shape except LinLin, whose
// second word is the one that says so.
double env_fall_level(uint32_t envShape, double progress) {
    if (envShape == (uint32_t)eEnvShapeLinLin) {
        return 1.0 - progress;
    }
    return (exp(-ENV_FALL_SHARPNESS * progress) - exp(-ENV_FALL_SHARPNESS))
           / (1.0 - exp(-ENV_FALL_SHARPNESS));
}

// A filter's resonance, as Q - 0.5 at the bottom of the dial up to 50 at the top, the range the
// Res dial prints.
// THE DAMPING FALLS LINEARLY AND Q IS ITS INVERSE SQUARE. Write d for the damping,
//
//     d = 1 - 0.9 * value / 127        1 at the bottom of the dial, 0.1 at the top
//     Q = 0.5 / d²                     0.5 at the bottom, 50 at the top
//
// which is the resonance of a two-pole section whose damping term the dial sets directly - the
// knob moves the pole towards the unit circle in a straight line, and the Q that results is not
// linear at all. That is why neither an exponential nor a straight line between 0.5 and 50 ever
// fitted: an earlier exponential matched only at the two endpoints and was out by as much as 245%
// in between, reading Q 10.9 where the dial showed 3.16.
//
// This replaces reading a 128-entry table through atof(). The table was right - it agreed
// with this to the printed precision at 126 of its 128 entries - but an index truncates, so a
// morphed or smoothed Res swept in whole dial steps instead of gliding, and every lookup cost a
// string parse on the audio thread.
//
// The two entries that disagree are raw 17 and 33, where the table reads 0.64 and 0.84 against
// this formula's 0.65 and 0.85. Both sit far from a rounding boundary, so one of the two is
// genuinely wrong rather than differently rounded; that is a question for the hardware, and one
// step of Q at the very bottom of the knob is not worth holding up the change for.
#define FLT_RESONANCE_DAMPING_SPAN    (0.9)

// Four one-poles in a loop reach 180 degrees of phase exactly at the corner, where each has
// contributed 45, and |G|^4 is then 1/4 - so the loop sustains itself at a feedback of 4 and the
// response there is infinite. The Res dial runs linearly up to JUST SHORT of that.
//
// 3.914 rather than 4.0, and the 2% matters: it is what gives maximum resonance a peak of +24.4 dB
// instead of an infinite one. It is not a taste decision - it is solved from the measured 4-pole peak
// and then CHECKED against the other two taps, which it was not fitted to:
//
// WHY NOT THE 4.08 THE CURVE FITS RETURN: least squares over a whole response is dominated by the
// passband and the skirts and is nearly blind to the height of a peak two decades narrower than
// either. The measured peak is FINITE, which by itself requires k below 4, and 3.914 is what
// reproduces it. The two numbers are not in conflict; one of them is simply the one that can see the
// peak.
//     tap    model peak    measured        model passband    measured
//      2       +30.33       +30.64            -13.7 dB       -13.8 dB
//      3       +27.35       +27.37            -13.7 dB       -15.0 dB
//      4       +24.36       +24.36            -13.8 dB       -14.8 dB
// Six readings from one constant, the worst of them 1.3 dB out. The peak SPACING falls out of the
// topology rather than being fitted at all - 5.97 and 2.99 dB against 6.28 and 3.01 measured - which
// is the strongest evidence that the four-pole loop is right, since a loop matching the tap predicts
// no such spacing.
//
// It also says the hardware does not quite self-oscillate at the top of the dial, which the finite
// measured peak had already implied.
#define FLT_LADDER_K_MAX    (3.914)

double flt_resonance_q(double paramValue) {
    double value = paramValue;
    double damping;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    damping = 1.0 - (FLT_RESONANCE_DAMPING_SPAN * value / 127.0);
    return 0.5 / (damping * damping);
}

// FLTCLASSIC IS A LADDER, AND ITS FEEDBACK ALWAYS GOES ROUND ALL FOUR POLES. The dB switch does not
// change the loop — it only chooses which stage the output is TAPPED from, stage 2, 3 or 4. Measured
// 2026-08-24 (noise through the filter, every setting divided by the same patch bypassed) and it is
// what settles a contradiction that had held this up: the passband droop said the feedback amount was
// the same in all three slope modes, while the peak heights said it had to be normalised per mode.
// Both are true of THIS topology and of no simpler one. Fitting each slope setting separately gives
// the SAME k to within 0.011 — 4.079 / 4.087 / 4.090 at Res 127 for the three taps, all at a fitted
// corner of 1040 Hz — where a loop that matched the tap gives 1.25 / 1.62 / 2.01 and fits three to
// six times worse (rms 0.5-0.9 dB against 3-6 dB).
//
// MASK THE NOISE FLOOR BEFORE FITTING. The 24 dB tap first fitted three times worse than the other
// two (rms 2.9 dB, k 3.83) purely because its stopband falls below the G2's own noise inside the
// window, so the fit was being asked to match the INSTRUMENT'S NOISE. Dropping every point below
// -45 dB took it to rms 0.91 dB and k 4.090. The 12 dB tap never reaches the floor in that window and
// does not move at all when the same mask is applied, which is what proves the cause.
//
// IT IS ALSO THE MUSICAL CHOICE, which is presumably why it was built this way: with the loop fixed
// at four poles, self-oscillation arrives at k = 4 whatever slope is selected, so the Res dial means
// the same thing in all three modes and a patch keeps its character when the slope is changed.
//
// The dial is LINEAR in k, k = 4 x Res/127: fitted 0.000, 0.987, 2.157, 3.169, 4.079 at Res 0, 32,
// 64, 96, 127, a straight line through the origin to within +/-0.08. The top of the dial therefore
// lands exactly on the self-oscillation threshold rather than short of it or past it.
//
// NOT flt_resonance_q() ABOVE, WHICH STAYS: that one is a biquad's Q, and the numbers it produces
// still match the values the synth DISPLAYS for the Res dial. This is the feedback amount the
// response actually has. Two different questions about the same knob.
double flt_ladder_feedback(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return FLT_LADDER_K_MAX * value / 127.0;
}

// Which stage the output is tapped from: 2, 3 or 4 poles for 12, 18 or 24 dB per octave. The loop
// length is NOT this — see flt_ladder_feedback() — it is always four.
uint32_t flt_ladder_tap(uint32_t slopeValue) {
    return 2 + flt_slope_extra_poles(slopeValue);
}

// The magnitude of G^tap / (1 + k.G^4) at f/fc = ratio, where G = 1/(1 + j.ratio) is one pole.
// Written out in real arithmetic so it carries no complex.h dependency into either caller.
//
// A CONTINUOUS-TIME MODEL, FOR DRAWING. The shape is shared with the engine — same topology, same
// linear-in-Res feedback, same tap — but the CONSTANT is not, and unifying them would be a mistake:
//   - this one is the ideal analogue ladder, where four one-poles reach 180 degrees exactly at the
//     corner and sustain at k = 4, so the dial's 3.914 sits just under it;
//   - soundEngine.c's LADDER_K_MAX is 4.3, and is right to be different: its loop carries a sample of
//     delay whose phase depends on the sample rate, and its stages saturate, both of which move the k
//     at which the loop actually oscillates. Its figure is measured against the instrument too, by a
//     different method (saw harmonics rather than noise), and the two agree on everything that IS
//     shared.
// So: take the topology from here, never the number.
double flt_ladder_magnitude(double ratio, double feedback, uint32_t tap) {
    double onePoleMag   = 1.0 / sqrt(1.0 + (ratio * ratio));
    double onePolePhase = -atan(ratio);
    double loopMag      = onePoleMag * onePoleMag * onePoleMag * onePoleMag;  // |G|^4
    double loopPhase    = 4.0 * onePolePhase;
    double denomReal    = 1.0 + (feedback * loopMag * cos(loopPhase));
    double denomImag    = feedback * loopMag * sin(loopPhase);
    double denom        = sqrt((denomReal * denomReal) + (denomImag * denomImag));
    double numerator    = pow(onePoleMag, (double)tap);

    // At the self-oscillation threshold the denominator goes to zero and the magnitude to infinity.
    // A drawn curve has to stay on the page, so the floor here is what stops a peak at maximum Res
    // from becoming a vertical line; it sits far above anything the box can show.
    if (denom < 1e-4) {
        denom = 1e-4;
    }
    return numerator / denom;
}

// The dB scroll button selects how many one-pole stages sit on top of the base two: 12, 18 or 24 dB
// per octave.
uint32_t flt_slope_extra_poles(uint32_t slopeValue) {
    return (slopeValue > 2) ? 2 : slopeValue;
}

// The Kbt scroll button is Off / 25% / 50% / 75% / 100% keyboard tracking (manual p.196).
double flt_kbt_amount(uint32_t kbtValue) {
    return (kbtValue > 4) ? 1.0 : ((double)kbtValue * 0.25);
}

// An LFO's speed in Hz, for a given Range setting. Shared with the sound engine so the rate heard
// and the rate shown cannot drift apart.
//
// THE TWO FAST RANGES ARE SEMITONE SCALES: a rate of base * 2^(value/12), i.e. the dial is a pitch,
// twelve steps to a doubling, the same shape as an oscillator's Tune. That is why 127 steps spans
// 2^(127/12) = 1535: the whole dial is ten and a half octaves of rate. These were previously written
// as an exponential fitted between the two endpoints the manual quotes, which lands on the same
// curve to within a percent — the fit and the real law agree because the range IS 127 semitones —
// but stating the base and the semitone directly says what the control actually is, and removes a
// 1.8 % error at the bottom of Rate Hi where the fitted endpoint was rounded to 0.26.
//
// ClkSync needs the patch's master clock, which the engine has no notion of, so it falls back to the
// slow end of Rate Lo rather than pretending to be in time with something.
#define LFO_SEMITONE_RATIO    (1.0 / 12.0)

double lfo_rate_hz(uint32_t rangeMode, double paramValue) {
    switch (rangeMode) {
        case 0:   // Rate Sub: a period of 699 s down to 5.46 s
        {
            // LINEAR IN FREQUENCY, not a semitone scale like the two below — the period is simply
            // 699.05 s divided by (value + 1), which is why the top of the dial lands on 699/128 =
            // 5.46 s, exactly the figure the manual quotes (p.148). Reading the two endpoints and
            // assuming the usual exponential sweep between them was out by up to 90% through the
            // middle: at raw 25 it gave a 269 s period where the hardware's divider gives 26.9 s.
            return (paramValue + 1.0) / 699.0506666667;
        }
        case 1:   // Rate Lo: 0.0159 Hz (62.9 s/cycle) to 24.4 Hz
        {
            return 0.0159 * exp2(paramValue * LFO_SEMITONE_RATIO);
        }
        case 2:   // Rate Hi: 0.2555 Hz to 392 Hz
        {
            return 0.2555 * exp2(paramValue * LFO_SEMITONE_RATIO);
        }
        case 3:   // BPM: three straight runs, 24..214, always a whole number of beats
        {
            // Confirmed unchanged against the instrument's own three-branch arithmetic, including
            // the two joins: the branches meet at 88 and at 152 whichever side of the boundary they
            // are taken from, so the one-step difference in where this splits changes no value.
            double bpm = (paramValue < 33.0) ? (24.0 + round(2.0 * paramValue))
                         : (paramValue < 97.0) ? (56.0 + round(paramValue))
                         : (154.0 + round(2.0 * (paramValue - 97.0)));

            return bpm / 60.0;
        }
        default:
        {
            return 1.0;
        }
    }
}

// Which parameter carries a delay's Time/Clk selector. It differs per module — DelayB puts it at 4,
// DelayA at 5, DlyStereo at 6, DelayQuad at 8 where it governs all four of its Time dials — and
// getting this wrong is the mistake that left DelayA permanently bypassed in the sound engine.
// Returns -1 for a module that has no such selector.
int delay_time_clk_param_index(tModuleType moduleType) {
    switch (moduleType) {
        case moduleTypeDelayB:
        {
            return 4;
        }
        case moduleTypeDelayA:
        {
            return 5;
        }
        case moduleTypeDlyStereo:
        {
            return 6;
        }
        case moduleTypeDelayQuad:
        {
            return 8;
        }
        default:
        {
            return -1;
        }
    }
}

// The maximum a delay's Time dial reaches, for a given Range setting. THERE ARE THREE DIFFERENT
// RANGE TABLES and the modules do not share one:
//
//   delayRangeStrMap     7 entries  5ms/25ms/100ms/500ms/1.0s/2.0s/2.7s  DlySingleA/B, DelayDual,
//                                                                        DelayQuad, DlyEight
//   delayABRangeStrMap   4 entries  500ms/1.0s/2.0s/2.7s                 DelayA, DelayB
//   dlyStereoRangeStrMap 3 entries  500ms/1.0s/1.35s                     DlyStereo
//
// This was one switch using the 7-entry maxima for all of them, so a DelayB set to its second Range
// showed 25 ms where the synth showed 1 s — the tables agree on neither length nor order.
double delay_range_max_seconds(tModuleType moduleType, uint32_t rangeValue) {
    static const double sevenWay[]  = {0.005, 0.025, 0.100, 0.500, 1.0, 2.0, 2.7};
    static const double abWay[]     = {0.500, 1.0, 2.0, 2.7};
    static const double stereoWay[] = {0.500, 1.0, 1.35};
    const double *      table       = sevenWay;
    uint32_t            count       = sizeof(sevenWay) / sizeof(sevenWay[0]);

    switch (moduleType) {
        case moduleTypeDelayA:
        case moduleTypeDelayB:
        {
            table = abWay;
            count = sizeof(abWay) / sizeof(abWay[0]);
            break;
        }
        case moduleTypeDlyStereo:
        {
            table = stereoWay;
            count = sizeof(stereoWay) / sizeof(stereoWay[0]);
            break;
        }
        default:
        {
            break;
        }
    }
    return table[(rangeValue < count) ? rangeValue : (count - 1)];
}

// A delay's Time dial (in Time mode, not Clk) as seconds.
//
// LINEAR, and counted in SAMPLES at the G2's 96 kHz engine rate rather than in seconds:
//
//     time = ((raw * step) + 1) / 96000
//
// The +1 is why raw 0 is a delay of one sample - 0.01 ms - rather than none at all, and every step
// is a whole number of samples, so the scale cannot be reproduced by interpolating between two
// times in seconds. The Range selector chooses `step`, and 127 of them land a hair OVER the time it
// advertises: Range 1.0s reaches 1.000135 s, which is exactly why the top of that dial is the one
// value on it reading in seconds rather than milliseconds.
//
// Hardware-confirmed on a DelayB at Range 1.0s, where step works out at 756 - raw 0, 1, 2, 3, 4,
// 13, 24, 126, 127 read 0.01m, 7.89m, 15.8m, 23.6m, 31.5m, 102m, 189m, 992m and 1.000s, and all
// nine agree. Only that one Range is confirmed; the rest derive their step the same way. The plain
// lerp between DELAY_TIME_MIN and the range maximum that this replaced was close but not equal - it
// gave 7.87m where the synth says 7.89m.
//
// Shared with the sound engine so the delay that is heard cannot drift from the one displayed.
double delay_time_seconds(double maxSeconds, double paramValue) {
    double step = round((maxSeconds * G2_ENGINE_SAMPLE_RATE) / 127.0);

    return ((paramValue * step) + 1.0) / G2_ENGINE_SAMPLE_RATE;
}

// Raw dial value to an index into clkSyncStrMap, for a delay in Clk mode.
//
// The value's top five bits select the division, so every slot is exactly 4 raw values wide and the
// dial has 32 of them. A delay only offers 22 of the divisions clkSyncStrMap can name — the manual
// gives its Clk range as 1/64T to 2/1 (p.182), against the LFO's full 64:1 to 1:64T (p.148), which
// makes sense of a module whose Range caps at 2.7s: 2/1 is already past that at any sane tempo. The
// ten divisions through the middle of the delay's range simply occupy two slots each. THAT is why
// the dial's buckets are uneven — 4 raw values per division at each end of the travel, 8 through
// the middle — and why no single scale factor reproduces it.
//
// Hardware-confirmed on a DelayB by stepping Time one raw unit at a time and recording where the
// synth's own display changed. Sweeping up and down agreed exactly (the down readings sit one
// value lower throughout, which is just the boundary being read from the other side), and the
// change points came out at 4, 8, 12, 16, 20, 24, 28, then every 8 to 108, then every 4 to 124.
//
// This was previously a proportional stretch of 22 divisions across 0-127, which agreed with the
// hardware at the two endpoints and nowhere else — 118 of the 128 raw values named the wrong
// division. clk_sync_beats() reads the same index, so the sound engine was mistimed to match.
//
// The four modules that offer Clk at all (DelayA, DelayB, DelayQuad, DlyStereo — see
// delay_time_clk_param_index(), and the manual names exactly those four) share one formatter in the
// reference, so they are assumed to share this table too; only DelayB is confirmed. The LFO's
// ClkSync is a DIFFERENT table and is not handled here.
static const uint8_t kClkSyncSlot[32] = {
    31, 30, 29, 28, 27, 26, 25, 24, 24, 23, 23, 22, 22, 21, 21, 20,
    20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 13, 12, 11, 10
};

uint32_t clk_sync_index(double paramValue) {
    int value = (int)paramValue;

    if (value < 0) {
        value = 0;
    } else if (value > 127) {
        value = 127;
    }
    return kClkSyncSlot[value >> 2];
}

// That division as a multiple of one beat. "1/4" IS the beat, so it is 1.0; D is dotted (x1.5) and
// T is a triplet (x2/3). Same entries clkSyncStrMap prints, so heard and shown cannot diverge.
double clk_sync_beats(double paramValue) {
    static const double beats[32] = {
        256.0,     192.0,     128.0,  96.0,      64.0,       48.0,      32.0,      24.0,
        16.0,       12.0,       8.0,   6.0,       4.0,        3.0, 8.0 / 3.0,       2.0,
        1.5,   4.0 / 3.0,       1.0,  0.75, 2.0 / 3.0,        0.5,     0.375, 1.0 / 3.0,
        0.25,     0.1875, 1.0 / 6.0, 0.125,   0.09375, 1.0 / 12.0,    0.0625, 1.0 / 24.0
    };

    return beats[clk_sync_index(paramValue)];
}

// The curves below were read off the hardware directly - the dial set to raw 0, 64 and 127 in turn
// and the synth's own panel display recorded - so each is anchored at three points rather than
// inferred. Where the top step is pinned to a round number that is noted, because the dials are not
// consistent about it and it is invisible from the middle of the scale.

// PShift's Semi: a QUARTER of a semitone per dial step, so the whole dial spans -16.0 to +15.75
// rather than the ±64 its name suggests. Read -16.0 / +0.0 / +15.8 at raw 0 / 64 / 127, and that
// last one says the top step is NOT rounded up to +16.
#define PSHIFT_SEMI_STEPS_PER_SEMITONE    (4.0)

double pshift_semitones(double paramValue) {
    return (paramValue - 64.0) / PSHIFT_SEMI_STEPS_PER_SEMITONE;
}

// Scratch's Ratio: playback speed as a signed multiple, -4.00 through 0 to +4.00, sixteen dial steps
// to each whole multiple. Zero is a standstill and the negative half plays backwards, which is what
// the control is for. Read -x4.00 / x0 / x4.00, and the top step IS pinned - the curve alone would
// give 3.94 at raw 127.
#define SCRATCH_STEPS_PER_MULTIPLE    (16.0)
#define SCRATCH_MAX_MULTIPLE          (4.0)

double scratch_ratio(double paramValue) {
    if (paramValue >= 127.0) {
        return SCRATCH_MAX_MULTIPLE;
    }
    return (paramValue - 64.0) / SCRATCH_STEPS_PER_MULTIPLE;
}

// The Digitizer's Sample Rate, in hertz. A PITCH SCALE, not a linear rate: twelve dial steps to a
// doubling, from 32.70 Hz - which is C1 - up to 50.2 kHz. Read 32.70 Hz / 1.32 kHz / 50.2 kHz at
// raw 0 / 64 / 127, all three matching to the printed precision.
double digitizer_rate_hz(double paramValue) {
    return 1760.0 * exp2((paramValue - 69.0) / 12.0);
}

// PitchTrack's Threshold, in decibels: a plain amplitude ratio against full scale, 20*log10(raw/127).
// Silence at the bottom of the dial and 0 dB at the top. Read as "- Infinity" / -6.0 dB / -0 dB.
//
// NOT the cubic-blended curve the mixer levels use - that one gives -17.6 dB at the middle of the
// dial where this reads -6.0, so the two are nothing like each other despite both being decibels.
double pitchtrack_threshold_db(double paramValue) {
    if (paramValue <= 0.0) {
        return -1.0 / 0.0;    // Silence - the panel shows this step as "- Infinity"
    }
    return 20.0 * log10(paramValue / 127.0);
}

// A flanger's sweep rate in hertz. COUNTED IN A 24-BIT FRACTION, not in hertz: the step is
// 384000/2^24, an exact binary fraction, so the whole scale is a straight line of 2^-24 units and
// the top of the dial lands on 2.91 Hz. The bottom step is HALF a step rather than nothing, which
// is why raw 0 reads 0.01 Hz and not 0.00.
#define FLANGER_RATE_STEP    (384000.0 / 16777216.0)

double flanger_rate_hz(double paramValue) {
    if (paramValue <= 0.0) {
        return FLANGER_RATE_STEP / 2.0;
    }
    return paramValue * FLANGER_RATE_STEP;
}

// A phaser's sweep rate in hertz. SQUARE IN THE DIAL VALUE, and counted in the same 24-bit fraction
// as the flanger: half of raw² whole steps of 24000, offset by 768000 so the bottom of the dial sits
// at 0.05 Hz rather than at nothing. The square is why the top of the dial moves so much faster per
// step than the bottom - 0.05 Hz to 11.6 Hz, with most of that in the last quarter of the travel.
//
// The halving truncates, so this must floor rather than round: at odd dial values the two differ by
// a whole 24000-unit step.
#define PHASER_RATE_STEP      (24000.0 / 16777216.0)
#define PHASER_RATE_OFFSET    (768000.0 / 16777216.0)

double phaser_rate_hz(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return (floor((value * value) / 2.0) * PHASER_RATE_STEP) + PHASER_RATE_OFFSET;
}

// A mixer level in decibels, for the channels whose Curve is set to dB.
//
// A CUBIC BLENDED WITH A LINE, not a plain logarithm: with x the dial as a fraction of full scale,
// the amplitude is x³ with 1% of a straight line mixed in, and the reading is 20·log10 of that. The
// 1% is what keeps the very bottom of the dial from diving to minus infinity as fast as a pure cube
// would, and it is why the scale cannot be written as so many dB per step.
//
// The three lowest steps are named rather than computed - silence, then two values the curve itself
// would place slightly differently - and naming them is the renderer's job, so this returns a plain
// -infinity at the bottom and leaves the wording alone. Reproduces every other reading of the
// printed scale exactly.
#define MIX_LEVEL_CUBIC_MIX    (0.99)

double mix_level_db(double paramValue) {
    double x = paramValue / 127.0;

    if (x <= 0.0) {
        return -1.0 / 0.0;    // Silence - the dial's bottom step, shown as "-oo"
    }
    return 20.0 * log10((x * (1.0 - MIX_LEVEL_CUBIC_MIX)) + (x * x * x * MIX_LEVEL_CUBIC_MIX));
}

// The patch's master volume in decibels: -78 dB at the bottom of the dial up to 0 at the top.
//
// The dial is exponential in the ATTENUATION rather than in the gain - (16/3) raised to how far
// DOWN the dial you are, scaled to 18 and shifted so the top reads exactly 0 - which is why the
// numbers crowd together at the quiet end and spread out near unity. Reproduces all 128 readings
// of the printed scale exactly.
#define PATCH_VOLUME_BASE    (16.0 / 3.0)
#define PATCH_VOLUME_SPAN    (18.0)

double patch_volume_db(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return -((pow(PATCH_VOLUME_BASE, (127.0 - value) / 127.0) * PATCH_VOLUME_SPAN) - PATCH_VOLUME_SPAN);
}

// LevAmp's amplification, as a multiplier. The manual (p.227) gives the range as "0.25 to 4.0 times
// the input level"; the dial walks that range exponentially, passing through unity at 64. Shared
// with the sound engine so what is heard and what the dial reads cannot drift apart.
//
// FOUR OCTAVES OVER 128 STEPS, which is why this is exp2(v / 32) and not a fitted exponential: the
// scale is 0.25 * 16^(v/128), and 16^(v/128) is 2^(4v/128), i.e. 2^(v/32). Thirty-two dial steps to
// a doubling. Reading it that way says what the control is — the same "the dial is really a pitch"
// shape the oscillators and the two fast LFO ranges have — rather than leaving a decimal constant
// that has to be taken on trust.
//
// This replaces exp(v * 0.0218), an exponential fitted between the endpoints. The true exponent is
// log(16) / 128 = 0.02166085, so the fit ran 1.77% high by the top of the dial — 3.898 where the
// scale gives 3.830 at raw 126.
//
// The curve reaches 4.0 at 128, one step past the end of a 0..127 dial, so the top step is pinned
// to 4.0 rather than the 3.966 the formula alone would give. That matches the manual's stated
// maximum, and it is the same top-step correction the Freq dial needs (see osc_freq_semitones).
#define LEV_AMP_STEPS_PER_OCTAVE    (32.0)

double lev_amp_gain(double paramValue) {
    if (paramValue >= 127.0) {
        return 4.0;    // Clip - the dial's top step is 4.0, which the curve only reaches at 128
    }
    return exp2(paramValue / LEV_AMP_STEPS_PER_OCTAVE) * 0.25;
}

#ifdef __cplusplus
}
#endif
