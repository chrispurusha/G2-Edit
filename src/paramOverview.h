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
// Notes: Docs/code-notes/paramOverview.h.md - "// notes §k" refers there.

#ifndef PARAM_OVERVIEW_H
#define PARAM_OVERVIEW_H

#include "types.h"
#include "synthlibTypes.h"
#include "floatingPanel.h"

// notes §1

#define PARAM_OVERVIEW_ROWS    (NUM_PARAM_PAGES * NUM_BANKS_PER_PAGE)   // A1..E3, 15 of them

typedef struct {
    bool       active;
    uint32_t   slot;               // which Slot's patch assignments are shown (own copy, as with
                                   // gParamPages — it doesn't move the canvas or the device)
    bool       showGlobal;         // false = the patch's own pages, true = the global pages
    bool       showMidi;           // View MIDI: boxes carry the CC# rather than the param name

    tRectangle close;
    bool       closePressed;
    tRectangle slotButton[MAX_SLOTS];
    tRectangle patchButton;
    tRectangle globalButton;
    tRectangle midiButton;
    tRectangle assignMidiButton;
    tRectangle clearMidiButton;
    tRectangle cell[PARAM_OVERVIEW_ROWS][NUM_KNOBS_PER_BANK];

    // Drag-to-move. dragFrom is the flat knob index the drag started on, -1 when none is in
    // progress. There is no separate "has moved far enough" test: the drop is a move only when it
    // lands on a DIFFERENT box, so a click that goes down and up on one box is inherently a no-op.
    int32_t        dragFrom;

    tFloatingPanel panel;   // see floatingPanel.h
} tParamOverviewEdit;

extern tParamOverviewEdit gParamOverview;

void open_param_overview_panel(uint32_t slot);
void close_param_overview_panel(void);
void render_param_overview_panel(void);
bool handle_param_overview_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_param_overview_key(int key, int mods, int action);

#endif /* PARAM_OVERVIEW_H */
