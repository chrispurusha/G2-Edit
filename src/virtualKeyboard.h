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

#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include "types.h"
#include "synthlibTypes.h"

// The Virtual Keyboard panel — the original editor's Tools > Virtual Keyboard (manual p.128).
// "Click on the keys of the Virtual Keyboard to play single notes. The selected note will be
// indicated by a black dot on the corresponding key. The note will sustain if you keep the mouse
// button depressed, just like pressing a key on a real keyboard."
//
// Opened from Settings > Virtual Keyboard. NO keyboard shortcut — the original's Ctrl-K, dropped
// on the owner's instruction, consistent with Parameter Pages and Parameter Overview.
//
// THE NOTE ITSELF GOES OUT OVER SUB_COMMAND_PLAY_NOTE (0x56), whose wire format is
// REVERSE-ENGINEERED FROM THE REFERENCE DECOMPILE AND UNCONFIRMED ON HARDWARE — see
// send_play_note() in usbComms.c for the derivation. If nothing sounds, that function is the thing
// to doubt, not this file: everything here is ordinary panel drawing and hit-testing.
//
// One note at a time, which is what the manual describes (a mouse has one button and the original
// speaks of "single notes"). noteOn is the sounding note, or -1. Any transition sends the note-off
// for the previous note before the note-on for the new one, so dragging across the keyboard can
// never leave one hanging.
//
// DRONE and REPEAT are the original's other two buttons, both plain toggles (manual p.128):
//   Drone  — "make the next played note start sounding 'infinitely'. Click the Drone button again
//            to disengage." So a key release stops sending the note-off; the note runs until
//            another key is played, Drone is switched off, or the panel closes.
//   Repeat — "make the last played note play repeatedly. Click the Repeat button again to
//            disengage." The note is re-struck on a timer, which is why this panel needs
//            virtual_keyboard_tick() driven from the render loop.
// "Hold" is the obvious name for Drone but is NOT what the original calls it, so the button says
// Drone to match.
//
// WHAT IS NOT DONE, and deliberately: the original expands its range by drag-resizing the window
// frame, and hides its button bar the same way. This panel has a fixed span and the four scroll
// buttons instead. Nothing else here depends on that, so it can be added later without rework.

#define VKB_KEYS_VISIBLE    (37)    // three octaves plus the top C, the usual span for a soft keyboard
#define VKB_MAX_WHITE       (VKB_KEYS_VISIBLE)

typedef struct {
    bool       active;
    uint32_t   firstNote;             // MIDI note at the left edge, always a C
    int32_t    noteOn;                // the sounding note, -1 when silent
    int32_t    lastNote;              // last note played, -1 if none yet — what Repeat re-strikes
    uint32_t   velocity;              // fixed; the wire format carries no velocity field
    bool       drone;
    bool       repeat;
    double     nextRepeatAt;          // glfwGetTime() stamp of the next re-strike

    tRectangle close;
    bool       closePressed;
    tRectangle octaveDown;
    tRectangle octaveUp;
    tRectangle noteDown;
    tRectangle noteUp;
    tRectangle droneButton;
    tRectangle repeatButton;

    // Hit rects, kept parallel to the drawn keys. Blacks are tested first because they overlap the
    // whites they sit between.
    tRectangle whiteKey[VKB_MAX_WHITE];
    uint32_t   whiteNote[VKB_MAX_WHITE];
    uint32_t   whiteCount;
    tRectangle blackKey[VKB_MAX_WHITE];
    uint32_t   blackNote[VKB_MAX_WHITE];
    uint32_t   blackCount;
} tVirtualKeyboard;

extern tVirtualKeyboard gVirtualKeyboard;

void open_virtual_keyboard_panel(void);
void close_virtual_keyboard_panel(void);
void render_virtual_keyboard_panel(void);
bool handle_virtual_keyboard_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_virtual_keyboard_key(int key, int mods, int action);

// Repeat's clock. tick() re-strikes the note when one is due and is a no-op otherwise, so the
// render loop can call it unconditionally; wants_ticks() tells that loop to wait with a timeout
// rather than sleeping on glfwWaitEvents(), which would otherwise stall the repeat until the next
// input event.
void virtual_keyboard_tick(void);
bool virtual_keyboard_wants_ticks(void);

#endif /* VIRTUAL_KEYBOARD_H */
