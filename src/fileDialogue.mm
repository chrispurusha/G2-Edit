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

#import "fileDialogue.h"
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

void open_file_read_dialogue_async(tFileDialogueCallback callback) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSOpenPanel * panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setTitle:@"Select a File"];

        [panel beginWithCompletionHandler:^(NSModalResponse result) {
             if (result == NSModalResponseOK) {
                 NSString * path = [panel.URL path];

                 if (callback) {
                     callback([path UTF8String]);
                 }
             } else {
                 if (callback) {
                     callback(NULL);
                 }
             }
         }];
    });
}

void show_alert_async(const char * title, const char * message) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert = [[NSAlert alloc] init];

        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    });
}

// Shows a two-button confirm/cancel alert for destructive actions (e.g. Bank Restore, which
// overwrites the target bank on the device). "Cancel" is added first so it's the default
// (Return-key) action — an accidental Enter should never trigger the destructive path.
void show_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle, tConfirmCallback callback) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];
    NSString * confirmString = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert          = [[NSAlert alloc] init];

        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:confirmString];

        NSModalResponse response = [alert runModal];

        if (callback) {
            callback(response == NSAlertSecondButtonReturn);
        }
    });
}

// Same as show_confirm_dialogue_async above, plus an accessory NSTextField for picking a target
// bank number that may differ from whatever bank the title/message refer to (Bank Restore's
// "restore this backup into a different bank" case). The field is clamped to
// [1, maxBank1Indexed] via NSNumberFormatter and pre-filled with defaultTargetBank1Indexed.
void show_bank_target_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle,
                                             uint32_t defaultTargetBank1Indexed, uint32_t maxBank1Indexed,
                                             tBankTargetConfirmCallback callback) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];
    NSString * confirmString = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert               = [[NSAlert alloc] init];

        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:confirmString];

        NSTextField * label           = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 26, 260, 18)];
        [label setStringValue:@"Restore to Bank:"];
        [label setBezeled:NO];
        [label setDrawsBackground:NO];
        [label setEditable:NO];
        [label setSelectable:NO];

        NSNumberFormatter * formatter = [[NSNumberFormatter alloc] init];
        [formatter setAllowsFloats:NO];
        [formatter setMinimum:@(1)];
        [formatter setMaximum:@(maxBank1Indexed)];

        NSTextField * field           = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 2, 80, 22)];
        [field setFormatter:formatter];
        [field setStringValue:[NSString stringWithFormat:@"%u", defaultTargetBank1Indexed]];

        NSView * accessory            = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 260, 50)];
        [accessory addSubview:label];
        [accessory addSubview:field];
        [alert setAccessoryView:accessory];
        [[alert window] setInitialFirstResponder:field];

        NSModalResponse response      = [alert runModal];
        bool confirmed                = (response == NSAlertSecondButtonReturn);
        uint32_t target               = defaultTargetBank1Indexed;

        if (confirmed) {
            NSInteger value = [field integerValue];

            if (value < 1) {
                value = 1;
            } else if (value > (NSInteger)maxBank1Indexed) {
                value = (NSInteger)maxBank1Indexed;
            }
            target          = (uint32_t)value;
        }

        if (callback) {
            callback(confirmed, target);
        }
    });
}

void open_folder_dialogue_async(tFileDialogueCallback callback, const char * title) {
    NSString * titleString = (title && title[0] != '\0')
                            ? [NSString stringWithUTF8String:title]
                            : @"Select a Folder";

    dispatch_async(dispatch_get_main_queue(), ^{
        NSOpenPanel * panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:NO];
        [panel setCanChooseDirectories:YES];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanCreateDirectories:YES];
        [panel setTitle:titleString];

        [panel beginWithCompletionHandler:^(NSModalResponse result) {
             if (result == NSModalResponseOK) {
                 NSString * path = [panel.URL path];

                 if (callback) {
                     callback([path UTF8String]);
                 }
             } else {
                 if (callback) {
                     callback(NULL);
                 }
             }
         }];
    });
}

void open_file_write_dialogue_async(tFileDialogueCallback callback, const char * defaultName) {
    // Capture defaultName before dispatching — it may be stack-allocated
    NSString * nameString = (defaultName && defaultName[0] != '\0')
                          ? [NSString stringWithUTF8String:defaultName]
                          : @"patch.pch2";

    dispatch_async(dispatch_get_main_queue(), ^{
        NSSavePanel * panel = [NSSavePanel savePanel];
        [panel setTitle:@"Save File As"];
        [panel setPrompt:@"Save"];
        [panel setCanCreateDirectories:YES];
        [panel setNameFieldStringValue:nameString];
        [panel setMessage:@"Choose where to save your file."];
        [panel setShowsTagField:NO];
        [panel setExtensionHidden:NO];

        [panel beginWithCompletionHandler:^(NSModalResponse result) {
             if (result == NSModalResponseOK) {
                 NSString * path = panel.URL.path;

                 if (callback) {
                     callback([path UTF8String]);
                 }
             } else {
                 if (callback) {
                     callback(NULL);
                 }
             }
         }];
    });
}

