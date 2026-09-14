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
// Notes: Docs/code-notes/paramCurves.h.md - "// notes §k" refers there.

#ifndef __PARAM_CURVES_H__
#define __PARAM_CURVES_H__

#include <stdbool.h>
#include "types.h"

// What a dial's raw 0..127 value means in real units. Deliberately carries NO drawing dependency -
// see paramCurves.c - so that the sound engine can be linked into a host with no GUI at all.

#ifdef __cplusplus
extern "C" {
#endif

// notes §1
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

// notes §2
typedef enum {
    eFilterTopologyLadder = 0,     // FltNord (FltClassic until §21): four-pole loop, the dB switch moves the tap
    eFilterTopologyCascadeLP,      // FltLP:  N identical one-poles, no resonance
    eFilterTopologyCascadeHP,      // FltHP:  the same, high-pass
    eFilterTopologyBiquad,         // FltStatic: two poles, flat passband, resonance as Q
    eFilterTopologyClassic,        // FltClassic: the instrument's own loop (reference §21)
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

// FltNord's GC toggle, as a linear broadband gain. MEASURED: GC does not reshape the filter at all
// - the peak above the passband is the same with it on or off - it attenuates everything as
// resonance rises, which is what stops the peak running away.
double flt_nord_gc_gain(double resParam);
double flt_kbt_amount(uint32_t kbtValue);                                 // Kbt scroll: 0, 0.25, 0.5, 0.75, 1.0
double lev_amp_gain(double paramValue);                                   // LevAmp multiplier: 0 (silent) .. 4.0x, unity at 64; measured, piecewise
double constant_level(double paramValue, bool bipolar);                   // a Constant's output, 1.0 = 64 units - notes §44
double lfo_rate_hz(uint32_t rangeMode, double paramValue);                // LFO speed in Hz for a Range setting
double vibrato_rate_hz(double paramValue);                                // the patch Vibrato's rate - notes §45

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
double mix_level_gain(double paramValue);   // the same curve as an amplitude, 0..1

// The patch's master volume in dB: -78 at the bottom of the dial, 0 at the top.
double patch_volume_db(double paramValue);

// An envelope segment's length in seconds: the 0.5 ms .. 45 s scale the manual quotes.
double adr_time_seconds(double paramValue);

// notes §3
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

// Sound engine reference §17.2 - the instrument's envelope curves: the Log and Exp attacks cover a factor of 16
// (24 dB) in the attack time, and decay and release fall 40 dB in theirs.
#define ENV_RISE_SHARPNESS     (2.772588722239781)    // ln 16
#define ENV_FALL_SHARPNESS     (4.605170185988091)    // ln 100
#define ENV_LOG_RISE_TARGET    (16.0 / 15.0)          // the Log attack's one-pole aims here, arriving at 1

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

// Reads one parameter for a *_build() below: the engine passes its morph-following reader, a graph
// on the module face the raw one.
typedef double (*tParamReader)(tModule * module, uint32_t variation, uint32_t index);

// The Shaper group: seven memoryless transfer functions, shared by the engine and the graphs.
typedef enum {
    eShaperClip = 0,
    eShaperOverdrive,
    eShaperSaturate,
    eShaperShpExp,
    eShaperWaveWrap,
    eShaperShpStatic,
    eShaperRect,
} tShaperKind;

typedef struct {
    tShaperKind kind;
    uint32_t    curve;       // the Type/Curve/Mode drop-down, raw - a drop-down carries no morph
    bool        sym;         // Clip and Overdrive: Sym shapes both halves, Asym the positive one
    double      amount;      // the dial, 0..1
    double      mod;         // the modulation attenuator, 0..1
    uint32_t    signalLeg;   // input leg carrying the signal; the other one is the modulation
    bool        active;
} tShaperSettings;

bool shaper_settings_build(tModule * module, uint32_t variation, tParamReader dial, tShaperSettings * out); // false: not a shaper
double shaper_transfer(const tShaperSettings * settings, double amount, double input);                      // input and amount clamped

// EqPeak, Eq2Band and Eq3band as the bands they add up to - §11 of the engine reference.
typedef struct {
    double inputLevel;
    bool   active;
    double lowHz;           // 0 = no low shelf
    double lowGain;
    double highHz;          // 0 = no high shelf
    double highGain;
    double peakHz;          // 0 = no peak
    double peakDamping;
    double peakGain;
} tEqBands;

bool eq_bands_build(tModule * module, uint32_t variation, tParamReader dial, tEqBands * out);  // false: not an EQ
double eq_magnitude(const tEqBands * bands, double hz);                                        // the bands' shape; level excluded

// FltComb - §13 of the engine reference.
typedef struct {
    double feedForward;    // per unit of g
    double feedback;
    double extraDelay;     // samples at FLTCOMB_REFERENCE_RATE
    double gainDbPerG2;
} tCombShape;

#define FLTCOMB_REFERENCE_RATE    (96000.0)    // the engine rate §13's sample offsets were measured at

const tCombShape * flt_comb_shape(uint32_t type);                                             // Notch, Peak, Deep
double flt_comb_feedback(double fbParam);                                                     // g: -1..+1, none at 64
double flt_comb_delay_samples(double control, const tCombShape * shape, double sampleRate);
double flt_comb_magnitude(const tCombShape * shape, double g, double delaySamples, double cyclesPerSample);

// FltPhase. NOT played by the engine, and only partly measured - see paramCurves.c before relying on it.
typedef struct {
    double   centreHz;
    double   q;
    uint32_t sections;     // allpass sections, one notch each
    double   g;
    uint32_t type;         // fltPhaseTypeStrMap: Notch, Peak, Deep
} tPhaserSettings;

bool flt_phase_settings_build(tModule * module, uint32_t variation, tParamReader dial, tPhaserSettings * out); // false: not FltPhase
double flt_phase_magnitude(const tPhaserSettings * settings, double hz);

// Operator (a DX7 operator): its frequency by the DX7's laws - paramCurves.c's notes §41.
#define OPERATOR_RATIO_FIXED_PARAM    (2)
#define OPERATOR_FINE_PARAM           (4)
#define OPERATOR_COARSE_MAX           (31u)
#define OPERATOR_FINE_MAX             (99u)

double operator_ratio(uint32_t coarse, uint32_t fine);       // Ratio mode: the multiple of the played pitch
double operator_fixed_hz(uint32_t coarse, uint32_t fine);    // Fixed mode

// Compress: its dials by the instrument's own readings, and its static curve - paramCurves.c's notes §42.
#define COMPRESS_PARAM_THRESHOLD      (0)
#define COMPRESS_PARAM_RATIO          (1)
#define COMPRESS_PARAM_REFLVL         (4)
#define COMPRESS_DB_OFFSET            (30.0)   // Thr and RefLvl: dB = raw - this
#define COMPRESS_THRESHOLD_OFF_RAW    (42u)    // the Thr dial reads "Off" here
#define COMPRESS_LEVEL_RAW_MAX        (42u)
#define COMPRESS_RATIO_RAW_MAX        (66u)

double compress_ratio(uint32_t raw);                                                            // 1.0:1 up to about 95:1
uint32_t compress_ratio_raw(double ratio);                                                      // the nearest dial value
double compress_out_db(double inDb, uint32_t thresholdRaw, uint32_t refRaw, uint32_t ratioRaw); // after the detector has settled
uint32_t compress_meter_lit(double reductionDb);                                                // LEDs lit for this much gain reduction (over Thr x (1 - 1/ratio))
double compress_meter_reduction_db(uint32_t lit);                                               // the least reduction that lights this many, for the graph's live point
// DXRouter: the 32 DX7 algorithms as who-modulates-whom, shared by the graph and the sound engine -
// paramCurves.c's notes §43.
#define DX_OPERATORS     (6)
#define DX_ALGORITHMS    (32)

typedef struct {
    uint8_t target[DX_OPERATORS];   // bit k: modulates operator k + 1; no bits: a carrier
    uint8_t feedbackFrom;           // operator numbers, 1-6
    uint8_t feedbackTo;
} tDxAlgorithm;

const tDxAlgorithm * dx_algorithm(uint32_t index);   // 0..31 is algorithm 1..32; out of range reads as 1

#ifdef __cplusplus
}
#endif

#endif
