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
#include "globalVars.h"

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

// Same as show_confirm_dialogue_async above, plus an accessory NSPopUpButton (labeled fieldLabel)
// for picking a bank number — a dropdown rather than a typed number field, consistent with how
// Load/Store/Delete let the user pick from a list instead of typing an index. Generic across every
// "pick a bank number" case in Backup/Restore (which bank to back up, which bank's backup to
// restore, and the possibly-different target bank to restore into), hence the caller-supplied
// fieldLabel rather than a hardcoded one. Unlike open_bank_browser()'s list (SynthLib's
// bankBrowser.h, which deliberately starts unselected), defaulting to a pre-selected item here is
// correct: there's no name/identity per bank to force the user to confirm by eye, just a number,
// and re-picking the same bank as last time is the common case.
void show_bank_target_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle,
                                             const char * fieldLabel,
                                             uint32_t defaultTargetBank1Indexed, uint32_t maxBank1Indexed,
                                             tBankTargetConfirmCallback callback) {
    NSString * titleString   = [NSString stringWithUTF8String:(title ? title : "")];
    NSString * messageString = [NSString stringWithUTF8String:(message ? message : "")];
    NSString * confirmString = [NSString stringWithUTF8String:(confirmButtonTitle ? confirmButtonTitle : "OK")];
    NSString * labelString   = [NSString stringWithUTF8String:(fieldLabel ? fieldLabel : "Bank:")];

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert * alert          = [[NSAlert alloc] init];

        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:titleString];
        [alert setInformativeText:messageString];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:confirmString];

        NSTextField * label      = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 26, 260, 18)];
        [label setStringValue:labelString];
        [label setBezeled:NO];
        [label setDrawsBackground:NO];
        [label setEditable:NO];
        [label setSelectable:NO];

        NSPopUpButton * popup    = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 2, 140, 22) pullsDown:NO];

        for (uint32_t bank1Indexed = 1; bank1Indexed <= maxBank1Indexed; bank1Indexed++) {
            [popup addItemWithTitle:[NSString stringWithFormat:@"Bank %u", bank1Indexed]];
        }

        if ((defaultTargetBank1Indexed >= 1) && (defaultTargetBank1Indexed <= maxBank1Indexed)) {
            [popup selectItemAtIndex:(NSInteger)(defaultTargetBank1Indexed - 1)];
        }
        NSView * accessory       = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 260, 50)];
        [accessory addSubview:label];
        [accessory addSubview:popup];
        [alert setAccessoryView:accessory];
        [[alert window] setInitialFirstResponder:popup];

        NSModalResponse response = [alert runModal];
        bool confirmed           = (response == NSAlertSecondButtonReturn);
        uint32_t target          = defaultTargetBank1Indexed;

        if (confirmed) {
            NSInteger selected = [popup indexOfSelectedItem];

            if (selected >= 0) {
                target = (uint32_t)selected + 1;
            }
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

