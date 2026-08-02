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

#ifndef PATCH_ADJUSTER_H
#define PATCH_ADJUSTER_H

#include "types.h"
#include "synthlibTypes.h"

// The Patch Adjuster — the original editor's Tools > Patch Adjuster (manual ch.8, p.113). Eight
// knobs that each nudge EVERY parameter of one category across the whole patch at once: "turning
// the Attack knob to the left will decrease all attack times in the patch, relative to their
// current position... You do not need to track down where the specific parameters are situated in
// a complex patch."
//
// NOT THE MUTATOR, despite both being ways to shape a patch without hunting for parameters. The
// Mutator generates new patches at random (mutate/cross/interpolate); this is deterministic and
// semantic. The manual pairs them — the Adjuster is the last touch after the Mutator turns up
// something promising.
//
// NOT PATCH PARAMETERS AND NOT MORPH GROUPS EITHER. The manual is explicit: the knobs "act as
// remote, relative editing tools for all parameters of a specific category, wherever they are
// located in the patch. Because of their different nature, these knobs cannot be assigned to MIDI
// Controllers or to physical knobs on the synth." Nothing here is stored in the patch; only the
// parameters the knobs move are.
//
// HOW A KNOB APPLIES, decoded from CPatch::ApplyDistribution() in the reference (see
// adjuster_apply()). Every knob runs -50..+50 with 0 at centre, and works from a BASELINE snapshot
// of the patch rather than from the live values:
//     amount > 0:  new = orig + (max - orig) * amount/50      — interpolate toward maximum
//     amount < 0:  new = orig * (50 + amount)/50              — interpolate toward zero
//     amount == 0: untouched
// Working from a baseline is what makes the knobs behave the way the manual describes: returning
// one to centre restores its category exactly, and several knobs can be off-centre at once without
// fighting each other, because each owns a disjoint set of parameters.
//
// COMMITTING: "As soon as you move the focus to another variation or add or remove a module, the
// changes will be permanently applied, and the knobs will return to their middle positions." That
// is adjuster_note_patch_changed(), which re-takes the baseline and zeroes the knobs.

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
    bool     active;
    uint32_t slot;
    uint32_t variation;                      // the variation the baseline was taken from
    int32_t  amount[adjusterKnobMax];        // -ADJUSTER_RANGE..+ADJUSTER_RANGE, 0 = centre
    bool     haveBaseline;
    uint32_t moduleCount;                    // to notice an add/remove and commit

    // Baseline values, indexed [location][moduleIndex][paramIndex]. Only the active variation is
    // snapshotted — that is the one the knobs edit, and it is the one whose change commits.
    // locationMax covers Va/Fx/Morph; the Morph row is never populated, since patch-settings
    // pseudo-modules carry nothing any of the eight categories claims.
    uint8_t    baseline[locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS];

    tRectangle close;
    bool       closePressed;
    tRectangle knobRect[adjusterKnobMax];
    tRectangle centreRect[adjusterKnobMax];  // the centre marker: click to return this knob to 0
    tRectangle resetAll;
    int32_t    dragKnob;                     // knob being dragged, -1 when none
    double     dragStartY;
    int32_t    dragStartAmount;
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
