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
// Notes: Docs/code-notes/virtualKeyboard.h.md - "// notes §k" refers there.

#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include "types.h"
#include "floatingPanel.h"
#include "synthlibTypes.h"

// notes §1

#define VKB_KEYS_VISIBLE    (37)    // three octaves plus the top C, the usual span for a soft keyboard
#define VKB_MAX_WHITE       (VKB_KEYS_VISIBLE)

typedef struct {
    bool active;

    // Floating, not modal — see floatingPanel.h. Holds the panel's position between frames, which is
    // what stopped it re-centring itself and made it movable.
    tFloatingPanel panel;

    uint32_t       firstNote;         // MIDI note at the left edge, always a C
    int32_t        noteOn;            // the sounding note, -1 when silent
    int32_t        lastNote;          // last note played, -1 if none yet — what Repeat re-strikes
    int32_t        sustainedNote;     // shift-latched note, left ringing after its key is released; -1 = none
    uint32_t       velocity;          // fixed; the wire format carries no velocity field
    bool           drone;
    bool           repeat;
    double         nextRepeatAt;      // glfwGetTime() stamp of the next re-strike

    tRectangle     close;
    bool           closePressed;
    tRectangle     octaveDown;
    tRectangle     octaveUp;
    tRectangle     noteDown;
    tRectangle     noteUp;
    tRectangle     droneButton;
    tRectangle     repeatButton;

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

// The computer keyboard as note entry, INDEPENDENT of whether the panel is open — the panel is a
// view of this state, not a precondition for it. Routed from key_event() after the text-edit
// handlers, so typing a patch or module name can never play a note. True when it consumed the key.
bool handle_note_entry_key(int key, int mods, int action);

// notes §2
void virtual_keyboard_tick(void);
bool virtual_keyboard_wants_ticks(void);

#endif /* VIRTUAL_KEYBOARD_H */
