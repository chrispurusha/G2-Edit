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

// The measured wave laws. See waveModels.h for why this file exists and why four of the eight waves
// are parameters rather than samples. Every constant below was fitted to a capture of the real
// instrument, not taken from the manual - several of the manual's descriptions are wrong, and where
// they are, the comment says so.

#include <math.h>

#include "waveModels.h"

// -- Sine1 -------------------------------------------------------------------
//
// The manual: "a phase modulated sine wave. At 50% Shape setting, the signal is a perfect sine wave
// and at 99% similar to a sawtooth wave". MEASURED 2026-08-23 over a nine-point Shape sweep,
// recovered by inverting sin() on the captured cycle to read the phase warp w(p) off the hardware:
//   - Shape 0 gives w(p) = p to three decimals, i.e. a pure sine. The harmonics grow from Shape 16
//     upwards, so the dial acts over its whole range and not just above half.
//   - w(0.5) = 0.500 at every Shape, so the descending zero crossing never moves and the duty stays
//     at 0.500. An earlier model let the duty collapse to 0.10, which drew a sine's top lobe crammed
//     into a tenth of the cycle with a full-width bottom lobe hung off it.
//   - w(1 - p) = 1 - w(p): the warp is odd-symmetric about the centre.
//   - The slope is piecewise constant with ONE breakpoint per half, sitting exactly at the peak,
//     where w = 0.25.
// So it is a one-parameter warp. b is the breakpoint and b = 0.25 is the identity, which is why
// Shape 0 is a clean sine; as b shrinks the rise compresses into the front of the cycle and the
// remainder becomes a long linear sweep from w = 0.25 to w = 0.75 - a slow fall from peak to
// trough, which is the sawtooth the manual describes.
double wave_sine1(double phase, double shape) {
    // RE-MEASURED 2026-08-30. The LINEAR term was already right; the QUADRATIC one was about five
    // times too large, which lifted b at the top of the dial and made our Sine1 duller than the
    // instrument - reported by ear as "hardware definitely sounds brighter than our version".
    //
    //     raw        0      16      32      64      96     127
    //     b     0.2500  0.2185  0.1870  0.1250  0.0630  0.0080     <- measured
    //     b     0.2500  0.2188  0.1896  0.1367  0.0915  0.0550     <- previous law
    //
    // Fitted per point against the harmonic series and anchored at the identity warp, residuals
    // +/-0.0013. The model FORM is confirmed by the same fit: it reproduces the measured harmonics
    // to 0.21 dB or better at every setting, and to 0.02 dB at raw 127 (-8.0/-11.9/-14.5/-16.5/
    // -18.2/-19.6 against a measured -8.1/-11.9/-14.6/-16.6/-18.2/-19.5). At full Shape the wave is
    // about 2 dB under an ideal sawtooth across the series, which is the manual's "similar to a
    // sawtooth wave" and is what it sounds like.
    //
    // HOW TO MEASURE THIS, because getting it wrong is easy and cost several wrong answers here.
    // Do NOT read harmonics off a period-synchronously averaged cycle: the period comes from an
    // INTEGER sample lag, so it is quantised, and over hundreds of averaged cycles the accumulated
    // phase error smears the upper harmonics away. That looks exactly like a capture-chain lowpass
    // - it produced an apparent 25 dB roll-off by the sixth harmonic and a whole set of plausible,
    // wrong constants. Refine f0 on the harmonic sum and read each harmonic with a windowed DFT
    // instead (tools has the working version). Validate the method on OscB's own sawtooth and
    // square first: the saw must come back at -6.0/-9.5/-12.0 and the square must show no even
    // harmonics at all. Ours now do, to 0.2 dB.
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

// -- Sine2 -------------------------------------------------------------------
//
// The manual: "a Sine -> Double Sine signal". A two-segment phase warp is the right FORM - it
// correlates 0.999+ with the capture - but an earlier model ran it the wrong way, opening the
// breakpoint out and widening the positive lobe. The hardware CLOSES it: the breakpoint runs 0.495
// down to 0.030, narrowing the positive lobe to a spike.
//
// The mean of sin() over this warp is exactly 2(2d - 1)/pi. Removing it is what lifts the spike
// above the trough, and the hardware's output is AC coupled, so removing it is also what the
// instrument does.
double wave_sine2(double phase, double shape) {
    double d    = 0.5 - (0.51 * shape) + (0.026 * shape * shape);

    if (d < 0.03) {
        d = 0.03;
    }
    double w    = (phase < d) ? (0.5 * (phase / d))
               : (0.5 + (0.5 * ((phase - d) / (1.0 - d))));
    double mean = 2.0 * ((2.0 * d) - 1.0) / M_PI;

    return (sin(2.0 * M_PI * w) - mean) / (1.0 - mean);
}

// -- Sine3 -------------------------------------------------------------------
//
// The manual calls this "a Sine -> Even harmonics signal", AND THE MANUAL IS WRONG: the measured
// spectrum at full Shape is 1, 0.90, 0.81, 0.73, 0.65 - a FULL harmonic series, with the 3rd and 5th
// as strong as the 2nd and 4th. Nothing that adds only even harmonics can produce that.
//
// What does produce it is the Poisson kernel: a geometric harmonic series, every harmonic present
// with amplitude ratio^n. This is its closed form.
double wave_sine3(double phase, double shape) {
    double ratio = shape * (1.0 - (0.105 * shape)); // measured 0.24, 0.48, 0.71, 0.90
    double theta = 2.0 * M_PI * phase;
    double denom = 1.0 - (2.0 * ratio * cos(theta)) + (ratio * ratio);

    if (denom < 1e-9) {
        denom = 1e-9;
    }
    return (sin(theta) / denom) * (1.0 - (ratio * ratio));
}

// -- Sine4 -------------------------------------------------------------------
//
// "a Sine -> Odd harmonics signal", and this time the manual is right: measured 1, 0.02, 0.95, 0.02,
// 0.90 for harmonics 1..5 at full Shape - odd only. (An early reading of an ASCII plot called it
// "double-humped, so the odd-harmonic model must be wrong". That was the wrong conclusion: two humps
// per cycle is exactly what strong odd harmonics look like.)
//
// The same kernel as Sine3 but in 2*theta, which is what makes the series odd-only, normalised by
// its own peak so the amplitude does not run away as ratio approaches 1.
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
