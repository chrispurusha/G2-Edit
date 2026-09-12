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
// Notes: Docs/code-notes/misc.mm.md - "// notes §k" refers there.

// notes §1

#import "misc.h"
#import <Cocoa/Cocoa.h>

#include "usbComms.h"
#include "prefs.h"
#include "audioOutput.h"
#include "midiInput.h"

// notes §2
void setup_main_menu(void) {
    NSMenu * menuBar = [[NSApplication sharedApplication] mainMenu];

    if (menuBar == nil) {
        menuBar = [[NSMenu alloc] init];
        [[NSApplication sharedApplication] setMainMenu:menuBar];
    }
    prefs_init("G2-Edit");

    // Which audio device and output pair the sound engine should use. Read here because prefs_init()
    // has just run and the engine may be switched on before anything else touches audioOutput.
    audio_output_load_settings();
    midi_input_load_settings();
    load_saved_settings();
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

// notes §3
bool platform_any_mouse_button_down(void) {
    return [NSEvent pressedMouseButtons] != 0;
}
