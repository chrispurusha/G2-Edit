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

#include "waveModels.h"

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

// notes §4
double wave_sine2(double phase, double shape) {
    double d    = 0.5 - (0.51 * shape) + (0.026 * shape * shape);

    // notes §5
    if (d < 0.005) {
        d = 0.005;
    }
    double w    = (phase < d) ? (0.5 * (phase / d))
               : (0.5 + (0.5 * ((phase - d) / (1.0 - d))));
    double mean = 2.0 * ((2.0 * d) - 1.0) / M_PI;

    return (sin(2.0 * M_PI * w) - mean) / (1.0 - mean);
}

// notes §6
double wave_sine3(double phase, double shape) {
    double ratio = shape * (1.0 - (0.105 * shape)); // measured 0.24, 0.48, 0.71, 0.90
    double theta = 2.0 * M_PI * phase;
    double denom = 1.0 - (2.0 * ratio * cos(theta)) + (ratio * ratio);

    if (denom < 1e-9) {
        denom = 1e-9;
    }
    return (sin(theta) / denom) * (1.0 - (ratio * ratio));
}

// notes §7
double wave_sine4(double phase, double shape) {
    double ratio = 0.94 * shape;                    // measured 0.24, 0.48, 0.71, 0.94
    double theta = 2.0 * M_PI * phase;
    double denom = 1.0 - (2.0 * ratio * cos(2.0 * theta)) + (ratio * ratio);

    if (denom < 1e-9) {
        denom = 1e-9;
    }
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

// Pulse: "a Pulse with selectable ASYMMETRIC pulse width". Measured 50% high down to 1% high.
double wave_pulse_duty(double shape) {
    return 0.5 - (shape * 0.49);
}

// SymPulse: one cycle is High for this long, then Low for the same, then zero for the rest. It is
// simply half the remaining Shape with NO floor under it - at Shape 1 the wave is silent, which is
// what the capture shows and is not a defect.
double wave_sympulse_half_segment(double shape) {
    return 0.5 * (1.0 - shape);
}
