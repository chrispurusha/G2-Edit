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
#if defined (__APPLE__)
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#else
#include <semaphore.h>
#endif

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
#define FLTNORD_PARAM_FMLIN      (7)   // §23.5
#define FLTNORD_PARAM_RESM       (9)   // §23.5
#define FLT_PARAM_RES            (3)
#define FLT_PARAM_SLOPE          (4)
#define FLT_PARAM_ACTIVE         (5)

#define ENV_PARAM_SHAPE          (0)
#define ENV_PARAM_ATTACK         (1)
#define ENV_PARAM_DECAY          (2)
#define ENV_PARAM_SUSTAIN        (3)
#define ENV_PARAM_RELEASE        (4)
#define ENV_PARAM_OUT_TYPE       (5)   // posStrMap: Pos, PosInv, Neg, NegInv, Bip, BipInv
#define ENV_INPUT_GATE           (1)   // node input: 0 is the audio, 1 the Gate jack, 2 AM
#define ENV_INPUT_AM             (2)
#define ENV_INPUT_MOD            (3)   // §17.10 - the time-mod jacks follow, one per modulated dial

#define LEVAMP_PARAM_GAIN        (0)
#define LEVAMP_PARAM_TYPE        (1)   // 0 = lin, 1 = exp

// notes §5
#define OUT_PARAM_DESTINATION    (0)
#define FXIN_PARAM_SOURCE        (0)   // Fx-In's "In from": inFxStrMap, 0 = FX 1/2, 1 = FX 3/4
#define OUT_PARAM_ACTIVE         (1)   // 2toOut's Bypass, non-zero is on
#define OUT_PARAM_PAD            (2)   // padStrMap: 0 dB or +6 dB

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
#define PULSE_DIAL_TOP         (127.0)

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

// §7.2a - the instrument's Color law: a one-pole whose corner falls exponentially from 20 kHz to
// 12 Hz, and a level compensation that rises as the dial cubed
#define NOISE_CORNER_TOP_HZ       (20000.0)
#define NOISE_CORNER_BOTTOM_HZ    (12.0)
#define NOISE_COMPENSATION        (1.0 / 65536.0)

static void noise_colour(double value, double sampleRate, double * pole, double * gain) {
    double colour = fmin(127.0, fmax(0.0, value));
    double corner = NOISE_CORNER_TOP_HZ * pow(NOISE_CORNER_BOTTOM_HZ / NOISE_CORNER_TOP_HZ, colour / 127.0);

    *pole = exp(-2.0 * M_PI * corner / sampleRate);
    *gain = 1.0 + (NOISE_COMPENSATION * colour * colour * colour);
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

// §40 - OscPerc: Coarse 0, Fine 1, Tune Mode 2, KBT 3, Pitch mod 4, then these, and Mute 8
#define PERC_PARAM_DECAY            (5)
#define PERC_PARAM_CLICK            (6)
#define PERC_PARAM_PUNCH            (7)
#define OSCNOISE_PARAM_WIDTH_MOD    (5)    // §8.1 - the module tables have 5 and 6 swapped
#define OSCNOISE_PARAM_WIDTH        (6)

static const tOscParams kOscParams[] = {
    //  type             tune cent kbt pmod ptype on  wparam wmode shape aWaves
    {moduleTypeOscB,     0, 1, 2,  3, 4,  9,  8, -1,  6, false},
    {moduleTypeOscA,     0, 1, 2,  3, 6,  5,  4, -1, -1, true },
    {moduleTypeOscC,     0, 1, 2,  7, 3,  5, -1,  0, -1, true },              // FmM 4, FM type 6: FM not modelled, as on OscB
    // §6.1a - OscD's parameter 3 IS its Tune Mode (the instrument's own parameter list); it has no
    // Pitch M dial, its one Pitch input being unattenuated
    {moduleTypeOscD,     0, 1, 2, -1, 3,  4, -1,  0, -1, true },
    {moduleTypeOscNoise, 0, 1, 2,  3, 4,  7, -1, -1, -1, false},
    {moduleTypeOscDual,  0, 1, 2,  3, 4, 10, -1, -1, -1, false},
    {moduleTypeOscPerc,  0, 1, 3,  4, 2,  8, -1, -1, -1, false},        // §40
    {moduleTypeOscPM,    0, 1, 2,  6, 3,  5, -1,  0, -1, false},        // §53 - PhM 4
};

static double perc_decay_word(double dial);

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
#define DSP_FULL_SCALE            (4.0)  // notes §196 - a 24-bit word's range, which every stored value saturates to
#define MUX8X_SPACING             (9.0)  // §70.11 - units of Ctrl between Mux8-1X's inputs
#define MUX8X_CTRL_MAX            (63.0)
#define VALSW_PARAM_VALUE         (0)    // the Ctrl threshold, 0-64 units in whole steps
#define VALSW_VALUE_TOP           (63)   // the top step reads 64, not 63 (render_paramType1UniPolShort)
#define VALSW_MATCH_UNITS         (0.5)  // §34 - Ctrl selects On while within half a unit of the value
#define MONOKEY_PARAM_PRIORITY    (0)    // monoKeyStrMap {Last, Lo, Hi}
#define GLIDE_PARAM_TIME          (0)
#define GLIDE_PARAM_ON            (1)    // offOnStrMap, default On
#define GLIDE_PARAM_SHAPE         (2)    // logStrMap {Log, Lin}: 0 is Log
#define GLIDE_SHAPE_LIN           (1)
#define GLIDE_LIN_STEP_SCALE      (0.1)  // §36.1 - Lin steps a tenth of the envelope's attack step

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
#define DRUM_PITCH_SEMIS_MAX       (64.0)    // §39.6 - Pitch and bend share one saturating word
#define DRUM_OSC_TICK_HZ           (24000.0) // §39.6 - the oscillators' own rate
#define DRUM_OSC_SMOOTH            (8.0)     // §39.6 - the output low-pass is 8x the increment
// §39.4a - the click: held one envelope tick at half the dialled level, then a one-pole decay
#define DRUM_INSTRUMENT_RATE       (96000.0) // the rate the instrument's own coefficients are for
#define DRUM_CLICK_DECAY_96K       (0.780851)
#define DRUM_CLICK_PEAK            (0.5)
// §39.10 - the noise path, in the instrument's own words
#define DRUM_DSP_TO_ENGINE         (4.0)                 // a DSP word is a quarter of the engine's unit
#define DRUM_VEL_WORD              (0.25)                // an unpatched Vel is 64 units
#define DRUM_STRIKE_WORD           (2.0 * DRUM_VEL_WORD) // §39.6 - the resonators are struck with Vel << 1
#define DRUM_LFSR_TAPS             (0xD71D87u)
#define DRUM_COLOUR_POLE_96K       (0.967525)
#define DRUM_SWEEP_SEMIS           (512.0)   // semitones per unit of envelope x sweep word
#define DRUM_RES_GAIN              (0.99)    // A.Y[6], written by the Noise Type action
#define DRUM_Q_SCALE               (0.9)     // A.X[9]
#define DRUM_CUTOFF_BASE_HZ        (16.3516) // the host's cutoff table, four entries higher

#define DRUM_MASTER_PHASE          (0)
#define DRUM_SLAVE_PHASE           (1)
#define DRUM_MASTER_ENV            (2)
#define DRUM_SLAVE_ENV             (3)
#define DRUM_NOISE_ENV             (4)
#define DRUM_BEND_ENV              (5)
#define DRUM_TICK                  (6)
#define DRUM_CLICK_ENV             (7)
#define DRUM_CLICK_HOLD            (8)
#define DRUM_MASTER_SMOOTH         (9)
#define DRUM_SLAVE_SMOOTH          (10)
#define DRUM_STATE_SLOTS           (11)
#define DRUM_LED_FLOOR             (1.0 / 128.0) // §39.5 - the lamp is out once the hit is inaudible

#define CHORUS_PARAM_DETUNE        (0)
#define CHORUS_PARAM_AMOUNT        (1)
#define CHORUS_PARAM_ACTIVE        (2)

// Compress: threshold and reference level run 0..42, ratio 0..66.
#define COMP_PARAM_THRESHOLD       (0)
#define COMP_PARAM_RATIO           (1)
#define COMP_PARAM_ATTACK          (2)
#define COMP_PARAM_RELEASE         (3)
#define COMP_PARAM_REFLVL          (4)
#define COMP_PARAM_SIDECHAIN       (5)  // §25.2 - detect on the side-chain input instead of In
#define COMP_PARAM_ACTIVE          (6)

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
    int mono;       // polyMonoStrMap: 1 is Mono, one LFO shared by every voice (§42)
    int rateMod;    // §50 - Rate M, the attenuator on the second rate input
    int kbt;        // §50 - offTo100KbStrMap: Off, 25, 50, 75, 100%
    int phase;      // §28.4 - the Phase dial
    int phaseMod;   // §28.4 - Phase M, on the Phase M input
    int shapeMod;   // §28.4 - Shape M, on the Shape M input (LfoShpA)
} tLfoParams;

// LfoB's rate dial is an ordinary dial rather than the LFORate type, but it indexes the same sweeps.
static const tLfoParams kLfoA    = {0, 7, 4, 6, -1, 5, 1, 3, 2, -1, -1, -1};
static const tLfoParams kLfoB    = {0, 2, 4, 8, -1, 7, 5, 1, 3, 6, 9, -1};
static const tLfoParams kLfoC    = {0, 3, -1, 2, -1, 4, 1, -1, -1, -1, -1, -1};
static const tLfoParams kLfoShpA = {0, 1, 11, 10, 5, 4, 9, 3, 2, 7, 6, 8};
#define LFO_RANGE_CLK          (4u)                   // rangeLfoStrMap: Sub, Lo, Hi, BPM, Clk
#define LFOSHPA_SHAPE_SCALE    (0x7c28f5 / 8388608.0) // §28.6 - the counter part hands the shape on x 0.97

// §28.6 - LfoShpA's Shape dial as the word (v - 64)/64 in 0..1, pinned to full at 127
static double lfo_shape_dial(double v) {
    return (v >= 127.0) ? 1.0 : (v / 128.0);
}

// §28.4 - the counter part's inputs after the two rate inputs: Rst, then LfoB's Phase M, or
// LfoShpA's Shape M and Phase M
#define LFO_IN_RESET               (2u)
#define LFOB_IN_PHASE_MOD          (3u)
#define LFOSHPA_IN_SHAPE_MOD       (3u)
#define LFOSHPA_IN_PHASE_MOD       (4u)
#define LFOSHPA_IN_DIR             (5u)        // §28.4
#define IN4_BUS_FIRST              (2u)        // §69.12 - voice_area_outputs_for_fx()'s number for Bus 1/2
#define CTRLRCV_TICK_HZ            (24000.0)   // §70.13 - the receive part's rate
#define MIDI_CC_GLOBAL_WHEEL_1     (96u)       // §70.13 - G2X Global Wheel 1
#define MIDI_CC_GLOBAL_WHEEL_2     (97u)
#define STATUS_TICK_HZ             (24000.0)   // §70.13 - the Status part's rate
#define SEQ_CONTROL_TICK_HZ        (24000.0)   // §58 - a sequencer part's rate when it is not up-rated
#define RNDCLKB_IN_STEP_MOD        (3u)        // §70.9
#define RNDSTEP_SEED_OFFSET        (0x4AAAAB)  // §70.9 - the Step M part's Y2 and X4
#define RNDSTEP_SEED_SLOPE         (-0x2AAAAB)
#define RNDSTEP_SHIFT              (10)        // its Y1
#define RND_SHIFT_TAPS             (0x872B41u) // §70.9 - Rnd2's feedback taps
#define RNDSTATE_START_LEVEL       (0.25)      // §70.9 - RndPattern State's Y5 in its frame
#define SEQCTR_REST_WORD           (0x220000)  // §70.10 - Ctrl out of range: the rest pair's index
#define SEQCTR_BIPOLAR_CENTRE      (0x100000)  // §70.10 - Pol bipolar: the centre subtracted before doubling
#define SEQCTR_TRIG_TICKS          (4.0)       // §70.10 - Trig's length in Pulse mode
#define NOISEGATE_FALL1            (0x2746)    // §70.6 - the follower's first stage, per 96 kHz sample
#define NOISEGATE_RISE2            (0x7FFFFF)  // its second: instant up
#define NOISEGATE_FALL2            (0x1A10)    // and slow down
#define NOISEGATE_OPEN             (0x200000)
#define NOISEGATE_ATK_SHORTEST     (0.00025)   // §70.6 - Attack: 0.25 ms (1 + 19 v / 127)^2
#define NOISEGATE_REL_OFFSET       (35.72)     // Release: ((v + 35.72) / 162.72)^5 s, dial 0 0.5 ms
#define NOISEGATE_REL_SHORTEST     (0.0005)
#define LFO_MAX_INPUTS             (6u)
#define LFO_RESET_PHASE            (0.5)     // §28.4 - the counter's word 0
#define LFO_RST_PREV_HIGH          (1u)      // gLogicPrev bit: the Rst input was above zero

#define CONST_PARAM_VALUE          (0)
#define CONST_PARAM_BIP_UNI        (1) // bipUniStrMap: 0 is Bipolar, 1 Unipolar - §16.1
#define CONSTSW_PARAM_ON           (1) // §44 - ConstSwT: Value 0, the switch 1, Bip/Uni 2
#define CONSTSW_PARAM_BIP_UNI      (2)
#define SW1TO8_OUTS                (8) // §45 - Out 1..8, then Ctrl
#define LOGICDLY_PARAM_TIME        (0) // §46 - Time 0, TimeMod 1, Range 2; the type is mode 0
#define LOGICDLY_PARAM_TIMEMOD     (1)
#define LOGICDLY_PARAM_RANGE       (2)
#define LOGICDLY_MODE_TYPE         (0) // logicDelayModeStrMap: Pos, Neg, Cycle
#define LOGICDLY_TYPE_NEG          (1u)
#define LOGICDLY_TYPE_CYCLE        (2u)
#define RNDA_PARAM_RATE            (0) // §47 - RandomA
#define RNDA_PARAM_MONO            (1)
#define RNDA_PARAM_OUTTYPE         (2) // bipPosNegStrMap
#define RNDA_PARAM_RANGE           (3)
#define RNDA_PARAM_ACTIVE          (4)
#define RNDA_PARAM_EDGE            (5)
#define RNDA_PARAM_STEP            (6)
#define COMPLEV_PARAM_LEVEL        (0) // §48 - C
#define NOTEQ_PARAM_RANGE          (0) // §49 - Range, then Notes (0 is Off)
#define NOTEQ_PARAM_NOTES          (1)
#define OSCMST_PARAM_COARSE        (0) // §51 - Pitch, Cent, Kbt, the tune mode (display only), Pitch M
#define OSCMST_PARAM_FINE          (1)
#define OSCMST_PARAM_KBT           (2)
#define OSCMST_PARAM_PITCHMOD      (4)
#define OSCPM_PARAM_PHM            (4)   // §53
#define OSCPM_CYCLES_PER_UNIT      (8.0) // §53 - phase moved per full-scale (64-unit) input at full PhM
#define PHASER_PARAM_TYPE          (0)   // §55 - Type, Rate, FB, Bypass
#define PHASER_PARAM_RATE          (1)
#define PHASER_PARAM_FB            (2)
#define PHASER_PARAM_ACTIVE        (3)
#define PHASER_TICK_HZ             (24000.0) // §55 - the LFO and the coefficients step at the control rate
#define FLTVOICE_PARAM_VOWEL1      (0)       // §56 - Vowel1-3, Level, Vowel, VowelMod, Freq, FreqMod, Res, Bypass
#define FLTVOICE_PARAM_LEVEL       (3)
#define FLTVOICE_PARAM_VOWEL       (4)
#define FLTVOICE_PARAM_VOWELMOD    (5)
#define FLTVOICE_PARAM_FREQ        (6)
#define FLTVOICE_PARAM_FREQMOD     (7)
#define FLTVOICE_PARAM_RES         (8)
#define FLTVOICE_PARAM_ACTIVE      (9)
#define FLTVOICE_VOWELS            (9)    // vowelStrMap: A, E, I, O, U, Y, AA, AE, OE
#define FLTVOICE_FINE_OFFSET       (-0.5) // §56.1 - the fine table starts at -50 cents, and the part reads it from there
#define FREQSHIFT_PARAM_SHIFT      (0)    // §57 - FreqShift, Mod, Range, Bypass
#define FREQSHIFT_PARAM_MOD        (1)
#define FREQSHIFT_PARAM_RANGE      (2)
#define FREQSHIFT_PARAM_ACTIVE     (3)
#define MAX_FREQSHIFT_LINES        (2)  // §57 - per patch; more run bypassed
#define FREQSHIFT_STATES           (20)
#define MAX_SEQ_LINES              (8)  // §58 - step sequencers per patch; more stay silent
#define SEQ_STEPS                  (16)
#define SEQ_PARAM_CYCLE            (32) // §58 - after the two rows of 16
#define SEQ_PARAM_LENGTH           (33)
#define SEQ_X_WORDS                (16)
#define SEQ_Y_WORDS                (48)
#define SEQREC_X_WORDS             (6)  // §58.1 - SeqNote's record part: its own X frame
#define SEQREC_Y                   (44) // §58.1 - and its Y frame, after the 16-step part's 44 words
#define SEQREC_LAST_STEP           (37) // §58.1 - the linker's Y0: the 16th step's word, the last it may write
#define SEQREC_DELAY_OFF           (0x7FFFFF)
#define SEQREC_DELAY_24K           (0x10CC)
#define SEQ_IN_REC_VAL             (6u)             // SeqNote's inputs after Clk, Rst, Loop, Park, Note, Trig
#define SEQ_IN_REC_ENABLE          (7u)
#define MAX_CLKGEN_LINES           (4)              // §59 - clock generators per patch
#define MAX_METNOISE_LINES         (4)              // §66 - per patch; more run silent
#define MAX_FLTPHASE_LINES         (4)              // §67 - per patch; more pass their input through
#define MAX_DLYCLOCK_LINES         (4)              // §69.7 - per patch; more output nothing
#define MAX_FXBUF_LINES            (4)              // §70 - Flanger, PShift, Scratch: short post-mix buffers
#define DSP_WORD_SCALE             (8388608.0)      // a 24-bit word's 1.0
#define FLANGER_RING               (512.0)          // §70.2 - the ring, in 96 kHz samples
#define FLANGER_RANGE_STEP         (56300.0)        // §70.2 - Range: v x 0xdbec of a sweep word
#define FLANGER_MIN_WORD           (0x128000u)      // §70.2 - the sweep's floor: 74 samples
#define FLANGER_IN_GAIN            (0.8)            // §70.2
#define FLANGER_MIX_GAIN           (0.3)            // §70.2
#define PSHIFT_WINDOW_X1           (127.0 / 2048.0) // §70.3 - the shortest Delay's tap span, of the line
#define PSHIFT_TAP_SCALE           (9728.0)         // §70.3 - the 100 ms line's length word, in samples
#define PSHIFT_RATE_SCALE          (6990.67)        // §70.3 - phase step per tick at a ratio of 0
#define SCRATCH_RATE_SCALE         (6990.0)         // §70.3
#define SCRATCH_SMOOTH_IN          (0.0100002)      // §70.3 - 0x147ae
#define SCRATCH_SMOOTH_POLE        (0.99)           // §70.3 - 0x7eb852
#define KARPLUS_LINE               (7000.0)         // §70.4 - OscString's line, in 96 kHz samples
#define KARPLUS_DECAY_A            (2.070135)       // §70.4 - the Decay law's three constants
#define KARPLUS_DECAY_B            (11.050795)
#define KARPLUS_DECAY_R            (0.9869)
#define VOCODER_EMPHASIS_0         (0x5061f1)             // §70.8 - the pre-emphasis: 8 (e0 x - e1 e0 x[n-1])
#define VOCODER_EMPHASIS_1         (0x4bd344)
#define VOCODER_OUT_GAIN           (0x651eb8)             // §70.8 - the output converter's gain, then x 8
#define LEVSCALER_SLOPE_MAX        (341.0)                // §70.12 - a Gain dial's word at full: 8.02 dB an octave
#define DRIVER_TYPE_BOW            (1u)                   // §70.5 - Reed, Bow, -Lip-, -Mallet-
#define RESONATOR_ALGS             (5u)                   // §70.4a - String1, String2, Tube1, Tube2, Tube3
#define RESONATOR_TRIM             (6.0)                  // §70.4a - the period less the loop's own six samples
#define RESONATOR_LOSS             (0x7c28f6 / 8388608.0) // §70.4a - line 1's fixed 0.97
#define MAX_PITCH_TRACKER_LINES    (4)                    // §70.7 - Pitch Trackers per patch; more read silence
#define PD_RELEASE_FAST            (0x2746 / 8388608.0)   // §70.7 - the followers' first-stage release, a 96 kHz sample
#define PD_RELEASE_GATE            (0x1a10 / 8388608.0)   // §70.7 - the gate follower's second stage
#define PD_RELEASE_PEAK            (0x270a / 8388608.0)   // §70.7 - the peak followers' second stage
#define PD_ATTACK                  (0x7fffff / 8388608.0)
#define PD_LP_POLE                 (0x7d6103 / 8388608.0) // §70.7 - the low-pass, ~316 Hz
#define PD_HP_POLE                 (0x7fe645 / 8388608.0) // §70.7 - the DC blocker, ~12 Hz
#define PD_PEAK_SHARE              (0x733333 / 8388608.0) // §70.7 - a peak is 0.9 of its envelope
#define FXBUF_SAMPLES              (16384)
#define MAX_FXBUF_VOICE_LINES      (2)                    // notes §197 - in the Voice area, per voice
#define FXBUF_INSTANCES            (MAX_FXBUF_LINES + (MAX_FXBUF_VOICE_LINES * MAX_VOICES))
#define MAX_STRING_LINES           (4)                    // §70 - OscString one each, Resonator two (its two lines)
#define STRING_SAMPLES             (8192)                 // §70.4 - OscString's line is 7000 samples at 96 kHz
#define MAX_BASIC_LINES            (2)                    // §70 - Vocoder: per-voice filter states
#define DLYCLOCK_SLOTS             (128)
#define FLTPHASE_W_FREQ            (0)                    // §67 - the node's words: the pitch part's X2
#define FLTPHASE_W_PITCHM          (1)                    // Y0
#define FLTPHASE_W_SPREADM         (2)                    // X4
#define FLTPHASE_W_SPREAD          (3)                    // Y3
#define FLTPHASE_W_FB              (4)                    // the filter part's X8
#define FLTPHASE_W_FBM             (5)                    // Y9
#define FLTPHASE_W_LEVEL           (6)                    // Y1
#define FLTPHASE_W_LOOP            (7)                    // Y10, set by Type
#define FLTPHASE_W_DRY             (8)                    // Y11, set by Type
#define CLKGEN_PARAM_TEMPO         (0)                    // §59 - Tempo, On, Source, Sync every, Swing
#define CLKGEN_PARAM_ACTIVE        (1)
#define CLKGEN_PARAM_SOURCE        (2)                    // clkSrcStrMap: 0 Int, 1 Master
#define CLKGEN_PARAM_SYNC          (3)
#define CLKGEN_PARAM_SWING         (4)
#define CLKGEN_TICK_HZ             (24000.0)
#define DLYSGL_PARAM_TIMEMOD       (1)         // §52 - DlySingleB: Time 0, Time M 1; the range is mode 0
#define DSP_WORD_PER_ENGINE        (2097152.0) // 0x200000, 64 units: the engine's 1.0 as a 24-bit word

#define FXIN_PARAM_ACTIVE          (1)
#define FXIN_PARAM_PAD             (2) // db12PadStrMap: +6 dB, 0 dB, -6 dB, -12 dB

// §9.1
#define MAX_NODE_INPUTS            (10)

#define MAX_BACK_EDGES             (16) // notes §192

// §9.3
#define NODE_OUTPUTS               (9)

// §9.2
#define MAX_NODE_LEVELS            (12)

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
#define OSC_INSTRUMENT_RATE       (96000.0)
#define OSC_SYNC_PHASE            (0.55)   // §6.6 - the sync part's reset word, -0.9, in the engine's phase
#define OSC_PM_SYNC_PHASE         (0.05)   // §53 - the same word in OscPM's phase over 0..1
#define OSC_SYNC_PREV_HIGH        (4u)     // gLogicPrev bit: the Sync input was above zero
#define OSCPM_SYNC_SLOT           (1)      // §53 - OscPM's inputs: PitchVar, Sync, Phase M, Pitch
#define OSC_EDGE_SAMPLES          (2.0)
#define OSC_CORNER_LIMIT_MULTI    (2.0)    // OscA, OscB
#define OSC_CORNER_LIMIT_PARTS    (1.0)    // OscC, OscD

// notes §16
#define OSCB_TUNE_UNITY           (64.0)
// §6.8 - linear FM: Lin adds 2 x amount x input words a sample (a unit is 1/4 of a word, a word is
// 48 kHz of increment); Trk 64 x amount x input x the key increment, and the Pitch part makes the
// oscillator's increment 32 x Coarse x Fine words of it, 0x1c20d/2^23 and 1/2 at unity
#define FM_LIN_HZ                   (OSC_INSTRUMENT_RATE / 4.0)
#define FM_TRK_SCALE                (8388608.0 / 0x1c20d)
#define FM_MAX_DEVIATION_HZ         (OSC_INSTRUMENT_RATE / 2.0)
#define MIDI_NOTE_A440              (69.0)
#define MIDI_NOTE_MIDDLE_C          (60.0)
#define KBT_REFERENCE_NOTE          (64.0)    // §21.3 - the instrument's pitch zero, E4: where KBT moves nothing

// notes §17
#define VOICE_GAIN                  (0.15)

// Where the output starts bending rather than shearing.
#define OUTPUT_KNEE                 (0.80)
#define OUTPUT_COUPLING_TAU         (0.01357) // notes §198 - seconds: the G2's outputs, a pole at 11.7 Hz
#define DAC_FILTER_HZ               (33500.0) // notes §199 - the G2's output stage: a two-pole low-pass
#define DAC_FILTER_Q                (0.66)
#define ENVELOPE_SECONDS            (0.005)   // the anti-click ramp used when no EnvADSR is in the chain

// Every ladder runs its full four poles whatever slope is selected — see ladder_filter().
#define LADDER_POLES                (6) // state available: FltLP's 36 dB setting is six poles
#define FILTER_STATE_SLOTS          (8) // per filter node: FltNord's two stages need eight (§23.1)
#define LADDER_LOOP_POLES           (4) // the RESONANCE loop is four long whatever is tapped - measured

// notes §18
#define MAX_ENGINE_NODES            (128)
#define MAX_DX_OPERATORS            (24) // §14 - six per DXRouter, so four routers

// §14 - Operator and DXRouter, in the instrument's own words.
#define DX_LEVEL_TOP                (0x7fffff)         // §14.2 - the log level's top; kDxAmpWords is read at level >> 16
#define DX_LEVEL_BIAS               (-0x77660)         // §14.4 - every operator sits this far below the top
#define DX_HOLD_MARK                (-0x800000)        // §14.2 - a holding segment's target, below every level
#define DX_ACC_WORD                 ((int64_t)1 << 24) // a word at the accumulator's precision
#define DX_RATE_TOP                 (99)
#define DX_ATTACK_JUMP_BELOW        (0x34b5dd)         // §14.2 - the attack's step, by how far up it has come
#define DX_ATTACK_JUMP              (0xa3d7)
#define DX_ATTACK_LOW_BELOW         (0x600000)
#define DX_ATTACK_LOW               (0x7ae14)
#define DX_ATTACK_MID_BELOW         (0x75c28f)
#define DX_ATTACK_MID               (0x28f5c)
#define DX_ATTACK_BASE              (0x1a9fc)
#define DX_DECAY                    (0xbd567)
#define DX_RATESCALE_KEY_OFFSET     (0x158000)   // §14.4 - rate scaling counts up from the keyboard's bottom
#define DX_KBSCALE_SPAN_MAX         (63)         // §14.4 - in steps of two semitones
#define DX_KBSCALE_LIN_NUM          (162263)     // §14.4 - the Lin curve is d x 162263/40, Exp a quarter of it
#define DX_KBSCALE_LIN_DEN          (40)
#define DX_KBSCALE_SHIFT            (7)
#define DX_VEL_INDEX_SHIFT          (14)         // §14.4 - Vel's word to its kDxVelocityWords index
#define DX_DETUNE_CENTS_PER_STEP    (1.0)
#define DX_E4_HZ                    (329.6276)
#define DX_FM_CYCLES_PER_UNIT       ((0x345487 / 8388608.0) * 64.0 / 2.0 / DSP_FULL_SCALE)   // §14.3

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
    eNodeOscPerc,        // §40 - a struck, decaying resonator
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
    eNodeValSw,          // §34 - ValSw2-1: In 2 while Ctrl equals the value
    eNodeMonoKey,        // §35 - the keyboard's last/lowest/highest key, shared by every voice
    eNodeGlide,          // §36 - a slew for control signals
    eNodeAudioIn,        // §37 - 2-In: the engine has no audio input, so silence
    eNodeInvert,         // §38.1 - two logic inverters
    eNodeGate,           // §38.2 - two two-input gates, each with its own type
    eNodeFlipFlop,       // §38.3 - D-type or Set-Reset
    eNodeSandH,          // §38.5 - sample on the clock's rising edge, hold
    eNodeKeyQuant,       // §41 - pitch snapped to the keys that are on
    eNodeClkDiv,         // §38.4 - divide a clock by 1..128, Gated or Toggled
    eNodeDrumSynth,      // §39 - two oscillators, a swept noise filter, bend and click
    eNodeMinMax,         // §43 - the lesser and the greater of two inputs
    eNodeSw1to8,         // §45 - one input to the selected one of eight outputs, plus Ctrl
    eNodeLogicDelay,     // §46 - delays a logic signal's rising edge, falling edge or whole pulse
    eNodeRandomA,        // §47 - a random LFO: new values once a cycle, stepped or glided
    eNodeCompLev,        // §48 - logic HIGH while In has reached the C level
    eNodeNoteQuant,      // §49 - pitch scaled by Range and snapped to steps of n semitones
    eNodeOscMaster,      // §51 - no sound: the pitch an oscillator would play, as a signal
    eNodeDlySingle,      // §52 - DlySingleA/B: a bare delay line, B's time modulated
    eNodeOscPM,          // §53 - sine or triangle with a phase-modulation input
    eNodePhaser,         // §55 - two or three swept second-order allpass sections with feedback
    eNodeFltVoice,       // §56 - four formant resonators, blended across three vowels
    eNodeFreqShift,      // §57 - a Bode shifter: a Hilbert pair and a quadrature oscillator
    eNodeSeq16,          // §58 - SeqVal, SeqNote and SeqEvent: the one 16-step part
    eNodeClkGen,         // §59 - a tempo clock: 1/96, 1/16, active and a sync pulse
    eNodeNoteScaler,     // §60 - In x Range
    eNodeNoteSend,       // §62 - notes to this slot: a gate's edges play and release the engine's own voices
    eNodeRndClkA,        // §64 - a random value drawn on each clock edge
    eNodeRndTrig,        // §64 - each clock pulse passed with a probability
    eNodeDlyStereo,      // §65 - two DelayB lines from one input, each feeding back itself and the other
    eNodeMetNoise,       // §66 - six squares at fixed ratios, through eight one-pole high-passes
    eNodeFltPhase,       // §67 - six state-variable allpass stages, tapped after 1-6, with feedback and dry mix
    eNodeValSw12,        // §68.2 - In to Out 2 while Ctrl equals the value, else Out 1
    eNodeMux8to1,        // §68.3 - the input Ctrl picks, 4 units a step
    eNodeMux1to8,        // §68.3 - In on the output Ctrl picks
    eNodeTandH,          // §68.4 - follows In while Ctrl is high, holds it while low
    eNodeWindSw,         // §68.5 - In and a high Gate while Ctrl is within From..To
    eNodeCounter8,       // §68.6 - one of eight outputs high, stepped by the clock
    eNodeBinCounter,     // §68.6 - the clock count's eight bits
    eNodeADConv,         // §68.7 - In as an 8-bit two's complement code, half a unit a step
    eNodeDAConv,         // §68.7 - the reverse
    eNodeRatePass,       // §68.8 - Red2Blue and Blue2Red: the engine runs every signal at one rate
    eNodeCompSig,        // §69.2 - logic high while A >= B
    eNodeLevMod,         // §69.3 - a crossfade from In through AM to In x Mod
    eNodeEnvFollow,      // §69.4 - a peak stage, then an attack/release one-pole
    eNodePartQuant,      // §69.5 - pitch to the nearest harmonic partial's interval
    eNodeDlyShiftReg,    // §69.6 - eight clocked samples, Out 1 the newest
    eNodeDlyClock,       // §69.7 - the value clocked in N clocks ago
    eNodeDigitizer,      // §69.8 - sample and hold at an exponential rate, then a bit mask
    eNodeWahWah,         // §69.9 - a state-variable band-pass swept along sweep squared
    eNodeNoteDet,        // §69.11 - Gate, Vel and RVel of one key, from the held-key table
    eNodeMultiTap,       // §70.1 - DelayDual, DelayQuad, DlyEight: taps on one line
    eNodeFlanger,        // §70.2 - an LFO-swept short delay with feedback
    eNodePShift,         // §70.3 - PShift and Scratch: two crossfaded moving taps
    eNodeOscString,      // §70.4 - a tuned delay loop with decay and damping
    eNodeResonator,      // §70.4 - OscString's loop, with a pickup position
    eNodeDriver,         // §70.5 - a reed or bow table between an excitation and a return
    eNodeNoiseGate,      // §70.6 - a follower opening a gate above the threshold
    eNodePitchTrack,     // §70.7 - PitchTrack and ZeroCnt: the period between rising zero crossings
    eNodeVocoder,        // §70.8 - sixteen analysis bands driving sixteen synthesis bands
    eNodeRndPattern,     // §70.9 - a clocked random pattern reseeded every Loop steps
    eNodeSeqCtr,         // §70.10 - the step Ctrl picks, with a crossfade
    eNodeMux8to1X,       // §70.11 - Mux8-1 with a crossfade between neighbours
    eNodeLevScaler,      // §70.12 - a gain from the key's distance to a breakpoint
    eNodeStatus,         // §70.13 - Patch Active, Var Active, Voice No.
    eNodeDevice,         // §70.13 - the performance controls
    eNodeCtrlRcv,        // §70.13 - no MIDI CC stream reaches the engine: both outputs 0
    eNodeSink,           // §70.13 - MIDI senders and routers: nothing to render
    eNodeIn4Bus,         // §69.12 - 4-In from Bus: Bus 1/2 on outputs 1-2, Bus 3/4 on 3-4
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
    int32_t decay;      // §17.11 - EnvMulti's fall multiplier, Q23
    uint8_t rising;     // chooses the attack-shaped recurrence and which way the end test runs
    uint8_t sustain;    // held while the gate is
    int8_t  modLeg;     // §17.10 - the node input carrying this stage's time mod, or -1
    uint8_t dial;       // its own time dial, so the mod can re-read the stage from dial + offset
    uint8_t modAmount;  // the mod amount dial
    uint8_t pad[3];
} tEnvSegment;

typedef struct {
    tNodeKind kind;
    uint32_t  moduleIndex;   // so per-node audio state can survive a knob turn (see topology_signature)
    uint32_t  location;      // Voice or FX — the two areas number their modules independently

    // notes §21
    int32_t   in[MAX_NODE_INPUTS];
    uint32_t  srcOut[MAX_NODE_INPUTS];
    uint32_t  srcLeg[MAX_NODE_INPUTS];     // which of the source's NODE_OUTPUTS legs that output is
    uint32_t  inCount;
    int8_t    syncSlot;                    // §6.6 - which input is the Sync jack, -1 for none
    int8_t    shapeModSlot;                // §6.7 - which input is the Shape Mod jack, -1 for none
    int8_t    fmSlot;                      // §6.8 - which input is the FM jack, -1 for none
    bool      fmTrack;                     // §6.8 - FM Trk rather than FM Lin
    bool      active;                      // the module's own power button
    // notes §192 - a leg that closes a loop reads its source's value from the previous sample
    uint32_t  backMask;                    // which legs
    uint8_t   backModule[MAX_NODE_INPUTS]; // while building: the module the leg waits for
    uint8_t   backOut[MAX_NODE_INPUTS];    // and which of its outputs
    uint8_t   backSlot[MAX_NODE_INPUTS];   // once resolved: its slot in gBackValue
    uint8_t   backPad[2];

    double    level[MAX_NODE_LEVELS];  // mixer channel levels, then 1.0 for the Chain input(s);
                                       // MixStereo: L and R gain of each channel, interleaved
    uint32_t  levelCount;              // how many of level[] are in use, and so smoothed
    bool      mixStereo;               // mixer: inputs are L/R pairs sharing a channel's level

    tOscWave  wave;                    // oscillator
    bool      oscKbt;
    double    oscCornerLimit;          // §6.3
    double    basePitch;
    double    shape;
    double    shapeModAmount; // §6.7 - the Shape M dial as a word fraction
    double    fmAmount;       // §6.8 - the FM dial through its attenuator curve
    double    rateHz;         // LFO speed
    uint32_t  polarity;       // LFO output range, posStrMap order
    bool      shpWave;        // LFO uses LfoShpA's waveform set rather than the plain one
    bool      lfoMono;        // §42 - reads the shared phase in gLfoMonoPhase, not the voice's own
    double    lfoRateMod;     // §50 - the second rate input's attenuation
    double    lfoKbt;         // §50 - how much of the key's distance from E4 the rate follows
    bool      lfoHasSync;     // §54 - a second output, Snc
    bool      lfoHasReset;    // §28.4 - a Rst input
    int8_t    lfoPhaseSlot;   // §28.4 - the Phase M input, -1 for none
    int8_t    lfoShapeSlot;   // §28.4 - the Shape M input, -1 for none
    double    lfoPhase;       // §28.4 - the read point's offset from the counter, in cycles
    double    lfoPhaseMod;    // §28.4 - Phase M, cycles per input unit
    double    lfoShapeMod;    // §28.4 - Shape M, shape per input unit
    uint8_t   vowel[3];       // §56 - FltVoice's three vowels
    uint8_t   vowelPad[5];

    // The filter's Freq DIAL VALUE (0..127, fractional), not a frequency. Kept in dial units because
    // that is the domain modulation and keyboard tracking act in, and because the dial is itself
    // logarithmic in frequency — see filter_step().
    double          cutoffParam; // filter
    double          resonance;
    uint32_t        extraPoles;
    uint32_t        tapStage;        // which pole is tapped: 0-based, so N poles is tapStage N-1
    tFilterTopology topology;
    tFilterShape    fltShape;        // multi-mode filters only; low-pass for the rest
    double          fltGain;         // FltNord's GC attenuation; 1.0 for every other filter
    double          fltKbt;
    double          fltFmAmount;     // §23.5 - FltNord's FM lin dial
    double          fltResModAmount; // §23.5 - FltNord's Res M dial
    double          modAmount;       // how far the Env input moves the cutoff, 0..2 (the dial's 0..200%)

    double          attack;          // envelope, in seconds
    double          decay;
    double          sustain;         // 0..1
    double          release;
    int32_t         envSustainQ;     // §17.6 - where the bipolar output types centre
    // §17.9 - the stage list this envelope plays, from the map every envelope module shares with its
    // own face. ADSR is the four it always was; the others are however many their map gives.
    tEnvSegment     envStage[ENV_MAX_STAGES];
    uint32_t        envStageCount;
    int32_t         envSustainStage; // the held stage, or -1: a gate release jumps past it
    uint32_t        envOutType;      // §17.6
    bool            envReset;        // §17.7
    bool            envKeyGate;      // §17.4 - the keys gate it: KB on, or a Gate jack fed by a module not played
    bool            envMulti;        // §17.11 - EnvMulti's own segment arithmetic

    double          gain;            // LevAmp
    double          pulseSeconds;    // Pulse gate width, and the logic Delay's time
    double          pulseDial;       // §18.3 - the Time dial and its Range, for a Time Mod input to move
    double          pulseTimeMod;    // the TimeMod dial: dial steps per 64 units of Mod
    uint32_t        pulseRange;
    double          timeSecondsR;    // §65 - DlyStereo's second line
    int32_t         dlyStereoFb[4];  // §65 - FB L, FB R, X-FB L, X-FB R words
    int32_t         metWords[4];     // §66 - Freq base, Freq mod, Colour base, Colour mod
    int32_t         phaseWords[9];   // §67 - FLTPHASE_W_*
    uint32_t        outCount;        // §68.1 - a Sw1-n's outputs before its Ctrl
    double          bx[20];          // §70 - the basic modules' dial values, by module
    double          rndStep;         // §47 - RandomA: the one-pole's coefficient, from Step
    double          rndScale;        // §47 - and the draw's pre-scale, from the same Step
    double          rndStepWord;     // §70.9 - RndClkB: the Step dial's word, for its Step M part
    double          rndStepModWord;  // §70.9 - and Step M's
    bool            rndShiftReg;     // §70.9 - RndClkB's Character Rnd2: the shift-register generator
    double          rndEdge;         // §47 - the Edge word
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
    tOverdriveWords od;              // §71 - OverDrive's Type and Shape words

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
    double          drumDecay[4];     // master, slave, noise, bend
    double          drumLevel[2];     // master, slave
    double          drumNoiseWord;    // §39.10 - the filter coefficient at 96 kHz before the sweep
    double          drumNoiseDamp;
    double          drumSweepSemis;   // at full envelope and velocity
    double          drumBendSemis;    // §39.6 - at full envelope and velocity
    double          drumClick;
    double          drumClickDecay;   // §39.8 - per sample at the engine's rate
    double          drumNoiseLevel;
    // §41 - KeyQuant, in the instrument's words
    int32_t         kqRange;
    int32_t         kqOffset;          // Y[0]
    int32_t         kqThreshold[12];   // X[2..13], a never-reached word for a key that is off
    bool            kqAnyKey;
    // §40 - OscPerc
    double          percDecay;         // the Decay word, per 96 kHz sample
    double          percClick;         // Click squared, as the host sends it
    bool            percPunch;
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

static void keyquant_build(tEngineNode * node, tModule * module, uint32_t variation);
static void random_a_build(tEngineNode * node, tModule * module, uint32_t variation);
static void fltvoice_build(tEngineNode * node, tModule * module, uint32_t variation);

// notes §22
#define MAX_ENGINE_TAPS    (4)

// §14 - one Operator patched into a DXRouter, as the router's node plays it: dials in the instrument's words.
typedef struct {
    bool    present;
    bool    active;
    bool    kbt;
    bool    sync;
    bool    fixed;
    uint8_t lCurve;                // -Lin, -Exp, +Exp, +Lin
    uint8_t rCurve;
    uint8_t rate[4];               // R1-R4
    double  ratio;
    double  fixedHz;
    double  detune;                // a frequency factor
    int32_t level[4];              // L1-L4, kDxLevelWords
    int32_t outLevel;              // Level, as an offset from the top
    int32_t keyVel;                // Vel / 7
    int32_t rateScale;
    int32_t breakPoint;            // a note word
    int32_t lDepth;
    int32_t rDepth;
} tDxOperator;

// §58 - what a step sequencer's parameters make of the shared part's words
typedef struct {
    int32_t table[2 * SEQ_STEPS];   // row 1 then row 2, step by step: the part's Y 7, 8, 9, ...
    int32_t length;                 // the Length drop-down: 0 is one step, 15 sixteen
    uint8_t cycle;                  // the Cycle switch: wrap at the end, rather than stop
    uint8_t bipolar;                // the value row reads (v - 64) units, not v / 2
    uint8_t gate[2];                // each row a gate (held) rather than a trigger (with the clock)
    uint8_t record;                 // §58.1 - SeqNote: the record part follows the 16-step one
    uint8_t controlRate;            // §58 - not up-rated: the part ticks at 24 kHz
    int32_t recordDelay;            // §58.1 - its Y2, per tick of the part
} tSeqConfig;

static void seq_config_build(tSeqConfig * cfg, tModule * module, uint32_t variation);

// §59 - what a ClkGen's dials make of its part's words
typedef struct {
    int32_t tempo;     // the tempo word, floor(BPM x 279.625)
    int32_t sync;      // Sync every: 2^sync beats
    int32_t swing;
    uint8_t active;
    uint8_t pad[3];
} tClkGenConfig;

typedef struct {
    uint32_t      nodeCount;
    int32_t       tap;                           // the node whose output reaches the speakers, -1 for silence
    int32_t       extraTap[MAX_ENGINE_TAPS - 1]; // further Out modules, summed with `tap`
    uint32_t      extraTapCount;
    // Patch-wide settings, from the hidden modules in the Morph location rather than from any module
    // on the canvas. Vibrato is how a patch gets aftertouch vibrato with no LFO in it anywhere.
    uint32_t      vibratoSource; // 0 off, 1 aftertouch, 2 wheel
    double        vibratoCents;
    double        vibratoHz;
    tGlideMode    glideMode;                // patch-wide, not per node
    double        glideSeconds;
    double        octaveSemis;              // §63a - the patch's Octave Shift, in semitones
    double        bendSemitones;            // 0 when the patch has bend switched off
    uint64_t      topology;                 // changes shape => the audio thread resets its per-node state
    uint32_t      voiceCount;               // how many voices this patch may sound at once, 1 for Mono/Legato
    uint64_t      build;                    // which build this is - the velocity table names the one it belongs to
    tEngineNode   node[MAX_ENGINE_NODES];
    tDxOperator   dxOp[MAX_DX_OPERATORS];   // §14 - each DXRouter node's six, from its dxBase
    uint32_t      dxOpCount;
    double        slotGain;                 // §63 - the patch's own Volume and its on switch
    uint32_t      backCount;                // notes §192 - loop-closing legs, each a slot in gBackValue
    tSeqConfig    seq[MAX_SEQ_LINES];       // §58 - each sequencer's step table and the words its switches set
    tClkGenConfig clkGen[MAX_CLKGEN_LINES]; // §59
    int32_t       backSrc[MAX_BACK_EDGES];
    uint32_t      backLeg[MAX_BACK_EDGES];
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

#define VEL_MORPH_LEVELS       (128)  // §26.2 - one row per velocity, so the Vel morph is exact
#define KEY_MORPH_LEVELS       (128)  // §26.2 - one row per note, so the Keyb morph is exact
#define KEY_MORPH_ZERO_NOTE    (36.0) // §26.2 - the Keyb morph is 0 at C1 (note 36) and full five octaves up
#define KEY_MORPH_SPAN         (60.0)
#define MAX_AXIS_LEVELS        (128)
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
#define MORPH_MASK_WORDS    (4u)       // 256 bits, checked against both of the above below

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
#define METER_DECAY    (0.00005)
#define METER_FLOOR    (0.0078125)         // 2^-7, below which the meter law reads 0 (§1.1)
static double               gMeterEnvBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][2];
#define gMeterEnv      (gMeterEnvBank[SE])

static _Atomic int32_t      gOutputGainMilliBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 1000};
static double               gSlotGainNowBank[SOUND_ENGINE_MAX_ENGINES]     = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = -1.0}; // §63
#define gSlotGainNow        (gSlotGainNowBank[SE])
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

// notes §200 - the tempo when the G2's master clock is not known (a lone patch file, no G2)
#define ENGINE_REFERENCE_BPM     (120.0)
#define MASTER_CLOCK_BPM_MIN     (30u)
#define MASTER_CLOCK_BPM_MAX     (240u)

// notes §200 - the tempo every Clk-synced module and a Master-sourced ClkGen follow: the G2's master clock
static double engine_master_bpm(void) {
    uint32_t bpm = gGlobalSettings.masterClock;

    return ((bpm >= MASTER_CLOCK_BPM_MIN) && (bpm <= MASTER_CLOCK_BPM_MAX)) ? (double)bpm : ENGINE_REFERENCE_BPM;
}

// notes §200 - whether the master clock runs; with none known (no G2, no performance) it is taken as running
static bool engine_master_running(void) {
    uint32_t bpm = gGlobalSettings.masterClock;

    return ((bpm >= MASTER_CLOCK_BPM_MIN) && (bpm <= MASTER_CLOCK_BPM_MAX)) ? (gGlobalSettings.masterClockRunning != 0) : true;
}


static double gDeviceRateBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 48000.0};
#define gDeviceRate    (gDeviceRateBank[SE])
static double gSampleRateBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 96000.0};
#define gSampleRate    (gSampleRateBank[SE])

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

static tVoice                 gVoiceBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES];
#define gVoice         (gVoiceBank[SE])
static uint64_t               gVoiceClockBank[SOUND_ENGINE_MAX_ENGINES];
#define gVoiceClock    (gVoiceClockBank[SE])

// notes §31
static _Atomic uint32_t       gEngineVoicesBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = 1};
#define gEngineVoices    (gEngineVoicesBank[SE])

// Whether the patch is in LEGATO voice mode, the one mode where a key played while another is held
// does not restart the envelopes. Published beside gEngineVoices for the same reason: it is read by
// voice_note_on() on the audio thread, per note, where copying the snapshot to ask would be absurd.
static _Atomic bool           gEngineLegatoBank[SOUND_ENGINE_MAX_ENGINES];
#define gEngineLegato    (gEngineLegatoBank[SE])

// Mono OR Legato: the modes where releasing the sounding key goes back to one still held. §15.2
static _Atomic bool           gEngineMonoBank[SOUND_ENGINE_MAX_ENGINES];
#define gEngineMono    (gEngineMonoBank[SE])


// §15.1 - the keys held down, as a count per key. Audio thread only: voice_note_on/off keep it.
#define MIDI_KEY_COUNT    (128)
static uint8_t                gKeyHeldBank[SOUND_ENGINE_MAX_ENGINES][MIDI_KEY_COUNT];
#define gKeyHeld          (gKeyHeldBank[SE])

// §35 - the velocity each held key was played at, as the instrument keeps one beside its held
// count. Audio thread only, like gKeyHeld above.
static uint8_t                gKeyVelocityBank[SOUND_ENGINE_MAX_ENGINES][MIDI_KEY_COUNT];
#define gKeyVelocity              (gKeyVelocityBank[SE])
#define NOTEDET_VELOCITY_SCALE    (128.0)                       // §69.11
static uint8_t                gKeyReleaseVelocityBank[SOUND_ENGINE_MAX_ENGINES][MIDI_KEY_COUNT];
#define gKeyReleaseVelocity       (gKeyReleaseVelocityBank[SE]) // §69.11 - NoteDet's RVel, held from note-off to note-off

// notes §32
static _Atomic uint32_t       gLoadPercentBank[SOUND_ENGINE_MAX_ENGINES];
#define gLoadPercent    (gLoadPercentBank[SE])

static void reset_voices(void);
static uint32_t voice_count_for_patch(uint32_t slot);

static double                 gVibratoPhaseBank[SOUND_ENGINE_MAX_ENGINES];
#define gVibratoPhase        (gVibratoPhaseBank[SE])
static tSoundEngineParams     gLastGoodParamsBank[SOUND_ENGINE_MAX_ENGINES];
#define gLastGoodParams      (gLastGoodParamsBank[SE])
static uint64_t               gSeenTopologyBank[SOUND_ENGINE_MAX_ENGINES];
#define gSeenTopology        (gSeenTopologyBank[SE])

// notes §33
#define OSC_OVERSAMPLE       (4 / ENGINE_OVERSAMPLE)   // the MAXIMUM; gOscOversample is what runs
// notes §34
#define OSC_DECIMATE_TAPS    (48)

// The engine's own output filter, removing everything above the DEVICE's Nyquist before the extra
// samples are dropped. Same windowed-sinc design as the oscillators' — see the note there on why the
// transition width, not the oversampling factor, is what governs the result.
#define OUT_DECIMATE_TAPS    (64)

static double                 gOutDecimateBank[SOUND_ENGINE_MAX_ENGINES][OUT_DECIMATE_TAPS];
#define gOutDecimate         (gOutDecimateBank[SE])
static double                 gOutHistoryBank[SOUND_ENGINE_MAX_ENGINES][4][OUT_DECIMATE_TAPS];      // [pair*2 + channel]; one shared cursor, see the render loop
#define gOutHistory          (gOutHistoryBank[SE])
static uint32_t               gOutHistoryPosBank[SOUND_ENGINE_MAX_ENGINES];
static double                 gOutCouplingBank[SOUND_ENGINE_MAX_ENGINES][4][2];                    // notes §198 - last input, last output
#define gOutCoupling         (gOutCouplingBank[SE])
static double                 gDacStateBank[SOUND_ENGINE_MAX_ENGINES][4];                          // notes §199 - the past input
static double                 gDacStateOutBank[SOUND_ENGINE_MAX_ENGINES][4][2];                    // and two past outputs
#define gDacState            (gDacStateBank[SE])
#define gDacStateOut         (gDacStateOutBank[SE])
static double                 gDacCoefBank[SOUND_ENGINE_MAX_ENGINES][4];                           // b0, b1, a1, a2
static double                 gDacCoefRateBank[SOUND_ENGINE_MAX_ENGINES];
#define gDacCoef             (gDacCoefBank[SE])
#define gDacCoefRate         (gDacCoefRateBank[SE])
static _Atomic bool           gDacEmulationBank[SOUND_ENGINE_MAX_ENGINES] = {[(0) ... SOUND_ENGINE_MAX_ENGINES - 1] = false};
#define gDacEmulation        (gDacEmulationBank[SE])
#define gOutHistoryPos       (gOutHistoryPosBank[SE])

static double                 gOscDecimateBank[SOUND_ENGINE_MAX_ENGINES][OSC_DECIMATE_TAPS];
#define gOscDecimate         (gOscDecimateBank[SE])

// notes §35
static float                  gOscHistoryBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][OSC_DECIMATE_TAPS];
#define gOscHistory       (gOscHistoryBank[SE])
static uint32_t               gOscHistoryPosBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gOscHistoryPos    (gOscHistoryPosBank[SE])

static double                 gPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
// §7.1
static uint32_t               gNoiseSeedBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gNoiseSeed                (gNoiseSeedBank[SE])
static uint32_t               gStartPhaseSeedBank[SOUND_ENGINE_MAX_ENGINES];
#define gStartPhaseSeed           (gStartPhaseSeedBank[SE])
#define START_PHASE_FIRST_SEED    (0x2545F491u)    // notes §63
static double                 gNoiseLpBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gNoiseLp                  (gNoiseLpBank[SE])
#define gPhase                    (gPhaseBank[SE])
static double                 gLfoLastPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoLastPhase             (gLfoLastPhaseBank[SE])
static double                 gLfoMonoPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];                               // §42
#define gLfoMonoPhase             (gLfoMonoPhaseBank[SE])
static double                 gLfoMonoRateBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];                                // §50 - its rate, as a voice last read it
#define gLfoMonoRate              (gLfoMonoRateBank[SE])
static double                 gBackValueBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_BACK_EDGES];                        // notes §192
#define gBackValue                (gBackValueBank[SE])
static double                 gFreqShiftBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_FREQSHIFT_LINES][FREQSHIFT_STATES]; // §57
// §58 - each sequencer's working words, the part's own X and Y memory, per voice
typedef struct {
    int32_t x[SEQ_X_WORDS];
    int32_t y[SEQ_Y_WORDS];
    int32_t rx[SEQREC_X_WORDS];      // §58.1
    int32_t loaded[2 * SEQ_STEPS];   // the table as last copied in: a recorded step stays until its dial moves
    int32_t held[3];                 // §58 - the outputs between a control-rate part's ticks
    double  tick;                    // time to its next tick, in ticks
    bool    ready;
} tSeqState;
static tSeqState              gSeqBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_SEQ_LINES];
#define gSeq    (gSeqBank[SE])
// §59 - each ClkGen's working words per voice, and how many engine samples remain to its next tick
typedef struct {
    int32_t  x[16];
    int32_t  y[16];
    int32_t  out[4];
    uint32_t wait;
    bool     ready;
} tClkGenState;
static tClkGenState           gClkGenBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_CLKGEN_LINES];
#define gClkGen       (gClkGenBank[SE])
#define gFreqShift    (gFreqShiftBank[SE])
// §66 - each MetNoise's six phases and eight high-pass states per voice
typedef struct {
    int32_t phase[6];
    int32_t hp[8];
    bool    ready;
} tMetNoiseState;
static tMetNoiseState         gMetNoiseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_METNOISE_LINES];
// §70 - the basic modules' buffers: post-mix FX (one each), per-voice loops, per-voice filter states
static float                  gFxBufBank[SOUND_ENGINE_MAX_ENGINES][FXBUF_INSTANCES][FXBUF_SAMPLES];
#define gFxBuf         (gFxBufBank[SE])
static uint32_t               gFxBufWriteBank[SOUND_ENGINE_MAX_ENGINES][FXBUF_INSTANCES];
#define gFxBufWrite    (gFxBufWriteBank[SE])
typedef struct {
    int32_t phase;      // §70.2/§70.3 - a signed 24-bit LFO or tap phase, stepped on the 24 kHz tick
    double  tick;       // engine samples since the last 24 kHz tick
    double  delay[2];   // the taps' delays in 96 kHz samples, as the last tick set them
    double  gain[2];    // §70.3 - the taps' crossfade gains
    double  feedback;   // §70.2 - the Flanger's second output, fed back
    double  ratio;      // §70.3 - Scratch's smoothed ratio word
    bool    ready;
} tFxBufState;
static tFxBufState            gFxStateBank[SOUND_ENGINE_MAX_ENGINES][FXBUF_INSTANCES];
#define gFxState    (gFxStateBank[SE])
typedef struct {
    double tick;          // engine samples to the next 96 kHz step
    double pre[2];        // the gate's two-stage follower
    double gate;
    double lp;
    double hp;            // the DC blocker's state word
    double pos[2];        // the positive and negative peak followers
    double neg[2];
    double flip;          // the flip-flop: Period, and what the counter counts
    double count;         // ticks since its last rising edge
    double last;
    double pitch;
} tPitchTracker;
static tPitchTracker          gPitchTrackerBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_PITCH_TRACKER_LINES];
#define gPitchTracker    (gPitchTrackerBank[SE])
static float                  gStringBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_STRING_LINES][STRING_SAMPLES];
#define gString          (gStringBank[SE])
static uint32_t               gStringWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_STRING_LINES];
#define gStringWrite     (gStringWriteBank[SE])
typedef struct {
    double tick;              // engine samples to the next 24 kHz tick
    double ctrlSec[2];        // §70.8 - the 96 kHz converters' section states
    double inSec[4][2];
    double outSec[4][2];
    double ctrlHeld;          // the decimated inputs and the held output
    double inHeld;
    double outHeld;
    double analysis[16][4][2];
    double synthesis[16][4][2];
    double peak[16];          // each band's two-stage follower
    double env[16];
    double emphasisLast;
} tVocoderState;
static tVocoderState          gVocoderBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_BASIC_LINES];
#define gVocoder    (gVocoderBank[SE])
// §69.7 - each DlyClock's ring and write position, per voice
typedef struct {
    double   slot[DLYCLOCK_SLOTS];
    uint32_t write;
} tDlyClockState;
static tDlyClockState         gDlyClockBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DLYCLOCK_LINES];
#define gDlyClock    (gDlyClockBank[SE])
// §67 - each FltPhase's six stages (two words each) and its feedback word, per voice
typedef struct {
    int32_t stage[6][2];
    int32_t feedback;
} tFltPhaseState;
static tFltPhaseState         gFltPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_FLTPHASE_LINES];
#define gFltPhase    (gFltPhaseBank[SE])
#define gMetNoise    (gMetNoiseBank[SE])
static _Thread_local uint32_t sEvalVoice;                                                                                  // the voice eval_node() is on, for signal_in()'s loop legs

// §47 - a Mono RandomA is one generator for every voice
typedef struct {
    double   state[4];
    double   phase;
    double   out;
    uint32_t seed;
    uint32_t pad;
} tRandomAShared;
static tRandomAShared         gRndMonoBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gRndMono     (gRndMonoBank[SE])
static double                 gLfoHeldBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoHeld     (gLfoHeldBank[SE])
static double                 gLfoSlopeBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoSlope    (gLfoSlopeBank[SE])
static int32_t                gLfoSeedBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoSeed     (gLfoSeedBank[SE])
static int32_t                gLfoStepBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLfoStep     (gLfoStepBank[SE])
static double                 gLadderBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][FILTER_STATE_SLOTS];
#define gLadder      (gLadderBank[SE])

// Delay memory. Held as float rather than double purely for size — half a second per line at any
// sensible rate, four lines, is enough for the delays a patch normally has and keeps this under a
// megabyte. Nodes beyond that many run dry rather than sharing a line and smearing into each other.
#define MAX_DELAY_LINES        (4)
// notes §36
#define DELAY_LINE_SAMPLES     (134400 * ENGINE_OVERSAMPLE)
static float                  gDelayLineBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES][DELAY_LINE_SAMPLES];
#define MAX_COMB_LINES         (2)                                                     // FltCombs per patch that sound; any more pass their input dry
#define MAX_CHORUS_FX_LINES    (8)                                                     // StChorus in the FX area: one buffer each
#define MAX_CHORUS_LINES       (2)                                                     // StChorus in the Voice area: one per voice; any more pass dry
#define CHORUS_INSTANCES       (MAX_CHORUS_FX_LINES + (MAX_CHORUS_LINES * MAX_VOICES)) // notes §197
#define COMB_LINE_SAMPLES      (16384)                                                 // a power of two; §13.2's longest delay at a 96 kHz engine is 11,737
#define COMB_FRACTION_STEPS    (512.0)                                                 // §13.4 - the read's coefficient table
#define COMB_FB_MOD_GAIN       (2.0)                                                   // §13.4 - FB Mod x input is taken x 8 in words
static float                  gCombLineBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_COMB_LINES][COMB_LINE_SAMPLES];
#define gCombLine              (gCombLineBank[SE])
static uint32_t               gCombWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_COMB_LINES];
#define gCombWrite             (gCombWriteBank[SE])
static double                 gCombFbBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_COMB_LINES];    // §13.4 - c x the last tap
#define gCombFb                (gCombFbBank[SE])
#define gDelayLine             (gDelayLineBank[SE])
static uint32_t               gDelayWriteBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];
#define gDelayWrite            (gDelayWriteBank[SE])
static double                 gDelayDampBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];
#define gDelayDamp             (gDelayDampBank[SE])
static double                 gDelayHpBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];          // the HP's lowpass half; the filter is x - this
#define gDelayHp               (gDelayHpBank[SE])
static double                 gDelayHpBBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];         // §24.2 - the HP's second state
#define gDelayHpB              (gDelayHpBBank[SE])
static double                 gDelayFbBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES];          // §24.1 - last sample's feedback, written with this one
#define gDelayFb               (gDelayFbBank[SE])
static double                 gDelayModBank[SOUND_ENGINE_MAX_ENGINES][MAX_DELAY_LINES][3];      // §24.6 - FB, wet, dry, from last sample
#define gDelayMod              (gDelayModBank[SE])

// notes §37

#define CHORUS_SAMPLES     (2048 * ENGINE_OVERSAMPLE)
#define CHORUS_CHANNELS    (2)
static float                  gChorusLineBank[SOUND_ENGINE_MAX_ENGINES][CHORUS_INSTANCES][CHORUS_CHANNELS][CHORUS_SAMPLES];
#define gChorusLine        (gChorusLineBank[SE])
static uint32_t               gChorusWriteBank[SOUND_ENGINE_MAX_ENGINES][CHORUS_INSTANCES][CHORUS_CHANNELS];
#define gChorusWrite       (gChorusWriteBank[SE])
static int32_t                gChorusPhaseBank[SOUND_ENGINE_MAX_ENGINES][CHORUS_INSTANCES];
#define gChorusPhase       (gChorusPhaseBank[SE])         // §19.2 - a signed 24-bit LFO phase
static int32_t                gChorusTrimBank[SOUND_ENGINE_MAX_ENGINES][CHORUS_INSTANCES];

// §70.13 - what MIDI has brought in, for CtrlRcv, NoteRcv and Device: rows 0-15 the channels, MIDI_ROW_THIS
// the channel this slot listens on. Written by the MIDI thread, read by the audio thread.
#define MIDI_ROWS        (17u)
#define MIDI_ROW_THIS    (16u)
#define MIDI_ROW_KEYB    (17u)   // a module's Channel "Keyb": the keys that play the voices, as NoteDet
static _Atomic uint8_t        gMidiCcValueBank[SOUND_ENGINE_MAX_ENGINES][MIDI_ROWS][MIDI_KEY_COUNT];
#define gMidiCcValue     (gMidiCcValueBank[SE])
static _Atomic uint32_t       gMidiCcCountBank[SOUND_ENGINE_MAX_ENGINES][MIDI_ROWS][MIDI_KEY_COUNT];
#define gMidiCcCount     (gMidiCcCountBank[SE])
static _Atomic uint8_t        gMidiNoteVelBank[SOUND_ENGINE_MAX_ENGINES][MIDI_ROWS][MIDI_KEY_COUNT];    // 0 while up
#define gMidiNoteVel     (gMidiNoteVelBank[SE])
static _Atomic uint8_t        gMidiNoteRelBank[SOUND_ENGINE_MAX_ENGINES][MIDI_ROWS][MIDI_KEY_COUNT];
#define gMidiNoteRel     (gMidiNoteRelBank[SE])
#define gChorusTrim      (gChorusTrimBank[SE])            // §19.2 - this instance's rate trim
static double                 gChorusTickBank[SOUND_ENGINE_MAX_ENGINES][CHORUS_INSTANCES];
#define gChorusTick      (gChorusTickBank[SE])
static void chorus_reset(uint32_t line);

// Pulse: the countdown still to run, and the previous input, so a rising edge can be seen. Per voice,
// because the gate is fired by that voice's own envelope.
static uint32_t               gPulseCountBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gPulseCount    (gPulseCountBank[SE])
// §36 - one Glide module's slewed output, per voice. `primed` is what makes the FIRST value it
// ever sees arrive whole instead of being glided up from zero.
static double                 gGlideOutBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gGlideOut       (gGlideOutBank[SE])
static bool                   gGlidePrimedBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gGlidePrimed    (gGlidePrimedBank[SE])
static double                 gGlideTickBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gGlideTick      (gGlideTickBank[SE])

// §38 - what a logic module remembers between samples: the edge detectors' last input, the latched
// output, and the divider's count. `logicPrev` holds Clk in bit 0 and the D/S input in bit 1.
static uint8_t                gLogicPrevBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLogicPrev     (gLogicPrevBank[SE])
static bool                   gLogicStateBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLogicState    (gLogicStateBank[SE])
static uint32_t               gLogicCountBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gLogicCount    (gLogicCountBank[SE])

// §39 - one DrumSynth's per-voice state: two phases, four envelopes, the tick accumulator and the
// click. Its noise filter uses gLadder, as every other filter here does.
static double                 gDrumStateBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES][DRUM_STATE_SLOTS];
#define gDrumState    (gDrumStateBank[SE])

static double                 gPulsePrevBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gPulsePrev    (gPulsePrevBank[SE])

// Compressor gain-reduction state, one per node.
static double                 gCompEnvBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gCompEnv             (gCompEnvBank[SE])
static double                 gCompGrBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];       // §25.2 - the smoothed gain reduction
#define gCompGr              (gCompGrBank[SE])
static double                 gCompLimBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];      // §25.2 - the Level limiter
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

static const float            kRvRoomSize[REVERB_TYPE_COUNT] = {0.78f, 0.98f, 1.19f, 1.31f};

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

static double     gEnvLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvLevel             (gEnvLevelBank[SE])
static int32_t    gEnvQBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvQ                 (gEnvQBank[SE])         // §17.3 - the level in the instrument's integers
static double     gEnvTickBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvTick              (gEnvTickBank[SE])
// notes §61 - a changed dial glides linearly to its new value over 128 steps of 94 samples (125 ms at
// 96 kHz), and a new value restarts the glide from wherever it has got to
#define PARAM_RAMP_SAMPLES    (128.0 * 94.0)

typedef struct {
    double value;
    double target;
    double step;    // per engine sample; 0 once the target is reached
} tParamRamp;

static tParamRamp gSmoothShapeBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothShape     (gSmoothShapeBank[SE])
static tParamRamp gSmoothCutoffBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothCutoff    (gSmoothCutoffBank[SE])
static tParamRamp gSmoothResBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothRes       (gSmoothResBank[SE])
static tParamRamp gSmoothGainBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothGain      (gSmoothGainBank[SE])
static tParamRamp gSmoothLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][MAX_NODE_LEVELS];
#define gSmoothLevel     (gSmoothLevelBank[SE])

// Where the per-sample smoothing pass leaves its results, for the voice passes to read. Not per
// voice: a knob is in one place however many notes are sounding, and smoothing it inside the voice
// loop would advance the filter once per voice — so a sweep would speed up as more keys went down.
static double     gSmoothedShapeBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedShape     (gSmoothedShapeBank[SE])
static double     gSmoothedCutoffBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedCutoff    (gSmoothedCutoffBank[SE])
static double     gSmoothedResBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedRes       (gSmoothedResBank[SE])
static double     gSmoothedGainBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothedGain      (gSmoothedGainBank[SE])
static double     gSmoothedLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES][MAX_NODE_LEVELS];
#define gSmoothedLevel     (gSmoothedLevelBank[SE])
// Until a node has been seen once there is nothing to interpolate FROM, so the first sample snaps.
// Also what stops a patch load sweeping every parameter up from whatever the last patch left.
static bool       gSmoothPrimedBank[SOUND_ENGINE_MAX_ENGINES][MAX_ENGINE_NODES];
#define gSmoothPrimed    (gSmoothPrimedBank[SE])

static uint32_t   gEnvStageBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvStage        (gEnvStageBank[SE])

// The voice's trigger count this envelope last started an attack for. When the voice's count moves
// past it, a note-on has asked for a restart that the gate alone cannot show - see envelope_step().
static uint32_t   gEnvTriggerBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gEnvTrigger    (gEnvTriggerBank[SE])

// §14 - per voice, per Operator of every DXRouter node: phase, envelope (its log level, segment and
// the amplitude read from it), and its last output for the feedback loop; per voice and node, the gate
// and trigger last seen. The segments are the instrument's records, in order.
typedef enum {
    eDxRise1 = 0,
    eDxRise2,
    eDxRise3,
    eDxHold,
    eDxRelease,
    eDxIdle
} tDxStage;

static double     gDxPhaseBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS];
#define gDxPhase       (gDxPhaseBank[SE])
static int32_t    gDxLevelBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS];
#define gDxLevel       (gDxLevelBank[SE])
static double     gDxAmpBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS];
#define gDxAmp         (gDxAmpBank[SE])
static uint32_t   gDxEnvStageBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS];
#define gDxEnvStage    (gDxEnvStageBank[SE])
static double     gDxOutBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_DX_OPERATORS][2];
#define gDxOut         (gDxOutBank[SE])
static bool       gDxGateBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gDxGate        (gDxGateBank[SE])
static uint32_t   gDxTriggerBank[SOUND_ENGINE_MAX_ENGINES][MAX_VOICES][MAX_ENGINE_NODES];
#define gDxTrigger     (gDxTriggerBank[SE])

typedef enum {
    eEnvIdle = 0,
    eEnvAttack,
    eEnvDecay,
    eEnvSustain,
    eEnvRelease,
} tEnvStage;

// §70.13 - a controller as it arrived; `listened` says the slot's own channel takes it too
void sound_engine_midi_cc(uint32_t channel, uint32_t controller, uint32_t value, bool listened) {
    SE_LOCAL;

    if ((channel >= 16u) || (controller >= MIDI_KEY_COUNT)) {
        return;
    }
    uint32_t rows[2] = {channel, MIDI_ROW_THIS};

    for (uint32_t r = 0; r < (listened ? 2u : 1u); r++) {
        atomic_store(&gMidiCcValue[rows[r]][controller], (uint8_t)((value > 127u) ? 127u : value));
        atomic_fetch_add(&gMidiCcCount[rows[r]][controller], 1u);
    }
}

// §70.13 - a note as it arrived, on or off with its velocity
void sound_engine_midi_note(uint32_t channel, uint32_t note, uint32_t velocity, bool on, bool listened) {
    SE_LOCAL;

    if ((channel >= 16u) || (note >= MIDI_KEY_COUNT)) {
        return;
    }
    uint32_t rows[2] = {channel, MIDI_ROW_THIS};
    uint8_t  vel     = (uint8_t)((velocity > 127u) ? 127u : ((velocity == 0u) ? 1u : velocity));

    for (uint32_t r = 0; r < (listened ? 2u : 1u); r++) {
        if (on == true) {
            atomic_store(&gMidiNoteVel[rows[r]][note], vel);
        } else {
            atomic_store(&gMidiNoteVel[rows[r]][note], 0u);
            atomic_store(&gMidiNoteRel[rows[r]][note], (uint8_t)((velocity > 127u) ? 127u : velocity));
        }
    }
}

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

void sound_engine_set_dac_emulation(bool on) {
    SE_LOCAL;

    atomic_store(&gDacEmulation, on);
}

bool sound_engine_dac_emulation(void) {
    SE_LOCAL;

    return atomic_load(&gDacEmulation);
}

// notes §199 - Vicanek's matched second-order low-pass: the analogue poles exactly, the zeros chosen
// so the magnitude matches the analogue one at DC, at the corner and at Nyquist
static void dac_filter_design(double rate) {
    SE_LOCAL;

    double w0   = 2.0 * M_PI * DAC_FILTER_HZ / rate;
    double zeta = 1.0 / (2.0 * DAC_FILTER_Q);
    double r    = exp(-zeta * w0);
    double a1   = -2.0 * r * cos(sqrt(1.0 - (zeta * zeta)) * w0);
    double a2   = r * r;
    double A0   = (1.0 + a1 + a2) * (1.0 + a1 + a2);
    double A1   = (1.0 - a1 + a2) * (1.0 - a1 + a2);
    double A2   = -4.0 * a2;
    double phi1 = sin(w0 * 0.5) * sin(w0 * 0.5);
    double phi0 = 1.0 - phi1;
    double phi2 = 4.0 * phi0 * phi1;
    double R1   = ((A0 * phi0) + (A1 * phi1) + (A2 * phi2)) * DAC_FILTER_Q * DAC_FILTER_Q;
    double B1   = (R1 - (A0 * phi0)) / phi1;
    double b0   = 0.5 * (sqrt(A0) + sqrt(fmax(B1, 0.0)));

    gDacCoef[0]  = b0;
    gDacCoef[1]  = sqrt(A0) - b0;
    gDacCoef[2]  = a1;
    gDacCoef[3]  = a2;
    gDacCoefRate = rate;
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
        return GLIDE_LIN_STEP_SCALE * DSP_FULL_SCALE / ticks;    // §36.1 - a constant step
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
            gLfoMonoPhase[i]     = 0.0;
            gLfoMonoRate[i]      = -1.0;
            memset(&gRndMono[i], 0, sizeof(gRndMono[i]));   // §47 - Mono starts from seed 0
            gLfoHeld[v][i]       = 0.0;
            gLfoSlope[v][i]      = 0.0;
            gLfoSeed[v][i]       = 0;
            gLfoStep[v][i]       = 0;
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

            if (i < MAX_BACK_EDGES) {
                gBackValue[v][i] = 0.0;
            }

            if (i < MAX_CLKGEN_LINES) {
                gClkGen[v][i].ready = false;   // §59
            }

            if (i < MAX_SEQ_LINES) {
                gSeq[v][i].ready = false;   // §58 - started afresh, from its own frame, at its next sample
            }

            if (i < MAX_STRING_LINES) {
                memset(gString[v][i], 0, sizeof(gString[v][i]));
                gStringWrite[v][i] = 0;
            }

            if (i < MAX_BASIC_LINES) {
                memset(&gVocoder[v][i], 0, sizeof(gVocoder[v][i]));
            }

            if ((v == 0) && (i < FXBUF_INSTANCES)) {
                memset(gFxBuf[i], 0, sizeof(gFxBuf[i]));
                gFxBufWrite[i] = 0;
                memset(&gFxState[i], 0, sizeof(gFxState[i]));
            }

            if (i < MAX_DLYCLOCK_LINES) {
                memset(&gDlyClock[v][i], 0, sizeof(gDlyClock[v][i]));
            }

            if (i < MAX_FLTPHASE_LINES) {
                memset(&gFltPhase[v][i], 0, sizeof(gFltPhase[v][i]));
            }

            if (i < MAX_METNOISE_LINES) {
                gMetNoise[v][i].ready = false;   // §66 - fresh random phases at its next sample
            }

            if (i < MAX_PITCH_TRACKER_LINES) {
                memset(&gPitchTracker[v][i], 0, sizeof(gPitchTracker[v][i]));
            }

            if (i < MAX_FREQSHIFT_LINES) {
                memset(gFreqShift[v][i], 0, sizeof(gFreqShift[v][i]));
                gFreqShift[v][i][17] = 0.5;   // §57 - the second phase starts a quarter cycle on
            }
            gEnvLevel[v][i]    = 0.0;
            gEnvQ[v][i]        = 0;
            gEnvTick[v][i]     = 0.0;
            gEnvStage[v][i]    = ENV_STAGE_IDLE;
            gEnvTrigger[v][i]  = gVoice[v].trigger;     // nothing pending: idle already attacks on a gate
            gCompEnv[v][i]     = 0.0;
            gPulseCount[v][i]  = 0;
            gPulsePrev[v][i]   = 0.0;
            gGlideOut[v][i]    = 0.0;        // §36
            gGlidePrimed[v][i] = false;
            gGlideTick[v][i]   = 0.0;
            gLogicPrev[v][i]   = 0u;        // §38
            gLogicState[v][i]  = false;
            gLogicCount[v][i]  = 0u;
            memset(gDrumState[v][i], 0, sizeof(gDrumState[v][i]));   // §39
        }
    }

    // §14
    for (v = 0; v < MAX_VOICES; v++) {
        for (i = 0; i < MAX_DX_OPERATORS; i++) {
            gDxPhase[v][i]    = 0.0;
            gDxLevel[v][i]    = 0;
            gDxAmp[v][i]      = 0.0;
            gDxEnvStage[v][i] = eDxRelease;   // §14.2 - where the instrument starts one
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
    memset(gCombFb, 0, sizeof(gCombFb));

    for (i = 0; i < MAX_ENGINE_NODES; i++) {
        gSmoothPrimed[i] = false;
    }

    for (i = 0; i < CHORUS_INSTANCES; i++) {
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
static void split_worker_ensure(void);   // notes §202

static void engine_prime(void) {
    SE_LOCAL;

    build_decimator();
    // notes §65
    gNoteRead = atomic_load(&gNoteWrite);
    reset_node_state();
    reset_voices();
    split_worker_ensure();
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
    split_worker_ensure();

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
    memset(gKeyReleaseVelocity, 0, sizeof(gKeyReleaseVelocity));
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

// §26.2 - the Keyb amount counts the patch's Octave Shift, so the row is the shifted note
static double gKeyMorphShiftBank[SOUND_ENGINE_MAX_ENGINES];
#define gKeyMorphShift    (gKeyMorphShiftBank[SE])

static uint8_t key_row(int32_t note) {
    SE_LOCAL;

    int32_t row = note + (int32_t)lround(gKeyMorphShift);

    row = (row < 0) ? 0 : row;
    return (uint8_t)((row < KEY_MORPH_LEVELS) ? row : (KEY_MORPH_LEVELS - 1));
}

// The Keyb morph's amount is (note - 36)/60 on the instrument, beyond 0..1 at either end (§26.2).
static double axis_amount(tMorphAxis axis, uint32_t row) {
    if (axis == eAxisVelocity) {
        return (double)row / (double)(VEL_MORPH_LEVELS - 1);
    }
    return ((double)row - KEY_MORPH_ZERO_NOTE) / KEY_MORPH_SPAN;
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
    gKeyMorphShift            = params->octaveSemis;
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
    gKeyHeld[note]            = 0;
    gKeyReleaseVelocity[note] = release;

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

// §17.11 - EnvMulti's four segments as the instrument holds them: the attack words for its Shape time
// every segment, a fall also has the decay multiplier, and the held segment and the last do not advance.
#define ENVMULTI_SEGMENTS         (4u)
#define ENVMULTI_PARAM_TIME       (4u)
#define ENVMULTI_PARAM_SUSTAIN    (9u)
#define ENVMULTI_SUSTAIN_NONE     (3u)

static void envmulti_stages_build(tEngineNode * node, tModule * module, uint32_t variation, uint32_t shape) {
    uint32_t sustain = module->param[variation][ENVMULTI_PARAM_SUSTAIN].value;

    for (uint32_t i = 0; i < ENVMULTI_SEGMENTS; i++) {
        tEnvSegment * stage = &node->envStage[i];
        double        level = param_value(module, variation, i);
        double        time  = param_value(module, variation, ENVMULTI_PARAM_TIME + i);

        memset(stage, 0, sizeof(*stage));
        stage->target  = (level >= 127.0) ? ENV_TOP : ((int32_t)level << 16);
        stage->sustain = (uint8_t)((i == sustain) || (i == (ENVMULTI_SEGMENTS - 1u)));
        stage->modLeg  = -1;
        stage->decay   = env_q23(exp(-ENV_FALL_SHARPNESS / env_ticks(env_time_seconds(time))));
        env_stage_rates(stage, env_attack_seconds(time, shape), shape, true);
    }

    node->envStageCount   = ENVMULTI_SEGMENTS;
    node->envSustainStage = (sustain < ENVMULTI_SUSTAIN_NONE) ? (int32_t)sustain : -1;
    node->envMulti        = true;
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

    if (module->type == moduleTypeEnvMulti) {
        envmulti_stages_build(node, module, variation, map.shape);
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
        case eNodeSw1to8:
        {
            return SW1TO8_OUTS + 1u;   // §45 - Out 1..8 and Ctrl
        }
        case eNodeMux1to8:             // §68.3
        case eNodeCounter8:            // §68.6
        case eNodeBinCounter:
        case eNodeADConv:              // §68.7
        case eNodeDlyShiftReg:         // §69.6
        {
            return 8u;
        }
        case eNodeNoteDet:             // §69.11 - Gate, Vel, RVel
        case eNodePitchTrack:          // §70.7 - Period, Pitch, Gate
        case eNodeStatus:              // §70.13
        {
            return 3u;
        }
        case eNodeIn4Bus:              // §69.12 - four outputs
        {
            return 4u;
        }
        case eNodeMultiTap:            // §70.1 - up to eight taps
        case eNodeDevice:              // §70.13 - seven controls
        {
            return 8u;
        }
        case eNodeSeq16:
        {
            return 3u;                 // §58 - Link, the value row, the other row
        }
        case eNodeClkGen:
        {
            return 4u;                 // §59 - 1/96, 1/16, ClkActive, Sync
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
static void basic_build(tEngineNode * node, tModule * module, uint32_t variation);                                                                // §70
static void noise_gate_build(tEngineNode * node, tModule * module, uint32_t variation);                                                           // §70.6
static int32_t follower_coef_word(double seconds);                                                                                                // §69.4
static int32_t dial_mod_word(double dial);                                                                                                        // §67
static int32_t dly_dial_word(double dial);                                                                                                        // §24.2
static double flt_nord_mod_amount(double dial);                                                                                                   // §23.5
static void comp_words(tEngineNode * node, double thrDial, double ratioDial, double atkDial, double relDial, double lvlDial);                     // §25.1

// §17.4 - each envelope's KB (keyboard gate) parameter, or -1 where the module has none
static int env_kb_param(tModuleType type) {
    switch (type) {
        case moduleTypeEnvADDSR:  return 0;

        case moduleTypeEnvADSR:
        case moduleTypeEnvADR:
        case moduleTypeEnvAHD:    return 6;

        case moduleTypeModAHD:    return 7;

        case moduleTypeModADSR:   return 9;

        case moduleTypeEnvMulti:  return 11;

        default:                  return -1;
    }
}

// §17.4 - each envelope's Normal/Reset parameter, or -1 where the module has none
static int env_reset_param(tModuleType type) {
    switch (type) {
        case moduleTypeEnvADR:    return 2;

        case moduleTypeEnvAHD:    return 3;

        case moduleTypeEnvADSR:   return 7;

        case moduleTypeEnvMulti:  return 8;

        case moduleTypeEnvADDSR:  return 10;

        default:                  return -1;
    }
}

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
        case moduleTypeOscPerc:
        {
            *kind = eNodeOscPerc;
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
        case moduleTypeSwOnOffM:     // §68.1 - the same part; the button is held rather than latched
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
        case moduleTypeSw2to1M:      // §68.1
        case moduleTypeSw4to1:       // §68.1
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
            // §37 - the jacks on the back are silent here; §61 - a bus is the Voice area's own 2-Outs,
            // bridged like the FX input (twoToInSourceStrMap: In 1/2, In 3/4, Bus 1/2, Bus 3/4)
            uint32_t source = module->param[gPatchDescr[module->key.slot].activeVariation][0].value;

            *kind = (source >= 2u) ? eNodeFxIn : eNodeAudioIn;
            return true;
        }
        case moduleType4toIn:
        {
            // §69.12 - the jacks are silent here; Bus is both buses, bridged as 2-In's are (fourToInSourceStrMap)
            uint32_t source = module->param[gPatchDescr[module->key.slot].activeVariation][0].value;

            *kind = (source == 1u) ? eNodeIn4Bus : eNodeAudioIn;
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
        case moduleTypeSandH:
        {
            *kind = eNodeSandH;   // §38.5
            return true;
        }
        case moduleTypeConstSwT:
        case moduleTypeConstSwM:     // §68.1
        {
            *kind = eNodeConstant;   // §44 - a Constant with an on/off switch
            return true;
        }
        case moduleTypeMinMax:
        {
            *kind = eNodeMinMax;     // §43
            return true;
        }
        case moduleTypeSw1to8:
        case moduleTypeSw1to2:       // §68.1
        case moduleTypeSw1to2M:      // §68.1
        case moduleTypeSw1to4:       // §68.1
        {
            *kind = eNodeSw1to8;     // §45
            return true;
        }
        case moduleTypeValSw1to2:
        {
            *kind = eNodeValSw12;    // §68.2
            return true;
        }
        case moduleTypeMux8to1:
        {
            *kind = eNodeMux8to1;    // §68.3
            return true;
        }
        case moduleTypeMux1to8:
        {
            *kind = eNodeMux1to8;    // §68.3
            return true;
        }
        case moduleTypeTandH:
        {
            *kind = eNodeTandH;      // §68.4
            return true;
        }
        case moduleTypeWindSw:
        {
            *kind = eNodeWindSw;     // §68.5
            return true;
        }
        case moduleType8Counter:
        {
            *kind = eNodeCounter8;   // §68.6
            return true;
        }
        case moduleTypeBinCounter:
        {
            *kind = eNodeBinCounter; // §68.6
            return true;
        }
        case moduleTypeADConv:
        {
            *kind = eNodeADConv;     // §68.7
            return true;
        }
        case moduleTypeDAConv:
        {
            *kind = eNodeDAConv;     // §68.7
            return true;
        }
        case moduleTypeRed2Blue:
        case moduleTypeBlue2Red:
        {
            *kind = eNodeRatePass;   // §68.8
            return true;
        }
        case moduleTypeCompSig:
        {
            *kind = eNodeCompSig;    // §69.2
            return true;
        }
        case moduleTypeLevMod:
        {
            *kind = eNodeLevMod;     // §69.3
            return true;
        }
        case moduleTypeEnvFollow:
        {
            *kind = eNodeEnvFollow;  // §69.4
            return true;
        }
        case moduleTypePartQuant:
        {
            *kind = eNodePartQuant;  // §69.5
            return true;
        }
        case moduleTypeDlyShiftReg:
        {
            *kind = eNodeDlyShiftReg; // §69.6
            return true;
        }
        case moduleTypeDlyClock:
        {
            *kind = eNodeDlyClock;   // §69.7
            return true;
        }
        case moduleTypeDigitizer:
        {
            *kind = eNodeDigitizer;  // §69.8
            return true;
        }
        case moduleTypeWahWah:
        {
            *kind = eNodeWahWah;     // §69.9
            return true;
        }
        case moduleTypeRndClkB:     // §70.9 - RndClkA's node, its dials where RndClkB keeps them
        {
            *kind = eNodeRndClkA;
            return true;
        }
        case moduleTypeDelayDual:
        case moduleTypeDelayQuad:
        case moduleTypeDlyEight:
        {
            *kind = eNodeMultiTap;   // §70.1
            return true;
        }
        case moduleTypeFlanger:
        {
            *kind = eNodeFlanger;   // §70.2
            return true;
        }
        case moduleTypePShift:
        case moduleTypeScratch:
        {
            *kind = eNodePShift;   // §70.3
            return true;
        }
        case moduleTypeOscString:
        {
            *kind = eNodeOscString;   // §70.4
            return true;
        }
        case moduleTypeResonator:
        {
            *kind = eNodeResonator;   // §70.4
            return true;
        }
        case moduleTypeDriver:
        {
            *kind = eNodeDriver;   // §70.5
            return true;
        }
        case moduleTypeNoiseGate:
        {
            *kind = eNodeNoiseGate;   // §70.6
            return true;
        }
        case moduleTypePitchTrack:
        case moduleTypeZeroCnt:
        {
            *kind = eNodePitchTrack;   // §70.7
            return true;
        }
        case moduleTypeVocoder:
        {
            *kind = eNodeVocoder;   // §70.8
            return true;
        }
        case moduleTypeRndPattern:
        {
            *kind = eNodeRndPattern;   // §70.9
            return true;
        }
        case moduleTypeSeqCtr:
        {
            *kind = eNodeSeqCtr;   // §70.10
            return true;
        }
        case moduleTypeMux8to1X:
        {
            *kind = eNodeMux8to1X;   // §70.11
            return true;
        }
        case moduleTypeLevScaler:
        {
            *kind = eNodeLevScaler;   // §70.12
            return true;
        }
        case moduleTypeStatus:
        {
            *kind = eNodeStatus;   // §70.13
            return true;
        }
        case moduleTypeDevice:
        {
            *kind = eNodeDevice;   // §70.13
            return true;
        }
        case moduleTypeCtrlRcv:
        {
            *kind = eNodeCtrlRcv;   // §70.13
            return true;
        }
        case moduleTypeCtrlSend:
        case moduleTypePCSend:
        case moduleTypeAutomate:
        case moduleTypeNoteZone:
        {
            *kind = eNodeSink;   // §70.13
            return true;
        }
        case moduleTypeNoteDet:
        case moduleTypeNoteRcv:      // §70.13 - NoteDet's, whatever the channel
        {
            *kind = eNodeNoteDet;    // §69.11
            return true;
        }
        case moduleTypeDelay:
        {
            *kind = eNodeLogicDelay; // §46
            return true;
        }
        case moduleTypeRandomA:
        case moduleTypeRandomB:      // §69.10 - RandomA's parts, with rate inputs and Kbt
        {
            *kind = eNodeRandomA;    // §47
            return true;
        }
        case moduleTypeCompLev:
        {
            *kind = eNodeCompLev;    // §48
            return true;
        }
        case moduleTypeNoteQuant:
        {
            *kind = eNodeNoteQuant;  // §49
            return true;
        }
        case moduleTypeOscMaster:
        {
            *kind = eNodeOscMaster;  // §51
            return true;
        }
        case moduleTypeClkGen:
        {
            *kind = eNodeClkGen;     // §59
            return true;
        }
        case moduleTypeNoteScaler:
        {
            *kind = eNodeNoteScaler; // §60
            return true;
        }
        case moduleTypeNoteSend:
        {
            *kind = eNodeNoteSend;   // §62
            return true;
        }
        case moduleTypeRndClkA:
        {
            *kind = eNodeRndClkA;    // §64
            return true;
        }
        case moduleTypeRndTrig:
        {
            *kind = eNodeRndTrig;    // §64
            return true;
        }
        case moduleTypeDlyStereo:
        {
            *kind = eNodeDlyStereo;  // §65
            return true;
        }
        case moduleTypeMetNoise:
        {
            *kind = eNodeMetNoise;   // §66
            return true;
        }
        case moduleTypeFltPhase:
        {
            *kind = eNodeFltPhase;   // §67
            return true;
        }
        case moduleTypeSeqVal:
        case moduleTypeSeqLev:       // §69.1 - SeqVal's part and laws
        case moduleTypeSeqNote:
        case moduleTypeSeqEvent:
        {
            *kind = eNodeSeq16;      // §58
            return true;
        }
        case moduleTypeFreqShift:
        {
            *kind = eNodeFreqShift;  // §57
            return true;
        }
        case moduleTypeFltVoice:
        {
            *kind = eNodeFltVoice;   // §56
            return true;
        }
        case moduleTypePhaser:
        {
            *kind = eNodePhaser;     // §55
            return true;
        }
        case moduleTypeOscPM:
        {
            *kind = eNodeOscPM;      // §53
            return true;
        }
        case moduleTypeDlySingleA:
        case moduleTypeDlySingleB:
        {
            *kind = eNodeDlySingle;  // §52
            return true;
        }
        case moduleTypeKeyQuant:
        {
            *kind = eNodeKeyQuant;   // §41
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

// §6.7 - the oscillators carrying the instrument's shape-modulation part: their Shape Mod jack, which is
// the fifth input on all three, and the Shape M dial that scales it
typedef struct {
    tModuleType type;
    uint32_t    shapeModParam;
} tShapeModSpec;

#define SHAPE_MOD_INPUT    (4u)
#define SHAPE_WORD_MAX     (8388607.0 / 8388608.0)
#define SHAPE_WORD_MIN     (-1.0)

static const tShapeModSpec kShapeModSpec[] = {
    {moduleTypeOscB,    7u},
    {moduleTypeOscShpA, 8u},
    {moduleTypeOscShpB, 7u},
};

// §6.8 - the oscillators carrying the instrument's linear-FM part: the FM jack's input number, the
// FM amount dial and the FM Lin/Trk menu, in the patch's own parameter order
typedef struct {
    tModuleType type;
    uint32_t    fmInput;
    uint32_t    fmAmountParam;
    uint32_t    fmTypeParam;
} tFmSpec;

static const tFmSpec       kFmSpec[]       = {
    {moduleTypeOscB,    3u, 5u, 10u},
    {moduleTypeOscC,    2u, 4u,  6u},
    {moduleTypeOscShpA, 3u, 5u,  6u},
    {moduleTypeOscShpB, 3u, 5u,  9u},
};

static const tFmSpec * fm_spec(tModuleType moduleType) {
    for (uint32_t i = 0; i < (sizeof(kFmSpec) / sizeof(kFmSpec[0])); i++) {
        if (kFmSpec[i].type == moduleType) {
            return &kFmSpec[i];
        }
    }

    return NULL;
}

static int fm_connector_index(tModuleType moduleType) {
    const tFmSpec * spec = fm_spec(moduleType);

    return (spec != NULL) ? connector_index_for_input(moduleType, spec->fmInput, anyConnectorType) : -1;
}

static const tShapeModSpec * shape_mod_spec(tModuleType moduleType) {
    for (uint32_t i = 0; i < (sizeof(kShapeModSpec) / sizeof(kShapeModSpec[0])); i++) {
        if (kShapeModSpec[i].type == moduleType) {
            return &kShapeModSpec[i];
        }
    }

    return NULL;
}

static int shape_mod_connector_index(tModuleType moduleType) {
    return (shape_mod_spec(moduleType) != NULL)
           ? connector_index_for_input(moduleType, SHAPE_MOD_INPUT, anyConnectorType) : -1;
}

// §6.7 - the Shape M dial as the part's word: v/128, with 127 counting as full
static double shape_mod_amount(tModule * module, uint32_t variation) {
    const tShapeModSpec * spec = shape_mod_spec(module->type);

    return (spec != NULL) ? wave_shape_word(param_value(module, variation, spec->shapeModParam) / 127.0) : 0.0;
}

// §6.8 - the FM dial through the attenuator curve the part's word takes, and the Lin/Trk menu
static void set_osc_fm(tEngineNode * node, tModule * module, uint32_t variation) {
    const tFmSpec * spec = fm_spec(module->type);

    node->fmAmount = (spec != NULL) ? type_ii_attenuator(param_value(module, variation, spec->fmAmountParam)) : 0.0;
    node->fmTrack  = (spec != NULL) && (module->param[variation][spec->fmTypeParam].value != 0u);
}

// §28.4 - the counter part: the Phase dial is a word of v/64 on top of a per-waveform offset, Phase M
// v/128 (127 full) times four, Shape M v/128 times eight. A word is half a cycle; a unit a quarter word.
static void lfo_phase_build(tEngineNode * node, tModule * module, uint32_t variation, const tLfoParams * p) {
    bool isB    = (module->type == moduleTypeLfoB);
    bool isShpA = (module->type == moduleTypeLfoShpA);

    node->lfoHasReset  = isB || isShpA;
    node->lfoPhaseSlot = isB ? (int8_t)LFOB_IN_PHASE_MOD : (isShpA ? (int8_t)LFOSHPA_IN_PHASE_MOD : (int8_t)-1);
    node->lfoShapeSlot = isShpA ? (int8_t)LFOSHPA_IN_SHAPE_MOD : (int8_t)-1;
    node->lfoPhase     = (p->phase >= 0) ? (param_value(module, variation, (uint32_t)p->phase) / 128.0) : 0.0;
    node->lfoPhaseMod  = (p->phaseMod >= 0)
                         ? (wave_shape_word(param_value(module, variation, (uint32_t)p->phaseMod) / 127.0) / 2.0) : 0.0;
    node->lfoShapeMod  = (p->shapeMod >= 0)
                         ? wave_shape_word(param_value(module, variation, (uint32_t)p->shapeMod) / 127.0) : 0.0;

    // LfoB's Sine and Tri read half a word behind LfoA's for the same counter; its Saw and Sqr do not
    if (isB && ((uint32_t)node->wave < 2u)) {
        node->lfoPhase -= 0.25;
    }
}

// §6.6 - the connector index of a module's Sync input, -1 when it has none
static int sync_connector_index(tModuleType moduleType) {
    uint32_t index = 0;

    for (uint32_t entry = 0; entry < array_size_connector_location_list(); entry++) {
        const tConnectorLocation * loc = &connectorLocationList[entry];

        if (loc->moduleType != moduleType) {
            continue;
        }

        if ((loc->direction == connectorDirIn) && (loc->label != NULL) && (strcmp(loc->label, "Sync") == 0)) {
            return (int)index;
        }
        index++;
    }

    return -1;
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

            // §23.5 - FltNord: PitchVar, Pitch, FM lin and Res, in that order
            if (moduleType == moduleTypeFltNord) {
                *connectors = derived;
                return inputs_in_module_order(moduleType, 5, derived);
            }

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
            // §6.3; §6.6 - the Sync jack, where there is one, follows; §6.7 - then Shape Mod; §6.8 - then FM
            static const uint32_t oscCIn[]    = {3, 0};
            static const uint32_t oscDualIn[] = {0, 1, 3, 4};
            const uint32_t *      base        = oscIn;
            uint32_t              count       = (moduleType == moduleTypeOscD) ? 1u : 2u;
            int                   sync        = sync_connector_index(moduleType);
            int                   shapeMod    = shape_mod_connector_index(moduleType);
            int                   fm          = fm_connector_index(moduleType);

            if (moduleType == moduleTypeOscDual) {
                base  = oscDualIn;
                count = 4u;
            } else if (moduleType == moduleTypeOscC) {
                base  = oscCIn;
                count = 2u;
            }
            memcpy(derived, base, count * sizeof(uint32_t));

            if (sync >= 0) {
                derived[count++] = (uint32_t)sync;
            }

            if (shapeMod >= 0) {
                derived[count++] = (uint32_t)shapeMod;
            }

            if (fm >= 0) {
                derived[count++] = (uint32_t)fm;
            }
            *connectors = derived;
            return count;
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
        case eNodeCompress:         // §25.2 - In L, In R, then the side-chain
        {
            *connectors = derived;
            return inputs_in_module_order(moduleType, 3u, derived);
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
        case eNodeIn4Bus:
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
        case eNodeKeyQuant:         // §41 - In
        case eNodeSandH:            // §38.5 - In, Ctrl
        case eNodeMinMax:           // §43 - A, B
        case eNodeSw1to8:           // §45 - In
        case eNodeLogicDelay:       // §46 - In, then the time Mod input
        case eNodeRandomA:          // §47 - Pitch, which moves the rate as an LFO's does (§50)
        case eNodeOscMaster:        // §51 - Pitch, PitchVar
        case eNodeDlySingle:        // §52 - In, then B's Time mod
        case eNodeOscPM:            // §53 - PitchVar, Sync, Phase M, Pitch
        case eNodeFltVoice:         // §56 - In, Vowel, FreqMod
        case eNodeFreqShift:        // §57 - Mod, In
        case eNodeSeq16:            // §58 - Clk, Rst, Loop, Park, then the two rows' inputs
        case eNodeClkGen:           // §59 - Rst
        case eNodeNoteScaler:       // §60 - In
        case eNodeNoteSend:         // §62 - Gate, Vel, Note
        case eNodeRndClkA:          // §64 - Clk, Rst, Seed
        case eNodeRndTrig:          // §64 - Clk, Rst, Seed, Prob
        case eNodeDlyStereo:        // §65 - In
        case eNodeMetNoise:         // §66 - FreqMod, ColourMod
        case eNodeFltPhase:         // §67 - In, PitchVar, Spr, FM, Pitch
        case eNodeValSw12:          // §68.2 - In, Ctrl
        case eNodeMux8to1:          // §68.3 - In 1..8, Ctrl
        case eNodeMux1to8:          // §68.3 - In, Ctrl
        case eNodeTandH:            // §68.4 - In, Ctrl
        case eNodeWindSw:           // §68.5 - In, Ctrl
        case eNodeCounter8:         // §68.6 - Clk, Rst
        case eNodeBinCounter:
        case eNodeADConv:           // §68.7 - In
        case eNodeDAConv:           // §68.7 - D0..D7
        case eNodeRatePass:         // §68.8 - In
        case eNodeCompSig:          // §69.2 - A, B
        case eNodeLevMod:           // §69.3 - In, Mod, ModDepth
        case eNodeEnvFollow:        // §69.4 - In
        case eNodePartQuant:        // §69.5 - In
        case eNodeDlyShiftReg:      // §69.6 - In, Clk
        case eNodeDlyClock:         // §69.7 - In, Clk
        case eNodeDigitizer:        // §69.8 - In, Rate mod
        case eNodeWahWah:           // §69.9 - In, Sweep
        case eNodeMultiTap:
        case eNodeFlanger:
        case eNodePShift:
        case eNodeOscString:
        case eNodeResonator:
        case eNodeDriver:
        case eNodeNoiseGate:
        case eNodePitchTrack:
        case eNodeVocoder:
        case eNodeRndPattern:
        case eNodeSeqCtr:
        case eNodeMux8to1X:
        case eNodeLevScaler:
        case eNodeStatus:
        case eNodeDevice:
        case eNodeCtrlRcv:
        case eNodeSink:
        case eNodeCompLev:          // §48 - A
        case eNodeNoteQuant:        // §49 - In
        case eNodeFlipFlop:         // §38.3 - Clk, Rst, In
        case eNodeClkDiv:           // §38.4 - Clk, Rst
        case eNodeDrumSynth:        // §39 - Trig, Vel, Pitch
        {
            *connectors = derived;
            return inputs_in_module_order(moduleType, MAX_NODE_INPUTS, derived);
        }
        case eNodeFltMulti:
        case eNodeOscNoise:
        case eNodeOscPerc:          // §40 - Pitch, PitchVar, Trig
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
            // One jack or two, in the module's OWN order: In first and Mod second, and ShpStatic and
            // Rect have no Mod jack at all. Asking
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
        case eNodePhaser:           // §55 - In
        {
            *connectors = oneIn;
            return 1;
        }
        case eNodeLfo:              // §50 - the rate inputs: fixed, then the one Rate M scales
        {
            uint32_t count = inputs_in_module_order(moduleType, LFO_MAX_INPUTS, derived);
            *connectors = derived;
            return count;
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

// notes §75 - EVERY Voice-area output on the bus, since the G2 sums them; up to `max`
static uint32_t voice_area_outputs_for_fx(uint32_t slot, uint32_t wantedBus, tModule ** found, uint32_t max) {
    uint32_t index = 0;
    uint32_t count = 0;

    for (index = 0; index < MAX_NUM_MODULES; index++) {
        tModule * module      = get_module_slot(slot, (uint32_t)locationVa, index);

        if (module == NULL) {
            continue;
        }
        uint32_t  variation   = gPatchDescr[slot].activeVariation;
        uint32_t  destination = module->param[variation][OUT_PARAM_DESTINATION].value;

        if (module->type == moduleType2toOut) {
            // FX 1/2 is destination 2 and FX 3/4 is 3, so the bus index is the destination less 2.
            if ((destination >= 2) && ((destination - 2) == wantedBus) && (count < max)) {
                found[count++] = module;
            }
        } else if (module->type == moduleType4toOut) {
            // A 4-Out has one "Fx" setting covering all four channels, so it feeds both pairs.
            if ((destination == 1) && (count < max)) {
                found[count++] = module;
            }
        }
    }

    return count;
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

#define OSCDUAL_PARAM_SQUARE_LEVEL    (5)     // §12.1
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
#define OP_PARAM_KEYVEL             (6)
#define OP_PARAM_RATESCALE          (7)
#define OP_PARAM_R1                 (8)      // R1 L1 R2 L2 R3 L3 R4 L4 run from here
#define OP_PARAM_BRPT               (17)
#define OP_PARAM_LCURVE             (18)
#define OP_PARAM_LDEPTH             (19)
#define OP_PARAM_RCURVE             (20)
#define OP_PARAM_RDEPTH             (21)
#define OP_PARAM_LEVEL              (22)
#define OP_PARAM_ACTIVE             (23)
#define OP_SEVENTH_WORD             (0x124924)   // §14.4 - Vel as sevenths
#define OP_RATESCALE_WORD           (0x84210)
#define OP_DEPTH_WORD               (0xffff)     // L-Depth and R-Depth, 0-99
#define OP_NOTE_WORD                (0x8000)     // one semitone of a Note input
#define OP_BREAKPOINT_E4            (47)         // the BrPt value at E4

// §14.2 - L1-L4 and Level as a log level: 0x10000 is one step of kDxAmpWords. 100-127, which the G2
// accepts, are the instrument's own words past the DX range, read by their low 24 bits (dx_word24())
static const uint32_t kDxLevelWords[128] = {
    0x00000000, 0x0003ffff, 0x0007ffff, 0x000bffff, 0x000fffff, 0x0013ffff, 0x0015ffff, 0x0017ffff,
    0x0019ffff, 0x001bffff, 0x001dffff, 0x001fffff, 0x0021ffff, 0x0023ffff, 0x0025ffff, 0x0027ffff,
    0x00296f96, 0x002adf2d, 0x002c4ec4, 0x002dbe5b, 0x002f2df2, 0x00309d89, 0x0031a3b6, 0x0032a9e4,
    0x0033b011, 0x0034b63e, 0x0035bc6c, 0x0036c299, 0x0037c8c6, 0x0038cef4, 0x0039d521, 0x003adb4e,
    0x003be17c, 0x003ce7a9, 0x003dedd6, 0x003ef404, 0x003ffa31, 0x0041005f, 0x0042068c, 0x00430cb9,
    0x004412e7, 0x00451914, 0x00461f41, 0x0047256f, 0x00482b9c, 0x004931c9, 0x004a37f7, 0x004b3e24,
    0x004c4451, 0x004d4a7f, 0x004e50ac, 0x004f56d9, 0x00505d07, 0x00516334, 0x00526961, 0x00536f8f,
    0x005475bc, 0x00557be9, 0x00568217, 0x00578844, 0x00588e71, 0x0059949f, 0x005a9acc, 0x005ba0fa,
    0x005ca727, 0x005dad54, 0x005eb382, 0x005fb9af, 0x0060bfdc, 0x0061c60a, 0x0062cc37, 0x0063d264,
    0x0064d892, 0x0065debf, 0x0066e4ec, 0x0067eb1a, 0x0068f147, 0x0069f774, 0x006afda2, 0x006c03cf,
    0x006d09fc, 0x006e102a, 0x006f1657, 0x00701c84, 0x007122b2, 0x007228df, 0x00732f0c, 0x0074353a,
    0x00753b67, 0x00764195, 0x007747c2, 0x00784def, 0x0079541d, 0x007a5a4a, 0x007b6077, 0x007c66a5,
    0x007d6cd2, 0x007e72ff, 0x007f397f, 0x007fffff, 0x02bb0cf8, 0x015d867c, 0x00e90453, 0x00aec33e,
    0x008bcf65, 0x00748229, 0x0063dd48, 0x0057619f, 0x004dac1c, 0x0045e7b2, 0x003f8cd1, 0x003a4115,
    0x0035c5ec, 0x0031eea4, 0x002e9a77, 0x002bb0d0, 0x00291ee1, 0x0026d60e, 0x0024caca, 0x0022f3d9,
    0x002149c3, 0x001fc668, 0x001e64bd, 0x001d208a, 0x001bf647, 0x001ae2f6, 0x0019e409, 0x0018f752,
};


// §14.2 - amplitude at each whole log level, 0x200000 full scale; read between by the low 16 bits
static const int32_t  kDxAmpWords[129] = {
    0x000000, 0x000048, 0x00004e, 0x000055, 0x00005c, 0x000064, 0x00006c, 0x000075,
    0x00007f, 0x00008a, 0x000096, 0x0000a2, 0x0000b0, 0x0000bf, 0x0000cf, 0x0000e0,
    0x0000f3, 0x000108, 0x00011e, 0x000136, 0x000151, 0x00016d, 0x00018c, 0x0001ad,
    0x0001d1, 0x0001f9, 0x000223, 0x000251, 0x000283, 0x0002b9, 0x0002f4, 0x000334,
    0x000379, 0x0003c4, 0x000415, 0x00046d, 0x0004cd, 0x000535, 0x0005a5, 0x00061f,
    0x0006a3, 0x000732, 0x0007cd, 0x000875, 0x00092c, 0x0009f2, 0x000ac8, 0x000bb1,
    0x000cad, 0x000dbe, 0x000ee7, 0x001028, 0x001185, 0x0012ff, 0x001498, 0x001655,
    0x001836, 0x001a41, 0x001c77, 0x001edd, 0x002176, 0x002448, 0x002757, 0x002aa7,
    0x002e3f, 0x003225, 0x00365e, 0x003af3, 0x003fea, 0x00454d, 0x004b24, 0x005178,
    0x005856, 0x005fc7, 0x0067d9, 0x007098, 0x007a15, 0x00845e, 0x008f85, 0x009b9c,
    0x00a8b9, 0x00b6f0, 0x00c659, 0x00d70f, 0x00e92e, 0x00fcd3, 0x011220, 0x012938,
    0x014243, 0x015d69, 0x017ada, 0x019ac5, 0x01bd60, 0x01e2e6, 0x020b95, 0x0237b2,
    0x026786, 0x029b62, 0x02d39c, 0x031093, 0x0352ad, 0x039a58, 0x03e80d, 0x043c4e,
    0x0497a9, 0x04fab5, 0x05661b, 0x05da8c, 0x0658cd, 0x06e1b0, 0x07761d, 0x08170a,
    0x08c586, 0x0982b6, 0x0a4fd6, 0x0b2e3e, 0x0c1f63, 0x0d24d9, 0x0e4056, 0x0f73b5,
    0x10c0fa, 0x122a53, 0x13b21e, 0x155aea, 0x172781, 0x191ae6, 0x1b385d, 0x1d8373,
    0x200000,
};

// §14.2 - a rising segment's step per tick at each rate
static const int32_t  kDxAttackWords[100]         = {
    0x000057, 0x00005e, 0x000066, 0x000070, 0x00007a, 0x000087, 0x000096, 0x0000a7,
    0x0000bc, 0x0000d4, 0x0000f0, 0x000112, 0x000139, 0x000167, 0x00019a, 0x0001d2,
    0x000209, 0x000241, 0x000280, 0x0002c6, 0x000315, 0x00036d, 0x0003d0, 0x000441,
    0x0004c1, 0x000553, 0x0005fe, 0x0006c9, 0x0007b6, 0x0008b9, 0x0009b7, 0x000ab4,
    0x000bd3, 0x000d16, 0x000e85, 0x001022, 0x0011f3, 0x0013fc, 0x00163f, 0x0018b8,
    0x001b63, 0x001e5f, 0x0021e9, 0x00261a, 0x002b13, 0x0030f0, 0x0037c8, 0x003f95,
    0x004822, 0x0050e2, 0x0058d1, 0x006052, 0x0068bf, 0x00723f, 0x007d00, 0x008937,
    0x009723, 0x00a712, 0x00b95d, 0x00ce70, 0x00e6c7, 0x0102ef, 0x012389, 0x014938,
    0x01749b, 0x01a627, 0x01ddf6, 0x021b79, 0x025d0c, 0x029f8f, 0x02de26, 0x031c10,
    0x036131, 0x03ae95, 0x040574, 0x04673d, 0x04d598, 0x055279, 0x05e022, 0x068135,
    0x0738bc, 0x080a2d, 0x08f972, 0x0a0ad6, 0x0b42e6, 0x0ca62d, 0x0e38ae, 0x0ffd16,
    0x11f372, 0x141772, 0x165e30, 0x18ee9a, 0x1c095f, 0x1fc215, 0x2422ad, 0x291b66,
    0x2e6850, 0x3371b5, 0x37411e, 0x38b9f9,
};

// §14.2 - a falling segment's step per tick at each rate
static const int32_t  kDxDecayWords[100]          = {
    0x000014, 0x000015, 0x000016, 0x000017, 0x000019, 0x00001a, 0x00001c, 0x00001e,
    0x000020, 0x000022, 0x000024, 0x000027, 0x00002a, 0x00002e, 0x000032, 0x000037,
    0x00003c, 0x000042, 0x000049, 0x000052, 0x00005b, 0x000067, 0x000074, 0x000084,
    0x000097, 0x0000ad, 0x0000c6, 0x0000e2, 0x000100, 0x00011f, 0x00013b, 0x000154,
    0x000172, 0x000193, 0x0001b8, 0x0001e3, 0x000215, 0x00024e, 0x000290, 0x0002dd,
    0x000338, 0x0003a2, 0x00041f, 0x0004b3, 0x000561, 0x00062d, 0x000718, 0x000820,
    0x00093c, 0x000a5b, 0x000b5c, 0x000c4e, 0x000d5c, 0x000e8a, 0x000fdd, 0x00115a,
    0x001307, 0x0014ed, 0x001714, 0x001986, 0x001c51, 0x001f82, 0x00232a, 0x00275b,
    0x002c29, 0x0031aa, 0x0037f1, 0x003f10, 0x004710, 0x004feb, 0x005981, 0x006442,
    0x007103, 0x00802d, 0x009233, 0x00a78a, 0x00c084, 0x00dd1c, 0x00fc8f, 0x011cd7,
    0x013a38, 0x01564c, 0x01766f, 0x019b69, 0x01c62a, 0x01f7e0, 0x0231f9, 0x027638,
    0x02c6bf, 0x032619, 0x039733, 0x041d34, 0x04bb10, 0x05729b, 0x0642d1, 0x07251b,
    0x0809ee, 0x08d6a0, 0x096813, 0x099d63,
};

// §14.4 - the level offset at each velocity, before Vel scales it
static const int32_t  kDxVelocityWords[128]       = {
    -0x788a9b, -0x372ef3, -0x366451, -0x359ae5, -0x34d2ad, -0x3345db, -0x328141, -0x31bddc,
    -0x30fbac, -0x303ab0, -0x2f7ae8, -0x2ebc56, -0x2ebc56, -0x2dfef8, -0x2d42cf, -0x2c87da,
    -0x2c87da, -0x2bce1a, -0x2b158f, -0x2b158f, -0x2a5e38, -0x2a5e38, -0x29a816, -0x28f329,
    -0x28f329, -0x283f71, -0x283f71, -0x278ced, -0x278ced, -0x26db9d, -0x26db9d, -0x26db9d,
    -0x262b83, -0x262b83, -0x257c9d, -0x257c9d, -0x257c9d, -0x24ceeb, -0x24ceeb, -0x24226f,
    -0x24226f, -0x24226f, -0x237727, -0x237727, -0x237727, -0x22cd14, -0x22cd14, -0x222435,
    -0x222435, -0x222435, -0x217c8b, -0x217c8b, -0x217c8b, -0x20d616, -0x20d616, -0x20d616,
    -0x2030d5, -0x2030d5, -0x1f8cc9, -0x1f8cc9, -0x1f8cc9, -0x1ee9f2, -0x1ee9f2, -0x1e484f,
    -0x1e484f, -0x1e484f, -0x1da7e1, -0x1da7e1, -0x1d08a8, -0x1d08a8, -0x1c6aa3, -0x1c6aa3,
    -0x1bcdd3, -0x1bcdd3, -0x1b3238, -0x1a97d1, -0x1a97d1, -0x19fe9f, -0x19fe9f, -0x1966a2,
    -0x18cfd9, -0x183a45, -0x183a45, -0x17a5e5, -0x1712bb, -0x1680c5, -0x15f003, -0x156077,
    -0x14d21f, -0x1444fb, -0x13b90c, -0x132e52, -0x12a4cd, -0x119560, -0x110f79, -0x108ac6,
    -0x0f84fe, -0x0f03ea, -0x0e055e, -0x0d87e7, -0x0c9098, -0x0c16bf, -0x0b26ab, -0x0ab071,
    -0x09c799, -0x0954fc, -0x087361, -0x080462, -0x072a02, -0x06bea1, -0x05eb7e, -0x051d2c,
    -0x04b7d3, -0x03f0be, -0x038f02, -0x02cf29, -0x021423, -0x01b86f, -0x0104a4, -0x0055ac,
    0x005478,   0x00f9ca,  0x019a4a,  0x0235f7,  0x02ccd2,  0x035eda,  0x03ec0f,  0x047472,
};

// §14.3 - the feedback amount at Feedback 0-7: algorithms 6 and 32, 4, 18, and every other
static const int32_t  kDxFeedbackWords[4][8]      = {
    {0x000000, 0x004f94, 0x00e165, 0x01cd13, 0x030152, 0x05b697, 0x09be85, 0x0ca7a5},
    {0x000000, 0x00e165, 0x01f901, 0x0381a9, 0x055a1a, 0x0938fc, 0x0ca7a5, 0x127360},
    {0x000000, 0x004456, 0x017d62, 0x03c70e, 0x07534c, 0x0ca7a5, 0x1d04cd, 0x345487},
    {0x000000, 0x004456, 0x017d62, 0x03c70e, 0x07534c, 0x0ca7a5, 0x17d0ea, 0x2c8241},
};

// §14.5 - Main's gain word at each algorithm; Main is twice it times the carriers' sum
static const int32_t  kDxMainWords[DX_ALGORITHMS] = {
    0x400000,   0x400000, 0x400000, 0x400000, 0x300000, 0x300000, 0x400000,  0x400000,
    0x400000,   0x400000, 0x400000, 0x400000, 0x400000, 0x400000, 0x400000, -0x800000,
    -0x800000, -0x800000, 0x300000, 0x300000, 0x200000, 0x200000, 0x200000,  0x1b3333,
    0x1b3333,   0x300000, 0x300000, 0x300000, 0x200000, 0x200000, 0x1b3333,  0x180000,
};

// §14.2 - the instrument's dial tables
static int32_t dx_word24(uint32_t word) {
    return (int32_t)(word << 8) >> 8;
}

static uint32_t dx_level_entry(double value) {
    long v = lround(value);

    return kDxLevelWords[(v < 0) ? 0 : ((v > 127) ? 127 : v)];
}

static int32_t dx_level_word(double value) {
    return dx_word24(dx_level_entry(value));
}

static uint8_t dx_rate(double value) {
    long v = lround(value);

    return (uint8_t)((v < 0) ? 0 : ((v > DX_RATE_TOP) ? DX_RATE_TOP : v));
}

// §14.3 - the feedback amount, by algorithm group and Feedback
static int32_t dx_feedback_word(uint32_t algorithm, double value) {
    long     v     = lround(value);
    uint32_t group = ((algorithm == 5u) || (algorithm == 31u)) ? 0u : ((algorithm == 3u) ? 1u : ((algorithm == 17u) ? 2u : 3u));

    return kDxFeedbackWords[group][(v < 0) ? 0 : ((v > 7) ? 7 : v)];
}

// §14.1 - the router and the Operators on its six inputs, gathered into one node: their FM runs
// through the router in both directions, which a chain of separate nodes cannot evaluate.
static void dx_build(tSoundEngineParams * params, tEngineNode * node, tModule * router, uint32_t variation) {
    node->dxAlgorithm  = (uint32_t)param_value(router, variation, DXROUTER_PARAM_ALGORITHM);
    node->dxFeedback   = dx_feedback_word(node->dxAlgorithm, param_value(router, variation, DXROUTER_PARAM_FEEDBACK)) / DSP_WORD_SCALE;
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

        op->present    = true;
        op->active     = (param_value(source, variation, OP_PARAM_ACTIVE) != 0.0);
        op->kbt        = (param_value(source, variation, OP_PARAM_KBT) != 0.0);
        op->sync       = (param_value(source, variation, OP_PARAM_SYNC) != 0.0);
        op->fixed      = (source->param[variation][OPERATOR_RATIO_FIXED_PARAM].value != 0);
        op->ratio      = operator_ratio(coarse, fine);
        op->fixedHz    = operator_fixed_hz(coarse, fine);
        op->detune     = exp2(((param_value(source, variation, OP_PARAM_DETUNE) - 7.0) * DX_DETUNE_CENTS_PER_STEP) / 1200.0);
        op->outLevel   = dx_word24(dx_level_entry(param_value(source, variation, OP_PARAM_LEVEL)) - DX_LEVEL_TOP);
        op->keyVel     = (int32_t)lround(param_value(source, variation, OP_PARAM_KEYVEL)) * OP_SEVENTH_WORD;
        op->rateScale  = (int32_t)lround(param_value(source, variation, OP_PARAM_RATESCALE)) * OP_RATESCALE_WORD;
        op->breakPoint = ((int32_t)lround(param_value(source, variation, OP_PARAM_BRPT)) - OP_BREAKPOINT_E4) * OP_NOTE_WORD;
        op->lCurve     = (uint8_t)source->param[variation][OP_PARAM_LCURVE].value;
        op->rCurve     = (uint8_t)source->param[variation][OP_PARAM_RCURVE].value;
        op->lDepth     = (int32_t)lround(param_value(source, variation, OP_PARAM_LDEPTH)) * OP_DEPTH_WORD;
        op->rDepth     = (int32_t)lround(param_value(source, variation, OP_PARAM_RDEPTH)) * OP_DEPTH_WORD;

        for (uint32_t s = 0; s < 4; s++) {
            op->rate[s]  = dx_rate(param_value(source, variation, OP_PARAM_R1 + (2 * s)));
            op->level[s] = dx_level_word(param_value(source, variation, OP_PARAM_R1 + (2 * s) + 1));
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
    bool          backLeg[MAX_NODE_INPUTS]        = {false};
    uint8_t       backModule[MAX_NODE_INPUTS]     = {0};
    uint8_t       backOut[MAX_NODE_INPUTS]        = {0};

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

            // notes §192 - still being built means this leg closes a loop through it
            if (  (resolvedIn[c] < 0) && (source != NULL) && (source->key.location < (uint32_t)locationMax)
               && (source->key.index < MAX_NUM_MODULES) && (source->key.index < 256u)
               && (sNodeBuilding[source->key.location][source->key.index] == true)) {
                backLeg[c]    = true;
                backModule[c] = (uint8_t)source->key.index;
                backOut[c]    = (uint8_t)sourceOutput;
            }
        }

        inCount = count;

        // §69.12 - 4-In from Bus: the 2-Outs sent to Bus 1/2, then those sent to Bus 3/4; select counts the first
        if (kind == eNodeIn4Bus) {
            tModule * feeder[MAX_NODE_INPUTS / 2];
            uint32_t  firstBus = voice_area_outputs_for_fx(module->key.slot, IN4_BUS_FIRST, feeder, MAX_NODE_INPUTS / 4);
            uint32_t  total    = firstBus + voice_area_outputs_for_fx(module->key.slot, IN4_BUS_FIRST + 1u, &feeder[firstBus],
                                                                      MAX_NODE_INPUTS / 4);

            inCount      = 0;
            node->select = firstBus;

            for (uint32_t f = 0; f < total; f++) {
                int32_t source = add_node(params, feeder[f], variation, depth + 1);

                resolvedIn[2u * f]            = source;
                resolvedSrcOut[2u * f]        = 0;
                resolvedIn[(2u * f) + 1u]     = source;
                resolvedSrcOut[(2u * f) + 1u] = 1;
                inCount                       = 2u * (f + 1u);
            }
        }

        // notes §79
        if (kind == eNodeFxIn) {
            uint32_t  wantedBus = module->param[variation][FXIN_PARAM_SOURCE].value;
            tModule * feeder[MAX_NODE_INPUTS / 2];
            uint32_t  feeders   = voice_area_outputs_for_fx(module->key.slot, wantedBus, feeder, MAX_NODE_INPUTS / 2);

            // notes §80 - one leg pair per feeder, summed at evaluation
            resolvedIn[0] = -1;
            resolvedIn[1] = -1;
            inCount       = 2;

            for (uint32_t f = 0; f < feeders; f++) {
                int32_t source = add_node(params, feeder[f], variation, depth + 1);

                resolvedIn[2u * f]            = source;
                resolvedSrcOut[2u * f]        = 0;
                resolvedIn[(2u * f) + 1u]     = source;
                resolvedSrcOut[(2u * f) + 1u] = 1;
                inCount                       = 2u * (f + 1u);
            }
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
    {
        // counted back from the end, in the order input_connectors() appends them
        bool     oscKind     = ((kind == eNodeOsc) || (kind == eNodeOscShp));
        bool     hasFm       = oscKind && (fm_connector_index(module->type) >= 0);
        bool     hasShapeMod = oscKind && (shape_mod_connector_index(module->type) >= 0);
        bool     hasSync     = oscKind && (sync_connector_index(module->type) >= 0);
        uint32_t next        = inCount;

        node->fmSlot       = hasFm ? (int8_t)(--next) : (int8_t)-1;
        node->shapeModSlot = hasShapeMod ? (int8_t)(--next) : (int8_t)-1;
        node->syncSlot     = hasSync ? (int8_t)(--next)
                             : (int8_t)((kind == eNodeOscPM) ? OSCPM_SYNC_SLOT : -1);
    }
    node->active      = true;

    {
        uint32_t c = 0;

        for (c = 0; c < MAX_NODE_INPUTS; c++) {
            node->in[c]     = (c < inCount) ? resolvedIn[c] : -1;

            if ((c < inCount) && (backLeg[c] == true)) {
                node->backMask     |= (1u << c);
                node->backModule[c] = backModule[c];
                node->backOut[c]    = backOut[c];
            }
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
            node->oscKbt         = (param_value(module, variation,
                                                isShpA ? SHPA_PARAM_KBT : SHPB_PARAM_KBT) != 0.0);
            node->basePitch      = param_value(module, variation,
                                               isShpA ? SHPA_PARAM_TUNE : SHPB_PARAM_TUNE)
                                   + (osc_fine_cents(param_value(module, variation,
                                                                 isShpA ? SHPA_PARAM_CENT
                                                            : SHPB_PARAM_CENT)) / 100.0);
            // notes §82
            node->shape          = param_value(module, variation,
                                               isShpA ? SHPA_PARAM_SHAPE : SHPB_PARAM_SHAPE) / 127.0;
            node->modAmount      = type_ii_attenuator(param_value(module, variation,
                                                                  isShpA ? SHPA_PARAM_PITCH_MOD
                                                             : SHPB_PARAM_PITCH_MOD));
            node->active         = (param_value(module, variation,
                                                isShpA ? SHPA_PARAM_ACTIVE : SHPB_PARAM_ACTIVE) != 0.0);
            node->shapeModAmount = shape_mod_amount(module, variation);
            set_osc_fm(node, module, variation);
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
            node->select = (module->param[variation][COMP_PARAM_SIDECHAIN].value != 0) ? 1u : 0u;
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
                                        * (60.0 / engine_master_bpm());

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

            node->rateHz     = lfo_rate_hz(range, param_value(module, variation, (uint32_t)p->rate));

            if (range == LFO_RANGE_CLK) {
                node->rateHz *= engine_master_bpm() / ENGINE_REFERENCE_BPM;   // §28.2 - lfo_rate_hz() takes 120 BPM
            }
            node->wave       = (p->waveform >= 0)
                             ? (tOscWave)param_value(module, variation, (uint32_t)p->waveform) : eOscWaveSine;

            if (module->type == moduleTypeLfoC) {
                node->wave = (tOscWave)module->mode[0].value;   // §28 - LfoC's waveform is a mode (lfoWaveStrMap)
            }
            node->polarity   = (p->polarity >= 0)
                             ? (uint32_t)param_value(module, variation, (uint32_t)p->polarity) : 0;
            node->shape      = (p->shape >= 0)   // §28.6 - the word (v - 64)/64, here in 0..1
                             ? lfo_shape_dial(param_value(module, variation, (uint32_t)p->shape)) : 0.5;
            node->active     = (param_value(module, variation, (uint32_t)p->active) != 0.0);
            node->shpWave    = (module->type == moduleTypeLfoShpA);
            node->lfoMono    = (module->param[variation][p->mono].value != 0); // a drop-down, read raw
            node->lfoHasSync = (module->type == moduleTypeLfoB) || (module->type == moduleTypeLfoShpA);
            node->lfoRateMod = (p->rateMod >= 0) ? type_ii_attenuator(param_value(module, variation, (uint32_t)p->rateMod)) : 0.0;
            node->lfoKbt     = (p->kbt >= 0) ? ((double)module->param[variation][p->kbt].value * 0.25) : 0.0;
            lfo_phase_build(node, module, variation, p);
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
        case eNodeStatus:
        {
            node->select = variation + 1u;    // §70.13 - Var Active dips when this changes
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
        case eNodeKeyQuant:
        {
            keyquant_build(node, module, variation);
            break;
        }
        case eNodeOscPerc:
        {
            const tOscParams * p = osc_params(module->type);

            if (p == NULL) {
                break;
            }
            set_osc_pitch(node, module, variation, p);
            node->percDecay = perc_decay_word(param_value(module, variation, PERC_PARAM_DECAY));
            node->percClick = pow(dial_fraction(param_value(module, variation, PERC_PARAM_CLICK)), 2.0);
            node->percPunch = (module->param[variation][PERC_PARAM_PUNCH].value != 0);
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

            if (node->shaper.kind == eShaperOverdrive) {
                overdrive_words(node->shaper.curve, node->shaper.sym, &node->od);
            }
            break;
        }
        case eNodeConstant:
        {
            if ((module->type == moduleTypeConstSwT) || (module->type == moduleTypeConstSwM)) {
                // §44 - off, the instrument clears the output; the switch is read raw, as a drop-down
                bool on = (module->param[variation][CONSTSW_PARAM_ON].value != 0);

                node->constant = on ? constant_level(param_value(module, variation, CONST_PARAM_VALUE),
                                                     module->param[variation][CONSTSW_PARAM_BIP_UNI].value == 0)
                                    : 0.0;
                break;
            }
            node->constant = constant_level(param_value(module, variation, CONST_PARAM_VALUE),
                                            module->param[variation][CONST_PARAM_BIP_UNI].value == 0);
            break;
        }
        case eNodeSw1to8:
        {
            node->select   = (uint32_t)module->param[variation][SWSEL_PARAM_SELECT].value; // §45
            node->outCount = (module->type == moduleTypeSw1to8) ? SW1TO8_OUTS
                             : ((module->type == moduleTypeSw1to4) ? 4u : 2u);             // §68.1
            break;
        }
        case eNodeValSw12:
        {
            // §68.2 - ValSw2-1's value (§34)
            double raw = param_value(module, variation, VALSW_PARAM_VALUE);

            node->constant = ((raw >= (double)VALSW_VALUE_TOP) ? 64.0 : raw) / UNITS_PER_FULL_SCALE;
            break;
        }
        case eNodeLevMod:
        {
            // §69.3 - Depth 0 (v x 2^16, the ModDepth input's attenuator), Balance 1 ((v - 64) x 2^14)
            double depth   = param_value(module, variation, 0);
            double balance = param_value(module, variation, 1);

            node->phaseWords[0] = dial_mod_word(depth);
            node->phaseWords[1] = (balance >= 127.0) ? 0x100000 : (((int32_t)floor(balance) - 64) * 16384);
            break;
        }
        case eNodePartQuant:
        {
            // §69.5 - Range reads (v & ~1) x 2^16
            node->phaseWords[0] = ((int32_t)floor(param_value(module, variation, 0)) & ~1) * 65536;
            break;
        }
        case eNodeDigitizer:
        {
            // §69.8 - Bits 0 (v + 1 bits, 12 Off), Rate 1 ((v - 64) x 2^14, a semitone a step), Rate mod 2
            // (v x 2^16), on 3
            uint32_t bits = (uint32_t)module->param[variation][0].value;
            double   rate = param_value(module, variation, 1);

            node->phaseWords[0] = (bits > 11u) ? 0xFFFFFF : (int32_t)(~((1u << (23u - bits)) - 1u) & 0xFFFFFFu);
            node->phaseWords[1] = (rate >= 127.0) ? 0x100000 : (((int32_t)floor(rate) - 64) * 16384);
            node->phaseWords[2] = dial_mod_word(param_value(module, variation, 2));
            node->active        = (module->param[variation][3].value != 0);
            break;
        }
        case eNodeMultiTap:
        case eNodeFlanger:
        case eNodePShift:
        case eNodeOscString:
        case eNodeResonator:
        case eNodeDriver:
        case eNodeNoiseGate:
        case eNodePitchTrack:
        case eNodeVocoder:
        case eNodeRndPattern:
        case eNodeSeqCtr:
        case eNodeMux8to1X:
        case eNodeLevScaler:
        {
            basic_build(node, module, variation);   // §70
            break;
        }
        case eNodeNoteDet:
        {
            node->select = (uint32_t)module->param[variation][0].value % MIDI_KEY_COUNT;   // §69.11
            // §70.13 - NoteRcv's Channel: 1-16, This, Keyb; NoteDet is always the keys
            node->bx[0]  = (module->type == moduleTypeNoteRcv) ? (double)module->param[variation][1].value : (double)MIDI_ROW_KEYB;
            break;
        }
        case eNodeCtrlRcv:
        {
            // §70.13 - Ctrl 0, Channel 1 (1-16, This, Keyb - the keys' channel, here the slot's own)
            uint32_t row = module->param[variation][1].value;

            node->select = module->param[variation][0].value % MIDI_KEY_COUNT;
            node->bx[0]  = (double)((row >= MIDI_ROW_THIS) ? MIDI_ROW_THIS : row);
            break;
        }
        case eNodeWahWah:
        {
            // §69.9 - Sweep mod 0 (v x 2^16), Sweep 1 (v x 2^14, 127 = 2^21), on 2
            double sweep = param_value(module, variation, 1);

            node->phaseWords[0] = dial_mod_word(param_value(module, variation, 0));
            node->phaseWords[1] = (sweep >= 127.0) ? 0x200000 : ((int32_t)floor(sweep) * 16384);
            node->active        = (module->param[variation][2].value != 0);
            break;
        }
        case eNodeDlyClock:
        {
            node->select = (uint32_t)module->param[variation][0].value % DLYCLOCK_SLOTS;   // §69.7 - clocks
            break;
        }
        case eNodeEnvFollow:
        {
            // §69.4 - each dial a one-pole reaching 1% in its labelled time at 96 kHz: Attack 0.53 ms (1) to
            // 1 s (127), 0 instant; Release 10 ms (0) to 3 s (127)
            double attack  = floor(param_value(module, variation, 0));
            double release = floor(param_value(module, variation, 1));

            node->phaseWords[0] = (attack < 1.0) ? 0x7fffff : follower_coef_word(0.53e-3 * pow(1000.0 / 0.53, (attack - 1.0) / 126.0));
            node->phaseWords[1] = follower_coef_word(10.0e-3 * pow(300.0, release / 127.0));
            break;
        }
        case eNodeWindSw:
        {
            // §68.5 - From 0 and To 1, v x 2^14 (half a unit a step, 127 = 64 units)
            double from = param_value(module, variation, 0);
            double to   = param_value(module, variation, 1);

            node->constant  = (from >= 127.0) ? 1.0 : (floor(from) / 128.0);
            node->modAmount = (to >= 127.0) ? 1.0 : (floor(to) / 128.0);
            break;
        }
        case eNodeLogicDelay:
        {
            // §46 - the same time law and ranges as Pulse (§18)
            node->pulseDial    = param_value(module, variation, LOGICDLY_PARAM_TIME);
            node->pulseRange   = (uint32_t)module->param[variation][LOGICDLY_PARAM_RANGE].value;
            node->pulseTimeMod = param_value(module, variation, LOGICDLY_PARAM_TIMEMOD);
            node->pulseSeconds = pulse_time_seconds(node->pulseDial, node->pulseRange);
            node->select       = (uint32_t)module->mode[LOGICDLY_MODE_TYPE].value;
            break;
        }
        case eNodeRandomA:
        {
            random_a_build(node, module, variation);
            break;
        }
        case eNodeCompLev:
        {
            // §48 - (C - 64) units as a word, the top step just under 64
            double c = param_value(module, variation, COMPLEV_PARAM_LEVEL);

            node->constant = (c >= 127.0) ? (0x1FFFFF / DSP_WORD_PER_ENGINE) : ((c - 64.0) / UNITS_PER_FULL_SCALE);
            break;
        }
        case eNodeFltVoice:
        {
            fltvoice_build(node, module, variation);
            break;
        }
        case eNodeFltPhase:
        {
            // §67 - PitchM 0, Freq 1, SpreadM 2, FB 3, Notches 4, Spread 5, on 6, Level 7, FBM 8, Type 9,
            // Kbt 10. Freq is 2 sin(pi fc / 96 kHz) for fc = 100 x 160^(v/127) Hz.
            static const int32_t kLoop[3] = {0, 0x7ff000, 0x7ff000};
            static const int32_t kDry[3]  = {0x7fffff, -0x333334, 0x4ccccc};
            double               freq     = param_value(module, variation, 1);
            double               spread   = param_value(module, variation, 5);
            double               fb       = param_value(module, variation, 3);
            uint32_t             type     = (uint32_t)module->param[variation][9].value % 3u;
            double               fc       = 100.0 * pow(160.0, freq / 127.0);
            double               level    = mix_level_gain(param_value(module, variation, 7));

            node->phaseWords[FLTPHASE_W_FREQ]    = (freq >= 127.0) ? 0x7fffff : (int32_t)lround(16777216.0 * sin(M_PI * fc / 96000.0));
            node->phaseWords[FLTPHASE_W_PITCHM]  = dial_mod_word(param_value(module, variation, 0));
            node->phaseWords[FLTPHASE_W_SPREADM] = dial_mod_word(param_value(module, variation, 2));
            node->phaseWords[FLTPHASE_W_SPREAD]  = (spread >= 127.0) ? 0x200000 : ((int32_t)floor(spread) << 14);
            node->phaseWords[FLTPHASE_W_FB]      = (fb >= 127.0) ? 0x7fffff : (((int32_t)floor(fb) - 64) * 131072);
            node->phaseWords[FLTPHASE_W_FBM]     = dial_mod_word(param_value(module, variation, 8));
            node->phaseWords[FLTPHASE_W_LEVEL]   = (int32_t)fmin(8388607.0, (double)llround(level * 8388608.0));
            node->phaseWords[FLTPHASE_W_LOOP]    = kLoop[type];
            node->phaseWords[FLTPHASE_W_DRY]     = kDry[type];
            node->select                         = (uint32_t)module->param[variation][4].value % 6u;
            node->fltKbt                         = param_value(module, variation, 10) * 0.25;
            node->active                         = (module->param[variation][6].value != 0);
            break;
        }
        case eNodeMetNoise:
        {
            // §66 - Colour 0, Freq 1, On 2, Freq Mod 3, Colour Mod 4; a dial v reads v x 2^14 against a
            // 2^21 full scale, a mod amount v x 2^16 (127 = full). Colour is inverted.
            uint32_t idx[4] = {1u, 3u, 0u, 4u};

            for (uint32_t k = 0; k < 4u; k++) {
                int32_t v = (int32_t)floor(param_value(module, variation, idx[k]));

                if ((k & 1u) == 0u) {
                    node->metWords[k] = (v >= 127) ? 0x200000 : (v << 14);
                } else {
                    node->metWords[k] = (v >= 127) ? 0x7fffff : (v << 16);
                }
            }

            node->metWords[2] = 0x200000 - node->metWords[2];
            node->active      = (module->param[variation][2].value != 0);
            break;
        }
        case eNodeDlyStereo:
        {
            // §65 - Time L 0, Time R 1, FB L 2, FB R 3, X-FB L 4, X-FB R 5, Time/Clk 6, LP 7, Dry/Wet 8, On 9,
            // HP 10; the range is mode 0 (500 ms, 1 s, 1.35 s). DelayB's laws throughout.
            double maxTime = delay_range_max_seconds(module->type, module->mode[0].value);
            bool   clocked = (module->param[variation][6].value != 0);

            for (uint32_t ch = 0; ch < 2u; ch++) {
                double dial    = param_value(module, variation, ch);
                double seconds = 0.0;

                if (clocked == true) {
                    seconds = clk_sync_beats(dial) * (60.0 / engine_master_bpm());   // notes §85

                    while ((seconds > maxTime) && (seconds > 0.0)) {
                        seconds *= 0.5;                                               // notes §86
                    }
                } else {
                    seconds = fmax(0.0, delay_time_seconds(maxTime, dial) - (1.0 / G2_ENGINE_SAMPLE_RATE));
                }

                if (ch == 0u) {
                    node->timeSeconds = seconds;
                } else {
                    node->timeSecondsR = seconds;
                }
            }

            delay_words(node, param_value(module, variation, 7), param_value(module, variation, 10), 0.0,
                        param_value(module, variation, 8), 0.0, 0.0);

            for (uint32_t k = 0; k < 4u; k++) {
                node->dlyStereoFb[k] = dly_dial_word(param_value(module, variation, 2u + k));
            }

            node->active = (module->param[variation][9].value != 0);
            break;
        }
        case eNodeRndClkA:
        {
            // §64 - Step's word is the host's square law, 512 (v^2 + 1), saturating at 127; then as RandomA's
            int32_t v    = (int32_t)param_value(module, variation, 0);
            double  word = (v >= 127) ? 8388607.0 : fmin(8388607.0, (512.0 * v * v) + 512.0);

            node->rndStep  = word / 8388608.0;
            node->rndScale = floor(fmin(sqrt(8388608.0 / word), 8192.0) * 2048.0) / 8388608.0;

            if (module->type == moduleTypeRndClkB) {
                // §70.9 - Step 0, OutType 1, on 2, Mode 3 (Mono), Step M 4
                node->rndStepWord    = (v >= 127) ? 8388607.0 : (double)(v << 16);
                node->rndStepModWord = (param_value(module, variation, 4) >= 127.0) ? 8388607.0
                                       : (double)((int32_t)param_value(module, variation, 4) << 16);
                node->lfoMono        = (module->param[variation][3].value != 0);
                node->rndShiftReg    = (module->mode[0].value == 1u);    // Character: Rnd1, Rnd2
                node->polarity       = (uint32_t)module->param[variation][1].value;
                node->active         = (module->param[variation][2].value != 0);
                break;
            }
            node->lfoMono  = (module->param[variation][1].value != 0);
            node->polarity = (uint32_t)module->param[variation][3].value;
            node->active   = (module->param[variation][4].value != 0);
            break;
        }
        case eNodeRndTrig:
        {
            // §64 - Density (v - 64) x 2^17 (127 full scale), StepM v x 2^8, on switch, Mono
            double v = param_value(module, variation, 0);

            node->constant  = (v >= 127.0) ? 8388607.0 : ((floor(v) - 64.0) * 131072.0);
            node->modAmount = (param_value(module, variation, 1) >= 127.0) ? 8388607.0 : (floor(param_value(module, variation, 1)) * 65536.0);
            node->active    = (module->param[variation][2].value != 0);
            node->lfoMono   = (module->param[variation][3].value != 0);
            break;
        }
        case eNodeNoteSend:
        {
            // §62 - Vel v x 2^14, Note v x 2^15 + 2^14 (a half to round with); midiChanStrMap 16 is This,
            // 17-20 Slot A-D. Only a note to this slot plays here; the rest would leave by MIDI.
            uint32_t channel = (uint32_t)module->param[variation][2].value;

            node->depth    = floor(param_value(module, variation, 0)) * 16384.0;
            node->constant = (floor(param_value(module, variation, 1)) * 32768.0) + 16384.0;
            node->active   = (channel == 16u) || (channel == (17u + module->key.slot));
            break;
        }
        case eNodeNoteScaler:
        {
            // §60 - the Range word v x 2^16, 127 reading full scale
            double range = param_value(module, variation, 0);

            node->constant = (range >= 127.0) ? 8388607.0 : floor(range * 65536.0);
            break;
        }
        case eNodeClkGen:
        {
            // §59 - numbered in node order, as the hand-out below does it
            uint32_t line = 0;

            for (int32_t k = 0; k < self; k++) {
                line += (params->node[k].kind == eNodeClkGen) ? 1u : 0u;
            }

            if (line < MAX_CLKGEN_LINES) {
                tClkGenConfig * cfg    = &params->clkGen[line];
                uint32_t        index  = (uint32_t)param_value(module, variation, CLKGEN_PARAM_TEMPO);
                // §59 - Master follows the instrument's global clock
                double          bpm    = (module->param[variation][CLKGEN_PARAM_SOURCE].value != 0) ? engine_master_bpm()
                                        : ((index < 32u) ? (24.0 + (2.0 * index))
                                           : ((index < 96u) ? (56.0 + index) : ((2.0 * index) - 40.0)));

                cfg->tempo  = (int32_t)floor(bpm * 279.625);
                cfg->sync   = (int32_t)module->param[variation][CLKGEN_PARAM_SYNC].value;
                cfg->swing  = (int32_t)module->param[variation][CLKGEN_PARAM_SWING].value;
                // §59 - on Master it reads the master clock's own count, so a stopped master stops it
                bool            master = (module->param[variation][CLKGEN_PARAM_SOURCE].value != 0);

                cfg->active = (uint8_t)(  (module->param[variation][CLKGEN_PARAM_ACTIVE].value != 0)
                                       && ((master == false) || (engine_master_running() == true)));
            }
            break;
        }
        case eNodeSeq16:
        {
            // §58 - the line the hand-out below will give it: sequencers are numbered in node order
            uint32_t line = 0;

            for (int32_t k = 0; k < self; k++) {
                line += (params->node[k].kind == eNodeSeq16) ? 1u : 0u;
            }

            if (line < MAX_SEQ_LINES) {
                seq_config_build(&params->seq[line], module, variation);
            }
            break;
        }
        case eNodeFreqShift:
        {
            // §57 - the shift word v/128 (127 = 1), cubed and scaled by the range's word; Mod read linear
            static const double kRangeWord[3] = {0x80, 0x42C0, 0x42E40};
            uint32_t            range         = (uint32_t)module->param[variation][FREQSHIFT_PARAM_RANGE].value;
            double              shift         = param_value(module, variation, FREQSHIFT_PARAM_SHIFT);

            node->constant  = (shift >= 127.0) ? 1.0 : (shift / 128.0);
            node->modAmount = dial_fraction(param_value(module, variation, FREQSHIFT_PARAM_MOD));
            node->depth     = kRangeWord[(range < 3u) ? range : 2u] / 8388608.0;
            node->active    = (module->param[variation][FREQSHIFT_PARAM_ACTIVE].value != 0);
            break;
        }
        case eNodePhaser:
        {
            // §55 - the host's words: the counter's step, and FB as floor(v x 113 / 127) / 128
            double rate = param_value(module, variation, PHASER_PARAM_RATE);
            double fb   = floor((param_value(module, variation, PHASER_PARAM_FB) * 113.0) / 127.0);

            node->rateHz = phaser_rate_hz(rate);                                        // shared with the readout
            node->depth  = fb / 128.0;
            node->select = (uint32_t)module->param[variation][PHASER_PARAM_TYPE].value; // phaserTypeStrMap
            node->active = (module->param[variation][PHASER_PARAM_ACTIVE].value != 0);
            break;
        }
        case eNodeOscPM:
        {
            const tOscParams * p = osc_params(module->type);

            if (p == NULL) {
                break;
            }
            set_osc_pitch(node, module, variation, p);
            node->wave      = (tOscWave)module->mode[p->waveMode].value;   // oscPmWaveStrMap: Sin, Tri
            node->modAmount = fmin(1.0, type_ii_attenuator(param_value(module, variation, OSCPM_PARAM_PHM)));
            break;
        }
        case eNodeDlySingle:
        {
            // §52 - the Time dial and the samples one step of it is in this range; Time M read raw
            double maxTime = delay_range_max_seconds(module->type, module->mode[DELAY_MODE_RANGE].value);

            node->constant    = param_value(module, variation, DELAY_PARAM_TIME);
            node->timeSeconds = round((maxTime * G2_ENGINE_SAMPLE_RATE) / 127.0) / G2_ENGINE_SAMPLE_RATE;
            node->modAmount   = (module->type == moduleTypeDlySingleB)
                                ? param_value(module, variation, DLYSGL_PARAM_TIMEMOD) : 0.0;
            break;
        }
        case eNodeOscMaster:
        {
            // §51 - Coarse in whole semitones whatever the tune mode shows, Cent +-half a semitone
            node->constant   = ((param_value(module, variation, OSCMST_PARAM_COARSE) - 64.0)
                                + ((param_value(module, variation, OSCMST_PARAM_FINE) - 64.0) / 128.0))
                               / PITCH_MOD_SEMITONES;
            node->lfoKbt     = (module->param[variation][OSCMST_PARAM_KBT].value != 0) ? 1.0 : 0.0;
            node->lfoRateMod = type_ii_attenuator(param_value(module, variation, OSCMST_PARAM_PITCHMOD));
            break;
        }
        case eNodeNoteQuant:
        {
            // §49 - Range is the negated word v x 2^16, 127 reading -1.0; Notes is read raw
            double range = param_value(module, variation, NOTEQ_PARAM_RANGE);

            node->constant = (range >= 127.0) ? -8388608.0 : -floor(range * 65536.0);
            node->select   = (uint32_t)module->param[variation][NOTEQ_PARAM_NOTES].value;
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
            // §34 - the value in units, its top step reading 64 rather than 63.
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

            node->drumMasterHz   = drum_master_hz(param_value(module, variation, DRUM_PARAM_MASTER_FREQ));
            node->drumSlaveRatio = drum_slave_ratio(param_value(module, variation, DRUM_PARAM_SLAVE_RATIO));
            node->drumLevel[0]   = mix_level_gain(param_value(module, variation, DRUM_PARAM_MASTER_LEVEL));
            node->drumLevel[1]   = mix_level_gain(param_value(module, variation, DRUM_PARAM_SLAVE_LEVEL));
            // §39.10 - the coefficient is the cutoff table's word, used directly: 2 sin(pi f/192000)
            node->drumNoiseWord  = 2.0 * sin(M_PI * DRUM_CUTOFF_BASE_HZ
                                             * exp2((param_value(module, variation, DRUM_PARAM_NOISE_FREQ) + 4.0) / 12.0)
                                             / (2.0 * DRUM_INSTRUMENT_RATE));
            // §39.9 - Res arrives as dial/512 capped at a quarter
            node->drumNoiseDamp  = 1.0 - (4.0 * DRUM_RES_GAIN
                                          * fmin(0.25, param_value(module, variation, DRUM_PARAM_NOISE_RES) / 512.0));
            node->drumSweepSemis = DRUM_SWEEP_SEMIS * DRUM_VEL_WORD
                                   * fmin(1.0, param_value(module, variation, DRUM_PARAM_NOISE_SWEEP) / 128.0);
            node->drumBendSemis  = DRUM_PITCH_SEMIS_MAX
                                   * mix_level_gain(param_value(module, variation, DRUM_PARAM_BEND_AMOUNT));
            node->drumClick      = mix_level_gain(param_value(module, variation, DRUM_PARAM_CLICK));
            node->drumClickDecay = pow(DRUM_CLICK_DECAY_96K, DRUM_INSTRUMENT_RATE / gSampleRate);
            node->drumNoiseLevel = mix_level_gain(param_value(module, variation, DRUM_PARAM_NOISE_AMOUNT));
            node->drumFilterType = (uint32_t)module->param[variation][DRUM_PARAM_NOISE_TYPE].value;
            node->active         = (module->param[variation][DRUM_PARAM_ON].value != 0);
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
        case eNodeIn4Bus:
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
            node->shapeModAmount = shape_mod_amount(module, variation);
            set_osc_fm(node, module, variation);
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
            node->fltKbt          = (map.kbt >= 0)
                              ? flt_kbt_amount((uint32_t)param_value(module, variation, (uint32_t)map.kbt)) : 0.0;
            node->fltFmAmount     = (module->type == moduleTypeFltNord) ? flt_nord_mod_amount(param_value(module, variation, FLTNORD_PARAM_FMLIN)) : 0.0;
            node->fltResModAmount = (module->type == moduleTypeFltNord) ? flt_nord_mod_amount(param_value(module, variation, FLTNORD_PARAM_RESM)) : 0.0;
            node->modAmount       = (map.env >= 0)
                              ? (param_value(module, variation, (uint32_t)map.env) * 2.0 / 128.0) : 0.0;
            node->active          = (param_value(module, variation, (uint32_t)map.active) != 0.0);
            node->fltGainComp     = (  (module->type == moduleTypeFltStatic)
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

                // §17.4 - EnvD and EnvH have no KB: only their Trig jack starts them
                int       kb         = env_kb_param(module->type);
                int       reset      = env_reset_param(module->type);

                node->envKeyGate = ((kb >= 0) && (module->param[variation][kb].value != 0)) || unplayed;
                node->envReset   = (reset >= 0) && (module->param[variation][reset].value != 0);
            }
            break;
        }
        case eNodePulse:
        {
            node->pulseDial    = param_value(module, variation, PULSE_PARAM_TIME);
            node->pulseRange   = (uint32_t)param_value(module, variation, PULSE_PARAM_RANGE);
            node->pulseTimeMod = param_value(module, variation, PULSE_PARAM_TIMEMOD);
            node->pulseSeconds = pulse_time_seconds(node->pulseDial, node->pulseRange);
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
           || (kind == eNodeOscPerc)
           || (kind == eNodeOscPM)
           || (kind == eNodeDx)
           || (kind == eNodeDrumSynth)
           || (kind == eNodeMetNoise);   // §66
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
                        || (node->kind == eNodeDlySingle)   // §52 - one shared line, like DelayA/B
                        || (node->kind == eNodeDlyStereo)   // §65 - likewise
                        || (node->kind == eNodeMultiTap)    // §70.1
                        || (node->kind == eNodeReverb);

        for (uint32_t c = 0; (c < node->inCount) && (node->postMix == false); c++) {
            int32_t in = node->in[c];

            if ((node->backMask & (1u << c)) != 0u) {
                continue;   // notes §192 - a loop's closing leg does not decide where a node runs
            }

            if ((in >= 0) && (in < (int32_t)params->nodeCount) && (params->node[in].postMix == true)) {
                node->postMix = true;
            }
        }
    }
}

// §62 - a NoteSend feeds no Out, so building back from the Outs never reaches it; each is a root of
// its own, with whatever drives it
static void add_note_senders(tSoundEngineParams * params, uint32_t variation) {
    for (uint32_t l = 0; l < 2; l++) {
        uint32_t location = (l == 0) ? (uint32_t)locationFx : (uint32_t)locationVa;

        for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
            tModule * module = get_module_slot(engine_slot(), location, index);

            if ((module != NULL) && (module->type == moduleTypeNoteSend)) {
                (void)add_node(params, module, variation, 0);
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

    // §63 - the patch Volume: the same exp curve as a mixer level, in the active variation; off is silence
    {
        tModule * volume = get_module_slot(engine_slot(), (uint32_t)locationMorph, patchModuleVolume);
        uint32_t  active = gPatchDescr[engine_slot()].activeVariation;

        snapshot.slotGain = 1.0;

        if ((volume != NULL) && (active < NUM_VARIATIONS)) {
            snapshot.slotGain = (volume->param[active][VOLUME_MUTE].value != 0)
                                ? mix_level_gain((double)volume->param[active][VOLUME_LEVEL].value) : 0.0;
        }
    }

    // Glide and Bend come from the patch, not from any module in the chain — they sit on hidden
    // modules in the Morph location alongside the rest of the patch settings.
    {
        tModule * glide = get_module_slot(engine_slot(), (uint32_t)locationMorph, patchModuleGlide);
        tModule * bend  = get_module_slot(engine_slot(), (uint32_t)locationMorph, patchModuleBend);

        // §63a - the patch's Octave Shift transposes the keyboard, stored 0..4 with 2 as none
        {
            tModule * sustain = get_module_slot(engine_slot(), (uint32_t)locationMorph, patchModuleSustain);
            // a patch begun in the editor has no settings module, and its zeroes would read as -2 octaves
            uint32_t  shift   = ((sustain != NULL) && (sustain->active == true)) ? sustain->param[0][OCTAVE_SHIFT].value : OCTAVE_SHIFT_ZERO;

            snapshot.octaveSemis = 12.0 * ((double)((shift <= 4u) ? shift : OCTAVE_SHIFT_ZERO) - OCTAVE_SHIFT_ZERO);
        }

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

                    add_note_senders(&snapshot, variation);
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

    // notes §192 - point each loop-closing leg at its source, now that the source has a node
    for (uint32_t r = 0; r < snapshot.nodeCount; r++) {
        tEngineNode * reader = &snapshot.node[r];

        for (uint32_t c = 0; (c < reader->inCount) && (reader->backMask != 0u); c++) {
            if ((reader->backMask & (1u << c)) == 0u) {
                continue;
            }
            int32_t  found = -1;

            for (uint32_t k = 0; k < snapshot.nodeCount; k++) {
                if (  (snapshot.node[k].moduleIndex == reader->backModule[c])
                   && (snapshot.node[k].location == reader->location)) {
                    found = (int32_t)k;
                    break;
                }
            }

            if ((found < 0) || (snapshot.backCount >= MAX_BACK_EDGES)) {
                reader->backMask &= ~(1u << c);   // left unpatched, as before
                continue;
            }
            uint32_t legs  = node_output_legs(snapshot.node[found].kind);
            uint32_t out   = reader->backOut[c];

            reader->in[c]                        = found;
            reader->backSlot[c]                  = (uint8_t)snapshot.backCount;
            snapshot.backSrc[snapshot.backCount] = found;
            snapshot.backLeg[snapshot.backCount] = (out == 0u) ? 0u : ((legs > 2u) ? ((out < legs) ? out : (legs - 1u)) : 1u);
            snapshot.backCount++;
        }
    }

    // Hand out the shared delay lines. Done here rather than in add_node() so the assignment is
    // stable for a given chain — the audio thread keys its buffers off it.
    {
        uint32_t i          = 0;
        uint32_t lines      = 0;
        uint32_t verbs      = 0;
        uint32_t combs      = 0;
        uint32_t choruses   = 0;
        uint32_t shifters   = 0;
        uint32_t sequencers = 0;
        uint32_t clocks     = 0;
        uint32_t metals     = 0;
        uint32_t phases     = 0;
        uint32_t registers  = 0;
        uint32_t fxbufs     = 0;
        uint32_t strings    = 0;
        uint32_t basics     = 0;
        uint32_t trackers   = 0;

        for (i = 0; i < snapshot.nodeCount; i++) {
            if (  (snapshot.node[i].kind == eNodeDelay) || (snapshot.node[i].kind == eNodeDlySingle)
               || (snapshot.node[i].kind == eNodeMultiTap)) {
                snapshot.node[i].line = lines++;
            } else if ((snapshot.node[i].kind == eNodeFlanger) || (snapshot.node[i].kind == eNodePShift)) {
                snapshot.node[i].line = fxbufs++;
            } else if (snapshot.node[i].kind == eNodeOscString) {
                snapshot.node[i].line = strings++;
            } else if (snapshot.node[i].kind == eNodeResonator) {
                snapshot.node[i].line = strings;   // §70.4a - two lines, this and the next
                strings              += 2u;
            } else if (snapshot.node[i].kind == eNodeVocoder) {
                snapshot.node[i].line = basics++;
            } else if (snapshot.node[i].kind == eNodePitchTrack) {
                snapshot.node[i].line = trackers++;
            } else if (snapshot.node[i].kind == eNodeDlyStereo) {
                snapshot.node[i].line = lines;   // §65 - two lines, this and the next
                lines                += 2u;
            } else if (snapshot.node[i].kind == eNodeReverb) {
                snapshot.node[i].line = verbs++;
            } else if (snapshot.node[i].kind == eNodeFltComb) {
                snapshot.node[i].line = combs++;
            } else if (snapshot.node[i].kind == eNodeFreqShift) {
                snapshot.node[i].line = shifters++;
            } else if (snapshot.node[i].kind == eNodeSeq16) {
                snapshot.node[i].line = sequencers++;
            } else if (snapshot.node[i].kind == eNodeClkGen) {
                snapshot.node[i].line = clocks++;
            } else if (snapshot.node[i].kind == eNodeMetNoise) {
                snapshot.node[i].line = metals++;
            } else if (snapshot.node[i].kind == eNodeFltPhase) {
                snapshot.node[i].line = phases++;
            } else if (snapshot.node[i].kind == eNodeDlyClock) {
                snapshot.node[i].line = registers++;
            } else if (snapshot.node[i].kind == eNodeChorus) {
                snapshot.node[i].line = choruses++;
            }
        }
    }
    mark_post_mix_nodes(&snapshot);

    // notes §197 - the choruses again, numbered within their own area's pool
    {
        uint32_t fxChoruses    = 0u;
        uint32_t voiceChoruses = 0u;

        for (uint32_t k = 0; k < snapshot.nodeCount; k++) {
            if (snapshot.node[k].kind == eNodeChorus) {
                snapshot.node[k].line = (snapshot.node[k].postMix == true) ? fxChoruses++ : voiceChoruses++;
            }
        }
    }
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
    double rise   = fmin(fmax(1.0 - y, TRISAW_SHORTEST_RISE * x), 2.0 - (TRISAW_SHORTEST_RISE * x));
    double peak   = -y;
    double trough = osc_wrap_two(peak - rise);
    double since  = osc_wrap_two(p - trough);
    double value  = (since < 0.0) ? (since + 2.0) : since;
    double turn   = (2.0 / rise) + (2.0 / (2.0 - rise));

    value = (value < rise) ? (-1.0 + (2.0 * value / rise)) : (1.0 - (2.0 * (value - rise) / (2.0 - rise)));
    double atWrap = (fabs(y) < 1.0) ? shp_corner(osc_wrap_two(p - 1.0), x, turn) : 0.0;    // at y = +-1 the peak is the wrap

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
            double rise = fmin(fmax(0.5 * (1.0 - y), SHP_SINE1_SHORTEST * inc96), 1.0 - (SHP_SINE1_SHORTEST * inc96));    // peaks at half a cycle

            return wave_sine1_limited(fmod(phase + 0.5 + (0.5 * rise), 1.0), shape, SHP_SINE1_SHORTEST * inc96);
        }
        case 1:
        {
            // §27.2 - four samples at the least, and a gain of 1 + |Shape|; the DC blocker follows
            double lobe = fmin(fmax(0.5 * (1.0 - y), SHP_SINE2_SHORTEST * inc96), 1.0 - (SHP_SINE2_SHORTEST * inc96));    // ends at half a cycle

            return wave_sine2_limited(fmod(phase + 0.5 + lobe, 1.0), shape, SHP_SINE2_SHORTEST * inc96) * (1.0 + fabs(y));
        }
        case 2:
        {
            return wave_sine3_instrument(fmod(phase + SHP_DSF_ORIGIN, 1.0), fmax(shape, 0.0), inc96);    // §27.3, §6.7
        }
        case 3:
        {
            return wave_sine4_instrument(fmod(phase + SHP_DSF_ORIGIN, 1.0), fmax(shape, 0.0), inc96);
        }
        case 4:
        {
            return shp_trisaw(osc_instrument_phase(phase), x, y);
        }
        case 5:
        {
            return -osc_saw(phase, edge) - osc_saw(fmod(phase + 1.0 + (0.5 * y), 1.0), edge);
        }
        case 6:
        {
            return shp_pulse(osc_instrument_phase(phase), x, y);
        }
        default:
        {
            return shp_sympulse(phase, edge, fabs(y));    // §6.7
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

// §52.1 - the delay tap's read: four-point Lagrange, `back` samples behind the sample just written
static double delay_ring_lagrange(const float * mem, uint32_t written, double back) {
    double   d     = fmin(fmax(back, 0.0), (double)(DELAY_LINE_SAMPLES - 3));
    uint32_t whole = (uint32_t)d;
    double   t     = d - (double)whole;
    double   y[4];

    // whole - 1 .. whole + 2 behind; nothing is newer than the sample just written, which stands in
    for (uint32_t k = 0; k < 4u; k++) {
        uint32_t age = (whole + k > 0u) ? (whole + k - 1u) : 0u;

        y[k] = (double)mem[(written + DELAY_LINE_SAMPLES - age) % DELAY_LINE_SAMPLES];
    }

    return (y[0] * (-t * (t - 1.0) * (t - 2.0) / 6.0))
           + (y[1] * ((t + 1.0) * (t - 1.0) * (t - 2.0) / 2.0))
           + (y[2] * (-(t + 1.0) * t * (t - 2.0) / 2.0))
           + (y[3] * ((t + 1.0) * t * (t - 1.0) / 6.0));
}

// §52 - DlySingleA/B: the input goes in, and comes out Time x step samples later; Time M adds
// In x TimeMod dial steps.
static double dly_single_step(uint32_t line, double input, double modIn, const tEngineNode * spec) {
    SE_LOCAL;

    if (line >= MAX_DELAY_LINES) {
        return input;
    }
    uint32_t write = gDelayWrite[line];
    float *  mem   = gDelayLine[line];
    double   dial  = fmax(0.0, fmin(127.0, spec->constant + (spec->modAmount * modIn)));
    double   back  = dial * spec->timeSeconds * gSampleRate;

    mem[write]        = (float)input;
    gDelayWrite[line] = (write + 1u) % DELAY_LINE_SAMPLES;

    return delay_ring_lagrange(mem, write, back);
}

// §65 - DlyStereo: DelayB's tap (§24) on two lines fed from the one input; each line's feedback is
// FB x its own filtered tap plus X-FB x the other's.
static void dly_stereo_step(const tEngineNode * spec, double input, double * outL, double * outR) {
    SE_LOCAL;

    uint32_t line    = spec->line;

    if (((line + 1u) >= MAX_DELAY_LINES) || (spec->active == false)) {   // bypassed: no work while off
        *outL = input;
        *outR = input;
        return;
    }
    int32_t  half    = dly_sat(((int64_t)dly_sat((int64_t)floor(input * 2097152.0)) * 0x400000) >> 23);
    int32_t  c       = (int32_t)spec->damping;
    int32_t  f       = (int32_t)spec->hpCoeff;
    int32_t  xw      = (int32_t)spec->amount;
    int32_t  wet     = dly_host_mul((xw < 0x400000) ? (xw << 1) : 0x7fffff, (xw < 0x400000) ? (xw << 1) : 0x7fffff);
    int32_t  dryRamp = (xw > 0x400000) ? ((0x7fffff - xw) * 2) : 0x7fffff;
    int32_t  dry     = dly_host_mul(dryRamp, dryRamp);
    int32_t  gain    = dly_sat(0x800000 - (int64_t)f + ((-(int64_t)f * f) >> 23));
    int32_t  y[2];
    double   out[2];

    for (uint32_t ch = 0; ch < 2u; ch++) {
        uint32_t l       = line + ch;
        double   exact   = ((ch == 0u) ? spec->timeSeconds : spec->timeSecondsR) * gSampleRate;
        uint32_t samples = (exact < 0.5) ? 0u : (uint32_t)lround(exact);
        uint32_t write   = gDelayWrite[l];

        if (samples >= DELAY_LINE_SAMPLES) {
            samples = DELAY_LINE_SAMPLES - 1;
        }
        gDelayLine[l][write] = (float)dly_sat((int64_t)half + (int64_t)gDelayFb[l]);

        int32_t  tap     = (int32_t)gDelayLine[l][(write + DELAY_LINE_SAMPLES - samples) % DELAY_LINE_SAMPLES] & ~0xff;
        int32_t  x2      = (int32_t)gDelayDamp[l];
        int32_t  lp      = dly_sat((int64_t)x2 + ((((int64_t)c * tap) - ((int64_t)c * x2)) >> 23));
        int32_t  a       = (int32_t)gDelayHp[l];
        int32_t  b       = (int32_t)gDelayHpB[l];
        int32_t  t       = b + (int32_t)(((int64_t)f * a) >> 23);
        int32_t  high    = dly_sat((int64_t)lp - (3 * (int64_t)b) + ((((int64_t)f * b) - ((int64_t)f * a)) >> 23));

        y[ch]                = dly_sat(((int64_t)high * gain) >> 23);
        out[ch]              = (double)dly_sat((((int64_t)y[ch] * wet) + ((int64_t)half * dry)) >> 22) / 2097152.0;
        gDelayDamp[l]        = lp;
        gDelayHp[l]          = dly_sat(t);
        gDelayHpB[l]         = dly_sat((int64_t)b + (((int64_t)f * high) >> 23));
        gDelayWrite[l]       = (write + 1) % DELAY_LINE_SAMPLES;
    }

    gDelayFb[line]      = dly_sat((((int64_t)y[0] * spec->dlyStereoFb[0]) + ((int64_t)y[1] * spec->dlyStereoFb[2])) >> 23);
    gDelayFb[line + 1u] = dly_sat((((int64_t)y[1] * spec->dlyStereoFb[1]) + ((int64_t)y[0] * spec->dlyStereoFb[3])) >> 23);

    *outL               = out[0];
    *outR               = out[1];
}

// an engine value as the DSP word a cable carries, saturated
static int32_t engine_word(double value) {
    return dly_sat((int64_t)floor(value * DSP_WORD_PER_ENGINE));
}

// §69.5 - PartQuant: |In x Range| rounded to whole units picks partial k (at most 31), whose interval
// 12 log2(k + 1) semitones is the output, with In's sign
static double part_quant_step(const tEngineNode * spec, double input) {
    int32_t  in    = engine_word(input);
    int64_t  prod  = (int64_t)in * spec->phaseWords[0];
    int64_t  mag   = ((prod < 0) ? -prod : prod) >> 23;
    int64_t  index = (mag + 0x4000) >> 15;
    uint32_t k     = (index > 31) ? 31u : (uint32_t)index;
    double   word  = (double)llround(12.0 * log2((double)k + 1.0) * 32768.0);

    return ((in < 0) ? -word : word) / DSP_WORD_PER_ENGINE;
}

// §69.9 - WahWah: s = (Sweep + Sweep in x mod) x 4, floored at zero; q = s^2 moves the three words
// from their bottom to their top (frequency, damping, gain). A state-variable filter takes In at 1/4;
// the band-pass x the gain x 16 is the output. state: 0 low-pass, 1 band-pass.
static double wah_wah_step(double state[2], const tEngineNode * spec, double input, double sweepIn) {
    int32_t in   = engine_word(input);

    if (spec->active == false) {
        return (double)in / DSP_WORD_PER_ENGINE;
    }
    int64_t sv   = (((int64_t)spec->phaseWords[1] << 23) + ((int64_t)engine_word(sweepIn) * spec->phaseWords[0])) >> 21;
    int32_t s    = dly_sat((sv <= 0) ? 0 : sv);
    int32_t q    = dly_sat(((int64_t)s * s) >> 23);
    int32_t damp = dly_sat(0x66666 + (((int64_t)q * 0x15c28f) >> 23));
    int32_t freq = dly_sat(0x1374c + (((int64_t)q * 0x7ced9) >> 23));
    int32_t gain = dly_sat(0x11eb85 + (((int64_t)q * 0x333333) >> 23));
    int32_t lpW  = (int32_t)state[0];
    int32_t bpW  = (int32_t)state[1];
    int64_t lp   = ((int64_t)lpW << 23) + (2 * (int64_t)freq * bpW);
    int32_t hp   = dly_sat((((int64_t)in << 21) - lp - (2 * (int64_t)damp * bpW)) >> 23);

    bpW      = dly_sat((((int64_t)bpW << 23) + (2 * (int64_t)hp * freq)) >> 23);
    state[0] = (double)dly_sat(lp >> 23);
    state[1] = (double)bpW;
    return (double)dly_sat((((int64_t)bpW * gain) << 4) >> 23) / DSP_WORD_PER_ENGINE;
}

// §69.8 - Digitizer: the rate's pitch word (Rate + Mod x amount, x 8) through the semitone and cent
// tables, x 0x1c20c; the phase gains twice that a sample, and each time it overflows +-1 the input is
// taken. The output is the held word ANDed with the Bits mask. state: 0 the phase, 1 the held word.
static double digitizer_step(double state[2], const tEngineNode * spec, double input, double rateMod) {
    int32_t in    = engine_word(input);

    if (spec->active == false) {
        return (double)in / DSP_WORD_PER_ENGINE;
    }
    int32_t p     = dly_sat((((int64_t)spec->phaseWords[1] << 23) + ((int64_t)engine_word(rateMod) * spec->phaseWords[2])) >> 20);
    int32_t semi  = (int32_t)llround(131072.0 * pow(2.0, (double)(p >> 17) / 12.0));
    int32_t cent  = (int32_t)llround(4194304.0 * pow(2.0, (double)((p >> 10) & 127) / 1536.0));
    int32_t e     = dly_sat(((int64_t)semi * cent) >> 23);
    int64_t inc   = ((int64_t)e * 0x1c20c) >> 16;
    int64_t phase = (int64_t)state[0] + (2 * inc);

    if ((phase >= 0x800000) || (phase < -0x800000)) {
        state[1] = (double)in;
    }
    state[0] = (double)((int32_t)((uint32_t)phase << 8) >> 8);
    return (double)((int32_t)state[1] & (int32_t)(spec->phaseWords[0] | 0xFF000000u)) / DSP_WORD_PER_ENGINE;
}

// §69.4 - the one-pole coefficient that falls to 1% of a step in `seconds` at 96 kHz
static int32_t follower_coef_word(double seconds) {
    return (int32_t)fmin(8388607.0, (double)llround(8388608.0 * (1.0 - exp(-log(100.0) / (seconds * 96000.0)))));
}

// §69.3 - LevMod: v = (Balance + ModDepth x Depth) x 8; Out = In x (1/2 - v/2) + (In x Mod x 4) x (1/2 + v/2)
static double lev_mod_step(const tEngineNode * spec, double input, double mod, double depthIn) {
    int32_t in  = engine_word(input);
    int64_t acc = ((int64_t)spec->phaseWords[1] << 23) + ((int64_t)engine_word(depthIn) * spec->phaseWords[0]);
    int64_t v   = acc >> 20;
    bool    lo  = ((acc & 0xFFFFF) != 0);
    int32_t hi  = dly_sat(v);
    int32_t dry = dly_sat(0x400000 - ((int64_t)(hi >> 1) + ((((hi & 1) != 0) || ((hi == v) && lo)) ? 1 : 0)));
    int32_t wet = dly_sat(0x400000 + (int64_t)(hi >> 1));
    int32_t am  = dly_sat(((int64_t)in * engine_word(mod)) >> 21);

    return (double)dly_sat((((int64_t)in * dry) + ((int64_t)am * wet)) >> 23) / DSP_WORD_PER_ENGINE;
}

// §69.4 - EnvFollow: |In| lifts the peak word at once and it falls by 0.0012 of the gap a sample; the
// output follows the peak with the Attack coefficient rising and the Release one falling.
// state: 0 the peak word, 1 the output word.
static double env_follow_step(double state[2], const tEngineNode * spec, double input) {
    int32_t in   = engine_word(input);
    int32_t mag  = (in < 0) ? -in : in;
    int32_t peak = (int32_t)state[0];
    int32_t out  = (int32_t)state[1];
    int32_t gap  = dly_sat((int64_t)mag - peak);
    int64_t acc  = (int64_t)peak << 23;

    if (((int64_t)mag - peak) > 0) {
        acc += (int64_t)gap << 23;
    } else if (((int64_t)mag - peak) < 0) {
        acc += (int64_t)0x2746 * gap;
    }
    int64_t diff = acc - ((int64_t)out << 23);
    int32_t step = dly_sat(diff >> 23);
    int64_t next = (int64_t)out << 23;

    if (diff > 0) {
        next += (int64_t)spec->phaseWords[0] * step;
    } else if ((diff >> 23) < 0) {
        next += (int64_t)spec->phaseWords[1] * step;
    }
    state[0] = (double)dly_sat(acc >> 23);
    state[1] = (double)dly_sat(next >> 23);
    return state[1] / DSP_WORD_PER_ENGINE;
}

// §67 - a modulation amount dial: v x 2^16, 127 full scale
static int32_t dial_mod_word(double dial) {
    return (dial >= 127.0) ? 0x7fffff : ((int32_t)floor(dial) * 65536);
}

// §67 - FltPhase. The pitch part: Pitch + PitchVar x PitchM (x4) through the semitone and cent tables
// (2^16 at zero), times Freq x the KBT word; Spread + Spr x SpreadM (x4), floored at 0x30000. Then six
// allpass stages carried in the accumulator, tapped after stage Notches + 1; the amount (FB + FM x FBM)
// times Type's loop word feeds the tap back to the first stage, times its dry word mixes the input in.
static double flt_phase_step(uint32_t voice, const tEngineNode * spec, double input, const double mods[4], double voicePitch) {
    SE_LOCAL;

    if (spec->line >= MAX_FLTPHASE_LINES) {
        return input;
    }
    const int32_t *  w      = spec->phaseWords;

    if (spec->active == false) {
        return (double)dly_sat(((int64_t)engine_word(input) * w[FLTPHASE_W_LEVEL]) >> 23) / DSP_WORD_PER_ENGINE;   // bypassed: no work while off
    }
    tFltPhaseState * st     = &gFltPhase[voice][spec->line];
    int32_t          p      = dly_sat((((int64_t)engine_word(mods[3]) << 23) + ((int64_t)engine_word(mods[0]) * w[FLTPHASE_W_PITCHM])) >> 21);
    int32_t          semi   = (int32_t)llround(131072.0 * pow(2.0, (double)(p >> 17) / 12.0));
    int32_t          cent   = (int32_t)llround(4194304.0 * pow(2.0, (double)((p >> 10) & 127) / 1536.0));
    int32_t          e      = dly_sat(((int64_t)semi * cent) >> 23);
    double           kbtOct = ((spec->fltKbt > 0.0) && (voicePitch >= 0.0)) ? (spec->fltKbt * (voicePitch - KBT_REFERENCE_NOTE) / 12.0) : 0.0;
    int32_t          kbt    = (int32_t)fmin(8388607.0, (double)llround(262144.0 * pow(2.0, kbtOct)));
    int32_t          kb     = dly_sat(((int64_t)w[FLTPHASE_W_FREQ] * kbt) >> 18);
    int32_t          b      = dly_sat(((int64_t)kb * e) >> 16);
    int64_t          sp     = (((int64_t)w[FLTPHASE_W_SPREAD] << 23) + ((int64_t)engine_word(mods[1]) * w[FLTPHASE_W_SPREADM])) >> 21;
    int32_t          a      = dly_sat((sp < 0x30000) ? 0x30000 : sp);
    int32_t          k      = 0;
    int64_t          dry    = (int64_t)engine_word(input) * w[FLTPHASE_W_LEVEL];
    int32_t          direct = dly_sat(dry >> 23);
    int64_t          acc    = (dry >> 3) + ((int64_t)st->feedback << 23);
    int32_t          tap    = 0;

    b = (b < 0) ? 0 : b;
    k = dly_sat(a + ((-(int64_t)a * b) >> 24));

    for (uint32_t s = 0; s < 6u; s++) {
        int32_t * sv  = st->stage[s];
        int64_t   s1n = ((int64_t)sv[0] << 23) + ((int64_t)b * sv[1]);
        int32_t   t   = 0;

        acc  -= 2 * (int64_t)k * sv[1];
        sv[0] = dly_sat(s1n >> 23);
        t     = dly_sat((acc - s1n) >> 23);
        sv[1] = dly_sat((((int64_t)sv[1] << 23) + ((int64_t)t * b)) >> 23);
        acc   = s1n + ((int64_t)t << 23) - (2 * (int64_t)k * sv[1]);

        if (s == spec->select) {
            tap = dly_sat(acc >> 23);
        }
    }

    int32_t amt  = dly_sat((((int64_t)w[FLTPHASE_W_FB] << 23) + (((int64_t)engine_word(mods[2]) * w[FLTPHASE_W_FBM]) << 3)) >> 23);
    int32_t loop = dly_sat(((int64_t)amt * w[FLTPHASE_W_LOOP]) >> 23);
    int32_t mix  = dly_sat(((int64_t)amt * w[FLTPHASE_W_DRY]) >> 23);

    st->feedback = dly_sat(((int64_t)tap * loop) >> 23);

    return (double)dly_sat(((((int64_t)tap * 0x7fffff) << 3) + ((int64_t)direct * mix)) >> 23) / DSP_WORD_PER_ENGINE;
}

// §66 - a mod part: the base word plus the input times the amount, shifted up two, floored at zero
static int32_t met_mod_word(int32_t base, double input, int32_t amount) {
    int64_t in  = dly_sat((int64_t)floor(input * DSP_WORD_PER_ENGINE));
    int64_t acc = (((int64_t)base << 23) + (in * amount)) >> 21;

    return (acc < 0) ? 0 : dly_sat(acc);
}

// §66 - MetNoise: six squares of amplitude 1/8 at fixed ratios to a squared Freq word, less 3/8; then
// eight one-pole high-passes t = s + c1.x, s' = c0.t - c1.x, the last shifted up two.
static double met_noise_step(uint32_t voice, const tEngineNode * spec, double freqMod, double colourMod) {
    SE_LOCAL;

    static const int32_t kRatio[6] = {0x1e354, 0x28b44, 0x2d7b9, 0x362fd, 0x46666, 0x4aec3};

    if ((spec->line >= MAX_METNOISE_LINES) || (spec->active == false)) {   // off: no work while off
        return 0.0;
    }
    tMetNoiseState *     st        = &gMetNoise[voice][spec->line];

    if (st->ready == false) {
        for (uint32_t k = 0; k < 6u; k++) {
            gStartPhaseSeed ^= gStartPhaseSeed << 13;
            gStartPhaseSeed ^= gStartPhaseSeed >> 17;
            gStartPhaseSeed ^= gStartPhaseSeed << 5;
            st->phase[k]     = (int32_t)(gStartPhaseSeed << 8) >> 8;
        }

        memset(st->hp, 0, sizeof(st->hp));
        st->ready = true;
    }
    int32_t              f         = dly_sat(0x3504f4 + (((int64_t)0x4afb0c * met_mod_word(spec->metWords[0], freqMod, spec->metWords[1])) >> 23));
    int32_t              freq      = dly_sat(((int64_t)f * f) >> 23);
    int32_t              u         = met_mod_word(spec->metWords[2], -colourMod, spec->metWords[3]);
    int32_t              a         = dly_sat(0x400000 + (((int64_t)u * -0xc0000) >> 23));
    int32_t              c0        = dly_sat(0x567af7 + (((int64_t)0x2522e4 * u) >> 23));
    int32_t              c1        = dly_sat(a + (((int64_t)a * c0) >> 23));
    int64_t              sum       = -0x300000;

    for (uint32_t k = 0; k < 6u; k++) {
        int64_t next = (int64_t)st->phase[k] + (((int64_t)freq * kRatio[k]) >> 23);

        if (next < 0) {
            sum += 0x100000;
        }
        st->phase[k] = (int32_t)((uint32_t)next << 8) >> 8;
    }

    int32_t              x         = (spec->active == true) ? dly_sat(sum) : 0;

    for (uint32_t k = 0; k < 7u; k++) {
        int32_t t = dly_sat((int64_t)st->hp[k] + (((int64_t)c1 * x) >> 23));

        st->hp[k] = dly_sat(((-(int64_t)c1 * x) + ((int64_t)c0 * t)) >> 23);
        x         = t;
    }

    int64_t              last      = ((int64_t)st->hp[7] << 23) + ((int64_t)c1 * x);

    st->hp[7] = dly_sat(((-(int64_t)c1 * x) + ((int64_t)c0 * dly_sat(last >> 23))) >> 23);
    return (double)dly_sat(last >> 21) / DSP_WORD_PER_ENGINE;
}

// §19 - StChorus. Tap positions count samples at CHORUS_TAP_RATE_HZ.
#define CHORUS_TICK_HZ           (24000.0)       // the LFO steps at the control rate
#define CHORUS_PHASE_HALF        (8388608)       // a signed 24-bit phase: -1..1 is one LFO cycle
#define CHORUS_DETUNE_STEP       (8.0)           // phase step per tick per Detune step
#define CHORUS_TAP_RATE_HZ       (96000.0)
#define CHORUS_TAP1_MAX          (505.0)
#define CHORUS_TAP1_SPAN         (504.0)
#define CHORUS_TAP2_MIN          (65.0)
#define CHORUS_TAP2_SPAN         (378.0)         // three quarters of tap 1's, the other way
#define CHORUS_FRACTION_STEPS    (32.0)          // a tap position resolves to 1/32 sample

// §40.2 - the Decay dial's per-sample multiplier, the instrument's own stored table (Q23). It follows
// no single law (notes §195), so it is carried as the instrument stores it.
static const int32_t kPercDecayWord[128] = {
    5715092, 6482641, 6914464, 7190968, 7383210, 7524700, 7633282, 7719319,
    7789242, 7847252, 7896206, 7938118, 7974447, 8006272, 8034415, 8059506,
    8082041, 8102411, 8120932, 8137861, 8153408, 8167745, 8181018, 8193348,
    8204838, 8215574, 8225630, 8235070, 8243949, 8252314, 8260206, 8267660,
    8274708, 8281377, 8287692, 8293675, 8299343, 8304715, 8309806, 8314630,
    8319200, 8323528, 8327625, 8331501, 8335166, 8338629, 8341900, 8344985,
    8347895, 8350635, 8353215, 8355640, 8357919, 8360059, 8362066, 8363946,
    8365706, 8367353, 8368892, 8370329, 8371670, 8372921, 8374086, 8375170,
    8376179, 8377117, 8377988, 8378797, 8379547, 8380243, 8380888, 8381485,
    8382038, 8382550, 8383024, 8383461, 8383865, 8384239, 8384584, 8384902,
    8385196, 8385467, 8385717, 8385948, 8386160, 8386356, 8386536, 8386703,
    8386856, 8386996, 8387126, 8387246, 8387355, 8387457, 8387550, 8387635,
    8387714, 8387786, 8387853, 8387914, 8387970, 8388022, 8388070, 8388113,
    8388153, 8388190, 8388224, 8388256, 8388284, 8388311, 8388335, 8388357,
    8388377, 8388396, 8388413, 8388429, 8388444, 8388457, 8388470, 8388481,
    8388491, 8388501, 8388509, 8388518, 8388525, 8388532, 8388538, 8388544,
};

static double perc_decay_word(double dial) {
    double   at   = fmin(127.0, fmax(0.0, dial));
    uint32_t i    = (uint32_t)at;
    uint32_t next = (i < 127u) ? (i + 1u) : i;

    return (kPercDecayWord[i] + ((kPercDecayWord[next] - kPercDecayWord[i]) * (at - i))) / 8388608.0;
}

// §41 - KeyQuant. The host's key table sets the thresholds, the part's frame the values
// it snaps to; both are 15-bit fractions of an octave.
#define KQ_PARAM_RANGE        (0)
#define KQ_PARAM_CAPTURE      (1)        // captureStrMap {Closest, Evenly}
#define KQ_PARAM_FIRST_KEY    (2)        // E, F ... D#
#define KQ_OCTAVE             (0x8000)
#define KQ_NEVER              (0x7fffff)
#define KQ_WORD               (2097152.0) // an engine unit is 64 units, 2^21 in the DSP's word
#define KQ_TWELFTH            (699051)    // X[1]
#define KQ_TIMES_TWELVE       (6)         // X[14]; the multiply's own doubling makes it 12

static const int32_t kKeyQuantKeys[12]   = {0, 2730, 5460, 8190, 10920, 13650, 16380, 19110, 21840, 24570, 27300, 30030};
static const int32_t kKeyQuantValues[12] = {0, 2730, 5461, 8192, 10922, 13653, 16384, 19114, 21845, 24576, 27306, 30037};

static int32_t dsp_limit24(int64_t acc) {
    return ((acc >> 47) == 0 || (acc >> 47) == -1) ? (int32_t)((uint32_t)(acc >> 24) << 8) >> 8
           : ((acc < 0) ? -0x800000 : 0x7fffff);
}

// §47 - Step sets two words: the one-pole's coefficient, and a pre-scale of the draw that keeps
// the output's spread the same whatever the coefficient
static void random_a_build(tEngineNode * node, tModule * module, uint32_t variation) {
    static const uint32_t kStepWord[] = {0x080200u, 0x200200u, 0x480200u, 0x7FFFFFu};
    static const uint32_t kEdgeWord[] = {0x010000u, 0x020000u, 0x040000u, 0x100000u, 0x7FFFFFu};
    // §69.10 - RandomB: Rate 0, Mono 1, Kbt 2, Rate mod 3, Step 4 (a dial), on 5, OutType 6, Range 7, Edge 8
    bool                  isB         = (module->type == moduleTypeRandomB);
    uint32_t              step        = (uint32_t)module->param[variation][RNDA_PARAM_STEP].value;
    uint32_t              edge        = (uint32_t)module->param[variation][isB ? 8u : RNDA_PARAM_EDGE].value;
    uint32_t              range       = (uint32_t)module->param[variation][isB ? 7u : RNDA_PARAM_RANGE].value;
    double                stepDial    = floor(param_value(module, variation, 4));
    double                word        = isB ? ((stepDial >= 127.0) ? 8388607.0 : fmin(8388607.0, (512.0 * stepDial * stepDial) + 512.0))
                                        : (double)kStepWord[(step < 4u) ? step : 3u];
    double                scale       = fmin(sqrt(8388608.0 / word), 8192.0) * 2048.0;

    node->rateHz     = lfo_rate_hz(range, param_value(module, variation, RNDA_PARAM_RATE));

    if (range == LFO_RANGE_CLK) {
        node->rateHz *= engine_master_bpm() / ENGINE_REFERENCE_BPM;   // §28.2
    }
    node->rndStep    = word / 8388608.0;
    node->rndScale   = floor(scale) / 8388608.0;
    node->rndEdge    = (double)kEdgeWord[(edge < 5u) ? edge : 4u] / 8388608.0;
    node->polarity   = (uint32_t)module->param[variation][isB ? 6u : RNDA_PARAM_OUTTYPE].value;
    node->lfoMono    = (module->param[variation][RNDA_PARAM_MONO].value != 0);
    node->active     = (module->param[variation][isB ? 5u : RNDA_PARAM_ACTIVE].value != 0);
    node->lfoKbt     = isB ? ((double)module->param[variation][2].value * 0.25) : 0.0;
    node->lfoRateMod = isB ? type_ii_attenuator(param_value(module, variation, 3)) : 0.0;
}

// §41 - the host's key update: Closest puts each threshold halfway between neighbouring keys that are on,
// with the octave's seam moved into the gap above the last; Evenly shares the octave out
static void keyquant_build(tEngineNode * node, tModule * module, uint32_t variation) {
    bool     on[12];
    uint32_t count   = 0;
    uint32_t first   = 0;
    uint32_t last    = 0;
    bool     closest = (module->param[variation][KQ_PARAM_CAPTURE].value == 0);
    uint32_t dial    = module->param[variation][KQ_PARAM_RANGE].value;

    for (uint32_t k = 0; k < 12u; k++) {
        on[k] = (module->param[variation][KQ_PARAM_FIRST_KEY + k].value != 0);

        if (on[k] == true) {
            first = (count == 0) ? k : first;
            last  = k;
            count++;
        }
    }

    node->kqRange   = (dial >= 127u) ? 0x7fffff : (int32_t)((dial << 16) - ((dial != 0) ? 5u : 0u));
    node->kqOffset  = (closest == true) ? ((kKeyQuantKeys[first] + kKeyQuantKeys[last] - KQ_OCTAVE) >> 1) : 0;
    node->kqAnyKey  = (count != 0);

    for (uint32_t k = 0, seen = 0, prev = last; k < 12u; k++) {
        if (on[k] == false) {
            node->kqThreshold[k] = KQ_NEVER;
            continue;
        }
        node->kqThreshold[k] = (seen == 0) ? 0
                               : (closest == true) ? (int32_t)(((uint32_t)(kKeyQuantKeys[prev] + kKeyQuantKeys[k]) >> 1) - (uint32_t)node->kqOffset)
                               : (int32_t)(seen * (KQ_OCTAVE / count));
        seen++;
        prev                 = k;
    }

    node->kqOffset *= 12;
}

// §41 - the part's program, in its own integer arithmetic
static double keyquant_step(const tEngineNode * spec, double input) {
    int32_t in      = (int32_t)fmax(-8388608.0, fmin(8388607.0, floor(input * KQ_WORD)));
    int64_t scaled  = 2 * (int64_t)spec->kqRange * in;
    int64_t a       = scaled - ((int64_t)spec->kqOffset << 24);
    int32_t shifted = dsp_limit24(a);
    int64_t octaves;
    int64_t result;

    a       = 2 * (int64_t)KQ_TWELFTH * shifted;
    octaves = (int64_t)((int32_t)((uint32_t)((a >> 24) & 0xFF8000) << 8) >> 8); // AND with 0xFF8000
    a       = (a >> 24) & 0x007FFF;                                             // the place within the octave

    if (spec->kqAnyKey == false) {
        return (double)dsp_limit24(scaled) / KQ_WORD;                           // no keys: the scaled input
    }
    result  = (int64_t)shifted;                                                // what b holds if nothing matches

    for (uint32_t k = 0; k < 12u; k++) {
        if (a >= spec->kqThreshold[k]) {
            result = kKeyQuantValues[k];
        }
    }

    result  = 2 * KQ_TIMES_TWELVE * (int64_t)dsp_limit24((result + octaves) << 24);
    return (double)((int32_t)((uint32_t)result << 8) >> 8) / KQ_WORD;
}

// §40 - OscPerc, the part's own program: the phase step and the stores are the DSP's, at 96 kHz
#define PERC_RATE         (96000.0)
#define PERC_WORD         (0.25)                 // a DSP word is a quarter of the engine's unit
#define PERC_SMOOTH       (8.0)                  // the output low-pass is 8x the resonator's k
#define PERC_LOW          (0)                    // gLadder slots
#define PERC_HIGH         (1)
#define PERC_LAST_TRIG    (2)
#define PERC_OUT          (3)
#define PERC_PHASE        (4)

static double perc_clamp(double x) {
    return fmin(1.0 - (1.0 / 8388608.0), fmax(-1.0, x));
}

// §40 - one sample of OscPerc, in DSP words until the end. A Trig edge (up through zero) strikes the
// resonator at twice the Trig's level and restarts the phase; Punch doubles k until the phase saturates,
// i.e. for the first half cycle; Click crossfades the resonator's two states.
static double perc_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double hz, double trigIn) {
    SE_LOCAL;

    double * st    = gLadder[voice][node];
    double   scale = PERC_RATE / gSampleRate;
    double   inc   = 2.0 * hz / gSampleRate;
    double   k     = M_PI * inc;
    double   phase = st[PERC_PHASE] + inc;
    double   trig  = trigIn * PERC_WORD;
    double   decay = pow(spec->percDecay, scale);
    double   high;
    double   smooth;

    if ((spec->percPunch == true) && (phase < 1.0)) {
        k *= 2.0;
    }
    st[PERC_PHASE]     = perc_clamp(phase);
    st[PERC_LOW]       = perc_clamp(st[PERC_LOW] + (k * st[PERC_HIGH]));
    high               = (st[PERC_HIGH] * decay) - (k * st[PERC_LOW]);

    if ((trig > 0.0) && (st[PERC_LAST_TRIG] <= 0.0)) {
        high           = 2.0 * trig;
        st[PERC_PHASE] = 0.0;
    }
    st[PERC_LAST_TRIG] = trig;
    st[PERC_HIGH]      = perc_clamp(high);
    smooth             = 1.0 - pow(1.0 - fmin(1.0 - (1.0 / 8388608.0), PERC_SMOOTH * k), scale);
    st[PERC_OUT]       = perc_clamp(st[PERC_OUT]
                                    + (smooth * (((st[PERC_HIGH] * spec->percClick)
                                                  + (st[PERC_LOW] * (1.0 - spec->percClick))) - st[PERC_OUT])));
    return st[PERC_OUT] / PERC_WORD;
}

// §8 - the module's own words, as fractions of the 24-bit word
#define OSCNOISE_Q23              (8388608.0)
#define OSCNOISE_LFSR_TAPS        (0xd71d87u)
#define OSCNOISE_TILT_POLE        (0x7bd7db / OSCNOISE_Q23)       // §8.2 - a one-pole at ~500 Hz
#define OSCNOISE_H_MAX            (0x518368 / OSCNOISE_Q23 / 2.0) // §8.3 - the pitch ceiling, 9.73 kHz
#define OSCNOISE_Q_BASE           (0x790000 / OSCNOISE_Q23)       // §8.4 - damping from Width
#define OSCNOISE_Q_WIDTH          (0x330000 / OSCNOISE_Q23)
#define OSCNOISE_DAMP_SPAN        (0x7eb852 / OSCNOISE_Q23)
#define OSCNOISE_GAIN_DAMP_MAX    (0x140000 / OSCNOISE_Q23)       // §8.5 - the input gain's damping term
#define OSCNOISE_SECTIONS         (3)
#define gOscNoiseState            gDrumState                      // a node is one kind: OscNoise uses the drum synth's words

static double white_noise(uint32_t * seed) {
    uint32_t x = *seed;

    x    ^= x << 13;
    x    ^= x >> 17;
    x    ^= x << 5;
    *seed = x;
    return ((double)x / 2147483648.0) - 1.0;
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

// §71 - OverDrive, the instrument's own. Each 24 kHz tick sets the drive and the blend from the dials and
// the Mod input; each sample feeds back (Heavy), shapes twice, takes out the DC and blends with the dry.
#define OD_FILTER      (0xfa0 / DSP_WORD_SCALE)       // §71.1 - the wet path's 7 Hz high-pass
#define OD_TOP         (8388607.0 / DSP_WORD_SCALE)
#define OD_SHAPED      (0)                            // gLadder slots: the shaped sample last time,
#define OD_BAND        (1)                            // the high-pass's two states,
#define OD_LOW         (2)
#define OD_DRIVE       (3)                            // and this tick's drive, feedback and blend
#define OD_FEEDBACK    (4)
#define OD_DRY         (5)
#define OD_WET         (6)

static double overdrive_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double input, double modulation) {
    SE_LOCAL;

    double * st  = gLadder[voice][n];
    double   x   = overdrive_word_saturate(input / DSP_FULL_SCALE);

    if (spec->active == false) {
        return input;
    }

    if (gEnvTick[voice][n] <= 0.0) {
        double drive = overdrive_word_saturate(fmax(0.0, spec->shaper.amount + (spec->shaper.mod * modulation)));
        double rest  = overdrive_word_saturate(OD_TOP - drive);

        st[OD_DRIVE]        = drive;
        st[OD_FEEDBACK]     = overdrive_word_saturate(drive * spec->od.feedback) / 2.0;
        st[OD_DRY]          = overdrive_word_saturate(rest * rest);
        st[OD_WET]          = overdrive_word_saturate(OD_TOP - st[OD_DRY]);
        gEnvTick[voice][n] += 1.0;
    }
    gEnvTick[voice][n] -= ENV_TICK_HZ / gSampleRate;

    double   v   = overdrive_word_saturate((st[OD_FEEDBACK] * st[OD_SHAPED]) + ((OD_TOP - st[OD_FEEDBACK]) * x));
    double   raw = 0.0;
    double   low = st[OD_LOW] + (OD_FILTER * st[OD_BAND]);
    double   e   = 0.0;

    overdrive_poly(&spec->od, overdrive_poly(&spec->od, overdrive_drive_gain(&spec->od, st[OD_DRIVE], v), NULL), &raw);
    st[OD_SHAPED]       = overdrive_word_saturate(raw);
    e                   = overdrive_word_saturate((raw / 4.0) - low - (2.0 * OD_TOP * st[OD_BAND]));
    st[OD_LOW]          = overdrive_word_saturate(low);
    st[OD_BAND]         = overdrive_word_saturate(st[OD_BAND] + (OD_FILTER * e));

    return DSP_FULL_SCALE * overdrive_word_saturate((st[OD_DRY] * x) + (st[OD_WET] * e));
}

// §18.3 - the time in samples, with a Time Mod input moving the dial by Mod x TimeMod steps
static uint32_t pulse_time_samples(const tEngineNode * spec, double modIn) {
    SE_LOCAL;

    double seconds = spec->pulseSeconds;

    if (spec->in[1] >= 0) {
        seconds = pulse_time_seconds(fmin(fmax(spec->pulseDial + (modIn * spec->pulseTimeMod), 0.0), PULSE_DIAL_TOP),
                                     spec->pulseRange);
    }
    double samples = seconds * gSampleRate;

    return (samples < 1.0) ? 1u : (uint32_t)samples;
}

// notes §106 - gPulseCount is the samples since the rising edge plus one, 0 while idle, so a Time
// Mod input moving during the pulse moves where it ends
static double pulse_step(uint32_t voice, uint32_t node, double input, double modIn, const tEngineNode * spec) {
    SE_LOCAL;

    double   prev    = gPulsePrev[voice][node];
    uint32_t samples = pulse_time_samples(spec, modIn);

    gPulsePrev[voice][node] = input;

    if ((prev <= PULSE_THRESHOLD) && (input > PULSE_THRESHOLD)) {
        gPulseCount[voice][node] = 1u;
    }

    if (gPulseCount[voice][node] > 0) {
        if (gPulseCount[voice][node] > samples) {
            gPulseCount[voice][node] = 0;
            return 0.0;
        }
        gPulseCount[voice][node]++;
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
// §25.2 - one detector and one gain for both channels; the detector hears the larger of L and R, or the
// side-chain when its switch is on. Returns L and writes R.
static double compress_step(uint32_t voice, uint32_t node, double inputL, double inputR, double sideChain,
                            const tEngineNode * spec, double * outputR) {
    SE_LOCAL;

    int32_t  in     = dly_sat((int64_t)floor(inputL * 2097152.0));
    int32_t  inR    = dly_sat((int64_t)floor(inputR * 2097152.0));
    int32_t  sc     = dly_sat((int64_t)floor(sideChain * 2097152.0));
    int32_t  loud   = ((in < 0 ? -(int64_t)in : (int64_t)in) < (inR < 0 ? -(int64_t)inR : (int64_t)inR)) ? inR : in;
    int32_t  heard  = (spec->select != 0u) ? sc : loud;
    int64_t  det    = (heard < 0) ? -(int64_t)heard : (int64_t)heard;
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

    *outputR              = (double)dly_sat(((int64_t)level * inR) >> 16) / 2097152.0;

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
    for (uint32_t i = 0; i < CHORUS_INSTANCES; i++) {
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

    while (current < 0.0) {    // §6.8 - through-zero FM
        current += 1.0;
    }
    *phase = current;
    return current;
}

// §53 - OscPM: the phase advances as any oscillator's; the modulation is added to the phase it is
// read at, not accumulated. The sine is the part's odd polynomial on the folded phase, about
// -cos(pi p) with p the phase over -1..1; the triangle is the folded phase itself.
// §55 - the Phaser, in the part's own fractions (the engine's 1.0 is a quarter of full scale). State in
// gLadder: 0-2 the sections' X states, 3-5 their Y states, 6 the fed-back output, 7 the LFO counter
// over -1..1; gPulseCount counts the samples to the next control tick.
// §56 - each vowel's four formants as (frequency, gain) words, the instrument's table
static const int32_t kFltVoiceVowels[FLTVOICE_VOWELS][8] = {
    {341628, 8388607,  497288, 5332584, 1327619, 4331900, 1625086, 3326178},  // A
    {191744, 8388607, 1217437, 5332584, 1533876, 4331900, 1932563, 3326178},  // E
    {152193, 8388607, 1327619, 5332584, 1932563, 4331900, 2107472, 3326178},  // I
    {175818, 8388607,  361946, 5332584, 1327619,  375203, 1824096,  183333},  // O
    {175818, 8388607,  912048, 5332584, 1253104, 4331900, 1772165, 3326178},  // U
    {143648, 8388607, 1116391, 5332584, 1447787, 4331900, 1824096, 3326178},  // Y
    {221513, 8388607,  383462, 5332584, 1327619,  938010, 1824096,   72035},  // AA
    {430417, 8388607,  938768, 5332584, 1366541, 4331900, 1932563, 3326178},  // AE
    {255928, 8388607,  886082, 5332584, 1289832, 4331900, 1932563, 3326178},  // OE
};

// §56 - the host's resonance word: a straight line in the dial, squared, in its own 16-bit halves
static double fltvoice_res_word(double dial) {
    int32_t  param = (dial >= 127.0) ? (0x7FFFFF >> 1) : ((((int32_t)dial) << 16) >> 1);
    uint32_t u     = (uint32_t)param + 0x131745Du;
    uint32_t lo    = u & 0xFFFFu;
    int32_t  hi    = (int32_t)u >> 16;
    uint32_t x     = 0x7FFFFFu - (uint32_t)((((int32_t)((lo * 0x2Bu) + ((lo * 0xE72Au) >> 16)) + (hi * 0xE72A)) >> 7) + (hi * 0x5600));
    int32_t  hx    = (int32_t)((x * 2u) | (x >> 31)) >> 16;
    uint32_t lx    = (x * 2u) & 0xFFFEu;

    return (double)((((int32_t)(((uint32_t)hx * lx * 2u) + ((lx * lx) >> 16))) >> 7) + (hx * hx * 0x200)) / 8388608.0;
}

static void fltvoice_build(tEngineNode * node, tModule * module, uint32_t variation) {
    for (uint32_t k = 0; k < 3u; k++) {
        uint32_t v = (uint32_t)module->param[variation][FLTVOICE_PARAM_VOWEL1 + k].value;

        node->vowel[k] = (uint8_t)((v < FLTVOICE_VOWELS) ? v : 0u);
    }

    node->gain      = fmin(1.0, type_ii_attenuator(param_value(module, variation, FLTVOICE_PARAM_LEVEL))) / 2.0;
    node->constant  = param_value(module, variation, FLTVOICE_PARAM_VOWEL);
    node->modAmount = dial_fraction(param_value(module, variation, FLTVOICE_PARAM_VOWELMOD));
    node->shape     = param_value(module, variation, FLTVOICE_PARAM_FREQ);
    node->depth     = dial_fraction(param_value(module, variation, FLTVOICE_PARAM_FREQMOD));
    node->resonance = fltvoice_res_word(param_value(module, variation, FLTVOICE_PARAM_RES));
    node->active    = (module->param[variation][FLTVOICE_PARAM_ACTIVE].value != 0);
}

static double phaser_sat(double x) {
    return (x > 1.0) ? 1.0 : ((x < -1.0) ? -1.0 : x);
}

static double phaser_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double input) {
    SE_LOCAL;

    if (spec->active == false) {
        return phaser_sat(input / 4.0) * 4.0;   // bypassed: no work while off
    }
    double * st       = gLadder[voice][n];
    bool     typeII   = (spec->select != 0u);
    double   in       = phaser_sat(input / 4.0);
    uint32_t ticks    = (uint32_t)lround(gSampleRate / PHASER_TICK_HZ);

    if (gPulseCount[voice][n] == 0u) {
        // the counter steps, then the sweep is worked out from it
        double step = spec->rateHz * 2.0 / PHASER_TICK_HZ;
        double p    = st[7] + step;

        st[7]                 = (p >= 1.0) ? (p - 2.0) : p;
        gPulseCount[voice][n] = (ticks > 0u) ? ticks : 1u;
    }
    gPulseCount[voice][n]--;

    double   x        = (2.0 * fabs(st[7])) - 1.0; // the triangle; the sine is its polynomial
    double   sweep    = 0.0;

    if (typeII == false) {
        double x2 = x * x;

        sweep = x * (1.570404 - (x2 * (0.645885 - (x2 * 0.071614))));
    } else {
        double v = phaser_sat(0.5 + (x / 2.0));

        sweep = phaser_sat((2.0 * v * v) - 1.0);
    }
    double   g        = typeII ? ((0x03CE3A + (sweep * 0x02BE49)) / 8388608.0) : ((0x0EC973 + (sweep * 0x0940CC)) / 8388608.0);
    double   c        = typeII ? (0x79FFFF / 8388608.0) : (0x5F0000 / 8388608.0);
    double   b        = in + (spec->depth * st[6]);
    double   a        = 0.0;
    double   mid      = 0.0;
    uint32_t sections = typeII ? 3u : 2u;

    for (uint32_t k = 0; k < 3u; k++) {
        double s = st[k];
        double t = st[3 + k];
        double w = 0.0;

        a         = t + (2.0 * g * s);
        b         = b - a;
        st[3 + k] = phaser_sat(a);
        b         = b - (2.0 * c * s);
        w         = phaser_sat(b);
        st[k]     = phaser_sat(s + (2.0 * g * w));
        a         = (a + w) - (2.0 * c * st[k]);

        if (k == 1u) {
            mid = phaser_sat(a);
        }
        b         = a;
    }

    double chain = (sections == 3u) ? phaser_sat(a) : mid;
    double dry   = typeII ? (4200000.0 / 8388608.0) : (0x47FFFF / 8388608.0);
    double wet   = typeII ? (0x4EF6D8 / 8388608.0) : (0x453000 / 8388608.0);

    st[6] = chain;

    return phaser_sat((in * dry) + (chain * wet)) * 4.0;
}

// §56 - FltVoice. The control part places the vowel (-1 the first, 0 the second, +1 the third), shifts
// every formant by Freq in semitones, and hands four (damping, frequency, gain) triples to the audio
// part: four resonators on the one input, summing the first's low-pass and the others' band-pass.
// State in gLadder: band-pass then low-pass, per formant.
static double fltvoice_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double input, double vowelIn,
                            double freqIn) {
    SE_LOCAL;

    double *        st   = gLadder[voice][n];
    double          in   = phaser_sat(input / 4.0);
    double          pos  = phaser_sat(((spec->constant >= 127.0) ? 1.0 : ((spec->constant - 64.0) / 64.0))
                                      + (vowelIn * spec->modAmount * 2.0));
    double          semi = phaser_sat((((spec->shape >= 127.0) ? 1.0 : ((spec->shape - 64.0) / 64.0)) / 1.0)
                                      + (freqIn * spec->depth * 2.0)) * 32.0;
    double          mult = exp2((semi + FLTVOICE_FINE_OFFSET) / 12.0);    // §56.1
    const int32_t * mid  = kFltVoiceVowels[spec->vowel[1]];
    const int32_t * side = kFltVoiceVowels[spec->vowel[(pos < 0.0) ? 0 : 2]];
    double          span = fabs(pos);
    double          sum  = 0.0;

    if (spec->active == false) {
        return input;
    }

    for (uint32_t k = 0; k < 4u; k++) {
        double f  = phaser_sat(((mid[2u * k] + (span * (side[2u * k] - mid[2u * k]))) / 8388608.0) * mult);
        double g  = phaser_sat(((mid[(2u * k) + 1u] + (span * (side[(2u * k) + 1u] - mid[(2u * k) + 1u]))) / 8388608.0)
                               * spec->gain);
        double bp = st[2u * k];
        double lp = phaser_sat(st[(2u * k) + 1u] + (f * bp));
        double hp = phaser_sat((g * in) - (spec->resonance * bp) - lp);

        st[(2u * k) + 1u] = lp;
        st[2u * k]        = phaser_sat(bp + (f * hp));
        sum              += (k == 0u) ? lp : st[2u * k];
    }

    return phaser_sat(sum) * 4.0;
}

// §57 - FreqShift. Each chain is four allpass sections in z^-2, (c + z^-2) / (1 + c z^-2); the second
// chain hears the input a sample late. Two phases a quarter cycle apart, over -1..1, drive the part's
// sine polynomial; Down is cos H1 + sin H2 and Up cos H1 - sin H2. State: 0-7 and 8-15 the sections'
// two-sample memories, 16 and 17 the phases, 18 the delayed input.
static const double kFreqShiftCoef[2][4] = {
    {-1896664.0 / 8388608.0, -7007843.0 / 8388608.0, -8200703.0 / 8388608.0, -8365959.0 / 8388608.0},
    {-5046587.0 / 8388608.0, -7869353.0 / 8388608.0, -8321499.0 / 8388608.0, -8382736.0 / 8388608.0},
};

static double freqshift_sine(double phase) {
    double x  = (2.0 * fabs(phase)) - 1.0;
    double x2 = x * x;

    return x * (1.570404 - (x2 * (0.645885 - (x2 * 0.071614))));
}

static double freqshift_wrap(double p) {
    return (p >= 1.0) ? (p - 2.0) : ((p < -1.0) ? (p + 2.0) : p);
}

// §58 - the step sequencers' parameters as the part's words. SeqVal's values read v x 2^14 (127 = 64
// units) and Bip/Uni is its own switch; SeqNote's are notes, always Bipolar, so v - 64 is semitones
// from E4; SeqEvent's two rows are both on/off. An on/off step is 0 or 64 units.
static void seq_config_build(tSeqConfig * cfg, tModule * module, uint32_t variation) {
    bool event = (module->type == moduleTypeSeqEvent);

    memset(cfg, 0, sizeof(*cfg));

    for (uint32_t k = 0; k < SEQ_STEPS; k++) {
        int32_t v1 = (int32_t)param_value(module, variation, k);
        int32_t v2 = (int32_t)param_value(module, variation, SEQ_STEPS + k);

        cfg->table[2u * k]        = event ? ((v1 != 0) ? 0x200000 : 0) : ((v1 >= 127) ? 0x200000 : (v1 << 14));
        cfg->table[(2u * k) + 1u] = (v2 != 0) ? 0x200000 : 0;
    }

    cfg->controlRate = (module->upRate == 0u);
    cfg->cycle       = (module->param[variation][SEQ_PARAM_CYCLE].value != 0);
    cfg->length      = (int32_t)module->param[variation][SEQ_PARAM_LENGTH].value;

    if ((module->type == moduleTypeSeqVal) || (module->type == moduleTypeSeqLev)) {
        cfg->bipolar = (module->param[variation][34].value == 0);   // bipUniStrMap
        cfg->gate[0] = 1u;
        cfg->gate[1] = (uint8_t)(module->param[variation][35].value != 0);
    } else if (module->type == moduleTypeSeqNote) {
        cfg->bipolar     = 1u;
        cfg->gate[0]     = 1u;
        cfg->gate[1]     = (uint8_t)(module->param[variation][34].value != 0);
        cfg->record      = 1u;
        // §58.1 - the record part's bank: 0x7FFFFF where it runs at audio rate, 0x10CC at control rate
        cfg->recordDelay = (module->upRate != 0u) ? SEQREC_DELAY_OFF : SEQREC_DELAY_24K;
    } else {
        cfg->gate[0] = (uint8_t)(module->param[variation][34].value != 0);
        cfg->gate[1] = (uint8_t)(module->param[variation][35].value != 0);
    }
}

static int32_t seq_sat(int64_t v) {
    return (v > 0x7FFFFF) ? 0x7FFFFF : ((v < -0x800000) ? -0x800000 : (int32_t)v);
}

// §58 - the part's arithmetic, word for word: the sign of -previous x now is how it sees an edge
typedef struct {
    uint32_t hi;
    uint32_t lo;
} tSeqTest;

static tSeqTest seq_edge(int32_t previous, uint32_t now) {
    uint32_t p = (uint32_t)(-previous) * now;
    tSeqTest t = {(p >> 23) | ((uint32_t)((uint64_t)((int64_t)(-previous) * (int64_t)(int32_t)now) >> 32) << 9), p * 0x200u};

    if (0x7FFFFFFFu < (t.hi ^ 0x80000000u)) {
        t.hi = now;   // the same sign or a zero: the test falls to the new value itself
        t.lo = 0u;
    }
    return t;
}

static bool seq_above(tSeqTest t) {
    return (0x80000000u < (t.hi ^ 0x80000000u)) || ((0x80000000u - (t.hi ^ 0x80000000u)) < (uint32_t)(t.lo != 0u));
}

// §58 - one sample (or control tick) of the 16-step part. in[] is Clk, Rst, Loop, Park, then the
// value row's and the other row's inputs, which are added to their outputs; out[] is Link, value row,
// other row. State words keep the part's own numbering (reference §58).
static void seq16_step(tSeqState * st, const tSeqConfig * cfg, const int32_t in[6], int32_t out[3]) {
    int32_t * X = st->x;
    int32_t * Y = st->y;

    if (st->ready == false) {
        static const int32_t kX[SEQ_X_WORDS]       = {0x200000, 0, 0x2B, 1, 0, 0, 0, 0, 0, 0, 0x80000, 7, 0, 0, 0, 0};
        static const int32_t kRecX[SEQREC_X_WORDS] = {0, 0, 0x7F, 0x80, 0x200000, 0};

        memcpy(X, kX, sizeof(kX));
        memset(Y, 0, sizeof(st->y));
        Y[1]             = 0x10;
        Y[3]             = 0x200000;
        Y[5]             = 0x11;
        Y[6]             = 0x2B;
        memcpy(st->rx, kRecX, sizeof(kRecX));
        Y[SEQREC_Y]      = SEQREC_LAST_STEP;
        Y[SEQREC_Y + 1u] = 0x200000;

        for (uint32_t k = 0; k < (2u * SEQ_STEPS); k++) {
            Y[7u + k]     = cfg->table[k];
            st->loaded[k] = cfg->table[k];
        }

        st->ready        = true;
    }
    // the words the switches set, every sample, so a change applies without a restart
    X[8]  = cfg->length;
    X[4]  = cfg->length + 1;
    X[7]  = (cfg->length < 1) ? cfg->length : (cfg->length - 1);
    X[14] = cfg->bipolar ? 0x100000 : 0;

    for (uint32_t k = 0; k < (2u * SEQ_STEPS); k++) {
        if (cfg->table[k] != st->loaded[k]) {
            Y[7u + k]     = cfg->table[k];
            st->loaded[k] = cfg->table[k];
        }
    }

    Y[39] = cfg->bipolar ? 0x100000 : 0;   // one step past the table: the value a 17th step would read
    Y[41] = Y[39];

    uint32_t high      = (uint32_t)Y[3];
    uint32_t pend      = (uint32_t)Y[0];
    tSeqTest t         = seq_edge(X[0], (uint32_t)in[1]); // Rst

    X[0]  = in[1];

    if (seq_above(t)) {
        pend = high;
    }
    t     = seq_edge(X[1], (uint32_t)in[2]);           // Loop
    X[1]  = in[2];

    if (seq_above(t)) {
        pend = high;
    }

    if (0x80000000u < ((uint32_t)Y[2] ^ 0x80000000u)) {
        pend = 0u;
    }
    uint32_t test      = (uint32_t)Y[4];
    uint32_t step      = (uint32_t)Y[1];

    if (0x80000000u < (test ^ 0x80000000u)) {
        step += (uint32_t)X[3];
        test  = pend;
        pend  = (0x80000000u < (test ^ 0x80000000u)) ? 0u : pend;
        step  = (0x80000000u < (test ^ 0x80000000u)) ? 0u : step;
    }
    int32_t  pair      = Y[X[2]];

    if ((cfg->cycle != 0u) && ((int32_t)step == X[4])) {
        step = 0u;
    }
    Y[0]      = seq_sat((int32_t)pend);
    Y[1]      = seq_sat((int32_t)step);

    uint32_t flag      = 0u;
    uint32_t hold      = (uint32_t)X[6];

    t         = seq_edge(X[5], (uint32_t)in[0]);       // Clk
    X[5]      = in[0];

    if (seq_above(t)) {
        flag = high;
    }
    step      = (uint32_t)Y[1];
    Y[4]      = seq_sat((int32_t)flag);

    uint32_t mark      = hold;

    if (seq_above(t)) {
        t.hi = step - (uint32_t)X[7];
        t.lo = 0u;
    }

    if (seq_above(t)) {
        mark = high;
    }
    uint32_t over      = step - (uint32_t)X[8];
    int32_t  last      = Y[5];

    if (0x80000000u < (over ^ 0x80000000u)) {
        mark = high;
        step = (uint32_t)last;
    }
    Y[1]      = seq_sat((int32_t)step);

    if (0x80000000u < ((uint32_t)Y[0] ^ 0x80000000u)) {
        mark = high;
    }
    out[0]    = seq_sat((int32_t)mark);                // Link

    t         = seq_edge(X[9], (uint32_t)in[3]);       // Park
    X[9]      = in[3];

    int32_t  rest      = Y[0];
    int32_t  at        = Y[1];

    if (seq_above(t)) {
        rest = 0;
        at   = last;
    }
    Y[0]      = seq_sat(rest);
    Y[1]      = seq_sat(at);

    uint32_t stop      = (uint32_t)Y[2];

    if (seq_above(t)) {
        stop = high;
    }

    if (!seq_above(t)) {
        t.hi = stop - (uint32_t)X[10];
        t.lo = 0u;
        stop = t.hi;
    }

    if (!seq_above(t)) {
        stop = 0u;
    }
    Y[2]      = seq_sat((int32_t)stop);

    int32_t  clockThen = X[13];
    int32_t  target    = Y[6];

    X[13]     = X[12];
    X[12]     = in[0];
    Y[target] = seq_sat(((int64_t)Y[1] * 2) + X[11]);

    int32_t  value     = Y[pair];
    int32_t  other     = Y[pair + 1];
    bool     low       = ((uint32_t)clockThen ^ 0x80000000u) < 0x80000001u; // the clock two ticks back not high

    if ((cfg->gate[0] == 0u) && low) {
        value = 0;
    }

    if ((cfg->gate[1] == 0u) && low) {
        other = 0;
    }
    value    -= X[14];

    if (cfg->bipolar) {
        value *= 2;
    }
    out[1]    = seq_sat((int64_t)value + in[4]);
    out[2]    = seq_sat((int64_t)other + in[5]);
}

// §58.1 - one tick of SeqNote's record part, after the 16-step part: `value` is that part's value row.
// While Rec Enable is high the output is Rec itself, and once armed the current step takes Rec
// rounded to a note.
static int32_t seqrec_step(tSeqState * st, int32_t delay, int32_t value, int32_t rec, int32_t enable) {
    int32_t * X     = st->rx;
    int32_t * Y     = &st->y[SEQREC_Y];
    int32_t   at    = Y[-1];                                                     // the 16-step part's word 43: the current step's value word
    int64_t   count = (enable > 0) ? ((int64_t)X[1] + (2 * (int64_t)delay)) : 0; // two adds a tick
    bool      armed = (enable > 0) && (count > 0x7FFFFF);
    int64_t   sum   = (int64_t)Y[1] + rec;
    int64_t   note  = 0;

    Y[2] = delay;
    X[0] = value;
    X[1] = seq_sat(count);
    Y[3] = rec;

    if (sum >= 0) {
        int64_t rest = sum & 0x7FFF;

        note = sum >> 15;

        if ((rest > 0x4000) || ((rest == 0x4000) && ((note & 1) != 0))) {   // rnd: convergent
            note++;
        }
    }

    if (note >= X[2]) {
        note = X[3];
    }
    X[5] = armed ? X[4] : 0;

    if ((at >= 0) && (at < SEQREC_Y) && armed && (at <= Y[0])) {
        st->y[at] = seq_sat(note << 14);
    }
    X[0] = (enable > 0) ? Y[3] : X[0];
    return X[0];
}

static int32_t sext24(int32_t w) {
    return (w & 0x800000) ? (w | ~0xFFFFFF) : (w & 0xFFFFFF);
}

static int32_t word_mulhi(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 23);
}

// §59 - one control tick of ClkGen (internal clock). The phase covers one "sync every" period and
// steps by tempo x 0x55555 >> sync; out[] is 1/96, 1/16, Sync, Active in the part's own order.
static void clkgen_tick(tClkGenState * st, const tClkGenConfig * cfg, int32_t rst) {
    int32_t * X = st->x;
    int32_t * Y = st->y;

    if (st->ready == false) {
        static const int32_t kX[13] = {8, 0, 0x200000, 0, 0x20000, 0xFFFFFF, 0, 0xFFFFFF, 0xFFFFFF, 0x200000, 0, 6, 0x133333};
        static const int32_t kY[9]  = {0x42, 0x15555, 0, 0xA3D7, 0x66666, 4, 0x866666, 0x200000, 0x800000};

        memset(st, 0, sizeof(*st));

        for (uint32_t k = 0; k < 13u; k++) {
            X[k] = sext24(kX[k]);
        }

        for (uint32_t k = 0; k < 9u; k++) {
            Y[k] = sext24(kY[k]);
        }

        st->ready = true;
    }
    // the words the dials set
    Y[2]       = cfg->tempo;
    X[4]       = 0x80000 >> cfg->sync;
    Y[5]       = 1 << cfg->sync;
    Y[1]       = 0x55555 >> cfg->sync;
    Y[8]       = sext24((cfg->swing * 0x8000) + 0x800000);
    Y[6]       = sext24((cfg->swing * 0x8000) + 0x866666);

    // Rst: a rising edge restarts the phase
    tSeqTest t         = seq_edge(X[1], (uint32_t)rst);
    bool     reset     = seq_above(t);
    int32_t  restart   = reset ? X[8] : X[6];
    int32_t  previous  = X[7];

    X[1]       = rst;
    X[6]       = seq_sat(restart);
    X[7]       = seq_sat(reset ? 0 : previous);

    int32_t  running   = X[3];

    X[3]       = seq_sat(X[2]);

    if (cfg->active == 0u) {
        running = 0;
    }
    int32_t  phase     = X[7] + word_mulhi(Y[2], Y[1]);

    X[8]       = seq_sat(phase);
    phase      = sext24(phase);

    if (running == 0) {
        phase = X[5];
    }
    st->out[3] = seq_sat(running);   // Active
    X[7]       = seq_sat(phase);

    int32_t  sync      = X[9];

    if ((phase - Y[3]) >= 0) {
        sync = 0;
    }

    if (phase < 0) {
        sync = 0;
    }
    int32_t  whole     = (phase < -0x800000) ? 0 : ((phase > 0x7FFFFF) ? 0x7FFFFF : phase);
    int32_t  eighth    = (int32_t)((uint32_t)whole * (uint32_t)Y[5] * 0x200u) >> 8; // wraps twice a beat

    st->out[2] = seq_sat(sync);                                                     // Sync

    int32_t  sixteenth = 0;

    if ((eighth - Y[4]) < 0) {
        sixteenth = Y[7];
    }
    int32_t  clamped   = (eighth < -0x800000) ? 0 : ((eighth > 0x7FFFFF) ? 0x7FFFFF : eighth);

    if ((eighth - X[10]) < 0) {
        sixteenth = 0;
    }

    if ((eighth - Y[6]) < 0) {
        sixteenth = Y[7];   // the swung half: its window moves with Swing
    }

    if ((eighth - Y[8]) < 0) {
        sixteenth = 0;
    }
    st->out[1] = seq_sat(sixteenth);                                                   // 1/16

    int32_t  sub       = (int32_t)((uint32_t)X[11] * (uint32_t)clamped * 0x200u) >> 8; // six to a 16th
    int32_t  tick      = ((sub - X[12]) >= 0) ? 0 : Y[7];

    if (sub < 0) {
        tick = 0;
    }
    st->out[0] = seq_sat(tick);      // 1/96
}

static void freqshift_step(uint32_t voice, const tEngineNode * spec, double input, double modIn, double * down, double * up) {
    SE_LOCAL;

    if ((spec->line >= MAX_FREQSHIFT_LINES) || (spec->active == false)) {
        *down = input;
        *up   = input;
        return;
    }
    double * st = gFreqShift[voice][spec->line];
    double   in = phaser_sat(input / 4.0);
    double   h[2];

    for (uint32_t chain = 0; chain < 2u; chain++) {
        double u = (chain == 0u) ? in : st[18];

        for (uint32_t k = 0; k < 4u; k++) {
            double * z = &st[(8u * chain) + (2u * k)];
            double   c = kFreqShiftCoef[chain][k];
            double   w = u - (c * z[0]);

            u    = z[0] + (c * w);
            z[0] = z[1];
            z[1] = w;
        }

        h[chain] = u;
    }

    st[18] = in;

    double   x   = fmax(0.0, fmin(1.0, spec->constant + (modIn * spec->modAmount)));
    double   inc = x * x * x * spec->depth;

    st[16] = freqshift_wrap(st[16] + inc);
    st[17] = freqshift_wrap(st[17] + inc);

    double   c1  = freqshift_sine(st[17]);
    double   s1  = freqshift_sine(st[16]);

    *down  = phaser_sat((c1 * h[0]) + (s1 * h[1])) * 4.0;
    *up    = phaser_sat((c1 * h[0]) - (s1 * h[1])) * 4.0;
}

static double osc_corner(double distance, double inc96, double squareLimit);

static double osc_pm_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double hz, double phaseMod, bool sync) {
    SE_LOCAL;

    if (sync == true) {    // §6.6 - the accumulator restarts from the sync word, then takes this sample's step
        gPhase[voice][n] = OSC_PM_SYNC_PHASE;
    }
    static const double c1 = (double)0x64803500 / 1073741824.0;
    static const double c3 = -(double)0x29159580 / 1073741824.0;
    static const double c5 = (double)0x4955480 / 1073741824.0;
    double              at = advance_phase(&gPhase[voice][n], hz / gSampleRate)
                             + (OSCPM_CYCLES_PER_UNIT * phaseMod * spec->modAmount);
    double              p  = (2.0 * (at - floor(at))) - 1.0;
    double              x  = (2.0 * fabs(p)) - 1.0;

    if ((uint32_t)spec->wave != 0u) {
        // §53 - the triangle part OscC and OscD use: its corners rounded as theirs are (§6.3)
        double c     = at - floor(at);
        double inc96 = hz / OSC_INSTRUMENT_RATE;
        double top   = (c < 0.5) ? c : (c - 1.0);

        return x + osc_corner(c - 0.5, inc96, OSC_CORNER_LIMIT_PARTS) - osc_corner(top, inc96, OSC_CORNER_LIMIT_PARTS);
    }
    double              x2 = x * x;

    return x * (c1 + (x2 * (c3 + (x2 * c5))));
}

// notes §143
// notes §61
static double smooth_to(tParamRamp * ramp, double target, double samples, bool primed) {
    if (primed == false) {
        ramp->value  = target;
        ramp->target = target;
        ramp->step   = 0.0;
        return target;
    }

    if (target != ramp->target) {
        ramp->target = target;
        ramp->step   = (target - ramp->value) / samples;
    }

    if (ramp->step != 0.0) {
        ramp->value += ramp->step;

        if (((ramp->step > 0.0) && (ramp->value >= target)) || ((ramp->step < 0.0) && (ramp->value <= target))) {
            ramp->value = target;
            ramp->step  = 0.0;
        }
    }
    return ramp->value;
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

#define FLTNORD_H_MAX         (0x518368 / 8388608.0)  // §23.2 - h at most: 20.8 kHz
#define FLTNORD_RES_SCALE     (0x7eb852 / 8388608.0)  // 0.99; band-reject takes half
#define FLTNORD_H_WORD_MAX    (8388607.0 / 8388608.0) // §23.5 - the pitch part's h saturates to a word
#define FLTNORD_LEAK          (0.9)                   // §23.3 - HP and BR take back 0.9 of their last output

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

static double flt_nord_mod_amount(double dial) {
    return (dial >= 127.0) ? 1.0 : (dial / 128.0);
}

// §23 - FltNord: one stage, or two of the same type for 24 dB (band-reject stays one).
static double nord_filter(double * state, double input, double half, double resDial, tFilterShape shape, bool slope24, bool gainComp,
                          double fm, double resMod) {
    double h  = fabs(fmin(half, FLTNORD_H_WORD_MAX) + fm);                           // §23.5
    double r  = fmin(fmax(flt_nord_mod_amount(resDial) + resMod, -1.0), 1.0);        // §23.5

    h = (h >= 1.0) ? 0.0 : fmin(h, FLTNORD_H_MAX);
    double d  = fmin(1.0 - (((shape == eFilterShapeBandReject) ? 0.5 : FLTNORD_RES_SCALE) * r), 1.0);
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

// §8.2 - two unity-peak band-passes in series, then the measured level.
static double oscnoise_word(double v) {
    return fmin(1.0, fmax(-1.0, v));
}

// §8 - LFSR noise, a one-pole tilt, then three gain-compensated Chamberlin band-passes at the pitch,
// all in the instrument's own words (a fraction of the 24-bit word, which saturates at +-1)
static double oscnoise_step(uint32_t voice, uint32_t node, double hz, double width) {
    SE_LOCAL;

    double * st    = gOscNoiseState[voice][node];     // tilt, then (input before, low, band) per section
    uint32_t lfsr  = (gNoiseSeed[voice][node] & 0xFFFFFFu) << 1;
    double   pole  = pow(OSCNOISE_TILT_POLE, OSC_INSTRUMENT_RATE / gSampleRate);
    double   topHz = OSCNOISE_H_MAX * OSC_INSTRUMENT_RATE / M_PI;
    double   h     = M_PI * fmin(fmax(hz, 0.0), topHz) / gSampleRate;
    double   w     = fmin(1.0, fmax(0.0, width));
    double   q     = OSCNOISE_Q_BASE - (OSCNOISE_Q_WIDTH * w * w);
    double   d     = oscnoise_word(1.0 - (OSCNOISE_DAMP_SPAN * q));
    double   d2    = d * d;
    double   gain  = 2.0 * (d2 + (fmin(d, OSCNOISE_GAIN_DAMP_MAX) / 16.0));
    double   fb    = d2 * (1.0 - h);
    double   x;

    lfsr                    = ((lfsr & 0x1000000u) != 0u) ? ((lfsr ^ OSCNOISE_LFSR_TAPS) & 0xFFFFFFu) : (lfsr & 0xFFFFFFu);
    gNoiseSeed[voice][node] = (lfsr == 0u) ? 5555u : lfsr;
    st[0]                   = oscnoise_word((pole * st[0]) + (((1.0 - pole) / 4.0) * ((double)(((int32_t)(lfsr << 8)) >> 8) / OSCNOISE_Q23)));
    x                       = st[0];

    for (uint32_t k = 0; k < OSCNOISE_SECTIONS; k++) {
        double * sec  = &st[1u + (3u * k)];
        double   pre  = gain * (x + sec[0]) * 0.5;
        double   low  = oscnoise_word(sec[1] + (h * sec[2]));
        double   high = oscnoise_word(pre - low - (fb * sec[2]));

        sec[0] = x;
        sec[1] = low;
        sec[2] = oscnoise_word(sec[2] + (4.0 * h * high));
        x      = oscnoise_word(sec[2] * (1.0 - h));
    }

    return oscnoise_word(4.0 * x) * DSP_FULL_SCALE;
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
// §14.4 - where the envelope sits below its dials for this note and velocity: Level, keyboard level
// scaling about the break point, and Vel, as one offset added to every segment's target. Kept at the
// accumulator's precision (a word is 1 << 24), as the instrument carries it into the target.
static int64_t dx_level_offset(const tDxOperator * op, int32_t noteWord, uint32_t velIndex) {
    bool     left   = (noteWord < op->breakPoint);
    int32_t  span   = abs(noteWord - op->breakPoint) >> 16;
    uint32_t curve  = left ? op->lCurve : op->rCurve;
    int64_t  depth  = left ? op->lDepth : op->rDepth;
    int64_t  steps  = (span > DX_KBSCALE_SPAN_MAX) ? DX_KBSCALE_SPAN_MAX : span;
    int64_t  exp    = (curve == 1u) || (curve == 2u);
    int64_t  shape  = (steps * DX_KBSCALE_LIN_NUM) / (DX_KBSCALE_LIN_DEN << (2 * exp));
    int64_t  scaled = (2 * depth * shape) << DX_KBSCALE_SHIFT;
    int64_t  level  = ((int64_t)op->outLevel * DX_ACC_WORD) + ((curve < 2u) ? -scaled : scaled);

    if (level > 0) {
        level = 0;
    }
    return level + ((int64_t)DX_LEVEL_BIAS * DX_ACC_WORD) + (2 * (int64_t)kDxVelocityWords[velIndex] * op->keyVel);
}

// §14.4 - how many rate steps faster this note runs the envelope
static int32_t dx_rate_offset(const tDxOperator * op, int32_t noteWord) {
    int64_t sum = 2 * (int64_t)op->rateScale * (noteWord + DX_RATESCALE_KEY_OFFSET);

    return (sum < 0) ? 0 : (int32_t)(sum >> 39);
}

// §14.2 - one tick of an Operator's envelope: the log level steps towards the segment's target and,
// landing on it, moves on to the next segment. A holding segment never lands.
static void dx_envelope_tick(uint32_t voice, uint32_t slot, const tDxOperator * op, int64_t offset, int32_t rateOffset) {
    SE_LOCAL;

    uint32_t seg   = gDxEnvStage[voice][slot];
    int64_t  level = gDxLevel[voice][slot];
    bool     hold  = (seg == (uint32_t)eDxHold) || (seg == (uint32_t)eDxIdle);
    uint32_t s     = (seg == (uint32_t)eDxRelease) ? 3u : seg;
    int64_t  goal  = ((int64_t)op->level[s] * DX_ACC_WORD) + offset;
    int32_t  rate  = hold ? 0 : op->rate[s];
    int64_t  step  = 0;

    goal = hold ? DX_HOLD_MARK : (((goal < 0) ? 0 : goal) >> 24);

    if (rate > 0) {
        rate = ((rate + rateOffset) > DX_RATE_TOP) ? DX_RATE_TOP : (rate + rateOffset);
    }

    if (hold == false) {
        if (level < goal) {
            int64_t inc = kDxAttackWords[rate];

            step  = (level < DX_ATTACK_JUMP_BELOW) ? ((int64_t)DX_ATTACK_JUMP * DX_ACC_WORD) : 0;
            step += 2 * inc * ((level < DX_ATTACK_LOW_BELOW) ? DX_ATTACK_LOW : 0);
            step += 2 * inc * ((level < DX_ATTACK_MID_BELOW) ? DX_ATTACK_MID : 0);
            step += 2 * inc * DX_ATTACK_BASE;
        } else {
            step = 2 * (int64_t)kDxDecayWords[rate] * DX_DECAY;
        }
        step >>= 24;
    }

    if (llabs(level - goal) <= step) {
        level = goal;
    } else {
        level += (level < goal) ? step : -step;
    }

    if ((level == goal) && (seg < (uint32_t)eDxIdle)) {
        seg++;
    }
    gDxLevel[voice][slot]    = (int32_t)level;
    gDxEnvStage[voice][slot] = seg;
}

// §14.2 - the amplitude a log level reads, between whole steps of kDxAmpWords
static double dx_amplitude(int32_t level) {
    uint32_t whole = (uint32_t)level >> 16;
    double   part  = (double)(level & 0xffff) / 65536.0;
    double   lo    = kDxAmpWords[whole];

    return (lo + (part * ((double)kDxAmpWords[whole + 1u] - lo))) / DSP_WORD_PER_ENGINE;
}

// §14 - one sample of a DXRouter and its Operators, for one voice.
static double dx_step(uint32_t voice, uint32_t node, const tEngineNode * spec, const tDxOperator * ops, double voicePitch) {
    SE_LOCAL;

    const tDxAlgorithm * alg               = dx_algorithm(spec->dxAlgorithm);
    double               out[DX_OPERATORS] = {0.0};
    double               mix               = 0.0;
    bool                 gate              = gVoice[voice].gate;
    bool                 strike            = gate && ((gDxGate[voice][node] == false) || (gDxTrigger[voice][node] != gVoice[voice].trigger));
    double               note              = (voicePitch >= 0.0) ? voicePitch : 64.0;
    double               noteHz            = 440.0 * exp2((note - MIDI_NOTE_A440) / 12.0);
    bool                 tick              = (gEnvTick[voice][node] <= 0.0);

    // §14.4 - what the Operators' Note and Vel inputs carry from the Keyboard
    int32_t              noteWord          = (gVoice[voice].note >= 0) ? ((gVoice[voice].note - (int32_t)KEYBOARD_PITCH_ZERO) * OP_NOTE_WORD) : 0;
    uint32_t             velIndex          = (((uint32_t)gVoice[voice].velocity * (uint32_t)DSP_WORD_PER_ENGINE) / 127u) >> DX_VEL_INDEX_SHIFT;

    velIndex               = (velIndex > 127u) ? 127u : velIndex;

    if (tick == true) {
        gEnvTick[voice][node]  += 1.0;
        gDxGate[voice][node]    = gate;
        gDxTrigger[voice][node] = gVoice[voice].trigger;
    }
    gEnvTick[voice][node] -= ENV_TICK_HZ / gSampleRate;

    // Modulators before what they modulate: every DX7 modulation runs from a higher operator to a lower one.
    for (int32_t k = DX_OPERATORS - 1; k >= 0; k--) {
        // §26.2 - the parameters come from the voice's own set; the state arrays stay keyed on dxBase
        uint32_t            slot = spec->dxBase + (uint32_t)k;
        const tDxOperator * op   = &ops[k];
        double              fm   = 0.0;
        double              hz   = 0.0;
        double              y    = 0.0;

        if (op->present == false) {
            continue;
        }

        if (tick == true) {
            // §14.2 - the level steps first; then a gate edge or a gate down moves the segment
            dx_envelope_tick(voice, slot, op, dx_level_offset(op, noteWord, velIndex), dx_rate_offset(op, noteWord));

            if (strike == true) {
                gDxEnvStage[voice][slot] = eDxRise1;

                if (op->sync == true) {
                    gDxPhase[voice][slot] = 0.0;
                }
            } else if (gate == false) {
                gDxEnvStage[voice][slot] = eDxRelease;
            }
            gDxAmp[voice][slot] = dx_amplitude(gDxLevel[voice][slot]);
        }

        for (uint32_t m = (uint32_t)k + 1u; m < DX_OPERATORS; m++) {
            if ((alg->target[m] & (1u << (uint32_t)k)) != 0) {
                fm += out[m];
            }
        }

        if ((uint32_t)k == (alg->feedbackTo - 1u)) {    // §14.3 - the source's last sample
            fm += spec->dxFeedback * gDxOut[voice][spec->dxBase + alg->feedbackFrom - 1u][0];
        }
        hz                     = op->fixed ? op->fixedHz : ((op->kbt ? noteHz : DX_E4_HZ) * op->ratio);
        gDxPhase[voice][slot] += (hz * op->detune) / gSampleRate;
        gDxPhase[voice][slot] -= floor(gDxPhase[voice][slot]);

        if (op->active == true) {
            y = sin(2.0 * M_PI * (gDxPhase[voice][slot] + (DX_FM_CYCLES_PER_UNIT * fm))) * gDxAmp[voice][slot];
        }
        gDxOut[voice][slot][0] = y;
        out[k]                 = y;
    }

    for (uint32_t k = 0; k < DX_OPERATORS; k++) {
        if (((alg->target[k] == 0) || ((alg->alsoMain & (1u << k)) != 0)) && (ops[k].present == true)) {
            mix += out[k];
        }
    }

    return 2.0 * mix * (kDxMainWords[(spec->dxAlgorithm < DX_ALGORITHMS) ? spec->dxAlgorithm : 0u] / DSP_WORD_SCALE);   // §14.5
}

// True while any of the router's Operators holds a level above silence, or the key is down.
static bool dx_voice_sounding(const tSoundEngineParams * params, const tEngineNode * spec, uint32_t voice) {
    SE_LOCAL;

    for (uint32_t k = 0; k < DX_OPERATORS; k++) {
        uint32_t slot = spec->dxBase + k;

        if ((params->dxOp[slot].present == true) && ((gVoice[voice].gate == true) || (gDxLevel[voice][slot] > 0))) {
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

// §70.13 - a controller value as the part takes it: v x 2^14, 127 full
static double midi_cc_level(uint8_t value) {
    return (value >= 127u) ? 1.0 : ((double)value / 128.0);
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

static int32_t sat24(int64_t value) {
    return (int32_t)((value > ENV_TOP) ? ENV_TOP : ((value < -0x800000) ? -0x800000 : value));
}

// §17.11 - EnvMulti, one tick in the instrument's words. state: the segment's progress, the level it
// started from, the segment last ticked (+1, 0 for none) and the gate last seen.
static double envmulti_step(uint32_t voice, uint32_t node, const tEngineNode * spec, bool gate) {
    SE_LOCAL;

    if (gEnvTick[voice][node] <= 0.0) {
        double * state  = gLadder[voice][node];
        int32_t  level  = gEnvQ[voice][node];
        uint32_t seg    = gEnvStage[voice][node];
        bool     was    = (state[3] != 0.0);
        bool     retrig = (spec->envKeyGate == true) && (gEnvTrigger[voice][node] != gVoice[voice].trigger);

        gEnvTick[voice][node] += 1.0;

        if ((gate == true) && ((was == false) || (retrig == true))) {
            seg                      = 0u;
            gEnvTrigger[voice][node] = gVoice[voice].trigger;

            if (spec->envReset == true) {
                level = spec->envStage[ENVMULTI_SEGMENTS - 1u].target;    // Reset restarts from L4
            }
        } else if ((gate == false) && (was == true) && (spec->envSustainStage >= 0)) {
            seg = (uint32_t)spec->envSustainStage + 1u;    // a release starts the segment after the held one
        }
        state[3]               = (gate == true) ? 1.0 : 0.0;

        if (seg < ENVMULTI_SEGMENTS) {
            const tEnvSegment * s        = &spec->envStage[seg];
            bool                entered  = (state[2] != (double)(seg + 1u));

            if (entered == true) {
                state[0] = 0.0;
                state[1] = (double)level;
                state[2] = (double)(seg + 1u);
            }
            int64_t             progress = (int64_t)s->add + ((2 * (int64_t)s->half * (int64_t)state[0]) >> 23);
            int32_t             p        = sat24(progress);
            int32_t             start    = (int32_t)state[1];
            int32_t             delta    = sat24((int64_t)s->target - start);

            if (((uint32_t)spec->wave == (uint32_t)eEnvShapeLinLin) || (delta >= 0)) {
                level = sat24(start + (((int64_t)p * delta) >> 23));
            } else {
                level = sat24(s->target + (((int64_t)s->decay * ((int64_t)level - s->target)) >> 23));
            }
            state[0] = (double)p;

            if ((progress != p) && (s->sustain == 0u)) {
                seg++;    // the segment's time is up: the next starts on the next tick
            }
        }
        gEnvQ[voice][node]     = level;
        gEnvStage[voice][node] = seg;
        gEnvLevel[voice][node] = (double)level / ENV_FULL_SCALE_STEPS;
    }
    gEnvTick[voice][node] -= ENV_TICK_HZ / gSampleRate;
    return gEnvLevel[voice][node];
}

static double envelope_step(uint32_t voice, uint32_t node, const tEngineNode * spec, bool gate,
                            double value[][NODE_OUTPUTS]) {
    SE_LOCAL;

    if (spec->envMulti == true) {
        return envmulti_step(voice, node, spec, gate);
    }

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
    SE_LOCAL;

    int32_t source = spec->in[input];

    if ((input >= spec->inCount) || (source < 0)) {
        return 0.0;
    }

    if ((spec->backMask & (1u << input)) != 0u) {
        return gBackValue[sEvalVoice][spec->backSlot[input]];   // notes §192 - last sample's
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
#define OSCDUAL_PW_DEPTH       (1.0)   // §12.4 - a unit input at full amount moves PW one dial range
#define OSCDUAL_PHASE_DEPTH    (0.5)   // §12.4 - half a cycle

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
            return osc_offset_pulse(phase, edge, fmin(fmax(shape, SHAPE_WORD_MIN), 1.0));    // §6.7
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
// §6.8 - FM Lin adds a deviation of its own; FM Trk one in proportion to the key's pitch,
// which is the oscillator's frequency without its Tune offset. The sum is saturated to a phase word.
static double osc_fm_hz(const tEngineNode * spec, double frequency, double fmIn) {
    double deviation = spec->fmAmount * fmIn
                       * ((spec->fmTrack == true)
                          ? ((FM_TRK_SCALE * frequency) / exp2((spec->basePitch - OSCB_TUNE_UNITY) / 12.0))
                          : FM_LIN_HZ);

    return fmin(fmax(deviation, -FM_MAX_DEVIATION_HZ), FM_MAX_DEVIATION_HZ);
}

static double oscillator_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double voicePitch,
                              double pitchDirect, double pitchVar, double shape, bool sync, double fmIn) {
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

    if (fmIn != 0.0) {
        frequency += osc_fm_hz(spec, frequency, fmIn);    // §6.8 - may run backwards: linear FM passes zero
    }
    double   inc96     = fabs(frequency) / OSC_INSTRUMENT_RATE;

    // §6.3 - the basic waves are drawn for a 96 kHz sample and need no oversampling of their own
    if ((spec->kind == eNodeOsc) || (spec->kind == eNodeOscShp)) {
        dt  = frequency / gSampleRate;
        double phase = advance_phase(&gPhase[voice][node], dt);

        if (sync == true) {    // §6.6
            phase               = fmod(OSC_SYNC_PHASE + fabs(dt), 1.0);
            gPhase[voice][node] = phase;
        }
        sum = osc_waveform(voice, node, spec, phase, dt, inc96, shape);
    } else {
        dt = frequency / (gSampleRate * (double)gOscOversample);

        for (step = 0; step < gOscOversample; step++) {
            double phase = advance_phase(&gPhase[voice][node], dt);

            if ((sync == true) && (step == 0u)) {    // §6.6
                phase               = fmod(OSC_SYNC_PHASE + fabs(dt), 1.0);
                gPhase[voice][node] = phase;
            }
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
// §50 - the rate inputs and KBT move the rate exponentially, a unit (a semitone) at a time
static double lfo_rate_now(const tEngineNode * spec, double fixedIn, double varIn, double voicePitch) {
    double semitones = ((fixedIn + (varIn * spec->lfoRateMod)) * PITCH_MOD_SEMITONES)
                       + (spec->lfoKbt * (voicePitch - KEYBOARD_PITCH_ZERO));

    return (semitones == 0.0) ? spec->rateHz : (spec->rateHz * exp2(semitones / 12.0));
}

// §28.3 - the instrument's random generator: a 24-bit linear congruence, one draw per step
#define LFO_RND_MULTIPLIER    (0xb2d9du)
#define LFO_RND_INCREMENT     (0x361963u)

static int32_t lfo_random_draw(int32_t seed) {
    uint32_t product = (uint32_t)seed * LFO_RND_MULTIPLIER;
    uint32_t base    = LFO_RND_INCREMENT << 8;
    uint32_t sum     = base + (product << 9);
    uint32_t carry   = (sum < base) ? 1u : 0u;

    return (int32_t)((((product >> 23) + carry) << 31) | (sum >> 1)) >> 8;
}

// §28.3 - RndSt and Rnd: a draw as the phase rises through its middle, the step going half way to
// it, and Rnd following the steps through a two-pole smoother at the LFO's own rate
static double lfo_random_wave(uint32_t voice, uint32_t node, const tEngineNode * spec, double phase, double rateHz) {
    SE_LOCAL;

    if ((gLfoLastPhase[voice][node] <= 0.5) && (phase > 0.5)) {
        gLfoSeed[voice][node]  = lfo_random_draw(gLfoSeed[voice][node]);
        gLfoStep[voice][node] += (int32_t)floor((double)(gLfoSeed[voice][node] - gLfoStep[voice][node]) / 2.0);
    }
    double step  = (double)gLfoStep[voice][node] / 8388608.0;

    if ((uint32_t)spec->wave == 4u) {
        return step;
    }
    double x     = fmin(16.0 * rateHz / gSampleRate, 1.0);
    double out   = fmin(fmax(gLfoHeld[voice][node] + (x * gLfoSlope[voice][node]), -1.0), 1.0);
    double error = fmin(fmax(step - out - ((2.0 - x) * gLfoSlope[voice][node]), -1.0), 1.0);

    gLfoHeld[voice][node]  = out;
    gLfoSlope[voice][node] = fmin(fmax(gLfoSlope[voice][node] + (x * error), -1.0), 1.0);
    return out;
}

// §28.5 - the LFO's sine part is the oscillator's polynomial (the same frame words), read a quarter
// cycle on so it keeps sin(2 pi phase)'s phase
static double lfo_sine(double phase) {
    double p = phase + 0.25;

    return wave_sine_polynomial(osc_triangle(p - floor(p), 0.5));
}

// §28.6 - LfoShpA's skewed triangle: (a s + |a - s| - 1) / (s^2 - 1), mapped onto -1..1
static double lfo_shp_skew(double a, double s) {
    return (2.0 * (((a * s) + fabs(a - s) - 1.0) / ((s * s) - 1.0))) - 1.0;
}

// §28.6 - the bell waves read half a shape further on, and sit at -1 past the shape
static double lfo_shp_bell(double a, double s) {
    double at = a + (0.5 * s);

    at = (at >= 1.0) ? (at - 2.0) : at;
    return (at >= s) ? -1.0 : (1.0 - (2.0 * fabs(lfo_shp_skew(at, s))));
}

// §28.6 - LfoShpA's six waves (lfoShpAWaveStrMap order) from the counter plus Phase, in cycles, and
// the Shape in 0..1
static double lfo_shp_wave(uint32_t wave, double cycles, double shape) {
    static const double kOffset[6] = {0.5, 0.25, 0.25, 0.5, 0.25, 0.5};   // §28.6 - each wave's own phase
    double              at         = cycles + kOffset[(wave < 6u) ? wave : 0u];
    double              a          = (2.0 * (at - floor(at))) - 1.0;
    double              s          = LFOSHPA_SHAPE_SCALE * ((2.0 * shape) - 1.0);
    double              out        = 0.0;

    switch (wave) {
        case 1:   // CosBell
        {
            out = wave_sine_polynomial(fmin(fmax(lfo_shp_bell(a, s), -1.0), 1.0));
            break;
        }
        case 2:   // TriBell
        {
            out = fmin(fmax(lfo_shp_bell(a, s), -1.0), 1.0);
            break;
        }
        case 3:   // Saw>Tri
        {
            out = fmin(fmax(lfo_shp_skew(a, s), -1.0), 1.0);
            break;
        }
        case 4:   // Tri>Sqr: the triangle, steepened up to five times and clipped
        {
            out = fmin(fmax(((2.0 * fabs(a)) - 1.0) * (1.0 + (2.0 * (s + 1.0))), -1.0), 1.0);
            break;
        }
        case 5:   // Pulse
        {
            out = (a < s) ? 1.0 : -1.0;
            break;
        }
        default:  // Sine: the skewed triangle through the sine polynomial
        {
            out = wave_sine_polynomial(fmin(fmax(lfo_shp_skew(a, s), -1.0), 1.0));
            break;
        }
    }
    return out;
}

static double lfo_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double rateHz,
                       double readOffset, double shapeNow) {
    SE_LOCAL;

    double counter = (spec->lfoMono == true) ? gLfoMonoPhase[node]
                                             : advance_phase(&gPhase[voice][node], rateHz / gSampleRate);
    double phase   = counter + readOffset;    // §28.4 - the waves read ahead of the counter
    double wave    = 0.0;

    phase = phase - floor(phase);

    if (spec->active == false) {
        return 0.0;
    }

    if (spec->shpWave == true) {
        wave = lfo_shp_wave((uint32_t)spec->wave, counter + readOffset, shapeNow);
    } else {
        // lfoWaveStrMap: Sin, Tri, Saw, Squ, RndSt, Rnd
        switch ((uint32_t)spec->wave) {
            case 1:
            {
                wave = osc_triangle(phase, 0.5);
                break;
            }
            case 2:
            {
                wave = 1.0 - (2.0 * phase);   // §28 - the instrument's saw falls (its part negates the phase)
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
                wave = lfo_random_wave(voice, node, spec, counter, rateHz);
                break;
            }
            default:
            {
                wave = lfo_sine(phase);
                break;
            }
        }
    }
    gLfoLastPhase[voice][node] = counter;

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

#define FLTMULTI_DAMPING_SPAN    (0.99)                 // §10.2
#define FLTMULTI_DAMPING_TOP     (0.01)                 // §10.2 - the instrument's own value at Res 127
#define FLTMULTI_H_MAX           (0x518368 / 8388608.0) // §10.2 - the coefficient's ceiling, 20.8 kHz at 96 kHz

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
    double   topHz     = (OSC_INSTRUMENT_RATE / M_PI) * asin(FLTMULTI_H_MAX);
    double   cutoff    = fmin(fmin(flt_cutoff_hz(control), topHz), gSampleRate * 0.45);
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

// §13.4 - the instrument's four-point Lagrange read, `delay` samples back from the next write (so at least
// 2), its fraction taken to 1/512 as its coefficient table is
static double comb_read(const float * line, uint32_t write, double delay) {
    const uint32_t mask  = COMB_LINE_SAMPLES - 1u;
    double         whole = floor(delay);
    double         t     = floor((delay - whole) * COMB_FRACTION_STEPS) / COMB_FRACTION_STEPS;
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

static double dsp_saturate(double value) {
    return fmin(fmax(value, -DSP_FULL_SCALE), DSP_FULL_SCALE);
}

// §13.4 - the instrument's comb: the line takes half the levelled input plus c x the last tap, and the
// output is twice b x that input plus the tap, every word saturated.
static double fltcomb_step(uint32_t voice, const tEngineNode * spec, double input, double pitchVar, double pitchDirect,
                           double voicePitch, double cutoffParam, double fbModInput) {
    SE_LOCAL;

    const tCombShape * shape    = flt_comb_shape(spec->combType);
    float *            line     = gCombLine[voice][spec->line];
    uint32_t *         write    = &gCombWrite[voice][spec->line];
    double *           fb       = &gCombFb[voice][spec->line];
    double             control  = cutoffParam + ((pitchDirect + (pitchVar * spec->modAmount)) * PITCH_MOD_SEMITONES);

    if ((spec->fltKbt > 0.0) && (voicePitch >= 0.0)) {
        control += (voicePitch - KBT_REFERENCE_NOTE) * spec->fltKbt;
    }
    control      = fmin(fmax(control, FLT_CONTROL_MIN), FLT_CONTROL_MAX);

    double             delay    = flt_comb_delay_samples(control, shape, gSampleRate);
    double             g        = fmin(fmax(spec->combFeedback + (COMB_FB_MOD_GAIN * spec->combFbMod * fbModInput), -1.0), 1.0);
    double             levelled = dsp_saturate(0.5 * spec->combLevel * input);
    double             tap      = comb_read(line, *write, fmin(fmax(delay, 2.0), (double)(COMB_LINE_SAMPLES - 3)));

    line[*write] = (float)dsp_saturate(levelled + *fb);
    *write       = (*write + 1u) & (COMB_LINE_SAMPLES - 1u);
    *fb          = dsp_saturate(dsp_saturate(shape->feedback * g) * tap);
    return dsp_saturate(2.0 * ((dsp_saturate(shape->feedForward * g) * levelled) + tap));
}

static double filter_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input, double mod, double pitchDirect, double voicePitch,
                          double cutoffParam, double resonance, double fmIn, double resIn) {
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
                               spec->fltShape, spec->tapStage >= 2u, spec->fltGainComp,
                               spec->fltFmAmount * fmIn, spec->fltResModAmount * resIn);
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
#define GLIDE_PREV_ON    (1u)    // gLogicPrev bit: Glide On as the part saw it last

static double glide_step(uint32_t voice, uint32_t node, double input, double gateIn,
                         const tEngineNode * spec) {
    SE_LOCAL;

    double current = gGlideOut[voice][node];
    // The button, unless something is patched into Glide On, which then decides. §38 - a logic
    // input is HIGH whenever it is above zero, which is how the instrument's own logic parts test
    // one; there is no halfway threshold.
    bool   onNow   = (spec->in[1] >= 0) ? (gateIn > 0.0) : spec->active;
    // §36.2 - the part decides with the Glide On it saw LAST time, and only then keeps this one: a
    // note whose Gate turns the glide on arrives whole
    bool   gliding = ((gLogicPrev[voice][node] & GLIDE_PREV_ON) != 0u);

    gLogicPrev[voice][node]  = (uint8_t)((gLogicPrev[voice][node] & ~GLIDE_PREV_ON) | (onNow ? GLIDE_PREV_ON : 0u));

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

static int32_t saturate_word(int64_t x) {
    return (x > 0x7FFFFF) ? 0x7FFFFF : ((x < -0x800000) ? -0x800000 : (int32_t)x);
}

// §49 - the part's arithmetic in its own words: scale by Range, count whole steps of n semitones
// (a semitone is 2^15) with the step's reciprocal as 0x7FFFFF / n, round half up, multiply back.
static double note_quant_step(const tEngineNode * spec, double input) {
    int32_t in     = saturate_word((int64_t)llround(input * DSP_WORD_PER_ENGINE));
    int32_t scaled = saturate_word((-(int64_t)in * (int64_t)spec->constant) >> 23);

    if (spec->select == 0u) {
        return (double)scaled / DSP_WORD_PER_ENGINE;
    }
    int64_t n      = (int64_t)spec->select;
    int64_t prod   = (int64_t)scaled * (0x7FFFFF / n);
    int64_t steps  = (prod + ((int64_t)1 << 37)) >> 38;

    return (double)saturate_word(steps * n * 32768) / DSP_WORD_PER_ENGINE;
}

static double saturate_unit(double x) {
    return (x > 1.0) ? 1.0 : ((x < -1.0) ? -1.0 : x);
}

// §47 - one sample of RandomA: a new draw each time the cycle crosses its middle, a one-pole towards
// it in a fine domain folded back into range, then a two-stage glide whose rate is Edge x the rate.
// state: 0 the one-pole, 1 the folded value, 2 and 3 the glide's velocity and position.
// §47 - one draw of the random parts' generator: the LCG, the one-pole towards the scaled draw, and the
// value it folds to, into state[0] (the one-pole) and state[1] (the value)
static void random_draw_with(double state[2], uint32_t * seed, double step, double scale, bool shiftReg) {
    double r = 0.0;
    double b = 0.0;

    if (shiftReg == true) {
        // §70.9 - Rnd2: a 24-bit shift register, the bit falling off the top fed back through the taps
        uint32_t shifted = *seed << 1;

        *seed = (shifted & 0xFFFFFFu) ^ (((shifted & 0x1000000u) != 0u) ? RND_SHIFT_TAPS : 0u);
    } else {
        *seed = ((*seed * 0xB2D9Du) + 0x361963u) & 0xFFFFFFu;
    }
    r         = (double)((int32_t)(*seed << 8) >> 8) / 8388608.0;
    state[0] += step * ((r * scale) - state[0]);
    b         = fmax(-256.0, fmin(256.0, state[0] * 8192.0));
    b         = (b >= 1.0) ? (2.0 - b) : ((b < -1.0) ? (-2.0 - b) : b);
    state[1]  = saturate_unit(b);
}

static void random_draw(double state[2], uint32_t * seed, const tEngineNode * spec) {
    random_draw_with(state, seed, spec->rndStep, spec->rndScale, spec->rndShiftReg);
}

// §70.9 - RndClkB's Step M part: from the step word s, the one-pole's word q = s^2 + 0x200 and the draw's
// pre-scale, the part's own approximation to 1/sqrt(q) - a linear seed on the mantissa, its exponent halved
static void rnd_step_words(int32_t s, double * step, double * scale) {
    int64_t square = (int64_t)s * s;
    int64_t top    = (square >> 23) + 0x200;
    int64_t full   = (top > 0x7FFFFF) ? ((int64_t)0x7FFFFF << 23) : ((top << 23) | (square & 0x7FFFFF));
    int     e      = 0;

    while (((full >> 23) < 0x400000) && (full != 0)) {
        full <<= 1;
        e--;
    }
    int32_t m      = (int32_t)(full >> 23);

    if ((e & 1) != 0) {
        m >>= 1;
        e++;
    }
    int64_t r      = ((int64_t)RNDSTEP_SEED_OFFSET << 23) + ((int64_t)m * RNDSTEP_SEED_SLOPE);
    int     shift  = (e >> 1) + RNDSTEP_SHIFT;

    r      = (shift >= 0) ? (r >> shift) : (r << -shift);
    *step  = (double)((top > 0x7FFFFF) ? 0x7FFFFF : top) / 8388608.0;
    *scale = (double)((r >> 23) > 0x7FFFFF ? 0x7FFFFF : (r >> 23)) / 8388608.0;
}

// §47 - Bip, Pos or Neg
static double random_level_shift(const tEngineNode * spec, double value) {
    return (spec->polarity == 0u) ? value : ((spec->polarity == 1u) ? (0.5 + (0.5 * value)) : (-0.5 + (0.5 * value)));
}

static double random_a_step(double state[4], uint32_t * seed, double * phase, const tEngineNode * spec, double rateHz) {
    SE_LOCAL;

    double inc  = rateHz / gSampleRate;
    double prev = *phase;
    double cur  = advance_phase(phase, inc);

    if ((prev <= 0.5) && (cur > 0.5)) {
        random_draw(state, seed, spec);
    }
    double coef = (spec->rndEdge > (8388352.0 / 8388608.0)) ? 1.0 : fmin(1.0, 256.0 * 2.0 * inc * spec->rndEdge);
    double pos  = saturate_unit(state[3] + (coef * state[2]));
    double d    = saturate_unit(((state[1] - pos) - (2.0 * state[2])) + (coef * state[2]));

    state[3] = pos;
    state[2] = saturate_unit(state[2] + (coef * d));

    if (spec->active == false) {
        return 0.0;
    }
    return random_level_shift(spec, pos);
}

// §64 - RndClkA: a draw on each rising clock edge, with no glide. Rst reloads the generator from Seed
// and clears the one-pole. state: gLadder 0-1 as random_draw(); gPulseCount says it has started.
static double rnd_clk_a_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double clock, double rst, double seedIn,
                             const double * stepModIn) {
    SE_LOCAL;

    double * st   = gLadder[voice][n];
    uint8_t  prev = gLogicPrev[voice][n];
    bool     clk  = logic_high(clock);
    bool     res  = logic_high(rst);

    if (gPulseCount[voice][n] == 0u) {
        gPulseCount[voice][n] = 1u;

        if (spec->lfoMono == true) {
            gNoiseSeed[voice][n] = 0u;   // Mono: every voice runs the one sequence from the start
        }
    }

    if (res && ((prev & LOGIC_PREV_DATA) == 0u)) {
        gNoiseSeed[voice][n] = (uint32_t)seq_sat(llround(seedIn * DSP_WORD_PER_ENGINE)) & 0xFFFFFFu;
        st[0]                = 0.0;
    }

    if (clk && ((prev & LOGIC_PREV_CLOCK) == 0u)) {
        if (stepModIn != NULL) {
            // §70.9 - Step + 4 x input x Step M, held to 0..full, through the part's own words
            int64_t s = (int64_t)spec->rndStepWord
                        + (((int64_t)seq_sat(llround(*stepModIn * DSP_WORD_PER_ENGINE)) * (int64_t)spec->rndStepModWord) >> 21);
            double  step;
            double  scale;

            rnd_step_words((int32_t)((s < 0) ? 0 : ((s > 0x7FFFFF) ? 0x7FFFFF : s)), &step, &scale);
            random_draw_with(st, &gNoiseSeed[voice][n], step, scale, spec->rndShiftReg);
        } else {
            random_draw(st, &gNoiseSeed[voice][n], spec);
        }
    }
    gLogicPrev[voice][n] = (uint8_t)((clk ? LOGIC_PREV_CLOCK : 0u) | (res ? LOGIC_PREV_DATA : 0u));
    return (spec->active == true) ? random_level_shift(spec, st[1]) : 0.0;
}

// §64 - RndTrig: on each rising clock edge a draw decides whether this pulse passes (below the
// threshold, probability Density / 128); a passed pulse is held HIGH while the clock stays high.
static double rnd_trig_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double clock, double rst, double seedIn,
                            double probIn) {
    SE_LOCAL;

    uint8_t  prev = gLogicPrev[voice][n];
    bool     clk  = logic_high(clock);
    bool     res  = logic_high(rst);
    double * held = &gLadder[voice][n][0];

    if (gPulseCount[voice][n] == 0u) {
        gPulseCount[voice][n] = 1u;

        if (spec->lfoMono == true) {
            gNoiseSeed[voice][n] = 0u;
        }
    }

    if (res && ((prev & LOGIC_PREV_DATA) == 0u)) {
        gNoiseSeed[voice][n] = (uint32_t)seq_sat(llround(seedIn * DSP_WORD_PER_ENGINE)) & 0xFFFFFFu;
    }

    if (clk == false) {
        *held = 0.0;
    } else if ((prev & LOGIC_PREV_CLOCK) == 0u) {
        int64_t probWord  = llround(probIn * DSP_WORD_PER_ENGINE);
        int64_t threshold = seq_sat((int64_t)spec->constant + ((probWord * (int64_t)spec->modAmount) >> 20));

        gNoiseSeed[voice][n] = ((gNoiseSeed[voice][n] * 0xB2D9Du) + 0x361963u) & 0xFFFFFFu;

        if (((int32_t)(gNoiseSeed[voice][n] << 8) >> 8) <= threshold) {
            *held = 1.0;
        }
    }
    gLogicPrev[voice][n] = (uint8_t)((clk ? LOGIC_PREV_CLOCK : 0u) | (res ? LOGIC_PREV_DATA : 0u));
    return (spec->active == true) ? *held : 0.0;
}

// §69.7 - DlyClock: each rising Clk writes In and outputs what was written `select` clocks before it;
// the output holds between clocks. state: gLadder[0] the output.
static double dly_clock_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double input, double clock) {
    SE_LOCAL;

    bool     clk = (clock > 0.0);
    double * out = &gLadder[voice][n][0];

    if ((spec->line < MAX_DLYCLOCK_LINES) && clk && ((gLogicPrev[voice][n] & LOGIC_PREV_CLOCK) == 0u)) {
        tDlyClockState * st = &gDlyClock[voice][spec->line];

        st->slot[st->write] = input;
        *out                = st->slot[(st->write + DLYCLOCK_SLOTS - spec->select) % DLYCLOCK_SLOTS];
        st->write           = (st->write + 1u) % DLYCLOCK_SLOTS;
    }
    gLogicPrev[voice][n] = (uint8_t)(clk ? LOGIC_PREV_CLOCK : 0u);
    return *out;
}

// §68.3 - Ctrl's word >> 17 (4 units a step), clamped to the eight inputs
static uint32_t mux_select(double ctrl) {
    int32_t step = engine_word(ctrl) >> 17;

    return (step < 0) ? 0u : ((step > 7) ? 7u : (uint32_t)step);
}

static double logic_level(bool high);

// §70 - the basic modules' dials into bx[] (and a few named fields), module by module
// §70.4 - the share of the loop Decay takes away each period, 0 at Decay 127; i = 127 - Decay,
// interpolated as the dial is
static double karplus_decay_step(double i) {
    if (i <= 0.0) {
        return 0.0;
    }
    return fmin(exp(KARPLUS_DECAY_A - (KARPLUS_DECAY_B * pow(KARPLUS_DECAY_R, i))), 1.0);
}

static void basic_build(tEngineNode * node, tModule * module, uint32_t variation) {
    double * bx = node->bx;

    memset(bx, 0, sizeof(node->bx));
    node->active = true;

    switch (module->type) {
        case moduleTypeDelayDual:
        case moduleTypeDelayQuad:
        case moduleTypeDlyEight:
        {
            // §70.1 - Dual: Time 0/2, mod 1/3; Quad: Time 0/2/4/6, mod 1/3/5/7, Time/Clk 8; Eight: Time 0
            uint32_t taps = (module->type == moduleTypeDelayDual) ? 2u : ((module->type == moduleTypeDelayQuad) ? 4u : 1u);

            node->timeSeconds = delay_range_max_seconds(module->type, module->mode[0].value);
            node->select      = (module->type == moduleTypeDelayDual) ? 0u : ((module->type == moduleTypeDelayQuad) ? 1u : 2u);

            for (uint32_t k = 0; k < taps; k++) {
                bx[k]      = param_value(module, variation, 2u * k);
                bx[4u + k] = (taps > 1u) ? param_value(module, variation, (2u * k) + 1u) : 0.0;
            }

            bx[8]             = (module->type == moduleTypeDelayQuad) && (module->param[variation][8].value != 0);
            bx[9]             = engine_master_bpm();
            break;
        }
        case moduleTypeFlanger:
        {
            // §70.2 - Rate a phase step of v x 16 (8 at 0), Range v x 0xdbec, FB v x 7000000/127 of a word
            double rate = param_value(module, variation, 0);

            bx[0]        = (rate > 0.0) ? (rate * 16.0) : 8.0;
            bx[1]        = param_value(module, variation, 1) * FLANGER_RANGE_STEP;
            bx[2]        = floor(param_value(module, variation, 2) * 7000000.0 / 127.0) / DSP_WORD_SCALE;
            node->active = (module->param[variation][3].value != 0);
            break;
        }
        case moduleTypePShift:
        {
            // §70.3 - Coarse and Fine dials, Pitch M v/128, Delay the window's range
            bx[0]        = param_value(module, variation, 0);
            bx[1]        = param_value(module, variation, 1);
            bx[2]        = (module->param[variation][2].value >= 127) ? 1.0 : (param_value(module, variation, 2) / 128.0);
            bx[3]        = (double)module->param[variation][3].value;
            node->active = (module->param[variation][4].value != 0);
            node->select = 0u;
            break;
        }
        case moduleTypeScratch:
        {
            // §70.3 - Ratio a word of (v - 64)/512 (127 = 1/8), Ratio M v/128, Delay the window's range
            bx[0]        = (module->param[variation][0].value >= 127) ? 0.125 : ((param_value(module, variation, 0) - 64.0) / 512.0);
            bx[2]        = (module->param[variation][1].value >= 127) ? 1.0 : (param_value(module, variation, 1) / 128.0);
            bx[3]        = (double)module->param[variation][2].value;
            node->active = (module->param[variation][3].value != 0);
            node->select = 1u;
            break;
        }
        case moduleTypeOscString:
        {
            // §70.4 - the oscillators' pitch dials; Decay the loop gain, Damp the loop's one-pole
            static const tOscParams kString = {moduleTypeOscString, 0, 1, 2, 3, 4, 7, -1, -1, -1, false};
            double                  damp    = param_value(module, variation, 6);

            set_osc_pitch(node, module, variation, &kString);
            bx[0] = 1.0 - karplus_decay_step(127.0 - param_value(module, variation, 5));
            bx[1] = (damp <= 0.0) ? 1.0 : ((127.0 - damp) / 128.0);
            break;
        }
        case moduleTypeResonator:
        {
            // §70.4a - the oscillators' pitch dials, Decay and Damp as OscString's, Pos v/128, Alg the junction
            static const tOscParams kString = {moduleTypeOscString, 0, 1, 2, 3, 4, 7, -1, -1, -1, false};
            double                  damp    = param_value(module, variation, 6);

            set_osc_pitch(node, module, variation, &kString);
            bx[0]        = 1.0 - karplus_decay_step(127.0 - param_value(module, variation, 5));
            bx[1]        = (damp <= 0.0) ? 1.0 : ((127.0 - damp) / 128.0);
            bx[2]        = (module->param[variation][8].value >= 127) ? 1.0 : (param_value(module, variation, 8) / 128.0);
            node->select = (uint32_t)module->param[variation][9].value % RESONATOR_ALGS;
            break;
        }
        case moduleTypeDriver:
        {
            // §70.5 - Stiffness and Embouchure as words (v/128, 127 full); Bow takes Embouchure / 16
            double stiff = param_value(module, variation, 0);
            double emb   = param_value(module, variation, 1);

            node->select = module->mode[0].value;
            bx[0]        = (module->param[variation][0].value >= 127) ? 1.0 : (stiff / 128.0);
            bx[1]        = (module->param[variation][1].value >= 127) ? 1.0 : (emb / 128.0);
            bx[1]       /= (node->select == DRIVER_TYPE_BOW) ? 16.0 : 1.0;
            break;
        }
        case moduleTypeNoiseGate:
        {
            noise_gate_build(node, module, variation);   // §70.6
            break;
        }
        case moduleTypePitchTrack:
        case moduleTypeZeroCnt:
        {
            // §70.7 - PitchTrack's Threshold: the gate opens at v/128 of a unit-level signal and closes at 3/4 of it
            bx[0]        = (module->param[variation][0].value >= 127) ? 1.0 : (param_value(module, variation, 0) / 128.0);
            node->select = (module->type == moduleTypeZeroCnt) ? 1u : 0u;
            break;
        }
        case moduleTypeVocoder:
        {
            // §70.8 - BandSel 1-16 (0 Off, 1-16 the analysis band routed there), Emphasis 16, Monitor 17
            for (uint32_t k = 0; k < 16u; k++) {
                bx[k] = (double)module->param[variation][k].value;
            }

            bx[16] = (module->param[variation][16].value != 0);
            bx[17] = (module->param[variation][17].value != 0);
            break;
        }
        case moduleTypeRndPattern:
        {
            // §70.9 - Pattern 0, Bank 1, Step 2, Loop 3, Step M 4, OutType 5, on 6; Wave a mode (Val, State)
            int32_t v       = (int32_t)param_value(module, variation, 2);
            double  word    = (v >= 127) ? 8388607.0 : fmin(8388607.0, (512.0 * v * v) + 512.0);
            int32_t pattern = (int32_t)param_value(module, variation, 0);
            int32_t bank    = (int32_t)param_value(module, variation, 1);
            int32_t stepM   = (int32_t)param_value(module, variation, 4);

            node->rndStep  = word / 8388608.0;
            node->rndScale = floor(fmin(sqrt(8388608.0 / word), 8192.0) * 2048.0) / 8388608.0;
            node->polarity = (uint32_t)module->param[variation][5].value;
            node->active   = (module->param[variation][6].value != 0);
            bx[0]          = (pattern >= 127) ? 0x200000 : ((pattern - 64) * 0x8000);      // its X0
            bx[1]          = (bank >= 127) ? 0x4000 : ((bank - 64) * 0x100);               // its Y0
            bx[2]          = -(double)module->param[variation][3].value;                   // Y1: the loop starts at -Loop
            bx[3]          = (double)module->mode[0].value;
            bx[4]          = (v >= 127) ? 0x7FFFFF : ((v - 64) * 0x20000);                 // State's threshold
            bx[5]          = (stepM >= 127) ? 0x7FFFFF : (stepM << 16);
            break;
        }
        case moduleTypeSeqCtr:
        {
            // §70.10 - steps 0-15 as words (v x 2^14), events 16-31 as a bit mask, Pulse 32, Pol 33, XFade 34
            node->select = 0u;

            for (uint32_t k = 0; k < 16u; k++) {
                int32_t v = (int32_t)param_value(module, variation, k);

                bx[k]         = (v >= 127) ? 0x200000 : (v << 14);
                node->select |= (module->param[variation][16u + k].value != 0) ? (1u << k) : 0u;
            }

            bx[16]       = (double)module->param[variation][34].value;            // XFade 0-3
            bx[17]       = (module->param[variation][33].value == 0) ? 1.0 : 0.0; // Pol: bipolar
            bx[18]       = (module->param[variation][32].value != 0) ? 1.0 : 0.0; // Pulse 1: a gate
            bx[19]       = (module->upRate == 0u) ? 1.0 : 0.0;                    // §58 - not up-rated: 24 kHz
            break;
        }
        case moduleTypeMux8to1X:
        {
            // §70.11 - the gain at an input and its fall per unit of distance, from X-Fade's words
            int32_t d = (0x7fff - (int32_t)(param_value(module, variation, 0) * 256.0)) >> 1;

            bx[0] = (512.0 * (double)((8 * d) + 0x4000)) / 8388608.0;
            bx[1] = (2.0 * 56.0 * (double)((8 * d) + 0x2000)) / 8388608.0;
            break;
        }
        case moduleTypeLevScaler:
        {
            // §70.12 - L.Gain 0 and R.Gain 2 as the instrument's slope words, BrkPnt 1 in keys from E4, Kbt 3
            double l  = param_value(module, variation, 0);
            double bp = param_value(module, variation, 1);
            double r  = param_value(module, variation, 2);

            bx[0] = (l <= 0.0) ? LEVSCALER_SLOPE_MAX : (-LEVSCALER_SLOPE_MAX * (l - 64.0) / 63.0);
            bx[1] = (module->param[variation][1].value >= 127) ? 64.0 : (bp - 64.0);
            bx[2] = (r <= 0.0) ? -LEVSCALER_SLOPE_MAX : (LEVSCALER_SLOPE_MAX * (r - 64.0) / 63.0);
            bx[3] = (module->param[variation][3].value != 0);
            break;
        }
        default:
        {
            break;
        }
    }
}

// §70 - a linearly interpolated read `delay` samples behind the write position
static double ring_read(const float * ring, uint32_t size, uint32_t write, double delay) {
    double   d    = fmin(fmax(delay, 1.0), (double)(size - 2u));
    uint32_t i    = (uint32_t)d;
    double   frac = d - (double)i;
    double   a    = ring[(write + size - i) % size];
    double   b    = ring[(write + size - i - 1u) % size];

    return a + ((b - a) * frac);
}

// §70.1 - taps on one of the shared delay lines. out[] gets Dual's two, Quad's Main then its four,
// or Eight's eight.
static void multi_tap_step(const tEngineNode * spec, double input, const double mods[4], double out[NODE_OUTPUTS]) {
    SE_LOCAL;

    uint32_t       l     = spec->line;

    if (l >= MAX_DELAY_LINES) {
        return;
    }
    const double * bx    = spec->bx;
    uint32_t       write = gDelayWrite[l];
    double         maxS  = spec->timeSeconds;

    gDelayLine[l][write] = (float)input;

    if (spec->select == 2u) {
        double spacing = delay_time_seconds(maxS / 8.0, bx[0]);

        for (uint32_t k = 0; k < 8u; k++) {
            out[k] = ring_read(gDelayLine[l], DELAY_LINE_SAMPLES, write, spacing * (double)(k + 1u) * gSampleRate);
        }
    } else {
        uint32_t taps  = (spec->select == 0u) ? 2u : 4u;
        uint32_t first = (spec->select == 0u) ? 0u : 1u;

        for (uint32_t k = 0; k < taps; k++) {
            double seconds = 0.0;

            if (bx[8] != 0.0) {
                seconds = clk_sync_beats(bx[k]) * (60.0 / bx[9]);

                while ((seconds > maxS) && (seconds > 0.0)) {
                    seconds *= 0.5;
                }
            } else {
                double dial = fmin(127.0, fmax(0.0, bx[k] + (mods[k] * UNITS_PER_FULL_SCALE * bx[4u + k] / 64.0)));

                seconds = delay_time_seconds(maxS, dial);
            }
            out[first + k] = delay_ring_lagrange(gDelayLine[l], write, seconds * gSampleRate);    // §52.1
        }

        if (spec->select == 1u) {
            out[0] = ring_read(gDelayLine[l], DELAY_LINE_SAMPLES, write, maxS * gSampleRate);
        }
    }
    gDelayWrite[l] = (write + 1u) % DELAY_LINE_SAMPLES;
}

// notes §197 - the FX area's line is shared; the Voice area's is one per voice (the first two lines)
static uint32_t fx_buf_instance(const tEngineNode * spec, uint32_t voice) {
    if (spec->postMix == true) {
        return (spec->line < MAX_FXBUF_LINES) ? spec->line : FXBUF_INSTANCES;
    }
    return (spec->line < MAX_FXBUF_VOICE_LINES) ? (MAX_FXBUF_LINES + (spec->line * MAX_VOICES) + voice) : FXBUF_INSTANCES;
}

// A 4-point Lagrange read `delay` samples behind `write`, on a ring of `size`
static double ring_read_lagrange(const float * ring, uint32_t size, uint32_t write, double delay) {
    double   d  = fmax(delay, 1.0);
    uint32_t i  = (uint32_t)d;
    double   t  = d - (double)i;
    double   y0 = ring[(write + (4u * size) - i + 1u) % size];
    double   y1 = ring[(write + (4u * size) - i) % size];
    double   y2 = ring[(write + (4u * size) - i - 1u) % size];
    double   y3 = ring[(write + (4u * size) - i - 2u) % size];

    return (-t * (t - 1.0) * (t - 2.0) / 6.0 * y0) + ((t + 1.0) * (t - 1.0) * (t - 2.0) / 2.0 * y1)
           - ((t + 1.0) * t * (t - 2.0) / 2.0 * y2) + ((t + 1.0) * t * (t - 1.0) / 6.0 * y3);
}

// The 24 kHz tick the instruments' control parts run on, in engine samples
static bool fx_tick(tFxBufState * st) {
    SE_LOCAL;

    double period = 4.0 * gSampleRate / G2_ENGINE_SAMPLE_RATE;

    st->tick += 1.0;

    if (st->tick >= period) {
        st->tick -= period;
        return true;
    }
    return false;
}

// §70.2 - Flanger: a triangle sweeps a 4-point read of a 512-sample ring; the ring takes 0.8 In plus
// twice FB times the second output, 0.6 In + 0.3 x the read; Out is 0.6 In + 0.8 x the read
static double flanger_step(const tEngineNode * spec, uint32_t voice, double input) {
    SE_LOCAL;

    uint32_t      l     = fx_buf_instance(spec, voice);

    if ((l >= FXBUF_INSTANCES) || (spec->active == false)) {   // bypassed: no work while off
        return input;
    }
    tFxBufState * st    = &gFxState[l];
    double        scale = gSampleRate / G2_ENGINE_SAMPLE_RATE;
    uint32_t      size  = (uint32_t)lround(FLANGER_RING * scale);
    uint32_t      write = (gFxBufWrite[l] + 1u) % size;

    if (st->ready == false) {
        // the instrument draws the start phase at load; drawn from the line here so that a render repeats
        st->phase = chorus_sign24(chorus_scramble(0x9E3779B9u ^ ((l + 1u) * 0x85EBCA6Bu)));
        st->tick  = 4.0 * scale;
        st->ready = true;
    }

    if (fx_tick(st) == true) {
        st->phase    = chorus_sign24((uint32_t)st->phase + (uint32_t)spec->bx[0]);
        uint32_t word = (uint32_t)floor(spec->bx[1] * fabs((double)st->phase) / DSP_WORD_SCALE) + FLANGER_MIN_WORD;

        st->delay[0] = (double)(word >> 14) + 1.0 + ((double)((word >> 9) & 31u) / 32.0);
    }
    gFxBuf[l][write] = (float)dsp_saturate((FLANGER_IN_GAIN * input) + (2.0 * spec->bx[2] * st->feedback));
    gFxBufWrite[l]   = write;

    double        wet   = ring_read_lagrange(gFxBuf[l], size, write, st->delay[0] * scale);

    st->feedback     = dsp_saturate((FLANGER_MIX_GAIN * 2.0 * input) + (FLANGER_MIX_GAIN * wet));
    return dsp_saturate((FLANGER_MIX_GAIN * 2.0 * input) + ((FLANGER_MIX_GAIN + 0.5) * wet));
}

// §70.3 - PShift's pitch ratio: Coarse in quarter semitones (interpolated between semitone steps),
// Fine +-50 cents, Pitch M half a semitone a unit at full
static double pitch_shift_ratio(const tEngineNode * spec, double mod) {
    double quarter = spec->bx[0] / 4.0;
    double step    = floor(quarter);
    double frac    = quarter - step;
    double coarse  = (exp2((step - 16.0) / 12.0) * (1.0 - frac)) + (exp2((step - 15.0) / 12.0) * frac);
    double fine    = exp2((spec->bx[1] - 64.0) * 50.0 / 64.0 / 1200.0);
    double semis   = fmin(fmax(mod * 32.0 * spec->bx[2], -64.0), 64.0);

    return coarse * fine * exp2(semis / 12.0);
}

// §70.3 - PShift and Scratch: two taps on a 100 ms line, half a cycle of one phase apart, each delayed
// 9728 x X1 x (1 + phase) samples and weighted 1 - phase^2; the sum is inverted. The phase steps on the
// 24 kHz tick by 6990.67 x (1 - ratio), times 8/4/2/1 as the Delay range doubles X1 from 127/2048.
static double pitch_shift_step(const tEngineNode * spec, uint32_t voice, double input, double mod) {
    SE_LOCAL;

    uint32_t      l      = fx_buf_instance(spec, voice);

    if ((l >= FXBUF_INSTANCES) || (spec->active == false)) {
        return input;
    }
    tFxBufState * st     = &gFxState[l];
    double        scale  = gSampleRate / G2_ENGINE_SAMPLE_RATE;
    uint32_t      mode   = (uint32_t)spec->bx[3] & 3u;
    double        window = PSHIFT_WINDOW_X1 * (double)(1u << mode);
    uint32_t      write  = (gFxBufWrite[l] + 1u) % FXBUF_SAMPLES;

    if (st->ready == false) {
        st->tick  = 4.0 * scale;
        st->ready = true;
    }

    if (fx_tick(st) == true) {
        double level = 1.0;
        double step  = 0.0;

        if (spec->select == 1u) {
            // Scratch: the ratio word through a one-pole of 0.99 a tick, the Mod input added unsmoothed;
            // the level rises from silence over the first 3/1024 of it, and 4 x the ratio is the speed
            st->ratio = fmin(fmax((SCRATCH_SMOOTH_IN * spec->bx[0]) + (SCRATCH_SMOOTH_POLE * st->ratio)
                                  + (spec->bx[2] * mod / DSP_FULL_SCALE), -1.0), 1.0);
            level     = fmin(fmax(64.0 * ((8.0 * fabs(st->ratio)) - (1.0 / 128.0)), 0.0), 1.0);
            step      = SCRATCH_RATE_SCALE * (1.0 - (4.0 * fmin(fmax(8.0 * st->ratio, -1.0), 1.0)));
        } else {
            step = PSHIFT_RATE_SCALE * (1.0 - pitch_shift_ratio(spec, mod));
        }
        step      = fmin(fmax(step * (double)(8u >> mode), -DSP_WORD_SCALE), DSP_WORD_SCALE - 1.0);
        st->phase = chorus_sign24((uint32_t)st->phase + (uint32_t)(int32_t)step);

        for (uint32_t k = 0; k < 2u; k++) {
            double p = (double)chorus_sign24((uint32_t)st->phase + (k * 0x800000u)) / DSP_WORD_SCALE;

            st->delay[k] = PSHIFT_TAP_SCALE * window * (1.0 + p);
            st->gain[k]  = level * (1.0 - (p * p));
        }
    }
    gFxBuf[l][write] = (float)dsp_saturate(input);
    gFxBufWrite[l]   = write;

    double out = 0.0;

    for (uint32_t k = 0; k < 2u; k++) {
        out += st->gain[k] * ring_read_lagrange(gFxBuf[l], FXBUF_SAMPLES, write, fmin(st->delay[k] * scale, (double)(FXBUF_SAMPLES - 4u)));
    }

    return dsp_saturate(-out);
}

static double osc_frequency_hz(const tEngineNode * spec, double voicePitch, double pitchDirect, double pitchVar);

// §70.4 - OscString: the line read 96000/f samples back (a 4-point read), through the Damp one-pole s;
// the line takes In + Decay x s, Out is s (0 with the module off, the loop running on)
static double karplus_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double excite, double pitchIn,
                           double pitchVar, double voicePitch) {
    SE_LOCAL;

    uint32_t l      = spec->line;

    if ((l >= MAX_STRING_LINES) || (spec->active == false)) {   // off: no work while off
        return 0.0;
    }
    float *  ring   = gString[voice][l];
    uint32_t write  = gStringWrite[voice][l];
    double   hz     = osc_frequency_hz(spec, voicePitch, pitchIn, pitchVar);
    double   period = (hz > 0.0) ? fmin(G2_ENGINE_SAMPLE_RATE / hz, KARPLUS_LINE - 2.0) : (KARPLUS_LINE - 2.0);
    double   delay  = fmin(fmax(period, 2.0) * gSampleRate / G2_ENGINE_SAMPLE_RATE, (double)(STRING_SAMPLES - 4u));
    double * lp     = &gLadder[voice][n][0];
    double   back   = ring_read_lagrange(ring, STRING_SAMPLES, write, delay - 1.0);

    *lp                    = dsp_saturate(*lp + (spec->bx[1] * (back - *lp)));
    write                  = (write + 1u) % STRING_SAMPLES;
    ring[write]            = (float)dsp_saturate(excite + (spec->bx[0] * *lp));
    gStringWrite[voice][l] = write;
    return *lp;
}

// §70.4a - Resonator: two lines, Pos and 1 - Pos of the period (less six samples) long, meet at a junction
// whose six words the Alg selects. Line 1 takes 0.97 (a D1 + b D2 + c Exc); line 2 takes Decay x four
// half-sample averages of the Damp one-pole of (d D2 + e D1 + f Exc). Out1 is a mix of the two lines, Out2
// what enters line 2. In words (a quarter of an engine unit); every stored word saturates.
static const double kResonatorJunction[RESONATOR_ALGS][6] = {
    {0.0,  1.0, -1.0, 0.0,  1.0, 1.0},     // String1
    {0.0, -1.0,  1.0, 0.0, -1.0, 1.0},     // String2
    {0.0, -1.0,  0.0, 0.0,  0.0, 1.0},     // Tube1
    {0.0,  1.0,  0.0, 1.0,  0.0, 1.0},     // Tube2
    {1.0,  0.0,  1.0, 1.0,  0.0, 1.0},     // Tube3
};
static const double kResonatorMix[RESONATOR_ALGS][2]      = {
    {0.0, 0.0}, {-1.0, -1.0}, {0.0, 1.0}, {-1.0, 1.0}, {-1.0, -1.0},   // Out1 = [0] D2 + [1] D1
};

static void resonator_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double excite, double pitchIn,
                           double pitchVar, double voicePitch, double out[2]) {
    SE_LOCAL;

    uint32_t       l      = spec->line;

    out[0] = 0.0;
    out[1] = 0.0;

    if (((l + 1u) >= MAX_STRING_LINES) || (spec->active == false)) {   // off: no work while off
        return;
    }
    const double * j      = kResonatorJunction[spec->select];
    const double * mix    = kResonatorMix[spec->select];
    double         scale  = gSampleRate / G2_ENGINE_SAMPLE_RATE;
    double         hz     = osc_frequency_hz(spec, voicePitch, pitchIn, pitchVar);
    double         period = (hz > 0.0) ? fmin(G2_ENGINE_SAMPLE_RATE / hz, KARPLUS_LINE - 2.0) : (KARPLUS_LINE - 2.0);
    double         span   = fmax(period - RESONATOR_TRIM, 0.0);
    double         len[2] = {span * spec->bx[2], span * (1.0 - spec->bx[2])};
    double         line[2];
    double *       st     = gLadder[voice][n];
    double         e      = excite / DSP_FULL_SCALE;

    for (uint32_t k = 0; k < 2u; k++) {
        double delay = fmin((len[k] + 2.0) * scale, (double)(STRING_SAMPLES - 4u));

        line[k] = ring_read_lagrange(gString[voice][l + k], STRING_SAMPLES, gStringWrite[voice][l + k], fmax(delay - 1.0, 1.0));
    }

    double         a      = fmin(fmax(RESONATOR_LOSS * ((j[0] * line[0]) + (j[1] * line[1]) + (j[2] * e)), -1.0), 1.0);
    double         x      = 0.0;

    st[0]  = fmin(fmax(st[0] + (spec->bx[1] * (((j[3] * line[1]) + (j[4] * line[0]) + (j[5] * e)) - st[0])), -1.0), 1.0);
    x      = st[0];

    for (uint32_t k = 1; k <= 4u; k++) {
        double y = 0.5 * (x + st[k]);

        st[k] = x;
        x     = y;
    }

    double         b      = fmin(fmax(spec->bx[0] * x, -1.0), 1.0);
    double         w[2]   = {a, b};

    for (uint32_t k = 0; k < 2u; k++) {
        uint32_t write = (gStringWrite[voice][l + k] + 1u) % STRING_SAMPLES;

        gString[voice][l + k][write] = (float)w[k];
        gStringWrite[voice][l + k]   = write;
    }

    out[0] = fmin(fmax((mix[0] * line[1]) + (mix[1] * line[0]), -1.0), 1.0) * DSP_FULL_SCALE;
    out[1] = b * DSP_FULL_SCALE;
}

// §70.6 - NoiseGate: a two-stage peak follower in the part's own words opens a gate above Threshold and
// closes it below three quarters of it (the gate is held open when the module is switched off); the gate
// runs an attack-hold-release envelope (§17.3's arithmetic, its own times), and Out is In x that.
// state: 0-1 the follower's two stages, 2 the gate, as words.
static void noise_gate_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double input, double value[][NODE_OUTPUTS],
                            double out[2]) {
    SE_LOCAL;

    double * st   = gLadder[voice][n];
    int64_t  in   = llabs((int64_t)seq_sat(llround(input * DSP_WORD_PER_ENGINE)));
    int64_t  y0   = (int64_t)st[0];
    int64_t  y1   = (int64_t)st[1];
    int64_t  diff = in - y0;
    int64_t  acc  = (y0 << 23) + ((diff > 0) ? ((int64_t)seq_sat(diff) << 23)
                                              : ((diff < 0) ? ((int64_t)NOISEGATE_FALL1 * seq_sat(diff)) : 0));
    int64_t  hi   = acc >> 23;

    st[0]  = (double)seq_sat(hi);
    diff   = hi - y1;

    int64_t  acc2 = (y1 << 23) + (((diff > 0) || ((diff == 0) && ((acc & 0x7FFFFF) != 0)))
                                   ? ((int64_t)NOISEGATE_RISE2 * seq_sat(diff))
                                   : ((diff < 0) ? ((int64_t)NOISEGATE_FALL2 * seq_sat(diff)) : 0));
    int64_t  hi2  = acc2 >> 23;
    double   gate = st[2];

    st[1]  = (double)seq_sat(hi2);

    if (hi2 < (int64_t)spec->bx[1]) {
        gate = 0.0;    // below three quarters of the threshold: shut
    }

    if ((spec->active == false) || (acc2 > ((int64_t)spec->bx[0] << 23))) {
        gate = (double)NOISEGATE_OPEN;
    }
    st[2]  = gate;

    double   env  = envelope_step(voice, n, spec, gate > 0.0, value);

    out[0] = input * env;
    out[1] = env;
}

static void noise_gate_build(tEngineNode * node, tModule * module, uint32_t variation) {
    int32_t       thr   = (int32_t)param_value(module, variation, 0);
    double        atk   = param_value(module, variation, 1);
    double        rel   = param_value(module, variation, 2);
    double        relX  = (rel <= 0.0) ? NOISEGATE_REL_SHORTEST : pow((rel + NOISEGATE_REL_OFFSET) / (127.0 + NOISEGATE_REL_OFFSET), 5.0);
    tEnvSegment * stage = node->envStage;
    int32_t       word  = (thr >= 127) ? 0x200000 : (thr << 14);

    node->bx[0]           = (double)word;                        // its Y2: open above
    node->bx[1]           = (double)((word >> 1) + (word >> 2)); // its X3: shut below
    node->active          = (module->param[variation][3].value != 0);
    memset(stage, 0, 3u * sizeof(*stage));
    env_stage_rates(&stage[0], NOISEGATE_ATK_SHORTEST * pow(1.0 + (19.0 * atk / 127.0), 2.0), (uint32_t)eEnvShapeLogExp, true);
    stage[0].rising       = 1u;
    stage[0].target       = ENV_TOP;
    stage[1].sustain      = 1u;
    stage[1].target       = ENV_TOP;
    stage[1].half         = ENV_HALF_UNITY;
    env_stage_rates(&stage[2], relX, (uint32_t)eEnvShapeLogExp, false);

    for (uint32_t k = 0; k < 3u; k++) {
        stage[k].modLeg = -1;
    }

    node->envStageCount   = 3u;
    node->envSustainStage = 1;
    node->wave            = (tOscWave)eEnvShapeLogExp;
    node->envOutType      = 0u;
}

#define PITCH_TRACK_ZERO_HZ    (82.4068892)   // §70.7 - the counter's 0 units is E2

// §70.7a - ZeroCnt, as its parts run it: the input read once a 24 kHz tick, the ticks between rising
// crossings (the last reading <= 0, this one > 0) counted whole, and the count read as a pitch.
// state: 0 ticks since the last crossing, 1 last reading, 2 pitch, 3 the tick accumulator.
static void zero_count_step(double state[5], double input, double out[3]) {
    SE_LOCAL;

    state[3] -= ENV_TICK_HZ / gSampleRate;

    if (state[3] <= 0.0) {
        state[3] += 1.0;
        state[0] += 1.0;

        if ((state[1] <= 0.0) && (input > 0.0)) {
            state[2] = (12.0 * log2((ENV_TICK_HZ / state[0]) / PITCH_TRACK_ZERO_HZ)) / UNITS_PER_FULL_SCALE;
            state[0] = 0.0;
        }
        state[1]  = input;
    }
    out[0]    = state[2];
}

// §70.7 - a two-stage follower: an instant first stage releasing by `release1` a tick, then a second
// that attacks by `attack` and releases by `release2`
static void pd_follow(double env[2], double x, double release1, double attack, double release2) {
    double d = x - env[0];

    env[0] += (d > 0.0) ? d : (release1 * d);
    d       = env[0] - env[1];
    env[1] += ((d > 0.0) ? attack : release2) * d;
}

// §70.7 - PitchTrack, every 96 kHz sample: |In| through a two-stage follower opens Gate at the threshold
// and closes it at 3/4 of it; In through a 316 Hz low-pass and a DC blocker sets a flip-flop at each
// positive peak (the half-wave reaching 0.9 of its follower), and the raw In's negative peaks reset it.
// The flip-flop is Period; the ticks between its rising edges are the pitch, as ZeroCnt's (§70.7a).
static void pitch_track_step(uint32_t voice, double state[5], const tEngineNode * spec, double input, double out[3]) {
    SE_LOCAL;

    if (spec->select == 1u) {
        zero_count_step(state, input, out);
        return;
    }

    if (spec->line >= MAX_PITCH_TRACKER_LINES) {
        out[0] = 0.0;
        out[1] = 0.0;
        out[2] = 0.0;
        return;
    }
    tPitchTracker * pd = &gPitchTracker[voice][spec->line];

    pd->tick -= G2_ENGINE_SAMPLE_RATE / gSampleRate;

    if (pd->tick <= 0.0) {
        double v     = 0.0;
        double x     = 0.0;

        pd->tick  += 1.0;

        pd_follow(pd->pre, fabs(input), PD_RELEASE_FAST, PD_ATTACK, PD_RELEASE_GATE);
        pd->gate   = (pd->pre[1] < 0.75 * spec->bx[0]) ? 0.0 : pd->gate;
        pd->gate   = (pd->pre[1] > spec->bx[0]) ? 1.0 : pd->gate;

        pd->lp     = dsp_saturate(((1.0 - PD_LP_POLE) * input) + (PD_LP_POLE * pd->lp));
        v          = dsp_saturate(pd->hp + (0.5 * (1.0 + PD_HP_POLE) * pd->lp));
        pd->hp     = dsp_saturate((-0.5 * (1.0 + PD_HP_POLE) * pd->lp) + (PD_HP_POLE * v));

        x          = fmax(v, 0.0);
        pd_follow(pd->pos, x, PD_RELEASE_FAST, PD_ATTACK, PD_RELEASE_PEAK);
        bool   set   = (PD_PEAK_SHARE * pd->pos[1]) <= x;

        x          = fmax(-input, 0.0);
        pd_follow(pd->neg, x, PD_RELEASE_FAST, PD_ATTACK, PD_RELEASE_PEAK);
        bool   reset = (PD_PEAK_SHARE * pd->neg[1]) <= x;

        pd->flip   = (set == true) ? 1.0 : pd->flip;
        pd->flip   = (reset == true) ? 0.0 : pd->flip;

        pd->count += 1.0;

        if ((pd->last <= 0.0) && (pd->flip > 0.0)) {
            pd->pitch = (12.0 * log2((G2_ENGINE_SAMPLE_RATE / pd->count) / PITCH_TRACK_ZERO_HZ)) / UNITS_PER_FULL_SCALE;
            pd->count = 0.0;
        }
        pd->last   = pd->flip;
    }
    out[0] = logic_level(pd->flip > 0.0);
    out[1] = pd->pitch;
    out[2] = logic_level(pd->gate > 0.0);
}

// §70.8 - the sixteen bands' sections, in program order: band 0 two sections (k0..k4, k1..k4), the
// rest four (k0..k4, then k1..k4 three times). Analysis and synthesis share them.
static const int32_t kVocoderBandWords[264] = {
    0x000253, 0x3ee1d5, 0x8149cd, 0x400000, 0x7fffff, 0x3d5541, 0x82b932, 0x3ff99d,
    0x7ff99d, 0x000485, 0x3f98ee, 0x80db30, 0x3ffffe, 0x800002, 0x3fbd93, 0x80728d,
    0x3ffffe, 0x800002, 0x3f2177, 0x813811, 0x3ff696, 0x7ff696, 0x3f46b7, 0x80f741,
    0x400000, 0x7fffff, 0x000e63, 0x3f2aa3, 0x816706, 0x3ffffb, 0x800005, 0x3fb056,
    0x80c830, 0x400000, 0x800000, 0x3f0b5b, 0x81b478, 0x3ffa0e, 0x7ffa0d, 0x3f9139,
    0x815877, 0x400000, 0x7fffff, 0x0004dd, 0x3fa695, 0x814a3e, 0x3ffff8, 0x800008,
    0x3f8c09, 0x820a5c, 0x3ffff8, 0x800008, 0x3efbd4, 0x825f63, 0x3ffc86, 0x7ffc86,
    0x3f165e, 0x820119, 0x3ffc86, 0x7ffc86, 0x001196, 0x3eef77, 0x82ec0f, 0x3fffee,
    0x800012, 0x3f969a, 0x820a29, 0x400000, 0x800000, 0x3ed41e, 0x836d07, 0x3ff677,
    0x7ff676, 0x3f7b2b, 0x831de5, 0x400000, 0x7fffff, 0x000f8a, 0x3f84f6, 0x83234e,
    0x3ffff4, 0x80000c, 0x3f6849, 0x84a8c1, 0x3ffff4, 0x80000c, 0x3ea7de, 0x84e9f7,
    0x3ffb4d, 0x7ffb4d, 0x3ec462, 0x8439d7, 0x3ffb4d, 0x7ffb4d, 0x001b8f, 0x3f70a9,
    0x84b5a7, 0x3fffdc, 0x800024, 0x3f5229, 0x86d675, 0x3fffdc, 0x800024, 0x3e7459,
    0x86fede, 0x3ff852, 0x7ff851, 0x3e929a, 0x860e53, 0x3ff852, 0x7ff851, 0x006437,
    0x3e56c0, 0x889da6, 0x3fffbe, 0x800042, 0x3f5857, 0x86ef8e, 0x400000, 0x800000,
    0x3e35fb, 0x89e761, 0x3ff81a, 0x7ff81a, 0x3f3739, 0x89e7b6, 0x400000, 0x7fffff,
    0x003254, 0x3f3781, 0x8a12c2, 0x3fffb3, 0x80004d, 0x3f11ad, 0x8e4bae, 0x3fffb3,
    0x80004d, 0x3ddfed, 0x8e0835, 0x3ffc33, 0x7ffc33, 0x3e0542, 0x8c3990, 0x3ffc33,
    0x7ffc33, 0x00c874, 0x3d7b58, 0x93d2fd, 0x3fffa3, 0x80005d, 0x3f109b, 0x8e8c9e,
    0x400000, 0x800000, 0x3da562, 0x914e02, 0x3ffcd0, 0x7ffcd0, 0x3ee5e0, 0x9480f2,
    0x3ffcd0, 0x7ffcd0, 0x0067c8, 0x3edf53, 0x94dfff, 0x3ffff2, 0x80000e, 0x3eaec9,
    0x9d4b04, 0x3ffff2, 0x80000e, 0x3cfcd7, 0x9bf665, 0x3ff9eb, 0x7ff9eb, 0x3d2c6d,
    0x986f93, 0x3ff9eb, 0x7ff9eb, 0x01df83, 0x3c8371, 0xa29336, 0x3ffeb3, 0x80014d,
    0x3e9a48, 0x9ddcf7, 0x400000, 0x800000, 0x3c4c4d, 0xa79dd7, 0x3ff682, 0x7ff682,
    0x3e61c9, 0xaa01df, 0x400000, 0x7fffff, 0x0153b1, 0x3b37b1, 0xb8ea28, 0x3ffeef,
    0x800111, 0x3e2d31, 0xaae1c6, 0x400000, 0x800000, 0x3b79b0, 0xb16430, 0x3ffc4f,
    0x7ffc4f, 0x3de92c, 0xbd2c61, 0x3ffc4f, 0x7ffc4f, 0x026144, 0x3978cf, 0xd3a0ff,
    0x3ffd55, 0x8002ab, 0x3d7602, 0xbe85c6, 0x400000, 0x800000, 0x39c2c1, 0xc7ea4a,
    0x3ffb74, 0x7ffb74, 0x3d293f, 0xdb4c01, 0x3ffb74, 0x7ffb74, 0x06448e, 0x367279,
    0xebc8a5, 0x3ffc0f, 0x8003f2, 0x3650e6, 0xfef948, 0x3ffb92, 0x7ffb92, 0x3c0666,
    0xdd67e8, 0x3ffc0f, 0x8003f2, 0x3be360, 0x0d050b, 0x3ffb92, 0x7ffb92, 0x0a31b5,
    0x3295d7, 0x210b2b, 0x3ffc88, 0x800378, 0x3b3b0e, 0x4b6535, 0x3ffe35, 0x7ffe35,
    0x33b5ab, 0x394ead, 0x3ffc88, 0x800378, 0x3a1359, 0x0f89e4, 0x3ffe35, 0x7ffe35,
};
// §70.8 - each band's follower: release, attack (a share of the step a 24 kHz tick)
static const int32_t kVocoderFollowWords[16][2] = {
    {0x004432, 0x021d9b},
    {0x006385, 0x0390be},
    {0x0077ee, 0x04bdef},
    {0x00899c, 0x05c519},
    {0x009890, 0x06ab05},
    {0x00a8dd, 0x0796f5},
    {0x00b929, 0x088c93},
    {0x00cc29, 0x099330},
    {0x00e082, 0x0aae14},
    {0x00f78d, 0x0bf1b1},
    {0x0113fe, 0x0d6050},
    {0x013478, 0x0f0451},
    {0x015bab, 0x10fb43},
    {0x01904f, 0x137bb1},
    {0x01d90e, 0x16e21b},
    {0x024b3b, 0x1bf5b7},
};
// §70.8 - the 96 kHz converters: Ctrl's one section, then In's and Out's four (the same words)
static const int32_t kVocoderConverterWords[22] = {
    0x038379, 0x1d3308, 0xb0dadc, 0x400000, 0x7fffff, 0x014527, 0x349ba3, 0x99a801,
    0x3e2077, 0x7e195c, 0x2f3e73, 0x980f42, 0x3ffedb, 0x7ff7a0, 0x2c551b, 0x9635ca,
    0x3d5df2, 0x7d56de, 0x3bd67d, 0x9807bf, 0x3e2077, 0x7e1954,
};

static const int8_t  kVocoderBandShift[16][3]   = {
    {-10,  0,  0}, {-1, -3, -10}, {-4, -1, -10}, {-1, -3, -9}, {-4, -2, -8}, {-1, -4, -8}, {-1, -4, -8}, {-5, -2, -7},
    { -1, -4, -7}, {-4, -3,  -6}, {-2, -4,  -5}, {-5, -2, -5}, {-3, -3, -4}, {-3, -3, -3}, {-3, -3, -2}, {-4, -2, -1},
};
static const int8_t  kVocoderConverterShift[3]  = {-5, -5, -4};

static double vocoder_word(int32_t w) {
    return (double)(((w & 0x800000) != 0) ? (w - 0x1000000) : w) / 8388608.0;
}

// §70.8 - a chain of the instrument's sections: w = 2(u - k1 w2 - k2 w1), y = w/2 + k3 w2 + k4 w1, the
// first fed k0 x the input, each later one the last's output shifted; every stored word saturates
static double vocoder_chain(double state[][2], const int32_t * k, uint32_t sections, const int8_t * shift, double x) {
    double u = vocoder_word(k[0]) * x;

    k++;

    for (uint32_t i = 0; i < sections; i++, k += 4) {
        if (i > 0) {
            u = ldexp(u, shift[i - 1]);
        }
        double * m = state[i];
        double   v = u - (vocoder_word(k[0]) * m[0]) - (vocoder_word(k[1]) * m[1]);
        double   w = fmin(fmax(2.0 * v, -1.0), 1.0);

        u    = v + (vocoder_word(k[2]) * m[0]) + (vocoder_word(k[3]) * m[1]);
        m[0] = m[1];
        m[1] = w;
    }

    return u;
}

static double vocoder_word_saturate(double v) {
    return fmin(fmax(v, -1.0), 1.0);
}

// §70.8 - Vocoder: Ctrl and In converted down to 24 kHz; on that tick sixteen analysis bands of Ctrl
// (optionally pre-emphasised), each rectified into a two-stage follower, scale sixteen synthesis bands
// of In through the BandSel routing; the sum is converted back up. Words are 1/4 of an engine unit.
static double vocoder_step(uint32_t voice, const tEngineNode * spec, double ctrl, double input) {
    SE_LOCAL;

    if (spec->line >= MAX_BASIC_LINES) {
        return 0.0;
    }
    tVocoderState * st    = &gVocoder[voice][spec->line];
    double          c     = ctrl / DSP_FULL_SCALE;
    double          x     = input / DSP_FULL_SCALE;
    double          scale = gSampleRate / G2_ENGINE_SAMPLE_RATE;
    double          cDown = vocoder_word_saturate(2.0 * vocoder_chain(&st->ctrlSec, kVocoderConverterWords, 1u, kVocoderConverterShift, c));
    double          xDown = vocoder_word_saturate(2.0 * vocoder_chain(st->inSec, &kVocoderConverterWords[5], 4u, kVocoderConverterShift, x));

    st->tick -= 1.0;

    if (st->tick <= 0.0) {
        double sum = 0.0;
        double a   = cDown;

        st->tick += 4.0 * scale;

        if (spec->bx[16] != 0.0) {
            double e0 = vocoder_word(VOCODER_EMPHASIS_0);

            a                = vocoder_word_saturate(8.0 * ((e0 * cDown) - (vocoder_word(VOCODER_EMPHASIS_1) * st->emphasisLast)));
            st->emphasisLast = vocoder_word_saturate(e0 * cDown);
        }

        for (uint32_t b = 0, at = 0; b < 16u; b++) {
            uint32_t sections = (b == 0u) ? 2u : 4u;
            double   rel      = vocoder_word(kVocoderFollowWords[b][0]);
            double   att      = vocoder_word(kVocoderFollowWords[b][1]);
            double   rect     = vocoder_word_saturate(fabs(vocoder_chain(st->analysis[b], &kVocoderBandWords[at], sections, kVocoderBandShift[b], a)));

            st->peak[b] = vocoder_word_saturate(fmax(rect, (rel * rect) + ((1.0 - rel) * st->peak[b])));
            st->env[b]  = vocoder_word_saturate(fmax((rel * st->peak[b]) + ((1.0 - rel) * st->env[b]),
                                                     (att * st->peak[b]) + ((1.0 - att) * st->env[b])));
            at         += 1u + (4u * sections);
        }

        for (uint32_t k = 0, at = 0; k < 16u; k++) {
            uint32_t sections = (k == 0u) ? 2u : 4u;
            uint32_t route    = (uint32_t)spec->bx[k];
            double   band     = vocoder_word_saturate(vocoder_chain(st->synthesis[k], &kVocoderBandWords[at], sections, kVocoderBandShift[k], xDown));

            if ((route >= 1u) && (route <= 16u)) {
                sum += st->env[route - 1u] * band;
            }
            at += 1u + (4u * sections);
        }

        st->outHeld = vocoder_word_saturate(16.0 * sum);
    }
    double out = vocoder_chain(st->outSec, &kVocoderConverterWords[5], 4u, kVocoderConverterShift, st->outHeld);

    if (spec->bx[17] != 0.0) {
        return ctrl;
    }
    return vocoder_word_saturate(out * vocoder_word(VOCODER_OUT_GAIN) * 8.0) * DSP_FULL_SCALE;
}

static void random_draw(double state[2], uint32_t * seed, const tEngineNode * spec);
static double random_level_shift(const tEngineNode * spec, double value);

// §70.9 - RndPattern, its loop part then Val's generator or State's gate, in the parts' words. The loop
// part reseeds the generator from Pattern, Bank and the two Seed inputs on a rising Rst (unpatched it
// reads high) or once its counter, restarted at -Loop, has counted past zero; each rising Clk counts and
// draws. state: 0-1 as random_draw(), 2 the counter, 3 Clk and 4 Rst as last read, 5 State's level, 6 started.
static double rnd_pattern_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double clock, double rst,
                               double seedIn, double seedFineIn, double probIn) {
    SE_LOCAL;

    double * st    = gLadder[voice][n];
    int32_t  clkW  = seq_sat(llround(clock * DSP_WORD_PER_ENGINE));
    int32_t  rstW  = (spec->in[1] >= 0) ? seq_sat(llround(rst * DSP_WORD_PER_ENGINE)) : 0x200000;
    bool     rstUp = (rstW > 0) && (st[4] <= 0.0);
    bool     clkUp = (clkW > 0) && (st[3] <= 0.0);
    bool     state = (spec->bx[3] != 0.0);

    st[4] = (double)rstW;
    st[3] = (double)clkW;

    if (st[6] == 0.0) {
        st[6] = 1.0;
        st[5] = RNDSTATE_START_LEVEL;    // State's level word before its first tick
    }

    if (rstUp || (st[2] > 0.0)) {
        int64_t seed = (int64_t)spec->bx[1] + (int64_t)spec->bx[0]
                       + (seq_sat(llround(seedFineIn * DSP_WORD_PER_ENGINE)) >> 7) + seq_sat(llround(seedIn * DSP_WORD_PER_ENGINE));

        gNoiseSeed[voice][n] = (uint32_t)seq_sat(seed) & 0xFFFFFFu;
        st[0]                = 0.0;
        st[2]                = spec->bx[2];
    }

    if (clkUp) {
        st[2] = fmin(st[2] + 1.0, 8388607.0);

        if (state == false) {
            random_draw(st, &gNoiseSeed[voice][n], spec);
        } else {
            int64_t thr = (int64_t)spec->bx[4]
                          + (((int64_t)seq_sat(llround(probIn * DSP_WORD_PER_ENGINE)) * (int64_t)spec->bx[5]) >> 20);

            gNoiseSeed[voice][n] = ((gNoiseSeed[voice][n] * 0xB2D9Du) + 0x361963u) & 0xFFFFFFu;

            if ((int64_t)((int32_t)(gNoiseSeed[voice][n] << 8) >> 8) <= (int64_t)seq_sat(thr)) {
                st[5] = 1.0;
            }
        }
    }

    if (state && (clkW <= 0)) {
        st[5] = -1.0;    // State is low while the clock is
    }

    if (spec->active == false) {
        return 0.0;
    }
    return random_level_shift(spec, state ? st[5] : st[1]);
}

// §70.10 - SeqCtr, its own program in its own words. Ctrl / 4 units picks the step, and Ctrl at or above 64
// units or below 0 picks a rest pair; within a step, w = (1 - the position) x the X-Fade scale, saturated,
// weighs the step against the next (step 16's next is step 1). Trig carries the step's event: as a gate, or
// for four ticks after the step word changes - which, the part's own test, is whenever Ctrl's low 16 bits
// are not zero. Not up-rated, the part ticks at 24 kHz and its outputs hold between (§58). state: 0 the step
// index as last stored, 1 the trig counter, 2 the time to the next tick, 3-4 the held outputs.
static int64_t seq_ctr_limit(int64_t acc) {
    int64_t top = acc >> 47;

    return ((top == 0) || (top == -1)) ? (acc >> 24) : ((acc < 0) ? -0x800000 : 0x7FFFFF);
}

static void seq_ctr_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double ctrl, double valIn, double trigIn,
                         double out[2]) {
    SE_LOCAL;

    double * st    = gLadder[voice][n];

    if (spec->bx[19] != 0.0) {
        if (st[2] > 0.0) {
            st[2] -= SEQ_CONTROL_TICK_HZ / gSampleRate;
            out[0] = st[3];
            out[1] = st[4];
            return;
        }
        st[2] += 1.0 - (SEQ_CONTROL_TICK_HZ / gSampleRate);
    }
    int32_t  c     = seq_sat(llround(ctrl * DSP_WORD_PER_ENGINE));
    int32_t  field = c & 0x1FFFF;
    int32_t  a1    = ((c >= 0x200000) || (c < 0)) ? SEQCTR_REST_WORD : c;
    int32_t  index = (a1 >> 16) & ~1;
    bool     moved = ((double)index != st[0]) || ((a1 & 0xFFFF) != 0);
    int64_t  one   = (int64_t)1 << 47;
    int64_t  w     = ((int64_t)1 << 23) - ((int64_t)field << 6);     // 1 - the position, Q23
    int32_t  shift = (int32_t)spec->bx[16];
    int32_t  vk    = 0;
    int32_t  vNext = 0;
    int32_t  event = 0;

    st[0]  = (double)index;
    st[1]  = (moved ? SEQCTR_TRIG_TICKS : st[1]) - 1.0;

    if (index < 32) {
        vk    = (int32_t)spec->bx[index >> 1];
        vNext = (int32_t)spec->bx[((index >> 1) + 1) & 15];
        event = (((spec->select >> (index >> 1)) & 1u) != 0u) ? 0x200000 : 0;
    } else {
        vk    = (spec->bx[17] != 0.0) ? SEQCTR_BIPOLAR_CENTRE : 0;    // the rest pair
        vNext = vk;
    }
    w      = (shift == 0) ? 0x7FFFFF : ((shift == 1) ? (w << 2) : ((shift == 2) ? (w << 1) : w));
    w      = (w > 0x7FFFFF) ? 0x7FFFFF : w;

    int64_t  acc   = (((int64_t)vk * w) + ((int64_t)vNext * (((int64_t)1 << 23) - w))) << 1;

    if (spec->bx[17] != 0.0) {
        acc = (acc - ((int64_t)SEQCTR_BIPOLAR_CENTRE << 24)) << 1;
    }
    acc   += (int64_t)seq_sat(llround(valIn * DSP_WORD_PER_ENGINE)) << 24;
    out[0] = (double)seq_ctr_limit(acc) / DSP_WORD_PER_ENGINE;

    int64_t  trig  = ((spec->bx[18] != 0.0) || (st[1] >= 0.0)) ? ((int64_t)event << 24) : 0;

    trig  += (int64_t)seq_sat(llround(trigIn * DSP_WORD_PER_ENGINE)) << 24;
    out[1] = (double)seq_ctr_limit(trig) / DSP_WORD_PER_ENGINE;
    st[3]  = out[0];
    st[4]  = out[1];
    (void)one;
}

// §70.11 - Mux8-1X: input k sits at Ctrl = 9k units (Ctrl held to 0..63), and takes a gain that falls
// linearly with Ctrl's distance from it, saturated to 0..1
static double mux8x_step(const tEngineNode * spec, double ctrl, const double in[8]) {
    double c   = fmin(fmax(ctrl * UNITS_PER_FULL_SCALE, 0.0), MUX8X_CTRL_MAX);
    double sum = 0.0;

    for (uint32_t k = 0; k < 8u; k++) {
        double gain = spec->bx[0] - (spec->bx[1] * fabs(c - (MUX8X_SPACING * (double)k)));

        sum += fmin(fmax(gain, 0.0), 1.0) * in[k];
    }

    return fmin(fmax(sum, -DSP_FULL_SCALE), DSP_FULL_SCALE);
}

// §70.12 - LevScaler: dB = L x octaves below the breakpoint, or R x octaves above; Level is that gain
// (1.0 = 64 units at 0 dB) and Out is In x it. The key is the voice's (Kbt) or the Note input (E4 = 0).
// §70.5 - Driver, in words (a quarter of an engine unit): In1 the excitation, In2 the return. Reed (and
// Lip and Mallet, which share its part): r = Emb - 4 Stiff (In2 - In1), saturated, out = In1 + r (In2 - In1).
// Bow: v = 8 Stiff |In1 - In2 + Emb|, saturated at 1, out = min(3 (1 - v)^3, 1) (In1 - In2).
static double driver_step(const tEngineNode * spec, double in1, double in2) {
    double b = in1 / DSP_FULL_SCALE;
    double a = in2 / DSP_FULL_SCALE;

    if (spec->select == DRIVER_TYPE_BOW) {
        double v = fmin(8.0 * spec->bx[0] * fabs(b - a + spec->bx[1]), 1.0);
        double f = fmin(3.0 * (1.0 - v) * (1.0 - v) * (1.0 - v), 1.0);

        return dsp_saturate(f * (b - a) * DSP_FULL_SCALE);
    }
    double r = fmin(fmax(spec->bx[1] - (4.0 * spec->bx[0] * (a - b)), -1.0), 1.0);

    return fmin(fmax(b + (r * (a - b)), -1.0), 1.0) * DSP_FULL_SCALE;
}

// §70.12 - the key (Note plus, with Kbt, the keyboard) less BrkPnt, in keys, times the slope word of its
// side over 256 is a step of the semitone gain table (0.5 dB), clamped to -128..95; Level is that gain and
// Out is In times it, both saturated
static void lev_scaler_step(const tEngineNode * spec, double noteIn, double input, double voicePitch, double out[2]) {
    double key   = (noteIn * UNITS_PER_FULL_SCALE)
                   + (((spec->bx[3] != 0.0) && (voicePitch >= 0.0)) ? (voicePitch - KEYBOARD_PITCH_ZERO) : 0.0);
    double d     = key - spec->bx[1];
    double step  = fmin(fmax(d * ((d < 0.0) ? spec->bx[0] : spec->bx[2]) / 256.0, -128.0), 95.0);
    double whole = floor(step);
    double gain  = exp2(whole / 12.0) + ((step - whole) * (exp2((whole + 1.0) / 12.0) - exp2(whole / 12.0)));

    out[0] = fmin(gain, DSP_FULL_SCALE);
    out[1] = dsp_saturate(input * out[0]);
}

// §68.6 - both counters step on a rising Clk (above zero now, not before) and are held at zero while
// Rst is high, the reset read after the step. The 8Counter wraps after its eighth output.
static void counter_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double clock, double rst, double out[NODE_OUTPUTS]) {
    SE_LOCAL;

    double * count = &gLadder[voice][n][0];
    bool     clk   = (clock > 0.0);

    if (clk && ((gLogicPrev[voice][n] & LOGIC_PREV_CLOCK) == 0u)) {
        *count = (spec->kind == eNodeCounter8) ? fmod(*count + 1.0, 8.0) : fmod(*count + 1.0, 256.0);
    }
    gLogicPrev[voice][n] = (uint8_t)(clk ? LOGIC_PREV_CLOCK : 0u);

    if (rst > 0.0) {
        *count = 0.0;
    }

    for (uint32_t k = 0; k < 8u; k++) {
        out[k] = logic_level((spec->kind == eNodeCounter8) ? ((uint32_t)*count == k) : ((((uint32_t)*count >> k) & 1u) != 0u));
    }
}

static double logic_level(bool high) {
    return (high == true) ? LOGIC_HIGH_LEVEL : 0.0;
}

// §46 - Pos and Neg run a counter while the input is in the state whose leading edge is delayed,
// and pass that state on once it has run the time out; Cycle shifts one whole pulse by the time.
static double logic_delay_step(uint32_t voice, uint32_t n, const tEngineNode * spec, double input, double modIn) {
    SE_LOCAL;

    bool       high    = logic_high(input);
    uint32_t   delay   = pulse_time_samples(spec, modIn);    // §18.3
    uint32_t * count   = &gPulseCount[voice][n];
    bool       wasHigh = ((gLogicPrev[voice][n] & LOGIC_PREV_CLOCK) != 0u);
    double *   fellAt  = &gLadder[voice][n][0];  // Cycle: samples from the rise to the fall, 0 until it falls
    bool       out     = false;

    gLogicPrev[voice][n] = high ? LOGIC_PREV_CLOCK : 0u;

    if (spec->select == LOGICDLY_TYPE_CYCLE) {
        // *count is samples since the rise plus one, 0 while idle: one pulse at a time
        if ((*count == 0u) && (high == true) && (wasHigh == false)) {
            *count  = 1u;
            *fellAt = 0.0;
        }

        if (*count > 0u) {
            if ((*fellAt == 0.0) && (high == false)) {
                *fellAt = (double)*count;
            }
            out = (*count > delay) && ((*fellAt == 0.0) || ((double)*count < ((double)delay + *fellAt)));

            if ((*fellAt > 0.0) && ((double)*count >= ((double)delay + *fellAt))) {
                *count = 0u;
            } else {
                (*count)++;
            }
        }
        return logic_level(out);
    }
    bool counting = (spec->select == LOGICDLY_TYPE_NEG) ? (high == false) : high;

    if (counting == false) {
        *count = 0u;
    } else if (*count < delay) {
        (*count)++;
    }
    out = (spec->select == LOGICDLY_TYPE_NEG) ? ((counting == false) || (*count < delay))
                                               : (*count >= delay);
    return logic_level(out);
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

static double drum_clamp(double x) {
    return fmin(1.0, fmax(-1.0, x));
}

// §39.10 - one Chamberlin stage of the noise filter; state is {low, band}
static double drum_svf_stage(double * state, double in, double f, double q, uint32_t type) {
    double low  = drum_clamp(state[0] + (f * state[1]));
    double high = drum_clamp(in - low - (2.0 * q * state[1]));
    double band = drum_clamp(state[1] + (f * high));

    state[0] = low;
    state[1] = band;
    return (type == 2u) ? high : ((type == 1u) ? band : low);
}

// §39.10 - the noise path in the DSP's words: LFSR, colour, envelope and amount, then two
// Chamberlin stages sharing one swept coefficient. `env` is velocity times the noise envelope.
static double drum_noise_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double env) {
    SE_LOCAL;

    double * state = gLadder[voice][node];    // low, band, low, band, last noise, colour
    uint32_t lfsr  = (gNoiseSeed[voice][node] & 0xFFFFFFu) << 1;
    double   pole  = pow(DRUM_COLOUR_POLE_96K, DRUM_INSTRUMENT_RATE / gSampleRate);
    double   word  = spec->drumNoiseWord * exp2(spec->drumSweepSemis * env / 12.0);
    double   hz    = (DRUM_INSTRUMENT_RATE / M_PI) * asin(fmin(1.0, word) * 0.5);
    double   f     = 2.0 * sin(M_PI * fmin(hz, gSampleRate * 0.25) / gSampleRate);
    double   q     = DRUM_Q_SCALE * spec->drumNoiseDamp * (1.0 - (f * 0.5));
    double   noise;
    double   colour;
    double   tap;

    lfsr                    = ((lfsr & 0x1000000u) != 0u) ? ((lfsr ^ DRUM_LFSR_TAPS) & 0xFFFFFFu) : (lfsr & 0xFFFFFFu);
    gNoiseSeed[voice][node] = (lfsr == 0u) ? 5555u : lfsr;
    noise                   = (double)(((int32_t)(lfsr << 8)) >> 8) / 8388608.0;
    colour                  = drum_clamp((0.5 * (1.0 + pole) * (noise - state[4])) + (pole * state[5]));
    state[4]                = noise;
    state[5]                = colour;
    tap                     = drum_svf_stage(&state[0], drum_clamp(colour * DRUM_VEL_WORD * env * spec->drumNoiseLevel), f, q,
                                             spec->drumFilterType);
    tap                     = drum_svf_stage(&state[2], drum_clamp(spec->drumNoiseDamp * tap), f, q, spec->drumFilterType);
    return tap * DRUM_DSP_TO_ENGINE;
}

// §39.6 - the increment a resonator is given for a frequency, saturating as the instrument's does
static double drum_osc_increment(double hz) {
    return fmin(1.0, fmax(0.0, 2.0 * M_PI * hz / DRUM_OSC_TICK_HZ));
}

// §39.6 - one oscillator: the resonator's own frequency and level for its increment, through a
// one-pole low-pass whose coefficient is eight times that increment
static double drum_osc_step(double * phase, double * smooth, double increment) {
    SE_LOCAL;

    double hz   = DRUM_OSC_TICK_HZ * asin(increment * 0.5) / M_PI;
    double gain = 1.0 / sqrt(1.0 - (increment * increment * 0.25));
    double a    = 1.0 - pow(1.0 - fmin(1.0, DRUM_OSC_SMOOTH * increment), DRUM_OSC_TICK_HZ / gSampleRate);

    if (increment <= 0.0) {
        return *smooth;
    }
    *phase   = advance_phase(phase, hz / gSampleRate);
    *smooth += a * ((gain * sin(*phase * 2.0 * M_PI)) - *smooth);
    return *smooth;
}

// §39 - one DrumSynth sample. Trig restarts every envelope and both oscillators; each envelope decays
// by its own multiplier per envelope tick. Vel (64 units unpatched) scales the strike, and with it the
// levels, the bend, the sweep, the click and the noise.
static double drum_synth_step(uint32_t voice, uint32_t node, const tEngineNode * spec,
                              double trig, double pitchIn, double velIn) {
    SE_LOCAL;

    double * st  = gDrumState[voice][node];
    bool     hit = (trig > 0.0) && ((gLogicPrev[voice][node] & LOGIC_PREV_CLOCK) == 0u);
    double   vel = (spec->in[1] >= 0) ? fmin(fmax(velIn, 0.0), 1.0) : 1.0;

    gLogicPrev[voice][node] = (uint8_t)((trig > 0.0) ? LOGIC_PREV_CLOCK : 0u);

    if (hit == true) {
        // §39.6 - the oscillators are resonators struck from rest, so every hit starts at phase 0
        st[DRUM_MASTER_PHASE]  = 0.0;
        st[DRUM_SLAVE_PHASE]   = 0.0;
        st[DRUM_MASTER_SMOOTH] = 0.0;
        st[DRUM_SLAVE_SMOOTH]  = 0.0;
        st[DRUM_MASTER_ENV]    = 1.0;
        st[DRUM_SLAVE_ENV]     = 1.0;
        st[DRUM_NOISE_ENV]     = 1.0;
        st[DRUM_BEND_ENV]      = 1.0;
        st[DRUM_CLICK_ENV]     = 1.0;
        st[DRUM_CLICK_HOLD]    = gSampleRate / ENV_TICK_HZ;
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

    if (st[DRUM_CLICK_HOLD] > 0.0) {
        st[DRUM_CLICK_HOLD] -= 1.0;
    } else {
        st[DRUM_CLICK_ENV] *= spec->drumClickDecay;
    }
    {
        // §39.6 - a Pitch input is one unit a semitone; the bend adds to it and the sum saturates
        double semis  = (pitchIn * PITCH_MOD_SEMITONES) + (spec->drumBendSemis * vel * st[DRUM_BEND_ENV]);
        double master = drum_osc_increment(spec->drumMasterHz
                                           * exp2(fmin(DRUM_PITCH_SEMIS_MAX, fmax(-DRUM_PITCH_SEMIS_MAX, semis)) / 12.0));
        double slave  = fmin(1.0, master * spec->drumSlaveRatio);
        double out    = 0.0;

        out += drum_osc_step(&st[DRUM_MASTER_PHASE], &st[DRUM_MASTER_SMOOTH], master)
               * st[DRUM_MASTER_ENV] * spec->drumLevel[0] * vel * DRUM_STRIKE_WORD * DRUM_DSP_TO_ENGINE;
        out += drum_osc_step(&st[DRUM_SLAVE_PHASE], &st[DRUM_SLAVE_SMOOTH], slave)
               * st[DRUM_SLAVE_ENV] * spec->drumLevel[1] * vel * DRUM_STRIKE_WORD * DRUM_DSP_TO_ENGINE;
        out += drum_noise_step(voice, node, spec, vel * st[DRUM_NOISE_ENV]);
        out += st[DRUM_CLICK_ENV] * spec->drumClick * vel * DRUM_CLICK_PEAK * DRUM_DSP_TO_ENGINE;
        return out;
    }
}

// §6.7 - the Shape word plus four times input x Shape M, saturated. An input of 1.0 is
// a quarter of full scale, so the factor of four makes it input x Shape M in word terms.
static double osc_shape_modulated(const tEngineNode * spec, double shape, double input) {
    bool   dialFraction = (spec->kind == eNodeOscShp);    // the shape oscillators keep dial/127
    double word         = (dialFraction ? wave_shape_word(shape) : shape) + (input * spec->shapeModAmount);

    word = fmin(fmax(word, SHAPE_WORD_MIN), SHAPE_WORD_MAX);

    if (dialFraction == false) {
        return word;
    }
    return (word >= SHAPE_WORD_MAX) ? 1.0 : ((word * 128.0) / 127.0);
}

// §28.4 - a rising edge on Rst clears the counter to its word 0 (half way round the engine's phase)
static void lfo_reset_edge(uint32_t voice, uint32_t n, const tEngineNode * spec, double value[][NODE_OUTPUTS], double rateHz) {
    SE_LOCAL;

    bool high    = (signal_in(spec, value, LFO_IN_RESET) > 0.0);
    bool wasHigh = ((gLogicPrev[voice][n] & LFO_RST_PREV_HIGH) != 0u);

    gLogicPrev[voice][n] = (uint8_t)((gLogicPrev[voice][n] & ~LFO_RST_PREV_HIGH) | (high ? LFO_RST_PREV_HIGH : 0u));

    if ((high == false) || (wasHigh == true)) {
        return;
    }

    if (spec->lfoMono == true) {
        gLfoMonoPhase[n] = LFO_RESET_PHASE;
    } else {
        gPhase[voice][n] = LFO_RESET_PHASE - (rateHz / gSampleRate);    // lfo_step advances it onto 0.5
    }
}

// §28.4 - the Phase dial plus Phase M, in cycles
static double lfo_read_offset(const tEngineNode * spec, double value[][NODE_OUTPUTS]) {
    double offset = spec->lfoPhase;

    if (spec->lfoPhaseSlot >= 0) {
        offset += spec->lfoPhaseMod * signal_in(spec, value, (uint32_t)spec->lfoPhaseSlot);
    }
    return offset;
}

// §28.4 - LfoShpA's Shape plus Shape M, saturated
static double lfo_shape_now(const tEngineNode * spec, double value[][NODE_OUTPUTS], double shape) {
    if (spec->lfoShapeSlot < 0) {
        return shape;
    }
    return fmin(fmax(shape + (spec->lfoShapeMod * signal_in(spec, value, (uint32_t)spec->lfoShapeSlot)), 0.0), 1.0);
}

// §6.6 - a rising crossing of the Sync input: the last sample at or below zero, this one above
static bool osc_sync_edge(uint32_t voice, uint32_t n, const tEngineNode * spec, double value[][NODE_OUTPUTS]) {
    SE_LOCAL;

    if ((spec->syncSlot < 0) || (spec->in[spec->syncSlot] < 0)) {
        return false;
    }
    bool high    = (signal_in(spec, value, (uint32_t)spec->syncSlot) > 0.0);
    bool wasHigh = ((gLogicPrev[voice][n] & OSC_SYNC_PREV_HIGH) != 0u);

    gLogicPrev[voice][n] = (uint8_t)((gLogicPrev[voice][n] & ~OSC_SYNC_PREV_HIGH) | (high ? OSC_SYNC_PREV_HIGH : 0u));
    return (high == true) && (wasHigh == false);
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

    sEvalVoice = voice;

    double              a      = signal_in(spec, value, 0);

    for (uint32_t leg = 0; leg < NODE_OUTPUTS; leg++) {
        value[n][leg] = 0.0;
    }

    switch (spec->kind) {
        case eNodeLfo:
        {
            double rate = lfo_rate_now(spec, a, signal_in(spec, value, 1), voicePitch);

            // §28.4 - LfoShpA's Dir scales the counter's step: unpatched it reads 1, negative runs backwards
            if ((spec->lfoShapeSlot >= 0) && (spec->in[LFOSHPA_IN_DIR] >= 0)) {
                rate *= signal_in(spec, value, LFOSHPA_IN_DIR);
            }
            gLfoMonoRate[n] = rate;

            if (spec->lfoHasReset == true) {
                lfo_reset_edge(voice, n, spec, value, rate);
            }
            value[n][0]     = lfo_step(voice, n, spec, rate, lfo_read_offset(spec, value), lfo_shape_now(spec, value, spec->shape));

            // §54 - LfoB and LfoShpA's Snc: HIGH through the second half of the counter's cycle
            if (spec->lfoHasSync == true) {
                double counter = (spec->lfoMono == true) ? gLfoMonoPhase[n] : gPhase[voice][n];

                value[n][1] = logic_level(counter >= 0.5);
            } else {
                value[n][1] = value[n][0];
            }
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

            if ((spec->shapeModSlot >= 0) && (spec->in[spec->shapeModSlot] >= 0)) {
                shape = osc_shape_modulated(spec, shape, signal_in(spec, value, (uint32_t)spec->shapeModSlot));
            }
            double fmIn = ((spec->fmSlot >= 0) && (spec->in[spec->fmSlot] >= 0))
                          ? signal_in(spec, value, (uint32_t)spec->fmSlot) : 0.0;

            value[n][0] = (spec->active == true)
                              ? oscillator_step(voice, n, spec, voicePitch, a, signal_in(spec, value, 1), shape,
                                                osc_sync_edge(voice, n, spec, value), fmIn)
                              : 0.0;
            break;
        }
        case eNodeFilter:
        {
            // spec->fltGain is FltNord's GC and is 1.0 for every other filter, so this costs a
            // multiply and changes nothing where the module has no such control.
            value[n][0] = filter_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2), voicePitch,
                                      cutoff, res, signal_in(spec, value, 3), signal_in(spec, value, 4)) * spec->fltGain;
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
                value[n][0] = (spec->modAmtOneMinus == true) ? a : 0.0; // §29.4 - off: 1-m passes In, m is silent
            } else {
                double share = fmin(1.0, fmax(-1.0, gain * mod));       // §29.2 - held to one register's range

                value[n][0] = (spec->modAmtOneMinus == true) ? (a * ((1.0 - gain) + share)) : (a * share);
            }
            break;
        }
        case eNodeSwitch:
        {
            // §30 - closed passes In, or 64 units with nothing patched; open sends nothing. Ctrl is the
            // position, 4 units a step as every switch's is (§33.1): 4 closed, 0 open.
            double closed = (spec->active == true) ? LOGIC_HIGH_LEVEL : 0.0;

            value[n][0] = (spec->in[0] < 0) ? closed : (a * ((spec->active == true) ? 1.0 : 0.0));
            value[n][1] = (spec->active == true) ? (SWSEL_CTRL_UNITS / UNITS_PER_FULL_SCALE) : 0.0;
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
            // §34 - In 2 while Ctrl EQUALS the value, In 1 otherwise
            double ctrl = signal_in(spec, value, 2);

            value[n][0] = (fabs(ctrl - spec->constant) <= (VALSW_MATCH_UNITS / UNITS_PER_FULL_SCALE)) ? signal_in(spec, value, 1) : a;
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
        case eNodeKeyQuant:
        {
            value[n][0] = keyquant_step(spec, a);
            break;
        }
        case eNodeCompLev:
        {
            value[n][0] = logic_level(a >= spec->constant);   // §48
            break;
        }
        case eNodeNoteQuant:
        {
            value[n][0] = note_quant_step(spec, a);
            break;
        }
        case eNodePhaser:
        {
            value[n][0] = phaser_step(voice, n, spec, a);
            break;
        }
        case eNodeValSw12:
        {
            // §68.2 - Out 2 while Ctrl equals the value, Out 1 otherwise
            value[n][(fabs(signal_in(spec, value, 1) - spec->constant) <= (VALSW_MATCH_UNITS / UNITS_PER_FULL_SCALE)) ? 1 : 0] = a;
            break;
        }
        case eNodeMux8to1:
        {
            value[n][0] = signal_in(spec, value, mux_select(signal_in(spec, value, 8)));
            break;
        }
        case eNodeMux1to8:
        {
            value[n][mux_select(signal_in(spec, value, 1))] = a;
            break;
        }
        case eNodeTandH:
        {
            // §68.4 - Ctrl above zero tracks; the held value is the last one tracked
            double * held = &gLadder[voice][n][0];

            if (signal_in(spec, value, 1) > 0.0) {
                *held = a;
            }
            value[n][0] = *held;
            break;
        }
        case eNodeWindSw:
        {
            double ctrl = signal_in(spec, value, 1);
            bool   in   = (ctrl >= spec->constant) && (ctrl <= spec->modAmount);

            value[n][0] = in ? a : 0.0;
            value[n][1] = logic_level(in);
            break;
        }
        case eNodeCounter8:
        case eNodeBinCounter:
        {
            counter_step(voice, n, spec, a, signal_in(spec, value, 1), value[n]);
            break;
        }
        case eNodeADConv:
        {
            // §68.7 - In x 4 as a word, saturated, then its top eight bits
            int32_t code = dly_sat((int64_t)engine_word(a) * 4) >> 16;

            for (uint32_t bit = 0; bit < 8u; bit++) {
                value[n][bit] = logic_level(((code >> bit) & 1) != 0);
            }

            break;
        }
        case eNodeDAConv:
        {
            // §68.7 - D7 is the sign; half a unit a step
            int32_t code = 0;

            for (uint32_t bit = 0; bit < 8u; bit++) {
                code |= (signal_in(spec, value, bit) > 0.0) ? (1 << bit) : 0;
            }

            value[n][0] = (double)((code >= 128) ? (code - 256) : code) / (2.0 * UNITS_PER_FULL_SCALE);
            break;
        }
        case eNodeRatePass:
        {
            value[n][0] = a;
            break;
        }
        case eNodePartQuant:
        {
            value[n][0] = part_quant_step(spec, a);
            break;
        }
        case eNodeDlyShiftReg:
        {
            // §69.6 - on a rising Clk the eight shift one along and In enters at Out 1; they hold between
            double * reg = gLadder[voice][n];
            bool     clk = (signal_in(spec, value, 1) > 0.0);

            if (clk && ((gLogicPrev[voice][n] & LOGIC_PREV_CLOCK) == 0u)) {
                memmove(&reg[1], &reg[0], 7u * sizeof(double));
                reg[0] = a;
            }
            gLogicPrev[voice][n] = (uint8_t)(clk ? LOGIC_PREV_CLOCK : 0u);

            for (uint32_t k = 0; k < 8u; k++) {
                value[n][k] = reg[k];
            }

            break;
        }
        case eNodeMultiTap:
        {
            double mods[4] = {signal_in(spec, value, 1), signal_in(spec, value, 2), signal_in(spec, value, 3), signal_in(spec, value, 4)};

            multi_tap_step(spec, a, mods, value[n]);
            break;
        }
        case eNodeFlanger:
        {
            value[n][0] = flanger_step(spec, voice, a);
            break;
        }
        case eNodePShift:
        {
            // PShift's inputs are PitchVar then In; Scratch's In then Mod
            value[n][0] = (spec->select == 0u) ? pitch_shift_step(spec, voice, signal_in(spec, value, 1), a)
                          : pitch_shift_step(spec, voice, a, signal_in(spec, value, 1));
            break;
        }
        case eNodeOscString:
        {
            value[n][0] = karplus_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2), voicePitch);
            value[n][1] = value[n][0];
            break;
        }
        case eNodeResonator:
        {
            resonator_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2), voicePitch, value[n]);
            break;
        }
        case eNodeDriver:
        {
            value[n][0] = driver_step(spec, a, signal_in(spec, value, 1));
            break;
        }
        case eNodeNoiseGate:
        {
            noise_gate_step(voice, n, spec, a, value, value[n]);
            break;
        }
        case eNodePitchTrack:
        {
            pitch_track_step(voice, gLadder[voice][n], spec, a, value[n]);
            break;
        }
        case eNodeVocoder:
        {
            value[n][0] = vocoder_step(voice, spec, a, signal_in(spec, value, 1));   // Ctrl, In
            break;
        }
        case eNodeRndPattern:
        {
            value[n][0] = rnd_pattern_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2),
                                           signal_in(spec, value, 3), signal_in(spec, value, 4));
            break;
        }
        case eNodeSeqCtr:
        {
            seq_ctr_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2), value[n]);
            break;
        }
        case eNodeMux8to1X:
        {
            double in8[8];

            for (uint32_t k = 0; k < 8u; k++) {
                in8[k] = signal_in(spec, value, k);
            }

            value[n][0] = mux8x_step(spec, signal_in(spec, value, 8), in8);
            break;
        }
        case eNodeLevScaler:
        {
            lev_scaler_step(spec, a, signal_in(spec, value, 1), voicePitch, value[n]);
            break;
        }
        case eNodeStatus:
        {
            // §70.13 - Var Active is low for one tick after the variation changes; Voice No. is 4
            // units a voice (the index's low five bits), so it picks SeqCtr's steps one per voice
            double * state = gLadder[voice][n];

            if (state[0] != (double)spec->select) {
                state[1] = (state[0] != 0.0) ? (gSampleRate / STATUS_TICK_HZ) : 0.0;
                state[0] = (double)spec->select;
            }
            value[n][0] = LOGIC_HIGH_LEVEL;
            value[n][1] = (state[1] > 0.0) ? 0.0 : LOGIC_HIGH_LEVEL;
            state[1]   -= 1.0;
            value[n][2] = (spec->postMix == true) ? 0.0 : ((double)(voice & 0x1fu) * 4.0 / UNITS_PER_FULL_SCALE);
            break;
        }
        case eNodeDevice:
        {
            // §70.13 - Wheel, Aftertouch, Control pedal, Sustain, Pitch stick, Global wheels 1 and 2
            value[n][0] = (double)atomic_load(&gMorphMilli[MORPH_GROUP_WHEEL]) / 1000.0;
            value[n][1] = (double)atomic_load(&gMorphMilli[MORPH_GROUP_AFTERTOUCH]) / 1000.0;
            value[n][2] = (double)atomic_load(&gMorphMilli[5]) / 1000.0;
            value[n][3] = logic_level(atomic_load(&gSustainPedal));
            value[n][4] = (double)atomic_load(&gBendMilli) / 1000.0;
            // the G2X global wheels arrive as CC 96 and CC 97 on the slot's channel (manual, MIDI CC list)
            value[n][5] = midi_cc_level(atomic_load(&gMidiCcValue[MIDI_ROW_THIS][MIDI_CC_GLOBAL_WHEEL_1]));
            value[n][6] = midi_cc_level(atomic_load(&gMidiCcValue[MIDI_ROW_THIS][MIDI_CC_GLOBAL_WHEEL_2]));
            break;
        }
        case eNodeCtrlRcv:
        {
            // §70.13 - each arrival latches Val and raises Rcv for one 24 kHz tick. state: 0 the arrivals
            // seen (+1, 0 before the first sample), 1 Rcv's time left, 2 Val as latched
            double * st    = gLadder[voice][n];
            uint32_t row   = (uint32_t)spec->bx[0];
            double   count = (double)atomic_load(&gMidiCcCount[row][spec->select]) + 1.0;

            if ((st[0] != 0.0) && (count != st[0])) {
                st[1] = gSampleRate / CTRLRCV_TICK_HZ;
                st[2] = midi_cc_level(atomic_load(&gMidiCcValue[row][spec->select]));
            }
            st[0]       = count;
            value[n][0] = (st[1] > 0.0) ? LOGIC_HIGH_LEVEL : 0.0;
            value[n][1] = st[2];
            st[1]      -= 1.0;
            break;
        }
        case eNodeSink:
        {
            break;
        }
        case eNodeNoteDet:
        {
            // §69.11 - the host writes both velocities as v x 2^14: v / 128 here
            if ((uint32_t)spec->bx[0] < MIDI_ROWS) {
                // §70.13 - NoteRcv on a MIDI channel: the gate and velocities as they last arrived there
                uint32_t row  = (uint32_t)spec->bx[0];
                uint8_t  vel  = atomic_load(&gMidiNoteVel[row][spec->select]);
                double * held = &gLadder[voice][n][0];   // Vel as the last note-on left it

                *held       = (vel > 0u) ? ((double)vel / NOTEDET_VELOCITY_SCALE) : *held;
                value[n][0] = logic_level(vel > 0u);
                value[n][1] = *held;
                value[n][2] = (double)atomic_load(&gMidiNoteRel[row][spec->select]) / NOTEDET_VELOCITY_SCALE;
                break;
            }
            value[n][0] = logic_level(gKeyHeld[spec->select] > 0u);
            value[n][1] = (double)gKeyVelocity[spec->select] / NOTEDET_VELOCITY_SCALE;
            value[n][2] = (double)gKeyReleaseVelocity[spec->select] / NOTEDET_VELOCITY_SCALE;
            break;
        }
        case eNodeWahWah:
        {
            value[n][0] = wah_wah_step(gLadder[voice][n], spec, a, signal_in(spec, value, 1));
            break;
        }
        case eNodeDigitizer:
        {
            value[n][0] = digitizer_step(gLadder[voice][n], spec, a, signal_in(spec, value, 1));
            break;
        }
        case eNodeDlyClock:
        {
            value[n][0] = dly_clock_step(voice, n, spec, a, signal_in(spec, value, 1));
            break;
        }
        case eNodeCompSig:
        {
            value[n][0] = logic_level(engine_word(a) >= engine_word(signal_in(spec, value, 1)));   // §69.2
            break;
        }
        case eNodeLevMod:
        {
            value[n][0] = lev_mod_step(spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2));
            break;
        }
        case eNodeEnvFollow:
        {
            value[n][0] = env_follow_step(&gLadder[voice][n][0], spec, a);
            break;
        }
        case eNodeFltPhase:
        {
            double mods[4] = {signal_in(spec, value, 1), signal_in(spec, value, 2), signal_in(spec, value, 3), signal_in(spec, value, 4)};

            value[n][0] = flt_phase_step(voice, spec, a, mods, voicePitch);
            break;
        }
        case eNodeMetNoise:
        {
            value[n][0] = met_noise_step(voice, spec, a, signal_in(spec, value, 1));
            break;
        }
        case eNodeDlyStereo:
        {
            dly_stereo_step(spec, a, &value[n][0], &value[n][1]);
            break;
        }
        case eNodeRndClkA:
        {
            // §70.9 - RndClkB's Step M jack links in its Step M part; unpatched, B is A
            double stepMod = signal_in(spec, value, RNDCLKB_IN_STEP_MOD);
            bool   linked  = (spec->inCount > RNDCLKB_IN_STEP_MOD) && (spec->in[RNDCLKB_IN_STEP_MOD] >= 0);

            value[n][0] = rnd_clk_a_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2),
                                         linked ? &stepMod : NULL);
            break;
        }
        case eNodeRndTrig:
        {
            value[n][0] = rnd_trig_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2),
                                        signal_in(spec, value, 3));
            break;
        }
        case eNodeNoteSend:
        {
            // §62 - the part's note and velocity bytes, sent on the gate's edges
            bool gate    = (a > 0.0);
            bool wasHigh = ((gLogicPrev[voice][n] & LOGIC_PREV_CLOCK) != 0u);

            gLogicPrev[voice][n] = gate ? LOGIC_PREV_CLOCK : 0u;

            if ((spec->active == true) && (gate != wasHigh)) {
                if (gate == true) {
                    int64_t noteWord = (int64_t)spec->constant + llround(signal_in(spec, value, 2) * DSP_WORD_PER_ENGINE);
                    int64_t velWord  = (int64_t)spec->depth + llround(signal_in(spec, value, 1) * DSP_WORD_PER_ENGINE);
                    int32_t note     = (int32_t)((noteWord < 0) ? 0 : (noteWord >> 15));
                    int32_t vel      = (int32_t)((velWord < 0) ? 0 : (seq_sat(velWord * 4) >> 16));

                    note                  = (note > 127) ? 127 : note;
                    vel                   = (vel > 127) ? 127 : vel;
                    gPulseCount[voice][n] = (uint32_t)note + 1u;   // what the release must name
                    sound_engine_note(note, (uint8_t)vel, true);
                } else if (gPulseCount[voice][n] > 0u) {
                    sound_engine_note((int32_t)gPulseCount[voice][n] - 1, 0, false);
                    gPulseCount[voice][n] = 0u;
                }
            }
            break;
        }
        case eNodeNoteScaler:
        {
            int32_t in = seq_sat(llround(a * DSP_WORD_PER_ENGINE));

            value[n][0] = seq_sat(((int64_t)in * (int64_t)spec->constant) >> 23) / DSP_WORD_PER_ENGINE;   // §60
            break;
        }
        case eNodeClkGen:
        {
            // §59 - a tick every 24 kHz; outputs held between. Module outputs are 1/96, 1/16, ClkActive,
            // Sync, which are the part's outputs 0, 1, 3, 2
            if (spec->line < MAX_CLKGEN_LINES) {
                tClkGenState * st = &gClkGen[voice][spec->line];

                if ((st->ready == false) || (st->wait == 0u)) {
                    uint32_t ticks = (uint32_t)lround(gSampleRate / CLKGEN_TICK_HZ);

                    clkgen_tick(st, &paramsIn->clkGen[spec->line], seq_sat(llround(a * DSP_WORD_PER_ENGINE)));
                    st->wait = (ticks > 0u) ? ticks : 1u;
                }
                st->wait--;
                value[n][0] = st->out[0] / DSP_WORD_PER_ENGINE;
                value[n][1] = st->out[1] / DSP_WORD_PER_ENGINE;
                value[n][2] = st->out[3] / DSP_WORD_PER_ENGINE;
                value[n][3] = st->out[2] / DSP_WORD_PER_ENGINE;
            }
            break;
        }
        case eNodeSeq16:
        {
            // §58 - words in, words out; the engine's 1.0 is 64 units, 0x200000
            if (spec->line < MAX_SEQ_LINES) {
                int32_t            in[8];
                int32_t            out[3];
                const tSeqConfig * cfg = &paramsIn->seq[spec->line];
                tSeqState *        st  = &gSeq[voice][spec->line];

                if (st->ready == false) {
                    st->tick = 0.0;
                    memset(st->held, 0, sizeof(st->held));
                }

                // §58 - a control-rate part ticks at 24 kHz and its outputs hold between ticks
                if ((cfg->controlRate != 0u) && (st->tick > 0.0)) {
                    memcpy(out, st->held, sizeof(out));
                } else {
                    for (uint32_t k = 0; k < 8u; k++) {
                        in[k] = ((k < 6u) || (cfg->record != 0u)) ? seq_sat(llround(signal_in(spec, value, k) * DSP_WORD_PER_ENGINE)) : 0;
                    }

                    seq16_step(st, cfg, in, out);

                    if (cfg->record != 0u) {
                        out[1] = seqrec_step(st, cfg->recordDelay, out[1], in[SEQ_IN_REC_VAL], in[SEQ_IN_REC_ENABLE]);
                    }
                    memcpy(st->held, out, sizeof(st->held));
                    st->tick += 1.0;
                }

                if (cfg->controlRate != 0u) {
                    st->tick -= SEQ_CONTROL_TICK_HZ / gSampleRate;
                }
                value[n][0] = out[0] / DSP_WORD_PER_ENGINE;
                value[n][1] = out[1] / DSP_WORD_PER_ENGINE;
                value[n][2] = out[2] / DSP_WORD_PER_ENGINE;
            }
            break;
        }
        case eNodeFreqShift:
        {
            // §57 - Mod is input 0, In input 1; Down and Up
            freqshift_step(voice, spec, signal_in(spec, value, 1), a, &value[n][0], &value[n][1]);
            break;
        }
        case eNodeFltVoice:
        {
            value[n][0] = fltvoice_step(voice, n, spec, a, signal_in(spec, value, 1), signal_in(spec, value, 2));
            break;
        }
        case eNodeOscPM:
        {
            // §53 - Pitch is input 3, PitchVar input 0; Phase M input 2
            double hz = osc_frequency_hz(spec, voicePitch, signal_in(spec, value, 3), a);

            value[n][0] = (spec->active == true) ? osc_pm_step(voice, n, spec, hz, signal_in(spec, value, 2), osc_sync_edge(voice, n, spec, value)) : 0.0;
            break;
        }
        case eNodeDlySingle:
        {
            value[n][0] = dly_single_step(spec->line, a, signal_in(spec, value, 1), spec);
            break;
        }
        case eNodeOscMaster:
        {
            // §51 - the key, Pitch and PitchVar x Pitch M on top of the dials, saturated as a word
            double key = spec->lfoKbt * (voicePitch - KEYBOARD_PITCH_ZERO) / PITCH_MOD_SEMITONES;

            value[n][0] = fmax(-4.0, fmin(4.0, key + spec->constant + a + (signal_in(spec, value, 1) * spec->lfoRateMod)));
            break;
        }
        case eNodeMinMax:
        {
            // §43 - Min, then Max; the inputs are read as they are and the results saturate
            double b = signal_in(spec, value, 1);

            value[n][0] = fmax(-4.0, fmin(4.0, fmin(a, b)));
            value[n][1] = fmax(-4.0, fmin(4.0, fmax(a, b)));
            break;
        }
        case eNodeSw1to8:
        {
            // §45 - In on the selected output, nothing on the rest, and which one it is on Ctrl
            uint32_t pick = (spec->select < spec->outCount) ? spec->select : 0u;

            value[n][pick]           = a;
            value[n][spec->outCount] = ((double)pick * SWSEL_CTRL_UNITS) / UNITS_PER_FULL_SCALE;
            break;
        }
        case eNodeLogicDelay:
        {
            value[n][0] = logic_delay_step(voice, n, spec, a, signal_in(spec, value, 1));
            break;
        }
        case eNodeRandomA:
        {
            if (spec->lfoMono == true) {
                gLfoMonoRate[n] = lfo_rate_now(spec, a, signal_in(spec, value, 1), voicePitch);
                value[n][0]     = gRndMono[n].out;   // §47 - stepped once a sample, before the voices
            } else {
                value[n][0] = random_a_step(gLadder[voice][n], &gNoiseSeed[voice][n], &gPhase[voice][n], spec,
                                            lfo_rate_now(spec, a, signal_in(spec, value, 1), voicePitch));   // §69.10 - RandomB's RateVar
            }
            break;
        }
        case eNodeSandH:
        {
            // §38.5 - the instrument's edge test: Ctrl above zero now, not above it before
            double * held  = &gLadder[voice][n][0];
            double   clock = signal_in(spec, value, 1);

            if ((clock > 0.0) && ((gLogicPrev[voice][n] & LOGIC_PREV_CLOCK) == 0u)) {
                *held = a;
            }
            gLogicPrev[voice][n] = (uint8_t)((clock > 0.0) ? LOGIC_PREV_CLOCK : 0u);
            value[n][0]          = *held;
            break;
        }
        case eNodeDrumSynth:
        {
            // §39 - Trig, Vel, Pitch in, in the instrument's input order; one audio output.
            value[n][0] = (spec->active == true)
                          ? drum_synth_step(voice, n, spec, a, signal_in(spec, value, 2),
                                            signal_in(spec, value, 1))
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
            value[n][0] = pulse_step(voice, n, a, signal_in(spec, value, 1), spec);
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
        case eNodeOscPerc:
        {
            double hz  = osc_frequency_hz(spec, voicePitch, a, signal_in(spec, value, 1));
            double out = perc_step(voice, n, spec, hz, signal_in(spec, value, 2));

            value[n][0] = (spec->active == true) ? out : 0.0;
            break;
        }
        case eNodeOscNoise:
        {
            // §8.4 - Width + Width M x the input, 64 units at 127 moving it across the whole dial
            double width = spec->oscNoiseWidth + (spec->oscNoiseWidthMod * signal_in(spec, value, 2));
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
            // `a` is input leg 0; spec->shaper.signalLeg says which leg carries the signal.
            double sig = (spec->shaper.signalLeg == 0) ? a : signal_in(spec, value, 1);
            double mod = (spec->shaper.signalLeg == 0) ? signal_in(spec, value, 1) : a;

            value[n][0] = (spec->shaper.kind == eShaperOverdrive) ? overdrive_step(voice, n, spec, sig, mod) : shaper_step(sig, mod, spec);
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
            if ((spec->active == true) && (spec->line < ((spec->postMix == true) ? MAX_CHORUS_FX_LINES : MAX_CHORUS_LINES))) {
                uint32_t instance = (spec->postMix == true) ? spec->line
                                    : (MAX_CHORUS_FX_LINES + (spec->line * MAX_VOICES) + voice);   // notes §197

                chorus_step(instance, a, spec->depth, spec->amount, &value[n][0], &value[n][1]);
            } else {
                value[n][0] = a;
                value[n][1] = a;
            }
            break;
        }
        case eNodeCompress:
        {
            // §25.2 - the module's first output jack (the right-hand one) is R, its second L
            double inR  = signal_in(spec, value, 1);
            double outR = inR;
            double outL = (spec->active == true) ? compress_step(voice, n, a, inR, signal_in(spec, value, 2), spec, &outR) : a;

            value[n][0] = outR;
            value[n][1] = outL;
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
            double left  = 0.0;
            double right = 0.0;

            for (uint32_t c = 0; (c + 1u) < spec->inCount; c += 2u) {
                left  += signal_in(spec, value, c);
                right += signal_in(spec, value, c + 1u);
            }

            value[n][0] = (spec->active == true) ? (left * gain) : 0.0;
            value[n][1] = (spec->active == true) ? (right * gain) : 0.0;
            break;
        }
        case eNodeIn4Bus:
        {
            // §69.12 - the first select pairs are Bus 1/2, the rest Bus 3/4
            double sum[4] = {0.0, 0.0, 0.0, 0.0};

            for (uint32_t c = 0; (c + 1u) < spec->inCount; c += 2u) {
                uint32_t leg = ((c / 2u) < spec->select) ? 0u : 2u;

                sum[leg]      += signal_in(spec, value, c);
                sum[leg + 1u] += signal_in(spec, value, c + 1u);
            }

            for (uint32_t k = 0; k < 4u; k++) {
                value[n][k] = (spec->active == true) ? (sum[k] * gain) : 0.0;
            }

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
        case eNodeCompress:     // §25.2 - R and L
        case eNodeMixStereo:    // a genuine stereo pair
        case eNodeFltMulti:     // three outputs of its own
        case eNodeFxIn:         // the FX bus's two legs
        case eNodeMonoKey:      // §35 - Pitch, Gate and Vel
        case eNodeSwSelect:     // §33 - Out and Ctrl, which are not a stereo pair
        case eNodeInvert:       // §38.1 - two independent inverters
        case eNodeGate:         // §38.2 - two independent gates
        case eNodeFlipFlop:     // §38.3 - NotQ and Q
        case eNodeSwitch:       // §30 - likewise
        case eNodeMinMax:       // §43 - Min and Max
        case eNodeLfo:          // §54 - Out and, on two of them, Snc
        case eNodeFreqShift:    // §57 - Down and Up
        case eNodeDlyStereo:    // §65 - Out1 and Out2
        case eNodeSeq16:        // §58 - Link, the value row and the other row
        case eNodeClkGen:       // §59 - four outputs
        case eNodeSw1to8:       // §45 - eight outputs and Ctrl
        case eNodeValSw12:      // §68 - separate outputs, none a stereo pair
        case eNodeMux1to8:
        case eNodeWindSw:
        case eNodeCounter8:
        case eNodeBinCounter:
        case eNodeADConv:
        case eNodeDlyShiftReg:  // §69.6
        case eNodeNoteDet:      // §69.11
        case eNodeMultiTap:     // §70 - separate outputs, none a stereo pair
        case eNodeIn4Bus:       // §69.12 - four outputs, the buses' two pairs
        case eNodeSeqCtr:       // §70.10 - Val and Trig
        case eNodeNoiseGate:
        case eNodePitchTrack:
        case eNodeResonator:
        case eNodeLevScaler:
        case eNodeStatus:
        case eNodeDevice:
        case eNodeCtrlRcv:
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
    // notes §196
    {
        double limit = (spec->kind == eNodeOut) ? (2.0 * DSP_FULL_SCALE) : DSP_FULL_SCALE;   // the Out's extra 6 dB

        value[n][0] = fmin(fmax(value[n][0], -limit), limit);
        value[n][1] = fmin(fmax(value[n][1], -limit), limit);
    }

    // notes §192 - what the loop legs reading this node will see next sample; a node after the mix
    // runs once, so every voice sees the same
    for (uint32_t e = 0; e < paramsIn->backCount; e++) {
        if (paramsIn->backSrc[e] == (int32_t)n) {
            uint32_t first = (spec->postMix == true) ? 0u : voice;
            uint32_t last  = (spec->postMix == true) ? (MAX_VOICES - 1u) : voice;

            for (uint32_t v = first; v <= last; v++) {
                gBackValue[v][e] = fmin(fmax(value[n][paramsIn->backLeg[e]], -DSP_FULL_SCALE), DSP_FULL_SCALE);
            }
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

// notes §201 - the per-sample work of one engine, as two passes: the voices (with note events and the
// smoothing of their own dials) and everything after the mix. Serial, they run back to back; split,
// the FX pass runs on the audio thread one SPLIT_LAG behind the voices on the worker.
typedef enum {
    eSmoothAll,
    eSmoothVoice,
    eSmoothFx
} tSmoothWhich;

typedef struct {
    bool   chainHasEnvelope;
    bool   droneMode;
    double envelopeStep;
    double rampSamples;
    double glideStep;
} tStageCtx;

static void stage_smooth(const tSoundEngineParams * p, double rampSamples, tSmoothWhich which) {
    SE_LOCAL;

    // PARAMETER SMOOTHING IS PER SAMPLE, NOT PER VOICE. It tracks where a knob is, which is
    // one thing however many notes are sounding — and running it inside the voice loop would
    // advance it once per voice, so a knob would sweep faster the more keys were held.
    for (uint32_t n = 0; n < p->nodeCount; n++) {
        const tEngineNode * spec     = &p->node[n];

        if ((which != eSmoothAll) && (spec->postMix != (which == eSmoothFx))) {
            continue;
        }
        bool                primed   = gSmoothPrimed[n];

        gSmoothedShape[n]  = smooth_to(&gSmoothShape[n], spec->shape, rampSamples, primed);
        // notes §177
        gSmoothedCutoff[n] = smooth_to(&gSmoothCutoff[n], spec->cutoffParam, rampSamples, primed);
        gSmoothedRes[n]    = smooth_to(&gSmoothRes[n], spec->resonance, rampSamples, primed);
        gSmoothedGain[n]   = smooth_to(&gSmoothGain[n], spec->gain, rampSamples, primed);

        // §9.2
        for (uint32_t c = 0; c < spec->levelCount; c++) {
            gSmoothedLevel[n][c] = smooth_to(&gSmoothLevel[n][c], spec->level[c], rampSamples, primed);
        }

        gSmoothPrimed[n]   = true;

        // §42 - a Mono LFO is one LFO, so it moves once a sample however many voices read it
        // §50 - at the rate a voice last read from its inputs, the dial's own before any has
        double              monoRate = (gLfoMonoRate[n] >= 0.0) ? gLfoMonoRate[n] : spec->rateHz;

        if ((spec->kind == eNodeLfo) && (spec->lfoMono == true)) {
            (void)advance_phase(&gLfoMonoPhase[n], monoRate / gSampleRate);
        }

        if ((spec->kind == eNodeRandomA) && (spec->lfoMono == true)) {
            tRandomAShared * shared = &gRndMono[n];

            shared->out = random_a_step(shared->state, &shared->seed, &shared->phase, spec, monoRate);
        }
    }
}

static void stage_voices(const tSoundEngineParams * p, const tStageCtx * ctx, double value[][NODE_OUTPUTS], tSmoothWhich smooth) {
    SE_LOCAL;

    uint32_t n       = 0;
    double   voiceSum[MAX_ENGINE_NODES][NODE_OUTPUTS];

    // One event per sample. A chord's worth of note-ons arriving together therefore lands over
    // consecutive samples rather than all but the last being thrown away, and every note takes
    // effect where it actually arrived instead of at the next buffer boundary.
    start_pending_steals(p);   // §15.3a - before the queue: a voice about to trig is busy
    (void)take_next_note_event(p);

    // notes §176
    double   vibrato = 0.0;

    if (p->vibratoSource != eVibratoOff) {
        uint32_t group = (p->vibratoSource == eVibratoWheel)
                     ? MORPH_GROUP_WHEEL : MORPH_GROUP_AFTERTOUCH;
        double   depth = (double)atomic_load(&gMorphMilli[group]) / 1000.0;

        gVibratoPhase += p->vibratoHz / gSampleRate;

        if (gVibratoPhase >= 1.0) {
            gVibratoPhase -= 1.0;
        }
        vibrato        = (sin(gVibratoPhase * 2.0 * M_PI) * depth * p->vibratoCents) / 100.0;
    }
    double   bend    = ((double)atomic_load(&gBendMilli) / 1000.0) * p->bendSemitones;

    stage_smooth(p, ctx->rampSamples, smooth);

    memset(voiceSum, 0, (size_t)p->nodeCount * sizeof(voiceSum[0]));

    // notes §178
    for (uint32_t v = 0; v < p->voiceCount; v++) {
        tVoice * voice     = &gVoice[v];

        // notes §179
        bool     freeVoice = free_voice_runs(v, ctx->chainHasEnvelope, ctx->droneMode);
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
        if ((freeVoice == true) && (voice->gate == false) && (ctx->chainHasEnvelope == false)) {
            voice->sounding = false;
            freeRun         = true;
        }

        // notes §181
        if (voice->note >= 0) {
            bool   sliding = (p->glideMode == eGlideNormal)
                             || ((p->glideMode == eGlideAuto) && (voice->glideActive == true));

            double gap     = (double)voice->note - voice->glidePitch;

            if ((sliding == true) && (ctx->glideStep > 0.0) && (fabs(gap) > ctx->glideStep)) {
                voice->glidePitch += (gap > 0.0) ? ctx->glideStep : -ctx->glideStep;
            } else {
                voice->glidePitch = (double)voice->note;
            }
        }
        double voicePitch = voice->glidePitch + bend + vibrato + p->octaveSemis;

        // The anti-click ramp, per voice. Only used when the patch has no EnvADSR to shape
        // the note itself — with one, this would just double up on it.
        double rampTarget = ((voice->gate == true) || (freeRun == true)) ? 1.0 : 0.0;

        if (voice->envelope < rampTarget) {
            voice->envelope += ctx->envelopeStep;

            if (voice->envelope > rampTarget) {
                voice->envelope = rampTarget;
            }
        } else if (voice->envelope > rampTarget) {
            voice->envelope -= ctx->envelopeStep;

            if (voice->envelope < rampTarget) {
                voice->envelope = rampTarget;
            }
        }

        // Past the limit (counted from its envelopes finishing, below), wind the voice down
        // rather than cutting it. voice->fade reaching zero is what retires it.
        if (  (voice->gate == false)
           && (freeRun == false)
           && (ctx->droneMode == false)
           && (voice->released > (uint32_t)(VOICE_MAX_TAIL_SECONDS * gSampleRate))) {
            voice->fade -= 1.0 / (VOICE_FADE_SECONDS * gSampleRate);

            if (voice->fade < 0.0) {
                voice->fade = 0.0;
            }
        }
        double level   = ((ctx->chainHasEnvelope == true) ? 1.0 : voice->envelope) * voice->fade;

        for (n = 0; n < p->nodeCount; n++) {
            if (p->node[n].postMix == true) {
                continue;
            }
            eval_node(v, n, p, value, voicePitch);
        }

        // The voices SUM, which is what playing more than one note at once means. Only the
        // per-voice nodes are summed here — everything inside the voice was read from
        // value[] during its own pass, before the next voice overwrites it.
        double leaving = 0.0;

        for (n = 0; n < p->nodeCount; n++) {
            if (p->node[n].postMix == true) {
                continue;
            }

            for (uint32_t leg = 0; leg < NODE_OUTPUTS; leg++) {
                voiceSum[n][leg] += value[n][leg] * level;
            }

            // What this voice is putting out, measured at its Out modules — the point where
            // it leaves the voice for the mix or for the FX Area.
            if (p->node[n].kind == eNodeOut) {
                double magnitude = fabs(value[n][0] * level);

                if (magnitude > leaving) {
                    leaving = magnitude;
                }
            }
        }

        voice->quiet    = (leaving < VOICE_SILENCE) ? (voice->quiet + 1) : 0;

        bool finished = (freeRun == false) && (voice_is_finished(p, v, ctx->chainHasEnvelope) == true);

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
    for (n = 0; n < p->nodeCount; n++) {
        if (p->node[n].postMix == false) {
            for (uint32_t leg = 0; leg < NODE_OUTPUTS; leg++) {
                value[n][leg] = voiceSum[n][leg];
            }

            meter_node(&p->node[n], n, value[n][0], value[n][1]);
        }
    }
}

static void stage_fx(const tSoundEngineParams * p, double value[][NODE_OUTPUTS]) {
    SE_LOCAL;

    uint32_t n            = 0;
    double   sample[2][2] = {{0.0, 0.0}, {0.0, 0.0}};   // [output pair][channel]

    // notes §183
    for (n = 0; n < p->nodeCount; n++) {
        if (p->node[n].postMix == false) {
            continue;
        }
        eval_node(0, n, p, value, KEYBOARD_PITCH_ZERO);    // §16.2a - no key after the mix: E4, 0 units
    }

    if (p->tap >= 0) {
        // Tapping a module means listening to its main output; for an envelope used as an amp
        // that is its shaped audio rather than the envelope signal. See tap_pair().
        {
            double   first[2] = {0.0, 0.0};
            uint32_t d        = p->node[p->tap].outDest & 1U;

            tap_pair(p, p->tap, value, first);
            sample[d][0] += first[0];
            sample[d][1] += first[1];
        }

        // notes §184
        for (uint32_t t = 0; t < p->extraTapCount; t++) {
            double   extra[2] = {0.0, 0.0};
            uint32_t d        = p->node[p->extraTap[t]].outDest & 1U;

            tap_pair(p, p->extraTap[t], value, extra);
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

    // §63 - the patch Volume, glided over ~10 ms so a turn does not step; it starts where it is set
    gSlotGainNow = (gSlotGainNow < 0.0) ? p->slotGain
                   : (gSlotGainNow + ((p->slotGain - gSlotGainNow) * (1.0 - exp(-1.0 / (0.01 * gSampleRate)))));

    // The gain, the knee and the clamp are all PER CHANNEL. The knee especially: shaping the
    // two channels together off a common peak would make one duck when the other got loud,
    // which is a stereo image moving under a limiter rather than an output stage.
    for (uint32_t q = 0; q < 4; q++) {
        double * sp = &sample[q >> 1][q & 1];

        // notes §198 - the outputs are AC-coupled
        {
            double y = *sp - gOutCoupling[q][0] + (exp(-1.0 / (OUTPUT_COUPLING_TAU * gSampleRate)) * gOutCoupling[q][1]);

            gOutCoupling[q][0] = *sp;
            gOutCoupling[q][1] = y;
            *sp                = y;
        }

        // notes §199 - and roll off at the top, as its converter and output stage do
        if (atomic_load(&gDacEmulation) == true) {
            if (gDacCoefRate != gSampleRate) {
                dac_filter_design(gSampleRate);
            }
            double y = (gDacCoef[0] * *sp) + (gDacCoef[1] * gDacState[q])
                       - (gDacCoef[2] * gDacStateOut[q][0]) - (gDacCoef[3] * gDacStateOut[q][1]);

            gDacState[q]       = *sp;
            gDacStateOut[q][1] = gDacStateOut[q][0];
            gDacStateOut[q][0] = y;
            *sp                = y;
        }
        *sp                           *= VOICE_GAIN * gSlotGainNow;
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

// notes §202 - the two platform pieces the split needs: a semaphore the audio thread can signal without
// blocking, and a worker scheduled like the audio thread it works for.
#if defined (__APPLE__)
typedef dispatch_semaphore_t tSplitSem;

static tSplitSem split_sem_create(void) {
    return dispatch_semaphore_create(0);
}

static void split_sem_signal(tSplitSem sem) {
    (void)dispatch_semaphore_signal(sem);
}

static void split_sem_wait(tSplitSem sem) {
    (void)dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
}

static void split_thread_make_realtime(void) {
    mach_timebase_info_data_t            base;
    thread_time_constraint_policy_data_t policy;

    (void)mach_timebase_info(&base);
    double                               perMs = 1.0e6 * (double)base.denom / (double)base.numer;

    policy.period      = (uint32_t)(5.0 * perMs);
    policy.computation = (uint32_t)(1.0 * perMs);
    policy.constraint  = (uint32_t)(5.0 * perMs);
    policy.preemptible = 1;
    (void)thread_policy_set(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY,
                            (thread_policy_t)&policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
}
#else
typedef sem_t * tSplitSem;

static tSplitSem split_sem_create(void) {
    tSplitSem sem = (tSplitSem)calloc(1, sizeof(sem_t));

    if (sem != NULL) {
        (void)sem_init(sem, 0, 0);
    }
    return sem;
}

static void split_sem_signal(tSplitSem sem) {
    (void)sem_post(sem);
}

static void split_sem_wait(tSplitSem sem) {
    while (sem_wait(sem) != 0) {
    }
}

static void split_thread_make_realtime(void) {
}
#endif

// notes §202 - the voices on their own thread, the FX pass one SPLIT_LAG behind on the audio thread. The
// two meet only at what the FX pass and the output taps read of the voice sum: those values go across
// in a ring of per-sample records, and nothing else is shared that both passes write.
#define SPLIT_LAG          (32u)    // graph samples the FX pass trails the voices
#define SPLIT_RING         (128u)   // records, comfortably more than the lag
#define SPLIT_MAX_PAIRS    (32u)    // voice-sum values one record carries

typedef struct {
    double v[SPLIT_MAX_PAIRS];
} tSplitRecord;

typedef struct {
    bool                       started;
    tSplitSem                  go;
    _Atomic bool               busy;
    _Atomic uint64_t           produced;
    _Atomic uint64_t           consumed;
    // the job, written before `go` is signalled and left alone until `busy` clears
    tG2Document *              doc;
    const tSoundEngineParams * params;
    tStageCtx                  ctx;
    uint64_t                   count;
    // what the ring carries, and the graph it was primed for
    uint32_t                   pairCount;
    uint16_t                   pairNode[SPLIT_MAX_PAIRS];
    uint8_t                    pairLeg[SPLIT_MAX_PAIRS];
    uint64_t                   primedTopology;
    uint32_t                   primedMode;
    bool                       primed;     // the last block ran split, on this ring
    tSplitRecord               ring[SPLIT_RING];
} tSplit;

static tSplit           gSplitBank[SOUND_ENGINE_MAX_ENGINES];
#define gSplit    (gSplitBank[SE])

static _Atomic uint32_t gSplitMode = eSplitThreaded;

static void split_pause(void) {
#if defined (__aarch64__)
    __asm__ __volatile__ ("yield");
#elif defined (__x86_64__)
    __builtin_ia32_pause();
#endif
}

// notes §202 - a kind after the mix that reads the keyboard or a voice would see them a lag early
static bool split_kind_reads_voices(const tEngineNode * spec) {
    switch (spec->kind) {
        case eNodeKeyboard:
        case eNodeMonoKey:
        case eNodeNoteDet:
        case eNodeDx:
        {
            return true;
        }
        case eNodeEnv:
        {
            return spec->envKeyGate;
        }
        default:
        {
            return false;
        }
    }
}

static bool split_add_pair(tSplit * sp, int32_t node, uint32_t leg) {
    for (uint32_t k = 0; k < sp->pairCount; k++) {
        if ((sp->pairNode[k] == (uint16_t)node) && (sp->pairLeg[k] == (uint8_t)leg)) {
            return true;
        }
    }

    if (sp->pairCount >= SPLIT_MAX_PAIRS) {
        return false;
    }
    sp->pairNode[sp->pairCount] = (uint16_t)node;
    sp->pairLeg[sp->pairCount]  = (uint8_t)leg;
    sp->pairCount++;
    return true;
}

// notes §202 - whether this graph can be split, and the values that have to cross
static bool split_plan(const tSoundEngineParams * p, tSplit * sp) {
    bool anyAfterMix = false;

    sp->pairCount = 0;

    for (uint32_t n = 0; n < p->nodeCount; n++) {
        const tEngineNode * spec = &p->node[n];

        for (uint32_t c = 0; c < spec->inCount; c++) {
            int32_t src     = spec->in[c];

            if ((src < 0) || ((uint32_t)src >= p->nodeCount)) {
                continue;
            }
            bool    crosses = (p->node[src].postMix != spec->postMix);

            if ((crosses == true) && ((spec->postMix == false) || ((spec->backMask & (1u << c)) != 0u))) {
                return false;   // the voices reading after the mix, or a loop across the two
            }

            if ((crosses == true) && (split_add_pair(sp, src, spec->srcLeg[c]) == false)) {
                return false;
            }
        }

        if (spec->postMix == true) {
            anyAfterMix = true;

            if (split_kind_reads_voices(spec) == true) {
                return false;
            }
        }
    }

    for (int32_t t = -1; t < (int32_t)p->extraTapCount; t++) {
        int32_t tap = (t < 0) ? p->tap : p->extraTap[t];

        if (  (tap >= 0) && (p->node[tap].postMix == false)
           && ((split_add_pair(sp, tap, 0u) == false) || (split_add_pair(sp, tap, 1u) == false))) {
            return false;
        }
    }

    return anyAfterMix;
}

static void split_produce_one(tSplit * sp, const tSoundEngineParams * p, const tStageCtx * ctx) {
    SE_LOCAL;

    double         value[MAX_ENGINE_NODES][NODE_OUTPUTS];
    uint64_t       at  = atomic_load_explicit(&sp->produced, memory_order_relaxed);

    while ((at - atomic_load_explicit(&sp->consumed, memory_order_acquire)) >= SPLIT_RING) {
        split_pause();
    }
    stage_voices(p, ctx, value, eSmoothVoice);

    tSplitRecord * rec = &sp->ring[at % SPLIT_RING];

    for (uint32_t k = 0; k < sp->pairCount; k++) {
        rec->v[k] = value[sp->pairNode[k]][sp->pairLeg[k]];
    }

    atomic_store_explicit(&sp->produced, at + 1u, memory_order_release);
}

static void split_consume_one(tSplit * sp, const tSoundEngineParams * p, double rampSamples) {
    SE_LOCAL;

    double               value[MAX_ENGINE_NODES][NODE_OUTPUTS];
    uint64_t             at  = atomic_load_explicit(&sp->consumed, memory_order_relaxed);

    while (atomic_load_explicit(&sp->produced, memory_order_acquire) <= at) {
        split_pause();
    }
    const tSplitRecord * rec = &sp->ring[at % SPLIT_RING];

    for (uint32_t k = 0; k < sp->pairCount; k++) {
        value[sp->pairNode[k]][sp->pairLeg[k]] = rec->v[k];
    }

    atomic_store_explicit(&sp->consumed, at + 1u, memory_order_release);
    stage_smooth(p, rampSamples, eSmoothFx);
    stage_fx(p, value);
}

// A fresh ring holds SPLIT_LAG silent records, which is the lag.
static void split_prime(tSplit * sp, uint64_t topology, uint32_t mode) {
    sp->primed         = true;
    memset(sp->ring, 0, sizeof(sp->ring));
    atomic_store(&sp->consumed, 0u);
    atomic_store(&sp->produced, (uint64_t)SPLIT_LAG);
    sp->primedTopology = topology;
    sp->primedMode     = mode;
}

static void * split_worker(void * arg) {
    tSplit * sp = (tSplit *)arg;

    split_thread_make_realtime();

    for ( ; ;) {
        split_sem_wait(sp->go);
        gDoc = sp->doc;     // the engine's document, and with it the engine's banks

        for (uint64_t i = 0; i < sp->count; i++) {
            split_produce_one(sp, sp->params, &sp->ctx);
        }

        atomic_store_explicit(&sp->busy, false, memory_order_release);
    }

    return NULL;
}

// Started once per engine, outside the audio callback, and left waiting between blocks.
static void split_worker_ensure(void) {
    SE_LOCAL;

    tSplit *  sp = &gSplit;
    pthread_t thread;

    if (sp->started == true) {
        return;
    }
    {
        const char * forced = getenv("G2_ENGINE_SPLIT");     // 0 serial, 1 inline, 2 threaded

        if ((forced != NULL) && (forced[0] >= '0') && (forced[0] <= '2')) {
            sound_engine_set_split_mode((uint32_t)(forced[0] - '0'));
        }
    }
    sp->go = split_sem_create();

    if (pthread_create(&thread, NULL, split_worker, sp) == 0) {
        (void)pthread_detach(thread);
        sp->started = true;
    }
}

void sound_engine_set_split_mode(uint32_t mode) {
    atomic_store(&gSplitMode, (mode <= eSplitThreaded) ? mode : eSplitThreaded);
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
    double    envelopeStep = 1.0 / (ENVELOPE_SECONDS * gSampleRate);
    double    rampSamples  = PARAM_RAMP_SAMPLES * gSampleRate / G2_ENGINE_SAMPLE_RATE; // the glide's length in samples
    double    glideStep    = (params.glideSeconds > 0.0)
                          ? (12.0 / (params.glideSeconds * gSampleRate)) : 0.0;
    tStageCtx ctx          = {chainHasEnvelope, droneMode, envelopeStep, rampSamples, glideStep};

    // notes §202 - split this block if the graph allows; a new graph or mode starts a fresh ring
    uint32_t  splitMode    = atomic_load(&gSplitMode);
    tSplit *  sp           = &gSplit;
    bool      split        = false;

    if (splitMode != eSplitSerial) {
        uint32_t oldCount  = sp->pairCount;
        uint16_t oldNode[SPLIT_MAX_PAIRS];
        uint8_t  oldLeg[SPLIT_MAX_PAIRS];

        memcpy(oldNode, sp->pairNode, sizeof(oldNode));
        memcpy(oldLeg, sp->pairLeg, sizeof(oldLeg));
        split = (split_plan(&params, sp) == true) && ((splitMode == eSplitInline) || (sp->started == true));

        bool     samePairs = (oldCount == sp->pairCount)
                             && (memcmp(oldNode, sp->pairNode, sizeof(oldNode)) == 0) && (memcmp(oldLeg, sp->pairLeg, sizeof(oldLeg)) == 0);

        if (  (split == true)
           && ((sp->primed == false) || (samePairs == false) || (sp->primedTopology != params.topology) || (sp->primedMode != splitMode))) {
            split_prime(sp, params.topology, splitMode);
        }
    }
    sp->primed = split;

    if ((split == true) && (splitMode == eSplitThreaded)) {
        sp->doc    = gDoc;
        sp->params = &params;
        sp->ctx    = ctx;
        sp->count  = (uint64_t)frameCount * gOversample;
        atomic_store_explicit(&sp->busy, true, memory_order_release);
        split_sem_signal(sp->go);
    }

    for (frame = 0; frame < frameCount; frame++) {
        uint32_t sub = 0;

        // gOversample passes of the whole graph per output sample (§29a - one, where the device is
        // already at the instrument's own rate). Note events are consumed inside, so they land on
        // the finer grid too rather than being quantised to the output rate.
        for (sub = 0; sub < gOversample; sub++) {
            double value[MAX_ENGINE_NODES][NODE_OUTPUTS];

            if (split == false) {
                stage_voices(&params, &ctx, value, eSmoothAll);
                stage_fx(&params, value);
            } else {
                if (splitMode == eSplitInline) {
                    split_produce_one(sp, &params, &ctx);
                }
                split_consume_one(sp, &params, rampSamples);
            }
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

    // notes §202 - the worker finishes its block before this call returns, so it never runs between them
    if ((split == true) && (splitMode == eSplitThreaded)) {
        while (atomic_load_explicit(&sp->busy, memory_order_acquire) == true) {
            split_pause();
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
    memset(&gOutCoupling, 0, sizeof(gOutCoupling));
    memset(&gDacState, 0, sizeof(gDacState));
    memset(&gDacStateOut, 0, sizeof(gDacStateOut));
    gDacCoefRate = 0.0;
    memset(&gOscDecimate, 0, sizeof(gOscDecimate));
    memset(&gOscHistory, 0, sizeof(gOscHistory));
    memset(&gOscHistoryPos, 0, sizeof(gOscHistoryPos));
    memset(&gPhase, 0, sizeof(gPhase));
    memset(&gLfoLastPhase, 0, sizeof(gLfoLastPhase));
    memset(&gLfoHeld, 0, sizeof(gLfoHeld));
    memset(&gLfoSlope, 0, sizeof(gLfoSlope));
    memset(&gLfoSeed, 0, sizeof(gLfoSeed));
    memset(&gLfoStep, 0, sizeof(gLfoStep));
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
    gDacEmulation     = true;
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
