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

#ifndef __MIDI_CC_LIST_H__
#define __MIDI_CC_LIST_H__

#include "sysIncludes.h"
#include "types.h"
#include "floatingPanel.h"

#ifdef __cplusplus
extern "C" {
#endif

// Every MIDI CC assignment in the patch, in one list — the original editor's "MIDI Controller"
// function (manual p.143), which it reaches from a right-click menu and the M key. Here it is a
// menu entry only, matching the standing decision for this family of panels.
//
// NAMED FOR THE ORIGINAL'S FUNCTION, not for what it contains, because "MIDI CC Assignments" is
// almost exactly what the View menu's overlay is called — and that overlay annotates parameters on
// the canvas rather than listing them, so two near-identical names for two different things sent
// the owner to the wrong one.
//
// WHY A LIST AND NOT JUST THE OVERLAY: the View MIDI CC Assignments overlay annotates parameters on
// the canvas, so it can only ever show the location being viewed — an FX assignment is invisible
// while the Voice Area is on screen. The manual makes the same distinction, pointing at this
// function for "the complete list of all MIDI CC# assignments in a Patch".
//
// It also answers a question MIDI Learn creates: pressing L STEALS the CC from whatever held it,
// silently and by design, and this is the only place that shows what it took.

void open_midi_cc_list_panel(uint32_t slot);
// THE STATE IS PUBLISHED, as gParamPages and gParamOverview already are, because the panel is a
// FLOATING one now: the shared registry in graphics.c takes the address of its tFloatingPanel and of
// its `active` flag, and both have to be compile-time constants to sit in a static table. It was
// file-static in midiCcList.c until 2026-08-20.
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
