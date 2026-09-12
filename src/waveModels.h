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
// Notes: Docs/code-notes/waveModels.h.md - "// notes §k" refers there.

#ifndef WAVE_MODELS_H
#define WAVE_MODELS_H

#include <stdint.h>

// notes §1

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
