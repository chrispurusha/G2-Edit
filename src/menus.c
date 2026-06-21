/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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
#include "utils.h"
#include "utilsGraphics.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
#include "menus.h"

// ── Synth settings action targets ──────────────────────────────────────────

static uint8_t * gSettingU8Target = NULL;
static int8_t *  gSettingI8Target = NULL;

static void send_synth_settings_msg(void) {
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdWriteSynthSettings;
    msg_send(&gCommandQueue, &msg);
}

static void action_setting_u8(int index) {
    *gSettingU8Target = (uint8_t)gContextMenu.items[index].param;
    send_synth_settings_msg();
}

static void action_setting_i8(int index) {
    *gSettingI8Target = (int8_t)(int32_t)gContextMenu.items[index].param;
    send_synth_settings_msg();
}

// ── Patch descriptor action targets ────────────────────────────────────────

static void send_patch_descr_update(uint32_t slot) {
    tMessageContent messageContent = {0};

    messageContent.cmd  = eMsgCmdWritePatchDescr;
    messageContent.slot = slot;
    msg_send(&gCommandQueue, &messageContent);
}

static void action_set_patch_type(int index) {
    uint32_t slot = atomic_load(&gSlot);

    gPatchDescr[slot].category = gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active        = false;
}

static void action_set_mono_poly(int index) {
    uint32_t slot = atomic_load(&gSlot);

    gPatchDescr[slot].monoPoly = (uint8_t)gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active        = false;
}

static void action_set_voice_count(int index) {
    uint32_t slot = atomic_load(&gSlot);

    gPatchDescr[slot].voiceCount = (uint8_t)gContextMenu.items[index].param;
    send_patch_descr_update(slot);
    gContextMenu.active          = false;
}

static void action_copy_variation(int index) {
    uint32_t        sourceVariation = (uint32_t)(gContextMenu.items[index].param >> 4) & 0xF;
    uint32_t        targetVariation = (uint32_t)(gContextMenu.items[index].param) & 0xF;
    uint32_t        slot            = atomic_load(&gSlot);
    uint32_t        numParams       = 0;
    uint32_t        paramIndex      = 0;
    uint32_t        morphIndex      = 0;
    tModule         module          = {0};
    tMessageContent msg             = {0};

    LOG_DEBUG("Copy variation %u to %u\n", sourceVariation, targetVariation);

    reset_walk_module();

    while (walk_next_module(&module)) {
        if (module.key.slot == slot) {
            numParams = module_param_count(module.type);

            for (paramIndex = 0; paramIndex < numParams; paramIndex++) {
                module.param[targetVariation][paramIndex].value = module.param[sourceVariation][paramIndex].value;

                for (morphIndex = 0; morphIndex < NUM_MORPHS; morphIndex++) {
                    module.param[targetVariation][paramIndex].morphRange[morphIndex] = module.param[sourceVariation][paramIndex].morphRange[morphIndex];
                }
            }

            write_module(module.key, &module);
        }
    }
    finish_walk_module();

    msg.cmd                             = eMsgCmdCopyVariation;
    msg.slot                            = slot;
    msg.copyVariationData.fromVariation = sourceVariation;
    msg.copyVariationData.toVariation   = targetVariation;
    msg_send(&gCommandQueue, &msg);

    gContextMenu.active                 = false;
    atomic_store(&gReDraw, true);
}

// ── Static menu item arrays (menuResources.h uses the actions above) ───────

#include "menuResources.h"

// ── Core context menu mechanism ─────────────────────────────────────────────

static void clamp_menu_to_screen(tMenuItem * items, uint32_t columns) {
    int      count        = 0;
    double   renderWidth  = get_render_width() / gGlobalGuiScale;
    double   renderHeight = get_render_height() / gGlobalGuiScale;
    double   cellH        = STANDARD_TEXT_HEIGHT + (5 * 2);
    uint32_t cols         = (columns > 1) ? columns : 1;

    while (items[count].label != NULL) {
        count++;
    }
    int      rows         = (count + (int)cols - 1) / (int)cols;
    double   menuHeight   = rows * cellH;

    if (gContextMenu.coord.y + menuHeight > (renderHeight - SCROLLBAR_WIDTH)) {
        gContextMenu.coord.y = (renderHeight - SCROLLBAR_WIDTH) - menuHeight;
    }

    if (gContextMenu.coord.y < 0.0) {
        gContextMenu.coord.y = 0.0;
    }

    if (gContextMenu.cellWidth > 0.0) {
        double menuWidth = gContextMenu.cellWidth * (double)cols;

        if (gContextMenu.coord.x + menuWidth > renderWidth - SCROLLBAR_WIDTH) {
            gContextMenu.coord.x = renderWidth - SCROLLBAR_WIDTH - menuWidth;
        }

        if (gContextMenu.coord.x < 0.0) {
            gContextMenu.coord.x = 0.0;
        }
    }
}

void open_context_menu(tCoord coord, tMenuItem * items, uint32_t columns, double cellWidth) {
    gContextMenu.coord     = coord;
    gContextMenu.items     = items;
    gContextMenu.columns   = columns;
    gContextMenu.cellWidth = cellWidth;
    gContextMenu.active    = true;
    clamp_menu_to_screen(items, (columns > 1) ? columns : 1);
}

bool handle_context_menu_click(tCoord coord) {
    if ((gContextMenu.active == false) || (gContextMenu.items == NULL)) {
        return false;
    }
    double   size        = 0.0;
    double   largestSize = 0.0;
    double   itemHeight  = STANDARD_TEXT_HEIGHT;
    uint32_t columns     = (gContextMenu.columns > 1) ? gContextMenu.columns : 1;

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        size = get_text_width(gContextMenu.items[i].label, itemHeight, eNoCache);

        if (size > largestSize) {
            largestSize = size;
        }
    }

    double   cellW       = (largestSize + (5 * 2) > itemHeight) ? largestSize + (5 * 2) : itemHeight;
    double   cellH       = itemHeight + (5 * 2);

    cellW = (gContextMenu.cellWidth > 0.0) ? gContextMenu.cellWidth : cellW;

    for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
        int        col      = (int)(i % columns);
        int        row      = (int)(i / columns);
        tRectangle itemRect = {
            {gContextMenu.coord.x + col * cellW, gContextMenu.coord.y + row * cellH},
            {cellW,                              cellH                             }
        };

        if (within_rectangle(coord, itemRect)) {
            if (gContextMenu.items[i].action != NULL) {
                if (gContextMenu.items[i].subMenu == NULL) {
                    gContextMenu.active = false;
                }
                gContextMenu.items[i].action(i);
            } else if (gContextMenu.items[i].subMenu != NULL) {
                open_context_menu(gContextMenu.coord, gContextMenu.items[i].subMenu, 0, 0.0);
            } else {
                gContextMenu.active = false;
            }
            return true;
        }
    }

    memset(&gContextMenu, 0, sizeof(gContextMenu));

    return false;
}

// ── Helper ──────────────────────────────────────────────────────────────────

tCoord below_rect(tRectangle r) {
    tCoord c = {r.coord.x, r.coord.y + r.size.h};

    return c;
}

// ── Synth settings dropdowns ────────────────────────────────────────────────

void open_midi_chan_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gMidiChanItems, 0, 0.0);
}

void open_sysex_id_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gSysexIdItems, 0, 0.0);
}

void open_tune_semi_dropdown(tCoord coord, int8_t * target) {
    static tMenuItem items[26];
    static char      labels[25][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 25; i++) {
            int val = i - 12;
            snprintf(labels[i], sizeof(labels[i]), "%+d", val);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_i8;
            items[i].param   = (uint32_t)(int32_t)val;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingI8Target = target;
    open_context_menu(coord, items, 5, 0.0);
}

void open_tune_cent_dropdown(tCoord coord, int8_t * target) {
    static tMenuItem items[102];
    static char      labels[101][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 101; i++) {
            int val = i - 50;
            snprintf(labels[i], sizeof(labels[i]), "%+d", val);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_i8;
            items[i].param   = (uint32_t)(int32_t)val;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingI8Target = target;
    open_context_menu(coord, items, 10, 0.0);
}

void open_octave_shift_dropdown(tCoord coord, int8_t * target) {
    gSettingI8Target = target;
    open_context_menu(coord, gOctaveShiftItems, 0, 0.0);
}

void open_pedal_gain_dropdown(tCoord coord, uint8_t * target) {
    static tMenuItem items[34];
    static char      labels[33][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 33; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%.2f", 1.0 + i / 64.0);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_u8;
            items[i].param   = (uint32_t)i;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingU8Target = target;
    open_context_menu(coord, items, 4, 0.0);
}

void open_patch_sort_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPatchSortItems, 0, 0.0);
}

void open_perf_sort_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPerfSortItems, 0, 0.0);
}

void open_on_off_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gOnOffItems, 0, 0.0);
}

void open_active_off_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gActiveOffItems, 0, 0.0);
}

void open_pedal_polarity_dropdown(tCoord coord, uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPedalPolarityItems, 0, 0.0);
}

// ── Patch settings menus ────────────────────────────────────────────────────

void open_patch_type_context_menu(tCoord coord) {
    open_context_menu(coord, gPatchTypeItems, 0, 0.0);
}

void open_mono_poly_context_menu(tCoord coord) {
    open_context_menu(coord, gMonoPolyItems, 0, 0.0);
}

void open_voice_count_context_menu(tCoord coord) {
    static tMenuItem menuItems[33];
    static char      labels[32][4];
    static bool      labelsInitialised = false;
    uint32_t         slot              = atomic_load(&gSlot);
    uint32_t         assignedVoices    = gAssignedVoices[slot];

    if (!labelsInitialised) {
        for (int i = 0; i < 32; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%d", i + 1);
            menuItems[i].label   = labels[i];
            menuItems[i].action  = action_set_voice_count;
            menuItems[i].param   = (uint32_t)i;
            menuItems[i].subMenu = NULL;
        }

        menuItems[32]     = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        labelsInitialised = true;
    }

    for (int i = 0; i < 32; i++) {
        bool invalid = (assignedVoices > 0) && ((uint32_t)(i + 1) > assignedVoices);
        menuItems[i].colour = invalid ? (tRgb)RGB_RED_5 : (tRgb)RGB_GREY_3;
    }

    open_context_menu(coord, menuItems, 4, 0.0);
}

void open_variation_copy_menu(tCoord coord, uint32_t sourceVariation) {
    static tMenuItem menuItems[NUM_VARIATIONS + 1];
    static char      labels[NUM_VARIATIONS][32];
    int              count           = 0;
    uint32_t         targetVariation = 0;

    memset(&labels, 0, sizeof(labels));

    for (targetVariation = 0; targetVariation < NUM_VARIATIONS; targetVariation++) {
        if (targetVariation != sourceVariation) {
            if (targetVariation == VARIATION_INIT) {
                snprintf(labels[targetVariation], sizeof(labels[targetVariation]), "Copy to Init");
            } else {
                snprintf(labels[targetVariation], sizeof(labels[targetVariation]), "Copy to variation %u", targetVariation + 1);
            }
            menuItems[count].label   = labels[targetVariation];
            menuItems[count].colour  = (tRgb)RGB_GREY_3;
            menuItems[count].action  = action_copy_variation;
            menuItems[count].param   = (int)((sourceVariation << 4) | targetVariation);
            menuItems[count].subMenu = NULL;
            count++;
        }
    }

    menuItems[count] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    open_context_menu(coord, menuItems, 0, 0.0);
}

#ifdef __cplusplus
}
#endif
