/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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

char * open_file_read_dialogue(void) {
    @autoreleasepool {
        NSOpenPanel * panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setTitle:@"Select a File"];

        if ([panel runModal] == NSModalResponseOK) {
            NSString * path = [panel.URL path];
            return strdup([path UTF8String]); // Caller must free this
        } else {
            return NULL;
        }
    }
}

char * open_file_write_dialogue(void) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        [panel setTitle:@"Save File As"];
        [panel setPrompt:@"Save"];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanCreateDirectories:YES];
        [panel setMessage:@"Select an existing file to overwrite or choose a folder and type a new filename."];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL *selectedURL = panel.URL;

            // ✅ If the user picked a folder, prompt for filename
            BOOL isDir = NO;
            [[NSFileManager defaultManager] fileExistsAtPath:selectedURL.path isDirectory:&isDir];

            NSString *path = nil;
            if (isDir) {
                // Prompt user for a new filename
                NSAlert *alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Enter new filename"];
                [alert setInformativeText:@"You selected a folder. Enter a filename to save inside it:"];
                NSTextField *input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)];
                [alert setAccessoryView:input];
                [alert addButtonWithTitle:@"OK"];
                [alert addButtonWithTitle:@"Cancel"];

                if ([alert runModal] != NSAlertFirstButtonReturn) return NULL;

                NSString *filename = [input stringValue];
                if (filename.length == 0) return NULL;

                path = [selectedURL.path stringByAppendingPathComponent:filename];
            } else {
                // ✅ If existing file, confirm overwrite
                NSFileManager *fm = [NSFileManager defaultManager];
                if ([fm fileExistsAtPath:selectedURL.path]) {
                    NSAlert *alert = [[NSAlert alloc] init];
                    [alert setMessageText:@"File already exists"];
                    [alert setInformativeText:@"Do you want to overwrite the existing file?"];
                    [alert addButtonWithTitle:@"Overwrite"];
                    [alert addButtonWithTitle:@"Cancel"];
                    if ([alert runModal] != NSAlertFirstButtonReturn) return NULL;
                }
                path = selectedURL.path;
            }

            return strdup(path.UTF8String); // caller must free
        }

        return NULL;
    }
}
