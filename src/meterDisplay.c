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
 */// Notes: Docs/code-notes/meterDisplay.c.md - "// notes §k" refers there.

// notes §1

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"
#include "utils.h"
#include "globalVars.h"
#include "meterDisplay.h"

#define METER_LEVEL_MASK      (0x0Fu)
#define METER_FALL_STEP_MS    (50.0)    // notes §2 - one row down per step
#define LAMP_MIN_ON_MS        (80.0)    // notes §3

static bool meter_key_valid(tModuleKey key) {
    return (key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES);
}

uint32_t meter_display_level(tModuleKey key, uint32_t leg, uint32_t value) {
    if ((meter_key_valid(key) == false) || (leg >= METER_DISPLAY_LEGS)) {
        return value;
    }
    tMeterDisplay * meter = &gMeterDisplay[key.slot][key.location][key.index];
    uint32_t        level = value & METER_LEVEL_MASK;
    double          now   = get_time_ms();

    // notes §2 - up at once, down a row a step; the hold bits above the level pass through as sent
    if (level >= meter->shown[leg]) {
        meter->shown[leg]  = (uint8_t)level;
        meter->stepMs[leg] = now;
    } else if ((now - meter->stepMs[leg]) >= METER_FALL_STEP_MS) {
        meter->shown[leg]--;
        meter->stepMs[leg] = now;
    }

    if (meter->shown[leg] != level) {
        gMeterDisplaySettling = true;
    }
    return (value & ~METER_LEVEL_MASK) | meter->shown[leg];
}

uint32_t meter_display_lamp(tModuleKey key, uint32_t lamp, uint32_t value) {
    if ((meter_key_valid(key) == false) || (lamp >= MAX_LEDS_PER_MODULE)) {
        return value;
    }
    tMeterDisplay * meter = &gMeterDisplay[key.slot][key.location][key.index];
    double          now   = get_time_ms();

    if (value != 0u) {
        meter->lampShown[lamp] = (uint8_t)value;
        meter->lampMs[lamp]    = now;
        return value;
    }

    // notes §3 - a flash shorter than a frame or two would otherwise never be seen
    if ((meter->lampShown[lamp] != 0u) && ((now - meter->lampMs[lamp]) < LAMP_MIN_ON_MS)) {
        gMeterDisplaySettling = true;
        return meter->lampShown[lamp];
    }
    meter->lampShown[lamp] = 0u;
    return 0u;
}

bool meter_display_take_settling(void) {
    bool settling = gMeterDisplaySettling;

    gMeterDisplaySettling = false;
    return settling;
}
