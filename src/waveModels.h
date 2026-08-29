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

#ifndef WAVE_MODELS_H
#define WAVE_MODELS_H

#include <stdint.h>

// ── The shaped oscillator waves, measured from the instrument ───────────────────────────────────
//
// ONE HOME FOR EVERY MEASURED CONSTANT, so a wave cannot be corrected in one place and left wrong in
// another. This is exactly what paramCurves.c exists for and was pulled out of renderParams.c to
// achieve; these wave laws had the same problem and now get the same treatment.
//
// THE PROBLEM THIS SOLVES WAS REAL, not theoretical. The editor DREW these waves from laws measured
// off the hardware in August 2026 (Docs/todo.txt has the capture method, the sweeps and the
// correlation figures), while soundEngine.c's osc_shp_wave() carried a separate set derived from the
// manual's prose. The two disagreed on things the measurement had settled: the engine remapped Shape
// as (shape - 0.5)/0.49 CLAMPED AT ZERO, so HALF THE DIAL WAS DEAD where the capture shows harmonics
// growing from raw 16 upward; its Sine3 was a rectified sine, an even-harmonic reading the spectrum
// disproves; its Sine4 was a tanh soft clip; its Sine1 had both the wrong phase and the wrong
// endpoint. Those were corrected in place on 2026-08-23 — leaving two copies that agreed on the day
// and nothing to keep them agreeing.
//
// PLATFORM-FREE, like paramCurves.c. Nothing here draws or makes a sound; it needs no graphics API
// and no audio device, which is what lets both the editor and the VST3 plug-in link it.
//
// ── Why this is two kinds of function ───────────────────────────────────────
//
// SINE1..SINE4 ARE COMPLETE. They are closed-form and have no discontinuity, so every consumer wants
// the identical value at a given phase and there is nothing to decide. Call them and use the answer.
//
// THE OTHER FOUR ARE PARAMETERS ONLY, and that is deliberate rather than half-finished. TriSaw,
// DblSaw, Pulse and SymPulse all contain a step, and how you render a step is a property of what you
// are rendering INTO, not of the wave:
//   - the sound engine band-limits, because an unlimited step aliases across the whole spectrum;
//   - the editor draws a 200-point curve, and a hard step between two sample points is simply
//     missed, so it adds explicit narrow ramps at the crossings to make the drawn line pass through
//     zero where the real wave does.
// Handing both a single "sample" function would force one of them to undo the other's work. What
// they must share is the measured law — the duty, the skew, the detune — and that is what is here.
// The shape of the wave is settled; the sampling of it is the caller's business.

// Shape is the raw 0-127 parameter normalised to 0..1. It is NOT a percentage: the dial DISPLAYS
// 50 + 50*shape percent, so the manual's "50%" is raw 0 and its "99%" is raw 124, and the manual's
// figures land correctly once read that way. The dial acts over its whole range.

// ── Complete waves ──────────────────────────────────────────────────────────
double wave_sine1(double phase, double shape);
double wave_sine2(double phase, double shape);
double wave_sine3(double phase, double shape);
double wave_sine4(double phase, double shape);

// Sine1..Sine4 by index (0..3), for a caller that already has the waveform number. Returns 0.0 for
// any other index — the four that follow are not complete waves and cannot be answered here.
double wave_sine_by_index(uint32_t waveform, double phase, double shape);

// ── Measured parameters of the waves that step ──────────────────────────────

// TriSaw: where the peak sits. 0.5 is a symmetric triangle, approaching 1.0 a sawtooth.
double wave_trisaw_peak(double shape);

// DblSaw: two near-full sawtooths, the second detuned from the first by this fraction of a cycle —
// 0 (in phase) to 0.5 (antiphase, measured).
double wave_dblsaw_detune(double shape);

// Both of DblSaw's saws are near-full sawtooths rather than triangles; this is where their peak
// sits, in the same terms as wave_trisaw_peak().
#define WAVE_DBLSAW_PEAK    (0.97)

// Pulse: the high fraction of the cycle. Measured 50% high down to 1% high.
double wave_pulse_duty(double shape);

// SymPulse: one cycle is High for this long, then Low for the same, then zero for the remainder —
// so this is half the non-zero part. At shape 1 it vanishes and the wave is silent.
double wave_sympulse_half_segment(double shape);

#endif // WAVE_MODELS_H
