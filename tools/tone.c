/*
 * tone — a sine out of chosen channels of an audio interface, for driving the G2's inputs.
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
// Notes: Docs/code-notes/tone.c.md - "// notes §k" refers there.

// notes §1

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_CHANNELS    (64)
#define MAX_STEPS       (256)

static double         gPhase;
static double         gStep;
static _Atomic float  gAmp;     // written by main() at each step, read by the render callback
static bool           gPlay[MAX_CHANNELS];

static bool ok(OSStatus status, const char * what) {
    if (status == noErr) {
        return true;
    }
    fprintf(stderr, "error: %s failed (%d)\n", what, (int)status);
    return false;
}

static char * device_name(AudioObjectID device) {
    AudioObjectPropertyAddress address = {kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    CFStringRef                name    = NULL;
    UInt32                     size    = sizeof(CFStringRef);   // the property IS a pointer, not the string's bytes

    if (AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &name) != noErr || name == NULL) {
        return NULL;
    }
    CFIndex                    max     = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name), kCFStringEncodingUTF8) + 1;
    char *                     out     = calloc(1, (size_t)max);

    if (out != NULL) {
        CFStringGetCString(name, out, max, kCFStringEncodingUTF8);
    }
    CFRelease(name);
    return out;
}

// Output channel count, summed over the device's output streams. Zero means input-only.
static uint32_t device_output_channels(AudioObjectID device) {
    AudioObjectPropertyAddress address  = {kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain};
    UInt32                     size     = 0;

    if (AudioObjectGetPropertyDataSize(device, &address, 0, NULL, &size) != noErr || size == 0) {
        return 0;
    }
    AudioBufferList *          list     = malloc(size);
    uint32_t                   channels = 0;

    if (list != NULL) {
        if (AudioObjectGetPropertyData(device, &address, 0, NULL, &size, list) == noErr) {
            for (UInt32 i = 0; i < list->mNumberBuffers; i++) {
                channels += list->mBuffers[i].mNumberChannels;
            }
        }
        free(list);
    }
    return channels;
}

static double device_rate(AudioObjectID device) {
    AudioObjectPropertyAddress address = {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    Float64                    rate    = 0.0;
    UInt32                     size    = sizeof(rate);

    if (AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &rate) != noErr) {
        return 0.0;
    }
    return (double)rate;
}

static AudioObjectID * all_devices(uint32_t * countOut) {
    AudioObjectPropertyAddress address = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    UInt32                     size    = 0;

    *countOut = 0;

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) != noErr) {
        return NULL;
    }
    AudioObjectID *            list    = malloc(size);

    if (list == NULL) {
        return NULL;
    }

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, list) != noErr) {
        free(list);
        return NULL;
    }
    *countOut = size / (uint32_t)sizeof(AudioObjectID);
    return list;
}

static void list_devices(void) {
    uint32_t        count = 0;
    AudioObjectID * list  = all_devices(&count);

    printf("output devices:\n");

    for (uint32_t i = 0; i < count; i++) {
        uint32_t channels = device_output_channels(list[i]);

        if (channels > 0) {
            char * name = device_name(list[i]);

            printf("  %-40s %2u ch  %.0f Hz\n", (name != NULL) ? name : "?", channels, device_rate(list[i]));
            free(name);
        }
    }
    free(list);
}

// The first device with outputs whose name contains `wanted`.
static AudioObjectID find_device(const char * wanted, uint32_t * channelsOut) {
    uint32_t        count = 0;
    AudioObjectID * list  = all_devices(&count);
    AudioObjectID   found = kAudioObjectUnknown;

    for (uint32_t i = 0; (i < count) && (found == kAudioObjectUnknown); i++) {
        uint32_t channels = device_output_channels(list[i]);
        char *   name     = device_name(list[i]);

        if ((channels > 0) && (name != NULL) && (strstr(name, wanted) != NULL)) {
            found        = list[i];
            *channelsOut = channels;
            fprintf(stderr, "device \"%s\": %u output channels at %.0f Hz\n", name, channels, device_rate(list[i]));
        }
        free(name);
    }
    free(list);
    return found;
}

static OSStatus render(void * ref, AudioUnitRenderActionFlags * flags, const AudioTimeStamp * ts,
                       UInt32 bus, UInt32 frames, AudioBufferList * io) {
    (void)ref;
    (void)flags;
    (void)ts;
    (void)bus;
    float amp = atomic_load_explicit(&gAmp, memory_order_relaxed);

    for (UInt32 f = 0; f < frames; f++) {
        float x = amp * (float)sin(gPhase);

        gPhase += gStep;

        if (gPhase >= (2.0 * M_PI)) {
            gPhase -= 2.0 * M_PI;
        }

        for (UInt32 b = 0; b < io->mNumberBuffers; b++) {
            ((float *)io->mBuffers[b].mData)[f] = ((b < MAX_CHANNELS) && gPlay[b]) ? x : 0.0f;
        }
    }
    return noErr;
}

static void pause_seconds(double seconds) {
    struct timespec t = {(time_t)seconds, (long)((seconds - floor(seconds)) * 1e9)};

    nanosleep(&t, NULL);
}

static void usage(const char * self) {
    fprintf(stderr,
            "usage: %s --list\n"
            "       %s --device <name> --channels 0[,1...] [--hz F] [--db L[,L...]] [--hold S]\n"
            "  --channels  output channels to play on, 0-indexed; every other channel is silent\n"
            "  --hz        frequency, default 1000\n"
            "  --db        level in dBFS, default -20; a list plays each in turn\n"
            "  --hold      seconds per level, default 3\n", self, self);
}

int main(int argc, char ** argv) {
    const char * wanted    = NULL;
    double       hz        = 1000.0;
    double       hold      = 3.0;
    double       levels[MAX_STEPS];
    uint32_t     stepCount = 0;
    bool         anyOn     = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) {
            list_devices();
            return 0;
        } else if ((strcmp(argv[i], "--device") == 0) && ((i + 1) < argc)) {
            wanted = argv[++i];
        } else if ((strcmp(argv[i], "--channels") == 0) && ((i + 1) < argc)) {
            for (char * tok = strtok(argv[++i], ","); tok != NULL; tok = strtok(NULL, ",")) {
                unsigned long c = strtoul(tok, NULL, 10);

                if (c < MAX_CHANNELS) {
                    gPlay[c] = true;
                    anyOn    = true;
                }
            }
        } else if ((strcmp(argv[i], "--hz") == 0) && ((i + 1) < argc)) {
            hz = atof(argv[++i]);
        } else if ((strcmp(argv[i], "--db") == 0) && ((i + 1) < argc)) {
            for (char * tok = strtok(argv[++i], ","); (tok != NULL) && (stepCount < MAX_STEPS); tok = strtok(NULL, ",")) {
                levels[stepCount++] = atof(tok);
            }
        } else if ((strcmp(argv[i], "--hold") == 0) && ((i + 1) < argc)) {
            hold = atof(argv[++i]);
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if ((wanted == NULL) || (anyOn == false)) {
        usage(argv[0]);
        return 2;
    }

    if (stepCount == 0) {
        levels[stepCount++] = -20.0;
    }

    // notes §2
    for (uint32_t s = 0; s < stepCount; s++) {
        if (levels[s] > 0.0) {
            fprintf(stderr, "error: %.1f dBFS is above full scale\n", levels[s]);
            return 2;
        }
    }
    uint32_t                    channels = 0;
    AudioObjectID               device   = find_device(wanted, &channels);

    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "error: no output device matching \"%s\" (try --list)\n", wanted);
        return 1;
    }
    double                      rate     = device_rate(device);

    gStep = 2.0 * M_PI * hz / rate;
    atomic_store(&gAmp, (float)pow(10.0, levels[0] / 20.0));

    AudioComponentDescription   desc     = {kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0};
    AudioComponent              comp     = AudioComponentFindNext(NULL, &desc);
    AudioUnit                   unit     = NULL;

    if ((comp == NULL) || !ok(AudioComponentInstanceNew(comp, &unit), "AudioComponentInstanceNew")) {
        return 1;
    }

    if (!ok(AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, sizeof(device)), "set device")) {
        return 1;
    }
    AudioStreamBasicDescription format   = {0};

    format.mSampleRate       = rate;
    format.mFormatID         = kAudioFormatLinearPCM;
    format.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    format.mBytesPerPacket   = sizeof(float);
    format.mFramesPerPacket  = 1;
    format.mBytesPerFrame    = sizeof(float);
    format.mChannelsPerFrame = channels;
    format.mBitsPerChannel   = 32;

    if (!ok(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format)), "set format")) {
        return 1;
    }
    AURenderCallbackStruct      callback = {render, NULL};

    if (  !ok(AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback)), "set callback")
       || !ok(AudioUnitInitialize(unit), "AudioUnitInitialize")
       || !ok(AudioOutputUnitStart(unit), "AudioOutputUnitStart")) {
        return 1;
    }

    // notes §3
    for (uint32_t s = 0; s < stepCount; s++) {
        atomic_store(&gAmp, (float)pow(10.0, levels[s] / 20.0));
        printf("%.0f Hz at %.1f dBFS\n", hz, levels[s]);
        fflush(stdout);
        pause_seconds(hold);
    }
    AudioOutputUnitStop(unit);
    AudioUnitUninitialize(unit);
    AudioComponentInstanceDispose(unit);
    return 0;
}
