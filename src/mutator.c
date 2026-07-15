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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mutator.h"
#include "defs.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "protocol.h"
#include "undo.h"

// ─── Classification ─────────────────────────────────────────────────────────

bool category_is_locked(tMutatorCategory category, const tMutatorLocks * locks) {
    bool anySolo = false;
    int  i       = 0;

    for (i = 0; i < mutatorCatMax; i++) {
        if (locks->solo[i]) {
            anySolo = true;
            break;
        }
    }

    if (anySolo) {
        return !locks->solo[category];
    }
    return locks->locked[category];
}

// Manual's "PERMANENTLY LOCKED PARAMETERS": signal-type selectors and mute/bypass buttons on
// oscillators, filters and effects. These map directly onto existing tParamType values - no
// name-based heuristic needed. paramTypeEnable is deliberately excluded here: it's reused for
// legitimate per-step/per-channel content (sequencer step-events, mixer channel enables, KeyQuant
// note toggles - confirmed by grepping moduleResources.h), not a module-level bypass switch.
bool mutator_is_permanently_locked(tParamType paramType) {
    switch (paramType) {
        case paramTypeBypass:
        case paramTypeToggle:
        case paramTypeMenu:
        case paramTypeStrMap:
        case paramTypePush:
            return true;

        default:
            return false;
    }
}

tMutatorCategory classify_param(tModuleType moduleType, tLocation location, tParamType paramType) {
    const char * name = gModuleProperties[moduleType].name;

    if (location == locationFx) {
        return mutatorCatEffects;
    }

    if (paramType == paramTypeOscFreq) {
        return mutatorCatOscFreq;
    }

    if (paramType == paramTypeFine) {
        return mutatorCatOscFine;
    }

    if ((strncmp(name, "Env", 3) == 0) || (strcmp(name, "ModADSR") == 0) || (strcmp(name, "ModAHD") == 0)) {
        if ((paramType == paramTypeADRTime) || (paramType == paramTypeCommonDial)) {
            return mutatorCatEnvelope;
        }
    }

    if (strncmp(name, "Seq", 3) == 0) {
        if (paramType == paramTypeSlider) {
            return mutatorCatSeqValue;
        }

        if (paramType == paramTypeEnable) {
            return mutatorCatSeqEvent;
        }
    }

    if ((strncmp(name, "Dly", 3) == 0) || (strncmp(name, "Delay", 5) == 0)) {
        if ((paramType == paramTypeTime) || (paramType == paramTypeTimeClk) || (paramType == paramTypeCommonDial)) {
            return mutatorCatDelays;
        }
    }
    return mutatorCatNone;
}

// ─── Schema ──────────────────────────────────────────────────────────────────

static const tParamLocation * find_param_location(tModuleType moduleType, uint32_t paramIndex) {
    uint32_t seen = 0;
    uint32_t i    = 0;

    for (i = 0; i < array_size_param_location_list(); i++) {
        if (paramLocationList[i].moduleType == moduleType) {
            if (seen == paramIndex) {
                return &paramLocationList[i];
            }
            seen++;
        }
    }

    return NULL;
}

uint32_t mutator_build_schema(uint32_t slot, tMutatorSchemaEntry * entries, uint32_t maxEntries) {
    uint32_t count = 0;
    uint32_t loc   = 0;
    uint32_t idx   = 0;
    uint32_t p     = 0;

    for (loc = (uint32_t)locationFx; loc <= (uint32_t)locationVa; loc++) {
        for (idx = 0; idx < MAX_NUM_MODULES; idx++) {
            tModule * module    = get_module_slot(slot, loc, idx);

            if ((module == NULL) || !module->active || module->excludeFromMutation) {
                continue;
            }
            uint32_t  numParams = module_param_count(module->type);

            for (p = 0; p < numParams; p++) {
                const tParamLocation * paramLoc = find_param_location(module->type, p);

                if ((paramLoc == NULL) || mutator_is_permanently_locked(paramLoc->type)) {
                    continue;
                }

                if (count >= maxEntries) {
                    return count;
                }
                entries[count].moduleKey  = module->key;
                entries[count].paramIndex = p;
                entries[count].category   = classify_param(module->type, (tLocation)loc, paramLoc->type);
                entries[count].range      = paramLoc->range;
                count++;
            }
        }
    }

    return count;
}

void mutator_chromosome_path(const uint8_t * genome, uint32_t count, tCoord * outPoints) {
    double   heading = 0.0;   // degrees, running accumulator (not wrapped - cos/sin below wrap it)
    double   x       = 0.0;
    double   y       = 0.0;
    uint32_t i       = 0;

    for (i = 0; i < count; i++) {
        outPoints[i].x = x;
        outPoints[i].y = y;

        if ((i & 1) == 0) {
            heading += (double)genome[i] - 63.0;
        } else {
            double radians = heading * (M_PI / 180.0);
            double step    = (double)genome[i];

            x += step * cos(radians);
            y += step * sin(radians);
        }
    }
}

// ─── Genome read/write ───────────────────────────────────────────────────────

void mutator_read_genome(const tMutatorSchemaEntry * schema, uint32_t count, uint32_t slot, uint32_t variation, uint8_t * outValues) {
    uint32_t i = 0;

    for (i = 0; i < count; i++) {
        tModuleKey key    = schema[i].moduleKey;
        key.slot     = slot;
        tModule *  module = get_module_slot(key.slot, key.location, key.index);
        outValues[i] = (module != NULL) ? module->param[variation][schema[i].paramIndex].value : 0;
    }
}

void mutator_apply_genome(const tMutatorSchemaEntry * schema, uint32_t count, uint32_t slot, uint32_t variation, const uint8_t * values, bool pushUndo) {
    uint32_t i = 0;

    for (i = 0; i < count; i++) {
        tModuleKey key        = schema[i].moduleKey;
        key.slot                                   = slot;
        tModule *  module     = get_module_slot(key.slot, key.location, key.index);

        if (module == NULL) {
            continue;
        }
        uint32_t   paramIndex = schema[i].paramIndex;
        uint32_t   oldValue   = module->param[variation][paramIndex].value;
        uint32_t   newValue   = values[i];

        if (oldValue == newValue) {
            continue;
        }
        module->param[variation][paramIndex].value = (uint8_t)newValue;
        send_param_value(slot, key, paramIndex, variation, newValue);

        if (pushUndo) {
            undo_push_param_change(key, paramIndex, variation, oldValue, newValue);
        }
    }
}

// ─── Random helpers ──────────────────────────────────────────────────────────

// Uniform [0,1)
static double frand(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

// Triangular noise centered on 0, range (-1, 1)
static double triangular(void) {
    return frand() - frand();
}

static uint32_t clamp_u32(int32_t value, uint32_t range) {
    if (value < 0) {
        return 0;
    }

    if (value >= (int32_t)range) {
        return range - 1;
    }
    return (uint32_t)value;
}

// ─── Operators ───────────────────────────────────────────────────────────────

void mutator_mutate(const uint8_t * base, const tMutatorSchemaEntry * schema, uint32_t count, double prob, double range, const tMutatorLocks * locks, uint8_t * out) {
    uint32_t i = 0;

    for (i = 0; i < count; i++) {
        if (category_is_locked(schema[i].category, locks) || (frand() >= prob)) {
            out[i] = base[i];
            continue;
        }
        double  offset = triangular() * range * (double)schema[i].range;
        int32_t value  = (int32_t)base[i] + (int32_t)lround(offset);
        out[i] = (uint8_t)clamp_u32(value, schema[i].range);
    }
}

void mutator_randomize(const uint8_t * base, const tMutatorSchemaEntry * schema, uint32_t count, const tMutatorLocks * locks, uint8_t * out) {
    uint32_t i = 0;

    for (i = 0; i < count; i++) {
        if (category_is_locked(schema[i].category, locks)) {
            out[i] = base[i];
            continue;
        }
        double   unit  = 0.0; // 0..1 shaped sample
        uint32_t range = schema[i].range;

        switch (schema[i].category) {
            case mutatorCatEnvelope:
                unit = frand();
                unit = unit * unit; // biased toward short times
                break;

            case mutatorCatOscFine:
                unit = (triangular() + 1.0) / 2.0; // biased hard toward center (in tune)
                break;

            case mutatorCatOscFreq:
                unit = (frand() + ((triangular() + 1.0) / 2.0)) / 2.0; // lightly biased toward center
                break;

            default:
                unit = frand();
                break;
        }
        out[i] = (uint8_t)clamp_u32((int32_t)lround(unit * (double)(range - 1)), range);
    }
}

void mutator_interpolate(const uint8_t * mother, const uint8_t * father, const tMutatorSchemaEntry * schema, uint32_t count, double t, uint8_t * out) {
    uint32_t i = 0;

    for (i = 0; i < count; i++) {
        double blended = (double)mother[i] + (((double)father[i] - (double)mother[i]) * t);
        out[i] = (uint8_t)clamp_u32((int32_t)lround(blended), schema[i].range);
    }
}

void mutator_cross(const uint8_t * mother, const uint8_t * father, const tMutatorSchemaEntry * schema, uint32_t count, double crossProb, uint8_t * out) {
    uint32_t i          = 0;
    bool     fromFather = false;

    for (i = 0; i < count; i++) {
        if (frand() < crossProb) {
            fromFather = !fromFather;
        }
        out[i] = fromFather ? father[i] : mother[i];
    }
}
