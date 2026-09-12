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
// Notes: Docs/code-notes/paramPages.h.md - "// notes §k" refers there.

#ifndef PARAM_PAGES_H
#define PARAM_PAGES_H

#include "types.h"
#include "synthlibTypes.h"
#include "floatingPanel.h"

// notes §1

// notes §2
typedef struct {
    bool       assigned;
    tModuleKey key;
    uint32_t   paramIndex;
    uint32_t   paramRef;
    tModule *  module;
} tKnobTarget;

// Resolves one of the 120 knob assignments. `index` is the flat knob index, i.e.
// ((page * NUM_BANKS_PER_PAGE) + bank) * NUM_KNOBS_PER_BANK + position. `slot` is ignored when
// showGlobal is true, since a global assignment carries its own Slot per knob.
tKnobTarget param_pages_knob_target(bool showGlobal, uint32_t slot, uint32_t index);

// The module's patch-given name if it has one, else its type name.
const char * param_pages_module_display_name(const tModule * module);

// The label render_param_common() will put on this param — a name the patch carries for it wins
// over the paramLocationList one, which is the precedence that function itself applies.
const char * param_pages_knob_param_label(const tKnobTarget * target);

typedef struct {
    bool           active;
    uint32_t       slot;             // which Slot's patch pages are shown (own copy, as with
                                     // gPatchParamsEdit - selecting a page here doesn't change
                                     // the Slot the canvas or the device is on)
    uint32_t       page;             // 0..NUM_PARAM_PAGES-1, i.e. row A..E
    uint32_t       bank;             // 0..NUM_BANKS_PER_PAGE-1, i.e. column 1..3
    bool           showGlobal;       // false = the patch's own pages, true = the global pages

    tRectangle     close;
    bool           closePressed;
    tRectangle     slotButton[MAX_SLOTS];
    tRectangle     patchButton;
    tRectangle     globalButton;
    tRectangle     pageButton[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE];
    tRectangle     knobCell[NUM_KNOBS_PER_BANK];   // whole cell, for the right-click menu
    tRectangle     knobWidget[NUM_KNOBS_PER_BANK]; // just the dial/button, for click and drag

    tFloatingPanel panel;                          // see floatingPanel.h
} tParamPagesEdit;

extern tParamPagesEdit gParamPages;

void open_param_pages_panel(uint32_t slot);
void close_param_pages_panel(void);
void render_param_pages_panel(void);
bool handle_param_pages_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_param_pages_key(int key, int mods, int action);

#endif /* PARAM_PAGES_H */
