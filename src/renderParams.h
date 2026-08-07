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

//  Created by Øyvind Jacobsen Bjørkås on 25/06/2025.

#ifndef __RENDER_PARAMS_H__
#define __RENDER_PARAMS_H__

#include "sysIncludes.h"
#include "types.h"
#include "synthlibTypes.h"

// Selects the drawing area the widgets below render into: moduleArea (the default) for the
// scrolled/zoomed patch canvas, mainArea for a panel reusing the same widgets. Set it around
// the panel's own render pass and put it back afterwards.
void set_param_render_area(tArea area);

// The arithmetic behind the oscillator dials, split out from the renderers that print it so
// that the sound engine can derive its pitch and shape from exactly the same numbers the dial
// text shows. Changing a curve here changes what you see and what you hear together, which is
// the point - the two drifting apart would be invisible until it sounded wrong.
//
// Each takes the raw 0..127 param value. Which of the frequency curves applies is decided by
// the module's own PitchType param, whose index differs per module - osc_pitch_type_param_index()
// gives it, or -1 for a module that has no such param.
int osc_pitch_type_param_index(tModule * module);
double osc_freq_semitones(double paramValue);   // PitchType 0 "Semi":   -64 .. +63 semitones
double osc_freq_hz(double paramValue);          // PitchType 1 "Freq":   8.1758 Hz .. 12.55 kHz
double osc_freq_factor(double paramValue);      // PitchType 2 "Factor": 0.0248x .. 38.072x
double osc_fine_cents(double paramValue);       // Cent dial:            -50 .. +50 cents
double osc_shape_percent(double paramValue);    // Shape dial:           50% .. 99%

// The same idea for the filters: one definition of the cutoff, resonance and slope curves, shared
// by the dial text, the response curve drawn on the module, and the sound engine.
double flt_cutoff_hz(double paramValue);                   // Freq dial:  13.75 Hz .. ~21 kHz
double flt_resonance_q(double paramValue);                 // Res dial:   Q 0.5 .. 50
uint32_t flt_slope_extra_poles(uint32_t slopeValue);       // 0/1/2 extra one-pole stages: 12/18/24 dB
double flt_kbt_amount(uint32_t kbtValue);                  // Kbt scroll: 0, 0.25, 0.5, 0.75, 1.0
double lev_amp_gain(double paramValue);                    // LevAmp multiplier: 0.25x .. 4.0x, unity at 64
double lfo_rate_hz(uint32_t rangeMode, double paramValue); // LFO speed in Hz for a Range setting

// An envelope segment's length: the 0.5 ms .. 45 s scale ADRTimeStrMap prints, as seconds.
double adr_time_seconds(double paramValue);

// A delay's Time/Clk selector index, which differs per module type; -1 if it has none.
int delay_time_clk_param_index(tModuleType moduleType);

// The G2's own engine rate. Delay times are a whole number of SAMPLES at this rate, so it sets the
// grid the Time dial lands on — see delay_time_seconds(). Nothing to do with the rate G2-Edit's own
// sound engine happens to be running at.
#define G2_ENGINE_SAMPLE_RATE    (96000.0)

// The maximum a delay's Time dial reaches for a Range setting. Three different range tables exist
// and the delay modules do not share one — see the implementation.
double delay_range_max_seconds(tModuleType moduleType, uint32_t rangeValue);

double delay_time_seconds(double maxSeconds, double paramValue);
uint32_t clk_sync_index(double paramValue);
double clk_sync_beats(double paramValue);

tRectangle render_paramType1Freq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1OscFreq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Fine(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1GeneralFreq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Shape(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1FreqDrum(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1LFORate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Int(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1dB(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1MixLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Time(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1TimeClk(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1ADRTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1PulseTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Pitch(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1BipLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Partials(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1UniPol(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1LevAmpDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Pan(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1NoteDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Resonance(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Slider(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1StrMap(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1FreqShift(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);

tRectangle render_paramType1StandardToggle(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1Bypass(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1Enable(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);


#endif // __RENDER_PARAMS_H__
