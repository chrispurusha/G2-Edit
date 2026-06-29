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
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "patchParamsResources.h"

static tRgb col_grey(void) {
    return (tRgb)RGB_BACKGROUND_GREY;
}

static tRgb col_on_off(uint8_t val) {
    return val ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
}

// ── Arpeggiator ───────────────────────────────────────────────────────────────

static const char * arpRateLabels[] = {"1/8", "1/8T", "1/16", "1/16T"};
static const char * arpDirLabels[]  = {"Up", "Down", "Up+Dn", "Random"};
static const char * arpOctLabels[]  = {"1 oct", "2 oct", "3 oct", "4 oct"};

static const char * get_arp_rate(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_ARPEGGIATOR);
    uint8_t   ri  = mod ? mod->param[0][ARP_SPEED].value : 0;

    return (ri < 4) ? arpRateLabels[ri] : arpRateLabels[0];
}

static const char * get_arp_dir(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_ARPEGGIATOR);
    uint8_t   di  = mod ? mod->param[0][ARP_DIRECTION].value : 0;

    return (di < 4) ? arpDirLabels[di] : arpDirLabels[0];
}

static const char * get_arp_oct(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_ARPEGGIATOR);
    uint8_t   oi  = mod ? mod->param[0][ARP_OCTAVES].value : 0;

    return (oi < 4) ? arpOctLabels[oi] : arpOctLabels[0];
}

static const char * get_arp_en(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_ARPEGGIATOR);
    uint8_t   val = mod ? mod->param[0][ARP_ON_OFF].value : 0;

    return val ? "On" : "Off";
}

static tRgb col_arp_en(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_ARPEGGIATOR);
    uint8_t   val = mod ? mod->param[0][ARP_ON_OFF].value : 0;

    return col_on_off(val);
}

const tPatchParamItem kPPArp[kPPArpCount]         = {
    {"Rate:",   pPArpRate,      "1/16T",  get_arp_rate, col_grey  },
    {"Dir:",    pPArpDirection, "Random", get_arp_dir,  col_grey  },
    {"Range:",  pPArpOctaves,   "4 oct",  get_arp_oct,  col_grey  },
    {"Enable:", pPArpEnabled,   "On",     get_arp_en,   col_arp_en},
};

// ── Vibrato ───────────────────────────────────────────────────────────────────

static const char *   srcLabels[]                 = {"Off", "AfTouch", "Wheel"};

static const char * get_vibrato_source(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_VIBRATO);
    uint8_t   si  = mod ? mod->param[0][VIBRATO_MOD].value : 0;

    return (si < 3) ? srcLabels[si] : srcLabels[0];
}

const tPatchParamItem kPPVibrato[kPPVibratoCount] = {
    {"Source:", pPVibratoSource, "AfTouch", get_vibrato_source, col_grey},
};

// ── Glide ─────────────────────────────────────────────────────────────────────

static const char *   modeLabels[]                = {"Off", "Normal", "Auto"};

const char * get_glide_time_str(uint8_t gi) {
    return (gi < 128) ? patch_settings_glideStrMap[gi] : patch_settings_glideStrMap[0];
}

static const char * get_glide_mode(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_GLIDE);
    uint8_t   mi  = mod ? mod->param[0][GLIDE_TYPE].value : 2;

    return (mi < 3) ? modeLabels[mi] : modeLabels[2];
}

const tPatchParamItem kPPGlide[kPPGlideCount]     = {
    {"Mode:", pPGlideMode, "Normal", get_glide_mode, col_grey},
};

// ── Bend ──────────────────────────────────────────────────────────────────────

static const char * get_bend_range(void) {
    static char buf[16] = {0};
    tModule *   mod     = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_BEND);
    uint8_t     val     = mod ? mod->param[0][BEND_RANGE].value : 0;

    snprintf(buf, sizeof(buf), "%u semi", (unsigned)(val + 1));
    return buf;
}

static const char * get_bend_en(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_BEND);
    uint8_t   val = mod ? mod->param[0][BEND_ON_OFF].value : 0;

    return val ? "On" : "Off";
}

static tRgb col_bend_en(void) {
    tModule * mod = get_module_slot(gPatchParamsEdit.slot, (uint32_t)locationMorph, PATCH_BEND);
    uint8_t   val = mod ? mod->param[0][BEND_ON_OFF].value : 0;

    return col_on_off(val);
}

const tPatchParamItem kPPBend[kPPBendCount] = {
    {"Range:",  pPBendRange,   "24 semi", get_bend_range, col_grey   },
    {"Enable:", pPBendEnabled, "On",      get_bend_en,    col_bend_en},
};

#ifdef __cplusplus
}
#endif
