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
#include "audioOutput.h"
#include "soundEngine.h"

// A default-output AudioUnit. This is the plainest route to the speakers that macOS offers: no
// Objective-C, no session management, and it follows whatever the user has picked as their system
// output device. See audioOutput.h.

#define OUTPUT_CHANNELS    (2)

static AudioUnit gOutputUnit = NULL;
static bool      gRunning    = false;
static double    gSampleRate = 0.0;

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
    AudioComponentDescription   description = {0};
    AudioComponent              component   = NULL;
    AURenderCallbackStruct      callback    = {0};
    AudioStreamBasicDescription format      = {0};
    UInt32                      formatSize  = (UInt32)sizeof(format);
    OSStatus                    status      = noErr;

    if (gRunning == true) {
        return true;
    }
    description.componentType         = kAudioUnitType_Output;
    description.componentSubType      = kAudioUnitSubType_DefaultOutput;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;

    component                         = AudioComponentFindNext(NULL, &description);

    if (component == NULL) {
        LOG_ERROR("Sound engine: no default output audio component\n");
        return false;
    }
    status                            = AudioComponentInstanceNew(component, &gOutputUnit);

    if ((status != noErr) || (gOutputUnit == NULL)) {
        LOG_ERROR("Sound engine: AudioComponentInstanceNew failed (%d)\n", (int)status);
        gOutputUnit = NULL;
        return false;
    }
    // Take the device's own rate rather than forcing one, so CoreAudio does no rate conversion on
    // our behalf and the engine's idea of the rate is the real one.
    status                            = AudioUnitGetProperty(gOutputUnit, kAudioUnitProperty_StreamFormat,
                                                             kAudioUnitScope_Output, 0, &format, &formatSize);

    if (status != noErr) {
        LOG_ERROR("Sound engine: could not read the output stream format (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    gSampleRate                       = format.mSampleRate;

    if (gSampleRate <= 0.0) {
        gSampleRate = 48000.0;
    }
    // 32-bit float, interleaved, so the render callback gets a single buffer to fill.
    format.mFormatID                  = kAudioFormatLinearPCM;
    format.mFormatFlags               = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mSampleRate                = gSampleRate;
    format.mChannelsPerFrame          = OUTPUT_CHANNELS;
    format.mBitsPerChannel            = 32;
    format.mFramesPerPacket           = 1;
    format.mBytesPerFrame             = (format.mBitsPerChannel / 8) * format.mChannelsPerFrame;
    format.mBytesPerPacket            = format.mBytesPerFrame * format.mFramesPerPacket;

    status                            = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_StreamFormat,
                                                             kAudioUnitScope_Input, 0, &format, (UInt32)sizeof(format));

    if (status != noErr) {
        LOG_ERROR("Sound engine: could not set the output stream format (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    callback.inputProc                = render_callback;
    status                            = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_SetRenderCallback,
                                                             kAudioUnitScope_Input, 0, &callback, (UInt32)sizeof(callback));

    if (status != noErr) {
        LOG_ERROR("Sound engine: could not install the render callback (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    // The engine has to know the rate before the first callback can arrive, so tell it before the
    // unit is initialised rather than after it is started.
    sound_engine_set_sample_rate(gSampleRate);

    status                            = AudioUnitInitialize(gOutputUnit);

    if (status != noErr) {
        LOG_ERROR("Sound engine: AudioUnitInitialize failed (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    status                            = AudioOutputUnitStart(gOutputUnit);

    if (status != noErr) {
        LOG_ERROR("Sound engine: AudioOutputUnitStart failed (%d)\n", (int)status);
        audio_output_stop();
        return false;
    }
    gRunning                          = true;
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
