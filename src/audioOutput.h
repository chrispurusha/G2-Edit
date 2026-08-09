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

#ifndef __AUDIO_OUTPUT_H__
#define __AUDIO_OUTPUT_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

// The platform half of the sound engine: opens an output device and pulls audio from soundEngine.c
// on the real-time thread CoreAudio provides. This is the only file in the engine that knows what
// operating system it is on — soundEngine.c is plain C and stays portable.
//
// The device and the pair of channels within it are both selectable, so the engine can be sent to
// one output of a multi-channel interface and compared against the G2 coming back on another. Both
// are remembered between runs: the device by its UID rather than its index, since indices shuffle
// as interfaces come and go.
//
// sound_engine_start()/stop() own the lifetime; nothing calls start/stop here directly.

bool audio_output_start(void);
void audio_output_stop(void);

// The device's sample rate once running, or 0 if it is not.
double audio_output_sample_rate(void);

// Longest device UID handled. CoreAudio UIDs are short in practice; this is simply generous.
#define AUDIO_DEVICE_UID_MAX    (160)

// Enumeration, for building the device menu. The list is rebuilt on each count() call, so call that
// first and treat the indices as valid only until the next one.
uint32_t audio_output_device_count(void);
const char * audio_output_device_name(uint32_t index);
uint32_t audio_output_device_channels(uint32_t index);
bool audio_output_device_is_selected(uint32_t index);

// The UID of an enumerated device. A caller that will act on a choice LATER — a menu, whose items
// outlive the list they were built from — must take this at build time and act on the UID, never on
// the index. See audio_output_select_device_by_uid() below.
const char * audio_output_device_uid(uint32_t index);

// Selecting any of these restarts the audio device if it is running, so a change takes effect
// immediately, and records the choice for next time.
//
// Left and right are chosen SEPARATELY rather than as a pair. On a desk the two legs of a monitor
// path are not necessarily neighbours, and forcing 29/30 when the wiring wants 29/31 would mean
// repatching the desk to suit the software.
// BY UID, NOT BY INDEX, and that is the whole point: the list is re-enumerated on every count()
// call, so an index taken when a menu was built can name a different device by the time the item is
// clicked — a Bluetooth device appearing, an aggregate device being created, a display waking up. The
// remembered preference has always been a UID for exactly this reason (see the note at the top of
// this file); the act of selecting used to be an index anyway, which left the robust half undermined
// by the fragile one.
//
// Returns false if the UID names no current device, or if the device could not be started — a device
// can be present and still refuse to open, being in exclusive use by something else or unable to
// offer the rate asked of it. The old index-based call discarded that outcome, so a device that
// failed to start looked selected and simply made no sound.
bool audio_output_select_device_by_uid(const char * uid);
void audio_output_select_left_channel(uint32_t channel);    // 0-based: 0 is output 1
void audio_output_select_right_channel(uint32_t channel);

// The channels currently chosen, 0-based, and how many the selected device offers.
uint32_t audio_output_left_channel(void);
uint32_t audio_output_right_channel(void);
uint32_t audio_output_selected_device_channels(void);

// Buffer size, in frames. Fewer frames means a note takes effect sooner — the buffer length is the
// floor on how late a keypress can land — at the cost of waking the audio thread more often. 0 means
// leave whatever the device is already set to.
// The engine's output attenuation in dB, 0 or negative. Remembered between runs like the device and
// channel choices; the engine itself holds no preference of its own.
int32_t audio_output_level_db(void);
void audio_output_select_level_db(int32_t db);

uint32_t audio_output_buffer_frames(void);
void audio_output_select_buffer_frames(uint32_t frames);

// Reads the remembered device, channels and buffer size. Call once at startup, after prefs_init().
void audio_output_load_settings(void);

#ifdef __cplusplus
}
#endif

#endif // __AUDIO_OUTPUT_H__
