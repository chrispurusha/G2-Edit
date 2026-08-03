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

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "dataBase.h"
#include "cableChain.h"
#include "globalVars.h"
#include "renderParams.h"
#include "audioOutput.h"
#include "midiInput.h"
#include "soundEngine.h"

// See soundEngine.h for what this does and does not attempt.

// Parameter indices, in the order moduleResources.h lists them for each module type.
#define OSCB_PARAM_TUNE          (0)
#define OSCB_PARAM_CENT          (1)
#define OSCB_PARAM_KBT           (2)
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

// OscShpB lays its parameters out differently from OscB — Active is 8, not 9, and the waveform is
// at 10 with eight choices rather than at 8 with five.
#define SHPB_PARAM_TUNE          (0)
#define SHPB_PARAM_CENT          (1)
#define SHPB_PARAM_KBT           (2)
#define SHPB_PARAM_PITCH_TYPE    (4)
#define SHPB_PARAM_SHAPE         (6)
#define SHPB_PARAM_ACTIVE        (8)
#define SHPB_PARAM_WAVEFORM      (10)

// Mix4to1C: one level per input, then a pad and a curve.
#define MIX_PARAM_LEVEL_BASE     (0)
#define MIX_PARAM_CURVE          (9)   // 0 = linear taper, otherwise exponential

// A node takes at most this many inputs — four, which is what the 4-into-1 mixers need.
#define MAX_NODE_INPUTS          (4)

// Which connector carries the signal into each module. Everything the walk follows is a module's
// FIRST input; LevMult and 2toOut take a second as well.
#define CONNECTOR_IN_A          (0)
#define CONNECTOR_IN_B          (1)
#define FLT_CONNECTOR_ENV_IN    (2)   // FltClassic's control input, the one beside its Env knob

// The G2 caps the total pitch modulation reaching an oscillator or filter at +/-64 semitones
// (manual p.78), which is what an Env amount of 100% corresponds to.
#define FULL_MOD_SEMITONES    (64.0)

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

#define VOICE_GAIN            (0.25)       // headroom — one voice runs nowhere near full scale
#define ENVELOPE_SECONDS      (0.005)      // the anti-click ramp used when no EnvADSR is in the chain

// Envelope times. The G2's range runs from well under a millisecond to tens of seconds; this is an
// exponential fit across that span rather than a reading of the hardware's own table, in keeping
// with the rest of the engine approximating rather than reproducing.
#define ENV_TIME_MIN    (0.0005)
#define ENV_TIME_MAX    (45.0)

// Every ladder runs its full four poles whatever slope is selected — see ladder_filter().
#define LADDER_POLES    (4)

// The chain the engine renders. Small and fixed: these are hand-built sketches, not whole patches,
// and a bound is what keeps the walk safe against a patch that feeds back into itself.
#define MAX_ENGINE_NODES    (12)

typedef enum {
    eNodeOsc = 0,        // OscB
    eNodeOscShp,         // OscShpB — a different parameter layout and waveform set
    eNodeFilter,
    eNodeLevAmp,
    eNodeLevMult,
    eNodeMix,            // Mix4to1C
    eNodeEnv,            // envelope, and a VCA for whatever is patched into its audio input
    eNodePassThru,       // an effect that is not modelled yet: passes its input along unchanged
    eNodeOut,
} tNodeKind;

typedef struct {
    tNodeKind kind;
    uint32_t  moduleIndex;   // so per-node audio state can survive a knob turn (see topology_signature)

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
} tEngineNode;

typedef struct {
    uint32_t    nodeCount;
    int32_t     tap;           // the node whose output reaches the speakers, -1 for silence
    uint64_t    topology;      // changes shape => the audio thread resets its per-node state
    tEngineNode node[MAX_ENGINE_NODES];
} tSoundEngineParams;

// Published by the UI thread, consumed by the audio thread, via a seqlock: the writer makes the
// sequence odd before touching the snapshot and even again after, so a reader that sees an odd
// sequence — or a different one either side of its copy — knows it read during a write. Neither
// side ever blocks, and the audio thread never waits on the UI thread. A plain pair of buffers
// would not do: the UI can publish twice while one audio buffer is being filled, which is long
// enough to land back on the buffer the audio thread is mid-copy of.
static tSoundEngineParams gParams         = {0};
static _Atomic uint32_t   gParamsSeq      = 0;

#define PARAMS_READ_ATTEMPTS    (4)   // then keep last good — a retry loop must not spin in audio

// Note state. The generation counter is what makes a retrigger unambiguous — restriking the same
// note changes no other field, and Repeat on the virtual keyboard does exactly that.
static _Atomic int32_t    gNote           = -1;
static _Atomic uint32_t   gNoteGeneration = 0;

static _Atomic bool       gActive         = false;

// Highest absolute sample the audio thread has produced since this was last read. Purely a
// diagnostic — it is what lets a test say "sound is coming out" without a pair of ears.
static _Atomic uint32_t   gPeakMilli      = 0;

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

static tSoundEngineStatus gStatus         = eStatusOff;
static uint32_t           gPlayingCount   = 0;   // how many modules are in the rendered chain

// Audio-thread-only state. Nothing else may touch these.
static double             gSampleRate     = 48000.0;
static uint32_t           gSeenGeneration = 0;
static bool               gGateOpen       = false;
static int32_t            gVoiceNote      = -1;
static tSoundEngineParams gLastGoodParams = {0};
static uint64_t           gSeenTopology   = 0;
static double             gEnvelope       = 0.0;   // the anti-click ramp, when no EnvADSR is present

// Per-node state, indexed by node position. Carried across snapshots while the topology signature
// holds, so turning a knob does not restart the oscillator or reopen the envelope.
static double             gPhase[MAX_ENGINE_NODES];
static double             gSuperPhase[MAX_ENGINE_NODES][2];
static double             gLadder[MAX_ENGINE_NODES][LADDER_POLES];
static double             gEnvLevel[MAX_ENGINE_NODES];
static uint32_t           gEnvStage[MAX_ENGINE_NODES];

typedef enum {
    eEnvIdle = 0,
    eEnvAttack,
    eEnvDecay,
    eEnvSustain,
    eEnvRelease,
} tEnvStage;

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
        gPhase[i]         = 0.0;
        gSuperPhase[i][0] = 0.0;
        gSuperPhase[i][1] = 0.0;
        gLadder[i][0]     = 0.0;
        gLadder[i][1]     = 0.0;
        gLadder[i][2]     = 0.0;
        gLadder[i][3]     = 0.0;
        gEnvLevel[i]      = 0.0;
        gEnvStage[i]      = eEnvIdle;
    }
}

bool sound_engine_start(void) {
    if (atomic_load(&gActive) == true) {
        return true;
    }
    // Start from silence rather than inheriting whatever the last run left behind.
    reset_node_state();
    gEnvelope  = 0.0;
    gGateOpen  = false;
    gVoiceNote = -1;

    if (audio_output_start() == false) {
        return false;
    }
    atomic_store(&gActive, true);

    // A real keyboard playing the engine, alongside the on-screen one. Not fatal if it fails: the
    // engine is perfectly usable from the Virtual Keyboard, so a missing MIDI service should not
    // stop the audio that already started.
    if (midi_input_start() == false) {
        LOG_ERROR("Sound engine: MIDI input unavailable, Virtual Keyboard only\n");
    }
    return true;
}

void sound_engine_stop(void) {
    if (atomic_load(&gActive) == false) {
        return;
    }
    // Clear the flag first: the device teardown below waits for any render in flight to finish, and
    // that render should already be seeing an inactive engine.
    atomic_store(&gActive, false);
    midi_input_stop();     // before the audio device, so no note can arrive for a stopped voice
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
                     (midi_input_source_count() > 0) ? " - MIDI in connected" : " - use the Virtual Keyboard");
            return text;
        }
        default:
        {
            return "Off";
        }
    }
}

const char * sound_engine_debug_text(void) {
    static char  text[1024];
    size_t       used       = 0;
    uint32_t     i          = 0;
    // One entry per tNodeKind, in enum order. Kept in step with it — a short array here is read off
    // the end by the kindName[n->kind] below, which is a stack overflow rather than a wrong label.
    const char * kindName[] = {
        "Osc", "OscShp", "Filter", "LevAmp", "LevMult", "Mix", "Env", "PassThru", "Out"
    };

    used += (size_t)snprintf(text + used, sizeof(text) - used,
                             "active=%d status=%d nodes=%u tap=%d variation=%u peak=%.3f\n",
                             (int)atomic_load(&gActive), (int)gStatus, (unsigned)gParams.nodeCount,
                             (int)gParams.tap, (unsigned)gPatchDescr[gSlot].activeVariation,
                             (double)atomic_exchange(&gPeakMilli, 0) / 1000.0);

    for (i = 0; (i < gParams.nodeCount) && (used < sizeof(text)); i++) {
        const tEngineNode * n = &gParams.node[i];

        used += (size_t)snprintf(text + used, sizeof(text) - used,
                                 "[%u] %-8s mod=%u in=%d/%d/%d/%d src=%u/%u/%u/%u active=%d "
                                 "wave=%d kbt=%d pitch=%.2f shape=%.2f "
                                 "cut=%.1f res=%.2f poles=%u env=%.2f "
                                 "a=%.3f d=%.3f s=%.2f r=%.3f gain=%.2f\n",
                                 (unsigned)i,
                                 (n->kind < (sizeof(kindName) / sizeof(kindName[0]))) ? kindName[n->kind] : "?",
                                 (unsigned)n->moduleIndex,
                                 (int)n->in[0], (int)n->in[1], (int)n->in[2], (int)n->in[3],
                                 (unsigned)n->srcOut[0], (unsigned)n->srcOut[1],
                                 (unsigned)n->srcOut[2], (unsigned)n->srcOut[3], (int)n->active,
                                 (int)n->wave, (int)n->oscKbt, n->basePitch, n->shape,
                                 n->cutoffHz, n->resonance, (unsigned)n->extraPoles, n->modAmount,
                                 n->attack, n->decay, n->sustain, n->release, n->gain);
    }

    return text;
}

void sound_engine_note(int32_t note, bool on) {
    atomic_store(&gNote, on ? note : -1);
    atomic_fetch_add(&gNoteGeneration, 1);
}

// ---------------------------------------------------------------------------------------------
// Building the chain (UI thread)
// ---------------------------------------------------------------------------------------------

// An exponential fit across the G2's envelope range. See ENV_TIME_MIN/MAX.
static double env_time_seconds(double paramValue) {
    return ENV_TIME_MIN * pow(ENV_TIME_MAX / ENV_TIME_MIN, paramValue / 127.0);
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
        // Not modelled, but they sit in the signal path of real patches and stopping the chain at
        // them would mean hearing nothing at all. Passing the audio through unchanged loses their
        // character but keeps everything downstream audible.
        case moduleTypeStChorus:
        case moduleTypeCompress:
        {
            *kind = eNodePassThru;
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
static uint32_t input_connectors(tNodeKind kind, const uint32_t ** connectors) {
    static const uint32_t oneIn[]    = {CONNECTOR_IN_A};
    static const uint32_t twoIn[]    = {CONNECTOR_IN_A, CONNECTOR_IN_B};
    static const uint32_t filterIn[] = {CONNECTOR_IN_A, FLT_CONNECTOR_ENV_IN};
    static const uint32_t mixIn[]    = {0, 1, 2, 3};
    static const uint32_t envIn[]    = {0};   // connector 0 is the audio the envelope shapes
    static const uint32_t none[]     = {0};

    switch (kind) {
        case eNodeFilter:
        {
            *connectors = filterIn;
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
            *connectors = mixIn;
            return 4;
        }
        case eNodeEnv:
        {
            *connectors = envIn;
            return 1;
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
            if (params->node[existing].moduleIndex == module->key.index) {
                return (int32_t)existing;
            }
        }
    }

    // Inputs first, so they land at lower node indices than this one.
    {
        const uint32_t * connectors = NULL;
        uint32_t         count      = input_connectors(kind, &connectors);
        uint32_t         c          = 0;

        for (c = 0; c < count; c++) {
            uint32_t  sourceOutput = 0;
            tModule * source       = module_feeding(module, connectors[c], &sourceOutput);

            resolvedIn[c]     = add_node(params, source, variation, depth + 1);
            resolvedSrcOut[c] = sourceOutput;
        }

        inCount = count;
    }

    if (params->nodeCount >= MAX_ENGINE_NODES) {
        return -1;
    }
    self              = (int32_t)params->nodeCount++;
    node              = &params->node[self];
    memset(node, 0, sizeof(*node));
    node->kind        = kind;
    node->moduleIndex = module->key.index;
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
            // The shape oscillators offer eight waveforms. Four of them are sine variants of rising
            // brightness and the rest are shaped saws and pulses; they are mapped onto the waveforms
            // this engine has, which is an approximation — Sine2..4 in particular are brighter than a
            // plain sine and here they are not.
            static const tOscWave shpWave[] = {
                eOscWaveSine,     eOscWaveSine, eOscWaveSine,   eOscWaveSine,
                eOscWaveTriangle, eOscWaveSaw,  eOscWaveSquare, eOscWaveSquare
            };
            uint32_t              w         = module->param[variation][SHPB_PARAM_WAVEFORM].value;

            node->wave      = shpWave[(w < 8) ? w : 0];
            node->oscKbt    = (module->param[variation][SHPB_PARAM_KBT].value != 0);
            node->basePitch = (double)module->param[variation][SHPB_PARAM_TUNE].value
                              + (osc_fine_cents((double)module->param[variation][SHPB_PARAM_CENT].value) / 100.0);
            node->shape     = osc_shape_percent((double)module->param[variation][SHPB_PARAM_SHAPE].value) / 100.0;
            node->active    = (module->param[variation][SHPB_PARAM_ACTIVE].value != 0);
            break;
        }
        case eNodeMix:
        {
            uint32_t c   = 0;
            bool     exp = (module->param[variation][MIX_PARAM_CURVE].value != 0);

            for (c = 0; c < MAX_NODE_INPUTS; c++) {
                double knob = (double)module->param[variation][MIX_PARAM_LEVEL_BASE + c].value / 127.0;

                node->level[c] = exp ? (knob * knob) : knob;
            }

            break;
        }
        case eNodeOsc:
        {
            double tune      = (double)module->param[variation][OSCB_PARAM_TUNE].value;
            double cent      = (double)module->param[variation][OSCB_PARAM_CENT].value;
            int    pitchType = (int)module->param[variation][OSCB_PARAM_PITCH_TYPE].value;

            // Factor and Partial set the pitch as a ratio against a master oscillator, which the
            // engine has no notion of; reading the dial as Semi at least tracks the knob.
            if (pitchType > 1) {
                LOG_DEBUG("Sound engine: OscB PitchType %d not supported, reading Tune as Semi\n", pitchType);
            }
            node->wave      = (tOscWave)module->param[variation][OSCB_PARAM_WAVEFORM].value;
            node->oscKbt    = (module->param[variation][OSCB_PARAM_KBT].value != 0);
            node->basePitch = tune + (osc_fine_cents(cent) / 100.0);
            node->shape     = osc_shape_percent((double)module->param[variation][OSCB_PARAM_SHAPE].value) / 100.0;
            node->active    = (module->param[variation][OSCB_PARAM_ACTIVE].value != 0);
            break;
        }
        case eNodeFilter:
        {
            node->cutoffHz   = flt_cutoff_hz((double)module->param[variation][FLT_PARAM_FREQ].value);
            node->resonance  = (double)module->param[variation][FLT_PARAM_RES].value / 127.0;
            node->extraPoles = flt_slope_extra_poles(module->param[variation][FLT_PARAM_SLOPE].value);
            node->fltKbt     = flt_kbt_amount(module->param[variation][FLT_PARAM_KBT].value);
            node->modAmount  = (double)module->param[variation][FLT_PARAM_ENV].value * 2.0 / 128.0;
            node->active     = (module->param[variation][FLT_PARAM_ACTIVE].value != 0);
            break;
        }
        case eNodeEnv:
        {
            node->attack  = env_time_seconds((double)module->param[variation][ENV_PARAM_ATTACK].value);
            node->decay   = env_time_seconds((double)module->param[variation][ENV_PARAM_DECAY].value);
            node->sustain = (double)module->param[variation][ENV_PARAM_SUSTAIN].value / 127.0;
            node->release = env_time_seconds((double)module->param[variation][ENV_PARAM_RELEASE].value);
            break;
        }
        case eNodeLevAmp:
        {
            double knob = (double)module->param[variation][LEVAMP_PARAM_GAIN].value / 64.0;   // 64 is unity

            // Type selects a linear or an exponential taper; exp is the default and the musical one.
            node->gain = (module->param[variation][LEVAMP_PARAM_TYPE].value != 0) ? (knob * knob) : knob;
            break;
        }
        case eNodeOut:
        {
            node->active = (module->param[variation][OUT_PARAM_ACTIVE].value != 0);
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
              ^ ((uint64_t)(uint32_t)(params->node[i].in[0] + 1) << 10)
              ^ ((uint64_t)(uint32_t)(params->node[i].in[1] + 1))
              ^ ((uint64_t)(uint32_t)(params->node[i].in[2] + 1) << 30)
              ^ ((uint64_t)(uint32_t)(params->node[i].in[3] + 1) << 50);
    }

    return sig;
}

// With nothing selected, play the patch: find an Out module with something patched into it.
static tModule * find_output_module(void) {
    const uint32_t locations[] = {(uint32_t)locationVa, (uint32_t)locationFx};
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

static double osc_saw(double phase, double dt) {
    return ((2.0 * phase) - 1.0) - poly_blep(phase, dt);
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
static double ladder_filter(double * state, double input, double g, double k, uint32_t tapStage) {
    double   feedback = state[LADDER_POLES - 1];
    double   x        = 0.0;
    uint32_t i        = 0;

    // Feeding the output back subtracts from the input, so a ladder loses passband level as
    // resonance rises — authentic, but taken raw it just sounds like the filter getting quieter and
    // duller as you turn the knob up, which buries the peak you turned it up for. Putting half of it
    // back keeps the level roughly steady across the sweep while leaving some of the thinning that
    // gives the design its character.
    input *= 1.0 + (k * 0.5);
    x      = input - (k * feedback);

    // Soft-clip the feedback path rather than the output: it keeps self-oscillation bounded without
    // dulling the signal when resonance is low.
    if (x > 1.0) {
        x = 1.0;
    } else if (x < -1.0) {
        x = -1.0;
    }

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
        if (gEnvStage[node] == eEnvIdle) {
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

static double oscillator_step(uint32_t node, const tEngineNode * spec, int32_t voiceNote) {
    double pitch     = spec->basePitch;
    double frequency = 0.0;
    double dt        = 0.0;
    double phase     = 0.0;

    // Kbt on transposes the played note by the oscillator's offset from unity; Kbt off leaves the
    // keyboard disconnected and the oscillator holds the pitch Tune names.
    if ((spec->oscKbt == true) && (voiceNote >= 0)) {
        pitch = (double)voiceNote + (spec->basePitch - OSCB_TUNE_UNITY);
    }
    frequency = 440.0 * pow(2.0, (pitch - MIDI_NOTE_A440) / 12.0);

    // Above Nyquist there is no waveform left to produce, only aliasing. Return silence rather than
    // just stopping the phase: a halted sawtooth is not silence, it is a DC offset held at whatever
    // level the waveform sat at, which thumps.
    if (frequency > (gSampleRate * 0.5)) {
        return 0.0;
    }
    dt        = frequency / gSampleRate;
    phase     = advance_phase(&gPhase[node], dt);

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

static double filter_step(uint32_t node, const tEngineNode * spec, double input, double mod, int32_t voiceNote) {
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
    if ((spec->fltKbt > 0.0) && (voiceNote >= 0)) {
        cutoff *= pow(2.0, ((double)voiceNote - MIDI_NOTE_MIDDLE_C) * spec->fltKbt / 12.0);
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
    uint32_t           generation       = 0;
    uint32_t           frame            = 0;
    bool               chainHasEnvelope = false;
    uint32_t           n                = 0;

    if ((out == NULL) || (channelCount == 0)) {
        return;
    }
    memset(out, 0, (size_t)frameCount * channelCount * sizeof(float));

    if (atomic_load(&gActive) == false) {
        return;
    }
    params     = read_params();
    generation = atomic_load(&gNoteGeneration);

    if (params.topology != gSeenTopology) {
        gSeenTopology = params.topology;
        reset_node_state();
    }

    if (generation != gSeenGeneration) {
        int32_t note = atomic_load(&gNote);

        gSeenGeneration = generation;

        // A fresh note restarts every oscillator's cycle, so each strike sounds the same rather than
        // depending on wherever the phases happened to be.
        if ((note >= 0) && ((gGateOpen == false) || (note != gVoiceNote))) {
            for (n = 0; n < MAX_ENGINE_NODES; n++) {
                gPhase[n]         = 0.0;
                gSuperPhase[n][0] = 0.0;
                gSuperPhase[n][1] = 0.0;
            }
        }

        // Only the gate closes on note-off. gVoiceNote holds the pitch it was last played at,
        // because the release is still sounding and it has to keep that pitch — clearing it here
        // dropped the oscillator back to its own Tune value mid-release, which came out as a fixed
        // tone at whatever note the dial named rather than the note just let go of.
        if (note >= 0) {
            gVoiceNote = note;
        }
        gGateOpen       = (note >= 0);
    }

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
                case eNodeOsc:
                case eNodeOscShp:
                {
                    value[n][0] = (spec->active == true) ? oscillator_step(n, spec, gVoiceNote) : 0.0;
                    break;
                }
                case eNodeFilter:
                {
                    value[n][0] = filter_step(n, spec, a, signal_in(spec, value, 1), gVoiceNote);
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

                    for (c = 0; c < spec->inCount; c++) {
                        value[n][0] += signal_in(spec, value, c) * spec->level[c];
                    }

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
                    value[n][0] = (spec->active == true) ? (a + signal_in(spec, value, 1)) : 0.0;
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
        sample *= VOICE_GAIN;

        if (chainHasEnvelope == false) {
            sample *= gEnvelope;
        }

        // A guard, not a limiter. Nothing above should reach full scale; if a bug ever does, this is
        // what stops it arriving at the speakers at full volume.
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
