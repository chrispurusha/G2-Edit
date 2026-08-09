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

// Everything that doesn't strictly need Objective-C/Cocoa has moved out of this file — File/
// Settings/Backup/Restore menu actions live in menuActions.c, and settings persistence lives in
// persistence.c (backed by SynthLib's cross-platform prefs.h rather than NSUserDefaults). What's
// left here is genuinely Mac-only: the minimal native app menu Cocoa itself requires, and
// sleep/wake notifications (NSWorkspace has no cross-platform equivalent in this codebase).

#import "misc.h"
#import <Cocoa/Cocoa.h>

#include "usbComms.h"
#include "prefs.h"
#include "audioOutput.h"
#include "midiInput.h"

// Sets up the minimal native Cocoa app menu (Quit/About/Hide/Services — GLFW's Cocoa backend
// already populates these at index 0), then restores window/zoom/dial-mode/last-folder state
// from the prefs file (see load_saved_settings() in persistence.c; settings used to live in
// NSUserDefaults, now a plain text file via SynthLib's prefs.h so the same mechanism can work on
// Windows/Linux too). File/Settings/Backup/Restore/Controls/View menus used to be constructed
// here too; they're now the in-window bar built in src/appMenuBar.c on top of SynthLib's menuBar
// engine, sharing menuActions.c's action functions.
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

// WHETHER ANY MOUSE BUTTON IS PHYSICALLY DOWN, asked of the window server rather than of our own
// event history.
//
// This exists for one failure: the mouse-up that never arrives. A captured dial drag has the pointer
// hidden and decoupled from the hardware, so losing the release does not merely leave a dial held —
// it leaves the user with no pointer at all. Every other way of answering "is a drag still going"
// is derived from the event that went missing: our drag flags were set by the press and cleared by
// the release, and glfwGetMouseButton() reports the last event GLFW was handed, which is the same
// stream. [NSEvent pressedMouseButtons] reports the hardware, so it is true whether or not we were
// told — and it is what the VST3 shell already uses for the same job (vst3/g2GlView.m).
bool platform_any_mouse_button_down(void) {
    return [NSEvent pressedMouseButtons] != 0;
}
