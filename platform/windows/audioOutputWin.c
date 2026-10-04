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

// Windows stand-in for src/audioOutput.c (Docs/windows-port-plan.md, step 3). No device yet: the
// output never starts, but the settings are read and written exactly as on the Mac, so the menus
// work and the prefs carry over when the WASAPI version replaces this.

#ifdef __cplusplus
extern "C" {
#endif

#include "defs.h"
#include "synthlibDefs.h"
#include "prefs.h"
#include "audioOutput.h"
#include "soundEngine.h"

#define PREF_KEY_DEVICE         "audioOutputDeviceUID"
#define PREF_KEY_LEFT           "audioOutputLeftChannel"
#define PREF_KEY_RIGHT          "audioOutputRightChannel"
#define PREF_KEY_CHANNEL        "audioOutputFirstChannel"
#define PREF_KEY_BUFFER         "audioOutputBufferFrames"
#define PREF_KEY_AHEAD          "audioRenderAheadMs"
#define PREF_KEY_VOICETHREAD    "engineVoiceThread"
#define PREF_KEY_LEVEL          "audioOutputLevelDb"

static char     gSelectedUid[AUDIO_DEVICE_UID_MAX] = {0};
static uint32_t gLeftChannel                       = 0;
static uint32_t gRightChannel                      = 1;
static uint32_t gBufferFrames                      = 0;
static uint32_t gAheadMs                           = 0;
static int32_t  gLevelDb                           = 0;

bool audio_output_start(void) {
    LOG_ERROR("Sound engine: no audio output on Windows yet\n");
    return false;
}

void audio_output_stop(void) {
}

double audio_output_sample_rate(void) {
    return 0.0;
}

uint32_t audio_output_device_count(void) {
    return 0;
}

const char * audio_output_device_name(uint32_t index) {
    (void)index;
    return "";
}

uint32_t audio_output_device_channels(uint32_t index) {
    (void)index;
    return 0;
}

bool audio_output_device_is_selected(uint32_t index) {
    (void)index;
    return false;
}

const char * audio_output_device_uid(uint32_t index) {
    (void)index;
    return "";
}

bool audio_output_select_device_by_uid(const char * uid) {
    (void)uid;
    return false;   // no device list to find it in
}

void audio_output_select_left_channel(uint32_t channel) {
    gLeftChannel = channel;
    prefs_set_int(PREF_KEY_LEFT, (long)channel);
}

void audio_output_select_right_channel(uint32_t channel) {
    gRightChannel = channel;
    prefs_set_int(PREF_KEY_RIGHT, (long)channel);
}

uint32_t audio_output_left_channel(void) {
    return gLeftChannel;
}

uint32_t audio_output_right_channel(void) {
    return gRightChannel;
}

uint32_t audio_output_selected_device_channels(void) {
    return 0;
}

int32_t audio_output_level_db(void) {
    return gLevelDb;
}

void audio_output_select_dac_emulation(bool on) {
    prefs_set_int(PREF_KEY_DAC_EMULATION, on ? 1 : 0);
    sound_engine_set_dac_emulation(on);
}

void audio_output_select_level_db(int32_t db) {
    if (db > 0) {
        db = 0;
    }
    gLevelDb = db;
    prefs_set_int(PREF_KEY_LEVEL, (long)db);
    sound_engine_set_output_level_db((double)db);
}

uint32_t audio_output_buffer_frames(void) {
    return gBufferFrames;
}

void audio_output_select_buffer_frames(uint32_t frames) {
    gBufferFrames = frames;
    prefs_set_int(PREF_KEY_BUFFER, (long)frames);
}

uint32_t audio_output_render_ahead_ms(void) {
    return gAheadMs;
}

void audio_output_select_render_ahead_ms(uint32_t ms) {
    gAheadMs = ms;
    prefs_set_int(PREF_KEY_AHEAD, (long)ms);
}

uint32_t audio_output_render_ahead_underruns(void) {
    return 0;
}

bool audio_output_voice_thread(void) {
    return prefs_get_int(PREF_KEY_VOICETHREAD, 1) != 0;
}

void audio_output_select_voice_thread(bool on) {
    prefs_set_int(PREF_KEY_VOICETHREAD, on ? 1 : 0);
    sound_engine_set_split_mode(on ? eSplitThreaded : eSplitSerial);
}

bool audio_output_economy(void) {
    return prefs_get_int(PREF_KEY_ECONOMY, 0) != 0;
}

void audio_output_select_economy(bool on) {
    prefs_set_int(PREF_KEY_ECONOMY, on ? 1 : 0);
    sound_engine_set_economy(on);
}

bool audio_output_poll_rate_change(void) {
    return false;
}

uint32_t audio_output_overload_count(void) {
    return 0;
}

const char * audio_output_thread_text(void) {
    return "no audio output";
}

void audio_output_load_settings(void) {
    const char * uid = prefs_get_string(PREF_KEY_DEVICE, "");

    if (uid != NULL) {
        strncpy(gSelectedUid, uid, sizeof(gSelectedUid) - 1);
    }
    sound_engine_set_dac_emulation(prefs_get_int(PREF_KEY_DAC_EMULATION, 0) != 0);
    gLevelDb      = (int32_t)prefs_get_int(PREF_KEY_LEVEL, 0);

    if (gLevelDb > 0) {
        gLevelDb = 0;
    }
    sound_engine_set_output_level_db((double)gLevelDb);

    if (prefs_has_key(PREF_KEY_LEFT) == true) {
        gLeftChannel  = (uint32_t)prefs_get_int(PREF_KEY_LEFT, 0);
        gRightChannel = (uint32_t)prefs_get_int(PREF_KEY_RIGHT, 1);
    } else {
        uint32_t first = (uint32_t)prefs_get_int(PREF_KEY_CHANNEL, 0);

        gLeftChannel  = first;
        gRightChannel = first + 1;
    }
    gBufferFrames = (uint32_t)prefs_get_int(PREF_KEY_BUFFER, 0);
    gAheadMs      = (uint32_t)prefs_get_int(PREF_KEY_AHEAD, 0);
    sound_engine_set_split_mode((prefs_get_int(PREF_KEY_VOICETHREAD, 1) != 0) ? eSplitThreaded : eSplitSerial);
    sound_engine_set_economy(prefs_get_int(PREF_KEY_ECONOMY, 0) != 0);
}

#ifdef __cplusplus
}
#endif
