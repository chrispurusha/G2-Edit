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
// Notes: Docs/code-notes/soundEngine.h.md - "// notes §k" refers there.

#ifndef __SOUND_ENGINE_H__
#define __SOUND_ENGINE_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "types.h"

// How many engines a process can hold. One in the application; in the plug-in each instance takes
// one, and a performance will one day take four. Unused banks are zero-fill and cost no memory.
#ifdef SYNTHLIB_PLUGIN_BUILD
#define SOUND_ENGINE_MAX_ENGINES    (32)
#else
#define SOUND_ENGINE_MAX_ENGINES    (1)
#endif

// notes §1

// Whether the engine is running and holding the audio device.
bool sound_engine_active(void);

// One line saying what the engine is doing, or why it is silent — "Playing OscB", "Select an OscB
// to play", and so on. The Experimental menu shows this under the toggle, because an engine that is
// on but silent otherwise gives no clue which of the several reasons applies. UI thread only.
const char * sound_engine_status_text(void);

// Why there is no vibrato, in one line: whether the keyboard is sending pressure at all, where that
// has left the morph, and whether the patch actually put an LFO into the graph. Those three failures
// all present as silence but need different fixes. UI thread only.
const char * sound_engine_modulation_text(void);

// Opens the audio device and starts rendering. False if the device would not start, in which case
// nothing is held and the engine stays inactive.
bool sound_engine_start(void);
void sound_engine_stop(void);

// Plug-in host entry points: same preparation as sound_engine_start()/stop(), but the caller owns
// the audio device and drives sound_engine_render() itself. audioOutput.c is not involved.
void sound_engine_start_hosted(double sampleRate);
void sound_engine_stop_hosted(void);

// notes §2
void sound_engine_render_reverb_ir(double deviceRate, uint32_t type, uint32_t timeValue, uint32_t brightValue, float * out, uint32_t frames);

// notes §3
bool sound_engine_meters_dirty(void);

bool sound_engine_module_meter(uint32_t location, uint32_t moduleIndex, uint32_t leg, uint32_t * value);

// The same for a module's LED. Only LFOs publish one today and only index 0; everything else falls
// back to the value the instrument sent.
bool sound_engine_module_led(uint32_t location, uint32_t moduleIndex, uint32_t ledIndex, uint32_t * value);

void sound_engine_render_chorus(double deviceRate, uint32_t detuneValue, uint32_t amountValue, const float * in, float * out, uint32_t frames);

// notes §4
void sound_engine_set_output_level_db(double db);

bool sound_engine_set_morph(uint32_t group, double amount);

// Pitch bend, -1..+1 across the wheel's travel. How many semitones that is comes from the patch's
// own Bend setting, so the engine bends by the same amount the G2 would. Called from the MIDI thread.
void sound_engine_pitch_bend(double bend);

// notes §5
void sound_engine_note(int32_t note, bool on);

// Whether the engine will sound more than one note at once, i.e. the patch is Poly with a voice
// count above 1. The note stack needs this: in Mono, releasing a key falls back to the newest note
// still held, and in Poly it must not, because that note already has a voice of its own sounding it.
bool sound_engine_is_polyphonic(void);

// How many voices the current patch may sound at once, and how many are audible right now. For the
// status line — the second figure is what tells you whether a chord is being cut short.
uint32_t sound_engine_voice_count(void);
uint32_t sound_engine_voices_sounding(void);

// Worst render load since this was last called, as a percentage of real time — reading it clears the
// peak. Approaching 100 % means the engine is running out of its deadline, which is what crackling
// is; well below it means a crackle is something else.
uint32_t sound_engine_load_percent(void);

// UI thread. Reads the current selection and publishes a parameter snapshot for the audio thread.
// Cheap enough to call on every redraw, which is what graphics.c does — every parameter change
// forces one, so nothing else needs to poll.
void sound_engine_update_from_patch(void);

// notes §6
const char * sound_engine_debug_text(void);

// Audio thread, real-time context: no locks, no allocation, no logging below this line.
// Fills frameCount frames of interleaved float, channelCount channels wide.
void sound_engine_set_sample_rate(double sampleRate);
void sound_engine_render(float * out, uint32_t frameCount, uint32_t channelCount);

// notes §7
bool sound_engine_attach(void);
void sound_engine_detach(void);

// The current document's engine index - for state kept per engine outside this file (noteStack.c).
uint32_t sound_engine_index(void);

// Binds the current engine to one slot of its document; -1 (the default) follows the selected slot.
// For performance mode, which will run one engine per slot - not built yet.
void sound_engine_bind_slot(int32_t slot);

#ifdef __cplusplus
}
#endif

#endif // __SOUND_ENGINE_H__
