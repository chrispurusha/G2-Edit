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

// The VST3 wrapper around the sound engine.
//
// This file is the plug-in's whole platform layer - it is to a host what audioOutput.c is to
// CoreAudio, and it exists precisely because soundEngine.c was kept free of either. Nothing in the
// engine knows it is being hosted.
//
// Built against pluginterfaces/ ONLY: the SDK's public.sdk helper classes are not used, so there is
// no CMake and nothing to reconcile with G2-Edit's Xcode-only build. The cost is that the COM
// plumbing below - reference counting, queryInterface, the factory - is written out by hand rather
// than inherited. It is dull but it is all here, which is the point.
//
// The processor and the controller are SEPARATE classes — see the longer note above G2EditPlugin for
// why, and why the obvious single-object arrangement had to be abandoned.
//
// The controller also implements IMidiMapping. That is not optional decoration: a VST3 host converts
// continuous controllers, pitch bend and aftertouch into PARAMETER CHANGES rather than delivering
// them as MIDI events, and IMidiMapping is the only place a plug-in says which parameter each one
// becomes. Without it, notes play and every expressive control does nothing at all.
//
// There IS an editor (g2Editor.mm, an IPlugView) — but the parameters still matter independently of
// it: they are what a host automates and what its generic panel shows, and exposing none made the
// plug-in look broken even though it loaded and played correctly.

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <string>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include "g2Editor.h"

extern "C" {
#include "soundEngine.h"
#include "g2Patch.h"
#include "noteStack.h"
}

using namespace Steinberg;
using namespace Steinberg::Vst;

// Stable identity. A host remembers a plug-in by this, so it must never change once a project has
// been saved against it.
static const FUID kG2EditProcessorUID(0x7D14B03C, 0x6E284A97, 0x8C5F1D62, 0xB93A47E1);
static const FUID kG2EditControllerUID(0x2B69F58D, 0x41A70C36, 0x95E284BF, 0x1D6035CA);

#define G2_VENDOR       "Chris Purusha"
#define G2_PLUGIN_NAME  "G2 Alike"
#define G2_VERSION      "0.1.0"

// Where the patch comes from, with no editor to choose one. Checked in order:
//   1. the path the host restored with the project (setState below)
//   2. $G2_VST3_PATCH
//   3. ~/Documents/G2-Edit/plugin.pch2
// The host-stored path is what makes a project reopen sounding as it did; the environment variable
// is for driving it from a test script, and the fixed location is so it does something sensible with
// neither set.
static std::string default_patch_path(void) {
    const char * env = getenv("G2_VST3_PATCH");

    if ((env != nullptr) && (env[0] != '\0')) {
        return std::string(env);
    }
    const char * home = getenv("HOME");

    return std::string(home ? home : ".") + "/Documents/G2-Edit/plugin.pch2";
}

// A MORPH DOES NOT REACH THE AUDIO THREAD BY ITSELF, and this flag is how the plug-in copes.
//
// sound_engine_set_morph() only records the position. Unlike pitch bend, which the audio thread
// reads directly, a morph is folded into the parameter SNAPSHOT, and that snapshot is only rebuilt
// by sound_engine_update_from_patch(). The standalone editor rebuilds it on every redraw, which is
// why moving a morph there requires asking for one — and why its mod wheel response is capped at
// the frame rate, since a full canvas repaint sits between the wheel and the sound.
//
// The plug-in cannot borrow that arrangement: it has to work with the editor window closed. So the
// rebuild happens in process() instead, once per block, and only when something actually moved.
//
// A FLAG RATHER THAN REBUILDING ON THE SPOT, because the snapshot is published through a SEQLOCK
// (gParamsSeq in soundEngine.c). A seqlock tolerates exactly one writer; the audio thread is already
// its reader, and the controller's parameter changes arrive on the host's UI thread. Letting both
// write would corrupt it. So both merely SET this, and process() — one thread, once per block — is
// the only writer.
static std::atomic<bool> gMorphSnapshotDirty{false};

// Processor and controller are SEPARATE CLASSES, both registered with the factory.
//
// VST3 also permits one object to implement both, and that is what this was — it is simpler, and a
// hand-written test host accepted it happily. Ableton did not: it loaded the plug-in, reported
// "parameter count is 0", and gave a wrench icon that opened nothing, because it obtains the
// controller by instantiating the class named by IComponent::getControllerClassId() and does not
// fall back to asking the component for IEditController. Splitting them is the shape every host
// expects, so it is the shape used here.
class G2EditPlugin : public IComponent, public IAudioProcessor {
public:
    G2EditPlugin(void) : refCount(1), sampleRate(44100.0), active(false) {
        // params[] otherwise zero-initialises, and zero is FULL BEND DOWN rather than centre. It
        // would only be read back, not applied — nothing calls the engine until the host sends a
        // value — but a plug-in reporting a two-semitone-flat bend it is not applying is a trap.
        params[kParamBend]  = 0.5;
        params[kParamLevel] = 1.0;
    }
    virtual ~G2EditPlugin(void) {}

    // ---- FUnknown ------------------------------------------------------------------------------
    tresult PLUGIN_API queryInterface(const TUID iid, void ** obj) SMTG_OVERRIDE {
        QUERY_INTERFACE(iid, obj, FUnknown::iid, IComponent)
        QUERY_INTERFACE(iid, obj, IPluginBase::iid, IComponent)
        QUERY_INTERFACE(iid, obj, IComponent::iid, IComponent)
        QUERY_INTERFACE(iid, obj, IAudioProcessor::iid, IAudioProcessor)
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef(void) SMTG_OVERRIDE {
        return (uint32)++refCount;
    }

    uint32 PLUGIN_API release(void) SMTG_OVERRIDE {
        int32 c = --refCount;

        if (c == 0) {
            delete this;
            return 0;
        }
        return (uint32)c;
    }

    // ---- IPluginBase / IComponent --------------------------------------------------------------
    tresult PLUGIN_API initialize(FUnknown * context) SMTG_OVERRIDE {
        (void)context;

        if (patchPath.empty()) {
            patchPath = default_patch_path();
        }
        load_patch();
        return kResultOk;
    }

    tresult PLUGIN_API terminate(void) SMTG_OVERRIDE {
        sound_engine_stop_hosted();
        return kResultOk;
    }

    tresult PLUGIN_API getControllerClassId(TUID classId) SMTG_OVERRIDE {
        // This is how the host finds the controller. Returning kNotImplemented here — which is what
        // a single-object processor+controller does — left Ableton with no controller and therefore
        // no parameters and no panel.
        memcpy(classId, kG2EditControllerUID.toTUID(), sizeof(TUID));
        return kResultOk;
    }

    tresult PLUGIN_API setIoMode(IoMode mode) SMTG_OVERRIDE {
        (void)mode;
        return kNotImplemented;
    }

    int32 PLUGIN_API getBusCount(MediaType type, BusDirection dir) SMTG_OVERRIDE {
        if ((type == kAudio) && (dir == kOutput)) {
            return 1;               // one stereo out
        }

        if ((type == kEvent) && (dir == kInput)) {
            return 1;               // MIDI in, which is how notes arrive
        }
        return 0;
    }

    tresult PLUGIN_API getBusInfo(MediaType type, BusDirection dir, int32 index, BusInfo & bus) SMTG_OVERRIDE {
        if ((type == kAudio) && (dir == kOutput) && (index == 0)) {
            bus.mediaType    = kAudio;
            bus.direction    = kOutput;
            bus.channelCount = 2;
            bus.busType      = kMain;
            bus.flags        = BusInfo::kDefaultActive;
            copy_name(bus.name, "Output");
            return kResultOk;
        }

        if ((type == kEvent) && (dir == kInput) && (index == 0)) {
            bus.mediaType    = kEvent;
            bus.direction    = kInput;
            bus.channelCount = 1;
            bus.busType      = kMain;
            bus.flags        = BusInfo::kDefaultActive;
            copy_name(bus.name, "MIDI In");
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult PLUGIN_API getRoutingInfo(RoutingInfo & inInfo, RoutingInfo & outInfo) SMTG_OVERRIDE {
        (void)inInfo;
        (void)outInfo;
        return kNotImplemented;
    }

    tresult PLUGIN_API activateBus(MediaType type, BusDirection dir, int32 index, TBool state) SMTG_OVERRIDE {
        (void)type;
        (void)dir;
        (void)index;
        (void)state;
        return kResultOk;
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE {
        if (state) {
            sound_engine_start_hosted(sampleRate);
            active = true;
            // The chain is resolved HERE, not when the patch was read. sound_engine_update_from_patch()
            // returns immediately while the engine is inactive, so calling it at initialize() time —
            // which is the obvious place, and where this used to be — silently did nothing and the
            // plug-in rendered silence from a perfectly good patch.
            sound_engine_update_from_patch();
        } else {
            sound_engine_stop_hosted();
            active = false;
        }
        return kResultOk;
    }

    // The patch is identified by PATH rather than embedded wholesale. A .pch2 is small enough to
    // embed, and doing so would make a project self-contained, but it would also freeze a copy: edit
    // the patch in G2-Edit and the project would go on playing the old one, silently. Storing the
    // path keeps one patch with one meaning.
    tresult PLUGIN_API setState(IBStream * state) SMTG_OVERRIDE {
        if (state == nullptr) {
            return kResultFalse;
        }
        char  buffer[1024] = {0};
        int32 read         = 0;

        if (state->read(buffer, (int32)sizeof(buffer) - 1, &read) != kResultOk) {
            return kResultFalse;
        }
        buffer[(read > 0 && read < (int32)sizeof(buffer)) ? read : 0] = '\0';

        if (buffer[0] != '\0') {
            patchPath = buffer;
            load_patch();
        }
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream * state) SMTG_OVERRIDE {
        if (state == nullptr) {
            return kResultFalse;
        }
        int32 written = 0;

        return state->write((void *)patchPath.c_str(), (int32)patchPath.size(), &written);
    }

    // ---- IAudioProcessor -----------------------------------------------------------------------
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement * inputs, int32 numIns,
                                          SpeakerArrangement * outputs, int32 numOuts) SMTG_OVERRIDE {
        (void)inputs;

        if ((numIns == 0) && (numOuts == 1) && (outputs[0] == SpeakerArr::kStereo)) {
            return kResultOk;
        }
        return kResultFalse;
    }

    tresult PLUGIN_API getBusArrangement(BusDirection dir, int32 index, SpeakerArrangement & arr) SMTG_OVERRIDE {
        if ((dir == kOutput) && (index == 0)) {
            arr = SpeakerArr::kStereo;
            return kResultOk;
        }
        return kInvalidArgument;
    }

    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE {
        // The engine renders float. A host asking for double is told no and will hand us float.
        return (symbolicSampleSize == kSample32) ? kResultTrue : kResultFalse;
    }

    uint32 PLUGIN_API getLatencySamples(void) SMTG_OVERRIDE {
        return 0;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup & setup) SMTG_OVERRIDE {
        sampleRate = setup.sampleRate;
        sound_engine_set_sample_rate(sampleRate);
        return kResultOk;
    }

    tresult PLUGIN_API setProcessing(TBool state) SMTG_OVERRIDE {
        (void)state;
        return kResultOk;
    }

    uint32 PLUGIN_API getTailSamples(void) SMTG_OVERRIDE {
        return kInfiniteTail;       // there is a reverb and a delay in here
    }

    tresult PLUGIN_API process(ProcessData & data) SMTG_OVERRIDE {
        // Automation, before anything is rendered. Only the LAST point in each queue is taken: the
        // engine has no notion of a parameter ramping within a block, so interpolating between
        // points would be inventing a resolution it cannot use. Same block-granularity trade as the
        // notes below.
        if (data.inputParameterChanges != nullptr) {
            int32 queues = data.inputParameterChanges->getParameterCount();

            for (int32 q = 0; q < queues; q++) {
                IParamValueQueue * queue = data.inputParameterChanges->getParameterData(q);

                if (queue == nullptr) {
                    continue;
                }
                int32 points = queue->getPointCount();

                if (points <= 0) {
                    continue;
                }
                int32      offset = 0;
                ParamValue value  = 0.0;

                if (queue->getPoint(points - 1, offset, value) == kResultOk) {
                    ParamID id = queue->getParameterId();

                    if (id < (ParamID)kNumParams) {
                        apply_param(id, value);
                    }
                }
            }
        }

        // Fold any moved morph into the parameter snapshot. Once per block rather than once per
        // change, and only here — see gMorphSnapshotDirty for why this is the sole writer. Costs a
        // database walk, which is what the standalone pays on every frame anyway.
        if (gMorphSnapshotDirty.exchange(false) == true) {
            sound_engine_update_from_patch();
        }

        // Notes first, for the whole block. The engine timestamps nothing, so sample-accurate event
        // placement inside the block is lost - a note lands at the start of the buffer it arrived
        // in. At 44.1k and a 512 frame buffer that is under 12 ms of jitter; worth revisiting only
        // once the engine itself can place an event within a block.
        if (data.inputEvents != nullptr) {
            int32 count = data.inputEvents->getEventCount();

            for (int32 i = 0; i < count; i++) {
                Event e = {};

                if (data.inputEvents->getEvent(i, e) != kResultOk) {
                    continue;
                }

                if (e.type == Event::kNoteOnEvent) {
                    // A note-on at zero velocity is a note-off, as it is over MIDI.
                    //
                    // Through the shared note stack, NOT straight to the engine. The engine is
                    // monophonic, so releasing a note has to fall back to whatever is still held or
                    // legato playing breaks — hold D, play F, let F go, and the D under your finger
                    // must come back rather than the sound stopping. noteStack.c is the application's
                    // own logic, moved out of midiInput.c so both get it from one place.
                    if (e.noteOn.velocity > 0.0f) {
                        note_stack_note_on((uint8_t)e.noteOn.pitch);
                    } else {
                        note_stack_note_off((uint8_t)e.noteOn.pitch);
                    }
                } else if (e.type == Event::kNoteOffEvent) {
                    note_stack_note_off((uint8_t)e.noteOff.pitch);
                } else if (e.type == Event::kPolyPressureEvent) {
                    // POLYPHONIC key pressure, which arrives as an EVENT and not as a parameter
                    // change. IMidiMapping's kAfterTouch is CHANNEL pressure (MIDI 0xD0) only, so a
                    // keyboard sending poly pressure (0xA0) — and plenty do — reaches a plug-in by
                    // this path or not at all. midiInput.c handles both for the same reason.
                    //
                    // The engine has one voice, so as in the application only the note actually
                    // sounding may move the morph; without that test a key still held underneath
                    // would fight the one being played.
                    if ((int32)e.polyPressure.pitch == note_stack_top()) {
                        if (sound_engine_set_morph(kMorphGroupAftertouch,
                                                   (double)e.polyPressure.pressure) == true) {
                            gMorphSnapshotDirty.store(true);
                        }
                    }
                }
            }
        }

        if ((data.numOutputs < 1) || (data.outputs[0].numChannels < 2) || (data.numSamples <= 0)) {
            return kResultOk;
        }
        float ** out = data.outputs[0].channelBuffers32;

        if ((out == nullptr) || (out[0] == nullptr) || (out[1] == nullptr)) {
            return kResultOk;
        }
        // sound_engine_render() writes INTERLEAVED frames; VST3 hands us one buffer per channel, so
        // it renders into a scratch block and is de-interleaved out. The block is bounded by
        // kMaxBlock and looped, so an unusually large buffer size cannot overrun it.
        int32 done = 0;

        while (done < data.numSamples) {
            int32 chunk = data.numSamples - done;

            if (chunk > kMaxBlock) {
                chunk = kMaxBlock;
            }
            sound_engine_render(scratch, (uint32_t)chunk, 2);

            for (int32 i = 0; i < chunk; i++) {
                out[0][done + i] = scratch[i * 2];
                out[1][done + i] = scratch[i * 2 + 1];
            }
            done += chunk;
        }
        data.outputs[0].silenceFlags = 0;
        return kResultOk;
    }

    static FUnknown * createInstance(void * /*context*/) {
        return (IAudioProcessor *)new G2EditPlugin();
    }

private:
    static const int32  kMaxBlock   = 4096;
    static const int32  kMorphCount = 8;                 // NUM_MORPHS, without pulling defs.h into C++
    static const ParamID kParamLevel = 8;
    static const ParamID kParamBend  = 9;                // pitch bend; 0.5 is centre
    static const int32  kNumParams  = 10;

    // Which of parameters 0-7 the G2 wires aftertouch to (midiInput.c's MORPH_GROUP_AFTERTOUCH).
    static const ParamID kMorphGroupAftertouch = 3;

    // The output trim attenuates only — sound_engine_set_output_level_db() clamps anything at or
    // above 0 dB to unity, deliberately, so this is a fader and not a boost into the limiter.
    static constexpr double kLevelMinDb = -60.0;

    static double level_db(ParamValue normalized) {
        return kLevelMinDb + (normalized * (0.0 - kLevelMinDb));
    }

    // Both the host's generic panel (via setParamNormalized, on its UI thread) and automation (via
    // process(), on the audio thread) land here. Every engine entry point it calls stores through an
    // atomic, so there is nothing to guard.
    void apply_param(ParamID id, ParamValue value) {
        if (value < 0.0) {
            value = 0.0;
        } else if (value > 1.0) {
            value = 1.0;
        }
        params[id] = value;

        if (id < (ParamID)kMorphCount) {
            // The return says whether the position actually changed — no point rebuilding a
            // snapshot for a controller resending a value it already sent.
            if (sound_engine_set_morph((uint32_t)id, value) == true) {
                gMorphSnapshotDirty.store(true);
            }
        } else if (id == kParamLevel) {
            sound_engine_set_output_level_db(level_db(value));
        } else if (id == kParamBend) {
            // The host hands pitch bend over as 0..1 with 0.5 at rest; the engine wants -1..+1, and
            // decides for itself how many semitones that is from the patch's own Bend setting.
            sound_engine_pitch_bend((value * 2.0) - 1.0);
        }
    }

    void load_patch(void) {
        // THE PATH, not a patch compiled into the binary. The built-in patch was a scaffold from
        // before the plug-in had an editor: with no way to choose a file, embedding one removed a
        // whole class of "why is it silent" while the rest was proven. File > Open Patch File... has
        // replaced it, and a plug-in that quietly plays somebody else's lead patch on load is worse
        // than one that starts empty.
        //
        // Slot 0: a plug-in instance is one patch, and the four-slot performance layout is a
        // hardware notion with nothing to map onto here. An empty path or a missing file simply
        // leaves the canvas empty, which is honest.
        (void)g2_plugin_load_patch(patchPath.c_str(), 0);

        // Only meaningful once the engine is live — see setActive(). Harmless when it is not, and
        // called anyway so that a patch swapped in mid-session takes effect immediately.
        if (active) {
            sound_engine_update_from_patch();
        }
    }

    static void copy_name(String128 dst, const char * src) {
        int i = 0;

        for (; (src[i] != '\0') && (i < 127); i++) {
            dst[i] = (TChar)src[i];
        }
        dst[i] = 0;
    }

    std::atomic<int32> refCount;
    ParamValue         params[kNumParams] = {0};
    double             sampleRate;
    bool               active;
    std::string        patchPath;
    float              scratch[kMaxBlock * 2];
};


// The controller half. Holds the parameters the host draws its generic panel from, and pushes them
// into the engine as they move. The engine's own state is process-wide (globals reached through
// atomics), so the controller can write to it directly without knowing which processor it belongs
// to — which also means two instances of this plug-in in one project would fight over the same
// engine. One instance only, for now.
// IMidiMapping IS WHY PITCH BEND AND THE WHEELS WORK AT ALL. A VST3 host does not deliver them as
// MIDI events the way note on/off arrive: continuous controllers, pitch bend and aftertouch are
// converted by the host into PARAMETER CHANGES, and this interface is the only place a plug-in gets
// to say which parameter each one should become. Without it the host has nowhere to send them, so
// notes play and every expressive control does nothing — silently, since nothing is technically
// wrong.
class G2EditController : public IEditController, public IMidiMapping {
public:
    G2EditController(void) : refCount(1) {
        params[kParamLevel] = 1.0;
        params[kParamBend]  = 0.5;    // centred: 0.5 is no bend, as the host's 0..1 range requires
    }

    virtual ~G2EditController(void) {}

    // Two interfaces now, so the casts matter: each branch must hand back a pointer to the right
    // base. Returning the IEditController pointer for an IMidiMapping query would have the host
    // call through the wrong vtable.
    tresult PLUGIN_API queryInterface(const TUID iid, void ** obj) SMTG_OVERRIDE {
        QUERY_INTERFACE(iid, obj, FUnknown::iid, IEditController)
        QUERY_INTERFACE(iid, obj, IPluginBase::iid, IEditController)
        QUERY_INTERFACE(iid, obj, IEditController::iid, IEditController)
        QUERY_INTERFACE(iid, obj, IMidiMapping::iid, IMidiMapping)
        *obj = nullptr;
        return kNoInterface;
    }

    // The G2 hard-wires its wheels and pedals to particular morph groups (manual; morphStrMap in
    // moduleResources.h names them), and those morph groups are ALREADY exposed as parameters 0-7.
    // So most of this maps a MIDI controller onto a parameter that exists rather than inventing a
    // new one — the same routing the standalone editor performs in midiInput.c, expressed the way
    // VST3 wants it. Only pitch bend needs a parameter of its own, having no morph group.
    tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                   CtrlNumber midiControllerNumber,
                                                   ParamID & id) SMTG_OVERRIDE {
        (void)channel;    // one engine voice, all channels drive it — as the app's Omni handling does

        if (busIndex != 0) {
            return kResultFalse;
        }

        switch (midiControllerNumber) {
            case kPitchBend:        id = kParamBend;                 return kResultTrue;
            case kCtrlModWheel:     id = kMorphGroupWheel;           return kResultTrue;
            case kAfterTouch:       id = kMorphGroupAftertouch;      return kResultTrue;
            case kCtrlSustainOnOff: id = kMorphGroupSustain;         return kResultTrue;
            case kCtrlFoot:         id = kMorphGroupCtrlPedal;       return kResultTrue;
            default:                                                 return kResultFalse;
        }
    }

    uint32 PLUGIN_API addRef(void) SMTG_OVERRIDE {
        return (uint32)++refCount;
    }

    uint32 PLUGIN_API release(void) SMTG_OVERRIDE {
        int32 c = --refCount;

        if (c == 0) {
            delete this;
            return 0;
        }
        return (uint32)c;
    }

    tresult PLUGIN_API initialize(FUnknown * context) SMTG_OVERRIDE {
        (void)context;
        return kResultOk;
    }

    tresult PLUGIN_API terminate(void) SMTG_OVERRIDE {
        return kResultOk;
    }

    // ---- IEditController -----------------------------------------------------------------------
    // There is no custom editor - createView() returns nothing - so these parameters ARE the user
    // interface: the host draws its own generic panel from them. A plug-in that exposes none gets an
    // empty panel and looks broken, which is exactly how it looked before these existed.
    //
    // The eight morph groups are the G2's own performance controls, so they are the right things to
    // put in front of a host: automating a morph is the nearest thing to playing the hardware.
    // The host hands the controller the processor's saved state so the two agree. Ours is a patch
    // path, which the controller does not act on — the processor loads the patch — so this only has
    // to accept it without complaint.
    tresult PLUGIN_API setComponentState(IBStream * state) SMTG_OVERRIDE {
        (void)state;
        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream * state) SMTG_OVERRIDE {
        (void)state;
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream * state) SMTG_OVERRIDE {
        (void)state;
        return kResultOk;
    }

    int32 PLUGIN_API getParameterCount(void) SMTG_OVERRIDE {
        return kNumParams;
    }

    tresult PLUGIN_API getParameterInfo(int32 paramIndex, ParameterInfo & info) SMTG_OVERRIDE {
        if ((paramIndex < 0) || (paramIndex >= kNumParams)) {
            return kResultFalse;
        }
        memset(&info, 0, sizeof(info));
        info.id                 = (ParamID)paramIndex;
        info.stepCount          = 0;                    // continuous
        info.unitId             = 0;                    // kRootUnitId
        info.flags              = ParameterInfo::kCanAutomate;

        if (paramIndex < kMorphCount) {
            char title[32];

            snprintf(title, sizeof(title), "Morph %d", paramIndex + 1);
            copy_name(info.title, title);
            copy_name(info.shortTitle, title);
            info.defaultNormalizedValue = 0.0;
        } else if (paramIndex == (int32)kParamLevel) {
            copy_name(info.title, "Output Level");
            copy_name(info.shortTitle, "Level");
            copy_name(info.units, "dB");
            info.defaultNormalizedValue = 1.0;           // unity; the trim only attenuates
        } else {
            // Pitch bend. It has to be a real, declared parameter even though nobody would choose to
            // automate it — IMidiMapping can only name a parameter that exists, so this is what the
            // host writes the wheel's position into.
            copy_name(info.title, "Pitch Bend");
            copy_name(info.shortTitle, "Bend");
            info.defaultNormalizedValue = 0.5;           // centre
        }
        return kResultOk;
    }

    tresult PLUGIN_API getParamStringByValue(ParamID id, ParamValue valueNormalized, String128 string) SMTG_OVERRIDE {
        char text[32];

        if (id < kMorphCount) {
            snprintf(text, sizeof(text), "%.1f%%", valueNormalized * 100.0);
        } else if (id == kParamLevel) {
            double db = level_db(valueNormalized);

            if (db <= kLevelMinDb) {
                snprintf(text, sizeof(text), "-inf");
            } else {
                snprintf(text, sizeof(text), "%.1f", db);
            }
        } else if (id == kParamBend) {
            snprintf(text, sizeof(text), "%+.2f", (valueNormalized * 2.0) - 1.0);
        } else {
            return kResultFalse;
        }
        copy_name(string, text);
        return kResultOk;
    }

    tresult PLUGIN_API getParamValueByString(ParamID id, TChar * string, ParamValue & valueNormalized) SMTG_OVERRIDE {
        (void)id;
        (void)string;
        (void)valueNormalized;
        return kResultFalse;                             // typed entry not supported; the host falls back to its knob
    }

    ParamValue PLUGIN_API normalizedParamToPlain(ParamID id, ParamValue valueNormalized) SMTG_OVERRIDE {
        return (id == kParamLevel) ? level_db(valueNormalized) : (valueNormalized * 100.0);
    }

    ParamValue PLUGIN_API plainParamToNormalized(ParamID id, ParamValue plainValue) SMTG_OVERRIDE {
        if (id == kParamLevel) {
            return (plainValue - kLevelMinDb) / (0.0 - kLevelMinDb);
        }
        return plainValue / 100.0;
    }

    ParamValue PLUGIN_API getParamNormalized(ParamID id) SMTG_OVERRIDE {
        return (id < kNumParams) ? params[id] : 0.0;
    }

    tresult PLUGIN_API setParamNormalized(ParamID id, ParamValue value) SMTG_OVERRIDE {
        if (id >= kNumParams) {
            return kResultFalse;
        }
        apply_param(id, value);
        return kResultOk;
    }

    // Kept so the editor panel can report its moves back through beginEdit/performEdit/endEdit —
    // without it the host would see slider changes appear from nowhere and could not record them.
    tresult PLUGIN_API setComponentHandler(IComponentHandler * handler) SMTG_OVERRIDE {
        componentHandler = handler;
        return kResultOk;
    }

    IPlugView * PLUGIN_API createView(FIDString name) SMTG_OVERRIDE {
        if ((name == nullptr) || (strcmp(name, ViewType::kEditor) != 0)) {
            return nullptr;
        }
        // Names the patch actually playing, which while the built-in one is forced is not the path.
        // No patch name passed: the editor draws its own topbar and reads the loaded name from
        // g2Menu.c, which is the one place that knows it. The argument survives from the AppKit
        // panel that used to print it.
        return g2_create_editor_view(this, componentHandler, "");
    }


    static FUnknown * createInstance(void * /*context*/) {
        return (IEditController *)new G2EditController();
    }

private:
    static const int32   kMorphCount = 8;                // NUM_MORPHS, without pulling defs.h into C++
    static const ParamID kParamLevel = 8;
    static const ParamID kParamBend  = 9;                // pitch bend; 0.5 is centre
    static const int32   kNumParams  = 10;

    // Which morph group the G2 wires each physical control to (midiInput.c's MORPH_GROUP_*), and
    // therefore which of parameters 0-7 a MIDI controller should drive. Named here so the mapping
    // above reads as intent rather than as four bare numbers.
    static const ParamID kMorphGroupWheel      = 0;
    static const ParamID kMorphGroupAftertouch = 3;
    static const ParamID kMorphGroupSustain    = 4;
    static const ParamID kMorphGroupCtrlPedal  = 5;

    // The output trim attenuates only — sound_engine_set_output_level_db() clamps anything at or
    // above 0 dB to unity, deliberately, so this is a fader and not a boost into the limiter.
    static constexpr double kLevelMinDb = -60.0;

    static double level_db(ParamValue normalized) {
        return kLevelMinDb + (normalized * (0.0 - kLevelMinDb));
    }

    void apply_param(ParamID id, ParamValue value) {
        if (value < 0.0) {
            value = 0.0;
        } else if (value > 1.0) {
            value = 1.0;
        }
        params[id] = value;

        if (id < (ParamID)kMorphCount) {
            // The return says whether the position actually changed — no point rebuilding a
            // snapshot for a controller resending a value it already sent.
            if (sound_engine_set_morph((uint32_t)id, value) == true) {
                gMorphSnapshotDirty.store(true);
            }
        } else if (id == kParamLevel) {
            sound_engine_set_output_level_db(level_db(value));
        } else if (id == kParamBend) {
            // The host hands pitch bend over as 0..1 with 0.5 at rest; the engine wants -1..+1, and
            // decides for itself how many semitones that is from the patch's own Bend setting.
            sound_engine_pitch_bend((value * 2.0) - 1.0);
        }
    }

    static void copy_name(String128 dst, const char * src) {
        int i = 0;

        for (; (src[i] != '\0') && (i < 127); i++) {
            dst[i] = (TChar)src[i];
        }
        dst[i] = 0;
    }

    std::atomic<int32>  refCount;
    IComponentHandler * componentHandler = nullptr;
    ParamValue          params[kNumParams] = {0};
};

// ---- Factory -----------------------------------------------------------------------------------

// IPluginFactory3, not plain IPluginFactory. The base interface can only report a class CATEGORY
// ("Audio Module Class"), which says a plug-in makes audio but not whether it is an instrument or an
// effect. A host that cannot tell assumes effect, then looks for the audio input an effect must have,
// finds none, and rejects the plug-in — which is exactly what Ableton did:
//
//     error: Vst3: plugin has an effect category, but no valid audio input bus
//
// The SUBCATEGORY that settles it ("Instrument|Synth") only exists on PClassInfo2, which arrived with
// IPluginFactory2. So the extra interfaces below are not optional polish for an instrument; without
// them it will not load at all.
class G2EditFactory : public IPluginFactory3 {
public:
    G2EditFactory(void) : refCount(1) {}
    virtual ~G2EditFactory(void) {}

    tresult PLUGIN_API queryInterface(const TUID iid, void ** obj) SMTG_OVERRIDE {
        QUERY_INTERFACE(iid, obj, FUnknown::iid, IPluginFactory)
        QUERY_INTERFACE(iid, obj, IPluginFactory::iid, IPluginFactory)
        QUERY_INTERFACE(iid, obj, IPluginFactory2::iid, IPluginFactory2)
        QUERY_INTERFACE(iid, obj, IPluginFactory3::iid, IPluginFactory3)
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef(void) SMTG_OVERRIDE {
        return (uint32)++refCount;
    }

    uint32 PLUGIN_API release(void) SMTG_OVERRIDE {
        int32 c = --refCount;

        if (c == 0) {
            delete this;
            return 0;
        }
        return (uint32)c;
    }

    tresult PLUGIN_API getFactoryInfo(PFactoryInfo * info) SMTG_OVERRIDE {
        if (info == nullptr) {
            return kInvalidArgument;
        }
        memset(info, 0, sizeof(PFactoryInfo));
        strncpy(info->vendor, G2_VENDOR, PFactoryInfo::kNameSize - 1);
        strncpy(info->url, "https://github.com/chrispurusha", PFactoryInfo::kURLSize - 1);
        strncpy(info->email, "", PFactoryInfo::kEmailSize - 1);
        info->flags = PFactoryInfo::kUnicode;
        return kResultOk;
    }

    int32 PLUGIN_API countClasses(void) SMTG_OVERRIDE {
        return 2;                                        // processor + controller
    }

    // Class 0 is the processor, class 1 the controller. The controller is registered under
    // kVstComponentControllerClass, NOT kVstAudioEffectClass — a host enumerating instruments must
    // not find two.
    tresult PLUGIN_API getClassInfo(int32 index, PClassInfo * info) SMTG_OVERRIDE {
        if ((info == nullptr) || (index < 0) || (index > 1)) {
            return kInvalidArgument;
        }
        memset(info, 0, sizeof(PClassInfo));
        info->cardinality = PClassInfo::kManyInstances;

        if (index == 0) {
            memcpy(info->cid, kG2EditProcessorUID.toTUID(), sizeof(TUID));
            strncpy(info->category, kVstAudioEffectClass, PClassInfo::kCategorySize - 1);
            strncpy(info->name, G2_PLUGIN_NAME, PClassInfo::kNameSize - 1);
        } else {
            memcpy(info->cid, kG2EditControllerUID.toTUID(), sizeof(TUID));
            strncpy(info->category, kVstComponentControllerClass, PClassInfo::kCategorySize - 1);
            strncpy(info->name, G2_PLUGIN_NAME " Controller", PClassInfo::kNameSize - 1);
        }
        return kResultOk;
    }

    // The one that actually matters — see the note on the class. kInstrumentSynth is "Instrument|Synth".
    tresult PLUGIN_API getClassInfo2(int32 index, PClassInfo2 * info) SMTG_OVERRIDE {
        if ((info == nullptr) || (index < 0) || (index > 1)) {
            return kInvalidArgument;
        }
        memset(info, 0, sizeof(PClassInfo2));
        info->cardinality = PClassInfo::kManyInstances;
        info->classFlags  = 0;
        strncpy(info->vendor, G2_VENDOR, PClassInfo2::kVendorSize - 1);
        strncpy(info->version, G2_VERSION, PClassInfo2::kVersionSize - 1);
        strncpy(info->sdkVersion, kVstVersionString, PClassInfo2::kVersionSize - 1);

        if (index == 0) {
            memcpy(info->cid, kG2EditProcessorUID.toTUID(), sizeof(TUID));
            strncpy(info->category, kVstAudioEffectClass, PClassInfo::kCategorySize - 1);
            strncpy(info->name, G2_PLUGIN_NAME, PClassInfo::kNameSize - 1);
            strncpy(info->subCategories, PlugType::kInstrumentSynth, PClassInfo2::kSubCategoriesSize - 1);
        } else {
            memcpy(info->cid, kG2EditControllerUID.toTUID(), sizeof(TUID));
            strncpy(info->category, kVstComponentControllerClass, PClassInfo::kCategorySize - 1);
            strncpy(info->name, G2_PLUGIN_NAME " Controller", PClassInfo::kNameSize - 1);
        }
        return kResultOk;
    }

    tresult PLUGIN_API getClassInfoUnicode(int32 index, PClassInfoW * info) SMTG_OVERRIDE {
        if ((info == nullptr) || (index < 0) || (index > 1)) {
            return kInvalidArgument;
        }
        memset(info, 0, sizeof(PClassInfoW));
        info->cardinality = PClassInfo::kManyInstances;
        info->classFlags  = 0;

        if (index == 0) {
            memcpy(info->cid, kG2EditProcessorUID.toTUID(), sizeof(TUID));
            strncpy(info->category, kVstAudioEffectClass, PClassInfo::kCategorySize - 1);
            strncpy(info->subCategories, PlugType::kInstrumentSynth, PClassInfo2::kSubCategoriesSize - 1);
            widen(info->name, G2_PLUGIN_NAME, PClassInfo::kNameSize);
        } else {
            memcpy(info->cid, kG2EditControllerUID.toTUID(), sizeof(TUID));
            strncpy(info->category, kVstComponentControllerClass, PClassInfo::kCategorySize - 1);
            widen(info->name, G2_PLUGIN_NAME " Controller", PClassInfo::kNameSize);
        }
        widen(info->vendor, G2_VENDOR, PClassInfo2::kVendorSize);
        widen(info->version, G2_VERSION, PClassInfo2::kVersionSize);
        widen(info->sdkVersion, kVstVersionString, PClassInfo2::kVersionSize);
        return kResultOk;
    }

    tresult PLUGIN_API setHostContext(FUnknown * context) SMTG_OVERRIDE {
        (void)context;
        return kResultOk;
    }

    tresult PLUGIN_API createInstance(FIDString cid, FIDString _iid, void ** obj) SMTG_OVERRIDE {
        FUnknown * instance = nullptr;

        if (memcmp(cid, kG2EditProcessorUID.toTUID(), sizeof(TUID)) == 0) {
            instance = G2EditPlugin::createInstance(nullptr);
        } else if (memcmp(cid, kG2EditControllerUID.toTUID(), sizeof(TUID)) == 0) {
            instance = G2EditController::createInstance(nullptr);
        } else {
            return kResultFalse;
        }
        // _iid is a FIDString (const char *); queryInterface's TUID parameter decays to the same
        // thing, so it is passed straight through — a cast to TUID would be a cast to an array type.
        tresult    result   = instance->queryInterface(_iid, obj);

        instance->release();        // queryInterface took its own reference
        return result;
    }

private:
    // ASCII into the char16 fields PClassInfoW uses. Everything here is ASCII, so a widening copy is
    // the whole conversion.
    static void widen(char16 * dst, const char * src, int32 capacity) {
        int32 i = 0;

        for (; (src[i] != '\0') && (i < capacity - 1); i++) {
            dst[i] = (char16)src[i];
        }
        dst[i] = 0;
    }

    std::atomic<int32> refCount;
};

extern "C" {
SMTG_EXPORT_SYMBOL IPluginFactory * PLUGIN_API GetPluginFactory(void) {
    return new G2EditFactory();
}

// macOS loads a .vst3 as a bundle, so these are the entry points rather than a plain dylib's.
SMTG_EXPORT_SYMBOL bool bundleEntry(void * ref) {
    (void)ref;
    return true;
}

SMTG_EXPORT_SYMBOL bool bundleExit(void) {
    return true;
}
}
