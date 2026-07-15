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

#ifndef MUTATOR_UI_H
#define MUTATOR_UI_H

#include "mutator.h"
#include "synthlibTypes.h"

// Patch Mutator floater: panel chrome + drag, Mutate/Randomize/Interpolate/Cross operators with
// Probability/Range/Cross-probability sliders, Mother/Children x6/Father row (click to
// focus+audition), 7 Quick Lock category buttons, a Temporary Storage grid (click an empty slot
// to save the focused genome there, click a saved slot to load it as Mother), and a Patch
// Variations mirror row (click = load as Mother, Cmd-click = commit the focused genome into that
// variation's edit buffer). Right-click empties a Storage or Mother/Children/Father box.
// Drag-and-drop works between any two boxes across all three rows - Mother/Children/Father,
// Temporary Storage, and Patch Variations (plain drag = copy - dragging onto Father loads it
// quietly, without auditioning, since it's just the other breeding parent; Shift-drag =
// Interpolate; Cmd-drag = Cross); a plain drop onto a Variation commits there with confirmation,
// since that's a real hardware write. Deliberately deferred: multi-select for the Exclude From
// Mutation toggle.

#define MUTATOR_NUM_BOXES              8    // 0=Mother, 1-6=Children, 7=Father
#define MUTATOR_NUM_REAL_VARIATIONS    8
#define MUTATOR_NUM_STORAGE            24   // 3 rows of 8, per the manual

typedef enum {
    mutatorFocusNone   = -1,
    mutatorFocusMother = 0,
    // 1..6 are children
    mutatorFocusFather = 7,
} tMutatorFocusKind;

typedef struct {
    bool                active;
    uint32_t            slot;

    tRectangle          panelRect;
    bool                draggingPanel;
    tCoord              dragMouseStart;
    tCoord              dragPanelStart;
    int32_t             draggingSlider;   // -1 = none, else 0=Prob, 1=Range, 2=Cross Prob
    double              dragLastY;        // last mouse y while dragging a dial (vertical drag = adjust)

    tMutatorSchemaEntry schema[MUTATOR_MAX_SCHEMA];
    uint32_t            schemaCount;

    uint8_t             genome[MUTATOR_NUM_BOXES][MUTATOR_MAX_SCHEMA];
    bool                genomeValid[MUTATOR_NUM_BOXES];

    int32_t             focus; // mutatorFocusNone, or 0..7 (tMutatorFocusKind / child index 1-6)

    bool                auditionBackupValid;
    uint32_t            auditionVariation;
    uint8_t             auditionBackup[MUTATOR_MAX_SCHEMA];

    double              mutateProb;  // 0..1
    double              mutateRange; // 0..1
    bool                linkProbRange;
    double              crossProb;   // 0..1

    tMutatorLocks       locks;

    uint8_t             storageGenome[MUTATOR_NUM_STORAGE][MUTATOR_MAX_SCHEMA];
    bool                storageValid[MUTATOR_NUM_STORAGE];

    // Hit-rects, recomputed every render() call, read back by the mouse handler.
    tRectangle          boxRect[MUTATOR_NUM_BOXES];
    tRectangle          operatorRect[4];   // Mutate, Randomize, Interpolate, Cross
    bool                operatorPressed[4];
    tRectangle          probSliderRect;
    tRectangle          rangeSliderRect;
    tRectangle          crossSliderRect;
    tRectangle          linkButtonRect;
    tRectangle          categoryLockRect[mutatorCatNone];           // one per real category (excludes mutatorCatNone itself)
    tRectangle          categorySoloRect[mutatorCatNone];
    tRectangle          variationRect[MUTATOR_NUM_REAL_VARIATIONS]; // mirrors the 8 real hardware variations
    tRectangle          storageRect[MUTATOR_NUM_STORAGE];
    tRectangle          titleBarRect;
    tRectangle          closeButtonRect;
} tMutatorState;

extern tMutatorState gMutator;

void open_mutator_panel(uint32_t slot);
void close_mutator_panel(void);

void render_mutator_panel(void);
bool handle_mutator_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_mutator_key(int key, int mods, int action);
void handle_mutator_cursor_pos(tCoord coord);

#endif /* MUTATOR_UI_H */
