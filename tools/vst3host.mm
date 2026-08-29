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

// ── A minimal VST3 host, for looking at our own editor ──────────────────────────────────────────
//
// Loads a .vst3, instantiates the controller, asks it for its editor view, and puts that view in a
// window it owns — which is the one relationship with a plug-in view that matters and the one that
// cannot be tested any other way. Optionally screenshots the window and exits, so a rendering
// change in the plug-in can be diffed the same way one in the application is.
//
// THIS EXISTED TWICE BEFORE AND WAS LOST TWICE, because both times it was written into a scratchpad
// rather than the repository (see todo.txt, "the hand-written test host in the scratchpad"). It is
// here now for that reason as much as any other.
//
// WHAT IT PROVES, AND WHAT IT DOES NOT. It proves the plug-in loads, instantiates, and that its
// editor draws. It does NOT prove a host will accept it: an earlier version of this harness asked
// only for IPluginFactory and so never noticed that IPluginFactory2 was absent — which is exactly
// what Ableton rejected the plug-in for. It asks for IPluginFactory2 now and reports what it finds,
// but the general warning stands. Ableton's own ~/Library/Preferences/Ableton/Live */Log.txt names
// a rejection cause precisely and remains the last word.
//
// Build: ./do-vst3host    (see tools/README.md)

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

using namespace Steinberg;

// IPlugFrame's IID is not instantiated by any of the SDK's *iids.cpp files, because those cover
// what a PLUG-IN implements and IPlugFrame is the one interface the HOST implements. Defining it
// here is the documented way round that and is why this file, not the build script, ends up owning
// it.
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

static void usage(void) {
    fprintf(stderr,
            "usage: vst3host <plugin.vst3> [--seconds N] [--shot out.png] [--size WxH]\n"
            "  --seconds N   quit after N seconds (default: run until the window is closed)\n"
            "  --shot PATH   screenshot the window just before quitting; implies --seconds\n"
            "  --size WxH    ask the view for this size in points before attaching\n");
}

int main(int argc, const char ** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    const char * bundlePath = argv[1];
    const char * shotPath   = nullptr;
    double       seconds    = 0.0;
    int          wantW      = 0;
    int          wantH      = 0;

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
        } else {
            usage();
            return 2;
        }
    }
    @autoreleasepool {
        // A .vst3 is a bundle; the binary is Contents/MacOS/<name>. Loading it with dlopen rather
        // than CFBundle is deliberate — it is the smallest thing that works and it keeps the
        // failure messages legible.
        NSString * base   = [NSString stringWithUTF8String:bundlePath];
        NSString * name   = [[base lastPathComponent] stringByDeletingPathExtension];
        NSString * binary = [NSString stringWithFormat:@"%@/Contents/MacOS/%@", base, name];

        void *     handle = dlopen([binary UTF8String], RTLD_NOW | RTLD_LOCAL);

        if (handle == nullptr) {
            fprintf(stderr, "dlopen failed: %s\n", dlerror());
            return 1;
        }
        // macOS bundles are expected to export bundleEntry/bundleExit; call them if present.
        typedef bool (*BundleEntryFn)(CFBundleRef);
        BundleEntryFn entry = (BundleEntryFn)dlsym(handle, "bundleEntry");

        if (entry != nullptr) {
            entry(nullptr);
        }
        typedef IPluginFactory * (*GetFactoryFn)();
        GetFactoryFn getFactory = (GetFactoryFn)dlsym(handle, "GetPluginFactory");

        if (getFactory == nullptr) {
            fprintf(stderr, "no GetPluginFactory export\n");
            return 1;
        }
        IPluginFactory * factory = getFactory();

        if (factory == nullptr) {
            fprintf(stderr, "GetPluginFactory returned null\n");
            return 1;
        }
        // IPluginFactory2 SPECIFICALLY, because its absence is what a host rejects an instrument
        // for and the previous harness could not see. Reported, not required.
        IPluginFactory2 * factory2 = nullptr;

        if (factory->queryInterface(IPluginFactory2::iid, (void **)&factory2) != kResultTrue) {
            factory2 = nullptr;
        }
        printf("factory: %d classes, IPluginFactory2 %s\n",
               factory->countClasses(), (factory2 != nullptr) ? "YES" : "NO — a host may refuse this");

        TUID controllerCid;
        TUID componentCid;
        bool haveController = false;
        bool haveComponent  = false;

        for (int32 i = 0; i < factory->countClasses(); i++) {
            PClassInfo info = {};

            if (factory->getClassInfo(i, &info) != kResultTrue) {
                continue;
            }
            const char * sub = "";
            PClassInfo2  info2 = {};

            if ((factory2 != nullptr) && (factory2->getClassInfo2(i, &info2) == kResultTrue)) {
                sub = info2.subCategories;
            }
            printf("  [%d] %-28s category=%-24s %s\n", i, info.name, info.category, sub);

            if (strcmp(info.category, kVstComponentControllerClass) == 0) {
                memcpy(controllerCid, info.cid, sizeof(TUID));
                haveController = true;
            } else if (strcmp(info.category, kVstAudioEffectClass) == 0) {
                memcpy(componentCid, info.cid, sizeof(TUID));
                haveComponent = true;
            }
        }

        // THE COMPONENT FIRST, and it matters more than it looks. A real host creates the component,
        // initialises it, and only then goes to the class its getControllerClassId() names. Ours
        // loads the patch in IComponent::initialize(), so a harness that skipped straight to the
        // controller got an editor drawing an EMPTY DATABASE — a correct-looking window with no
        // modules in it, which is a very convincing way to not notice that nothing was loaded.
        Vst::IComponent * component = nullptr;

        if (haveComponent) {
            if ((factory->createInstance(componentCid, Vst::IComponent::iid, (void **)&component) == kResultTrue)
               && (component != nullptr)) {
                component->initialize(nullptr);
                printf("component: initialised\n");
            } else {
                printf("component: could not be created (the editor will draw an empty patch)\n");
            }
        }

        if (!haveController) {
            fprintf(stderr, "no class registered under %s — a host would find no editor\n",
                    kVstComponentControllerClass);
            return 1;
        }
        Vst::IEditController * controller = nullptr;

        if ((factory->createInstance(controllerCid, Vst::IEditController::iid, (void **)&controller) != kResultTrue)
           || (controller == nullptr)) {
            fprintf(stderr, "could not create the controller\n");
            return 1;
        }
        controller->initialize(nullptr);
        printf("controller: %d parameters\n", controller->getParameterCount());

        IPlugView * view = controller->createView(Vst::ViewType::kEditor);

        if (view == nullptr) {
            fprintf(stderr, "createView returned null — no editor\n");
            return 1;
        }

        if (view->isPlatformTypeSupported(kPlatformTypeNSView) != kResultTrue) {
            fprintf(stderr, "the view does not support NSView\n");
            return 1;
        }
        ViewRect size = {};

        if ((wantW > 0) && (wantH > 0)) {
            size.right  = wantW;
            size.bottom = wantH;
            view->onSize(&size);
        } else if (view->getSize(&size) != kResultTrue) {
            fprintf(stderr, "getSize failed; falling back to 1000x700\n");
            size.right  = 1000;
            size.bottom = 700;
        }
        printf("view size: %dx%d\n", size.getWidth(), size.getHeight());

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect     rect   = NSMakeRect(200, 200, size.getWidth(), size.getHeight());
        NSWindow * window = [[NSWindow alloc]
                             initWithContentRect:rect
                                       styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                  | NSWindowStyleMaskResizable)
                                         backing:NSBackingStoreBuffered
                                           defer:NO];

        [window setTitle:[NSString stringWithFormat:@"vst3host — %@", name]];

        // THE POINT OF THE HARNESS: the view goes in as a SUBVIEW of a window the host owns,
        // which is the relationship a real host has with it, and the one that catches the
        // difference between a view that draws and a view that only draws when it owns its window.
        HostFrame frame;

        frame.window = window;
        view->setFrame(&frame);

        NSView * container = [window contentView];

        if (view->attached((__bridge void *)container, kPlatformTypeNSView) != kResultTrue) {
            fprintf(stderr, "attached() refused\n");
            return 1;
        }
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        if (seconds > 0.0) {
            // STOPPING NEEDS AN EVENT TO LAND ON, which is the whole of a bug that made this
            // window refuse to close until the mouse was moved.
            //
            // -[NSApplication stop:] does not end the run loop. It sets a flag that is only acted
            // on once the CURRENT event finishes being dispatched — so with the pointer sitting
            // still, -run stays blocked in nextEventMatchingMask: and the flag is never reached.
            // Any stray event releases it, which is why moving the mouse appeared to "let" the
            // window close.
            //
            // Posting a dummy application-defined event immediately afterwards gives the run loop
            // the event it is waiting for, and -run returns at once whether or not anything else
            // is happening.
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
                // Just before the stop, so the window has had the full time to draw.
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                             (int64_t)((seconds - 0.25) * NSEC_PER_SEC)),
                               dispatch_get_main_queue(), ^{
                    NSRect  f   = [window frame];
                    NSRect  c   = [window contentRectForFrameRect:f];
                    CGFloat top = NSMaxY([[NSScreen mainScreen] frame]) - NSMaxY(c);
                    NSString * cmd = [NSString stringWithFormat:
                                      @"/usr/sbin/screencapture -x -R %d,%d,%d,%d '%s'",
                                      (int)c.origin.x, (int)top, (int)c.size.width, (int)c.size.height,
                                      shotPath];

                    system([cmd UTF8String]);
                });
            }
        }
        [NSApp run];

        view->removed();
        view->release();
        controller->terminate();
        controller->release();

        if (component != nullptr) {
            component->terminate();
            component->release();
        }
    }
    return 0;
}
