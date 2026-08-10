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

double osc_shape_percent(double paramValue) {
    return paramValue * 49.0 / 127.0 + 50.0;
}

// 13.75 Hz is A-1, so like the oscillators' Tune the filter's Freq dial is really a pitch — its
// value counts semitones up from there, reaching about 21 kHz at 127 (manual: "13.76 Hz to 21.1 kHz").
double flt_cutoff_hz(double paramValue) {
    // exp2 rather than pow(2, x): the same value, but this is called once per filter per voice
    // per oversampled sample, where a general pow() is several times the cost of the base-2 form.
    return 13.75 * exp2(paramValue / 12.0);
}

// An envelope segment's length in seconds — the 0.5 ms to 45 s scale the manual quotes for the Decay
// knobs (p.196) and that ADRTimeStrMap prints.
//
// COMPUTED, not looked up. The scale is an exact EIGHTH POWER of the dial value shifted by a
// constant, and pinning the exponent at a whole 8 fits better than letting it float (which lands on
// 8.01) — which is what says 8 is the real number rather than an artefact. It also matches how the
// hardware would evaluate it: powers there are built by repeated squaring, so an integer exponent is
// what the machine wants, and 8 is three squarings.
//
// Reproduces every entry of the printed scale that carries three significant figures to within
// 0.33%, median 0.078% — i.e. to the limit of what the printed table can confirm. The wider gap at
// the very short end (5% at raw 2) is against entries printed to ONE significant figure: "0.7ms"
// admits anything from 0.65 to 0.75, and this lands at 0.736 well inside that.
//
// Computing rather than tabulating also handles the fractional dial values that morphs and the
// engine's parameter smoothing produce. A table index truncates those, so a morphed attack would
// step between whole dial positions instead of sweeping; a curve just passes through them.
//
// Supersedes an earlier fit of the same shape (offset 39.55, exponent 7.9421) which was twice as
// far out — 0.72% worst, 0.36% median.
#define ADR_TIME_SCALE     (7.385186e-17)
#define ADR_TIME_OFFSET    (40.150)

double adr_time_seconds(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return ADR_TIME_SCALE * pow(value + ADR_TIME_OFFSET, 8.0);
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
// This replaces reading filter_resonanceStrMap through atof(). The table was right - it agrees
// with this to the printed precision at 126 of its 128 entries - but an index truncates, so a
// morphed or smoothed Res swept in whole dial steps instead of gliding, and every lookup cost a
// string parse on the audio thread.
//
// The two entries that disagree are raw 17 and 33, where the table reads 0.64 and 0.84 against
// this formula's 0.65 and 0.85. Both sit far from a rounding boundary, so one of the two is
// genuinely wrong rather than differently rounded; that is a question for the hardware, and one
// step of Q at the very bottom of the knob is not worth holding up the change for.
#define FLT_RESONANCE_DAMPING_SPAN    (0.9)

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
