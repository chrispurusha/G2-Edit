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

// ── A minimal Audio Unit host, for looking at our own editor ────────────────────────────────────
//
// The counterpart to vst3host.mm, and it exists for the one thing auval does not do: auval
// instantiates the plug-in, renders it and sends it MIDI, but it never opens the Cocoa editor. That
// leaves the whole kAudioUnitProperty_CocoaUI path - our own bundle found by identifier, the view
// class loaded out of it by name, the view put into a window somebody else owns - untested by the
// one tool that otherwise says "AU VALIDATION SUCCEEDED".
//
// It also renders a few blocks with a note held and reports the peak, so "does it make a sound from
// the patch it loaded" is answered in the same command.
//
// WHAT IT PROVES, AND WHAT IT DOES NOT. It proves the component is registered, instantiates, draws
// its editor and produces audio. It does NOT prove a real host will accept it - the same warning
// vst3host.mm carries. auval is the other half of the answer, and a real host is the last word.
//
// THE COMPONENT MUST BE INSTALLED. Unlike a .vst3, which is a path this could dlopen, an Audio Unit
// is found through the system's component registry - so it has to be in
// /Library/Audio/Plug-Ins/Components (or the per-user one) and AudioComponentRegistrar has to have
// noticed it. After a rebuild: killall -9 AudioComponentRegistrar.
//
// Build: ./do-auhost    (see tools/README.md)

#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>
#import <AudioUnit/AUCocoaUIView.h>

#include <stdio.h>
#include <string.h>

static OSType four_cc(const char * s) {
    return ((OSType)(unsigned char)s[0] << 24) | ((OSType)(unsigned char)s[1] << 16) |
           ((OSType)(unsigned char)s[2] << 8)  |  (OSType)(unsigned char)s[3];
}

// A few blocks with a note held, to answer "is it making a sound" in the same run. De-interleaved
// float, which is the only format the wrapper accepts - see synthlibPluginAu.c.
static float render_peak(AudioUnit au, UInt32 blocks, UInt32 frames) {
    const UInt32 channels = 2;
    float *      samples  = (float *)calloc((size_t)frames * channels, sizeof(float));

    if (samples == NULL) {
        return -1.0f;
    }
    // An AudioBufferList with two buffers needs one AudioBuffer more than the struct declares.
    size_t            listBytes = sizeof(AudioBufferList) + sizeof(AudioBuffer);
    AudioBufferList * list      = (AudioBufferList *)calloc(1, listBytes);

    if (list == NULL) {
        free(samples);
        return -1.0f;
    }
    list->mNumberBuffers = channels;

    float peak = 0.0f;

    for (UInt32 b = 0; b < blocks; b++) {
        AudioTimeStamp             ts    = {0};
        AudioUnitRenderActionFlags flags = 0;

        ts.mSampleTime = (Float64)(b * frames);
        ts.mFlags      = kAudioTimeStampSampleTimeValid;

        for (UInt32 c = 0; c < channels; c++) {
            list->mBuffers[c].mNumberChannels = 1;
            list->mBuffers[c].mDataByteSize   = frames * (UInt32)sizeof(float);
            list->mBuffers[c].mData           = samples + ((size_t)c * frames);
        }
        OSStatus err = AudioUnitRender(au, &flags, &ts, 0, frames, list);

        if (err != noErr) {
            fprintf(stderr, "AudioUnitRender failed: %d\n", (int)err);
            break;
        }

        for (UInt32 i = 0; i < (frames * channels); i++) {
            float v = fabsf(samples[i]);

            if (v > peak) {
                peak = v;
            }
        }
    }
    free(list);
    free(samples);
    return peak;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        const char * type         = "aumu";
        const char * subtype      = "G2al";
        const char * manufacturer = "CPur";
        double       seconds      = 0.0;
        const char * shotPath     = NULL;

        for (int i = 1; i < argc; i++) {
            if ((strcmp(argv[i], "--seconds") == 0) && ((i + 1) < argc)) {
                seconds = atof(argv[++i]);
            } else if ((strcmp(argv[i], "--shot") == 0) && ((i + 1) < argc)) {
                shotPath = argv[++i];

                if (seconds <= 0.0) {
                    seconds = 3.0;
                }
            } else if ((strcmp(argv[i], "--component") == 0) && ((i + 3) < argc)) {
                type         = argv[++i];
                subtype      = argv[++i];
                manufacturer = argv[++i];
            } else {
                fprintf(stderr,
                        "usage: auhost [--component TYPE SUBTYPE MANU] [--seconds N] [--shot out.png]\n"
                        "  defaults to aumu G2al CPur, which is \"G2 Alike\"\n"
                        "  --seconds N   hold the window open for N seconds, then quit\n"
                        "  --shot PATH   screenshot the window just before quitting; implies --seconds\n");
                return 2;
            }
        }
        AudioComponentDescription want = {0};

        want.componentType         = four_cc(type);
        want.componentSubType      = four_cc(subtype);
        want.componentManufacturer = four_cc(manufacturer);

        AudioComponent comp = AudioComponentFindNext(NULL, &want);

        if (comp == NULL) {
            fprintf(stderr, "no such component: %s %s %s\n", type, subtype, manufacturer);
            fprintf(stderr, "  is it installed in /Library/Audio/Plug-Ins/Components?\n");
            fprintf(stderr, "  after a rebuild: killall -9 AudioComponentRegistrar\n");
            return 1;
        }
        CFStringRef nameRef = NULL;

        AudioComponentCopyName(comp, &nameRef);
        printf("component: %s\n", (nameRef != NULL)
               ? [(__bridge NSString *)nameRef UTF8String] : "(unnamed)");

        AudioUnit au  = NULL;
        OSStatus  err = AudioComponentInstanceNew(comp, &au);

        if ((err != noErr) || (au == NULL)) {
            fprintf(stderr, "AudioComponentInstanceNew failed: %d\n", (int)err);
            return 1;
        }
        UInt32 paramCount = 0;

        err = AudioUnitGetPropertyInfo(au, kAudioUnitProperty_ParameterList,
                                       kAudioUnitScope_Global, 0, &paramCount, NULL);
        printf("parameters: %u\n", (unsigned)(paramCount / sizeof(AudioUnitParameterID)));

        err = AudioUnitInitialize(au);

        if (err != noErr) {
            fprintf(stderr, "AudioUnitInitialize failed: %d\n", (int)err);
            return 1;
        }

        // A note, then a few blocks. Middle C at full velocity, which is what the standalone editor's
        // virtual keyboard sends.
        MusicDeviceMIDIEvent(au, 0x90, 60, 100, 0);

        float peak = render_peak(au, 16, 512);

        printf("render peak: %.4f%s\n", peak,
               (peak > 0.0001f) ? "" : "   <-- SILENT (patch loaded? modules supported?)");
        MusicDeviceMIDIEvent(au, 0x80, 60, 0, 0);

        // ---- the editor, which is what this harness is really for ----------------------------
        AudioUnitCocoaViewInfo viewInfo = {0};
        UInt32                 infoSize = sizeof(viewInfo);

        err = AudioUnitGetProperty(au, kAudioUnitProperty_CocoaUI, kAudioUnitScope_Global, 0,
                                   &viewInfo, &infoSize);

        if (err != noErr) {
            fprintf(stderr, "no Cocoa UI: kAudioUnitProperty_CocoaUI returned %d\n", (int)err);
            return 1;
        }
        NSBundle * viewBundle = [NSBundle bundleWithURL:(__bridge NSURL *)viewInfo.mCocoaAUViewBundleLocation];

        if (viewBundle == nil) {
            fprintf(stderr, "could not open the view bundle the plug-in named\n");
            return 1;
        }
        NSString * className = (__bridge NSString *)viewInfo.mCocoaAUViewClass[0];

        printf("editor class: %s\n", [className UTF8String]);

        // LOADED OUT OF THE PLUG-IN'S OWN BUNDLE BY NAME, exactly as a real host does it. This is
        // the step that catches a class name the wrapper reported but the binary does not define.
        Class viewClass = [viewBundle classNamed:className];

        if (viewClass == nil) {
            fprintf(stderr, "the view bundle does not define %s\n", [className UTF8String]);
            return 1;
        }
        id<AUCocoaUIBase> factory = [[viewClass alloc] init];
        NSView *          editor  = [factory uiViewForAudioUnit:au withSize:NSMakeSize(0.0, 0.0)];

        if (editor == nil) {
            fprintf(stderr, "uiViewForAudioUnit: returned nil\n");
            return 1;
        }
        NSSize size = [editor frame].size;

        printf("view size: %dx%d\n", (int)size.width, (int)size.height);

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSWindow * window = [[NSWindow alloc]
                             initWithContentRect:NSMakeRect(200, 200, size.width, size.height)
                                       styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                  | NSWindowStyleMaskResizable)
                                         backing:NSBackingStoreBuffered
                                           defer:NO];

        [window setTitle:@"auhost"];

        // THE POINT OF THE HARNESS: the view goes in as a SUBVIEW of a window the host owns, which
        // is the relationship a real host has with it.
        [[window contentView] addSubview:editor];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        if (seconds > 0.0) {
            // STOPPING NEEDS AN EVENT TO LAND ON - the same trap vst3host.mm documents at length.
            // -[NSApplication stop:] only sets a flag, acted on once the current event finishes
            // being dispatched, so with the pointer sitting still -run stays blocked. The dummy
            // event is what releases it.
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                           dispatch_get_main_queue(), ^{
                [NSApp stop:nil];
                [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                                    location:NSZeroPoint
                                               modifierFlags:0
                                                   timestamp:0
                                                windowNumber:0
                                                     context:nil
                                                     subtype:0
                                                       data1:0
                                                       data2:0]
                         atStart:YES];
            });

            if (shotPath != NULL) {
                // Just before the stop, so the window has had the full time to draw.
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                             (int64_t)((seconds - 0.25) * NSEC_PER_SEC)),
                               dispatch_get_main_queue(), ^{
                    NSRect     f   = [window frame];
                    NSRect     c   = [window contentRectForFrameRect:f];
                    CGFloat    top = NSMaxY([[NSScreen mainScreen] frame]) - NSMaxY(c);
                    NSString * cmd = [NSString stringWithFormat:
                                      @"/usr/sbin/screencapture -x -R %d,%d,%d,%d '%s'",
                                      (int)c.origin.x, (int)top, (int)c.size.width, (int)c.size.height,
                                      shotPath];

                    system([cmd UTF8String]);
                });
            }
        }
        [NSApp run];

        [editor removeFromSuperview];
        AudioUnitUninitialize(au);
        AudioComponentInstanceDispose(au);
    }
    return 0;
}
