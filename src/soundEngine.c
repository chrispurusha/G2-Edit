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
// Notes: Docs/sound-engine-notes.md - "// notes §k" refers there.

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "dataBase.h"
#include "cableChain.h"
#include "globalVars.h"
#include "renderParams.h"
#include "paramCurves.h"
#include "moduleResourcesAccess.h"
#include "patchParamsResources.h"
#include "audioOutput.h"
#include "midiInput.h"
#include "waveModels.h"
#include "soundEngine.h"

// See soundEngine.h for what this does and does not attempt.

// Parameter indices, in the order moduleResources.h lists them for each module type.
#define OSCB_PARAM_TUNE          (0)
#define OSCB_PARAM_CENT          (1)
#define OSCB_PARAM_KBT           (2)
#define OSCB_PARAM_PITCH_MOD     (3)   // depth for the two pitch modulation inputs
#define OSCB_PARAM_PITCH_TYPE    (4)
#define OSCB_PARAM_SHAPE         (6)
#define OSCB_PARAM_WAVEFORM      (8)
#define OSCB_PARAM_ACTIVE        (9)   // A power button: non-zero is on, 0 is bypassed

// OscA is OscB with the Shape dial and the FM section taken away, so its parameters sit at different
// indices and its waveform list is its own. It shares the oscillator DSP entirely - see eNodeOsc.
#define OSCA_PARAM_TUNE          (0)
#define OSCA_PARAM_CENT          (1)
#define OSCA_PARAM_KBT           (2)
#define OSCA_PARAM_PITCH_MOD     (3)
#define OSCA_PARAM_WAVEFORM      (4)
#define OSCA_PARAM_ACTIVE        (5)
#define OSCA_PARAM_PITCH_TYPE    (6)

// notes §1
typedef struct {
    int freq;
    int env;
    int kbt;
    int res;
    int slope;               // parameter index, or -1 when the module has none
    int slopeMode;           // MODE index, or -1; FltLP keeps its Slope here, not in a parameter
    int shape;               // FilterType parameter for the multi-mode filters, or -1
    int gc;                  // FltNord's Gain Control toggle, or -1 for a module without one
    int active;
} tFilterParams;

// notes §2
static bool engine_no_free_run(void) {
    static int cached = -1;

    if (cached < 0) {
        const char * v = getenv("G2_ENGINE_NO_FREERUN");
        cached = ((v != NULL) && (v[0] != '\0')) ? 1 : 0;
    }
    return cached == 1;
}

bool engine_filter_legacy(void) {
    static int cached = -1;

    if (cached < 0) {
        const char * v = getenv("G2_FILTER_LEGACY");
        cached = ((v != NULL) && (v[0] != '\0')) ? 1 : 0;
    }
    return cached == 1;
}

static bool filter_param_map(tModuleType type, tFilterParams * map) {
    // notes §3
    switch (type) {
        case moduleTypeFltClassic:
        {
            *map = (tFilterParams){
                .freq = 0, .env = 1, .kbt = 2, .res = 3, .slope = 4, .slopeMode = -1, .gc = -1, .shape = -1, .active = 5
            };
            return true;
        }
        case moduleTypeFltLP:
        {
            // notes §4
            if (engine_filter_legacy()) {
                *map = (tFilterParams){
                    .freq = 0, .env = 1, .kbt = 2, .res = -1, .slope = 3, .slopeMode = -1, .gc = -1, .shape = -1, .active = 4
                };
                return true;
            }
            *map = (tFilterParams){
                .freq = 0, .env = 1, .kbt = 2, .res = -1, .slope = -1, .slopeMode = 0, .gc = -1, .shape = -1, .active = 3
            };
            return true;
        }
        case moduleTypeFltHP:
        {
            // Laid out exactly like FltLP, and its Slope is a mode for the same reason.
            *map = (tFilterParams){
                .freq = 0, .env = 1, .kbt = 2, .res = -1, .slope = -1, .slopeMode = 0, .gc = -1, .shape = -1, .active = 3
            };
            return true;
        }
        case moduleTypeFltStatic:
        {
            // Freq, Res, FilterType, Bypass, GC. No slope: it is two poles, always.
            *map = (tFilterParams){
                .freq = 0, .env = -1, .kbt = -1, .res = 1, .slope = -1, .slopeMode = -1, .active = 3
            };
            return true;
        }
        case moduleTypeFltNord:
        {
            // Freq, Pitch, Kbt, GC, Res, dB/Oct, Bypass, FmLin, FilterType, ResM.
            *map = (tFilterParams){
                .freq = 0, .env = 1, .kbt = 2, .res = 4, .slope = 5, .slopeMode = -1, .gc = 3, .shape = 8, .active = 6
            };
            return true;
        }
        default:
        {
            return false;
        }
    }
}

#define FLT_PARAM_FREQ           (0)
#define FLT_PARAM_ENV            (1)   // modulation depth for the Env input, 0..200%
#define FLT_PARAM_KBT            (2)
#define FLT_PARAM_RES            (3)
#define FLT_PARAM_SLOPE          (4)
#define FLT_PARAM_ACTIVE         (5)

#define ENV_PARAM_SHAPE          (0)
#define ENV_PARAM_ATTACK         (1)
#define ENV_PARAM_DECAY          (2)
#define ENV_PARAM_SUSTAIN        (3)
#define ENV_PARAM_RELEASE        (4)

#define LEVAMP_PARAM_GAIN        (0)
#define LEVAMP_PARAM_TYPE        (1)   // 0 = lin, 1 = exp

// notes §5
#define OUT_PARAM_DESTINATION    (0)
#define FXIN_PARAM_SOURCE        (0)   // Fx-In's "In from": inFxStrMap, 0 = FX 1/2, 1 = FX 3/4
#define OUT_PARAM_ACTIVE         (1)   // 2toOut's Bypass, non-zero is on
#define OUT_PARAM_PAD            (2)   // padStrMap: 0 dB or -6 dB

// Glide and Bend are per-PATCH settings rather than module parameters: they live on hidden modules
// in the Morph location, which is where the G2 keeps the things the patch-settings page edits.
typedef enum {
    eGlideOff = 0,
    eGlideNormal,
    eGlideAuto,     // glides only between overlapping notes
} tGlideMode;

// OscShpB lays its parameters out differently from OscB — Active is 8, not 9, and the waveform is
// at 10 with eight choices rather than at 8 with five.
#define SHPB_PARAM_TUNE            (0)
#define SHPB_PARAM_CENT            (1)
#define SHPB_PARAM_KBT             (2)
#define SHPB_PARAM_PITCH_MOD       (3)
#define SHPB_PARAM_PITCH_TYPE      (4)
#define SHPB_PARAM_SHAPE           (6)
#define SHPB_PARAM_ACTIVE          (8)

// notes §6
#define SHPB_MODE_WAVEFORM         (0)

// notes §7
#define SHPA_PARAM_TUNE            (0)
#define SHPA_PARAM_CENT            (1)
#define SHPA_PARAM_KBT             (2)
#define SHPA_PARAM_PITCH_MOD       (3)
#define SHPA_PARAM_SHAPE           (7)
#define SHPA_PARAM_WAVEFORM        (9)
#define SHPA_PARAM_ACTIVE          (10)

// notes §8
#define CLIP_PARAM_LEVEL_MOD       (0)
#define CLIP_PARAM_LEVEL           (1)
#define CLIP_PARAM_SHAPE           (2)
#define CLIP_PARAM_ACTIVE          (3)

#define OD_PARAM_AMOUNT_MOD        (0)
#define OD_PARAM_AMOUNT            (1)
#define OD_PARAM_ACTIVE            (2)
#define OD_PARAM_TYPE              (3)
#define OD_PARAM_SHAPE             (4)

#define SAT_PARAM_AMOUNT           (0)
#define SAT_PARAM_AMOUNT_MOD       (1)
#define SAT_PARAM_ACTIVE           (2)
#define SAT_PARAM_CURVE            (3)

#define SHPEXP_PARAM_AMOUNT        (0)
#define SHPEXP_PARAM_AMOUNT_MOD    (1)
#define SHPEXP_PARAM_ACTIVE        (2)
#define SHPEXP_PARAM_CURVE         (3)

#define WRAP_PARAM_AMOUNT_MOD      (0)
#define WRAP_PARAM_AMOUNT          (1)
#define WRAP_PARAM_ACTIVE          (2)

#define SHPSTATIC_PARAM_MODE       (0)
#define SHPSTATIC_PARAM_ACTIVE     (1)

#define RECT_PARAM_MODE            (0)
#define RECT_PARAM_ACTIVE          (1)

// Pulse: a one-shot gate fired by a RISING EDGE at its input. Time and Range set how long it stays
// high; there is no power button, so it is always live.
#define PULSE_PARAM_TIME       (0)
#define PULSE_PARAM_TIMEMOD    (1)
#define PULSE_PARAM_RANGE      (2)

// What counts as a logic high. The G2's logic signals are full-scale 0/1, so anything near the
// middle separates them; the envelope that drives this in the measurement patch sweeps the whole
// range, so the exact threshold is not delicate.
#define PULSE_THRESHOLD    (0.5)

// §3.1 - where each summing mixer keeps its controls; -1 is "has none".
typedef struct {
    tModuleType type;
    uint8_t     channels;   // level-controlled channels; the Chain input(s) follow them
    bool        stereo;     // each channel is an L/R pair of input legs
    int8_t      lev;        // channel 1's level dial
    int8_t      levStep;    // how far apart successive channels' dials sit
    int8_t      on;         // channel 1's On button
    int8_t      onStep;
    int8_t      inv;        // channel 1's Inv switch (invStrMap {Pos, Inv})
    int8_t      invStep;
    int8_t      curve;      // expStrMap drop-down
    int8_t      pad;        // mixerPadStrMap / db12BPadStrMap drop-down
} tMixSpec;

static const tMixSpec kMixSpecs[] = {
    //  type                 ch  stereo  lev st  on  st  inv st curve pad
    {moduleTypeMix1to1A, 1, false,  0, 1,  1, 1, -1, 0,  2, -1},
    {moduleTypeMix1to1S, 1, true,   0, 1,  1, 1, -1, 0,  2, -1},
    {moduleTypeMix2to1A, 2, false,  0, 2,  1, 2, -1, 0,  4, -1},    // Lev1 On1 Lev2 On2 - interleaved
    {moduleTypeMix2to1B, 2, false,  1, 2, -1, 0,  0, 2,  4, -1},    // Inv1 Lev1 Inv2 Lev2
    {moduleTypeMix4to1A, 4, false, -1, 0, -1, 0, -1, 0, -1, -1},
    {moduleTypeMix4to1B, 4, false,  0, 1, -1, 0, -1, 0,  4, -1},
    {moduleTypeMix4to1C, 4, false,  0, 1,  4, 1, -1, 0,  9,  8},
    {moduleTypeMix4to1S, 4, true,   0, 1,  4, 1, -1, 0,  8, -1},
    {moduleTypeMix8to1A, 8, false, -1, 0, -1, 0, -1, 0, -1,  0},
    {moduleTypeMix8to1B, 8, false,  0, 1, -1, 0, -1, 0,  8,  9},
    {moduleTypeMixFader, 8, false,  0, 1,  8, 1, -1, 0, 16, 17},
};

// §4
typedef enum {
    eFadePan = 0,       // In, Mod -> L, R            PanMod 0, Pan 1, LogLin 2
    eFadeCross,         // In1, In2, Mod -> Out       MixMod 0, Mix 1, LogLin 2
    eFadeOneToTwo,      // In, Ctrl -> Out1, Out2     Mix 0, MixMod 1
    eFadeTwoToOne,      // In1, In2, Ctrl -> Out      Mix 0, MixMod 1
} tFadeKind;

// §7.2
static const struct {
    double dial;
    double cornerHz;
    double rmsDb;
}                     kNoiseColour[] = {
    {  0, 18305.6,  -7.67}, {  8, 11848.2,  -8.66}, { 16, 7664.3,  -9.63}, { 24, 4869.2, -10.18},
    { 32,  3164.4, -10.22}, { 40,  2015.7,  -9.78}, { 48, 1352.6,  -9.37}, { 56,  905.5,  -8.86},
    { 64,   614.9,  -8.55}, { 72,   438.8,  -8.67}, { 80,  322.6,  -9.06}, { 88,  241.8,  -9.74},
    { 96,   193.6, -10.72}, {104,   163.8, -11.76}, {112,  142.6, -12.75}, {120,  136.2, -14.55},
    {127,   128.8, -14.84},
};

// §7.2 - the gain makes uniform white noise come out at the tabulated RMS.
static void noise_colour(double value, double sampleRate, double * pole, double * gain) {
    const uint32_t last   = (uint32_t)(sizeof(kNoiseColour) / sizeof(kNoiseColour[0])) - 1u;
    uint32_t       i      = 0;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }

    while ((i < (last - 1u)) && (value > kNoiseColour[i + 1u].dial)) {
        i++;
    }
    double         t      = (value - kNoiseColour[i].dial) / (kNoiseColour[i + 1u].dial - kNoiseColour[i].dial);
    double         corner = exp(log(kNoiseColour[i].cornerHz)
                                + (t * (log(kNoiseColour[i + 1u].cornerHz) - log(kNoiseColour[i].cornerHz))));
    double         rmsDb  = kNoiseColour[i].rmsDb + (t * (kNoiseColour[i + 1u].rmsDb - kNoiseColour[i].rmsDb));
    double         a      = exp(-2.0 * M_PI * corner / sampleRate);

    *pole = a;
    *gain = pow(10.0, rmsDb / 20.0) / sqrt((1.0 - a) / (3.0 * (1.0 + a)));
}

// §2.1
static double dial_fraction(double value) {
    return (value >= 127.0) ? 1.0 : (value / 128.0);
}

// §6.1 - where each basic oscillator keeps its dials and its waveform; -1 is "has none".
typedef struct {
    tModuleType type;
    int8_t      tune;
    int8_t      cent;
    int8_t      kbt;
    int8_t      pitchMod;       // the PitchVar attenuator
    int8_t      pitchType;      // pitchTypeStrMap {Semi, Freq, Factor, Partial}
    int8_t      active;
    int8_t      waveParam;      // waveform as a parameter...
    int8_t      waveMode;       // ...or as a mode
    int8_t      shape;
    bool        aWaves;         // OscA's six, with three fixed pulse widths
} tOscParams;

#define OSCNOISE_PARAM_WIDTH_MOD    (5)    // §8.1 - the module tables have 5 and 6 swapped
#define OSCNOISE_PARAM_WIDTH        (6)

static const tOscParams kOscParams[] = {
    //  type             tune cent kbt pmod ptype on  wparam wmode shape aWaves
    {moduleTypeOscB,     0, 1, 2,  3, 4, 9,  8, -1,  6, false},
    {moduleTypeOscA,     0, 1, 2,  3, 6, 5,  4, -1, -1, true },
    {moduleTypeOscC,     0, 1, 2,  7, 3, 5, -1,  0, -1, true },               // FmM 4, FM type 6: FM not modelled, as on OscB
    {moduleTypeOscD,     0, 1, 2, -1, 3, 4, -1,  0, -1, true },
    {moduleTypeOscNoise, 0, 1, 2,  3, 4, 7, -1, -1, -1, false},
};

static const tOscParams * osc_params(tModuleType type) {
    for (uint32_t i = 0; i < (sizeof(kOscParams) / sizeof(kOscParams[0])); i++) {
        if (kOscParams[i].type == type) {
            return &kOscParams[i];
        }
    }

    return NULL;
}

static const tMixSpec * mix_spec(tModuleType type) {
    for (uint32_t i = 0; i < (sizeof(kMixSpecs) / sizeof(kMixSpecs[0])); i++) {
        if (kMixSpecs[i].type == type) {
            return &kMixSpecs[i];
        }
    }

    return NULL;
}

#define MIX_CURVE_LIN               (1) // expStrMap is {"Exp", "Lin", "dB"} — Lin is the middle one

// StChorus: a detune depth and an amount, then its power button.
#define CHORUS_PARAM_DETUNE         (0)
#define CHORUS_PARAM_AMOUNT         (1)
#define CHORUS_PARAM_ACTIVE         (2)

// Compress: threshold and reference level run 0..42, ratio 0..66.
#define COMP_PARAM_THRESHOLD        (0)
#define COMP_PARAM_RATIO            (1)
#define COMP_PARAM_ATTACK           (2)
#define COMP_PARAM_RELEASE          (3)
#define COMP_PARAM_REFLVL           (4)
#define COMP_PARAM_ACTIVE           (6)

// Read off the instrument's own dial displays, not guessed. See where they are used.
#define COMP_THRESHOLD_OFFSET_DB    (30.0)      // displayed dB = raw - this
#define COMP_THRESHOLD_OFF          (42.0)      // the dial reads "Off" here
#define COMP_THRESHOLD_NONE         (1.0e9)     // an amplitude nothing reaches
#define COMP_ATTACK_MIN_S           (0.00053)   // raw 1; raw 0 is "Fast", i.e. instant
#define COMP_ATTACK_MAX_S           (0.767)
#define COMP_RELEASE_MIN_S          (0.125)
#define COMP_RELEASE_MAX_S          (10.2)

// The Ratio dial, in three straight runs that repeat a decade higher above raw 34 — 1.0:1 up to
// about 95:1. Transcribed from the instrument's own formatter rather than fitted.
static double compressor_ratio(double rawValue) {
    int  raw    = (int)rawValue;
    bool decade = (raw > 34);
    int  p      = decade ? (raw - 35) : raw;
    int  tenths = 0;

    if (p < 0) {
        p = 0;
    }

    if (p <= 9) {
        tenths = p + 10;
    } else if (p < 25) {
        tenths = p * 2;
    } else {
        tenths = (p * 5) - 75;
    }

    if (decade) {
        tenths *= 10;
    }
    return (double)tenths / 10.0;
}

// notes §9
#define DELAY_PARAM_TIME        (0)
#define DELAY_PARAM_FEEDBACK    (1)
#define DELAY_PARAM_LP          (2)   // DelayA calls this Filter; both are a damping control
#define DELAY_PARAM_DRYWET      (3)
#define DELAY_PARAM_HP          (8)

// notes §10
#define DELAY_LP_MIN_HZ         (660.0)

// notes §11
#define DELAY_HP_LOG_A          (1.74224)
#define DELAY_HP_LOG_B          (0.100227)
#define DELAY_HP_LOG_C          (-0.000377517)
#define DELAY_LP_MAX_HZ         (20000.0)
#define DELAYA_PARAM_ACTIVE     (4)
#define DELAYB_PARAM_ACTIVE     (7)
#define DELAY_MODE_RANGE        (0)

#define REVERB_PARAM_TIME       (0)
#define REVERB_PARAM_BRIGHT     (1)
#define REVERB_PARAM_DRYWET     (2)
#define REVERB_PARAM_ACTIVE     (3)

// notes §12
typedef struct {
    int rate;
    int range;      // which of the Rate Sub/Lo/Hi/BPM/Clk sweeps the rate dial walks
    int waveform;
    int polarity;   // posStrMap: Pos, PosInv, Neg, NegInv, Bip, BipInv
    int shape;      // LfoShpA only
    int active;
} tLfoParams;

// LfoB's rate dial is an ordinary dial rather than the LFORate type, but it indexes the same sweeps.
static const tLfoParams kLfoA    = {0, 7, 4, 6, -1, 5};
static const tLfoParams kLfoB    = {0, 2, 4, 8, -1, 7};
static const tLfoParams kLfoC    = {0, 3, -1, 2, -1, 4};
static const tLfoParams kLfoShpA = {0, 1, 11, 10, 5, 4};

#define CONST_PARAM_VALUE      (0)
#define CONST_PARAM_BIPOLAR    (1)    // 0 = unipolar 0..1, otherwise -1..+1

#define FXIN_PARAM_ACTIVE      (1)
#define FXIN_PARAM_PAD         (2)    // db12PadStrMap: +6 dB, 0 dB, -6 dB, -12 dB

// §9.1
#define MAX_NODE_INPUTS        (10)

// §9.3
#define NODE_OUTPUTS           (3)

// §9.2
#define MAX_NODE_LEVELS        (12)

// Which connector carries the signal into each module. Everything the walk follows is a module's
// FIRST input; LevMult and 2toOut take a second as well.
#define CONNECTOR_IN_A          (0)
#define CONNECTOR_IN_B          (1)
// notes §13
#define FLT_CONNECTOR_ENV_IN    (2)

// The G2 caps the total pitch modulation reaching an oscillator or filter at +/-64 semitones
// (manual p.78), which is what an Env amount of 100% corresponds to.
#define FULL_MOD_SEMITONES     (64.0)

// notes §14
#define PITCH_MOD_SEMITONES    (12.0)

// notes §15
static double type_ii_attenuator(double knob) {
    return knob * knob;
}

// Aftertouch's morph group. The G2 hard-wires the eight — morphStrMap lists them Wheel, Vel, Keyb,
// Aft.Tch, ... — so aftertouch is group 3. midiInput.c has the same constant for the same reason.
#define MORPH_GROUP_WHEEL         (0)
#define MORPH_GROUP_AFTERTOUCH    (3)

// patchModuleVibrato's Mod setting, in the order the patch-settings dropdown offers them.
typedef enum {
    eVibratoOff = 0,
    eVibratoAfterTouch,
    eVibratoWheel,
} tVibratoSource;

// Waveform menu order, matching shapeTypeStrMap in moduleResources.h.
typedef enum {
    eOscWaveSine = 0,
    eOscWaveTriangle,
    eOscWaveSaw,
    eOscWaveSquare,
    eOscWaveSuper,
} tOscWave;

// notes §16
#define OSCB_TUNE_UNITY       (64.0)
#define MIDI_NOTE_A440        (69.0)
#define MIDI_NOTE_MIDDLE_C    (60.0)

// notes §17
#define VOICE_GAIN            (0.15)

// Where the output starts bending rather than shearing.
#define OUTPUT_KNEE           (0.80)
#define ENVELOPE_SECONDS      (0.005)      // the anti-click ramp used when no EnvADSR is in the chain

// Every ladder runs its full four poles whatever slope is selected — see ladder_filter().
#define LADDER_POLES          (6)   // state available: FltLP's 36 dB setting is six poles
#define LADDER_LOOP_POLES     (4)   // the RESONANCE loop is four long whatever is tapped - measured

// notes §18
#define MAX_ENGINE_NODES      (28)

// notes §19
#define MAX_VOICES            (32)

// What counts as an inaudible voice, and how long it has to stay that way before the voice can be
// handed to another note. -80 dB is below anything that survives the output stage; the window is
// long enough that a waveform passing through zero cannot be mistaken for silence.
#define VOICE_SILENCE             (1.0e-4)
#define VOICE_SILENCE_SECONDS     (0.02)

// notes §20
#define VOICE_MAX_TAIL_SECONDS    (2.0)
#define VOICE_FADE_SECONDS        (0.03)

// Which of the seven transfer functions a shaper node carries. Stored rather than re-derived from
// the module type so the render loop never reaches back into the patch database.
typedef enum {
    eShaperClip = 0,
    eShaperOverdrive,
    eShaperSaturate,
    eShaperShpExp,
    eShaperWaveWrap,
    eShaperShpStatic,
    eShaperRect,
} tShaperKind;

typedef enum {
    eNodeOsc = 0,        // OscB
    eNodeOscShp,         // OscShpB — a different parameter layout and waveform set
    eNodeFilter,
    eNodeLevAmp,
    eNodeLevMult,
    eNodeMix,            // Mix4to1C
    eNodeEnv,            // envelope, and a VCA for whatever is patched into its audio input
    eNodeChorus,
    eNodeCompress,
    eNodeDelay,
    eNodeReverb,
    eNodeLfo,            // LfoA/B/C/ShpA — a control-rate source, full scale out
    eNodeConstant,
    eNodeFxIn,           // the FX area's feed from the Voice area — no cable, an implicit link
    eNodePassThru,       // an effect that is not modelled yet: passes its input along unchanged
    eNodePulse,          // a one-shot gate, fired by a rising edge at its input
    eNodeShaper,         // the whole Shaper group - a memoryless transfer function
    eNodeFade,           // Pan, X-Fade, Fade1-2, Fade2-1 - one position, two weights (fade_weights())
    eNodeMixStereo,      // MixStereo: six mono channels, each levelled and panned, to a stereo pair
    eNodeNoise,          // Noise: white noise through the Color dial's one-pole low-pass
    eNodeOscNoise,       // §8
    eNodeFltMulti,       // §10 - LP, BP and HP from one filter
    eNodeEq,             // §11 - EqPeak, Eq2Band, Eq3band
    eNodeOut,
} tNodeKind;

typedef struct {
    tNodeKind kind;
    uint32_t  moduleIndex;   // so per-node audio state can survive a knob turn (see topology_signature)
    uint32_t  location;      // Voice or FX — the two areas number their modules independently

    // notes §21
    int32_t   in[MAX_NODE_INPUTS];
    uint32_t  srcOut[MAX_NODE_INPUTS];
    uint32_t  srcLeg[MAX_NODE_INPUTS]; // which of the source's NODE_OUTPUTS legs that output is
    uint32_t  inCount;
    bool      active;                  // the module's own power button

    double    level[MAX_NODE_LEVELS];  // mixer channel levels, then 1.0 for the Chain input(s);
                                       // MixStereo: L and R gain of each channel, interleaved
    uint32_t  levelCount;              // how many of level[] are in use, and so smoothed
    bool      mixStereo;               // mixer: inputs are L/R pairs sharing a channel's level

    tOscWave  wave;                    // oscillator
    bool      oscKbt;
    double    basePitch;
    double    shape;
    double    rateHz;        // LFO speed
    uint32_t  polarity;      // LFO output range, posStrMap order
    bool      shpWave;       // LFO uses LfoShpA's waveform set rather than the plain one

    // The filter's Freq DIAL VALUE (0..127, fractional), not a frequency. Kept in dial units because
    // that is the domain modulation and keyboard tracking act in, and because the dial is itself
    // logarithmic in frequency — see filter_step().
    double          cutoffParam; // filter
    double          resonance;
    uint32_t        extraPoles;
    uint32_t        tapStage;  // which pole is tapped: 0-based, so N poles is tapStage N-1
    tFilterTopology topology;
    tFilterShape    fltShape;  // multi-mode filters only; low-pass for the rest
    double          fltGain;   // FltNord's GC attenuation; 1.0 for every other filter
    double          fltKbt;
    double          modAmount; // how far the Env input moves the cutoff, 0..2 (the dial's 0..200%)

    double          attack;    // envelope, in seconds
    double          decay;
    double          sustain;   // 0..1
    double          release;

    double          gain;         // LevAmp
    double          pulseSeconds; // Pulse gate width
    uint32_t        outDest;      // Out module: 0 = outputs 1/2, 1 = outputs 3/4

    double          depth;        // chorus detune depth, and the delay's feedback
    double          amount;       // chorus wet amount, delay/reverb dry-wet
    double          timeSeconds;  // delay time
    double          damping;      // delay LP / reverb brightness, 0..1
    double          hpCoeff;      // delay HP in the feedback loop; 0 = filter off
    double          threshold;    // compressor
    double          ratio;
    double          refLevel;     // the level the compressor drives TOWARDS - see compress_step()
    double          attackCoeff;
    double          releaseCoeff;
    // Shaper group. `shaperIn` is which input leg carries the signal rather than the modulation,
    // because WaveWrap puts its Mod jack FIRST and every other shaper puts it second.
    uint32_t        shaperKind;    // tShaperKind
    uint32_t        shaperCurve;   // the Type/Curve/Mode drop-down, raw - a mode carries no morph
    bool            shaperSym;     // Clip and Overdrive: Sym shapes both halves, Asym the positive one
    double          shaperAmount;  // the dial, 0..1
    double          shaperMod;     // the modulation attenuator, 0..1
    uint32_t        shaperIn;      // input leg carrying the signal; the other one is the modulation

    double          constant;      // Constant module's value
    // Fade family (§4); the position rides on the shape smoother.
    double          noisePole;     // Noise: the one-pole low-pass's feedback coefficient, from Color
    double          noiseGain;     // and the gain that keeps its level where the instrument's is
    double          oscNoiseWidth; // §8.3, as a dial fraction
    double          oscNoiseWidthMod;
    bool            fltSixDb;      // FltMulti dB/Oct: 0 is 6 dB
    bool            fltGainComp;   // FltMulti GComp
    double          eqInputLevel;  // §11
    double          eqLowHz;       // 0 = no low shelf
    double          eqLowGain;
    double          eqHighHz;      // 0 = no high shelf
    double          eqHighGain;
    double          eqPeakHz;      // 0 = no peak
    double          eqPeakDamping;
    double          eqPeakGain;
    uint32_t        fadeKind;      // tFadeKind
    double          fadeMod;       // the modulation attenuator, 0..1
    bool            fadeLog;       // logStrMap {Log, Lin}: 0 is Log. The two faders have no choice
    uint32_t        line;          // which shared delay line this node owns, if it needs one
    double          brightness;    // reverb, 0..1 as the dial reads it — HIGH IS BRIGHT
    double          timeNorm;      // reverb Time as the dial reads it, 0..1 — drives the diffusion
    uint32_t        reverbType;    // reverb room size: Small/Medium/Large/Hall

    // Evaluated ONCE per sample, after the voices are summed, rather than once per voice. True for
    // everything in the FX Area, for the three module kinds that own a shared delay buffer wherever
    // they sit, and for anything downstream of one of those. See mark_post_mix_nodes().
    bool postMix;
} tEngineNode;

// notes §22
#define MAX_ENGINE_TAPS    (4)

typedef struct {
    uint32_t    nodeCount;
    int32_t     tap;                           // the node whose output reaches the speakers, -1 for silence
    int32_t     extraTap[MAX_ENGINE_TAPS - 1]; // further Out modules, summed with `tap`
    uint32_t    extraTapCount;
    // Patch-wide settings, from the hidden modules in the Morph location rather than from any module
    // on the canvas. Vibrato is how a patch gets aftertouch vibrato with no LFO in it anywhere.
    uint32_t    vibratoSource; // 0 off, 1 aftertouch, 2 wheel
    double      vibratoCents;
    double      vibratoHz;
    tGlideMode  glideMode;     // patch-wide, not per node
    double      glideSeconds;
    double      bendSemitones; // 0 when the patch has bend switched off
    uint64_t    topology;      // changes shape => the audio thread resets its per-node state
    uint32_t    voiceCount;    // how many voices this patch may sound at once, 1 for Mono/Legato
    tEngineNode node[MAX_ENGINE_NODES];
} tSoundEngineParams;

// notes §23
#ifdef SYNTHLIB_PLUGIN_BUILD
#define SE          (tEngineIdx)
#define SE_LOCAL    const uint32_t tEngineIdx                                      = gDoc->engineIndex
#else
#define SE          (0u)
#define SE_LOCAL    (void)0
#endif

// WHICH SLOT THIS ENGINE PLAYS: -1 follows the document's selected slot, which is all there is today;
// a performance will bind one engine to each slot. See sound_engine_bind_slot().
static int32_t gPatchSlotBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = -1};
#define gPatchSlot    (gPatchSlotBank[SE])

// The reverb type each engine last laid its delay lines out for - see the reverb's reset.
static uint32_t                    sLastTypeBank[SOUND_ENGINE_MAX_ENGINES]         = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = UINT32_MAX};

static uint32_t engine_slot(void) {
    SE_LOCAL;

    return (gPatchSlot >= 0) ? (uint32_t)gPatchSlot : (uint32_t)gSlot;
}

static tSoundEngineParams          gParamsBank[SOUND_ENGINE_MAX_ENGINES];
#define gParams       (gParamsBank[SE])
static _Atomic uint32_t            gParamsSeqBank[SOUND_ENGINE_MAX_ENGINES];
#define gParamsSeq    (gParamsSeqBank[SE])

// notes §24
static pthread_mutex_t             gParamsWriteMutexBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = PTHREAD_MUTEX_INITIALIZER};
#define gParamsWriteMutex       (gParamsWriteMutexBank[SE])

#define PARAMS_READ_ATTEMPTS    (4)   // then keep last good — a retry loop must not spin in audio

// notes §25
#define NOTE_QUEUE_SIZE         (64)

typedef struct {
    int32_t          note;
    bool             on;
    _Atomic uint32_t sequence;   // claim index + 1 once written; 0 means never used
} tNoteEvent;

static tNoteEvent                  gNoteQueueBank[SOUND_ENGINE_MAX_ENGINES][NOTE_QUEUE_SIZE];
#define gNoteQueue    (gNoteQueueBank[SE])
static _Atomic uint32_t            gNoteWriteBank[SOUND_ENGINE_MAX_ENGINES];
#define gNoteWrite    (gNoteWriteBank[SE])
static uint32_t                    gNoteReadBank[SOUND_ENGINE_MAX_ENGINES]; // audio thread only
#define gNoteRead     (gNoteReadBank[SE])

static _Atomic bool                gActiveBank[SOUND_ENGINE_MAX_ENGINES];
#define gActive       (gActiveBank[SE])

// Morph positions, 0..1, one per group. Written by the MIDI thread as controllers move, read by the
// UI thread when it builds a snapshot. Plain atomics: each is independent and a torn read is not
// possible on a value this size.
static _Atomic uint32_t            gMorphMilliBank[SOUND_ENGINE_MAX_ENGINES][NUM_MORPHS];
#define gMorphMilli    (gMorphMilliBank[SE])
// The highest each morph has reached. The live value is useless as a diagnostic — by the time you
// have let go of the key and opened a menu to look at it, it has fallen back to zero.
static _Atomic uint32_t            gMorphPeakMilliBank[SOUND_ENGINE_MAX_ENGINES][NUM_MORPHS];
#define gMorphPeakMilli     (gMorphPeakMilliBank[SE])

// notes §26
#define METER_VALUE_MASK    (0xFFu)
#define METER_WRITTEN       (1u << 8)
#define METER_LEG_SHIFT     (16u)   // leg 1 above the flag; leg 0 occupies METER_VALUE_MASK

// notes §27
static _Atomic bool                gMetersDirtyBank[SOUND_ENGINE_MAX_ENGINES];
#define gMetersDirty    (gMetersDirtyBank[SE])
static _Atomic uint32_t            gModuleMeterBank[SOUND_ENGINE_MAX_ENGINES][locationMax][MAX_NUM_MODULES];
#define gModuleMeter    (gModuleMeterBank[SE])

// notes §28
static _Atomic uint32_t            gModuleLedBank[SOUND_ENGINE_MAX_ENGINES][locationMax][MAX_NUM_MODULES];
#define gModuleLed    (gModuleLedBank[SE])

// The follower behind the level meters. Per NODE, not per voice: the face has one meter however many
// voices are sounding, and only voice 0 writes it. About 200 ms of release at 96 kHz.
#define METER_DECAY         (0.00005)
static double                      gMeterEnvBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][2];
#define gMeterEnv           (gMeterEnvBank[SE])

static _Atomic int32_t             gOutputGainMilliBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 1000};
#define gOutputGainMilli    (gOutputGainMilliBank[SE])

static _Atomic int32_t             gBendMilliBank[SOUND_ENGINE_MAX_ENGINES];
#define gBendMilli          (gBendMilliBank[SE])

// Highest absolute sample the audio thread has produced since this was last read. Purely a
// diagnostic — it is what lets a test say "sound is coming out" without a pair of ears.
static _Atomic uint32_t            gPeakMilliBank[SOUND_ENGINE_MAX_ENGINES];
#define gPeakMilli    (gPeakMilliBank[SE])

// The peak BEFORE the output gain, so the real headroom a patch needs is visible rather than being
// hidden by whatever the guard clamped it to.
static _Atomic uint32_t            gRawPeakMilliBank[SOUND_ENGINE_MAX_ENGINES];
#define gRawPeakMilli    (gRawPeakMilliBank[SE])

// Why the engine is or is not making a sound. UI thread only — written while building the snapshot,
// read by the menu.
typedef enum {
    eStatusOff = 0,
    eStatusNoOutput,          // nothing selected, and no audible Out module to fall back to
    eStatusMultipleSelected,
    eStatusUnsupportedModule,
    eStatusNoSource,
    eStatusChainTooDeep,
    eStatusBypassed,
    eStatusPlaying,
} tSoundEngineStatus;

static tSoundEngineStatus          gStatusBank[SOUND_ENGINE_MAX_ENGINES]     = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = eStatusOff};
#define gStatus              (gStatusBank[SE])
static uint32_t                    gPlayingCountBank[SOUND_ENGINE_MAX_ENGINES]; // how many modules are in the rendered chain
#define gPlayingCount        (gPlayingCountBank[SE])

// notes §29
#define ENGINE_OVERSAMPLE    (2)

// The tempo a clock-synced module works to. The engine does not run the patch's master clock, so
// anything set to Clk needs a reference; 120 BPM is the obvious one and makes 1/4 exactly half a
// second. See the delay's Clk branch — this is a stand-in, not the hardware's tempo.
#define ENGINE_REFERENCE_BPM    (120.0)

static double                      gDeviceRateBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 48000.0};
#define gDeviceRate             (gDeviceRateBank[SE])
static double                      gSampleRateBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 96000.0};
#define gSampleRate             (gSampleRateBank[SE])

// notes §30
typedef struct {
    int32_t  note;         // MIDI note this voice holds, -1 for none
    bool     gate;         // key still down
    bool     sounding;     // rendered this block: gate open, or still releasing
    double   glidePitch;   // chases `note`; fractional, since a glide is mostly between two notes
    bool     glideActive;  // this note began while another was still held — see the Auto glide mode
    double   envelope;     // the anti-click ramp, used only when the patch has no EnvADSR
    uint64_t age;          // allocation order, so the oldest can be identified for stealing
    uint32_t quiet;        // consecutive samples this voice's output has been inaudible
    uint32_t released;     // samples since the key came up, 0 while it is held
    double   fade;         // 1.0 normally; driven to 0 to retire a voice that will not stop on its own
    uint32_t trigger;      // counts note-ons that restart the envelopes - see voice_note_on()
} tVoice;

static tVoice                      gVoiceBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES];
#define gVoice         (gVoiceBank[SE])
static uint64_t                    gVoiceClockBank[SOUND_ENGINE_MAX_ENGINES];
#define gVoiceClock    (gVoiceClockBank[SE])

// notes §31
static _Atomic uint32_t            gEngineVoicesBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 1};
#define gEngineVoices    (gEngineVoicesBank[SE])

// Whether the patch is in LEGATO voice mode, the one mode where a key played while another is held
// does not restart the envelopes. Published beside gEngineVoices for the same reason: it is read by
// voice_note_on() on the audio thread, per note, where copying the snapshot to ask would be absurd.
static _Atomic bool                gEngineLegatoBank[SOUND_ENGINE_MAX_ENGINES];
#define gEngineLegato    (gEngineLegatoBank[SE])

// notes §32
static _Atomic uint32_t            gLoadPercentBank[SOUND_ENGINE_MAX_ENGINES];
#define gLoadPercent    (gLoadPercentBank[SE])

static void reset_voices(void);
static uint32_t voice_count_for_patch(uint32_t slot);

static double                      gVibratoPhaseBank[SOUND_ENGINE_MAX_ENGINES];
#define gVibratoPhase        (gVibratoPhaseBank[SE])
static tSoundEngineParams          gLastGoodParamsBank[SOUND_ENGINE_MAX_ENGINES];
#define gLastGoodParams      (gLastGoodParamsBank[SE])
static uint64_t                    gSeenTopologyBank[SOUND_ENGINE_MAX_ENGINES];
#define gSeenTopology        (gSeenTopologyBank[SE])

// notes §33
#define OSC_OVERSAMPLE       (4 / ENGINE_OVERSAMPLE)
// notes §34
#define OSC_DECIMATE_TAPS    (48)

// The engine's own output filter, removing everything above the DEVICE's Nyquist before the extra
// samples are dropped. Same windowed-sinc design as the oscillators' — see the note there on why the
// transition width, not the oversampling factor, is what governs the result.
#define OUT_DECIMATE_TAPS    (64)

static double                      gOutDecimateBank[SOUND_ENGINE_MAX_ENGINES][OUT_DECIMATE_TAPS];
#define gOutDecimate         (gOutDecimateBank[SE])
static double                      gOutHistoryBank[SOUND_ENGINE_MAX_ENGINES][4][OUT_DECIMATE_TAPS]; // [pair*2 + channel]; one shared cursor, see the render loop
#define gOutHistory          (gOutHistoryBank[SE])
static uint32_t                    gOutHistoryPosBank[SOUND_ENGINE_MAX_ENGINES];
#define gOutHistoryPos       (gOutHistoryPosBank[SE])

static double                      gOscDecimateBank[SOUND_ENGINE_MAX_ENGINES][OSC_DECIMATE_TAPS];
#define gOscDecimate         (gOscDecimateBank[SE])

// notes §35
static float                       gOscHistoryBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][OSC_DECIMATE_TAPS];
#define gOscHistory       (gOscHistoryBank[SE])
static uint32_t                    gOscHistoryPosBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gOscHistoryPos    (gOscHistoryPosBank[SE])

static double                      gPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
// §7.1
static uint32_t                    gNoiseSeedBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gNoiseSeed       (gNoiseSeedBank[SE])
static double                      gNoiseLpBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gNoiseLp         (gNoiseLpBank[SE])
#define gPhase           (gPhaseBank[SE])
static double                      gLfoLastPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoLastPhase    (gLfoLastPhaseBank[SE])
static double                      gLfoTargetBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoTarget       (gLfoTargetBank[SE])
static double                      gLfoHeldBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoHeld         (gLfoHeldBank[SE])
static double                      gSuperPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][2];
#define gSuperPhase      (gSuperPhaseBank[SE])
static double                      gLadderBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][LADDER_POLES];
#define gLadder          (gLadderBank[SE])

// Delay memory. Held as float rather than double purely for size — half a second per line at any
// sensible rate, four lines, is enough for the delays a patch normally has and keeps this under a
// megabyte. Nodes beyond that many run dry rather than sharing a line and smearing into each other.
#define MAX_DELAY_LINES       (4)
// notes §36
#define DELAY_LINE_SAMPLES    (134400 * ENGINE_OVERSAMPLE)
static float                       gDelayLineBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES][DELAY_LINE_SAMPLES];
#define gDelayLine            (gDelayLineBank[SE])
static uint32_t                    gDelayWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];
#define gDelayWrite           (gDelayWriteBank[SE])
static double                      gDelayDampBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];
#define gDelayDamp            (gDelayDampBank[SE])
static double                      gDelayHpBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES]; // the HP's lowpass half; the filter is x - this
#define gDelayHp              (gDelayHpBank[SE])

// notes §37
#define CHORUS_PHASE0         (0.3836)             // chorus_triangle(0.3836) = +0.5343

#define CHORUS_SAMPLES        (2048 * ENGINE_OVERSAMPLE)
#define CHORUS_CHANNELS       (2)
static float                       gChorusLineBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][CHORUS_CHANNELS][CHORUS_SAMPLES];
#define gChorusLine           (gChorusLineBank[SE])
static uint32_t                    gChorusWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][CHORUS_CHANNELS];
#define gChorusWrite          (gChorusWriteBank[SE])
static double                      gChorusLfoBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gChorusLfo            (gChorusLfoBank[SE])

// Pulse: the countdown still to run, and the previous input, so a rising edge can be seen. Per voice,
// because the gate is fired by that voice's own envelope.
static uint32_t                    gPulseCountBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gPulseCount    (gPulseCountBank[SE])
static double                      gPulsePrevBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gPulsePrev     (gPulsePrevBank[SE])

// Compressor gain-reduction state, one per node.
static double                      gCompEnvBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gCompEnv               (gCompEnvBank[SE])

// notes §38
#define REVERB_COMBS           (16)
#define REVERB_ALLPASS         (3)

// notes §39
#define REVERB_MODE_TYPE       (0)
#define REVERB_TYPE_COUNT      (4)

// notes §40
#define REVERB_DAMP_MAX        (0.8370)
#define REVERB_BRIGHT_K        (57.799)
#define REVERB_DAMP_CEILING    (0.9000)
// notes §41

static const double                kReverbDecayBase[REVERB_TYPE_COUNT]  = {0.045, 0.29, 0.39, 0.32};
static const double                kReverbDecaySlope[REVERB_TYPE_COUNT] = {0.02238, 0.04094, 0.06082, 0.08212};

// notes §42
#define REVERB_DIFFUSE_SLOPE    (0.75)
#define REVERB_DIFFUSE_BASE     (0.40)
#define REVERB_DIFFUSE_MIN      (0.45)
#define REVERB_DIFFUSE_MAX      (0.62)

// notes §43
static const double                kReverbTypeScale[REVERB_TYPE_COUNT]  = {1.0, 1.2690, 1.5255, 1.6795};
#define REVERB_SCALE_MAX           (1.6795)

// notes §44
#define REVERB_COMB_BASE           (1667 * ENGINE_OVERSAMPLE) // the LONGEST comb of EITHER channel; buffers are sized from it
#define REVERB_ALLPASS_BASE        (225 * ENGINE_OVERSAMPLE)
// notes §45
#define REVERB_SPREAD              (110)
#define REVERB_CHANNELS            (2)

#define REVERB_COMB_MAX            (((REVERB_COMB_BASE * 18) / 10) + REVERB_SPREAD + 1)
#define REVERB_ALLPASS_MAX         (((REVERB_ALLPASS_BASE * 18) / 10) + REVERB_SPREAD + 1)

// notes §46
#define REVERB_PREDELAY_MAXSAMP    (641 * ENGINE_OVERSAMPLE)   // the largest below, Hall left
#define REVERB_PREDELAY_MAX        (((REVERB_PREDELAY_MAXSAMP * 11) / 10) + 1)
// notes §47
#define RV_OUTTAPS                 (7)
// THE ALLPASS COEFFICIENTS ARE THE INSTRUMENT'S, read straight out of its mixing gains: the two it
// pairs 0.4820 with 0.7676 and 0.3102 with 0.9038, and 1 - g*g for those g values is exactly those
// two numbers. The input diffuser's own gains come out heavier, at 0.75/0.5.
#define RV_DIFFUSE    (0.5000)
#define RV_TANK_A     (0.4820)
#define RV_TANK_B     (0.3102)

// Sixteen taps summed with alternating signs add up like a random walk, so the sum grows as the
// square root of the count and this is 1/sqrt(16). REVERB_WET_GAIN sets the level; this only keeps
// the tap count from changing it.
#define RV_TAP_SCALE    (0.37796447300922720)

// 1/sqrt(8) -- what makes the 8-point Hadamard butterfly orthogonal rather than a gain of 8.
#define RV_HADAMARD     (0.35355339059327373)

// notes §48

// notes §49
typedef enum {
    eRvPre = 0,
    eRvDf1,
    eRvDf2,
    eRvDf3,
    eRvDf4,
    eRvDf5,
    eRvDf6,
    eRvLn0,
    eRvLn1,
    eRvLn2,
    eRvLn3,
    eRvLn4,
    eRvLn5,
    eRvLn6,
    eRvLn7,
    eRvSpanCount
} tRvSpan;

#define RV_LINES        (8)

// notes §50
#define RV_MOD_DEPTH    (28)

// notes §51
#define RV_MOD_LOSS     (1.0000)

static const double   kRvModHz[RV_LINES]                      = {
    0.61, 0.73, 0.89, 1.03, 1.19, 1.31, 1.47, 1.61
};

static double         gRvLfoBank[SOUND_ENGINE_MAX_ENGINES][RV_LINES];
#define gRvLfo          (gRvLfoBank[SE])
#define RV_DIFFUSERS    (6)

// notes §52
static const uint32_t kRvLen[eRvSpanCount]                    = {
    1060,                            // pre-delay
    43,     73,  107, 145, 277, 389, // the input diffuser
    661,   673,  739, 811,           // the four short lines
    2297, 2459, 4001, 5323           // and the four long ones
};

static const uint32_t kRvDiffuser[RV_DIFFUSERS]               = {
    eRvDf1, eRvDf2, eRvDf3, eRvDf4, eRvDf5, eRvDf6
};
static const uint32_t kRvLineDl[RV_LINES]                     = {
    eRvLn0, eRvLn1, eRvLn2, eRvLn3, eRvLn4, eRvLn5, eRvLn6, eRvLn7
};

// notes §53
static const uint32_t kRvTapLine[REVERB_CHANNELS][RV_OUTTAPS] = {
    {eRvLn4, eRvLn4, eRvLn5, eRvLn6, eRvLn6, eRvLn7, eRvLn7},     // left
    {eRvLn4, eRvLn4, eRvLn5, eRvLn6, eRvLn6, eRvLn7, eRvLn7}      // right
};
static const double   kRvTapFrac[REVERB_CHANNELS][RV_OUTTAPS] = {
    {   // left  — earliest is 0.1301 on Ln4 = 299 samples, which is what kRvTankLead measures
        0.1301, 0.6935, 0.6339, 0.5312, 0.8379, 0.4515, 0.8550
    },
    {   // right — earliest is 0.0823 on Ln4 = 189 samples, 110 ahead of the left set
        0.0823, 0.3000, 0.2000, 0.1200, 0.4200, 0.1000, 0.4963
    }
};
static const double   kRvTapSign[RV_OUTTAPS]                  = {1.0, -1.0, 1.0, 1.0, -1.0, -1.0, 1.0};

// notes §54

static uint32_t       gRvAddrBank[SOUND_ENGINE_MAX_ENGINES][eRvSpanCount + 1];
#define gRvAddr    (gRvAddrBank[SE])

// notes §55
#define RV_RATE    (gSampleRate / 96000.0)

// notes §56


// notes §57
#define RV_MEM_SHIFT    (17)
#define RV_MEM          (1u << RV_MEM_SHIFT)

static float          gRvMemBank[SOUND_ENGINE_MAX_ENGINES][RV_MEM];
#define gRvMem          (gRvMemBank[SE])
static uint32_t       gRvCurBank[SOUND_ENGINE_MAX_ENGINES];
#define gRvCur          (gRvCurBank[SE])
static double         gRvDampBank[SOUND_ENGINE_MAX_ENGINES][RV_LINES];
#define gRvDamp         (gRvDampBank[SE])


// The two input poles. MEASURED, not chosen: the instrument's reverb is far darker than what goes
// into it, and this is the filter that makes it so -- see the fit by REVERB_INPUT_LP_HZ.
static double         gRevInLpBank[SOUND_ENGINE_MAX_ENGINES];
#define gRevInLp     (gRevInLpBank[SE])
static double         gRevInLp2Bank[SOUND_ENGINE_MAX_ENGINES];
#define gRevInLp2    (gRevInLp2Bank[SE])
static double         gRevInLp3Bank[SOUND_ENGINE_MAX_ENGINES];
#define gRevInLp3    (gRevInLp3Bank[SE])
static double         gRevInLp4Bank[SOUND_ENGINE_MAX_ENGINES];
#define gRevInLp4    (gRevInLp4Bank[SE])
static double         gRvLoopBank[SOUND_ENGINE_MAX_ENGINES][RV_LINES];
#define gRvLoop      (gRvLoopBank[SE])

// [room type][channel], in samples at the base rate. NOT scaled by kReverbTypeScale — see above.
static const uint32_t kReverbPreDelay[REVERB_TYPE_COUNT][REVERB_CHANNELS] = {
    {619 * ENGINE_OVERSAMPLE, 564 * ENGINE_OVERSAMPLE},    // Small   12.89 / 11.75 ms
    {626 * ENGINE_OVERSAMPLE, 571 * ENGINE_OVERSAMPLE},    // Medium  13.05 / 11.90 ms
    {638 * ENGINE_OVERSAMPLE, 583 * ENGINE_OVERSAMPLE},    // Large   13.30 / 12.14 ms
    {641 * ENGINE_OVERSAMPLE, 586 * ENGINE_OVERSAMPLE}     // Hall    13.36 / 12.20 ms
};
// notes §58
#define REVERB_INPUT_LP_HZ      (4600.0)
#define REVERB_INPUT_LP2_HZ     (4600.0)

// notes §59
#define REVERB_INPUT_LP_TIME    (0.7)

// notes §60
#define REVERB_INPUT_LP4_HZ     (12000.0)
static float          gPreDelayBank[SOUND_ENGINE_MAX_ENGINES][REVERB_CHANNELS][REVERB_PREDELAY_MAX];
#define gPreDelay               (gPreDelayBank[SE])
static uint32_t       gPreDelayPosBank[SOUND_ENGINE_MAX_ENGINES][REVERB_CHANNELS];
#define gPreDelayPos            (gPreDelayPosBank[SE])
static double         gEnvLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvLevel               (gEnvLevelBank[SE])
// notes §61
#define PARAM_SMOOTH_SECONDS    (0.008)

static double         gSmoothShapeBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothShape            (gSmoothShapeBank[SE])
static double         gSmoothCutoffBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothCutoff           (gSmoothCutoffBank[SE])
static double         gSmoothResBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothRes              (gSmoothResBank[SE])
static double         gSmoothGainBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothGain             (gSmoothGainBank[SE])
static double         gSmoothLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][MAX_NODE_LEVELS];
#define gSmoothLevel            (gSmoothLevelBank[SE])

// Where the per-sample smoothing pass leaves its results, for the voice passes to read. Not per
// voice: a knob is in one place however many notes are sounding, and smoothing it inside the voice
// loop would advance the filter once per voice — so a sweep would speed up as more keys went down.
static double         gSmoothedShapeBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedShape     (gSmoothedShapeBank[SE])
static double         gSmoothedCutoffBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedCutoff    (gSmoothedCutoffBank[SE])
static double         gSmoothedResBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedRes       (gSmoothedResBank[SE])
static double         gSmoothedGainBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedGain      (gSmoothedGainBank[SE])
static double         gSmoothedLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][MAX_NODE_LEVELS];
#define gSmoothedLevel     (gSmoothedLevelBank[SE])
// Until a node has been seen once there is nothing to interpolate FROM, so the first sample snaps.
// Also what stops a patch load sweeping every parameter up from whatever the last patch left.
static bool           gSmoothPrimedBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothPrimed    (gSmoothPrimedBank[SE])

static double         gEnvProgressBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvProgress     (gEnvProgressBank[SE])
static double         gEnvStartBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvStart        (gEnvStartBank[SE])
static uint32_t       gEnvStageBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvStage        (gEnvStageBank[SE])

// The voice's trigger count this envelope last started an attack for. When the voice's count moves
// past it, a note-on has asked for a restart that the gate alone cannot show - see envelope_step().
static uint32_t       gEnvTriggerBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvTrigger    (gEnvTriggerBank[SE])

typedef enum {
    eEnvIdle = 0,
    eEnvAttack,
    eEnvDecay,
    eEnvSustain,
    eEnvRelease,
} tEnvStage;

void sound_engine_pitch_bend(double bend) {
    SE_LOCAL;

    if (bend < -1.0) {
        bend = -1.0;
    } else if (bend > 1.0) {
        bend = 1.0;
    }
    atomic_store(&gBendMilli, (int32_t)(bend * 1000.0));
}

void sound_engine_set_output_level_db(double db) {
    SE_LOCAL;

    double gain = pow(10.0, db / 20.0);

    if (db >= 0.0) {
        gain = 1.0;    // attenuation only: this is a trim, not a boost into the limiter
    }
    atomic_store(&gOutputGainMilli, (int32_t)((gain * 1000.0) + 0.5));
}

bool sound_engine_set_morph(uint32_t group, double amount) {
    SE_LOCAL;

    uint32_t scaled = 0;

    if (group >= NUM_MORPHS) {
        return false;
    }

    if (amount < 0.0) {
        amount = 0.0;
    } else if (amount > 1.0) {
        amount = 1.0;
    }
    scaled = (uint32_t)(amount * 1000.0);

    // A controller repeating its current value is common — a wheel resting at zero, a sustain pedal
    // held down — and each one would otherwise cost a redraw of the whole patch.
    return atomic_exchange(&gMorphMilli[group], scaled) != scaled;
}

// notes §62
static double glide_time_seconds(double setting) {
    const char * lowText  = NULL;
    const char * highText = NULL;
    double       fraction = 0.0;
    double       low      = 0.0;
    double       high     = 0.0;
    int          index    = 0;

    if (setting < 0.0) {
        setting = 0.0;
    } else if (setting > 127.0) {
        setting = 127.0;
    }
    index    = (int)setting;
    fraction = setting - (double)index;
    lowText  = get_glide_time_str((uint8_t)index);
    highText = get_glide_time_str((uint8_t)((index < 127) ? (index + 1) : 127));

    if ((lowText == NULL) || (highText == NULL)) {
        return 0.019;
    }
    low      = (double)atoi(lowText) / 1000.0;
    high     = (double)atoi(highText) / 1000.0;
    return low + ((high - low) * fraction);
}

// A parameter's value with every morph applied. morphRange is a SIGNED 8-bit offset from the dialled
// value — under 128 it is positive, at or above it is that value minus 256 — so a morph sweeps the
// parameter from where the knob sits towards its morph target as the controller moves.
static double param_value(tModule * module, uint32_t variation, uint32_t index) {
    SE_LOCAL;

    const tParam * param = &module->param[variation][index];
    double         value = (double)param->value;
    uint32_t       group = 0;

    for (group = 0; group < NUM_MORPHS; group++) {
        uint32_t raw = param->morphRange[group];

        if (raw == 0) {
            continue;
        }
        {
            int32_t offset = (raw < 128) ? (int32_t)raw : ((int32_t)raw - 256);
            double  amount = (double)atomic_load(&gMorphMilli[group]) / 1000.0;

            value += (double)offset * amount;
        }
    }

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return value;
}

bool sound_engine_active(void) {
    SE_LOCAL;

    return atomic_load(&gActive);
}

// The device's rate; the ENGINE runs at ENGINE_OVERSAMPLE times this. gSampleRate is the internal
// rate, so every coefficient already derived from it — envelope and glide times, filter and chorus
// coefficients, LFO and oscillator increments — scales with no further change.
void sound_engine_set_sample_rate(double sampleRate) {
    SE_LOCAL;

    if (sampleRate > 0.0) {
        gDeviceRate = sampleRate;
        gSampleRate = sampleRate * (double)ENGINE_OVERSAMPLE;
    }
}

static void reset_node_state(void) {
    SE_LOCAL;

    uint32_t i = 0;
    uint32_t v = 0;

    for (v = 0; v < MAX_VOICES; v++) {
        for (i = 0; i < MAX_ENGINE_NODES; i++) {
            gLfoLastPhase[v][i]  = 0.0;
            gLfoTarget[v][i]     = 0.0;
            gLfoHeld[v][i]       = 0.0;
            gOscHistoryPos[v][i] = 0;
            memset(gOscHistory[v][i], 0, sizeof(gOscHistory[v][i]));

            // notes §63
            gPhase[v][i]         = fmod(((double)i + ((double)v * 0.618034)) * 0.381966, 1.0);
            gSuperPhase[v][i][0] = 0.0;
            gSuperPhase[v][i][1] = 0.0;
            gNoiseSeed[v][i]     = 0x9E3779B9u ^ ((v + 1u) * 0x85EBCA6Bu) ^ ((i + 1u) * 0xC2B2AE35u);
            gNoiseLp[v][i]       = 0.0;
            gLadder[v][i][0]     = 0.0;
            gLadder[v][i][1]     = 0.0;
            gLadder[v][i][2]     = 0.0;
            gLadder[v][i][3]     = 0.0;
            gLadder[v][i][4]     = 0.0;
            gLadder[v][i][5]     = 0.0;
            gEnvLevel[v][i]      = 0.0;
            gEnvProgress[v][i]   = 0.0;
            gEnvStart[v][i]      = 0.0;
            gEnvStage[v][i]      = eEnvIdle;
            gEnvTrigger[v][i]    = gVoice[v].trigger;   // nothing pending: idle already attacks on a gate
            gCompEnv[v][i]       = 0.0;
            gPulseCount[v][i]    = 0;
            gPulsePrev[v][i]     = 0.0;
        }
    }

    for (i = 0; i < MAX_ENGINE_NODES; i++) {
        gSmoothPrimed[i]   = false;
        gChorusWrite[i][0] = 0;
        gChorusWrite[i][1] = 0;
        gChorusLfo[i]      = CHORUS_PHASE0;
        memset(gChorusLine[i], 0, sizeof(gChorusLine[i]));
    }

    memset(gDelayLine, 0, sizeof(gDelayLine));
    memset(gDelayWrite, 0, sizeof(gDelayWrite));
    memset(gDelayDamp, 0, sizeof(gDelayDamp));
    memset(gDelayHp, 0, sizeof(gDelayHp));
    memset(gPreDelay, 0, sizeof(gPreDelay));
    memset(gRvMem, 0, sizeof(gRvMem));
    gRvCur = 0;
    memset((void *)gModuleMeter, 0, sizeof(gModuleMeter));   // no stale meters after a stop or reload
    memset((void *)gModuleLed, 0, sizeof(gModuleLed));
    memset(gMeterEnv, 0, sizeof(gMeterEnv));
    memset(gRvDamp, 0, sizeof(gRvDamp));
    memset(gRvLoop, 0, sizeof(gRvLoop));
    memset(gPreDelayPos, 0, sizeof(gPreDelayPos));
}

// notes §64
static void build_decimator(void) {
    SE_LOCAL;

    double   cutoff = 0.45 / (double)OSC_OVERSAMPLE;    // as a fraction of the oversampled rate
    double   sum    = 0.0;
    uint32_t i      = 0;

    for (i = 0; i < OSC_DECIMATE_TAPS; i++) {
        double offset = (double)i - ((double)(OSC_DECIMATE_TAPS - 1) / 2.0);
        double sinc   = (fabs(offset) < 1e-9)
                        ? (2.0 * cutoff)
                        : (sin(2.0 * M_PI * cutoff * offset) / (M_PI * offset));
        double window = 0.42
                        - (0.50 * cos((2.0 * M_PI * (double)i) / (double)(OSC_DECIMATE_TAPS - 1)))
                        + (0.08 * cos((4.0 * M_PI * (double)i) / (double)(OSC_DECIMATE_TAPS - 1)));

        gOscDecimate[i] = sinc * window;
        sum            += gOscDecimate[i];
    }

    // Normalise to unity gain at DC, so oversampling does not change the level.
    for (i = 0; i < OSC_DECIMATE_TAPS; i++) {
        gOscDecimate[i] /= sum;
    }

    cutoff         = 0.45 / (double)ENGINE_OVERSAMPLE;
    sum            = 0.0;

    for (i = 0; i < OUT_DECIMATE_TAPS; i++) {
        double offset = (double)i - ((double)(OUT_DECIMATE_TAPS - 1) / 2.0);
        double sinc   = (fabs(offset) < 1e-9)
                        ? (2.0 * cutoff)
                        : (sin(2.0 * M_PI * cutoff * offset) / (M_PI * offset));
        double window = 0.42
                        - (0.50 * cos((2.0 * M_PI * (double)i) / (double)(OUT_DECIMATE_TAPS - 1)))
                        + (0.08 * cos((4.0 * M_PI * (double)i) / (double)(OUT_DECIMATE_TAPS - 1)));

        gOutDecimate[i] = sinc * window;
        sum            += gOutDecimate[i];
    }

    for (i = 0; i < OUT_DECIMATE_TAPS; i++) {
        gOutDecimate[i] /= sum;
    }

    memset(gOutHistory, 0, sizeof(gOutHistory));
    gOutHistoryPos = 0;
}

// Everything sound_engine_start() does APART from opening an audio device. Split out so a host that
// owns the device already — the VST3 wrapper, which is handed a buffer to fill rather than asking
// CoreAudio for one — can prepare the engine without audioOutput.c being involved at all.
static void engine_prime(void) {
    SE_LOCAL;

    build_decimator();
    // notes §65
    gNoteRead = atomic_load(&gNoteWrite);
    reset_node_state();
    reset_voices();
}

// For a plug-in host: prime the engine and mark it live, but leave the audio device alone. The
// caller drives sound_engine_render() from its own process callback.
void sound_engine_start_hosted(double sampleRate) {
    SE_LOCAL;

    sound_engine_set_sample_rate(sampleRate);
    engine_prime();
    atomic_store(&gActive, true);
}

void sound_engine_stop_hosted(void) {
    SE_LOCAL;

    atomic_store(&gActive, false);
}

bool sound_engine_start(void) {
    SE_LOCAL;

    if (atomic_load(&gActive) == true) {
        return true;
    }
    build_decimator();
    // notes §66
    gNoteRead = atomic_load(&gNoteWrite);
    reset_node_state();
    reset_voices();

    if (audio_output_start() == false) {
        return false;
    }
    atomic_store(&gActive, true);

    return true;
}

void sound_engine_stop(void) {
    SE_LOCAL;

    if (atomic_load(&gActive) == false) {
        return;
    }
    // Clear the flag first: the device teardown below waits for any render in flight to finish, and
    // that render should already be seeing an inactive engine.
    atomic_store(&gActive, false);
    audio_output_stop();
}

const char * sound_engine_status_text(void) {
    SE_LOCAL;

    static char text[80];

    if (atomic_load(&gActive) == false) {
        return "Off";
    }

    switch (gStatus) {
        case eStatusNoOutput:
        {
            return "Select a module, or patch something into an Out";
        }
        case eStatusMultipleSelected:
        {
            return "Select one module only";
        }
        case eStatusUnsupportedModule:
        {
            return "That module is not implemented yet";
        }
        case eStatusNoSource:
        {
            return "Nothing is patched into it";
        }
        case eStatusChainTooDeep:
        {
            return "Chain too long, or it loops back on itself";
        }
        case eStatusBypassed:
        {
            return "That module is switched off";
        }
        case eStatusPlaying:
        {
            // The voice figures are what say whether a chord is being cut short: sounding against
            // allowed, the second being the patch's own Poly count.
            snprintf(text, sizeof(text), "Playing %u module%s, %u/%u voices, load %u%%%s",
                     (unsigned)gPlayingCount, (gPlayingCount == 1) ? "" : "s",
                     (unsigned)sound_engine_voices_sounding(), (unsigned)sound_engine_voice_count(),
                     (unsigned)sound_engine_load_percent(),
                     (midi_input_connected_count() > 0) ? " - MIDI in" : " - Virtual Keyboard");
            return text;
        }
        default:
        {
            return "Off";
        }
    }
}

// notes §67
const char * sound_engine_modulation_text(void) {
    SE_LOCAL;

    static char text[160];
    char        vib[48] = {0};
    uint32_t    lfos    = 0;
    uint32_t    i       = 0;

    // gParams is the UI thread's own copy, the same one sound_engine_debug_text() reads.
    for (i = 0; i < gParams.nodeCount; i++) {
        if (gParams.node[i].kind == eNodeLfo) {
            lfos++;
        }
    }

    {
        static const char * source[] = {"Off", "AfTouch", "Wheel"};
        uint32_t            mod      = (gParams.vibratoSource < 3) ? gParams.vibratoSource : 0;

        snprintf(vib, sizeof(vib), "Vib %s %ucnt %.1fHz", source[mod],
                 (unsigned)gParams.vibratoCents, gParams.vibratoHz);
    }

    snprintf(text, sizeof(text), "Aftertouch %u msg, morph %u%% peak %u%%, %s, %u LFO of %u nodes",
             (unsigned)midi_input_pressure_count(),
             (unsigned)((atomic_load(&gMorphMilli[MORPH_GROUP_AFTERTOUCH]) + 5) / 10),
             (unsigned)((atomic_load(&gMorphPeakMilli[MORPH_GROUP_AFTERTOUCH]) + 5) / 10),
             vib, (unsigned)lfos, (unsigned)gParams.nodeCount);
    return text;
}

const char * sound_engine_debug_text(void) {
    SE_LOCAL;

    // Big enough for a full patch: a couple of dozen nodes at roughly 230 characters each. It was
    // 1024, which silently cut the listing off after five nodes.
    static char  text[8192];
    size_t       used       = 0;
    uint32_t     i          = 0;
    // One entry per tNodeKind, in enum order. Kept in step with it — a short array here is read off
    // the end by the kindName[n->kind] below, which is a stack overflow rather than a wrong label.
    const char * kindName[] = {
        "Osc",    "OscShp",   "Filter", "LevAmp", "LevMult", "Mix",   "Env",
        "Chorus", "Compress", "Delay",  "Reverb", "Lfo",     "Const", "FxIn","PassThru","Pulse", "Out"
    };

    used += (size_t)snprintf(text + used, sizeof(text) - used,
                             "active=%d status=%d nodes=%u tap=%d extraTaps=%u variation=%u peak=%.3f rawpeak=%.3f\n",
                             (int)atomic_load(&gActive), (int)gStatus, (unsigned)gParams.nodeCount,
                             (int)gParams.tap, (unsigned)gParams.extraTapCount,
                             (unsigned)gPatchDescr[engine_slot()].activeVariation,
                             (double)atomic_exchange(&gPeakMilli, 0) / 1000.0,
                             (double)atomic_exchange(&gRawPeakMilli, 0) / 1000.0);

    for (i = 0; (i < gParams.nodeCount) && (used < sizeof(text)); i++) {
        const tEngineNode * n = &gParams.node[i];

        used += (size_t)snprintf(text + used, sizeof(text) - used,
                                 "[%u] %-8s mod=%u n=%u in=%d/%d src=%u/%u active=%d "
                                 "wave=%d kbt=%d pitch=%.2f shape=%.2f "
                                 "cut=%.1f res=%.2f poles=%u env=%.2f fltkbt=%.2f "
                                 "a=%.3f d=%.3f s=%.2f r=%.3f gain=%.2f time=%.3f mix=%.2f fb=%.2f\n",
                                 (unsigned)i,
                                 (n->kind < (sizeof(kindName) / sizeof(kindName[0]))) ? kindName[n->kind] : "?",
                                 (unsigned)n->moduleIndex,
                                 (unsigned)n->inCount,
                                 (int)n->in[0], (int)n->in[1],
                                 (unsigned)n->srcOut[0], (unsigned)n->srcOut[1],
                                 (int)n->active,
                                 (int)n->wave, (int)n->oscKbt, n->basePitch, n->shape,
                                 flt_cutoff_hz(n->cutoffParam), n->resonance, (unsigned)n->extraPoles, n->modAmount, n->fltKbt,
                                 n->attack, n->decay, n->sustain, n->release, n->gain,
                                 n->timeSeconds, n->amount, n->depth);
    }

    return text;
}

void sound_engine_note(int32_t note, bool on) {
    SE_LOCAL;

    uint32_t claim = atomic_fetch_add(&gNoteWrite, 1);
    uint32_t slot  = claim % NOTE_QUEUE_SIZE;

    gNoteQueue[slot].note = note;
    gNoteQueue[slot].on   = on;

    // Published last: the consumer treats a slot as filled only once this matches.
    atomic_store(&gNoteQueue[slot].sequence, claim + 1);
}

// ── VOICE ALLOCATION (audio thread) ─────────────────────────────────────────────────────────────

static void reset_voices(void) {
    SE_LOCAL;

    uint32_t v = 0;

    for (v = 0; v < MAX_VOICES; v++) {
        gVoice[v].note        = -1;
        gVoice[v].gate        = false;
        gVoice[v].sounding    = false;
        gVoice[v].glidePitch  = -1.0;
        gVoice[v].glideActive = false;
        gVoice[v].envelope    = 0.0;
        gVoice[v].age         = 0;
        gVoice[v].quiet       = 0;
        gVoice[v].released    = 0;
        gVoice[v].fade        = 1.0;
    }

    gVoiceClock = 0;
}

// notes §68
static uint32_t voice_count_for_patch(uint32_t slot) {
    uint32_t count = 1;

    if (gPatchDescr[slot].monoPoly == monoPolyPoly) {
        count = (uint32_t)gPatchDescr[slot].voiceCount + 1;
    }

    if (count < 1) {
        count = 1;
    }
    return (count > MAX_VOICES) ? MAX_VOICES : count;
}

// The voice already holding a note, or -1. Matched whether or not the key is still down: a repeated
// note-on for something still releasing belongs on the voice that is releasing it, or the release
// carries on underneath the new note as a duplicate.
static int32_t voice_holding_note(int32_t note, uint32_t count) {
    SE_LOCAL;

    for (uint32_t v = 0; v < count; v++) {
        if ((gVoice[v].note == note) && (gVoice[v].sounding || gVoice[v].gate)) {
            return (int32_t)v;
        }
    }

    return -1;
}

// notes §69
static uint32_t voice_to_allocate(uint32_t count) {
    SE_LOCAL;

    uint32_t best    = 0;
    uint64_t bestAge = UINT64_MAX;

    for (uint32_t v = 0; v < count; v++) {
        if ((gVoice[v].sounding == false) && (gVoice[v].gate == false)) {
            return v;
        }
    }

    for (uint32_t v = 0; v < count; v++) {   // released but still ringing: the oldest of them
        if ((gVoice[v].gate == false) && (gVoice[v].age < bestAge)) {
            bestAge = gVoice[v].age;
            best    = v;
        }
    }

    if (bestAge != UINT64_MAX) {
        return best;
    }

    for (uint32_t v = 0; v < count; v++) {   // everything is held: steal the oldest
        if (gVoice[v].age < bestAge) {
            bestAge = gVoice[v].age;
            best    = v;
        }
    }

    return best;
}

static void voice_note_on(int32_t note) {
    SE_LOCAL;

    uint32_t count = atomic_load(&gEngineVoices);

    // Bounded BEFORE it is used to pick a voice, not after. A published count is already clamped,
    // but a zero would send voice_to_allocate() round an empty loop and every note would land on
    // voice 0 — one note at a time, silently, with no obvious cause.
    if (count < 1) {
        count = 1;
    } else if (count > MAX_VOICES) {
        count = MAX_VOICES;
    }
    int32_t  held  = voice_holding_note(note, count);
    uint32_t v     = (held >= 0) ? (uint32_t)held : voice_to_allocate(count);
    tVoice * voice = &gVoice[v];
    // notes §70
    voice->glideActive = voice->gate;

    // notes §71
    if ((voice->gate == false) || (atomic_load(&gEngineLegato) == false)) {
        voice->trigger++;
    }

    if (voice->glidePitch < 0.0) {
        voice->glidePitch = (double)note;   // first note this voice has had: start where it is played
    }
    voice->note        = note;
    voice->gate        = true;
    voice->sounding    = true;
    voice->released    = 0;
    voice->fade        = 1.0;   // a stolen voice may have been fading; this note cancels that
    voice->age         = ++gVoiceClock;
}

// A note-off names its note; -1 is all-notes-off. Only the gate closes — the voice keeps its note
// and goes on sounding its release, at the pitch it was played at.
static void voice_note_off(int32_t note) {
    SE_LOCAL;

    for (uint32_t v = 0; v < MAX_VOICES; v++) {
        if ((note < 0) || (gVoice[v].note == note)) {
            gVoice[v].gate = false;
        }
    }
}

// Peak render load since the last read, as a percentage of real time. READING IT CLEARS IT, so the
// figure is always "the worst buffer since you last looked".
uint32_t sound_engine_load_percent(void) {
    SE_LOCAL;

    return atomic_exchange(&gLoadPercent, 0);
}

bool sound_engine_is_polyphonic(void) {
    SE_LOCAL;

    return atomic_load(&gEngineVoices) > 1;
}

uint32_t sound_engine_voice_count(void) {
    SE_LOCAL;

    return atomic_load(&gEngineVoices);
}

// Read without a lock from whichever thread asks. It is a display figure that changes every time a
// key moves, so a torn read is one frame of a number that is about to change anyway.
uint32_t sound_engine_voices_sounding(void) {
    SE_LOCAL;

    uint32_t count  = 0;
    uint32_t voices = atomic_load(&gEngineVoices);

    // Only the voices this patch may use. Lowering a patch's voice count can leave a higher voice
    // flagged as sounding when it is no longer rendered or allocated; counting those would report
    // more voices in use than the engine is actually running.
    if (voices > MAX_VOICES) {
        voices = MAX_VOICES;
    }

    for (uint32_t v = 0; v < voices; v++) {
        if (gVoice[v].sounding == true) {
            count++;
        }
    }

    return count;
}

// Audio thread. Applies the next queued event if there is one, returning false when the queue is
// empty. Called per sample, so a note lands on the sample it arrived rather than at the next buffer
// boundary.
static bool take_next_note_event(void) {
    SE_LOCAL;

    uint32_t write = atomic_load(&gNoteWrite);
    uint32_t slot  = 0;

    if (gNoteRead >= write) {
        return false;
    }

    // notes §72
    if ((write - gNoteRead) > NOTE_QUEUE_SIZE) {
        gNoteRead = write - NOTE_QUEUE_SIZE;
    }
    slot = gNoteRead % NOTE_QUEUE_SIZE;

    if (atomic_load(&gNoteQueue[slot].sequence) != (gNoteRead + 1)) {
        return false;   // claimed but not yet written; it will be there next time round
    }

    if ((gNoteQueue[slot].on == true) && (gNoteQueue[slot].note >= 0)) {
        voice_note_on(gNoteQueue[slot].note);
    } else {
        voice_note_off(gNoteQueue[slot].note);
    }
    gNoteRead++;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Building the chain (UI thread)
// ---------------------------------------------------------------------------------------------

// notes §73
static double pulse_time_seconds(double value, uint32_t range) {
    // ln(width in 96 kHz samples) = k0 + k1*d + k2*d^2 + k3*d^3
    const double k0      = 2.11883047;
    const double k1      = 0.07714113828;
    const double k2      = -0.0000864025056;
    const double k3      = 0.0000004707716063;
    double       samples = exp(k0 + (k1 * value) + (k2 * value * value)
                               + (k3 * value * value * value));
    double       seconds = samples / 96000.0;

    if (range == 1) {
        seconds *= 10.0;          // Lo
    } else if (range == 2) {
        seconds *= 100.0;         // Hi
    }
    return seconds;
}

static double env_time_seconds(double paramValue) {
    return adr_time_seconds(paramValue);
}

static const tLfoParams * lfo_params(tModuleType type) {
    switch (type) {
        case moduleTypeLfoA:
        {
            return &kLfoA;
        }
        case moduleTypeLfoB:
        {
            return &kLfoB;
        }
        case moduleTypeLfoC:
        {
            return &kLfoC;
        }
        default:
        {
            return &kLfoShpA;
        }
    }
}

// §9.3
static uint32_t node_output_legs(tNodeKind kind) {
    return (kind == eNodeFltMulti) ? 3u : 2u;
}

static bool module_kind(tModule * module, tNodeKind * kind) {
    switch (module->type) {
        case moduleTypeOscB:
        case moduleTypeOscA:
        case moduleTypeOscC:
        case moduleTypeOscD:
        {
            *kind = eNodeOsc;       // see kOscParams
            return true;
        }
        case moduleTypeFltClassic:
        case moduleTypeFltLP:
        case moduleTypeFltHP:
        case moduleTypeFltStatic:
        case moduleTypeFltNord:
        {
            // All five measured filters. FltComb and FltPhase are deliberately absent: a comb and a
            // phaser are delay structures, not response curves, and neither has been modelled yet.
            *kind = eNodeFilter;
            return true;
        }
        case moduleTypeLfoA:
        case moduleTypeLfoB:
        case moduleTypeLfoC:
        case moduleTypeLfoShpA:
        {
            *kind = eNodeLfo;
            return true;
        }
        case moduleTypeLevAmp:
        {
            *kind = eNodeLevAmp;
            return true;
        }
        case moduleTypeLevMult:
        {
            *kind = eNodeLevMult;
            return true;
        }
        case moduleTypePulse:
        {
            *kind = eNodePulse;
            return true;
        }
        case moduleTypeEnvADSR:
        {
            *kind = eNodeEnv;
            return true;
        }
        case moduleTypeOscShpB:
        case moduleTypeOscShpA:
        {
            *kind = eNodeOscShp;
            return true;
        }
        case moduleTypeMix1to1A:
        case moduleTypeMix1to1S:
        case moduleTypeMix2to1A:
        case moduleTypeMix2to1B:
        case moduleTypeMix4to1A:
        case moduleTypeMix4to1B:
        case moduleTypeMix4to1C:
        case moduleTypeMix4to1S:
        case moduleTypeMix8to1A:
        case moduleTypeMix8to1B:
        case moduleTypeMixFader:
        {
            *kind = eNodeMix;       // see kMixSpecs
            return true;
        }
        case moduleTypePan:
        case moduleTypeXtoFade:
        case moduleTypeFade1to2:
        case moduleTypeFade2to1:
        {
            *kind = eNodeFade;
            return true;
        }
        case moduleTypeMixStereo:
        {
            *kind = eNodeMixStereo;
            return true;
        }
        case moduleTypeNoise:
        {
            *kind = eNodeNoise;
            return true;
        }
        case moduleTypeOscNoise:
        {
            *kind = eNodeOscNoise;
            return true;
        }
        case moduleTypeFltMulti:
        {
            *kind = eNodeFltMulti;
            return true;
        }
        case moduleTypeEqPeak:
        case moduleTypeEq2Band:
        case moduleTypeEq3band:
        {
            *kind = eNodeEq;
            return true;
        }
        case moduleTypeStChorus:
        {
            *kind = eNodeChorus;
            return true;
        }
        case moduleTypeCompress:
        {
            *kind = eNodeCompress;
            return true;
        }
        case moduleTypeDelayB:
        case moduleTypeDelayA:
        {
            *kind = eNodeDelay;
            return true;
        }
        case moduleTypeReverb:
        {
            *kind = eNodeReverb;
            return true;
        }
        case moduleTypeClip:
        case moduleTypeOverdrive:
        case moduleTypeSaturate:
        case moduleTypeShpExp:
        case moduleTypeWaveWrap:
        case moduleTypeShpStatic:
        case moduleTypeRect:
        {
            *kind = eNodeShaper;
            return true;
        }
        case moduleTypeConstant:
        {
            *kind = eNodeConstant;
            return true;
        }
        case moduleTypeFxtoIn:
        {
            *kind = eNodeFxIn;
            return true;
        }
        case moduleType2toOut:
        case moduleType4toOut:
        {
            *kind = eNodeOut;
            return true;
        }
        default:
        {
            return false;
        }
    }
}

// Which connectors each kind draws its signal from, in the order the node stores them.
#define anyConnectorType    ((tConnectorType) - 1)
static int connector_index_for_input(tModuleType moduleType, uint32_t nth, tConnectorType wantedType);

// The first `max` input connectors, in the order the module's own resources list them.
static uint32_t inputs_in_module_order(tModuleType moduleType, uint32_t max, uint32_t * derived) {
    uint32_t count = 0;

    while (count < max) {
        int found = connector_index_for_input(moduleType, count, anyConnectorType);

        if (found < 0) {
            break;
        }
        derived[count] = (uint32_t)found;
        count++;
    }
    return count;
}

static uint32_t input_connectors(tNodeKind kind, tModuleType moduleType, bool stereoMix, const uint32_t ** connectors) {
    // Derived from the module resources rather than written out — see connector_index_for_input().
    // Static because the chain is built on one thread; the contents are rewritten per call.
    // Per THREAD: two instances can build their chains at the same moment on different threads.
    static _Thread_local uint32_t derived[MAX_NODE_INPUTS];
    static const uint32_t         oneIn[]       = {CONNECTOR_IN_A};
    static const uint32_t         twoIn[]       = {CONNECTOR_IN_A, CONNECTOR_IN_B};
    static const uint32_t         mixIn[]       = {0, 1, 2, 3};
    // In1L, In1R .. In4L, In4R as RAW CONNECTOR indices: Mix4to1S's connector list really does run
    // Out, Out, then ten inputs, so the first eight input legs are connectors 2..9.
    static const uint32_t         mixStereoIn[] = {2, 3, 4, 5, 6, 7, 8, 9};
    static const uint32_t         envIn[]       = {0};              // connector 0 is the audio the envelope shapes
    // OscB has "two pitch modulation inputs, one frequency modulation input, one sync modulation
    // input and a Shape modulation input" (manual, OscB). The two pitch inputs are the control-rate
    // pair at 0 and 1; both are summed and scaled by the one Pitch knob the module carries.
    static const uint32_t         oscIn[]       = {0, 1};
    static const uint32_t         none[]        = {0};

    switch (kind) {
        case eNodeFilter:
        {
            // Input 0 is the audio; the first CONTROL input is the one the Env knob scales. Asking
            // the resources gets this right for any filter, whatever order its connectors sit in —
            // FltClassic interleaves them as In(audio), Out, In(control), In(control).
            int audioIn   = connector_index_for_input(moduleType, 0, connectorTypeAudio);
            int controlIn = connector_index_for_input(moduleType, 1, anyConnectorType);

            derived[0]  = (audioIn >= 0) ? (uint32_t)audioIn : CONNECTOR_IN_A;
            derived[1]  = (controlIn >= 0) ? (uint32_t)controlIn : FLT_CONNECTOR_ENV_IN;
            *connectors = derived;
            return 2;
        }
        case eNodeOsc:
        case eNodeOscShp:
        {
            // §6.3
            static const uint32_t oscCIn[] = {3, 0};

            if (moduleType == moduleTypeOscC) {
                *connectors = oscCIn;
                return 2;
            }
            *connectors = oscIn;
            return (moduleType == moduleTypeOscD) ? 1 : 2;
        }
        case eNodeLevMult:
        case eNodePulse:
        case eNodeOut:
        {
            *connectors = twoIn;
            return 2;
        }
        case eNodeMix:
        {
            // §3.1, §3.5 - channels first, then the Chain input(s).
            uint32_t count = inputs_in_module_order(moduleType, MAX_NODE_INPUTS, derived);

            if (count == 0) {
                count = stereoMix ? 8 : 4;

                for (uint32_t leg = 0; leg < count; leg++) {
                    derived[leg] = stereoMix ? mixStereoIn[leg] : mixIn[leg];
                }
            }
            *connectors = derived;
            return count;
        }
        case eNodeChorus:
        case eNodeDelay:
        case eNodeCompress:
        {
            *connectors = oneIn;
            return 1;
        }
        case eNodeReverb:
        {
            *connectors = twoIn;
            return 2;
        }
        case eNodeEnv:
        {
            *connectors = envIn;
            return 1;
        }
        case eNodeFxIn:
        {
            *connectors = none;   // filled in by the Voice-area bridge, not by a cable
            return 0;
        }
        case eNodeNoise:
        {
            *connectors = none;     // a source: no inputs at all
            return 0;
        }
        case eNodeFltMulti:
        case eNodeOscNoise:
        {
            uint32_t count = inputs_in_module_order(moduleType, 3u, derived);
            *connectors = derived;
            return count;
        }
        case eNodeMixStereo:
        {
            uint32_t count = inputs_in_module_order(moduleType, 6u, derived);
            *connectors = derived;
            return count;
        }
        case eNodeFade:
        {
            uint32_t count = inputs_in_module_order(moduleType, 3u, derived);
            *connectors = derived;
            return count;
        }
        case eNodeShaper:
        {
            // One jack or two, in the module's OWN order: WaveWrap's Mod comes first and every
            // other shaper's comes second, and ShpStatic and Rect have no Mod jack at all. Asking
            // the resources for the nth input keeps all three cases out of a table here.
            uint32_t leg   = 0;
            uint32_t count = 0;

            for (leg = 0; leg < 2; leg++) {
                int found = connector_index_for_input(moduleType, leg, anyConnectorType);

                if (found < 0) {
                    break;
                }
                derived[leg] = (uint32_t)found;
                count++;
            }

            *connectors = derived;
            return count;
        }
        case eNodeLevAmp:
        case eNodePassThru:
        case eNodeEq:
        {
            *connectors = oneIn;
            return 1;
        }
        default:
        {
            *connectors = none;
            return 0;
        }
    }
}

// The module feeding a given input connector, or NULL. cable_chain_find_root() does the walking —
// it follows a chain back to the output that sources it, including through the input-to-input links
// the G2 uses for serial chains, and returns false if the chain never reaches a real output.

// ---------------------------------------------------------------------------------------------
// Connector lookup, derived from the module resources rather than hard-coded
// ---------------------------------------------------------------------------------------------

// notes §74
static int connector_index_for_input(tModuleType moduleType, uint32_t nth, tConnectorType wantedType) {
    uint32_t total   = module_connector_count(moduleType);
    uint32_t seen    = 0;
    uint32_t index   = 0;
    uint32_t listLen = array_size_connector_location_list();
    uint32_t entry   = 0;

    for (entry = 0; entry < listLen; entry++) {
        const tConnectorLocation * loc = &connectorLocationList[entry];

        if (loc->moduleType != moduleType) {
            continue;
        }

        if (index >= total) {
            break;
        }

        if (loc->direction == connectorDirIn) {
            // Audio and Control are interchangeable as signal carriers here — the engine works in
            // doubles throughout — so a caller asking for Control accepts Audio too and vice versa.
            // Logic is what must not be mistaken for either.
            bool typeOk = (wantedType == anyConnectorType)
                          || (loc->type == wantedType)
                          || (  ((wantedType == connectorTypeControl) || (wantedType == connectorTypeAudio))
                             && ((loc->type == connectorTypeControl) || (loc->type == connectorTypeAudio)));

            if (typeOk == true) {
                if (seen == nth) {
                    return (int)index;
                }
                seen++;
            }
        }
        index++;
    }

    return -1;
}

static tModule * module_feeding(tModule * sink, uint32_t connectorIndex, uint32_t * sourceOutput) {
    tCableNode inputNode = {0};
    tCableNode root      = {0};

    *sourceOutput = 0;

    if (cable_chain_node_from_connector(sink, connectorIndex, &inputNode) == false) {
        return NULL;
    }

    if (cable_chain_find_root(sink->key.slot, sink->key.location, inputNode, &root) == false) {
        return NULL;    // nothing plugged in, or a chain with no source at the far end
    }
    *sourceOutput = root.ioCount;
    return get_module_slot(sink->key.slot, sink->key.location, root.moduleIndex);
}

// notes §75
static tModule * voice_area_output_for_fx(uint32_t slot, uint32_t wantedBus) {
    uint32_t index = 0;

    for (index = 0; index < MAX_NUM_MODULES; index++) {
        tModule * module      = get_module_slot(slot, (uint32_t)locationVa, index);

        if (module == NULL) {
            continue;
        }
        uint32_t  variation   = gPatchDescr[slot].activeVariation;
        uint32_t  destination = module->param[variation][OUT_PARAM_DESTINATION].value;

        if (module->type == moduleType2toOut) {
            // FX 1/2 is destination 2 and FX 3/4 is 3, so the bus index is the destination less 2.
            if ((destination >= 2) && ((destination - 2) == wantedBus)) {
                return module;
            }
        } else if (module->type == moduleType4toOut) {
            // A 4-Out has one "Fx" setting covering all four channels, so it feeds both pairs.
            if (destination == 1) {
                return module;
            }
        }
    }

    return NULL;
}

static const double kEqLowShelfHz[]  = {80.0, 110.0, 160.0};      // §11.2
static const double kEqHighShelfHz[] = {8000.0, 6000.0, 12000.0}; // measured order, not the names'

#define EQ_MID_OCTAVES    (1.0)    // §11.3

static double eq_dial_gain(double dial) {
    return pow(10.0, ((dial - 64.0) * (18.0 / 64.0)) / 20.0);    // §11.1
}

static double eq_peak_damping(double octaves) {
    double ratio = exp2(octaves);

    return 2.0 * (ratio - 1.0) / sqrt(ratio);
}

// §11.4 - a cut mirrors the boost of the same size.
static void eq_mirror_cuts(tEngineNode * node) {
    if ((node->eqLowHz > 0.0) && (node->eqLowGain < 1.0)) {
        node->eqLowHz /= node->eqLowGain;
    }

    if ((node->eqHighHz > 0.0) && (node->eqHighGain < 1.0)) {
        node->eqHighHz *= node->eqHighGain;
    }

    if ((node->eqPeakHz > 0.0) && (node->eqPeakGain < 1.0)) {
        node->eqPeakDamping /= node->eqPeakGain;
    }
}

static double eq_shelf_hz(const double * table, uint32_t selector) {
    return table[(selector > 2u) ? 2u : selector];
}

static void eq_build(tEngineNode * node, tModule * module, uint32_t variation) {
    node->eqLowHz  = 0.0;
    node->eqHighHz = 0.0;
    node->eqPeakHz = 0.0;

    switch (module->type) {
        case moduleTypeEqPeak:
        {
            node->eqPeakHz      = flt_cutoff_hz(param_value(module, variation, 0));
            node->eqPeakGain    = eq_dial_gain(param_value(module, variation, 1));
            node->eqPeakDamping = eq_peak_damping((128.0 - param_value(module, variation, 2)) / 64.0);
            node->active        = (param_value(module, variation, 3) != 0.0);
            node->eqInputLevel  = mix_level_gain(param_value(module, variation, 4));
            break;
        }
        case moduleTypeEq2Band:
        {
            node->eqLowGain    = eq_dial_gain(param_value(module, variation, 0));
            node->eqHighGain   = eq_dial_gain(param_value(module, variation, 1));
            node->eqInputLevel = mix_level_gain(param_value(module, variation, 2));
            node->active       = (param_value(module, variation, 3) != 0.0);
            node->eqLowHz      = eq_shelf_hz(kEqLowShelfHz, module->param[variation][4].value);
            node->eqHighHz     = eq_shelf_hz(kEqHighShelfHz, module->param[variation][5].value);
            break;
        }
        default:
        {
            node->eqLowGain     = eq_dial_gain(param_value(module, variation, 0));
            node->eqPeakGain    = eq_dial_gain(param_value(module, variation, 1));
            node->eqPeakHz      = 100.0 * pow(80.0, param_value(module, variation, 2) / 127.0);
            node->eqPeakDamping = eq_peak_damping(EQ_MID_OCTAVES);
            node->eqHighGain    = eq_dial_gain(param_value(module, variation, 3));
            node->eqInputLevel  = mix_level_gain(param_value(module, variation, 4));
            node->active        = (param_value(module, variation, 5) != 0.0);
            node->eqLowHz       = eq_shelf_hz(kEqLowShelfHz, module->param[variation][6].value);
            node->eqHighHz      = eq_shelf_hz(kEqHighShelfHz, module->param[variation][7].value);
            break;
        }
    }
    eq_mirror_cuts(node);
}

// §11 - the shelves, then the peak; each adds its boost to what passes through.
static double eq_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input) {
    SE_LOCAL;

    double * state  = gLadder[voice][node];       // low shelf, high shelf, the peak's two
    double   signal = input * spec->eqInputLevel;

    if (spec->eqLowHz > 0.0) {
        double pole = exp(-2.0 * M_PI * spec->eqLowHz / gSampleRate);

        state[0] += (1.0 - pole) * (signal - state[0]);
        signal   += (spec->eqLowGain - 1.0) * state[0];
    }

    if (spec->eqHighHz > 0.0) {
        double pole = exp(-2.0 * M_PI * spec->eqHighHz / gSampleRate);
        double half = 0.5 * (1.0 + pole) * signal;
        double high = state[1] + half;

        state[1] = (pole * high) - half;
        signal  += (spec->eqHighGain - 1.0) * high;
    }

    if (spec->eqPeakHz > 0.0) {                               // §11.5
        double g       = tan(M_PI * fmin(spec->eqPeakHz, gSampleRate * 0.45) / gSampleRate);
        double damping = spec->eqPeakDamping;
        double high    = (signal - ((damping + g) * state[2]) - state[3]) / (1.0 + (damping * g) + (g * g));
        double band    = (g * high) + state[2];

        state[2] = (g * high) + band;
        state[3] = (2.0 * g * band) + state[3];
        signal  += (spec->eqPeakGain - 1.0) * damping * band;
    }
    return signal;
}

static void set_osc_pitch(tEngineNode * node, tModule * module, uint32_t variation, const tOscParams * p) {
    double tune      = param_value(module, variation, (uint32_t)p->tune);
    double cent      = param_value(module, variation, (uint32_t)p->cent);
    int    pitchType = (int)param_value(module, variation, (uint32_t)p->pitchType);

    // Factor and Partial set the pitch against a master oscillator, which the engine does not have.
    if (pitchType > 1) {
        LOG_DEBUG("Sound engine: Osc PitchType %d not supported, reading Tune as Semi\n", pitchType);
    }
    node->oscKbt    = (param_value(module, variation, (uint32_t)p->kbt) != 0.0);
    node->basePitch = tune + (osc_fine_cents(cent) / 100.0);
    node->modAmount = (p->pitchMod >= 0)
                      ? type_ii_attenuator(param_value(module, variation, (uint32_t)p->pitchMod) / 127.0)
                      : 0.0;
    node->active    = (param_value(module, variation, (uint32_t)p->active) != 0.0);
}

// notes §76
static int32_t add_node(tSoundEngineParams * params, tModule * module, uint32_t variation, uint32_t depth) {
    SE_LOCAL;

    tNodeKind     kind                            = eNodeOsc;
    tEngineNode * node                            = NULL;
    int32_t       self                            = 0;
    // notes §77
    int32_t       resolvedIn[MAX_NODE_INPUTS];
    uint32_t      resolvedSrcOut[MAX_NODE_INPUTS] = {0};

    for (uint32_t leg = 0; leg < MAX_NODE_INPUTS; leg++) {
        resolvedIn[leg] = -1;
    }

    uint32_t      inCount                         = 0;

    if ((module == NULL) || (depth >= MAX_ENGINE_NODES) || (params->nodeCount >= MAX_ENGINE_NODES)) {
        return -1;
    }

    if (module_kind(module, &kind) == false) {
        return -1;
    }
    // Already in the chain? One envelope commonly feeds several places — the filter's Env input and
    // a LevMult at once, say — and it is the same signal at each, so reuse the node rather than
    // evaluating it twice and spending two slots of the budget on it.
    {
        uint32_t existing = 0;

        for (existing = 0; existing < params->nodeCount; existing++) {
            // Keyed on the area as well as the index: the two areas number their modules
            // independently, so a Voice module and an FX module routinely share an index and
            // matching on the index alone silently merged two unrelated modules into one node.
            if (  (params->node[existing].moduleIndex == module->key.index)
               && (params->node[existing].location == module->key.location)) {
                return (int32_t)existing;
            }
        }
    }

    // Inputs first, so they land at lower node indices than this one.
    {
        const uint32_t * connectors                     = NULL;
        uint32_t         count                          = input_connectors(kind, module->type, (mix_spec(module->type) != NULL) && mix_spec(module->type)->stereo, &connectors);
        uint32_t         c                              = 0;
        // notes §78
        uint32_t         connectorList[MAX_NODE_INPUTS] = {0};

        if (count > MAX_NODE_INPUTS) {
            count = MAX_NODE_INPUTS;
        }

        for (c = 0; c < count; c++) {
            connectorList[c] = connectors[c];
        }

        for (c = 0; c < count; c++) {
            uint32_t  sourceOutput = 0;
            tModule * source       = module_feeding(module, connectorList[c], &sourceOutput);

            resolvedIn[c]     = add_node(params, source, variation, depth + 1);
            resolvedSrcOut[c] = sourceOutput;
        }

        inCount = count;

        // notes §79
        if (kind == eNodeFxIn) {
            uint32_t  wantedBus = module->param[variation][FXIN_PARAM_SOURCE].value;
            tModule * feeder    = voice_area_output_for_fx(module->key.slot, wantedBus);
            int32_t   source    = (feeder != NULL) ? add_node(params, feeder, variation, depth + 1) : -1;

            // notes §80
            resolvedIn[0]     = source;
            resolvedSrcOut[0] = 0;
            resolvedIn[1]     = source;
            resolvedSrcOut[1] = 1;
            inCount           = 2;
        }
    }

    if (params->nodeCount >= MAX_ENGINE_NODES) {
        return -1;
    }
    self              = (int32_t)params->nodeCount++;
    node              = &params->node[self];
    memset(node, 0, sizeof(*node));
    node->kind        = kind;
    node->moduleIndex = module->key.index;
    node->location    = module->key.location;
    node->inCount     = inCount;
    node->active      = true;

    {
        uint32_t c = 0;

        for (c = 0; c < MAX_NODE_INPUTS; c++) {
            node->in[c]     = (c < inCount) ? resolvedIn[c] : -1;
            node->srcOut[c] = (c < inCount) ? resolvedSrcOut[c] : 0;
            node->srcLeg[c] = 0;

            // §9.3
            if ((c < inCount) && (resolvedIn[c] >= 0) && (resolvedSrcOut[c] > 0)) {
                uint32_t legs = node_output_legs(params->node[resolvedIn[c]].kind);

                node->srcLeg[c] = (legs > 2u) ? ((resolvedSrcOut[c] < legs) ? resolvedSrcOut[c] : (legs - 1u)) : 1u;
            }
        }
    }

    switch (kind) {
        case eNodeOscShp:
        {
            // notes §81
            bool isShpA = (module->type == moduleTypeOscShpA);

            if (isShpA == true) {
                // A's six waveforms onto B's eight: the first five coincide and A's SymPulse is B's
                // eighth entry. Its Waveform is a parameter, not a mode.
                static const uint32_t kShpAWave[] = {0u, 1u, 2u, 3u, 4u, 7u};
                uint32_t              w           = (uint32_t)param_value(module, variation,
                                                                          SHPA_PARAM_WAVEFORM);

                node->wave = (tOscWave)kShpAWave[(w < 6u) ? w : 0u];
            } else {
                node->wave = (tOscWave)module->mode[SHPB_MODE_WAVEFORM].value;
            }
            node->oscKbt    = (param_value(module, variation,
                                           isShpA ? SHPA_PARAM_KBT : SHPB_PARAM_KBT) != 0.0);
            node->basePitch = param_value(module, variation,
                                          isShpA ? SHPA_PARAM_TUNE : SHPB_PARAM_TUNE)
                              + (osc_fine_cents(param_value(module, variation,
                                                            isShpA ? SHPA_PARAM_CENT
                                                            : SHPB_PARAM_CENT)) / 100.0);
            // notes §82
            node->shape     = param_value(module, variation,
                                          isShpA ? SHPA_PARAM_SHAPE : SHPB_PARAM_SHAPE) / 127.0;
            node->modAmount = type_ii_attenuator(param_value(module, variation,
                                                             isShpA ? SHPA_PARAM_PITCH_MOD
                                                             : SHPB_PARAM_PITCH_MOD) / 127.0);
            node->active    = (param_value(module, variation,
                                           isShpA ? SHPA_PARAM_ACTIVE : SHPB_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeChorus:
        {
            // Detune sets how far the delay is swept, Amount how much of the wet signal is heard.
            node->depth  = param_value(module, variation, CHORUS_PARAM_DETUNE) / 127.0;
            node->amount = param_value(module, variation, CHORUS_PARAM_AMOUNT) / 127.0;
            node->active = (param_value(module, variation, CHORUS_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeCompress:
        {
            // notes §83
            double thrRaw = param_value(module, variation, COMP_PARAM_THRESHOLD);
            double att    = param_value(module, variation, COMP_PARAM_ATTACK);
            double rel    = param_value(module, variation, COMP_PARAM_RELEASE) / 127.0;

            if (thrRaw >= COMP_THRESHOLD_OFF) {
                node->threshold = COMP_THRESHOLD_NONE;   // "Off": nothing ever reaches it
            } else {
                node->threshold = pow(10.0, (thrRaw - COMP_THRESHOLD_OFFSET_DB) / 20.0);
            }
            node->ratio        = compressor_ratio(param_value(module, variation, COMP_PARAM_RATIO));

            // REF LEVEL, which this module ignored entirely until 2026-09-07 - see compress_step().
            // Same dB offset as the threshold, and no "Off" position: the manual gives its range as
            // -30 to +12 dB, and the dial is 43 steps, so raw 0..42 maps straight onto that.
            node->refLevel     = pow(10.0, (param_value(module, variation, COMP_PARAM_REFLVL)
                                            - COMP_THRESHOLD_OFFSET_DB) / 20.0);

            // Raw 0 is "Fast" — a coefficient of 1 follows the input with no lag at all.
            node->attackCoeff  = (att <= 0.0) ? 1.0
                                 : (1.0 - exp(-1.0 / (gSampleRate * (COMP_ATTACK_MIN_S
                                                                     * pow(COMP_ATTACK_MAX_S / COMP_ATTACK_MIN_S,
                                                                           (att - 1.0) / 126.0)))));
            node->releaseCoeff = 1.0 - exp(-1.0 / (gSampleRate * (COMP_RELEASE_MIN_S
                                                                  * pow(COMP_RELEASE_MAX_S / COMP_RELEASE_MIN_S, rel))));
            node->active       = (param_value(module, variation, COMP_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeDelay:
        {
            // notes §84
            uint32_t range   = module->mode[DELAY_MODE_RANGE].value;
            double   maxTime = delay_range_max_seconds(module->type, range);

            {
                int  clkIndex = delay_time_clk_param_index(module->type);
                bool clocked  = (clkIndex >= 0)
                                && (module->param[variation][clkIndex].value != 0);

                if (clocked == true) {
                    // notes §85
                    node->timeSeconds = clk_sync_beats(param_value(module, variation, DELAY_PARAM_TIME))
                                        * (60.0 / ENGINE_REFERENCE_BPM);

                    // notes §86
                    while ((node->timeSeconds > maxTime) && (node->timeSeconds > 0.0)) {
                        node->timeSeconds *= 0.5;
                    }
                } else {
                    // Shared with the readout so the two cannot disagree — see delay_time_seconds()
                    // in renderParams.c for the derivation and its hardware confirmation.
                    node->timeSeconds = delay_time_seconds(maxTime,
                                                           param_value(module, variation, DELAY_PARAM_TIME));
                }
            }
            // notes §87
            node->depth = param_value(module, variation, DELAY_PARAM_FEEDBACK) / 127.0;
            // notes §88
            {
                double lp    = param_value(module, variation, DELAY_PARAM_LP) / 127.0;
                double fc    = DELAY_LP_MIN_HZ * pow(DELAY_LP_MAX_HZ / DELAY_LP_MIN_HZ, lp);
                double coeff = 1.0 - exp(-2.0 * M_PI * fc / gSampleRate);

                if (coeff > 1.0) {
                    coeff = 1.0;
                }
                node->damping = 1.0 - coeff;
            }
            {
                // HP 0 is the filter switched out, not merely its lowest cutoff — measured flat.
                double hp = param_value(module, variation, DELAY_PARAM_HP);

                if (hp <= 0.0) {
                    node->hpCoeff = 0.0;
                } else {
                    double fc = exp(DELAY_HP_LOG_A + (DELAY_HP_LOG_B * hp) + (DELAY_HP_LOG_C * hp * hp));

                    node->hpCoeff = 1.0 - exp(-2.0 * M_PI * fc / gSampleRate);

                    if (node->hpCoeff > 1.0) {
                        node->hpCoeff = 1.0;
                    }
                }
            }
            node->amount = param_value(module, variation, DELAY_PARAM_DRYWET) / 127.0;
            node->active = (param_value(module, variation,
                                        (module->type == moduleTypeDelayA)
                                             ? DELAYA_PARAM_ACTIVE : DELAYB_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeReverb:
        {
            // notes §89
            node->timeNorm   = param_value(module, variation, REVERB_PARAM_TIME) / 127.0;
            {
                uint32_t reverbType = module->mode[REVERB_MODE_TYPE].value;

                if (reverbType >= REVERB_TYPE_COUNT) {
                    reverbType = 0;
                }
                node->timeSeconds = kReverbDecayBase[reverbType]
                                    + (kReverbDecaySlope[reverbType]
                                       * param_value(module, variation, REVERB_PARAM_TIME));
            }
            // Named for the dial, not for the filter coefficient it used to be assigned straight to
            // — see reverb_step(), which now does the inversion itself.
            node->brightness = param_value(module, variation, REVERB_PARAM_BRIGHT) / 127.0;
            node->amount     = param_value(module, variation, REVERB_PARAM_DRYWET) / 127.0;
            node->active     = (param_value(module, variation, REVERB_PARAM_ACTIVE) != 0.0);
            // Raw, like every other drop-down: a mode cannot carry a morph (manual p.20).
            node->reverbType = module->mode[REVERB_MODE_TYPE].value;
            break;
        }
        case eNodeLfo:
        {
            const tLfoParams * p     = lfo_params(module->type);
            uint32_t           range = (p->range >= 0)
                                       ? (uint32_t)param_value(module, variation, (uint32_t)p->range) : 1;

            node->rateHz   = lfo_rate_hz(range, param_value(module, variation, (uint32_t)p->rate));
            node->wave     = (p->waveform >= 0)
                             ? (tOscWave)param_value(module, variation, (uint32_t)p->waveform) : eOscWaveSine;
            node->polarity = (p->polarity >= 0)
                             ? (uint32_t)param_value(module, variation, (uint32_t)p->polarity) : 0;
            node->shape    = (p->shape >= 0)
                             ? (param_value(module, variation, (uint32_t)p->shape) / 127.0) : 0.5;
            node->active   = (param_value(module, variation, (uint32_t)p->active) != 0.0);
            node->shpWave  = (module->type == moduleTypeLfoShpA);
            break;
        }
        case eNodeFltMulti:
        {
            // Freq 0, FreqM 1, KBT 2, GComp 3, Res 4, dB/Oct 5, On 6 - §10.1
            node->cutoffParam = param_value(module, variation, 0);
            node->modAmount   = dial_fraction(param_value(module, variation, 1));
            node->fltKbt      = param_value(module, variation, 2) * 0.25;
            node->fltGainComp = (module->param[variation][3].value != 0);
            node->resonance   = param_value(module, variation, 4) / 127.0;
            node->fltSixDb    = (module->param[variation][5].value == 0);
            node->active      = (param_value(module, variation, 6) != 0.0);
            break;
        }
        case eNodeEq:
        {
            eq_build(node, module, variation);
            break;
        }
        case eNodeOscNoise:
        {
            const tOscParams * p = osc_params(module->type);

            if (p == NULL) {
                break;
            }
            set_osc_pitch(node, module, variation, p);
            node->oscNoiseWidth    = dial_fraction(param_value(module, variation, OSCNOISE_PARAM_WIDTH));
            node->oscNoiseWidthMod = dial_fraction(param_value(module, variation, OSCNOISE_PARAM_WIDTH_MOD));
            break;
        }
        case eNodeNoise:
        {
            noise_colour(param_value(module, variation, 0), gSampleRate, &node->noisePole, &node->noiseGain);
            node->active = (param_value(module, variation, 1) != 0.0);
            break;
        }
        case eNodeMixStereo:
        {
            // §5 - Lev1..6 are params 0..5, Pan1..6 are 6..11, LevMaster is 12.
            static const double kPanScale = (127.0 / 128.0) * (127.0 / 128.0);   // see above
            double              master    = mix_level_gain(param_value(module, variation, 12));

            for (uint32_t c = 0; c < 6u; c++) {
                double level = mix_level_gain(param_value(module, variation, c)) * master;
                double u     = param_value(module, variation, 6u + c) / 127.0;

                if (u > 1.0) {
                    u = 1.0;
                } else if (u < 0.0) {
                    u = 0.0;
                }
                node->level[2u * c]       = level * kPanScale * (1.0 - (u * u));
                node->level[(2u * c) + 1] = level * kPanScale * (1.0 - ((1.0 - u) * (1.0 - u)));
            }

            node->levelCount = 12u;
            break;
        }
        case eNodeFade:
        {
            switch (module->type) {
                case moduleTypePan:
                case moduleTypeXtoFade:
                {
                    node->fadeKind = (module->type == moduleTypePan) ? eFadePan : eFadeCross;
                    node->shape    = dial_fraction(param_value(module, variation, 1));
                    node->fadeMod  = dial_fraction(param_value(module, variation, 0));
                    node->fadeLog  = (module->param[variation][2].value == 0);
                    break;
                }
                default:
                {
                    node->fadeKind = (module->type == moduleTypeFade1to2) ? eFadeOneToTwo : eFadeTwoToOne;
                    node->shape    = dial_fraction(param_value(module, variation, 0));
                    node->fadeMod  = dial_fraction(param_value(module, variation, 1));
                    node->fadeLog  = false;
                    break;
                }
            }
            break;
        }
        case eNodeShaper:
        {
            // Each module names its own dials; the drop-downs are read raw because a drop-down
            // cannot carry a morph (manual p.20). Where a module has no dial at all - ShpStatic and
            // Rect are pure mode selectors - the amount stays at full and nothing reads it.
            node->shaperAmount = 1.0;
            node->shaperMod    = 0.0;
            node->shaperCurve  = 0;
            node->shaperSym    = true;
            node->shaperIn     = 0;
            node->active       = true;

            switch (module->type) {
                case moduleTypeClip:
                {
                    node->shaperKind   = eShaperClip;
                    node->shaperAmount = param_value(module, variation, CLIP_PARAM_LEVEL) / 127.0;
                    node->shaperMod    = param_value(module, variation, CLIP_PARAM_LEVEL_MOD) / 127.0;
                    node->shaperSym    = (module->param[variation][CLIP_PARAM_SHAPE].value != 0);
                    node->active       = (param_value(module, variation, CLIP_PARAM_ACTIVE) != 0.0);
                    break;
                }
                case moduleTypeOverdrive:
                {
                    node->shaperKind   = eShaperOverdrive;
                    node->shaperAmount = param_value(module, variation, OD_PARAM_AMOUNT) / 127.0;
                    node->shaperMod    = param_value(module, variation, OD_PARAM_AMOUNT_MOD) / 127.0;
                    node->shaperCurve  = module->param[variation][OD_PARAM_TYPE].value;
                    node->shaperSym    = (module->param[variation][OD_PARAM_SHAPE].value != 0);
                    node->active       = (param_value(module, variation, OD_PARAM_ACTIVE) != 0.0);
                    break;
                }
                case moduleTypeSaturate:
                {
                    node->shaperKind   = eShaperSaturate;
                    node->shaperAmount = param_value(module, variation, SAT_PARAM_AMOUNT) / 127.0;
                    node->shaperMod    = param_value(module, variation, SAT_PARAM_AMOUNT_MOD) / 127.0;
                    node->shaperCurve  = module->param[variation][SAT_PARAM_CURVE].value;
                    node->active       = (param_value(module, variation, SAT_PARAM_ACTIVE) != 0.0);
                    break;
                }
                case moduleTypeShpExp:
                {
                    node->shaperKind   = eShaperShpExp;
                    node->shaperAmount = param_value(module, variation, SHPEXP_PARAM_AMOUNT) / 127.0;
                    node->shaperMod    = param_value(module, variation, SHPEXP_PARAM_AMOUNT_MOD) / 127.0;
                    node->shaperCurve  = module->param[variation][SHPEXP_PARAM_CURVE].value;
                    node->active       = (param_value(module, variation, SHPEXP_PARAM_ACTIVE) != 0.0);
                    break;
                }
                case moduleTypeWaveWrap:
                {
                    // THE ONLY SHAPER WHOSE MOD JACK COMES FIRST, so its signal is on leg 1.
                    node->shaperKind   = eShaperWaveWrap;
                    node->shaperAmount = param_value(module, variation, WRAP_PARAM_AMOUNT) / 127.0;
                    node->shaperMod    = param_value(module, variation, WRAP_PARAM_AMOUNT_MOD) / 127.0;
                    node->shaperIn     = 1;
                    node->active       = (param_value(module, variation, WRAP_PARAM_ACTIVE) != 0.0);
                    break;
                }
                case moduleTypeShpStatic:
                {
                    node->shaperKind  = eShaperShpStatic;
                    node->shaperCurve = module->param[variation][SHPSTATIC_PARAM_MODE].value;
                    node->active      = (param_value(module, variation, SHPSTATIC_PARAM_ACTIVE) != 0.0);
                    break;
                }
                case moduleTypeRect:
                default:
                {
                    node->shaperKind  = eShaperRect;
                    node->shaperCurve = module->param[variation][RECT_PARAM_MODE].value;
                    node->active      = (param_value(module, variation, RECT_PARAM_ACTIVE) != 0.0);
                    break;
                }
            }
            break;
        }
        case eNodeConstant:
        {
            double v = param_value(module, variation, CONST_PARAM_VALUE) / 127.0;

            node->constant = (param_value(module, variation, CONST_PARAM_BIPOLAR) != 0.0)
                             ? ((v * 2.0) - 1.0) : v;
            break;
        }
        case eNodeFxIn:
        {
            // db12PadStrMap is {"+6dB", "0dB", "-6dB", "-12dB"}, and the default is the FIRST entry,
            // so a freshly created FxtoIn is boosting by 6 dB rather than sitting at unity.
            static const double padGain[] = {2.0, 1.0, 0.5, 0.25};
            uint32_t            pad       = (uint32_t)param_value(module, variation, FXIN_PARAM_PAD);

            node->active = (param_value(module, variation, FXIN_PARAM_ACTIVE) != 0.0);
            node->gain   = padGain[(pad < 4) ? pad : 1];
            break;
        }
        case eNodeMix:
        {
            const tMixSpec *    mix       = mix_spec(module->type);
            // notes §90
            bool                linear    = (mix != NULL) && (mix->curve >= 0)
                                            && (module->param[variation][(uint32_t)mix->curve].value == MIX_CURVE_LIN);

            // notes §91
            static const double kMixPad[] = {1.0, 0.5, 0.25};
            double              pad       = 1.0;

            if (mix == NULL) {
                break;
            }

            if (mix->pad >= 0) {
                uint32_t padValue = (uint32_t)param_value(module, variation, (uint32_t)mix->pad);

                pad = kMixPad[(padValue < 3u) ? padValue : 2u];
            }
            node->mixStereo = mix->stereo;

            for (uint32_t c = 0; c < mix->channels; c++) {
                double level = 1.0;     // no dial at all: Mix4-1A and Mix8-1A sum at unity

                if (mix->lev >= 0) {
                    double raw = param_value(module, variation, (uint32_t)(mix->lev + ((int)c * mix->levStep)));

                    // §3.2 (Exp, dB), §3.3 (Lin)
                    level = linear ? dial_fraction(raw) : mix_level_gain(raw);
                }

                if ((mix->on >= 0) && (module->param[variation][(uint32_t)(mix->on + ((int)c * mix->onStep))].value == 0)) {
                    level = 0.0;
                }

                if ((mix->inv >= 0) && (module->param[variation][(uint32_t)(mix->inv + ((int)c * mix->invStep))].value != 0)) {
                    level = -level;
                }
                node->level[c] = level * pad;
            }

            // §3.5
            for (uint32_t c = mix->channels; c < MAX_NODE_INPUTS; c++) {
                node->level[c] = 1.0;
            }

            node->levelCount = node->inCount;

            break;
        }
        case eNodeOsc:
        {
            // notes §92
            const tOscParams * p    = osc_params(module->type);

            if (p == NULL) {
                break;
            }
            set_osc_pitch(node, module, variation, p);

            // A drop-down is read raw: a mode cannot carry a morph (manual p.20).
            uint32_t           wave = (p->waveMode >= 0) ? module->mode[p->waveMode].value
                            : (uint32_t)param_value(module, variation, (uint32_t)p->waveParam);

            if (p->aWaves == true) {
                // THE THREE SQUARES ARE FIXED DUTIES, reached through the same Shape the DSP already
                // uses: wave_pulse_duty() is 0.5 - shape * 0.49, so 0, 0.5102 and 0.8163 land exactly
                // on 50%, 25% and 10%. Nothing in the oscillator itself needed changing.
                static const double kSqrShape[] = {0.0, 0.510204081632653, 0.816326530612245};

                node->wave  = (wave >= 3u) ? eOscWaveSquare : (tOscWave)wave;
                node->shape = (wave >= 3u) ? kSqrShape[(wave - 3u) < 3u ? (wave - 3u) : 2u] : 0.0;
            } else {
                node->wave  = (tOscWave)wave;
                node->shape = osc_shape_percent(param_value(module, variation, (uint32_t)p->shape)) / 100.0;
            }
            break;
        }
        case eNodeFilter:
        {
            node->cutoffParam = param_value(module, variation, FLT_PARAM_FREQ);
            tFilterParams map = {
                .freq = 0, .env = 1, .kbt = 2, .res = 3, .slope = 4, .slopeMode = -1, .gc = -1, .shape = -1, .active = 5
            };

            (void)filter_param_map(module->type, &map);

            // A filter with no resonance control sits at the bottom of its range, not the middle.
            node->resonance   = (map.res >= 0)
                               ? (param_value(module, variation, (uint32_t)map.res) / 127.0) : 0.0;

            // FltClassic taps 2/3/4 poles out of a 4-pole loop; FltLP has no loop at all and its
            // six slope settings are simply ONE TO SIX cascaded poles at the same corner, measured
            // 2026-08-30. Both arrive here as a 0-based tap, so filter_step() needs no special case.
            if (map.slopeMode >= 0) {
                uint32_t slope  = (uint32_t)module->mode[map.slopeMode].value;

                uint32_t maxTap = engine_filter_legacy() ? (LADDER_LOOP_POLES - 1) : (LADDER_POLES - 1);

                if (slope > maxTap) {
                    slope = maxTap;
                }
                node->extraPoles = slope;
                node->tapStage   = slope;               // 6db..36db -> 1..6 poles
            } else if (map.slope >= 0) {
                node->extraPoles = flt_slope_extra_poles((uint32_t)param_value(module, variation, (uint32_t)map.slope));
                node->tapStage   = 1 + node->extraPoles;   // 12db..24db -> 2..4 poles
            } else {
                node->extraPoles = 0;
                node->tapStage   = 1;
            }

            switch (module->type) {
                case moduleTypeFltHP:     node->topology = eFilterTopologyCascadeHP;
                    break;
                case moduleTypeFltLP:     node->topology = eFilterTopologyCascadeLP;
                    break;
                case moduleTypeFltStatic: node->topology = eFilterTopologyBiquad;
                    break;
                default:                  node->topology = eFilterTopologyLadder;
                    break;
            }
            node->fltShape = (map.shape >= 0)
                             ? (tFilterShape)param_value(module, variation, (uint32_t)map.shape)
                             : eFilterShapeLowPass;

            // FltNord's dB/Oct picks 12 or 24, i.e. a two- or four-pole tap on the SAME four-pole
            // loop - the loop does not shorten. flt_nord_tap() returns the pole count; tapStage is
            // zero-based.
            if (module->type == moduleTypeFltNord) {
                node->tapStage = flt_nord_tap((uint32_t)param_value(module, variation, (uint32_t)map.slope)) - 1u;
            }

            // notes §93
            if (map.gc >= 0) {
                double res = param_value(module, variation, (uint32_t)map.res);
                double gc  = (param_value(module, variation, (uint32_t)map.gc) != 0.0)
                             ? flt_nord_gc_gain(res) : 1.0;

                node->fltGain = (1.0 + flt_ladder_feedback(res)) * gc;
            } else {
                node->fltGain = 1.0;
            }
            node->fltKbt    = (map.kbt >= 0)
                              ? flt_kbt_amount((uint32_t)param_value(module, variation, (uint32_t)map.kbt)) : 0.0;
            node->modAmount = (map.env >= 0)
                              ? (param_value(module, variation, (uint32_t)map.env) * 2.0 / 128.0) : 0.0;
            node->active    = (param_value(module, variation, (uint32_t)map.active) != 0.0);
            break;
        }
        case eNodeEnv:
        {
            // Read raw: Shape is a drop-down, and drop-downs cannot be morphed (manual p.20).
            node->wave    = (tOscWave)module->param[variation][ENV_PARAM_SHAPE].value;
            node->attack  = env_time_seconds(param_value(module, variation, ENV_PARAM_ATTACK));
            node->decay   = env_time_seconds(param_value(module, variation, ENV_PARAM_DECAY));
            node->sustain = param_value(module, variation, ENV_PARAM_SUSTAIN) / 127.0;
            node->release = env_time_seconds(param_value(module, variation, ENV_PARAM_RELEASE));
            break;
        }
        case eNodePulse:
        {
            node->pulseSeconds = pulse_time_seconds(param_value(module, variation, PULSE_PARAM_TIME),
                                                    (uint32_t)param_value(module, variation, PULSE_PARAM_RANGE));
            break;
        }
        case eNodeLevAmp:
        {
            // The manual (p.227) gives the range as 0.25x to 4.0x, which is what the dial displays;
            // sharing lev_amp_gain() with the dial keeps the two from drifting apart. This is NOT a
            // plain knob/64, which would run 0x to 2x and reach unity in the wrong place.
            node->gain = lev_amp_gain(param_value(module, variation, LEVAMP_PARAM_GAIN));
            break;
        }
        case eNodeOut:
        {
            node->active  = (param_value(module, variation, OUT_PARAM_ACTIVE) != 0.0);
            // notes §94
            node->gain    = (param_value(module, variation, OUT_PARAM_PAD) != 0.0) ? 2.0 : 1.0;
            // notes §95
            node->outDest = (  (module->type == moduleType2toOut)
                            && (param_value(module, variation, OUT_PARAM_DESTINATION) >= 1.0)) ? 1U : 0U;
            break;
        }
        default:
        {
            break;
        }
    }
    return self;
}

// notes §96
static bool chain_has_source(const tSoundEngineParams * params) {
    uint32_t i = 0;

    for (i = 0; i < params->nodeCount; i++) {
        if (  (params->node[i].kind == eNodeOsc)
           || (params->node[i].kind == eNodeOscShp)
           || (params->node[i].kind == eNodePulse)
           || (params->node[i].kind == eNodeNoise)
           || (params->node[i].kind == eNodeOscNoise)) {
            return true;
        }
    }

    return false;
}

// True when every generator feeding the chain is switched off, which is silence for a reason worth
// reporting. A Pulse counts alongside the oscillators, for the reason given at chain_has_source(). A bypassed filter or Out is not counted: those pass through or are the tap itself.
static bool chain_is_bypassed(const tSoundEngineParams * params) {
    uint32_t i = 0;

    for (i = 0; i < params->nodeCount; i++) {
        if (  (  (params->node[i].kind == eNodeOsc)
              || (params->node[i].kind == eNodeOscShp)
              || (params->node[i].kind == eNodePulse)
              || (params->node[i].kind == eNodeNoise)
              || (params->node[i].kind == eNodeOscNoise))
           && (params->node[i].active == true)) {
            return false;
        }
    }

    return true;
}

// Changes whenever the shape of the chain changes, so the audio thread knows to drop its per-node
// state. Turning a knob leaves this alone, which is what keeps a note running while you tweak.
static uint64_t topology_signature(const tSoundEngineParams * params) {
    uint64_t sig = params->nodeCount;
    uint32_t i   = 0;

    for (i = 0; i < params->nodeCount; i++) {
        sig = (sig * 1099511628211ull)
              ^ ((uint64_t)params->node[i].kind << 40)
              ^ ((uint64_t)params->node[i].moduleIndex << 20)
              ^ ((uint64_t)params->node[i].location << 56);

        for (uint32_t c = 0; c < MAX_NODE_INPUTS; c++) {
            sig = (sig * 1099511628211ull) ^ (uint64_t)(uint32_t)(params->node[i].in[c] + 1)
                  ^ ((uint64_t)params->node[i].srcOut[c] << 32);
        }
    }

    return sig;
}

// notes §97
static bool out_module_is_audible(tModule * module) {
    uint32_t destination = 0;

    if (module == NULL) {
        return false;
    }
    destination = module->param[gPatchDescr[module->key.slot].activeVariation][OUT_PARAM_DESTINATION].value;

    if (module->type == moduleType4toOut) {
        return destination == 0;              // "Out"; "Fx" and "Bus" are internal
    }
    return destination <= 1;                  // "Out 1/2" or "Out 3/4"
}

static tModule * find_output_module(void) {
    const uint32_t locations[] = {(uint32_t)locationFx, (uint32_t)locationVa};
    uint32_t       l           = 0;
    uint32_t       index       = 0;

    for (l = 0; l < 2; l++) {
        for (index = 0; index < MAX_NUM_MODULES; index++) {
            tModule * module = get_module_slot(engine_slot(), locations[l], index);

            if ((module == NULL) || (module->type == 0)) {
                continue;
            }

            if (  ((module->type == moduleType2toOut) || (module->type == moduleType4toOut))
               && (out_module_is_audible(module) == true)) {
                return module;
            }
        }
    }

    return NULL;
}

// notes §98
static void mark_post_mix_nodes(tSoundEngineParams * params) {
    for (uint32_t n = 0; n < params->nodeCount; n++) {
        tEngineNode * node = &params->node[n];

        node->postMix = (node->location == (uint32_t)locationFx)
                        || (node->kind == eNodeDelay)
                        || (node->kind == eNodeChorus)
                        || (node->kind == eNodeReverb);

        for (uint32_t c = 0; (c < node->inCount) && (node->postMix == false); c++) {
            int32_t in = node->in[c];

            if ((in >= 0) && (in < (int32_t)params->nodeCount) && (params->node[in].postMix == true)) {
                node->postMix = true;
            }
        }
    }
}

void sound_engine_update_from_patch(void) {
    SE_LOCAL;

    tSoundEngineParams snapshot  = {0};
    tModule *          tapModule = NULL;
    uint32_t           variation = 0;

    if (atomic_load(&gActive) == false) {
        return;
    }
    snapshot.tap = -1;

    // Glide and Bend come from the patch, not from any module in the chain — they sit on hidden
    // modules in the Morph location alongside the rest of the patch settings.
    {
        tModule * glide = get_module_slot(engine_slot(), (uint32_t)locationMorph, patchModuleGlide);
        tModule * bend  = get_module_slot(engine_slot(), (uint32_t)locationMorph, patchModuleBend);

        if (glide != NULL) {
            uint32_t mode = glide->param[0][GLIDE_TYPE].value;

            snapshot.glideMode    = (mode <= (uint32_t)eGlideAuto) ? (tGlideMode)mode : eGlideOff;
            snapshot.glideSeconds = glide_time_seconds(glide->param[0][GLIDE_SPEED].value);
        }
        {
            tModule * vibrato = get_module_slot(engine_slot(), (uint32_t)locationMorph, patchModuleVibrato);

            if (vibrato != NULL) {
                // Depth is in cents as the dial reads it, and the rate dial spans 4 to 8 Hz.
                snapshot.vibratoSource = vibrato->param[0][VIBRATO_MOD].value;
                snapshot.vibratoCents  = (double)vibrato->param[0][VIBRATO_DEPTH].value;
                snapshot.vibratoHz     = 4.0 + (((double)vibrato->param[0][VIBRATO_RATE].value / 127.0) * 4.0);
            }
        }

        if ((bend != NULL) && (bend->param[0][BEND_ON_OFF].value != 0)) {
            // The dial reads one more than it stores, so 0 is a single semitone.
            snapshot.bendSemitones = (double)bend->param[0][BEND_RANGE].value + 1.0;
        }
    }

    // notes §99
    {
        tapModule = find_output_module();

        if (tapModule == NULL) {
            gStatus = eStatusNoOutput;
        } else {
            tNodeKind kind = eNodeOsc;

            variation = gPatchDescr[tapModule->key.slot].activeVariation;

            if (module_kind(tapModule, &kind) == false) {
                gStatus = eStatusUnsupportedModule;
            } else {
                snapshot.tap = add_node(&snapshot, tapModule, variation, 0);

                // Every other audible Out module, summed with the first.
                if (snapshot.tap >= 0) {
                    for (uint32_t l = 0; l < 2; l++) {
                        uint32_t location = (l == 0) ? (uint32_t)locationFx : (uint32_t)locationVa;

                        for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
                            tModule * other = get_module_slot(engine_slot(), location, index);

                            if ((other == NULL) || (other == tapModule)) {
                                continue;
                            }

                            if (  (  (other->type != moduleType2toOut)
                                  && (other->type != moduleType4toOut))
                               || (out_module_is_audible(other) == false)) {
                                continue;
                            }

                            if (snapshot.extraTapCount >= (MAX_ENGINE_TAPS - 1)) {
                                break;
                            }
                            int32_t   extra = add_node(&snapshot, other, variation, 0);

                            if (extra >= 0) {
                                snapshot.extraTap[snapshot.extraTapCount++] = extra;
                            }
                        }
                    }
                }

                if (snapshot.tap < 0) {
                    // The kind lookup above already succeeded, so this is the node budget or the
                    // recursion guard, not an unknown module — most likely a patch that feeds back
                    // into itself.
                    gStatus = eStatusChainTooDeep;
                } else if (chain_has_source(&snapshot) == false) {
                    gStatus = eStatusNoSource;
                } else if (chain_is_bypassed(&snapshot) == true) {
                    gStatus = eStatusBypassed;
                } else {
                    gStatus       = eStatusPlaying;
                    gPlayingCount = snapshot.nodeCount;
                }
            }
        }
    }

    if (gStatus != eStatusPlaying) {
        snapshot.tap = -1;    // publish silence rather than a half-built chain
    }
    // Hand out the shared delay lines. Done here rather than in add_node() so the assignment is
    // stable for a given chain — the audio thread keys its buffers off it.
    {
        uint32_t i     = 0;
        uint32_t lines = 0;
        uint32_t verbs = 0;

        for (i = 0; i < snapshot.nodeCount; i++) {
            if (snapshot.node[i].kind == eNodeDelay) {
                snapshot.node[i].line = lines++;
            } else if (snapshot.node[i].kind == eNodeReverb) {
                snapshot.node[i].line = verbs++;
            }
        }
    }
    mark_post_mix_nodes(&snapshot);
    snapshot.topology   = topology_signature(&snapshot);
    snapshot.voiceCount = voice_count_for_patch(engine_slot());

    // How many voices the audio thread may allocate. Published separately as well as in the snapshot
    // because the note stack asks the same question from the MIDI thread, where reading the whole
    // snapshot to answer it would be absurd.
    atomic_store(&gEngineVoices, snapshot.voiceCount);
    atomic_store(&gEngineLegato, gPatchDescr[engine_slot()].monoPoly == monoPolyLegato);

    // The snapshot above was built into a local, so only this section needs the writers' mutex.
    pthread_mutex_lock(&gParamsWriteMutex);
    atomic_fetch_add(&gParamsSeq, 1);    // now odd — a reader seeing this discards its copy
    gParams             = snapshot;
    atomic_fetch_add(&gParamsSeq, 1);    // even again, snapshot is whole
    pthread_mutex_unlock(&gParamsWriteMutex);
}

// Audio thread half of the seqlock. Returns the newest whole snapshot, or the last one it managed to
// read cleanly if the UI thread happens to be publishing right now — one buffer of slightly stale
// parameters is inaudible, and blocking here would not be.
static tSoundEngineParams read_params(void) {
    SE_LOCAL;

    uint32_t attempt = 0;

    for (attempt = 0; attempt < PARAMS_READ_ATTEMPTS; attempt++) {
        uint32_t           before = atomic_load(&gParamsSeq);
        tSoundEngineParams copy;

        if ((before & 1u) != 0u) {
            continue;    // mid-write
        }
        copy = gParams;

        // notes §100
        atomic_thread_fence(memory_order_acquire);

        if (atomic_load(&gParamsSeq) == before) {
            gLastGoodParams = copy;
            break;
        }
    }

    return gLastGoodParams;
}

// ---------------------------------------------------------------------------------------------
// DSP
// ---------------------------------------------------------------------------------------------

// Two-sample correction applied either side of a waveform discontinuity. Without it a sawtooth or a
// pulse folds every harmonic above Nyquist back down into the audible range as a metallic buzz.
// t is the phase at the discontinuity, dt the phase increment per sample.
static double poly_blep(double t, double dt) {
    if (dt <= 0.0) {
        return 0.0;
    }

    if (t < dt) {
        t = t / dt;
        return (t + t) - (t * t) - 1.0;
    }

    if (t > (1.0 - dt)) {
        t = (t - 1.0) / dt;
        return (t * t) + (t + t) + 1.0;
    }
    return 0.0;
}

// Ramps DOWN, matching the G2. Measured against a hardware capture of a single saw at C7: the G2's
// ramp falls where a plain (2 * phase) - 1 rises. Alone this is inaudible, but it decides whether a
// second oscillator mixed against this one reinforces or cancels, so it has to match.
static double osc_saw(double phase, double dt) {
    return poly_blep(phase, dt) - ((2.0 * phase) - 1.0);
}

static double osc_square(double phase, double dt, double width) {
    double value = (phase < width) ? 1.0 : -1.0;

    // One correction at the rising edge (phase 0) and one at the falling edge (phase == width).
    value += poly_blep(phase, dt);
    value -= poly_blep(fmod((phase - width) + 1.0, 1.0), dt);
    return value;
}

// Symmetry-adjustable triangle: rises over the first `width` of the cycle and falls over the rest,
// so width 0.5 is the usual symmetrical shape. Not band-limited — see the header.
static double osc_triangle(double phase, double width) {
    if (phase < width) {
        return ((2.0 * phase) / width) - 1.0;
    }
    return 1.0 - ((2.0 * (phase - width)) / (1.0 - width));
}

// notes §101
static double osc_shp_wave(uint32_t waveform, double phase, double dt, double shape) {
    // notes §102
    switch (waveform) {
        case 0:
        case 1:
        case 2:
        case 3:
            // Closed-form and continuous — no step to band-limit, so the shared value is used as it
            // is, and is bit-for-bit what the editor draws.
            return wave_sine_by_index(waveform, phase, shape);

        case 4:
        {
            return osc_triangle(phase, wave_trisaw_peak(shape));
        }
        case 5:
        {
            double second = fmod(phase + wave_dblsaw_detune(shape), 1.0);

            return (osc_saw(phase, dt) + osc_saw(second, dt)) * 0.5;
        }
        case 6:
        {
            return osc_square(phase, dt, wave_pulse_duty(shape));
        }
        default:
        {
            // SymPulse: High, then Low, then silence for the rest of the cycle. Not band-limited,
            // and deliberately so — its edges are already the two the square shares, and at Shape 1
            // the wave vanishes entirely, which is what the capture shows.
            double w = wave_sympulse_half_segment(shape);

            if (phase < w) {
                return 1.0;
            }

            if (phase < (2.0 * w)) {
                return -1.0;
            }
            return 0.0;
        }
    }
}

// A delay line with feedback and a one-pole damping filter in the loop — the usual arrangement, and
// what the LP knob on the module controls.
static double delay_step(uint32_t line, double input, double timeSeconds, double feedback,
                         double damping, double hpCoeff, double mix) {
    SE_LOCAL;

    uint32_t samples = (uint32_t)(timeSeconds * gSampleRate);
    uint32_t readPos = 0;
    double   wet     = 0.0;

    if (line >= MAX_DELAY_LINES) {
        return input;
    }

    if (samples < 1) {
        samples = 1;
    } else if (samples >= DELAY_LINE_SAMPLES) {
        samples = DELAY_LINE_SAMPLES - 1;
    }
    readPos                             = (gDelayWrite[line] + DELAY_LINE_SAMPLES - samples) % DELAY_LINE_SAMPLES;
    wet                                 = (double)gDelayLine[line][readPos];

    // Damping in the feedback path, so each repeat is duller than the last rather than the dry
    // signal being filtered once.
    gDelayDamp[line]                   += (1.0 - damping) * (wet - gDelayDamp[line]);
    double   fed     = gDelayDamp[line];

    // notes §103
    if (hpCoeff > 0.0) {
        gDelayHp[line] += hpCoeff * (fed - gDelayHp[line]);
        fed             = fed - gDelayHp[line];
    }
    gDelayLine[line][gDelayWrite[line]] = (float)(input + (fed * feedback));
    gDelayWrite[line]                   = (gDelayWrite[line] + 1) % DELAY_LINE_SAMPLES;

    // notes §104
    {
        double wetRamp = (mix >= 0.5) ? 1.0 : (mix * 2.0);
        double dryRamp = (mix <= 0.5) ? 1.0 : ((1.0 - mix) * 2.0);

        return (input * dryRamp * dryRamp * dryRamp) + (wet * wetRamp * wetRamp * wetRamp);
    }
}

// notes §105
#define CHORUS_RATE_MAX_HZ    (1.3905)             // 0.010949 Hz per dial step
#define CHORUS_CENTRE_S       (0.002677)           // the fixed point both taps pass through
#define CHORUS_TAP_A_S        (0.002628)           // one tap swings this far...
#define CHORUS_TAP_B_S        (0.001943)           // ...the other the opposite way by THIS much
#define MS_SQRT1_2            (0.70710678118654752)
#define CHORUS_WET_A          (1.4742)             // wet/dry ratio law - see chorus_tap()
#define CHORUS_WET_B          (0.7744)
#define CHORUS_BLEND_K        (0.9542)             // overall trim on the pair of blend gains

// notes §106
static double shaper_odd_power(double x, double p) {
    // |x|^p with the sign carried through: an odd-symmetric power curve, which is what a shaper
    // graph that passes through the origin unchanged has to be.
    if (x < 0.0) {
        return -pow(-x, p);
    }
    return pow(x, p);
}

// Fold rather than clip: a triangle of period 4 that runs straight through [-1, 1] and turns back
// on itself outside it, so 1.5 comes back as 0.5 and 3.0 as -1.0. This is what makes WaveWrap
// generate its own overtones instead of the clipped ones a limiter would.
static double shaper_fold(double x) {
    double y = fmod(x + 1.0, 4.0);

    if (y < 0.0) {
        y += 4.0;
    }
    return (y <= 2.0) ? (y - 1.0) : (3.0 - y);
}

static double shaper_clamp(double x) {
    if (x > 1.0) {
        return 1.0;
    }

    if (x < -1.0) {
        return -1.0;
    }
    return x;
}

#define OSCNOISE_Q_AT_FULL_WIDTH      (3.34)    // §8.3
#define OSCNOISE_Q_GROWTH_PER_STEP    (0.032)
#define OSCNOISE_LEVEL                (0.5957)  // -4.5 dB RMS, §8.4

static double white_noise(uint32_t * seed) {
    uint32_t x = *seed;

    x    ^= x << 13;
    x    ^= x >> 17;
    x    ^= x << 5;
    *seed = x;
    return ((double)x / 2147483648.0) - 1.0;
}

static double oscnoise_q(double widthFraction) {
    double dial = fmin(127.0, fmax(0.0, widthFraction * 128.0));

    return OSCNOISE_Q_AT_FULL_WIDTH * exp(OSCNOISE_Q_GROWTH_PER_STEP * (127.0 - dial));
}

// §4.2, §4.3
#define MOD_INPUT_SCALE    (4.0)

static void fade_weights(const tEngineNode * spec, double pos, double * wa, double * wb) {
    if (pos < 0.0) {
        pos = 0.0;
    } else if (pos > 1.0) {
        pos = 1.0;
    }

    switch ((tFadeKind)spec->fadeKind) {
        case eFadePan:
        case eFadeCross:
        {
            if (spec->fadeLog) {
                *wa = 1.0 - (pos * pos);
                *wb = 1.0 - ((1.0 - pos) * (1.0 - pos));
            } else {
                *wa = 1.0 - pos;
                *wb = pos;
            }
            break;
        }
        default:
        {
            double x = (2.0 * pos) - 1.0;

            *wa = (x < 0.0) ? -x : 0.0;
            *wb = (x > 0.0) ? x : 0.0;
            break;
        }
    }
}

static double shaper_step(double input, double modulation, const tEngineNode * spec) {
    // The mod jack adds to the dial through its own attenuator, and the sum is clamped to the
    // dial's range - the same treatment the filter's cutoff modulation gets.
    double amount = spec->shaperAmount + (spec->shaperMod * modulation);
    double x      = shaper_clamp(input);

    if (spec->active == false) {
        return input;                 // Bypass passes the signal through untouched
    }

    if (amount < 0.0) {
        amount = 0.0;
    } else if (amount > 1.0) {
        amount = 1.0;
    }

    switch ((tShaperKind)spec->shaperKind) {
        case eShaperRect:
        {
            // Exact, from the manual: discard negatives, discard positives, mirror negatives up,
            // mirror positives down. rectStrMap is {HalfPos, HalfNeg, FullPos, FullNeg}.
            switch (spec->shaperCurve) {
                case 0:  return (x > 0.0) ? x : 0.0;

                case 1:  return (x < 0.0) ? x : 0.0;

                case 2:  return fabs(x);

                default: return -fabs(x);
            }
        }
        case eShaperShpStatic:
        {
            // notes §107
            static const double kExp[] = {1.0 / 3.0, 0.5, 2.0, 3.0};
            uint32_t            curve  = (spec->shaperCurve < 4) ? spec->shaperCurve : 2;

            return shaper_odd_power(x, kExp[curve]);
        }
        case eShaperShpExp:
        {
            // notes §108
            static const double kExp[] = {2.0, 3.0, 4.0, 5.0};
            uint32_t            curve  = (spec->shaperCurve < 4) ? spec->shaperCurve : 0;

            return shaper_odd_power(x, 1.0 + (amount * (kExp[curve] - 1.0)));
        }
        case eShaperSaturate:
        {
            // notes §109
            static const double kCurve[] = {4.0, 16.0, 64.0, 256.0};
            uint32_t            curve    = (spec->shaperCurve < 4) ? spec->shaperCurve : 0;
            double              k        = amount * kCurve[curve];

            if (k < 1e-6) {
                return x;
            }
            double              shaped   = log(1.0 + (k * fabs(x))) / log(1.0 + k);

            return (x < 0.0) ? -shaped : shaped;
        }
        case eShaperWaveWrap:
        {
            // notes §110
            return shaper_fold(x * (1.0 + (amount * 8.0)));
        }
        case eShaperOverdrive:
        {
            // notes §111
            static const double kKnee[]  = {2.0, 16.0, 3.0, 6.0};
            static const double kDrive[] = {8.0, 8.0, 24.0, 32.0};
            uint32_t            type     = (spec->shaperCurve < 4) ? spec->shaperCurve : 0;
            double              driven   = x * (1.0 + (amount * kDrive[type]));
            double              shaped   = driven / pow(1.0 + pow(fabs(driven), kKnee[type]),
                                                        1.0 / kKnee[type]);

            // Asym shapes only the positive peaks (manual), so the negative half stays linear -
            // and then meets the headroom, which is where its own harmonics come from.
            if ((spec->shaperSym == false) && (driven < 0.0)) {
                shaped = shaper_clamp(driven);
            }
            return ((1.0 - amount) * x) + (amount * shaped);
        }
        case eShaperClip:
        default:
        {
            // notes §112
            double t = pow(2.0, -6.0 * amount);

            if (x > t) {
                return t;
            }

            if ((spec->shaperSym == true) && (x < -t)) {
                return -t;
            }
            return x;
        }
    }
}

// TimeMod is NOT implemented: the module has a modulation input for its width and this ignores it,
// which is honest rather than inventing a law for it. Nothing measured so far uses it.
static double pulse_step(uint32_t voice, uint32_t node, double input, const tEngineNode * spec) {
    SE_LOCAL;

    double   prev    = gPulsePrev[voice][node];
    double   width   = spec->pulseSeconds * gSampleRate;
    uint32_t samples = (width < 1.0) ? 1U : (uint32_t)width;

    gPulsePrev[voice][node] = input;

    if ((prev <= PULSE_THRESHOLD) && (input > PULSE_THRESHOLD)) {
        gPulseCount[voice][node] = samples;
    }

    if (gPulseCount[voice][node] > 0) {
        gPulseCount[voice][node]--;
        return 1.0;
    }
    return 0.0;
}

// notes §113
static double chorus_triangle(double phase) {
    double p = phase - floor(phase);

    return (p < 0.5) ? (-1.0 + (4.0 * p)) : (3.0 - (4.0 * p));
}

// notes §114
static double chorus_read(uint32_t node, uint32_t ch, double delaySeconds) {
    SE_LOCAL;

    double   want  = delaySeconds * gSampleRate;
    uint32_t w     = gChorusWrite[node][ch];

    if (want < 2.0) {
        want = 2.0;
    } else if (want > (double)(CHORUS_SAMPLES - 3)) {
        want = (double)(CHORUS_SAMPLES - 3);
    }
    uint32_t whole = (uint32_t)want;
    double   fr    = want - (double)whole;

    // y0..y3 are whole-1, whole, whole+1 and whole+2 samples ago; the answer sits between y1 and y2.
#define CHR(ago)    ((double)gChorusLine[node][ch][(w + CHORUS_SAMPLES - (ago)) % CHORUS_SAMPLES])
    double   y0    = CHR(whole - 1);
    double   y1    = CHR(whole);
    double   y2    = CHR(whole + 1);
    double   y3    = CHR(whole + 2);
#undef CHR

    return y1 + (0.5 * fr * ((y2 - y0)
                             + fr * ((2.0 * y0) - (5.0 * y1) + (4.0 * y2) - y3
                                     + fr * ((3.0 * (y1 - y2)) + y3 - y0))));
}

static double chorus_tap(uint32_t node, uint32_t ch, double input, double phase, double amount) {
    SE_LOCAL;

    double wet = 0.0;

    // notes §115
    double tri = chorus_triangle(phase);                   // [-1, 1]

    // notes §116
    wet                                           = MS_SQRT1_2
                                                    * (chorus_read(node, ch, CHORUS_CENTRE_S + (CHORUS_TAP_A_S * tri))
                                                       + chorus_read(node, ch, CHORUS_CENTRE_S - (CHORUS_TAP_B_S * tri)));

    gChorusLine[node][ch][gChorusWrite[node][ch]] = (float)input;
    gChorusWrite[node][ch]                        = (gChorusWrite[node][ch] + 1) % CHORUS_SAMPLES;

    // notes §117
    {
        // notes §118
        double dryGain = CHORUS_BLEND_K * (CHORUS_WET_A - (CHORUS_WET_B * amount));
        double wetGain = CHORUS_BLEND_K * amount;

        return (input * dryGain) + (wet * wetGain);
    }
}

// notes §119
static void chorus_step(uint32_t node, double input, double depth, double amount,
                        double * outLeft, double * outRight) {
    SE_LOCAL;

    double phase = gChorusLfo[node];

    // notes §120
    *outLeft          = chorus_tap(node, 0, input, phase, amount);
    // A QUARTER CYCLE, not a half - of the TRUE LFO. The measured antiphase was in the FOLDED
    // separation, which runs at twice the LFO, so half a cycle there is a quarter of one here. What
    // reaches the wire is unchanged; only its description is.
    *outRight         = chorus_tap(node, 1, input, phase + 0.25, amount);

    gChorusLfo[node] += (CHORUS_RATE_MAX_HZ * depth) / gSampleRate;

    if (gChorusLfo[node] >= 1.0) {
        gChorusLfo[node] -= 1.0;
    }
}

// notes §121
static double compress_step(uint32_t voice, uint32_t node, double input, const tEngineNode * spec) {
    SE_LOCAL;

    double level = fabs(input);
    double gain  = 1.0;

    if (level > gCompEnv[voice][node]) {
        gCompEnv[voice][node] += spec->attackCoeff * (level - gCompEnv[voice][node]);
    } else {
        gCompEnv[voice][node] += spec->releaseCoeff * (level - gCompEnv[voice][node]);
    }

    // COMP_THRESHOLD_NONE is the dial's "Off", an amplitude nothing reaches. The clamp below would
    // give a gain of exactly 1 for it anyway - env and target both pin to the same huge number - but
    // only after a pow() per sample to arrive at what the branch already knows.
    if ((spec->threshold > 0.0) && (spec->threshold < COMP_THRESHOLD_NONE)) {
        double target = (spec->refLevel > spec->threshold) ? spec->refLevel : spec->threshold;
        double env    = (gCompEnv[voice][node] > spec->threshold) ? gCompEnv[voice][node]
                        : spec->threshold;

        gain = pow(target / env, 1.0 - (1.0 / spec->ratio));
    }

    // notes §122
    if (spec->threshold > 0.0) {
        static const double   kExcessDb[] = {0.0, 3.0, 6.0, 9.0, 12.0, 15.0};
        static const uint32_t kLit[]      = {1u, 3u, 5u, 6u, 7u, 8u};
        uint32_t              lit         = 0u;

        if (gCompEnv[voice][node] > spec->threshold) {
            double over = 20.0 * log10(gCompEnv[voice][node] / spec->threshold);

            lit = kLit[0];

            for (uint32_t k = 1u; k < (uint32_t)(sizeof(kLit) / sizeof(kLit[0])); k++) {
                if (over >= kExcessDb[k]) {
                    lit = kLit[k];
                } else {
                    double f = (over - kExcessDb[k - 1u]) / (kExcessDb[k] - kExcessDb[k - 1u]);

                    lit = kLit[k - 1u] + (uint32_t)((f * (double)(kLit[k] - kLit[k - 1u])) + 0.5);
                    break;
                }
            }
        }
        uint32_t              packed      = METER_WRITTEN | (((lit == 0u) ? 0u : ((1u << lit) - 1u))
                                                             & METER_VALUE_MASK);

        if (atomic_exchange_explicit(&gModuleMeter[spec->location][spec->moduleIndex],
                                     packed, memory_order_relaxed) != packed) {
            atomic_store_explicit(&gMetersDirty, true, memory_order_relaxed);
        }
    }
    return input * gain;
}

// notes §123
static void reverb_step(double input, double timeSeconds, double timeNorm, double brightness,
                        double mix, uint32_t type, double * outLeft, double * outRight) {
    SE_LOCAL;

    double   sum[REVERB_CHANNELS] = {0.0, 0.0};
    uint32_t ch                   = 0;
    uint32_t i                    = 0;
    double   lfo[RV_LINES];
    // notes §124
    double   dial                 = brightness * 127.0;

    // notes §125
    double   damp                 = REVERB_DAMP_MAX * exp(-dial / REVERB_BRIGHT_K);

    if (damp > REVERB_DAMP_CEILING) {
        damp = REVERB_DAMP_CEILING;
    }
    double   scale                = kReverbTypeScale[(type < REVERB_TYPE_COUNT) ? type : 0];

    // ONE TRIP ROUND THE LOOP, and the gain that costs. The sections either side of it are
    // lossless, so this single number is the whole decay: 60 dB in the requested time, three
    // decades over however many trips fit into it.
    double   gRvGain[RV_LINES];
    // Per engine (sLastTypeBank, at file scope so engine_reset_state() can reach it): two instances
    // on different reverb types must each notice their own change.
#define sLastType    (sLastTypeBank[SE])

    // notes §126
    if (type != sLastType) {
        memset(gPreDelay, 0, sizeof(gPreDelay));
        memset(gRvMem, 0, sizeof(gRvMem));
        gRvCur     = 0;
        memset(gRvDamp, 0, sizeof(gRvDamp));
        memset(gRvLfo, 0, sizeof(gRvLfo));
        gRevInLp   = 0.0;
        gRevInLp2  = 0.0;
        gRevInLp3  = 0.0;
        gRevInLp4  = 0.0;
        memset(gRvLoop, 0, sizeof(gRvLoop));
        memset(gPreDelayPos, 0, sizeof(gPreDelayPos));
        sLastType  = type;

        // Lay the spans out end to end. Each one starts where the last finished, so a section
        // writing at its own base and reading at the next gets exactly its own length of delay and
        // no two sections can ever share a cell.
        gRvAddr[0] = 16;

        for (i = 0; i < eRvSpanCount; i++) {
            // notes §127
            static const uint32_t kRvTankLead[REVERB_TYPE_COUNT] = {303, 382, 459, 505};

            uint32_t              lead                           = (uint32_t)((double)kRvTankLead[(type < REVERB_TYPE_COUNT) ? type : 0] * RV_RATE);
            uint32_t              meas                           = kReverbPreDelay[(type < REVERB_TYPE_COUNT) ? type : 0][0];
            uint32_t              len;

            len            = (i == (uint32_t)eRvPre)
                  ? ((meas > lead) ? (meas - lead) : 2)
                  : (uint32_t)((double)kRvLen[i] * scale * RV_RATE);

            gRvAddr[i + 1] = gRvAddr[i] + ((len < 2) ? 2 : len);
        }
    }

    // notes §128
    for (i = 0; i < RV_LINES; i++) {
        // THE LINE ALONE, not the allpass in front of it. An allpass passes a fraction of its input
        // straight through -- that is what the -g feedforward term is -- so only some of the energy
        // ever takes its delay, and charging the decay for the whole of it ran a Hall 30% fast.
        double len = (double)(gRvAddr[kRvLineDl[i] + 1] - gRvAddr[kRvLineDl[i]]);

        gRvGain[i] = (timeSeconds > 0.01)
                     ? (pow(10.0, (-3.0 * len) / (gSampleRate * timeSeconds)) / RV_MOD_LOSS)
                     : 0.0;
    }

    // notes §129
    for (i = 0; i < RV_LINES; i++) {
        lfo[i]    = gRvLfo[i];
        gRvLfo[i] = gRvLfo[i] + (kRvModHz[i] / gSampleRate);

        if (gRvLfo[i] >= 1.0) {
            gRvLfo[i] -= 1.0;
        }
    }

    // ONE TANK, RUN ONCE. It used to run twice, once per channel, over two buffers holding the same
    // state — see kRvTapFrac for the measurement that showed the second copy was earning nothing.
    {
        double diffused = input;

        // notes §130

        // notes §131
        {
            double   v      = diffused;
            uint32_t modMax = (uint32_t)(RV_MOD_DEPTH * RV_RATE);
            uint32_t i      = 0;
            double   line[RV_LINES];

#define RVR(a)       ((double)gRvMem[(gRvCur + (a)) & (RV_MEM - 1)])
#define RVW(a, x)    (gRvMem[(gRvCur + (a)) & (RV_MEM - 1)] = (float)(x))

            // A plain line: hand `v` in, get it back L samples later.
#define RVDLY(n)                         \
   do {                                  \
       double d = RVR(gRvAddr[(n) + 1]); \
       RVW(gRvAddr[n], v);               \
       v = d;                            \
   }                                     \
   while (0)

            // notes §132
#define RVDLYM(n, off)                                                   \
   do {                                                                  \
       double   rd = (double)(gRvAddr[(n) + 1] - modMax - 3) + (off);    \
       uint32_t ri = (uint32_t)rd;                                       \
       double   fr = rd - (double)ri;                                    \
       double   y0 = RVR(ri - 1);                                        \
       double   y1 = RVR(ri);                                            \
       double   y2 = RVR(ri + 1);                                        \
       double   y3 = RVR(ri + 2);                                        \
       double   d  = y1 + (0.5 * fr * ((y2 - y0)                         \
                                       + fr * ((2.0 * y0) - (5.0 * y1)   \
                                               + (4.0 * y2) - y3         \
                                               + fr * ((3.0 * (y1 - y2)) \
                                                       + y3 - y0))));    \
       RVW(gRvAddr[n], v);                                               \
       v = d;                                                            \
   } while (0)

#define RVAP(n, g)                       \
   do {                                  \
       double d = RVR(gRvAddr[(n) + 1]); \
       double w = v + ((g) * d);         \
       RVW(gRvAddr[n], w);               \
       v = d - ((g) * w);                \
   } while (0)

            // notes §133
            {
                double a1 = exp(-2.0 * M_PI * REVERB_INPUT_LP_HZ / gSampleRate);
                double a2 = exp(-2.0 * M_PI * REVERB_INPUT_LP2_HZ / gSampleRate);

                gRevInLp  = ((1.0 - a1) * v) + (a1 * gRevInLp);
                double a3 = REVERB_INPUT_LP_TIME * timeNorm;

                gRevInLp2 = ((1.0 - a2) * gRevInLp) + (a2 * gRevInLp2);
                double a4 = exp(-2.0 * M_PI * REVERB_INPUT_LP4_HZ / gSampleRate);

                gRevInLp3 = ((1.0 - a3) * gRevInLp2) + (a3 * gRevInLp3);
                gRevInLp4 = ((1.0 - a4) * gRevInLp3) + (a4 * gRevInLp4);
                v         = gRevInLp4;
            }

            // The input stage: pre-delay, then four short allpasses that smear the attack before
            // the tank ever sees it, so no single early tap stands out as an echo.
            RVDLY(eRvPre);

            for (i = 0; i < RV_DIFFUSERS; i++) {
                RVAP(kRvDiffuser[i], RV_DIFFUSE);
            }

            diffused = v;

            // notes §134
            for (i = 0; i < RV_LINES; i++) {
                v          = ((i & 1) ? -diffused : diffused) + gRvLoop[i];

                RVDLYM(kRvLineDl[i], (0.5 - (0.5 * cos(2.0 * M_PI * lfo[i]))) * (double)modMax);

                // notes §135
                gRvDamp[i] = ((1.0 - damp) * v) + (damp * gRvDamp[i]);
                line[i]    = gRvDamp[i];
            }

            // notes §136
            {
                double a0 = line[0] + line[4], a4 = line[0] - line[4];
                double a1 = line[1] + line[5], a5 = line[1] - line[5];
                double a2 = line[2] + line[6], a6 = line[2] - line[6];
                double a3 = line[3] + line[7], a7 = line[3] - line[7];
                double b0 = a0 + a2, b2 = a0 - a2;
                double b1 = a1 + a3, b3 = a1 - a3;
                double b4 = a4 + a6, b6 = a4 - a6;
                double b5 = a5 + a7, b7 = a5 - a7;

                // PER-LINE DECAY GAIN, each line losing 60 dB in the requested time over ITS OWN
                // length. One gain shared by all eight would decay the short lines faster than the
                // long ones and leave the tail's colour drifting as it faded.
                gRvLoop[0] = (b0 + b1) * RV_HADAMARD * gRvGain[0];
                gRvLoop[1] = (b0 - b1) * RV_HADAMARD * gRvGain[1];
                gRvLoop[2] = (b2 + b3) * RV_HADAMARD * gRvGain[2];
                gRvLoop[3] = (b2 - b3) * RV_HADAMARD * gRvGain[3];
                gRvLoop[4] = (b4 + b5) * RV_HADAMARD * gRvGain[4];
                gRvLoop[5] = (b4 - b5) * RV_HADAMARD * gRvGain[5];
                gRvLoop[6] = (b6 + b7) * RV_HADAMARD * gRvGain[6];
                gRvLoop[7] = (b6 - b7) * RV_HADAMARD * gRvGain[7];
            }

            // notes §137
            for (ch = 0; ch < REVERB_CHANNELS; ch++) {
                double tapSum = 0.0;

                for (i = 0; i < RV_OUTTAPS; i++) {
                    uint32_t n   = kRvTapLine[ch][i];
                    uint32_t len = gRvAddr[n + 1] - gRvAddr[n];
                    uint32_t at  = gRvAddr[n] + (uint32_t)(kRvTapFrac[ch][i] * (double)len);

                    tapSum += kRvTapSign[i] * RVR(at);
                }

                sum[ch] = tapSum * RV_TAP_SCALE;
            }

            gRvCur = (gRvCur - 1u) & (RV_MEM - 1);

#undef RVAP
#undef RVDLYM
#undef RVDLY
#undef RVR
#undef RVW
        }
    }

    // notes §138
#define REVERB_WET_GAIN    (0.5002)

    sum[0] *= REVERB_WET_GAIN;
    sum[1] *= REVERB_WET_GAIN;

    // notes §139
    {
        double wetRamp = (mix >= 0.5) ? 1.0 : (mix * 2.0);
        double dryRamp = (mix <= 0.5) ? 1.0 : ((1.0 - mix) * 2.0);
        double dry     = input * dryRamp * dryRamp * dryRamp;
        double wet     = wetRamp * wetRamp * wetRamp;

        // THE DRY SIDE IS THE SAME IN BOTH CHANNELS. It is the module's mono input; only the tail is
        // a pair, which is exactly what the instrument's correlation of +0.03 describes — two tails,
        // one source.
        *outLeft  = dry + (sum[0] * wet);
        *outRight = dry + (sum[1] * wet);
    }
}

// notes §140
void sound_engine_render_reverb_ir(double deviceRate, uint32_t type, uint32_t timeValue,
                                   uint32_t brightValue, float * out, uint32_t frames) {
    SE_LOCAL;

    if ((out == NULL) || (frames == 0) || (deviceRate <= 0.0)) {
        return;
    }

    if (type >= REVERB_TYPE_COUNT) {
        type = 0;
    }
    gSampleRate = deviceRate * (double)ENGINE_OVERSAMPLE;

    // notes §141
    memset(gPreDelay, 0, sizeof(gPreDelay));
    memset(gRvMem, 0, sizeof(gRvMem));
    gRvCur      = 0;
    memset(gRvDamp, 0, sizeof(gRvDamp));
    memset(gRvLoop, 0, sizeof(gRvLoop));
    memset(gPreDelayPos, 0, sizeof(gPreDelayPos));

    double timeSeconds = kReverbDecayBase[type] + (kReverbDecaySlope[type] * (double)timeValue);
    double timeNorm    = (double)timeValue / 127.0;
    double brightness  = (double)brightValue / 127.0;

    for (uint32_t i = 0; i < frames; i++) {
        double in   = (i == 0) ? 1.0 : 0.0;
        double wetL = 0.0;
        double wetR = 0.0;

        // mix at 1.0 is fully wet, matching DryWet 127 on the hardware — and with the dry/wet law
        // above that means the dry ramp is zero, so nothing of the click itself is in the output.
        reverb_step(in, timeSeconds, timeNorm, brightness, 1.0, type, &wetL, &wetR);

        out[(i * 2) + 0] = (float)wetL;
        out[(i * 2) + 1] = (float)wetR;
    }
}

// notes §142
bool sound_engine_meters_dirty(void) {
    SE_LOCAL;

    return atomic_exchange_explicit(&gMetersDirty, false, memory_order_relaxed);
}

// The LED the engine would light, alongside sound_engine_module_meter() and false in the same cases.
// ledIndex is accepted for the modules that will eventually have more than one; only 0 is published
// today, and anything else falls back to the database.
bool sound_engine_module_led(uint32_t location, uint32_t moduleIndex, uint32_t ledIndex, uint32_t * value) {
    SE_LOCAL;

    if (  (value == NULL) || (ledIndex != 0u) || (location >= (uint32_t)locationMax)
       || (moduleIndex >= MAX_NUM_MODULES)) {
        return false;
    }

    if (atomic_load_explicit(&gActive, memory_order_relaxed) == false) {
        return false;
    }
    uint32_t stored = atomic_load_explicit(&gModuleLed[location][moduleIndex], memory_order_relaxed);

    if ((stored & METER_WRITTEN) == 0u) {
        return false;
    }
    *value = stored & METER_VALUE_MASK;
    return true;
}

bool sound_engine_module_meter(uint32_t location, uint32_t moduleIndex, uint32_t leg, uint32_t * value) {
    SE_LOCAL;

    if ((leg > 1u) || (value == NULL) || (location >= (uint32_t)locationMax) || (moduleIndex >= MAX_NUM_MODULES)) {
        return false;
    }

    if (atomic_load_explicit(&gActive, memory_order_relaxed) == false) {
        return false;
    }
    uint32_t stored = atomic_load_explicit(&gModuleMeter[location][moduleIndex], memory_order_relaxed);

    if ((stored & METER_WRITTEN) == 0u) {
        return false;       // nothing has ever metered this module
    }
    *value = (stored >> (leg * METER_LEG_SHIFT)) & METER_VALUE_MASK;
    return true;
}

void sound_engine_render_chorus(double deviceRate, uint32_t detuneValue, uint32_t amountValue,
                                const float * in, float * out, uint32_t frames) {
    SE_LOCAL;

    if ((in == NULL) || (out == NULL) || (frames == 0) || (deviceRate <= 0.0)) {
        return;
    }
    gSampleRate = deviceRate * (double)ENGINE_OVERSAMPLE;

    // A second render in one process would otherwise start with the previous one's line and LFO
    // phase - the same trap the reverb IR clears for.
    memset(gChorusLine, 0, sizeof(gChorusLine));
    memset(gChorusWrite, 0, sizeof(gChorusWrite));
    memset(gChorusLfo, 0, sizeof(gChorusLfo));

    for (uint32_t i = 0; i < MAX_ENGINE_NODES; i++) {
        gChorusLfo[i] = CHORUS_PHASE0;
    }

    double depth  = (double)detuneValue / 127.0;
    double amount = (double)amountValue / 127.0;

    for (uint32_t i = 0; i < frames; i++) {
        double l = 0.0;
        double r = 0.0;

        chorus_step(0, (double)in[i], depth, amount, &l, &r);
        out[(i * 2) + 0] = (float)l;
        out[(i * 2) + 1] = (float)r;
    }
}

static double advance_phase(double * phase, double dt) {
    double current = *phase + dt;

    while (current >= 1.0) {
        current -= 1.0;
    }
    *phase = current;
    return current;
}

// notes §143
static double smooth_to(double * current, double target, double coeff, bool primed) {
    if (primed == false) {
        *current = target;
    } else {
        *current += coeff * (target - *current);
    }
    return *current;
}

// notes §144
#define LADDER_KNEE    (0.7)

// The highest one-pole coefficient the four-stage feedback model stays well behaved at — see the
// measurements where it is applied. 0.806 was clean, 0.842 was not.
#define LADDER_MAX_G    (0.80)

static double ladder_saturate(double x) {
    double magnitude = fabs(x);
    double excess    = 0.0;

    if (magnitude <= LADDER_KNEE) {
        return x;
    }
    excess    = (magnitude - LADDER_KNEE) / (1.0 - LADDER_KNEE);
    magnitude = LADDER_KNEE + ((1.0 - LADDER_KNEE) * (1.0 - exp(-excess)));
    return (x < 0.0) ? -magnitude : magnitude;
}

// notes §145
static double cascade_hp_filter(double * state, double input, double g, uint32_t poles) {
    double   x = input;
    uint32_t i = 0;

    for (i = 0; i < poles; i++) {
        state[i] += g * (x - state[i]);   // the low-pass part of this stage
        x         = x - state[i];         // and the high-pass is what is left
    }

    return x;
}

// notes §146
static double svf_filter(double * state, double input, double f, double q, tFilterShape shape) {
    double low  = state[0];
    double band = state[1];
    double high = input - low - (q * band);

    band    += f * high;
    low     += f * band;

    state[0] = low;
    state[1] = band;

    switch (shape) {
        case eFilterShapeBandPass:
        {
            return band;
        }
        case eFilterShapeHighPass:
        {
            return high;
        }
        case eFilterShapeBandReject:
        {
            return low + high;
        }
        default:
        {
            return low;
        }
    }
}

// §8.2 - two unity-peak band-passes in series, then the measured level.
static double oscnoise_step(uint32_t voice, uint32_t node, double hz, double widthFraction) {
    SE_LOCAL;

    double * resonators = gLadder[voice][node];       // four of its six slots
    double   q          = oscnoise_q(widthFraction);
    double   centre     = fmin(fmax(hz, 1.0), gSampleRate / 8.0);
    double   f          = 2.0 * sin(M_PI * centre / gSampleRate);
    double   white      = white_noise(&gNoiseSeed[voice][node]);
    double   first      = svf_filter(&resonators[0], white, f, 1.0 / q, eFilterShapeBandPass) / q;
    double   second     = svf_filter(&resonators[2], first, f, 1.0 / q, eFilterShapeBandPass) / q;
    double   bandwidth  = M_PI * centre / (4.0 * q);   // noise bandwidth of the pair
    double   gain       = OSCNOISE_LEVEL / sqrt((1.0 / 3.0) * bandwidth / (gSampleRate * 0.5));

    return second * gain;
}

static double ladder_filter(double * state, double input, double g, double k, uint32_t tapStage) {
    // The feedback tap is pinned to the FOURTH pole and must stay there. LADDER_POLES grew to six
    // for FltLP's 36 dB setting, which has no resonance at all; taking the loop from the new last
    // pole instead would have quietly retuned every FltClassic in every patch.
    double   feedback = state[LADDER_LOOP_POLES - 1];
    double   x        = 0.0;
    uint32_t i        = 0;

    // notes §147
    x = input - (k * feedback);

    // notes §148
    x = ladder_saturate(x);

    // Run the loop's four, plus any further poles this tap needs. FltClassic therefore costs
    // exactly what it did before LADDER_POLES grew.
    uint32_t poles    = (tapStage + 1u > (uint32_t)LADDER_LOOP_POLES) ? (tapStage + 1u) : (uint32_t)LADDER_LOOP_POLES;

    for (i = 0; i < poles; i++) {
        state[i] += g * (x - state[i]);
        x         = state[i];
    }

    return state[tapStage];
}

// notes §149
static double envelope_step(uint32_t voice, uint32_t node, const tEngineNode * spec, bool gate) {
    SE_LOCAL;

    double level = gEnvLevel[voice][node];
    double step  = 0.0;

    if (gate == true) {
        // notes §150
        if (  (gEnvStage[voice][node] == eEnvIdle)
           || (gEnvStage[voice][node] == eEnvRelease)
           || (gEnvTrigger[voice][node] != gVoice[voice].trigger)) {
            gEnvStage[voice][node]    = eEnvAttack;
            gEnvProgress[voice][node] = 0.0;
            gEnvStart[voice][node]    = level;   // rise from wherever a fast retrigger caught it
            gEnvTrigger[voice][node]  = gVoice[voice].trigger;
        }
    } else if (gEnvStage[voice][node] != eEnvIdle) {
        if (gEnvStage[voice][node] != eEnvRelease) {
            gEnvProgress[voice][node] = 0.0;
            gEnvStart[voice][node]    = level;   // fall from the level the key was let go at
        }
        gEnvStage[voice][node] = eEnvRelease;
    }

    switch (gEnvStage[voice][node]) {
        case eEnvAttack:
        {
            step                       = 1.0 / (spec->attack * gSampleRate);
            gEnvProgress[voice][node] += step;

            if (gEnvProgress[voice][node] >= 1.0) {
                gEnvProgress[voice][node] = 0.0;
                level                     = 1.0;
                gEnvStage[voice][node]    = eEnvDecay;
            } else {
                // From wherever the stage began, so a note struck during release still rises
                // smoothly from the level it had rather than jumping.
                level = gEnvStart[voice][node]
                        + ((1.0 - gEnvStart[voice][node]) * env_attack_level((uint32_t)spec->wave, gEnvProgress[voice][node]));
            }
            break;
        }
        case eEnvDecay:
        {
            step                       = 1.0 / (spec->decay * gSampleRate);
            gEnvProgress[voice][node] += step;

            if (gEnvProgress[voice][node] >= 1.0) {
                gEnvProgress[voice][node] = 0.0;
                level                     = spec->sustain;
                gEnvStage[voice][node]    = eEnvSustain;
            } else {
                level = spec->sustain
                        + ((1.0 - spec->sustain) * env_fall_level((uint32_t)spec->wave, gEnvProgress[voice][node]));
            }

            if (level <= spec->sustain) {
                level                  = spec->sustain;
                gEnvStage[voice][node] = eEnvSustain;
            }
            break;
        }
        case eEnvSustain:
        {
            level = spec->sustain;
            break;
        }
        case eEnvRelease:
        {
            step                       = 1.0 / (spec->release * gSampleRate);
            gEnvProgress[voice][node] += step;

            if (gEnvProgress[voice][node] >= 1.0) {
                gEnvProgress[voice][node] = 0.0;
                level                     = 0.0;
                gEnvStage[voice][node]    = eEnvIdle;
            } else {
                level = gEnvStart[voice][node] * env_fall_level((uint32_t)spec->wave, gEnvProgress[voice][node]);
            }

            if (level <= 0.0) {
                level                  = 0.0;
                gEnvStage[voice][node] = eEnvIdle;
            }
            break;
        }
        default:
        {
            level = 0.0;
            break;
        }
    }
    gEnvLevel[voice][node] = level;
    return level;
}

// The signal arriving at one of a node's inputs: whichever output of whichever node feeds it.
static double signal_in(const tEngineNode * spec, double value[][NODE_OUTPUTS], uint32_t input) {
    int32_t source = spec->in[input];

    if ((input >= spec->inCount) || (source < 0)) {
        return 0.0;
    }
    return value[source][spec->srcLeg[input]];
}

// notes §151
static double osc_waveform(uint32_t voice, uint32_t node, const tEngineNode * spec, double phase, double dt, double shape) {
    SE_LOCAL;

    // The shape oscillators have their own eight waveforms, and Shape morphs each of them rather
    // than acting as a pulse width, so they do not share the switch below.
    if (spec->kind == eNodeOscShp) {
        return osc_shp_wave((uint32_t)spec->wave, phase, dt, shape);
    }

    switch (spec->wave) {
        case eOscWaveSine:
        {
            return sin(phase * 2.0 * M_PI);
        }
        case eOscWaveTriangle:
        {
            // notes §152
            return osc_triangle(phase, 0.5);
        }
        case eOscWaveSaw:
        {
            return osc_saw(phase, dt);
        }
        case eOscWaveSquare:
        {
            return osc_square(phase, dt, shape);
        }
        case eOscWaveSuper:
        {
            // Approximation: three saws a few cents apart. The G2's own "sup" is a different
            // algorithm — see the header.
            double up   = dt * 1.0059;    // about +10 cents
            double down = dt * 0.9941;    // about -10 cents
            double sum  = osc_saw(phase, dt);

            sum += osc_saw(advance_phase(&gSuperPhase[voice][node][0], up), up);
            sum += osc_saw(advance_phase(&gSuperPhase[voice][node][1], down), down);
            return sum / 3.0;
        }
        default:
        {
            return 0.0;
        }
    }
}

static double osc_frequency_hz(const tEngineNode * spec, double voicePitch, double pitchDirect, double pitchVar) {
    double pitch = spec->basePitch;

    // Kbt on transposes the played note by the oscillator's offset from unity; Kbt off leaves the
    // keyboard disconnected and the oscillator holds the pitch Tune names.
    if ((spec->oscKbt == true) && (voicePitch >= 0.0)) {
        pitch = voicePitch + (spec->basePitch - OSCB_TUNE_UNITY);
    }

    // notes §153
    if ((pitchDirect != 0.0) || ((spec->modAmount > 0.0) && (pitchVar != 0.0))) {
        pitch += (pitchDirect + (pitchVar * spec->modAmount)) * PITCH_MOD_SEMITONES;
    }
    // exp2, not pow(2, x). Identical result, and this runs once per oscillator per voice per
    // oversampled sample — at fifteen voices that is a few million calls a second.
    return 440.0 * exp2((pitch - MIDI_NOTE_A440) / 12.0);
}

// notes §154
static double oscillator_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double voicePitch,
                              double pitchDirect, double pitchVar, double shape) {
    SE_LOCAL;
    double   frequency = 0.0;
    double   dt        = 0.0;
    double   sum       = 0.0;
    uint32_t step      = 0;
    uint32_t tap       = 0;

    frequency = osc_frequency_hz(spec, voicePitch, pitchDirect, pitchVar);

    // notes §155
    if (frequency > (gSampleRate * 0.5)) {
        return 0.0;
    }
    dt        = frequency / (gSampleRate * (double)OSC_OVERSAMPLE);

    for (step = 0; step < OSC_OVERSAMPLE; step++) {
        double phase = advance_phase(&gPhase[voice][node], dt);

        gOscHistory[voice][node][gOscHistoryPos[voice][node]] = (float)osc_waveform(voice, node, spec, phase, dt, shape);
        gOscHistoryPos[voice][node]                           = (gOscHistoryPos[voice][node] + 1) % OSC_DECIMATE_TAPS;
    }

    // notes §156
    {
        const float * history = gOscHistory[voice][node];
        uint32_t      oldest  = gOscHistoryPos[voice][node];

        for (tap = 0; tap < OSC_DECIMATE_TAPS; tap++) {
            sum += (double)history[oldest] * gOscDecimate[OSC_DECIMATE_TAPS - 1 - tap];
            oldest++;

            if (oldest >= OSC_DECIMATE_TAPS) {
                oldest = 0;
            }
        }
    }

    return sum;
}

// notes §157
static double lfo_step(uint32_t voice, uint32_t node, const tEngineNode * spec) {
    SE_LOCAL;

    double phase = advance_phase(&gPhase[voice][node], spec->rateHz / gSampleRate);
    double wave  = 0.0;

    if (spec->active == false) {
        return 0.0;
    }

    if (spec->shpWave == true) {
        // LfoShpA's six shapes, Shape morphing each one. lfoShpAWaveStrMap order.
        switch ((uint32_t)spec->wave) {
            case 1:
            {
                wave = -cos(phase * 2.0 * M_PI);
                break;
            }                                                                          // CosBell
            case 2:
            {
                wave = (osc_triangle(phase, 0.5) + 1.0) - 1.0;
                break;
            }                                                                          // TriBell
            case 3:
            {
                wave = osc_triangle(phase, 0.5 + (0.49 * spec->shape));
                break;
            }                                                                           // Saw>Tri
            // notes §158
            case 4:                                                                     // Sqr2Tri
            {
                double tri = osc_triangle(phase, 0.5);
                double dry = 1.0 + (20.0 * spec->shape);

                wave = tanh(tri * dry) / tanh(dry);
                break;
            }
            case 5:
            {
                wave = (phase < (0.5 + (0.49 * spec->shape))) ? 1.0 : -1.0;
                break;
            }                                                                                // Pulse
            default:
            {
                wave = sin(phase * 2.0 * M_PI);
                break;
            }                                                                                // Sine
        }
    } else {
        // lfoWaveStrMap: Sin, Tri, Saw, Squ, RndSt, Rnd. The two random settings step a new value
        // once per cycle; Rnd smooths between steps where RndSt jumps.
        switch ((uint32_t)spec->wave) {
            case 1:
            {
                wave = osc_triangle(phase, 0.5);
                break;
            }
            case 2:
            {
                wave = (2.0 * phase) - 1.0;
                break;
            }
            case 3:
            {
                wave = (phase < 0.5) ? 1.0 : -1.0;
                break;
            }
            case 4:
            case 5:
            {
                if (phase < gLfoLastPhase[voice][node]) {
                    gLfoTarget[voice][node] = ((double)rand() / (double)RAND_MAX * 2.0) - 1.0;
                }
                wave = (spec->wave == 4) ? gLfoTarget[voice][node]
                       : (gLfoHeld[voice][node] + ((gLfoTarget[voice][node] - gLfoHeld[voice][node]) * phase));

                if (phase < gLfoLastPhase[voice][node]) {
                    gLfoHeld[voice][node] = gLfoTarget[voice][node];
                }
                break;
            }
            default:
            {
                wave = sin(phase * 2.0 * M_PI);
                break;
            }
        }
    }
    gLfoLastPhase[voice][node] = phase;

    {
        double unipolar = (wave + 1.0) * 0.5;

        switch (spec->polarity) {
            case 1:
            {
                return 1.0 - unipolar;
            }                                      // PosInv
            case 2:
            {
                return -unipolar;
            }                                      // Neg
            case 3:
            {
                return unipolar - 1.0;
            }                                      // NegInv
            case 4:
            {
                return wave;
            }                                      // Bip
            case 5:
            {
                return -wave;
            }                                      // BipInv
            default:
            {
                return unipolar;
            }                                      // Pos
        }
    }
}

// notes §159
#define FLT_CONTROL_MIN         (0.0)
#define FLT_CONTROL_MAX         (127.0)

#define FLTMULTI_DAMPING_MIN    (0.02)    // §10.3 - keeps Res 127 finite

static double fltmulti_damping(double resDial) {
    return fmax(FLTMULTI_DAMPING_MIN, 1.0 - (resDial / 127.0));
}

// §10.2 - LP, BP and HP into the node's three legs.
static void fltmulti_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input, double pitchVar,
                          double pitchDirect, double voicePitch, double cutoffParam, double resonance, double legs[3]) {
    SE_LOCAL;

    double   control   = cutoffParam + ((pitchDirect + (pitchVar * spec->modAmount)) * PITCH_MOD_SEMITONES);

    if ((spec->fltKbt > 0.0) && (voicePitch >= 0.0)) {
        control += (voicePitch - MIDI_NOTE_MIDDLE_C) * spec->fltKbt;
    }
    control  = fmin(fmax(control, FLT_CONTROL_MIN), FLT_CONTROL_MAX);

    double * state     = gLadder[voice][node];            // low, band, the low before last
    double   cutoff    = fmin(flt_cutoff_hz(control), gSampleRate * 0.45);
    double   tuning    = 2.0 * sin(M_PI * cutoff / gSampleRate);
    double   damping   = fltmulti_damping(resonance * 127.0);
    double   bandScale = 1.0 - (0.5 * tuning);
    double   feedback  = 2.0 * damping * damping * bandScale;
    double   drive     = (spec->fltGainComp == true) ? (damping * input) : input;
    double   lowBefore = state[2];
    double   lowPrev   = state[0];
    double   bandPrev  = state[1];
    double   low       = lowPrev + (tuning * bandPrev);
    double   high      = drive - low - (feedback * bandPrev);
    double   band      = bandPrev + (tuning * high);

    state[0] = low;
    state[1] = band;
    state[2] = lowPrev;

    double   lowOut    = 0.25 * (low + (2.0 * lowPrev) + lowBefore);
    double   bandOut   = 0.5 * bandScale * (band + bandPrev);
    double   highOut   = bandScale * high;

    if (spec->fltSixDb == true) {                         // §10.3
        legs[0] = 0.5 * (low + band + lowPrev + bandPrev);
        legs[1] = lowOut - highOut;
        legs[2] = highOut + bandOut;
    } else {
        legs[0] = lowOut;
        legs[1] = bandOut;
        legs[2] = highOut;
    }
}

static double filter_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input, double mod, double voicePitch,
                          double cutoffParam, double resonance) {
    SE_LOCAL;

    double control = cutoffParam;
    double cutoff  = 0.0;
    double g       = 0.0;

    if (spec->active == false) {
        return input;    // a bypassed filter passes its input straight through
    }

    // notes §160
    if ((spec->modAmount > 0.0) && (mod != 0.0)) {
        control += mod * spec->modAmount * FULL_MOD_SEMITONES;
    }

    // notes §161
    if ((spec->fltKbt > 0.0) && (voicePitch >= 0.0)) {
        control += (voicePitch - MIDI_NOTE_MIDDLE_C) * spec->fltKbt;
    }

    if (control < FLT_CONTROL_MIN) {
        control = FLT_CONTROL_MIN;
    }

    if (control > FLT_CONTROL_MAX) {
        control = FLT_CONTROL_MAX;
    }
    cutoff = flt_cutoff_hz(control);

    // Nyquist guard. With the control clamp above this cannot bite at any normal device rate — the
    // top of the dial is 21.1 kHz against an engine running at 96 kHz — so it is a guard against an
    // unusually low device rate, not part of the instrument's behaviour.
    if (cutoff > (gSampleRate * 0.45)) {
        cutoff = gSampleRate * 0.45;
    }

    if (cutoff < 1.0) {
        cutoff = 1.0;
    }
    g      = 1.0 - exp(-2.0 * M_PI * cutoff / gSampleRate);

    // notes §162
    if (g > LADDER_MAX_G) {
        g = LADDER_MAX_G;
    }
    // notes §163
#define LADDER_K_MAX    (4.3)

    switch (spec->topology) {
        case eFilterTopologyCascadeHP:
        {
            return cascade_hp_filter(gLadder[voice][node], input, g, spec->tapStage + 1u);
        }
        case eFilterTopologyBiquad:
        {
            // notes §164
            double f    = 2.0 * sin(M_PI * 0.5 * g);
            double damp = 1.0 / flt_static_q(resonance * 127.0);

            if (f > 1.0) {
                f = 1.0;            // keep the section stable at the top of its range
            }
            return svf_filter(gLadder[voice][node], input, f, damp, spec->fltShape);
        }
        default:
        {
            // Ladder for FltClassic and FltNord; FltLP arrives here too with resonance 0, which
            // makes the loop a plain cascade and the tap its pole count.
            return ladder_filter(gLadder[voice][node], input, g, LADDER_K_MAX * resonance, spec->tapStage);
        }
    }
}

// notes §165
static void eval_node(uint32_t voice, uint32_t n, const tSoundEngineParams * paramsIn,
                      double value[][NODE_OUTPUTS], double voicePitch) {
    SE_LOCAL;

    const tEngineNode * spec = &paramsIn->node[n];
    double              a    = signal_in(spec, value, 0);

    value[n][0] = 0.0;
    value[n][1] = 0.0;
    value[n][2] = 0.0;

    switch (spec->kind) {
        case eNodeLfo:
        {
            value[n][0] = lfo_step(voice, n, spec);
            value[n][1] = value[n][0];

            // The panel LED, lit on the positive half of the cycle - see gModuleLed. Only voice 0
            // publishes: a polyphonic patch runs one LFO per voice and the face has one LED, and the
            // instrument shows a single blink rather than however many voices happen to be sounding.
            if (voice == 0u) {
                uint32_t lamp = METER_WRITTEN | ((value[n][0] > 0.0) ? 1u : 0u);

                if (atomic_exchange_explicit(&gModuleLed[spec->location][spec->moduleIndex],
                                             lamp, memory_order_relaxed) != lamp) {
                    atomic_store_explicit(&gMetersDirty, true, memory_order_relaxed);
                }
            }
            break;
        }
        case eNodeOsc:
        case eNodeOscShp:
        {
            // Connector 0 is the direct Pitch input, connector 1 the knob-attenuated
            // PitchVar — see oscillator_step().
            value[n][0] = (spec->active == true)
                              ? oscillator_step(voice, n, spec, voicePitch, a, signal_in(spec, value, 1),
                                                gSmoothedShape[n])
                              : 0.0;
            break;
        }
        case eNodeFilter:
        {
            // spec->fltGain is FltNord's GC and is 1.0 for every other filter, so this costs a
            // multiply and changes nothing where the module has no such control.
            value[n][0] = filter_step(voice, n, spec, a, signal_in(spec, value, 1), voicePitch,
                                      gSmoothedCutoff[n], gSmoothedRes[n]) * spec->fltGain;
            break;
        }
        case eNodeEnv:
        {
            double env = envelope_step(voice, n, spec, gVoice[voice].gate);

            // Output 0 is the envelope itself, for patching at a modulation input. Output 1
            // is whatever audio is patched into the module, shaped by that envelope — the
            // G2's envelopes carry their own VCA, and this patch uses it as the amp.
            value[n][0] = env;
            value[n][1] = a * env;
            break;
        }
        case eNodeLevAmp:
        {
            value[n][0] = a * gSmoothedGain[n];
            break;
        }
        case eNodeLevMult:
        {
            value[n][0] = a * signal_in(spec, value, 1);
            break;
        }
        case eNodePulse:
        {
            value[n][0] = pulse_step(voice, n, a, spec);
            break;
        }
        case eNodeFltMulti:
        {
            if (spec->active == false) {
                value[n][0] = a;
                value[n][1] = a;
                value[n][2] = a;
                break;
            }
            double legs[3] = {0.0, 0.0, 0.0};

            fltmulti_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2), voicePitch,
                          gSmoothedCutoff[n], gSmoothedRes[n], legs);
            value[n][0] = legs[0];
            value[n][1] = legs[1];
            value[n][2] = legs[2];
            break;
        }
        case eNodeEq:
        {
            value[n][0] = (spec->active == true) ? eq_step(voice, n, spec, a) : a;
            break;
        }
        case eNodeOscNoise:
        {
            double width = spec->oscNoiseWidth
                           + (MOD_INPUT_SCALE * spec->oscNoiseWidthMod * signal_in(spec, value, 2));
            double hz    = osc_frequency_hz(spec, voicePitch, a, signal_in(spec, value, 1));

            value[n][0] = (spec->active == true) ? oscnoise_step(voice, n, hz, width) : 0.0;
            break;
        }
        case eNodeNoise:
        {
            double white = white_noise(&gNoiseSeed[voice][n]);

            gNoiseLp[voice][n] = (spec->noisePole * gNoiseLp[voice][n])
                                 + ((1.0 - spec->noisePole) * spec->noiseGain * white);
            value[n][0]        = (spec->active == true) ? gNoiseLp[voice][n] : 0.0;
            break;
        }
        case eNodeMixStereo:
        {
            double left  = 0.0;
            double right = 0.0;

            for (uint32_t c = 0; c < spec->inCount; c++) {
                double in = signal_in(spec, value, c);

                left  += in * gSmoothedLevel[n][2u * c];
                right += in * gSmoothedLevel[n][(2u * c) + 1];
            }

            value[n][0] = left;
            value[n][1] = right;
            break;
        }
        case eNodeFade:
        {
            bool   oneIn = (spec->fadeKind == eFadePan) || (spec->fadeKind == eFadeOneToTwo);
            double pos   = gSmoothedShape[n] + (MOD_INPUT_SCALE * spec->fadeMod * signal_in(spec, value, oneIn ? 1u : 2u));
            double wa    = 0.0;
            double wb    = 0.0;

            fade_weights(spec, pos, &wa, &wb);

            if (oneIn) {
                value[n][0] = a * wa;       // L / Out1
                value[n][1] = a * wb;       // R / Out2
            } else {
                value[n][0] = (a * wa) + (signal_in(spec, value, 1) * wb);
                value[n][1] = value[n][0];
            }
            break;
        }
        case eNodeShaper:
        {
            // `a` is input leg 0. WaveWrap is the one shaper whose Mod jack comes first, so for it
            // the signal is on leg 1 and the modulation on leg 0 - spec->shaperIn says which.
            double sig = (spec->shaperIn == 0) ? a : signal_in(spec, value, 1);
            double mod = (spec->shaperIn == 0) ? signal_in(spec, value, 1) : a;

            value[n][0] = shaper_step(sig, mod, spec);
            break;
        }
        case eNodeMix:
        {
            uint32_t c           = 0;

            // notes §166
            bool     stereoPairs = spec->mixStereo;
            double   legScale    = stereoPairs ? 0.5 : 1.0;

            for (c = 0; c < spec->inCount; c++) {
                uint32_t channel = stereoPairs ? (c / 2) : c;

                value[n][0] += signal_in(spec, value, c) * legScale * gSmoothedLevel[n][channel];
            }

            break;
        }
        case eNodeChorus:
        {
            if (spec->active == true) {
                chorus_step(n, a, spec->depth, spec->amount, &value[n][0], &value[n][1]);
            } else {
                value[n][0] = a;
                value[n][1] = a;
            }
            break;
        }
        case eNodeCompress:
        {
            value[n][0] = (spec->active == true) ? compress_step(voice, n, a, spec) : a;
            value[n][1] = value[n][0];
            break;
        }
        case eNodeDelay:
        {
            value[n][0] = (spec->active == true)
                              ? delay_step(spec->line, a, spec->timeSeconds, spec->depth,
                                           spec->damping, spec->hpCoeff, spec->amount) : a;
            value[n][1] = value[n][0];
            break;
        }
        case eNodeReverb:
        {
            // Only the first reverb in a chain is modelled; see the DSP note above.
            double in = (a + signal_in(spec, value, 1)) * 0.5;

            if ((spec->active == true) && (spec->line == 0)) {
                reverb_step(in, spec->timeSeconds, spec->timeNorm, spec->brightness,
                            spec->amount, spec->reverbType, &value[n][0], &value[n][1]);
            } else {
                value[n][0] = in;
                value[n][1] = in;
            }
            break;
        }
        case eNodeConstant:
        {
            value[n][0] = spec->constant;
            value[n][1] = spec->constant;
            break;
        }
        case eNodeFxIn:
        {
            // The two legs stay apart — see the bridge in add_node() for why leg 1 is resolved at
            // all. `a` is the feeder's left, input 1 its right.
            value[n][0] = (spec->active == true) ? (a * gSmoothedGain[n]) : 0.0;
            value[n][1] = (spec->active == true)
                          ? (signal_in(spec, value, 1) * gSmoothedGain[n]) : 0.0;
            break;
        }
        case eNodePassThru:
        {
            value[n][0] = a;
            value[n][1] = a;         // stereo pairs feed both legs from the one signal
            break;
        }
        case eNodeOut:
        {
            // notes §167
            if (spec->active == true) {
                bool   haveLeft  = (spec->inCount > 0) && (spec->in[0] >= 0);
                bool   haveRight = (spec->inCount > 1) && (spec->in[1] >= 0);
                double left      = a;
                double right     = signal_in(spec, value, 1);

                if (haveLeft == false) {
                    left = right;
                }

                if (haveRight == false) {
                    right = left;
                }
                value[n][0] = left * gSmoothedGain[n];
                value[n][1] = right * gSmoothedGain[n];
            }
            break;
        }
        default:
        {
            break;
        }
    }

    // notes §168
    switch (spec->kind) {
        case eNodeEnv:
        case eNodeChorus:
        case eNodeReverb:
        case eNodeFade:         // writes both legs itself: Pan and Fade1-2 have two outputs
        case eNodeMixStereo:    // a genuine stereo pair
        case eNodeFltMulti:     // three outputs of its own
        case eNodeOut:
        {
            break;
        }
        default:
        {
            value[n][1] = value[n][0];
            break;
        }
    }

    // §1.1
    switch (spec->kind) {
        case eNodeMix:
        case eNodeMixStereo:
        case eNodeFxIn:
        case eNodeOut:
        {
            if (voice == 0u) {
                // BOTH LEGS, because a stereo module draws two meters and feeding only the left would
                // leave the right showing whatever the instrument last sent - which is worse than
                // showing nothing, because it looks live and is not.
                uint32_t packed = METER_WRITTEN;

                for (uint32_t leg = 0u; leg < 2u; leg++) {
                    double peak  = fabs(value[n][leg]);
                    int    level = 0;

                    if (peak > gMeterEnv[n][leg]) {
                        gMeterEnv[n][leg] = peak;
                    } else {
                        gMeterEnv[n][leg] += METER_DECAY * (peak - gMeterEnv[n][leg]);
                    }

                    if (gMeterEnv[n][leg] > 0.0) {
                        int exponent = 0;

                        // peak = f x 2^exponent with f in [0.5, 1), so 0.5..1 gives exponent 0.
                        (void)frexp(gMeterEnv[n][leg], &exponent);

                        if (exponent <= 0) {
                            level = 7 + exponent;
                        } else if (exponent == 1) {
                            level = 9;
                        } else if (exponent == 2) {
                            level = 11;
                        } else {
                            level = 12 | 0x40;      // the instrument's clip bit, alongside its top value
                        }
                    }

                    if (level < 0) {
                        level = 0;
                    }
                    packed |= ((uint32_t)level << (leg * METER_LEG_SHIFT));
                }

                if (atomic_exchange_explicit(&gModuleMeter[spec->location][spec->moduleIndex],
                                             packed, memory_order_relaxed) != packed) {
                    atomic_store_explicit(&gMetersDirty, true, memory_order_relaxed);
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

// notes §169
static void tap_pair(const tSoundEngineParams * paramsIn, int32_t node, double value[][NODE_OUTPUTS], double out[2]) {
    switch (paramsIn->node[node].kind) {
        case eNodeEnv:
        {
            out[0] = value[node][1];
            out[1] = value[node][1];
            break;
        }
        case eNodeOut:
        {
            out[0] = value[node][0];
            out[1] = value[node][1];
            break;
        }
        default:
        {
            out[0] = value[node][0];
            out[1] = value[node][0];
            break;
        }
    }
}

// notes §170
static bool voice_is_finished(const tSoundEngineParams * paramsIn, uint32_t v, bool chainHasEnvelope) {
    SE_LOCAL;

    if (gVoice[v].gate == true) {
        return false;
    }

    if (chainHasEnvelope == false) {
        return gVoice[v].envelope <= 0.0;
    }

    for (uint32_t n = 0; n < paramsIn->nodeCount; n++) {
        // Per-voice envelopes only. One after the mix is shaping the effect, not the note, and it
        // has no per-voice state to ask.
        if ((paramsIn->node[n].kind != eNodeEnv) || (paramsIn->node[n].postMix == true)) {
            continue;
        }

        if ((gEnvStage[v][n] != (uint32_t)eEnvIdle) || (fabs(gEnvLevel[v][n]) > 1.0e-5)) {
            return false;
        }
    }

    return true;
}

void sound_engine_render(float * out, uint32_t frameCount, uint32_t channelCount) {
    SE_LOCAL;

    tSoundEngineParams params;
    uint32_t           frame            = 0;
    bool               chainHasEnvelope = false;
    uint32_t           n                = 0;

    struct timespec    started          = {0};

    (void)clock_gettime(CLOCK_MONOTONIC, &started);

    if ((out == NULL) || (channelCount == 0)) {
        return;
    }
    memset(out, 0, (size_t)frameCount * channelCount * sizeof(float));

    if (atomic_load(&gActive) == false) {
        return;
    }
    params = read_params();

    if (params.topology != gSeenTopology) {
        // notes §171
        LOG_DEBUG("TOPOLOGY CHANGE %llu -> %llu, nodes %u, tap %d — delay and reverb buffers cleared\n",
                  (unsigned long long)gSeenTopology, (unsigned long long)params.topology,
                  (unsigned)params.nodeCount, params.tap);
        gSeenTopology = params.topology;
        reset_node_state();
    }
    // notes §172

    if (params.tap < 0) {
        return;
    }

    // A snapshot that has never been published carries a voice count of zero, and zero voices render
    // silence — which would look exactly like the engine being broken. One voice is the safe reading
    // of "not told yet", and it is what the engine did before it could count.
    if (params.voiceCount < 1) {
        params.voiceCount = 1;
    } else if (params.voiceCount > MAX_VOICES) {
        params.voiceCount = MAX_VOICES;
    }

    // notes §173
    for (n = 0; n < params.nodeCount; n++) {
        if ((params.node[n].kind == eNodeEnv) && (params.node[n].postMix == false)) {
            chainHasEnvelope = true;
            break;
        }
    }

    // notes §174
    if (engine_no_free_run() == false) {
        double idleSamples = (double)frameCount * (double)ENGINE_OVERSAMPLE;

        for (uint32_t v = 0; v < params.voiceCount; v++) {
            // notes §175
            bool rendered = (gVoice[v].sounding == true)
                            || ((v == 0) && (chainHasEnvelope == false) && (engine_no_free_run() == false));

            if (rendered == true) {
                continue;
            }

            for (n = 0; n < params.nodeCount; n++) {
                const tEngineNode * idle = &params.node[n];
                double              step = 0.0;

                if ((idle->kind == eNodeOsc) || (idle->kind == eNodeOscShp)) {
                    // No note is held, so the oscillator sits at the pitch Tune names - the same
                    // branch oscillator_step() takes when voicePitch is negative.
                    double freq = 440.0 * exp2((idle->basePitch - MIDI_NOTE_A440) / 12.0);

                    if ((freq > 0.0) && (freq <= (gSampleRate * 0.5))) {
                        step = freq / gSampleRate;
                    }
                } else if (idle->kind == eNodeLfo) {
                    step = idle->rateHz / gSampleRate;
                }

                if (step > 0.0) {
                    gPhase[v][n] = fmod(gPhase[v][n] + (step * idleSamples), 1.0);
                }
            }
        }
    }

    for (frame = 0; frame < frameCount; frame++) {
        uint32_t sub = 0;

        // ENGINE_OVERSAMPLE passes of the whole graph per output sample. Note events are consumed
        // inside, so they land on the finer grid too rather than being quantised to the output rate.
        for (sub = 0; sub < ENGINE_OVERSAMPLE; sub++) {
            double value[MAX_ENGINE_NODES][NODE_OUTPUTS];
            double voiceSum[MAX_ENGINE_NODES][NODE_OUTPUTS];

            // One event per sample. A chord's worth of note-ons arriving together therefore lands over
            // consecutive samples rather than all but the last being thrown away, and every note takes
            // effect where it actually arrived instead of at the next buffer boundary.
            (void)take_next_note_event();

            // notes §176
            double vibrato      = 0.0;

            if (params.vibratoSource != eVibratoOff) {
                uint32_t group = (params.vibratoSource == eVibratoWheel)
                             ? MORPH_GROUP_WHEEL : MORPH_GROUP_AFTERTOUCH;
                double   depth = (double)atomic_load(&gMorphMilli[group]) / 1000.0;

                gVibratoPhase += params.vibratoHz / gSampleRate;

                if (gVibratoPhase >= 1.0) {
                    gVibratoPhase -= 1.0;
                }
                vibrato        = (sin(gVibratoPhase * 2.0 * M_PI) * depth * params.vibratoCents) / 100.0;
            }
            double bend         = ((double)atomic_load(&gBendMilli) / 1000.0) * params.bendSemitones;
            double sample[2][2] = {{0.0, 0.0}, {0.0, 0.0}};   // [output pair][channel]

            double envelopeStep = 1.0 / (ENVELOPE_SECONDS * gSampleRate);
            double smoothCoeff  = 1.0 - exp(-1.0 / (PARAM_SMOOTH_SECONDS * gSampleRate));
            // Depends on the patch and the rate, not on the voice, so it is worked out once here
            // rather than once per voice — an exp() per voice per sample is not free at eight of them.
            double glideCoeff   = (params.glideSeconds > 0.0)
                                  ? (1.0 - exp(-4.6 / (params.glideSeconds * gSampleRate))) : 1.0;

            // PARAMETER SMOOTHING IS PER SAMPLE, NOT PER VOICE. It tracks where a knob is, which is
            // one thing however many notes are sounding — and running it inside the voice loop would
            // advance it once per voice, so a knob would sweep faster the more keys were held.
            for (n = 0; n < params.nodeCount; n++) {
                const tEngineNode * spec   = &params.node[n];
                bool                primed = gSmoothPrimed[n];

                gSmoothedShape[n]  = smooth_to(&gSmoothShape[n], spec->shape, smoothCoeff, primed);
                // notes §177
                gSmoothedCutoff[n] = smooth_to(&gSmoothCutoff[n], spec->cutoffParam, smoothCoeff, primed);
                gSmoothedRes[n]    = smooth_to(&gSmoothRes[n], spec->resonance, smoothCoeff, primed);
                gSmoothedGain[n]   = smooth_to(&gSmoothGain[n], spec->gain, smoothCoeff, primed);

                // §9.2
                for (uint32_t c = 0; c < spec->levelCount; c++) {
                    gSmoothedLevel[n][c] = smooth_to(&gSmoothLevel[n][c], spec->level[c], smoothCoeff, primed);
                }

                gSmoothPrimed[n]   = true;
            }

            memset(voiceSum, 0, sizeof(voiceSum));

            // notes §178
            for (uint32_t v = 0; v < params.voiceCount; v++) {
                tVoice * voice     = &gVoice[v];

                // notes §179
                bool     freeVoice = (v == 0) && (chainHasEnvelope == false)
                                     && (engine_no_free_run() == false);
                bool     freeRun   = (voice->sounding == false) && (freeVoice == true);

                if ((voice->sounding == false) && (freeRun == false)) {
                    continue;               // costs nothing when it is not playing
                }

                // notes §180
                if ((freeVoice == true) && (voice->gate == false) && (chainHasEnvelope == false)) {
                    voice->sounding = false;
                    freeRun         = true;
                }

                // notes §181
                if (voice->note >= 0) {
                    bool sliding = (params.glideMode == eGlideNormal)
                                   || ((params.glideMode == eGlideAuto) && (voice->glideActive == true));

                    if ((sliding == true) && (params.glideSeconds > 0.0)) {
                        voice->glidePitch += glideCoeff * ((double)voice->note - voice->glidePitch);
                    } else {
                        voice->glidePitch = (double)voice->note;
                    }
                }
                double voicePitch = voice->glidePitch + bend + vibrato;

                // The anti-click ramp, per voice. Only used when the patch has no EnvADSR to shape
                // the note itself — with one, this would just double up on it.
                double rampTarget = ((voice->gate == true) || (freeRun == true)) ? 1.0 : 0.0;

                if (voice->envelope < rampTarget) {
                    voice->envelope += envelopeStep;

                    if (voice->envelope > rampTarget) {
                        voice->envelope = rampTarget;
                    }
                } else if (voice->envelope > rampTarget) {
                    voice->envelope -= envelopeStep;

                    if (voice->envelope < rampTarget) {
                        voice->envelope = rampTarget;
                    }
                }
                voice->released = ((voice->gate == true) || (freeRun == true)) ? 0 : (voice->released + 1);

                // Past the limit, wind the voice down rather than cutting it. voice->fade reaching
                // zero is what retires it below.
                if (  (voice->gate == false)
                   && (freeRun == false)
                   && (voice->released > (uint32_t)(VOICE_MAX_TAIL_SECONDS * gSampleRate))) {
                    voice->fade -= 1.0 / (VOICE_FADE_SECONDS * gSampleRate);

                    if (voice->fade < 0.0) {
                        voice->fade = 0.0;
                    }
                }
                double level   = ((chainHasEnvelope == true) ? 1.0 : voice->envelope) * voice->fade;

                for (n = 0; n < params.nodeCount; n++) {
                    if (params.node[n].postMix == true) {
                        continue;
                    }
                    eval_node(v, n, &params, value, voicePitch);
                }

                // The voices SUM, which is what playing more than one note at once means. Only the
                // per-voice nodes are summed here — everything inside the voice was read from
                // value[] during its own pass, before the next voice overwrites it.
                double leaving = 0.0;

                for (n = 0; n < params.nodeCount; n++) {
                    if (params.node[n].postMix == true) {
                        continue;
                    }
                    voiceSum[n][0] += value[n][0] * level;
                    voiceSum[n][1] += value[n][1] * level;
                    voiceSum[n][2] += value[n][2] * level;

                    // What this voice is putting out, measured at its Out modules — the point where
                    // it leaves the voice for the mix or for the FX Area.
                    if (params.node[n].kind == eNodeOut) {
                        double magnitude = fabs(value[n][0] * level);

                        if (magnitude > leaving) {
                            leaving = magnitude;
                        }
                    }
                }

                voice->quiet = (leaving < VOICE_SILENCE) ? (voice->quiet + 1) : 0;

                // notes §182
                if (  (freeRun == false)
                   && (  (  (voice_is_finished(&params, v, chainHasEnvelope) == true)
                         && (voice->quiet > (uint32_t)(VOICE_SILENCE_SECONDS * gSampleRate)))
                      || (voice->fade <= 0.0))) {
                    voice->sounding = false;
                    voice->quiet    = 0;
                    voice->released = 0;
                    voice->fade     = 1.0;
                }
            }

            // What everything after the mix sees of the voices is their SUM.
            for (n = 0; n < params.nodeCount; n++) {
                if (params.node[n].postMix == false) {
                    value[n][0] = voiceSum[n][0];
                    value[n][1] = voiceSum[n][1];
                    value[n][2] = voiceSum[n][2];
                }
            }

            // notes §183
            for (n = 0; n < params.nodeCount; n++) {
                if (params.node[n].postMix == false) {
                    continue;
                }
                eval_node(0, n, &params, value, 0.0);
            }

            if (params.tap >= 0) {
                // Tapping a module means listening to its main output; for an envelope used as an amp
                // that is its shaped audio rather than the envelope signal. See tap_pair().
                {
                    double   first[2] = {0.0, 0.0};
                    uint32_t d        = params.node[params.tap].outDest & 1U;

                    tap_pair(&params, params.tap, value, first);
                    sample[d][0] += first[0];
                    sample[d][1] += first[1];
                }

                // notes §184
                for (uint32_t t = 0; t < params.extraTapCount; t++) {
                    double   extra[2] = {0.0, 0.0};
                    uint32_t d        = params.node[params.extraTap[t]].outDest & 1U;

                    tap_pair(&params, params.extraTap[t], value, extra);
                    sample[d][0] += extra[0];
                    sample[d][1] += extra[1];
                }
            }
            // notes §185
            {
                uint32_t rawMilli = (uint32_t)(fmax(fmax(fabs(sample[0][0]), fabs(sample[0][1])),
                                                    fmax(fabs(sample[1][0]), fabs(sample[1][1]))) * 1000.0);

                if (rawMilli > atomic_load(&gRawPeakMilli)) {
                    atomic_store(&gRawPeakMilli, rawMilli);
                }
            }

            // The gain, the knee and the clamp are all PER CHANNEL. The knee especially: shaping the
            // two channels together off a common peak would make one duck when the other got loud,
            // which is a stereo image moving under a limiter rather than an output stage.
            for (uint32_t q = 0; q < 4; q++) {
                double * sp = &sample[q >> 1][q & 1];

                *sp                           *= VOICE_GAIN;
                // notes §186
                *sp                           *= (double)atomic_load(&gOutputGainMilli) / 1000.0;

                // notes §187
                if (*sp > OUTPUT_KNEE) {
                    *sp = OUTPUT_KNEE + ((1.0 - OUTPUT_KNEE) * tanh((*sp - OUTPUT_KNEE) / (1.0 - OUTPUT_KNEE)));
                } else if (*sp < -OUTPUT_KNEE) {
                    *sp = -OUTPUT_KNEE - ((1.0 - OUTPUT_KNEE) * tanh((-*sp - OUTPUT_KNEE) / (1.0 - OUTPUT_KNEE)));
                }

                if (*sp > 1.0) {
                    *sp = 1.0;
                } else if (*sp < -1.0) {
                    *sp = -1.0;
                }
                // Every internal sample goes through the decimator; only the last of each group produces
                // an output. Feeding all of them is the point — dropping the others without filtering is
                // exactly what would fold the high end back down.
                gOutHistory[q][gOutHistoryPos] = *sp;
            }

            // ONE position for both lines: they are written in lockstep, so one cursor serves.
            gOutHistoryPos = (gOutHistoryPos + 1) % OUT_DECIMATE_TAPS;
        }

        {
            uint32_t channel      = 0;
            uint32_t tap          = 0;
            double   milli        = 0.0;
            double   outSample[4] = {0.0, 0.0, 0.0, 0.0};

            // Walked rather than recomputed, as in the oscillator decimator above and for the same
            // reason — the same taps in the same order, without a division per tap. All four
            // channels share the walk and the coefficient lookup; only the history line differs.
            {
                uint32_t oldest = gOutHistoryPos;

                for (tap = 0; tap < OUT_DECIMATE_TAPS; tap++) {
                    double coeff = gOutDecimate[OUT_DECIMATE_TAPS - 1 - tap];

                    outSample[0] += gOutHistory[0][oldest] * coeff;
                    outSample[1] += gOutHistory[1][oldest] * coeff;
                    outSample[2] += gOutHistory[2][oldest] * coeff;
                    outSample[3] += gOutHistory[3][oldest] * coeff;
                    oldest++;

                    if (oldest >= OUT_DECIMATE_TAPS) {
                        oldest = 0;
                    }
                }
            }

            milli = fmax(fmax(fabs(outSample[0]), fabs(outSample[1])),
                         fmax(fabs(outSample[2]), fabs(outSample[3]))) * 1000.0;

            if ((uint32_t)milli > atomic_load(&gPeakMilli)) {
                atomic_store(&gPeakMilli, (uint32_t)milli);
            }

            // notes §188
            for (channel = 0; channel < channelCount; channel++) {
                double v = (channelCount >= 4)
                           ? outSample[channel & 3U]
                           : (outSample[channel & 1U] + outSample[2U + (channel & 1U)]);

                out[(frame * channelCount) + channel] = (float)v;
            }
        }
    }

    // What that cost, against what it bought. frameCount / gDeviceRate is the time the buffer will
    // take to play, i.e. the whole deadline; anything approaching 100 % is the engine running out of
    // it, and what that sounds like is crackling.
    {
        struct timespec finished  = {0};

        (void)clock_gettime(CLOCK_MONOTONIC, &finished);

        double          spent     = ((double)(finished.tv_sec - started.tv_sec))
                                    + (((double)(finished.tv_nsec - started.tv_nsec)) / 1.0e9);
        double          available = (gDeviceRate > 0.0) ? ((double)frameCount / gDeviceRate) : 0.0;

        if ((available > 0.0) && (spent >= 0.0)) {
            uint32_t percent = (uint32_t)((spent / available) * 100.0);

            if (percent > atomic_load(&gLoadPercent)) {
                atomic_store(&gLoadPercent, percent);
            }
        }
    }
}

#ifdef __cplusplus
}
#endif

#ifdef SYNTHLIB_PLUGIN_BUILD

// Every bank entry of the CURRENT engine back to its starting state, generated from the declarations
// above so that no piece of engine state can be missed. A claimed engine may have been an earlier
// instance's, and its delay lines would otherwise play that instance's tail into this one.
static void engine_reset_state(void) {
    SE_LOCAL;

    memset(&gParams, 0, sizeof(gParams));
    memset(&gNoiseSeed, 0, sizeof(gNoiseSeed));     // reseeded by reset_node_state()
    memset(&gNoiseLp, 0, sizeof(gNoiseLp));
    memset(&gParamsSeq, 0, sizeof(gParamsSeq));
    memset(&gParamsWriteMutex, 0, sizeof(gParamsWriteMutex));
    memset(&gNoteQueue, 0, sizeof(gNoteQueue));
    memset(&gNoteWrite, 0, sizeof(gNoteWrite));
    memset(&gNoteRead, 0, sizeof(gNoteRead));
    memset(&gActive, 0, sizeof(gActive));
    memset(&gMorphMilli, 0, sizeof(gMorphMilli));
    memset(&gMorphPeakMilli, 0, sizeof(gMorphPeakMilli));
    memset(&gMetersDirty, 0, sizeof(gMetersDirty));
    memset(&gModuleMeter, 0, sizeof(gModuleMeter));
    memset(&gModuleLed, 0, sizeof(gModuleLed));
    memset(&gMeterEnv, 0, sizeof(gMeterEnv));
    memset(&gOutputGainMilli, 0, sizeof(gOutputGainMilli));
    memset(&gBendMilli, 0, sizeof(gBendMilli));
    memset(&gPeakMilli, 0, sizeof(gPeakMilli));
    memset(&gRawPeakMilli, 0, sizeof(gRawPeakMilli));
    memset(&gStatus, 0, sizeof(gStatus));
    memset(&gPlayingCount, 0, sizeof(gPlayingCount));
    memset(&gDeviceRate, 0, sizeof(gDeviceRate));
    memset(&gSampleRate, 0, sizeof(gSampleRate));
    memset(&gVoice, 0, sizeof(gVoice));
    memset(&gVoiceClock, 0, sizeof(gVoiceClock));
    memset(&gEngineVoices, 0, sizeof(gEngineVoices));
    memset(&gEngineLegato, 0, sizeof(gEngineLegato));
    memset(&gLoadPercent, 0, sizeof(gLoadPercent));
    memset(&gVibratoPhase, 0, sizeof(gVibratoPhase));
    memset(&gLastGoodParams, 0, sizeof(gLastGoodParams));
    memset(&gSeenTopology, 0, sizeof(gSeenTopology));
    memset(&gOutDecimate, 0, sizeof(gOutDecimate));
    memset(&gOutHistory, 0, sizeof(gOutHistory));
    memset(&gOutHistoryPos, 0, sizeof(gOutHistoryPos));
    memset(&gOscDecimate, 0, sizeof(gOscDecimate));
    memset(&gOscHistory, 0, sizeof(gOscHistory));
    memset(&gOscHistoryPos, 0, sizeof(gOscHistoryPos));
    memset(&gPhase, 0, sizeof(gPhase));
    memset(&gLfoLastPhase, 0, sizeof(gLfoLastPhase));
    memset(&gLfoTarget, 0, sizeof(gLfoTarget));
    memset(&gLfoHeld, 0, sizeof(gLfoHeld));
    memset(&gSuperPhase, 0, sizeof(gSuperPhase));
    memset(&gLadder, 0, sizeof(gLadder));
    memset(&gDelayLine, 0, sizeof(gDelayLine));
    memset(&gDelayWrite, 0, sizeof(gDelayWrite));
    memset(&gDelayDamp, 0, sizeof(gDelayDamp));
    memset(&gDelayHp, 0, sizeof(gDelayHp));
    memset(&gChorusLine, 0, sizeof(gChorusLine));
    memset(&gChorusWrite, 0, sizeof(gChorusWrite));
    memset(&gChorusLfo, 0, sizeof(gChorusLfo));
    memset(&gPulseCount, 0, sizeof(gPulseCount));
    memset(&gPulsePrev, 0, sizeof(gPulsePrev));
    memset(&gCompEnv, 0, sizeof(gCompEnv));
    memset(&gRvLfo, 0, sizeof(gRvLfo));
    memset(&gRvAddr, 0, sizeof(gRvAddr));
    memset(&gRvMem, 0, sizeof(gRvMem));
    memset(&gRvCur, 0, sizeof(gRvCur));
    memset(&gRvDamp, 0, sizeof(gRvDamp));
    memset(&gRevInLp, 0, sizeof(gRevInLp));
    memset(&gRevInLp2, 0, sizeof(gRevInLp2));
    memset(&gRevInLp3, 0, sizeof(gRevInLp3));
    memset(&gRevInLp4, 0, sizeof(gRevInLp4));
    memset(&gRvLoop, 0, sizeof(gRvLoop));
    memset(&gPreDelay, 0, sizeof(gPreDelay));
    memset(&gPreDelayPos, 0, sizeof(gPreDelayPos));
    memset(&gEnvLevel, 0, sizeof(gEnvLevel));
    memset(&gSmoothShape, 0, sizeof(gSmoothShape));
    memset(&gSmoothCutoff, 0, sizeof(gSmoothCutoff));
    memset(&gSmoothRes, 0, sizeof(gSmoothRes));
    memset(&gSmoothGain, 0, sizeof(gSmoothGain));
    memset(&gSmoothLevel, 0, sizeof(gSmoothLevel));
    memset(&gSmoothedShape, 0, sizeof(gSmoothedShape));
    memset(&gSmoothedCutoff, 0, sizeof(gSmoothedCutoff));
    memset(&gSmoothedRes, 0, sizeof(gSmoothedRes));
    memset(&gSmoothedGain, 0, sizeof(gSmoothedGain));
    memset(&gSmoothedLevel, 0, sizeof(gSmoothedLevel));
    memset(&gSmoothPrimed, 0, sizeof(gSmoothPrimed));
    memset(&gEnvProgress, 0, sizeof(gEnvProgress));
    memset(&gEnvStart, 0, sizeof(gEnvStart));
    memset(&gEnvStage, 0, sizeof(gEnvStage));
    memset(&gEnvTrigger, 0, sizeof(gEnvTrigger));
    pthread_mutex_init(&gParamsWriteMutex, NULL);
    gOutputGainMilli  = 1000;
    gStatus           = eStatusOff;
    gDeviceRate       = 48000.0;
    gSampleRate       = 96000.0;
    gEngineVoices     = 1;
    sLastTypeBank[SE] = UINT32_MAX;    // no layout yet: the first call resets
    gPatchSlot        = -1;
}
#endif

// ── Engines for documents ───────────────────────────────────────────────────────────────────────

#ifdef SYNTHLIB_PLUGIN_BUILD
static _Atomic bool gEngineClaimed[SOUND_ENGINE_MAX_ENGINES];
#endif

bool sound_engine_attach(void) {
#ifdef SYNTHLIB_PLUGIN_BUILD
    for (uint32_t i = 0; i < SOUND_ENGINE_MAX_ENGINES; i++) {
        bool expected = false;

        if (atomic_compare_exchange_strong(&gEngineClaimed[i], &expected, true)) {
            gDoc->engineIndex = i;
            engine_reset_state();
            return true;
        }
    }

    return false;
#else
    return true;    // the application's one engine is its document's from the start
#endif
}

void sound_engine_detach(void) {
    SE_LOCAL;

#ifdef SYNTHLIB_PLUGIN_BUILD
    uint32_t index = gDoc->engineIndex;

    atomic_store(&gActive, false);

    if (index < SOUND_ENGINE_MAX_ENGINES) {
        atomic_store(&gEngineClaimed[index], false);
    }
#endif
}

uint32_t sound_engine_index(void) {
    SE_LOCAL;

    return SE;
}

void sound_engine_bind_slot(int32_t slot) {
    SE_LOCAL;

    gPatchSlot = ((slot >= 0) && (slot < MAX_SLOTS)) ? slot : -1;
}
