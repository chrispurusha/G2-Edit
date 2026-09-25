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
// Notes: Docs/code-notes/waveModels.c.md - "// notes §k" refers there.

// notes §1

#include <math.h>
#include <stdint.h>

#include "waveModels.h"

#define SINE2_POLY_1        (1.5704)
#define SINE2_POLY_3        (-0.6419)
#define SINE2_POLY_5        (0.0716)

#define DSF_RATIO_SCALE     (8279556.0 / 8388608.0)  // §27.3 - the part's Y[0]
#define DSF_RATIO_PITCH     (8.0)
#define DSF_LEVEL_SLOPE     (0x5a / 128.0)           // §27.3 - #$5a, a fraction in the MSBs
#define DSF_DIVIDE_STEPS    (16)
#define DSP_WORD            (8388608.0)

// The instrument's Shape word: dial/128, with 127 counting as full. `shape` is dial/127 here.
double wave_shape_word(double shape) {
    return (shape >= 1.0) ? 1.0 : ((shape * 127.0) / 128.0);
}

// notes §2
double wave_sine1_limited(double phase, double shape, double shortestRise) {
    // notes §3 - the rising half takes (1 - Shape)/2 of the cycle, never less than shortestRise
    double rise = 0.5 * (1.0 - wave_shape_word(shape));

    rise = (rise < shortestRise) ? shortestRise : rise;
    rise = (rise > 0.5) ? 0.5 : rise;

    double at   = fmod(phase + (0.5 * rise), 1.0);    // 0 where the rise begins
    double theta;

    if (at < 0.0) {
        at += 1.0;
    }

    if (at < rise) {
        theta = -M_PI_2 + (M_PI * (at / rise));
    } else {
        theta = M_PI_2 + (M_PI * ((at - rise) / (1.0 - rise)));
    }
    return sin(theta);
}

double wave_sine1(double phase, double shape) {
    return wave_sine1_limited(phase, shape, 0.0);
}

// A fifth-order odd polynomial, the instrument's own, close to sin(pi x / 2) - its sines are this of a triangle
double wave_sine_polynomial(double x) {
    return x * (SINE2_POLY_1 + (x * x * (SINE2_POLY_3 + (x * x * SINE2_POLY_5))));
}

// notes §4 - the instrument's Sine2 (reference §27.2), before its gain and its DC blocker
double wave_sine2_limited(double phase, double shape, double shortestLobe) {
    // Shape takes the positive half-sine down to (1 - s)/2 of the cycle and gives the negative half
    // the rest; the positive lobe never narrows past shortestLobe.
    double s     = wave_shape_word(shape);
    double limit = 1.0 - (2.0 * shortestLobe);

    s = (s > limit) ? limit : s;
    s = (s < 0.0) ? 0.0 : s;

    double lobe  = 0.5 * (1.0 - s);                  // the positive half's share of the cycle
    double at    = fmod(phase, 1.0);                 // 0 where the positive half begins
    double x;

    if (at < 0.0) {
        at += 1.0;
    }

    if (at < lobe) {
        x = 1.0 - fabs(1.0 - (2.0 * at / lobe));
    } else {
        x = -(1.0 - fabs(1.0 - (2.0 * (at - lobe) / (1.0 - lobe))));
    }
    return wave_sine_polynomial(x);
}

double wave_sine2(double phase, double shape) {
    return wave_sine2_limited(phase, shape, 0.0);
}

// §27.3 - Sine3 and Sine4's common ratio: Shape times a factor that falls with pitch (inc96 is the phase
// increment per 96 kHz sample, as a fraction of a cycle)
double wave_dsf_ratio(double shape, double inc96) {
    double ratio = wave_shape_word(shape) * (DSF_RATIO_SCALE - (DSF_RATIO_PITCH * inc96));

    return (ratio < 0.0) ? 0.0 : ratio;
}

static int64_t dsp_wrap56(int64_t v) {
    return (int64_t)((uint64_t)v << 8) >> 8;
}

// §27.3a - the part's division, step for step: sixteen DIVs on 24-bit words, the sign restored, then
// ASL #32. Once the quotient reaches 1 it wraps, which is what bounds Sine3 at the top of the dial.
static double dsf_divide(double numerator, double denominator) {
    int32_t num   = (int32_t)floor(numerator * DSP_WORD);
    int32_t den   = (int32_t)fmin(floor(denominator * DSP_WORD), DSP_WORD - 1.0);
    int64_t acc   = (int64_t)((num < 0) ? -num : num) << 24;
    int     carry = 0;

    for (int step = 0; step < DSF_DIVIDE_STEPS; step++) {
        int negative = acc < 0;

        acc   = dsp_wrap56((int64_t)(((uint64_t)acc << 1) | (uint64_t)carry));
        acc   = dsp_wrap56(negative ? (acc + ((int64_t)den << 24)) : (acc - ((int64_t)den << 24)));
        carry = acc >= 0;
    }

    if (num < 0) {
        acc = dsp_wrap56(-acc);
    }
    acc = dsp_wrap56((int64_t)((uint64_t)acc << 32));
    return (double)((int32_t)((uint32_t)(acc >> 24) << 8) >> 8) / DSP_WORD;
}

// §27.3 - a DSF as the part computes it: sin/16 over (1 - 2r cos + r^2)/4, then the level. `cosine` is
// cos(theta) for Sine3 and cos(2 theta) for Sine4. The result is in the engine's unit, four DSP words.
static double dsf_instrument(double theta, double cosine, double shape, double inc96) {
    double ratio    = wave_dsf_ratio(shape, inc96);
    double quotient = dsf_divide(sin(theta) / 16.0, (1.0 - (2.0 * ratio * cosine) + (ratio * ratio)) / 4.0);

    return 4.0 * quotient * (1.0 - (DSF_LEVEL_SLOPE * wave_shape_word(shape)));
}

// §27.3 - the instrument's Sine3: the whole harmonic series, times a level that falls with Shape
double wave_sine3_instrument(double phase, double shape, double inc96) {
    double theta = 2.0 * M_PI * phase;

    return dsf_instrument(theta, cos(theta), shape, inc96);
}

// §27.3 - and Sine4: the odd harmonics only, a further 1/(1 + r) down
double wave_sine4_instrument(double phase, double shape, double inc96) {
    double theta = 2.0 * M_PI * phase;

    return dsf_instrument(theta, cos(2.0 * theta), shape, inc96);
}

// notes §6 - the shapes as the editor draws them, at unit peak
double wave_sine3(double phase, double shape) {
    double ratio = wave_dsf_ratio(shape, 0.0);
    double theta = 2.0 * M_PI * phase;
    double denom = 1.0 - (2.0 * ratio * cos(theta)) + (ratio * ratio);

    return (sin(theta) / denom) * (1.0 - (ratio * ratio));
}

// notes §7
double wave_sine4(double phase, double shape) {
    double ratio = wave_dsf_ratio(shape, 0.0);
    double theta = 2.0 * M_PI * phase;
    double denom = 1.0 - (2.0 * ratio * cos(2.0 * theta)) + (ratio * ratio);
    double y     = (1.0 + ratio) * sin(theta) / denom;
    double peak  = (ratio >= (3.0 - (2.0 * sqrt(2.0))))
                  ? ((1.0 + ratio) / (4.0 * sqrt(ratio) * (1.0 - ratio)))
                  : (1.0 / (1.0 + ratio));

    return y / peak;
}

double wave_sine_by_index(uint32_t waveform, double phase, double shape) {
    switch (waveform) {
        case 0:
            return wave_sine1(phase, shape);

        case 1:
            return wave_sine2(phase, shape);

        case 2:
            return wave_sine3(phase, shape);

        case 3:
            return wave_sine4(phase, shape);

        default:
            return 0.0;    // 4..7 step, and cannot be answered without knowing how they are sampled
    }
}

// -- The four that step ------------------------------------------------------
// Parameters only. See waveModels.h for why.

// TriSaw: Shape skews the breakpoint from a symmetric triangle towards a sawtooth - the instrument's
// symmetry is raw/128, so Shape 0 (displayed 50%) gives 0.5 and Shape 1 gives 0.996. The engine also
// holds the fall to at least two samples at the note's pitch, as the instrument does (notes §8).
double wave_trisaw_peak(double shape) {
    return 0.5 + (shape * (127.0 / 256.0));
}

// DblSaw: "Double Saw signal. At 50% Shape setting, the signal consists of two saws in phase". The
// second saw's offset, measured 0 (in phase) to 0.5 (antiphase).
double wave_dblsaw_detune(double shape) {
    return wave_shape_word(shape) * 0.5;
}

// OscShpB's Pulse: high for (1 - Shape)/2 of the cycle, 50% down to nothing - the instrument's law.
double wave_shpb_pulse_duty(double shape) {
    return 0.5 * (1.0 - wave_shape_word(shape));
}

// SymPulse: one cycle is High for this long, then Low for the same, then zero for the rest. It is
// simply half the remaining Shape with NO floor under it - at Shape 1 the wave is silent, which is
// what the capture shows and is not a defect.
double wave_sympulse_half_segment(double shape) {
    return 0.5 * (1.0 - shape);
}
