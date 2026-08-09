/*
 * capture — multichannel audio capture for the G2 measurement rig.
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

// WHY THIS EXISTS RATHER THAN A LINE OF ffmpeg.
//
// ffmpeg's avfoundation input REPORTS the interface's real rate and channel count and then delivers
// something else: an AVCaptureSession converts audio to 48 kHz, so a 192 kHz interface yields a file
// LABELLED 192000 containing 48 kHz frames — a quarter of the samples, stretched over four times the
// stated duration. Nothing in the file says so. Measuring a delay line from that is off by 4x, and
// the only clue is that a 5 second capture claims to be 1.14 seconds long.
//
// It also has no working channel selection here: `-channels` is rejected outright by this build, and
// what arrives is whatever the device presents.
//
// So this talks to the HAL through AUHAL, which hands over the device's own format untouched. It
// prints the rate and channel count it actually got, and writes 32-bit PCM — see write_wav32() for
// why that width and not 24.
//
// Build:  cc -O2 -Wall -o capture capture.c -framework CoreAudio -framework AudioToolbox \
//                                           -framework CoreFoundation
// Usage:  ./capture --list
//         ./capture --device Fireface --seconds 12 --out cap.wav [--rate 192000]

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// The capture callback runs on a real-time thread, so it only ever memcpy's into a buffer that was
// allocated before the unit started. No malloc, no file I/O, no locks in there.
typedef struct {
    float *         samples;
    size_t          capacityFrames;
    uint32_t        channels;
    _Atomic size_t  writtenFrames;
    _Atomic size_t  overrunFrames;   // frames that arrived after the buffer filled
    AudioBufferList bufferList;      // one interleaved buffer, pointed at scratch below
    float *         scratch;
    size_t          scratchFrames;
} tCapture;

static AudioUnit gUnit = NULL;

static bool ok(OSStatus status, const char * what) {
    if (status == noErr) {
        return true;
    }
    // OSStatus is often a four-character code, which is far more use than the number.
    char code[5] = {0};

    code[0] = (char)((status >> 24) & 0xFF);
    code[1] = (char)((status >> 16) & 0xFF);
    code[2] = (char)((status >> 8) & 0xFF);
    code[3] = (char)(status & 0xFF);

    bool printable = true;

    for (int i = 0; i < 4; i++) {
        if ((code[i] < 32) || (code[i] > 126)) {
            printable = false;
        }
    }
    fprintf(stderr, "error: %s failed (%d", what, (int)status);

    if (printable) {
        fprintf(stderr, " '%s'", code);
    }
    fprintf(stderr, ")\n");
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

// Input channel count, summed over the device's input streams. Zero means output-only, which is how
// a device that cannot be recorded from is recognised without trying to open it.
static uint32_t device_input_channels(AudioObjectID device) {
    AudioObjectPropertyAddress address  = {kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMain};
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

    printf("input devices:\n");

    for (uint32_t i = 0; i < count; i++) {
        uint32_t channels = device_input_channels(list[i]);

        if (channels == 0) {
            continue;
        }
        char *   name     = device_name(list[i]);

        printf("  %-40s %2u in  %.0f Hz\n", (name != NULL) ? name : "?", channels, device_rate(list[i]));
        free(name);
    }
    free(list);
}

static AudioObjectID find_device(const char * wanted) {
    uint32_t        count = 0;
    AudioObjectID * list  = all_devices(&count);
    AudioObjectID   found = kAudioObjectUnknown;

    for (uint32_t i = 0; (i < count) && (found == kAudioObjectUnknown); i++) {
        if (device_input_channels(list[i]) == 0) {
            continue;
        }
        char *      name  = device_name(list[i]);

        if ((name != NULL) && (strcasestr(name, wanted) != NULL)) {
            found = list[i];
        }
        free(name);
    }
    free(list);
    return found;
}

static OSStatus input_callback(void * refCon, AudioUnitRenderActionFlags * flags, const AudioTimeStamp * timeStamp,
                               UInt32 bus, UInt32 frames, AudioBufferList * unused) {
    (void)unused;
    tCapture * capture = (tCapture *)refCon;

    if (frames > capture->scratchFrames) {
        atomic_fetch_add(&capture->overrunFrames, frames);
        return noErr;
    }
    capture->bufferList.mNumberBuffers              = 1;
    capture->bufferList.mBuffers[0].mNumberChannels = capture->channels;
    capture->bufferList.mBuffers[0].mDataByteSize   = frames * capture->channels * (UInt32)sizeof(float);
    capture->bufferList.mBuffers[0].mData           = capture->scratch;

    OSStatus   status  = AudioUnitRender(gUnit, flags, timeStamp, bus, frames, &capture->bufferList);

    if (status != noErr) {
        atomic_fetch_add(&capture->overrunFrames, frames);
        return noErr;   // keep the stream running; the count is reported at the end
    }
    size_t     written = atomic_load(&capture->writtenFrames);

    if ((written + frames) > capture->capacityFrames) {
        atomic_fetch_add(&capture->overrunFrames, frames);
        return noErr;
    }
    memcpy(capture->samples + (written * capture->channels), capture->scratch, frames * capture->channels * sizeof(float));
    atomic_store(&capture->writtenFrames, written + frames);
    return noErr;
}

static void put32(FILE * f, uint32_t v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

static void put16(FILE * f, uint16_t v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}

// 32-bit PCM, not 24 — not for the dynamic range (the interface has nowhere near it) but because a
// 4-byte sample is a width Python's `array` module can load in ONE call, so analyse_ir.py can slice
// a channel out of a 12 million sample capture instantly instead of calling int.from_bytes() per
// sample. At these sizes that is the difference between a minute and a moment, and it is what keeps
// the analyser dependency-free.
//
// Clamped rather than wrapped: a wrapped sample looks exactly like a transient, and this rig
// measures transients.
static bool write_wav32(const char * path, const float * samples, size_t frames, uint32_t channels, double rate) {
    FILE *   f          = fopen(path, "wb");

    if (f == NULL) {
        fprintf(stderr, "error: cannot write %s\n", path);
        return false;
    }
    uint32_t dataBytes  = (uint32_t)(frames * channels * 4);
    uint32_t byteRate   = (uint32_t)(rate * channels * 4);

    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + dataBytes);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);                        // PCM
    put16(f, (uint16_t)channels);
    put32(f, (uint32_t)rate);
    put32(f, byteRate);
    put16(f, (uint16_t)(channels * 4)); // block align
    put16(f, 32);
    fwrite("data", 1, 4, f);
    put32(f, dataBytes);

    for (size_t i = 0; i < (frames * channels); i++) {
        double  v = (double)samples[i];

        if (v > 1.0) {
            v = 1.0;
        }

        if (v < -1.0) {
            v = -1.0;
        }
        int32_t s = (int32_t)(v * 2147483520.0);   // just inside INT32_MAX, so +1.0 cannot wrap

        put32(f, (uint32_t)s);
    }
    fclose(f);
    return true;
}

int main(int argc, char ** argv) {
    const char * wanted  = NULL;
    const char * outPath = NULL;
    double       seconds = 10.0;
    double       askRate = 0.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) {
            list_devices();
            return 0;
        } else if ((strcmp(argv[i], "--device") == 0) && ((i + 1) < argc)) {
            wanted = argv[++i];
        } else if ((strcmp(argv[i], "--out") == 0) && ((i + 1) < argc)) {
            outPath = argv[++i];
        } else if ((strcmp(argv[i], "--seconds") == 0) && ((i + 1) < argc)) {
            seconds = atof(argv[++i]);
        } else if ((strcmp(argv[i], "--rate") == 0) && ((i + 1) < argc)) {
            askRate = atof(argv[++i]);
        } else {
            fprintf(stderr, "usage: %s --list\n       %s --device <name> --out <file.wav> [--seconds N] [--rate R]\n",
                    argv[0], argv[0]);
            return 2;
        }
    }

    if ((wanted == NULL) || (outPath == NULL) || (seconds <= 0.0)) {
        fprintf(stderr, "error: --device and --out are required\n");
        return 2;
    }
    AudioObjectID device = find_device(wanted);

    if (device == kAudioObjectUnknown) {
        fprintf(stderr, "error: no input device matching '%s'. Try --list.\n", wanted);
        return 1;
    }
    char *        name   = device_name(device);

    // Asking for a rate is a request, not a guarantee — the interface may be clocked externally or
    // held at a rate by another running app. Whatever it settles at is what gets reported and
    // written, so a mismatch is visible rather than silently scaling every measurement.
    if (askRate > 0.0) {
        AudioObjectPropertyAddress address = {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        Float64                    rate    = (Float64)askRate;

        AudioObjectSetPropertyData(device, &address, 0, NULL, sizeof(rate), &rate);
        usleep(200000);
    }
    AudioComponentDescription     description = {
        kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0
    };
    AudioComponent                component   = AudioComponentFindNext(NULL, &description);

    if (component == NULL) {
        fprintf(stderr, "error: no AUHAL component\n");
        return 1;
    }

    if (!ok(AudioComponentInstanceNew(component, &gUnit), "AudioComponentInstanceNew")) {
        return 1;
    }
    UInt32                        enable      = 1;

    if (!ok(AudioUnitSetProperty(gUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &enable, sizeof(enable)), "enable input")) {
        return 1;
    }
    enable = 0;

    if (!ok(AudioUnitSetProperty(gUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &enable, sizeof(enable)), "disable output")) {
        return 1;
    }

    if (!ok(AudioUnitSetProperty(gUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, sizeof(device)), "set device")) {
        return 1;
    }
    AudioStreamBasicDescription   deviceFormat = {0};
    UInt32                        size         = sizeof(deviceFormat);

    if (!ok(AudioUnitGetProperty(gUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 1, &deviceFormat, &size), "get device format")) {
        return 1;
    }
    uint32_t                      channels     = deviceFormat.mChannelsPerFrame;
    double                        rate         = deviceFormat.mSampleRate;

    // Interleaved float32, one buffer — the simplest thing the callback can memcpy in one go.
    AudioStreamBasicDescription   clientFormat = {
        .mSampleRate       = rate,
        .mFormatID         = kAudioFormatLinearPCM,
        .mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
        .mBytesPerPacket   = (UInt32)(channels * sizeof(float)),
        .mFramesPerPacket  = 1,
        .mBytesPerFrame    = (UInt32)(channels * sizeof(float)),
        .mChannelsPerFrame = channels,
        .mBitsPerChannel   = 32,
    };

    if (!ok(AudioUnitSetProperty(gUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &clientFormat, sizeof(clientFormat)), "set client format")) {
        return 1;
    }
    tCapture                      capture      = {0};

    capture.channels       = channels;
    capture.capacityFrames = (size_t)(seconds * rate) + (size_t)rate;   // a second of slack
    capture.samples        = calloc(capture.capacityFrames * channels, sizeof(float));
    capture.scratchFrames  = 65536;
    capture.scratch        = calloc(capture.scratchFrames * channels, sizeof(float));

    if ((capture.samples == NULL) || (capture.scratch == NULL)) {
        fprintf(stderr, "error: out of memory for %.1f s x %u ch\n", seconds, channels);
        return 1;
    }
    AURenderCallbackStruct        callback     = {input_callback, &capture};

    if (!ok(AudioUnitSetProperty(gUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &callback, sizeof(callback)), "set callback")) {
        return 1;
    }

    if (!ok(AudioUnitInitialize(gUnit), "AudioUnitInitialize")) {
        return 1;
    }

    if (!ok(AudioOutputUnitStart(gUnit), "AudioOutputUnitStart")) {
        return 1;
    }
    // The driver script watches for this line before it starts firing triggers, so it must be
    // flushed and it must mean "the stream is running".
    printf("capturing: %s, %u channels, %.0f Hz, %.1f s\n", (name != NULL) ? name : "?", channels, rate, seconds);
    fflush(stdout);

    size_t                        target       = (size_t)(seconds * rate);

    // A STALLED DEVICE MUST NOT BE AN INFINITE WAIT. Unplugging the interface — or swapping a USB
    // isolator into the chain — stops the callback dead, and a loop that only watches the frame count
    // then waits forever: a 123 second recording sat at 5 minutes and counting, holding up the rest of
    // a sweep, with nothing written and nothing said. So progress is what is waited on, not completion,
    // and a stall ends the recording with what it has plus a warning that says why.
    double                        stallLimit   = 2.0;
    size_t                        seen         = 0;
    struct timespec               lastProgress = {0};

    clock_gettime(CLOCK_MONOTONIC, &lastProgress);

    bool                          stalled      = false;

    while (atomic_load(&capture.writtenFrames) < target) {
        usleep(20000);

        size_t          now  = atomic_load(&capture.writtenFrames);
        struct timespec time = {0};

        clock_gettime(CLOCK_MONOTONIC, &time);

        if (now > seen) {
            seen         = now;
            lastProgress = time;
            continue;
        }
        double          idle = (double)(time.tv_sec - lastProgress.tv_sec)
                             + ((double)(time.tv_nsec - lastProgress.tv_nsec) / 1e9);

        if (idle > stallLimit) {
            stalled = true;
            break;
        }
    }
    AudioOutputUnitStop(gUnit);
    AudioUnitUninitialize(gUnit);
    AudioComponentInstanceDispose(gUnit);

    size_t                        frames       = atomic_load(&capture.writtenFrames);
    size_t                        lost         = atomic_load(&capture.overrunFrames);

    if (!write_wav32(outPath, capture.samples, frames, channels, rate)) {
        return 1;
    }
    printf("wrote %s: %zu frames (%.2f s), %u channels, %.0f Hz\n", outPath, frames, (double)frames / rate, channels, rate);

    if (stalled) {
        printf("WARNING: the device stopped delivering audio %.1f s before the end — recording cut short.\n"
               "         Check the interface is still connected and still at %.0f Hz.\n",
               (double)(target - frames) / rate, rate);
    }

    if (lost > 0) {
        printf("WARNING: %zu frames were dropped — timings in this file have a gap in them\n", lost);
    }
    free(capture.samples);
    free(capture.scratch);
    free(name);
    return (lost > 0) ? 1 : 0;
}
