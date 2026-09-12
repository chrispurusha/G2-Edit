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
// Notes: Docs/code-notes/g2Plugin.c.md - "// notes §k" refers there.

// notes §1

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysIncludes.h"
#include "defs.h"                   // TARGET_FRAME_BUFF_WIDTH / _HEIGHT - the locked canvas ratio

#include "synthlibPlugin.h"

#include "globalVars.h"             // the document, and the names that are macros onto it
#include "dataBase.h"               // init_patch(), slot_has_modules()
#include "soundEngine.h"
#include "noteStack.h"
#include "prefs.h"
#include "g2Patch.h"
#include "g2Prefs.h"
#include "g2View.h"

// ------------------------------------------------------------------------------------------------
// Identity
// ------------------------------------------------------------------------------------------------

// notes §2
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

// notes §3
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

#define G2_MAX_BLOCK     (4096)
#define G2_MAX_EVENTS    (256)

// A note waiting for its sample. Both wrappers hand a block's notes over BEFORE asking for the block,
// each with its offset into it, so the note is held until the render reaches that offset.
typedef struct {
    uint32_t offset;
    uint8_t  note;
    bool     on;
} tG2NoteEvent;

typedef struct {
    // This instance's G2: its four slots, its settings, and through engineIndex its engine.
    tG2Document *    doc;

    char             patchPath[1024];
    bool             active;
    double           sampleRate;
    double           params[G2_NUM_PARAMS];

    // See the note below.
    atomic_bool      morphSnapshotDirty;

    // This block's notes, in arrival order - which both wrappers make offset order. Audio thread only.
    tG2NoteEvent     events[G2_MAX_EVENTS];
    uint32_t         eventCount;

    // sound_engine_render() writes INTERLEAVED frames and both plug-in formats hand over one buffer
    // per channel, so it renders here and is de-interleaved out. Bounded by G2_MAX_BLOCK and looped,
    // so an unusually large host buffer cannot overrun it.
    float            scratch[G2_MAX_BLOCK * 2];
} tG2Plugin;

// EVERY ENTRY STARTS HERE. Makes this instance's document - and so its engine - the current one on the
// calling thread. Cheap (one thread-local store), and it has to be unconditional: a host may call two
// instances from one thread in turn, and a stale selection would play or edit the other one.
static tG2Plugin * enter(void * inst) {
    tG2Plugin * g2 = (tG2Plugin *)inst;

    g2_document_select(g2->doc);
    return g2;
}

// notes §4

// ------------------------------------------------------------------------------------------------

static double level_db(double normalized) {
    return G2_LEVEL_MIN_DB + (normalized * (0.0 - G2_LEVEL_MIN_DB));
}

// notes §5
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
    // notes §6
    if ((g2_plugin_open_file(g2->patchPath, 0) == eG2FileFailed) && (g2->patchPath[0] != '\0')) {
        snprintf(gSavedPatchPath[0], FILE_PATH_SIZE, "%s", g2->patchPath);
    }

    // Only meaningful once the engine is live - see g2_set_active(). Harmless when it is not, and
    // called anyway so that a patch swapped in mid-session takes effect immediately.
    if (g2->active == true) {
        sound_engine_update_from_patch();
    }
}

// ------------------------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------------------------

static void * g2_create(const tSynthLibPluginDesc * desc) {
    tG2Plugin * g2 = (tG2Plugin *)calloc(1, sizeof(tG2Plugin));

    (void)desc;     // one variant only - see synthlib_plugin_variants() at the foot of this file

    if (g2 == NULL) {
        return NULL;
    }
    g2->doc = g2_document_create();

    if (g2->doc == NULL) {
        free(g2);
        return NULL;
    }
    enter(g2);

    // AN ENGINE OF ITS OWN, or no instance at all. Running out (SOUND_ENGINE_MAX_ENGINES) fails the
    // load, which a host reports, rather than quietly sharing an engine with another track.
    if (sound_engine_attach() == false) {
        g2_document_select(NULL);
        g2_document_destroy(g2->doc);
        free(g2);
        return NULL;
    }
    note_stack_all_off();

    // All four slots start as the application's new empty patch, so selecting B, C or D in the editor
    // shows an empty patch rather than zeroed storage. Slot A is replaced by the patch loaded below.
    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        init_patch(slot);
    }

    for (uint32_t i = 0; i < G2_NUM_PARAMS; i++) {
        g2->params[i] = gParams[i].defaultNormalized;
    }
    g2->sampleRate = 44100.0;
    return g2;
}

static void g2_destroy(void * inst) {
    tG2Plugin * g2 = enter(inst);

    sound_engine_stop_hosted();
    sound_engine_detach();
    g2_document_select(NULL);
    g2_document_destroy(g2->doc);
    free(g2);
}

static void g2_initialize(void * inst) {
    tG2Plugin * g2 = enter(inst);

    if (g2->patchPath[0] == '\0') {
        default_patch_path(g2->patchPath, sizeof(g2->patchPath));
    }
    load_patch(g2);
}

static void g2_terminate(void * inst) {
    (void)enter(inst);
    sound_engine_stop_hosted();
}

static void g2_set_sample_rate(void * inst, double sampleRate) {
    tG2Plugin * g2 = enter(inst);

    g2->sampleRate = sampleRate;
    sound_engine_set_sample_rate(sampleRate);
}

static void g2_set_active(void * inst, bool active) {
    tG2Plugin * g2 = enter(inst);

    if (active == true) {
        sound_engine_start_hosted(g2->sampleRate);
        g2->active = true;

        // notes §7
        sound_engine_update_from_patch();
    } else {
        sound_engine_stop_hosted();
        g2->active = false;
    }
}

static void g2_reset(void * inst) {
    tG2Plugin * g2 = enter(inst);

    g2->eventCount = 0;
    note_stack_all_off();
}

// Renders `frames` into out[0..1] starting at `from`, in chunks the scratch buffer can hold.
static void render_span(tG2Plugin * g2, float ** out, uint32_t from, uint32_t frames) {
    uint32_t done = 0;

    while (done < frames) {
        uint32_t chunk = frames - done;

        if (chunk > (uint32_t)G2_MAX_BLOCK) {
            chunk = (uint32_t)G2_MAX_BLOCK;
        }
        sound_engine_render(g2->scratch, chunk, 2);

        for (uint32_t i = 0; i < chunk; i++) {
            out[0][from + done + i] = g2->scratch[i * 2];
            out[1][from + done + i] = g2->scratch[(i * 2) + 1];
        }
        done += chunk;
    }
}

static void apply_note(const tG2NoteEvent * e) {
    if (e->on) {
        note_stack_note_on(e->note);
    } else {
        note_stack_note_off(e->note);
    }
}

// ------------------------------------------------------------------------------------------------
// Audio
// ------------------------------------------------------------------------------------------------

static void g2_process(void * inst,
                       const float * const * in, uint32_t numIn,
                       float ** out, uint32_t numOut,
                       uint32_t frames,
                       const tSynthLibTransport * transport) {
    tG2Plugin * g2 = enter(inst);

    // An instrument: there is no input, and the transport is not read. The engine free-runs and has
    // nothing to sync to - a patch is a patch whether the host is rolling or not.
    (void)in;
    (void)numIn;
    (void)transport;

    if ((numOut < 2u) || (out == NULL)) {
        return;
    }

    // Fold any moved morph into the parameter snapshot. Once per block rather than once per change,
    // and only here - see morphSnapshotDirty for why this is the sole writer. Costs a database
    // walk, which is what the standalone pays on every frame anyway.
    if (atomic_exchange(&g2->morphSnapshotDirty, false) == true) {
        sound_engine_update_from_patch();
    }

    // notes §8
    uint32_t pos = 0;

    for (uint32_t i = 0; i < g2->eventCount; i++) {
        uint32_t at = (g2->events[i].offset < frames) ? g2->events[i].offset : frames;

        if (at > pos) {
            render_span(g2, out, pos, at - pos);
            pos = at;
        }
        apply_note(&g2->events[i]);
    }
    g2->eventCount = 0;

    if (pos < frames) {
        render_span(g2, out, pos, frames - pos);
    }
}

// Held for g2_process(), or applied at once if a host sends more notes in one block than there is
// room for - a note early is better than a note lost.
static void queue_note(tG2Plugin * g2, uint8_t note, bool on, uint32_t sampleOffset) {
    tG2NoteEvent e = {sampleOffset, note, on};

    if (g2->eventCount < (uint32_t)G2_MAX_EVENTS) {
        g2->events[g2->eventCount++] = e;
    } else {
        apply_note(&e);
    }
}

// ------------------------------------------------------------------------------------------------
// Events
// ------------------------------------------------------------------------------------------------

// notes §9
static void g2_note_on(void * inst, uint8_t channel, uint8_t note, float velocity,
                       uint32_t sampleOffset) {
    tG2Plugin * g2 = enter(inst);

    (void)channel;
    (void)velocity;                             // the engine has no velocity response yet
    queue_note(g2, note, true, sampleOffset);
}

static void g2_note_off(void * inst, uint8_t channel, uint8_t note, float velocity,
                        uint32_t sampleOffset) {
    tG2Plugin * g2 = enter(inst);

    (void)channel;
    (void)velocity;
    queue_note(g2, note, false, sampleOffset);
}

static void g2_poly_pressure(void * inst, uint8_t channel, uint8_t note, float pressure,
                             uint32_t sampleOffset) {
    tG2Plugin * g2 = enter(inst);

    (void)channel;
    (void)sampleOffset;

    // The engine has one voice, so as in the application only the note actually SOUNDING may move
    // the morph; without that test a key still held underneath would fight the one being played.
    if ((int32_t)note != note_stack_top()) {
        return;
    }

    if (sound_engine_set_morph(G2_MORPH_AFTERTOUCH, (double)pressure) == true) {
        atomic_store(&g2->morphSnapshotDirty, true);
    }
}

// ------------------------------------------------------------------------------------------------
// Parameters
// ------------------------------------------------------------------------------------------------

// Both a host's generic panel (on its UI thread) and automation (on the audio thread) land here.
// Every engine entry point it calls stores through an atomic, so there is nothing to guard.
static void g2_set_param(void * inst, uint32_t id, double normalized) {
    tG2Plugin * g2 = enter(inst);

    if (id >= G2_NUM_PARAMS) {
        return;
    }
    g2->params[id] = normalized;

    if (id < (uint32_t)G2_MORPH_COUNT) {
        // The return says whether the position actually changed - no point rebuilding a snapshot for
        // a host resending a value it already sent.
        if (sound_engine_set_morph(id, normalized) == true) {
            atomic_store(&g2->morphSnapshotDirty, true);
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

static bool g2_param_text(const tSynthLibPluginDesc * desc, void * inst, uint32_t id,
                          double normalized, char * out, size_t len) {
    (void)desc;
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

// notes §10
#define G2_STATE_HEADER    "G2Alike state 2\n"

static size_t g2_get_state(void * inst, void * out, size_t len) {
    tG2Plugin * g2 = enter(inst);
    char        text[(MAX_SLOTS + 1) * (FILE_PATH_SIZE + 16) + 64];
    size_t      used = 0;

    used += (size_t)snprintf(text + used, sizeof(text) - used, "%s", G2_STATE_HEADER);

    if ((gGlobalSettings.perfMode == 1) && (gSavedPerfPath[0] != '\0')) {
        used += (size_t)snprintf(text + used, sizeof(text) - used, "perf=%s\n", gSavedPerfPath);
    } else {
        for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
            const char * path = gSavedPatchPath[slot];

            if ((path[0] != '\0') && (used < sizeof(text))) {
                used += (size_t)snprintf(text + used, sizeof(text) - used, "slot%u=%s\n", (unsigned)slot, path);
            }
        }
    }

    if (used < sizeof(text)) {
        used += (size_t)snprintf(text + used, sizeof(text) - used, "perfmode=%u\nselected=%u\n",
                                 (unsigned)gGlobalSettings.perfMode, (unsigned)gSlot);
    }

    if (used > sizeof(text)) {
        used = sizeof(text);
    }

    if ((out != NULL) && (len >= used)) {
        memcpy(out, text, used);
    }
    return used;
}

// A v2 state record, read into this before anything is loaded, so a record that names only some
// slots can empty the rest (see g2_set_state()).
typedef struct {
    char    perf[FILE_PATH_SIZE];
    char    slot[MAX_SLOTS][FILE_PATH_SIZE];
    int32_t perfMode;
    int32_t selected;
} tG2State;

static void parse_state_line(tG2State * state, char * line) {
    char * eq = strchr(line, '=');

    if (eq == NULL) {
        return;     // Not ours; a later version's line, perhaps. Skipped rather than refused
    }
    *eq = '\0';

    const char * key   = line;
    const char * value = eq + 1;

    if (strcmp(key, "perf") == 0) {
        snprintf(state->perf, sizeof(state->perf), "%s", value);
    } else if ((strncmp(key, "slot", 4) == 0) && (key[4] >= '0') && (key[4] < (char)('0' + MAX_SLOTS)) && (key[5] == '\0')) {
        snprintf(state->slot[key[4] - '0'], sizeof(state->slot[0]), "%s", value);
    } else if (strcmp(key, "perfmode") == 0) {
        state->perfMode = atoi(value);
    } else if (strcmp(key, "selected") == 0) {
        state->selected = atoi(value);
    }
}

static void g2_set_state(void * inst, const void * data, size_t len) {
    tG2Plugin * g2        = enter(inst);
    size_t      headerLen = strlen(G2_STATE_HEADER);

    if ((data == NULL) || (len == 0u)) {
        return;
    }

    if ((len < headerLen) || (memcmp(data, G2_STATE_HEADER, headerLen) != 0)) {
        // The old format: a bare path, into slot A.
        if (len >= sizeof(g2->patchPath)) {
            len = sizeof(g2->patchPath) - 1u;
        }
        memcpy(g2->patchPath, data, len);
        g2->patchPath[len] = '\0';
        load_patch(g2);
        return;
    }
    tG2State * state = (tG2State *)calloc(1, sizeof(tG2State));
    char *     text  = (char *)malloc(len + 1u);
    char *     save  = NULL;

    if ((state == NULL) || (text == NULL)) {
        free(state);
        free(text);
        return;
    }
    state->perfMode = -1;
    state->selected = -1;
    memcpy(text, data, len);
    text[len] = '\0';

    for (char * line = strtok_r(text + headerLen, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        parse_state_line(state, line);
    }
    free(text);

    // notes §11
    gSavedPerfPath[0] = '\0';
    snprintf(g2->patchPath, sizeof(g2->patchPath), "%s", state->slot[0]);

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        gSavedPatchPath[slot][0] = '\0';
    }

    if (state->perf[0] != '\0') {
        if (g2_plugin_open_file(state->perf, 0) == eG2FileFailed) {
            for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
                clear_slot_data(slot);
                init_patch(slot);
            }
            snprintf(gSavedPerfPath, FILE_PATH_SIZE, "%s", state->perf);
        }
    } else {
        for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
            if ((state->slot[slot][0] != '\0') && (g2_plugin_open_file(state->slot[slot], slot) != eG2FileFailed)) {
                continue;
            }
            // Nothing named, or the file has gone: an empty slot, as the editor's A-D would show it.
            clear_slot_data(slot);
            init_patch(slot);
            snprintf(gSavedPatchPath[slot], FILE_PATH_SIZE, "%s", state->slot[slot]);
        }
    }

    // AFTER the files: loading a performance sets both of these from the file itself, and the
    // record says what they were when the project was saved.
    if ((state->perfMode == 0) || (state->perfMode == 1)) {
        gGlobalSettings.perfMode = (uint8_t)state->perfMode;
    }

    if ((state->selected >= 0) && (state->selected < MAX_SLOTS)) {
        gSlot = (uint32_t)state->selected;
    }
    free(state);

    if (g2->active == true) {
        sound_engine_update_from_patch();
    }
    g2_view_request_redraw();
}

// ------------------------------------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------------------------------------

// notes §12
static void * g2_create_view(const tSynthLibPluginDesc * desc, void * inst, double width, double height) {
    tG2Plugin * g2 = enter(inst);

    (void)desc;

    // The view is handed its instance's document and selects it itself before every frame and every
    // event - its callbacks come from AppKit, not through this file.
    return g2_view_create(g2->doc, width, height);
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

static const tSynthLibBus gOutputs[1] = {
    { "Output", 2, false, true }
};

static const tSynthLibPluginDesc gDescriptor = {
    .name              = "G2 Alike",
    .vendor            = "Chris Purusha",
    .url               = "https://github.com/chrispurusha",
    .email             = "",
    .version           = "0.1.0",

    .isInstrument      = true,
    .vst3SubCategory   = NULL,          // "Instrument|Synth", from isInstrument
    .inputs            = NULL,
    .numInputs         = 0,
    .outputs           = gOutputs,
    .numOutputs        = 1,
    .wantsMidiIn       = true,
    .wantsTransport    = false,

    // Levels, not events: a morph or the output level delivered twice - once here on the UI thread,
    // once by the host inside a block - is harmless, and on a host that never routes the controller's
    // values to the processor it is the only way a move on the host's own panel is heard.
    .controllerAppliesParams = true,

    .vst3ProcessorUid  = gProcessorUid,
    .vst3ControllerUid = gControllerUid,

    .auType            = G2_AU_TYPE,
    .auSubType         = G2_AU_SUBTYPE,
    .auManufacturer    = G2_AU_MANUFACTURER,
    .auVersion         = G2_AU_VERSION,
    .auBundleId        = G2_AU_BUNDLE_ID,

    .params            = gParams,
    .numParams         = G2_NUM_PARAMS,

    // notes §13
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

        .process       = g2_process,

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

// ONE VARIANT. The set exists because a binary MAY register several plug-ins - GenBridge registers
// itself as both an effect and an instrument - and G2 Alike is simply a list of length one.
static const tSynthLibPluginSet gVariants = {
    .variants = &gDescriptor,
    .count    = 1
};

const tSynthLibPluginSet * synthlib_plugin_variants(void) {
    return &gVariants;
}
