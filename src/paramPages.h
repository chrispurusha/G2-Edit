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

#ifndef PARAM_PAGES_H
#define PARAM_PAGES_H

#include "types.h"
#include "synthlibTypes.h"

// The Parameter Pages panel - the on-screen stand-in for the G2 front panel's PARAMETER PAGES
// and their eight ASSIGNABLE KNOBS (manual p.19 and p.89-91).
//
// A patch has 120 knob assignments, laid out as 8 knobs x 15 pages, and the 15 pages are
// themselves a matrix 3 wide (banks 1-3) by 5 high (rows A-E) - so a page is named A1, D2 and so
// on. There are another 120 GLOBAL assignments, the same shape but shared by all four Slots and
// each recording which Slot its module lives in; one toggle switches the panel between the two.
//
// Nothing here owns any data: the assignments are gKnobArray[slot] / gGlobalKnobArray, already
// parsed from the patch by protocol.c and already round-tripping to file and device, and the
// values are the module params themselves. The panel is a second view onto them, so an edit made
// here IS an edit to the module parameter - it goes out to the G2 and lands in the undo history
// exactly as if it had been made on the patch canvas. The reverse direction is free for the same
// reason: turning the knob on the hardware sends a param change, usbComms.c writes it into the
// same module param, and the panel redraws from it.
//
// Knob index -> page mapping is page = index / 24, bank = (index % 24) / 8, position = index % 8,
// matching the assign/deassign menus in menus.c and the canvas hover overlay in moduleGraphics.c.
// NEEDS A HARDWARE CONFIRM: that A1,A2,A3,B1... really is the order the G2 numbers them in, and
// not column-major.

typedef struct {
    bool       active;
    uint32_t   slot;                 // which Slot's patch pages are shown (own copy, as with
                                     // gPatchParamsEdit - selecting a page here doesn't change
                                     // the Slot the canvas or the device is on)
    uint32_t   page;                 // 0..NUM_PARAM_PAGES-1, i.e. row A..E
    uint32_t   bank;                 // 0..NUM_BANKS_PER_PAGE-1, i.e. column 1..3
    bool       showGlobal;           // false = the patch's own pages, true = the global pages

    tRectangle close;
    bool       closePressed;
    tRectangle slotButton[MAX_SLOTS];
    tRectangle patchButton;
    tRectangle globalButton;
    tRectangle pageButton[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE];
    tRectangle knobCell[NUM_KNOBS_PER_BANK];      // whole cell, for the right-click menu
    tRectangle knobWidget[NUM_KNOBS_PER_BANK];    // just the dial/button, for click and drag
} tParamPagesEdit;

extern tParamPagesEdit gParamPages;

void open_param_pages_panel(uint32_t slot);
void close_param_pages_panel(void);
void render_param_pages_panel(void);
bool handle_param_pages_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_param_pages_key(int key, int mods, int action);

#endif /* PARAM_PAGES_H */
