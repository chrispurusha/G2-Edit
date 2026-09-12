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
// Notes: Docs/code-notes/paramCurves.c.md - "// notes §k" refers there.

// notes §1

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

// notes §2
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

// notes §3
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

// notes §4
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

// notes §5
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

// notes §6
#define FLT_RESONANCE_DAMPING_SPAN    (0.9)

// notes §7
#define FLT_LADDER_K_MAX              (4.0)

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

// notes §8
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

// notes §9
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

// notes §10

// notes §11
uint32_t flt_cascade_poles(uint32_t slopeMode) {
    return (slopeMode > 5u) ? 6u : (slopeMode + 1u);
}

double flt_cascade_magnitude(double ratio, uint32_t poles, bool highPass) {
    double   onePole = 1.0 / sqrt(1.0 + (ratio * ratio));
    double   stage   = highPass ? (ratio * onePole) : onePole; // |jw/(1+jw)| against |1/(1+jw)|
    double   out     = 1.0;
    uint32_t i       = 0;

    for (i = 0; i < poles; i++) {
        out *= stage;
    }

    return out;
}

// notes §12
double flt_static_q(double paramValue) {
    double value   = paramValue;
    double damping = 0.0;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 126.0) {
        value = 126.0;      // one step short of zero damping, so the curve stays finite
    }
    damping = 1.0 - (value / 127.0);
    return 0.5 / (damping * damping);
}

// A two-pole section at f/fc = ratio, as low-pass, band-pass or high-pass. Written in real
// arithmetic for the same reason flt_ladder_magnitude() is - no complex.h in either caller.
double flt_biquad_magnitude(double ratio, double q, tFilterShape shape) {
    double r2    = ratio * ratio;
    double real  = 1.0 - r2;
    double imag  = ratio / q;
    double denom = sqrt((real * real) + (imag * imag));
    double num   = 1.0;

    switch (shape) {
        case eFilterShapeHighPass:
        {
            num = r2;
            break;
        }
        case eFilterShapeBandPass:
        {
            num = ratio / q;
            break;
        }
        case eFilterShapeBandReject:
        {
            num = fabs(1.0 - r2);
            break;
        }
        default:
        {
            num = 1.0;      // low-pass
            break;
        }
    }
    return num / denom;
}

// notes §13
double flt_nord_gc_gain(double resParam) {
    double v  = resParam / 127.0;
    double dB = (-10.897 * v) + (13.140 * v * v) - (26.503 * v * v * v);

    return pow(10.0, dB / 20.0);
}

// notes §14
uint32_t flt_nord_tap(uint32_t dbOctValue) {
    return (dbOctValue == 0u) ? 2u : 4u;    // 12dB taps two poles, 24dB taps four
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

// notes §15
#define LFO_SEMITONE_RATIO    (1.0 / 12.0)
#define LFO_HI_BASE_HZ        (0.2555)      // Rate Hi at dial 0; Rate Lo is this over 16

double lfo_rate_hz(uint32_t rangeMode, double paramValue) {
    switch (rangeMode) {
        case 0:   // Rate Sub: a period of 699 s down to 5.46 s
        {
            // notes §16
            return (paramValue + 1.0) / 699.0506666667;
        }
        case 1:   // Rate Lo: 0.01597 Hz (62.6 s/cycle) to 24.5 Hz
        {
            // notes §17
            return (LFO_HI_BASE_HZ / 16.0) * exp2(paramValue * LFO_SEMITONE_RATIO);
        }
        case 2:   // Rate Hi: 0.2555 Hz to 392 Hz
        {
            // MEASURED EXACT: 0.25553 Hz at dial 0 and 4.08794 Hz at 48, against 0.2555 and 4.0880
            // predicted - 0.01% and 0.001%. This constant needed no correction.
            return LFO_HI_BASE_HZ * exp2(paramValue * LFO_SEMITONE_RATIO);
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

// notes §18
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

// notes §19
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

// notes §20
double delay_time_seconds(double maxSeconds, double paramValue) {
    double step = round((maxSeconds * G2_ENGINE_SAMPLE_RATE) / 127.0);

    return ((paramValue * step) + 1.0) / G2_ENGINE_SAMPLE_RATE;
}

// notes §21
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

// notes §22

// PShift's Semi: a QUARTER of a semitone per dial step, so the whole dial spans -16.0 to +15.75
// rather than the ±64 its name suggests. Read -16.0 / +0.0 / +15.8 at raw 0 / 64 / 127, and that
// last one says the top step is NOT rounded up to +16.
#define PSHIFT_SEMI_STEPS_PER_SEMITONE    (4.0)

double pshift_semitones(double paramValue) {
    return (paramValue - 64.0) / PSHIFT_SEMI_STEPS_PER_SEMITONE;
}

// notes §23
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

// notes §24
double pitchtrack_threshold_db(double paramValue) {
    if (paramValue <= 0.0) {
        return -1.0 / 0.0;    // Silence - the panel shows this step as "- Infinity"
    }
    return 20.0 * log10(paramValue / 127.0);
}

// notes §25
#define FLANGER_RATE_STEP    (384000.0 / 16777216.0)

double flanger_rate_hz(double paramValue) {
    if (paramValue <= 0.0) {
        return FLANGER_RATE_STEP / 2.0;
    }
    return paramValue * FLANGER_RATE_STEP;
}

// notes §26
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

// notes §27
#define MIX_LEVEL_CUBIC_MIX    (0.99)

double mix_level_db(double paramValue) {
    double gain = mix_level_gain(paramValue);

    if (gain <= 0.0) {
        return -1.0 / 0.0;    // Silence - the dial's bottom step, shown as "-oo"
    }
    return 20.0 * log10(gain);
}

// The same curve as an amplitude (sound engine reference §3.2).
double mix_level_gain(double paramValue) {
    double x = paramValue / 127.0;

    if (x <= 0.0) {
        return 0.0;
    }

    if (x > 1.0) {
        x = 1.0;
    }
    return (x * (1.0 - MIX_LEVEL_CUBIC_MIX)) + (x * x * x * MIX_LEVEL_CUBIC_MIX);
}

// notes §28
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

// notes §29
#define LEV_AMP_LINEAR_TOP    (24.0)       // dial position where the linear bottom segment ends
#define LEV_AMP_UNITY         (64.0)       // and where the multiplier passes through 1.0

double lev_amp_gain(double paramValue) {
    double value = paramValue;

    if (value <= 0.0) {
        return 0.0;    // Fully closed - measured 64 dB down, which is the noise floor, not a level
    }

    if (value >= 127.0) {
        return 4.0;
    }

    if (value <= LEV_AMP_LINEAR_TOP) {
        return value / 96.0;
    }

    if (value <= LEV_AMP_UNITY) {
        return 0.25 * exp2((value - LEV_AMP_LINEAR_TOP) / 20.0);
    }

    if (value <= 96.0) {
        return exp2((value - LEV_AMP_UNITY) / 32.0);
    }
    return 2.0 * exp2((value - 96.0) / 31.0);
}

#ifdef __cplusplus
}
#endif
