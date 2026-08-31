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

// Where each filter module keeps its parameters. They are NOT all laid out like FltClassic: FltLP
// has no resonance at all, and its Slope sits one index earlier because of it. Reading a missing
// parameter would take whatever the next one happens to be — for FltLP that would be Slope read as
// resonance, i.e. a filter that self-oscillates because a menu index landed in a gain.
//
// -1 for a parameter the module does not have.
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

// G2_FILTER_LEGACY=1 restores the filter behaviour as it stood before 2026-08-30 — FltLP reading
// its Slope from the Bypass parameter, and a maximum of four poles. It exists so the corrected
// filters can be A/B'd by ear against the old ones without a rebuild, and so a regression can be
// backed out in one environment variable rather than a revert.
// THE VOICE AREA RUNS CONTINUOUSLY ON THE INSTRUMENT, and this makes the engine do the same.
// Set G2_ENGINE_NO_FREERUN=1 to get the old note-gated behaviour back without a rebuild, the same
// way G2_FILTER_LEGACY works.
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
    // DESIGNATED INITIALISERS DELIBERATELY. These were positional, and adding slopeMode in the
    // middle of the struct silently shifted every one of them - slopeMode took what active meant
    // and active became 0, which switched the filter off rather than failing to build. Naming the
    // fields makes the next insertion harmless.
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
            // Freq, FreqMod, Kbt, Bypass - no Res, and SLOPE IS A MODE, not a parameter.
            //
            // This read .slope = 3 and .active = 4 until 2026-08-30. FltLP has FOUR parameters, so
            // that took Slope from param 3 - which is BYPASS - and the active flag from a param that
            // does not exist. The slope therefore followed the bypass switch, and only two of the
            // six slopes were ever reachable. modeLocationList had already moved Slope to a mode on
            // 2026-08-15; this map was not moved with it.
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

#define FLT_PARAM_FREQ       (0)
#define FLT_PARAM_ENV        (1)       // modulation depth for the Env input, 0..200%
#define FLT_PARAM_KBT        (2)
#define FLT_PARAM_RES        (3)
#define FLT_PARAM_SLOPE      (4)
#define FLT_PARAM_ACTIVE     (5)

#define ENV_PARAM_SHAPE      (0)
#define ENV_PARAM_ATTACK     (1)
#define ENV_PARAM_DECAY      (2)
#define ENV_PARAM_SUSTAIN    (3)
#define ENV_PARAM_RELEASE    (4)

#define LEVAMP_PARAM_GAIN    (0)
#define LEVAMP_PARAM_TYPE    (1)       // 0 = lin, 1 = exp

// "Out to" — where the module sends. NOT every Out module reaches the speakers: the other settings
// are internal routing, and a patch commonly uses one to feed its FX area.
//   2toOut, outToStrMap:     Out 1/2, Out 3/4, FX 1/2, FX 3/4, Bus 1/2, Bus 3/4  -> 0,1 audible
//   4toOut, outTo4OutStrMap: Out, Fx, Bus                                        -> 0   audible
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

// The waveform selector is a MODE, not a parameter — the G2 keeps drop-down selectors out of the
// parameter list entirely because, unlike a knob, they cannot be assigned to a morph group or a
// controller and hold one setting across every variation (manual p.20). It lives in
// modeLocationList, so it is read from module->mode[] and reading param[10] found nothing.
#define SHPB_MODE_WAVEFORM    (0)

// Pulse: a one-shot gate fired by a RISING EDGE at its input. Time and Range set how long it stays
// high; there is no power button, so it is always live.
#define PULSE_PARAM_TIME       (0)
#define PULSE_PARAM_TIMEMOD    (1)
#define PULSE_PARAM_RANGE      (2)

// What counts as a logic high. The G2's logic signals are full-scale 0/1, so anything near the
// middle separates them; the envelope that drives this in the measurement patch sweeps the whole
// range, so the exact threshold is not delicate.
#define PULSE_THRESHOLD             (0.5)

// Mix4to1C: one level per input, then a pad and a curve.
#define MIX_PARAM_LEVEL_BASE        (0)
// The four Channel Mute buttons sit directly after the four level dials. offOnColourMap indexes
// them {grey, green}, so NON-ZERO IS ENABLED — a lit button is a channel that sounds.
#define MIX_PARAM_ENABLE_BASE       (4)
#define MIX_CHANNELS                (4)
// The two mixers put Curve in different places: Mix4to1C has a Pad at 8 and Curve at 9, Mix4to1S
// has no Pad and Curve at 8.
#define MIX_PARAM_PAD               (8) // Mix4to1C only: 0 dB or -6 dB on every input at once
#define MIX_PARAM_CURVE             (9)
#define MIXS_PARAM_CURVE            (8)
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

// DelayB. Its range is a MODE, like the shape oscillators' waveform.
// The two delays share their first four parameters but NOT their Bypass: DelayA has six parameters
// with Bypass at 4, DelayB has nine with Bypass at 7 (and an extra HP at 8). Reading DelayB's index
// on a DelayA lands past the end of its parameter list, reads zero, and silently bypasses it.
#define DELAY_PARAM_TIME        (0)
#define DELAY_PARAM_FEEDBACK    (1)
#define DELAY_PARAM_LP          (2)   // DelayA calls this Filter; both are a damping control
#define DELAY_PARAM_DRYWET      (3)
#define DELAY_PARAM_HP          (8)

// The LP dial's cutoff, swept exponentially across its travel — see where node->damping is set.
//
// FITTED TO A BURST MEASUREMENT, which is how to measure anything inside a feedback loop: a short
// burst of saw leaves the repeats separated in time, so each can be transformed on its own and
// repeat[n+1]/repeat[n] IS the per-pass response, feedback and filter together. Normalising that to
// its own flattest point leaves the filter alone.
//
//     LP 127   flat within 0.2 dB from 0.5 to 15 kHz  -> wide open, cutoff at or above 20 kHz
//     LP  64   -1.9 dB at 2.8 kHz, -3.2 at 3.7, -4.2 at 5.6  -> one-pole knee near 3.5 kHz
//     LP   0   only two repeats survive at all        -> cutoff well down, most energy removed
//
// 660 Hz at the bottom puts fc(64) at 3.7 kHz, which is that knee. A CONTINUOUS tone cannot measure
// this: the repeats overlap, and at high feedback the loop regenerates and the ratios stop meaning
// anything — measured per-pass "gains" came out above unity at FB 100.
#define DELAY_LP_MIN_HZ    (660.0)

// The HP dial's cutoff, as a QUADRATIC IN THE DIAL VALUE — log fc = a + b*hp + c*hp^2, not the
// exponential the LP uses. That is not a preference, it is what the instrument does: an exponential
// fitted to the two ends misses the middle of this dial by a factor of 2.4.
//
// MEASURED with the burst method — short saw burst, repeats separated in time, repeat[n+1]/repeat[n]
// per harmonic, each row referenced to the 6-12 kHz band which sits above every cutoff here:
//
//     HP    32     64     96    127
//     -3dB  92    841   2342   4561 Hz        fit error  +0.4  -1.1  +1.1  -0.4 dB
//
// VALIDATED at three settings that were NOT used to fit it:
//
//     HP    48     80    112
//     meas 313   1208   3696 Hz               error      -0.6  +2.1  +0.1 dB
//
// HP 0 is the filter switched out rather than its lowest cutoff — it measures flat within 0.9 dB.
//
// The reference band matters more than it looks: taking it at 1.5-4.5 kHz, as a first attempt did,
// puts the reference INSIDE the transition band at high settings, which flattens the measured curve
// and hides the filter completely. HP 127 looked unmeasurable until the reference moved above it.
#define DELAY_HP_LOG_A         (1.74224)
#define DELAY_HP_LOG_B         (0.100227)
#define DELAY_HP_LOG_C         (-0.000377517)
#define DELAY_LP_MAX_HZ        (20000.0)
#define DELAYA_PARAM_ACTIVE    (4)
#define DELAYB_PARAM_ACTIVE    (7)
#define DELAY_MODE_RANGE       (0)

#define REVERB_PARAM_TIME      (0)
#define REVERB_PARAM_BRIGHT    (1)
#define REVERB_PARAM_DRYWET    (2)
#define REVERB_PARAM_ACTIVE    (3)

// The four LFO variants share a design but not a parameter order, so each one carries its own index
// set. A -1 means the variant does not have that control at all: LfoC has no waveform selector and
// no Kbt, and only LfoShpA has a Shape dial.
//
// None of them has an output LEVEL. G2 LFOs emit at full scale and the DEPTH is set at the
// destination — the oscillator's own Pitch modulation knob — which is exactly how a vibrato patch is
// wired, and why no level parameter is read here.
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

// A node takes at most this many inputs. Eight, because the STEREO 4-into-1 mixer has four channels
// of two legs each and both legs have to be read: patches routinely put two different modules on the
// left and right of one channel — this patch feeds its two delays to In2L and In2R — so taking only
// the left leg silently dropped whichever module sat on the right.
#define MAX_NODE_INPUTS    (8)

// Which connector carries the signal into each module. Everything the walk follows is a module's
// FIRST input; LevMult and 2toOut take a second as well.
#define CONNECTOR_IN_A    (0)
#define CONNECTOR_IN_B    (1)
// FltClassic's control input, the one beside its Env knob.
//
// A RAW CONNECTOR INDEX, like every other entry in the connector maps below — cable_chain_node_from_
// connector() indexes module->connector[] directly and derives the ioCount itself. FltClassic's
// connectors run In(audio), Out(audio), In(control), In(control), so 2 is the first control input.
// Briefly changed to 1 on the mistaken belief that these were input-direction indices; 1 is the
// module's OUTPUT connector, which broke the filter outright.
#define FLT_CONNECTOR_ENV_IN    (2)

// The G2 caps the total pitch modulation reaching an oscillator or filter at +/-64 semitones
// (manual p.78), which is what an Env amount of 100% corresponds to.
#define FULL_MOD_SEMITONES    (64.0)

// A full-scale bipolar signal on an oscillator's Pitch input sweeps one octave either way: the
// manual's own worked example (p.78) is an A4 modulated "up and down by one octave", and it stays an
// octave whatever note is played, because a Pitch input modulates on the note scale rather than
// linearly in frequency. That is a much smaller range than the filter's Env input above.
#define PITCH_MOD_SEMITONES    (12.0)

// The oscillators' Pitch mod-amount knob is an ATTENUATOR TYPE II — exponential, not linear. The
// manual names the family ("the pitch mod-input on the various oscillators ... are examples of Type
// II attenuation", p.79) and says what it means: "a setting of 50 attenuates the incoming signal by
// a factor considerably less than 0.5". Reading the knob linearly, as this did, leaves roughly twice
// the modulation at a half-open knob, which on a vibrato patch is the difference between a detune
// and a siren.
//
// SQUARED is the same approximation the mixer's Exp/dB curve already uses (see eNodeMix, which the
// manual confirms is the same Type II scale). Exact at both ends — 0 "shuts off the modulation
// completely", 127 "leaves the incoming signal unaffected" — and convex between them, which is the
// shape described. The exact law is not stated numerically anywhere in the manual, and the knob has
// no value display to read it off, so this is closer rather than right.
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

// The G2's Tune value is the MIDI note number of the oscillator's base pitch: value 0 is 8.1758 Hz
// (note 0) and value 127 is 12.55 kHz (note 127), which is why the "Semi" display shows the value
// minus 64 and the "Freq" display shows the same dial in Hz. So a single pitch calculation covers
// both display modes, and 64 is the value at which the oscillator plays the note as struck.
#define OSCB_TUNE_UNITY       (64.0)
#define MIDI_NOTE_A440        (69.0)
#define MIDI_NOTE_MIDDLE_C    (60.0)

// Output trim. A busy patch — several oscillators into a mixer, a resonant filter, then a second
// mixer summing dry against delays and reverb — genuinely reaches five or six times a single
// oscillator's level, and that is the G2's own arrangement rather than anything wrong. So the trim
// has to leave room for it: 0.15 puts a hot patch just under full scale instead of 3 dB into the
// clipper, at the cost of a single-oscillator sketch being quieter.
#define VOICE_GAIN           (0.15)

// Where the output starts bending rather than shearing.
#define OUTPUT_KNEE          (0.80)
#define ENVELOPE_SECONDS     (0.005)       // the anti-click ramp used when no EnvADSR is in the chain

// Every ladder runs its full four poles whatever slope is selected — see ladder_filter().
#define LADDER_POLES         (6)    // state available: FltLP's 36 dB setting is six poles
#define LADDER_LOOP_POLES    (4)    // the RESONANCE loop is four long whatever is tapped - measured

// The chain the engine renders. Small and fixed: these are hand-built sketches, not whole patches,
// and a bound is what keeps the walk safe against a patch that feeds back into itself.
// Raised from 12 once whole patches came into scope: a real one runs to a couple of dozen modules
// across the Voice and FX areas.
#define MAX_ENGINE_NODES    (28)

// How many voices the engine can hold at once. The patch's own figure is what actually governs it —
// see voice_count_for_patch() — and this is only the ceiling that sizes the state arrays. 32 is the
// most the patch descriptor can ask for: voiceCount is a 5-bit field holding the count MINUS ONE.
//
// COST IS PER SOUNDING VOICE, NOT PER ALLOCATED VOICE. Only voices actually producing sound are
// rendered (see voice_is_finished()), so a 32-voice patch played one note at a time costs what the
// monophonic engine cost. Holding 32 notes really does cost 32 times as much, which no amount of
// arranging avoids — it is 32 copies of the Voice Area.
#define MAX_VOICES    (32)

// What counts as an inaudible voice, and how long it has to stay that way before the voice can be
// handed to another note. -80 dB is below anything that survives the output stage; the window is
// long enough that a waveform passing through zero cannot be mistaken for silence.
#define VOICE_SILENCE            (1.0e-4)
#define VOICE_SILENCE_SECONDS    (0.02)

// How long a voice may go on sounding after its key is up and its envelope has finished, before it
// is faded out and taken back. A patch whose EnvADSR modulates only the filter never stops on its
// own — correct, and what the hardware does, but on a soft synth it means every voice the patch owns
// stays in the render for ever, and the cost of that is permanent rather than while you are playing.
// The fade is what makes taking it back inaudible; without one this would be a click.
#define VOICE_MAX_TAIL_SECONDS    (2.0)
#define VOICE_FADE_SECONDS        (0.03)

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
    eNodeOut,
} tNodeKind;

typedef struct {
    tNodeKind kind;
    uint32_t  moduleIndex;   // so per-node audio state can survive a knob turn (see topology_signature)
    uint32_t  location;      // Voice or FX — the two areas number their modules independently

    // What feeds each input: the node index, and WHICH of that node's outputs the cable came from.
    // The output matters because a module can offer more than one — an EnvADSR's first output is the
    // envelope itself and its second is the audio it has shaped, and a cable to one means something
    // quite different from a cable to the other.
    int32_t  in[MAX_NODE_INPUTS];
    uint32_t srcOut[MAX_NODE_INPUTS];
    uint32_t inCount;
    bool     active;                 // the module's own power button

    double   level[MAX_NODE_INPUTS]; // mixer channel levels

    tOscWave wave;                   // oscillator
    bool     oscKbt;
    double   basePitch;
    double   shape;
    double   rateHz;         // LFO speed
    uint32_t polarity;       // LFO output range, posStrMap order
    bool     shpWave;        // LFO uses LfoShpA's waveform set rather than the plain one

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
    double          attackCoeff;
    double          releaseCoeff;
    double          constant;   // Constant module's value
    uint32_t        line;       // which shared delay line this node owns, if it needs one
    double          brightness; // reverb, 0..1 as the dial reads it — HIGH IS BRIGHT
    double          timeNorm;   // reverb Time as the dial reads it, 0..1 — drives the diffusion
    uint32_t        reverbType; // reverb room size: Small/Medium/Large/Hall

    // Evaluated ONCE per sample, after the voices are summed, rather than once per voice. True for
    // everything in the FX Area, for the three module kinds that own a shared delay buffer wherever
    // they sit, and for anything downstream of one of those. See mark_post_mix_nodes().
    bool postMix;
} tEngineNode;

// A patch can hold more than one Out module — SimpleLead has two, a Voice Area output carrying the
// dry voice and an FX Area output carrying the delays and reverb — and on the hardware they SUM at
// the sockets. Tapping only the first one silently drops the other, which on that patch means
// hearing the effects with no dry signal underneath them.
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

// Published by the UI thread, consumed by the audio thread, via a seqlock: the writer makes the
// sequence odd before touching the snapshot and even again after, so a reader that sees an odd
// sequence — or a different one either side of its copy — knows it read during a write. Neither
// side ever blocks, and the audio thread never waits on the UI thread. A plain pair of buffers
// would not do: the UI can publish twice while one audio buffer is being filled, which is long
// enough to land back on the buffer the audio thread is mid-copy of.
static tSoundEngineParams gParams           = {0};
static _Atomic uint32_t   gParamsSeq        = 0;

// SERIALISES WRITERS ONLY. The audio thread never takes this — it is the seqlock's reader and stays
// lock-free, so there is no priority inversion to worry about.
//
// A seqlock tolerates exactly one writer, and for a long time there was one: the render thread,
// rebuilding the snapshot every frame. That is what forced a morph to go the long way round —
// sound_engine_set_morph() records the position, but only a rebuild folds it into what the audio
// thread reads, so the MIDI thread had to ask for a REDRAW and wait for it. Mod wheel response was
// therefore capped at the frame rate, with a full canvas repaint sitting between the wheel and the
// sound.
//
// With writers serialised here, any thread may rebuild. The MIDI thread now does so immediately on a
// morph change (midiInput.c) instead of waiting to be drawn.
static pthread_mutex_t    gParamsWriteMutex = PTHREAD_MUTEX_INITIALIZER;

#define PARAMS_READ_ATTEMPTS    (4)   // then keep last good — a retry loop must not spin in audio

// Note events queue up here rather than being a single "current note" the audio thread samples once
// per buffer. Two things were wrong with that: the note only took effect at a buffer boundary, which
// is audible jitter at any sensible buffer size, and if two events landed inside one buffer only the
// last survived — so fast playing dropped notes.
//
// Written by the MIDI thread and the UI thread, drained by the audio thread. Multiple producers, one
// consumer: the write index is claimed with a fetch_add so no two producers take the same slot, and
// each slot publishes its own sequence number afterwards so the consumer can tell a slot that has
// been claimed from one that has actually been filled in.
#define NOTE_QUEUE_SIZE    (64)

typedef struct {
    int32_t          note;
    bool             on;
    _Atomic uint32_t sequence;   // claim index + 1 once written; 0 means never used
} tNoteEvent;

static tNoteEvent         gNoteQueue[NOTE_QUEUE_SIZE];
static _Atomic uint32_t   gNoteWrite                  = 0;
static uint32_t           gNoteRead                   = 0; // audio thread only

static _Atomic bool       gActive                     = false;

// Morph positions, 0..1, one per group. Written by the MIDI thread as controllers move, read by the
// UI thread when it builds a snapshot. Plain atomics: each is independent and a torn read is not
// possible on a value this size.
static _Atomic uint32_t   gMorphMilli[NUM_MORPHS]     = {0};
// The highest each morph has reached. The live value is useless as a diagnostic — by the time you
// have let go of the key and opened a menu to look at it, it has fallen back to zero.
static _Atomic uint32_t   gMorphPeakMilli[NUM_MORPHS] = {0};

// Pitch bend as it arrives, -1..+1. Scaled to semitones by the patch's own Bend range at render
// time, so changing the range takes effect without the wheel having to move.
// Output attenuation, as a gain x1000 so the audio thread reads one atomic rather than calling pow.
// Applied BEFORE the output knee, which is the point of it: pulling a hot patch down so the limiter
// stops being the thing that controls the level.
static _Atomic int32_t    gOutputGainMilli            = 1000;

static _Atomic int32_t    gBendMilli                  = 0;

// Highest absolute sample the audio thread has produced since this was last read. Purely a
// diagnostic — it is what lets a test say "sound is coming out" without a pair of ears.
static _Atomic uint32_t   gPeakMilli                  = 0;

// The peak BEFORE the output gain, so the real headroom a patch needs is visible rather than being
// hidden by whatever the guard clamped it to.
static _Atomic uint32_t   gRawPeakMilli               = 0;

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

static tSoundEngineStatus gStatus                     = eStatusOff;
static uint32_t           gPlayingCount               = 0; // how many modules are in the rendered chain

// Audio-thread-only state. Nothing else may touch these.
// THE WHOLE GRAPH RUNS OVERSAMPLED, which is what the G2 does: its audio rate is 96 kHz against a
// typical 48 kHz output (manual p.71). Two things need it and cannot get it any other way — the
// ladder filter, whose model stops holding as its poles approach Nyquist, and any nonlinearity,
// whose harmonics fold back down if they are made too close to the output rate.
//
// Doing it for the WHOLE graph rather than per node is both simpler and better: there is no input
// to interpolate for each nonlinear node and no per-node decimator, just one filter at the very
// end. The linear parts (mixers, amplifiers, delay, reverb) gain nothing from it but cost little,
// and having one rate throughout means nothing has to know it is happening.
#define ENGINE_OVERSAMPLE    (2)

// The tempo a clock-synced module works to. The engine does not run the patch's master clock, so
// anything set to Clk needs a reference; 120 BPM is the obvious one and makes 1/4 exactly half a
// second. See the delay's Clk branch — this is a stand-in, not the hardware's tempo.
#define ENGINE_REFERENCE_BPM    (120.0)

static double             gDeviceRate                 = 48000.0;
static double             gSampleRate                 = 96000.0;

// ── VOICES ──────────────────────────────────────────────────────────────────────────────────────
//
// One of these per simultaneously sounding note. Everything here used to be a single global, which
// is what made the engine monophonic — not any shortage of oscillators, just one copy of "which note
// is playing and is its key still down".
//
// `note` OUTLIVES THE GATE. A released voice is still sounding its release, and that release has to
// stay at the pitch it was played at, so the note is only cleared when the voice is taken for
// something else.
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
} tVoice;

static tVoice             gVoice[MAX_VOICES] = {0};
static uint64_t           gVoiceClock        = 0;

// Published for the note stack, which has to know whether to release the note it was given or to
// fall back to the newest one still held. An atomic rather than a look into the parameter snapshot:
// it is read from the MIDI thread, and copying the whole snapshot to answer one question would be
// absurd. See sound_engine_is_polyphonic().
static _Atomic uint32_t   gEngineVoices      = 1;

// RENDER LOAD, as a percentage of real time, peak-held. The time spent inside sound_engine_render()
// against the time the buffer it filled will take to play: at 100 % the engine is using the whole of
// its deadline and the next buffer is late, which is heard as crackling rather than as anything
// musical. Peak-held because the interesting figure is the worst buffer, not the average — one late
// buffer in a hundred is plainly audible and would vanish into a mean.
static _Atomic uint32_t   gLoadPercent       = 0;

static void reset_voices(void);
static uint32_t voice_count_for_patch(uint32_t slot);

static double             gVibratoPhase      = 0.0;
static tSoundEngineParams gLastGoodParams    = {0};
static uint64_t           gSeenTopology      = 0;

// Per-node state, indexed by node position. Carried across snapshots while the topology signature
// holds, so turning a knob does not restart the oscillator or reopen the envelope.
// Oversampling for the oscillator section. The G2 runs its audio at 96 kHz against the 48 kHz
// typical here, so 2x alone would match the hardware's rate; 4x is used because polyBLEP's residual
// error falls with the square of the phase step, and the shape waveforms have no band-limiting of
// their own at all and depend entirely on this.
//
// The two numbers are not independent, and the filter is the one that matters. Measured on a
// sawtooth at C7 (the worst case in the audible range), residual aliasing went:
//
//     32 taps -34 dB | 64 taps -43 dB | 128 taps -71 dB | 256 taps -73 dB
//
// and 8x oversampling at 256 taps measured the same as 4x at 128 — what counts is the transition
// width, which is taps DIVIDED BY the oversampling factor, so doubling the rate without doubling the
// filter buys nothing and costs twice the arithmetic. 4x/128 sits at the knee: the hardware capture
// this was matched against measures about -32 dB, so the engine is now well clear of it.
//
// Decimation only computes the samples it keeps, so the cost is 128 multiply-accumulates plus four
// waveform evaluations per oscillator per output sample.
// Relative to the ENGINE rate, which is itself oversampled — so the oscillators still run at four
// times the device rate overall, as they did when the graph ran at the device rate and this was 4.
#define OSC_OVERSAMPLE    (4 / ENGINE_OVERSAMPLE)
// 48, not the 128 this began with, and the difference is CPU rather than taste. This filter is the
// engine's single largest cost — it runs once per oscillator per voice per oversampled sample, so
// its length is multiplied by the polyphony, and at eight voices 128 taps was enough on its own to
// miss the audio deadline (CoreAudio reports that as "skipping cycle due to overload", heard as
// crackling).
//
// MEASURED, worst image folding back into 0..20 kHz when decimating 192 kHz to 96 kHz, against the
// deviation the filter causes inside that band:
//
//     128 taps  -116 dB   0.00 dB      48 taps   -90 dB   0.00 dB
//      64 taps   -98 dB   0.00 dB      32 taps   -82 dB   0.00 dB
//
// The passband is untouched at every length because the cutoff sits at 43 kHz, far above anything
// audible — the taps buy stopband depth alone. 90 dB is below the noise floor of any playback path
// this will meet, so the remaining 26 dB was being paid for in CPU and heard by nobody.
#define OSC_DECIMATE_TAPS    (48)

// The engine's own output filter, removing everything above the DEVICE's Nyquist before the extra
// samples are dropped. Same windowed-sinc design as the oscillators' — see the note there on why the
// transition width, not the oversampling factor, is what governs the result.
#define OUT_DECIMATE_TAPS    (64)

static double   gOutDecimate[OUT_DECIMATE_TAPS];
static double   gOutHistory[4][OUT_DECIMATE_TAPS];   // [pair*2 + channel]; one shared cursor, see the render loop
static uint32_t gOutHistoryPos = 0;

static double   gOscDecimate[OSC_DECIMATE_TAPS];

// ── PER-VOICE NODE STATE ────────────────────────────────────────────────────────────────────────
//
// The Voice Area is instantiated once PER VOICE on the hardware and the FX Area once for the whole
// patch (manual p.85: "you don't need a separate Reverb in each voice, all voices can share one
// Reverb module in the FX Area"). So everything a Voice Area module remembers between samples —
// oscillator phase, filter poles, envelope stage — has to exist once per voice, or two notes held
// together share one oscillator phase and one envelope and behave as one.
//
// Indexed [voice][node]. The voice index is 0 for everything in the FX Area, which is evaluated once
// after the voices have been summed.
//
// NOT per voice, deliberately: the delay lines, the chorus lines and the reverb. They are the large
// buffers, they are FX modules, and one shared instance is what the hardware has. A patch that puts
// one of them in the VOICE area gets a single shared instance rather than one per voice — an
// approximation, and the only one in this split.
// FLOAT, not double, and for the same reason the tap count came down: at eight voices this array is
// walked a few million times a second and the loop is bound by how fast it can be read rather than
// by the arithmetic. Halving the bytes halves that. The accumulation is still done in double.
static float    gOscHistory[MAX_VOICES][MAX_ENGINE_NODES][OSC_DECIMATE_TAPS];
static uint32_t gOscHistoryPos[MAX_VOICES][MAX_ENGINE_NODES];

static double   gPhase[MAX_VOICES][MAX_ENGINE_NODES];
static double   gLfoLastPhase[MAX_VOICES][MAX_ENGINE_NODES];
static double   gLfoTarget[MAX_VOICES][MAX_ENGINE_NODES];
static double   gLfoHeld[MAX_VOICES][MAX_ENGINE_NODES];
static double   gSuperPhase[MAX_VOICES][MAX_ENGINE_NODES][2];
static double   gLadder[MAX_VOICES][MAX_ENGINE_NODES][LADDER_POLES];

// Delay memory. Held as float rather than double purely for size — half a second per line at any
// sensible rate, four lines, is enough for the delays a patch normally has and keeps this under a
// megabyte. Nodes beyond that many run dry rather than sharing a line and smearing into each other.
#define MAX_DELAY_LINES    (4)
// Long enough for the longest range the Time dial offers (2.7 s), at the INTERNAL rate. It used to
// be a flat 48000, i.e. one second at 48 kHz — so the top of the dial was silently truncated to
// well under half the delay it promised.
// 2.8 s at 48 kHz, times the oversampling — integer arithmetic so it stays a constant expression an
// array can be sized with.
#define DELAY_LINE_SAMPLES    (134400 * ENGINE_OVERSAMPLE)
static float    gDelayLine[MAX_DELAY_LINES][DELAY_LINE_SAMPLES];
static uint32_t gDelayWrite[MAX_DELAY_LINES];
static double   gDelayDamp[MAX_DELAY_LINES];
static double   gDelayHp[MAX_DELAY_LINES];   // the HP's lowpass half; the filter is x - this

// The chorus's own short sweep, plus its LFO phase. TWO LINES PER NODE: the instrument runs left and
// right through the same algorithm with their LFOs in ANTIPHASE, so one phase accumulator serves
// both — the right channel simply reads it half a cycle along. See chorus_step().
#define CHORUS_SAMPLES     (2048 * ENGINE_OVERSAMPLE)
#define CHORUS_CHANNELS    (2)
static float    gChorusLine[MAX_ENGINE_NODES][CHORUS_CHANNELS][CHORUS_SAMPLES];
static uint32_t gChorusWrite[MAX_ENGINE_NODES][CHORUS_CHANNELS];
static double   gChorusLfo[MAX_ENGINE_NODES];

// Pulse: the countdown still to run, and the previous input, so a rising edge can be seen. Per voice,
// because the gate is fired by that voice's own envelope.
static uint32_t gPulseCount[MAX_VOICES][MAX_ENGINE_NODES];
static double   gPulsePrev[MAX_VOICES][MAX_ENGINE_NODES];

// Compressor gain-reduction state, one per node.
static double   gCompEnv[MAX_VOICES][MAX_ENGINE_NODES];

// A Schroeder reverb: eight combs into three allpasses. One reverb is modelled; any further ones pass
// their input through, which is what a patch with two of them would mostly sound like anyway.
//
// SIXTEEN COMBS SINCE 2026-08-18, AND THE SHORTAGE IS WHY IT FLUTTERED. This bank started as
// Freeverb's, which specifies eight; only the FIRST FOUR were ever here, so the tank ran at a quarter
// of the density it now has. A comb is a periodic echo generator, and too few of them means the tail
// is not a decay at all but a train of discrete echoes at the comb recirculation rates.
//
// MEASURED AGAINST THE INSTRUMENT, same patch and settings (Hall, Time 122, Bright 64), using the
// MODULATION SPECTRUM of the tail's envelope — which is what "flutter" actually names, and the only
// metric tried that separated the two. Envelope ripple and echo density both said the engine was
// FINE, and both were wrong:
//
//     hardware    flat: nothing above 2.9% anywhere from 5 to 58 Hz
//     4 -> 8      5.4% at 18.2 Hz, standing clear of everything around it. 18.2 Hz is a 55 ms
//                 period against Hall's longest comb of 56.6 ms — one comb ringing on its own
//     + damping   4.4%, no longer clear of its neighbours (see comb_damping)
//     16 combs    3.6% and FLAT, with the 18 Hz peak gone entirely
//
// The other eight lengths continue Freeverb's progression and are prime, so no two lines reinforce.
// THE INSTRUMENT HAS ABOUT 31 DELAY LINES (see Docs/todo.txt), so sixteen is still short of it —
// but it is now dense enough that the flutter does not survive the measurement.
#define REVERB_COMBS      (16)
#define REVERB_ALLPASS    (3)

// The Reverb's TYPE selector — Small, Medium, Large, Hall (reverbTypeStrMap) — is what sets the size
// of the room, and it was not read at all: all four types sounded identical, which is most of why
// this reverb does not sound like the instrument's. It is a MODE, not a parameter, so it comes from
// module->mode[] like OscShpB's waveform does.
#define REVERB_MODE_TYPE     (0)
#define REVERB_TYPE_COUNT    (4)

// Decay time against the Time dial, MEASURED per room type 2026-08-09: seconds = base + slope * value,
// with value the raw 0..127. See the long note at the point of use for the measurements, for why the
// slope is not simply proportional to room size, and for why these are early-decay figures rather than
// true RT60.
//
// This replaced "Range: 1.1 ms to 17.58 s" (manual p.251) driven through a cubic whose exponent was
// fitted by ear. The endpoints were the only documented part and the measurement does not reach them:
// Large tops out at 8.11 s. The manual's figure is left recorded here because it is still unexplained,
// not because it is unused — nothing reads it now.
// How hard the Brightness dial damps the comb loop. The one-pole coefficient is _MAX times brightness
// raised to _CURVE, and both are FITTED against nine measured points of the instrument's own dial.
//
// WHAT TO MEASURE THIS AGAINST, because two other metrics sent me the wrong way first. The right target
// is the ratio of HIGH-BAND to LOW-BAND DECAY RATE, which is what an in-loop lowpass actually controls.
// Hall at Time 127, decay of 3-10 kHz over decay of 150-800 Hz:
//
//     Brightness       16     64    127
//     hardware       0.54   0.75   0.95
//     engine         0.53   0.70   1.00     (with the constants below)
//
//   - Broadband decay is NOT the target: it follows whichever band holds the energy, so it agreed with
//     several quite different filters. It also made the engine look 40-60% short of its decay target
//     when the low band was in fact within a second of the hardware; the shortfall was the measurement.
//   - Absolute tail COLOUR at a fixed moment is not the target either: scored that way, an exponent of
//     1.0 beat 0.15, which is the opposite of what the decay rates say. Colour at an instant mixes the
//     loop's damping with everything outside the loop, so it cannot isolate this coefficient.
//
// _MAX stays at 1.0 because the hardware's loop is very nearly transparent at the top of its dial: its
// HF and LF then decay within 0.6 s of each other, and its tail darkens by only 1-3 dB over three
// seconds against 14-16 dB at mid dial. Anything below 1.0 damps at full brightness, which the
// instrument does not do — a fitted 0.5 left every setting darkening and shortened the decay with it.
#define REVERB_DAMP_MAX    (0.62)
// RE-FITTED 2026-08-18 FOR THE NEW STRUCTURE. The old 0.15 was fitted against a comb bank, where
// the damping sat inside every comb's own loop and bit hard. In a feedback network the signal passes
// the damping once per circuit instead, so the same exponent barely moved the tail at all: the
// high-to-low decay ratio measured 0.89 / 0.94 / 0.96 at Brightness 16 / 64 / 127 against the
// instrument's 0.54 / 0.75 / 0.95, i.e. the dial did almost nothing. At 0.70 it reads
// 0.51 / 0.83 / 0.96. The ends are close; the middle is still about 0.08 too bright, which says the
// dial's shape is not a pure power law on this structure.
#define REVERB_BRIGHT_CURVE    (0.70)

static const double kReverbDecayBase[REVERB_TYPE_COUNT]  = {0.045, 0.29, 0.39, 0.32};
static const double kReverbDecaySlope[REVERB_TYPE_COUNT] = {0.02238, 0.04094, 0.06082, 0.08212};

// The allpass diffusion coefficient rises with the reverb time and is held between two limits. The
// slope and the limits are the instrument's; what drives them is normalised Time here, which is the
// part that is inferred rather than known — but the limits are close enough together that the whole
// range is only 0.45..0.62, so being wrong about the position within it is a small error and being
// outside it would not be.
#define REVERB_DIFFUSE_SLOPE    (0.75)
#define REVERB_DIFFUSE_BASE     (0.40)
#define REVERB_DIFFUSE_MIN      (0.45)
#define REVERB_DIFFUSE_MAX      (0.62)

// How much bigger each type's room is than the base set below. THE SHAPE OF THIS IS RIGHT: the
// instrument really does scale every one of its delay lines by a single factor per type, so one
// number per room is the correct form rather than a convenience.
//
// MEASURED ON THE HARDWARE, 2026-08-09, and the four numbers are no longer guesses. A click was fed
// through the Reverb at 192 kHz with the dry impulse captured on a second output pair, and the tail's
// autocorrelation gives the lengths the tank recirculates at (tools/measure.py, tools/analyse_ir.py).
// Fitting ONE scale per room against every lag Small shows lands within 0.1% on the strong ones:
//
//     Small  2408 -> Large  3669  (predicted 3673.4, -0.12%)   -> Hall  4042  (4044.2, -0.06%)
//     Small  2422 -> Large  3695  (predicted 3694.8, +0.01%)   -> Hall  4064  (4067.7, -0.09%)
//     Small  4215 -> Large  6430  (predicted 6430.0, +0.00%)   -> Hall  7078  (7079.1, -0.02%)
//     Small  4599 -> Large  7016  (predicted 7015.8, +0.00%)   -> Hall  7723  (7724.0, -0.01%)
//
// Four independent lengths agreeing with a ONE-parameter fit to a hundredth of a percent is not a
// coincidence, and it settles the assumption as well as the numbers: the instrument does scale the
// whole tank by a single factor. The short diffusion lags scale by the same factor (Small 216 ->
// Large 329, exact; Small 576 -> Hall 965, -0.25%), so it is the room and not just the tail.
//
// SMALL IS THE REFERENCE (1.0), not Large, because Small is the room whose lengths were measured most
// completely — the base table below is Small's. See [[project_g2_reverb_measurements]].
static const double kReverbTypeScale[REVERB_TYPE_COUNT] = {1.0, 1.2690, 1.5255, 1.6795};
#define REVERB_SCALE_MAX    (1.6795)

// Mutually prime lengths, so the combs do not reinforce each other into a ringing tone. The buffers
// are sized from the longest of each set rather than a hand-written number — getting those out of
// step is a buffer overrun, and it is the kind that only shows up as a crash much later. The base
// set is scaled with the rate (these are sample counts, so leaving them fixed would halve the room)
// AND by the type, hence the extra headroom for the largest type in the two MAX figures.
//
// THE BASE SET IS STILL NOT THE INSTRUMENT'S, and now that the scale above is measured it is the only
// part of the room that is not. Measured Small recirculates at 2376, 2408, 2422, 4215 and 4599 samples
// at 96 kHz on Out 3 alone — a tight cluster of three plus two lines at roughly 1.8x — where this base
// set is four lengths spread over 1.21:1 and nothing long. It is deliberately NOT swapped for the
// measured numbers yet: three lengths within 2% of each other in a PARALLEL COMB BANK beat against
// each other, so the measured set only makes sense in a network that feeds each line from the others,
// and the measurement does not say which topology the instrument uses. Loading them into this
// structure could easily sound worse while matching the numbers better.
//
// The scale fix stands on its own, though: it is a ratio, so it is right whatever the base set is, and
// it moves Small from half the room to the whole of it.
#define REVERB_COMB_BASE       (1667 * ENGINE_OVERSAMPLE)   // the LONGEST comb of EITHER channel; buffers are sized from it
#define REVERB_ALLPASS_BASE    (225 * ENGINE_OVERSAMPLE)
// INTEGER ARITHMETIC, NOT A CAST OF A FLOAT PRODUCT. These size static arrays, and an array bound has
// to be an integer constant expression — `(uint32_t)(base * 1.6)` is not one, so clang accepted it only
// as a GNU extension, "variable length array folded to constant array". A static VLA is not something
// to leave resting on an extension.
//
// x18/10 COVERS REVERB_SCALE_MAX (1.6795) WITH ROOM TO SPARE, and it must: this pair and the scale
// table are one decision in two places, so a scale raised without raising this writes past the end of
// every delay line. It was 16/10 when the largest type was exactly 1.6, which the measured 1.6795 then
// silently outgrew by 66 samples per comb. Rounding up rather than tracking the scale exactly costs a
// few kilobytes and removes the trap.
// THE STEREO SPREAD: the right channel runs the SAME topology with every line lengthened by this.
//
// Deliberately a spread rather than the instrument's own measured lengths. Its two output channels
// are near-disjoint tap sets (Small L 2338/2407/2420/4214/5022 against R 2378/2904/3903/3972/4046)
// but its tank has about 31 lines against this one's 4 combs and 3 allpasses, and the ROUTING is not
// recoverable — a real set of lengths in the wrong arrangement sounds plausible and is wrong, which
// is the hardest kind of error to find. See the REVERB entry in Docs/todo.txt.
//
// So this claims no new structure. It is Freeverb's own answer to the same question, and what makes
// it honest is that the thing it is aimed at IS measured: the instrument's two outputs correlate at
// +0.012..+0.045, so 0.03 is the target, and tools/render + analyse_ir.py read the same number off
// this code. Tuned against that — see the note in sound_engine_render_reverb_ir().
// THE RIGHT CHANNEL READS ITS TAPS EARLIER, NOT LATER. The instrument's two outputs were measured
// 1.14 ms apart with the RIGHT one arriving first -- 12.89 ms against 11.75 in the Small room, and
// the same gap in each of the other three -- so this is SUBTRACTED from the right channel's tap
// offsets. 110 samples is that gap at 96 kHz. It used to be added, which put the right channel on
// the wrong side of the left; no amount of correcting the magnitude would have found that.
#define REVERB_SPREAD         (110)
#define REVERB_CHANNELS       (2)

#define REVERB_COMB_MAX       (((REVERB_COMB_BASE * 18) / 10) + REVERB_SPREAD + 1)
#define REVERB_ALLPASS_MAX    (((REVERB_ALLPASS_BASE * 18) / 10) + REVERB_SPREAD + 1)

// PER-CHANNEL PRE-DELAY, MEASURED PER ROOM ON THE HARDWARE 2026-08-18. The instrument's two outputs
// do not start together, and this engine had no pre-delay at all, so both tails began at the input.
//
// THIS IS THE ONE LINE IN THIS REVERB THAT DOES NOT FOLLOW THE ONE-FACTOR RULE. Every other length
// here is a room-size factor times a constant (see kReverbTypeScale), and an earlier version of this
// table assumed the pre-delay was too. It is not: across the four rooms the left pre-delay moves only
// 12.90 -> 13.33 ms, a 3.3% spread, where kReverbTypeScale spans 68%. Scaling it would have put Hall
// at 21.67 ms against a measured 13.33. So these are eight independent numbers, not two and a factor.
//
// AND THE OLD RIGHT-CHANNEL FIGURE WAS SIMPLY WRONG — recorded as 7.54 ms, actually 11.75 ms. At
// 7.5 ms both channels are still in the noise (3.8-6.7% of peak); the right channel leaves it at
// 11.50 ms and the left at 12.75 ms, read sample by sample off the raw capture rather than through a
// threshold. So the two channels are about 1.15 ms apart, not 5.34.
//
// MEASURED TWICE, INDEPENDENTLY, AND THEY AGREE TO 0.06 ms: once from the stored 192 kHz Fireface
// captures (tools/ rig, Time sweeps read at Time 0 where the tail clears between impulses) and once
// live into a QU-24 at 48 kHz. Pre-delay is a fixed line length — Type sets the delay lengths, Time
// only the feedback gain — which the measurement confirms: the same figures come back at Time 0, 32
// and 64.
//
// Expressed as sample counts at the 48 kHz base rate, times ENGINE_OVERSAMPLE, for the same reason
// the comb lengths are: an array bound has to be an INTEGER constant expression. Sizing one with a
// float cast is what produced the -Wgnu-folding-constant pair recorded in Docs/todo.txt.
//
//                        Small   Medium   Large    Hall
//     left    (ms)       12.89   13.05    13.30    13.36
//     right   (ms)       11.75   11.90    12.14    12.20
#define REVERB_PREDELAY_MAXSAMP    (641 * ENGINE_OVERSAMPLE)   // the largest below, Hall left
#define REVERB_PREDELAY_MAX        (((REVERB_PREDELAY_MAXSAMP * 11) / 10) + 1)
// ─── THE INSTRUMENT'S OWN REVERB STRUCTURE ──────────────────────────────────────────────────────
//
// Recovered 2026-08-18 and rebuilt here. It is NOT a bank of parallel combs, which is what this used
// to be and why no amount of tuning ever made it sound right: a comb generates a periodic echo at
// its own rate, and with every line between 23 and 34 ms that periodicity is audible as flutter,
// worst at the end of a tail where the density thins.
//
// The instrument is a serial allpass diffuser feeding a feedback delay network, read by a set of
// fixed output taps. Its lines span 1.1 ms to 235 ms; ours spanned 23 to 34 ms and nothing else,
// which is the whole difference.
//
// THE LENGTHS ARE EXACT, in samples at the base rate for the Small room, scaled by kReverbTypeScale
// for the others. What is inferred rather than measured is how the mixing stages are wired to each
// other — the tap set, the stage pairings and the coefficients are all recovered.
#define RV_OUTTAPS    (16)
// THE ALLPASS COEFFICIENTS ARE THE INSTRUMENT'S, read straight out of its mixing gains: the two it
// pairs 0.4820 with 0.7676 and 0.3102 with 0.9038, and 1 - g*g for those g values is exactly those
// two numbers. The input diffuser's own gains come out heavier, at 0.75/0.5.
#define RV_DIFFUSE    (0.5000)
#define RV_TANK_A     (0.4820)
#define RV_TANK_B     (0.3102)

// Sixteen taps summed with alternating signs add up like a random walk, so the sum grows as the
// square root of the count and this is 1/sqrt(16). REVERB_WET_GAIN sets the level; this only keeps
// the tap count from changing it.
#define RV_TAP_SCALE    (0.35355339059327373)

// 1/sqrt(8) -- what makes the 8-point Hadamard butterfly orthogonal rather than a gain of 8.
#define RV_HADAMARD     (0.35355339059327373)

// ONE TRIP IS BOTH BRANCHES, since each feeds the other: the two branch lengths added. In samples
// at 96 kHz, and NOT scaled by the room — every span scales together, so the trip scales with it,
// which is why a Hall rings longer than a Small room at the same Time setting.
// Nothing shared here any more: each line gets its own decay gain from its own length, worked out
// in gRvGain below. A single figure for the whole tank is what a series loop needs, and this is not
// one.

// THE LAYOUT. Spans laid end to end, each one a line; a section writes at its own base and reads at
// the next, so these lengths ARE the delays. Every length is the instrument's, recovered from the
// spacing of its tap addresses: the allpasses at 672, 738, 666 and 812, the lines at 2300, 2456,
// 3999 and 5326.
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

#define RV_LINES    (8)

// MODULATION DEPTH, in samples at 96 kHz, and the rate each line sweeps it at.
//
// A TANK WITH FIXED DELAYS HAS FIXED MODES, and fixed modes ring -- that is what a metallic reverb
// is. Measured as how much a tail's magnitude spectrum resembles itself a moment later, over
// 400 Hz to 4 kHz and at matched resolution:
//
//                      0.05 s  0.15 s  0.35 s  0.75 s  1.50 s
//     the instrument    +0.31   +0.23   +0.26   +0.19   +0.34
//     fixed delays      +0.74   +0.74   +0.73   +0.76   +0.73
//
// The instrument's fine structure is somewhere else a twentieth of a second later and stays that
// way; a fixed tank is still three-quarters itself a second and a half on. Sliding each line a few
// samples breaks the modes up without moving anything the ear hears as pitch: 16 samples at about
// 1 Hz is a peak shift near 0.9 cents, and on the shortest line it is 2.4% of its length.
//
// THE RATES SHARE NO SIMPLE RATIO, for the same reason the line lengths do not -- eight sweeps that
// realign every cycle would put their own period into the tail, which is the fault being fixed.
#define RV_MOD_DEPTH    (28)

// WHAT THE SWEEP COSTS THE DECAY. Reading a delay line at a fractional position means interpolating
// between two samples, and linear interpolation is a mild lowpass -- so every pass round the tank
// loses a little that a whole-sample read would not. Measured, it shortened a Hall from RT60 9.67 s
// to 8.87 s where the dial had asked for 10.34. It DIVIDES the gain rather than scaling the
// exponent: the interpolator takes a fixed amount out of every pass whatever the decay setting, so
// scaling the exponent over-corrects exactly where the requested loss is largest -- it put a short
// room 8% long while fixing the long one. Not a fudge factor for something unexplained: it is the
// interpolator's own attenuation, and it would change if the interpolator did.
//
// THIS RESTORES THE DIAL AS THE ONLY PLACE DECAY IS SET. What the dial asks for is
// kReverbDecayBase/Slope, and those are a separate question -- see the note there for how they now
// stand against the hardware.
#define RV_MOD_LOSS    (0.9955)

static const double kRvModHz[RV_LINES] = {
    0.61, 0.73, 0.89, 1.03, 1.19, 1.31, 1.47, 1.61
};

static double       gRvLfo[RV_LINES];
#define RV_DIFFUSERS    (6)

// EIGHT LINES IN PARALLEL, EACH WITH AN ALLPASS IN FRONT OF IT, MIXED INTO ONE ANOTHER.
//
// THE INPUT DIFFUSER IS WHAT MAKES IT DENSE, and density is a separate question from anything the
// frequency response can show. Measured as normalised echo density -- the fraction of samples in a
// sliding window exceeding that window's own standard deviation, over the 0.3173 a Gaussian gives,
// so 1.0 means fully dense, each side measured from its OWN wet onset:
//
//                        5 ms   10 ms   20 ms   40 ms
//     the instrument      0.97    0.99    1.01    1.00
//     four diffusers      0.78    0.82    0.99    1.00
//     six diffusers       0.85    0.99    1.08    1.01
//
// Sparse early reflections are heard as separate echoes, which is a metallic ring, and the two short
// sections at the head of the chain -- 43 and 73 -- are what fixed it. TEN sections made it worse,
// not better, dropping the 10 ms figure to 0.66: a run of very short allpasses lays its own
// repeating fine structure over the response. Six is where it matches.
//
// ALIGN BOTH SIDES TO THEIR OWN ONSET before comparing this. Measured from t=0 the engine's early
// windows sit in the pre-delay's silence and read 0.45 at 5 ms, which is the measurement and not
// the tank.
//
// THE LINE LENGTHS MUST SHARE NO COMMON FACTOR. Lines whose lengths share a factor share a period,
// and a shared period is a ring. The recovered figures -- 666, 672, 738, 812, 2300, 2456, 3999,
// 5326 -- are every one of them even and three share a 3, so these are the nearest prime to each.
static const uint32_t kRvLen[eRvSpanCount]      = {
    1060,                            // pre-delay
    43,     73,  107, 145, 277, 389, // the input diffuser
    661,   673,  739, 811,           // the four short lines
    2297, 2459, 4001, 5323           // and the four long ones
};

static const uint32_t kRvDiffuser[RV_DIFFUSERS] = {
    eRvDf1, eRvDf2, eRvDf3, eRvDf4, eRvDf5, eRvDf6
};
static const uint32_t kRvLineDl[RV_LINES]       = {
    eRvLn0, eRvLn1, eRvLn2, eRvLn3, eRvLn4, eRvLn5, eRvLn6, eRvLn7
};

// Which line each output tap reads, and how far along it.
static const uint32_t kRvTapLine[RV_OUTTAPS]    = {
    eRvLn0, eRvLn1, eRvLn2, eRvLn3, eRvLn4, eRvLn5, eRvLn6, eRvLn7
};
static const double   kRvTapFrac[RV_OUTTAPS]    = {
    0.83, 0.59, 0.89, 0.67, 0.13, 0.37, 0.11, 0.41
};

static uint32_t       gRvAddr[eRvSpanCount + 1];

// THE RECOVERED LENGTHS ARE ALREADY IN 96 kHz SAMPLES — that is the rate the instrument's tank runs
// at and the rate every recovered figure is quoted in. They must NOT be multiplied by
// ENGINE_OVERSAMPLE the way the old Freeverb constants were: those were 44.1 kHz numbers that needed
// scaling up, these are not. Doing it anyway made every delay twice as long as it should be, put the
// feedback loop at 1.7 s instead of 0.85, and had the tail arriving in audible waves about a second
// apart. Converting by the engine's ACTUAL rate keeps the times right at any device rate.
#define RV_RATE    (gSampleRate / 96000.0)

// The sixteen recovered tap ADDRESSES are gone from here. They were positions in the instrument's
// own memory map, and this tank lays its spans out differently, so an address off that map means
// nothing against this one; kRvTapLine/kRvTapFrac say which line and how far along instead. What
// carried over is the count and the spread — sixteen taps scattered across every long line.


// ONE SHARED MEMORY PER CHANNEL, big enough for the largest room's highest address
// (21432 * 1.6795 + 1200, about 37200) with room to spare. The instrument uses 32768 words and
// wraps; the next power of two above what the addresses need costs 256 kB a channel and removes
// any question of a site aliasing onto another.
#define RV_MEM_SHIFT    (17)
#define RV_MEM          (1u << RV_MEM_SHIFT)

static float          gRvMem[REVERB_CHANNELS][RV_MEM];
static uint32_t       gRvCur[REVERB_CHANNELS];
static double         gRvDamp[REVERB_CHANNELS][RV_LINES];

// The low band the upper half of the dial subtracts, and the pole that defines it. Roughly 400 Hz
// at 96 kHz — low enough that taking some of it out reads as "brighter" rather than "thinner".
#define RV_LOW_A    (0.9744)
static double         gRvLow[REVERB_CHANNELS][RV_LINES];

// The two input poles. MEASURED, not chosen: the instrument's reverb is far darker than what goes
// into it, and this is the filter that makes it so -- see the fit by REVERB_INPUT_LP_HZ.
static double         gRevInLp[REVERB_CHANNELS];
static double         gRevInLp2[REVERB_CHANNELS];
static double         gRevInLp3[REVERB_CHANNELS];
static double         gRevInLp4[REVERB_CHANNELS];
static double         gRvLoop[REVERB_CHANNELS][RV_LINES];

// [room type][channel], in samples at the base rate. NOT scaled by kReverbTypeScale — see above.
static const uint32_t kReverbPreDelay[REVERB_TYPE_COUNT][REVERB_CHANNELS] = {
    {619 * ENGINE_OVERSAMPLE, 564 * ENGINE_OVERSAMPLE},    // Small   12.89 / 11.75 ms
    {626 * ENGINE_OVERSAMPLE, 571 * ENGINE_OVERSAMPLE},    // Medium  13.05 / 11.90 ms
    {638 * ENGINE_OVERSAMPLE, 583 * ENGINE_OVERSAMPLE},    // Large   13.30 / 12.14 ms
    {641 * ENGINE_OVERSAMPLE, 586 * ENGINE_OVERSAMPLE}     // Hall    13.36 / 12.20 ms
};
// THE TANK IS FED THROUGH A LOWPASS, because the instrument's tail STARTS darker than ours did.
//
// This is NOT the same thing as the in-loop damping, and the two were confused for a whole session.
// The damping sets how fast the high end DECAYS relative to the low, and it is already right —
// measured against the instrument at Hall / Time 122 / Bright 64, the decay rates agree closely
// (hardware -5.6 dB/s low and -7.7 high, engine -6.0 and -7.6, i.e. a high-to-low decay-time ratio
// of 0.72 against 0.79, on a fitted target of 0.75). What was wrong is where the tail STARTS: the
// instrument's is band-limited going in, ours was white.
//
// MEASURED, same capture, tail spectrum normalised so the two agree below 500 Hz:
//
//     1 kHz  -1.4 dB      2 kHz  -3.7 dB      4 kHz  -9.0 dB      8 kHz  -17.9 dB
//
// which is a one-pole to within about 3 dB at the very top. Hence the cutoff below, fitted to those
// four points. This is what the owner heard as "the G2 has more low end" — it does, relatively,
// because ours had far too much top.
// TWO POLES, not one: a single pole matched the instrument up to 2 kHz but left 4 and 8 kHz 2.0 and
// 6.7 dB too bright, because the instrument's roll-off is steeper than 6 dB/octave at the very top.
// The second pole sits an octave up so it barely touches the region the first one already fitted.
#define REVERB_INPUT_LP_HZ     (4600.0)
#define REVERB_INPUT_LP2_HZ    (4600.0)

// A THIRD POLE, AND THIS ONE IS THE INSTRUMENT'S OWN, not a fit. Its coefficient comes straight off
// the Time dial as 0.7 * time, so the filter closes as the room gets longer -- and the hardware does
// exactly that. Measured at Brightness 64, Hall, relative to 1 kHz:
//
//                   4 kHz    8 kHz   12 kHz   16 kHz
//     Time  32      -4.26   -12.86   -18.81   -22.07
//     Time 122      -5.93   -15.42   -21.84   -26.05
//
// A longer tail is a darker one, by 1.7 dB at 4 kHz rising to 4.0 dB at 16 kHz, and this pole is
// where that comes from. The two fixed poles above it carry the rest: at Time 32 this one is nearly
// wide open, yet the instrument is still 12.9 dB down at 8 kHz, so most of the darkness does not
// move with the dial and cannot be this.
#define REVERB_INPUT_LP_TIME    (0.7)

// A FOURTH POLE, well above the other three, and this one IS a fit. With the three above it the
// engine still ran 2 to 3 dB bright from 8 kHz up at both ends of the Time dial -- a fixed shortfall
// that gets steeper with frequency, which wants another pole rather than a lower corner on the ones
// already there. Dropping those instead would have cost a decibel at 4 kHz, where the match is
// already good.
#define REVERB_INPUT_LP4_HZ    (12000.0)
static float    gPreDelay[REVERB_CHANNELS][REVERB_PREDELAY_MAX];
static uint32_t gPreDelayPos[REVERB_CHANNELS];
static double   gEnvLevel[MAX_VOICES][MAX_ENGINE_NODES];
// Linear 0..1 through the current segment, and the level it started from. Shaping this rather than
// the step keeps a segment's DURATION exactly what its dial says, whatever curve it draws.
// PARAMETER SMOOTHING. The G2 runs its modulation at 24 kHz (manual p.71 — "modules can process and
// output signals at two sample rates: 96kHz and 24kHz", the lower one being for modulation), so a
// dial being turned arrives at the DSP finely stepped and needs no smoothing of its own. This engine
// rebuilds its parameter snapshot on a REDRAW, i.e. at frame rate, which is a few hundred times
// coarser — and a stepped parameter is audible as zipper noise, most obviously on a Shape sweep
// where each step moves the waveform itself.
//
// Interpolating per sample toward the snapshot value restores what the hardware gets for free.
// The time constant is short enough not to lag a deliberate move and long enough to bridge the gap
// between frames.
#define PARAM_SMOOTH_SECONDS    (0.008)

static double   gSmoothShape[MAX_ENGINE_NODES];
static double   gSmoothCutoff[MAX_ENGINE_NODES];
static double   gSmoothRes[MAX_ENGINE_NODES];
static double   gSmoothGain[MAX_ENGINE_NODES];
static double   gSmoothLevel[MAX_ENGINE_NODES][MAX_NODE_INPUTS];

// Where the per-sample smoothing pass leaves its results, for the voice passes to read. Not per
// voice: a knob is in one place however many notes are sounding, and smoothing it inside the voice
// loop would advance the filter once per voice — so a sweep would speed up as more keys went down.
static double   gSmoothedShape[MAX_ENGINE_NODES];
static double   gSmoothedCutoff[MAX_ENGINE_NODES];
static double   gSmoothedRes[MAX_ENGINE_NODES];
static double   gSmoothedGain[MAX_ENGINE_NODES];
static double   gSmoothedLevel[MAX_ENGINE_NODES][MAX_NODE_INPUTS];
// Until a node has been seen once there is nothing to interpolate FROM, so the first sample snaps.
// Also what stops a patch load sweeping every parameter up from whatever the last patch left.
static bool     gSmoothPrimed[MAX_ENGINE_NODES];

static double   gEnvProgress[MAX_VOICES][MAX_ENGINE_NODES];
static double   gEnvStart[MAX_VOICES][MAX_ENGINE_NODES];
static uint32_t gEnvStage[MAX_VOICES][MAX_ENGINE_NODES];

typedef enum {
    eEnvIdle = 0,
    eEnvAttack,
    eEnvDecay,
    eEnvSustain,
    eEnvRelease,
} tEnvStage;

void sound_engine_pitch_bend(double bend) {
    if (bend < -1.0) {
        bend = -1.0;
    } else if (bend > 1.0) {
        bend = 1.0;
    }
    atomic_store(&gBendMilli, (int32_t)(bend * 1000.0));
}

void sound_engine_set_output_level_db(double db) {
    double gain = pow(10.0, db / 20.0);

    if (db >= 0.0) {
        gain = 1.0;    // attenuation only: this is a trim, not a boost into the limiter
    }
    atomic_store(&gOutputGainMilli, (int32_t)((gain * 1000.0) + 0.5));
}

bool sound_engine_set_morph(uint32_t group, double amount) {
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

// The Glide dial's 128 settings are a table of times running from 19 ms to 6.27 s, written as text
// for the patch-settings display. Reading the milliseconds back out of it means the engine glides
// for exactly as long as the editor says it will.
// Glide time. INTERPOLATED between table entries rather than computed, because unlike the A/D/R
// curve this table has no decent closed form — the best power-law fit is 17% out at the median and
// 38% at worst, which would be a far bigger error than reading it. Interpolating gives what
// computing was wanted for, a value that moves continuously with a morphed or smoothed dial, while
// staying exact at every position the dial can actually stop on.
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
    return atomic_load(&gActive);
}

// The device's rate; the ENGINE runs at ENGINE_OVERSAMPLE times this. gSampleRate is the internal
// rate, so every coefficient already derived from it — envelope and glide times, filter and chorus
// coefficients, LFO and oscillator increments — scales with no further change.
void sound_engine_set_sample_rate(double sampleRate) {
    if (sampleRate > 0.0) {
        gDeviceRate = sampleRate;
        gSampleRate = sampleRate * (double)ENGINE_OVERSAMPLE;
    }
}

static void reset_node_state(void) {
    uint32_t i = 0;
    uint32_t v = 0;

    for (v = 0; v < MAX_VOICES; v++) {
        for (i = 0; i < MAX_ENGINE_NODES; i++) {
            gLfoLastPhase[v][i]  = 0.0;
            gLfoTarget[v][i]     = 0.0;
            gLfoHeld[v][i]       = 0.0;
            gOscHistoryPos[v][i] = 0;
            memset(gOscHistory[v][i], 0, sizeof(gOscHistory[v][i]));

            // Spread rather than zeroed, for the same reason the note-on path leaves them alone:
            // from the very first note the oscillators should be at unrelated points in their
            // cycles. The step is irrational-ish so no two land together — and the VOICE is folded
            // into it as well, so two voices playing the same note are not phase-locked copies of
            // each other. Held notes on the hardware do not cancel and reinforce like that.
            gPhase[v][i]         = fmod(((double)i + ((double)v * 0.618034)) * 0.381966, 1.0);
            gSuperPhase[v][i][0] = 0.0;
            gSuperPhase[v][i][1] = 0.0;
            gLadder[v][i][0]     = 0.0;
            gLadder[v][i][1]     = 0.0;
            gLadder[v][i][2]     = 0.0;
            gLadder[v][i][3]     = 0.0;
            gEnvLevel[v][i]      = 0.0;
            gEnvProgress[v][i]   = 0.0;
            gEnvStart[v][i]      = 0.0;
            gEnvStage[v][i]      = eEnvIdle;
            gCompEnv[v][i]       = 0.0;
            gPulseCount[v][i]    = 0;
            gPulsePrev[v][i]     = 0.0;
        }
    }

    for (i = 0; i < MAX_ENGINE_NODES; i++) {
        gSmoothPrimed[i]   = false;
        gChorusWrite[i][0] = 0;
        gChorusWrite[i][1] = 0;
        gChorusLfo[i]      = 0.0;
        memset(gChorusLine[i], 0, sizeof(gChorusLine[i]));
    }

    memset(gDelayLine, 0, sizeof(gDelayLine));
    memset(gDelayWrite, 0, sizeof(gDelayWrite));
    memset(gDelayDamp, 0, sizeof(gDelayDamp));
    memset(gDelayHp, 0, sizeof(gDelayHp));
    memset(gPreDelay, 0, sizeof(gPreDelay));
    memset(gRvMem, 0, sizeof(gRvMem));
    memset(gRvCur, 0, sizeof(gRvCur));
    memset(gRvDamp, 0, sizeof(gRvDamp));
    memset(gRvLow, 0, sizeof(gRvLow));
    memset(gRvLoop, 0, sizeof(gRvLoop));
    memset(gPreDelayPos, 0, sizeof(gPreDelayPos));
}

// The lowpass that turns OSC_OVERSAMPLE samples back into one. A windowed sinc: cut just under the
// output rate's Nyquist so nothing is lost from the audible band, with a Blackman window to hold the
// stopband down where the images sit — an image that survives here is exactly the aliasing the
// oversampling was meant to remove.
static void build_decimator(void) {
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
    build_decimator();
    // Start from silence rather than inheriting whatever the last run left behind. That includes
    // the note queue: anything posted while the engine was off — the Virtual Keyboard, or MIDI from
    // a previous run — is stale, and starting the read index behind the write index would have the
    // audio thread chewing through history instead of playing what is being pressed now.
    gNoteRead = atomic_load(&gNoteWrite);
    reset_node_state();
    reset_voices();
}

// For a plug-in host: prime the engine and mark it live, but leave the audio device alone. The
// caller drives sound_engine_render() from its own process callback.
void sound_engine_start_hosted(double sampleRate) {
    sound_engine_set_sample_rate(sampleRate);
    engine_prime();
    atomic_store(&gActive, true);
}

void sound_engine_stop_hosted(void) {
    atomic_store(&gActive, false);
}

bool sound_engine_start(void) {
    if (atomic_load(&gActive) == true) {
        return true;
    }
    build_decimator();
    // Start from silence rather than inheriting whatever the last run left behind. That includes
    // the note queue: anything posted while the engine was off — the Virtual Keyboard, or MIDI from
    // a previous run — is stale, and starting the read index behind the write index would have the
    // audio thread chewing through history instead of playing what is being pressed now.
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
    if (atomic_load(&gActive) == false) {
        return;
    }
    // Clear the flag first: the device teardown below waits for any render in flight to finish, and
    // that render should already be seeing an inactive engine.
    atomic_store(&gActive, false);
    audio_output_stop();
}

const char * sound_engine_status_text(void) {
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

// Answers the question "why is there no vibrato" without a debugger: whether the keyboard is
// sending pressure at all, where that has left the morph, and whether the patch actually put an LFO
// into the graph. Those three failures look identical from the outside — silence — but need
// completely different fixes.
const char * sound_engine_modulation_text(void) {
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
                             (unsigned)gPatchDescr[gSlot].activeVariation,
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
    uint32_t claim = atomic_fetch_add(&gNoteWrite, 1);
    uint32_t slot  = claim % NOTE_QUEUE_SIZE;

    gNoteQueue[slot].note = note;
    gNoteQueue[slot].on   = on;

    // Published last: the consumer treats a slot as filled only once this matches.
    atomic_store(&gNoteQueue[slot].sequence, claim + 1);
}

// ── VOICE ALLOCATION (audio thread) ─────────────────────────────────────────────────────────────

static void reset_voices(void) {
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

// How many voices this patch may use at once. Mono and Legato are one voice whatever the count says,
// and in Poly the descriptor's field holds the count MINUS ONE — the topbar's readout does exactly
// this arithmetic (topbarRender.c), and taking it from the same place is what stops the engine
// playing a different number of notes from the one on screen.
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
    for (uint32_t v = 0; v < count; v++) {
        if ((gVoice[v].note == note) && (gVoice[v].sounding || gVoice[v].gate)) {
            return (int32_t)v;
        }
    }

    return -1;
}

// Which voice a new note should take, out of the `count` the patch allows. In preference order: one
// that is doing nothing, then the longest-released, then the oldest still held. Only the last of
// those is a steal — cutting a note off — and it is what a polyphonic instrument does when it runs
// out, so it is worth being sure the two cheaper cases are exhausted first.
static uint32_t voice_to_allocate(uint32_t count) {
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
    // Auto glide only slides between overlapping notes, which is the point of it: a phrase played
    // legato slides, a detached note starts where it means to. Whether THIS VOICE'S gate is already
    // open is that test — and it is why the check has to happen before the gate is opened below.
    //
    // Per voice rather than patch-wide: in Poly each note lands on a voice of its own, which was not
    // playing anything, so nothing slides. That is correct. A glide in Poly only happens when a
    // voice is reused, which is also what the hardware does.
    voice->glideActive = voice->gate;

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
    for (uint32_t v = 0; v < MAX_VOICES; v++) {
        if ((note < 0) || (gVoice[v].note == note)) {
            gVoice[v].gate = false;
        }
    }
}

// Peak render load since the last read, as a percentage of real time. READING IT CLEARS IT, so the
// figure is always "the worst buffer since you last looked".
uint32_t sound_engine_load_percent(void) {
    return atomic_exchange(&gLoadPercent, 0);
}

bool sound_engine_is_polyphonic(void) {
    return atomic_load(&gEngineVoices) > 1;
}

uint32_t sound_engine_voice_count(void) {
    return atomic_load(&gEngineVoices);
}

// Read without a lock from whichever thread asks. It is a display figure that changes every time a
// key moves, so a torn read is one frame of a number that is about to change anyway.
uint32_t sound_engine_voices_sounding(void) {
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
    uint32_t write = atomic_load(&gNoteWrite);
    uint32_t slot  = 0;

    if (gNoteRead >= write) {
        return false;
    }

    // If the writer has lapped us the oldest events have already been overwritten, and the slot the
    // read index points at now holds something far newer. Waiting for a sequence number that can
    // never arrive would wedge the queue for good — every later note silently dropped — so skip
    // forward to the oldest event still intact. Losing the tail of a burst is recoverable; wedging
    // is not, and wedging is what made rapid playing fall apart.
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

// The exact scale the dial prints, rather than the power-law fit this used to be — see
// adr_time_seconds() in renderParams.c. Shared so the envelope that is heard cannot take a
// different time from the one shown.
// THE PULSE'S WIDTH IN SECONDS, as a closed form rather than a copy of the dial's 128 readings.
//
// The instrument's own readout for the Lo range runs 1.04 ms at dial 0 to 10.0 s at dial 127, and it
// is a geometric progression — a constant 7.49% per step — so the whole table is two endpoints and an
// exponent. Written this way deliberately: a 128-entry table truncates the FRACTIONAL dial values a
// morph or a smoothed knob produces, and would disagree with the dial's own text between steps.
//
// Range shifts it by a decade either way: Sub a tenth, Hi ten times (pulseRangeStrMap order).
static double pulse_time_seconds(double value, uint32_t range) {
    const double loMin   = 0.00104;
    const double loMax   = 10.0;
    double       seconds = loMin * pow(loMax / loMin, value / 127.0);

    if (range == 0) {
        seconds /= 10.0;          // Sub
    } else if (range == 2) {
        seconds *= 10.0;          // Hi
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

static bool module_kind(tModule * module, tNodeKind * kind) {
    switch (module->type) {
        case moduleTypeOscB:
        {
            *kind = eNodeOsc;
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
        {
            *kind = eNodeOscShp;
            return true;
        }
        case moduleTypeMix4to1C:
        case moduleTypeMix4to1S:
        {
            *kind = eNodeMix;
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

static uint32_t input_connectors(tNodeKind kind, tModuleType moduleType, bool stereoMix, const uint32_t ** connectors) {
    // Derived from the module resources rather than written out — see connector_index_for_input().
    // Static because the chain is built on one thread; the contents are rewritten per call.
    static uint32_t       derived[MAX_NODE_INPUTS];
    static const uint32_t oneIn[]       = {CONNECTOR_IN_A};
    static const uint32_t twoIn[]       = {CONNECTOR_IN_A, CONNECTOR_IN_B};
    static const uint32_t mixIn[]       = {0, 1, 2, 3};
    // In1L, In1R .. In4L, In4R as RAW CONNECTOR indices: Mix4to1S's connector list really does run
    // Out, Out, then ten inputs, so the first eight input legs are connectors 2..9.
    static const uint32_t mixStereoIn[] = {2, 3, 4, 5, 6, 7, 8, 9};
    static const uint32_t envIn[]       = {0};                      // connector 0 is the audio the envelope shapes
    // OscB has "two pitch modulation inputs, one frequency modulation input, one sync modulation
    // input and a Shape modulation input" (manual, OscB). The two pitch inputs are the control-rate
    // pair at 0 and 1; both are summed and scaled by the one Pitch knob the module carries.
    static const uint32_t oscIn[]       = {0, 1};
    static const uint32_t none[]        = {0};

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
            *connectors = oscIn;
            return 2;
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
            // A mono mixer's four inputs, or a stereo one's eight legs as four stereo pairs. Both
            // are just "the first N inputs", which the resources can answer — Mix4to1C happens to
            // put its inputs first and Mix4to1S puts its two outputs first, and neither fact needs
            // to be written down here any more.
            uint32_t count = stereoMix ? 8 : 4;
            uint32_t leg   = 0;

            for (leg = 0; leg < count; leg++) {
                int found = connector_index_for_input(moduleType, leg, anyConnectorType);

                derived[leg] = (found >= 0) ? (uint32_t)found : (stereoMix ? mixStereoIn[leg] : mixIn[leg]);
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
        case eNodeLevAmp:
        case eNodePassThru:
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

// The RAW connector index of a module type's Nth input, optionally restricted to a connector type.
//
// The engine used to carry hand-written index constants per module — CONNECTOR_IN_A, an "env in" at
// 2, a mixer's legs at {2..9}. Every one of those encodes a fact the resources already state, and
// getting one wrong is invisible: the signal simply never arrives, or arrives from the wrong socket.
// Two such constants were "corrected" in opposite directions in one session before it became clear
// they were describing the same thing badly. Ask the table instead.
//
// `anyConnectorType` means "any". Returns -1 when there is no such input, which callers treat as
// unconnected.
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

// The Voice area's Out module, which is what feeds the FX area. There is no cable for this link —
// the 2-Out's "Out to" setting routes it — so the walk has to make the jump itself when it reaches
// an Fx-In, or the whole FX chain would look like it had nothing patched into it.
// The Voice area Out that feeds a given Fx-In — the one whose "Out to" names the same FX bus the
// Fx-In is listening on.
//
// This used to return the FIRST Out module in the Voice area whatever it was set to, which routed
// signal into the FX area even when the Out was aimed at the speakers and nothing was being sent to
// FX at all. The two selectors have to agree for anything to cross:
//
//   2toOut "Out to":  0 Out 1/2, 1 Out 3/4, 2 FX 1/2, 3 FX 3/4, 4 Bus 1/2, 5 Bus 3/4
//   4toOut "Out to":  0 Out, 1 Fx, 2 Bus            — one setting for all four channels
//   Fx-In  "In from": 0 FX 1/2, 1 FX 3/4
//
// Returns NULL when nothing is feeding that bus, which is correct: an Fx-In listening to a bus
// nobody sends to receives silence.
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

// Adds `module` and everything upstream of it, depth first so a node's inputs always occupy lower
// indices than the node itself — which is what lets the audio thread evaluate the list as a single
// forward pass. Returns the node's index, or -1 if it could not be added.
//
// `depth` bounds the recursion. G2 patches are allowed to contain feedback loops, so without it a
// cycle would recurse until the stack ran out.
static int32_t add_node(tSoundEngineParams * params, tModule * module, uint32_t variation, uint32_t depth) {
    tNodeKind     kind                            = eNodeOsc;
    tEngineNode * node                            = NULL;
    int32_t       self                            = 0;
    // Every leg starts UNCONNECTED. This used to be written {-1, -1, -1, -1}, which supplies only
    // four of the eight and lets C zero-fill the rest — and 0 is not "unconnected", it is node 0,
    // the first node in the chain. A stereo mixer reads all eight legs, so its unpatched channels
    // were quietly summing in whatever node 0 happened to be, usually an oscillator, raw.
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
        uint32_t         count                          = input_connectors(kind, module->type, module->type == moduleTypeMix4to1S, &connectors);
        uint32_t         c                              = 0;
        // COPIED before the loop, because input_connectors() may hand back a pointer to a static
        // buffer and add_node() below recurses into itself for every input — a deeper node's own
        // call would otherwise overwrite this node's list while it is still being walked, leaving
        // every input after the first reading whatever the deepest module happened to want. The
        // chain then differs from one build to the next, and since the engine resets its node state
        // whenever the topology signature changes, the result is envelopes restarting continuously.
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

        // An Fx-In takes no cable: it carries whatever a Voice area Out sends across the FX bus it is
        // listening on. Follow that link explicitly, or a patch whose real output lives in the FX
        // area looks like it has nothing patched into it and plays silence. The bus has to MATCH,
        // though — see voice_area_output_for_fx().
        if (kind == eNodeFxIn) {
            uint32_t  wantedBus = module->param[variation][FXIN_PARAM_SOURCE].value;
            tModule * feeder    = voice_area_output_for_fx(module->key.slot, wantedBus);

            resolvedIn[0]     = (feeder != NULL) ? add_node(params, feeder, variation, depth + 1) : -1;
            resolvedSrcOut[0] = 0;
            inCount           = 1;
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
        }
    }

    switch (kind) {
        case eNodeOscShp:
        {
            // The waveform index is kept RAW: the shape oscillators have their own eight waveforms
            // with their own meanings, and Shape morphs each of them rather than acting as a pulse
            // width. osc_shp_wave() does the work — mapping these onto the plain oscillator's
            // waveforms lost the entire point of the module, since at 50% Shape all four Sine
            // variants ARE a plain sine and everything interesting happens as Shape opens.
            node->wave      = (tOscWave)module->mode[SHPB_MODE_WAVEFORM].value;
            node->oscKbt    = (param_value(module, variation, SHPB_PARAM_KBT) != 0.0);
            node->basePitch = param_value(module, variation, SHPB_PARAM_TUNE)
                              + (osc_fine_cents(param_value(module, variation, SHPB_PARAM_CENT)) / 100.0);
            // RAW, normalised to 0..1 - not the displayed percentage. waveModels.h states the
            // contract ("Shape is the raw 0-127 parameter normalised to 0..1. It is NOT a
            // percentage"), and module_shape_value() in moduleGraphics.c passes param/127 to draw
            // the same wave. Feeding osc_shape_percent()/100 here handed the models 0.5..0.99, so
            // the dial acted over the model's upper half only and raw 0 - the capture's pure sine -
            // came out already half-shaped. Drawn wave and heard wave disagreed, which is the drift
            // waveModels.c exists to make impossible.
            node->shape     = param_value(module, variation, SHPB_PARAM_SHAPE) / 127.0;
            node->modAmount = type_ii_attenuator(param_value(module, variation, SHPB_PARAM_PITCH_MOD) / 127.0);
            node->active    = (param_value(module, variation, SHPB_PARAM_ACTIVE) != 0.0);
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
            // ALL FOUR OF THESE WERE WRONG, and none of it needed the hardware: the instrument's own
            // dial readings settle every one. The note that used to sit here called the curve "an
            // approximation, not a reading of it", which was honest and is now unnecessary.
            //
            //   THRESHOLD  the dial reads raw - 30 dB, and raw 42 reads "Off". This had raw - 42,
            //              putting every setting 12 dB too low, and had no Off at all — so the
            //              compressor was still working where the instrument stops.
            //   RATIO      three straight runs, reaching about 95:1. This was 1 + raw/8, which tops
            //              out at 9.6:1 — a tenth of the range, so the hardest settings barely
            //              compressed.
            //   ATTACK     0.53 ms to 767 ms, and raw 0 is "Fast", i.e. instant. This was
            //              0.1 ms to 300 ms.
            //   RELEASE    125 ms to 10.2 s. This was 10 ms to 3 s.
            //
            // Attack and release are pure exponentials across the dial — fitted to the printed
            // scales, worst error 0.07 dB and 0.04 dB respectively, so the shape is not in doubt.
            double thrRaw = param_value(module, variation, COMP_PARAM_THRESHOLD);
            double att    = param_value(module, variation, COMP_PARAM_ATTACK);
            double rel    = param_value(module, variation, COMP_PARAM_RELEASE) / 127.0;

            if (thrRaw >= COMP_THRESHOLD_OFF) {
                node->threshold = COMP_THRESHOLD_NONE;   // "Off": nothing ever reaches it
            } else {
                node->threshold = pow(10.0, (thrRaw - COMP_THRESHOLD_OFFSET_DB) / 20.0);
            }
            node->ratio        = compressor_ratio(param_value(module, variation, COMP_PARAM_RATIO));

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
            // The range selector is a mode. Its four settings are progressively longer maximum
            // times; the dial then scales within the chosen one.
            // The four range settings, straight off delayABRangeStrMap: 500ms, 1.0s, 2.0s, 2.7s.
            // Three different Range tables exist and the delay modules do not share one — this
            // held only DelayA/DelayB's, so every other delay's Range was read against the wrong
            // list. delay_range_max_seconds() is the single definition, shared with the dial.
            uint32_t range   = module->mode[DELAY_MODE_RANGE].value;
            double   maxTime = delay_range_max_seconds(module->type, range);

            {
                int  clkIndex = delay_time_clk_param_index(module->type);
                bool clocked  = (clkIndex >= 0)
                                && (module->param[variation][clkIndex].value != 0);

                if (clocked == true) {
                    // Clock mode: the dial picks a musical division, not a time. The engine has no
                    // running master clock of its own, so it works to a FIXED 120 BPM reference —
                    // half a second to the beat. That keeps a clocked delay musically proportioned
                    // and the dial honest about which division it selects; it will not agree with a
                    // patch running at some other tempo on the hardware.
                    node->timeSeconds = clk_sync_beats(param_value(module, variation, DELAY_PARAM_TIME))
                                        * (60.0 / ENGINE_REFERENCE_BPM);

                    // The Range still caps it. Manual, Time/Clk scroll button: "if the delay time
                    // (based on the current Master Clock rate and the Sync factor) should exceed the
                    // selected 'Range' time, the actual delay time will automatically be divided by
                    // two." Halving repeatedly is what makes the long divisions land somewhere
                    // musical instead of simply being clamped to the Range — 2/1 is four seconds at
                    // 120 BPM, past every Range there is, so without this the top of the dial was
                    // wrong on every setting.
                    //
                    // The DISPLAY deliberately does not do this: the dial shows the sync factor you
                    // chose, which is what the hardware shows too.
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
            // FEEDBACK IS LINEAR TO EXACTLY UNITY, MEASURED ON THE INSTRUMENT 2026-08-15. This was
            // scaled by 0.95, which is why the engine's repeats died away where the hardware's hold.
            //
            // DelayB, Range 500 ms, LP wide open so the in-loop filter is transparent, decay read off
            // the repeat train of a note-gated click captured from the G2's main outputs:
            //
            //     FB dial      64        96       127
            //     measured   0.4973    0.7413    1.0004
            //     value/127  0.5039    0.7559    1.0000
            //     was (x.95) 0.4787    0.7181    0.9500
            //
            // At 127 the hardware does not decay AT ALL — nine repeats within 0.06 dB of each other,
            // then flat. The old 0.95 turned that infinite sustain into -0.45 dB a repeat, audibly
            // gone inside thirty. The measured values sit ~0.7% under value/127 at the two lower
            // settings, which is the residual loss of the LP even at its widest, not a different law.
            node->depth = param_value(module, variation, DELAY_PARAM_FEEDBACK) / 127.0;
            // LP IS A CUTOFF, AND 127 IS WIDE OPEN. This read the dial as an amount of damping and
            // had it the wrong way round, with a fatal end point: delay_step() uses (1 - damping) as
            // its one-pole coefficient, so LP 127 — the brightest, most ordinary setting there is —
            // gave a coefficient of exactly ZERO. The filter state then never updated, nothing was
            // ever fed back, and the delay produced NO REPEATS AT ALL. Anywhere near the top of the
            // dial it was near enough silent.
            //
            // MEASURED ON THE INSTRUMENT (DelayB, fully wet, FB 96, repeats measured after cutting
            // the oscillator). The dial runs dark-to-bright and the tail lengthens with it:
            //
            //     LP    0     32     64     96    127
            //     tilt  -37.1  -41.4  -23.2  -13.2  -10.7 dB   (2-10 kHz against 80-400 Hz)
            //     tail  0.5    1.6    1.8    2.0    2.0  s
            //
            // So LP 0 is dark and short, LP 127 open and long — the exact opposite of what this did.
            //
            // An exponential sweep of the cutoff fits that: 200 Hz at the bottom of the dial, 20 kHz
            // at the top. Against the measurements above, taking LP 127 as the open reference, it
            // predicts about 28 dB of extra rolloff at LP 0 where 26 was measured, and 8 dB at LP 64
            // where 12.5 was. Close, and the right shape — but the filter sits INSIDE the feedback
            // loop, so what is measured is several passes through it rather than one, and these
            // constants deserve a proper fit before they are called settled.
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
            // MEASURED ON THE HARDWARE 2026-08-09. The Time dial is LINEAR in decay time, with a
            // slope per room type — not the cubic that used to be here, and not reaching anything like
            // the 17.58 s the manual quotes:
            //
            //     room     Time 42   Time 85   Time 127    s per dial unit
            //     Small          -    1.95 s    2.89 s     0.0224
            //     Medium    2.01 s    3.72 s    5.49 s     0.0409
            //     Large     2.94 s    5.49 s    8.11 s     0.0608
            //     Hall      3.77 s    7.30 s   10.75 s     0.0821
            //
            // Straight lines (r2 0.996..0.998) whose slope agrees across both halves of the range to
            // three digits, so the shape is not in doubt. The old cubic gave 17.58 s at Time 127 where
            // Large measures 8.11 s — more than twice too long — and its exponent was openly a guess
            // fitted to make the midpoint musical.
            //
            // THE SLOPE IS NOT PROPORTIONAL TO ROOM SIZE (Hall/Small is 3.67 against a size ratio of
            // 1.68), so Type sets the feedback GAIN as well as the delay lengths. That is why this is a
            // table rather than kReverbTypeScale doing the work.
            //
            // READ AS EARLY DECAY, EXTRAPOLATED. The G2's tail only clears the measurement noise floor
            // by about 21 dB, so each figure is a straight-line fit over ~15 dB stretched to 60. If the
            // instrument has a double-slope tail — a bright early decay over a longer low-frequency one,
            // which reverbs often do — the late part is invisible here and the true RT60 is LONGER than
            // these numbers. That would also explain the manual's 17.58 s, which is not reachable even
            // at Brightness 127 (Hall measures 11.83 s there). Resolving it needs a quieter floor, not
            // a different formula. See the REVERB entry in todo.txt.
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
            uint32_t c      = 0;
            // Read raw, not through param_value(): Curve is a drop-down, and drop-downs cannot be
            // assigned to a morph group (manual p.20), so there is never a morph range on one.
            //
            // expStrMap is {"Exp", "Lin", "dB"} — Lin is the MIDDLE entry, so the test is against 1
            // and not against 0. The manual (p.216) is explicit that Exp and dB are the same curve:
            // "there is no functional difference between the Exp and the dB curves, it is just a
            // matter of whether you want the knobs to display an exact dB value or the basically
            // meaningless Exp value". So both non-Lin settings take the same branch.
            bool     linear = (module->param[variation][(module->type == moduleTypeMix4to1S)
                                                        ? MIXS_PARAM_CURVE : MIX_PARAM_CURVE].value == MIX_CURVE_LIN);

            // The -6 dB Pad attenuates every input together. Mix4to1S has no Pad; only Mix4to1C.
            double   pad    = (  (module->type != moduleTypeMix4to1S)
                              && (module->param[variation][MIX_PARAM_PAD].value != 0)) ? 0.5 : 1.0;

            // Only four channels: the parameters after the level dials are the Channel Mute
            // buttons, not four more levels. Reading all eight as levels was harmless only because
            // nothing downstream used level[4..7] — Mix4to1S has four channels too, its eight legs
            // being stereo pairs that share a level.
            for (c = 0; c < MIX_CHANNELS; c++) {
                double knob    = param_value(module, variation, MIX_PARAM_LEVEL_BASE + c) / 127.0;
                bool   enabled = (module->param[variation][MIX_PARAM_ENABLE_BASE + c].value != 0);

                // Approximation: the exponential taper is squared rather than the hardware's exact
                // "-infinity to 0 dB" attenuator law, which the manual does not state numerically.
                node->level[c] = enabled ? ((linear ? knob : (knob * knob)) * pad) : 0.0;
            }

            break;
        }
        case eNodeOsc:
        {
            double tune      = param_value(module, variation, OSCB_PARAM_TUNE);
            double cent      = param_value(module, variation, OSCB_PARAM_CENT);
            int    pitchType = (int)param_value(module, variation, OSCB_PARAM_PITCH_TYPE);

            // Factor and Partial set the pitch as a ratio against a master oscillator, which the
            // engine has no notion of; reading the dial as Semi at least tracks the knob.
            if (pitchType > 1) {
                LOG_DEBUG("Sound engine: OscB PitchType %d not supported, reading Tune as Semi\n", pitchType);
            }
            node->wave      = (tOscWave)param_value(module, variation, OSCB_PARAM_WAVEFORM);
            node->oscKbt    = (param_value(module, variation, OSCB_PARAM_KBT) != 0.0);
            node->basePitch = tune + (osc_fine_cents(cent) / 100.0);
            node->modAmount = type_ii_attenuator(param_value(module, variation, OSCB_PARAM_PITCH_MOD) / 127.0);
            node->shape     = osc_shape_percent(param_value(module, variation, OSCB_PARAM_SHAPE)) / 100.0;
            node->active    = (param_value(module, variation, OSCB_PARAM_ACTIVE) != 0.0);
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

            // GUARD THESE TWO. They were read unguarded because every filter mapped until now had
            // both; FltStatic has NEITHER an Env input nor a Kbt selector, and -1 cast to uint32_t
            // indexes far off the end of the parameter array. It CRASHED the application rather
            // than misbehaving, which is at least a loud failure - but the -1 convention is only
            // safe where every reader checks it, and two of them did not.
            // FLTNORD IS NOT FLTCLASSIC'S LADDER, and this is where that shows. MEASURED
            // 2026-08-30, both modules through the same rig at an input verified linear:
            //
            //     passband level    Res 0    Res 110
            //     FltClassic        -1.3 dB   -12.7 dB    droops - real ladder feedback
            //     FltNord, GC off   +0.5 dB    +1.6 dB    FLAT; only the peak grows, to +29.7
            //     FltNord, GC on    -1.3 dB   -15.5 dB    GC pulls it down
            //
            // We borrow FltClassic's ladder for FltNord, so our passband droops where the
            // instrument's does not. A four-pole ladder's DC gain is 1/(1 + k), so multiplying by
            // (1 + k) cancels exactly that droop and leaves the flat passband the hardware has;
            // GC's measured attenuation then goes on top. Applying GC WITHOUT the (1 + k) would
            // have counted the droop twice and left FltNord about 29 dB quiet at high resonance.
            //
            // This corrects the LEVEL behaviour. Whether FltNord's peak has the same shape as
            // FltClassic's is a separate question and still open - see Docs/todo.txt.
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
            node->gain    = (param_value(module, variation, OUT_PARAM_PAD) != 0.0) ? 0.5 : 1.0;
            // WHICH PHYSICAL PAIR IT FEEDS. A 2-Out's "Out to" selects Out 1/2 or Out 3/4, and a
            // measurement patch depends on the difference: the rig puts its dry reference on one
            // pair and the processed signal on the other, so summing them would destroy the very
            // comparison it exists to make. A 4-Out has one "Out" setting covering all four
            // channels and is only half-modelled here anyway (eNodeOut carries two legs, not four),
            // so it stays on the first pair.
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

// A chain has to start somewhere. An oscillator is the only thing here that generates a signal from
// nothing, so without one the whole thing renders silence and the menu should say why rather than
// leaving it a mystery — the usual cause is a filter with an empty input.
static bool chain_has_source(const tSoundEngineParams * params) {
    uint32_t i = 0;

    for (i = 0; i < params->nodeCount; i++) {
        if ((params->node[i].kind == eNodeOsc) || (params->node[i].kind == eNodeOscShp)) {
            return true;
        }
    }

    return false;
}

// True when every oscillator feeding the chain is switched off, which is silence for a reason worth
// reporting. A bypassed filter or Out is not counted: those pass through or are the tap itself.
static bool chain_is_bypassed(const tSoundEngineParams * params) {
    uint32_t i = 0;

    for (i = 0; i < params->nodeCount; i++) {
        if (  ((params->node[i].kind == eNodeOsc) || (params->node[i].kind == eNodeOscShp))
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
              ^ ((uint64_t)params->node[i].location << 56)
              ^ ((uint64_t)(uint32_t)(params->node[i].in[0] + 1) << 10)
              ^ ((uint64_t)(uint32_t)(params->node[i].in[1] + 1))
              ^ ((uint64_t)(uint32_t)(params->node[i].in[2] + 1) << 30)
              ^ ((uint64_t)(uint32_t)(params->node[i].in[3] + 1) << 50)
              ^ ((uint64_t)(uint32_t)(params->node[i].in[6] + 1) << 12)
              ^ ((uint64_t)(uint32_t)(params->node[i].in[7] + 1) << 34);
    }

    return sig;
}

// With nothing selected, play the patch: find the Out module that is actually the end of it.
//
// The FX area is searched FIRST and that ordering matters. A patch like this one has an Out in each
// area: the Voice area's is labelled "Fx Out" and routes into the FX area rather than to the
// speakers, and the FX area's is the real end of the chain. Taking the Voice one — which is what
// scanning in area order does — plays the patch dry, with the delays and reverb silently skipped.
// Does this Out module actually reach the speakers, or is it internal routing? Getting this wrong is
// audible in both directions: treat a send as an output and the FX area's input is heard raw
// alongside the finished signal; ignore a real output and the patch is silent.
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
            tModule * module = get_module_slot(gSlot, locations[l], index);

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

// Which nodes are evaluated after the voices are mixed rather than once per voice.
//
// THE DELAY, CHORUS AND REVERB MODULES OWN ONE BUFFER EACH, and a buffer has one write pointer. Run
// one of them once per voice and that pointer advances as many times per sample as there are notes
// held: the delay time divides by the number of voices and the read position sweeps at that multiple
// too, which is heard as the sound stretching and tearing — intermittently, because it only happens
// while more than one note is down. They are therefore evaluated exactly once, on the summed voices,
// which is what the FX Area already does and what the hardware does with the FX Area.
//
// The flag has to spread DOWNSTREAM as well. A module fed by one of these has an input that only
// exists after the mix, so it cannot be evaluated per voice either. add_node() lists every node
// after its own inputs, so one forward pass settles the whole graph.
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
        tModule * glide = get_module_slot(gSlot, (uint32_t)locationMorph, patchModuleGlide);
        tModule * bend  = get_module_slot(gSlot, (uint32_t)locationMorph, patchModuleBend);

        if (glide != NULL) {
            uint32_t mode = glide->param[0][GLIDE_TYPE].value;

            snapshot.glideMode    = (mode <= (uint32_t)eGlideAuto) ? (tGlideMode)mode : eGlideOff;
            snapshot.glideSeconds = glide_time_seconds(glide->param[0][GLIDE_SPEED].value);
        }
        {
            tModule * vibrato = get_module_slot(gSlot, (uint32_t)locationMorph, patchModuleVibrato);

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

    // SOUND COMES FROM THE PATCH'S AUDIBLE OUTPUTS, never from whatever happens to be selected.
    //
    // Auditioning the selected module was useful while the engine could only render a fragment of a
    // patch; now that it resolves the whole thing, a selection quietly changing what you hear is a
    // surprise rather than a feature. It also gave the application and the plug-in two different
    // signal paths from one patch — the plug-in has no selection and always took the outputs — which
    // hid engine faults in whichever path was not being listened to.
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
                            tModule * other = get_module_slot(gSlot, location, index);

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
    snapshot.voiceCount = voice_count_for_patch((uint32_t)gSlot);

    // How many voices the audio thread may allocate. Published separately as well as in the snapshot
    // because the note stack asks the same question from the MIDI thread, where reading the whole
    // snapshot to answer it would be absurd.
    atomic_store(&gEngineVoices, snapshot.voiceCount);

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
    uint32_t attempt = 0;

    for (attempt = 0; attempt < PARAMS_READ_ATTEMPTS; attempt++) {
        uint32_t           before = atomic_load(&gParamsSeq);
        tSoundEngineParams copy;

        if ((before & 1u) != 0u) {
            continue;    // mid-write
        }
        copy = gParams;

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

// OscShpB's eight waveforms. Shape runs 50%..99% and morphs each one — at 50% every waveform in the
// first four is a pure sine, and the character only appears as Shape is opened. Descriptions are
// from the G2 manual (p.176-177); the implementations are ordinary floating point approximations of
// what it describes, not models of the hardware.
//
// `t` below is Shape mapped to 0..1 across that 50%..99% range.
static double osc_shp_wave(uint32_t waveform, double phase, double dt, double shape) {
    // THE LAWS LIVE IN waveModels.c, shared with the wave the editor DRAWS so the two cannot drift
    // apart. They had: everything here once came from the manual's prose, which is wrong in several
    // places, and from a dial mapping that was wrong everywhere — (shape - 0.5)/0.49 clamped at
    // zero, so nothing happened below raw 64 and HALF THE DIAL WAS DEAD. That is fixed and, more to
    // the point, can no longer come back on one side only.
    //
    // What stays here is BAND-LIMITING, which is this file's business and not the drawing's: a step
    // rendered as a step aliases across the whole spectrum, so the four waves that contain one are
    // built from osc_saw/osc_square/osc_triangle, which take dt and limit accordingly.
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

    // Then the high-pass, also in the loop, so each repeat loses more low end than the last — the
    // counterpart to the LP above. Built as a one-pole lowpass subtracted from the signal, which is
    // the cheapest honest one-pole high-pass there is. A coefficient of zero is the dial at 0,
    // where the filter measures flat and is simply switched out.
    if (hpCoeff > 0.0) {
        gDelayHp[line] += hpCoeff * (fed - gDelayHp[line]);
        fed             = fed - gDelayHp[line];
    }
    gDelayLine[line][gDelayWrite[line]] = (float)(input + (fed * feedback));
    gDelayWrite[line]                   = (gDelayWrite[line] + 1) % DELAY_LINE_SAMPLES;

    // DRY/WET IS THE SAME NON-CROSSFADE THE REVERB USES, and this was a plain linear blend. The two
    // gains are independent, each a ramp cubed, and they overlap: dry holds full scale until the
    // knob passes the middle and only then falls, while wet reaches full AT the middle and stays.
    //
    // MEASURED ON THE INSTRUMENT — a saw through a real DelayB with the oscillator cut, so the
    // repeats could be read on their own. The repeat level came out IDENTICAL at DryWet 64 and 127,
    // both -12.2 dB against the dry reference, where a linear crossfade would put 64 a full 6 dB
    // below 127. Total output stayed flat within 0.8 dB across the whole dial.
    //
    // Unlike the reverb, the delay's wet needs NO overall attenuation: fully wet measures -0.3 dB
    // against fully dry, where the reverb needed -11.3. REVERB_WET_GAIN does not belong here.
    {
        double wetRamp = (mix >= 0.5) ? 1.0 : (mix * 2.0);
        double dryRamp = (mix <= 0.5) ? 1.0 : ((1.0 - mix) * 2.0);

        return (input * dryRamp * dryRamp * dryRamp) + (wet * wetRamp * wetRamp * wetRamp);
    }
}

// A short delay whose length is swept by a slow LFO — detune sets the sweep depth, amount how much
// of it is mixed in. Stereo on the hardware; mono here, since the engine sums to mono anyway.
//
// THE STEREO OFFSET IS HALF A CYCLE, measured 2026-08-15 and the one number the stereo chorus was
// waiting on (Docs/todo.txt). The two channels run the SAME algorithm with their LFOs in ANTIPHASE:
// comparing the phase of each channel's amplitude modulation gave R - L = 179.9, 179.6 and 178.9
// degrees across three captures at two tone frequencies and two Detune settings. Not a quarter cycle,
// which was the other candidate.
// Measured on the instrument — see the notes inside chorus_step().
// REMEASURED 2026-08-15 AND RAISED BY A FACTOR OF 3.9. The old 0.853 made the sweep four times
// slower than the instrument's, which is why turning Detune up did so much less here than there.
//
// Method, deliberately different from the null counting that produced the old figure: a steady tone
// through a real StChorus comes out AMPLITUDE modulated, because the comb notch walks across it as
// the delay sweeps, so the modulation rate IS the LFO rate — no disentangling of rate from depth.
// Captured from the G2's main outputs and read three independent ways, all agreeing:
//
//     Detune 32   0.840 Hz        Detune 64   1.680 Hz      exactly 2x for 2x the dial
//
//   - at a 98 Hz tone and again at 16 Hz. The second matters: there the sweep spans only 0.14 of a
//     wavelength, so the modulation CANNOT be a harmonic of the LFO, which is the one way this
//     method could have been read four times too fast.
//   - and by eye off the envelope: minima 1.19 s apart at Detune 32, which is 0.84 Hz.
//
// Proportional to the dial as before, so 127 gives 0.02625 * 127. THE OLD FIGURE IS EXACTLY 1/3.907
// OF THIS AT BOTH SETTINGS, which is close enough to 4 to suggest the null-counting method dropped a
// factor rather than being noisy — its own note says the gaps swell "once per half LFO cycle", and
// reading that as a whole cycle is worth two of the four. That is not explained, only bounded.
//
// RE-ANALYSED FROM THE RETAINED CAPTURES, 2026-08-15, offline and with no G2 present. The three
// files in ~/Documents/G2 Captures/ were demodulated at the tone frequency: for a single input tone
// the module's output is dry + m*delayed, so |z|^2 of the demodulate is a direct read-out of the
// comb argument, and its modulation IS the LFO. What that settled, in order of importance:
//
//   - THE RATE ABOVE IS CONFIRMED. AM fundamentals of 0.8382 Hz (Detune 32) and 1.6795 Hz (Detune
//     64), exactly 2:1, so 0.8382 * 127/32 = 3.327 Hz at Detune 127. An earlier pass in the same
//     session called this 4x too fast; that was a frequency scan capped at 1.2 Hz clipping the real
//     peak in the Detune 64 files, not a fault in the figure.
//   - THE HALF-CYCLE STEREO OFFSET IS CONFIRMED to a fraction of a degree: L/R phase at the AM
//     fundamental of 180.0, 179.9 and 180.1 degrees across the three files.
//   - THE LFO IS A SYMMETRIC TRIANGLE, NOT A SINE. This is the one that was wrong. Where the phase
//     swing is small the folded profile IS the LFO waveform, and the 32.7 Hz captures fold to a
//     triangle — straight flanks, sharp turn — with a fitted rise fraction of exactly 0.50. Fitting
//     P + Q*cos(th0 + X*triangle) to the folded profiles lands at R^2 = 0.9992..1.0000, where every
//     sinusoid-based estimator returned impossible sweeps of 4 to 14 ms against a 3 ms centre.
//   - THE SWEEP IS 2.38 ms, from the 98 Hz capture (both channels agreeing to 0.1%), which is the
//     well-conditioned one: at 32.7 Hz the swing and the mix trade off against each other and those
//     fits are not to be believed. The old 2.1 ms was close, so the frozen-delay cross-check below
//     stands. Both tone frequencies independently put the centre at 2.4..3.9 ms, bracketing
//     CHORUS_CENTRE_S — a consistency check that only passes if the model is right.
#define CHORUS_RATE_MAX_HZ    (3.334)    // at Detune 127; proportional to the dial below that
#define CHORUS_CENTRE_S       (0.0030)   // delay at the middle of the sweep
#define CHORUS_SWEEP_S        (0.00238)  // peak deviation either side, independent of Detune

// A one-shot gate: a rising edge at the input starts it, and it stays high for the width above.
//
// A RISING EDGE WHILE THE GATE IS STILL HIGH RESTARTS IT rather than being ignored. That is what the
// instrument does, and it matters only for an input faster than the width — the measurement patch
// fires one edge per note, so nothing there depends on it.
//
// TimeMod is NOT implemented: the module has a modulation input for its width and this ignores it,
// which is honest rather than inventing a law for it. Nothing measured so far uses it.
static double pulse_step(uint32_t voice, uint32_t node, double input, const tEngineNode * spec) {
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

// The LFO shape: a symmetric triangle in [-1, 1], phase in [0, 1). Measured, not assumed — see above.
//
// IT IS THE SHAPE, NOT THE DEPTH, THAT MAKES THIS SOUND LIKE A CHORUS. Pitch shift through a swept
// delay is the sweep VELOCITY, so a triangle gives a CONSTANT detune that flips sign twice a cycle
// — two steady pitches alternating, which is what doubling is — where a sine glides smoothly
// through zero to a peak and back, which is the textbook definition of vibrato. With a sine here,
// Amount 127 came out sounding like a slow vibrato rather than a chorus.
static double chorus_triangle(double phase) {
    double p = phase - floor(phase);

    return (p < 0.5) ? (-1.0 + (4.0 * p)) : (3.0 - (4.0 * p));
}

// ONE CHANNEL of the sweep, read at the LFO phase it is given. The two channels differ ONLY in that
// phase, which is why this is one function called twice rather than two structures — measured, see
// the antiphase note above chorus_step().
static double chorus_tap(uint32_t node, uint32_t ch, double input, double phase, double amount) {
    double   sweep   = 0.0;
    uint32_t samples = 0;
    uint32_t readPos = 0;
    double   wet     = 0.0;

    // The DEPTH is fixed; only the rate follows the dial. The shape is a TRIANGLE — measured, and
    // the difference between a chorus and a vibrato; see chorus_triangle().
    sweep                                         = CHORUS_CENTRE_S + (CHORUS_SWEEP_S * chorus_triangle(phase));
    samples                                       = (uint32_t)(sweep * gSampleRate);

    if (samples < 1) {
        samples = 1;
    } else if (samples >= CHORUS_SAMPLES) {
        samples = CHORUS_SAMPLES - 1;
    }
    readPos                                       = (gChorusWrite[node][ch] + CHORUS_SAMPLES - samples) % CHORUS_SAMPLES;
    wet                                           = (double)gChorusLine[node][ch][readPos];

    gChorusLine[node][ch][gChorusWrite[node][ch]] = (float)input;
    gChorusWrite[node][ch]                        = (gChorusWrite[node][ch] + 1) % CHORUS_SAMPLES;

    // A CONSTANT-POWER BLEND whose wet/dry ratio IS the dial, measured on the instrument.
    //
    // Setting Detune to zero makes the delay static, which turns the module into a plain comb
    // filter — and the depth of a comb's notches is a direct read-out of the dry/wet balance, since
    // equal parts cancel completely. Sweeping Amount on a real StChorus:
    //
    //     Amount        0     32     64     96    127
    //     wet/dry    0.02   0.28   0.64   0.94   0.91      (from notch depth)
    //     total      -0.0   -0.2   -0.0   +0.4   +1.1 dB
    //
    // So the ratio tracks the dial roughly one for one and the total stays flat. Both matter: this
    // used to be dry (1 - amount/2) against wet (amount/2), which gives a ratio of only 0.33 at the
    // middle of the dial where 0.64 was measured — half the chorus it should have been — and loses
    // 3 dB of level at the top where the instrument holds steady.
    //
    // Dividing by sqrt(1 + m^2) is what keeps the sum constant: at full Amount both legs sit at
    // 0.707 rather than both at 0.5.
    {
        double m     = amount;
        double scale = 1.0 / sqrt(1.0 + (m * m));

        return (input * scale) + (wet * m * scale);
    }
}

// STEREO, from one LFO: the right channel reads it HALF A CYCLE along. Measured 2026-08-15 and
// re-confirmed from the retained captures the same day — L/R phase at the AM fundamental of 180.0,
// 179.9 and 180.1 degrees across three files, so antiphase and not the quarter cycle that was the
// other candidate.
static void chorus_step(uint32_t node, double input, double depth, double amount,
                        double * outLeft, double * outRight) {
    double phase = gChorusLfo[node];

    // DETUNE SETS THE RATE, NOT THE DEPTH — this had it the other way round, with the rate fixed at
    // 0.7 Hz and the sweep scaled by the dial.
    //
    // MEASURED with a pure 1976 Hz tone through a real StChorus. Dry and wet beat against each other
    // as the delay moves, and one null is exactly one wavelength of delay change, so counting nulls
    // measures the sweep VELOCITY outright. The gaps between nulls swell and shrink once per half
    // LFO cycle, which separates rate from depth:
    //
    //     Detune 32   rate 0.215 Hz   depth 2.11 ms      Detune 0   no nulls at all: static
    //     Detune 64   rate 0.430 Hz   depth 2.04 ms
    //
    // Exactly twice the rate for twice the dial, at constant depth. Above about 96 the nulls come
    // too close to separate the two, so the top of the range is extrapolated from that proportion.
    // (The RATES in that table are the ones later found to be 3.907x low; the DEPTHS survived the
    // 2026-08-15 re-analysis nearly unchanged, at 2.38 ms.)
    //
    // CROSS-CHECKED against a quite different measurement: at Detune 0 the LFO stops wherever it
    // happens to be, and rebuilding the patch repeatedly froze the delay at 1.33, 1.52, 1.94, 2.13
    // and 5.33 ms. A 3 ms centre swept +/-2.38 ms spans 0.6 to 5.4 ms, and every one of those frozen
    // values falls inside it.
    // BOTH TAPS READ THE PHASE BEFORE IT ADVANCES, so the two channels are sampled at the same
    // instant rather than one being a sample ahead of the other.
    *outLeft          = chorus_tap(node, 0, input, phase, amount);
    *outRight         = chorus_tap(node, 1, input, phase + 0.5, amount);

    gChorusLfo[node] += (CHORUS_RATE_MAX_HZ * depth) / gSampleRate;

    if (gChorusLfo[node] >= 1.0) {
        gChorusLfo[node] -= 1.0;
    }
}

// Peak-following compressor. Above the threshold the excess is divided by the ratio; the follower
// has separate attack and release so it grabs quickly and lets go slowly.
static double compress_step(uint32_t voice, uint32_t node, double input, const tEngineNode * spec) {
    double level = fabs(input);
    double gain  = 1.0;

    if (level > gCompEnv[voice][node]) {
        gCompEnv[voice][node] += spec->attackCoeff * (level - gCompEnv[voice][node]);
    } else {
        gCompEnv[voice][node] += spec->releaseCoeff * (level - gCompEnv[voice][node]);
    }

    if ((gCompEnv[voice][node] > spec->threshold) && (spec->threshold > 0.0)) {
        double over = gCompEnv[voice][node] / spec->threshold;

        gain = pow(over, (1.0 / spec->ratio) - 1.0);
    }
    return input * gain;
}

// Schroeder reverb — parallel combs for density, allpasses to smear the result.
//
// brightness is the dial as it reads: HIGH IS BRIGHT. It used to be handed straight to the damping
// filter's coefficient, which inverted it — a knob labelled Brightness made the tail darker as it
// opened, and the manual's advice that "the most natural range is between 25 and 50" (p.251) landed
// on the dullest part of the travel instead of the liveliest.
static void reverb_step(double input, double timeSeconds, double timeNorm, double brightness,
                        double mix, uint32_t type, double * outLeft, double * outRight) {
    double   sum[REVERB_CHANNELS] = {0.0, 0.0};
    uint32_t ch                   = 0;
    uint32_t i                    = 0;
    double   lfo[RV_LINES];
    // A one-pole lowpass inside each comb, so every pass round the loop loses more high end — which
    // is what makes a tail decay into a thump rather than ringing on with the same tone.
    //
    // THE DIAL DRIVES THE COEFFICIENT THROUGH A CURVE, and it has to. Taken linearly — which is what
    // this was — the filter is savage over most of the travel: measured on an offline render of this
    // very code (tools/render), the decay reached 25% of its requested length at Brightness 32, 41% at
    // 64 and 53% at 96, only arriving at 100% when Brightness 127 switches the filter off altogether.
    // The instrument does not behave remotely like that: at Brightness 64 it decays for 10.76 s against
    // 11.83 s at 127, i.e. 91%, so most of the dial is nearly transparent to the DECAY while still
    // moving the tail's colour (its 6-20 kHz band gains about 5 dB from 64 to 127).
    //
    // Note this was NOT a feedback-gain error, which is where I first looked: the one-pole has unity DC
    // gain, so `fb` sets the low-frequency decay exactly right, and the render proves it by hitting the
    // requested time to within 0.01 s once the filter is out of the loop. What was wrong is how much
    // filter a given dial position asks for.
    //
    // The dial drives the coefficient through a curve — see REVERB_DAMP_MAX
    // for the numbers, and for why the obvious ways of scoring this are misleading. Taken linearly, as
    // this was, the loop damped high frequency about twice as fast as the instrument does at any given
    // dial position.
    //
    // TWO THINGS THIS IS NOT, both of which I diagnosed wrongly before measuring properly. It is not a
    // feedback-gain error: the one-pole has unity DC gain, so `fb` sets the low-frequency decay exactly
    // right, and an offline render confirms the requested time to within 0.01 s once the filter is out
    // of the loop. And the instrument's damping is not a fixed loss dressed up as a dial — its tail
    // demonstrably darkens as it decays, by 14-16 dB over three seconds at mid dial, which only
    // something inside the loop can do.
    // BRIGHTNESS 64 IS THE NEUTRAL DETENT, and the tail there is neither darkened nor lifted. That
    // is measured: at Type 3, Time 122, Brightness 64 the instrument's own tail decays at -5.47,
    // -5.35, -6.15 and -5.83 dB/s in the 125 Hz, 500 Hz, 2 kHz and 6 kHz bands — flat to within
    // 0.8 dB/s across six octaves, so nothing is being taken out of one end.
    //
    // This used to read 1.0 - bright^0.7, which puts a coefficient of 0.384 at the detent. A
    // one-pole that deep loses 3.6 dB of broadband energy EVERY TIME the signal passes it, twice
    // per trip round the tank, and that loss was most of why a Hall decayed in four seconds where
    // the instrument takes eleven. A damping control has to pass its neutral position through
    // untouched or it is a loss dressed up as a tone control.
    //
    // WHAT THE TWO HALVES DO IS NOT MEASURED. Both captures in hand hold Brightness fixed, so there
    // is no sweep to fit and the shape below is a symmetric guess about a measured centre: the
    // lower half damps the top with a one-pole, the upper half damps the bottom by the same law.
    // The centre is right; the ends need a Brightness sweep off the hardware before either
    // REVERB_DAMP_MAX or REVERB_BRIGHT_CURVE means anything.
    double          tilt      = (brightness - 0.5) * 2.0;
    double          dampLo    = (tilt < 0.0) ? (REVERB_DAMP_MAX * pow(-tilt, REVERB_BRIGHT_CURVE)) : 0.0;
    double          dampHi    = (tilt > 0.0) ? (REVERB_DAMP_MAX * pow(tilt, REVERB_BRIGHT_CURVE)) : 0.0;
    double          scale     = kReverbTypeScale[(type < REVERB_TYPE_COUNT) ? type : 0];

    // ONE TRIP ROUND THE LOOP, and the gain that costs. The sections either side of it are
    // lossless, so this single number is the whole decay: 60 dB in the requested time, three
    // decades over however many trips fit into it.
    double          gRvGain[RV_LINES];
    static uint32_t sLastType = REVERB_TYPE_COUNT;   // forces the reset below on the first call


    // Changing type resizes every delay line, so the positions into them are meaningless and the
    // contents are a room that no longer exists. Cleared rather than carried over — which is also
    // what the instrument does: "changing reverb type will force the Sound Engine to recalculate and
    // thus cause a brief moment of silence" (p.251).
    if (type != sLastType) {
        memset(gPreDelay, 0, sizeof(gPreDelay));
        memset(gRvMem, 0, sizeof(gRvMem));
        memset(gRvCur, 0, sizeof(gRvCur));
        memset(gRvDamp, 0, sizeof(gRvDamp));
        memset(gRvLow, 0, sizeof(gRvLow));
        memset(gRvLfo, 0, sizeof(gRvLfo));
        memset(gRevInLp, 0, sizeof(gRevInLp));
        memset(gRevInLp2, 0, sizeof(gRevInLp2));
        memset(gRevInLp3, 0, sizeof(gRevInLp3));
        memset(gRevInLp4, 0, sizeof(gRevInLp4));
        memset(gRvLoop, 0, sizeof(gRvLoop));
        memset(gPreDelayPos, 0, sizeof(gPreDelayPos));
        sLastType  = type;

        // Lay the spans out end to end. Each one starts where the last finished, so a section
        // writing at its own base and reading at the next gets exactly its own length of delay and
        // no two sections can ever share a cell.
        gRvAddr[0] = 16;

        for (i = 0; i < eRvSpanCount; i++) {
            // THE PRE-DELAY IS MEASURED, NOT SCALED. Every other span is a length recovered from
            // the instrument's tap spacing and grows with the room; this one was read off the
            // hardware per room, and barely moves between them, for the reason above. The left
            // channel's figure sets the layout and REVERB_SPREAD carries the right.
            // THE TANK HAS ITS OWN LEAD-IN and the pre-delay has to give it back. The earliest
            // wet sample cannot leave before the first output tap, which sits 13% along the first
            // long line; an allpass passes its input straight through, so nothing in front of that
            // tap delays anything. Measured onset is input-to-first-tap, so the span in front of
            // the tank is the measurement MINUS that lead-in, or every room lands 7.2 ms late.
            //
            // It also fixes the scaling. The lead-in grows with the room and the measurement does
            // not, so subtracting one from the other leaves a span that shrinks as the room grows
            // — which is what keeps the onset near-constant across rooms, the way the hardware's is.
            // THE TANK'S OWN LEAD-IN, per room, in samples at 96 kHz. The measured pre-delay is
            // input-to-first-wet-sample, so whatever the tank puts in front of its taps has to come
            // off the span ahead of them.
            //
            // THIS IS A TABLE BECAUSE IT IS NOT DERIVABLE, and pretending otherwise put the Small
            // room 0.49 ms late. Computing it as the median tap offset assumes the lead-in scales
            // with the room exactly as every span does; solving for what it would have to be to hit
            // the measured onsets gives 485.6 * scale + 110.4, i.e. a term that does NOT scale. The
            // input diffuser's allpasses each pass a fraction of their input straight through, so
            // the first arrival is a mixture of paths that scale and paths that partly do not, and
            // no single length stands in for it. Measured against the onsets in kReverbPreDelay,
            // which is the same kind of table for the same kind of reason.
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

    // Diffusion first: three short allpasses smear the input within a few milliseconds, so there is
    // something there before the combs respond and no single tap stands out as an echo.
    //
    // The coefficient RISES WITH THE REVERB TIME rather than sitting at a fixed 0.5, and both the
    // slope and the two limits it is held between are the instrument's own: a longer room diffuses
    // harder. The bounds are what matter most here — a coefficient outside them stops sounding like
    // this reverb — and they are narrow enough that the exact position within them is a detail.
    // ONE DECAY GAIN PER LINE. A line of L samples is traversed fs/L times a second, so losing 60 dB
    // in `timeSeconds` means losing 3 decades per timeSeconds, i.e. this per trip. The lines are
    // different lengths, so their gains differ; the Householder mix is orthogonal and takes nothing
    // out, which is what lets a closed form like this set the decay exactly with no trim fitted to a
    // render.
    for (i = 0; i < RV_LINES; i++) {
        // THE LINE ALONE, not the allpass in front of it. An allpass passes a fraction of its input
        // straight through -- that is what the -g feedforward term is -- so only some of the energy
        // ever takes its delay, and charging the decay for the whole of it ran a Hall 30% fast.
        double len = (double)(gRvAddr[kRvLineDl[i] + 1] - gRvAddr[kRvLineDl[i]]);

        gRvGain[i] = (timeSeconds > 0.01)
                     ? (pow(10.0, (-3.0 * len) / (gSampleRate * timeSeconds)) / RV_MOD_LOSS)
                     : 0.0;
    }

    // ONE BANK PER CHANNEL. The two run the same structure and decorrelate through their tap
    // phases, which is what the instrument does — its own outputs correlate at only +0.0044.
    // THE SWEEP PHASES, read once and used by both channels. The right channel runs a quarter cycle
    // behind, so the two never move their modes the same way at the same moment -- one more thing
    // keeping them uncorrelated, on top of the tap offset.
    for (i = 0; i < RV_LINES; i++) {
        lfo[i]    = gRvLfo[i];
        gRvLfo[i] = gRvLfo[i] + (kRvModHz[i] / gSampleRate);

        if (gRvLfo[i] >= 1.0) {
            gRvLfo[i] -= 1.0;
        }
    }

    for (ch = 0; ch < REVERB_CHANNELS; ch++) {
        uint32_t spread   = (ch == 0) ? 0 : REVERB_SPREAD;
        double   diffused = input;

        // THE PRE-DELAY IS A SPAN OF THE TANK'S OWN MEMORY, the first one, and it does not
        // scale quite like the rest: the instrument's addresses are roomSize * k + 1200 and that
        // 1200 is shared by every site, so the distance from the input to the first tap barely
        // moves between rooms. That is the explanation for a measurement taken long before there
        // was a structure to explain it — 12.89 ms in the Small room against 13.36 in the Hall,
        // while every line inside the room gets 68% longer.

        // ── THE TANK ──────────────────────────────────────────────────────────────────────────
        //
        // ONE BUFFER PER CHANNEL, A CURSOR THAT WALKS BACKWARDS, AND A LAYOUT OF NON-OVERLAPPING
        // SPANS. Nothing here computes a delay: a value written at address W reappears at address
        // W + L exactly L samples later, so each span IS its line and the two cannot drift apart.
        // Sections are visited in increasing address order and each reads before it writes, so the
        // read returns that section's own output from L samples ago rather than its neighbour's.
        //
        // FOUR ALLPASS SECTIONS, NOT TWELVE. The instrument's recovered mixing gains settle this
        // exactly: they pair 0.4820 with 0.7676 and 0.3102 with 0.9038, and 1 - g*g for those two
        // g values is 0.7677 and 0.9038. That identity is the allpass, written out — a section
        // takes its delayed content d and its input x to (g*d + x, (1 - g*g)*d - g*x) — so the
        // gain table names four of them and their coefficients, and nothing is being guessed here.
        //
        // An earlier build read the same site list as ONE serial chain of twelve allpasses. That is
        // what a chain of allpasses does to an impulse: at four trips a second it put fifty-odd
        // passes into every second of tail and turned the whole thing to noise. Four is the number
        // the gains support and the number a tank of this kind wants.
        //
        // THE LINES RUN IN PARALLEL AND MIX INTO EACH OTHER. The input reaches all four, and the
        // Householder reflection below sends each line's output into all four on the next pass, so
        // there is no single path back to the start and so no one period for the tail to ring at.
        {
            double   v      = diffused;
            uint32_t modMax = (uint32_t)(RV_MOD_DEPTH * RV_RATE);
            double   phOff  = (ch == 0) ? 0.0 : 0.25;
            uint32_t i      = 0;
            double   tapSum = 0.0;
            double   line[RV_LINES];

#define RVR(a)       ((double)gRvMem[ch][(gRvCur[ch] + (a)) & (RV_MEM - 1)])
#define RVW(a, x)    (gRvMem[ch][(gRvCur[ch] + (a)) & (RV_MEM - 1)] = (float)(x))

            // A plain line: hand `v` in, get it back L samples later.
#define RVDLY(n)                         \
   do {                                  \
       double d = RVR(gRvAddr[(n) + 1]); \
       RVW(gRvAddr[n], v);               \
       v = d;                            \
   }                                     \
   while (0)

            // An allpass section, the form the recovered gains describe.
            // A MODULATED LINE. The read position sweeps across the slack at the end of the span,
            // interpolating between the two samples it falls between -- without that the delay would
            // step a whole sample at a time and the steps would be heard as clicks.
#define RVDLYM(n, off)                                            \
   do {                                                           \
       double   rd = (double)(gRvAddr[(n) + 1] - modMax) + (off); \
       uint32_t ri = (uint32_t)rd;                                \
       double   fr = rd - (double)ri;                             \
       double   d  = (RVR(ri) * (1.0 - fr)) + (RVR(ri + 1) * fr); \
       RVW(gRvAddr[n], v);                                        \
       v = d;                                                     \
   } while (0)

#define RVAP(n, g)                       \
   do {                                  \
       double d = RVR(gRvAddr[(n) + 1]); \
       double w = v + ((g) * d);         \
       RVW(gRvAddr[n], w);               \
       v = d - ((g) * w);                \
   } while (0)

            // BAND-LIMIT THE FEED. The instrument's reverb is MUCH darker than its input, and this
            // is where that comes from. Measured as the wet energy per band against the dry impulse
            // in the same capture -- which divides the excitation out, so a hardware pulse and a
            // unit-sample render compare directly -- it runs -1.5 dB at 2 kHz, -5.9 at 4 kHz, -15.4
            // at 8 kHz and -26.1 at 16 kHz, all relative to 1 kHz. Two poles at 3.5 kHz land within
            // 0.5 dB of that across the whole range.
            //
            // IT IS A FIXED FILTER, NOT IN-LOOP DAMPING, and the tail says which: the instrument's
            // 6 kHz band decays only about 1 dB/s faster than its 125 Hz band, nowhere near enough
            // to account for a 15 dB deficit at 8 kHz. Something the signal passes ONCE takes that
            // out, so it belongs here in front of the tank and not inside it.
            //
            // Leaving it out is what made the tank sound metallic: dead flat to 16 kHz, 26 dB of
            // treble the instrument does not have.
            {
                double a1 = exp(-2.0 * M_PI * REVERB_INPUT_LP_HZ / gSampleRate);
                double a2 = exp(-2.0 * M_PI * REVERB_INPUT_LP2_HZ / gSampleRate);

                gRevInLp[ch]  = ((1.0 - a1) * v) + (a1 * gRevInLp[ch]);
                double a3 = REVERB_INPUT_LP_TIME * timeNorm;

                gRevInLp2[ch] = ((1.0 - a2) * gRevInLp[ch]) + (a2 * gRevInLp2[ch]);
                double a4 = exp(-2.0 * M_PI * REVERB_INPUT_LP4_HZ / gSampleRate);

                gRevInLp3[ch] = ((1.0 - a3) * gRevInLp2[ch]) + (a3 * gRevInLp3[ch]);
                gRevInLp4[ch] = ((1.0 - a4) * gRevInLp3[ch]) + (a4 * gRevInLp4[ch]);
                v             = gRevInLp4[ch];
            }

            // The input stage: pre-delay, then four short allpasses that smear the attack before
            // the tank ever sees it, so no single early tap stands out as an echo.
            RVDLY(eRvPre);

            for (i = 0; i < RV_DIFFUSERS; i++) {
                RVAP(kRvDiffuser[i], RV_DIFFUSE);
            }

            diffused = v;

            // THE FOUR LINES. Each gets the input with its own sign and its own share of the
            // previous sample's mix. Injecting in phase into every line drives the tank's common
            // mode -- the one where all four hold the same thing -- and that mode has a period of
            // its own, so it beats. In phase it put a 12.2 dB lobe at 6.8 Hz into the tail.
            for (i = 0; i < RV_LINES; i++) {
                v              = ((i & 1) ? -diffused : diffused) + gRvLoop[ch][i];

                RVDLYM(kRvLineDl[i], (0.5 - (0.5 * cos(2.0 * M_PI * (lfo[i] + phOff)))) * (double)modMax);

                // Brightness, one filter per line and inside the loop, so it accumulates with every
                // pass rather than colouring the output once on the way out.
                gRvDamp[ch][i] = ((1.0 - dampLo) * v) + (dampLo * gRvDamp[ch][i]);
                gRvLow[ch][i]  = ((1.0 - RV_LOW_A) * gRvDamp[ch][i]) + (RV_LOW_A * gRvLow[ch][i]);
                line[i]        = gRvDamp[ch][i] - (dampHi * gRvLow[ch][i]);
            }

            // THE MIXING MATRIX, a 4-point Hadamard as two butterfly stages. Orthogonal, so it moves
            // energy between the lines without creating or destroying any -- which is what lets the
            // decay below be a closed form rather than a figure trimmed against a render.
            //
            // EVERY LINE REACHES EVERY OTHER LINE ON EVERY PASS. That is what stops each one being a
            // comb in its own right: an echo entering one line leaves spread across all four, is
            // spread again a few milliseconds later, and the echo count squares instead of
            // repeating. Without it, four parallel lines are just four combs.
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
                gRvLoop[ch][0] = (b0 + b1) * RV_HADAMARD * gRvGain[0];
                gRvLoop[ch][1] = (b0 - b1) * RV_HADAMARD * gRvGain[1];
                gRvLoop[ch][2] = (b2 + b3) * RV_HADAMARD * gRvGain[2];
                gRvLoop[ch][3] = (b2 - b3) * RV_HADAMARD * gRvGain[3];
                gRvLoop[ch][4] = (b4 + b5) * RV_HADAMARD * gRvGain[4];
                gRvLoop[ch][5] = (b4 - b5) * RV_HADAMARD * gRvGain[5];
                gRvLoop[ch][6] = (b6 + b7) * RV_HADAMARD * gRvGain[6];
                gRvLoop[ch][7] = (b6 - b7) * RV_HADAMARD * gRvGain[7];
            }

            // THE OUTPUT TAPS read INSIDE the four lines, never at a section's own write address.
            // Every cell in this buffer holds delay state, and the state at a write address is a
            // section's input side -- broadband by construction, and sixteen of those summed is
            // white noise, which is exactly what an earlier build sounded like. A tap part-way
            // along a line is the circulating signal at that point of its trip, which is what a
            // reverb output is made of.
            //
            // ALTERNATING SIGNS, and the right channel reads the same fractions of the same lines
            // a little earlier -- see REVERB_SPREAD for why earlier and not later.
            for (i = 0; i < RV_OUTTAPS; i++) {
                uint32_t n   = kRvTapLine[i];
                uint32_t len = gRvAddr[n + 1] - gRvAddr[n];
                uint32_t off = (uint32_t)(kRvTapFrac[i] * (double)len);
                uint32_t at  = gRvAddr[n] + ((off > spread) ? (off - spread) : 0u);

                tapSum += (i & 1) ? -RVR(at) : RVR(at);
            }

            sum[ch]    = tapSum * RV_TAP_SCALE;
            gRvCur[ch] = (gRvCur[ch] - 1u) & (RV_MEM - 1);

#undef RVAP
#undef RVDLYM
#undef RVDLY
#undef RVR
#undef RVW
        }
    }

    // THE WET PATH IS QUIETER THAN THE DRY ONE, by about 11 dB, and this engine had it at almost
    // unity — which is why its reverb sat so much more prominently in a patch than the instrument's
    // does at the same settings.
    //
    // MEASURED BOTH SIDES THE SAME WAY, at the module's own defaults (Type 0, Time 64, Bright 64):
    //
    //   the instrument   full wet is 11.3 dB below full dry. A saw was fed through a real Reverb and
    //                    the oscillator cut mid-recording, so the tail could be measured on its own;
    //                    the DryWet dial was then swept and the steady output read at each step.
    //   this engine      the wet impulse response carries -1.1 dB of energy against the impulse that
    //                    produced it (tools/render, sqrt(sum h^2)), i.e. 10.2 dB too much.
    //
    // The dial's SHAPE was already right and is unchanged — sweeping DryWet on the instrument gives
    // 0.0 / 0.0 / +0.3 / -10.0 / -11.3 dB at 0/32/64/96/127, which the ramps below reproduce to
    // within 0.6 dB once this scale is applied. It was only ever the wet level that was wrong.
    //
    // RE-MEASURE IF THE COMB SET OR THEIR COUNT CHANGES: this is the sum of REVERB_COMBS parallel
    // combs, so its level moves with how many there are.
// RE-DERIVED THREE TIMES ON 2026-08-18. Twice because the comb count went 4 -> 8 -> 16, which is
// exactly what the warning above this line is for — this scales the SUM of REVERB_COMBS parallel
// combs, so it moves with how many there are, and each doubling added close to the 3 dB an incoherent
// sum predicts (2.96 then 3.17). Once more, and much larger, because REVERB_INPUT_LP_HZ then took
// 11.9 dB of high-frequency energy out of the tank: 0.31 -> 0.2205 -> 0.1531 -> 0.6028.
// The last one looks alarming beside the others and is not: the wet path is simply much quieter
// before this gain now that it is band-limited, and the figure it has to land on is unchanged.
//
// AND THE TARGET IS NO LONGER SECOND-HAND: -11.5 dB was measured on the instrument on 2026-08-18,
// wet-to-dry energy through the impulse rig, agreeing with the -11.3 dB the old figure came from.
// Re-derived for the tank. The figure below it is measured the same way it always was -- the wet
// impulse response's energy against the impulse that produced it, sqrt(sum h*h) from tools/render
// -- and the tank summed its taps 14.05 dB hotter than the comb bank it replaced, so this is the
// old 1.0695 scaled to put full wet back on the instrument's -11.3 dB.
#define REVERB_WET_GAIN    (0.3956)

    sum[0] *= REVERB_WET_GAIN;
    sum[1] *= REVERB_WET_GAIN;

    // DRY/WET IS NOT A CROSSFADE, and this was the largest single difference from the instrument.
    // The two gains are independent, each a ramp CUBED, and the ramps overlap: the dry side holds
    // full scale until the knob passes the middle and only then falls, while the wet side reaches
    // full scale AT the middle and stays there. So the centre detent is both signals at full, not
    // half of each — which is why the hardware's reverb at a middle setting is so much wetter, and
    // louder, than a linear blend of the same two signals.
    //
    // The cube makes the taper steep at the quiet end: a quarter-open knob passes an eighth of the
    // wet signal, where a linear reading would pass a quarter.
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

// Renders the Reverb's impulse response on its own — no patch, no voice, no audio device.
//
// WHY THE ENGINE HAS A MEASUREMENT ENTRY POINT. The room sizes and the decay law above came from
// putting a click through the real instrument and measuring what came back. The same click can go
// through this code, and then the two sit in the same units and the same analysis: lag sets, decay
// time, and how alike the two output channels are. That turns "does it sound like the G2" into a diff,
// which is the only way the remaining work — the delay lengths and the topology — can converge instead
// of being tuned by ear against a memory of the hardware.
//
// The click is one sample at full scale, not the ~13-sample band-limited pulse the instrument's
// converters produce. It does not need to match: the lengths are recovered from how the TAIL correlates
// with itself, which the excitation's shape does not enter.
//
// `out` receives `frames` interleaved stereo pairs at the ENGINE's rate, which is
// deviceRate * ENGINE_OVERSAMPLE — pass 48000 to get the 96 kHz the hardware measurements are
// expressed in, so a lag is the same integer in both.
//
// THE TWO CHANNELS ARE NOW A REAL PAIR, and this is where REVERB_SPREAD gets tuned: render, then read
// the L/R correlation off the result and compare it with the instrument's measured +0.012..+0.045.
// It used to be the standing example of what the harness could see and the engine could not do —
// both channels came back identical and the correlation read 1.000.
void sound_engine_render_reverb_ir(double deviceRate, uint32_t type, uint32_t timeValue,
                                   uint32_t brightValue, float * out, uint32_t frames) {
    if ((out == NULL) || (frames == 0) || (deviceRate <= 0.0)) {
        return;
    }

    if (type >= REVERB_TYPE_COUNT) {
        type = 0;
    }
    gSampleRate = deviceRate * (double)ENGINE_OVERSAMPLE;

    // Cleared explicitly rather than relying on reverb_step()'s own type-change reset: a second render
    // at the SAME type in one process would otherwise start inside the first one's tail, and the
    // resulting lag set would be a mixture of two rooms — the identical trap the hardware captures hit
    // when settings were grouped by counting.
    memset(gPreDelay, 0, sizeof(gPreDelay));
    memset(gRvMem, 0, sizeof(gRvMem));
    memset(gRvCur, 0, sizeof(gRvCur));
    memset(gRvDamp, 0, sizeof(gRvDamp));
    memset(gRvLow, 0, sizeof(gRvLow));
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

static double advance_phase(double * phase, double dt) {
    double current = *phase + dt;

    while (current >= 1.0) {
        current -= 1.0;
    }
    *phase = current;
    return current;
}

// A cascade of one-pole lowpasses with the last stage fed back to the input — the usual ladder
// arrangement, which is what gives a resonant peak at the cutoff and the gentle saturation the
// classic filters are liked for. Two stages is 12 dB/octave, three 18, four 24, matching the dB
// scroll button.
// Smooth saturation for a ladder stage: y = x - x^3/3, the first two terms of tanh's series, held
// flat outside +/-1 where the cubic would turn back on itself. Unity slope at the origin, so a quiet
// signal passes through untouched and only a driven one is shaped.
// One-pole move toward a target. Snapping when unprimed is what keeps a patch load instant.
static double smooth_to(double * current, double target, double coeff, bool primed) {
    if (primed == false) {
        *current = target;
    } else {
        *current += coeff * (target - *current);
    }
    return *current;
}

// LINEAR BELOW THE KNEE, saturating above it. The knee matters as much as the curve: a nonlinearity
// that acts on every sample generates harmonics on every sample, and this filter runs at the output
// rate with no oversampling, so anything it makes above Nyquist folds back down. With the cutoff up
// near Nyquist and the resonant feedback amplifying those products before they fold, a plain
// x - x^3/3 — only 0.2% away from linear at these levels — was enough to put audible rasp roughly
// 30 dB below the note. The hardware does not have this problem because it runs at 96 kHz.
//
// So below LADDER_KNEE the response is exactly linear and generates nothing at all; above it the
// curve approaches 1 exponentially, with unity slope at the knee so there is no corner to radiate
// harmonics of its own. A driven filter still compresses; an ordinary one is untouched.
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

// FltHP: N ONE-POLE HIGH-PASSES IN SERIES, measured 2026-08-30 - the slope mode is literally the
// pole count, 1 to 6, and every pole sits at the dial's own corner. Each stage is the complement of
// the one-pole low-pass the ladder uses, so the same state array serves both and a node is only ever
// one topology.
static double cascade_hp_filter(double * state, double input, double g, uint32_t poles) {
    double   x = input;
    uint32_t i = 0;

    for (i = 0; i < poles; i++) {
        state[i] += g * (x - state[i]);   // the low-pass part of this stage
        x         = x - state[i];         // and the high-pass is what is left
    }

    return x;
}

// FltStatic: A PLAIN RESONANT BIQUAD, and the only filter of the seven that is - its passband does
// not move with resonance, where FltClassic's and FltNord's drop away. A Chamberlin state-variable
// section gives low, band and high from one pair of states, which is what the FilterType selector
// needs; band-reject is low + high.
//
// state[0] is the low output, state[1] the band. TWO STATES ONLY, so it shares gLadder harmlessly.
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

static double ladder_filter(double * state, double input, double g, double k, uint32_t tapStage) {
    // The feedback tap is pinned to the FOURTH pole and must stay there. LADDER_POLES grew to six
    // for FltLP's 36 dB setting, which has no resonance at all; taking the loop from the new last
    // pole instead would have quietly retuned every FltClassic in every patch.
    double   feedback = state[LADDER_LOOP_POLES - 1];
    double   x        = 0.0;
    uint32_t i        = 0;

    // NO PASSBAND COMPENSATION. Feeding the output back subtracts from the input, so a ladder loses
    // passband level as resonance rises — and that is not an artefact to be corrected, it is the
    // specified behaviour: the manual (p.198, FltClassic) says "just like on analog filters the
    // amplitude of the passband will drop about 12 dB when the resonance is set to a high value".
    // Earlier versions put a quarter of the loss back, which made the filter louder than the
    // hardware exactly where a patch is most likely to be driven hard.
    x = input - (k * feedback);

    // The stage input saturates rather than clipping flat. A real ladder's transistor stages
    // compress smoothly, which is what rounds off the resonance peak instead of tearing it, and it
    // is what bounds self-oscillation. The cubic below is the standard cheap stand-in for that
    // curve: unity slope through zero, flattening to +/-2/3 at the limits, and constant beyond.
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

// One ADSR step. Times are in seconds.
//
// EnvADSR's Shape scroll button selects the curve, in envShapeStrMap order: LogExp, LinExp, ExpExp,
// LinLin - the first word naming the attack and the second the decay and release. The curves live in
// paramCurves.c, shared with the envelope the editor DRAWS on the module face; the two carried the
// same law with different sharpness constants until 2026-08-24, so the drawn envelope and the played
// one were never quite the same curve.
//
// APPLIED BY SHAPING A LINEAR 0..1 PROGRESS rather than by changing the step size, so a segment still
// takes exactly the time its dial states whatever curve it is drawn with. (This used to say the
// stages move linearly towards their targets; that stopped being true when the shapes were read.)
static double envelope_step(uint32_t voice, uint32_t node, const tEngineNode * spec, bool gate) {
    double level = gEnvLevel[voice][node];
    double step  = 0.0;

    if (gate == true) {
        // Retrigger from Release as well as from Idle. Only accepting Idle meant a note played
        // before the previous release had finished was ignored until it had: the envelope carried on
        // FALLING, holding the filter part open, and the attack began late from wherever it landed.
        // Attacking from the current level is what an ADSR does — the level is deliberately not
        // zeroed, so a fast retrigger rises from where it was rather than clicking to nothing first.
        if ((gEnvStage[voice][node] == eEnvIdle) || (gEnvStage[voice][node] == eEnvRelease)) {
            gEnvStage[voice][node]    = eEnvAttack;
            gEnvProgress[voice][node] = 0.0;
            gEnvStart[voice][node]    = level;   // rise from wherever a fast retrigger caught it
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
static double signal_in(const tEngineNode * spec, double value[][2], uint32_t input) {
    int32_t source = spec->in[input];

    if ((input >= spec->inCount) || (source < 0)) {
        return 0.0;
    }
    return value[source][(spec->srcOut[input] > 0) ? 1 : 0];
}

// One sample of the raw waveform, at whatever rate the caller is stepping the phase.
// `voice` IS NEEDED HERE, and its absence was a bug rather than an omission. gSuperPhase is
// [MAX_VOICES][MAX_ENGINE_NODES][2]; the Super branch below indexed it as gSuperPhase[node][0], which
// puts the NODE number in the VOICE position and 0/1 in the node position. The compiler had been saying
// so all along — passing `double (*)[2]` where a `double *` is expected is what a two-deep index into a
// three-deep array produces.
//
// It was not out of bounds, by luck: 28 nodes fits inside 32 voices. What it did do was ignore the
// voice entirely, so every voice sounding the same node shared one pair of phase accumulators, and two
// different Super oscillators trod on each other's storage. A single voice with one Super oscillator
// is unaffected — it read [node][0][0] and now reads [0][node][0], the same value in a different slot —
// so what changes audibly is polyphonic Super and multi-Super patches, which is the point.
static double osc_waveform(uint32_t voice, uint32_t node, const tEngineNode * spec, double phase, double dt, double shape) {
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
            // SHAPE DOES NOT REACH THE TRIANGLE. Measured on the instrument 2026-08-30: OscB set to
            // Tri returns exactly -19.2 / -28.1 / -34.0 dB with no even harmonics at raw 0, 64 AND
            // 127 - the same symmetric triangle at every point on the dial. Shape is the PULSE WIDTH
            // and only the square uses it; we were skewing the triangle with it, which turned Tri
            // into a sawtooth at the top of the dial. The sine and saw already ignore it.
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

// Runs the oscillator OSC_OVERSAMPLE times per output sample and filters the result back down.
//
// The oscillators are the only part of the graph that creates harmonics which were not already
// there — the filter, mixers and amplifiers below them are linear — so oversampling here alone
// removes the aliasing without disturbing the delay, chorus and reverb, whose buffers are sized in
// samples and would all have to be resized for a change of engine rate.
static double oscillator_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double voicePitch,
                              double pitchDirect, double pitchVar, double shape) {
    double   pitch     = spec->basePitch;
    double   frequency = 0.0;
    double   dt        = 0.0;
    double   sum       = 0.0;
    uint32_t step      = 0;
    uint32_t tap       = 0;

    // Kbt on transposes the played note by the oscillator's offset from unity; Kbt off leaves the
    // keyboard disconnected and the oscillator holds the pitch Tune names.
    if ((spec->oscKbt == true) && (voicePitch >= 0.0)) {
        pitch = voicePitch + (spec->basePitch - OSCB_TUNE_UNITY);
    }

    // The two pitch modulation inputs are NOT equivalent. The upper one ("Pitch") is direct — what
    // arrives is what it does — while the lower one ("PitchVar") is attenuated by the module's Pitch
    // knob, which is the knob drawn alongside it. A vibrato patch rides on the variable one, since
    // that is the knob an aftertouch morph can open.
    if ((pitchDirect != 0.0) || ((spec->modAmount > 0.0) && (pitchVar != 0.0))) {
        pitch += (pitchDirect + (pitchVar * spec->modAmount)) * PITCH_MOD_SEMITONES;
    }
    // exp2, not pow(2, x). Identical result, and this runs once per oscillator per voice per
    // oversampled sample — at fifteen voices that is a few million calls a second.
    frequency = 440.0 * exp2((pitch - MIDI_NOTE_A440) / 12.0);

    // Above Nyquist there is no waveform left to produce, only aliasing. Return silence rather than
    // just stopping the phase: a halted sawtooth is not silence, it is a DC offset held at whatever
    // level the waveform sat at, which thumps. The limit stays the OUTPUT rate's Nyquist even though
    // the oscillator now runs faster, because the decimator would remove anything above it anyway.
    if (frequency > (gSampleRate * 0.5)) {
        return 0.0;
    }
    dt        = frequency / (gSampleRate * (double)OSC_OVERSAMPLE);

    for (step = 0; step < OSC_OVERSAMPLE; step++) {
        double phase = advance_phase(&gPhase[voice][node], dt);

        gOscHistory[voice][node][gOscHistoryPos[voice][node]] = (float)osc_waveform(voice, node, spec, phase, dt, shape);
        gOscHistoryPos[voice][node]                           = (gOscHistoryPos[voice][node] + 1) % OSC_DECIMATE_TAPS;
    }

    // One output for every OSC_OVERSAMPLE inputs, so the filter only has to be evaluated at the
    // output rate however high the oversampling factor is.
    // THE INDEX IS WALKED, NOT RECOMPUTED. This loop is the engine's hottest: it runs once per
    // oscillator per voice per oversampled sample, so at eight voices it is executed a few million
    // times a second, and it used to do an integer division (the %) on every one of its 128 taps.
    // Walking the read position and wrapping with a comparison is the identical sequence of taps in
    // the identical order — bit-for-bit the same output — for a fraction of the cost.
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

// One LFO sample. The waveform is generated bipolar and then mapped into whichever range the Pos
// scroll button selects — posStrMap is {Pos, PosInv, Neg, NegInv, Bip, BipInv}, so half the settings
// are simply the inverse of another, which is what makes an LFO able to close something as it opens
// something else.
//
// Not band-limited, and deliberately so: an LFO runs at control rate on the hardware, well below
// anything that could alias into the audio band.
static double lfo_step(uint32_t voice, uint32_t node, const tEngineNode * spec) {
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
            // The synth names this shape Sqr2Tri, i.e. square AT one end of Shape and triangle at
            // the other. This runs the other way round - Shape at 0 gives very nearly a triangle
            // and winding it up drives the tanh into a square - so either the name reads
            // right-to-left or the morph is inverted. Nobody has listened to it against the
            // hardware, and a label is not enough to justify flipping a waveform, so it stands.
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

// MODULATION IS SUMMED INTO THE DIAL VALUE AND CLAMPED THERE, then converted to a frequency exactly
// once. This is not a rearrangement for tidiness — the clamp is the whole point, and it can only be
// applied in this domain.
//
// The Freq dial is a pitch: flt_cutoff_hz() is 13.75 * 2^(value/12), i.e. the value counts semitones
// up from A-1, reaching the 21.1 kHz the manual quotes at 127. Because the dial is already
// logarithmic in frequency, "multiply the cutoff by 2^(semitones/12)" and "add semitones to the dial
// value" are algebraically THE SAME OPERATION. The previous code did the former, so its shape was
// never actually wrong — what it lacked was any limit, because there is no natural place to put one
// in the frequency domain, and a modulated cutoff could run far past the top of the dial's range.
//
// It did not run away audibly only because two later clamps caught it: the Nyquist guard below and
// LADDER_MAX_G. Both are safety limits on the filter model, not statements about the instrument's
// range, so the cutoff was being bounded by an implementation detail at whatever frequency those
// happened to bite. Clamping the control to the dial's own 0..127 puts the limit where the hardware
// has it, and leaves the other two doing only the job they were written for.
#define FLT_CONTROL_MIN    (0.0)
#define FLT_CONTROL_MAX    (127.0)

static double filter_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double input, double mod, double voicePitch,
                          double cutoffParam, double resonance) {
    double control = cutoffParam;
    double cutoff  = 0.0;
    double g       = 0.0;

    if (spec->active == false) {
        return input;    // a bypassed filter passes its input straight through
    }

    // Whatever is patched into the Env input sweeps the cutoff, scaled by the Env knob. An envelope
    // there is what turns a static filter into one that opens and closes with the note.
    //
    // FULL_MOD_SEMITONES is 64 against a modAmount that reaches 2.0 (the dial's 0..200%), so a
    // full-scale modulator at the knob's maximum sweeps 128 semitones — the dial's whole range.
    // That the two constants multiply out to the range exactly is the reason to believe 64 rather
    // than some value fitted to make the old unclamped arithmetic sound reasonable.
    if ((spec->modAmount > 0.0) && (mod != 0.0)) {
        control += mod * spec->modAmount * FULL_MOD_SEMITONES;
    }

    // Kbt moves the cutoff with the note, relative to middle C, at the percentage the scroll button
    // selects (manual p.196). One semitone of note is one unit of dial, which is what makes 100% Kbt
    // track the keyboard exactly.
    // Tracks the SOUNDING pitch, so a glide carries the cutoff with it rather than snapping.
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

    // THE LADDER MODEL ONLY HOLDS WHILE g IS WELL BELOW 1. Each stage is a plain one-pole using the
    // previous sample's output, and four of those inside a feedback loop stop behaving as a filter
    // once the poles get close to Nyquist: measured with full resonance, everything up to g = 0.806
    // is clean at 60 dB of margin, g = 0.842 loses 7 dB of it, and by g = 0.874 the margin has
    // collapsed to 32 dB AND the passband has dropped 6 dB — the filter is misbehaving, not merely
    // adding a little distortion. What is heard is the saturation's harmonics folding back down,
    // amplified on the way by the resonant feedback.
    //
    // This USED to be load-bearing, and the note here used to say so: when the graph ran at the
    // output rate, the top of FltClassic's 21.1 kHz range (manual p.198) landed outside the model
    // and this clamp was what kept it from rasping. Both halves of that have since gone away. The
    // whole graph now runs oversampled (ENGINE_OVERSAMPLE), so 21.1 kHz against 96 kHz gives
    // g = 0.75, inside the model; and the control clamp in this function now stops the modulated
    // cutoff exceeding the top of the dial in the first place.
    //
    // So this is now a backstop that should never fire at a normal device rate, rather than
    // something shaping the sound. Left in place deliberately: it costs one comparison, and it is
    // the only thing standing between an unusual rate — or a future module whose range exceeds
    // FltClassic's — and a filter that misbehaves rather than merely distorts.
    if (g > LADDER_MAX_G) {
        g = LADDER_MAX_G;
    }
    // MAXIMUM FEEDBACK, and it is not the textbook 4. That figure is for a ladder with no delay in
    // its loop; this one has a sample of it, and how much phase that sample contributes depends on
    // the sample rate — so the rate at which the loop actually reaches oscillation moved when the
    // engine started running oversampled, and the resonance went quiet with it.
    //
    // 4.3 IS MEASURED AGAINST THE INSTRUMENT, not chosen by ear. A saw was put through a real
    // FltClassic and through this ladder, and both responses taken the same way — output over input
    // at each harmonic of a 98 Hz saw, so the source spectrum cancels and only the filter is left.
    // Peak height above the passband at Res 127, a displayed cutoff of 1397 Hz:
    //
    //                       12 dB      18 dB      24 dB
    //     the instrument    +31.5      +28.7      +25.9
    //     k = 4.3           +31.6      +28.5      +25.4      <- 0.3 dB mean error
    //     k = 5.0           +37.0      +33.9      +30.7      <- what this used to be
    //
    // The previous note here recorded k 5.0 as "+79/+81/+73 dB". That was a different measurement,
    // not this one, and the two are not comparable — which is precisely why it read as though the
    // engine were 50 dB out when it was really about 5.
    //
    // THE ANSWER DEPENDS ON INPUT LEVEL, because ladder_saturate() does. At a quarter of full scale
    // the same k peaks some 6 dB higher, the loop being driven less deeply into the knee. 4.3 is
    // right for a full-scale oscillator straight into the filter, which is how the instrument was
    // measured; a much quieter source will resonate more sharply here than it does there.
    //
    // RE-MEASURE IF ENGINE_OVERSAMPLE CHANGES — the loop's phase, and so the k at which it reaches
    // oscillation, is a property of the rate rather than of the filter.
    //
    // CONFIRMED INDEPENDENTLY 2026-08-24, by a measurement that shares nothing with the one above
    // except the instrument: noise through FltClassic, every setting divided by the same patch
    // bypassed, fitted against G^tap / (1 + k.G^4). It agrees with what this code already does, which
    // is worth recording precisely because it could have disagreed —
    //   - THE LOOP IS FOUR POLES LONG WHATEVER THE SLOPE, and the dB switch only moves the tap. That
    //     is what ladder_filter() has always done (feedback from state[LADDER_POLES - 1], return
    //     state[tapStage]) and it is now measured rather than assumed. Fitting each slope on its own
    //     gives the same k to within 0.011; a loop that matched the tap fits three to six times worse.
    //   - THE PEAK SPACING BETWEEN TAPS falls out of that topology at 3.01 dB and needs no fitting.
    //     The saw measurement above got 2.8 and 2.8; the noise one 3.2 and 3.0.
    //   - THE Res DIAL IS LINEAR IN FEEDBACK, which is how resonance is scaled here.
    //   - THE PASSBAND DROP AT FULL RESONANCE MEASURES -13.8 to -15.0 dB, against the "about 12 dB"
    //     the manual quotes and ladder_filter()'s note cites. Deliberately NOT compensating for it
    //     therefore remains right, and the real figure is a shade deeper than the manual's.
    // The continuous-time model used for the DRAWN response (flt_ladder_feedback(), paramCurves.c)
    // puts the top of the dial at k = 4.0 — measured 2026-08-30, when the hardware was found to sustain
    // an oscillation at Res 127; it read 3.914 before that, from a peak that was really a limited
    // oscillation. That is
    // NOT this number and must not replace it: that model has no delay in its loop and no saturation
    // in its stages, both of which move where oscillation actually starts. Same topology, different
    // constant, each measured for what it describes.
#define LADDER_K_MAX    (4.3)

    switch (spec->topology) {
        case eFilterTopologyCascadeHP:
        {
            return cascade_hp_filter(gLadder[voice][node], input, g, spec->tapStage + 1u);
        }
        case eFilterTopologyBiquad:
        {
            // g is the one-pole coefficient; the SVF wants 2.sin(pi.fc/sr), and for the corners a
            // patch actually uses the two agree closely enough that deriving one from the other
            // keeps a single cutoff path. Damping is 1/Q from the MEASURED resonance law, not from
            // the Q the dial prints - see flt_static_q() in paramCurves.c.
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

// One node's output for one voice, written into value[n]. Extracted so the Voice Area pass and the
// FX Area pass are the same code rather than two copies that could drift — they differ only in which
// nodes they visit and in the voice index they carry.
//
// `voice` selects the per-voice state; FX Area nodes are evaluated once with voice 0, which is also
// the only voice the shared delay/chorus/reverb buffers ever see.
static void eval_node(uint32_t voice, uint32_t n, const tSoundEngineParams * paramsIn,
                      double value[][2], double voicePitch) {
    const tEngineNode * spec = &paramsIn->node[n];
    double              a    = signal_in(spec, value, 0);

    value[n][0] = 0.0;
    value[n][1] = 0.0;

    switch (spec->kind) {
        case eNodeLfo:
        {
            value[n][0] = lfo_step(voice, n, spec);
            value[n][1] = value[n][0];
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
        case eNodeMix:
        {
            uint32_t c           = 0;

            // A stereo mixer reads eight legs but has only four level knobs, so both legs
            // of a channel share one — and each CHANNEL contributes the average of its
            // two legs, not their sum.
            //
            // That halving matters because the engine is mono. Where a stereo pair is
            // fed from one mono-collapsed module — an Fx-In's L and R, or a reverb's two
            // outputs — both legs carry the SAME value, so summing them counted that
            // channel twice. A patch mixing dry (one stereo source) against two separate
            // mono delays (a pair of different modules) therefore heard the dry and the
            // reverb 6 dB hot against the delays. Averaging is also the right mono
            // downmix for a genuinely stereo pair, so it is correct in both cases.
            bool     stereoPairs = (spec->inCount > (MAX_NODE_INPUTS / 2));
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
            value[n][0] = (spec->active == true) ? (a * gSmoothedGain[n]) : 0.0;
            value[n][1] = value[n][0];
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
            // THE TWO LEGS ARE THE LEFT AND RIGHT CHANNELS AND THEY STAY SEPARATE. This used to sum
            // them into one value, which is what made the whole engine mono however stereo the
            // modules feeding it were.
            //
            // AN UNPATCHED SOCKET MIRRORS THE OTHER, and that rule is load-bearing rather than
            // tidiness. Cabling only the left socket is very common, and letting signal_in() return
            // its usual 0.0 for the absent leg would play such a patch out of one speaker — which
            // the instrument never does, both of its sockets being real. Mirroring leaves every
            // one-socket patch exactly as it sounded before this change.
            //
            // WHAT DOES CHANGE IS THE DUAL-MONO PATCH: the same signal cabled to both sockets used
            // to be summed to 2a and that sum sent to both channels, i.e. 6 dB hot. It now plays at
            // a, which is what the hardware does with two sockets carrying the same thing.
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

    // EVERY NODE MUST LEAVE BOTH LEGS VALID, and most kinds are mono and write only leg 0 —
    // the oscillators, the filter, LevAmp, LevMult and the mixers all do.
    //
    // Leaving leg 1 at the zero this function starts it from was INVISIBLE while eNodeOut summed its
    // two legs: a spurious 0 on the right just made the sum equal the left, which is what got played.
    // It stopped being invisible the moment the Out module began keeping them apart.
    // PatchTestFiles/SimpleLead.pch2 cables one module's output 0 to Out L and its output 1 to Out R
    // — an entirely ordinary thing for a patch to do — and the right channel fell silent.
    //
    // The three exceptions fill both legs themselves and must NOT be flattened here: an envelope
    // keeps its SHAPED AUDIO in leg 1, and the chorus and the Out module are genuinely stereo.
    switch (spec->kind) {
        case eNodeEnv:
        case eNodeChorus:
        case eNodeReverb:
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
}

// One tapped module's stereo pair.
//
// DELIBERATELY CONSERVATIVE: only eNodeOut is known to fill BOTH legs with a genuine left and right.
// Most node kinds mirror leg 0 into leg 1, but some — the oscillators among them — write leg 0 and
// leave leg 1 at the zero eval_node() starts it from. Reading leg 1 blindly would give those a
// silent right channel, so anything that is not an Out module has its leg 0 mirrored, which is
// exactly what the mono path did before stereo. An envelope used as an amp is the standing
// exception: its SHAPED AUDIO is in leg 1 and is mono, so both channels take that.
static void tap_pair(const tSoundEngineParams * paramsIn, int32_t node, double value[][2], double out[2]) {
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

// A voice is done when its key is up AND it has stopped making sound — only then can it be handed
// to another note without cutting anything off. Which test that is depends on what is shaping the
// note: an EnvADSR's own release when the patch has one, the anti-click ramp when it does not.
//
// Asking the envelopes rather than watching the output level is deliberate: an envelope says when it
// has finished, where a level has to be watched for long enough to be sure it is not just passing
// through zero.
static bool voice_is_finished(const tSoundEngineParams * paramsIn, uint32_t v, bool chainHasEnvelope) {
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
        // WORTH LOGGING, because reset_node_state() below empties every delay line and reverb buffer
        // in the engine. A topology change that is real — a module added, a cable moved — has to do
        // that. One that is NOT real takes the delay repeats and the reverb tail with it, and what
        // is heard is the effect stopping dead and then filling up again from nothing.
        //
        // So if a delay or reverb is ever reported cutting out at random, this line is the first
        // thing to look for: if it fires when nothing about the patch changed, the signature is
        // unstable and the wipe is the symptom rather than the cause. Debug builds only.
        //
        // Not the explanation for every such report: 45 s of idle playing, 120 parameter edits and
        // repeated select/deselect cycles all produced ZERO changes here, so whatever else may cut a
        // delay short, it is not this under those conditions.
        LOG_DEBUG("TOPOLOGY CHANGE %llu -> %llu, nodes %u, tap %d — delay and reverb buffers cleared\n",
                  (unsigned long long)gSeenTopology, (unsigned long long)params.topology,
                  (unsigned)params.nodeCount, params.tap);
        gSeenTopology = params.topology;
        reset_node_state();
    }
    // Oscillator phases are deliberately NOT reset when a note starts. They free-run, as the G2's do
    // unless something is patched to their Sync input, and that matters more than it sounds: several
    // oscillators detuned by a few cents are what makes a patch thick, and starting them all at
    // phase zero has them summing as one voice for the seconds a 7 cent difference takes to drift
    // apart. Note events themselves are taken inside the sample loop below.

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

    // A PER-VOICE EnvADSR is the note's shape; the fixed ramp is only there to stop a click when
    // there is none to do that job. Per-voice only, and it has to be: an envelope after the mix
    // shapes the effect rather than the note, and counting it here would leave every voice unramped
    // AND have voice_is_finished() retire voices the moment a key came up.
    for (n = 0; n < params.nodeCount; n++) {
        if ((params.node[n].kind == eNodeEnv) && (params.node[n].postMix == false)) {
            chainHasEnvelope = true;
            break;
        }
    }

    // FREE-RUNNING PHASE - THE CHEAP HALF, AND THE HALF THAT MATTERS FOR EVERY PATCH.
    //
    // The instrument's oscillators and LFOs never stop, so a note started later finds them
    // somewhere else. Ours did not: gPhase is seeded once (a golden-ratio scatter, so voices start
    // decorrelated) and then FROZEN whenever a voice is idle, which meant two notes seconds apart
    // could begin on identical phase. Not a reset to zero - a freeze - but just as wrong.
    //
    // Advancing the STATE and computing the AUDIO are separable: the phase an oscillator would
    // have reached is arithmetic, so it does not need the graph. MEASURED at 12.8% CPU against
    // 12.3% with free-running off, i.e. half a point, where rendering the graph instead costs
    // over a point on a THREE-node patch and scales with the patch.
    // So do that once per block for the idle voices and skip the DSP entirely. Voices that ARE
    // sounding advance through the render as before, and the full free-run render still happens
    // for the patches that can actually be heard while idle.
    //
    // MEASURE INSTANTANEOUS CPU, NOT `ps -o %cpu`, which reports an average over the process's
    // whole lifetime and made an early version of this look fifteen times worse than it was.
    // Take the delta of `ps -o time=` over a fixed window instead.
    //
    // A "reset phase on note-on" option, for predictable bass, would zero gPhase for the allocated
    // voice instead. It is deliberately not the default and not written yet - the hardware
    // free-runs. See Docs/todo.txt.
    if (engine_no_free_run() == false) {
        double idleSamples = (double)frameCount * (double)ENGINE_OVERSAMPLE;

        for (uint32_t v = 0; v < params.voiceCount; v++) {
            // ONLY the voices the render will NOT touch. A free-running voice is rendered even
            // though it is not sounding, and oscillator_step() advances its phase as it goes - so
            // advancing it here as well moved it twice, jumping a whole block's worth at every
            // block boundary. The beat between the block rate and the oscillator's frequency put a
            // wah of a few Hz on every note. Get this condition wrong in the other direction and
            // the phase simply stops; it has to mirror the render's own test exactly.
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
            double value[MAX_ENGINE_NODES][2];
            double voiceSum[MAX_ENGINE_NODES][2];

            // One event per sample. A chord's worth of note-ons arriving together therefore lands over
            // consecutive samples rather than all but the last being thrown away, and every note takes
            // effect where it actually arrived instead of at the next buffer boundary.
            (void)take_next_note_event();

            // The patch's own Vibrato, which is nothing to do with the cabling: it lives on a hidden
            // module beside Glide and Bend, and is how a patch gets aftertouch vibrato without an LFO
            // anywhere in it. The chosen controller sets the depth, so at rest there is none.
            //
            // ONE PHASE FOR THE WHOLE PATCH, not one per voice: it is a property of the patch rather
            // than of a note, so a chord's notes wobble together instead of drifting apart.
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
                // Smoothed in DIAL units, not hertz. Smoothing a logarithmic control linearly in
                // frequency makes a knob move slowly at the bottom of its travel and leap at the
                // top; smoothing the dial value sweeps evenly in pitch, which is what the dial
                // means and what turning it sounds like.
                gSmoothedCutoff[n] = smooth_to(&gSmoothCutoff[n], spec->cutoffParam, smoothCoeff, primed);
                gSmoothedRes[n]    = smooth_to(&gSmoothRes[n], spec->resonance, smoothCoeff, primed);
                gSmoothedGain[n]   = smooth_to(&gSmoothGain[n], spec->gain, smoothCoeff, primed);

                for (uint32_t c = 0; c < MAX_NODE_INPUTS; c++) {
                    gSmoothedLevel[n][c] = smooth_to(&gSmoothLevel[n][c], spec->level[c], smoothCoeff, primed);
                }

                gSmoothPrimed[n]   = true;
            }

            memset(voiceSum, 0, sizeof(voiceSum));

            // ── VOICE AREA: the whole area, once per sounding voice ──────────────────────────
            //
            // Each voice is a complete instance of the Voice Area with its own oscillator phases,
            // filter state and envelopes, exactly as the hardware instantiates it. The FX Area is
            // NOT in here: it is one shared instance fed by the sum of the voices, which is what
            // lets a chord share one reverb instead of running 8 of them.
            for (uint32_t v = 0; v < params.voiceCount; v++) {
                tVoice * voice = &gVoice[v];

                // FREE-RUNNING. The instrument's Voice Area runs whether or not a key is down: an
                // oscillator patched to an output sounds on its own, an LFO keeps its phase, and a
                // note gates the ENVELOPE rather than the area. This engine used to skip the whole
                // voice when nothing was sounding, so any patch that depends on that produced
                // silence. Voice 0 is therefore always rendered, and while it holds no note it is
                // rendered UNGATED - no anti-click ramp to zero, no release tail, no retirement.
                //
                // ONLY voice 0. The hardware runs every allocated voice continuously, so a poly
                // patch really does stack that many free-running oscillators, but matching that
                // would cost 32 voices of idle CPU for a difference of level. One instance is the
                // deliberate approximation; the post-mix section below already runs unconditionally
                // for the same reason, so a reverb tail outlives the last note.
                // FREE-RUN ONLY WHERE IT CAN BE HEARD. A patch whose amp is a per-voice envelope
                // puts out nothing until a key is pressed, so rendering its whole graph while idle
                // computes silence. MEASURED on a drone patch, which takes this path: 3.3% CPU
                // against 2.0% with free-running off - so the render is worth about 1.3 points on
                // three nodes and more on a large patch. Not ruinous, but it buys nothing at all
                // for the enveloped case, and the phase advance before the frame loop already
                // covers the part that IS audible there (where the oscillators are when the next
                // note starts).
                //
                // The cost is that an LFO in an ENVELOPED patch still does not advance between
                // notes, which is what the instrument does. Fixing that properly means asking
                // whether a node reaches an Out without passing through a gated envelope, rather
                // than whether an envelope exists at all - see Docs/todo.txt.
                bool freeVoice = (v == 0) && (chainHasEnvelope == false)
                                 && (engine_no_free_run() == false);
                bool freeRun   = (voice->sounding == false) && (freeVoice == true);

                if ((voice->sounding == false) && (freeRun == false)) {
                    continue;               // costs nothing when it is not playing
                }

                // A KEY COMING UP MUST NOT INTERRUPT THE OSCILLATOR. The anti-click ramp exists for
                // patches with no envelope, and in exactly those the instrument carries on sounding
                // when the note is released - a key gates the envelope, and there isn't one. So the
                // free-running voice hands straight back to free running instead of ramping to
                // silence and then waiting out the release tail before resuming, which put an
                // audible dip in a drone that the hardware does not have. Phase and every other
                // per-voice state carry across untouched, so the level is continuous and there is
                // nothing to click. With an envelope in the chain the normal path still runs,
                // because there the envelope owns the release and cutting it would truncate it.
                if ((freeVoice == true) && (voice->gate == false) && (chainHasEnvelope == false)) {
                    voice->sounding = false;
                    freeRun         = true;
                }

                // Portamento. The sounding pitch chases the played note; how fast, and whether at
                // all, comes from the patch's Glide setting. Exponential rather than linear — it is
                // what a glide sounds like, and the coefficient is set so the remaining distance is
                // down to a percent by the time the dial says.
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

                // RETIRED ONLY WHEN IT HAS GONE QUIET AS WELL as finishing its envelope. The
                // envelope alone is not enough: a patch whose EnvADSR modulates the filter rather
                // than acting as the amp goes on sounding after that envelope is idle, and dropping
                // it from the render at that moment cuts it off mid-note with a click. A patch that
                // genuinely drones simply never frees the voice, so new notes take the others and
                // eventually steal — which is what the instrument does with a droning patch too.
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
                }
            }

            // ── AFTER THE MIX: one shared instance, whatever the polyphony ───────────────────────
            //
            // The FX Area, plus any delay, chorus or reverb sitting in the Voice Area and anything
            // downstream of one — see mark_post_mix_nodes(). Runs even with every voice silent, so a
            // reverb tail or a delay repeat carries on after the last note is released rather than
            // being cut off with it.
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

                // The patch's other Out modules, summed rather than mixed at some fraction: that is
                // what the hardware's sockets do when two areas both drive them. Summed per channel
                // AND PER PAIR, so a patch whose Out modules feed different physical pairs — which
                // is what every measurement patch does — keeps them apart instead of folding them
                // into one stereo image.
                for (uint32_t t = 0; t < params.extraTapCount; t++) {
                    double   extra[2] = {0.0, 0.0};
                    uint32_t d        = params.node[params.extraTap[t]].outDest & 1U;

                    tap_pair(&params, params.extraTap[t], value, extra);
                    sample[d][0] += extra[0];
                    sample[d][1] += extra[1];
                }
            }
            // With an envelope module shaping the note, the fixed ramp would only double up on it; it is
            // still applied when the chain has none.
            // THE METERS READ THE LOUDER CHANNEL. A per-channel peak would need a per-channel meter
            // to show it, and what these drive is one number.
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
                // The anti-click ramp is applied PER VOICE as each voice's output leaves the Voice Area
                // (see the voice loop), not here. Applying it to the mixed output would fade the whole
                // instrument — including the FX tail — every time any one note was released.
                // The user's own attenuation, ahead of the knee.
                *sp                           *= (double)atomic_load(&gOutputGainMilli) / 1000.0;

                // Soft knee rather than a hard edge. Below the knee nothing is touched at all, so ordinary
                // playing is untouched; above it the curve bends over instead of shearing the tops off, which
                // is both kinder to listen to and closer to what an overloaded analogue output does. The hard
                // clamp afterwards is only a guard against a bug producing something enormous.
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

            // FOUR CHANNELS IF THE CALLER ASKED FOR THEM, otherwise the two pairs are SUMMED.
            //
            // The summing is what keeps the application unchanged: its device is stereo, every Out
            // module used to be added together whatever pair it fed, and a patch sending anything to
            // Out 3/4 would fall silent if this suddenly routed by destination. A caller that wants
            // them apart — the measurement harness, which needs the rig's dry reference on one pair
            // and its processed signal on the other — asks for four and gets them.
            //
            // Choosing WHICH pair a stereo device should monitor, rather than always summing, wants
            // a menu item; see the todo. Summing is the answer that changes nothing until then.
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
