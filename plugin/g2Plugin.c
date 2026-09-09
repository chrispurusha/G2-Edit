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

// EVERYTHING ABOUT "G2 Alike" THAT A PLUG-IN FORMAT NEEDS TO KNOW, and nothing about any format.
//
// This file is the whole of what used to be g2Vst3.cpp's G2-specific half - ten parameters, a patch
// path, notes, and a canvas to draw. The VST3 plumbing that surrounded it, and the Audio Unit
// plumbing that would have had to be written a second time beside it, are now in SynthLib's
// plugin/ folder and are shared. `./do-plugin au` and `./do-plugin vst3` compile this identical file.
//
// It is C, and so is SynthLib's descriptor, so nothing here needs C++ or Objective-C: the two
// places that do - a VST3 vtable and a Cocoa view - are on the other side of the seam.
//
// THE ENGINE IS PROCESS-WIDE. soundEngine.c keeps its state in globals reached through atomics, so
// this "instance" is a handle rather than an owner: two copies of the plug-in in one project would
// fight over the same engine. Both wrappers know this and say so; it is a property of the engine,
// not of the wrapping.

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysIncludes.h"
#include "defs.h"                   // TARGET_FRAME_BUFF_WIDTH / _HEIGHT - the locked canvas ratio

#include "synthlibPlugin.h"

#include "soundEngine.h"
#include "noteStack.h"
#include "prefs.h"
#include "g2Patch.h"
#include "g2Prefs.h"
#include "g2View.h"

// ------------------------------------------------------------------------------------------------
// Identity
// ------------------------------------------------------------------------------------------------

// THESE BYTES MAY NEVER CHANGE. A host remembers a plug-in by them, so a project saved against this
// build must find the same numbers next time or it reopens with an empty slot.
//
// They are the same two ids g2Vst3.cpp declared as FUIDs, written out as bytes. A VST3 FUID built
// from four uint32s lays each one out big-endian on every platform except Windows, which is what
// makes 0x7D14B03C the four bytes 7D 14 B0 3C - so this is the identical plug-in to a host that
// already knows it, not a new one.
static const uint8_t gProcessorUid[16] = {
    0x7D, 0x14, 0xB0, 0x3C, 0x6E, 0x28, 0x4A, 0x97,
    0x8C, 0x5F, 0x1D, 0x62, 0xB9, 0x3A, 0x47, 0xE1
};

static const uint8_t gControllerUid[16] = {
    0x2B, 0x69, 0xF5, 0x8D, 0x41, 0xA7, 0x0C, 0x36,
    0x95, 0xE2, 0x84, 0xBF, 0x1D, 0x60, 0x35, 0xCA
};

#define FOUR_CC(a, b, c, d)    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
                                ((uint32_t)(c) << 8) | (uint32_t)(d))

// 'aumu' is kAudioUnitType_MusicDevice - an instrument. The subtype and manufacturer are ours, and
// carry the same "never change these" warning as the VST3 ids above; the manufacturer code is
// shared with any future Audio Unit from these projects.
#define G2_AU_TYPE            FOUR_CC('a', 'u', 'm', 'u')
#define G2_AU_SUBTYPE         FOUR_CC('G', '2', 'a', 'l')
#define G2_AU_MANUFACTURER    FOUR_CC('C', 'P', 'u', 'r')
#define G2_AU_VERSION         (0x00000100)     // 0.1.0, as 0xMMMMmmbb

// MUST MATCH CFBundleIdentifier IN THE .component's Info.plist. The Audio Unit wrapper looks its own
// bundle up by this string to tell a host where the editor's view class lives; a mismatch is a
// plug-in that loads, plays, and has no editor. do-plugin writes both from one place.
#define G2_AU_BUNDLE_ID       "com.chrispurusha.g2alike.au"

// ------------------------------------------------------------------------------------------------
// Parameters
// ------------------------------------------------------------------------------------------------

#define G2_MORPH_COUNT        (8)              // NUM_MORPHS
#define G2_PARAM_LEVEL        (8)
#define G2_PARAM_BEND         (9)
#define G2_NUM_PARAMS         (10)

// The output trim ATTENUATES ONLY - sound_engine_set_output_level_db() clamps anything at or above
// 0 dB to unity, deliberately, so this is a fader and not a boost into the limiter.
#define G2_LEVEL_MIN_DB       (-60.0)

// WHICH MORPH GROUP THE G2 WIRES EACH PHYSICAL CONTROL TO (midiInput.c's MORPH_GROUP_*), and
// therefore which of parameters 0-7 a MIDI control should drive. The G2 hard-wires its wheels and
// pedals to particular morph groups - morphStrMap in moduleResources.h names them - and those groups
// are already parameters, so most of the table below maps a control onto a parameter that exists
// rather than inventing one. Only pitch bend needs a parameter of its own, having no morph group.
//
// The wiring is in the table's midiControl column; this one is named because the poly-pressure path
// does not go through the table - see g2_poly_pressure().
#define G2_MORPH_AFTERTOUCH   (3)

static const tSynthLibParam gParams[G2_NUM_PARAMS] = {
    // The eight morph groups are the G2's own performance controls, so they are the right things to
    // put in front of a host: automating a morph is the nearest thing to playing the hardware.
    { 0, "Morph 1", "Morph 1", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_MOD_WHEEL },
    { 1, "Morph 2", "Morph 2", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE },
    { 2, "Morph 3", "Morph 3", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE },
    { 3, "Morph 4", "Morph 4", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_AFTERTOUCH },
    { 4, "Morph 5", "Morph 5", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_SUSTAIN },
    { 5, "Morph 6", "Morph 6", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_FOOT },
    { 6, "Morph 7", "Morph 7", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE },
    { 7, "Morph 8", "Morph 8", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE },

    { G2_PARAM_LEVEL, "Output Level", "Level", eSynthLibUnitDecibels,
      G2_LEVEL_MIN_DB, 0.0, 1.0, 0, SYNTHLIB_MIDI_NONE },

    // PITCH BEND HAS TO BE A REAL, DECLARED PARAMETER even though nobody would choose to automate
    // it: a VST3 host converts the wheel into a parameter change and can only name a parameter that
    // exists. 0.5 is centre, which is why the default is normalized and not plain.
    { G2_PARAM_BEND, "Pitch Bend", "Bend", eSynthLibUnitGeneric,
      -1.0, 1.0, 0.5, 0, SYNTHLIB_MIDI_PITCH_BEND }
};

// ------------------------------------------------------------------------------------------------
// The instance
// ------------------------------------------------------------------------------------------------

#define G2_MAX_BLOCK    (4096)

typedef struct {
    char             patchPath[1024];
    bool             active;
    double           sampleRate;
    double           params[G2_NUM_PARAMS];

    // sound_engine_render() writes INTERLEAVED frames and both plug-in formats hand over one buffer
    // per channel, so it renders here and is de-interleaved out. Bounded by G2_MAX_BLOCK and looped,
    // so an unusually large host buffer cannot overrun it.
    float            scratch[G2_MAX_BLOCK * 2];
} tG2Plugin;

// A MORPH DOES NOT REACH THE AUDIO THREAD BY ITSELF, and this flag is how the plug-in copes.
//
// sound_engine_set_morph() only records the position. Unlike pitch bend, which the audio thread
// reads directly, a morph is folded into the parameter SNAPSHOT, and that snapshot is only rebuilt
// by sound_engine_update_from_patch(). The standalone editor rebuilds it on every redraw, which is
// why moving a morph there requires asking for one - and why its mod wheel response is capped at the
// frame rate, since a full canvas repaint sits between the wheel and the sound.
//
// A plug-in cannot borrow that arrangement: it has to work with the editor window closed. So the
// rebuild happens in render() instead, once per block, and only when something actually moved.
//
// A FLAG RATHER THAN REBUILDING ON THE SPOT, because the snapshot is published through a SEQLOCK
// (gParamsSeq in soundEngine.c). A seqlock tolerates exactly one writer; the audio thread is already
// its reader, and a parameter change can arrive on the host's UI thread. Letting both write would
// corrupt it. So every setter merely sets this, and render() - one thread, once per block - is the
// only writer.
static atomic_bool gMorphSnapshotDirty;

// ------------------------------------------------------------------------------------------------

static double level_db(double normalized) {
    return G2_LEVEL_MIN_DB + (normalized * (0.0 - G2_LEVEL_MIN_DB));
}

// Where the patch comes from when the host has not restored one. Checked in order:
//   1. the path the host restored with the project (g2_set_state below)
//   2. $G2_PLUGIN_PATCH, then the older $G2_VST3_PATCH
//   3. ~/Documents/G2-Edit/plugin.pch2
//
// The host-stored path is what makes a project reopen sounding as it did; the environment variable
// is for driving it from a test script - a host launched from the Dock inherits no shell environment,
// so it only ever applies to a scripted run - and the fixed location is so it does something
// sensible with neither set.
static void default_patch_path(char * out, size_t len) {
    const char * env = getenv("G2_PLUGIN_PATCH");

    if ((env == NULL) || (env[0] == '\0')) {
        env = getenv("G2_VST3_PATCH");      // the name this had before there were two formats
    }

    if ((env != NULL) && (env[0] != '\0')) {
        snprintf(out, len, "%s", env);
        return;
    }
    const char * home = getenv("HOME");

    snprintf(out, len, "%s/Documents/G2-Edit/plugin.pch2", (home != NULL) ? home : ".");
}

static void load_patch(tG2Plugin * g2) {
    // THE PATH, not a patch compiled into the binary. The built-in patch was a scaffold from before
    // the plug-in had an editor: with no way to choose a file, embedding one removed a whole class of
    // "why is it silent" while the rest was proven. File > Open Patch File... has replaced it, and a
    // plug-in that quietly plays somebody else's lead patch on load is worse than one that starts
    // empty.
    //
    // Slot 0: a plug-in instance is one patch, and the four-slot performance layout is a hardware
    // notion with nothing to map onto here. An empty path or a missing file simply leaves the canvas
    // empty, which is honest.
    (void)g2_plugin_load_patch(g2->patchPath, 0);

    // Only meaningful once the engine is live - see g2_set_active(). Harmless when it is not, and
    // called anyway so that a patch swapped in mid-session takes effect immediately.
    if (g2->active == true) {
        sound_engine_update_from_patch();
    }
}

// ------------------------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------------------------

static void * g2_create(void) {
    tG2Plugin * g2 = (tG2Plugin *)calloc(1, sizeof(tG2Plugin));

    if (g2 == NULL) {
        return NULL;
    }

    for (uint32_t i = 0; i < G2_NUM_PARAMS; i++) {
        g2->params[i] = gParams[i].defaultNormalized;
    }
    g2->sampleRate = 44100.0;
    return g2;
}

static void g2_destroy(void * inst) {
    free(inst);
}

static void g2_initialize(void * inst) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    if (g2->patchPath[0] == '\0') {
        default_patch_path(g2->patchPath, sizeof(g2->patchPath));
    }
    load_patch(g2);
}

static void g2_terminate(void * inst) {
    (void)inst;
    sound_engine_stop_hosted();
}

static void g2_set_sample_rate(void * inst, double sampleRate) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    g2->sampleRate = sampleRate;
    sound_engine_set_sample_rate(sampleRate);
}

static void g2_set_active(void * inst, bool active) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    if (active == true) {
        sound_engine_start_hosted(g2->sampleRate);
        g2->active = true;

        // THE CHAIN IS RESOLVED HERE, not when the patch was read. sound_engine_update_from_patch()
        // returns immediately while the engine is inactive, so calling it at initialize() time -
        // which is the obvious place, and where this used to be - silently did nothing and the
        // plug-in rendered silence from a perfectly good patch.
        sound_engine_update_from_patch();
    } else {
        sound_engine_stop_hosted();
        g2->active = false;
    }
}

static void g2_reset(void * inst) {
    (void)inst;
    note_stack_all_off();
}

// ------------------------------------------------------------------------------------------------
// Audio
// ------------------------------------------------------------------------------------------------

static void g2_render(void * inst, float ** out, uint32_t numChannels, uint32_t frames) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    if ((numChannels < 2u) || (out == NULL)) {
        return;
    }

    // Fold any moved morph into the parameter snapshot. Once per block rather than once per change,
    // and only here - see gMorphSnapshotDirty for why this is the sole writer. Costs a database
    // walk, which is what the standalone pays on every frame anyway.
    if (atomic_exchange(&gMorphSnapshotDirty, false) == true) {
        sound_engine_update_from_patch();
    }
    uint32_t done = 0;

    while (done < frames) {
        uint32_t chunk = frames - done;

        if (chunk > (uint32_t)G2_MAX_BLOCK) {
            chunk = (uint32_t)G2_MAX_BLOCK;
        }
        sound_engine_render(g2->scratch, chunk, 2);

        for (uint32_t i = 0; i < chunk; i++) {
            out[0][done + i] = g2->scratch[i * 2];
            out[1][done + i] = g2->scratch[(i * 2) + 1];
        }
        done += chunk;
    }
}

// ------------------------------------------------------------------------------------------------
// Events
// ------------------------------------------------------------------------------------------------

// THROUGH THE SHARED NOTE STACK, NOT STRAIGHT TO THE ENGINE. The engine is monophonic, so releasing
// a note has to fall back to whatever is still held or legato playing breaks - hold D, play F, let F
// go, and the D under your finger must come back rather than the sound stopping. noteStack.c is the
// application's own logic, moved out of midiInput.c so both get it from one place.
static void g2_note_on(void * inst, uint8_t note, float velocity) {
    (void)inst;
    (void)velocity;                             // the engine has no velocity response yet
    note_stack_note_on(note);
}

static void g2_note_off(void * inst, uint8_t note) {
    (void)inst;
    note_stack_note_off(note);
}

static void g2_poly_pressure(void * inst, uint8_t note, float pressure) {
    (void)inst;

    // The engine has one voice, so as in the application only the note actually SOUNDING may move
    // the morph; without that test a key still held underneath would fight the one being played.
    if ((int32_t)note != note_stack_top()) {
        return;
    }

    if (sound_engine_set_morph(G2_MORPH_AFTERTOUCH, (double)pressure) == true) {
        atomic_store(&gMorphSnapshotDirty, true);
    }
}

// ------------------------------------------------------------------------------------------------
// Parameters
// ------------------------------------------------------------------------------------------------

// Both a host's generic panel (on its UI thread) and automation (on the audio thread) land here.
// Every engine entry point it calls stores through an atomic, so there is nothing to guard.
static void g2_set_param(void * inst, uint32_t id, double normalized) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    if (id >= G2_NUM_PARAMS) {
        return;
    }
    g2->params[id] = normalized;

    if (id < (uint32_t)G2_MORPH_COUNT) {
        // The return says whether the position actually changed - no point rebuilding a snapshot for
        // a host resending a value it already sent.
        if (sound_engine_set_morph(id, normalized) == true) {
            atomic_store(&gMorphSnapshotDirty, true);
        }
    } else if (id == (uint32_t)G2_PARAM_LEVEL) {
        sound_engine_set_output_level_db(level_db(normalized));
    } else if (id == (uint32_t)G2_PARAM_BEND) {
        // The host hands pitch bend over as 0..1 with 0.5 at rest; the engine wants -1..+1, and
        // decides for itself how many semitones that is from the patch's own Bend setting.
        sound_engine_pitch_bend((normalized * 2.0) - 1.0);
    }
}

static double g2_get_param(void * inst, uint32_t id) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    return (id < G2_NUM_PARAMS) ? g2->params[id] : 0.0;
}

static bool g2_param_text(void * inst, uint32_t id, double normalized, char * out, size_t len) {
    (void)inst;

    if (id == (uint32_t)G2_PARAM_LEVEL) {
        double db = level_db(normalized);

        // "-60.0 dB" is a real setting; the bottom of the fader is silence, and saying so is the
        // whole reason this parameter needs its own text.
        if (db <= G2_LEVEL_MIN_DB) {
            snprintf(out, len, "-inf");
        } else {
            snprintf(out, len, "%.1f dB", db);
        }
        return true;
    }

    if (id == (uint32_t)G2_PARAM_BEND) {
        snprintf(out, len, "%+.2f", (normalized * 2.0) - 1.0);
        return true;
    }
    return false;       // the morphs read fine as a percentage; SynthLib formats them
}

// ------------------------------------------------------------------------------------------------
// State
// ------------------------------------------------------------------------------------------------

// THE PATCH IS IDENTIFIED BY PATH rather than embedded wholesale. A .pch2 is small enough to embed,
// and doing so would make a project self-contained, but it would also freeze a copy: edit the patch
// in G2-Edit and the project would go on playing the old one, silently. Storing the path keeps one
// patch with one meaning.
//
// NO TERMINATOR IS WRITTEN. The blob's length is its length - synthlibPluginState.c records it - and
// this is also exactly what the plug-in's state was before that header existed, so a project saved
// by an older build still restores its patch.
static size_t g2_get_state(void * inst, void * out, size_t len) {
    tG2Plugin * g2   = (tG2Plugin *)inst;
    size_t      need = strlen(g2->patchPath);

    if ((out != NULL) && (len >= need) && (need > 0u)) {
        memcpy(out, g2->patchPath, need);
    }
    return need;
}

static void g2_set_state(void * inst, const void * data, size_t len) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    if ((data == NULL) || (len == 0u)) {
        return;
    }

    if (len >= sizeof(g2->patchPath)) {
        len = sizeof(g2->patchPath) - 1u;
    }
    memcpy(g2->patchPath, data, len);
    g2->patchPath[len] = '\0';
    load_patch(g2);
}

// ------------------------------------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------------------------------------

// THE EDITOR IS THE APPLICATION'S OWN CANVAS, not a second renderer. g2View.m is the NSView, g2Draw.c
// draws the frame by calling render_modules() / render_cables(), and the menu bar is the
// application's too - so the editor has File, Settings, Controls, Tools, View and Help.
//
// What made that possible was moving the drawing behind a render backend: the application reaches
// its window through GLFW, which creates and owns one, while a plug-in is handed an NSView the HOST
// owns and GLFW has no "adopt this existing NSView". gfx_attach_window() takes the host's view,
// SynthLib's utilsGraphics.c is the only thing that draws, and the same canvas code serves both.
static void * g2_create_view(void * inst, double width, double height) {
    (void)inst;
    return g2_view_create(width, height);
}

static void g2_view_resized(void * inst, void * view, double width, double height) {
    (void)inst;
    (void)view;
    (void)width;
    (void)height;

    // The view resizes its own layer from its backing size on the next frame, so all this needs is
    // for there to BE a next frame.
    g2_view_request_redraw();
}

static void g2_destroy_view(void * inst, void * view) {
    (void)inst;
    g2_view_destroy(view);
}

static long g2_editor_width_load(void) {
    g2_plugin_prefs_init();
    return prefs_get_int(G2_PREF_EDITOR_WIDTH, (long)900);
}

static void g2_editor_width_save(long width) {
    g2_plugin_prefs_init();
    prefs_set_int(G2_PREF_EDITOR_WIDTH, width);
}

// ------------------------------------------------------------------------------------------------
// The descriptor
// ------------------------------------------------------------------------------------------------

static const tSynthLibPluginDesc gDescriptor = {
    .name              = "G2 Alike",
    .vendor            = "Chris Purusha",
    .url               = "https://github.com/chrispurusha",
    .email             = "",
    .version           = "0.1.0",

    .isInstrument      = true,
    .numInputChannels  = 0,
    .numOutputChannels = 2,
    .wantsMidi         = true,

    .vst3ProcessorUid  = gProcessorUid,
    .vst3ControllerUid = gControllerUid,

    .auType            = G2_AU_TYPE,
    .auSubType         = G2_AU_SUBTYPE,
    .auManufacturer    = G2_AU_MANUFACTURER,
    .auVersion         = G2_AU_VERSION,
    .auBundleId        = G2_AU_BUNDLE_ID,

    .params            = gParams,
    .numParams         = G2_NUM_PARAMS,

    // 900 points wide, and the height follows the ratio the application locks its own window to
    // (TARGET_FRAME_BUFF_WIDTH : TARGET_FRAME_BUFF_HEIGHT, 2560:1440).
    //
    // THE LOCK IS WHAT COMPLETES THE SCALING. gGlobalGuiScale is derived from WIDTH alone, so on its
    // own a taller window would simply uncover more rows rather than drawing the patch larger. The
    // application never shows that because its window cannot be made taller without also becoming
    // wider. Below 640 the module text stops being legible; there is no maximum, since everything
    // scales.
    .editorDefaultWidth = 900.0,
    .editorMinWidth     = 640.0,
    .editorAspect       = (double)TARGET_FRAME_BUFF_WIDTH / (double)TARGET_FRAME_BUFF_HEIGHT,
    .editorWidthLoad    = g2_editor_width_load,
    .editorWidthSave    = g2_editor_width_save,

    .cb = {
        .create        = g2_create,
        .destroy       = g2_destroy,
        .initialize    = g2_initialize,
        .terminate     = g2_terminate,
        .setSampleRate = g2_set_sample_rate,
        .setActive     = g2_set_active,
        .reset         = g2_reset,

        .render        = g2_render,

        .noteOn        = g2_note_on,
        .noteOff       = g2_note_off,
        .polyPressure  = g2_poly_pressure,

        .setParam      = g2_set_param,
        .getParam      = g2_get_param,
        .paramText     = g2_param_text,

        .getState      = g2_get_state,
        .setState      = g2_set_state,

        .createView    = g2_create_view,
        .viewResized   = g2_view_resized,
        .destroyView   = g2_destroy_view
    }
};

const tSynthLibPluginDesc * synthlib_plugin_descriptor(void) {
    return &gDescriptor;
}
