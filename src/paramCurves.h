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

#ifndef __PARAM_CURVES_H__
#define __PARAM_CURVES_H__

#include <stdbool.h>
#include "types.h"

// What a dial's raw 0..127 value means in real units. Deliberately carries NO drawing dependency -
// see paramCurves.c - so that the sound engine can be linked into a host with no GUI at all.

#ifdef __cplusplus
extern "C" {
#endif

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
double lfo_shape_percent(double paramValue);    // LfoShpA Shape dial:    1% .. 99% (neutral at centre)

// PitchType 4 "Sub": the same note scale as Semi, eleven octaves down. fineSemitones is the Cent
// dial's offset, (cent - 64) / 128 of a semitone; pass 0.0 if it is not to hand.
double osc_sub_freq_hz(double paramValue, double fineSemitones);

// The same idea for the filters: one definition of the cutoff, resonance and slope curves, shared
// by the dial text, the response curve drawn on the module, and the sound engine.
double flt_cutoff_hz(double paramValue);                   // Freq dial:  13.75 Hz .. ~21 kHz
double flt_resonance_q(double paramValue);                 // Res dial:   Q 0.5 .. 50
uint32_t flt_slope_extra_poles(uint32_t slopeValue);       // 0/1/2 extra one-pole stages: 12/18/24 dB

// FltClassic as the ladder it actually is, measured 2026-08-24: the resonance loop is ALWAYS four
// poles and the dB switch only moves the output tap, which is why the Res dial means the same thing
// in every slope mode. See the note above flt_ladder_feedback() in paramCurves.c.
double flt_ladder_feedback(double paramValue);                            // Res dial:   feedback 0 .. 4 (self-oscillation)
uint32_t flt_ladder_tap(uint32_t slopeValue);                             // 2/3/4 poles tapped: 12/18/24 dB
double flt_ladder_magnitude(double ratio, double feedback, uint32_t tap); // |G^tap / (1 + k.G^4)| at f/fc

// Which shape a multi-mode filter is currently producing. FltStatic's FilterType selects among the
// first three; FltNord adds the fourth.
// Which of the three measured topologies a filter module uses. They are not variants of one
// another - see the notes in paramCurves.c.
typedef enum {
    eFilterTopologyLadder = 0,     // FltClassic, FltNord: four-pole loop, the dB switch moves the tap
    eFilterTopologyCascadeLP,      // FltLP:  N identical one-poles, no resonance
    eFilterTopologyCascadeHP,      // FltHP:  the same, high-pass
    eFilterTopologyBiquad          // FltStatic: two poles, flat passband, resonance as Q
} tFilterTopology;

typedef enum {
    eFilterShapeLowPass = 0,
    eFilterShapeBandPass,
    eFilterShapeHighPass,
    eFilterShapeBandReject
} tFilterShape;

// FltLP / FltHP: N identical one-poles at the dial's corner, N being the slope mode plus one.
uint32_t flt_cascade_poles(uint32_t slopeMode);
double flt_cascade_magnitude(double ratio, uint32_t poles, bool highPass);

// FltStatic: a plain resonant biquad, flat passband. Its Q is NOT the Q the dial prints -
// see the note in paramCurves.c before using flt_resonance_q() for a curve.
double flt_static_q(double paramValue);
double flt_biquad_magnitude(double ratio, double q, tFilterShape shape);

// FltNord: FltClassic's four-pole loop; the dB/Oct selector moves the tap, it does not shorten it.
uint32_t flt_nord_tap(uint32_t dbOctValue);
double flt_kbt_amount(uint32_t kbtValue);                                 // Kbt scroll: 0, 0.25, 0.5, 0.75, 1.0
double lev_amp_gain(double paramValue);                                   // LevAmp multiplier: 0 (silent) .. 4.0x, unity at 64; measured, piecewise
double lfo_rate_hz(uint32_t rangeMode, double paramValue);                // LFO speed in Hz for a Range setting

// These four were read off the hardware at raw 0, 64 and 127 rather than inferred.
double pshift_semitones(double paramValue);                // PShift Semi:  -16.0 .. +15.75, quarter semitones
double scratch_ratio(double paramValue);                   // Scratch Ratio: -4.00 .. +4.00, 0 is a standstill
double digitizer_rate_hz(double paramValue);               // Digitizer Rate: 32.70 Hz .. 50.2 kHz, a pitch scale
double pitchtrack_threshold_db(double paramValue);         // PitchTrack Threshold: -inf .. 0 dB

double flanger_rate_hz(double paramValue);                 // Flanger Rate: 0.01 Hz .. 2.91 Hz, linear
double phaser_rate_hz(double paramValue);                  // Phaser Rate:  0.05 Hz .. 11.6 Hz, square in the dial

// A mixer channel's level in dB, for a channel whose Curve is dB. Returns -infinity at the bottom
// of the dial, where the printed scale reads "-oo"; the two steps above it are named rather than
// computed. Both are the caller's business - this half of the split stays numeric.
double mix_level_db(double paramValue);

// The patch's master volume in dB: -78 at the bottom of the dial, 0 at the top.
double patch_volume_db(double paramValue);

// An envelope segment's length in seconds: the 0.5 ms .. 45 s scale the manual quotes.
double adr_time_seconds(double paramValue);

// An envelope segment's SHAPE - the companion to adr_time_seconds() above, which gives its length.
// The Shape param names both halves at once (envShapeStrMap is {LogExp, LinExp, ExpExp, LinLin}),
// the first word naming the attack curve and the second the decay and release, so three of the four
// fall exponentially and only LinLin is straight throughout.
//
// SHARED SO THE DRAWN ENVELOPE AND THE PLAYED ONE CANNOT DISAGREE - and they did. The engine and the
// module face carried the same law with two different sharpness constants, 5.0 against 4.0, so the
// curve drawn on an EnvADSR was never quite the curve it played. Nothing announced it, because each
// file was self-consistent; it is exactly the drift this file exists to prevent. Both constants were
// also wrong: measured on the hardware 2026-08-24, the rise and the fall are not equally curved, so
// there are now two of them - see ENV_ATTACK_SHARPNESS and ENV_FALL_SHARPNESS in paramCurves.c.
typedef enum {
    eEnvShapeLogExp = 0,
    eEnvShapeLinExp,
    eEnvShapeExpExp,
    eEnvShapeLinLin,
} tEnvShape;

// Both take a linear 0..1 progress through the segment and shape it, so a segment still takes
// exactly the time its dial states whatever curve it is drawn with.
double env_attack_level(uint32_t envShape, double progress);   // rises 0 -> 1 across the segment
double env_fall_level(uint32_t envShape, double progress);     // falls 1 -> 0 across the segment

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
#ifdef __cplusplus
}
#endif

#endif
