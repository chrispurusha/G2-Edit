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

// notes §2
double wave_sine1(double phase, double shape) {
    // notes §3
    double b = 0.25 - (0.2551 * shape) + (0.0125 * shape * shape);
    double w = 0.0;

    // b reaches 0.0074 at full Shape, so this guard is only against a divide by zero.
    if (b < 0.004) {
        b = 0.004;
    }

    if (phase < b) {
        w = 0.25 * (phase / b);
    } else if (phase < (1.0 - b)) {
        w = 0.25 + (0.5 * ((phase - b) / (1.0 - (2.0 * b))));
    } else {
        w = 0.75 + (0.25 * ((phase - (1.0 - b)) / b));
    }
    return sin(2.0 * M_PI * w);
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

// TriSaw: Shape skews the breakpoint from a symmetric triangle towards a sawtooth. Shape 0
// (displayed 50%) gives 0.5, a triangle; Shape 1 gives 0.97, near-sawtooth.
double wave_trisaw_peak(double shape) {
    return 0.5 + (shape * 0.47);
}

// DblSaw: "Double Saw signal. At 50% Shape setting, the signal consists of two saws in phase". The
// second saw's offset, measured 0 (in phase) to 0.5 (antiphase).
double wave_dblsaw_detune(double shape) {
    return shape * 0.5;
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
