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

// notes §179 - voice 0 runs with no key held. Drone mode does it for every patch, as the instrument does;
// without it only a patch with no envelope, which is the only kind audible at rest.
static bool free_voice_runs(uint32_t v, bool chainHasEnvelope, bool droneMode) {
    return (v == 0) && (engine_no_free_run() == false) && ((droneMode == true) || (chainHasEnvelope == false));
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
            // Freq, Res, FilterType, Bypass, GC. No slope: it is two poles, always. Its GC is not
            // FltNord's, so it is read on its own (FLTSTATIC_PARAM_GC).
            *map = (tFilterParams){
                .freq = 0, .env = -1, .kbt = -1, .res = 1, .slope = -1, .slopeMode = -1, .gc = -1, .shape = 2, .active = 3
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
#define FLTSTATIC_PARAM_GC       (4)   // §10.4 - drive x damping
#define FLT_PARAM_RES            (3)
#define FLT_PARAM_SLOPE          (4)
#define FLT_PARAM_ACTIVE         (5)

#define ENV_PARAM_SHAPE          (0)
#define ENV_PARAM_ATTACK         (1)
#define ENV_PARAM_DECAY          (2)
#define ENV_PARAM_SUSTAIN        (3)
#define ENV_PARAM_RELEASE        (4)
#define ENV_PARAM_OUT_TYPE       (5)   // posStrMap: Pos, PosInv, Neg, NegInv, Bip, BipInv
#define ENV_PARAM_KB             (6)   // the keyboard gate, not key tracking (manual p.197)
#define ENV_PARAM_RESET          (7)   // 0 Normal, 1 Reset
#define ENV_INPUT_GATE           (1)   // node input: 0 is the audio, 1 the Gate jack, 2 AM
#define ENV_INPUT_AM             (2)
#define ENV_INPUT_MOD            (3)   // §17.10 - the time-mod jacks follow, one per modulated dial

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
#define SHPB_PARAM_TUNE          (0)
#define SHPB_PARAM_CENT          (1)
#define SHPB_PARAM_KBT           (2)
#define SHPB_PARAM_PITCH_MOD     (3)
#define SHPB_PARAM_PITCH_TYPE    (4)
#define SHPB_PARAM_SHAPE         (6)
#define SHPB_PARAM_ACTIVE        (8)

// notes §6
#define SHPB_MODE_WAVEFORM       (0)

// notes §7
#define SHPA_PARAM_TUNE          (0)
#define SHPA_PARAM_CENT          (1)
#define SHPA_PARAM_KBT           (2)
#define SHPA_PARAM_PITCH_MOD     (3)
#define SHPA_PARAM_SHAPE         (7)
#define SHPA_PARAM_WAVEFORM      (9)
#define SHPA_PARAM_ACTIVE        (10)

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
    {moduleTypeOscB,     0, 1, 2,  3,  4,  9,  8, -1,  6, false},
    {moduleTypeOscA,     0, 1, 2,  3,  6,  5,  4, -1, -1, true },
    {moduleTypeOscC,     0, 1, 2,  7,  3,  5, -1,  0, -1, true },             // FmM 4, FM type 6: FM not modelled, as on OscB
    // OscD has NO Pitch Type menu - five parameters, and 3 is its "Pitch" mod dial. This said 3,
    // which read that dial as the type; harmless while anything above Semi was refused, and wrong
    // the moment §6.1a started acting on it. -1 is "always Semi". Whether that dial is the PitchVar
    // attenuator, and so belongs in pmod, is the open question in todo.md - not assumed here.
    {moduleTypeOscD,     0, 1, 2, -1, -1,  4, -1,  0, -1, true },
    {moduleTypeOscNoise, 0, 1, 2,  3,  4,  7, -1, -1, -1, false},
    {moduleTypeOscDual,  0, 1, 2,  3,  4, 10, -1, -1, -1, false},
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

#define MIX_CURVE_LIN    (1)            // expStrMap is {"Exp", "Lin", "dB"} — Lin is the middle one

// StChorus: a detune depth and an amount, then its power button.
// §29 - ModAmt, and §30 - SwOnOffT. Parameter order validated against the G2 (param-validation.md).
#define MODAMT_PARAM_DEPTH     (0)
#define MODAMT_PARAM_ENABLE    (1)
#define MODAMT_PARAM_EXPLIN    (2)
#define MODAMT_PARAM_MODE      (3)    // the m/1-m button
#define MODAMT_EXPLIN_LIN      (1)    // expStrMap {Exp, Lin, dB}
#define SWITCH_PARAM_ON        (0)
#define LOGIC_HIGH_LEVEL       (1.0)  // a logic HIGH is 64 units (manual p.233), which is 1.0 in the engine (§16)

// §31-§36 - the routing, level and keyboard modules 01 Mini Emulator needs. Parameter order is the
// module tables' own, which param-validation.md has against the G2.
#define LEVCONV_PARAM_OUT         (0)    // posStrMap {Pos, PosInv, Neg, NegInv, Bip, BipInv}
#define LEVCONV_PARAM_IN          (1)    // levConvStrMap {Bip, Pos, Neg}
#define LEVADD_PARAM_VALUE        (0)
#define LEVADD_PARAM_BIP_UNI      (1)    // bipUniStrMap, 0 is BIPOLAR (§16.1)
#define SWSEL_PARAM_SELECT        (0)    // Sw2-1 and Sw8-1: which input is through
#define SWSEL_CTRL_UNITS          (4.0)  // §33 - In 1 is 0 units, In 2 is 4, ... In 8 is 28
#define UNITS_PER_FULL_SCALE      (64.0) // §16 - a signal of 1.0 in the engine is 64 units on the G2
#define VALSW_PARAM_VALUE         (0)    // the Ctrl threshold, 0-64 units in whole steps
#define VALSW_VALUE_TOP           (63)   // the top step reads 64, not 63 (render_paramType1UniPolShort)
#define MONOKEY_PARAM_PRIORITY    (0)    // monoKeyStrMap {Last, Lo, Hi}
#define GLIDE_PARAM_TIME          (0)
#define GLIDE_PARAM_ON            (1)    // offOnStrMap, default On
#define GLIDE_PARAM_SHAPE         (2)    // logStrMap {Log, Lin}: 0 is Log
#define GLIDE_SHAPE_LIN           (1)

// §38 - the Logic group. A logic input is HIGH above zero, and a logic HIGH output is 64 units,
// which is LOGIC_HIGH_LEVEL (§16, §30).
#define GATE_PARAM_TYPE_1       (0)      // gateTypeStrMap {AND, NAND, OR, NOR, XOR, NXOR}
#define GATE_PARAM_TYPE_2       (1)
#define FLIPFLOP_PARAM_TYPE     (0)      // flipFlopStrMap {D-type, RS-type}
#define FLIPFLOP_TYPE_RS        (1)
#define CLKDIV_PARAM_DIVIDER    (0)      // reads 1 to 128: the dial plus one
#define CLKDIV_PARAM_MODE       (1)      // divModeStrMap {Gated, Toggled}
#define CLKDIV_MODE_TOGGLED     (1)

// §39 - DrumSynth, in the module table's order. Each dial's conversion is the instrument's own:
// the four decays share the ENVELOPE decay table, the levels and the three amounts an exponential
// curve, and the noise filter the FILTER cutoff table.
#define DRUM_PARAM_MASTER_FREQ     (0)
#define DRUM_PARAM_SLAVE_RATIO     (1)
#define DRUM_PARAM_MASTER_DECAY    (2)
#define DRUM_PARAM_SLAVE_DECAY     (3)
#define DRUM_PARAM_MASTER_LEVEL    (4)
#define DRUM_PARAM_SLAVE_LEVEL     (5)
#define DRUM_PARAM_NOISE_FREQ      (6)
#define DRUM_PARAM_NOISE_RES       (7)
#define DRUM_PARAM_NOISE_SWEEP     (8)
#define DRUM_PARAM_NOISE_DECAY     (9)
#define DRUM_PARAM_NOISE_TYPE      (10)
#define DRUM_PARAM_BEND_AMOUNT     (11)
#define DRUM_PARAM_BEND_DECAY      (12)
#define DRUM_PARAM_CLICK           (13)
#define DRUM_PARAM_NOISE_AMOUNT    (14)
#define DRUM_PARAM_ON              (15)
// §39.3 - MEASURED off the instrument 2026-09-19. NOT one shared curve and NOT the engine's
// own level law: each is amplitude = (dial/127)^p, fitted to better than 0.9 dB rms over 45 dB.
// Click and Bend Amount are not measured yet and stay on the engine's level curve.

#define DRUM_SWEEP_OCTAVES      (5.0)
// §39.8 - the click is a one-pole decay, not a ramp: the instrument multiplies it by this each
// sample at 96 kHz, and its peak is a QUARTER of the dialled level.
#define DRUM_INSTRUMENT_RATE    (96000.0)   // the rate the instrument's own coefficients are for
#define DRUM_CLICK_DECAY_96K    (0.780851)
#define DRUM_CLICK_PEAK         (0.25)

#define DRUM_MASTER_PHASE       (0)
#define DRUM_SLAVE_PHASE        (1)
#define DRUM_MASTER_ENV         (2)
#define DRUM_SLAVE_ENV          (3)
#define DRUM_NOISE_ENV          (4)
#define DRUM_BEND_ENV           (5)
#define DRUM_TICK               (6)
#define DRUM_CLICK_ENV          (7)
#define DRUM_STATE_SLOTS        (8)
#define DRUM_LED_FLOOR          (1.0 / 128.0)   // §39.5 - the lamp is out once the hit is inaudible

#define CHORUS_PARAM_DETUNE     (0)
#define CHORUS_PARAM_AMOUNT     (1)
#define CHORUS_PARAM_ACTIVE     (2)

// Compress: threshold and reference level run 0..42, ratio 0..66.
#define COMP_PARAM_THRESHOLD    (0)
#define COMP_PARAM_RATIO        (1)
#define COMP_PARAM_ATTACK       (2)
#define COMP_PARAM_RELEASE      (3)
#define COMP_PARAM_REFLVL       (4)
#define COMP_PARAM_ACTIVE       (6)

// Read off the instrument's own dial displays, not guessed. See where they are used.

// notes §9
#define DELAY_PARAM_TIME        (0)
#define DELAY_PARAM_FEEDBACK    (1)
#define DELAY_PARAM_LP          (2)     // DelayA calls this Filter; both are a damping control
#define DELAY_PARAM_DRYWET      (3)
#define DELAY_PARAM_HP          (8)
#define DELAYB_PARAM_FBMOD      (5)     // §24.6
#define DELAYB_PARAM_MIXMOD     (6)

// notes §10

// notes §11
#define DELAYA_PARAM_ACTIVE    (4)
#define DELAYB_PARAM_ACTIVE    (7)
#define DELAY_MODE_RANGE       (0)

#define REVERB_PARAM_TIME      (0)
#define REVERB_PARAM_BRIGHT    (1)
#define REVERB_PARAM_DRYWET    (2)
#define REVERB_PARAM_ACTIVE    (3)
#define RV_POSITIONS           (36)         // §20.2 - kRvPlace
#define RV_COEFFS              (10)         // §20.3

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
#define CONST_PARAM_BIP_UNI    (1)    // bipUniStrMap: 0 is Bipolar, 1 Unipolar - §16.1

#define FXIN_PARAM_ACTIVE      (1)
#define FXIN_PARAM_PAD         (2)    // db12PadStrMap: +6 dB, 0 dB, -6 dB, -12 dB

// §9.1
#define MAX_NODE_INPUTS        (10)

// §9.3
#define NODE_OUTPUTS           (6)

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
#define FULL_MOD_SEMITONES      (64.0)

// notes §14
#define PITCH_MOD_SEMITONES     (64.0)

// §26 - the Keyboard module's six outputs, in its connector order
#define KEYBOARD_OUT_PITCH      (0)
#define KEYBOARD_OUT_GATE       (1)
#define KEYBOARD_OUT_LIN        (2)
#define KEYBOARD_OUT_RELEASE    (3)
#define KEYBOARD_OUT_NOTE       (4)
#define KEYBOARD_OUT_EXP        (5)
#define KEYBOARD_OUTPUTS        (6)
#define KEYBOARD_PITCH_ZERO     (64.0)    // E4 is 0 units (manual p.158)

// notes §15
static double type_ii_attenuator(double dial) {
    return mix_level_gain(dial);
}

// Aftertouch's morph group. The G2 hard-wires the eight — morphStrMap lists them Wheel, Vel, Keyb,
// Aft.Tch, ... — so aftertouch is group 3. midiInput.c has the same constant for the same reason.
#define MORPH_GROUP_WHEEL         (0)
#define MORPH_GROUP_VELOCITY      (1)    // §26.2 - per voice, not from gMorphMilli
#define MORPH_GROUP_KEYBOARD      (2)    // §26.2 - per voice, from its note
#define MORPH_GROUP_AFTERTOUCH    (3)
#define MORPH_GROUP_SUSTAIN       (4)    // §26.3 - the sustain pedal, which also holds the notes

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
    eOscWaveDualSaw,
    eOscWaveDual = 100,    // §12 - OscDual's own mix, never a selectable waveform
} tOscWave;

// §6.3
#define OSC_INSTRUMENT_RATE         (96000.0)
#define OSC_EDGE_SAMPLES            (2.0)
#define OSC_CORNER_LIMIT_MULTI      (2.0)  // OscA, OscB
#define OSC_CORNER_LIMIT_PARTS      (1.0)  // OscC, OscD

// notes §16
#define OSCB_TUNE_UNITY             (64.0)
#define MIDI_NOTE_A440              (69.0)
#define MIDI_NOTE_MIDDLE_C          (60.0)
#define KBT_REFERENCE_NOTE          (64.0)    // §21.3 - the instrument's pitch zero, E4: where KBT moves nothing

// notes §17
#define VOICE_GAIN                  (0.15)

// Where the output starts bending rather than shearing.
#define OUTPUT_KNEE                 (0.80)
#define ENVELOPE_SECONDS            (0.005) // the anti-click ramp used when no EnvADSR is in the chain

// Every ladder runs its full four poles whatever slope is selected — see ladder_filter().
#define LADDER_POLES                (6) // state available: FltLP's 36 dB setting is six poles
#define FILTER_STATE_SLOTS          (8) // per filter node: FltNord's two stages need eight (§23.1)
#define LADDER_LOOP_POLES           (4) // the RESONANCE loop is four long whatever is tapped - measured

// notes §18
#define MAX_ENGINE_NODES            (128)
#define MAX_DX_OPERATORS            (24) // §14 - six per DXRouter, so four routers

// §14 - Operator and DXRouter. See the reference for what each of these is and how sure it is.
#define DX_LEVEL_TOP                (99.0)   // Level and L1-L4 at the DX's top value read full scale
#define DX_LEVEL_DB_PER_STEP        (0.75)
#define DX_SILENT_DB                (-96.0)
#define DX_RATE_SLOWEST_SECONDS     (40.0)   // a full 96 dB sweep at rate 0 ...
#define DX_RATE_OCTAVES_PER_STEP    (0.1544) // ... halving every 6.5 steps, to 1 ms at rate 99
#define DX_DETUNE_CENTS_PER_STEP    (1.0)
#define DX_KBSCALE_FULL_DB          (24.0)   // a full depth's offset ...
#define DX_KBSCALE_SPAN_NOTES       (48.0)   // ... this far from the break point
#define DX_E4_HZ                    (329.6276)
#define DX_FM_CYCLES_PER_UNIT       (1.0)    // phase deviation, in cycles, per unit at an FM input

// notes §19
#define MAX_VOICES                  (32)

// What counts as an inaudible voice, and how long it has to stay that way before the voice can be
// handed to another note. -80 dB is below anything that survives the output stage; the window is
// long enough that a waveform passing through zero cannot be mistaken for silence.
#define VOICE_SILENCE             (1.0e-4)
#define VOICE_SILENCE_SECONDS     (0.02)

// notes §20
#define VOICE_MAX_TAIL_SECONDS    (2.0)
#define VOICE_FADE_SECONDS        (0.03)

// §17.1 - a linear attack steps full scale in whole increments at ENV_TICK_HZ, so its time is the
// dial's time rounded to the increment below; the Log and Exp attacks keep the dial's time. Up here
// rather than beside the envelope code because the steal below is counted in its ticks.
#define ENV_TICK_HZ    (24000.0)

// §15.3a - how long a STOLEN voice is held with its gate LOW before the note that took it trigs.
// The instrument runs the DSP a whole pass here; one envelope tick is the least that guarantees
// every envelope has SEEN the gate down, which is what makes the gate edge a real one.
#define VOICE_STEAL_GATE_TICKS    (1.0)

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
    eNodeFltComb,        // §13
    eNodeDx,             // §14 - a DXRouter and the Operators patched into it, as one node
    eNodeKeyboard,       // §26 - the voice's key as six signals
    eNodeModAmt,         // §29 - ModAmt: the Depth dial scales In by the Mod input
    eNodeSwitch,         // §30 - SwOnOffT: closed passes In, open outputs nothing
    eNodeLevConv,        // §31 - reads one range and writes another
    eNodeLevAdd,         // §32 - adds its dial to In
    eNodeSwSelect,       // §33 - Sw2-1 and Sw8-1: one of n inputs, plus a Ctrl output
    eNodeValSw,          // §34 - ValSw2-1: In 2 once Ctrl reaches the threshold
    eNodeMonoKey,        // §35 - the keyboard's last/lowest/highest key, shared by every voice
    eNodeGlide,          // §36 - a slew for control signals
    eNodeAudioIn,        // §37 - 2-In: the engine has no audio input, so silence
    eNodeInvert,         // §38.1 - two logic inverters
    eNodeGate,           // §38.2 - two two-input gates, each with its own type
    eNodeFlipFlop,       // §38.3 - D-type or Set-Reset
    eNodeClkDiv,         // §38.4 - divide a clock by 1..128, Gated or Toggled
    eNodeDrumSynth,      // §39 - two oscillators, a swept noise filter, bend and click
    eNodeOut,
} tNodeKind;

// §17.9 - one segment of an envelope, in the instrument's own per-tick words (§17.3). Sized to a
// whole number of 8-byte words: tEngineNode is merged word-wise per voice (§26.2.2) and asserts on it.
#define ENV_MAX_STAGES    (ENV_GRAPH_MAX_SEGMENTS)
#define ENV_STAGE_IDLE    (0xFFFFFFFFu)

typedef struct {
    int32_t half;       // half the multiplier, Q23
    int32_t add;        // the per-tick add, Q23
    int32_t target;     // where the segment heads, in envelope steps
    uint8_t rising;     // chooses the attack-shaped recurrence and which way the end test runs
    uint8_t sustain;    // held while the gate is
    int8_t  modLeg;     // §17.10 - the node input carrying this stage's time mod, or -1
    uint8_t dial;       // its own time dial, so the mod can re-read the stage from dial + offset
    uint8_t modAmount;  // the mod amount dial
    uint8_t pad[7];
} tEnvSegment;

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
    double    oscCornerLimit;          // §6.3
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
    uint32_t        tapStage;    // which pole is tapped: 0-based, so N poles is tapStage N-1
    tFilterTopology topology;
    tFilterShape    fltShape;    // multi-mode filters only; low-pass for the rest
    double          fltGain;     // FltNord's GC attenuation; 1.0 for every other filter
    double          fltKbt;
    double          modAmount;   // how far the Env input moves the cutoff, 0..2 (the dial's 0..200%)

    double          attack;      // envelope, in seconds
    double          decay;
    double          sustain;     // 0..1
    double          release;
    int32_t         envSustainQ; // §17.6 - where the bipolar output types centre
    // §17.9 - the stage list this envelope plays, from the map every envelope module shares with its
    // own face. ADSR is the four it always was; the others are however many their map gives.
    tEnvSegment     envStage[ENV_MAX_STAGES];
    uint32_t        envStageCount;
    int32_t         envSustainStage; // the held stage, or -1: a gate release jumps past it
    uint32_t        envOutType;      // §17.6
    bool            envReset;        // §17.7
    bool            envKeyGate;      // §17.4 - the keys gate it: KB on, or a Gate jack fed by a module not played

    double          gain;            // LevAmp
    double          pulseSeconds;    // Pulse gate width
    uint32_t        outDest;         // Out module: 0 = outputs 1/2, 1 = outputs 3/4

    double          depth;           // chorus detune depth, and the delay's feedback
    double          amount;          // chorus wet amount, delay/reverb dry-wet
    double          timeSeconds;     // delay time
    double          damping;         // delay LP / reverb brightness, 0..1
    double          hpCoeff;         // delay HP in the feedback loop; 0 = filter off
    double          threshold;       // compressor
    double          ratio;
    double          refLevel;        // the level the compressor drives TOWARDS - see compress_step()
    double          attackCoeff;
    double          releaseCoeff;
    // Shaper group. Stored rather than re-derived from the module type so the render loop never
    // reaches back into the patch database.
    tShaperSettings shaper;

    double          constant;        // Constant module's value
    // Fade family (§4); the position rides on the shape smoother.
    double          noisePole;       // Noise: the one-pole low-pass's feedback coefficient, from Color
    double          noiseGain;       // and the gain that keeps its level where the instrument's is
    double          oscNoiseWidth;   // §8.3, as a dial fraction
    double          oscNoiseWidthMod;
    bool            fltSixDb;        // FltMulti dB/Oct: 0 is 6 dB
    bool            fltGainComp;     // FltMulti GComp
    tEqBands        eq;              // §11
    double          dualSquareLevel; // §12
    double          dualSawLevel;
    double          dualSubLevel;
    double          dualSawPhase;      // a fraction of a cycle
    double          dualPwMod;
    double          dualPhaseMod;
    bool            dualSoft;
    double          combFeedback;        // §13.3 - g, -1..1
    double          combFbMod;
    double          delayFbMod;          // §24.6 - DelayB's modulation amounts, as words
    double          delayMixMod;
    double          compMakeup;          // §25.1 - the Compressor's make-up gain word
    uint32_t        combType;            // Notch, Peak, Deep
    double          combLevel;
    uint32_t        fadeKind;            // tFadeKind
    double          fadeMod;             // the modulation attenuator, 0..1
    bool            fadeLog;             // logStrMap {Log, Lin}: 0 is Log. The two faders have no choice
    uint32_t        line;                // which shared delay line this node owns, if it needs one
    uint32_t        reverbType;          // reverb room size: Small/Medium/Large/Hall
    int32_t         rvPos[RV_POSITIONS]; // §20.2 - ring positions, samples ahead of the cursor
    double          rvY[RV_COEFFS];      // §20.3
    double          rvDry;               // §20.5
    double          rvWet;

    bool            modAmtOneMinus; // §27 - ModAmt's m/1-m button: In stays at full level at Depth 0

    uint32_t        levConvIn;      // §31 - the range read,  levConvStrMap {Bip, Pos, Neg}
    uint32_t        levConvOut;     // §31 - the range written, posStrMap {Pos, PosInv, ... BipInv}
    uint32_t        select;         // §33 - which input a Sw2-1/Sw8-1 passes; §35 MonoKey's priority
    uint32_t        gateType[2];    // §38.2 - one per gate
    uint32_t        divider;        // §38.4 - 1 to 128
    bool            logicToggled;   // §38.4 - Toggled rather than Gated; §38.3 RS rather than D

    // §39 - DrumSynth. The decays are per-ENVELOPE-TICK multipliers, as §36.1's glide is.
    double          drumMasterHz;
    double          drumSlaveRatio;
    double          drumDecay[4];      // master, slave, noise, bend
    double          drumLevel[2];      // master, slave
    double          drumNoiseHz;
    double          drumNoiseRes;
    double          drumSweepOctaves;
    double          drumBendOctaves;
    double          drumClick;
    double          drumClickDecay;   // §39.8 - per sample at the engine's rate
    double          drumNoiseLevel;
    uint32_t        drumFilterType;
    uint32_t        inputCount;     // §33 - how many inputs that switch has (2 or 8)
    double          glideCoeff;     // §36 - per ENVELOPE TICK: Log's one-pole k, or Lin's step
    bool            glideLin;       // §36 - logStrMap {Log, Lin}: 0 is Log

    uint32_t        dxBase;         // §14 - where this router's six Operators sit in dxOp[]
    uint32_t        dxAlgorithm;    // 0..31
    double          dxFeedback;     // FM units fed back, from the Feedback selector

    // Evaluated ONCE per sample, after the voices are summed, rather than once per voice. True for
    // everything in the FX Area, for the three module kinds that own a shared delay buffer wherever
    // they sit, and for anything downstream of one of those. See mark_post_mix_nodes().
    bool postMix;
} tEngineNode;

// notes §22
#define MAX_ENGINE_TAPS    (4)

// §14 - one Operator patched into a DXRouter, as the router's node plays it.
typedef struct {
    bool     present;
    bool     active;
    bool     kbt;
    bool     sync;
    bool     fixed;
    double   ratio;
    double   fixedHz;
    double   detune;               // a frequency factor
    double   outputGain;           // Level
    double   rateDbPerSecond[4];   // R1-R4
    double   levelDb[4];           // L1-L4
    double   rateScale;            // 0..1
    double   bpNote;
    uint32_t lCurve;
    uint32_t rCurve;
    double   lDepth;               // 0..1
    double   rDepth;
} tDxOperator;

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
    tGlideMode  glideMode;              // patch-wide, not per node
    double      glideSeconds;
    double      bendSemitones;          // 0 when the patch has bend switched off
    uint64_t    topology;               // changes shape => the audio thread resets its per-node state
    uint32_t    voiceCount;             // how many voices this patch may sound at once, 1 for Mono/Legato
    uint64_t    build;                  // which build this is - the velocity table names the one it belongs to
    tEngineNode node[MAX_ENGINE_NODES];
    tDxOperator dxOp[MAX_DX_OPERATORS]; // §14 - each DXRouter node's six, from its dxBase
    uint32_t    dxOpCount;
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

// §26.2 - the nodes a per-voice morph moves (Vel, Keyb), built along that morph's axis. Written under
// gParamsWriteMutex behind their own sequence, and copied by the audio thread only when that changes.
typedef enum {
    eAxisVelocity = 0,
    eAxisKey,
    eAxisCount
} tMorphAxis;

#define VEL_MORPH_LEVELS       (32)   // velocity 0..127 in 31 steps
#define KEY_MORPH_LEVELS       (64)   // every other note, 0..126
#define KEY_MORPH_ZERO_NOTE    (36.0) // §26.2 - the Keyb morph is 0 at C1 (note 36) and full five octaves up
#define KEY_MORPH_SPAN         (60.0)
#define MAX_AXIS_LEVELS        (64)
#define MAX_VOICE_NODES        (8)
// §26.2 - how many DXRouters on one axis carry per-voice Operators. Two, not the engine's four: each
// costs 64 rows of six, and a patch with more than two morphed routers is the same rarity that
// MAX_VOICE_NODES already caps at eight.
#define MAX_VOICE_DX_NODES    (2)

// §26.2.2 - a node and a router's Operators as bitmaps of 8-byte words, so a voice can be given each
// word by whichever axis moves it. Eight bytes because every field in either struct is a double, a
// uint32 pair or smaller and none straddles that boundary.
#define MORPH_WORD_BYTES    (8u)
#define NODE_WORDS          ((uint32_t)(sizeof(tEngineNode) / MORPH_WORD_BYTES))
#define DX_SET_WORDS        ((uint32_t)((DX_OPERATORS * sizeof(tDxOperator)) / MORPH_WORD_BYTES))
#define MORPH_MASK_WORDS    (3u)       // 192 bits, checked against both of the above below

typedef struct {
    uint32_t    count;                     // nodes in the table; 0 when nothing is morphed on this axis
    int8_t      column[MAX_ENGINE_NODES];  // a node's column, or -1
    int8_t      dxSlot[MAX_ENGINE_NODES];  // §26.2 - a DXRouter node's bank of six Operators, or -1
    uint32_t    dxCount;
    tEngineNode node[MAX_AXIS_LEVELS][MAX_VOICE_NODES];
    tDxOperator dxOp[MAX_AXIS_LEVELS][MAX_VOICE_DX_NODES][DX_OPERATORS];
    // §26.2.2 - which words of each column this axis moves anywhere on it
    uint64_t    moves[MAX_VOICE_NODES][MORPH_MASK_WORDS];
    uint64_t    dxMoves[MAX_VOICE_DX_NODES][MORPH_MASK_WORDS];
} tMorphTable;

// §26.2.3 - the words BOTH axes move, built at the PAIR of amounts rather than at each axis alone.
// Only those words are held, not the whole node, which is what makes a 2-D table affordable: a node
// is 137 words and the parameters morphed on both axes are a handful.
#define PAIR_MAX_WORDS    (24u)
#define MAX_PAIR_NODES    (1u)     // one such node per patch; a second keeps the per-axis behaviour

typedef struct {
    uint8_t  word[PAIR_MAX_WORDS];   // which words of the node these are
    uint32_t wordCount;
    uint64_t value[VEL_MORPH_LEVELS][KEY_MORPH_LEVELS][PAIR_MAX_WORDS];
    // And the same for a DXRouter's six Operators, whose parameters are not in the node at all - which
    // is where this first showed up, an Operator's Level being the one dial morphed on both axes.
    uint8_t  dxWord[PAIR_MAX_WORDS];
    uint32_t dxWordCount;
    uint64_t dxValue[VEL_MORPH_LEVELS][KEY_MORPH_LEVELS][PAIR_MAX_WORDS];
} tPairTable;

typedef struct {
    uint64_t    build;                     // tSoundEngineParams.build these belong to
    tMorphTable axis[eAxisCount];
    // §26.2.2 - the nodes BOTH axes move: a voice plays a merge of the two rather than one of them
    int8_t      mergedSlot[MAX_ENGINE_NODES];
    uint32_t    mergedCount;
    int8_t      mergedDxSlot[MAX_ENGINE_NODES];   // and the same for a DXRouter's Operators
    uint32_t    mergedDxCount;
    // §26.2.3
    int8_t      pairSlot[MAX_ENGINE_NODES];
    uint32_t    pairCount;
    tPairTable  pair[MAX_PAIR_NODES];
} tVoiceMorphs;

// The word masks have to cover both objects, and neither may have a field straddling a word.
_Static_assert((sizeof(tEngineNode) % MORPH_WORD_BYTES) == 0u, "tEngineNode is not a whole number of words");
_Static_assert(((DX_OPERATORS * sizeof(tDxOperator)) % MORPH_WORD_BYTES) == 0u, "an Operator set is not a whole number of words");
_Static_assert(NODE_WORDS <= (MORPH_MASK_WORDS * 64u), "MORPH_MASK_WORDS is too small for tEngineNode");
_Static_assert(DX_SET_WORDS <= (MORPH_MASK_WORDS * 64u), "MORPH_MASK_WORDS is too small for an Operator set");

static tVoiceMorphs         gVoiceMorphsBank[SOUND_ENGINE_MAX_ENGINES];
#define gVoiceMorphs          (gVoiceMorphsBank[SE])
static _Atomic uint32_t     gVoiceMorphsSeqBank[SOUND_ENGINE_MAX_ENGINES];
#define gVoiceMorphsSeq       (gVoiceMorphsSeqBank[SE])
static tVoiceMorphs         gVoiceMorphsAudioBank[SOUND_ENGINE_MAX_ENGINES];          // audio thread only
#define gVoiceMorphsAudio     (gVoiceMorphsAudioBank[SE])
static uint32_t             gVoiceMorphsSeenBank[SOUND_ENGINE_MAX_ENGINES];           // audio thread only
#define gVoiceMorphsSeen      (gVoiceMorphsSeenBank[SE])
static bool                 gVoiceMorphsUsableBank[SOUND_ENGINE_MAX_ENGINES];         // audio thread only
#define gVoiceMorphsUsable    (gVoiceMorphsUsableBank[SE])
static uint8_t              gLastRowBank[SOUND_ENGINE_MAX_ENGINES][eAxisCount];       // audio thread only
#define gLastRow              (gLastRowBank[SE])
// §26.2.2 - the merged nodes themselves, remade when a voice takes a note and when the tables change.
// Audio thread only. The `Last` pair is what a post-mix node plays, which follows the latest note.
static tEngineNode          gMergedNodeBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_VOICE_NODES];
#define gMergedNode       (gMergedNodeBank[SE])
static tEngineNode          gMergedLastBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICE_NODES];
#define gMergedLast       (gMergedLastBank[SE])
static tDxOperator          gMergedOpsBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_VOICE_DX_NODES][DX_OPERATORS];
#define gMergedOps        (gMergedOpsBank[SE])
static tDxOperator          gMergedLastOpsBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICE_DX_NODES][DX_OPERATORS];
#define gMergedLastOps    (gMergedLastOpsBank[SE])
static uint64_t             gMergedBuildBank[SOUND_ENGINE_MAX_ENGINES];   // which build the merges belong to
#define gMergedBuild      (gMergedBuildBank[SE])
static uint64_t             gBuildSerialBank[SOUND_ENGINE_MAX_ENGINES];   // under gParamsWriteMutex
#define gBuildSerial      (gBuildSerialBank[SE])
// The last build at each axis's full amount, under gParamsWriteMutex: a changed morph range shows only here.
static tSoundEngineParams   gAxisProbeBank[SOUND_ENGINE_MAX_ENGINES][eAxisCount];
#define gAxisProbe        (gAxisProbeBank[SE])
// The Vel and Keyb morph amounts a build uses. Per thread: two instances build at once on different threads.
static _Thread_local double sBuildAxis[eAxisCount];

// §26.3
static _Atomic bool         gSustainPedalBank[SOUND_ENGINE_MAX_ENGINES];
#define gSustainPedal      (gSustainPedalBank[SE])
static bool                 gSustainSeenBank[SOUND_ENGINE_MAX_ENGINES];            // audio thread only
#define gSustainSeen       (gSustainSeenBank[SE])

// notes §25
#define NOTE_QUEUE_SIZE    (64)

typedef struct {
    int32_t          note;
    uint8_t          velocity;   // note-on velocity, or the release velocity with a note-off
    bool             on;
    _Atomic uint32_t sequence;   // claim index + 1 once written; 0 means never used
} tNoteEvent;

static tNoteEvent           gNoteQueueBank[SOUND_ENGINE_MAX_ENGINES][NOTE_QUEUE_SIZE];
#define gNoteQueue    (gNoteQueueBank[SE])
static _Atomic uint32_t     gNoteWriteBank[SOUND_ENGINE_MAX_ENGINES];
#define gNoteWrite    (gNoteWriteBank[SE])
static uint32_t             gNoteReadBank[SOUND_ENGINE_MAX_ENGINES];        // audio thread only
#define gNoteRead     (gNoteReadBank[SE])

static _Atomic bool         gActiveBank[SOUND_ENGINE_MAX_ENGINES];
#define gActive       (gActiveBank[SE])

// Morph positions, 0..1, one per group. Written by the MIDI thread as controllers move, read by the
// UI thread when it builds a snapshot. Plain atomics: each is independent and a torn read is not
// possible on a value this size.
static _Atomic uint32_t     gMorphMilliBank[SOUND_ENGINE_MAX_ENGINES][NUM_MORPHS];
#define gMorphMilli    (gMorphMilliBank[SE])
// The highest each morph has reached. The live value is useless as a diagnostic — by the time you
// have let go of the key and opened a menu to look at it, it has fallen back to zero.
static _Atomic uint32_t     gMorphPeakMilliBank[SOUND_ENGINE_MAX_ENGINES][NUM_MORPHS];
#define gMorphPeakMilli     (gMorphPeakMilliBank[SE])

// notes §26
#define METER_VALUE_MASK    (0xFFu)
#define METER_WRITTEN       (1u << 8)
#define METER_LEG_SHIFT     (16u)   // leg 1 above the flag; leg 0 occupies METER_VALUE_MASK

// notes §27
static _Atomic bool         gMetersDirtyBank[SOUND_ENGINE_MAX_ENGINES];
#define gMetersDirty    (gMetersDirtyBank[SE])
static _Atomic uint32_t     gModuleMeterBank[SOUND_ENGINE_MAX_ENGINES][locationMax][MAX_NUM_MODULES];
#define gModuleMeter    (gModuleMeterBank[SE])

// notes §28
static _Atomic uint32_t     gModuleLedBank[SOUND_ENGINE_MAX_ENGINES][locationMax][MAX_NUM_MODULES];
#define gModuleLed    (gModuleLedBank[SE])

// The follower behind the level meters. Per NODE, not per voice: the face has one meter however many
// voices are sounding, and only voice 0 writes it. About 200 ms of release at 96 kHz.
#define METER_DECAY         (0.00005)
#define METER_FLOOR         (0.0078125)    // 2^-7, below which the meter law reads 0 (§1.1)
static double               gMeterEnvBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][2];
#define gMeterEnv           (gMeterEnvBank[SE])

static _Atomic int32_t      gOutputGainMilliBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 1000};
#define gOutputGainMilli    (gOutputGainMilliBank[SE])

static _Atomic int32_t      gBendMilliBank[SOUND_ENGINE_MAX_ENGINES];
#define gBendMilli          (gBendMilliBank[SE])

// notes §20 - a released voice that is still sounding goes on until it is stolen, as on the instrument.
static _Atomic bool         gDroneModeBank[SOUND_ENGINE_MAX_ENGINES]       = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = true};
#define gDroneMode    (gDroneModeBank[SE])
static bool                 gDroneSeenBank[SOUND_ENGINE_MAX_ENGINES]       = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = true};        // audio thread only
#define gDroneSeen    (gDroneSeenBank[SE])

// Highest absolute sample the audio thread has produced since this was last read. Purely a
// diagnostic — it is what lets a test say "sound is coming out" without a pair of ears.
static _Atomic uint32_t     gPeakMilliBank[SOUND_ENGINE_MAX_ENGINES];
#define gPeakMilli    (gPeakMilliBank[SE])

// The peak BEFORE the output gain, so the real headroom a patch needs is visible rather than being
// hidden by whatever the guard clamped it to.
static _Atomic uint32_t     gRawPeakMilliBank[SOUND_ENGINE_MAX_ENGINES];
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

static tSoundEngineStatus   gStatusBank[SOUND_ENGINE_MAX_ENGINES]        = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = eStatusOff};
#define gStatus              (gStatusBank[SE])
static uint32_t             gPlayingCountBank[SOUND_ENGINE_MAX_ENGINES];        // how many modules are in the rendered chain
#define gPlayingCount        (gPlayingCountBank[SE])

// notes §29
#define ENGINE_OVERSAMPLE    (2)

// §29a - THE GRAPH RATE IS CAPPED, it is not simply the device rate doubled. The G2's own audio
// rate is 96 kHz and everything in this engine is fitted against the instrument there, so a device
// already at or above that needs no oversampling at all: it was paying twice the CPU (four times, at
// 192 kHz) to run the model at a rate the instrument never uses. ENGINE_OVERSAMPLE stays as the
// MAXIMUM, since the delay and chorus lines are sized with it at compile time.
#define ENGINE_GRAPH_RATE_MIN    (88200.0)   // 44.1 kHz doubled - the lowest graph rate in use today
#define OSC_GRAPH_RATE_MIN       (176400.0)  // notes §34 fitted the decimator at 192 kHz -> 96 kHz

static uint32_t             gOversampleBank[SOUND_ENGINE_MAX_ENGINES]    = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = ENGINE_OVERSAMPLE};
#define gOversample              (gOversampleBank[SE])
static uint32_t             gOscOversampleBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 4 / ENGINE_OVERSAMPLE};
#define gOscOversample           (gOscOversampleBank[SE])

// The tempo a clock-synced module works to. The engine does not run the patch's master clock, so
// anything set to Clk needs a reference; 120 BPM is the obvious one and makes 1/4 exactly half a
// second. See the delay's Clk branch — this is a stand-in, not the hardware's tempo.
#define ENGINE_REFERENCE_BPM    (120.0)

static double               gDeviceRateBank[SOUND_ENGINE_MAX_ENGINES]    = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 48000.0};
#define gDeviceRate             (gDeviceRateBank[SE])
static double               gSampleRateBank[SOUND_ENGINE_MAX_ENGINES]    = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 96000.0};
#define gSampleRate             (gSampleRateBank[SE])

// notes §30
typedef struct {
    int32_t  note;            // MIDI note this voice holds, -1 for none
    bool     gate;            // key still down
    bool     sounding;        // rendered this block: gate open, or still releasing
    double   glidePitch;      // chases `note`; fractional, since a glide is mostly between two notes
    bool     glideActive;     // this note began while another was still held — see the Auto glide mode
    double   envelope;        // the anti-click ramp, used only when the patch has no EnvADSR
    uint64_t age;             // allocation order, so the oldest can be identified for stealing
    uint64_t queueOrder;      // §15.1a - its place in the free queue; a note-off sends it to the back
    uint32_t quiet;           // consecutive samples this voice's output has been inaudible
    uint32_t released;        // samples since its envelopes finished with the key up, 0 until then (notes §20)
    double   fade;            // 1.0 normally; driven to 0 to retire a voice that will not stop on its own
    uint32_t trigger;         // counts note-ons that restart the envelopes - see voice_note_on()
    uint8_t  velocity;        // the note-on velocity, 1-127 - the Keyboard module's Lin and Exp (§26)
    uint8_t  release;         // the release velocity, 0 until the key comes up - its Release
    uint8_t  row[eAxisCount]; // §26.2 - which row of each per-voice morph table this note plays
    bool     sustained;       // §26.3 - its key is up but the sustain pedal holds it
    // §15.3a - a note waiting on this voice's gate having been seen LOW, which is what the
    // instrument's allocator waits for before it trigs. Samples left to wait, the note, its velocity.
    uint32_t stealWait;
    int32_t  stealNote;
    uint8_t  stealVelocity;
} tVoice;

static tVoice             gVoiceBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES];
#define gVoice         (gVoiceBank[SE])
static uint64_t           gVoiceClockBank[SOUND_ENGINE_MAX_ENGINES];
#define gVoiceClock    (gVoiceClockBank[SE])

// notes §31
static _Atomic uint32_t   gEngineVoicesBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 1};
#define gEngineVoices    (gEngineVoicesBank[SE])

// Whether the patch is in LEGATO voice mode, the one mode where a key played while another is held
// does not restart the envelopes. Published beside gEngineVoices for the same reason: it is read by
// voice_note_on() on the audio thread, per note, where copying the snapshot to ask would be absurd.
static _Atomic bool       gEngineLegatoBank[SOUND_ENGINE_MAX_ENGINES];
#define gEngineLegato    (gEngineLegatoBank[SE])

// Mono OR Legato: the modes where releasing the sounding key goes back to one still held. §15.2
static _Atomic bool       gEngineMonoBank[SOUND_ENGINE_MAX_ENGINES];
#define gEngineMono    (gEngineMonoBank[SE])


// §15.1 - the keys held down, as a count per key. Audio thread only: voice_note_on/off keep it.
#define MIDI_KEY_COUNT    (128)
static uint8_t            gKeyHeldBank[SOUND_ENGINE_MAX_ENGINES][MIDI_KEY_COUNT];
#define gKeyHeld          (gKeyHeldBank[SE])

// §35 - the velocity each held key was played at, as the instrument keeps one beside its held
// count. Audio thread only, like gKeyHeld above.
static uint8_t            gKeyVelocityBank[SOUND_ENGINE_MAX_ENGINES][MIDI_KEY_COUNT];
#define gKeyVelocity    (gKeyVelocityBank[SE])

// notes §32
static _Atomic uint32_t   gLoadPercentBank[SOUND_ENGINE_MAX_ENGINES];
#define gLoadPercent    (gLoadPercentBank[SE])

static void reset_voices(void);
static uint32_t voice_count_for_patch(uint32_t slot);

static double             gVibratoPhaseBank[SOUND_ENGINE_MAX_ENGINES];
#define gVibratoPhase        (gVibratoPhaseBank[SE])
static tSoundEngineParams gLastGoodParamsBank[SOUND_ENGINE_MAX_ENGINES];
#define gLastGoodParams      (gLastGoodParamsBank[SE])
static uint64_t           gSeenTopologyBank[SOUND_ENGINE_MAX_ENGINES];
#define gSeenTopology        (gSeenTopologyBank[SE])

// notes §33
#define OSC_OVERSAMPLE       (4 / ENGINE_OVERSAMPLE)   // the MAXIMUM; gOscOversample is what runs
// notes §34
#define OSC_DECIMATE_TAPS    (48)

// The engine's own output filter, removing everything above the DEVICE's Nyquist before the extra
// samples are dropped. Same windowed-sinc design as the oscillators' — see the note there on why the
// transition width, not the oversampling factor, is what governs the result.
#define OUT_DECIMATE_TAPS    (64)

static double             gOutDecimateBank[SOUND_ENGINE_MAX_ENGINES][OUT_DECIMATE_TAPS];
#define gOutDecimate         (gOutDecimateBank[SE])
static double             gOutHistoryBank[SOUND_ENGINE_MAX_ENGINES][4][OUT_DECIMATE_TAPS];          // [pair*2 + channel]; one shared cursor, see the render loop
#define gOutHistory          (gOutHistoryBank[SE])
static uint32_t           gOutHistoryPosBank[SOUND_ENGINE_MAX_ENGINES];
#define gOutHistoryPos       (gOutHistoryPosBank[SE])

static double             gOscDecimateBank[SOUND_ENGINE_MAX_ENGINES][OSC_DECIMATE_TAPS];
#define gOscDecimate         (gOscDecimateBank[SE])

// notes §35
static float              gOscHistoryBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][OSC_DECIMATE_TAPS];
#define gOscHistory       (gOscHistoryBank[SE])
static uint32_t           gOscHistoryPosBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gOscHistoryPos    (gOscHistoryPosBank[SE])

static double             gPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
// §7.1
static uint32_t           gNoiseSeedBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gNoiseSeed                (gNoiseSeedBank[SE])
static uint32_t           gStartPhaseSeedBank[SOUND_ENGINE_MAX_ENGINES];
#define gStartPhaseSeed           (gStartPhaseSeedBank[SE])
#define START_PHASE_FIRST_SEED    (0x2545F491u)    // notes §63
static double             gNoiseLpBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gNoiseLp                  (gNoiseLpBank[SE])
#define gPhase                    (gPhaseBank[SE])
static double             gLfoLastPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoLastPhase             (gLfoLastPhaseBank[SE])
static double             gLfoTargetBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoTarget                (gLfoTargetBank[SE])
static double             gLfoHeldBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoHeld                  (gLfoHeldBank[SE])
static double             gLadderBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][FILTER_STATE_SLOTS];
#define gLadder                   (gLadderBank[SE])

// Delay memory. Held as float rather than double purely for size — half a second per line at any
// sensible rate, four lines, is enough for the delays a patch normally has and keeps this under a
// megabyte. Nodes beyond that many run dry rather than sharing a line and smearing into each other.
#define MAX_DELAY_LINES       (4)
// notes §36
#define DELAY_LINE_SAMPLES    (134400 * ENGINE_OVERSAMPLE)
static float              gDelayLineBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES][DELAY_LINE_SAMPLES];
#define MAX_COMB_LINES        (2)        // FltCombs per patch that sound; any more pass their input dry
#define MAX_CHORUS_LINES      (2)        // StChorus lines per patch; any more pass their input dry
#define COMB_LINE_SAMPLES     (16384)    // a power of two; §13.2's longest delay at a 96 kHz engine is 11,737
static float              gCombLineBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_COMB_LINES][COMB_LINE_SAMPLES];
#define gCombLine             (gCombLineBank[SE])
static uint32_t           gCombWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_COMB_LINES];
#define gCombWrite            (gCombWriteBank[SE])
#define gDelayLine            (gDelayLineBank[SE])
static uint32_t           gDelayWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];
#define gDelayWrite           (gDelayWriteBank[SE])
static double             gDelayDampBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];
#define gDelayDamp            (gDelayDampBank[SE])
static double             gDelayHpBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];              // the HP's lowpass half; the filter is x - this
#define gDelayHp              (gDelayHpBank[SE])
static double             gDelayHpBBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];             // §24.2 - the HP's second state
#define gDelayHpB             (gDelayHpBBank[SE])
static double             gDelayFbBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];              // §24.1 - last sample's feedback, written with this one
#define gDelayFb              (gDelayFbBank[SE])
static double             gDelayModBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES][3];          // §24.6 - FB, wet, dry, from last sample
#define gDelayMod             (gDelayModBank[SE])

// notes §37

#define CHORUS_SAMPLES     (2048 * ENGINE_OVERSAMPLE)
#define CHORUS_CHANNELS    (2)
static float              gChorusLineBank[SOUND_ENGINE_MAX_ENGINES][MAX_CHORUS_LINES][CHORUS_CHANNELS][CHORUS_SAMPLES];
#define gChorusLine        (gChorusLineBank[SE])
static uint32_t           gChorusWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_CHORUS_LINES][CHORUS_CHANNELS];
#define gChorusWrite       (gChorusWriteBank[SE])
static int32_t            gChorusPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_CHORUS_LINES];
#define gChorusPhase       (gChorusPhaseBank[SE])         // §19.2 - a signed 24-bit LFO phase
static int32_t            gChorusTrimBank[SOUND_ENGINE_MAX_ENGINES][MAX_CHORUS_LINES];
#define gChorusTrim        (gChorusTrimBank[SE])          // §19.2 - this instance's rate trim
static double             gChorusTickBank[SOUND_ENGINE_MAX_ENGINES][MAX_CHORUS_LINES];
#define gChorusTick        (gChorusTickBank[SE])
static void chorus_reset(uint32_t line);

// Pulse: the countdown still to run, and the previous input, so a rising edge can be seen. Per voice,
// because the gate is fired by that voice's own envelope.
static uint32_t           gPulseCountBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gPulseCount    (gPulseCountBank[SE])
// §36 - one Glide module's slewed output, per voice. `primed` is what makes the FIRST value it
// ever sees arrive whole instead of being glided up from zero.
static double             gGlideOutBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gGlideOut       (gGlideOutBank[SE])
static bool               gGlidePrimedBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gGlidePrimed    (gGlidePrimedBank[SE])
static double             gGlideTickBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gGlideTick      (gGlideTickBank[SE])

// §38 - what a logic module remembers between samples: the edge detectors' last input, the latched
// output, and the divider's count. `logicPrev` holds Clk in bit 0 and the D/S input in bit 1.
static uint8_t            gLogicPrevBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLogicPrev     (gLogicPrevBank[SE])
static bool               gLogicStateBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLogicState    (gLogicStateBank[SE])
static uint32_t           gLogicCountBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLogicCount    (gLogicCountBank[SE])

// §39 - one DrumSynth's per-voice state: two phases, four envelopes, the tick accumulator and the
// click. Its noise filter uses gLadder, as every other filter here does.
static double             gDrumStateBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][DRUM_STATE_SLOTS];
#define gDrumState    (gDrumStateBank[SE])

static double             gPulsePrevBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gPulsePrev    (gPulsePrevBank[SE])

// Compressor gain-reduction state, one per node.
static double             gCompEnvBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gCompEnv             (gCompEnvBank[SE])
static double             gCompGrBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];           // §25.2 - the smoothed gain reduction
#define gCompGr              (gCompGrBank[SE])
static double             gCompLimBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];          // §25.2 - the Level limiter
#define gCompLim             (gCompLimBank[SE])

// §20 - the reverb: the instrument's own network, run on one ring of delay memory.
#define REVERB_MODE_TYPE     (0)
#define REVERB_TYPE_COUNT    (4)
#define REVERB_CHANNELS      (2)
#define RV_BASE_RATE         (96000.0)               // the network's own rate: every position counts its samples
#define RV_RING_BITS         (17)
#define RV_RING              (1u << RV_RING_BITS)    // four times the instrument's 32768 words
#define RV_TAP_BASE          (1200.0)                // §20.2 - each tap is room size x K + 1200 ...
#define RV_CURSOR_LEAD       (144.0)                 // ... counted from a cursor 0x90 into the buffer
#define RV_LFO_STEP          (174.0)                 // §20.4 - per base-rate sample, on a 24-bit phase
#define RV_LFO_HALF          (8388608.0)
#define RV_LFO_DEPTH         (76.0)                  // samples of travel, end to end
#define RV_LONGEST_K         (22599.0)               // §20.3 - the tap whose length the decay is set against
#define RV_WORD              (2097152.0)             // §20.6 - full scale in the instrument's 24-bit words
#define RV_COEF              (8388608.0)             // its coefficients' grid
#define RV_TOP               (4.0 - (1.0 / RV_WORD)) // the largest word: four times full scale

static const float        kRvRoomSize[REVERB_TYPE_COUNT] = {0.78f, 0.98f, 1.19f, 1.31f};

// §20.2 - the named positions of the network: a tap constant, and a fixed step from its tap.
typedef enum {
    eRvPre = 0,
    eRvAp1Out,
    eRvAp2In,
    eRvAp2Out,
    eRvAp3Out,
    eRvAp4In,
    eRvAp4Out,
    eRvTankInA,
    eRvApAIn,
    eRvApAOut,
    eRvModA,
    eRvApA2In,
    eRvApA2Out,
    eRvDampA,
    eRvTankInB,
    eRvApBIn,
    eRvApBOut,
    eRvModB,
    eRvApB2In,
    eRvApB2Out,
    eRvDampB,
    eRvTankOutB,
    eRvTapL,
    eRvTapR       = eRvTapL + 7,
    eRvPlaceCount = eRvTapR + 7
} tRvPlace;

typedef struct {
    double k;
    int    step;
} tRvTap;

static const tRvTap kRvPlace[eRvPlaceCount]        = {
    {    0.0,  3}, {  110.0, -1}, {  110.0,  0}, {  255.0,    0}, {  532.0, -1}, {  532.0,  0}, {  921.0,  0},
    { 1000.0, -1}, { 1004.0,  0}, { 1677.0, -1}, { 1677.0, -127}, { 4425.0,  0}, { 6726.0, -1}, { 7300.0,  0},
    {11651.0,  0}, {11655.0,  0}, {12393.0,  0}, {12393.0, -127}, {15080.0,  0}, {17536.0,  0}, {18600.0,  0},
    {22599.0,  0},
    { 3589.0,  0}, { 6307.0,  0}, { 8992.0,  0}, {12398.0,  110}, {15537.0,  0}, {18693.0,  0}, {21432.0,  0},
    { 1680.0,  0}, { 5403.0,  0}, { 7347.0,  0}, {10589.0,    0}, {14470.0, -1}, {17021.0, -1}, {19561.0, -1}
};

// §20.2 - the output taps' signs; the one marked 2 carries the first-tap gain, the rest the decay's.
static const int    kRvTapSign[REVERB_CHANNELS][7] = {
    {-1, 1, -1, 2,  1, -1,  1},
    { 2, 1, -1, 1, -1,  1, -1}
};

static float        gRvRingBank[SOUND_ENGINE_MAX_ENGINES][RV_RING];
#define gRvRing     (gRvRingBank[SE])
static uint32_t     gRvCurBank[SOUND_ENGINE_MAX_ENGINES];
#define gRvCur      (gRvCurBank[SE])
static double       gRvPhaseBank[SOUND_ENGINE_MAX_ENGINES];
#define gRvPhase    (gRvPhaseBank[SE])      // §20.4 - the LFO, -2^23..2^23 as the instrument counts

// §20.2, §20.3, §20.5 - positions, coefficients and mix, as the instrument's host sets them.
static void reverb_build(tEngineNode * node, uint32_t type, double timeDial, double brightDial, double mixDial) {
    SE_LOCAL;

    uint32_t room    = (type < REVERB_TYPE_COUNT) ? type : 0u;
    float    roomF   = kRvRoomSize[room];
    double   rate    = gSampleRate / RV_BASE_RATE;

    for (uint32_t i = 0; i < (uint32_t)eRvPlaceCount; i++) {
        double at = floor(((double)roomF * kRvPlace[i].k) + RV_TAP_BASE) - RV_CURSOR_LEAD + (double)kRvPlace[i].step;

        node->rvPos[i] = (int32_t)lround(at * rate);
    }

    // §20.3 - the instrument's arithmetic, single-precision roundings included: each moves a
    // coefficient by a step of its grid, which a tail at Brightness 0 turns into more than that.
    double   longest = (double)((int32_t)((roomF * (float)RV_LONGEST_K) + (float)RV_TAP_BASE)
                                - (int32_t)RV_TAP_BASE);
    float    d8      = (float)(1.0 + ((99.0 * (brightDial * 256.0)) / 32512.0));
    float    scaled  = (float)(3.0 * (double)(room + 1u));

    scaled      = scaled * (float)(timeDial * 256.0);
    scaled      = (float)((double)scaled / 32512.0);

    float    decay   = (timeDial > 0.0)
                      ? (float)pow(10.0, (-3.0 / (6.0 * (double)scaled * RV_BASE_RATE)) * longest) : 0.0f;
    float    d6      = (float)((double)d8 * 0.01);
    float    inGain  = (float)((double)d8 * 0.01 * 0.7);
    double * y       = node->rvY;

    y[0]        = (double)inGain;
    y[1]        = (double)(1.0f - inGain);
    y[2]        = 0.3;
    y[3]        = (double)(float)((double)decay * 0.3);
    y[4]        = (double)(float)((double)decay * (double)decay);
    y[5]        = (double)(float)((double)d6 * (double)decay);
    y[6]        = (double)(float)(1.0 - (double)d6);
    y[7]        = 0.63;
    y[8]        = (double)(float)fmin(0.62, fmax(0.45, ((double)decay * 0.75) + 0.4));
    y[9]        = (double)(float)fmin(0.48, fmax(0.30, ((double)decay * 0.55) + 0.25));

    for (uint32_t i = 0; i < RV_COEFFS; i++) {
        y[i] = trunc(y[i] * RV_COEF) / RV_COEF;    // §20.6 - truncated to the coefficient grid
    }

    // §20.5 - the instrument's mix words: 127 is its full scale, each law squared and held below 1
    double   top     = RV_COEF - 1.0;
    double   x       = (mixDial >= 127.0) ? top : floor(mixDial * 65536.0);
    double   wet     = (x < (RV_COEF / 2.0)) ? ((2.0 * x) / RV_COEF) : 1.0;
    double   dry     = (x > (RV_COEF / 2.0)) ? ((2.0 * (top - x)) / RV_COEF) : 1.0;

    node->rvDry = fmin(top, trunc(dry * dry * RV_COEF)) / RV_COEF;
    node->rvWet = fmin(top, trunc(wet * wet * RV_COEF)) / RV_COEF;
}

static double   gEnvLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvLevel               (gEnvLevelBank[SE])
static int32_t  gEnvQBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvQ                   (gEnvQBank[SE])       // §17.3 - the level in the instrument's integers
static double   gEnvTickBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvTick                (gEnvTickBank[SE])
// notes §61
#define PARAM_SMOOTH_SECONDS    (0.008)

static double   gSmoothShapeBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothShape            (gSmoothShapeBank[SE])
static double   gSmoothCutoffBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothCutoff           (gSmoothCutoffBank[SE])
static double   gSmoothResBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothRes              (gSmoothResBank[SE])
static double   gSmoothGainBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothGain             (gSmoothGainBank[SE])
static double   gSmoothLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][MAX_NODE_LEVELS];
#define gSmoothLevel            (gSmoothLevelBank[SE])

// Where the per-sample smoothing pass leaves its results, for the voice passes to read. Not per
// voice: a knob is in one place however many notes are sounding, and smoothing it inside the voice
// loop would advance the filter once per voice — so a sweep would speed up as more keys went down.
static double   gSmoothedShapeBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedShape     (gSmoothedShapeBank[SE])
static double   gSmoothedCutoffBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedCutoff    (gSmoothedCutoffBank[SE])
static double   gSmoothedResBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedRes       (gSmoothedResBank[SE])
static double   gSmoothedGainBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedGain      (gSmoothedGainBank[SE])
static double   gSmoothedLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][MAX_NODE_LEVELS];
#define gSmoothedLevel     (gSmoothedLevelBank[SE])
// Until a node has been seen once there is nothing to interpolate FROM, so the first sample snaps.
// Also what stops a patch load sweeping every parameter up from whatever the last patch left.
static bool     gSmoothPrimedBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothPrimed    (gSmoothPrimedBank[SE])

static uint32_t gEnvStageBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvStage        (gEnvStageBank[SE])

// The voice's trigger count this envelope last started an attack for. When the voice's count moves
// past it, a note-on has asked for a restart that the gate alone cannot show - see envelope_step().
static uint32_t gEnvTriggerBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvTrigger    (gEnvTriggerBank[SE])

// §14 - per voice, per Operator of every DXRouter node: phase, envelope (in dB, and its stage), and
// the last two outputs for the feedback loop; per voice and node, the gate and trigger last seen.
typedef enum {
    eDxRise1 = 0,
    eDxRise2,
    eDxRise3,
    eDxHold,
    eDxRelease,
    eDxIdle
} tDxStage;

static double   gDxPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS];
#define gDxPhase       (gDxPhaseBank[SE])
static double   gDxEnvDbBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS];
#define gDxEnvDb       (gDxEnvDbBank[SE])
static uint32_t gDxEnvStageBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS];
#define gDxEnvStage    (gDxEnvStageBank[SE])
static double   gDxOutBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS][2];
#define gDxOut         (gDxOutBank[SE])
static bool     gDxGateBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gDxGate        (gDxGateBank[SE])
static uint32_t gDxTriggerBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gDxTrigger     (gDxTriggerBank[SE])

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

void sound_engine_set_drone_mode(bool on) {
    SE_LOCAL;

    atomic_store(&gDroneMode, on);
}

bool sound_engine_drone_mode(void) {
    SE_LOCAL;

    return atomic_load(&gDroneMode);
}

#define SUSTAIN_PEDAL_DOWN    (0.5)   // CC64 at 64 and above, as MIDI has it

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

    if (group == MORPH_GROUP_SUSTAIN) {
        atomic_store(&gSustainPedal, amount >= SUSTAIN_PEDAL_DOWN);
    }
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

// §36.1 - the Glide module's coefficient for one ENVELOPE TICK. The instrument builds this from the
// envelope's own two time tables indexed by the Time dial, so the module is an envelope segment in
// disguise and needs no table of its own: §17.3's decay multiplier gives Log's one-pole, and its
// linear attack step gives Lin's ramp. Both follow `adr_time_seconds()`, which is where §17 already
// models those tables - agreement with the instrument's own values is better than 1% across the
// dial, and within a few steps at the very top where §17.3 already says the closed form parts from
// the table.
static double glide_tick_coeff(double setting, bool lin) {
    double ticks = adr_time_seconds(setting) * ENV_TICK_HZ;

    if (ticks < 1.0) {
        ticks = 1.0;
    }

    if (lin == true) {
        return 1.0 / ticks;                              // a constant step, full scale in that time
    }
    // TWICE the envelope's decay approach, which is what makes the dial's printed Time the time to
    // close the gap to 1% of it rather than the envelope's own reading of the same number.
    return 2.0 * (1.0 - exp(-log(100.0) / ticks));
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
            double  amount = (group == MORPH_GROUP_VELOCITY) ? sBuildAxis[eAxisVelocity]
                             : (group == MORPH_GROUP_KEYBOARD) ? sBuildAxis[eAxisKey]
                             : ((double)atomic_load(&gMorphMilli[group]) / 1000.0);

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

// §29a - the graph rate and the oscillators' rate for a given device rate. The smallest factor that
// reaches each target, never more than the compile-time maximum the buffers are sized for. At 44.1
// and 48 kHz both are what they have always been; above that they stop doubling.
static void set_oversampling(double deviceRate) {
    SE_LOCAL;

    gOversample    = (deviceRate >= ENGINE_GRAPH_RATE_MIN) ? 1u : (uint32_t)ENGINE_OVERSAMPLE;
    gSampleRate    = deviceRate * (double)gOversample;
    gOscOversample = (gSampleRate >= OSC_GRAPH_RATE_MIN) ? 1u : (uint32_t)OSC_OVERSAMPLE;
}

// The device's rate; the ENGINE runs at gOversample times this (§29a). gSampleRate is the internal
// rate, so every coefficient already derived from it — envelope and glide times, filter and chorus
// coefficients, LFO and oscillator increments — scales with no further change.
static void build_decimator(void);

void sound_engine_set_sample_rate(double sampleRate) {
    SE_LOCAL;

    if (sampleRate > 0.0) {
        uint32_t wasGraph = gOversample;
        uint32_t wasOsc   = gOscOversample;

        gDeviceRate = sampleRate;
        set_oversampling(sampleRate);

        // §29a - THE DECIMATORS ARE BUILT FROM THESE, so a rate that changes either factor has to
        // rebuild them. sound_engine_start() primes the engine BEFORE audio_output_start() tells it
        // the device's rate, so the application builds them once against whatever the factors were
        // and then learns the rate - which did not matter while the factor was a compile-time
        // constant and does now.
        if ((gOversample != wasGraph) || (gOscOversample != wasOsc)) {
            build_decimator();
        }
    }
}

// notes §63 - FOR THE OFFLINE HARNESSES ONLY (tools/morphcheck). Every voice's start phase comes from
// this seed, and it is deliberately never reset, so two engines started in turn sound different - as
// two power-ups of the instrument do. That also means a measurement cannot be repeated, which is what
// pinning it here is for. Neither the application nor the plug-in calls it.
void sound_engine_set_start_phase_seed(uint32_t seed) {
    SE_LOCAL;

    gStartPhaseSeed = (seed == 0u) ? START_PHASE_FIRST_SEED : seed;
}

static void reset_node_state(void) {
    SE_LOCAL;

    uint32_t i = 0;
    uint32_t v = 0;

    if (gStartPhaseSeed == 0u) {
        gStartPhaseSeed = START_PHASE_FIRST_SEED;
    }

    for (v = 0; v < MAX_VOICES; v++) {
        for (i = 0; i < MAX_ENGINE_NODES; i++) {
            gLfoLastPhase[v][i]  = 0.0;
            gLfoTarget[v][i]     = 0.0;
            gLfoHeld[v][i]       = 0.0;
            gOscHistoryPos[v][i] = 0;
            memset(gOscHistory[v][i], 0, sizeof(gOscHistory[v][i]));

            // notes §63
            gStartPhaseSeed     ^= gStartPhaseSeed << 13;
            gStartPhaseSeed     ^= gStartPhaseSeed >> 17;
            gStartPhaseSeed     ^= gStartPhaseSeed << 5;
            gPhase[v][i]         = (double)(gStartPhaseSeed >> 8) / 16777216.0;
            gNoiseSeed[v][i]     = 0x9E3779B9u ^ ((v + 1u) * 0x85EBCA6Bu) ^ ((i + 1u) * 0xC2B2AE35u);
            gNoiseLp[v][i]       = 0.0;
            memset(gLadder[v][i], 0, sizeof(gLadder[v][i]));
            gEnvLevel[v][i]      = 0.0;
            gEnvQ[v][i]          = 0;
            gEnvTick[v][i]       = 0.0;
            gEnvStage[v][i]      = ENV_STAGE_IDLE;
            gEnvTrigger[v][i]    = gVoice[v].trigger;   // nothing pending: idle already attacks on a gate
            gCompEnv[v][i]       = 0.0;
            gPulseCount[v][i]    = 0;
            gPulsePrev[v][i]     = 0.0;
            gGlideOut[v][i]      = 0.0;      // §36
            gGlidePrimed[v][i]   = false;
            gGlideTick[v][i]     = 0.0;
            gLogicPrev[v][i]     = 0u;      // §38
            gLogicState[v][i]    = false;
            gLogicCount[v][i]    = 0u;
            memset(gDrumState[v][i], 0, sizeof(gDrumState[v][i]));   // §39
        }
    }

    // §14
    for (v = 0; v < MAX_VOICES; v++) {
        for (i = 0; i < MAX_DX_OPERATORS; i++) {
            gDxPhase[v][i]    = 0.0;
            gDxEnvDb[v][i]    = DX_SILENT_DB;
            gDxEnvStage[v][i] = eDxIdle;
            gDxOut[v][i][0]   = 0.0;
            gDxOut[v][i][1]   = 0.0;
        }

        for (i = 0; i < MAX_ENGINE_NODES; i++) {
            gDxGate[v][i]    = false;
            gDxTrigger[v][i] = gVoice[v].trigger;
        }
    }

    memset(gCombLine, 0, sizeof(gCombLine));
    memset(gCombWrite, 0, sizeof(gCombWrite));

    for (i = 0; i < MAX_ENGINE_NODES; i++) {
        gSmoothPrimed[i] = false;
    }

    for (i = 0; i < MAX_CHORUS_LINES; i++) {
        chorus_reset(i);
    }

    memset(gDelayLine, 0, sizeof(gDelayLine));
    memset(gDelayWrite, 0, sizeof(gDelayWrite));
    memset(gDelayDamp, 0, sizeof(gDelayDamp));
    memset(gDelayHp, 0, sizeof(gDelayHp));
    memset(gDelayHpB, 0, sizeof(gDelayHpB));
    memset(gDelayFb, 0, sizeof(gDelayFb));
    memset(gDelayMod, 0, sizeof(gDelayMod));
    memset(gRvRing, 0, sizeof(gRvRing));
    gRvCur   = 0;
    gRvPhase = 0.0;
    memset((void *)gModuleMeter, 0, sizeof(gModuleMeter));   // no stale meters after a stop or reload
    memset((void *)gModuleLed, 0, sizeof(gModuleLed));
    memset(gMeterEnv, 0, sizeof(gMeterEnv));
}

// notes §64
static void build_decimator(void) {
    SE_LOCAL;

    double   cutoff = 0.45 / (double)gOscOversample;    // as a fraction of the oversampled rate
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

    cutoff         = 0.45 / (double)gOversample;
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

    static char text[256];
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

    // EVERY CONTROLLER THAT IS NOT AT REST, not just aftertouch. A morph left standing - a wheel, a
    // sustain or expression pedal a keyboard sends at something other than zero, a bend not centred
    // - changes the patch and there was no way to see it. It is the first thing to check when the
    // same patch sounds different in two places (CT, 2026-09-19).
    {
        static const char * groupName[NUM_MORPHS] = {
            "Wheel", "Vel", "Keyb", "AfTch", "Sustain", "Pedal", "M7", "M8"
        };
        char                moved[72]             = {0};
        size_t              used                  = 0;
        int32_t             bend                  = atomic_load(&gBendMilli);

        for (i = 0; i < NUM_MORPHS; i++) {
            uint32_t milli = atomic_load(&gMorphMilli[i]);

            // Velocity and Keyboard are per voice and never come from a controller (§26.2).
            if ((milli == 0u) || (i == MORPH_GROUP_VELOCITY) || (i == MORPH_GROUP_KEYBOARD)) {
                continue;
            }
            used += (size_t)snprintf(moved + used, sizeof(moved) - used, "%s%s %u%%",
                                     (used > 0) ? " " : "", groupName[i], (unsigned)((milli + 5) / 10));

            if (used >= (sizeof(moved) - 12)) {
                break;
            }
        }

        if ((used == 0) && (bend == 0)) {
            snprintf(moved, sizeof(moved), "controls at rest");
        } else if (bend != 0) {
            snprintf(moved + used, sizeof(moved) - used, "%sbend %+.2f", (used > 0) ? " " : "",
                     (double)bend / 1000.0);
        }
        snprintf(text, sizeof(text), "%s, AfTch %u msg peak %u%%, %s, %u LFO of %u nodes",
                 moved, (unsigned)midi_input_pressure_count(),
                 (unsigned)((atomic_load(&gMorphPeakMilli[MORPH_GROUP_AFTERTOUCH]) + 5) / 10),
                 vib, (unsigned)lfos, (unsigned)gParams.nodeCount);
    }
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
        "Osc",      "OscShp",   "Filter",   "LevAmp", "LevMult",   "Mix",    "Env",
        "Chorus",   "Compress", "Delay",    "Reverb", "Lfo",       "Const",  "FxIn",
        "PassThru", "Pulse",    "Shaper",   "Fade",   "MixStereo", "Noise",  "OscNoise",
        "FltMulti", "Eq",       "FltComb",  "Dx",     "Keyboard",  "ModAmt", "Switch",
        "LevConv",  "LevAdd",   "SwSelect", "ValSw",  "MonoKey",   "Glide",  "AudioIn",
        "Invert",   "Gate",     "FlipFlop", "ClkDiv", "DrumSyn",   "Out"
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

void sound_engine_note(int32_t note, uint8_t velocity, bool on) {
    SE_LOCAL;

    uint32_t claim = atomic_fetch_add(&gNoteWrite, 1);
    uint32_t slot  = claim % NOTE_QUEUE_SIZE;

    gNoteQueue[slot].note     = note;
    gNoteQueue[slot].velocity = velocity;
    gNoteQueue[slot].on       = on;

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
        gVoice[v].stealWait   = 0u;
        gVoice[v].queueOrder  = v;   // §15.1a - the queue starts in voice order, front to back
    }

    gVoiceClock = (uint64_t)MAX_VOICES;
    memset(gKeyVelocity, 0, sizeof(gKeyVelocity));   // §35
    memset(gKeyHeld, 0, sizeof(gKeyHeld));
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

// The highest key still held, or -1 when none is. §15.2
static int32_t highest_key_held(void) {
    SE_LOCAL;

    for (int32_t key = MIDI_KEY_COUNT - 1; key >= 0; key--) {
        if (gKeyHeld[key] > 0) {
            return key;
        }
    }

    return -1;
}

// §15.3 - every voice is held: the oldest goes, unless it has the lowest note and the new one is higher.
static uint32_t voice_to_steal(uint32_t count, int32_t note) {
    SE_LOCAL;

    int32_t lowest   = MIDI_KEY_COUNT;
    int32_t oldest   = -1;
    int32_t runnerUp = -1;

    for (uint32_t v = 0; v < count; v++) {
        if (gVoice[v].note < lowest) {
            lowest = gVoice[v].note;
        }

        if ((oldest < 0) || (gVoice[v].age < gVoice[oldest].age)) {
            runnerUp = oldest;
            oldest   = (int32_t)v;
        } else if ((runnerUp < 0) || (gVoice[v].age < gVoice[runnerUp].age)) {
            runnerUp = (int32_t)v;
        }
    }

    if ((oldest >= 0) && (runnerUp >= 0) && (gVoice[oldest].note == lowest) && (note > lowest)) {
        return (uint32_t)runnerUp;
    }
    return (oldest >= 0) ? (uint32_t)oldest : 0;
}

// notes §69
static uint32_t voice_to_allocate(uint32_t count, int32_t note, bool * stolen) {
    SE_LOCAL;

    uint32_t best      = 0;
    uint64_t bestOrder = UINT64_MAX;

    // §15.1a - ONE QUEUE, front to back, which is the whole of the instrument's rule. A note-off
    // unlinks that voice and relinks it at the BACK there and then, whether or not its release is
    // still sounding, and a new note comes off the FRONT - so it takes the voice released longest
    // ago. Nothing here asks whether a voice is still making a sound.
    for (uint32_t v = 0; v < count; v++) {
        if ((gVoice[v].gate == false) && (gVoice[v].queueOrder < bestOrder)) {
            bestOrder = gVoice[v].queueOrder;
            best      = v;
        }
    }

    if (bestOrder != UINT64_MAX) {
        return best;
    }
    *stolen = true;   // §15.3a - in EVERY mode, Mono and Legato included, as the instrument does
    return voice_to_steal(count, note);
}

// §26.2 - the table rows nearest a velocity and a note, and the morph amount each row was built at
static uint8_t velocity_row(uint8_t velocity) {
    return (uint8_t)lround(((double)velocity * (double)(VEL_MORPH_LEVELS - 1)) / 127.0);
}

static uint8_t key_row(int32_t note) {
    int32_t row = (note < 0) ? 0 : ((note + 1) / 2);

    return (uint8_t)((row < KEY_MORPH_LEVELS) ? row : (KEY_MORPH_LEVELS - 1));
}

// The Keyb morph's amount is (note - 36)/60 on the instrument, beyond 0..1 at either end (§26.2).
static double axis_amount(tMorphAxis axis, uint32_t row) {
    if (axis == eAxisVelocity) {
        return (double)row / (double)(VEL_MORPH_LEVELS - 1);
    }
    return ((double)(row * 2u) - KEY_MORPH_ZERO_NOTE) / KEY_MORPH_SPAN;
}

static uint32_t axis_rows(tMorphAxis axis) {
    return (axis == eAxisVelocity) ? VEL_MORPH_LEVELS : KEY_MORPH_LEVELS;
}

static void merge_voice_nodes(uint32_t voice, const tSoundEngineParams * params);
static void merge_last_nodes(const tSoundEngineParams * params);

// The second half of voice_note_on(): everything that starts a note once the voice is settled on.
// Split out because a STOLEN voice does not start its note here - it fades first (§15.3a), and
// start_pending_steals() calls this from the render loop when that fade has finished.
static void voice_start_note(uint32_t chosen, int32_t note, uint8_t velocity,
                             const tSoundEngineParams * params) {
    SE_LOCAL;

    tVoice * voice = &gVoice[chosen];

    voice->stealWait          = 0;

    // notes §70
    voice->glideActive        = voice->gate;

    // notes §71
    if ((voice->gate == false) || (atomic_load(&gEngineLegato) == false)) {
        voice->trigger++;
    }

    if (voice->glidePitch < 0.0) {
        voice->glidePitch = (double)note;   // first note this voice has had: start where it is played
    }
    voice->note               = note;
    voice->velocity           = velocity;
    voice->row[eAxisVelocity] = velocity_row(velocity);
    voice->row[eAxisKey]      = key_row(note);
    voice->sustained          = false;
    voice->release            = 0;
    voice->gate               = true;
    voice->sounding           = true;
    voice->released           = 0;
    voice->fade               = 1.0; // a stolen voice may have been fading; this note cancels that
    voice->age                = ++gVoiceClock;
    gLastRow[eAxisVelocity]   = voice->row[eAxisVelocity];
    gLastRow[eAxisKey]        = voice->row[eAxisKey];

    // §26.2.2 - this voice's rows have just changed, and gLastRow with them
    merge_voice_nodes((uint32_t)(voice - &gVoice[0]), params);
    merge_last_nodes(params);
}

static void voice_note_on(int32_t note, uint8_t velocity, const tSoundEngineParams * params) {
    SE_LOCAL;

    uint32_t count  = atomic_load(&gEngineVoices);

    // Bounded BEFORE it is used to pick a voice, not after. A published count is already clamped,
    // but a zero would send voice_to_allocate() round an empty loop and every note would land on
    // voice 0 — one note at a time, silently, with no obvious cause.
    if (count < 1) {
        count = 1;
    } else if (count > MAX_VOICES) {
        count = MAX_VOICES;
    }
    bool     stolen = false;
    uint32_t chosen = voice_to_allocate(count, note, &stolen);

    // THE KEY COUNT IS KEPT HERE, not in voice_start_note(), so that a stolen note held back for its
    // fade still counts as held the instant it arrived - otherwise a note-off inside those few
    // milliseconds would find nothing to release.
    if ((note < MIDI_KEY_COUNT) && (gKeyHeld[note] < UINT8_MAX)) {
        gKeyHeld[note]++;
    }

    if (note < MIDI_KEY_COUNT) {
        gKeyVelocity[note] = velocity;   // §35 - MonoKey's Vel, per key as the instrument keeps it
    }

    // §15.3a - A STEAL DROPS THE VOICE'S GATE AND WAITS FOR IT TO HAVE BEEN SEEN, which is the
    // whole of what the instrument's allocator does here: it writes a zero into that voice's gate
    // word and runs the DSP a pass before the new note trigs. Nothing is reset and nothing is cut -
    // the envelopes take their release stage for that pass, and the gate rising then restarts the
    // attack under §17.3 (from the level it is at) or §17.7 (from zero, where Reset is on). Which
    // of those a stolen note gets is the patch's own Reset switch, not a rule of the allocator.
    //
    // LEGATO DOES NOT DO IT. The instrument skips the gate drop outright in Legato and does not
    // re-raise the gate while a key is still held, so the note simply moves to the voice.
    if ((stolen == true) && (atomic_load(&gEngineLegato) == false)) {
        gVoice[chosen].gate          = false;
        gVoice[chosen].stealWait     = (uint32_t)(VOICE_STEAL_GATE_TICKS * gSampleRate / ENV_TICK_HZ) + 1u;
        gVoice[chosen].stealNote     = note;
        gVoice[chosen].stealVelocity = velocity;
        return;
    }
    voice_start_note(chosen, note, velocity, params);
}

// §15.3a - called once per sample from the render loop, beside the note queue. The instrument's
// wait-for-the-DSP in one line: a stolen voice has had its gate taken down, and the note that took
// it trigs once every envelope has run a tick with it down.
static void start_pending_steals(const tSoundEngineParams * params) {
    SE_LOCAL;

    for (uint32_t v = 0; v < params->voiceCount; v++) {
        if (gVoice[v].stealWait == 0u) {
            continue;
        }
        gVoice[v].stealWait--;

        if (gVoice[v].stealWait == 0u) {
            voice_start_note(v, gVoice[v].stealNote, gVoice[v].stealVelocity, params);
        }
    }
}

// A note-off names its key; -1 is all-notes-off. A released voice keeps its note and goes on
// sounding its release at the pitch it was played at - unless §15.2 sends it back to a held key.
static void voice_note_off(int32_t note, uint8_t release) {
    SE_LOCAL;

    if (note < 0) {
        memset(gKeyHeld, 0, sizeof(gKeyHeld));

        for (uint32_t v = 0; v < MAX_VOICES; v++) {
            if (gVoice[v].gate == true) {
                gVoice[v].release    = release;
                gVoice[v].queueOrder = ++gVoiceClock;   // §15.1a - all released, oldest-held first
            }
            gVoice[v].gate      = false;
            gVoice[v].sustained = false;
            gVoice[v].stealWait = 0u;         // §15.3a - a note waiting on a gate pass goes with the rest
        }

        return;
    }

    if (note >= MIDI_KEY_COUNT) {
        return;
    }
    gKeyHeld[note] = 0;

    int32_t highest = highest_key_held();
    bool    mono    = atomic_load(&gEngineMono);
    bool    legato  = atomic_load(&gEngineLegato);

    for (uint32_t v = 0; v < MAX_VOICES; v++) {
        tVoice * voice = &gVoice[v];

        // §15.3a - THE KEY CAME UP INSIDE THE GATE PASS. Its note is not on a voice yet, so the
        // test below would never see it: drop it here, and the voice stays as the steal left it -
        // gate down, releasing.
        if ((voice->stealWait != 0u) && (voice->stealNote == note)) {
            voice->stealWait = 0u;
        }

        if ((voice->gate == false) || (voice->note != note)) {
            continue;   // a key that was not sounding changes nothing but the keys held
        }

        if ((mono == false) || (highest < 0)) {
            voice->release = release;

            // §26.3
            if (atomic_load(&gSustainPedal) == true) {
                voice->sustained = true;
            } else {
                voice->gate       = false;
                voice->queueOrder = ++gVoiceClock;    // §15.1a - to the BACK, as the key comes up
            }
            continue;
        }
        // notes §189
        voice->glideActive   = true;

        if (legato == false) {
            voice->trigger++;
        }
        voice->note          = highest;
        voice->row[eAxisKey] = key_row(highest);
        voice->released      = 0;
        voice->fade          = 1.0;
        voice->age           = ++gVoiceClock;
    }
}

// Peak render load since the last read, as a percentage of real time. READING IT CLEARS IT, so the
// figure is always "the worst buffer since you last looked".
uint32_t sound_engine_load_percent(void) {
    SE_LOCAL;

    return atomic_exchange(&gLoadPercent, 0);
}

// Unlocked, like sound_engine_voices_sounding() below: a key moving between two reads is one poly
// pressure message applied or dropped, and the next one settles it.
bool sound_engine_note_sounding(int32_t note) {
    SE_LOCAL;

    uint32_t voices = atomic_load(&gEngineVoices);

    if (voices > MAX_VOICES) {
        voices = MAX_VOICES;
    }

    for (uint32_t v = 0; v < voices; v++) {
        if ((gVoice[v].gate == true) && (gVoice[v].note == note)) {
            return true;
        }
    }

    return false;
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
// §26.3 - the pedal coming up releases every note it was holding
static void sustain_pedal_follow(void) {
    SE_LOCAL;

    bool down = atomic_load(&gSustainPedal);

    if ((down == false) && (gSustainSeen == true)) {
        for (uint32_t v = 0; v < MAX_VOICES; v++) {
            if (gVoice[v].sustained == true) {
                gVoice[v].sustained  = false;
                gVoice[v].gate       = false;
                gVoice[v].queueOrder = ++gVoiceClock;   // §15.1a - released now, so to the back
            }
        }
    }
    gSustainSeen = down;
}

static bool take_next_note_event(const tSoundEngineParams * params) {
    SE_LOCAL;

    sustain_pedal_follow();

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
        voice_note_on(gNoteQueue[slot].note, gNoteQueue[slot].velocity, params);
    } else {
        voice_note_off(gNoteQueue[slot].note, gNoteQueue[slot].velocity);
    }
    gNoteRead++;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Building the chain (UI thread)
// ---------------------------------------------------------------------------------------------

// notes §73
static double pulse_time_seconds(double value, uint32_t range) {
    // §18 - ln(Sub width + 2, in 96 kHz samples) = k0 + k1*x + k2*x^2 + k3*x^3, x = dial / 127
    const double k0      = 2.30093;
    const double k1      = 8.76455853;
    const double k2      = 0.378462386;
    const double k3      = 0.0289283595;
    double       x       = value / 127.0;
    double       samples = exp(k0 + (k1 * x) + (k2 * x * x) + (k3 * x * x * x)) - 2.0;
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

#define ENV_FULL_SCALE_STEPS    (8388608.0)  // full scale in the tick's fixed-point increments

static double env_attack_seconds(double paramValue, uint32_t shape) {
    double seconds = adr_time_seconds(paramValue);

    if ((shape == (uint32_t)eEnvShapeLinExp) || (shape == (uint32_t)eEnvShapeLinLin)) {
        double increment = floor(ENV_FULL_SCALE_STEPS / (seconds * ENV_TICK_HZ));

        seconds = ENV_FULL_SCALE_STEPS / (fmax(increment, 1.0) * ENV_TICK_HZ);
    }
    return seconds;
}

// §17.2 - the four stages as per-sample recurrences at the engine's rate.
#define ENV_TOP           (0x7FFFFF)         // full scale, the largest 24-bit word
#define ENV_HALF_UNITY    (0x400000)         // half of a multiplier of 1, which the instrument doubles

static int32_t env_q23(double fraction) {
    return (int32_t)floor(fraction * ENV_FULL_SCALE_STEPS);
}

static double env_ticks(double seconds) {
    return fmax(seconds * ENV_TICK_HZ, 1.0);
}

// §17.9 - one stage's per-tick words. A rise uses the attack shapes of §17.3, a fall the decay one.
static void env_stage_rates(tEnvSegment * stage, double seconds, uint32_t shape, bool rising) {
    double ticks  = env_ticks(seconds);
    bool   linear = (shape == (uint32_t)eEnvShapeLinLin);

    if (rising == false) {
        stage->half = linear ? ENV_HALF_UNITY : (env_q23(exp(-ENV_FALL_SHARPNESS / ticks)) >> 1);
        stage->add  = linear ? -env_q23(1.0 / ticks) : 0;
        return;
    }

    switch (shape) {
        case eEnvShapeLogExp:
        {
            double mul = exp(-ENV_RISE_SHARPNESS / ticks);

            stage->half = env_q23(mul / 2.0);
            stage->add  = env_q23(ENV_LOG_RISE_TARGET * (1.0 - mul));
            break;
        }
        case eEnvShapeExpExp:
        {
            double mul = exp(ENV_RISE_SHARPNESS / ticks);

            stage->half = env_q23(mul / 2.0);
            stage->add  = env_q23((mul - 1.0) / (exp(ENV_RISE_SHARPNESS) - 1.0));
            break;
        }
        default:
        {
            stage->half = ENV_HALF_UNITY;
            stage->add  = env_q23(1.0 / ticks);
            break;
        }
    }
}

// §17.9 - the whole stage list, from the map the module shares with its face.
static void env_stages_build(tEngineNode * node, tModule * module, uint32_t variation) {
    tEnvGraph map;

    node->envStageCount   = 0;
    node->envSustainStage = -1;
    node->envSustainQ     = 0;

    if (env_stage_map(module->type, module->param[variation], &map, false) == false) {
        return;
    }
    node->wave            = (tOscWave)map.shape;
    node->envOutType      = map.outputType;

    double    from = map.startLevel;

    for (uint32_t i = 0; (i < map.count) && (node->envStageCount < ENV_MAX_STAGES); i++) {
        const tEnvGraphSegment * segment = &map.segment[i];
        tEnvSegment *            stage   = &node->envStage[node->envStageCount];
        // §16.3 - a dialled level is value/128 with 127 pinned to 1. The map's own level is a
        // DRAWING level (value/127) and is only used where a stage has no level parameter at all.
        // A held stage sits at whatever the stage before it reached - which is the DIALLED level,
        // where the map's own level for it is only a drawing value. §17.6's bipolar offset reads it.
        double                   level   = (segment->sustain != false)
                                           ? from
                                           : ((segment->levelParam >= 0)
                                              ? dial_fraction(param_value(module, variation, (uint32_t)segment->levelParam))
                                              : fabs(segment->level));
        bool                     rising  = level > from;

        stage->sustain   = (uint8_t)segment->sustain;
        stage->rising    = (uint8_t)rising;
        stage->target    = (int32_t)fmin((double)ENV_TOP, round(level * ENV_FULL_SCALE_STEPS));
        // §17.10 - what a run-time mod needs to re-read this stage's time from its own dial
        stage->modLeg    = -1;
        stage->dial      = 0u;
        stage->modAmount = 0u;

        if ((segment->timeParam >= 0) && (segment->timeModParam >= 0)) {
            stage->modLeg    = (int8_t)(ENV_INPUT_MOD + segment->timeParam);
            stage->dial      = (uint8_t)param_value(module, variation, (uint32_t)segment->timeParam);
            stage->modAmount = (uint8_t)param_value(module, variation, (uint32_t)segment->timeModParam);
        }

        if (segment->sustain != false) {
            node->envSustainStage = (int32_t)node->envStageCount;
            node->envSustainQ     = stage->target;   // §17.6 - where the bipolar types centre
            stage->half           = ENV_HALF_UNITY;
            stage->add            = 0;
        } else {
            // A rise reads its time through the attack curve, as §17.1 has it; a fall through §17.2.
            double seconds = (segment->timeParam >= 0)
                             ? (rising
                                ? env_attack_seconds(param_value(module, variation, (uint32_t)segment->timeParam), map.shape)
                                : env_time_seconds(param_value(module, variation, (uint32_t)segment->timeParam)))
                             : 0.0;

            env_stage_rates(stage, seconds, map.shape, rising);
        }
        node->envStageCount++;
        from = level;
    }
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
    switch (kind) {
        case eNodeFltMulti:
        {
            return 3u;
        }
        case eNodeKeyboard:
        {
            return KEYBOARD_OUTPUTS;
        }
        case eNodeMonoKey:
        {
            return 3u;   // §35 - Pitch, Gate, Vel
        }
        case eNodeInvert:     // §38.1 - Out 1 and Out 2
        case eNodeGate:       // §38.2 - Out1 and Out2
        case eNodeFlipFlop:   // §38.3 - NotQ then Q, in the module's own order
        {
            return 2u;
        }
        default:
        {
            return 2u;
        }
    }
}

static void delay_words(tEngineNode * node, double lpDial, double hpDial, double fbDial, double dryWetDial, double fbModDial, double mixModDial); // §24.2
static void comp_words(tEngineNode * node, double thrDial, double ratioDial, double atkDial, double relDial, double lvlDial);                     // §25.1

static bool module_kind(tModule * module, tNodeKind * kind) {
    switch (module->type) {
        case moduleTypeOscB:
        case moduleTypeOscA:
        case moduleTypeOscC:
        case moduleTypeOscD:
        case moduleTypeOscDual:
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
        case moduleTypeEnvADR:
        case moduleTypeEnvAHD:
        case moduleTypeEnvD:
        case moduleTypeEnvH:
        case moduleTypeEnvADDSR:
        case moduleTypeEnvMulti:
        case moduleTypeModADSR:
        case moduleTypeModAHD:
        {
            *kind = eNodeEnv;   // §17.9 - all nine play from the same stage map
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
        case moduleTypeFltComb:
        {
            *kind = eNodeFltComb;
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
        case moduleTypeDXRouter:
        {
            *kind = eNodeDx;        // §14 - with the Operators patched into it; an Operator alone is not played
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
        case moduleTypeKeyboard:
        {
            *kind = eNodeKeyboard;
            return true;
        }
        case moduleTypeModAmt:
        {
            *kind = eNodeModAmt;     // §29
            return true;
        }
        case moduleTypeSwOnOffT:
        {
            *kind = eNodeSwitch;     // §30
            return true;
        }
        case moduleTypeLevConv:
        {
            *kind = eNodeLevConv;    // §31
            return true;
        }
        case moduleTypeLevAdd:
        {
            *kind = eNodeLevAdd;     // §32
            return true;
        }
        case moduleTypeSw2to1:
        case moduleTypeSw8to1:
        {
            *kind = eNodeSwSelect;   // §33
            return true;
        }
        case moduleTypeValSw2to1:
        {
            *kind = eNodeValSw;      // §34
            return true;
        }
        case moduleTypeMonoKey:
        {
            *kind = eNodeMonoKey;    // §35
            return true;
        }
        case moduleTypeGlide:
        {
            *kind = eNodeGlide;      // §36
            return true;
        }
        case moduleType2toIn:
        {
            *kind = eNodeAudioIn;    // §37
            return true;
        }
        case moduleTypeInvert:
        {
            *kind = eNodeInvert;     // §38.1
            return true;
        }
        case moduleTypeGate:
        {
            *kind = eNodeGate;       // §38.2
            return true;
        }
        case moduleTypeFlipFlop:
        {
            *kind = eNodeFlipFlop;   // §38.3
            return true;
        }
        case moduleTypeClkDiv:
        {
            *kind = eNodeClkDiv;     // §38.4
            return true;
        }
        case moduleTypeDrumSynth:
        {
            *kind = eNodeDrumSynth;  // §39
            return true;
        }
        default:
        {
            return false;
        }
    }
}

// Whether the engine plays this module at all - the canvas greys out the rest while the engine runs.
// An Operator counts: it is played through the DXRouter it is patched into. A Name has no sound.
bool sound_engine_models_module(tModule * module) {
    tNodeKind kind = eNodeOsc;

    return (module_kind(module, &kind) == true) || (module->type == moduleTypeOperator) || (module->type == moduleTypeName);
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

// §17.10 - how many time-mod jacks a module has. Only the two "Mod" envelopes carry them, and each
// sits one connector past the dial it modulates, so the count is the number of dials that have one.
static uint32_t env_mod_jack_count(tModuleType moduleType) {
    switch (moduleType) {
        case moduleTypeModADSR:
        {
            return 4u;      // A, D, S, R
        }
        case moduleTypeModAHD:
        {
            return 3u;      // A, H, D
        }
        default:
        {
            return 0u;
        }
    }
}

// §17.4 - an envelope's three inputs in the node's own order (audio In, Gate, AM), looked up by the
// role each plays rather than by where it sits in the module's connector list. Falls back to the
// positional order for a type the role table does not cover, which is what every envelope used to
// get.
static uint32_t env_input_connectors(tModuleType moduleType, uint32_t * derived) {
    static const char * role[3]  = {"VCA Inputs", "Trig & Gate Inputs", "Amp Inputs"};
    uint32_t            count    = 0;

    for (uint32_t i = 0; i < 3u; i++) {
        uint32_t index = module_index_for_role(moduleType, roleKindInput, role[i]);

        if (index == MODULE_ROLE_NONE) {
            count = inputs_in_module_order(moduleType, 3u, derived);
            break;
        }
        derived[count++] = index;
    }

    // §17.10 - then the time-mod jacks, which sit one connector past each dial they modulate, so
    // node input ENV_INPUT_MOD + p carries the mod for the module's parameter p.
    uint32_t            modCount = env_mod_jack_count(moduleType);

    for (uint32_t i = 0; (i < modCount) && (count < MAX_NODE_INPUTS); i++) {
        int found = connector_index_for_input(moduleType, 1u + i, anyConnectorType);

        derived[count++] = (found >= 0) ? (uint32_t)found : 0u;
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

            // §21.3 - FltClassic's second control input, Pitch, has no knob
            if (moduleType == moduleTypeFltClassic) {
                int pitchIn = connector_index_for_input(moduleType, 2, anyConnectorType);

                if (pitchIn >= 0) {
                    derived[2]  = (uint32_t)pitchIn;
                    *connectors = derived;
                    return 3;
                }
            }
            *connectors = derived;
            return 2;
        }
        case eNodeOsc:
        case eNodeOscShp:
        {
            // §6.3
            static const uint32_t oscCIn[]    = {3, 0};
            static const uint32_t oscDualIn[] = {0, 1, 3, 4};    // §12.1 - Sync is not modelled

            if (moduleType == moduleTypeOscDual) {
                *connectors = oscDualIn;
                return 4;
            }

            if (moduleType == moduleTypeOscC) {
                *connectors = oscCIn;
                return 2;
            }
            *connectors = oscIn;
            return (moduleType == moduleTypeOscD) ? 1 : 2;
        }
        case eNodeLevMult:
        case eNodeModAmt:    // §29 - In, then the Mod input the Depth dial scales
        case eNodePulse:
        case eNodeOut:
        {
            *connectors = twoIn;
            return 2;
        }
        case eNodeSwitch:
        {
            *connectors = twoIn;     // §30 - only In is read; Ctrl is an output
            return 1;
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
        {
            // §24.6 - DelayB's two control inputs: FB modulation, then DryWet modulation
            if (moduleType == moduleTypeDelayB) {
                for (uint32_t k = 0; k < 3u; k++) {
                    int found = connector_index_for_input(moduleType, k, (k == 0u) ? connectorTypeAudio : anyConnectorType);

                    derived[k] = (found >= 0) ? (uint32_t)found : CONNECTOR_IN_A;
                }

                *connectors = derived;
                return 3;
            }
            *connectors = oneIn;
            return 1;
        }
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
            // §17.4 - BY ROLE, NOT BY POSITION. The first three input connectors are In, Gate and
            // AM on EnvADSR alone; ModADSR puts its four mod jacks in between, so taking the first
            // three gave it Gate, Attack M and Decay M - its audio In was never even looked at, the
            // chain stopped dead at the envelope and everything upstream of it went unbuilt. The
            // module role table already names all three for every envelope type.
            *connectors = derived;
            return env_input_connectors(moduleType, derived);
        }
        case eNodeFxIn:
        {
            *connectors = none;   // filled in by the Voice-area bridge, not by a cable
            return 0;
        }
        case eNodeDx:
        {
            *connectors = none;   // §14 - its Operators are gathered by dx_build(), not recursed into
            return 0;
        }
        case eNodeNoise:
        {
            *connectors = none;     // a source: no inputs at all
            return 0;
        }
        case eNodeMonoKey:          // §35 - the keyboard is its input
        case eNodeAudioIn:          // §37 - the jacks on the back, which this engine does not have
        {
            *connectors = none;
            return 0;
        }
        case eNodeLevConv:          // §31 - In
        case eNodeLevAdd:           // §32 - In
        {
            *connectors = oneIn;
            return 1;
        }
        case eNodeGlide:            // §36 - In, then the Glide On logic input
        {
            *connectors = twoIn;
            return 2;
        }
        case eNodeSwSelect:         // §33 - In 1..n, in the module's own order; Ctrl is an OUTPUT
        case eNodeValSw:            // §34 - In 1, In 2 (On) and Ctrl, likewise
        case eNodeInvert:           // §38.1 - In 1 and In 2, which the face interleaves with the outs
        case eNodeGate:             // §38.2 - In1_1, In1_2, In2_1, In2_2
        case eNodeFlipFlop:         // §38.3 - Clk, Rst, In
        case eNodeClkDiv:           // §38.4 - Clk, Rst
        case eNodeDrumSynth:        // §39 - Trig, Pitch, Vel
        {
            *connectors = derived;
            return inputs_in_module_order(moduleType, MAX_NODE_INPUTS, derived);
        }
        case eNodeFltMulti:
        case eNodeOscNoise:
        {
            uint32_t count = inputs_in_module_order(moduleType, 3u, derived);
            *connectors = derived;
            return count;
        }
        case eNodeFltComb:
        {
            uint32_t count = inputs_in_module_order(moduleType, 4u, derived);    // In, Pitch, PitchVar, FB Mod
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

static void eq_build(tEngineNode * node, tModule * module, uint32_t variation) {
    eq_bands_build(module, variation, param_value, &node->eq);
    node->active = node->eq.active;
}

// §11 - the shelves, then the peak; each adds its boost to what passes through.
static double eq_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input) {
    SE_LOCAL;

    const tEqBands * eq     = &spec->eq;
    double *         state  = gLadder[voice][node];       // low shelf, high shelf, the peak's two
    double           signal = input * eq->inputLevel;

    if (eq->lowHz > 0.0) {
        double pole = exp(-2.0 * M_PI * eq->lowHz / gSampleRate);

        state[0] += (1.0 - pole) * (signal - state[0]);
        signal   += (eq->lowGain - 1.0) * state[0];
    }

    if (eq->highHz > 0.0) {
        double pole = exp(-2.0 * M_PI * eq->highHz / gSampleRate);
        double half = 0.5 * (1.0 + pole) * signal;
        double high = state[1] + half;

        state[1] = (pole * high) - half;
        signal  += (eq->highGain - 1.0) * high;
    }

    if (eq->peakHz > 0.0) {                                   // §11.5
        double g       = tan(M_PI * fmin(eq->peakHz, gSampleRate * 0.45) / gSampleRate);
        double damping = eq->peakDamping;
        double high    = (signal - ((damping + g) * state[2]) - state[3]) / (1.0 + (damping * g) + (g * g));
        double band    = (g * high) + state[2];

        state[2] = (g * high) + band;
        state[3] = (2.0 * g * band) + state[3];
        signal  += (eq->peakGain - 1.0) * damping * band;
    }
    return signal;
}

// §6.1a - what the Tune dial MEANS, which is the Pitch Type drop-down's whole job. All four end as
// a `basePitch` in the Semi scale where 64 is unity, so the keyboard tracking below and everything
// downstream stay as they are - a ratio is just an offset in semitones. The laws are the ones the
// dial itself prints (renderParams.c), shared rather than restated so the two cannot drift.
//
// Freq and the sub-audio end of Partial are ABSOLUTE, so they ignore the key: Kbt is forced off for
// those rather than left to the button, which is what "a fixed frequency" means.
static double osc_base_pitch(int pitchType, double tune, bool * absolute, bool * silent) {
    *absolute = false;
    *silent   = false;

    switch (pitchType) {
        case 1:      // Freq - 8.1758 Hz to 12.55 kHz
        {
            *absolute = true;
            return MIDI_NOTE_A440 + (12.0 * log2(osc_freq_hz(tune) / 440.0));
        }

        case 2:      // Factor - 0.0248x to 38.072x of the note
        {
            return OSCB_TUNE_UNITY + (12.0 * log2(osc_freq_factor(tune)));
        }

        case 3:      // Partial - silence, then sub-audio hertz, then 1:n and n:1 of the note
        {
            if (tune <= 0.0) {
                *silent = true;
                return OSCB_TUNE_UNITY;
            }

            if (tune < 33.0) {
                const double minHz = 0.005;
                const double maxHz = 5.153;
                double       hz    = exp(((tune - 1.0) / 31.0) * log(maxHz / minHz)) * minHz;

                *absolute = true;
                return MIDI_NOTE_A440 + (12.0 * log2(hz / 440.0));
            }
            {
                double ratio = (tune < 64.0) ? (1.0 / ((64.0 - tune) + 1.0)) : ((tune - 64.0) + 1.0);

                return OSCB_TUNE_UNITY + (12.0 * log2(ratio));
            }
        }

        default:     // Semi, and anything a module offers that this does not know
        {
            return tune;
        }
    }
}

static void set_osc_pitch(tEngineNode * node, tModule * module, uint32_t variation, const tOscParams * p) {
    double tune      = param_value(module, variation, (uint32_t)p->tune);
    double cent      = param_value(module, variation, (uint32_t)p->cent);
    // -1 is a module with no Pitch Type menu at all, which is Semi and nothing else.
    int    pitchType = (p->pitchType >= 0) ? (int)param_value(module, variation, (uint32_t)p->pitchType) : 0;
    bool   absolute  = false;
    bool   silent    = false;

    if (pitchType > 3) {
        LOG_DEBUG("Sound engine: Osc PitchType %d not supported, reading Tune as Semi\n", pitchType);
    }
    node->oscKbt    = (param_value(module, variation, (uint32_t)p->kbt) != 0.0);
    node->basePitch = osc_base_pitch(pitchType, tune, &absolute, &silent) + (osc_fine_cents(cent) / 100.0);

    if (absolute == true) {
        node->oscKbt = false;
    }
    node->modAmount = (p->pitchMod >= 0)
                      ? type_ii_attenuator(param_value(module, variation, (uint32_t)p->pitchMod))
                      : 0.0;
    node->active    = (param_value(module, variation, (uint32_t)p->active) != 0.0) && (silent == false);
}

#define OSCDUAL_PARAM_SQUARE_LEVEL    (5)     // §12.1 - 6 and 11 are the other way round in the module tables
#define OSCDUAL_PARAM_PW_MOD          (6)
#define OSCDUAL_PARAM_SAW_LEVEL       (7)
#define OSCDUAL_PARAM_SAW_PHASE       (8)
#define OSCDUAL_PARAM_SUB_LEVEL       (9)
#define OSCDUAL_PARAM_PW              (11)
#define OSCDUAL_PARAM_PHASE_MOD       (12)
#define OSCDUAL_PARAM_SOFT            (13)

static void oscdual_build(tEngineNode * node, tModule * module, uint32_t variation) {
    node->wave            = eOscWaveDual;
    node->shape           = dial_fraction(param_value(module, variation, OSCDUAL_PARAM_PW));
    node->dualSquareLevel = dial_fraction(param_value(module, variation, OSCDUAL_PARAM_SQUARE_LEVEL));
    node->dualSawLevel    = dial_fraction(param_value(module, variation, OSCDUAL_PARAM_SAW_LEVEL));
    node->dualSubLevel    = dial_fraction(param_value(module, variation, OSCDUAL_PARAM_SUB_LEVEL));
    node->dualSawPhase    = dial_fraction(param_value(module, variation, OSCDUAL_PARAM_SAW_PHASE));
    node->dualPwMod       = dial_fraction(param_value(module, variation, OSCDUAL_PARAM_PW_MOD));
    node->dualPhaseMod    = dial_fraction(param_value(module, variation, OSCDUAL_PARAM_PHASE_MOD));
    node->dualSoft        = (module->param[variation][OSCDUAL_PARAM_SOFT].value != 0);
}

// notes §76
// §14 - Operator and DXRouter parameters, in the G2's order.
#define DXROUTER_PARAM_ALGORITHM    (0)
#define DXROUTER_PARAM_FEEDBACK     (1)
#define OP_PARAM_KBT                (0)
#define OP_PARAM_SYNC               (1)
#define OP_PARAM_COARSE             (3)
#define OP_PARAM_DETUNE             (5)
#define OP_PARAM_RATESCALE          (7)
#define OP_PARAM_R1                 (8)      // R1 L1 R2 L2 R3 L3 R4 L4 run from here
#define OP_PARAM_BRPT               (17)
#define OP_PARAM_LCURVE             (18)
#define OP_PARAM_LDEPTH             (19)
#define OP_PARAM_RCURVE             (20)
#define OP_PARAM_RDEPTH             (21)
#define OP_PARAM_LEVEL              (22)
#define OP_PARAM_ACTIVE             (23)
#define OP_DEPTH_MAX                (7.0)    // the depth menus' top value in the module table

static double dx_level_db(double value) {
    return (value <= 0.0) ? DX_SILENT_DB : ((fmin(value, DX_LEVEL_TOP) - DX_LEVEL_TOP) * DX_LEVEL_DB_PER_STEP);
}

static double dx_rate_db_per_second(double value) {
    return -DX_SILENT_DB / (DX_RATE_SLOWEST_SECONDS * exp2(-fmin(value, DX_LEVEL_TOP) * DX_RATE_OCTAVES_PER_STEP));
}

// §14.3: Feedback 7 feeds back half a cycle (pi), each step below it half as much.
static double dx_feedback_gain(double value) {
    return (value < 1.0) ? 0.0 : exp2(value - 8.0);
}

// §14.1 - the router and the Operators on its six inputs, gathered into one node: their FM runs
// through the router in both directions, which a chain of separate nodes cannot evaluate.
static void dx_build(tSoundEngineParams * params, tEngineNode * node, tModule * router, uint32_t variation) {
    node->dxAlgorithm  = (uint32_t)param_value(router, variation, DXROUTER_PARAM_ALGORITHM);
    node->dxFeedback   = dx_feedback_gain(param_value(router, variation, DXROUTER_PARAM_FEEDBACK));
    node->active       = false;

    if ((params->dxOpCount + DX_OPERATORS) > MAX_DX_OPERATORS) {
        return;     // more routers than the table holds: this one stays silent
    }
    node->dxBase       = params->dxOpCount;
    params->dxOpCount += DX_OPERATORS;

    for (uint32_t k = 0; k < DX_OPERATORS; k++) {
        tDxOperator * op           = &params->dxOp[node->dxBase + k];
        int           connector    = connector_index_for_input(router->type, k, anyConnectorType);
        uint32_t      sourceOutput = 0;
        tModule *     source       = (connector >= 0) ? module_feeding(router, (uint32_t)connector, &sourceOutput) : NULL;

        memset(op, 0, sizeof(*op));

        if ((source == NULL) || (source->type != moduleTypeOperator)) {
            continue;
        }
        uint32_t      coarse       = (uint32_t)param_value(source, variation, OP_PARAM_COARSE);
        uint32_t      fine         = (uint32_t)param_value(source, variation, OPERATOR_FINE_PARAM);
        double        level        = param_value(source, variation, OP_PARAM_LEVEL);

        op->present    = true;
        op->active     = (param_value(source, variation, OP_PARAM_ACTIVE) != 0.0);
        op->kbt        = (param_value(source, variation, OP_PARAM_KBT) != 0.0);
        op->sync       = (param_value(source, variation, OP_PARAM_SYNC) != 0.0);
        op->fixed      = (source->param[variation][OPERATOR_RATIO_FIXED_PARAM].value != 0);
        op->ratio      = operator_ratio(coarse, fine);
        op->fixedHz    = operator_fixed_hz(coarse, fine);
        op->detune     = exp2(((param_value(source, variation, OP_PARAM_DETUNE) - 7.0) * DX_DETUNE_CENTS_PER_STEP) / 1200.0);
        op->outputGain = (level <= 0.0) ? 0.0 : exp2(dx_level_db(level) / 6.0206);
        op->rateScale  = param_value(source, variation, OP_PARAM_RATESCALE) / 7.0;
        op->bpNote     = param_value(source, variation, OP_PARAM_BRPT);
        op->lCurve     = source->param[variation][OP_PARAM_LCURVE].value;
        op->rCurve     = source->param[variation][OP_PARAM_RCURVE].value;
        op->lDepth     = param_value(source, variation, OP_PARAM_LDEPTH) / OP_DEPTH_MAX;
        op->rDepth     = param_value(source, variation, OP_PARAM_RDEPTH) / OP_DEPTH_MAX;

        for (uint32_t s = 0; s < 4; s++) {
            op->rateDbPerSecond[s] = dx_rate_db_per_second(param_value(source, variation, OP_PARAM_R1 + (2 * s)));
            op->levelDb[s]         = dx_level_db(param_value(source, variation, OP_PARAM_R1 + (2 * s) + 1));
        }

        if (op->active) {
            node->active = true;
        }
    }
}

// notes §192
static _Thread_local bool sNodeBuilding[locationMax][MAX_NUM_MODULES];

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

    if (depth == 0) {
        memset(sNodeBuilding, 0, sizeof(sNodeBuilding));   // notes §192
    }

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

    // notes §192 - a patch may cable a module back into its own pitch or audio path. The G2 runs
    // such a loop with a delay in it; this walk would recurse until the budget ran out, so the leg
    // that closes the loop reads as unpatched instead.
    bool tracked = (module->key.location < (uint32_t)locationMax)
                   && (module->key.index < MAX_NUM_MODULES);

    if (tracked == true) {
        if (sNodeBuilding[module->key.location][module->key.index] == true) {
            return -1;
        }
        sNodeBuilding[module->key.location][module->key.index] = true;
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

    if (tracked == true) {
        sNodeBuilding[module->key.location][module->key.index] = false;
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
                                                             : SHPB_PARAM_PITCH_MOD));
            node->active    = (param_value(module, variation,
                                           isShpA ? SHPA_PARAM_ACTIVE : SHPB_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeChorus:
        {
            // Detune sets how far the delay is swept, Amount how much of the wet signal is heard.
            node->depth  = param_value(module, variation, CHORUS_PARAM_DETUNE);    // §19.2 - the raw dial
            node->amount = dial_fraction(param_value(module, variation, CHORUS_PARAM_AMOUNT));
            node->active = (param_value(module, variation, CHORUS_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeCompress:
        {
            // notes §83; §25.1 - the words the instrument's host sets
            comp_words(node, param_value(module, variation, COMP_PARAM_THRESHOLD), param_value(module, variation, COMP_PARAM_RATIO),
                       param_value(module, variation, COMP_PARAM_ATTACK), param_value(module, variation, COMP_PARAM_RELEASE),
                       param_value(module, variation, COMP_PARAM_REFLVL));
            node->active = (param_value(module, variation, COMP_PARAM_ACTIVE) != 0.0);
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
                    node->timeSeconds = fmax(0.0, delay_time_seconds(maxTime, param_value(module, variation, DELAY_PARAM_TIME))
                                             - (1.0 / G2_ENGINE_SAMPLE_RATE));    // §24.1 - Time x step samples; the readout adds one
                }
            }
            // notes §87; §24.2 - the words the instrument's host sets (DelayA has no HP)
            delay_words(node, param_value(module, variation, DELAY_PARAM_LP),
                        (module->type == moduleTypeDelayB) ? param_value(module, variation, DELAY_PARAM_HP) : 0.0,
                        param_value(module, variation, DELAY_PARAM_FEEDBACK), param_value(module, variation, DELAY_PARAM_DRYWET),
                        (module->type == moduleTypeDelayB) ? param_value(module, variation, DELAYB_PARAM_FBMOD) : 0.0,
                        (module->type == moduleTypeDelayB) ? param_value(module, variation, DELAYB_PARAM_MIXMOD) : 0.0);
            node->active = (param_value(module, variation,
                                        (module->type == moduleTypeDelayA)
                                             ? DELAYA_PARAM_ACTIVE : DELAYB_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeReverb:
        {
            // Raw, like every other drop-down: a mode cannot carry a morph (manual p.20).
            node->reverbType = module->mode[REVERB_MODE_TYPE].value;

            if (node->reverbType >= REVERB_TYPE_COUNT) {
                node->reverbType = 0;
            }
            node->active     = (param_value(module, variation, REVERB_PARAM_ACTIVE) != 0.0);
            reverb_build(node, node->reverbType, param_value(module, variation, REVERB_PARAM_TIME),
                         param_value(module, variation, REVERB_PARAM_BRIGHT),
                         param_value(module, variation, REVERB_PARAM_DRYWET));
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
        case eNodeFltComb:
        {
            // Freq 0, Pitch 1, Kbt 2, FB 3, FB Mod 4, Type 5, Level 6, On 7 - §13.1
            node->cutoffParam  = param_value(module, variation, 0);
            node->modAmount    = dial_fraction(param_value(module, variation, 1));
            node->fltKbt       = param_value(module, variation, 2) * 0.25;
            node->combFeedback = flt_comb_feedback(param_value(module, variation, 3));
            node->combFbMod    = dial_fraction(param_value(module, variation, 4));
            node->combType     = module->param[variation][5].value;
            node->combLevel    = mix_level_gain(param_value(module, variation, 6));
            node->active       = (param_value(module, variation, 7) != 0.0);
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
            // The drop-downs are read raw because a drop-down cannot carry a morph (manual p.20).
            shaper_settings_build(module, variation, param_value, &node->shaper);
            node->active = node->shaper.active;
            break;
        }
        case eNodeConstant:
        {
            node->constant = constant_level(param_value(module, variation, CONST_PARAM_VALUE),
                                            module->param[variation][CONST_PARAM_BIP_UNI].value == 0);
            break;
        }
        case eNodeModAmt:
        {
            // §29 - Depth rides on gain so it is smoothed and morphable; the drop-downs are read raw.
            double depth = param_value(module, variation, MODAMT_PARAM_DEPTH);

            node->gain           = (module->param[variation][MODAMT_PARAM_EXPLIN].value == MODAMT_EXPLIN_LIN)
                             ? ((depth >= 127.0) ? 1.0 : (depth / 128.0))
                             : mix_level_gain(depth);
            node->active         = (module->param[variation][MODAMT_PARAM_ENABLE].value != 0);
            node->modAmtOneMinus = (module->param[variation][MODAMT_PARAM_MODE].value != 0);
            break;
        }
        case eNodeSwitch:
        {
            node->active = (module->param[variation][SWITCH_PARAM_ON].value != 0);
            break;
        }
        case eNodeLevConv:
        {
            // §31 - both are drop-downs, so both are read raw: a drop-down cannot be morphed.
            node->levConvIn  = (uint32_t)module->param[variation][LEVCONV_PARAM_IN].value;
            node->levConvOut = (uint32_t)module->param[variation][LEVCONV_PARAM_OUT].value;
            break;
        }
        case eNodeLevAdd:
        {
            // §32 - the same dial law as a Constant (§16.1), which is what it adds.
            node->constant = constant_level(param_value(module, variation, LEVADD_PARAM_VALUE),
                                            module->param[variation][LEVADD_PARAM_BIP_UNI].value == 0);
            break;
        }
        case eNodeSwSelect:
        {
            // §33 - the selector is a radio button, read raw. inputCount is what the module has,
            // so Sw2-1 and Sw8-1 share one node kind and one evaluation.
            node->select     = (uint32_t)module->param[variation][SWSEL_PARAM_SELECT].value;
            node->inputCount = node->inCount;
            break;
        }
        case eNodeValSw:
        {
            // §34 - the threshold in units, its top step reading 64 rather than 63.
            double raw = param_value(module, variation, VALSW_PARAM_VALUE);

            node->constant = ((raw >= (double)VALSW_VALUE_TOP) ? 64.0 : raw) / UNITS_PER_FULL_SCALE;
            break;
        }
        case eNodeMonoKey:
        {
            node->select = (uint32_t)module->param[variation][MONOKEY_PARAM_PRIORITY].value;   // §35
            break;
        }
        case eNodeGate:
        {
            // §38.2 - two drop-downs, read raw: a drop-down cannot be morphed.
            node->gateType[0] = (uint32_t)module->param[variation][GATE_PARAM_TYPE_1].value;
            node->gateType[1] = (uint32_t)module->param[variation][GATE_PARAM_TYPE_2].value;
            break;
        }
        case eNodeFlipFlop:
        {
            node->logicToggled = (module->param[variation][FLIPFLOP_PARAM_TYPE].value == FLIPFLOP_TYPE_RS);
            break;
        }
        case eNodeClkDiv:
        {
            // §38.4 - the dial reads one more than it holds, 1 to 128.
            node->divider      = (uint32_t)module->param[variation][CLKDIV_PARAM_DIVIDER].value + 1u;
            node->logicToggled = (module->param[variation][CLKDIV_PARAM_MODE].value == CLKDIV_MODE_TOGGLED);
            break;
        }
        case eNodeDrumSynth:
        {
            // §39 - each dial through the conversion the instrument gives it. The four decays are
            // the ENVELOPE's decay multiplier per tick, which is what §36.1's glide uses too; the
            // levels and amounts are the exponential level curve; the noise filter is the filter
            // cutoff law. Read raw where the dial is a drop-down or a button.
            static const uint32_t decayParam[4] = {
                DRUM_PARAM_MASTER_DECAY, DRUM_PARAM_SLAVE_DECAY,
                DRUM_PARAM_NOISE_DECAY,  DRUM_PARAM_BEND_DECAY
            };

            for (uint32_t d = 0; d < 4u; d++) {
                double ticks = adr_time_seconds(param_value(module, variation, decayParam[d])) * ENV_TICK_HZ;

                node->drumDecay[d] = exp(-log(100.0) / ((ticks < 1.0) ? 1.0 : ticks));
            }

            node->drumMasterHz     = drum_master_hz(param_value(module, variation, DRUM_PARAM_MASTER_FREQ));
            node->drumSlaveRatio   = drum_slave_ratio(param_value(module, variation, DRUM_PARAM_SLAVE_RATIO));
            node->drumLevel[0]     = mix_level_gain(param_value(module, variation, DRUM_PARAM_MASTER_LEVEL));
            node->drumLevel[1]     = mix_level_gain(param_value(module, variation, DRUM_PARAM_SLAVE_LEVEL));
            node->drumNoiseHz      = flt_cutoff_hz(param_value(module, variation, DRUM_PARAM_NOISE_FREQ));
            // §39.9 - the instrument sends Res as dial/512 capped at a quarter, and the filter's
            // damping is 1 - 3.2 of that, so it FLOORS AT 0.2 rather than running to nothing.
            node->drumNoiseRes     = 1.0 - (3.2 * fmin(0.25, param_value(module, variation,
                                                                         DRUM_PARAM_NOISE_RES) / 512.0));
            node->drumSweepOctaves = DRUM_SWEEP_OCTAVES
                                     * dial_fraction(param_value(module, variation, DRUM_PARAM_NOISE_SWEEP));
            node->drumBendOctaves  = DRUM_SWEEP_OCTAVES
                                     * mix_level_gain(param_value(module, variation, DRUM_PARAM_BEND_AMOUNT));
            node->drumClick        = mix_level_gain(param_value(module, variation, DRUM_PARAM_CLICK));
            node->drumClickDecay   = pow(DRUM_CLICK_DECAY_96K, DRUM_INSTRUMENT_RATE / gSampleRate);
            node->drumNoiseLevel   = mix_level_gain(param_value(module, variation, DRUM_PARAM_NOISE_AMOUNT));
            node->drumFilterType   = (uint32_t)module->param[variation][DRUM_PARAM_NOISE_TYPE].value;
            node->active           = (module->param[variation][DRUM_PARAM_ON].value != 0);
            break;
        }
        case eNodeGlide:
        {
            // §36 - Time off the instrument's own displayed table, as the patch glide reads its own.
            node->glideLin   = (module->param[variation][GLIDE_PARAM_SHAPE].value == GLIDE_SHAPE_LIN);
            node->glideCoeff = glide_tick_coeff(param_value(module, variation, GLIDE_PARAM_TIME),
                                                node->glideLin);
            node->active     = (module->param[variation][GLIDE_PARAM_ON].value != 0);
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

            if (module->type == moduleTypeOscDual) {
                oscdual_build(node, module, variation);
                break;
            }
            // A drop-down is read raw: a mode cannot carry a morph (manual p.20).
            uint32_t           wave = (p->waveMode >= 0) ? module->mode[p->waveMode].value
                            : (uint32_t)param_value(module, variation, (uint32_t)p->waveParam);

            // §6.2 - shape is the pulse offset: Sqr50/25/10 are fixed ones, OscB's is its Shape dial
            if (p->aWaves == true) {
                static const double kSqrOffset[] = {0.0, 0.5, 0.875};

                node->wave  = (wave >= 3u) ? eOscWaveSquare : (tOscWave)wave;
                node->shape = (wave >= 3u) ? kSqrOffset[(wave - 3u) < 3u ? (wave - 3u) : 2u] : 0.0;
            } else {
                node->wave  = (wave > (uint32_t)eOscWaveDualSaw) ? eOscWaveDualSaw : (tOscWave)wave;
                node->shape = wave_shape_word(param_value(module, variation, (uint32_t)p->shape) / 127.0);
            }
            node->oscCornerLimit = ((module->type == moduleTypeOscC) || (module->type == moduleTypeOscD))
                                   ? OSC_CORNER_LIMIT_PARTS : OSC_CORNER_LIMIT_MULTI;
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
                case moduleTypeFltHP:     node->topology  = eFilterTopologyCascadeHP;
                    break;
                case moduleTypeFltLP:     node->topology  = eFilterTopologyCascadeLP;
                    break;
                case moduleTypeFltStatic: node->topology  = eFilterTopologyBiquad;
                    break;
                case moduleTypeFltClassic: node->topology = eFilterTopologyClassic;
                    break;
                case moduleTypeFltNord:    node->topology = engine_filter_legacy() ? eFilterTopologyLadder : eFilterTopologyNord;
                    break;
                default:                  node->topology  = eFilterTopologyLadder;
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

                node->fltGain = engine_filter_legacy() ? ((1.0 + flt_ladder_feedback(res)) * gc) : 1.0;    // §23.4 - GC is the drive now
            } else {
                node->fltGain = 1.0;
            }
            node->fltKbt      = (map.kbt >= 0)
                              ? flt_kbt_amount((uint32_t)param_value(module, variation, (uint32_t)map.kbt)) : 0.0;
            node->modAmount   = (map.env >= 0)
                              ? (param_value(module, variation, (uint32_t)map.env) * 2.0 / 128.0) : 0.0;
            node->active      = (param_value(module, variation, (uint32_t)map.active) != 0.0);
            node->fltGainComp = (  (module->type == moduleTypeFltStatic)
                                && (param_value(module, variation, FLTSTATIC_PARAM_GC) != 0.0))
                                || (  (module->type == moduleTypeFltNord) && (map.gc >= 0)
                                   && (param_value(module, variation, (uint32_t)map.gc) != 0.0));
            break;
        }
        case eNodeDx:
        {
            dx_build(params, node, module, variation);
            break;
        }
        case eNodeEnv:
        {
            // Read raw: Shape is a drop-down, and drop-downs cannot be morphed (manual p.20).
            // §17.9 - the stages come from the map this module shares with its own face, so every
            // envelope module plays, not just EnvADSR. The map sets wave (shape) and envOutType too.
            env_stages_build(node, module, variation);

            // §17.4
            {
                uint32_t  envJacks[3];
                uint32_t  envCount   = env_input_connectors(module->type, envJacks);
                int       gateJack   = (envCount > ENV_INPUT_GATE) ? (int)envJacks[ENV_INPUT_GATE] : -1;
                uint32_t  sourceLeg  = 0;
                tModule * gateSource = (gateJack >= 0) ? module_feeding(module, (uint32_t)gateJack, &sourceLeg) : NULL;
                bool      unplayed   = (gateSource != NULL) && (node->in[ENV_INPUT_GATE] < 0);

                // §17.4 - KB and Reset sit at EnvADSR's own parameter numbers. The other envelope
                // modules number theirs differently and are not read here yet, so they gate from the
                // key as an unpatched envelope does and never reset. envOutType came from the map.
                bool      adsr       = (module->type == moduleTypeEnvADSR);

                node->envKeyGate = adsr ? ((module->param[variation][ENV_PARAM_KB].value != 0) || unplayed)
                                        : true;
                node->envReset   = adsr && (module->param[variation][ENV_PARAM_RESET].value != 0);
            }
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
// A node that makes a signal out of nothing, so a chain containing one is not silent by
// construction. ONE list, because the two callers below disagreeing is how DrumSynth's own rig
// came to report "Nothing is patched into it" - notes §193.
static bool node_is_generator(tNodeKind kind) {
    return (kind == eNodeOsc)
           || (kind == eNodeOscShp)
           || (kind == eNodePulse)
           || (kind == eNodeNoise)
           || (kind == eNodeOscNoise)
           || (kind == eNodeDx)
           || (kind == eNodeDrumSynth);
}

static bool chain_has_source(const tSoundEngineParams * params) {
    uint32_t i = 0;

    for (i = 0; i < params->nodeCount; i++) {
        if (node_is_generator(params->node[i].kind) == true) {
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
        if ((node_is_generator(params->node[i].kind) == true) && (params->node[i].active == true)) {
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

// The whole chain from the patch, at the per-voice morph amounts in sBuildAxis. Database read lock held.
static void build_snapshot(tSoundEngineParams * out) {
    SE_LOCAL;

    static _Thread_local tSoundEngineParams snapshot;
    tModule *                               tapModule = NULL;
    uint32_t                                variation = 0;

    // Zeroed whole, padding too: whether a build changed anything is decided by comparing bytes.
    memset(&snapshot, 0, sizeof(snapshot));
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
                // §15.6 - depth is in cents as the dial reads it; the rate is vibrato_rate_hz().
                snapshot.vibratoSource = vibrato->param[0][VIBRATO_MOD].value;
                snapshot.vibratoCents  = (double)vibrato->param[0][VIBRATO_DEPTH].value;
                snapshot.vibratoHz     = vibrato_rate_hz((double)vibrato->param[0][VIBRATO_RATE].value);
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
        uint32_t i        = 0;
        uint32_t lines    = 0;
        uint32_t verbs    = 0;
        uint32_t combs    = 0;
        uint32_t choruses = 0;

        for (i = 0; i < snapshot.nodeCount; i++) {
            if (snapshot.node[i].kind == eNodeDelay) {
                snapshot.node[i].line = lines++;
            } else if (snapshot.node[i].kind == eNodeReverb) {
                snapshot.node[i].line = verbs++;
            } else if (snapshot.node[i].kind == eNodeFltComb) {
                snapshot.node[i].line = combs++;
            } else if (snapshot.node[i].kind == eNodeChorus) {
                snapshot.node[i].line = choruses++;
            }
        }
    }
    mark_post_mix_nodes(&snapshot);
    snapshot.topology   = topology_signature(&snapshot);
    snapshot.voiceCount = voice_count_for_patch(engine_slot());

    memcpy(out, &snapshot, sizeof(snapshot));
}

// §26.2 - one module's node alone, at the morph amounts in sBuildAxis. Its wiring is the base build's.
static bool build_module_node(const tEngineNode * base, uint32_t variation, tEngineNode * out, tDxOperator * opsOut) {
    SE_LOCAL;

    static _Thread_local tSoundEngineParams part;
    tModule *                               module = get_module_slot(engine_slot(), base->location, base->moduleIndex);

    memset(&part, 0, sizeof(part));
    part.tap         = -1;

    int32_t                                 self   = (module != NULL) ? add_node(&part, module, variation, 0) : -1;

    if (self < 0) {
        return false;
    }
    *out             = part.node[self];

    // §26.2 - a DXRouter's Operators were rebuilt at this amount too; out->dxBase below goes back to
    // the base build's, which is what the per-voice state arrays are keyed on.
    if (  (opsOut != NULL) && (base->kind == eNodeDx)
       && ((part.node[self].dxBase + DX_OPERATORS) <= part.dxOpCount)) {
        memcpy(opsOut, &part.dxOp[part.node[self].dxBase], DX_OPERATORS * sizeof(tDxOperator));
    }
    out->kind        = base->kind;
    out->moduleIndex = base->moduleIndex;
    out->location    = base->location;
    out->inCount     = base->inCount;
    out->line        = base->line;
    out->dxBase      = base->dxBase;
    out->postMix     = base->postMix;
    memcpy(out->in, base->in, sizeof(out->in));
    memcpy(out->srcOut, base->srcOut, sizeof(out->srcOut));
    memcpy(out->srcLeg, base->srcLeg, sizeof(out->srcLeg));
    return true;
}

// §26.2 - a DXRouter whose Operators alone move: the router's own node can be identical, since every
// Operator parameter lives on the Operator module rather than on it.
static bool dx_operators_differ(const tSoundEngineParams * base, const tSoundEngineParams * probe, uint32_t n) {
    uint32_t dxBase = base->node[n].dxBase;

    if (  (base->node[n].kind != eNodeDx) || (probe->node[n].dxBase != dxBase)
       || ((dxBase + DX_OPERATORS) > base->dxOpCount) || ((dxBase + DX_OPERATORS) > probe->dxOpCount)) {
        return false;
    }
    return memcmp(&probe->dxOp[dxBase], &base->dxOp[dxBase], DX_OPERATORS * sizeof(tDxOperator)) != 0;
}

// §26.2.2 - the words of `rows` copies of a `words`-word object that differ from the base one. Compared
// and copied eight bytes at a time through memcmp/memcpy, not through a uint64 pointer: a tEngineNode
// is not an array of integers and reading it as one is what strict aliasing forbids.
static void mark_moved_words(uint64_t * mask, const void * base, const void * rows, uint32_t words,
                             uint32_t rowCount, size_t rowStride) {
    memset(mask, 0, MORPH_MASK_WORDS * sizeof(uint64_t));

    for (uint32_t row = 0; row < rowCount; row++) {
        const char * from = (const char *)rows + (row * rowStride);

        for (uint32_t w = 0; w < words; w++) {
            size_t at = (size_t)w * MORPH_WORD_BYTES;

            if (memcmp(from + at, (const char *)base + at, MORPH_WORD_BYTES) != 0) {
                mask[w >> 6] |= (uint64_t)1u << (w & 63u);
            }
        }
    }
}

// §26.2.2 - one object built from the base and the two axes: each word from whichever axis moves it.
// Where BOTH move the same word the Keyb one stands, exactly as a voice used to play the whole Keyb
// node - eval_node() still adds both offsets to the smoothed values, which is where that case is
// handled as well as two per-axis builds allow (§26.2.0).
static void merge_moved_words(void * out, const void * base, uint32_t words,
                              const void * fromVel, const uint64_t * velMoves,
                              const void * fromKey, const uint64_t * keyMoves) {
    memcpy(out, base, (size_t)words * MORPH_WORD_BYTES);

    for (uint32_t w = 0; w < words; w++) {
        size_t   at  = (size_t)w * MORPH_WORD_BYTES;
        uint64_t bit = (uint64_t)1u << (w & 63u);

        if ((velMoves[w >> 6] & bit) != 0u) {
            memcpy((char *)out + at, (const char *)fromVel + at, MORPH_WORD_BYTES);
        }

        if ((keyMoves[w >> 6] & bit) != 0u) {
            memcpy((char *)out + at, (const char *)fromKey + at, MORPH_WORD_BYTES);
        }
    }
}

// §26.2.3 - the words both axes move, built at every PAIR of amounts. This is the one case two
// per-axis builds cannot answer: the instrument sums both offsets into one dial value and clamps once
// (§26.2.0), so a parameter both morphs move has to be built at (velocity, key) together. Only the
// shared words are kept - the rest of the node the merge already settles - which is what keeps a
// VEL_MORPH_LEVELS x KEY_MORPH_LEVELS table to a few hundred KB instead of a few hundred MB.
// The words of one object that BOTH masks move, packed into a list.
static uint32_t shared_words(uint8_t * out, uint32_t words, const uint64_t * velMoves, const uint64_t * keyMoves) {
    uint32_t count = 0;

    for (uint32_t w = 0; (w < words) && (count < PAIR_MAX_WORDS); w++) {
        uint64_t bit = (uint64_t)1u << (w & 63u);

        if (((velMoves[w >> 6] & bit) != 0u) && ((keyMoves[w >> 6] & bit) != 0u)) {
            out[count++] = (uint8_t)w;
        }
    }

    return count;
}

static void build_pair_table(tPairTable * pair, const tSoundEngineParams * base, uint32_t n,
                             const uint64_t * velMoves, const uint64_t * keyMoves,
                             const uint64_t * velDxMoves, const uint64_t * keyDxMoves, uint32_t variation) {
    SE_LOCAL;

    static _Thread_local tEngineNode cell;
    static _Thread_local tDxOperator ops[DX_OPERATORS];

    pair->wordCount   = shared_words(pair->word, NODE_WORDS, velMoves, keyMoves);
    pair->dxWordCount = (velDxMoves != NULL) ? shared_words(pair->dxWord, DX_SET_WORDS, velDxMoves, keyDxMoves) : 0u;

    if ((pair->wordCount == 0u) && (pair->dxWordCount == 0u)) {
        return;     // the two axes move disjoint parameters here: the merge is already exact
    }

    for (uint32_t v = 0; v < VEL_MORPH_LEVELS; v++) {
        for (uint32_t k = 0; k < KEY_MORPH_LEVELS; k++) {
            sBuildAxis[eAxisVelocity] = axis_amount(eAxisVelocity, v);
            sBuildAxis[eAxisKey]      = axis_amount(eAxisKey, k);

            if (build_module_node(&base->node[n], variation, &cell, ops) == false) {
                cell = base->node[n];
                memcpy(ops, &base->dxOp[base->node[n].dxBase], sizeof(ops));
            }

            for (uint32_t i = 0; i < pair->wordCount; i++) {
                memcpy(&pair->value[v][k][i], (const char *)&cell + ((size_t)pair->word[i] * MORPH_WORD_BYTES),
                       MORPH_WORD_BYTES);
            }

            for (uint32_t i = 0; i < pair->dxWordCount; i++) {
                memcpy(&pair->dxValue[v][k][i], (const char *)ops + ((size_t)pair->dxWord[i] * MORPH_WORD_BYTES),
                       MORPH_WORD_BYTES);
            }
        }
    }

    sBuildAxis[eAxisVelocity] = 0.0;
    sBuildAxis[eAxisKey]      = 0.0;
}

// §26.2 - one axis's table. The nodes that move are the ones its full-amount build does not agree with.
static void build_axis_table(tMorphAxis axis, const tSoundEngineParams * base, const tSoundEngineParams * probe) {
    SE_LOCAL;

    tMorphTable * table     = &gVoiceMorphs.axis[axis];
    uint32_t      variation = gPatchDescr[engine_slot()].activeVariation;
    uint32_t      count     = 0;

    table->count   = 0;
    table->dxCount = 0;
    memset(table->column, -1, sizeof(table->column));
    memset(table->dxSlot, -1, sizeof(table->dxSlot));
    memset(table->moves, 0, sizeof(table->moves));
    memset(table->dxMoves, 0, sizeof(table->dxMoves));

    if ((probe->nodeCount == base->nodeCount) && (probe->topology == base->topology)) {
        for (uint32_t n = 0; (n < base->nodeCount) && (count < MAX_VOICE_NODES); n++) {
            if (  (memcmp(&probe->node[n], &base->node[n], sizeof(tEngineNode)) != 0)
               || (dx_operators_differ(base, probe, n) == true)) {
                table->column[n] = (int8_t)count++;

                if (  (base->node[n].kind == eNodeDx) && (table->dxCount < MAX_VOICE_DX_NODES)
                   && ((base->node[n].dxBase + DX_OPERATORS) <= base->dxOpCount)) {
                    table->dxSlot[n] = (int8_t)table->dxCount++;
                }
            }
        }
    }

    for (uint32_t n = 0; (count > 0) && (n < base->nodeCount); n++) {
        if (table->column[n] < 0) {
            continue;
        }

        for (uint32_t row = 0; row < axis_rows(axis); row++) {
            tEngineNode * cell = &table->node[row][table->column[n]];
            tDxOperator * ops  = (table->dxSlot[n] >= 0) ? table->dxOp[row][table->dxSlot[n]] : NULL;

            sBuildAxis[axis] = axis_amount(axis, row);

            // The base build's Operators stand until this row's build replaces them.
            if (ops != NULL) {
                memcpy(ops, &base->dxOp[base->node[n].dxBase], DX_OPERATORS * sizeof(tDxOperator));
            }

            if (build_module_node(&base->node[n], variation, cell, ops) == false) {
                *cell = base->node[n];
            }
        }

        sBuildAxis[axis] = 0.0;

        // §26.2.2 - what this axis actually moves, so a voice can take those words and no others
        mark_moved_words(table->moves[table->column[n]], &base->node[n], &table->node[0][table->column[n]],
                         NODE_WORDS, axis_rows(axis), sizeof(table->node[0]));

        if (table->dxSlot[n] >= 0) {
            mark_moved_words(table->dxMoves[table->dxSlot[n]], &base->dxOp[base->node[n].dxBase],
                             &table->dxOp[0][table->dxSlot[n]][0], DX_SET_WORDS, axis_rows(axis),
                             sizeof(table->dxOp[0]));
        }
    }

    table->count = count;
}

void sound_engine_update_from_patch(void) {
    SE_LOCAL;

    static _Thread_local tSoundEngineParams snapshot;
    static _Thread_local tSoundEngineParams probe[eAxisCount];

    if (atomic_load(&gActive) == false) {
        return;
    }

    // §26.2 - at each per-voice morph's full amount as well, so a changed range is seen even when
    // nothing else moved
    for (uint32_t axis = 0; axis < eAxisCount; axis++) {
        sBuildAxis[axis] = 1.0;
        build_snapshot(&probe[axis]);
        sBuildAxis[axis] = 0.0;
    }

    build_snapshot(&snapshot);

    // How many voices the audio thread may allocate. Published separately as well as in the snapshot
    // because the note stack asks the same question from the MIDI thread, where reading the whole
    // snapshot to answer it would be absurd.
    atomic_store(&gEngineVoices, snapshot.voiceCount);
    atomic_store(&gEngineLegato, gPatchDescr[engine_slot()].monoPoly == monoPolyLegato);
    atomic_store(&gEngineMono, gPatchDescr[engine_slot()].monoPoly != monoPolyPoly);

    // The snapshot above was built outside the writers' mutex; the velocity table is built inside it,
    // since it is written in place.
    pthread_mutex_lock(&gParamsWriteMutex);
    // Rebuilt on every redraw, so the per-voice tables are rebuilt only when the chain really changed.
    bool changed = false;

    snapshot.build = gParams.build;

    for (uint32_t axis = 0; axis < eAxisCount; axis++) {
        probe[axis].build = gAxisProbe[axis].build;
        changed           = changed || (memcmp(&probe[axis], &gAxisProbe[axis], sizeof(tSoundEngineParams)) != 0);
    }

    changed        = changed || (memcmp(&snapshot, &gParams, sizeof(snapshot)) != 0);

    if (changed == true) {
        snapshot.build             = ++gBuildSerial;
        atomic_fetch_add(&gVoiceMorphsSeq, 1);    // odd while the tables are being written
        gVoiceMorphs.build         = snapshot.build;

        for (uint32_t axis = 0; axis < eAxisCount; axis++) {
            memcpy(&gAxisProbe[axis], &probe[axis], sizeof(tSoundEngineParams));
            build_axis_table((tMorphAxis)axis, &snapshot, &probe[axis]);
        }

        // §26.2.2 - which nodes BOTH axes move. Only those need merging; a node one axis moves is
        // already exact, since building at (a, 0) or (0, b) is building at its own pair.
        gVoiceMorphs.pairCount     = 0;
        memset(gVoiceMorphs.pairSlot, -1, sizeof(gVoiceMorphs.pairSlot));
        gVoiceMorphs.mergedCount   = 0;
        gVoiceMorphs.mergedDxCount = 0;
        memset(gVoiceMorphs.mergedSlot, -1, sizeof(gVoiceMorphs.mergedSlot));
        memset(gVoiceMorphs.mergedDxSlot, -1, sizeof(gVoiceMorphs.mergedDxSlot));

        for (uint32_t n = 0; n < snapshot.nodeCount; n++) {
            const tMorphTable * vel = &gVoiceMorphs.axis[eAxisVelocity];
            const tMorphTable * key = &gVoiceMorphs.axis[eAxisKey];

            if (  (vel->column[n] < 0) || (key->column[n] < 0)
               || (gVoiceMorphs.mergedCount >= MAX_VOICE_NODES)) {
                continue;
            }
            gVoiceMorphs.mergedSlot[n] = (int8_t)gVoiceMorphs.mergedCount++;

            if (  (vel->dxSlot[n] >= 0) && (key->dxSlot[n] >= 0)
               && (gVoiceMorphs.mergedDxCount < MAX_VOICE_DX_NODES)) {
                gVoiceMorphs.mergedDxSlot[n] = (int8_t)gVoiceMorphs.mergedDxCount++;
            }

            // §26.2.3 - the words both axes move are the ones the merge cannot settle, because the
            // answer is a build at the pair rather than either axis's own build.
            if (gVoiceMorphs.pairCount < MAX_PAIR_NODES) {
                bool         dxBoth = (vel->dxSlot[n] >= 0) && (key->dxSlot[n] >= 0);
                tPairTable * pair   = &gVoiceMorphs.pair[gVoiceMorphs.pairCount];

                build_pair_table(pair, &snapshot, n,
                                 vel->moves[vel->column[n]], key->moves[key->column[n]],
                                 dxBoth ? vel->dxMoves[vel->dxSlot[n]] : NULL,
                                 dxBoth ? key->dxMoves[key->dxSlot[n]] : NULL,
                                 gPatchDescr[engine_slot()].activeVariation);

                if ((pair->wordCount + pair->dxWordCount) > 0u) {
                    gVoiceMorphs.pairSlot[n] = (int8_t)gVoiceMorphs.pairCount++;
                }
            }
        }

        atomic_fetch_add(&gVoiceMorphsSeq, 1);
    }
    atomic_fetch_add(&gParamsSeq, 1);    // now odd — a reader seeing this discards its copy
    memcpy(&gParams, &snapshot, sizeof(snapshot));
    atomic_fetch_add(&gParamsSeq, 1);    // even again, snapshot is whole
    pthread_mutex_unlock(&gParamsWriteMutex);
}

// §26.2 - audio thread: take the per-voice tables when new ones are whole, and use them only with
// their own build. Only the cells in use are copied.
static void refresh_voice_morphs(uint64_t build) {
    SE_LOCAL;

    uint32_t seq = atomic_load(&gVoiceMorphsSeq);

    if ((seq != gVoiceMorphsSeen) && ((seq & 1u) == 0u)) {
        gVoiceMorphsAudio.build         = gVoiceMorphs.build;
        gVoiceMorphsAudio.mergedCount   = gVoiceMorphs.mergedCount;
        gVoiceMorphsAudio.mergedDxCount = gVoiceMorphs.mergedDxCount;
        gVoiceMorphsAudio.pairCount     = gVoiceMorphs.pairCount;
        memcpy(gVoiceMorphsAudio.mergedSlot, gVoiceMorphs.mergedSlot, sizeof(gVoiceMorphsAudio.mergedSlot));
        memcpy(gVoiceMorphsAudio.mergedDxSlot, gVoiceMorphs.mergedDxSlot, sizeof(gVoiceMorphsAudio.mergedDxSlot));
        memcpy(gVoiceMorphsAudio.pairSlot, gVoiceMorphs.pairSlot, sizeof(gVoiceMorphsAudio.pairSlot));
        memcpy(gVoiceMorphsAudio.pair, gVoiceMorphs.pair, gVoiceMorphs.pairCount * sizeof(tPairTable));

        for (uint32_t axis = 0; axis < eAxisCount; axis++) {
            const tMorphTable * from   = &gVoiceMorphs.axis[axis];
            tMorphTable *       to     = &gVoiceMorphsAudio.axis[axis];
            uint32_t            used   = (from->count < MAX_VOICE_NODES) ? from->count : MAX_VOICE_NODES;

            uint32_t            dxUsed = (from->dxCount < MAX_VOICE_DX_NODES) ? from->dxCount : MAX_VOICE_DX_NODES;

            to->count   = used;
            to->dxCount = dxUsed;
            memcpy(to->column, from->column, sizeof(to->column));
            memcpy(to->dxSlot, from->dxSlot, sizeof(to->dxSlot));
            memcpy(to->moves, from->moves, sizeof(to->moves));
            memcpy(to->dxMoves, from->dxMoves, sizeof(to->dxMoves));

            for (uint32_t row = 0; row < axis_rows((tMorphAxis)axis); row++) {
                memcpy(to->node[row], from->node[row], used * sizeof(tEngineNode));
                memcpy(to->dxOp[row], from->dxOp[row], dxUsed * DX_OPERATORS * sizeof(tDxOperator));
            }
        }

        atomic_thread_fence(memory_order_acquire);

        if (atomic_load(&gVoiceMorphsSeq) == seq) {
            gVoiceMorphsSeen = seq;
        } else {
            gVoiceMorphsAudio.build = 0;    // torn - try again next buffer
        }
    }
    gVoiceMorphsUsable = (gVoiceMorphsAudio.build == build)
                         && ((gVoiceMorphsAudio.axis[eAxisVelocity].count + gVoiceMorphsAudio.axis[eAxisKey].count) > 0);
}

// §26.2 - the node a voice plays on one axis: its row where that morph moves the node, else the base
static const tEngineNode * voice_morph_node(tMorphAxis axis, const tEngineNode * base, uint32_t n, uint32_t voice) {
    SE_LOCAL;

    const tMorphTable * table = &gVoiceMorphsAudio.axis[axis];

    if ((gVoiceMorphsUsable == false) || (table->column[n] < 0)) {
        return base;
    }
    uint8_t             row   = (base->postMix == true) ? gLastRow[axis] : gVoice[voice].row[axis];

    return &table->node[row][table->column[n]];
}

// §26.2 - the six Operators a voice plays on one axis, or NULL where that morph does not move them
static const tDxOperator * voice_morph_ops(tMorphAxis axis, const tEngineNode * base, uint32_t n, uint32_t voice) {
    SE_LOCAL;

    const tMorphTable * table = &gVoiceMorphsAudio.axis[axis];

    if ((gVoiceMorphsUsable == false) || (table->dxSlot[n] < 0)) {
        return NULL;
    }
    uint8_t             row   = (base->postMix == true) ? gLastRow[axis] : gVoice[voice].row[axis];

    return table->dxOp[row][table->dxSlot[n]];
}

// §26.2.2 - the merged nodes for one set of axis rows. `rows` is a voice's pair, or gLastRow for the
// post-mix nodes, which follow the latest note.
static void merge_nodes_for(const tSoundEngineParams * params, const uint8_t * rows,
                            tEngineNode * outNode, tDxOperator(*outOps)[DX_OPERATORS]) {
    SE_LOCAL;

    const tVoiceMorphs * morphs = &gVoiceMorphsAudio;
    const tMorphTable *  vel    = &morphs->axis[eAxisVelocity];
    const tMorphTable *  key    = &morphs->axis[eAxisKey];
    uint8_t              velRow = rows[eAxisVelocity];
    uint8_t              keyRow = rows[eAxisKey];

    for (uint32_t n = 0; n < params->nodeCount; n++) {
        int8_t slot     = morphs->mergedSlot[n];

        if (slot < 0) {
            continue;
        }
        merge_moved_words(&outNode[slot], &params->node[n], NODE_WORDS,
                          &vel->node[velRow][vel->column[n]], vel->moves[vel->column[n]],
                          &key->node[keyRow][key->column[n]], key->moves[key->column[n]]);

        int8_t dxSlot   = morphs->mergedDxSlot[n];

        if (dxSlot >= 0) {
            merge_moved_words(outOps[dxSlot], &params->dxOp[params->node[n].dxBase], DX_SET_WORDS,
                              vel->dxOp[velRow][vel->dxSlot[n]], vel->dxMoves[vel->dxSlot[n]],
                              key->dxOp[keyRow][key->dxSlot[n]], key->dxMoves[key->dxSlot[n]]);
        }
        // §26.2.3 - LAST, over whichever axis the merges above took: the words both axes move are
        // right only in the build at this voice's PAIR of amounts.
        int8_t pairSlot = morphs->pairSlot[n];

        if (pairSlot >= 0) {
            const tPairTable * pair = &morphs->pair[pairSlot];

            for (uint32_t i = 0; i < pair->wordCount; i++) {
                memcpy((char *)&outNode[slot] + ((size_t)pair->word[i] * MORPH_WORD_BYTES),
                       &pair->value[velRow][keyRow][i], MORPH_WORD_BYTES);
            }

            if (dxSlot >= 0) {
                for (uint32_t i = 0; i < pair->dxWordCount; i++) {
                    memcpy((char *)outOps[dxSlot] + ((size_t)pair->dxWord[i] * MORPH_WORD_BYTES),
                           &pair->dxValue[velRow][keyRow][i], MORPH_WORD_BYTES);
                }
            }
        }
    }
}

static void merge_voice_nodes(uint32_t voice, const tSoundEngineParams * params) {
    SE_LOCAL;

    if ((gVoiceMorphsUsable == false) || (gVoiceMorphsAudio.mergedCount == 0u)) {
        return;
    }
    merge_nodes_for(params, gVoice[voice].row, gMergedNode[voice], gMergedOps[voice]);
}

static void merge_last_nodes(const tSoundEngineParams * params) {
    SE_LOCAL;

    if ((gVoiceMorphsUsable == false) || (gVoiceMorphsAudio.mergedCount == 0u)) {
        return;
    }
    merge_nodes_for(params, gLastRow, gMergedLast, gMergedLastOps);
}

// §26.2.2 - the node a voice plays where BOTH axes move it. Where only one does, or neither, the
// per-axis node stands and nothing was merged.
static const tEngineNode * voice_spec_node(const tEngineNode * base, uint32_t n, uint32_t voice,
                                           const tEngineNode * byVel, const tEngineNode * byKey) {
    SE_LOCAL;

    if (gVoiceMorphsUsable == true) {
        int8_t slot = gVoiceMorphsAudio.mergedSlot[n];

        if (slot >= 0) {
            return (base->postMix == true) ? &gMergedLast[slot] : &gMergedNode[voice][slot];
        }
    }
    return (byKey != base) ? byKey : byVel;
}

static const tDxOperator * voice_merged_ops(const tEngineNode * base, uint32_t n, uint32_t voice) {
    SE_LOCAL;

    if (gVoiceMorphsUsable == false) {
        return NULL;
    }
    int8_t dxSlot = gVoiceMorphsAudio.mergedDxSlot[n];

    if (dxSlot < 0) {
        return NULL;
    }
    return (base->postMix == true) ? gMergedLastOps[dxSlot] : gMergedOps[voice][dxSlot];
}

// Audio thread half of the seqlock. Returns the newest whole snapshot, or the last one it managed to
// read cleanly if the UI thread happens to be publishing right now — one buffer of slightly stale
// parameters is inaudible, and blocking here would not be.
static tSoundEngineParams read_params(void) {
    SE_LOCAL;

    uint32_t attempt = 0;

    for (attempt = 0; attempt < PARAMS_READ_ATTEMPTS; attempt++) {
        uint32_t                                before = atomic_load(&gParamsSeq);
        static _Thread_local tSoundEngineParams copy;

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

// Ramps DOWN from +1 at phase 0. OscShpB and OscDual use it as it is; the basic oscillators' saw is its
// negation half a cycle on (§6.3).
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
#define SHP_WAVE_SINE2           (1u)
#define SINE2_DC_COEFF           (4000.0 / 8388608.0) // §27.2 - per sample at SINE2_DC_RATE
#define SINE2_DC_RATE            (96000.0)
#define SHP_SINE1_SHORTEST       (2.0)                // §27.5 - samples
#define SHP_SINE2_SHORTEST       (4.0)
#define SHP_DSF_ORIGIN           (0.75)               // §27.5 - where Sine3/Sine4 start against the instrument's cycle
#define TRISAW_SHORTEST_RISE     (2.0)                // samples
#define TRISAW_CORNER_SAMPLES    (2.0)

// §27.5 - the instrument's phase: 0 at phase 0, rising to +1 at half a cycle, wrapping to -1
static double osc_instrument_phase(double phase) {
    return (phase < 0.5) ? (2.0 * phase) : ((2.0 * phase) - 2.0);
}

#define OSC_WORD_ONE    (8388607.0 / 8388608.0)    // §27.5 - the instrument's largest word, its "1"

static double osc_wrap_two(double value) {
    return value - (2.0 * floor((value + 1.0) / 2.0));
}

// §27.5 - a corner's rounding: turn is the change of slope per unit of phase, x the phase step per sample
static double shp_corner(double distance, double x, double turn) {
    double v = TRISAW_CORNER_SAMPLES - fabs(distance / x);

    return (v > 0.0) ? (turn * x * v * v * v / 24.0) : 0.0;
}

// §27.5 - TriSaw: falls from +1 at p = -y, rises over at least two samples from p = -y - rise
static double shp_trisaw(double p, double x, double y) {
    double rise   = fmax(1.0 - y, TRISAW_SHORTEST_RISE * x);
    double peak   = -y;
    double trough = osc_wrap_two(peak - rise);
    double since  = osc_wrap_two(p - trough);
    double value  = (since < 0.0) ? (since + 2.0) : since;
    double turn   = (2.0 / rise) + (2.0 / (2.0 - rise));

    value = (value < rise) ? (-1.0 + (2.0 * value / rise)) : (1.0 - (2.0 * (value - rise) / (2.0 - rise)));
    double atWrap = (y < 1.0) ? shp_corner(osc_wrap_two(p - 1.0), x, turn) : 0.0;    // at y = 1 the peak is the wrap

    return value - shp_corner(osc_wrap_two(p - peak), x, turn) + atWrap;
}

// §27.5 - Pulse: high above p = y, each edge a straight line one sample either side, and DC-free
static double shp_pulse(double p, double x, double y) {
    double value = (p > y) ? 1.0 : -1.0;
    double rise  = osc_wrap_two(p - y) / x;
    double fall  = osc_wrap_two(p - OSC_WORD_ONE) / x;

    if (fabs(rise) < 1.0) {
        value = rise;
    }

    if (fabs(fall) < 1.0) {
        value = -fall;
    }
    return value + y;
}

// §27.5 - SymPulse: -1, then 0, then +1, the outer two each (1 - y)/2 of the cycle
static double shp_sympulse(double phase, double edge, double y) {
    double w     = 0.5 * (1.0 - y);
    double value = (phase < w) ? -1.0 : ((phase < (1.0 - w)) ? 0.0 : 1.0);

    return value - poly_blep(phase, edge) + (0.5 * poly_blep(fmod(phase + 1.0 - w, 1.0), edge))
           + (0.5 * poly_blep(fmod(phase + w, 1.0), edge));
}

// §27 - inc96 is the phase step per 96 kHz sample
static double osc_shp_wave(uint32_t waveform, double phase, double inc96, double shape) {
    double y    = wave_shape_word(shape);
    double x    = 2.0 * inc96;
    double edge = fmin(OSC_EDGE_SAMPLES * inc96, 0.5);

    // notes §102
    switch (waveform) {
        case 0:
        {
            double rise = fmax(0.5 * (1.0 - y), SHP_SINE1_SHORTEST * inc96);    // peaks at half a cycle

            return wave_sine1_limited(fmod(phase + 0.5 + (0.5 * rise), 1.0), shape, SHP_SINE1_SHORTEST * inc96);
        }
        case 1:
        {
            // §27.2 - four samples at the least, and a gain of 1 + Shape; the DC blocker follows
            double lobe = fmax(0.5 * (1.0 - y), SHP_SINE2_SHORTEST * inc96);    // ends at half a cycle

            return wave_sine2_limited(fmod(phase + 0.5 + lobe, 1.0), shape, SHP_SINE2_SHORTEST * inc96) * (1.0 + y);
        }
        case 2:
        {
            return wave_sine3_instrument(fmod(phase + SHP_DSF_ORIGIN, 1.0), shape, inc96);    // §27.3
        }
        case 3:
        {
            return wave_sine4_instrument(fmod(phase + SHP_DSF_ORIGIN, 1.0), shape, inc96);
        }
        case 4:
        {
            return shp_trisaw(osc_instrument_phase(phase), x, y);
        }
        case 5:
        {
            return -osc_saw(phase, edge) - osc_saw(fmod(phase + (0.5 * y), 1.0), edge);
        }
        case 6:
        {
            return shp_pulse(osc_instrument_phase(phase), x, y);
        }
        default:
        {
            return shp_sympulse(phase, edge, y);
        }
    }
}

// §24.2 - the host's own arithmetic for a dial word: a 24-bit fraction multiplied as two 16-bit halves.
static int32_t dly_host_mul(int32_t a, int32_t b) {
    int32_t  ah = a >> 16;
    int32_t  bh = b >> 16;
    uint32_t al = (uint32_t)a & 0xffffu;
    uint32_t bl = (uint32_t)b & 0xffffu;

    return ((int32_t)((ah * (int32_t)bl) + (int32_t)((al * bl) >> 16) + ((int32_t)al * bh)) >> 7) + (ah * bh * 0x200);
}

static int32_t dly_dial_word(double dial) {
    int32_t v16 = (int32_t)floor(dial * 256.0);

    return (v16 >= 0x7f00) ? 0x7fffff : (v16 << 8);    // v/128, with 127 the word's top
}

// §24.2 - DelayA/DelayB's words, as the instrument's host sets them.
static void delay_words(tEngineNode * node, double lpDial, double hpDial, double fbDial, double dryWetDial,
                        double fbModDial, double mixModDial) {
    int32_t lp = ((int32_t)floor(lpDial * 256.0) * 0xdc) + 0x12dbff;
    int32_t hp = (int32_t)floor(hpDial * 256.0) << 7;

    node->damping     = dly_host_mul(lp, dly_host_mul(lp, lp)); // the LP's coefficient
    node->hpCoeff     = dly_host_mul(hp, dly_host_mul(hp, hp)); // the HP's; 0 passes
    node->depth       = dly_dial_word(fbDial);
    node->amount      = dly_dial_word(dryWetDial);
    node->delayFbMod  = dly_dial_word(fbModDial);
    node->delayMixMod = dly_dial_word(mixModDial);
}

static int32_t dly_sat(int64_t v) {
    return (int32_t)((v > 0x7fffff) ? 0x7fffff : ((v < -0x800000) ? -0x800000 : v));
}

// §24 - DelayA/DelayB: the instrument's tap in its own integer arithmetic, on half-scale 24-bit words.
// The input and the last sample's feedback go into memory; the tap comes out through the LP and the
// HP, and that filtered signal is both the wet output and what feeds back.
static double delay_step(uint32_t line, double input, double fbModIn, double mixModIn, bool modLinked,
                         const tEngineNode * spec) {
    SE_LOCAL;

    if (line >= MAX_DELAY_LINES) {
        return input;
    }
    double   exact   = spec->timeSeconds * gSampleRate;
    uint32_t samples = (exact < 0.5) ? 0u : (uint32_t)lround(exact);

    if (samples >= DELAY_LINE_SAMPLES) {
        samples = DELAY_LINE_SAMPLES - 1;
    }
    int32_t  c       = (int32_t)spec->damping;
    int32_t  f       = (int32_t)spec->hpCoeff;
    int32_t  fb      = modLinked ? (int32_t)gDelayMod[line][0] : (int32_t)spec->depth;
    int32_t  mixWord = (int32_t)spec->amount;
    int32_t  half    = dly_sat(((int64_t)dly_sat((int64_t)floor(input * 2097152.0)) * 0x400000) >> 23);
    uint32_t write   = gDelayWrite[line];

    gDelayLine[line][write] = (float)dly_sat((int64_t)half + (int64_t)gDelayFb[line]);

    uint32_t readPos = (write + DELAY_LINE_SAMPLES - samples) % DELAY_LINE_SAMPLES;
    int32_t  tap     = (int32_t)gDelayLine[line][readPos] & ~0xff;    // §24.3 - 16-bit memory
    int32_t  x2      = (int32_t)gDelayDamp[line];
    int32_t  lp      = dly_sat((int64_t)x2 + ((((int64_t)c * tap) - ((int64_t)c * x2)) >> 23));
    int32_t  a       = (int32_t)gDelayHp[line];
    int32_t  b       = (int32_t)gDelayHpB[line];
    int32_t  t       = b + (int32_t)(((int64_t)f * a) >> 23);
    int32_t  high    = dly_sat((int64_t)lp - (3 * (int64_t)b) + ((((int64_t)f * b) - ((int64_t)f * a)) >> 23));
    int32_t  gain    = dly_sat(0x800000 - (int64_t)f + ((-(int64_t)f * f) >> 23));
    int32_t  y       = dly_sat(((int64_t)high * gain) >> 23);
    int32_t  xw      = mixWord;
    int32_t  wet     = dly_host_mul((xw < 0x400000) ? (xw << 1) : 0x7fffff, (xw < 0x400000) ? (xw << 1) : 0x7fffff);    // §24.4
    int32_t  dryRamp = (xw > 0x400000) ? ((0x7fffff - xw) * 2) : 0x7fffff;
    int32_t  dry     = dly_host_mul(dryRamp, dryRamp);

    if (modLinked) {
        wet = (int32_t)gDelayMod[line][1];    // §24.6 - the modulation part's words, not squared
        dry = (int32_t)gDelayMod[line][2];
    }
    int32_t  out     = dly_sat((((int64_t)y * wet) + ((int64_t)half * dry)) >> 22);

    gDelayDamp[line]  = lp;
    gDelayHp[line]    = dly_sat(t);
    gDelayHpB[line]   = dly_sat((int64_t)b + (((int64_t)f * high) >> 23));
    gDelayFb[line]    = dly_sat(((int64_t)y * fb) >> 23);
    gDelayWrite[line] = (write + 1) % DELAY_LINE_SAMPLES;

    // §24.6 - DelayB's modulation part, after the tap: FB and DryWet plus four times input x amount,
    // floored at zero, for the next sample.
    if (modLinked) {
        int64_t top  = (int64_t)0x7fffff << 21;
        int64_t fbv  = (((int64_t)dly_sat((int64_t)floor(fbModIn * 2097152.0)) * (int32_t)spec->delayFbMod) >> 21) + (int32_t)spec->depth;
        int64_t mixv = ((int64_t)dly_sat((int64_t)floor(mixModIn * 2097152.0)) * (int32_t)spec->delayMixMod)
                       + ((int64_t)(int32_t)spec->amount << 21);

        if (mixv < 0) {
            mixv = 0;
        } else if ((mixv >> 21) > 0x7fffff) {
            mixv = top;
        }
        gDelayMod[line][0] = dly_sat((fbv < 0) ? 0 : fbv);
        gDelayMod[line][1] = dly_sat(mixv >> 20);
        gDelayMod[line][2] = dly_sat((top - mixv) >> 20);
    }
    return (double)out / 2097152.0;
}

// §19 - StChorus. Tap positions count samples at CHORUS_TAP_RATE_HZ.
#define CHORUS_TICK_HZ                (24000.0)  // the LFO steps at the control rate
#define CHORUS_PHASE_HALF             (8388608)  // a signed 24-bit phase: -1..1 is one LFO cycle
#define CHORUS_DETUNE_STEP            (8.0)      // phase step per tick per Detune step
#define CHORUS_TAP_RATE_HZ            (96000.0)
#define CHORUS_TAP1_MAX               (505.0)
#define CHORUS_TAP1_SPAN              (504.0)
#define CHORUS_TAP2_MIN               (65.0)
#define CHORUS_TAP2_SPAN              (378.0)    // three quarters of tap 1's, the other way
#define CHORUS_FRACTION_STEPS         (32.0)     // a tap position resolves to 1/32 sample

#define OSCNOISE_Q_AT_FULL_WIDTH      (3.34)     // §8.3
#define OSCNOISE_Q_GROWTH_PER_STEP    (0.032)
#define OSCNOISE_LEVEL                (0.5957)   // -4.5 dB RMS, §8.4

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
    if (spec->active == false) {
        return input;                 // Bypass passes the signal through untouched
    }
    // The mod jack adds to the dial through its own attenuator, and the sum is clamped to the
    // dial's range - the same treatment the filter's cutoff modulation gets.
    return shaper_transfer(&spec->shaper, spec->shaper.amount + (spec->shaper.mod * modulation), input);
}

// notes §106
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

static int32_t chorus_sign24(uint32_t word) {
    word &= 0xFFFFFFu;
    return (word >= 0x800000u) ? ((int32_t)word - 0x1000000) : (int32_t)word;
}

static uint32_t chorus_scramble(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

// §19.2 - each instance draws its own start phase and rate trim, as the instrument does at load;
// drawn from the node index here so that a render repeats.
static void chorus_reset(uint32_t line) {
    SE_LOCAL;

    uint32_t h = chorus_scramble(0x9E3779B9u ^ ((line + 1u) * 0x85EBCA6Bu));

    gChorusPhase[line]    = chorus_sign24(h);
    gChorusTrim[line]     = chorus_sign24(chorus_scramble(h));
    gChorusTick[line]     = 0.0;
    gChorusWrite[line][0] = 0;
    gChorusWrite[line][1] = 0;
    memset(gChorusLine[line], 0, sizeof(gChorusLine[line]));
}

// §19.1 - 4-point Lagrange; the sample just written is 0 ago, so the shortest delay is 1.
static double chorus_read(uint32_t line, uint32_t ch, double delay) {
    SE_LOCAL;

    double   whole = fmin(fmax(floor(delay), 1.0), (double)(CHORUS_SAMPLES - 3));
    double   t     = fmin(fmax(delay - whole, 0.0), 1.0);
    uint32_t base  = gChorusWrite[line][ch] + CHORUS_SAMPLES - (uint32_t)whole;

#define CHR(offset)    ((double)gChorusLine[line][ch][(base + 1u - (offset)) % CHORUS_SAMPLES])
    double   ym1   = CHR(0u);
    double   y0    = CHR(1u);
    double   y1    = CHR(2u);
    double   y2    = CHR(3u);
#undef CHR

    return (ym1 * (-t * (t - 1.0) * (t - 2.0) / 6.0)) + (y0 * ((t + 1.0) * (t - 1.0) * (t - 2.0) / 2.0))
           + (y1 * (-(t + 1.0) * t * (t - 2.0) / 2.0)) + (y2 * ((t + 1.0) * t * (t - 1.0) / 6.0));
}

static double chorus_quantise(double samples) {
    double whole = floor(samples);

    return whole + (floor((samples - whole) * CHORUS_FRACTION_STEPS) / CHORUS_FRACTION_STEPS);
}

// §19.1, §19.3 - one channel: two taps either side of the triangle, then the mix.
static double chorus_tap(uint32_t line, uint32_t ch, double input, int32_t phase, double amount) {
    SE_LOCAL;

    double u     = fabs((double)phase / (double)CHORUS_PHASE_HALF);
    double scale = gSampleRate / CHORUS_TAP_RATE_HZ;
    double tap1  = chorus_quantise(CHORUS_TAP1_MAX - (CHORUS_TAP1_SPAN * u)) * scale;
    double tap2  = chorus_quantise(CHORUS_TAP2_MIN + (CHORUS_TAP2_SPAN * u)) * scale;

    gChorusLine[line][ch][gChorusWrite[line][ch]] = (float)input;

    double wet   = chorus_read(line, ch, tap1) + chorus_read(line, ch, tap2);

    gChorusWrite[line][ch]                        = (gChorusWrite[line][ch] + 1u) % CHORUS_SAMPLES;
    return (input * (1.0 - (0.5 * amount))) + (wet * 0.5 * amount);
}

// §19.2 - the right channel reads the LFO a quarter cycle on; the LFO steps at CHORUS_TICK_HZ.
static void chorus_step(uint32_t line, double input, double detune, double amount,
                        double * outLeft, double * outRight) {
    SE_LOCAL;

    int32_t phase = gChorusPhase[line];

    *outLeft           = chorus_tap(line, 0, input, phase, amount);
    *outRight          = chorus_tap(line, 1, input, chorus_sign24((uint32_t)phase + (CHORUS_PHASE_HALF / 2)), amount);

    // The instrument steps it after the taps, on the first sample and every fourth after.

    if (gChorusTick[line] <= 0.0) {
        int32_t step = (int32_t)floor(detune * CHORUS_DETUNE_STEP);

        step              += (int32_t)(((int64_t)step * gChorusTrim[line]) >> 25);     // x (1 + trim/4)
        gChorusTick[line] += 1.0;
        gChorusPhase[line] = chorus_sign24((uint32_t)(phase + step));
    }
    gChorusTick[line] -= CHORUS_TICK_HZ / gSampleRate;
}

// §25.1 - the instrument's own attack and release coefficients, one per dial step; the host interpolates.
static const int32_t kCompAttack[128]  = {
    8388607, 726261, 687218, 650184, 615064, 581770, 550213, 520311, 491982, 465150,
    439740,  415682, 392907, 371350, 350950, 331648, 313386, 296111, 279772, 264320,
    249708,  235892, 222830, 210481, 198809, 187777, 177350, 167496, 158184, 149386,
    141072,  133218, 125797, 118787, 112165, 105909, 100001,  94420,  89149,  84170,
    79469,    75028,  70835,  66875,  63136,  59605,  56271,  53123,  50150,  47343,
    44693,    42191,  39829,  37598,  35493,  33505,  31628,  29856,  28183,  26604,
    25113,    23705,  22377,  21122,  19938,  18820,  17765,  16769,  15829,  14941,
    14103,    13312,  12566,  11861,  11196,  10568,   9975,   9415,   8887,   8389,
    7918,      7474,   7055,   6659,   6285,   5933,   5600,   5286,   4989,   4709,
    4445,      4195,   3960,   3738,   3528,   3330,   3143,   2967,   2800,   2643,
    2495,      2355,   2223,   2098,   1980,   1869,   1764,   1665,   1572,   1484,
    1400,      1322,   1248,   1178,   1111,   1049,    990,    935,    882,    833,
    786,        742,    700,    661,    624,    589,    556, 525
};
static const int32_t kCompRelease[128] = {
    3219, 3109, 3003, 2901, 2802, 2707, 2614, 2525, 2439, 2356,
    2276, 2199, 2124, 2051, 1981, 1914, 1849, 1786, 1725, 1666,
    1609, 1555, 1502, 1451, 1401, 1353, 1307, 1263, 1220, 1178,
    1138, 1099, 1062, 1026,  991,  957,  924,  893,  863,  833,
    805,   777,  751,  725,  701,  677,  654,  631,  610,  589,
    569,   550,  531,  513,  495,  479,  462,  446,  431,  417,
    402,   389,  375,  363,  350,  338,  327,  316,  305,  295,
    285,   275,  265,  256,  248,  239,  231,  223,  216,  208,
    201,   194,  188,  181,  175,  169,  163,  158,  152,  147,
    142,   137,  133,  128,  124,  120,  116,  112,  108,  104,
    101,    97,   94,   91,   88,   85,   82,   79,   76,   74,
    71,     69,   66,   64,   62,   60,   58,   56,   54,   52,
    50,     49,   47,   45,   44,   42,   41, 39
};

static int32_t       kCompGain[128]; // §25.2 - 2^(-k/4) and the step to the next, built by comp_words()

static int32_t comp_table_word(const int32_t * table, double dial) {
    int32_t v16 = (int32_t)floor(dial * 256.0);
    int32_t i   = v16 >> 8;
    int32_t f   = v16 & 0xff;

    if (i >= 127) {
        return table[127];
    }
    return table[i] + (((table[i + 1] - table[i]) * f) >> 8);
}

// §25.1 - the host's words for Threshold, Ratio and Level, the make-up gain between them, and the
// attack and release coefficients.
static void comp_words(tEngineNode * node, double thrDial, double ratioDial, double atkDial, double relDial, double lvlDial) {
    int32_t ratio   = (int32_t)lround(ratioDial);
    bool    tenfold = (ratio > 34);
    int32_t step    = tenfold ? (ratio - 35) : ratio;
    int32_t r10     = (step < 10) ? (step + 10) : ((step < 25) ? (((step - 10) * 2) + 20) : (((step - 25) * 5) + 50));
    int32_t thr10   = ((int32_t)lround(thrDial) - 30) * 10;
    int32_t lvl10   = ((int32_t)lround(lvlDial) - 30) * 10;
    int32_t makeup  = 0;

    if (tenfold) {
        r10 *= 10;
    }

    if (thr10 < lvl10) {
        int32_t up = lvl10 - thr10;

        makeup = up - ((up * 10) / r10);
        makeup = (makeup > 420) ? 420 : makeup;
    }
    int32_t down    = 420 - makeup;
    int32_t pow60   = (int32_t)lround(1073741824.0 * exp2(-(double)(down % 60) / 60.0)) >> (down / 60);

    pow60              = (pow60 == 0x40000000) ? 0x3fffffff : pow60;

    node->threshold    = (double)fmin(0x7fffff, (double)(((int64_t)(thr10 + 840) * 0x80000) / 60));
    node->ratio        = (double)(0x800000 - (0x5000000 / r10));
    node->refLevel     = (double)fmin(0x7fffff, (double)(((int64_t)(1990 - makeup) * 0x80000) / 60));
    node->compMakeup   = (double)(pow60 >> 7);
    node->attackCoeff  = (double)comp_table_word(kCompAttack, atkDial);
    node->releaseCoeff = (double)comp_table_word(kCompRelease, relDial);

    for (int32_t k = 0; k < 64; k++) {
        int32_t v    = (int32_t)fmin(0x7fffff, lround(8388608.0 * exp2(-(double)k / 4.0)));
        int32_t next = (k < 63) ? (int32_t)fmin(0x7fffff, lround(8388608.0 * exp2(-(double)(k + 1) / 4.0))) : 0;

        kCompGain[2 * k]       = v;
        kCompGain[(2 * k) + 1] = v - next;
    }
}

// §25.2 - one sample, in the instrument's integer arithmetic: an instant peak with an exponential
// release, a piecewise-linear log2, the ratio's gain reduction and the Level limiter each smoothed,
// and the larger of the two back through the gain table, then the make-up gain.
static double compress_step(uint32_t voice, uint32_t node, double input, const tEngineNode * spec) {
    SE_LOCAL;

    int32_t  in     = dly_sat((int64_t)floor(input * 2097152.0));
    int64_t  det    = (in < 0) ? -(int64_t)in : (int64_t)in;
    int32_t  env    = (int32_t)gCompEnv[voice][node];
    int32_t  atk    = (int32_t)spec->attackCoeff;
    int32_t  rls    = (int32_t)spec->releaseCoeff;
    int64_t  diff   = det - env;
    int64_t  acc    = (int64_t)env << 32;

    if (diff > 0) {
        acc = (int64_t)(env + dly_sat(diff)) << 32;
    } else if (diff < 0) {
        acc += ((int64_t)rls * dly_sat(diff)) * 512;
    }
    gCompEnv[voice][node] = dly_sat(acc >> 32);

    if ((acc >> 32) < 0x80) {
        acc = (int64_t)0x80 << 32;                           // the detector's floor
    }
    int32_t  hi     = (int32_t)(acc >> 32);
    int32_t  e      = 31 - __builtin_clz((uint32_t)hi);
    uint32_t m      = (uint32_t)((uint64_t)acc >> e);
    int32_t  logHi  = ((e - 7) << 19) | (int32_t)(m >> 13);
    uint32_t logSub = (m & 0x1f00u) << 19;
    int32_t  over   = logHi - (int32_t)spec->threshold;
    int32_t  lvlHi  = logHi - (int32_t)spec->refLevel;
    uint32_t lvlSub = logSub;

    over                  = (over < 0) ? 0 : over;

    if ((lvlHi < 0) || ((lvlHi == 0) && (lvlSub == 0u))) {
        lvlHi  = 0;
        lvlSub = 0u;
    }
    int32_t  lim    = (int32_t)gCompLim[voice][node];     // the Level limiter: instant rise, release rate
    int32_t  dl     = lvlHi - lim;
    int64_t  limAcc = (int64_t)lim << 32;

    if ((dl > 0) || ((dl == 0) && (lvlSub != 0u))) {
        limAcc = (int64_t)(lim + dly_sat(dl)) << 32;
    } else if (dl < 0) {
        limAcc += ((int64_t)rls * dly_sat(dl)) * 512;
    }
    gCompLim[voice][node] = dly_sat(limAcc >> 32);

    int64_t  prod   = (int64_t)dly_sat(over) * (int32_t)spec->ratio; // the ratio's reduction: attack up, release down
    int32_t  gr     = (int32_t)gCompGr[voice][node];
    int32_t  dg     = (int32_t)(prod >> 23) - gr;
    int64_t  grAcc  = (int64_t)gr << 32;

    if ((dg > 0) || ((dg == 0) && ((prod & 0x7fffff) != 0))) {
        grAcc += ((int64_t)atk * dly_sat(dg)) * 512;
    }

    if (dg < 0) {
        grAcc += ((int64_t)rls * dly_sat(dg)) * 512;
    }
    gCompGr[voice][node]  = dly_sat(grAcc >> 32);

    int64_t  total  = (limAcc < grAcc) ? grAcc : limAcc;
    int32_t  tHi    = (int32_t)(total >> 32);
    int32_t  k      = (tHi >> 17) > 63 ? 63 : (tHi >> 17);
    int32_t  frac   = (int32_t)((((uint32_t)tHi & 0x1ffffu) << 6) | ((uint32_t)total >> 26));
    int32_t  gain   = dly_sat((int64_t)kCompGain[2 * k] + ((-(int64_t)dly_sat(frac) * kCompGain[(2 * k) + 1]) >> 23));
    int32_t  level  = dly_sat(((int64_t)gain * (int32_t)spec->compMakeup) >> 23);
    int32_t  out    = dly_sat(((int64_t)level * in) >> 16);

    // notes §122 - the meter shows the gain reduction
    {
        double   reductionDb = ((double)tHi / 524288.0) * 6.0206;
        uint32_t lit         = (reductionDb > 0.0) ? compress_meter_lit(reductionDb) : 0u;   // paramCurves.c
        uint32_t packed      = METER_WRITTEN | (((lit == 0u) ? 0u : ((1u << lit) - 1u)) & METER_VALUE_MASK);

        if (atomic_exchange_explicit(&gModuleMeter[spec->location][spec->moduleIndex],
                                     packed, memory_order_relaxed) != packed) {
            atomic_store_explicit(&gMetersDirty, true, memory_order_relaxed);
        }
    }
    return (double)out / 2097152.0;
}

// §20.6 - a stored word: rounded down to the 24-bit grid, and clamped to the word's range.
static double rv_word(double v) {
    return fmin(RV_TOP, fmax(-4.0, floor(v * RV_WORD) / RV_WORD));
}

// §20 - one sample of the instrument's reverb network: every read first, then every write.
static void reverb_step(double inLeft, double inRight, const tEngineNode * spec, double * outLeft, double * outRight) {
    SE_LOCAL;

    inLeft   = rv_word(inLeft);
    inRight  = rv_word(inRight);
    double          input                = rv_word((inLeft + inRight) / 2.0); // §20.1 - the network hears the two averaged

    if (spec->reverbType != sLastTypeBank[SE]) {
        memset(gRvRing, 0, sizeof(gRvRing));
        gRvCur            = 0;
        gRvPhase          = 0.0;
        sLastTypeBank[SE] = spec->reverbType;
    }
    const int32_t * p                    = spec->rvPos;
    const double *  y                    = spec->rvY;
    double          rate                 = gSampleRate / RV_BASE_RATE;
    uint32_t        cur                  = gRvCur;

#define RVR(o)       ((double)gRvRing[(cur + (uint32_t)(o)) & (RV_RING - 1u)])
#define RVW(o, v)    (gRvRing[(cur + (uint32_t)(o)) & (RV_RING - 1u)] = (float)rv_word(v))

    // §20.4 - the triangle both modulated taps follow
    double          sum                  = gRvPhase + (RV_LFO_STEP / rate);

    gRvPhase = (sum >= RV_LFO_HALF) ? (sum - (2.0 * RV_LFO_HALF)) : sum;
    // §20.4 - the triangle is the unwrapped sum's size, held at full scale on the sample it wraps
    double          span                 = RV_LFO_DEPTH * rate * fmin(fabs(sum), RV_LFO_HALF - 1.0) / RV_LFO_HALF;
    double          modA, modB;

    {
        double  at    = (double)p[eRvModA] - span;
        double  whole = floor(at);
        double  f     = at - whole;
        int32_t i     = (int32_t)whole;

        modA  = ((1.0 - f) * RVR(i)) + (f * RVR(i + 1));
        at    = (double)p[eRvModB] - span;
        whole = floor(at);
        f     = at - whole;
        i     = (int32_t)whole;
        modB  = ((1.0 - f) * RVR(i)) + (f * RVR(i + 1));
    }

    double          g = y[7], h = y[8], k = y[9];

    // §20.1 - the input: four allpasses
    double          x0                   = RVR(p[eRvPre]);
    double          a1                   = rv_word(RVR(p[eRvAp1Out]) - (g * x0));
    double          a2                   = rv_word(RVR(p[eRvAp2Out]) + (g * a1));
    double          a3                   = rv_word(RVR(p[eRvAp3Out]) - (g * a2));
    double          a4                   = rv_word(RVR(p[eRvAp4Out]) + (h * a3));

    // §20.1 - the tank's two halves, each feeding the other
    double          tankIn               = RVR(p[eRvTankInA]);
    double          xa = RVR(p[eRvApAIn]), da = RVR(p[eRvApAOut]);
    double          xa2 = RVR(p[eRvApA2In]), da2 = RVR(p[eRvApA2Out]);
    double          la = RVR(p[eRvDampA]), la1 = RVR(p[eRvDampA] + 1);
    double          fbA                  = RVR(p[eRvTankOutB]);
    double          inB                  = RVR(p[eRvTankInB]);
    double          xb = RVR(p[eRvApBIn]), db = RVR(p[eRvApBOut]);
    double          xb2 = RVR(p[eRvApB2In]), db2 = RVR(p[eRvApB2Out]);
    double          lb = RVR(p[eRvDampB]), lb1 = RVR(p[eRvDampB] + 1);
    double          inLp                 = RVR(0);
    double          wet[REVERB_CHANNELS] = {0.0, 0.0};

    for (uint32_t ch = 0; ch < REVERB_CHANNELS; ch++) {
        for (uint32_t t = 0; t < 7; t++) {
            int    sign = kRvTapSign[ch][t];
            double gain = (sign == 2) ? y[2] : ((double)sign * y[3]);

            wet[ch] += gain * RVR(p[((ch == 0) ? eRvTapL : eRvTapR) + t]);
        }

        wet[ch] = rv_word(wet[ch]);    // §20.6 - the seven taps summed whole, then rounded once
    }

    RVW(p[eRvPre], x0 + (g * a1));
    RVW(p[eRvAp2In], a1 - (g * a2));
    RVW(p[eRvAp2Out], a2 + (g * a3));
    RVW(p[eRvAp4In], a3 - (h * a4));
    RVW(p[eRvAp4Out], a4);

    RVW(p[eRvTankInA], tankIn + (y[4] * fbA));
    double          ya                   = rv_word(da - (h * xa)); // §20.6 - an allpass's output is a word before it is reused

    RVW(p[eRvApAIn], xa + (h * ya));
    RVW(p[eRvApAOut], ya);
    RVW(p[eRvModA], modA);
    double          ya2                  = rv_word(da2 + (k * xa2));

    RVW(p[eRvApA2In], xa2 - (k * ya2));
    RVW(p[eRvApA2Out], ya2);
    RVW(p[eRvDampA], (y[5] * la) + (y[6] * la1));

    RVW(p[eRvTankInB], tankIn + (y[4] * inB));
    double          yb                   = rv_word(db - (h * xb));

    RVW(p[eRvApBIn], xb + (h * yb));
    RVW(p[eRvApBOut], yb);
    RVW(p[eRvModB], modB);
    double          yb2                  = rv_word(db2 + (k * xb2));

    RVW(p[eRvApB2In], xb2 - (k * yb2));
    RVW(p[eRvApB2Out], yb2);
    RVW(p[eRvDampB], (y[5] * lb) + (y[6] * lb1));

    RVW(-1, (y[1] * inLp) + (y[0] * input));    // §20.1 - Brightness's input filter
#undef RVR
#undef RVW
    gRvCur    = (cur - 1u) & (RV_RING - 1u);

    *outLeft  = rv_word((spec->rvDry * inLeft) + (spec->rvWet * wet[0]));
    *outRight = rv_word((spec->rvDry * inRight) + (spec->rvWet * wet[1]));
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
    set_oversampling(deviceRate);

    tEngineNode node;

    memset(&node, 0, sizeof(node));
    node.reverbType   = type;
    reverb_build(&node, type, (double)timeValue, (double)brightValue, 127.0);   // fully wet
    memset(gRvRing, 0, sizeof(gRvRing));
    gRvCur            = 0;
    gRvPhase          = 0.0;
    sLastTypeBank[SE] = type;

    for (uint32_t i = 0; i < frames; i++) {
        double wetL = 0.0;
        double wetR = 0.0;

        reverb_step((i == 0) ? 1.0 : 0.0, (i == 0) ? 1.0 : 0.0, &node, &wetL, &wetR);
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
    set_oversampling(deviceRate);

    // A second render in one process would otherwise start with the previous one's line and LFO
    // phase - the same trap the reverb IR clears for.
    for (uint32_t i = 0; i < MAX_CHORUS_LINES; i++) {
        chorus_reset(i);
    }

    double depth  = (double)detuneValue;
    double amount = dial_fraction((double)amountValue);

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

static double flt_clip4(double v) {
    return fmin(4.0, fmax(-4.0, v));    // the instrument's word: four times full scale
}

// §22.1 - FltLP's and FltHP's coefficient: the dial's sin(pi f/fs), times what the modulation adds.
static double flt_stage_half(double dial, double shiftSemitones) {
    SE_LOCAL;

    return fmin(1.0, sin((M_PI * flt_cutoff_hz(dial)) / gSampleRate) * exp2(shiftSemitones / 12.0));
}

// §22.2 - FltLP: identical one-poles, y += 2h (x - y), the coefficient held below one.
static double flt_lp_stages(double * state, double input, double half, uint32_t poles) {
    double g = fmin(1.0, 2.0 * half);
    double x = input;

    for (uint32_t i = 0; i < poles; i++) {
        state[i] = flt_clip4(state[i] + (g * (x - state[i])));
        x        = state[i];
    }

    return x;
}

// §22.3 - FltHP: identical one-poles y = p y' + d (x - x'), p = 1 - 2h, d = 1 - h: unity at Nyquist.
static double flt_hp_stages(double * state, double input, double half, uint32_t poles) {
    double p = fmax(-1.0, 1.0 - (2.0 * half));
    double d = 1.0 - half;
    double x = input;

    for (uint32_t i = 0; i < poles; i++) {
        double y = flt_clip4(state[i] + (d * x));

        state[i] = flt_clip4((p * y) - (d * x));
        x        = y;
    }

    return x;
}

#define FLTNORD_H_MAX        (0x518368 / 8388608.0)    // §23.2 - h at most: 20.8 kHz
#define FLTNORD_RES_SCALE    (0x7eb852 / 8388608.0)    // 0.99; band-reject takes half
#define FLTNORD_LEAK         (0.9)                     // §23.3 - HP and BR take back 0.9 of their last output

// §23.1 - one stage: FltMulti's Chamberlin on the mean of two input samples, with the instrument's taps.
static double nord_stage(double * s, double input, double h, double q, tFilterShape shape) {
    double F       = 2.0 * h;
    double x       = 0.5 * (input + s[2]);
    double lowOld  = s[0];
    double bandOld = s[1];
    double low     = fmin(8.0, fmax(-8.0, lowOld + (F * bandOld)));
    double high    = x - low - (q * bandOld);
    double band    = flt_clip4(bandOld + (F * high));
    double y       = 0.0;

    switch (shape) {
        case eFilterShapeBandPass:   y = (1.0 - h) * band;
            break;
        case eFilterShapeHighPass:   y = (2.0 * (1.0 - h) * high) - (FLTNORD_LEAK * s[3]);
            break;
        case eFilterShapeBandReject: y = (2.0 * (x - (q * bandOld))) - (FLTNORD_LEAK * s[3]);
            break;
        default:                     y = 0.5 * (low + lowOld);
            break;
    }
    s[0] = low;
    s[1] = band;
    s[2] = input;
    s[3] = y;
    return flt_clip4(y);
}

// §23 - FltNord: one stage, or two of the same type for 24 dB (band-reject stays one).
static double nord_filter(double * state, double input, double half, double resDial, tFilterShape shape, bool slope24, bool gainComp) {
    double h  = fmin(half, FLTNORD_H_MAX);
    double r  = (resDial >= 127.0) ? 1.0 : (resDial / 128.0);
    double d  = 1.0 - (((shape == eFilterShapeBandReject) ? 0.5 : FLTNORD_RES_SCALE) * r);
    double qb = slope24 ? fmax(d * d, M_SQRT1_2 * d) : (d * d);
    double q  = 2.0 * qb * (1.0 - h);
    double y  = nord_stage(&state[0], flt_clip4(input * (gainComp ? d : 1.0)), h, q, shape);

    if (slope24 && (shape != eFilterShapeBandReject)) {
        y = nord_stage(&state[4], y, h, q, shape);
    }
    return y;
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

#define FLTCLASSIC_Q23           (8388608.0)                         // §21.4 - one is a quarter of the engine's range
#define FLTCLASSIC_A_MAX         (0x5851ec / FLTCLASSIC_Q23)         // §21.2 - pi f/fs at most: 21.1 kHz
#define FLTCLASSIC_DRIVE         (0x320000 / FLTCLASSIC_Q23)         // §21.1 - 25/64
#define FLTCLASSIC_CUBIC         (0x10aaaa / FLTCLASSIC_Q23)         // a third of it
#define FLTCLASSIC_ZERO_BASE     (0x3c28f6 / FLTCLASSIC_Q23)         // §21.2 - 0.47
#define FLTCLASSIC_ZERO_SLOPE    (0x10a3d7 / FLTCLASSIC_Q23)         // 0.13

// §21.4 - a stored value: rounded down to 23 bits and held inside the word.
static double fltclassic_q(double v) {
    return fmin(1.0 - (1.0 / FLTCLASSIC_Q23), fmax(-1.0, floor(v * FLTCLASSIC_Q23) / FLTCLASSIC_Q23));
}

// §21.1 - a clipped cubic into four one-pole stages, the middle two with a zero; the resonance
// comes from the fourth whatever the slope taps. §21.4 - in the instrument's arithmetic: one running
// sum, each stored value rounded down, so a silent filter stays silent until something rings it.
static double classic_filter(double * state, double input, double cutoff, double resDial, uint32_t tapStage) {
    SE_LOCAL;

    double a   = fmin((M_PI * cutoff) / gSampleRate, FLTCLASSIC_A_MAX);
    double p   = fltclassic_q(fmax(0.0, 1.0 - (2.0 * a) + (2.0 * a * a) - ((4.0 / 3.0) * a * a * a)));
    double z   = FLTCLASSIC_ZERO_BASE + fltclassic_q(FLTCLASSIC_ZERO_SLOPE * fmin(1.0 - (1.0 / FLTCLASSIC_Q23), 2.0 * p));
    double k8  = 8.0 * (floor(resDial * 256.0) * 137.0) / FLTCLASSIC_Q23;
    double fb  = ceil(k8 * state[3] * FLTCLASSIC_Q23) / FLTCLASSIC_Q23;
    double x   = fltclassic_q(fltclassic_q(input / 4.0) - fb);
    double x2  = fltclassic_q(x * x);
    double x3  = fltclassic_q(x2 * x);
    double acc = (FLTCLASSIC_DRIVE * x) - (FLTCLASSIC_CUBIC * x3);
    double s1  = state[0];
    double s2  = state[1];

    acc     += p * (s1 - fltclassic_q(acc));
    state[0] = fltclassic_q(acc);
    acc     += z * s1;
    double t1  = fltclassic_q(acc);

    acc     += p * (s2 - t1);
    state[1] = fltclassic_q(acc);
    acc     += z * s2;
    double t2  = fltclassic_q(acc);

    acc     += p * (state[2] - t2);
    state[2] = fltclassic_q(acc);
    acc     += p * (state[3] - state[2]);
    state[3] = fltclassic_q(acc);
    return 4.0 * ((tapStage >= 3u) ? state[3] : ((tapStage == 2u) ? state[2] : t2));
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
// §14.4 - the DX7's keyboard level scaling, as an offset in dB around the break point.
static double dx_kbscale_db(const tDxOperator * op, double note) {
    bool     left     = (note < op->bpNote);
    uint32_t curve    = left ? op->lCurve : op->rCurve;    // -Lin, -Exp, +Exp, +Lin
    double   depth    = left ? op->lDepth : op->rDepth;
    double   distance = fmin(fabs(note - op->bpNote) / DX_KBSCALE_SPAN_NOTES, 1.0);
    double   shape    = ((curve == 1u) || (curve == 2u)) ? ((exp(4.0 * distance) - 1.0) / (exp(4.0) - 1.0)) : distance;

    return ((curve < 2u) ? -1.0 : 1.0) * depth * shape * DX_KBSCALE_FULL_DB;
}

// §14.2 - the rate/level envelope, in dB: each stage moves at its rate towards its level.
static double dx_envelope_db(uint32_t voice, uint32_t slot, const tDxOperator * op, double note) {
    SE_LOCAL;

    uint32_t stage = gDxEnvStage[voice][slot];
    double   level = gDxEnvDb[voice][slot];

    if ((stage <= (uint32_t)eDxRise3) || (stage == (uint32_t)eDxRelease)) {
        uint32_t s      = (stage == (uint32_t)eDxRelease) ? 3u : stage;
        double   target = op->levelDb[s];
        double   step   = (op->rateDbPerSecond[s] * exp2((op->rateScale * (note - 60.0)) / 24.0)) / gSampleRate;

        if (fabs(target - level) <= step) {
            level = target;
            stage = (stage == (uint32_t)eDxRelease) ? (uint32_t)eDxIdle
                    : ((stage == (uint32_t)eDxRise3) ? (uint32_t)eDxHold : (stage + 1u));
        } else {
            level += (target > level) ? step : -step;
        }
    } else if (stage == (uint32_t)eDxHold) {
        level = op->levelDb[2];
    }
    gDxEnvDb[voice][slot]    = level;
    gDxEnvStage[voice][slot] = stage;
    return level;
}

// §14 - one sample of a DXRouter and its Operators, for one voice.
static double dx_step(uint32_t voice, uint32_t node, const tEngineNode * spec, const tDxOperator * ops, double voicePitch) {
    SE_LOCAL;

    const tDxAlgorithm * alg               = dx_algorithm(spec->dxAlgorithm);
    double               out[DX_OPERATORS] = {0.0};
    double               mix               = 0.0;
    uint32_t             carriers          = 0;
    bool                 gate              = gVoice[voice].gate;
    bool                 strike            = gate && ((gDxGate[voice][node] == false) || (gDxTrigger[voice][node] != gVoice[voice].trigger));
    bool                 letGo             = (gate == false) && gDxGate[voice][node];
    double               note              = (voicePitch >= 0.0) ? voicePitch : 64.0;
    double               noteHz            = 440.0 * exp2((note - MIDI_NOTE_A440) / 12.0);

    gDxGate[voice][node]    = gate;
    gDxTrigger[voice][node] = gVoice[voice].trigger;

    // Modulators before what they modulate: every DX7 modulation runs from a higher operator to a lower one.
    for (int32_t k = DX_OPERATORS - 1; k >= 0; k--) {
        // §26.2 - the parameters come from the voice's own set; the state arrays stay keyed on dxBase
        uint32_t            slot = spec->dxBase + (uint32_t)k;
        const tDxOperator * op   = &ops[k];
        double              fm   = 0.0;
        double              hz   = 0.0;
        double              env  = 0.0;
        double              y    = 0.0;

        if (op->present == false) {
            continue;
        }

        if (strike == true) {
            gDxEnvStage[voice][slot] = eDxRise1;

            if (op->sync == true) {
                gDxPhase[voice][slot] = 0.0;
            }
        } else if ((letGo == true) && (gDxEnvStage[voice][slot] < (uint32_t)eDxRelease)) {
            gDxEnvStage[voice][slot] = eDxRelease;
        }

        for (uint32_t m = (uint32_t)k + 1u; m < DX_OPERATORS; m++) {
            if ((alg->target[m] & (1u << (uint32_t)k)) != 0) {
                fm += out[m];
            }
        }

        if ((uint32_t)k == (alg->feedbackTo - 1u)) {    // §14.3 - the last two samples, averaged
            uint32_t from = spec->dxBase + alg->feedbackFrom - 1u;

            fm += spec->dxFeedback * 0.5 * (gDxOut[voice][from][0] + gDxOut[voice][from][1]);
        }
        hz                     = op->fixed ? op->fixedHz : ((op->kbt ? noteHz : DX_E4_HZ) * op->ratio);
        gDxPhase[voice][slot] += (hz * op->detune) / gSampleRate;
        gDxPhase[voice][slot] -= floor(gDxPhase[voice][slot]);
        env                    = dx_envelope_db(voice, slot, op, note);

        if ((op->active == true) && (env > DX_SILENT_DB)) {
            y = sin(2.0 * M_PI * (gDxPhase[voice][slot] + (DX_FM_CYCLES_PER_UNIT * fm)))
                * op->outputGain * exp2((env + dx_kbscale_db(op, note)) / 6.0206);
        }
        gDxOut[voice][slot][1] = gDxOut[voice][slot][0];
        gDxOut[voice][slot][0] = y;
        out[k]                 = y;
    }

    for (uint32_t k = 0; k < DX_OPERATORS; k++) {
        if ((alg->target[k] == 0) && (ops[k].present == true)) {
            mix += out[k];
            carriers++;
        }
    }

    return (carriers > 0u) ? (mix / (double)carriers) : 0.0;   // §14.5
}

// True while any of the router's Operators is still moving or holds a level above silence.
static bool dx_voice_sounding(const tSoundEngineParams * params, const tEngineNode * spec, uint32_t voice) {
    SE_LOCAL;

    for (uint32_t k = 0; k < DX_OPERATORS; k++) {
        uint32_t slot = spec->dxBase + k;

        if (  (params->dxOp[slot].present == true)
           && ((gDxEnvStage[voice][slot] != (uint32_t)eDxIdle) || (gDxEnvDb[voice][slot] > DX_SILENT_DB))) {
            return true;
        }
    }

    return false;
}

// A stage at or past the held one: a gate rising there restarts the envelope rather than continuing.
static bool env_stage_is_release(const tEngineNode * spec, uint32_t index) {
    return (spec->envSustainStage >= 0) && (index > (uint32_t)spec->envSustainStage);
}

// §17.3 - one tick of a segment in the instrument's integer arithmetic: the doubled product rounded
// down, never below the target, and not yet clamped, so the attack can see itself pass full scale.
static int32_t env_segment(int32_t level, int32_t half, int32_t add, int32_t target) {
    int64_t step = (int64_t)add + ((2 * (int64_t)half * ((int64_t)level - (int64_t)target)) >> 23);

    return (int32_t)(((step < 0) ? 0 : step) + target);
}

static double signal_in(const tEngineNode * spec, double value[][NODE_OUTPUTS], uint32_t input);

// §17.10 - a stage whose time dial is being modulated, re-read at the dial the mod puts it at. A
// control signal of 1.0 is PITCH_MOD_SEMITONES units, and at a mod amount of 64 one unit moves the
// dial one step, so the whole amount dial scales linearly from there. Rebuilt on the envelope's own
// tick rather than per sample, and only while a mod jack is actually patched.
static const tEnvSegment * env_stage_modulated(const tEngineNode * spec, const tEnvSegment * stage,
                                               double value[][NODE_OUTPUTS], tEnvSegment * scratch) {
    if ((stage->modLeg < 0) || (spec->in[stage->modLeg] < 0)) {
        return stage;
    }
    double units   = signal_in(spec, value, (uint32_t)stage->modLeg) * PITCH_MOD_SEMITONES;
    double dial    = (double)stage->dial + (units * (double)stage->modAmount / 64.0);

    dial     = fmin(127.0, fmax(0.0, dial));

    if (fabs(dial - (double)stage->dial) < 0.5) {
        return stage;                          // the mod is not moving it off its own dial
    }
    *scratch = *stage;

    double seconds = (stage->rising != 0u)
                     ? env_attack_seconds(dial, (uint32_t)spec->wave)
                     : env_time_seconds(dial);

    env_stage_rates(scratch, seconds, (uint32_t)spec->wave, stage->rising != 0u);
    return scratch;
}

static double envelope_step(uint32_t voice, uint32_t node, const tEngineNode * spec, bool gate,
                            double value[][NODE_OUTPUTS]) {
    SE_LOCAL;

    // §17.3 - the segments step at the envelope tick, and the level holds between ticks.
    if (gEnvTick[voice][node] <= 0.0) {
        int32_t  q     = gEnvQ[voice][node];

        gEnvTick[voice][node] += 1.0;

        // §17.9 - walk the stage list. The ADSR case is the four stages it always was and behaves
        // exactly as before: the decay runs toward the sustain target and simply never finishes while
        // the gate is up, which is what "do not advance into a sustain" says here.
        uint32_t index = gEnvStage[voice][node];

        if (index >= spec->envStageCount) {
            q = 0;                                  // idle
        } else {
            tEnvSegment         scratch;
            const tEnvSegment * stage = env_stage_modulated(spec, &spec->envStage[index], value, &scratch);

            if (stage->sustain != 0u) {
                q = env_segment(q, stage->half, stage->add, stage->target);
            } else {
                q = env_segment(q, stage->half, stage->add, (stage->rising != 0u) ? 0 : stage->target);

                bool     done = (stage->rising != 0u) ? (q > stage->target) : (q <= stage->target);
                uint32_t next = index + 1u;
                bool     hold = (next < spec->envStageCount) && (spec->envStage[next].sustain != 0u)
                                && (gate == true);

                if ((done == true) && (hold == false)) {
                    q                      = stage->target;
                    gEnvStage[voice][node] = next;

                    if (next >= spec->envStageCount) {
                        gEnvStage[voice][node] = ENV_STAGE_IDLE;
                    }
                }
            }
        }
        gEnvQ[voice][node]     = (q > ENV_TOP) ? ENV_TOP : q;
        gEnvLevel[voice][node] = (double)gEnvQ[voice][node] / ENV_FULL_SCALE_STEPS;

        // §17.3 - the gate is read at the tick and takes effect from the next one, as on the instrument.
        if (gate == true) {
            // notes §150
            if (  (gEnvStage[voice][node] == ENV_STAGE_IDLE)
               || (env_stage_is_release(spec, gEnvStage[voice][node]) == true)
               || ((spec->envKeyGate == true) && (gEnvTrigger[voice][node] != gVoice[voice].trigger))) {
                gEnvStage[voice][node]   = 0u;           // from the level it is at - §17.3
                gEnvTrigger[voice][node] = gVoice[voice].trigger;

                // §17.7 - Reset starts it from zero, in the tick the gate rises
                if (spec->envReset == true) {
                    gEnvQ[voice][node]     = 0;
                    gEnvLevel[voice][node] = 0.0;
                }
            }
        } else if (gEnvStage[voice][node] != ENV_STAGE_IDLE) {
            // §17.9 - the gate falling jumps PAST the held stage. With no held stage there is nothing
            // to jump past and a one-shot runs on to its end, which is what EnvAHD and EnvD want.
            if (spec->envSustainStage >= 0) {
                gEnvStage[voice][node] = (uint32_t)spec->envSustainStage + 1u;
            }
        }
    }
    gEnvTick[voice][node] -= ENV_TICK_HZ / gSampleRate;
    return gEnvLevel[voice][node];
}

// The signal arriving at one of a node's inputs: whichever output of whichever node feeds it.
static double signal_in(const tEngineNode * spec, double value[][NODE_OUTPUTS], uint32_t input) {
    int32_t source = spec->in[input];

    if ((input >= spec->inCount) || (source < 0)) {
        return 0.0;
    }
    return value[source][spec->srcLeg[input]];
}

// §6.3 - rises through 0 at phase 0, steps down at phase 0.5
static double osc_rising_saw(double phase, double edge) {
    return -osc_saw(fmod(phase + 0.5, 1.0), edge);
}

// notes §151
#define OSCDUAL_SOFT_GAIN      (2.0)   // §12.3
#define OSCDUAL_SOFT_POLE      (8.0)   // times inc96
#define OSCDUAL_PW_DEPTH       (4.0)   // §12.2 - the inputs' reach, in the dials' own terms
#define OSCDUAL_PHASE_DEPTH    (2.0)

// §12.3 - the octave below: low for the first half of its cycle. state: flip-flop, last phase, soft low-pass.
static double oscdual_sub(double * state, double phase, double inc96, bool soft) {
    double subPhase = 0.5 * (phase + state[0]);
    double square   = -osc_square(subPhase, fmin(0.5 * OSC_EDGE_SAMPLES * inc96, 0.5), 0.5);

    if (!soft) {
        return square;
    }
    state[4] += fmin(OSCDUAL_SOFT_POLE * inc96, 1.0) * (square - state[4]);
    return OSCDUAL_SOFT_GAIN * state[4];
}

// §12.2 - pulsePosition is the PW dial with its input added; state[5] is where the saw sits
static double oscdual_wave(uint32_t voice, uint32_t node, const tEngineNode * spec, double phase, double inc96, double pulsePosition) {
    SE_LOCAL;

    double * state = gLadder[voice][node];
    double   edge  = fmin(OSC_EDGE_SAMPLES * inc96, 0.5);
    double   low   = 0.5 * (1.0 - pulsePosition);
    double   out   = 0.0;

    if (phase < state[1]) {
        state[0] = 1.0 - state[0];
    }
    state[1] = phase;
    low     -= floor(low);

    if (spec->dualSquareLevel > 0.0) {
        out -= spec->dualSquareLevel * (osc_square(phase, edge, low) - ((2.0 * low) - 1.0));
    }

    if (spec->dualSawLevel > 0.0) {
        out += spec->dualSawLevel * osc_rising_saw(fmod(phase + state[5], 1.0), edge);
    }

    if ((spec->dualSubLevel > 0.0) || spec->dualSoft) {
        out += spec->dualSubLevel * oscdual_sub(state, phase, inc96, spec->dualSoft);
    }
    return out;
}

// §6.3 - a corner's correction; distance in cycles from it, inc96 the phase step per 96 kHz sample
static double osc_corner(double distance, double inc96, double squareLimit) {
    double v = OSC_EDGE_SAMPLES - (fabs(distance) / inc96);

    if (v <= 0.0) {
        return 0.0;
    }
    return (2.0 * inc96) * v * fmin(v * v, squareLimit) / 6.0;
}

// §6.3 - high from half the offset to half a cycle, and DC-free
static double osc_offset_pulse(double phase, double edge, double offset) {
    return osc_square(fmod(phase + 1.0 - (0.5 * offset), 1.0), edge, 0.5 * (1.0 - offset)) + offset;
}

// dt is the phase step per call, inc96 the step per 96 kHz sample (§6.3)
static double osc_waveform(uint32_t voice, uint32_t node, const tEngineNode * spec, double phase, double dt,
                           double inc96, double shape) {
    double edge = fmin(OSC_EDGE_SAMPLES * inc96, 0.5);

    // The shape oscillators have their own eight waveforms, and Shape morphs each of them rather
    // than acting as a pulse width, so they do not share the switch below.
    if (spec->kind == eNodeOscShp) {
        return osc_shp_wave((uint32_t)spec->wave, phase, inc96, shape);
    }

    switch (spec->wave) {
        case eOscWaveSine:
        {
            return wave_sine_polynomial(osc_triangle(phase, 0.5));
        }
        case eOscWaveTriangle:
        {
            // notes §152
            double trough = (phase < 0.5) ? phase : (phase - 1.0);

            return osc_triangle(phase, 0.5) + osc_corner(trough, inc96, spec->oscCornerLimit)
                   - osc_corner(phase - 0.5, inc96, spec->oscCornerLimit);
        }
        case eOscWaveSaw:
        {
            return osc_rising_saw(phase, edge);
        }
        case eOscWaveSquare:
        {
            return osc_offset_pulse(phase, edge, fmin(fmax(shape, 0.0), 1.0));
        }
        case eOscWaveDualSaw:
        {
            double offset = 0.5 * fmin(fmax(shape, 0.0), 1.0);

            return osc_rising_saw(phase, edge) + osc_rising_saw(fmod(phase + offset, 1.0), edge);
        }
        case eOscWaveDual:
        {
            return oscdual_wave(voice, node, spec, phase, inc96, shape);
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
    double   inc96     = frequency / OSC_INSTRUMENT_RATE;

    // §6.3 - the basic waves are drawn for a 96 kHz sample and need no oversampling of their own
    if ((spec->kind == eNodeOsc) || (spec->kind == eNodeOscShp)) {
        dt  = frequency / gSampleRate;
        sum = osc_waveform(voice, node, spec, advance_phase(&gPhase[voice][node], dt), dt, inc96, shape);
    } else {
        dt = frequency / (gSampleRate * (double)gOscOversample);

        for (step = 0; step < gOscOversample; step++) {
            double phase = advance_phase(&gPhase[voice][node], dt);

            gOscHistory[voice][node][gOscHistoryPos[voice][node]] = (float)osc_waveform(voice, node, spec, phase, dt, inc96, shape);
            gOscHistoryPos[voice][node]                           = (gOscHistoryPos[voice][node] + 1) % OSC_DECIMATE_TAPS;
        }

        // notes §156
        const float * history = gOscHistory[voice][node];
        uint32_t      oldest  = gOscHistoryPos[voice][node];

        if (gOscOversample == 1u) {
            // §29a - ONE SAMPLE IN, ONE OUT: there is no image to fold, so the filter is skipped
            // rather than run as a near-allpass. The history is still written above, so the taps
            // are warm the moment a rate change puts the oversampling back.
            sum = (double)history[(oldest + (OSC_DECIMATE_TAPS - 1u)) % OSC_DECIMATE_TAPS];
        } else {
            for (tap = 0; tap < OSC_DECIMATE_TAPS; tap++) {
                sum += (double)history[oldest] * gOscDecimate[OSC_DECIMATE_TAPS - 1 - tap];
                oldest++;

                if (oldest >= OSC_DECIMATE_TAPS) {
                    oldest = 0;
                }
            }
        }
    }

    // §27.2 - Sine2's DC blocker, at the instrument's rate and on its own coefficient
    if ((spec->kind == eNodeOscShp) && ((uint32_t)spec->wave == SHP_WAVE_SINE2)) {
        double * state = gLadder[voice][node];
        double   a     = SINE2_DC_COEFF * (SINE2_DC_RATE / gSampleRate);
        double   held  = state[0] + (a * state[1]);

        sum      = sum - held - (2.0 * state[0]);
        state[0] = state[0] + (a * sum);
        state[1] = held;
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
#define FLT_CONTROL_MIN          (0.0)
#define FLT_CONTROL_MAX          (127.0)

#define FLTMULTI_DAMPING_SPAN    (0.99)   // §10.2
#define FLTMULTI_DAMPING_TOP     (0.01)   // §10.2 - the instrument's own value at Res 127

static double fltmulti_damping(double resDial) {
    return (resDial >= 127.0) ? FLTMULTI_DAMPING_TOP : (1.0 - (FLTMULTI_DAMPING_SPAN * resDial / 128.0));
}

// §10.4 - zero at Res 127 on the instrument, which would leave a float loop lossless.
static double fltstatic_damping(double resDial) {
    return fmax(FLTMULTI_DAMPING_TOP, 1.0 - (resDial / 128.0));
}

// §10.2 - LP, BP and HP into the node's three legs.
static void fltmulti_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input, double pitchVar,
                          double pitchDirect, double voicePitch, double cutoffParam, double resonance, double legs[3]) {
    SE_LOCAL;

    double   control   = cutoffParam + ((pitchDirect + (pitchVar * spec->modAmount)) * PITCH_MOD_SEMITONES);

    if ((spec->fltKbt > 0.0) && (voicePitch >= 0.0)) {
        control += (voicePitch - KBT_REFERENCE_NOTE) * spec->fltKbt;
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

// §10.4 - FltMulti's filter (§10.2) with FltStatic's damping and drive, one output by FilterType.
static double fltstatic_step(double * state, double input, double tuning, double resDial, tFilterShape shape,
                             bool gainComp) {
    double damping   = fltstatic_damping(resDial);
    double bandScale = 1.0 - (0.5 * tuning);
    double feedback  = 2.0 * damping * damping * bandScale;
    double drive     = gainComp ? damping : 1.0;

    if (shape == eFilterShapeHighPass) {
        drive *= bandScale - (0.25 * tuning * tuning);
    } else if ((shape == eFilterShapeBandPass) && ((damping * damping) >= 0.5)) {
        drive = 2.0 * damping * damping;    // held to a unity peak while the damping is heavy
    }
    double lowBefore = state[2];
    double lowPrev   = state[0];
    double bandPrev  = state[1];
    double low       = lowPrev + (tuning * bandPrev);
    double high      = (drive * input) - low - (feedback * bandPrev);
    double band      = bandPrev + (tuning * high);

    state[0] = low;
    state[1] = band;
    state[2] = lowPrev;

    switch (shape) {
        case eFilterShapeBandPass:
        {
            return 0.5 * bandScale * (band + bandPrev);
        }
        case eFilterShapeHighPass:
        {
            return high;
        }
        default:
        {
            return 0.25 * (low + (2.0 * lowPrev) + lowBefore);
        }
    }
}

// Four-point Lagrange read, `delay` samples back from the next write (so at least 2).
static double comb_read(const float * line, uint32_t write, double delay) {
    const uint32_t mask  = COMB_LINE_SAMPLES - 1u;
    double         whole = floor(delay);
    double         t     = delay - whole;
    uint32_t       base  = (write - (uint32_t)whole) & mask;
    double         ym1   = line[(base + 1u) & mask];
    double         y0    = line[base];
    double         y1    = line[(base - 1u) & mask];
    double         y2    = line[(base - 2u) & mask];
    double         c1    = y1 - (ym1 / 3.0) - (y0 / 2.0) - (y2 / 6.0);
    double         c2    = ((ym1 + y1) / 2.0) - y0;
    double         c3    = ((y2 - ym1) / 6.0) + ((y0 - y1) / 2.0);

    return (((c3 * t) + c2) * t + c1) * t + y0;
}

// §13 - one section, k (1 + b z^-D) / (1 - c z^-D), in direct form II around one line per voice.
static double fltcomb_step(uint32_t voice, const tEngineNode * spec, double input, double pitchVar, double pitchDirect,
                           double voicePitch, double cutoffParam, double fbModInput) {
    SE_LOCAL;

    const tCombShape * shape   = flt_comb_shape(spec->combType);
    float *            line    = gCombLine[voice][spec->line];
    uint32_t *         write   = &gCombWrite[voice][spec->line];
    double             control = cutoffParam + ((pitchDirect + (pitchVar * spec->modAmount)) * PITCH_MOD_SEMITONES);

    if ((spec->fltKbt > 0.0) && (voicePitch >= 0.0)) {
        control += (voicePitch - KBT_REFERENCE_NOTE) * spec->fltKbt;
    }
    control      = fmin(fmax(control, FLT_CONTROL_MIN), FLT_CONTROL_MAX);

    double             delay   = flt_comb_delay_samples(control, shape, gSampleRate);
    double             g       = fmin(fmax(spec->combFeedback + (spec->combFbMod * fbModInput), -1.0), 1.0);
    double             delayed = comb_read(line, *write, fmin(fmax(delay, 2.0), (double)(COMB_LINE_SAMPLES - 3)));
    double             fed     = (input * spec->combLevel) + (shape->feedback * g * delayed);

    line[*write] = (float)fed;
    *write       = (*write + 1u) & (COMB_LINE_SAMPLES - 1u);
    return pow(10.0, (shape->gainDbPerG2 * g * g) / 20.0) * (fed + (shape->feedForward * g * delayed));
}

static double filter_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input, double mod, double pitchDirect, double voicePitch,
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
        control += (voicePitch - KBT_REFERENCE_NOTE) * spec->fltKbt;
    }
    control += pitchDirect * PITCH_MOD_SEMITONES;    // §21.3 - zero for every filter but FltClassic
    double shift   = control - cutoffParam;          // §22.1 - what the modulation adds, before the clamp

    if (control < FLT_CONTROL_MIN) {
        control = FLT_CONTROL_MIN;
    }

    if (control > FLT_CONTROL_MAX) {
        control = FLT_CONTROL_MAX;
    }
    cutoff   = flt_cutoff_hz(control);

    // Nyquist guard. With the control clamp above this cannot bite at any normal device rate — the
    // top of the dial is 21.1 kHz against an engine running at 96 kHz — so it is a guard against an
    // unusually low device rate, not part of the instrument's behaviour.
    if (cutoff > (gSampleRate * 0.45)) {
        cutoff = gSampleRate * 0.45;
    }

    if (cutoff < 1.0) {
        cutoff = 1.0;
    }
    g        = 1.0 - exp(-2.0 * M_PI * cutoff / gSampleRate);

    // notes §162
    if (g > LADDER_MAX_G) {
        g = LADDER_MAX_G;
    }
    // notes §163
#define LADDER_K_MAX    (4.3)

    switch (spec->topology) {
        case eFilterTopologyCascadeHP:
        {
            if (engine_filter_legacy()) {
                return cascade_hp_filter(gLadder[voice][node], input, g, spec->tapStage + 1u);
            }
            return flt_hp_stages(gLadder[voice][node], input, flt_stage_half(cutoffParam, shift), spec->tapStage + 1u);
        }
        case eFilterTopologyCascadeLP:
        {
            if (engine_filter_legacy()) {
                return ladder_filter(gLadder[voice][node], input, g, 0.0, spec->tapStage);
            }
            return flt_lp_stages(gLadder[voice][node], input, flt_stage_half(cutoffParam, shift), spec->tapStage + 1u);
        }
        case eFilterTopologyNord:
        {
            return nord_filter(gLadder[voice][node], input, flt_stage_half(cutoffParam, shift), resonance * 127.0,
                               spec->fltShape, spec->tapStage >= 2u, spec->fltGainComp);
        }
        case eFilterTopologyClassic:
        {
            return classic_filter(gLadder[voice][node], input, cutoff, resonance * 127.0, spec->tapStage);
        }
        case eFilterTopologyBiquad:
        {
            // notes §164
            return fltstatic_step(gLadder[voice][node], input, 2.0 * sin(M_PI * cutoff / gSampleRate),
                                  resonance * 127.0, spec->fltShape, spec->fltGainComp);
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
// §1.1 - one module's two legs through its peak follower, published for the canvas. notes §191
static void meter_node(const tEngineNode * spec, uint32_t n, double left, double right) {
    switch (spec->kind) {
        case eNodeMix:
        case eNodeMixStereo:
        case eNodeFxIn:
        case eNodeOut:
        {
            break;
        }
        default:
        {
            return;     // before SE_LOCAL: this is called for every Voice Area module, every sample
        }
    }
    SE_LOCAL;

    // BOTH LEGS, because a stereo module draws two meters and feeding only the left would
    // leave the right showing whatever the instrument last sent - which is worse than
    // showing nothing, because it looks live and is not.
    const double legs[2] = {left, right};
    uint32_t     packed  = METER_WRITTEN;

    for (uint32_t leg = 0u; leg < 2u; leg++) {
        double peak  = fabs(legs[leg]);
        int    level = 0;

        if (peak > gMeterEnv[n][leg]) {
            gMeterEnv[n][leg] = peak;
        } else {
            gMeterEnv[n][leg] += METER_DECAY * (peak - gMeterEnv[n][leg]);
        }

        if (gMeterEnv[n][leg] < METER_FLOOR) {
            gMeterEnv[n][leg] = 0.0;
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

typedef enum {
    eEnvOutPos = 0,
    eEnvOutPosInv,
    eEnvOutNeg,
    eEnvOutNegInv,
    eEnvOutBip,
    eEnvOutBipInv,
} tEnvOutType;   // posStrMap order

// §17.6 - the Output Type: the level (already times AM) inverted and offset. The bipolar pair are
// offset by Sustain, not by full scale, so the sustain stage sits at zero.
static double env_output(const tEngineNode * spec, double level) {
    double sustain = (double)spec->envSustainQ / ENV_FULL_SCALE_STEPS;

    switch (spec->envOutType) {
        case eEnvOutPosInv:
        {
            return 1.0 - level;
        }
        case eEnvOutNeg:
        {
            return level - 1.0;
        }
        case eEnvOutNegInv:
        {
            return -level;
        }
        case eEnvOutBip:
        {
            return level - sustain;
        }
        case eEnvOutBipInv:
        {
            return sustain - level;
        }
        default:
        {
            return level;
        }
    }
}

// §26 - the Keyboard module's outputs, in its connector order. One unit a semitone about E4 for the
// two pitches, full scale for the gate, and the velocities through the instrument's own curves.
static void keyboard_step(uint32_t voice, double voicePitch, double * out) {
    SE_LOCAL;

    const tVoice * v   = &gVoice[voice];
    double         lin = (double)v->velocity / 127.0;

    out[KEYBOARD_OUT_PITCH]   = (voicePitch - KEYBOARD_PITCH_ZERO) / PITCH_MOD_SEMITONES;
    out[KEYBOARD_OUT_GATE]    = (v->gate == true) ? 1.0 : 0.0;
    out[KEYBOARD_OUT_LIN]     = lin;
    out[KEYBOARD_OUT_RELEASE] = (double)v->release / 127.0;
    out[KEYBOARD_OUT_NOTE]    = ((v->note >= 0) ? ((double)v->note - KEYBOARD_PITCH_ZERO) : 0.0) / PITCH_MOD_SEMITONES;
    out[KEYBOARD_OUT_EXP]     = lin * lin * lin;
}

// §36 - the Glide module's slew, per voice. Log holds the TIME whatever the jump, so it is an
// exponential approach; Lin holds the RATE, an octave per Time, so it is a straight ramp. The Glide
// On input gates it: high (or nothing patched, with the button on) glides, otherwise In goes
// straight through. The first value a Glide ever sees arrives whole rather than being slewed up
// from zero, which is what `primed` is for.
static double glide_step(uint32_t voice, uint32_t node, double input, double gateIn,
                         const tEngineNode * spec) {
    SE_LOCAL;

    double current = gGlideOut[voice][node];
    // The button, unless something is patched into Glide On, which then decides. §38 - a logic
    // input is HIGH whenever it is above zero, which is how the instrument's own logic parts test
    // one; there is no halfway threshold.
    bool   gliding = (spec->in[1] >= 0) ? (gateIn > 0.0) : spec->active;

    if (gGlidePrimed[voice][node] == false) {
        gGlidePrimed[voice][node] = true;
        gGlideOut[voice][node]    = input;
        return input;
    }

    if ((gliding == false) || (spec->glideCoeff <= 0.0)) {
        gGlideOut[voice][node] = input;
        return input;
    }
    // §36.1 - AT THE ENVELOPE TICK RATE, because on the instrument this IS an envelope segment:
    // its coefficients come from the envelope's own tables. Running it per sample instead would
    // make the glide follow the engine's rate rather than the instrument's.
    gGlideTick[voice][node] -= ENV_TICK_HZ / gSampleRate;

    if (gGlideTick[voice][node] <= 0.0) {
        gGlideTick[voice][node] += 1.0;

        if (spec->glideLin == true) {
            double gap = input - current;

            if (fabs(gap) <= spec->glideCoeff) {
                current = input;
            } else {
                current += (gap > 0.0) ? spec->glideCoeff : -spec->glideCoeff;
            }
        } else {
            current += (input - current) * spec->glideCoeff;
        }
        gGlideOut[voice][node]   = current;
    }
    return current;
}

// §31 - the low and high ends of a LevConv range, in engine terms (1.0 is 64 units, §16).
static void lev_conv_range(bool isOut, uint32_t type, double * lo, double * hi) {
    if (isOut == false) {
        // levConvStrMap {Bip, Pos, Neg}
        switch (type) {
            case 1:  *lo = 0.0;
                *hi      = 1.0;
                break;                                // Pos
            case 2:  *lo = -1.0;
                *hi      = 0.0;
                break;                                // Neg
            default: *lo = -1.0;
                *hi      = 1.0;
                break;                                // Bip
        }
        return;
    }

    // posStrMap {Pos, PosInv, Neg, NegInv, Bip, BipInv} - the Inv forms are the same range, reversed
    switch (type) {
        case 1:  *lo = 1.0;
            *hi      = 0.0;
            break;                                // PosInv
        case 2:  *lo = -1.0;
            *hi      = 0.0;
            break;                                // Neg
        case 3:  *lo = 0.0;
            *hi      = -1.0;
            break;                                // NegInv
        case 4:  *lo = -1.0;
            *hi      = 1.0;
            break;                                // Bip
        case 5:  *lo = 1.0;
            *hi      = -1.0;
            break;                                // BipInv
        default: *lo = 0.0;
            *hi      = 1.0;
            break;                                // Pos
    }
}

// §35 - which key MonoKey reports, or -1 with nothing held. Shared by every voice, so it reads the
// keys the engine holds rather than anything belonging to one voice.
static int32_t mono_key_note(uint32_t priority, uint32_t voice) {
    SE_LOCAL;

    if (priority == 1u) {            // Lo
        for (int32_t k = 0; k < MIDI_KEY_COUNT; k++) {
            if (gKeyHeld[k] > 0u) {
                return k;
            }
        }

        return -1;
    }

    if (priority == 2u) {            // Hi
        for (int32_t k = MIDI_KEY_COUNT - 1; k >= 0; k--) {
            if (gKeyHeld[k] > 0u) {
                return k;
            }
        }

        return -1;
    }
    // Last is the mono voice's own note, which §15.2 has already handed back to a held key if the
    // one above it came up. A "last key pressed" of its own is what stopped a held note returning
    // when the note above it was released (CT, on 01 Mini Emulator).
    return gVoice[voice].note;       // and it outlives the key, as the voice's note does
}

// §38 - a logic input is HIGH above zero. There is no halfway threshold: that is how the
// instrument's own logic parts test one.
static bool logic_high(double value) {
    return value > 0.0;
}

// §38.2 - gateTypeStrMap {AND, NAND, OR, NOR, XOR, NXOR}
static bool gate_result(uint32_t type, bool a, bool b) {
    switch (type) {
        case 1:
        {
            return !(a && b);
        }                                   // NAND
        case 2:
        {
            return a || b;
        }                                   // OR
        case 3:
        {
            return !(a || b);
        }                                   // NOR
        case 4:
        {
            return a != b;
        }                                   // XOR
        case 5:
        {
            return a == b;
        }                                   // NXOR
        default:
        {
            return a && b;
        }                                   // AND
    }
}

#define LOGIC_PREV_CLOCK    (1u)
#define LOGIC_PREV_DATA     (2u)

static double logic_level(bool high) {
    return (high == true) ? LOGIC_HIGH_LEVEL : 0.0;
}

// The panel lamp a module shows, published for the face to read (notes §194). Only voice 0
// publishes: a polyphonic patch runs one of these per voice and the face has one LED, and the
// instrument shows a single lamp rather than however many voices happen to be sounding.
static void publish_module_led(uint32_t voice, const tEngineNode * spec, bool lit) {
    SE_LOCAL;

    if (voice != 0u) {
        return;
    }
    uint32_t lamp = METER_WRITTEN | ((lit == true) ? 1u : 0u);

    if (atomic_exchange_explicit(&gModuleLed[spec->location][spec->moduleIndex],
                                 lamp, memory_order_relaxed) != lamp) {
        atomic_store_explicit(&gMetersDirty, true, memory_order_relaxed);
    }
}

// §39 - one DrumSynth sample. Trig starts every envelope; each decays at its own per-tick
// multiplier, taken from the envelope's table. The bend sweeps the two oscillators DOWN from
// bendOctaves above their pitch, and the noise filter sweeps DOWN from sweepOctaves above its
// cutoff, both following their own decay (manual p.181). Velocity scales the two levels, the
// sweep, the bend, the click and the noise, and full velocity reaches the dialled settings.
static double drum_synth_step(uint32_t voice, uint32_t node, const tEngineNode * spec,
                              double trig, double pitchIn, double velIn) {
    SE_LOCAL;

    double * st  = gDrumState[voice][node];
    bool     hit = (trig > 0.0) && ((gLogicPrev[voice][node] & LOGIC_PREV_CLOCK) == 0u);
    double   vel = (spec->in[2] >= 0) ? fmin(fmax(velIn, 0.0), 1.0) : 1.0;

    gLogicPrev[voice][node] = (uint8_t)((trig > 0.0) ? LOGIC_PREV_CLOCK : 0u);

    if (hit == true) {
        st[DRUM_MASTER_ENV] = 1.0;
        st[DRUM_SLAVE_ENV]  = 1.0;
        st[DRUM_NOISE_ENV]  = 1.0;
        st[DRUM_BEND_ENV]   = 1.0;
        st[DRUM_CLICK_ENV]  = 1.0;
    }
    // The envelopes move on the envelope's own tick, as §36.1's glide does.
    st[DRUM_TICK]          -= ENV_TICK_HZ / gSampleRate;

    if (st[DRUM_TICK] <= 0.0) {
        st[DRUM_TICK]       += 1.0;
        st[DRUM_MASTER_ENV] *= spec->drumDecay[0];
        st[DRUM_SLAVE_ENV]  *= spec->drumDecay[1];
        st[DRUM_NOISE_ENV]  *= spec->drumDecay[2];
        st[DRUM_BEND_ENV]   *= spec->drumDecay[3];
    }
    // §39.5 - the face's lamp follows the master envelope, as the instrument's does
    publish_module_led(voice, spec, st[DRUM_MASTER_ENV] > DRUM_LED_FLOOR);

    st[DRUM_CLICK_ENV]     *= spec->drumClickDecay;
    {
        // §16.2 - a Pitch input is one unit a semitone.
        double bend   = spec->drumBendOctaves * vel * st[DRUM_BEND_ENV];
        double factor = exp2(bend + (pitchIn * PITCH_MOD_SEMITONES / 12.0));
        double master = spec->drumMasterHz * factor;
        double slave  = master * spec->drumSlaveRatio;
        double out    = 0.0;

        if ((master > 0.0) && (master < (gSampleRate * 0.5))) {
            st[DRUM_MASTER_PHASE] = advance_phase(&st[DRUM_MASTER_PHASE], master / gSampleRate);
            out                  += sin(st[DRUM_MASTER_PHASE] * 2.0 * M_PI)
                                    * st[DRUM_MASTER_ENV] * spec->drumLevel[0] * vel;
        }

        if ((slave > 0.0) && (slave < (gSampleRate * 0.5))) {
            st[DRUM_SLAVE_PHASE] = advance_phase(&st[DRUM_SLAVE_PHASE], slave / gSampleRate);
            out                 += sin(st[DRUM_SLAVE_PHASE] * 2.0 * M_PI)
                                   * st[DRUM_SLAVE_ENV] * spec->drumLevel[1] * vel;
        }
        // The noise, through its own sweeping multimode filter.
        {
            double   sweep  = spec->drumSweepOctaves * vel * st[DRUM_NOISE_ENV];
            double   cutoff = spec->drumNoiseHz * exp2(sweep);
            double * state  = gLadder[voice][node];
            double   noise  = white_noise(&gNoiseSeed[voice][node]);
            // A Chamberlin needs its coefficient held well below the rate it runs at or it goes
            // unstable at the top of a sweep - which is what the fifth Kick preset does. The
            // clamps are the ones nord_stage() uses on the same form (§23.1).
            double   f      = 2.0 * sin(M_PI * fmin(cutoff, gSampleRate * 0.20) / gSampleRate);
            double   q      = spec->drumNoiseRes;   // §39.9 - the instrument's own damping
            double   low    = fmin(8.0, fmax(-8.0, state[1] + (f * state[0])));
            double   high   = noise - low - (q * state[0]);
            double   band   = flt_clip4(state[0] + (f * high));
            double   picked;

            state[0] = band;
            state[1] = low;
            picked   = (spec->drumFilterType == 2u) ? high
                       : ((spec->drumFilterType == 1u) ? band : low);
            out     += picked * st[DRUM_NOISE_ENV] * spec->drumNoiseLevel * vel;
        }
        out += st[DRUM_CLICK_ENV] * spec->drumClick * vel * DRUM_CLICK_PEAK;
        return out;
    }
}

static void eval_node(uint32_t voice, uint32_t n, const tSoundEngineParams * paramsIn,
                      double value[][NODE_OUTPUTS], double voicePitch) {
    SE_LOCAL;

    const tEngineNode * base   = &paramsIn->node[n];
    const tEngineNode * byVel  = voice_morph_node(eAxisVelocity, base, n, voice);
    const tEngineNode * byKey  = voice_morph_node(eAxisKey, base, n, voice);
    // §26.2.2 - a node both morphs move plays a merge of the two, each word from the axis that moves
    // it; the smoothed values below still take both offsets, which is where a word both move lands
    const tEngineNode * spec   = voice_spec_node(base, n, voice, byVel, byKey);
    // The smoothed dial values follow the knob; a voice's velocity and key move them by ONE offset,
    // taken from the node this voice actually plays. §26.2.3 - that used to be two offsets, one per
    // axis, added in the value's own terms: right for a dial-unit field and wrong for a gain on a
    // curve. Where both axes move the same value, spec's word now comes from the build at the PAIR,
    // so the two amounts were summed before the conversion and the clamp, as the instrument does it.
    double              shape  = gSmoothedShape[n] + (spec->shape - base->shape);
    double              cutoff = gSmoothedCutoff[n] + (spec->cutoffParam - base->cutoffParam);
    double              res    = gSmoothedRes[n] + (spec->resonance - base->resonance);
    double              gain   = gSmoothedGain[n] + (spec->gain - base->gain);
    double              a      = signal_in(spec, value, 0);

    for (uint32_t leg = 0; leg < NODE_OUTPUTS; leg++) {
        value[n][leg] = 0.0;
    }

    switch (spec->kind) {
        case eNodeLfo:
        {
            value[n][0] = lfo_step(voice, n, spec);
            value[n][1] = value[n][0];

            publish_module_led(voice, spec, value[n][0] > 0.0);   // lit on the positive half
            break;
        }
        case eNodeOsc:
        case eNodeOscShp:
        {
            // Connector 0 is the direct Pitch input, connector 1 the knob-attenuated
            // PitchVar — see oscillator_step().

            if ((spec->kind == eNodeOsc) && (spec->wave == eOscWaveDual)) {    // §12.4
                double sawPhase = (0.5 - spec->dualSawPhase) - (OSCDUAL_PHASE_DEPTH * spec->dualPhaseMod * signal_in(spec, value, 3));

                shape               += OSCDUAL_PW_DEPTH * spec->dualPwMod * signal_in(spec, value, 2);
                gLadder[voice][n][5] = sawPhase - floor(sawPhase);
            }
            value[n][0] = (spec->active == true)
                              ? oscillator_step(voice, n, spec, voicePitch, a, signal_in(spec, value, 1), shape)
                              : 0.0;
            break;
        }
        case eNodeFilter:
        {
            // spec->fltGain is FltNord's GC and is 1.0 for every other filter, so this costs a
            // multiply and changes nothing where the module has no such control.
            value[n][0] = filter_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2), voicePitch,
                                      cutoff, res) * spec->fltGain;
            break;
        }
        case eNodeDx:
        {
            // §26.2.2 - merged where both axes move them, else whichever axis the node itself followed
            const tDxOperator * ops = voice_merged_ops(base, n, voice);

            if (ops == NULL) {
                ops = voice_morph_ops((byKey != base) ? eAxisKey : eAxisVelocity, base, n, voice);
            }

            if (ops == NULL) {
                ops = &paramsIn->dxOp[spec->dxBase];
            }
            value[n][0] = (spec->active == true) ? dx_step(voice, n, spec, ops, voicePitch) : 0.0;
            value[n][1] = value[n][0];
            break;
        }
        case eNodeKeyboard:
        {
            keyboard_step(voice, voicePitch, value[n]);
            break;
        }
        case eNodeEnv:
        {
            // §17.4
            bool   gate = ((spec->envKeyGate == true) && (gVoice[voice].gate == true)) || (signal_in(spec, value, ENV_INPUT_GATE) > 0.0);
            // §17.5 - an unpatched AM is full scale
            double am   = (spec->in[ENV_INPUT_AM] >= 0) ? fmin(fmax(signal_in(spec, value, ENV_INPUT_AM), -1.0), 1.0) : 1.0;
            double env  = env_output(spec, envelope_step(voice, n, spec, gate, value) * am);

            // Output 0 is the envelope itself, for patching at a modulation input. Output 1
            // is whatever audio is patched into the module, shaped by that envelope — the
            // G2's envelopes carry their own VCA, and this patch uses it as the amp.
            value[n][0] = env;
            value[n][1] = a * env;
            break;
        }
        case eNodeLevAmp:
        {
            value[n][0] = a * gain;
            break;
        }
        case eNodeLevMult:
        {
            value[n][0] = a * signal_in(spec, value, 1);
            break;
        }
        case eNodeModAmt:
        {
            // §29 - m: In through the Mod input at Depth. 1-m: In stays at full level at Depth 0.
            double mod = signal_in(spec, value, 1);

            if (spec->active == false) {
                value[n][0] = a;
            } else if (spec->modAmtOneMinus == true) {
                value[n][0] = a * ((1.0 - gain) + (gain * mod));
            } else {
                value[n][0] = a * gain * mod;
            }
            break;
        }
        case eNodeSwitch:
        {
            // §30 - closed passes In, or 64 units with nothing patched; open sends nothing.
            double closed = (spec->active == true) ? LOGIC_HIGH_LEVEL : 0.0;

            value[n][0] = (spec->in[0] < 0) ? closed : (a * ((spec->active == true) ? 1.0 : 0.0));
            value[n][1] = closed;
            break;
        }
        case eNodeLevConv:
        {
            // §31 - a straight line from the range it is told to read onto the one it writes, and
            // then SATURATED: the instrument's part computes offset + gain x In and clamps the
            // result to full scale, so an over-range input does not carry on past it.
            double inLo  = 0.0;
            double inHi  = 0.0;
            double outLo = 0.0;
            double outHi = 0.0;
            double out   = 0.0;

            lev_conv_range(false, spec->levConvIn, &inLo, &inHi);
            lev_conv_range(true, spec->levConvOut, &outLo, &outHi);
            out         = outLo + (((a - inLo) * (outHi - outLo)) / (inHi - inLo));
            value[n][0] = (out > 1.0) ? 1.0 : ((out < -1.0) ? -1.0 : out);
            break;
        }
        case eNodeLevAdd:
        {
            value[n][0] = a + spec->constant;   // §32
            break;
        }
        case eNodeSwSelect:
        {
            // §33 - the selected input on Out, and which one that is on Ctrl.
            uint32_t pick = (spec->select < spec->inputCount) ? spec->select : 0u;

            value[n][0] = signal_in(spec, value, pick);
            value[n][1] = ((double)pick * SWSEL_CTRL_UNITS) / UNITS_PER_FULL_SCALE;
            break;
        }
        case eNodeValSw:
        {
            // §34 - In 2 once Ctrl has REACHED the threshold, In 1 below it.
            double ctrl = signal_in(spec, value, 2);

            value[n][0] = (ctrl >= spec->constant) ? signal_in(spec, value, 1) : a;
            break;
        }
        case eNodeMonoKey:
        {
            // §35 - one keyboard: every voice reads the same three values out of this.
            int32_t key            = mono_key_note(spec->select, voice);
            bool    any            = false;

            for (int32_t k = 0; k < MIDI_KEY_COUNT; k++) {
                if (gKeyHeld[k] > 0u) {
                    any = true;
                    break;
                }
            }

            // §35.2 - bend and vibrato ride on the keyboard, not on the voice's note, so they are
            // what is left of voicePitch once this voice's own note is taken back off it.
            double  keyboardOffset = voicePitch - gVoice[voice].glidePitch;

            value[n][0] = ((key >= 0) ? (((double)key - KEYBOARD_PITCH_ZERO) + keyboardOffset) : 0.0)
                          / PITCH_MOD_SEMITONES;
            value[n][1] = (any == true) ? LOGIC_HIGH_LEVEL : 0.0;   // single-trigger: the LAST key up
            value[n][2] = (double)((key >= 0) ? gKeyVelocity[key] : 0u) / 127.0;
            break;
        }
        case eNodeGlide:
        {
            value[n][0] = glide_step(voice, n, a, signal_in(spec, value, 1), spec);   // §36
            break;
        }
        case eNodeAudioIn:
        {
            break;   // §37 - the engine has no audio input; both legs stay at zero
        }
        case eNodeInvert:
        {
            // §38.1 - two independent inverters. An unpatched input reads low, so its output
            // sits HIGH, which is what an inverter with nothing on it does.
            value[n][0] = logic_level(logic_high(a) == false);
            value[n][1] = logic_level(logic_high(signal_in(spec, value, 1)) == false);
            break;
        }
        case eNodeGate:
        {
            // §38.2 - two independent two-input gates, each with its own type.
            value[n][0] = logic_level(gate_result(spec->gateType[0], logic_high(a),
                                                  logic_high(signal_in(spec, value, 1))));
            value[n][1] = logic_level(gate_result(spec->gateType[1],
                                                  logic_high(signal_in(spec, value, 2)),
                                                  logic_high(signal_in(spec, value, 3))));
            break;
        }
        case eNodeFlipFlop:
        {
            // §38.3 - Clk, Rst, In. Outputs are NotQ then Q, in the module's own order.
            bool    clock    = logic_high(a);
            bool    reset    = logic_high(signal_in(spec, value, 1));
            bool    data     = logic_high(signal_in(spec, value, 2));
            uint8_t prev     = gLogicPrev[voice][n];
            bool    clkRise  = (clock == true) && ((prev & LOGIC_PREV_CLOCK) == 0u);
            bool    dataRise = (data == true) && ((prev & LOGIC_PREV_DATA) == 0u);

            if (reset == true) {
                // Rst has priority in both types, and in D-type it also holds the clock off.
                gLogicState[voice][n] = false;
            } else if (spec->logicToggled == true) {
                // Set-Reset: a rising edge on S sets. With S and Rst both low a clock TOGGLES,
                // and a constant high on either stops that - which the tests above already do.
                if (dataRise == true) {
                    gLogicState[voice][n] = true;
                } else if ((data == false) && (clkRise == true)) {
                    gLogicState[voice][n] = (gLogicState[voice][n] == false);
                }
            } else if (clkRise == true) {
                gLogicState[voice][n] = data;   // D-type: clock the D input through
            }
            gLogicPrev[voice][n] = (uint8_t)((clock ? LOGIC_PREV_CLOCK : 0u)
                                             | (data ? LOGIC_PREV_DATA : 0u));
            value[n][0]          = logic_level(gLogicState[voice][n] == false); // NotQ
            value[n][1]          = logic_level(gLogicState[voice][n]);          // Q
            break;
        }
        case eNodeDrumSynth:
        {
            // §39 - Trig, Pitch, Vel in; one audio output.
            value[n][0] = (spec->active == true)
                          ? drum_synth_step(voice, n, spec, a, signal_in(spec, value, 1),
                                            signal_in(spec, value, 2))
                          : 0.0;
            break;
        }
        case eNodeClkDiv:
        {
            // §38.4 - divide by 1..128. Gated passes every nth pulse with its shape unaltered;
            // Toggled flips on every nth EDGE, so an odd divider halves the frequency again.
            bool    clock   = logic_high(a);
            bool    reset   = logic_high(signal_in(spec, value, 1));
            uint8_t prev    = gLogicPrev[voice][n];
            bool    wasHigh = ((prev & LOGIC_PREV_CLOCK) != 0u);
            bool    rise    = (clock == true) && (wasHigh == false);
            bool    fall    = (clock == false) && (wasHigh == true);

            // The Rst input is the barred arrow: the reset waits for the next rising edge.
            if ((reset == true) && (rise == true)) {
                gLogicCount[voice][n] = 0u;
                gLogicState[voice][n] = false;
            } else if (spec->logicToggled == true) {
                if ((rise == true) || (fall == true)) {
                    gLogicCount[voice][n]++;

                    if (gLogicCount[voice][n] >= spec->divider) {
                        gLogicCount[voice][n] = 0u;
                        gLogicState[voice][n] = (gLogicState[voice][n] == false);
                    }
                }
            } else if (rise == true) {
                gLogicCount[voice][n] = (gLogicCount[voice][n] + 1u) % spec->divider;
            }
            gLogicPrev[voice][n] = (uint8_t)(clock ? LOGIC_PREV_CLOCK : 0u);

            // Gated passes the clock itself while the count is on the chosen pulse.
            value[n][0]          = (spec->logicToggled == true)
                          ? logic_level(gLogicState[voice][n])
                          : logic_level((gLogicCount[voice][n] == 0u) && (clock == true));
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
                          cutoff, res, legs);
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
        case eNodeFltComb:
        {
            value[n][0] = ((spec->active == true) && (spec->line < MAX_COMB_LINES))
                              ? fltcomb_step(voice, spec, a, signal_in(spec, value, 2), signal_in(spec, value, 1), voicePitch,
                                             cutoff, signal_in(spec, value, 3))
                              : a;
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

                left  += in * (gSmoothedLevel[n][2u * c] + (byVel->level[2u * c] - base->level[2u * c]) + (byKey->level[2u * c] - base->level[2u * c]));
                right += in * (gSmoothedLevel[n][(2u * c) + 1] + (byVel->level[(2u * c) + 1] - base->level[(2u * c) + 1]) + (byKey->level[(2u * c) + 1] - base->level[(2u * c) + 1]));
            }

            value[n][0] = left;
            value[n][1] = right;
            break;
        }
        case eNodeFade:
        {
            bool   oneIn = (spec->fadeKind == eFadePan) || (spec->fadeKind == eFadeOneToTwo);
            double pos   = shape + (MOD_INPUT_SCALE * spec->fadeMod * signal_in(spec, value, oneIn ? 1u : 2u));
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
            // the signal is on leg 1 and the modulation on leg 0 - spec->shaper.signalLeg says which.
            double sig = (spec->shaper.signalLeg == 0) ? a : signal_in(spec, value, 1);
            double mod = (spec->shaper.signalLeg == 0) ? signal_in(spec, value, 1) : a;

            value[n][0] = shaper_step(sig, mod, spec);
            break;
        }
        case eNodeMix:
        {
            uint32_t c           = 0;

            // notes §166 - a stereo mixer's inputs alternate L, R: each pair keeps its sides apart
            bool     stereoPairs = spec->mixStereo;

            for (c = 0; c < spec->inCount; c++) {
                uint32_t channel = stereoPairs ? (c / 2) : c;
                uint32_t leg     = stereoPairs ? (c % 2) : 0u;

                value[n][leg] += signal_in(spec, value, c) * (gSmoothedLevel[n][channel] + (byVel->level[channel] - base->level[channel]) + (byKey->level[channel] - base->level[channel]));
            }

            break;
        }
        case eNodeChorus:
        {
            if ((spec->active == true) && (spec->line < MAX_CHORUS_LINES)) {
                chorus_step(spec->line, a, spec->depth, spec->amount, &value[n][0], &value[n][1]);
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
                              ? delay_step(spec->line, a, signal_in(spec, value, 1), signal_in(spec, value, 2),
                                           (spec->inCount > 1u) && ((spec->in[1] >= 0) || ((spec->inCount > 2u) && (spec->in[2] >= 0))), spec) : a;
            value[n][1] = value[n][0];
            break;
        }
        case eNodeReverb:
        {
            // Only the first reverb in a chain is modelled; see the DSP note above.
            double inRight = signal_in(spec, value, 1);

            if ((spec->active == true) && (spec->line == 0)) {
                reverb_step(a, inRight, spec, &value[n][0], &value[n][1]);
            } else {
                value[n][0] = a;    // §20.5 - bypassed, each input passes to its own output
                value[n][1] = inRight;
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
            value[n][0] = (spec->active == true) ? (a * gain) : 0.0;
            value[n][1] = (spec->active == true)
                          ? (signal_in(spec, value, 1) * gain) : 0.0;
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
                value[n][0] = left * gain;
                value[n][1] = right * gain;
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
        case eNodeKeyboard:
        case eNodeEnv:
        case eNodeChorus:
        case eNodeReverb:
        case eNodeFade:         // writes both legs itself: Pan and Fade1-2 have two outputs
        case eNodeMixStereo:    // a genuine stereo pair
        case eNodeFltMulti:     // three outputs of its own
        case eNodeFxIn:         // the FX bus's two legs
        case eNodeMonoKey:      // §35 - Pitch, Gate and Vel
        case eNodeSwSelect:     // §33 - Out and Ctrl, which are not a stereo pair
        case eNodeInvert:       // §38.1 - two independent inverters
        case eNodeGate:         // §38.2 - two independent gates
        case eNodeFlipFlop:     // §38.3 - NotQ and Q
        case eNodeSwitch:       // §30 - likewise
        case eNodeAudioIn:      // §37 - silent, both legs already zero
        case eNodeOut:
        {
            break;
        }
        case eNodeMix:
        {
            if (spec->mixStereo == false) {
                value[n][1] = value[n][0];
            }
            break;
        }
        default:
        {
            value[n][1] = value[n][0];
            break;
        }
    }

    // The Voice Area's meters are fed from the voice sum instead - notes §191
    if (spec->postMix == true) {
        meter_node(spec, n, value[n][0], value[n][1]);
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
        if ((paramsIn->node[n].kind == eNodeDx) && (paramsIn->node[n].postMix == false)) {
            if (dx_voice_sounding(paramsIn, &paramsIn->node[n], v) == true) {
                return false;
            }
            continue;
        }

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

    static _Thread_local tSoundEngineParams params;    // notes §18 - too big for a callback's stack
    uint32_t                                frame            = 0;
    bool                                    chainHasEnvelope = false;
    uint32_t                                n                = 0;

    struct timespec                         started          = {0};

    (void)clock_gettime(CLOCK_MONOTONIC, &started);

    if ((out == NULL) || (channelCount == 0)) {
        return;
    }
    memset(out, 0, (size_t)frameCount * channelCount * sizeof(float));

    if (atomic_load(&gActive) == false) {
        return;
    }
    params = read_params();
    refresh_voice_morphs(params.build);

    // §26.2.2 - new tables, so every merged node is stale. Once per build, not once per block.
    if (gMergedBuild != params.build) {
        gMergedBuild = params.build;

        for (uint32_t v = 0; v < MAX_VOICES; v++) {
            merge_voice_nodes(v, &params);
        }

        merge_last_nodes(&params);
    }

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
        if (((params.node[n].kind == eNodeEnv) || (params.node[n].kind == eNodeDx)) && (params.node[n].postMix == false)) {
            chainHasEnvelope = true;   // a DXRouter's Operators carry their own envelopes (§14)
            break;
        }
    }

    bool droneMode = atomic_load(&gDroneMode);

    // notes §190
    if (  (droneMode == false) && (gDroneSeen == true)
       && (free_voice_runs(0, chainHasEnvelope, true) == true) && (free_voice_runs(0, chainHasEnvelope, false) == false)
       && (gVoice[0].sounding == false)) {
        gVoice[0].sounding = true;
        gVoice[0].released = 0;
        gVoice[0].quiet    = 0;
        gVoice[0].fade     = 1.0;
    }
    gDroneSeen = droneMode;

    // notes §174
    if (engine_no_free_run() == false) {
        double idleSamples = (double)frameCount * (double)gOversample;

        for (uint32_t v = 0; v < params.voiceCount; v++) {
            // notes §175
            bool rendered = (gVoice[v].sounding == true) || (free_voice_runs(v, chainHasEnvelope, droneMode) == true);

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
    // NONE OF THESE THREE DEPEND ON THE SAMPLE, so they are worked out once for the whole call
    // rather than per sample - the exp() in particular. They were inside the sample loop, hoisted
    // out of the voice loop but no further; the compiler will not lift them itself, because
    // eval_node() could in principle write gSampleRate.
    // §15.4 - a constant glide rate: the time is per octave, so this is semitones per sample.
    double envelopeStep = 1.0 / (ENVELOPE_SECONDS * gSampleRate);
    double smoothCoeff  = 1.0 - exp(-1.0 / (PARAM_SMOOTH_SECONDS * gSampleRate));
    double glideStep    = (params.glideSeconds > 0.0)
                          ? (12.0 / (params.glideSeconds * gSampleRate)) : 0.0;

    for (frame = 0; frame < frameCount; frame++) {
        uint32_t sub = 0;

        // gOversample passes of the whole graph per output sample (§29a - one, where the device is
        // already at the instrument's own rate). Note events are consumed inside, so they land on
        // the finer grid too rather than being quantised to the output rate.
        for (sub = 0; sub < gOversample; sub++) {
            double value[MAX_ENGINE_NODES][NODE_OUTPUTS];
            double voiceSum[MAX_ENGINE_NODES][NODE_OUTPUTS];

            // One event per sample. A chord's worth of note-ons arriving together therefore lands over
            // consecutive samples rather than all but the last being thrown away, and every note takes
            // effect where it actually arrived instead of at the next buffer boundary.
            start_pending_steals(&params);   // §15.3a - before the queue: a voice about to trig is busy
            (void)take_next_note_event(&params);

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

            memset(voiceSum, 0, (size_t)params.nodeCount * sizeof(voiceSum[0]));

            // notes §178
            for (uint32_t v = 0; v < params.voiceCount; v++) {
                tVoice * voice     = &gVoice[v];

                // notes §179
                bool     freeVoice = free_voice_runs(v, chainHasEnvelope, droneMode);
                bool     freeRun   = (voice->sounding == false) && (freeVoice == true);

                if ((voice->sounding == false) && (freeRun == false)) {
                    continue;               // costs nothing when it is not playing
                }

                // notes §180
                //
                // ONLY WHERE THERE IS NO ENVELOPE TO RELEASE. With one, the key coming up has to
                // start that release like any other voice's, and this used to hand the voice
                // straight to free-run instead - clearing `sounding` on a voice still audibly
                // releasing. The allocator reads `sounding`, so voice 0 then looked free from the
                // moment its key came up: once the other voices had each been used, EVERY note
                // landed on voice 0 and cut its own tail off (CT 2026-09-19, heard on 02 Big Pad as
                // stealing after a few notes, with thirteen voices sitting idle). It retires
                // through §182 now, and free-runs once it is actually finished.
                if ((freeVoice == true) && (voice->gate == false) && (chainHasEnvelope == false)) {
                    voice->sounding = false;
                    freeRun         = true;
                }

                // notes §181
                if (voice->note >= 0) {
                    bool   sliding = (params.glideMode == eGlideNormal)
                                     || ((params.glideMode == eGlideAuto) && (voice->glideActive == true));

                    double gap     = (double)voice->note - voice->glidePitch;

                    if ((sliding == true) && (glideStep > 0.0) && (fabs(gap) > glideStep)) {
                        voice->glidePitch += (gap > 0.0) ? glideStep : -glideStep;
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

                // Past the limit (counted from its envelopes finishing, below), wind the voice down
                // rather than cutting it. voice->fade reaching zero is what retires it.
                if (  (voice->gate == false)
                   && (freeRun == false)
                   && (droneMode == false)
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

                    for (uint32_t leg = 0; leg < NODE_OUTPUTS; leg++) {
                        voiceSum[n][leg] += value[n][leg] * level;
                    }

                    // What this voice is putting out, measured at its Out modules — the point where
                    // it leaves the voice for the mix or for the FX Area.
                    if (params.node[n].kind == eNodeOut) {
                        double magnitude = fabs(value[n][0] * level);

                        if (magnitude > leaving) {
                            leaving = magnitude;
                        }
                    }
                }

                voice->quiet    = (leaving < VOICE_SILENCE) ? (voice->quiet + 1) : 0;

                bool finished = (freeRun == false) && (voice_is_finished(&params, v, chainHasEnvelope) == true);

                // notes §20
                voice->released = (finished == true) ? (voice->released + 1) : 0;

                // notes §182
                if (  (voice->stealWait == 0u)
                   && (  ((finished == true) && (voice->quiet > (uint32_t)(VOICE_SILENCE_SECONDS * gSampleRate)))
                      || ((freeRun == false) && (voice->fade <= 0.0)))) {
                    voice->sounding = false;
                    voice->quiet    = 0;
                    voice->released = 0;
                    voice->fade     = 1.0;
                }
            }

            // What everything after the mix sees of the voices is their SUM, and so do their meters (notes §191).
            for (n = 0; n < params.nodeCount; n++) {
                if (params.node[n].postMix == false) {
                    for (uint32_t leg = 0; leg < NODE_OUTPUTS; leg++) {
                        value[n][leg] = voiceSum[n][leg];
                    }

                    meter_node(&params.node[n], n, value[n][0], value[n][1]);
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
            if (gOversample == 1u) {
                // §29a - the graph already runs at the output rate, so every internal sample IS an
                // output sample and there is nothing to fold down. 256 multiply-accumulates an
                // output sample, skipped.
                uint32_t last = (gOutHistoryPos + (OUT_DECIMATE_TAPS - 1u)) % OUT_DECIMATE_TAPS;

                outSample[0] = gOutHistory[0][last];
                outSample[1] = gOutHistory[1][last];
                outSample[2] = gOutHistory[2][last];
                outSample[3] = gOutHistory[3][last];
            } else {
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
    memset(&gEngineMono, 0, sizeof(gEngineMono));
    memset(&gKeyHeld, 0, sizeof(gKeyHeld));
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
    memset(&gLadder, 0, sizeof(gLadder));
    memset(&gDelayLine, 0, sizeof(gDelayLine));
    memset(&gDelayWrite, 0, sizeof(gDelayWrite));
    memset(&gDelayDamp, 0, sizeof(gDelayDamp));
    memset(&gDelayHp, 0, sizeof(gDelayHp));
    memset(&gDelayHpB, 0, sizeof(gDelayHpB));
    memset(&gDelayFb, 0, sizeof(gDelayFb));
    memset(&gDelayMod, 0, sizeof(gDelayMod));
    memset(&gChorusLine, 0, sizeof(gChorusLine));
    memset(&gChorusWrite, 0, sizeof(gChorusWrite));
    memset(&gChorusPhase, 0, sizeof(gChorusPhase));
    memset(&gChorusTrim, 0, sizeof(gChorusTrim));
    memset(&gChorusTick, 0, sizeof(gChorusTick));
    memset(&gPulseCount, 0, sizeof(gPulseCount));
    memset(&gPulsePrev, 0, sizeof(gPulsePrev));
    memset(&gCompEnv, 0, sizeof(gCompEnv));
    memset(&gCompGr, 0, sizeof(gCompGr));
    memset(&gCompLim, 0, sizeof(gCompLim));
    memset(&gRvRing, 0, sizeof(gRvRing));
    memset(&gRvCur, 0, sizeof(gRvCur));
    memset(&gRvPhase, 0, sizeof(gRvPhase));
    memset(&gEnvLevel, 0, sizeof(gEnvLevel));
    memset(&gEnvQ, 0, sizeof(gEnvQ));
    memset(&gEnvTick, 0, sizeof(gEnvTick));
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
    memset(&gEnvStage, 0, sizeof(gEnvStage));
    memset(&gEnvTrigger, 0, sizeof(gEnvTrigger));
    memset(&gVoiceMorphs, 0, sizeof(gVoiceMorphs));
    memset(&gVoiceMorphsSeq, 0, sizeof(gVoiceMorphsSeq));
    memset(&gVoiceMorphsAudio, 0, sizeof(gVoiceMorphsAudio));
    memset(&gVoiceMorphsSeen, 0, sizeof(gVoiceMorphsSeen));
    memset(&gVoiceMorphsUsable, 0, sizeof(gVoiceMorphsUsable));
    memset(&gLastRow, 0, sizeof(gLastRow));
    memset(&gBuildSerial, 0, sizeof(gBuildSerial));
    memset(&gAxisProbe, 0, sizeof(gAxisProbe));
    memset(&gSustainPedal, 0, sizeof(gSustainPedal));
    memset(&gSustainSeen, 0, sizeof(gSustainSeen));
    pthread_mutex_init(&gParamsWriteMutex, NULL);
    gOutputGainMilli  = 1000;
    gStatus           = eStatusOff;
    gDeviceRate       = 48000.0;
    gSampleRate       = 96000.0;
    gEngineVoices     = 1;
    sLastTypeBank[SE] = UINT32_MAX;    // no layout yet: the first call resets
    gPatchSlot        = -1;
    gDroneMode        = true;
    gDroneSeen        = true;
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
