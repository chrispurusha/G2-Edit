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

#ifdef __cplusplus
extern "C" {
#endif

#include "defs.h"
#include "types.h"
#include "globalVars.h"
#include "menus.h"
#include "perfSettingsResources.h"

static tRgb col_on_off(uint8_t val) {
    return val ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
}

// ── Slot Enable ───────────────────────────────────────────────────────────────

static const char * get_slot_enabled(int slot) {
    return gGlobalSettings.slot[slot].enabled ? "On" : "Off";
}

static tRgb col_slot_enabled(int slot) {
    return col_on_off(gGlobalSettings.slot[slot].enabled);
}

static void act_slot_enabled(int slot) {
    gGlobalSettings.slot[slot].enabled = !gGlobalSettings.slot[slot].enabled;
    send_perf_settings_msg();
}

// ── Slot Keyboard ─────────────────────────────────────────────────────────────

static const char * get_slot_keyboard(int slot) {
    return gPerfSettings.slot[slot].keyboardEnabled ? "On" : "Off";
}

static tRgb col_slot_keyboard(int slot) {
    return col_on_off(gPerfSettings.slot[slot].keyboardEnabled);
}

static void act_slot_keyboard(int slot) {
    gPerfSettings.slot[slot].keyboardEnabled = !gPerfSettings.slot[slot].keyboardEnabled;
    send_perf_settings_msg();
}

// ── Slot Hold ─────────────────────────────────────────────────────────────────

static const char * get_slot_hold(int slot) {
    return gPerfSettings.slot[slot].holdEnabled ? "On" : "Off";
}

static tRgb col_slot_hold(int slot) {
    return col_on_off(gPerfSettings.slot[slot].holdEnabled);
}

static void act_slot_hold(int slot) {
    gPerfSettings.slot[slot].holdEnabled = !gPerfSettings.slot[slot].holdEnabled;
    send_perf_settings_msg();
}

// ── Table ─────────────────────────────────────────────────────────────────────

const tPerfSlotToggle kPSSlotToggles[kPSSlotToggleCount] = {
    {gPerfSettingsPanelRects.slotEnabled,  get_slot_enabled,  col_slot_enabled,  act_slot_enabled },
    {gPerfSettingsPanelRects.slotKeyboard, get_slot_keyboard, col_slot_keyboard, act_slot_keyboard},
    {gPerfSettingsPanelRects.slotHold,     get_slot_hold,     col_slot_hold,     act_slot_hold    },
};

#ifdef __cplusplus
}
#endif
