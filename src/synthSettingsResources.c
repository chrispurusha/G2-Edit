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
#include "synthSettingsResources.h"

// ── Shared colour helpers ─────────────────────────────────────────────────────

static tRgb col_grey(void) {
    return (tRgb)RGB_BACKGROUND_GREY;
}

static tRgb col_on_off(uint8_t val) {
    return val ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
}

// ── Global section ────────────────────────────────────────────────────────────

static const char * get_global_chan(void) {
    static char buf[8] = {0};

    if (gSynthSettings.globalChan >= 0x10) {
        snprintf(buf, sizeof(buf), "Off");
    } else {
        snprintf(buf, sizeof(buf), "%u", (unsigned)gSynthSettings.globalChan + 1u);
    }
    return buf;
}

static const char * get_sysex_id(void) {
    static char buf[8] = {0};

    if (gSynthSettings.sysexId < 16) {
        snprintf(buf, sizeof(buf), "%u", gSynthSettings.sysexId + 1);
    } else {
        snprintf(buf, sizeof(buf), "All");
    }
    return buf;
}

static void act_global_chan(tCoord coord) {
    open_midi_chan_dropdown(coord, &gSynthSettings.globalChan);
}

static void act_sysex_id(tCoord coord) {
    open_sysex_id_dropdown(coord, &gSynthSettings.sysexId);
}

const tSynthSettingItem kSSGlobal[kSSGlobalCount] = {
    {"Global:",   &gSettingsPanelRects.globalChan, "Off", get_global_chan, col_grey, act_global_chan},
    {"SysEx ID:", &gSettingsPanelRects.sysexId,    "All", get_sysex_id,    col_grey, act_sysex_id   },
};

// ── Options section ───────────────────────────────────────────────────────────

static tRgb col_local_on(void) {
    return col_on_off(gSynthSettings.localOn);
}

static tRgb col_mem_protect(void) {
    return col_on_off(gSynthSettings.memoryProtect);
}

static tRgb col_prog_chg_rcv(void) {
    return col_on_off(gSynthSettings.progChangeRcv);
}

static tRgb col_prog_chg_snd(void) {
    return col_on_off(gSynthSettings.progChangeSnd);
}

static tRgb col_ctrl_rcv(void) {
    return col_on_off(gSynthSettings.controllersRcv);
}

static tRgb col_ctrl_snd(void) {
    return col_on_off(gSynthSettings.controllersSnd);
}

static tRgb col_send_clock(void) {
    return col_on_off(gSynthSettings.sendClock);
}

static tRgb col_recv_clock(void) {
    return col_on_off(gSynthSettings.receiveClock);
}

static const char * get_local_on(void) {
    return gSynthSettings.localOn ? "On" : "Off";
}

static const char * get_mem_protect(void) {
    return gSynthSettings.memoryProtect ? "On" : "Off";
}

static const char * get_prog_chg_rcv(void) {
    return gSynthSettings.progChangeRcv ? "On" : "Off";
}

static const char * get_prog_chg_snd(void) {
    return gSynthSettings.progChangeSnd ? "On" : "Off";
}

static const char * get_ctrl_rcv(void) {
    return gSynthSettings.controllersRcv ? "On" : "Off";
}

static const char * get_ctrl_snd(void) {
    return gSynthSettings.controllersSnd ? "On" : "Off";
}

static const char * get_send_clock(void) {
    return gSynthSettings.sendClock ? "On" : "Off";
}

static const char * get_recv_clock(void) {
    return gSynthSettings.receiveClock ? "On" : "Off";
}

static void act_local_on(tCoord c) {
    (void)c;
    gSynthSettings.localOn = !gSynthSettings.localOn;
    send_synth_settings_msg();
}

static void act_mem_protect(tCoord c) {
    (void)c;
    gSynthSettings.memoryProtect = !gSynthSettings.memoryProtect;
    send_synth_settings_msg();
}

static void act_prog_chg_rcv(tCoord c) {
    (void)c;
    gSynthSettings.progChangeRcv = !gSynthSettings.progChangeRcv;
    send_synth_settings_msg();
}

static void act_prog_chg_snd(tCoord c) {
    (void)c;
    gSynthSettings.progChangeSnd = !gSynthSettings.progChangeSnd;
    send_synth_settings_msg();
}

static void act_ctrl_rcv(tCoord c) {
    (void)c;
    gSynthSettings.controllersRcv = !gSynthSettings.controllersRcv;
    send_synth_settings_msg();
}

static void act_ctrl_snd(tCoord c) {
    (void)c;
    gSynthSettings.controllersSnd = !gSynthSettings.controllersSnd;
    send_synth_settings_msg();
}

static void act_send_clock(tCoord c) {
    (void)c;
    gSynthSettings.sendClock = !gSynthSettings.sendClock;
    send_synth_settings_msg();
}

static void act_recv_clock(tCoord c) {
    (void)c;
    gSynthSettings.receiveClock = !gSynthSettings.receiveClock;
    send_synth_settings_msg();
}

const tSynthSettingItem kSSOptions[kSSOptionsCount] = {
    {"Local On:",      &gSettingsPanelRects.localOn,        "On", get_local_on,     col_local_on,     act_local_on    },
    {"Mem Protect:",   &gSettingsPanelRects.memoryProtect,  "On", get_mem_protect,  col_mem_protect,  act_mem_protect },
    {"Prog Chg Rcv:",  &gSettingsPanelRects.progChangeRcv,  "On", get_prog_chg_rcv, col_prog_chg_rcv, act_prog_chg_rcv},
    {"Prog Chg Snd:",  &gSettingsPanelRects.progChangeSnd,  "On", get_prog_chg_snd, col_prog_chg_snd, act_prog_chg_snd},
    {"Ctrl Rcv:",      &gSettingsPanelRects.controllersRcv, "On", get_ctrl_rcv,     col_ctrl_rcv,     act_ctrl_rcv    },
    {"Ctrl Snd:",      &gSettingsPanelRects.controllersSnd, "On", get_ctrl_snd,     col_ctrl_snd,     act_ctrl_snd    },
    {"Send Clock:",    &gSettingsPanelRects.sendClock,      "On", get_send_clock,   col_send_clock,   act_send_clock  },
    {"Receive Clock:", &gSettingsPanelRects.receiveClock,   "On", get_recv_clock,   col_recv_clock,   act_recv_clock  },
};

// ── Tuning section ────────────────────────────────────────────────────────────

static const char * get_tune_semi(void) {
    static char buf[8] = {0};

    snprintf(buf, sizeof(buf), "%+d", (int)gSynthSettings.tuneSemi);
    return buf;
}

static const char * get_tune_cent(void) {
    static char buf[8] = {0};

    snprintf(buf, sizeof(buf), "%+d", (int)gSynthSettings.tuneCent);
    return buf;
}

static const char * get_glob_shift_active(void) {
    return (gSynthSettings.globalShiftActive & 0x01) ? "Active" : "Off";
}

static const char * get_glob_oct_shift(void) {
    static char buf[8] = {0};

    snprintf(buf, sizeof(buf), "%+d", (int)gSynthSettings.globalOctaveShift);
    return buf;
}

static tRgb col_glob_shift_active(void) {
    return (gSynthSettings.globalShiftActive & 0x01) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
}

static void act_tune_semi(tCoord coord) {
    open_tune_semi_dropdown(coord, &gSynthSettings.tuneSemi);
}

static void act_tune_cent(tCoord coord) {
    open_tune_cent_dropdown(coord, &gSynthSettings.tuneCent);
}

static void act_glob_shift_active(tCoord c) {
    (void)c;
    gSynthSettings.globalShiftActive = !gSynthSettings.globalShiftActive;
    send_synth_settings_msg();
}

static void act_glob_oct_shift(tCoord coord) {
    open_octave_shift_dropdown(coord, &gSynthSettings.globalOctaveShift);
}

const tSynthSettingItem kSSTuning[kSSTuningCount] = {
    {"Semi:",       &gSettingsPanelRects.tuneSemi,          "+12",    get_tune_semi,         col_grey,              act_tune_semi        },
    {"Cent:",       &gSettingsPanelRects.tuneCent,          "+50",    get_tune_cent,         col_grey,              act_tune_cent        },
    {"Glob Shift:", &gSettingsPanelRects.globalShiftActive, "Active", get_glob_shift_active, col_glob_shift_active, act_glob_shift_active},
    {"Oct:",        &gSettingsPanelRects.globalOctaveShift, "+2",     get_glob_oct_shift,    col_grey,              act_glob_oct_shift   },
};

// ── Pedal section ─────────────────────────────────────────────────────────────

static const char * get_pedal_polarity(void) {
    return gSynthSettings.pedalPolarity ? "Closed" : "Open";
}

static const char * get_pedal_gain(void) {
    static char buf[8] = {0};

    snprintf(buf, sizeof(buf), "%.2f", 1.0 + gSynthSettings.pedalGain / 64.0);
    return buf;
}

static void act_pedal_polarity(tCoord coord) {
    open_pedal_polarity_dropdown(coord, &gSynthSettings.pedalPolarity);
}

static void act_pedal_gain(tCoord coord) {
    open_pedal_gain_dropdown(coord, &gSynthSettings.pedalGain);
}

const tSynthSettingItem kSSPedal[kSSPedalCount] = {
    {"Polarity:", &gSettingsPanelRects.pedalPolarity, "Closed", get_pedal_polarity, col_grey, act_pedal_polarity},
    {"Gain:",     &gSettingsPanelRects.pedalGain,     "1.50",   get_pedal_gain,     col_grey, act_pedal_gain    },
};

// ── Sort section ──────────────────────────────────────────────────────────────

static const char *     sPatchSortLabels[3]     = {"Prog#", "Alpha", "Category"};
static const char *     sPerfSortLabels[3]      = {"Prog#", "Prog#", "Category"};

static const char * get_patch_sort(void) {
    uint8_t idx = gSynthSettings.patchSortMode;

    return (idx < 3) ? sPatchSortLabels[idx] : sPatchSortLabels[0];
}

static const char * get_perf_sort(void) {
    uint8_t idx = gSynthSettings.perfSortMode;

    return (idx < 3) ? sPerfSortLabels[idx] : sPerfSortLabels[0];
}

static void act_patch_sort(tCoord coord) {
    open_patch_sort_dropdown(coord, &gSynthSettings.patchSortMode);
}

static void act_perf_sort(tCoord coord) {
    open_perf_sort_dropdown(coord, &gSynthSettings.perfSortMode);
}

const tSynthSettingItem kSSSort[kSSSortCount] = {
    {"Patch:", &gSettingsPanelRects.patchSortMode, "Category", get_patch_sort, col_grey, act_patch_sort},
    {"Perf:",  &gSettingsPanelRects.perfSortMode,  "Category", get_perf_sort,  col_grey, act_perf_sort },
};

#ifdef __cplusplus
}
#endif
