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

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "dataBase.h"
#include "cableChain.h"
#include "globalVars.h"
#include "renderParams.h"
#include "patchParamsResources.h"
#include "audioOutput.h"
#include "midiInput.h"
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
#define SHPB_MODE_WAVEFORM      (0)

// Mix4to1C: one level per input, then a pad and a curve.
#define MIX_PARAM_LEVEL_BASE    (0)
// The two mixers put Curve in different places: Mix4to1C has a Pad at 8 and Curve at 9, Mix4to1S
// has no Pad and Curve at 8.
#define MIX_PARAM_PAD           (8)   // Mix4to1C only: 0 dB or -6 dB on every input at once
#define MIX_PARAM_CURVE         (9)
#define MIXS_PARAM_CURVE        (8)
#define MIX_CURVE_LIN           (1)   // expStrMap is {"Exp", "Lin", "dB"} — Lin is the middle one

// StChorus: a detune depth and an amount, then its power button.
#define CHORUS_PARAM_DETUNE     (0)
#define CHORUS_PARAM_AMOUNT     (1)
#define CHORUS_PARAM_ACTIVE     (2)

// Compress: threshold and reference level run 0..42, ratio 0..66.
#define COMP_PARAM_THRESHOLD    (0)
#define COMP_PARAM_RATIO        (1)
#define COMP_PARAM_ATTACK       (2)
#define COMP_PARAM_RELEASE      (3)
#define COMP_PARAM_ACTIVE       (6)

// DelayB. Its range is a MODE, like the shape oscillators' waveform.
// The two delays share their first four parameters but NOT their Bypass: DelayA has six parameters
// with Bypass at 4, DelayB has nine with Bypass at 7 (and an extra HP at 8). Reading DelayB's index
// on a DelayA lands past the end of its parameter list, reads zero, and silently bypasses it.
#define DELAY_PARAM_TIME        (0)
#define DELAY_PARAM_FEEDBACK    (1)
#define DELAY_PARAM_LP          (2)   // DelayA calls this Filter; both are a damping control
#define DELAY_PARAM_DRYWET      (3)
#define DELAYA_PARAM_ACTIVE     (4)
#define DELAYB_PARAM_ACTIVE     (7)
#define DELAY_MODE_RANGE        (0)

#define REVERB_PARAM_TIME       (0)
#define REVERB_PARAM_BRIGHT     (1)
#define REVERB_PARAM_DRYWET     (2)
#define REVERB_PARAM_ACTIVE     (3)

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
#define CONNECTOR_IN_A          (0)
#define CONNECTOR_IN_B          (1)
#define FLT_CONNECTOR_ENV_IN    (2)   // FltClassic's control input, the one beside its Env knob

// The G2 caps the total pitch modulation reaching an oscillator or filter at +/-64 semitones
// (manual p.78), which is what an Env amount of 100% corresponds to.
#define FULL_MOD_SEMITONES    (64.0)

// A full-scale bipolar signal on an oscillator's Pitch input sweeps one octave either way: the
// manual's own worked example (p.78) is an A4 modulated "up and down by one octave", and it stays an
// octave whatever note is played, because a Pitch input modulates on the note scale rather than
// linearly in frequency. That is a much smaller range than the filter's Env input above.
#define PITCH_MOD_SEMITONES    (12.0)

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
#define VOICE_GAIN          (0.15)

// Where the output starts bending rather than shearing.
#define OUTPUT_KNEE         (0.80)
#define ENVELOPE_SECONDS    (0.005)        // the anti-click ramp used when no EnvADSR is in the chain

// Envelope times. The G2's range runs from well under a millisecond to tens of seconds; this is an
// exponential fit across that span rather than a reading of the hardware's own table, in keeping
// with the rest of the engine approximating rather than reproducing.
#define ENV_TIME_MIN    (0.0005)
#define ENV_TIME_MAX    (45.0)

// Every ladder runs its full four poles whatever slope is selected — see ladder_filter().
#define LADDER_POLES    (4)

// The chain the engine renders. Small and fixed: these are hand-built sketches, not whole patches,
// and a bound is what keeps the walk safe against a patch that feeds back into itself.
// Raised from 12 once whole patches came into scope: a real one runs to a couple of dozen modules
// across the Voice and FX areas.
#define MAX_ENGINE_NODES    (28)

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

    double   cutoffHz;       // filter
    double   resonance;
    uint32_t extraPoles;
    double   fltKbt;
    double   modAmount;      // how far the Env input moves the cutoff, 0..2 (the dial's 0..200%)

    double   attack;         // envelope, in seconds
    double   decay;
    double   sustain;        // 0..1
    double   release;

    double   gain;           // LevAmp

    double   depth;          // chorus detune depth, and the delay's feedback
    double   amount;         // chorus wet amount, delay/reverb dry-wet
    double   timeSeconds;    // delay time
    double   damping;        // delay LP / reverb brightness, 0..1
    double   threshold;      // compressor
    double   ratio;
    double   attackCoeff;
    double   releaseCoeff;
    double   constant;       // Constant module's value
    uint32_t line;           // which shared delay line this node owns, if it needs one
} tEngineNode;

typedef struct {
    uint32_t    nodeCount;
    int32_t     tap;           // the node whose output reaches the speakers, -1 for silence
    // Patch-wide settings, from the hidden modules in the Morph location rather than from any module
    // on the canvas. Vibrato is how a patch gets aftertouch vibrato with no LFO in it anywhere.
    uint32_t    vibratoSource; // 0 off, 1 aftertouch, 2 wheel
    double      vibratoCents;
    double      vibratoHz;
    tGlideMode  glideMode;     // patch-wide, not per node
    double      glideSeconds;
    double      bendSemitones; // 0 when the patch has bend switched off
    uint64_t    topology;      // changes shape => the audio thread resets its per-node state
    tEngineNode node[MAX_ENGINE_NODES];
} tSoundEngineParams;

// Published by the UI thread, consumed by the audio thread, via a seqlock: the writer makes the
// sequence odd before touching the snapshot and even again after, so a reader that sees an odd
// sequence — or a different one either side of its copy — knows it read during a write. Neither
// side ever blocks, and the audio thread never waits on the UI thread. A plain pair of buffers
// would not do: the UI can publish twice while one audio buffer is being filled, which is long
// enough to land back on the buffer the audio thread is mid-copy of.
static tSoundEngineParams gParams    = {0};
static _Atomic uint32_t   gParamsSeq = 0;

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
    eStatusNoSelection,
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
static double             gSampleRate                 = 48000.0;
static bool               gGateOpen                   = false;
static int32_t            gVoiceNote                  = -1;

// The pitch actually sounding, in MIDI note numbers and fractional — it chases gVoiceNote rather
// than jumping to it, which is what portamento is. Kept as a double because a glide spends most of
// its time between notes. -1 means nothing has been played yet.
static double             gGlidePitch                 = -1.0;
static double             gVibratoPhase               = 0.0;
static bool               gGlideActive                = false;
static tSoundEngineParams gLastGoodParams             = {0};
static uint64_t           gSeenTopology               = 0;
static double             gEnvelope                   = 0.0; // the anti-click ramp, when no EnvADSR is present

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
#define OSC_OVERSAMPLE       (4)
#define OSC_DECIMATE_TAPS    (128)

static double         gOscDecimate[OSC_DECIMATE_TAPS];
static double         gOscHistory[MAX_ENGINE_NODES][OSC_DECIMATE_TAPS];
static uint32_t       gOscHistoryPos[MAX_ENGINE_NODES];

static double         gPhase[MAX_ENGINE_NODES];
static double         gLfoLastPhase[MAX_ENGINE_NODES];
static double         gLfoTarget[MAX_ENGINE_NODES];
static double         gLfoHeld[MAX_ENGINE_NODES];
static double         gSuperPhase[MAX_ENGINE_NODES][2];
static double         gLadder[MAX_ENGINE_NODES][LADDER_POLES];

// Delay memory. Held as float rather than double purely for size — half a second per line at any
// sensible rate, four lines, is enough for the delays a patch normally has and keeps this under a
// megabyte. Nodes beyond that many run dry rather than sharing a line and smearing into each other.
#define MAX_DELAY_LINES       (4)
#define DELAY_LINE_SAMPLES    (48000)
static float          gDelayLine[MAX_DELAY_LINES][DELAY_LINE_SAMPLES];
static uint32_t       gDelayWrite[MAX_DELAY_LINES];
static double         gDelayDamp[MAX_DELAY_LINES];

// The chorus's own short sweep, one per node, plus its LFO phase.
#define CHORUS_SAMPLES    (2048)
static float          gChorusLine[MAX_ENGINE_NODES][CHORUS_SAMPLES];
static uint32_t       gChorusWrite[MAX_ENGINE_NODES];
static double         gChorusLfo[MAX_ENGINE_NODES];

// Compressor gain-reduction state, one per node.
static double         gCompEnv[MAX_ENGINE_NODES];

// A Schroeder reverb: four combs into two allpasses. One reverb is modelled; any further ones pass
// their input through, which is what a patch with two of them would mostly sound like anyway.
#define REVERB_COMBS      (4)
#define REVERB_ALLPASS    (3)

// Mutually prime lengths, so the combs do not reinforce each other into a ringing tone. The buffers
// are sized from the longest of each set rather than a hand-written number — getting those out of
// step is a buffer overrun, and it is the kind that only shows up as a crash much later.
#define REVERB_COMB_MAX       (1356)
#define REVERB_ALLPASS_MAX    (225)
static const uint32_t kCombLen[REVERB_COMBS]      = {1116, 1188, 1277, REVERB_COMB_MAX};
static const uint32_t kAllpassLen[REVERB_ALLPASS] = {REVERB_ALLPASS_MAX, 149, 97};
static float          gComb[REVERB_COMBS][REVERB_COMB_MAX];
static uint32_t       gCombPos[REVERB_COMBS];
static double         gCombStore[REVERB_COMBS];
static float          gAllpass[REVERB_ALLPASS][REVERB_ALLPASS_MAX];
static uint32_t       gAllpassPos[REVERB_ALLPASS];
static double         gEnvLevel[MAX_ENGINE_NODES];
static uint32_t       gEnvStage[MAX_ENGINE_NODES];

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
static double glide_time_seconds(uint8_t setting) {
    const char * text = get_glide_time_str(setting);

    return (text != NULL) ? ((double)atoi(text) / 1000.0) : 0.019;
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

void sound_engine_set_sample_rate(double sampleRate) {
    if (sampleRate > 0.0) {
        gSampleRate = sampleRate;
    }
}

static void reset_node_state(void) {
    uint32_t i = 0;

    for (i = 0; i < MAX_ENGINE_NODES; i++) {
        // Spread rather than zeroed, for the same reason the note-on path leaves them alone: from
        // the very first note the oscillators should be at unrelated points in their cycles. The
        // step is irrational-ish so no two land together.
        gLfoLastPhase[i]  = 0.0;
        gLfoTarget[i]     = 0.0;
        gLfoHeld[i]       = 0.0;
        gOscHistoryPos[i] = 0;
        memset(gOscHistory[i], 0, sizeof(gOscHistory[i]));
        gPhase[i]         = fmod((double)i * 0.381966, 1.0);
        gSuperPhase[i][0] = 0.0;
        gSuperPhase[i][1] = 0.0;
        gLadder[i][0]     = 0.0;
        gLadder[i][1]     = 0.0;
        gLadder[i][2]     = 0.0;
        gLadder[i][3]     = 0.0;
        gEnvLevel[i]      = 0.0;
        gEnvStage[i]      = eEnvIdle;
        gChorusWrite[i]   = 0;
        gChorusLfo[i]     = 0.0;
        gCompEnv[i]       = 0.0;
        memset(gChorusLine[i], 0, sizeof(gChorusLine[i]));
    }

    memset(gDelayLine, 0, sizeof(gDelayLine));
    memset(gDelayWrite, 0, sizeof(gDelayWrite));
    memset(gDelayDamp, 0, sizeof(gDelayDamp));
    memset(gComb, 0, sizeof(gComb));
    memset(gCombPos, 0, sizeof(gCombPos));
    memset(gCombStore, 0, sizeof(gCombStore));
    memset(gAllpass, 0, sizeof(gAllpass));
    memset(gAllpassPos, 0, sizeof(gAllpassPos));
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
    gNoteRead    = atomic_load(&gNoteWrite);
    reset_node_state();
    gEnvelope    = 0.0;
    gGateOpen    = false;
    gVoiceNote   = -1;
    gGlidePitch  = -1.0;
    gGlideActive = false;

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
        case eStatusNoSelection:
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
            snprintf(text, sizeof(text), "Playing %u module%s%s", (unsigned)gPlayingCount,
                     (gPlayingCount == 1) ? "" : "s",
                     (midi_input_connected_count() > 0) ? " - MIDI in connected" : " - use the Virtual Keyboard");
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
        "Chorus", "Compress", "Delay",  "Reverb", "Lfo",     "Const", "FxIn","PassThru", "Out"
    };

    used += (size_t)snprintf(text + used, sizeof(text) - used,
                             "active=%d status=%d nodes=%u tap=%d variation=%u peak=%.3f rawpeak=%.3f\n",
                             (int)atomic_load(&gActive), (int)gStatus, (unsigned)gParams.nodeCount,
                             (int)gParams.tap, (unsigned)gPatchDescr[gSlot].activeVariation,
                             (double)atomic_exchange(&gPeakMilli, 0) / 1000.0,
                             (double)atomic_exchange(&gRawPeakMilli, 0) / 1000.0);

    for (i = 0; (i < gParams.nodeCount) && (used < sizeof(text)); i++) {
        const tEngineNode * n = &gParams.node[i];

        used += (size_t)snprintf(text + used, sizeof(text) - used,
                                 "[%u] %-8s mod=%u in=%d/%d/%d/%d src=%u/%u/%u/%u active=%d "
                                 "wave=%d kbt=%d pitch=%.2f shape=%.2f "
                                 "cut=%.1f res=%.2f poles=%u env=%.2f fltkbt=%.2f "
                                 "a=%.3f d=%.3f s=%.2f r=%.3f gain=%.2f time=%.3f mix=%.2f fb=%.2f\n",
                                 (unsigned)i,
                                 (n->kind < (sizeof(kindName) / sizeof(kindName[0]))) ? kindName[n->kind] : "?",
                                 (unsigned)n->moduleIndex,
                                 (int)n->in[0], (int)n->in[1], (int)n->in[2], (int)n->in[3],
                                 (unsigned)n->srcOut[0], (unsigned)n->srcOut[1],
                                 (unsigned)n->srcOut[2], (unsigned)n->srcOut[3], (int)n->active,
                                 (int)n->wave, (int)n->oscKbt, n->basePitch, n->shape,
                                 n->cutoffHz, n->resonance, (unsigned)n->extraPoles, n->modAmount, n->fltKbt,
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

    // Only the gate closes on note-off. gVoiceNote holds the pitch it was last played at, because
    // the release is still sounding and has to keep that pitch.
    if ((gNoteQueue[slot].on == true) && (gNoteQueue[slot].note >= 0)) {
        // Auto glide only slides between overlapping notes, which is the point of it: a phrase
        // played legato slides, a detached note starts where it means to. Whether the gate is
        // already open is exactly that test, so it has to be read BEFORE this note opens it.
        gGlideActive = (gGateOpen == true);
        gVoiceNote   = gNoteQueue[slot].note;
        gGateOpen    = true;

        if (gGlidePitch < 0.0) {
            gGlidePitch = (double)gVoiceNote;   // first note of all: start where it is played
        }
    } else {
        gGateOpen = false;
    }
    gNoteRead++;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Building the chain (UI thread)
// ---------------------------------------------------------------------------------------------

// An exponential fit across the G2's envelope range. See ENV_TIME_MIN/MAX.
static double env_time_seconds(double paramValue) {
    return ENV_TIME_MIN * pow(ENV_TIME_MAX / ENV_TIME_MIN, paramValue / 127.0);
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
        {
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
static uint32_t input_connectors(tNodeKind kind, bool stereoMix, const uint32_t ** connectors) {
    static const uint32_t oneIn[]       = {CONNECTOR_IN_A};
    static const uint32_t twoIn[]       = {CONNECTOR_IN_A, CONNECTOR_IN_B};
    static const uint32_t filterIn[]    = {CONNECTOR_IN_A, FLT_CONNECTOR_ENV_IN};
    static const uint32_t mixIn[]       = {0, 1, 2, 3};
    static const uint32_t mixStereoIn[] = {2, 3, 4, 5, 6, 7, 8, 9}; // In1L,In1R .. In4L,In4R
    static const uint32_t envIn[]       = {0};                      // connector 0 is the audio the envelope shapes
    // OscB has "two pitch modulation inputs, one frequency modulation input, one sync modulation
    // input and a Shape modulation input" (manual, OscB). The two pitch inputs are the control-rate
    // pair at 0 and 1; both are summed and scaled by the one Pitch knob the module carries.
    static const uint32_t oscIn[]       = {0, 1};
    static const uint32_t none[]        = {0};

    switch (kind) {
        case eNodeFilter:
        {
            *connectors = filterIn;
            return 2;
        }
        case eNodeOsc:
        case eNodeOscShp:
        {
            *connectors = oscIn;
            return 2;
        }
        case eNodeLevMult:
        case eNodeOut:
        {
            *connectors = twoIn;
            return 2;
        }
        case eNodeMix:
        {
            // Mix4to1C's four inputs are its first four connectors. Mix4to1S puts its two outputs
            // first and then interleaves the inputs as stereo pairs, so all eight legs are read and
            // summed to mono, with each channel's level applied to both of its legs.
            *connectors = stereoMix ? mixStereoIn : mixIn;
            return stereoMix ? 8 : 4;
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
static tModule * voice_area_output(uint32_t slot) {
    uint32_t index = 0;

    for (index = 0; index < MAX_NUM_MODULES; index++) {
        tModule * module = get_module_slot(slot, (uint32_t)locationVa, index);

        if ((module != NULL) && ((module->type == moduleType2toOut) || (module->type == moduleType4toOut))) {
            return module;
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
    int32_t       resolvedIn[MAX_NODE_INPUTS]     = {-1, -1, -1, -1};
    uint32_t      resolvedSrcOut[MAX_NODE_INPUTS] = {0};
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
        const uint32_t * connectors = NULL;
        uint32_t         count      = input_connectors(kind, module->type == moduleTypeMix4to1S, &connectors);
        uint32_t         c          = 0;

        for (c = 0; c < count; c++) {
            uint32_t  sourceOutput = 0;
            tModule * source       = module_feeding(module, connectors[c], &sourceOutput);

            resolvedIn[c]     = add_node(params, source, variation, depth + 1);
            resolvedSrcOut[c] = sourceOutput;
        }

        inCount = count;

        // An Fx-In takes no cable: it carries whatever the Voice area's Out sends across. Follow
        // that link explicitly, or a patch whose real output lives in the FX area looks like it has
        // nothing patched into it and plays silence.
        if (kind == eNodeFxIn) {
            resolvedIn[0]     = add_node(params, voice_area_output(module->key.slot), variation, depth + 1);
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
            node->shape     = osc_shape_percent(param_value(module, variation, SHPB_PARAM_SHAPE)) / 100.0;
            node->modAmount = param_value(module, variation, SHPB_PARAM_PITCH_MOD) / 127.0;
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
            // Threshold and reference run 0..42 on the dial, ratio 0..66. Read as dB below 0 and as
            // a ratio from 1:1 upwards — an approximation of the curve, not a reading of it.
            double thrDb = -42.0 + param_value(module, variation, COMP_PARAM_THRESHOLD);
            double att   = param_value(module, variation, COMP_PARAM_ATTACK) / 127.0;
            double rel   = param_value(module, variation, COMP_PARAM_RELEASE) / 127.0;

            node->threshold    = pow(10.0, thrDb / 20.0);
            node->ratio        = 1.0 + (param_value(module, variation, COMP_PARAM_RATIO) / 8.0);
            // 0.1 ms .. 300 ms attack, 10 ms .. 3 s release, as one-pole coefficients.
            node->attackCoeff  = 1.0 - exp(-1.0 / (gSampleRate * (0.0001 * pow(3000.0, att))));
            node->releaseCoeff = 1.0 - exp(-1.0 / (gSampleRate * (0.01 * pow(300.0, rel))));
            node->active       = (param_value(module, variation, COMP_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeDelay:
        {
            // The range selector is a mode. Its four settings are progressively longer maximum
            // times; the dial then scales within the chosen one.
            // The four range settings, straight off delayABRangeStrMap: 500ms, 1.0s, 2.0s, 2.7s.
            static const double rangeSeconds[] = {0.5, 1.0, 2.0, 2.7};
            uint32_t            range          = module->mode[DELAY_MODE_RANGE].value;
            double              maxTime        = rangeSeconds[(range < 4) ? range : 3];

            node->timeSeconds = maxTime * (param_value(module, variation, DELAY_PARAM_TIME) / 127.0);
            node->depth       = param_value(module, variation, DELAY_PARAM_FEEDBACK) / 127.0 * 0.95;
            node->damping     = param_value(module, variation, DELAY_PARAM_LP) / 127.0;
            node->amount      = param_value(module, variation, DELAY_PARAM_DRYWET) / 127.0;
            node->active      = (param_value(module, variation,
                                             (module->type == moduleTypeDelayA)
                                             ? DELAYA_PARAM_ACTIVE : DELAYB_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeReverb:
        {
            node->timeSeconds = 0.2 + (3.0 * (param_value(module, variation, REVERB_PARAM_TIME) / 127.0));
            node->damping     = param_value(module, variation, REVERB_PARAM_BRIGHT) / 127.0;
            node->amount      = param_value(module, variation, REVERB_PARAM_DRYWET) / 127.0;
            node->active      = (param_value(module, variation, REVERB_PARAM_ACTIVE) != 0.0);
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

            for (c = 0; c < MAX_NODE_INPUTS; c++) {
                double knob = param_value(module, variation, MIX_PARAM_LEVEL_BASE + c) / 127.0;

                // Approximation: the exponential taper is squared rather than the hardware's exact
                // "-infinity to 0 dB" attenuator law, which the manual does not state numerically.
                node->level[c] = (linear ? knob : (knob * knob)) * pad;
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
            node->modAmount = param_value(module, variation, OSCB_PARAM_PITCH_MOD) / 127.0;
            node->shape     = osc_shape_percent(param_value(module, variation, OSCB_PARAM_SHAPE)) / 100.0;
            node->active    = (param_value(module, variation, OSCB_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeFilter:
        {
            node->cutoffHz   = flt_cutoff_hz(param_value(module, variation, FLT_PARAM_FREQ));
            node->resonance  = param_value(module, variation, FLT_PARAM_RES) / 127.0;
            node->extraPoles = flt_slope_extra_poles((uint32_t)param_value(module, variation, FLT_PARAM_SLOPE));
            node->fltKbt     = flt_kbt_amount((uint32_t)param_value(module, variation, FLT_PARAM_KBT));
            node->modAmount  = param_value(module, variation, FLT_PARAM_ENV) * 2.0 / 128.0;
            node->active     = (param_value(module, variation, FLT_PARAM_ACTIVE) != 0.0);
            break;
        }
        case eNodeEnv:
        {
            node->attack  = env_time_seconds(param_value(module, variation, ENV_PARAM_ATTACK));
            node->decay   = env_time_seconds(param_value(module, variation, ENV_PARAM_DECAY));
            node->sustain = param_value(module, variation, ENV_PARAM_SUSTAIN) / 127.0;
            node->release = env_time_seconds(param_value(module, variation, ENV_PARAM_RELEASE));
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
            node->active = (param_value(module, variation, OUT_PARAM_ACTIVE) != 0.0);
            node->gain   = (param_value(module, variation, OUT_PARAM_PAD) != 0.0) ? 0.5 : 1.0;
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

            if ((module->type == moduleType2toOut) || (module->type == moduleType4toOut)) {
                return module;
            }
        }
    }

    return NULL;
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

    // You hear what you select, plus everything upstream of it the engine understands. With nothing
    // selected it falls back to the patch's own Out, so a finished patch just plays.
    if (gSelection.count > 1) {
        gStatus = eStatusMultipleSelected;
    } else {
        if (gSelection.count == 1) {
            tapModule = get_module(gSelection.keys[0]);
        } else {
            tapModule = find_output_module();
        }

        if (tapModule == NULL) {
            gStatus = eStatusNoSelection;
        } else {
            tNodeKind kind = eNodeOsc;

            variation = gPatchDescr[tapModule->key.slot].activeVariation;

            if (module_kind(tapModule, &kind) == false) {
                gStatus = eStatusUnsupportedModule;
            } else {
                snapshot.tap = add_node(&snapshot, tapModule, variation, 0);

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
    snapshot.topology = topology_signature(&snapshot);

    atomic_fetch_add(&gParamsSeq, 1);    // now odd — a reader seeing this discards its copy
    gParams           = snapshot;
    atomic_fetch_add(&gParamsSeq, 1);    // even again, snapshot is whole
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
    double t = (shape - 0.5) / 0.49;

    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }

    switch (waveform) {
        case 0:
        {
            // Sine1 — "a phase modulated sine wave... at 99% similar to a sawtooth". A two segment
            // phase warp: at 50% the warp is the identity and this is a plain sine; as Shape opens,
            // the first half of the cosine is crammed into an ever smaller part of the cycle, giving
            // a fast rise and a long fall.
            double d = (0.5 * (1.0 - t)) + (0.02 * t);
            double w = (phase < d) ? (0.5 * phase / d)
                       : (0.5 + (0.5 * (phase - d) / (1.0 - d)));

            return -cos(w * 2.0 * M_PI);
        }
        case 1:
        {
            // Sine2 — "Sine -> Double Sine". The two half cycles keep their shape but not their
            // width: at 99% the first covers almost the whole period and the second is a spike.
            double d = (0.5 * (1.0 - t)) + (0.99 * t);

            return (phase < d) ? sin(M_PI * phase / d)
                   : -sin(M_PI * (phase - d) / (1.0 - d));
        }
        case 2:
        {
            // Sine3 — "Sine -> Even harmonics". Rectification is what produces even harmonics, so
            // blend towards a full wave rectified sine with its DC removed.
            double raw = sin(phase * 2.0 * M_PI);

            return ((1.0 - t) * raw) + (t * ((2.0 * fabs(raw)) - 1.0));
        }
        case 3:
        {
            // Sine4 — "Sine -> Odd harmonics". A square holds only odd harmonics, so drive the sine
            // progressively harder into a soft clip, which approaches one.
            double raw   = sin(phase * 2.0 * M_PI);
            double drive = 1.0 + (30.0 * t);

            return tanh(drive * raw) / tanh(drive);
        }
        case 4:
        {
            // TriSaw — "Triangle -> Sawtooth", perfect triangle at 50% and a perfect saw at 99%.
            return osc_triangle(phase, (0.5 * (1.0 - t)) + (0.99 * t));
        }
        case 5:
        {
            // DblSaw — two sawtooths, the second drifting away from the first as Shape opens.
            // Approximate: the manual does not describe this one in the detail it gives the others.
            double second = fmod(phase + (0.5 * t), 1.0);

            return (osc_saw(phase, dt) + osc_saw(second, dt)) * 0.5;
        }
        case 6:
        {
            // Pulse — square at 50%, down to a 1% pulse at 99%. This one is a plain pulse width, so
            // the band limited square already does it.
            return osc_square(phase, dt, shape);
        }
        default:
        {
            // SymPulse — a SYMMETRIC pulse width: a positive pulse and an equally narrow negative
            // one, square at 50% and 1% wide at 99%. Not band limited; at narrow widths it is a
            // brighter waveform than this engine renders cleanly.
            double w = (0.5 * (1.0 - t)) + (0.01 * t);

            if (phase < w) {
                return 1.0;
            }

            if (phase < 0.5) {
                return 0.0;
            }

            if (phase < (0.5 + w)) {
                return -1.0;
            }
            return 0.0;
        }
    }
}

// A delay line with feedback and a one-pole damping filter in the loop — the usual arrangement, and
// what the LP knob on the module controls.
static double delay_step(uint32_t line, double input, double timeSeconds, double feedback,
                         double damping, double mix) {
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

    gDelayLine[line][gDelayWrite[line]] = (float)(input + (gDelayDamp[line] * feedback));
    gDelayWrite[line]                   = (gDelayWrite[line] + 1) % DELAY_LINE_SAMPLES;

    return (input * (1.0 - mix)) + (wet * mix);
}

// A short delay whose length is swept by a slow LFO — detune sets the sweep depth, amount how much
// of it is mixed in. Stereo on the hardware; mono here, since the engine sums to mono anyway.
static double chorus_step(uint32_t node, double input, double depth, double amount) {
    double   sweep   = 0.0;
    uint32_t samples = 0;
    uint32_t readPos = 0;
    double   wet     = 0.0;

    gChorusLfo[node]                     += 0.7 / gSampleRate; // a slow sweep, under a hertz

    if (gChorusLfo[node] >= 1.0) {
        gChorusLfo[node] -= 1.0;
    }
    // 4 ms centre, swept by up to 3 ms either way.
    sweep                                 = 0.004 + (0.003 * depth * sin(gChorusLfo[node] * 2.0 * M_PI));
    samples                               = (uint32_t)(sweep * gSampleRate);

    if (samples < 1) {
        samples = 1;
    } else if (samples >= CHORUS_SAMPLES) {
        samples = CHORUS_SAMPLES - 1;
    }
    readPos                               = (gChorusWrite[node] + CHORUS_SAMPLES - samples) % CHORUS_SAMPLES;
    wet                                   = (double)gChorusLine[node][readPos];

    gChorusLine[node][gChorusWrite[node]] = (float)input;
    gChorusWrite[node]                    = (gChorusWrite[node] + 1) % CHORUS_SAMPLES;

    // A balanced mix rather than wet piled on top of dry. It used to return input + wet, which at a
    // high Amount is close to double gain — a chorus should change the character at roughly constant
    // level, not act as a 6 dB boost.
    return (input * (1.0 - (amount * 0.5))) + (wet * amount * 0.5);
}

// Peak-following compressor. Above the threshold the excess is divided by the ratio; the follower
// has separate attack and release so it grabs quickly and lets go slowly.
static double compress_step(uint32_t node, double input, const tEngineNode * spec) {
    double level = fabs(input);
    double gain  = 1.0;

    if (level > gCompEnv[node]) {
        gCompEnv[node] += spec->attackCoeff * (level - gCompEnv[node]);
    } else {
        gCompEnv[node] += spec->releaseCoeff * (level - gCompEnv[node]);
    }

    if ((gCompEnv[node] > spec->threshold) && (spec->threshold > 0.0)) {
        double over = gCompEnv[node] / spec->threshold;

        gain = pow(over, (1.0 / spec->ratio) - 1.0);
    }
    return input * gain;
}

// Schroeder reverb — parallel combs for density, allpasses to smear the result.
static double reverb_step(double input, double timeSeconds, double damping, double mix) {
    double   diffused = input;
    double   sum      = 0.0;
    uint32_t i        = 0;

    // Diffusion first: three short allpasses smear the input within a few milliseconds, so there is
    // something there before the combs respond and no single tap stands out as an echo.
    for (i = 0; i < REVERB_ALLPASS; i++) {
        uint32_t len = kAllpassLen[i];
        double   out = (double)gAllpass[i][gAllpassPos[i]];
        double   in  = diffused + (out * 0.5);

        gAllpass[i][gAllpassPos[i]] = (float)in;
        gAllpassPos[i]              = (gAllpassPos[i] + 1) % len;
        diffused                    = out - (in * 0.5);
    }

    // Then the combs, in parallel, for the tail.
    for (i = 0; i < REVERB_COMBS; i++) {
        uint32_t len = kCombLen[i];
        double   out = (double)gComb[i][gCombPos[i]];
        // Feedback set so the tail decays to -60 dB over the chosen time.
        double   fb  = pow(0.001, ((double)len / gSampleRate) / timeSeconds);

        gCombStore[i]        += (1.0 - damping) * (out - gCombStore[i]);
        gComb[i][gCombPos[i]] = (float)(diffused + (gCombStore[i] * fb));
        gCombPos[i]           = (gCombPos[i] + 1) % len;
        sum                  += out;
    }

    // A little of the diffused input so the onset is early rather than waiting for the shortest comb
    // at ~23 ms. Kept low: the allpass chain is near enough flat in magnitude, so too much of this
    // reads as the dry signal leaking back through a send that is supposed to be fully wet.
    sum = (sum * 0.25) + (diffused * 0.12);

    return (input * (1.0 - mix)) + (sum * mix);
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
static double ladder_saturate(double x) {
    if (x <= -1.0) {
        return -2.0 / 3.0;
    }

    if (x >= 1.0) {
        return 2.0 / 3.0;
    }
    return x - ((x * x * x) / 3.0);
}

static double ladder_filter(double * state, double input, double g, double k, uint32_t tapStage) {
    double   feedback = state[LADDER_POLES - 1];
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

    for (i = 0; i < LADDER_POLES; i++) {
        state[i] += g * (x - state[i]);
        x         = state[i];
    }

    return state[tapStage];
}

// One ADSR step. Times are in seconds; each stage moves linearly towards its target, which is
// plenty for shaping a note and keeps the stage logic obvious.
static double envelope_step(uint32_t node, const tEngineNode * spec, bool gate) {
    double level = gEnvLevel[node];
    double step  = 0.0;

    if (gate == true) {
        // Retrigger from Release as well as from Idle. Only accepting Idle meant a note played
        // before the previous release had finished was ignored until it had: the envelope carried on
        // FALLING, holding the filter part open, and the attack began late from wherever it landed.
        // Attacking from the current level is what an ADSR does — the level is deliberately not
        // zeroed, so a fast retrigger rises from where it was rather than clicking to nothing first.
        if ((gEnvStage[node] == eEnvIdle) || (gEnvStage[node] == eEnvRelease)) {
            gEnvStage[node] = eEnvAttack;
        }
    } else if (gEnvStage[node] != eEnvIdle) {
        gEnvStage[node] = eEnvRelease;
    }

    switch (gEnvStage[node]) {
        case eEnvAttack:
        {
            step   = 1.0 / (spec->attack * gSampleRate);
            level += step;

            if (level >= 1.0) {
                level           = 1.0;
                gEnvStage[node] = eEnvDecay;
            }
            break;
        }
        case eEnvDecay:
        {
            step   = (1.0 - spec->sustain) / (spec->decay * gSampleRate);
            level -= step;

            if (level <= spec->sustain) {
                level           = spec->sustain;
                gEnvStage[node] = eEnvSustain;
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
            step   = 1.0 / (spec->release * gSampleRate);
            level -= step;

            if (level <= 0.0) {
                level           = 0.0;
                gEnvStage[node] = eEnvIdle;
            }
            break;
        }
        default:
        {
            level = 0.0;
            break;
        }
    }
    gEnvLevel[node] = level;
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
static double osc_waveform(uint32_t node, const tEngineNode * spec, double phase, double dt) {
    // The shape oscillators have their own eight waveforms, and Shape morphs each of them rather
    // than acting as a pulse width, so they do not share the switch below.
    if (spec->kind == eNodeOscShp) {
        return osc_shp_wave((uint32_t)spec->wave, phase, dt, spec->shape);
    }

    switch (spec->wave) {
        case eOscWaveSine:
        {
            return sin(phase * 2.0 * M_PI);
        }
        case eOscWaveTriangle:
        {
            return osc_triangle(phase, spec->shape);
        }
        case eOscWaveSaw:
        {
            return osc_saw(phase, dt);
        }
        case eOscWaveSquare:
        {
            return osc_square(phase, dt, spec->shape);
        }
        case eOscWaveSuper:
        {
            // Approximation: three saws a few cents apart. The G2's own "sup" is a different
            // algorithm — see the header.
            double up   = dt * 1.0059;    // about +10 cents
            double down = dt * 0.9941;    // about -10 cents
            double sum  = osc_saw(phase, dt);

            sum += osc_saw(advance_phase(&gSuperPhase[node][0], up), up);
            sum += osc_saw(advance_phase(&gSuperPhase[node][1], down), down);
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
static double oscillator_step(uint32_t node, const tEngineNode * spec, double voicePitch,
                              double pitchDirect, double pitchVar) {
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
    frequency = 440.0 * pow(2.0, (pitch - MIDI_NOTE_A440) / 12.0);

    // Above Nyquist there is no waveform left to produce, only aliasing. Return silence rather than
    // just stopping the phase: a halted sawtooth is not silence, it is a DC offset held at whatever
    // level the waveform sat at, which thumps. The limit stays the OUTPUT rate's Nyquist even though
    // the oscillator now runs faster, because the decimator would remove anything above it anyway.
    if (frequency > (gSampleRate * 0.5)) {
        return 0.0;
    }
    dt        = frequency / (gSampleRate * (double)OSC_OVERSAMPLE);

    for (step = 0; step < OSC_OVERSAMPLE; step++) {
        double phase = advance_phase(&gPhase[node], dt);

        gOscHistory[node][gOscHistoryPos[node]] = osc_waveform(node, spec, phase, dt);
        gOscHistoryPos[node]                    = (gOscHistoryPos[node] + 1) % OSC_DECIMATE_TAPS;
    }

    // One output for every OSC_OVERSAMPLE inputs, so the filter only has to be evaluated at the
    // output rate however high the oversampling factor is.
    for (tap = 0; tap < OSC_DECIMATE_TAPS; tap++) {
        uint32_t oldest = (gOscHistoryPos[node] + tap) % OSC_DECIMATE_TAPS;

        sum += gOscHistory[node][oldest] * gOscDecimate[OSC_DECIMATE_TAPS - 1 - tap];
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
static double lfo_step(uint32_t node, const tEngineNode * spec) {
    double phase = advance_phase(&gPhase[node], spec->rateHz / gSampleRate);
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
            case 4:                                                                     // Tri>Squ
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
                if (phase < gLfoLastPhase[node]) {
                    gLfoTarget[node] = ((double)rand() / (double)RAND_MAX * 2.0) - 1.0;
                }
                wave = (spec->wave == 4) ? gLfoTarget[node]
                       : (gLfoHeld[node] + ((gLfoTarget[node] - gLfoHeld[node]) * phase));

                if (phase < gLfoLastPhase[node]) {
                    gLfoHeld[node] = gLfoTarget[node];
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
    gLfoLastPhase[node] = phase;

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

static double filter_step(uint32_t node, const tEngineNode * spec, double input, double mod, double voicePitch) {
    double cutoff = spec->cutoffHz;
    double g      = 0.0;

    if (spec->active == false) {
        return input;    // a bypassed filter passes its input straight through
    }

    // Whatever is patched into the Env input sweeps the cutoff, scaled by the Env knob. An envelope
    // there is what turns a static filter into one that opens and closes with the note.
    if ((spec->modAmount > 0.0) && (mod != 0.0)) {
        cutoff *= pow(2.0, mod * spec->modAmount * FULL_MOD_SEMITONES / 12.0);
    }

    // Kbt moves the cutoff with the note, relative to middle C, at the percentage the scroll button
    // selects (manual p.196).
    // Tracks the SOUNDING pitch, so a glide carries the cutoff with it rather than snapping.
    if ((spec->fltKbt > 0.0) && (voicePitch >= 0.0)) {
        cutoff *= pow(2.0, (voicePitch - MIDI_NOTE_MIDDLE_C) * spec->fltKbt / 12.0);
    }

    if (cutoff > (gSampleRate * 0.45)) {
        cutoff = gSampleRate * 0.45;
    }

    if (cutoff < 1.0) {
        cutoff = 1.0;
    }
    g = 1.0 - exp(-2.0 * M_PI * cutoff / gSampleRate);

    // Up to just under 4, where a ladder self-oscillates. Stopping short keeps the resonance
    // dramatic without the filter screaming on its own with no note played.
    return ladder_filter(gLadder[node], input, g, 3.9 * spec->resonance, 1 + spec->extraPoles);
}

void sound_engine_render(float * out, uint32_t frameCount, uint32_t channelCount) {
    tSoundEngineParams params;
    uint32_t           frame            = 0;
    bool               chainHasEnvelope = false;
    double             voicePitch       = 0.0;
    uint32_t           n                = 0;

    if ((out == NULL) || (channelCount == 0)) {
        return;
    }
    memset(out, 0, (size_t)frameCount * channelCount * sizeof(float));

    if (atomic_load(&gActive) == false) {
        return;
    }
    params = read_params();

    if (params.topology != gSeenTopology) {
        gSeenTopology = params.topology;
        reset_node_state();
    }
    // Oscillator phases are deliberately NOT reset when a note starts. They free-run, as the G2's do
    // unless something is patched to their Sync input, and that matters more than it sounds: several
    // oscillators detuned by a few cents are what makes a patch thick, and starting them all at
    // phase zero has them summing as one voice for the seconds a 7 cent difference takes to drift
    // apart. Note events themselves are taken inside the sample loop below.

    if ((params.tap < 0) && (gEnvelope <= 0.0)) {
        return;
    }

// An EnvADSR in the chain is the note's shape; the fixed ramp is only there to stop a click when
// there is no envelope module to do the job.
    for (n = 0; n < params.nodeCount; n++) {
        if (params.node[n].kind == eNodeEnv) {
            chainHasEnvelope = true;
            break;
        }
    }

    for (frame = 0; frame < frameCount; frame++) {
        double value[MAX_ENGINE_NODES][2];

        // One event per sample. A chord's worth of note-ons arriving together therefore lands over
        // consecutive samples rather than all but the last being thrown away, and every note takes
        // effect where it actually arrived instead of at the next buffer boundary.
        (void)take_next_note_event();

        // Portamento. The sounding pitch chases the played note; how fast, and whether at all, comes
        // from the patch's Glide setting. Exponential rather than linear — it is what a glide sounds
        // like, and the coefficient is set so the remaining distance is down to a percent by the time
        // the dial says, which is close enough to the stated figure to be worth quoting.
        if (gVoiceNote >= 0) {
            bool sliding = (params.glideMode == eGlideNormal)
                           || ((params.glideMode == eGlideAuto) && (gGlideActive == true));

            if ((sliding == true) && (params.glideSeconds > 0.0)) {
                double coefficient = 1.0 - exp(-4.6 / (params.glideSeconds * gSampleRate));

                gGlidePitch += coefficient * ((double)gVoiceNote - gGlidePitch);
            } else {
                gGlidePitch = (double)gVoiceNote;
            }
        }
        // Bend rides on top of the glide, scaled by the patch's own Bend range.
        voicePitch = gGlidePitch
                     + (((double)atomic_load(&gBendMilli) / 1000.0) * params.bendSemitones);

        // The patch's own Vibrato, which is nothing to do with the cabling: it lives on a hidden
        // module beside Glide and Bend, and is how a patch gets aftertouch vibrato without an LFO
        // anywhere in it. The chosen controller sets the depth, so at rest there is none.
        if (params.vibratoSource != eVibratoOff) {
            uint32_t group = (params.vibratoSource == eVibratoWheel)
                             ? MORPH_GROUP_WHEEL : MORPH_GROUP_AFTERTOUCH;
            double   depth = (double)atomic_load(&gMorphMilli[group]) / 1000.0;

            gVibratoPhase += params.vibratoHz / gSampleRate;

            if (gVibratoPhase >= 1.0) {
                gVibratoPhase -= 1.0;
            }
            voicePitch    += (sin(gVibratoPhase * 2.0 * M_PI) * depth * params.vibratoCents) / 100.0;
        }
        double sample       = 0.0;
        double rampTarget   = ((gGateOpen == true) && (params.tap >= 0)) ? 1.0 : 0.0;
        double envelopeStep = 1.0 / (ENVELOPE_SECONDS * gSampleRate);

        if (gEnvelope < rampTarget) {
            gEnvelope += envelopeStep;

            if (gEnvelope > rampTarget) {
                gEnvelope = rampTarget;
            }
        } else if (gEnvelope > rampTarget) {
            gEnvelope -= envelopeStep;

            if (gEnvelope < rampTarget) {
                gEnvelope = rampTarget;
            }
        }

        // One forward pass. add_node() built the list depth first, so every node's inputs sit at
        // lower indices and are already evaluated by the time it is reached. Each node publishes two
        // outputs because some modules have two that mean different things — see tEngineNode.
        for (n = 0; n < params.nodeCount; n++) {
            const tEngineNode * spec = &params.node[n];
            double              a    = signal_in(spec, value, 0);

            value[n][0] = 0.0;
            value[n][1] = 0.0;

            switch (spec->kind) {
                case eNodeLfo:
                {
                    value[n][0] = lfo_step(n, spec);
                    value[n][1] = value[n][0];
                    break;
                }
                case eNodeOsc:
                case eNodeOscShp:
                {
                    // Connector 0 is the direct Pitch input, connector 1 the knob-attenuated
                    // PitchVar — see oscillator_step().
                    value[n][0] = (spec->active == true)
                                  ? oscillator_step(n, spec, voicePitch, a, signal_in(spec, value, 1))
                                  : 0.0;
                    break;
                }
                case eNodeFilter:
                {
                    value[n][0] = filter_step(n, spec, a, signal_in(spec, value, 1), voicePitch);
                    break;
                }
                case eNodeEnv:
                {
                    double env = envelope_step(n, spec, gGateOpen);

                    // Output 0 is the envelope itself, for patching at a modulation input. Output 1
                    // is whatever audio is patched into the module, shaped by that envelope — the
                    // G2's envelopes carry their own VCA, and this patch uses it as the amp.
                    value[n][0] = env;
                    value[n][1] = a * env;
                    break;
                }
                case eNodeLevAmp:
                {
                    value[n][0] = a * spec->gain;
                    break;
                }
                case eNodeLevMult:
                {
                    value[n][0] = a * signal_in(spec, value, 1);
                    break;
                }
                case eNodeMix:
                {
                    uint32_t c = 0;

                    // A stereo mixer reads eight legs but has only four level knobs, so both legs
                    // of a channel share one.
                    for (c = 0; c < spec->inCount; c++) {
                        uint32_t channel = (spec->inCount > MAX_NODE_INPUTS / 2) ? (c / 2) : c;

                        value[n][0] += signal_in(spec, value, c) * spec->level[channel];
                    }

                    break;
                }
                case eNodeChorus:
                {
                    value[n][0] = (spec->active == true)
                                  ? chorus_step(n, a, spec->depth, spec->amount) : a;
                    value[n][1] = value[n][0];   // stereo on the hardware, summed to mono here
                    break;
                }
                case eNodeCompress:
                {
                    value[n][0] = (spec->active == true) ? compress_step(n, a, spec) : a;
                    value[n][1] = value[n][0];
                    break;
                }
                case eNodeDelay:
                {
                    value[n][0] = (spec->active == true)
                                  ? delay_step(spec->line, a, spec->timeSeconds, spec->depth,
                                               spec->damping, spec->amount) : a;
                    value[n][1] = value[n][0];
                    break;
                }
                case eNodeReverb:
                {
                    // Only the first reverb in a chain is modelled; see the DSP note above.
                    double in = (a + signal_in(spec, value, 1)) * 0.5;

                    value[n][0] = ((spec->active == true) && (spec->line == 0))
                                  ? reverb_step(in, spec->timeSeconds, spec->damping, spec->amount) : in;
                    value[n][1] = value[n][0];
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
                    value[n][0] = (spec->active == true) ? (a * spec->gain) : 0.0;
                    value[n][1] = value[n][0];
                    break;
                }
                case eNodePassThru:
                {
                    value[n][0] = a;
                    value[n][1] = a;   // stereo pairs feed both legs from the one signal
                    break;
                }
                case eNodeOut:
                {
                    // Sums its inputs — the two legs are the left and right channels, and the engine
                    // is mono to the speakers for now.
                    value[n][0] = (spec->active == true)
                                  ? ((a + signal_in(spec, value, 1)) * spec->gain) : 0.0;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        if (params.tap >= 0) {
            // Tapping a module means listening to its main output; for an envelope used as an amp
            // that is its shaped audio rather than the envelope signal.
            sample = value[params.tap][(params.node[params.tap].kind == eNodeEnv) ? 1 : 0];
        }
        // With an envelope module shaping the note, the fixed ramp would only double up on it; it is
        // still applied when the chain has none.
        {
            uint32_t rawMilli = (uint32_t)(fabs(sample) * 1000.0);

            if (rawMilli > atomic_load(&gRawPeakMilli)) {
                atomic_store(&gRawPeakMilli, rawMilli);
            }
        }
        sample *= VOICE_GAIN;

        if (chainHasEnvelope == false) {
            sample *= gEnvelope;
        }

        // Soft knee rather than a hard edge. Below the knee nothing is touched at all, so ordinary
        // playing is untouched; above it the curve bends over instead of shearing the tops off, which
        // is both kinder to listen to and closer to what an overloaded analogue output does. The hard
        // clamp afterwards is only a guard against a bug producing something enormous.
        if (sample > OUTPUT_KNEE) {
            sample = OUTPUT_KNEE + ((1.0 - OUTPUT_KNEE) * tanh((sample - OUTPUT_KNEE) / (1.0 - OUTPUT_KNEE)));
        } else if (sample < -OUTPUT_KNEE) {
            sample = -OUTPUT_KNEE - ((1.0 - OUTPUT_KNEE) * tanh((-sample - OUTPUT_KNEE) / (1.0 - OUTPUT_KNEE)));
        }

        if (sample > 1.0) {
            sample = 1.0;
        } else if (sample < -1.0) {
            sample = -1.0;
        }
        {
            uint32_t channel = 0;
            uint32_t milli   = (uint32_t)(fabs(sample) * 1000.0);

            if (milli > atomic_load(&gPeakMilli)) {
                atomic_store(&gPeakMilli, milli);
            }

            for (channel = 0; channel < channelCount; channel++) {
                out[(frame * channelCount) + channel] = (float)sample;
            }
        }
    }
}

#ifdef __cplusplus
}
#endif
