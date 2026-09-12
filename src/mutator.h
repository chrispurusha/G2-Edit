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
// Notes: Docs/code-notes/mutator.h.md - "// notes §k" refers there.

#ifndef MUTATOR_H
#define MUTATOR_H

#include <stdbool.h>
#include <stdint.h>
#include "types.h"

// Mirrors the seven Quick Lock buttons in the official Clavia Patch Mutator (manual ch. 7).
// mutatorCatNone covers every continuous param that isn't one of those seven - still mutated
// or randomized normally, just not toggleable by a Quick Lock button.
typedef enum {
    mutatorCatOscFreq,
    mutatorCatOscFine,
    mutatorCatEnvelope,
    mutatorCatSeqValue,
    mutatorCatSeqEvent,
    mutatorCatDelays,
    mutatorCatEffects,
    mutatorCatNone,
    mutatorCatMax
} tMutatorCategory;

// notes §1
typedef struct {
    bool locked[mutatorCatMax];
    bool solo[mutatorCatMax];
} tMutatorLocks;

// One continuous, non-excluded parameter reachable by the schema walk.
typedef struct {
    tModuleKey       moduleKey;
    uint32_t         paramIndex;
    tMutatorCategory category;
    uint32_t         range;         // paramLocationList[...].range - number of valid values
} tMutatorSchemaEntry;

#define MUTATOR_MAX_SCHEMA    (2 * MAX_NUM_MODULES * MAX_PARAMS_PER_MODULE)

bool category_is_locked(tMutatorCategory category, const tMutatorLocks * locks);

// notes §2
bool mutator_is_permanently_locked(tParamType paramType);
tMutatorCategory classify_param(tModuleType moduleType, tLocation location, tParamType paramType);

// notes §3
uint32_t mutator_build_schema(uint32_t slot, tMutatorSchemaEntry * entries, uint32_t maxEntries);

// notes §4
void mutator_chromosome_path(const uint8_t * genome, uint32_t count, tCoord * outPoints);

// Reads/writes a flat genome (one uint8_t value per schema entry, same order) from/to the live
// module database at the given variation index.
void mutator_read_genome(const tMutatorSchemaEntry * schema, uint32_t count, uint32_t slot, uint32_t variation, uint8_t * outValues);

// notes §5
void mutator_apply_genome(const tMutatorSchemaEntry * schema, uint32_t count, uint32_t slot, uint32_t variation, const uint8_t * values, bool pushUndo);

// Operators - all pure functions of (base genome(s), schema, locks) -> out genome. out may not
// alias base/mother/father. All produce exactly one child; callers wanting six children (per the
// manual) call these six times.

// Mutate: per-entry probability `prob` of changing; if it changes, a triangular-noise offset
// (centered on 0) scaled by `range` (fraction 0..1 of the param's own value span) is applied.
// Locked categories (per `locks`) are copied unchanged from base.
void mutator_mutate(const uint8_t * base, const tMutatorSchemaEntry * schema, uint32_t count, double prob, double range, const tMutatorLocks * locks, uint8_t * out);

// Randomize: full reroll per non-locked entry, with a musically-shaped distribution per category
// (Envelope biased short, OscFine centered, OscFreq lightly centered, else flat uniform). Locked
// entries are copied from base.
void mutator_randomize(const uint8_t * base, const tMutatorSchemaEntry * schema, uint32_t count, const tMutatorLocks * locks, uint8_t * out);

// Interpolate: linear blend of mother->father at fraction t (0..1). Ignores locks - per the
// manual, Interpolate has to interpolate locked parameters too to give a smooth transition.
void mutator_interpolate(const uint8_t * mother, const uint8_t * father, const tMutatorSchemaEntry * schema, uint32_t count, double t, uint8_t * out);

// Cross: per-entry coin flip (probability crossProb) of switching which parent is being copied
// from. No new/interpolated values - every value is a real value from one parent or the other.
// Ignores locks, same reasoning as Interpolate.
void mutator_cross(const uint8_t * mother, const uint8_t * father, const tMutatorSchemaEntry * schema, uint32_t count, double crossProb, uint8_t * out);

#endif /* MUTATOR_H */
