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

// The Windows src/audioOutput.c: WASAPI through miniaudio (platform/windows/miniaudio.h). Same API, same
// pref keys, so the menus and settings work as on the Mac. Docs/code-notes/audioOutputWin.c.md.

#ifdef __cplusplus
extern "C" {
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>    // WideCharToMultiByte, for the endpoint IDs
#include <stdatomic.h>
#include <string.h>
#include "defs.h"
#include "synthlibDefs.h"
#include "prefs.h"
#include "audioOutput.h"
#include "soundEngine.h"
#include "miniaudio.h"

#define PREF_KEY_DEVICE         "audioOutputDeviceUID"
#define PREF_KEY_LEFT           "audioOutputLeftChannel"
#define PREF_KEY_RIGHT          "audioOutputRightChannel"
#define PREF_KEY_CHANNEL        "audioOutputFirstChannel"
#define PREF_KEY_BUFFER         "audioOutputBufferFrames"
#define PREF_KEY_AHEAD          "audioRenderAheadMs"
#define PREF_KEY_VOICETHREAD    "engineVoiceThread"
#define PREF_KEY_LEVEL          "audioOutputLevelDb"

#define MAX_DEVICES             (32)
#define MAX_CALLBACK_FRAMES     (8192)    // notes §2 - the stereo the engine renders before it is spread

typedef struct {
    ma_device_id id;
    char         uid[AUDIO_DEVICE_UID_MAX];
    char         name[128];
    uint32_t     channels;
} tWinDevice;

static ma_context       gContext;
static bool             gContextReady                      = false;
static ma_device        gDevice;
static bool             gDeviceOpen                        = false;
static tWinDevice       gDevices[MAX_DEVICES];
static uint32_t         gDeviceCount                       = 0;
static char             gSelectedUid[AUDIO_DEVICE_UID_MAX] = {0};
static _Atomic uint32_t gLeftChannel                       = 0;
static _Atomic uint32_t gRightChannel                      = 1;
static uint32_t         gBufferFrames                      = 0;
static uint32_t         gAheadMs                           = 0;
static int32_t          gLevelDb                           = 0;
static double           gSampleRate                        = 0.0;
static uint32_t         gOpenChannels                      = 0;
static char             gThreadText[96]                    = "no audio output";
static float            gStereo[MAX_CALLBACK_FRAMES * 2];

static bool context_ready(void) {
    ma_backend backends[] = { ma_backend_wasapi };

    if (gContextReady == false) {
        gContextReady = (ma_context_init(backends, 1, NULL, &gContext) == MA_SUCCESS);
    }
    return gContextReady;
}

// notes §1 - a WASAPI endpoint ID is UTF-16; as UTF-8 it is the uid the prefs keep, as the Mac keeps CoreAudio's
static void enumerate(void) {
    ma_device_info * infos = NULL;
    ma_uint32        count = 0;

    gDeviceCount = 0;

    if ((context_ready() == false) || (ma_context_get_devices(&gContext, &infos, &count, NULL, NULL) != MA_SUCCESS)) {
        return;
    }

    for (ma_uint32 i = 0; (i < count) && (gDeviceCount < MAX_DEVICES); i++) {
        tWinDevice *   d    = &gDevices[gDeviceCount];
        ma_device_info full = infos[i];

        d->id       = infos[i].id;
        snprintf(d->name, sizeof(d->name), "%s", infos[i].name);
        WideCharToMultiByte(CP_UTF8, 0, infos[i].id.wasapi, -1, d->uid, (int)sizeof(d->uid), NULL, NULL);
        d->channels = 2;

        if (ma_context_get_device_info(&gContext, ma_device_type_playback, &infos[i].id, &full) == MA_SUCCESS) {
            for (ma_uint32 f = 0; f < full.nativeDataFormatCount; f++) {
                if (full.nativeDataFormats[f].channels > d->channels) {
                    d->channels = full.nativeDataFormats[f].channels;
                }
            }
        }
        gDeviceCount++;
    }
}

static int selected_index(void) {
    for (uint32_t i = 0; i < gDeviceCount; i++) {
        if (strcmp(gDevices[i].uid, gSelectedUid) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// notes §2 - the engine renders the stereo pair; each frame's two samples go to the chosen outputs, the rest silent
static void data_callback(ma_device * device, void * output, const void * input, ma_uint32 frames) {
    float *  out      = (float *)output;
    uint32_t channels = device->playback.channels;
    uint32_t left     = atomic_load(&gLeftChannel);
    uint32_t right    = atomic_load(&gRightChannel);

    (void)input;

    while (frames > 0) {
        uint32_t chunk = (frames > MAX_CALLBACK_FRAMES) ? MAX_CALLBACK_FRAMES : frames;

        sound_engine_render(gStereo, chunk, 2);
        memset(out, 0, (size_t)chunk * channels * sizeof(float));

        for (uint32_t f = 0; f < chunk; f++) {
            if (left < channels) {
                out[(f * channels) + left] = gStereo[2u * f];
            }

            if (right < channels) {
                out[(f * channels) + right] = gStereo[(2u * f) + 1u];
            }
        }
        out    += (size_t)chunk * channels;
        frames -= chunk;
    }
}

bool audio_output_start(void) {
    if (gDeviceOpen == true) {
        return true;
    }
    enumerate();

    if (context_ready() == false) {
        LOG_ERROR("Sound engine: WASAPI is not available\n");
        return false;
    }
    int             chosen = selected_index();
    ma_device_config config = ma_device_config_init(ma_device_type_playback);

    config.playback.pDeviceID = (chosen >= 0) ? &gDevices[chosen].id : NULL;   // none chosen: the default output
    config.playback.format    = ma_format_f32;
    config.playback.channels  = 0;                                              // the device's own count
    config.sampleRate         = 0;                                              // and its own rate
    config.periodSizeInFrames = gBufferFrames;                                  // 0: miniaudio's low-latency default
    config.performanceProfile = ma_performance_profile_low_latency;
    config.wasapi.usage       = ma_wasapi_usage_pro_audio;                      // MMCSS "Pro Audio" for its thread
    config.noPreSilencedOutputBuffer = MA_TRUE;
    config.dataCallback       = data_callback;

    if (ma_device_init(&gContext, &config, &gDevice) != MA_SUCCESS) {
        LOG_ERROR("Sound engine: could not open the WASAPI output\n");
        return false;
    }
    gSampleRate   = (double)gDevice.sampleRate;
    gOpenChannels = gDevice.playback.channels;
    // the engine has to know the rate before the first callback, as on the Mac
    sound_engine_set_sample_rate(gSampleRate);

    if (ma_device_start(&gDevice) != MA_SUCCESS) {
        LOG_ERROR("Sound engine: could not start the WASAPI output\n");
        ma_device_uninit(&gDevice);
        return false;
    }
    gDeviceOpen = true;
    snprintf(gThreadText, sizeof(gThreadText), "WASAPI shared, %u Hz, %u ch, %u frames",
             (unsigned)gDevice.sampleRate, (unsigned)gOpenChannels, (unsigned)gDevice.playback.internalPeriodSizeInFrames);
    LOG_DEBUG("Sound engine: audio output started at %.0f Hz, %u channels\n", gSampleRate, (unsigned)gOpenChannels);
    return true;
}

void audio_output_stop(void) {
    if (gDeviceOpen == true) {
        ma_device_uninit(&gDevice);   // waits for a callback in flight to return
        gDeviceOpen = false;
    }
    gSampleRate = 0.0;
    snprintf(gThreadText, sizeof(gThreadText), "%s", "no audio output");
}

static void reopen_if_running(void) {
    if (gDeviceOpen == true) {
        audio_output_stop();
        (void)audio_output_start();
    }
}

double audio_output_sample_rate(void) {
    return gSampleRate;
}

uint32_t audio_output_device_count(void) {
    enumerate();
    return gDeviceCount;
}

const char * audio_output_device_name(uint32_t index) {
    return (index < gDeviceCount) ? gDevices[index].name : "";
}

uint32_t audio_output_device_channels(uint32_t index) {
    return (index < gDeviceCount) ? gDevices[index].channels : 0;
}

bool audio_output_device_is_selected(uint32_t index) {
    return (index < gDeviceCount) && (strcmp(gDevices[index].uid, gSelectedUid) == 0);
}

const char * audio_output_device_uid(uint32_t index) {
    return (index < gDeviceCount) ? gDevices[index].uid : "";
}

bool audio_output_select_device_by_uid(const char * uid) {
    if (uid == NULL) {
        return false;
    }
    snprintf(gSelectedUid, sizeof(gSelectedUid), "%s", uid);
    prefs_set_string(PREF_KEY_DEVICE, gSelectedUid);
    enumerate();
    reopen_if_running();
    return selected_index() >= 0;
}

void audio_output_select_left_channel(uint32_t channel) {
    atomic_store(&gLeftChannel, channel);
    prefs_set_int(PREF_KEY_LEFT, (long)channel);
}

void audio_output_select_right_channel(uint32_t channel) {
    atomic_store(&gRightChannel, channel);
    prefs_set_int(PREF_KEY_RIGHT, (long)channel);
}

uint32_t audio_output_left_channel(void) {
    return atomic_load(&gLeftChannel);
}

uint32_t audio_output_right_channel(void) {
    return atomic_load(&gRightChannel);
}

uint32_t audio_output_selected_device_channels(void) {
    int i = selected_index();

    return (gDeviceOpen == true) ? gOpenChannels : ((i >= 0) ? gDevices[i].channels : 2u);
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
    reopen_if_running();
}

// notes §3 - kept and saved, but not used: WASAPI's own buffering stands in for it here
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
    reopen_if_running();
}

bool audio_output_poll_rate_change(void) {
    return false;
}

// notes §4 - WASAPI reports no overloads; the engine's own "late" count stands in
uint32_t audio_output_overload_count(void) {
    return 0;
}

const char * audio_output_thread_text(void) {
    return gThreadText;
}

void audio_output_load_settings(void) {
    const char * uid = prefs_get_string(PREF_KEY_DEVICE, "");

    if (uid != NULL) {
        snprintf(gSelectedUid, sizeof(gSelectedUid), "%s", uid);
    }
    sound_engine_set_dac_emulation(prefs_get_int(PREF_KEY_DAC_EMULATION, 0) != 0);
    gLevelDb      = (int32_t)prefs_get_int(PREF_KEY_LEVEL, 0);

    if (gLevelDb > 0) {
        gLevelDb = 0;
    }
    sound_engine_set_output_level_db((double)gLevelDb);

    if (prefs_has_key(PREF_KEY_LEFT) == true) {
        atomic_store(&gLeftChannel, (uint32_t)prefs_get_int(PREF_KEY_LEFT, 0));
        atomic_store(&gRightChannel, (uint32_t)prefs_get_int(PREF_KEY_RIGHT, 1));
    } else {
        uint32_t first = (uint32_t)prefs_get_int(PREF_KEY_CHANNEL, 0);

        atomic_store(&gLeftChannel, first);
        atomic_store(&gRightChannel, first + 1);
    }
    gBufferFrames = (uint32_t)prefs_get_int(PREF_KEY_BUFFER, 0);
    gAheadMs      = (uint32_t)prefs_get_int(PREF_KEY_AHEAD, 0);
    sound_engine_set_split_mode((prefs_get_int(PREF_KEY_VOICETHREAD, 1) != 0) ? eSplitThreaded : eSplitSerial);
    sound_engine_set_economy(prefs_get_int(PREF_KEY_ECONOMY, 0) != 0);
}

#ifdef __cplusplus
}
#endif
