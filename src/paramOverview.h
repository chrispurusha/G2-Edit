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

#ifndef PARAM_OVERVIEW_H
#define PARAM_OVERVIEW_H

#include "types.h"
#include "synthlibTypes.h"
#include "floatingPanel.h"

// The Parameter Overview panel — the original editor's Tools > Parameter Overview (manual p.126).
// Where the Parameter Pages panel (paramPages.c) shows ONE page's eight knobs as live widgets,
// this shows ALL FIFTEEN pages' assignments at once, as a 15-row x 8-column grid of display
// boxes, so a patch's whole panel layout can be read and reorganised in one view.
//
// It is a second view onto the same gKnobArray[slot] / gGlobalKnobArray that Parameter Pages
// reads, and resolves assignments through that panel's param_pages_knob_target() so the two can
// never disagree about what a knob points at. Nothing here owns any data.
//
// WHAT A BOX SHOWS, and why not a live widget: the original's overview is a grid of grey display
// boxes carrying names, not controls, and 120 live param widgets would be both unreadable at this
// density and a great deal of per-frame work. So a box carries the module name and the parameter
// name — or the parameter's MIDI CC# when View MIDI is on, which is that button's whole purpose
// in the original.
//
// DRAG TO REORGANISE is the feature the manual leads with ("you can very quickly reorganize all
// your knob assignments"): drag a box onto another box to MOVE that assignment to the new panel
// position, swapping nothing and overwriting whatever was there. The original also supports
// dragging a box out onto a module parameter in the patch window to CREATE an assignment; that
// direction is not possible here while the panel is a full-canvas modal overlay, so creating an
// assignment stays where it already was — the canvas's right-click Assign menu. See todo.txt.
//
// Opened from Settings > Parameter Overview. NO keyboard shortcut: the original uses Ctrl-L, but
// the owner's standing call for this family of panels is menu-only (as with Parameter Pages,
// whose Ctrl-F was implemented and then removed).

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
