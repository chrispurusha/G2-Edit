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

// notes §2a - APP NAP AND THE EFFICIENCY CORES. This application draws only when something asks it
// to (synthlib_request_redraw()), so between gestures it looks idle to macOS - and an idle process
// gets napped: timers coalesced, threads deprioritised, work pushed onto the efficiency cores. A
// DAW never looks idle and never gets this, which is the shape of CT's observation that Ableton
// shows activity on the performance cores and the standalone does not.
//
// NSActivityLatencyCritical is the documented way to say "this process is doing audio": no napping
// and no timer coalescing. Held only while the audio output is open, so an editor with the engine
// switched off still naps as it should.
static id<NSObject> gAudioActivity = nil;    // strong under ARC, which is what holds the token

void platform_begin_audio_activity(void) {
    if (gAudioActivity != nil) {
        return;
    }
    gAudioActivity = [[NSProcessInfo processInfo]
                      beginActivityWithOptions:(NSActivityUserInitiated | NSActivityLatencyCritical)
                      reason:@"Sound engine is rendering audio"];
}

void platform_end_audio_activity(void) {
    if (gAudioActivity == nil) {
        return;
    }
    [[NSProcessInfo processInfo] endActivity:gAudioActivity];
    gAudioActivity = nil;
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
