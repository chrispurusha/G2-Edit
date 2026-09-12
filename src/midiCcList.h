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
// Notes: Docs/code-notes/midiCcList.h.md - "// notes §k" refers there.

#ifndef __MIDI_CC_LIST_H__
#define __MIDI_CC_LIST_H__

#include "sysIncludes.h"
#include "types.h"
#include "floatingPanel.h"

#ifdef __cplusplus
extern "C" {
#endif

// notes §1

void open_midi_cc_list_panel(uint32_t slot);
// notes §2
#define CC_LIST_ROWS    (24)   // 3 x 24 = 72 visible; a patch can hold MAX_NUM_CONTROLLERS (128)

typedef struct {
    bool           active;
    uint32_t       slot;
    bool           closePressed;
    tRectangle     close;
    tRectangle     slotButton[MAX_SLOTS];

    tFloatingPanel panel;   // see floatingPanel.h
} tMidiCcList;

extern tMidiCcList gMidiCcList;

void close_midi_cc_list_panel(void);
bool midi_cc_list_active(void);
void render_midi_cc_list_panel(void);
bool handle_midi_cc_list_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_midi_cc_list_key(int key, int mods, int action);

#ifdef __cplusplus
}
#endif

#endif // __MIDI_CC_LIST_H__
