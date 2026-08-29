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

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include <CoreMIDI/CoreMIDI.h>

#pragma clang diagnostic pop

#include "defs.h"
#include "synthlibDefs.h"
#include "globalVars.h"
#include "msgQueue.h"
#include "prefs.h"
#include "midiInput.h"
#include "synthlibGlobals.h"
#include "soundEngine.h"
#include "noteStack.h"

// See midiInput.h. CoreMIDI delivers on its own high-priority thread, so the read callback below
// does no allocation and no locking — it only touches the held-note stack and the engine's atomics.


// Which morph group each controller drives. The G2 hard-wires these — morphStrMap in
// moduleResources.h lists them in order as Wheel, Vel, Keyb, Aft.Tch, Sust.Pd, Ctrl.Pd, P.Stick,
// G.Wh — so a patch that morphs its filter frequency from the wheel is morphing group 0.
#define MORPH_GROUP_WHEEL         (0)
#define MORPH_GROUP_AFTERTOUCH    (3)
#define MORPH_GROUP_SUSTAIN       (4)
#define MORPH_GROUP_CTRL_PEDAL    (5)

#define MIDI_CC_MOD_WHEEL         (1)
#define MIDI_CC_CTRL_PEDAL        (4)
#define MIDI_CC_SUSTAIN           (64) // deeper than any hand; a stuck note past this is dropped, not queued

#define MAX_MIDI_SOURCES          (32)
#define SOURCE_NAME_SIZE          (96)

#define PREF_KEY_SOURCE           "midiInputSourceId"
#define PREF_KEY_CHANNEL          "midiInputChannel"
#define PREF_KEY_TO_SYNTH         "midiInputSendsToSynth"

typedef struct {
    MIDIEndpointRef endpoint;
    SInt32          uniqueId;
    char            name[SOURCE_NAME_SIZE];
} tMidiSource;

static MIDIClientRef    gClient          = 0;
static MIDIPortRef      gPort            = 0;
static _Atomic uint32_t gSourceCount     = 0;

static tMidiSource      gSource[MAX_MIDI_SOURCES];
static uint32_t         gSourceListCount = 0;

// The chosen source is remembered by CoreMIDI unique ID, not by position: the list reorders as
// devices come and go, and two identical interfaces share a name.
static SInt32           gSelectedId      = 0;    // 0 means nothing chosen yet
static bool             gEnabled         = true;
static _Atomic uint32_t gChannel         = MIDI_CHANNEL_OMNI;
static _Atomic bool     gSendsToSynth    = true;

// How many pressure messages have arrived, of either kind. This exists because the obvious way to
// answer "is the keyboard sending aftertouch at all" — logging each message — is exactly what must
// not happen here: LOG_DEBUG writes to stdout AND to the USB log file, and doing that per message on
// CoreMIDI's callback thread stalls MIDI input outright. An atomic counter costs nothing and the UI
// thread can read it whenever it likes.
static _Atomic uint32_t gPressureCount   = 0;

// The most recent controller number seen, for MIDI Learn (the L key). -1 until one arrives.
static _Atomic int32_t  gLastCC          = -1;

// The same message the Virtual Keyboard posts — see send_note() in virtualKeyboard.c. Posted from
// the CoreMIDI thread, which is safe: msg_send() takes a mutex and allocates, which would be wrong
// on the audio thread but is fine on this one.
static void send_note_to_synth(uint8_t note, uint8_t velocity, bool on) {
    tMessageContent msg = {0};

    if ((atomic_load(&gSendsToSynth) == false) || (!device_ready())) {
        return;
    }

    // WITH THE LOCAL ENGINE SOUNDING, THE G2 DOES NOT ALSO GET THE NOTE. Otherwise one key press
    // plays twice — once here and once on the instrument — which is exactly the comparison the
    // engine exists to make, ruined by doing both at once.
    //
    // NOTE-OFFS ALWAYS GO THROUGH. Enabling the engine while a key is held would otherwise swallow
    // the release and leave the G2 droning with no way to stop it short of a panic. A release sent
    // for a note the instrument is not playing is harmless, so this is the safe asymmetry rather
    // than a tidy one.
    if ((on == true) && (sound_engine_active() == true)) {
        return;
    }
    msg.cmd                   = eMsgCmdPlayNote;
    msg.playNoteData.note     = note;
    msg.playNoteData.velocity = velocity;
    msg.playNoteData.on       = on;
    msg_send(&gToUsbThread, &msg);
}

static void note_on(uint8_t note, uint8_t velocity) {
    note_stack_note_on(note);

    // The G2 gets the note as played. The last-note stack is for the local engine, which is
    // monophonic; the G2 does its own voice allocation and wants every note.
    send_note_to_synth(note, velocity, true);
}

static void note_off(uint8_t note) {
    note_stack_note_off(note);       // falls back to the newest note still held — see noteStack.h
    send_note_to_synth(note, 0, false);
}

static void all_notes_off(void) {
    uint8_t i = 0;

    // Release everything that was held on the G2 too, or a panic here would leave it droning. Walk
    // the stack BEFORE clearing it.
    for (i = 0; i < note_stack_count(); i++) {
        send_note_to_synth(note_stack_at(i), 0, false);
    }

    note_stack_all_off();
}

// A morph position is not read by the audio thread the way pitch bend is — it is folded into the
// parameter snapshot, and that snapshot is only rebuilt during a redraw. An idle window sits in
// glfwWaitEvents(), and turning a wheel produces no window event, so without this a morph would not
// be heard until something unrelated happened to wake the render loop. Safe from the MIDI thread.
//
// The redraw is wanted in its own right too: morphed dials move on screen as the wheel turns.
static void morph_moved(bool changed) {
    if (changed == true) {
        // FOLD IT IN HERE, not on the next redraw. sound_engine_set_morph() only records the
        // position; the audio thread reads a parameter SNAPSHOT, and until that is rebuilt the wheel
        // has moved nothing it can hear. This used to be left to the redraw below, which capped mod
        // wheel and aftertouch response at the frame rate and put a full canvas repaint between the
        // control and the sound — the wheel felt laggy, and a fast sweep arrived as steps.
        //
        // Safe from this thread since the snapshot's writers were given a mutex of their own
        // (gParamsWriteMutex in soundEngine.c); the audio thread is a lock-free reader and is not
        // held up by it.
        sound_engine_update_from_patch();

        // Still wanted in its own right: morphed dials move on screen as the wheel turns. It is now
        // only cosmetic, though, so a late frame no longer means a late sound.
        synthlib_request_redraw();
    }
}

// One MIDI 1.0 channel-voice message, as a Universal MIDI Packet word.
static void handle_message(uint32_t word) {
    uint8_t  status = (uint8_t)((word >> 16) & 0xFF);
    uint8_t  data1  = (uint8_t)((word >> 8) & 0x7F);
    uint8_t  data2  = (uint8_t)(word & 0x7F);
    uint8_t  kind   = (uint8_t)(status & 0xF0);
    uint32_t wanted = atomic_load(&gChannel);

    // Omni takes everything; otherwise only the chosen channel. Filtering here rather than per
    // message type means a controller chattering on another channel cannot move a morph either.
    //
    // An MPE controller needs Omni. MPE gives every note its own member channel and sends that
    // note's pressure and bend on the same channel, so picking a single channel throws away most of
    // the keyboard — and, because the notes on the surviving channel still play, it fails by
    // dropping expression rather than by going silent, which looks like the pressure not working.
    if ((wanted != MIDI_CHANNEL_OMNI) && (((uint32_t)(status & 0x0F) + 1) != wanted)) {
        return;
    }

    switch (kind) {
        case 0x90:
        {
            // Note on with velocity 0 is the note off half of running status, and plenty of
            // keyboards send only that form.
            if (data2 > 0) {
                note_on(data1, data2);
            } else {
                note_off(data1);
            }
            break;
        }
        case 0x80:
        {
            note_off(data1);
            break;
        }
        case 0xB0:
        {
            // 123 All Notes Off, 120 All Sound Off. Panic buttons; both silence the voice.
            if ((data1 == 123) || (data1 == 120)) {
                all_notes_off();
                break;    // channel-mode messages, not controllers anything can be learned from
            }
            // Recorded before the morph handling below, because MIDI Learn wants whatever arrived
            // last whether or not this editor does anything else with it.
            atomic_store(&gLastCC, (int32_t)data1);

            if (data1 == MIDI_CC_MOD_WHEEL) {
                morph_moved(sound_engine_set_morph(MORPH_GROUP_WHEEL, (double)data2 / 127.0));
            } else if (data1 == MIDI_CC_CTRL_PEDAL) {
                morph_moved(sound_engine_set_morph(MORPH_GROUP_CTRL_PEDAL, (double)data2 / 127.0));
            } else if (data1 == MIDI_CC_SUSTAIN) {
                morph_moved(sound_engine_set_morph(MORPH_GROUP_SUSTAIN, (double)data2 / 127.0));
            }
            break;
        }
        case 0xE0:
        {
            // Pitch bend is 14 bits split across the two data bytes, low 7 first, centred on 8192.
            // Sent as -1..+1; how many semitones that is comes from the patch's own Bend range.
            int32_t raw = (int32_t)(((uint32_t)data2 << 7) | (uint32_t)data1) - 8192;

            sound_engine_pitch_bend((double)raw / 8192.0);
            break;
        }
        case 0xA0:
        {
            // Polyphonic key pressure, which carries the note in the FIRST data byte and the
            // pressure in the second — the opposite way round from channel pressure below.
            //
            // Plenty of keyboards send this instead of channel pressure, and an MPE controller may
            // send either. The engine has a single voice, so only the note actually sounding is
            // allowed to move the morph; without that test a key still held underneath would fight
            // the one being played.
            if ((int32_t)data1 == note_stack_top()) {
                morph_moved(sound_engine_set_morph(MORPH_GROUP_AFTERTOUCH, (double)data2 / 127.0));
            }
            atomic_fetch_add(&gPressureCount, 1);
            break;
        }
        case 0xD0:
        {
            // Channel pressure. Its value is in the first data byte, not the second.
            morph_moved(sound_engine_set_morph(MORPH_GROUP_AFTERTOUCH, (double)data1 / 127.0));
            atomic_fetch_add(&gPressureCount, 1);
            break;
        }
        default:
        {
            break;
        }
    }
}

// CoreMIDI thread.
static void read_callback(const MIDIEventList * eventList, void * srcConnRefCon) {
    const MIDIEventPacket * packet = NULL;
    UInt32                  i      = 0;

    (void)srcConnRefCon;

    if (eventList == NULL) {
        return;
    }
    packet = &eventList->packet[0];

    for (i = 0; i < eventList->numPackets; i++) {
        UInt32 w = 0;

        for (w = 0; w < packet->wordCount; w++) {
            uint32_t word = (uint32_t)packet->words[w];

            // Message type 2 is a MIDI 1.0 channel voice message, which is always one word. Other
            // types (system, SysEx, MIDI 2.0 voice) are not what a note keyboard sends, so skipping
            // them also skips their extra words correctly.
            if (((word >> 28) & 0xF) == 0x2) {
                handle_message(word);
            }
        }

        packet = MIDIEventPacketNext(packet);
    }
}

// Rebuilds the list of sources on the system, with the name CoreMIDI shows and the unique ID the
// selection is remembered by.
static void rebuild_source_list(void) {
    ItemCount count = MIDIGetNumberOfSources();
    ItemCount i     = 0;

    gSourceListCount = 0;

    for (i = 0; (i < count) && (gSourceListCount < MAX_MIDI_SOURCES); i++) {
        MIDIEndpointRef endpoint = MIDIGetSource(i);
        CFStringRef     name     = NULL;
        SInt32          uniqueId = 0;

        if (endpoint == 0) {
            continue;
        }
        gSource[gSourceListCount].endpoint = endpoint;
        MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &uniqueId);
        gSource[gSourceListCount].uniqueId = uniqueId;
        gSource[gSourceListCount].name[0]  = '\0';

        if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) == noErr) {
            if (name != NULL) {
                CFStringGetCString(name, gSource[gSourceListCount].name,
                                   (CFIndex)SOURCE_NAME_SIZE, kCFStringEncodingUTF8);
                CFRelease(name);
            }
        }
        gSourceListCount++;
    }
}

// Connects the chosen source and nothing else. Called on startup, when the selection changes, and
// whenever CoreMIDI reports the setup has changed — so plugging the chosen keyboard in later picks
// it up without a restart.
static void connect_selected_source(void) {
    uint32_t i  = 0;
    uint32_t ok = 0;

    if (gPort == 0) {
        return;
    }

    // Drop everything first, or changing the selection would leave the old one still feeding in.
    for (i = 0; i < gSourceListCount; i++) {
        (void)MIDIPortDisconnectSource(gPort, gSource[i].endpoint);
    }

    rebuild_source_list();

    if (gEnabled == true) {
        for (i = 0; i < gSourceListCount; i++) {
            if ((gSelectedId != 0) && (gSource[i].uniqueId != gSelectedId)) {
                continue;
            }

            if (MIDIPortConnectSource(gPort, gSource[i].endpoint, NULL) == noErr) {
                ok++;
            }

            // A specific choice connects exactly one. With nothing chosen yet, everything is taken,
            // which is the useful default for a single keyboard on a desk.
            if (gSelectedId != 0) {
                break;
            }
        }
    }
    atomic_store(&gSourceCount, ok);
    LOG_DEBUG("MIDI input: %u source(s) connected%s\n", (unsigned)ok,
              (gEnabled == false) ? " (input disabled)" : "");
}

uint32_t midi_input_source_count(void) {
    rebuild_source_list();
    return gSourceListCount;
}

const char * midi_input_source_name(uint32_t index) {
    return (index < gSourceListCount) ? gSource[index].name : "";
}

// ONE SPECIFIC SOURCE, not "is connected". With every source taken, this used to tick all of them,
// which read as though each had been chosen individually; the All entry carries that now.
bool midi_input_source_is_selected(uint32_t index) {
    if ((gEnabled == false) || (gSelectedId == 0) || (index >= gSourceListCount)) {
        return false;
    }
    return gSource[index].uniqueId == gSelectedId;
}

bool midi_input_all_sources_selected(void) {
    return (gEnabled == true) && (gSelectedId == 0);
}

bool midi_input_is_enabled(void) {
    return gEnabled;
}

void midi_input_select_source(int32_t index) {
    if (index == MIDI_INPUT_NONE) {
        gEnabled = false;
        prefs_set_int(PREF_KEY_SOURCE, 0);
    } else if (index == MIDI_INPUT_ALL) {
        // -1 in prefs is "enabled, no specific source", which is what connect_selected_source() reads
        // as take-everything and what an unset preference already meant. 0 is taken: it means None.
        gEnabled    = true;
        gSelectedId = 0;
        prefs_set_int(PREF_KEY_SOURCE, -1);
    } else if ((uint32_t)index < gSourceListCount) {
        gEnabled    = true;
        gSelectedId = gSource[index].uniqueId;
        prefs_set_int(PREF_KEY_SOURCE, (long)gSelectedId);
    } else {
        return;
    }
    connect_selected_source();
}

uint32_t midi_input_channel(void) {
    return atomic_load(&gChannel);
}

void midi_input_select_channel(uint32_t channel) {
    atomic_store(&gChannel, (channel > 16) ? MIDI_CHANNEL_OMNI : channel);
    prefs_set_int(PREF_KEY_CHANNEL, (long)atomic_load(&gChannel));
}

bool midi_input_sends_to_synth(void) {
    return atomic_load(&gSendsToSynth);
}

void midi_input_set_sends_to_synth(bool enable) {
    atomic_store(&gSendsToSynth, enable);
    prefs_set_int(PREF_KEY_TO_SYNTH, enable ? 1 : 0);
}

int32_t midi_input_last_cc(void) {
    return atomic_load(&gLastCC);
}

uint32_t midi_input_pressure_count(void) {
    return atomic_load(&gPressureCount);
}

void midi_input_load_settings(void) {
    long stored = prefs_get_int(PREF_KEY_SOURCE, -1);

    if (stored == 0) {
        gEnabled = false;            // explicitly set to None last time
    } else if (stored > 0) {
        gEnabled    = true;
        gSelectedId = (SInt32)stored;
    } else {
        // -1, whether stored by the All entry or simply never set: enabled, no single source, so
        // connect_selected_source() takes everything. Spelled out rather than left to the initialisers
        // now that it is a choice a user can make and not only a default they inherit.
        gEnabled    = true;
        gSelectedId = 0;
    }
    atomic_store(&gChannel, (uint32_t)prefs_get_int(PREF_KEY_CHANNEL, MIDI_CHANNEL_OMNI));
    atomic_store(&gSendsToSynth, prefs_get_int(PREF_KEY_TO_SYNTH, 1) != 0);
}

// CoreMIDI tells us when devices come and go; without this a keyboard plugged in after the engine
// started would never be heard.
static void notify_callback(const MIDINotification * message, void * refCon) {
    (void)refCon;

    if ((message != NULL) && (message->messageID == kMIDIMsgSetupChanged)) {
        connect_selected_source();
    }
}

bool midi_input_start(void) {
    OSStatus status = noErr;

    if (gClient != 0) {
        return true;
    }
    note_stack_all_off();

    status = MIDIClientCreate(CFSTR("G2 Editor"), notify_callback, NULL, &gClient);

    if (status != noErr) {
        LOG_ERROR("MIDI input: MIDIClientCreate failed (%d)\n", (int)status);
        gClient = 0;
        return false;
    }
    status = MIDIInputPortCreateWithProtocol(gClient, CFSTR("G2 Editor In"), kMIDIProtocol_1_0,
                                             &gPort, ^ (const MIDIEventList * evtlist, void * srcConnRefCon) {
        read_callback(evtlist, srcConnRefCon);
    });

    if (status != noErr) {
        LOG_ERROR("MIDI input: MIDIInputPortCreateWithProtocol failed (%d)\n", (int)status);
        MIDIClientDispose(gClient);
        gClient = 0;
        gPort   = 0;
        return false;
    }
    connect_selected_source();
    return true;
}

void midi_input_stop(void) {
    if (gClient == 0) {
        return;
    }

    if (gPort != 0) {
        MIDIPortDispose(gPort);
        gPort = 0;
    }
    MIDIClientDispose(gClient);
    gClient = 0;
    note_stack_all_off();
    atomic_store(&gSourceCount, 0);
}

// How many sources are actually feeding in right now — 0 when input is set to None, or when the
// chosen device is not plugged in. Distinct from midi_input_source_count(), which is how many exist
// to choose from.
uint32_t midi_input_connected_count(void) {
    return atomic_load(&gSourceCount);
}

#ifdef __cplusplus
}
#endif
