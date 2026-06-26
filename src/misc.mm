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

#import "misc.h"
#import <Cocoa/Cocoa.h>
#include <stdatomic.h>
#include <string.h>
#include "globalVars.h"
#include "graphics.h"
#include "mouseHandle.h"
#include "usbComms.h"

@interface G2MenuTarget : NSObject
- (void)openPatch:(id)sender;
- (void)savePatch:(id)sender;
- (void)newPatch:(id)sender;
- (void)openNotes:(id)sender;
- (void)openSettings:(id)sender;
- (void)setDialModeRotary:(id)sender;
- (void)setDialModeVertical:(id)sender;
- (void)setDialModeHorizontal:(id)sender;
- (BOOL)validateMenuItem:(NSMenuItem *)item;
@end

@implementation G2MenuTarget

- (void)openPatch:(id)sender {
    gShowOpenFileReadDialogue = true;
    wake_glfw();
}

- (void)savePatch:(id)sender {
    gShowOpenFileWriteDialogue = true;
    wake_glfw();
}

- (void)newPatch:(id)sender {
    init_patch(gSlot);
    wake_glfw();
}

- (void)openNotes:(id)sender {
    uint32_t slot = gSlot;

    gPatchNotesEdit.active    = true;
    gPatchNotesEdit.slot      = slot;
    gPatchNotesEdit.cursorPos = gPatchNotesSize[slot];
    memset(gPatchNotesEdit.buffer, 0, sizeof(gPatchNotesEdit.buffer));
    memcpy(gPatchNotesEdit.buffer, gPatchNotes[slot], gPatchNotesSize[slot]);
    memset(gPatchNotesEdit.original, 0, sizeof(gPatchNotesEdit.original));
    memcpy(gPatchNotesEdit.original, gPatchNotes[slot], gPatchNotesSize[slot]);
    wake_glfw();
}

- (void)openSettings:(id)sender {
    uint32_t slot = gSlot;

    gPatchSettingsEdit.active = true;
    gPatchSettingsEdit.slot   = slot;
    wake_glfw();
}

- (void)setDialModeRotary:(id)sender {
    gDialMode = eDialModeRotary;
    [[NSUserDefaults standardUserDefaults] setInteger:gDialMode forKey:@"dialMode"];
}

- (void)setDialModeVertical:(id)sender {
    gDialMode = eDialModeVertical;
    [[NSUserDefaults standardUserDefaults] setInteger:gDialMode forKey:@"dialMode"];
}

- (void)setDialModeHorizontal:(id)sender {
    gDialMode = eDialModeHorizontal;
    [[NSUserDefaults standardUserDefaults] setInteger:gDialMode forKey:@"dialMode"];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL action = [item action];

    if (action == @selector(setDialModeRotary:)) {
        [item setState:(gDialMode == eDialModeRotary) ? NSControlStateValueOn : NSControlStateValueOff];
    } else if (action == @selector(setDialModeVertical:)) {
        [item setState:(gDialMode == eDialModeVertical) ? NSControlStateValueOn : NSControlStateValueOff];
    } else if (action == @selector(setDialModeHorizontal:)) {
        [item setState:(gDialMode == eDialModeHorizontal) ? NSControlStateValueOn : NSControlStateValueOff];
    }
    return YES;
}

@end

static NSMenuItem * make_item(NSString * title, SEL action, NSString * key, G2MenuTarget * target) {
    NSMenuItem * item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];

    [item setTarget:target];
    return item;
}

void setup_main_menu(void) {
    NSMenu *              menuBar   = [[NSApplication sharedApplication] mainMenu];
    static G2MenuTarget * target    = nil;

    if (target == nil) {
        target = [[G2MenuTarget alloc] init];
    }

    if (menuBar == nil) {
        menuBar = [[NSMenu alloc] init];
        [[NSApplication sharedApplication] setMainMenu:menuBar];
    }
    NSUserDefaults *      defaults  = [NSUserDefaults standardUserDefaults];

    if ([defaults objectForKey:@"dialMode"] != nil) {
        gDialMode = (tDialMode)[defaults integerForKey:@"dialMode"];
    }
    // File menu
    NSMenuItem *          fileMI    = [[NSMenuItem alloc] init];
    NSMenu *              fileMenu  = [[NSMenu alloc] initWithTitle:@"File"];

    [fileMenu addItem:make_item(@"Open Patch...", @selector(openPatch:), @"o", target)];
    [fileMenu addItem:make_item(@"Save Patch...", @selector(savePatch:), @"s", target)];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItem:make_item(@"New Patch", @selector(newPatch:), @"n", target)];
    [fileMI setSubmenu:fileMenu];
    [menuBar insertItem:fileMI atIndex:1];

    // Patch menu
    NSMenuItem *          patchMI   = [[NSMenuItem alloc] init];
    NSMenu *              patchMenu = [[NSMenu alloc] initWithTitle:@"Patch"];

    [patchMenu addItem:make_item(@"Notes", @selector(openNotes:), @"", target)];
    [patchMenu addItem:make_item(@"Settings", @selector(openSettings:), @",", target)];
    [patchMI setSubmenu:patchMenu];
    [menuBar insertItem:patchMI atIndex:2];

    // Controls menu
    NSMenuItem *          ctrlMI    = [[NSMenuItem alloc] init];
    NSMenu *              ctrlMenu  = [[NSMenu alloc] initWithTitle:@"Controls"];

    [ctrlMenu addItem:make_item(@"Rotary", @selector(setDialModeRotary:), @"", target)];
    [ctrlMenu addItem:make_item(@"Vertical", @selector(setDialModeVertical:), @"", target)];
    [ctrlMenu addItem:make_item(@"Horizontal", @selector(setDialModeHorizontal:), @"", target)];
    [ctrlMI setSubmenu:ctrlMenu];
    [menuBar insertItem:ctrlMI atIndex:3];
}

void register_sleep_wake_notifications(void) {
    [[[NSWorkspace sharedWorkspace] notificationCenter]
     addObserverForName:NSWorkspaceDidWakeNotification
     object:nil
     queue:nil
     usingBlock:^(NSNotification * note) {
         usb_signal_reconnect();
     }];
}
