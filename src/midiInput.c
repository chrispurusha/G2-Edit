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
#include "midiInput.h"
#include "soundEngine.h"

// See midiInput.h. CoreMIDI delivers on its own high-priority thread, so the read callback below
// does no allocation and no locking — it only touches the held-note stack and the engine's atomics.

#define MIDI_MAX_HELD    (16)   // deeper than any hand; a stuck note past this is dropped, not queued

static MIDIClientRef    gClient      = 0;
static MIDIPortRef      gPort        = 0;
static _Atomic uint32_t gSourceCount = 0;

// Last-note priority for a monophonic voice. Without this, releasing the first key of a legato pair
// would silence a note whose key is still held — pressing C then D then letting go of C should leave
// D sounding, and letting go of D should then return to C if it is still down.
static uint8_t          gHeld[MIDI_MAX_HELD];
static uint8_t          gHeldCount   = 0;

static void held_remove(uint8_t note) {
    uint8_t i = 0;

    for (i = 0; i < gHeldCount; i++) {
        if (gHeld[i] == note) {
            uint8_t j = 0;

            for (j = (uint8_t)(i + 1); j < gHeldCount; j++) {
                gHeld[j - 1] = gHeld[j];
            }

            gHeldCount--;
            return;
        }
    }
}

static void note_on(uint8_t note) {
    held_remove(note);   // a repeat without its note off must not occupy two slots

    if (gHeldCount < MIDI_MAX_HELD) {
        gHeld[gHeldCount++] = note;
    }
    sound_engine_note((int32_t)note, true);
}

static void note_off(uint8_t note) {
    held_remove(note);

    if (gHeldCount > 0) {
        sound_engine_note((int32_t)gHeld[gHeldCount - 1], true);   // fall back to the newest still held
    } else {
        sound_engine_note(-1, false);
    }
}

static void all_notes_off(void) {
    gHeldCount = 0;
    sound_engine_note(-1, false);
}

// One MIDI 1.0 channel-voice message, as a Universal MIDI Packet word.
static void handle_message(uint32_t word) {
    uint8_t status = (uint8_t)((word >> 16) & 0xFF);
    uint8_t data1  = (uint8_t)((word >> 8) & 0x7F);
    uint8_t data2  = (uint8_t)(word & 0x7F);
    uint8_t kind   = (uint8_t)(status & 0xF0);

    switch (kind) {
        case 0x90:
        {
            // Note on with velocity 0 is the note off half of running status, and plenty of
            // keyboards send only that form.
            if (data2 > 0) {
                note_on(data1);
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
            }
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

// Connect every source that exists right now. Reconnecting one that is already connected is
// harmless, which is what makes this safe to re-run on a setup change.
static void connect_all_sources(void) {
    ItemCount count = MIDIGetNumberOfSources();
    ItemCount i     = 0;
    uint32_t  ok    = 0;

    for (i = 0; i < count; i++) {
        MIDIEndpointRef source = MIDIGetSource(i);

        if (source == 0) {
            continue;
        }

        if (MIDIPortConnectSource(gPort, source, NULL) == noErr) {
            ok++;
        }
    }

    atomic_store(&gSourceCount, ok);
    LOG_DEBUG("MIDI input: %u source(s) connected\n", (unsigned)ok);
}

// CoreMIDI tells us when devices come and go; without this a keyboard plugged in after the engine
// started would never be heard.
static void notify_callback(const MIDINotification * message, void * refCon) {
    (void)refCon;

    if ((message != NULL) && (message->messageID == kMIDIMsgSetupChanged)) {
        connect_all_sources();
    }
}

bool midi_input_start(void) {
    OSStatus status = noErr;

    if (gClient != 0) {
        return true;
    }
    gHeldCount = 0;

    status     = MIDIClientCreate(CFSTR("G2 Editor"), notify_callback, NULL, &gClient);

    if (status != noErr) {
        LOG_ERROR("MIDI input: MIDIClientCreate failed (%d)\n", (int)status);
        gClient = 0;
        return false;
    }
    status     = MIDIInputPortCreateWithProtocol(gClient, CFSTR("G2 Editor In"), kMIDIProtocol_1_0,
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
    connect_all_sources();
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
    gClient    = 0;
    gHeldCount = 0;
    atomic_store(&gSourceCount, 0);
}

uint32_t midi_input_source_count(void) {
    return atomic_load(&gSourceCount);
}

#ifdef __cplusplus
}
#endif
