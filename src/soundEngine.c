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

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "dataBase.h"
#include "globalVars.h"
#include "renderParams.h"
#include "audioOutput.h"
#include "midiInput.h"
#include "soundEngine.h"

// See soundEngine.h for what this does and does not attempt.

// OscB's parameter indices, in the order moduleResources.h lists them for moduleTypeOscB.
#define OSCB_PARAM_TUNE          (0)
#define OSCB_PARAM_CENT          (1)
#define OSCB_PARAM_KBT           (2)
#define OSCB_PARAM_PITCH_TYPE    (4)
#define OSCB_PARAM_SHAPE         (6)
#define OSCB_PARAM_WAVEFORM      (8)
#define OSCB_PARAM_ACTIVE        (9)   // A power button: non-zero is on, 0 is bypassed

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
#define OSCB_TUNE_UNITY    (64.0)
#define MIDI_NOTE_A440     (69.0)

// Kbt off means the keyboard does not reach the oscillator at all (manual p.173), so it holds the
// pitch its Tune value names on its own.
#define VOICE_GAIN          (0.25)         // headroom — a single osc runs nowhere near full scale
#define ENVELOPE_SECONDS    (0.005)        // click suppression only; there is no envelope module here

typedef struct {
    bool     sounding;      // false publishes silence — nothing suitable is selected, or it is bypassed
    tOscWave wave;
    bool     kbt;
    double   basePitch;     // MIDI note number, Tune plus Cent, before the played note is applied
    double   shape;         // 0.5 .. 0.99
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

// Why the engine is or is not making a sound. UI thread only — written while building the snapshot,
// read by the menu.
typedef enum {
    eStatusOff = 0,
    eStatusNoSelection,
    eStatusMultipleSelected,
    eStatusUnsupportedModule,
    eStatusBypassed,
    eStatusPlaying,
} tSoundEngineStatus;

static tSoundEngineStatus gStatus         = eStatusOff;

// Audio-thread-only state. Nothing else may touch these.
static double             gSampleRate     = 48000.0;
static double             gPhase          = 0.0;
static double             gSuperPhase[2]  = {0.0, 0.0};
static double             gEnvelope       = 0.0;
static uint32_t           gSeenGeneration = 0;
static bool               gGateOpen       = false;
static int32_t            gVoiceNote      = -1;
static tSoundEngineParams gLastGoodParams = {0};

bool sound_engine_active(void) {
    return atomic_load(&gActive);
}

void sound_engine_set_sample_rate(double sampleRate) {
    if (sampleRate > 0.0) {
        gSampleRate = sampleRate;
    }
}

bool sound_engine_start(void) {
    if (atomic_load(&gActive) == true) {
        return true;
    }
    // Start from silence rather than inheriting whatever the last run left behind.
    gPhase         = 0.0;
    gSuperPhase[0] = 0.0;
    gSuperPhase[1] = 0.0;
    gEnvelope      = 0.0;
    gGateOpen      = false;
    gVoiceNote     = -1;

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
    if (atomic_load(&gActive) == false) {
        return "Off";
    }

    switch (gStatus) {
        case eStatusNoSelection:
        {
            return "Select an OscB module to play it";
        }
        case eStatusMultipleSelected:
        {
            return "Select one module only";
        }
        case eStatusUnsupportedModule:
        {
            return "Only OscB plays so far";
        }
        case eStatusBypassed:
        {
            return "That OscB is switched off";
        }
        case eStatusPlaying:
        {
            return (midi_input_source_count() > 0) ? "Playing OscB - MIDI in connected"
                                                   : "Playing OscB - no MIDI in, use the Virtual Keyboard";
        }
        default:
        {
            return "Off";
        }
    }
}

void sound_engine_note(int32_t note, bool on) {
    atomic_store(&gNote, on ? note : -1);
    atomic_fetch_add(&gNoteGeneration, 1);
}

void sound_engine_update_from_patch(void) {
    tSoundEngineParams snapshot  = {0};
    tModule *          module    = NULL;
    uint32_t           variation = 0;

    if (atomic_load(&gActive) == false) {
        return;
    }

    // Exactly one module, and it has to be one the engine knows. Anything else is silence rather
    // than a guess — the engine can stay switched on while you click around the patch. Each way of
    // being silent records why, so the menu can say so instead of leaving it a mystery.
    if (gSelection.count == 1) {
        module = get_module(gSelection.keys[0]);
    }

    if (gSelection.count == 0) {
        gStatus = eStatusNoSelection;
    } else if (gSelection.count > 1) {
        gStatus = eStatusMultipleSelected;
    } else if ((module == NULL) || (module->type != moduleTypeOscB)) {
        gStatus = eStatusUnsupportedModule;
    } else {
        gStatus = eStatusPlaying;
    }

    if ((module != NULL) && (module->type == moduleTypeOscB)) {
        double tune      = 0.0;
        double cent      = 0.0;
        int    pitchType = 0;

        variation          = gPatchDescr[module->key.slot].activeVariation;

        tune               = (double)module->param[variation][OSCB_PARAM_TUNE].value;
        cent               = (double)module->param[variation][OSCB_PARAM_CENT].value;
        pitchType          = (int)module->param[variation][OSCB_PARAM_PITCH_TYPE].value;

        // Factor and Partial set the pitch as a ratio against a master oscillator reached over a
        // cable, and cables are not followed yet, so there is nothing to be a ratio of. Reading the
        // dial as Semi at least tracks the knob instead of going silent or sitting at a wrong fixed
        // pitch.
        if (pitchType > 1) {
            LOG_DEBUG("Sound engine: OscB PitchType %d not supported, reading Tune as Semi\n", pitchType);
        }
        snapshot.sounding  = (module->param[variation][OSCB_PARAM_ACTIVE].value != 0);

        if (snapshot.sounding == false) {
            gStatus = eStatusBypassed;    // its own power button is off, which is easy to miss
        }
        snapshot.wave      = (tOscWave)module->param[variation][OSCB_PARAM_WAVEFORM].value;
        snapshot.kbt       = (module->param[variation][OSCB_PARAM_KBT].value != 0);
        snapshot.basePitch = tune + (osc_fine_cents(cent) / 100.0);
        snapshot.shape     = osc_shape_percent((double)module->param[variation][OSCB_PARAM_SHAPE].value) / 100.0;
    }
    atomic_fetch_add(&gParamsSeq, 1);    // now odd — a reader seeing this discards its copy
    gParams = snapshot;
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
    double current = *phase;

    current += dt;

    while (current >= 1.0) {
        current -= 1.0;
    }
    *phase   = current;
    return current;
}

void sound_engine_render(float * out, uint32_t frameCount, uint32_t channelCount) {
    tSoundEngineParams params;
    uint32_t           generation   = 0;
    double             dt           = 0.0;
    double             envelopeStep = 0.0;
    uint32_t           frame        = 0;
    bool               audible      = true;

    if ((out == NULL) || (channelCount == 0)) {
        return;
    }
    memset(out, 0, (size_t)frameCount * channelCount * sizeof(float));

    if (atomic_load(&gActive) == false) {
        return;
    }
    params     = read_params();
    generation = atomic_load(&gNoteGeneration);

    if (generation != gSeenGeneration) {
        int32_t note = atomic_load(&gNote);

        gSeenGeneration = generation;

        // A fresh note restarts the cycle, so every strike sounds the same rather than depending on
        // wherever the phase happened to be.
        if ((note >= 0) && ((gGateOpen == false) || (note != gVoiceNote))) {
            gPhase         = 0.0;
            gSuperPhase[0] = 0.0;
            gSuperPhase[1] = 0.0;
        }
        gVoiceNote      = note;
        gGateOpen       = (note >= 0);
    }

    if ((params.sounding == false) || (gSampleRate <= 0.0)) {
        // Still let the envelope fall, or switching selection mid-note would cut with a click.
        if (gEnvelope <= 0.0) {
            return;
        }
    }
    {
        // Kbt on transposes the played note by the oscillator's offset from unity; Kbt off leaves the
        // keyboard disconnected and the oscillator holds the pitch Tune names.
        double pitch     = params.basePitch;
        double frequency = 0.0;

        if ((params.kbt == true) && (gVoiceNote >= 0)) {
            pitch = (double)gVoiceNote + (params.basePitch - OSCB_TUNE_UNITY);
        }
        frequency = 440.0 * pow(2.0, (pitch - MIDI_NOTE_A440) / 12.0);

        // Above Nyquist there is no waveform left to produce, only aliasing. Fade the voice out
        // rather than just stopping the phase: a halted sawtooth or pulse is not silence, it is a
        // DC offset held at whatever level the waveform sat at, which thumps.
        if (frequency > (gSampleRate * 0.5)) {
            frequency = 0.0;
            audible   = false;
        }
        dt        = frequency / gSampleRate;
    }

    envelopeStep = 1.0 / (ENVELOPE_SECONDS * gSampleRate);

    for (frame = 0; frame < frameCount; frame++) {
        double target = ((gGateOpen == true) && (params.sounding == true) && (audible == true)) ? 1.0 : 0.0;
        double sample = 0.0;
        double phase  = 0.0;

        if (gEnvelope < target) {
            gEnvelope += envelopeStep;

            if (gEnvelope > target) {
                gEnvelope = target;
            }
        } else if (gEnvelope > target) {
            gEnvelope -= envelopeStep;

            if (gEnvelope < target) {
                gEnvelope = target;
            }
        }
        phase = advance_phase(&gPhase, dt);

        switch (params.wave) {
            case eOscWaveSine:
            {
                sample = sin(phase * 2.0 * M_PI);
                break;
            }
            case eOscWaveTriangle:
            {
                sample = osc_triangle(phase, params.shape);
                break;
            }
            case eOscWaveSaw:
            {
                sample = osc_saw(phase, dt);
                break;
            }
            case eOscWaveSquare:
            {
                sample = osc_square(phase, dt, params.shape);
                break;
            }
            case eOscWaveSuper:
            {
                // Approximation: three saws a few cents apart. The G2's own "sup" is a different
                // algorithm — see the header.
                double detuneUp   = dt * 1.0059;   // about +10 cents
                double detuneDown = dt * 0.9941;   // about -10 cents

                sample  = osc_saw(phase, dt);
                sample += osc_saw(advance_phase(&gSuperPhase[0], detuneUp), detuneUp);
                sample += osc_saw(advance_phase(&gSuperPhase[1], detuneDown), detuneDown);
                sample /= 3.0;
                break;
            }
            default:
            {
                sample = 0.0;
                break;
            }
        }
        sample *= gEnvelope * VOICE_GAIN;

        // A guard, not a limiter. Nothing above should reach full scale; if a bug ever does, this is
        // what stops it arriving at the speakers at full volume.
        if (sample > 1.0) {
            sample = 1.0;
        } else if (sample < -1.0) {
            sample = -1.0;
        }
        {
            uint32_t channel = 0;

            for (channel = 0; channel < channelCount; channel++) {
                out[(frame * channelCount) + channel] = (float)sample;
            }
        }
    }
}

#ifdef __cplusplus
}
#endif
