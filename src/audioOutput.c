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

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#pragma clang diagnostic pop

#include "defs.h"
#include "synthlibDefs.h"
#include "prefs.h"
#include "audioOutput.h"
#include "soundEngine.h"

// A HAL output AudioUnit rather than the default-output one. The difference is the whole point: the
// default-output unit always follows the system's chosen device and cannot be pointed anywhere else,
// while the HAL unit takes a device and a channel map — which is what allows the engine to be sent
// to, say, outputs 29/30 of an interface while the system carries on using the built-in speakers.
// See audioOutput.h.

#define OUTPUT_CHANNELS      (2)
#define MAX_AUDIO_DEVICES    (32)
#define DEVICE_NAME_SIZE     (96)
#define DEVICE_UID_SIZE      (160)

#define PREF_KEY_DEVICE      "audioOutputDeviceUID"
#define PREF_KEY_LEFT        "audioOutputLeftChannel"
#define PREF_KEY_RIGHT       "audioOutputRightChannel"
#define PREF_KEY_CHANNEL     "audioOutputFirstChannel"   // superseded; read once to migrate
#define PREF_KEY_BUFFER      "audioOutputBufferFrames"
#define PREF_KEY_LEVEL       "audioOutputLevelDb"

typedef struct {
    AudioObjectID id;
    char          name[DEVICE_NAME_SIZE];
    char          uid[DEVICE_UID_SIZE];
    uint32_t      channels;
} tAudioDevice;

static AudioUnit    gOutputUnit                   = NULL;
static bool         gRunning                      = false;
static double       gSampleRate                   = 0.0;

static tAudioDevice gDevice[MAX_AUDIO_DEVICES];
static uint32_t     gDeviceCount                  = 0;

// The chosen device is remembered by UID, not by index: indices shuffle as interfaces are plugged
// and unplugged, and picking up the wrong device after a reboot is worse than falling back.
static char         gSelectedUid[DEVICE_UID_SIZE] = {0};
static uint32_t     gLeftChannel                  = 0;
static uint32_t     gRightChannel                 = 1;

// 0 means "whatever the device already has", which is what it was before this was selectable.
static uint32_t     gBufferFrames                 = 0;

// The engine's output attenuation, in dB and never positive. Kept here with the other output
// settings rather than in the engine, because this is where the preferences plumbing already lives
// and where the rest of the audio path's remembered state is read at startup. The engine holds the
// working value; this owns the persistence.
static int32_t      gLevelDb                      = 0;

// Reads a CFString device property into a plain C buffer.
static void device_string_property(AudioObjectID device, AudioObjectPropertySelector selector,
                                   char * out, size_t outSize) {
    AudioObjectPropertyAddress address = {
        selector, kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef                text    = NULL;
    UInt32                     size    = (UInt32)sizeof(text);

    out[0] = '\0';

    if (AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &text) != noErr) {
        return;
    }

    if (text != NULL) {
        CFStringGetCString(text, out, (CFIndex)outSize, kCFStringEncodingUTF8);
        CFRelease(text);
    }
}

// How many output channels a device offers, summed across its streams.
static uint32_t device_output_channels(AudioObjectID device) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };
    UInt32                     size    = 0;
    AudioBufferList *          list    = NULL;
    uint32_t                   count   = 0;
    UInt32                     i       = 0;

    if (AudioObjectGetPropertyDataSize(device, &address, 0, NULL, &size) != noErr) {
        return 0;
    }
    list = (AudioBufferList *)malloc(size);

    if (list == NULL) {
        return 0;
    }

    if (AudioObjectGetPropertyData(device, &address, 0, NULL, &size, list) == noErr) {
        for (i = 0; i < list->mNumberBuffers; i++) {
            count += list->mBuffers[i].mNumberChannels;
        }
    }
    free(list);
    return count;
}

// Rebuilds the list of devices that can actually play something. Devices with no output channels —
// microphones, aggregate inputs — are left out rather than offered and then failing to open.
static void rebuild_device_list(void) {
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32                     size    = 0;
    AudioObjectID *            ids     = NULL;
    uint32_t                   total   = 0;
    uint32_t                   i       = 0;

    gDeviceCount = 0;

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) != noErr) {
        return;
    }
    ids          = (AudioObjectID *)malloc(size);

    if (ids == NULL) {
        return;
    }

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, ids) == noErr) {
        total = size / (UInt32)sizeof(AudioObjectID);

        for (i = 0; (i < total) && (gDeviceCount < MAX_AUDIO_DEVICES); i++) {
            uint32_t channels = device_output_channels(ids[i]);

            if (channels == 0) {
                continue;
            }
            gDevice[gDeviceCount].id       = ids[i];
            gDevice[gDeviceCount].channels = channels;
            device_string_property(ids[i], kAudioDevicePropertyDeviceNameCFString,
                                   gDevice[gDeviceCount].name, sizeof(gDevice[gDeviceCount].name));
            device_string_property(ids[i], kAudioDevicePropertyDeviceUID,
                                   gDevice[gDeviceCount].uid, sizeof(gDevice[gDeviceCount].uid));
            gDeviceCount++;
        }
    }
    free(ids);
}

// The device index matching the remembered UID, or the system default when there is no match — an
// interface that is not plugged in should fall back rather than refusing to make a sound.
static int32_t selected_device_index(void) {
    uint32_t i = 0;

    for (i = 0; i < gDeviceCount; i++) {
        if ((gSelectedUid[0] != '\0') && (strcmp(gDevice[i].uid, gSelectedUid) == 0)) {
            return (int32_t)i;
        }
    }

    {
        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        AudioObjectID              device  = 0;
        UInt32                     size    = (UInt32)sizeof(device);

        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, &device) == noErr) {
            for (i = 0; i < gDeviceCount; i++) {
                if (gDevice[i].id == device) {
                    return (int32_t)i;
                }
            }
        }
    }
    return (gDeviceCount > 0) ? 0 : -1;
}

uint32_t audio_output_device_count(void) {
    rebuild_device_list();
    return gDeviceCount;
}

const char * audio_output_device_name(uint32_t index) {
    return (index < gDeviceCount) ? gDevice[index].name : "";
}

uint32_t audio_output_device_channels(uint32_t index) {
    return (index < gDeviceCount) ? gDevice[index].channels : 0;
}

bool audio_output_device_is_selected(uint32_t index) {
    return (int32_t)index == selected_device_index();
}

uint32_t audio_output_buffer_frames(void) {
    return gBufferFrames;
}

// Asks the device for a buffer size. It is a property of the DEVICE, so it is shared with anything
// else playing through it; CoreAudio arbitrates. Best effort — a device that refuses keeps what it
// had, which costs responsiveness and nothing else.
static void apply_buffer_frames(AudioObjectID device) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyBufferFrameSizeRange,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    AudioValueRange            range   = {0.0, 0.0};
    UInt32                     size    = (UInt32)sizeof(range);
    UInt32                     frames  = gBufferFrames;

    if (gBufferFrames == 0) {
        return;
    }

    if (AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &range) == noErr) {
        if ((double)frames < range.mMinimum) {
            frames = (UInt32)range.mMinimum;
        } else if ((double)frames > range.mMaximum) {
            frames = (UInt32)range.mMaximum;
        }
    }
    address.mSelector = kAudioDevicePropertyBufferFrameSize;

    // Must happen BEFORE the device is bound to the unit. Once the HAL unit has the device, this is
    // accepted and then quietly ignored — the size the unit was bound with is the one that sticks.
    if (AudioObjectSetPropertyData(device, &address, 0, NULL, (UInt32)sizeof(frames), &frames) != noErr) {
        LOG_ERROR("Sound engine: the device would not take a %u frame buffer\n", (unsigned)frames);
    } else {
        LOG_DEBUG("Sound engine: buffer set to %u frames\n", (unsigned)frames);
    }
}

uint32_t audio_output_left_channel(void) {
    return gLeftChannel;
}

uint32_t audio_output_right_channel(void) {
    return gRightChannel;
}

uint32_t audio_output_selected_device_channels(void) {
    int32_t index = selected_device_index();

    return (index >= 0) ? gDevice[index].channels : 0;
}

void audio_output_load_settings(void) {
    const char * uid = prefs_get_string(PREF_KEY_DEVICE, "");

    if (uid != NULL) {
        strncpy(gSelectedUid, uid, sizeof(gSelectedUid) - 1);
    }
    // Left and right used to be one "first channel of a pair" setting. Carry an old one over rather
    // than dropping someone back to outputs 1/2 without explanation.
    // Read the level before anything can make a noise, and push it into the engine — the engine has
    // no preference of its own, so without this a remembered attenuation would be forgotten.
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
}

void audio_output_select_device(uint32_t index) {
    bool wasRunning = gRunning;

    if (index >= gDeviceCount) {
        return;
    }
    strncpy(gSelectedUid, gDevice[index].uid, sizeof(gSelectedUid) - 1);
    prefs_set_string(PREF_KEY_DEVICE, gSelectedUid);

    // Channels that existed on the old device may not exist on this one.
    if ((gLeftChannel >= gDevice[index].channels) || (gRightChannel >= gDevice[index].channels)) {
        gLeftChannel  = 0;
        gRightChannel = (gDevice[index].channels > 1) ? 1 : 0;
        prefs_set_int(PREF_KEY_LEFT, (long)gLeftChannel);
        prefs_set_int(PREF_KEY_RIGHT, (long)gRightChannel);
    }

    if (wasRunning == true) {
        audio_output_stop();
        (void)audio_output_start();
    }
}

static void reopen_if_running(void) {
    if (gRunning == true) {
        audio_output_stop();
        (void)audio_output_start();
    }
}

int32_t audio_output_level_db(void) {
    return gLevelDb;
}

void audio_output_select_level_db(int32_t db) {
    if (db > 0) {
        db = 0;
    }
    gLevelDb = db;
    prefs_set_int(PREF_KEY_LEVEL, (long)db);
    sound_engine_set_output_level_db((double)db);
}

void audio_output_select_buffer_frames(uint32_t frames) {
    gBufferFrames = frames;
    prefs_set_int(PREF_KEY_BUFFER, (long)frames);
    reopen_if_running();
}

void audio_output_select_left_channel(uint32_t channel) {
    gLeftChannel = channel;
    prefs_set_int(PREF_KEY_LEFT, (long)channel);
    reopen_if_running();
}

void audio_output_select_right_channel(uint32_t channel) {
    gRightChannel = channel;
    prefs_set_int(PREF_KEY_RIGHT, (long)channel);
    reopen_if_running();
}

// Real-time thread. Everything it calls must be lock-free and allocation-free, which is why it does
// nothing but hand the buffer straight to the engine.
static OSStatus render_callback(void *                       inRefCon,
                                AudioUnitRenderActionFlags * ioActionFlags,
                                const AudioTimeStamp *       inTimeStamp,
                                UInt32                       inBusNumber,
                                UInt32                       inNumberFrames,
                                AudioBufferList *            ioData) {
    (void)inRefCon;
    (void)ioActionFlags;
    (void)inTimeStamp;
    (void)inBusNumber;

    if ((ioData == NULL) || (ioData->mNumberBuffers == 0)) {
        return noErr;
    }
    // Interleaved, so there is exactly one buffer holding all the channels.
    sound_engine_render((float *)ioData->mBuffers[0].mData,
                        (uint32_t)inNumberFrames,
                        (uint32_t)ioData->mBuffers[0].mNumberChannels);
    return noErr;
}

bool audio_output_start(void) {
    AudioComponentDescription   description    = {0};
    AudioComponent              component      = NULL;
    AURenderCallbackStruct      callback       = {0};
    AudioStreamBasicDescription format         = {0};
    UInt32                      formatSize     = (UInt32)sizeof(format);
    OSStatus                    status         = noErr;
    uint32_t                    deviceChannels = 0;

    if (gRunning == true) {
        return true;
    }
    // Buffer size first, before an AudioUnit instance exists at all. Once this process has a unit —
    // even an unbound one — the HAL appears to settle the device's buffer and later requests are
    // accepted but ignored.
    {
        int32_t early = selected_device_index();

        if (early >= 0) {
            apply_buffer_frames(gDevice[early].id);
        }
    }
    description.componentType         = kAudioUnitType_Output;
    description.componentSubType      = kAudioUnitSubType_HALOutput;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;

    component                         = AudioComponentFindNext(NULL, &description);

    if (component == NULL) {
        LOG_ERROR("Sound engine: no HAL output audio component\n");
        return false;
    }
    status                            = AudioComponentInstanceNew(component, &gOutputUnit);

    if ((status != noErr) || (gOutputUnit == NULL)) {
        LOG_ERROR("Sound engine: AudioComponentInstanceNew failed (%d)\n", (int)status);
        gOutputUnit = NULL;
        return false;
    }
    // The HAL unit has output disabled on bus 0 by default; the default-output unit did not need
    // this, which is an easy thing to miss when moving between the two.
    {
        UInt32 enable = 1;

        status = AudioUnitSetProperty(gOutputUnit, kAudioOutputUnitProperty_EnableIO,
                                      kAudioUnitScope_Output, 0, &enable, (UInt32)sizeof(enable));

        if (status != noErr) {
            LOG_ERROR("Sound engine: could not enable output on the HAL unit (%d)\n", (int)status);
            audio_output_stop();
            return false;
        }
    }

    // Point the unit at the chosen device before anything else is asked of it — the stream format
    // and the channel map below are both properties OF that device, so setting them first would
    // configure whichever one the unit happened to default to.
    {
        int32_t index = selected_device_index();

        if (index < 0) {
            LOG_ERROR("Sound engine: no output device available\n");
            audio_output_stop();
            return false;
        }
        deviceChannels = gDevice[index].channels;
        status         = AudioUnitSetProperty(gOutputUnit, kAudioOutputUnitProperty_CurrentDevice,
                                              kAudioUnitScope_Global, 0,
                                              &gDevice[index].id, (UInt32)sizeof(AudioObjectID));

        if (status != noErr) {
            LOG_ERROR("Sound engine: could not select output device '%s' (%d)\n",
                      gDevice[index].name, (int)status);
            audio_output_stop();
            return false;
        }
        LOG_DEBUG("Sound engine: output device '%s', %u channels, L=out %u R=out %u\n",
                  gDevice[index].name, (unsigned)deviceChannels,
                  (unsigned)gLeftChannel + 1, (unsigned)gRightChannel + 1);
    }

    // Take the device's own rate rather than forcing one, so CoreAudio does no rate conversion on
    // our behalf and the engine's idea of the rate is the real one.
    status                   = AudioUnitGetProperty(gOutputUnit, kAudioUnitProperty_StreamFormat,
                                                    kAudioUnitScope_Output, 0, &format, &formatSize);

    if (status != noErr) {
        LOG_ERROR("Sound engine: could not read the output stream format (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    gSampleRate              = format.mSampleRate;

    if (gSampleRate <= 0.0) {
        gSampleRate = 48000.0;
    }
    // 32-bit float, interleaved, so the render callback gets a single buffer to fill.
    format.mFormatID         = kAudioFormatLinearPCM;
    format.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mSampleRate       = gSampleRate;
    format.mChannelsPerFrame = OUTPUT_CHANNELS;
    format.mBitsPerChannel   = 32;
    format.mFramesPerPacket  = 1;
    format.mBytesPerFrame    = (format.mBitsPerChannel / 8) * format.mChannelsPerFrame;
    format.mBytesPerPacket   = format.mBytesPerFrame * format.mFramesPerPacket;

    status                   = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_StreamFormat,
                                                    kAudioUnitScope_Input, 0, &format, (UInt32)sizeof(format));

    if (status != noErr) {
        LOG_ERROR("Sound engine: could not set the output stream format (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    callback.inputProc       = render_callback;
    status                   = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_SetRenderCallback,
                                                    kAudioUnitScope_Input, 0, &callback, (UInt32)sizeof(callback));

    if (status != noErr) {
        LOG_ERROR("Sound engine: could not install the render callback (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }

    // Route our stereo pair to the chosen output channels. The map has one entry per DEVICE channel
    // saying which of our two it takes, or -1 for silence — so sending to outputs 29 and 30 means a
    // map of -1s with 0 and 1 at positions 28 and 29. Without this the audio always lands on the
    // device's first pair, whatever the interface.
    if (  (deviceChannels >= OUTPUT_CHANNELS)
       && ((deviceChannels > OUTPUT_CHANNELS) || (gLeftChannel != 0) || (gRightChannel != 1))) {
        SInt32 * map = NULL;

        // Remembered channels that this device does not have would index off the end of the map.
        if (gLeftChannel >= deviceChannels) {
            gLeftChannel = 0;
        }

        if (gRightChannel >= deviceChannels) {
            gRightChannel = (deviceChannels > 1) ? 1 : 0;
        }
        map = (SInt32 *)malloc(sizeof(SInt32) * deviceChannels);

        if (map != NULL) {
            uint32_t c = 0;

            for (c = 0; c < deviceChannels; c++) {
                map[c] = -1;
            }

            map[gLeftChannel]  = 0;
            map[gRightChannel] = 1;

            status             = AudioUnitSetProperty(gOutputUnit, kAudioOutputUnitProperty_ChannelMap,
                                                      kAudioUnitScope_Output, 0,
                                                      map, (UInt32)(sizeof(SInt32) * deviceChannels));
            free(map);

            if (status != noErr) {
                LOG_ERROR("Sound engine: could not map to outputs %u and %u (%d)\n",
                          (unsigned)gLeftChannel + 1, (unsigned)gRightChannel + 1, (int)status);
            }
        }
    }
    // The engine has to know the rate before the first callback can arrive, so tell it before the
    // unit is initialised rather than after it is started.
    sound_engine_set_sample_rate(gSampleRate);

    status   = AudioUnitInitialize(gOutputUnit);

    if (status != noErr) {
        LOG_ERROR("Sound engine: AudioUnitInitialize failed (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    status   = AudioOutputUnitStart(gOutputUnit);

    if (status != noErr) {
        LOG_ERROR("Sound engine: AudioOutputUnitStart failed (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    gRunning = true;
    LOG_DEBUG("Sound engine: audio output started at %.0f Hz\n", gSampleRate);
    return true;
}

void audio_output_stop(void) {
    if (gOutputUnit == NULL) {
        gRunning    = false;
        gSampleRate = 0.0;
        return;
    }

    // AudioOutputUnitStop waits for a render in flight to return, so once it has, the callback can
    // no longer be running and the unit is safe to tear down.
    if (gRunning == true) {
        AudioOutputUnitStop(gOutputUnit);
    }
    AudioUnitUninitialize(gOutputUnit);
    AudioComponentInstanceDispose(gOutputUnit);

    gOutputUnit = NULL;
    gRunning    = false;
    gSampleRate = 0.0;
}

double audio_output_sample_rate(void) {
    return gRunning ? gSampleRate : 0.0;
}

#ifdef __cplusplus
}
#endif
