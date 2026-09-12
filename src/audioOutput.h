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
// Notes: Docs/code-notes/audioOutput.h.md - "// notes §k" refers there.

#ifndef __AUDIO_OUTPUT_H__
#define __AUDIO_OUTPUT_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

// notes §1

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

// notes §2
bool audio_output_select_device_by_uid(const char * uid);
void audio_output_select_left_channel(uint32_t channel);    // 0-based: 0 is output 1
void audio_output_select_right_channel(uint32_t channel);

// The channels currently chosen, 0-based, and how many the selected device offers.
uint32_t audio_output_left_channel(void);
uint32_t audio_output_right_channel(void);
uint32_t audio_output_selected_device_channels(void);

// notes §3
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
