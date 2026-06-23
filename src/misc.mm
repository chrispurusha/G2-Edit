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
    init_patch(atomic_load(&gSlot));
    wake_glfw();
}

- (void)openNotes:(id)sender {
    uint32_t slot = atomic_load(&gSlot);

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
    uint32_t slot = atomic_load(&gSlot);

    gPatchSettingsEdit.active = true;
    gPatchSettingsEdit.slot   = slot;
    wake_glfw();
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
