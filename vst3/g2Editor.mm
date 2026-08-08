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

// The plug-in's editor window.
//
// A HAND-BUILT COCOA PANEL, not the application's own canvas. That distinction is the whole design
// decision here, so it is worth stating plainly: G2-Edit draws through GLFW, and GLFW creates and
// owns its window. A plug-in is handed an NSView that the HOST owns and must draw into that. There
// is no GLFW call for "adopt this existing NSView", so the application's renderer cannot be pointed
// at a host window without replacing the layer underneath it.
//
// So this is a second, much smaller interface: AppKit controls for the things a host can usefully
// drive - the eight morph groups and an output trim - plus a readout of which patch is loaded. It is
// not a view of the patch, and it is not trying to be.
//
// Everything it changes goes through the controller's parameters rather than straight to the engine,
// so the host sees the moves, can record them, and stays in step with the panel.

#import <Cocoa/Cocoa.h>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include "g2Editor.h"
#include "g2GlView.h"

extern "C" {
#include "g2Menu.h"
}

using namespace Steinberg;
using namespace Steinberg::Vst;

// THE WHOLE WINDOW IS NOW THE CANVAS. The AppKit slider panel that used to fill it is gone: it
// existed because the application's renderer could not draw into a host-owned window, and it can.
// A patch is what the plug-in should show.
//
// The eight morph parameters and the output trim REMAIN as VST3 parameters — only their sliders
// went. A plug-in exposing no parameters leaves a host's generic panel empty and looks broken, and
// automation is the main reason a host wants them at all; neither depends on this window drawing
// them. The G2AlikePanel class below is kept, unreferenced, for the moment: it is the fallback if a
// host turns out not to tolerate the GL surface.
static const CGFloat kEditorWidth  = 900.0;

// Canvas plus the menu bar and the reserved topbar band above it, so adding the topbar's controls
// later does not steal height from the patch.
static const CGFloat kCanvasHeight = 600.0;
static const CGFloat kEditorHeight = kCanvasHeight + G2_PLUGIN_CHROME_HEIGHT;
static const int     kMorphCount   = 8;
static const int     kParamLevelId = 8;

// ------------------------------------------------------------------------------------------------
// The Cocoa side
// ------------------------------------------------------------------------------------------------

@interface G2AlikePanel : NSView
@property (assign) Steinberg::Vst::IEditController * controller;
@property (assign) Steinberg::Vst::IComponentHandler * handler;
@property (strong) NSMutableArray<NSSlider *> * sliders;
@property (strong) NSMutableArray<NSTextField *> * readouts;
@property (strong) NSTextField * patchLabel;
@end

@implementation G2AlikePanel

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];

    if (self == nil) {
        return nil;
    }
    self.sliders  = [NSMutableArray array];
    self.readouts = [NSMutableArray array];

    NSTextField * title = [self labelWithText:@"G2 Alike" atY:frame.size.height - 30.0
                                            x:16.0 width:200.0 bold:YES];

    [title setFont:[NSFont boldSystemFontOfSize:16.0]];

    // One row per morph group, then the output trim. Laid out by hand rather than with constraints:
    // it is a fixed-size window with nine identical rows, so autolayout would be more machinery than
    // arithmetic.
    CGFloat y = frame.size.height - 64.0;

    for (int i = 0; i <= kMorphCount; i++) {
        BOOL     isLevel = (i == kMorphCount);
        NSString * name  = isLevel ? @"Level" : [NSString stringWithFormat:@"Morph %d", i + 1];

        [self labelWithText:name atY:y x:16.0 width:70.0 bold:NO];

        NSSlider * s = [[NSSlider alloc] initWithFrame:NSMakeRect(92.0, y - 2.0, 280.0, 20.0)];

        [s setMinValue:0.0];
        [s setMaxValue:1.0];
        [s setDoubleValue:isLevel ? 1.0 : 0.0];
        [s setTag:i];
        [s setTarget:self];
        [s setAction:@selector(sliderMoved:)];
        [self addSubview:s];
        [self.sliders addObject:s];

        NSTextField * v = [self labelWithText:isLevel ? @"0.0" : @"0.0%"
                                          atY:y x:382.0 width:64.0 bold:NO];

        [v setAlignment:NSTextAlignmentRight];
        [self.readouts addObject:v];
        y -= 26.0;
    }

    // Which patch is playing. With no way to choose one from here, saying what is loaded is the next
    // most useful thing - a silent plug-in because the file is missing looks identical to a silent
    // plug-in because the patch uses unsupported modules.
    self.patchLabel = [self labelWithText:@"" atY:12.0 x:16.0 width:kEditorWidth - 32.0 bold:NO];
    [self.patchLabel setFont:[NSFont systemFontOfSize:10.0]];
    [self.patchLabel setTextColor:[NSColor secondaryLabelColor]];

    return self;
}

- (NSTextField *)labelWithText:(NSString *)text atY:(CGFloat)y x:(CGFloat)x width:(CGFloat)w bold:(BOOL)bold {
    NSTextField * l = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, w, 18.0)];

    [l setStringValue:text];
    [l setBezeled:NO];
    [l setDrawsBackground:NO];
    [l setEditable:NO];
    [l setSelectable:NO];
    [l setFont:bold ? [NSFont boldSystemFontOfSize:12.0] : [NSFont systemFontOfSize:12.0]];
    [self addSubview:l];
    return l;
}

// Wrapped in begin/performEdit/endEdit so the host treats a drag as one gesture — that is what lets
// it record automation and show the parameter as touched, rather than seeing a value appear from
// nowhere.
- (void)sliderMoved:(NSSlider *)sender {
    ParamID    id    = (ParamID)[sender tag];
    ParamValue value = [sender doubleValue];

    if (self.handler != nullptr) {
        self.handler->beginEdit(id);
        self.handler->performEdit(id, value);
        self.handler->endEdit(id);
    }

    if (self.controller != nullptr) {
        self.controller->setParamNormalized(id, value);
    }
    [self refreshReadout:(int)id value:value];
}

- (void)refreshReadout:(int)index value:(double)value {
    if ((index < 0) || (index >= (int)self.readouts.count)) {
        return;
    }
    NSString * text = (index == kParamLevelId)
                      ? [NSString stringWithFormat:@"%.1f dB", -60.0 + (value * 60.0)]
                      : [NSString stringWithFormat:@"%.1f%%", value * 100.0];

    [self.readouts[index] setStringValue:text];
}

// Pulls every value back out of the controller. Called when the window opens, so a panel reopened
// after automation has moved things shows where they actually are rather than where it left them.
- (void)syncFromController {
    if (self.controller == nullptr) {
        return;
    }

    for (int i = 0; i < (int)self.sliders.count; i++) {
        double v = self.controller->getParamNormalized((ParamID)i);

        [self.sliders[i] setDoubleValue:v];
        [self refreshReadout:i value:v];
    }
}

- (void)setPatchText:(const char *)path {
    if (path == nullptr) {
        return;
    }
    [self.patchLabel setStringValue:[NSString stringWithFormat:@"Patch: %s", path]];
}

@end

// ------------------------------------------------------------------------------------------------
// The VST3 side
// ------------------------------------------------------------------------------------------------

class G2AlikeView : public IPlugView {
public:
    G2AlikeView(IEditController * ctrl, IComponentHandler * hdlr, const char * patch)
        : refCount(1), controller(ctrl), handler(hdlr), panel(nil), patchPath(patch ? patch : "") {}

    // ARC owns the panel: clearing the strong reference is the whole teardown.
    virtual ~G2AlikeView(void) {
        panel = nil;
    }

    tresult PLUGIN_API queryInterface(const TUID iid, void ** obj) SMTG_OVERRIDE {
        QUERY_INTERFACE(iid, obj, FUnknown::iid, IPlugView)
        QUERY_INTERFACE(iid, obj, IPlugView::iid, IPlugView)
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef(void) SMTG_OVERRIDE {
        return (uint32)++refCount;
    }

    uint32 PLUGIN_API release(void) SMTG_OVERRIDE {
        int32 c = --refCount;

        if (c == 0) {
            delete this;
            return 0;
        }
        return (uint32)c;
    }

    // macOS hosts pass an NSView. Anything else — an HWND, an X11 window — is not something this
    // build can attach to, and saying so is what makes the host fall back gracefully rather than
    // hand over a pointer that would be misused.
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) SMTG_OVERRIDE {
        return (strcmp(type, kPlatformTypeNSView) == 0) ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API attached(void * parent, FIDString type) SMTG_OVERRIDE {
        if ((parent == nullptr) || (isPlatformTypeSupported(type) != kResultTrue)) {
            return kResultFalse;
        }
        NSView * host = (__bridge NSView *)parent;

        glView = g2_create_gl_view(NSMakeRect(0.0, 0.0, kEditorWidth, kEditorHeight));

        if (glView == nil) {
            return kResultFalse;    // no pixel format; better to fail than show an empty window
        }
        [host addSubview:glView];
        return kResultOk;
    }

    tresult PLUGIN_API removed(void) SMTG_OVERRIDE {
        // The GL view holds a context bound to this window, and tearing the window down around a
        // live surface is the sort of thing that works everywhere except the one host somebody
        // reports it from.
        if (glView != nil) {
            [glView removeFromSuperview];
            glView = nil;
        }
        return kResultOk;
    }

    tresult PLUGIN_API onWheel(float distance) SMTG_OVERRIDE { (void)distance; return kResultFalse; }
    tresult PLUGIN_API onKeyDown(char16 key, int16 code, int16 mods) SMTG_OVERRIDE { (void)key; (void)code; (void)mods; return kResultFalse; }
    tresult PLUGIN_API onKeyUp(char16 key, int16 code, int16 mods) SMTG_OVERRIDE { (void)key; (void)code; (void)mods; return kResultFalse; }

    tresult PLUGIN_API getSize(ViewRect * size) SMTG_OVERRIDE {
        if (size == nullptr) {
            return kInvalidArgument;
        }
        size->left   = 0;
        size->top    = 0;
        size->right  = (int32)kEditorWidth;
        size->bottom = (int32)kEditorHeight;
        return kResultOk;
    }

    tresult PLUGIN_API onSize(ViewRect * newSize) SMTG_OVERRIDE {
        (void)newSize;
        return kResultOk;
    }

    tresult PLUGIN_API onFocus(TBool state) SMTG_OVERRIDE { (void)state; return kResultOk; }

    tresult PLUGIN_API setFrame(IPlugFrame * frame) SMTG_OVERRIDE {
        plugFrame = frame;
        return kResultOk;
    }

    // Fixed size: nine rows of controls have no sensible reflow, and a host that cannot resize it
    // will not try.
    tresult PLUGIN_API canResize(void) SMTG_OVERRIDE {
        return kResultFalse;
    }

    tresult PLUGIN_API checkSizeConstraint(ViewRect * rect) SMTG_OVERRIDE {
        return getSize(rect);
    }

private:
    std::atomic<int32>  refCount;
    IEditController *   controller;
    IComponentHandler * handler;
    G2AlikePanel * __strong panel;
    NSView * __strong   glView    = nil;
    IPlugFrame *        plugFrame = nullptr;
    std::string         patchPath;
};

IPlugView * g2_create_editor_view(IEditController * controller, IComponentHandler * handler, const char * patchPath) {
    return new G2AlikeView(controller, handler, patchPath);
}
