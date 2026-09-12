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
// Notes: Docs/code-notes/patchAdjuster.h.md - "// notes §k" refers there.

#ifndef PATCH_ADJUSTER_H
#define PATCH_ADJUSTER_H

#include "types.h"
#include "floatingPanel.h"
#include "synthlibTypes.h"

// notes §1

typedef enum {
    adjusterAttack,
    adjusterDecay,
    adjusterSustain,
    adjusterRelease,
    adjusterModRate,
    adjusterTimbre,
    adjusterResonance,
    adjusterEffects,
    adjusterKnobMax,
    adjusterNone = adjusterKnobMax   // parameter belongs to no category
} tAdjusterKnob;

#define ADJUSTER_RANGE    (50)   // knob travel either side of centre, as in the original

typedef struct {
    bool           active;

    // Floating, not modal — see floatingPanel.h.
    tFloatingPanel panel;

    uint32_t       slot;
    uint32_t       variation;                // the variation the baseline was taken from
    int32_t        amount[adjusterKnobMax];  // -ADJUSTER_RANGE..+ADJUSTER_RANGE, 0 = centre
    bool           haveBaseline;
    uint32_t       moduleCount;              // to notice an add/remove and commit

    // notes §2
    uint8_t        baseline[locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS];

    tRectangle     close;
    bool           closePressed;
    tRectangle     knobRect[adjusterKnobMax];
    tRectangle     centreRect[adjusterKnobMax]; // the centre marker: click to return this knob to 0
    tRectangle     resetAll;
    int32_t        dragKnob;                    // knob being dragged, -1 when none
    double         dragStartY;
    int32_t        dragStartAmount;
} tPatchAdjuster;

extern tPatchAdjuster gPatchAdjuster;

void open_patch_adjuster_panel(uint32_t slot);
void close_patch_adjuster_panel(void);
void render_patch_adjuster_panel(void);
bool handle_patch_adjuster_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_patch_adjuster_key(int key, int mods, int action);
void handle_patch_adjuster_cursor_pos(tCoord coord);

// Called when something happens that the manual says commits the adjustment: a variation change or
// a module added or removed. Re-takes the baseline and returns every knob to centre. Safe to call
// when the panel is closed (does nothing).
void adjuster_note_patch_changed(void);

// Which knob owns this parameter, or adjusterNone. Exposed for testing and so the classification
// can be inspected from elsewhere without duplicating it.
tAdjusterKnob adjuster_classify_param(tModuleType moduleType, tLocation location, tParamType paramType, const char * label);

#endif /* PATCH_ADJUSTER_H */
