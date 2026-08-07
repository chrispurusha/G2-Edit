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
// One class implements IComponent, IAudioProcessor and IEditController together. VST3 allows a
// processor and a controller to be separate objects, and a large plug-in wants that separation, but
// this one has no parameters to automate and no editor, so splitting them would be three files of
// ceremony around nothing.

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
#include "pluginterfaces/vst/ivstevents.h"

extern "C" {
#include "soundEngine.h"
#include "g2Patch.h"
}

using namespace Steinberg;
using namespace Steinberg::Vst;

// Stable identity. A host remembers a plug-in by this, so it must never change once a project has
// been saved against it.
static const FUID kG2EditProcessorUID(0x9A47C1E2, 0x5B3D4F08, 0xA1726C93, 0xD4E8B550);

#define G2_VENDOR       "Chris Turner"
#define G2_PLUGIN_NAME  "G2 Edit"
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

class G2EditPlugin : public IComponent, public IAudioProcessor, public IEditController {
public:
    G2EditPlugin(void) : refCount(1), sampleRate(44100.0), active(false) {}
    virtual ~G2EditPlugin(void) {}

    // ---- FUnknown ------------------------------------------------------------------------------
    tresult PLUGIN_API queryInterface(const TUID iid, void ** obj) SMTG_OVERRIDE {
        QUERY_INTERFACE(iid, obj, FUnknown::iid, IComponent)
        QUERY_INTERFACE(iid, obj, IPluginBase::iid, IComponent)
        QUERY_INTERFACE(iid, obj, IComponent::iid, IComponent)
        QUERY_INTERFACE(iid, obj, IAudioProcessor::iid, IAudioProcessor)
        QUERY_INTERFACE(iid, obj, IEditController::iid, IEditController)
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
        // Processor and controller are the same object here, so there is no separate class to name.
        (void)classId;
        return kNotImplemented;
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
                    sound_engine_note(e.noteOn.pitch, e.noteOn.velocity > 0.0f);
                } else if (e.type == Event::kNoteOffEvent) {
                    sound_engine_note(e.noteOff.pitch, false);
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

    // ---- IEditController -----------------------------------------------------------------------
    // No automatable parameters yet: the patch is the state, and the morph groups are the obvious
    // first thing to expose once there is something to test against.
    tresult PLUGIN_API setComponentState(IBStream * state) SMTG_OVERRIDE {
        return setState(state);
    }

    int32 PLUGIN_API getParameterCount(void) SMTG_OVERRIDE {
        return 0;
    }

    tresult PLUGIN_API getParameterInfo(int32 paramIndex, ParameterInfo & info) SMTG_OVERRIDE {
        (void)paramIndex;
        (void)info;
        return kResultFalse;
    }

    tresult PLUGIN_API getParamStringByValue(ParamID id, ParamValue valueNormalized, String128 string) SMTG_OVERRIDE {
        (void)id;
        (void)valueNormalized;
        (void)string;
        return kResultFalse;
    }

    tresult PLUGIN_API getParamValueByString(ParamID id, TChar * string, ParamValue & valueNormalized) SMTG_OVERRIDE {
        (void)id;
        (void)string;
        (void)valueNormalized;
        return kResultFalse;
    }

    ParamValue PLUGIN_API normalizedParamToPlain(ParamID id, ParamValue valueNormalized) SMTG_OVERRIDE {
        (void)id;
        return valueNormalized;
    }

    ParamValue PLUGIN_API plainParamToNormalized(ParamID id, ParamValue plainValue) SMTG_OVERRIDE {
        (void)id;
        return plainValue;
    }

    ParamValue PLUGIN_API getParamNormalized(ParamID id) SMTG_OVERRIDE {
        (void)id;
        return 0.0;
    }

    tresult PLUGIN_API setParamNormalized(ParamID id, ParamValue value) SMTG_OVERRIDE {
        (void)id;
        (void)value;
        return kResultFalse;
    }

    tresult PLUGIN_API setComponentHandler(IComponentHandler * handler) SMTG_OVERRIDE {
        (void)handler;
        return kResultOk;
    }

    IPlugView * PLUGIN_API createView(FIDString name) SMTG_OVERRIDE {
        (void)name;
        return nullptr;             // no editor; the host draws a generic panel
    }

    static FUnknown * createInstance(void * /*context*/) {
        return (IAudioProcessor *)new G2EditPlugin();
    }

private:
    static const int32 kMaxBlock = 4096;

    void load_patch(void) {
        // Slot 0 always: a plug-in instance is one patch, and the four-slot performance layout is a
        // hardware notion with nothing to map onto here.
        g2_plugin_load_patch(patchPath.c_str(), 0);

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
    double             sampleRate;
    bool               active;
    std::string        patchPath;
    float              scratch[kMaxBlock * 2];
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
        return 1;
    }

    tresult PLUGIN_API getClassInfo(int32 index, PClassInfo * info) SMTG_OVERRIDE {
        if ((index != 0) || (info == nullptr)) {
            return kInvalidArgument;
        }
        memset(info, 0, sizeof(PClassInfo));
        memcpy(info->cid, kG2EditProcessorUID.toTUID(), sizeof(TUID));
        info->cardinality = PClassInfo::kManyInstances;
        strncpy(info->category, kVstAudioEffectClass, PClassInfo::kCategorySize - 1);
        strncpy(info->name, G2_PLUGIN_NAME, PClassInfo::kNameSize - 1);
        return kResultOk;
    }

    // The one that actually matters — see the note on the class. kInstrumentSynth is "Instrument|Synth".
    tresult PLUGIN_API getClassInfo2(int32 index, PClassInfo2 * info) SMTG_OVERRIDE {
        if ((index != 0) || (info == nullptr)) {
            return kInvalidArgument;
        }
        memset(info, 0, sizeof(PClassInfo2));
        memcpy(info->cid, kG2EditProcessorUID.toTUID(), sizeof(TUID));
        info->cardinality = PClassInfo::kManyInstances;
        info->classFlags  = 0;
        strncpy(info->category, kVstAudioEffectClass, PClassInfo::kCategorySize - 1);
        strncpy(info->name, G2_PLUGIN_NAME, PClassInfo::kNameSize - 1);
        strncpy(info->subCategories, PlugType::kInstrumentSynth, PClassInfo2::kSubCategoriesSize - 1);
        strncpy(info->vendor, G2_VENDOR, PClassInfo2::kVendorSize - 1);
        strncpy(info->version, G2_VERSION, PClassInfo2::kVersionSize - 1);
        strncpy(info->sdkVersion, kVstVersionString, PClassInfo2::kVersionSize - 1);
        return kResultOk;
    }

    tresult PLUGIN_API getClassInfoUnicode(int32 index, PClassInfoW * info) SMTG_OVERRIDE {
        if ((index != 0) || (info == nullptr)) {
            return kInvalidArgument;
        }
        memset(info, 0, sizeof(PClassInfoW));
        memcpy(info->cid, kG2EditProcessorUID.toTUID(), sizeof(TUID));
        info->cardinality = PClassInfo::kManyInstances;
        info->classFlags  = 0;
        strncpy(info->category, kVstAudioEffectClass, PClassInfo::kCategorySize - 1);
        strncpy(info->subCategories, PlugType::kInstrumentSynth, PClassInfo2::kSubCategoriesSize - 1);
        widen(info->name, G2_PLUGIN_NAME, PClassInfo::kNameSize);
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
        if (memcmp(cid, kG2EditProcessorUID.toTUID(), sizeof(TUID)) != 0) {
            return kResultFalse;
        }
        FUnknown * instance = G2EditPlugin::createInstance(nullptr);
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
