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
// Notes: Docs/code-notes/vst3host.mm.md - "// notes §k" refers there.

// notes §1

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"   // kVstAudioEffectClass lives here, not in ivstcomponent.h
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/base/ibstream.h"

#include <map>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

// notes §2
DEF_CLASS_IID(IPlugFrame)

// The host side of IPlugFrame. A view calls resizeView() when it wants to change size; a host that
// does not implement this at all is a host the plug-in cannot resize itself in, which is a
// realistic thing to test against but not a useful default. Ours agrees to whatever is asked.
class HostFrame : public IPlugFrame {
public:
    NSWindow * window = nil;

    tresult PLUGIN_API queryInterface(const TUID iid, void ** obj) SMTG_OVERRIDE {
        if ((memcmp(iid, IPlugFrame::iid, sizeof(TUID)) == 0)
           || (memcmp(iid, FUnknown::iid, sizeof(TUID)) == 0)) {
            *obj = this;
            return kResultTrue;
        }
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef() SMTG_OVERRIDE {
        return 1;
    }

    uint32 PLUGIN_API release() SMTG_OVERRIDE {
        return 1;
    }

    tresult PLUGIN_API resizeView(IPlugView * view, ViewRect * rect) SMTG_OVERRIDE {
        if ((rect == nullptr) || (window == nil)) {
            return kResultFalse;
        }
        NSRect frame = [window frame];
        NSRect content = NSMakeRect(0, 0, rect->getWidth(), rect->getHeight());
        NSRect wanted  = [window frameRectForContentRect:content];

        frame.origin.y += (frame.size.height - wanted.size.height);
        frame.size      = wanted.size;
        [window setFrame:frame display:YES];

        if (view != nullptr) {
            view->onSize(rect);
        }
        return kResultTrue;
    }
};

// notes §3

class Attributes : public IAttributeList {
public:
    std::map<std::string, int64> ints;
    int32 rc = 1;

    tresult PLUGIN_API queryInterface(const TUID, void ** o) override { *o = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef(void) override { return (uint32)++rc; }
    uint32 PLUGIN_API release(void) override { return (uint32)--rc; }

    tresult PLUGIN_API setInt(AttrID id, int64 value) override { ints[id] = value; return kResultOk; }
    tresult PLUGIN_API getInt(AttrID id, int64 & value) override {
        auto it = ints.find(id);
        if (it == ints.end()) { return kResultFalse; }
        value = it->second;
        return kResultOk;
    }
    tresult PLUGIN_API setFloat(AttrID, double) override { return kResultFalse; }
    tresult PLUGIN_API getFloat(AttrID, double &) override { return kResultFalse; }
    tresult PLUGIN_API setString(AttrID, const TChar *) override { return kResultFalse; }
    tresult PLUGIN_API getString(AttrID, TChar *, uint32) override { return kResultFalse; }
    tresult PLUGIN_API setBinary(AttrID, const void *, uint32) override { return kResultFalse; }
    tresult PLUGIN_API getBinary(AttrID, const void *&, uint32 &) override { return kResultFalse; }
};

class Message : public IMessage {
public:
    std::string id;
    Attributes  attrs;
    int32       rc = 1;

    tresult PLUGIN_API queryInterface(const TUID, void ** o) override { *o = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef(void) override { return (uint32)++rc; }
    uint32 PLUGIN_API release(void) override {
        int32 c = --rc;
        if (c == 0) { delete this; return 0; }
        return (uint32)c;
    }

    FIDString PLUGIN_API getMessageID(void) override { return id.c_str(); }
    void PLUGIN_API setMessageID(FIDString newId) override { id = (newId != nullptr) ? newId : ""; }
    IAttributeList * PLUGIN_API getAttributes(void) override { return &attrs; }
};

class HostApp : public IHostApplication {
public:
    int32 rc = 1;

    tresult PLUGIN_API queryInterface(const TUID iid, void ** o) override {
        QUERY_INTERFACE(iid, o, FUnknown::iid, IHostApplication)
        QUERY_INTERFACE(iid, o, IHostApplication::iid, IHostApplication)
        *o = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef(void) override { return (uint32)++rc; }
    uint32 PLUGIN_API release(void) override { return (uint32)--rc; }

    tresult PLUGIN_API getName(String128 name) override { name[0] = 0; return kResultOk; }
    tresult PLUGIN_API createInstance(TUID cid, TUID, void ** obj) override {
        if (memcmp(cid, IMessage::iid.toTUID(), sizeof(TUID)) == 0) {
            *obj = (IMessage *)new Message();
            return kResultOk;
        }
        *obj = nullptr;
        return kResultFalse;
    }
};

// An IBStream over a std::string - a patch path going in as state, and state going across to the
// controller the way a host passes it.
class MemStream : public IBStream {
public:
    std::string buf;
    size_t      pos = 0;
    int32       rc  = 1;

    tresult PLUGIN_API queryInterface(const TUID, void ** o) override { *o = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef(void) override { return (uint32)++rc; }
    uint32 PLUGIN_API release(void) override { return (uint32)--rc; }

    tresult PLUGIN_API read(void * b, int32 n, int32 * got) override {
        size_t avail = buf.size() - pos;
        size_t take  = ((size_t)n < avail) ? (size_t)n : avail;

        memcpy(b, buf.data() + pos, take);
        pos += take;
        if (got != nullptr) { *got = (int32)take; }
        return kResultOk;
    }
    tresult PLUGIN_API write(void * b, int32 n, int32 * put) override {
        buf.append((const char *)b, (size_t)n);
        if (put != nullptr) { *put = n; }
        return kResultOk;
    }
    tresult PLUGIN_API seek(int64 p, int32, int64 * r) override {
        pos = (size_t)p;
        if (r != nullptr) { *r = p; }
        return kResultOk;
    }
    tresult PLUGIN_API tell(int64 * p) override {
        if (p != nullptr) { *p = (int64)pos; }
        return kResultOk;
    }
};

// One note-on at a chosen offset, for --offset-test.
class OneNote : public IEventList {
public:
    Event event;
    int32 rc = 1;

    OneNote(int16 pitch, int32 offset) {
        memset(&event, 0, sizeof(event));
        event.sampleOffset    = offset;
        event.type            = Event::kNoteOnEvent;
        event.noteOn.pitch    = pitch;
        event.noteOn.velocity = 0.8f;
        event.noteOn.noteId   = -1;
    }
    tresult PLUGIN_API queryInterface(const TUID, void ** o) override { *o = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef(void) override { return (uint32)++rc; }
    uint32 PLUGIN_API release(void) override { return (uint32)--rc; }
    int32 PLUGIN_API getEventCount(void) override { return 1; }
    tresult PLUGIN_API getEvent(int32 index, Event & e) override {
        if (index != 0) { return kResultFalse; }
        e = event;
        return kResultOk;
    }
    tresult PLUGIN_API addEvent(Event &) override { return kResultFalse; }
};

struct Instance {
    IComponent *       component  = nullptr;
    IEditController *  controller = nullptr;
    IConnectionPoint * compPoint  = nullptr;
    IConnectionPoint * ctrlPoint  = nullptr;
    IPlugView *        view       = nullptr;
    ViewRect           size       = {};
    HostFrame          frame;
    NSWindow *         window     = nil;
};

static HostApp gHostApp;

static void usage(void) {
    fprintf(stderr,
            "usage: vst3host <plugin.vst3> [--seconds N] [--shot out.png] [--size WxH]\n"
            "                [--instances N] [--patch PATH]... [--offset-test N] [--reopen N] [--dump-state]\n"
            "  --seconds N    quit after N seconds (default: run until the windows are closed)\n"
            "  --shot PATH    screenshot each window just before quitting; with more than one\n"
            "                 instance, instance i goes to PATH with -i before the extension\n"
            "  --size WxH     ask each view for this size in points before attaching\n"
            "  --instances N  load N instances into this one process, each with its own window\n"
            "  --patch PATH   hand the next instance this state (a raw patch path, for G2 Alike)\n"
            "  --offset-test N render a note placed at sample N of a block and report where the sound\n"
            "                 starts - use a patch that is silent without a note, and compare with N=0\n"
            "  --reopen N     close and re-create the first editor N times, as a host does when the\n"
            "                 user opens and shuts it - for leaks and exhausted window slots\n"
            "  --dump-state   print the state each instance saves, after --patch has been applied\n");
}

// Renders a few silent blocks, then one with a note-on at `offset`, and returns the first frame of
// that block whose output is audible - or -1 if none was.
static int offset_probe(IComponent * component, int32 offset) {
    IAudioProcessor * proc = nullptr;

    if ((component->queryInterface(IAudioProcessor::iid, (void **)&proc) != kResultTrue) || (proc == nullptr)) {
        return -2;
    }
    ProcessSetup setup = {kRealtime, kSample32, 512, 48000.0};

    proc->setupProcessing(setup);
    component->setActive(true);
    proc->setProcessing(true);

    float            left[512]  = {0};
    float            right[512] = {0};
    float *          chans[2]   = {left, right};
    AudioBusBuffers  outBus     = {};
    ProcessData      data       = {};
    OneNote          note(60, offset);
    int              first      = -1;

    outBus.numChannels       = 2;
    outBus.channelBuffers32  = (Sample32 **)chans;
    data.processMode         = kRealtime;
    data.symbolicSampleSize  = kSample32;
    data.numSamples          = 512;
    data.numOutputs          = 1;
    data.outputs             = &outBus;

    for (int b = 0; b < 4; b++) {
        data.inputEvents = (b == 3) ? &note : nullptr;
        proc->process(data);
    }

    for (int i = 0; i < 512; i++) {
        if (fabsf(left[i]) > 1.0e-4f) {
            first = i;
            break;
        }
    }
    proc->setProcessing(false);
    component->setActive(false);
    proc->release();
    return first;
}

int main(int argc, const char ** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    const char *               bundlePath = argv[1];
    const char *               shotPath   = nullptr;
    double                     seconds    = 0.0;
    int                        wantW      = 0;
    int                        wantH      = 0;
    int                        count      = 1;
    int                        offsetTest = -1;
    int                        reopen     = 0;
    bool                       dumpState  = false;
    std::vector<std::string>   patches;

    for (int i = 2; i < argc; i++) {
        if ((strcmp(argv[i], "--seconds") == 0) && ((i + 1) < argc)) {
            seconds = atof(argv[++i]);
        } else if ((strcmp(argv[i], "--shot") == 0) && ((i + 1) < argc)) {
            shotPath = argv[++i];

            if (seconds <= 0.0) {
                seconds = 3.0;
            }
        } else if ((strcmp(argv[i], "--size") == 0) && ((i + 1) < argc)) {
            sscanf(argv[++i], "%dx%d", &wantW, &wantH);
        } else if ((strcmp(argv[i], "--instances") == 0) && ((i + 1) < argc)) {
            count = atoi(argv[++i]);
            count = (count < 1) ? 1 : ((count > 8) ? 8 : count);
        } else if ((strcmp(argv[i], "--patch") == 0) && ((i + 1) < argc)) {
            patches.push_back(argv[++i]);
        } else if (strcmp(argv[i], "--dump-state") == 0) {
            dumpState = true;
        } else if ((strcmp(argv[i], "--reopen") == 0) && ((i + 1) < argc)) {
            reopen = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--offset-test") == 0) && ((i + 1) < argc)) {
            offsetTest = atoi(argv[++i]);
        } else {
            usage();
            return 2;
        }
    }
    @autoreleasepool {
        // A .vst3 is a bundle; the binary is Contents/MacOS/<name>. dlopen rather than CFBundle: the
        // smallest thing that works, and it keeps the failure messages legible.
        NSString * base   = [NSString stringWithUTF8String:bundlePath];
        NSString * name   = [[base lastPathComponent] stringByDeletingPathExtension];
        NSString * binary = [NSString stringWithFormat:@"%@/Contents/MacOS/%@", base, name];
        void *     handle = dlopen([binary UTF8String], RTLD_NOW | RTLD_LOCAL);

        if (handle == nullptr) {
            fprintf(stderr, "dlopen failed: %s\n", dlerror());
            return 1;
        }
        typedef bool (*BundleEntryFn)(CFBundleRef);
        BundleEntryFn entry = (BundleEntryFn)dlsym(handle, "bundleEntry");

        if (entry != nullptr) {
            entry(nullptr);
        }
        typedef IPluginFactory * (*GetFactoryFn)();
        GetFactoryFn     getFactory = (GetFactoryFn)dlsym(handle, "GetPluginFactory");
        IPluginFactory * factory    = (getFactory != nullptr) ? getFactory() : nullptr;

        if (factory == nullptr) {
            fprintf(stderr, "no plug-in factory\n");
            return 1;
        }
        // IPluginFactory2 SPECIFICALLY, because its absence is what a host rejects an instrument for
        // and an earlier harness could not see. Reported, not required.
        IPluginFactory2 * factory2 = nullptr;

        if (factory->queryInterface(IPluginFactory2::iid, (void **)&factory2) != kResultTrue) {
            factory2 = nullptr;
        }
        printf("factory: %d classes, IPluginFactory2 %s\n",
               factory->countClasses(), (factory2 != nullptr) ? "YES" : "NO - a host may refuse this");

        TUID componentCid;
        TUID fallbackControllerCid;
        bool haveComponent  = false;
        bool haveController = false;

        for (int32 i = 0; i < factory->countClasses(); i++) {
            PClassInfo   info  = {};
            PClassInfo2  info2 = {};
            const char * sub   = "";

            if (factory->getClassInfo(i, &info) != kResultTrue) {
                continue;
            }

            if ((factory2 != nullptr) && (factory2->getClassInfo2(i, &info2) == kResultTrue)) {
                sub = info2.subCategories;
            }
            printf("  [%d] %-28s category=%-24s %s\n", i, info.name, info.category, sub);

            if ((strcmp(info.category, kVstAudioEffectClass) == 0) && !haveComponent) {
                memcpy(componentCid, info.cid, sizeof(TUID));
                haveComponent = true;
            } else if ((strcmp(info.category, kVstComponentControllerClass) == 0) && !haveController) {
                memcpy(fallbackControllerCid, info.cid, sizeof(TUID));
                haveController = true;
            }
        }

        if (!haveComponent) {
            fprintf(stderr, "no class registered under %s\n", kVstAudioEffectClass);
            return 1;
        }
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        std::vector<Instance> instances((size_t)count);

        for (int n = 0; n < count; n++) {
            Instance & in = instances[(size_t)n];

            // THE COMPONENT FIRST, as a real host does: G2 Alike loads its patch in initialize(), and
            // a harness that went straight to the controller drew an empty database - a correct-looking
            // window with no modules in it.
            if ((factory->createInstance(componentCid, IComponent::iid, (void **)&in.component) != kResultTrue)
               || (in.component == nullptr)) {
                fprintf(stderr, "instance %d: the component could not be created\n", n + 1);
                return 1;
            }
            in.component->initialize(&gHostApp);

            if (!patches.empty()) {
                MemStream state;

                state.buf = patches[(size_t)n % patches.size()];
                in.component->setState(&state);
            }

            // THE CONTROLLER THE COMPONENT NAMES, not whichever controller class the factory listed
            // last - with two variants in one factory that paired an effect's processor with an
            // instrument's controller (GenBridge's copy of this harness, 2026-09-11).
            TUID controllerCid;

            if (in.component->getControllerClassId(controllerCid) != kResultTrue) {
                if (!haveController) {
                    fprintf(stderr, "instance %d: no controller class\n", n + 1);
                    return 1;
                }
                memcpy(controllerCid, fallbackControllerCid, sizeof(TUID));
            }

            if ((factory->createInstance(controllerCid, IEditController::iid, (void **)&in.controller) != kResultTrue)
               || (in.controller == nullptr)) {
                fprintf(stderr, "instance %d: the controller could not be created\n", n + 1);
                return 1;
            }
            in.controller->initialize(&gHostApp);

            // CONNECTED, as a host connects them, which is how the controller learns which
            // processor - which instance - it is the editor for.
            in.component->queryInterface(IConnectionPoint::iid, (void **)&in.compPoint);
            in.controller->queryInterface(IConnectionPoint::iid, (void **)&in.ctrlPoint);

            if ((in.compPoint != nullptr) && (in.ctrlPoint != nullptr)) {
                in.compPoint->connect(in.ctrlPoint);
                in.ctrlPoint->connect(in.compPoint);
            }

            // And the component's state across to the controller, as a host passes it.
            {
                MemStream state;

                if (in.component->getState(&state) == kResultTrue) {
                    if (dumpState) {
                        // Printable as it stands; anything else - a wrapper header, a count, a
                        // binary value - as a dot, so the plug-in's own text can be read.
                        printf("instance %d: state, %zu bytes:\n", n + 1, state.buf.size());

                        for (unsigned char c : state.buf) {
                            putchar(((c >= 32) && (c < 127)) || (c == '\n') ? (int)c : '.');
                        }
                        putchar('\n');
                    }
                    state.pos = 0;
                    in.controller->setComponentState(&state);
                }
            }
            printf("instance %d: component and controller up, %s, %d parameters\n", n + 1,
                   ((in.compPoint != nullptr) && (in.ctrlPoint != nullptr)) ? "connected" : "NOT connected",
                   in.controller->getParameterCount());

            // ONE PROBE PER INSTANCE: a note left held by an earlier probe would still be sounding.
            if (offsetTest >= 0) {
                printf("instance %d: a note at sample %d is heard from frame %d\n",
                       n + 1, offsetTest, offset_probe(in.component, (int32)offsetTest));
            }
            in.view = in.controller->createView(ViewType::kEditor);

            if ((in.view == nullptr) || (in.view->isPlatformTypeSupported(kPlatformTypeNSView) != kResultTrue)) {
                fprintf(stderr, "instance %d: no NSView editor\n", n + 1);
                return 1;
            }
            ViewRect size = {};

            if ((wantW > 0) && (wantH > 0)) {
                size.right  = wantW;
                size.bottom = wantH;
                in.view->onSize(&size);
            } else if (in.view->getSize(&size) != kResultTrue) {
                size.right  = 1000;
                size.bottom = 700;
            }
            printf("instance %d: view size %dx%d\n", n + 1, size.getWidth(), size.getHeight());
            in.size = size;

            // Side by side, so each window can be seen - and captured - without another on top of it.
            NSRect rect = NSMakeRect(40 + (n * (size.getWidth() + 24)), 200, size.getWidth(), size.getHeight());

            in.window = [[NSWindow alloc] initWithContentRect:rect
                                                    styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                               | NSWindowStyleMaskResizable)
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
            [in.window setTitle:[NSString stringWithFormat:@"vst3host — %@ [%d]", name, n + 1]];

            // THE POINT OF THE HARNESS: the view goes in as a SUBVIEW of a window the host owns, which
            // is the relationship a real host has with it.
            in.frame.window = in.window;
            in.view->setFrame(&in.frame);

            if (in.view->attached((__bridge void *)[in.window contentView], kPlatformTypeNSView) != kResultTrue) {
                fprintf(stderr, "instance %d: attached() refused\n", n + 1);
                return 1;
            }
            [in.window makeKeyAndOrderFront:nil];
        }
        [NSApp activateIgnoringOtherApps:YES];

        // notes §4
        if (reopen > 0) {
            Instance *  first = &instances[0];
            __block int left  = reopen;

            [NSTimer scheduledTimerWithTimeInterval:0.3 repeats:YES block:^(NSTimer * t) {
                if (left <= 0) {
                    [t invalidate];
                    return;
                }
                first->view->removed();
                first->view->release();
                first->view = first->controller->createView(ViewType::kEditor);

                if (first->view == nullptr) {
                    fprintf(stderr, "reopen: createView returned null after %d\n", reopen - left);
                    [t invalidate];
                    return;
                }
                first->view->onSize(&first->size);
                first->view->setFrame(&first->frame);

                if (first->view->attached((__bridge void *)[first->window contentView], kPlatformTypeNSView) != kResultTrue) {
                    fprintf(stderr, "reopen: attached() refused after %d\n", reopen - left);
                }
                left--;

                if (left == 0) {
                    printf("reopen: editor closed and re-created %d times\n", reopen);
                }
            }];
        }

        if (seconds > 0.0) {
            // STOPPING NEEDS AN EVENT TO LAND ON. -[NSApplication stop:] only sets a flag that is acted
            // on after the current event, so with the pointer still, -run would stay blocked; the
            // application-defined event posted straight after is that event.
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

            if (shotPath != nullptr) {
                // Just before the stop, so every window has had the full time to draw. By WINDOW
                // (screencapture -l), not by rectangle, so an overlapping window cannot get in.
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)((seconds - 0.25) * NSEC_PER_SEC)),
                               dispatch_get_main_queue(), ^{
                    for (int n = 0; n < count; n++) {
                        std::string path = shotPath;

                        if (count > 1) {
                            size_t dot = path.rfind('.');
                            std::string suffix = "-" + std::to_string(n + 1);

                            path = (dot == std::string::npos) ? (path + suffix) : (path.substr(0, dot) + suffix + path.substr(dot));
                        }
                        NSString * cmd = [NSString stringWithFormat:@"/usr/sbin/screencapture -x -o -l %ld '%s'",
                                          (long)[instances[(size_t)n].window windowNumber], path.c_str()];

                        system([cmd UTF8String]);
                    }
                });
            }
        }
        [NSApp run];

        for (Instance & in : instances) {
            in.view->removed();
            in.view->release();

            if ((in.compPoint != nullptr) && (in.ctrlPoint != nullptr)) {
                in.compPoint->disconnect(in.ctrlPoint);
                in.ctrlPoint->disconnect(in.compPoint);
            }

            if (in.compPoint != nullptr) {
                in.compPoint->release();
            }

            if (in.ctrlPoint != nullptr) {
                in.ctrlPoint->release();
            }
            in.controller->terminate();
            in.controller->release();
            in.component->terminate();
            in.component->release();
        }
    }
    return 0;
}
