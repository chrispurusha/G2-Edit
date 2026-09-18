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

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "synthlibGlobals.h"
#include "sysIncludes.h"
#include "defs.h"                   // TARGET_FRAME_BUFF_WIDTH / _HEIGHT - the locked canvas ratio

#include "synthlibPlugin.h"

#include "globalVars.h"             // the document, and the names that are macros onto it
#include "dataBase.h"               // init_patch(), slot_has_modules()
#include "soundEngine.h"
#include "noteStack.h"
#include "prefs.h"
#include "splitView.h"              // SPLIT_POS_MAX - the divider the record carries
#include "g2Patch.h"
#include "patchWrite.h"             // the instance as a .prf2 image, for the host's project
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
    // NOT SAVED, like the bend below: a project reopens with the wheel and pedals at rest, as the
    // hardware powers up, not wherever they were left - a wheel left up opens the filter it morphs.
    { 0, "Morph 1", "Morph 1", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_MOD_WHEEL, SYNTHLIB_PARAM_NO_SAVE },
    { 1, "Morph 2", "Morph 2", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE, SYNTHLIB_PARAM_NO_SAVE },
    { 2, "Morph 3", "Morph 3", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE, SYNTHLIB_PARAM_NO_SAVE },
    { 3, "Morph 4", "Morph 4", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_AFTERTOUCH, SYNTHLIB_PARAM_NO_SAVE },
    { 4, "Morph 5", "Morph 5", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_SUSTAIN, SYNTHLIB_PARAM_NO_SAVE },
    { 5, "Morph 6", "Morph 6", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_FOOT, SYNTHLIB_PARAM_NO_SAVE },
    { 6, "Morph 7", "Morph 7", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE, SYNTHLIB_PARAM_NO_SAVE },
    { 7, "Morph 8", "Morph 8", eSynthLibUnitPercent, 0.0, 100.0, 0.0, 0, SYNTHLIB_MIDI_NONE, SYNTHLIB_PARAM_NO_SAVE },

    { G2_PARAM_LEVEL, "Output Level", "Level", eSynthLibUnitDecibels,
      G2_LEVEL_MIN_DB, 0.0, 1.0, 0, SYNTHLIB_MIDI_NONE },

    // PITCH BEND HAS TO BE A REAL, DECLARED PARAMETER even though nobody would choose to automate
    // it: a VST3 host converts the wheel into a parameter change and can only name a parameter that
    // exists. 0.5 is centre, which is why the default is normalized and not plain.
    { G2_PARAM_BEND, "Pitch Bend", "Bend", eSynthLibUnitGeneric,
      -1.0, 1.0, 0.5, 0, SYNTHLIB_MIDI_PITCH_BEND, SYNTHLIB_PARAM_NO_SAVE }
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
    uint8_t  velocity;   // 0-127: the note-on velocity, or the release velocity
    bool     on;
} tG2NoteEvent;

typedef struct {
    // This instance's G2: its four slots, its settings, and through engineIndex its engine.
    tG2Document *    doc;

    bool             active;
    double           sampleRate;
    double           params[G2_NUM_PARAMS];

    // See the note below.
    atomic_bool      morphSnapshotDirty;

    // notes §14 - the thread that acts on that flag, so the rebuild is not done on the audio thread
    pthread_t        rebuildThread;
    bool             rebuildRunning;
    atomic_bool      rebuildStop;

    // This block's notes, in arrival order - which both wrappers make offset order. Audio thread only.
    tG2NoteEvent     events[G2_MAX_EVENTS];
    uint32_t         eventCount;

    // sound_engine_render() writes INTERLEAVED frames and both plug-in formats hand over one buffer
    // per channel, so it renders here and is de-interleaved out. Bounded by G2_MAX_BLOCK and looped,
    // so an unusually large host buffer cannot overrun it.
    float            scratch[G2_MAX_BLOCK * 2];

    // The state record, built when a host asks its size and handed over by the call that follows -
    // so the two agree, and the performance is written once per save, not twice.
    uint8_t *        stateRecord;
    size_t           stateRecordLen;
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

// notes §14 - how often the worker looks at morphSnapshotDirty. A morph reaches the sound within
// this, which is already better than the standalone editor's frame rate.
#define G2_REBUILD_POLL_US    (4000)

static void * rebuild_worker(void * arg) {
    tG2Plugin * g2 = (tG2Plugin *)arg;

    // This thread's engine, exactly as every host entry point does it (enter()).
    g2_document_select(g2->doc);
    pthread_setname_np("G2 Alike rebuild");

    while (atomic_load(&g2->rebuildStop) == false) {
        if (atomic_exchange(&g2->morphSnapshotDirty, false) == true) {
            sound_engine_update_from_patch();
        }
        usleep(G2_REBUILD_POLL_US);
    }

    return NULL;
}

static void rebuild_worker_start(tG2Plugin * g2) {
    if (g2->rebuildRunning == true) {
        return;
    }
    atomic_store(&g2->rebuildStop, false);

    if (pthread_create(&g2->rebuildThread, NULL, rebuild_worker, g2) == 0) {
        g2->rebuildRunning = true;
    }
}

// Joined rather than detached: the worker holds this instance's document and engine, and both are
// freed the moment the host is finished with it.
static void rebuild_worker_stop(tG2Plugin * g2) {
    if (g2->rebuildRunning == false) {
        return;
    }
    atomic_store(&g2->rebuildStop, true);
    pthread_join(g2->rebuildThread, NULL);
    g2->rebuildRunning = false;
}

// ------------------------------------------------------------------------------------------------

static double level_db(double normalized) {
    return G2_LEVEL_MIN_DB + (normalized * (0.0 - G2_LEVEL_MIN_DB));
}

// NOTHING LOADS A PATCH BY ITSELF ANY MORE (2026-09-16, CT). default_patch_path() and load_patch()
// went with the decision: no host-restored path, no $G2_PLUGIN_PATCH, no ~/Documents/G2-Edit/
// plugin.pch2. An instance comes up on the empty patch g2_create() makes, and a patch arrives only
// through File > Open Patch File... - until the host holds the patch DATA, which is the next piece
// of work. See g2Plugin.c.md §5-§6 for what was here and why it went.

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
    // shows an empty patch rather than zeroed storage. Nothing replaces slot A any more: no patch is
    // loaded until the File menu opens one (2026-09-16).
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

    rebuild_worker_stop(g2);
    sound_engine_stop_hosted();
    sound_engine_detach();
    g2_document_select(NULL);
    g2_document_destroy(g2->doc);
    free(g2->stateRecord);
    free(g2);
}

static void g2_initialize(void * inst) {
    // Nothing to load: g2_create() has already made every slot an empty patch, and a file arrives
    // only through the File menu. The call still has to happen - every entry point selects this
    // instance's document before touching anything.
    (void)enter(inst);
}

static void g2_terminate(void * inst) {
    tG2Plugin * g2 = enter(inst);

    rebuild_worker_stop(g2);
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
        rebuild_worker_start(g2);
    } else {
        rebuild_worker_stop(g2);
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
        note_stack_note_on(e->note, e->velocity);
    } else {
        note_stack_note_off(e->note, e->velocity);
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

    // notes §14 - a moved morph is folded into the snapshot by rebuild_worker(), not here: the
    // rebuild costs milliseconds and takes a mutex the editor's thread also holds.

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
// A host's 0-1 velocity as MIDI's 0-127. A note-on never goes below 1, which MIDI would read as an off.
static uint8_t midi_velocity(float velocity, bool on) {
    long v = lround((double)velocity * 127.0);

    v = (v > 127) ? 127 : ((v < 0) ? 0 : v);
    return (uint8_t)(((on == true) && (v < 1)) ? 1 : v);
}

static void queue_note(tG2Plugin * g2, uint8_t note, float velocity, bool on, uint32_t sampleOffset) {
    tG2NoteEvent e = {sampleOffset, note, midi_velocity(velocity, on), on};

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
    queue_note(g2, note, velocity, true, sampleOffset);
}

static void g2_note_off(void * inst, uint8_t channel, uint8_t note, float velocity,
                        uint32_t sampleOffset) {
    tG2Plugin * g2 = enter(inst);

    (void)channel;
    queue_note(g2, note, velocity, false, sampleOffset);
}

static void g2_poly_pressure(void * inst, uint8_t channel, uint8_t note, float pressure,
                             uint32_t sampleOffset) {
    tG2Plugin * g2 = enter(inst);

    (void)channel;
    (void)sampleOffset;

    // As in the application, only a note actually SOUNDING may move the morph; in Mono a key still
    // held underneath would otherwise fight the one being played.
    if (sound_engine_note_sounding((int32_t)note) == false) {
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

// The key the patch data follows: "data=<bytes>\n", then that many bytes of .prf2 image, then nothing.
#define G2_STATE_DATA_KEY    "data="

static void build_state_record(tG2Plugin * g2) {
    // Settings as text, as before; no file names (notes §10). The instance itself follows as a
    // performance image - all four slots, their dividers, the performance settings and global knobs.
    char      text[256 + CLAVIA_NAME_SIZE];
    size_t    used      = 0;
    size_t    imageLen  = 0;
    uint8_t * image     = NULL;

    used += (size_t)snprintf(text + used, sizeof(text) - used, "%s", G2_STATE_HEADER);
    used += (size_t)snprintf(text + used, sizeof(text) - used, "perfmode=%u\nselected=%u\n",
                             (unsigned)gGlobalSettings.perfMode, (unsigned)gSlot);
    // notes §10 - the editor's mouse mode and the engine's drone mode too.
    used += (size_t)snprintf(text + used, sizeof(text) - used, "dialmode=%d\ndrone=%d\n",
                             (int)synthlib_dial_mode(), (sound_engine_drone_mode() == true) ? 1 : 0);
    // notes §10 - the performance's name, which its image does not carry
    used += (size_t)snprintf(text + used, sizeof(text) - used, "perfname=%s\n", gGlobalSettings.perfName);

    database_read_lock();
    image = write_perf_to_memory(&imageLen);
    database_read_unlock();

    if (image != NULL) {
        used += (size_t)snprintf(text + used, sizeof(text) - used, "%s%zu\n", G2_STATE_DATA_KEY, imageLen);
    }

    if (used > sizeof(text)) {
        used = sizeof(text);
    }
    free(g2->stateRecord);
    g2->stateRecordLen = 0;
    g2->stateRecord    = (uint8_t *)malloc(used + imageLen);

    if (g2->stateRecord != NULL) {
        memcpy(g2->stateRecord, text, used);

        if (image != NULL) {
            memcpy(g2->stateRecord + used, image, imageLen);
        }
        g2->stateRecordLen = used + imageLen;
    }
    free(image);
}

static size_t g2_get_state(void * inst, void * out, size_t len) {
    tG2Plugin * g2 = enter(inst);

    // The size question builds the record; the write that follows hands over that same one.
    if ((out == NULL) || (g2->stateRecord == NULL)) {
        build_state_record(g2);
    }

    if (out == NULL) {
        return g2->stateRecordLen;
    }
    size_t      take = (len < g2->stateRecordLen) ? len : g2->stateRecordLen;

    if (g2->stateRecord != NULL) {
        memcpy(out, g2->stateRecord, take);
    }
    free(g2->stateRecord);
    g2->stateRecord = NULL;
    return take;
}

// A v2 state record, read into this before anything is loaded, so a record that names only some
// slots can empty the rest (see g2_set_state()).
typedef struct {
    // No file names. A record written before 2026-09-16 still carries perf= and slot0..3=, and they
    // are skipped now like any other key this build does not know - which is exactly the intent:
    // reopening a project must not reload the file it was last pointed at.
    int32_t perfMode;
    int32_t selected;
    int32_t dialMode;
    int32_t drone;

    // Per slot: each slot holds its own patch and so its own divider. -1 is "the record did not say".
    // Written 2026-09-16 only; a record with patch data carries the dividers in that instead.
    int32_t split[MAX_SLOTS];

    bool    havePerfName;
    char    perfName[CLAVIA_NAME_SIZE + 1];
} tG2State;

static void parse_state_line(tG2State * state, char * line) {
    char * eq = strchr(line, '=');

    if (eq == NULL) {
        return;     // Not ours; a later version's line, perhaps. Skipped rather than refused
    }
    *eq = '\0';

    const char * key   = line;
    const char * value = eq + 1;

    if (strcmp(key, "perfmode") == 0) {
        state->perfMode = atoi(value);
    } else if (strcmp(key, "selected") == 0) {
        state->selected = atoi(value);
    } else if (strcmp(key, "dialmode") == 0) {
        state->dialMode = atoi(value);
    } else if (strcmp(key, "drone") == 0) {
        state->drone = atoi(value);
    } else if (strcmp(key, "perfname") == 0) {
        state->havePerfName = true;
        snprintf(state->perfName, sizeof(state->perfName), "%s", value);
    } else if (strcmp(key, "split") == 0) {
        // One position per slot, comma separated. A short list leaves the rest at -1, so a record
        // written by a build that knew fewer slots still says what it knew.
        const char * p = value;

        for (uint32_t slot = 0; (slot < MAX_SLOTS) && (p != NULL) && (*p != '\0'); slot++) {
            state->split[slot] = atoi(p);
            p                  = strchr(p, ',');

            if (p != NULL) {
                p++;
            }
        }
    }
}

static void g2_set_state(void * inst, const void * data, size_t len) {
    tG2Plugin * g2        = enter(inst);
    size_t      headerLen = strlen(G2_STATE_HEADER);

    if ((data == NULL) || (len == 0u)) {
        return;
    }

    if ((len < headerLen) || (memcmp(data, G2_STATE_HEADER, headerLen) != 0)) {
        // The oldest format was a bare path, and this build does not open one: nothing loads a patch
        // by itself any more. Such a record holds nothing else, so it restores nothing and the
        // instance keeps the empty patch g2_create() made.
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
    state->dialMode = -1;
    state->drone    = -1;

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        state->split[slot] = -1;
    }
    memcpy(text, data, len);
    text[len] = '\0';

    // The patch data, if the record has any: everything after the data= line, which is the last
    const uint8_t * image    = NULL;
    size_t          imageLen = 0;
    char *          dataKey  = strstr(text + headerLen, "\n" G2_STATE_DATA_KEY);

    if (dataKey != NULL) {
        char * lineEnd = strchr(dataKey + 1, '\n');

        if (lineEnd != NULL) {
            size_t declared = (size_t)strtoul(dataKey + 1 + strlen(G2_STATE_DATA_KEY), NULL, 10);
            size_t at       = (size_t)(lineEnd + 1 - text);

            if ((at <= len) && (declared <= (len - at))) {
                image    = (const uint8_t *)data + at;
                imageLen = declared;
            }
            dataKey[1] = '\0';    // the settings text ends at the data= line
        }
    }

    for (char * line = strtok_r(text + headerLen, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        parse_state_line(state, line);
    }

    // notes §11
    //
    // EVERY SLOT BACK TO AN EMPTY PATCH. The record names no file and nothing here opens one, so a
    // reopened project starts empty and waits for File > Open - or, once it exists, for the patch
    // data the host will hold. The remembered paths are cleared with them, so that File > Save
    // cannot point at a file whose contents were never loaded.
    gSavedPerfPath[0] = '\0';

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        gSavedPatchPath[slot][0] = '\0';
        clear_slot_data(slot);
        init_patch(slot);
    }

    // notes §11 - and then the patches the project saved, if it did
    bool restored = (image != NULL) && g2_plugin_parse_perf_image(image, (int64_t)imageLen);

    free(text);

    if (state->havePerfName == true) {
        snprintf(gGlobalSettings.perfName, sizeof(gGlobalSettings.perfName), "%s", state->perfName);
    }

    // AFTER the slots, which is what the record is for: it says what these were when the project was
    // saved, and the empty patches above carry the defaults.
    if ((state->perfMode == 0) || (state->perfMode == 1)) {
        gGlobalSettings.perfMode = (uint8_t)state->perfMode;
    }

    if ((state->selected >= 0) && (state->selected < MAX_SLOTS)) {
        gSlot = (uint32_t)state->selected;
    }

    if ((state->dialMode >= (int32_t)eDialModeRotary) && (state->dialMode <= (int32_t)eDialModeHorizontal)) {
        synthlib_set_dial_mode((tDialMode)state->dialMode);
    }

    // LAST, and the order still matters even with no file in it: init_patch() above sets
    // barPosition to SPLIT_POS_MAX (Voice Area full), so a divider applied any earlier would be
    // overwritten by the empty patch a moment later.
    for (uint32_t slot = 0; (slot < MAX_SLOTS) && (restored == false); slot++) {
        if (state->split[slot] >= 0) {
            gPatchDescr[slot].barPosition = (uint16_t)((state->split[slot] > SPLIT_POS_MAX)
                                                       ? SPLIT_POS_MAX : state->split[slot]);
        }
    }
    sound_engine_set_drone_mode(state->drone != 0);     // notes §10 - absent (-1) is the default, on
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
