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
// Notes: Docs/code-notes/frontPanel.c.md - "// notes §k" refers there.

// notes §1

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"           // first: it defines G2_EDIT, which synthlibDefs.h reads
#include "frontPanel.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "types.h"
#include "dataBase.h"
#include "globalVars.h"
#include "menus.h"
#include "moduleGraphics.h"
#include "moduleResourcesAccess.h"
#include "paramCurves.h"
#include "paramPages.h"
#include "protocol.h"
#include "renderParams.h"
#include "undo.h"
#include "utilsGraphics.h"
#include "clickRegion.h"
#include "geometry.h"
#include "inputState.h"
#include "utils.h"
#include "prefs.h"
#include "canvasDrag.h"
#include "palette.h"
#include "selection.h"

#define FP_MARGIN                (16.0)
#define FP_GAP                   (14.0) // between the four displays
#define FP_PAGE_COLUMN_W         (150.0)
#define FP_DIAL                  (34.0)
#define FP_DISPLAY_MIN_W         (170.0)
#define FP_DISPLAY_MAX_W         (300.0)
#define FP_DOUBLE_PRESS_MS       (400.0)
#define FP_PAGE_BUTTON_W         (24.0)
#define FP_PAGE_BUTTON_GAP       (8.0)
#define FP_DIAL_BELOW_DISPLAY    (8.0)
#define FP_BUTTON_W              (64.0)
#define FP_LED_W                 (10.0)
#define FP_LED_H                 (5.0)
#define FP_LED_GAP               (2.0)
#define FP_FRAME                 (8.0)  // the navy surround's width
#define FP_BEZEL                 (4.0)  // round each display
#define FP_POSITIONS             (NUM_KNOBS_PER_BANK)

// notes §13 - sampled from the manual's picture of the panel (p.25)
static const tRgb        kFrameColour                     = {0.18, 0.15, 0.37}; // the navy surround
static const tRgb        kPanelColour                     = {0.57, 0.63, 0.67}; // the control area
static const tRgb        kBezelColour                     = {0.14, 0.12, 0.13};
static const tRgb        kDisplayColour                   = {0.55, 0.75, 0.25}; // the LCDs
static const tRgb        kLedLit                          = {0.91, 0.24, 0.18};
static const tRgb        kLedOff                          = {0.36, 0.37, 0.40};
static const tRgb        kPanelText                       = {0.18, 0.15, 0.37}; // printed on the panel, as the frame

static const char *const kRowLabel[NUM_PARAM_PAGES]       = {"A", "B", "C", "D", "E"};
static const char *const kColumnLabel[NUM_BANKS_PER_PAGE] = {"1", "2", "3"};
static const char *const kSlotLabel[MAX_SLOTS]            = {"A", "B", "C", "D"};

// ─── Patch settings ──────────────────────────────────────────────────────────

// notes §4
typedef enum {
    eSettingModule = 0,     // a hidden locationMorph module's param, in the active variation (notes §4)
    eSettingClock,
    eSettingClockRun,
    eSettingVoices,
    eSettingMonoPoly,
} tSettingKind;

typedef struct {
    tSettingKind        kind;
    uint32_t            moduleIndex;
    uint32_t            param;
    uint32_t            minimum;
    uint32_t            count;        // values from minimum
    const char *const * names;        // one per value, or NULL to use the format
    const char *        format;       // printf of (value + offset)
    int32_t             offset;
} tSetting;

typedef struct {
    const char * title;
    tSetting     knob;
    tSetting     button;
} tSettingPair;

static const char *const  kOffOn[]                = {"Off", "On"};
static const char *const  kStopRun[]              = {"Stop", "Run"};
static const char *const  kArpRate[]              = {"1/8", "1/8T", "1/16", "1/16T"};
static const char *const  kArpDir[]               = {"Up", "Down", "Up+Dn", "Random"};
static const char *const  kArpRange[]             = {"1 Oct", "2 Oct", "3 Oct", "4 Oct"};
static const char *const  kVibSource[]            = {"Off", "AfTouch", "Wheel"};
static const char *const  kGlideMode[]            = {"Off", "Normal", "Auto"};
static const char *const  kVolumeMute[]           = {"Muted", "On"};

// notes §5
static const tSettingPair kSettings[FP_POSITIONS] = {
    {
        "Master Clock",{eSettingClock,                                          0,             0, 30, 211, NULL,                       "%d BPM",  0},
        {
            eSettingClockRun, 0, 0, 0, 2, kStopRun, NULL, 0
        }
    },
    {
        "Voices",{eSettingVoices,                                         0,             0,  0,  32, NULL,                       "%d",      1},
        {
            eSettingMonoPoly, 0, 0, 0, monoPolyMax, monoPolyStrMap, NULL, 0
        }
    },
    {
        "Arpeggiator",{eSettingModule,                    patchModuleArpeggiator, ARP_SPEED,      0,   4, kArpRate,                   NULL,      0},
        {
            eSettingModule, patchModuleArpeggiator, ARP_ON_OFF, 0, 2, kOffOn, NULL, 0
        }
    },
    {
        "Arp Dir/Range",{eSettingModule,                    patchModuleArpeggiator, ARP_DIRECTION,  0,   4, kArpDir,                    NULL,      0},
        {
            eSettingModule, patchModuleArpeggiator, ARP_OCTAVES, 0, 4, kArpRange, NULL, 0
        }
    },
    {
        "Vibrato",{eSettingModule,                    patchModuleVibrato,     VIBRATO_DEPTH,  0, 101, NULL,                       "%d cnt",  0},
        {
            eSettingModule, patchModuleVibrato, VIBRATO_MOD, 0, 3, kVibSource, NULL, 0
        }
    },
    {
        "Glide",{eSettingModule,                    patchModuleGlide,       GLIDE_SPEED,    0, 128, patch_settings_glideStrMap, NULL,      0},
        {
            eSettingModule, patchModuleGlide, GLIDE_TYPE, 0, 3, kGlideMode, NULL, 0
        }
    },
    {
        "Bend",{eSettingModule,                    patchModuleBend,        BEND_RANGE,     0,  24, NULL,                       "%d semi", 1},
        {
            eSettingModule, patchModuleBend, BEND_ON_OFF, 0, 2, kOffOn, NULL, 0
        }
    },
    {
        "Patch Level",{eSettingModule,                    patchModuleVolume,      VOLUME_LEVEL,   0, 128, NULL,                       NULL,      0},
        {
            eSettingModule, patchModuleVolume, VOLUME_MUTE, 0, 2, kVolumeMute, NULL, 0
        }
    },
};

static tModule * setting_module(const tSetting * setting, uint32_t slot) {
    return get_module_slot(slot, (uint32_t)locationMorph, setting->moduleIndex);
}

// Patch settings are per variation (sound-engine-reference §63a): the one being played
static uint32_t setting_variation(const tSetting * setting, uint32_t slot) {
    (void)setting;

    return patch_settings_variation(slot);
}

static uint32_t setting_value(const tSetting * setting, uint32_t slot) {
    switch (setting->kind) {
        case eSettingClock:     return gGlobalSettings.masterClock;

        case eSettingClockRun:  return gGlobalSettings.masterClockRunning;

        case eSettingVoices:    return gPatchDescr[slot].voiceCount;

        case eSettingMonoPoly:  return gPatchDescr[slot].monoPoly;

        default:
        {
            tModule * module = setting_module(setting, slot);

            return (module != NULL) ? module->param[setting_variation(setting, slot)][setting->param].value : setting->minimum;
        }
    }
}

// Sets it locally and sends it; the undo is the caller's, once per gesture
static void setting_set(const tSetting * setting, uint32_t slot, uint32_t value) {
    switch (setting->kind) {
        case eSettingClock:
            gGlobalSettings.masterClock        = (uint8_t)value;
            send_master_clock_bpm(value);
            break;

        case eSettingClockRun:
            gGlobalSettings.masterClockRunning = (uint8_t)value;
            send_master_clock_run(value);
            break;

        case eSettingVoices:
            gPatchDescr[slot].voiceCount       = (uint8_t)value;
            send_patch_descr_update(slot);
            break;

        case eSettingMonoPoly:
            gPatchDescr[slot].monoPoly         = (uint8_t)value;
            send_patch_descr_update(slot);
            break;

        case eSettingModule:
        default:
        {
            tModule * module    = setting_module(setting, slot);
            uint32_t  variation = setting_variation(setting, slot);

            if (module == NULL) {
                return;
            }
            module->param[variation][setting->param].value = (uint8_t)value;

            send_param_value(slot, module->key, setting->param, variation, value);
            break;
        }
    }
}

static void setting_push_undo(const tSetting * setting, uint32_t slot, uint32_t oldValue, uint32_t newValue) {
    if (oldValue == newValue) {
        return;
    }

    switch (setting->kind) {
        case eSettingVoices:
            undo_push_patch_descr(slot, UNDO_PATCH_DESCR_VOICE_COUNT, (uint8_t)oldValue, (uint8_t)newValue);
            break;

        case eSettingMonoPoly:
            undo_push_patch_descr(slot, UNDO_PATCH_DESCR_MONO_POLY, (uint8_t)oldValue, (uint8_t)newValue);
            break;

        case eSettingModule:
        {
            tModuleKey key = {slot, (uint32_t)locationMorph, setting->moduleIndex};

            undo_push_param_change(key, setting->param, setting_variation(setting, slot), oldValue, newValue);
            break;
        }
        default:
            break;     // the clock is the instrument's, not the patch's - nothing to undo, as on the topbar
    }
}

static void setting_text(const tSetting * setting, uint32_t value, char * out, size_t outSize) {
    uint32_t index = (value >= setting->minimum) ? (value - setting->minimum) : 0u;

    if (index >= setting->count) {
        index = setting->count - 1u;
    }

    if (setting->names != NULL) {
        snprintf(out, outSize, "%s", setting->names[index]);
    } else if (setting->format != NULL) {
        snprintf(out, outSize, setting->format, (int)value + setting->offset);
    } else {
        // only the patch Level has neither
        double db = patch_volume_db((double)value);

        snprintf(out, outSize, (db <= -10.0) ? "%.0f dB" : "%.1f dB", db);
    }
}

// ─── State ───────────────────────────────────────────────────────────────────

// notes §9 - one drag for both kinds of panel knob: a patch setting, or an assigned parameter that
// the canvas does not drag (a switch or a list), which the G2's knob steps through
typedef struct {
    bool        active;
    bool        isParam;
    bool        moved;
    uint32_t    position;
    uint32_t    slot;
    tKnobTarget target;       // isParam
    uint32_t    variation;    // isParam
    uint32_t    minimum;
    uint32_t    count;
    uint32_t    startValue;
    double      unitAccum;
    tRectangle  rect;
} tPanelDrag;

static tPanelDrag sDrag;
static double     sLastSettingsPress = 0.0;

// The click-region user data: stable addresses, one per control
static uint32_t   sKnobCtx[FP_POSITIONS];
static uint32_t   sButtonCtx[FP_POSITIONS];
static uint32_t   sRowCtx[NUM_PARAM_PAGES];
static uint32_t   sColumnCtx[NUM_BANKS_PER_PAGE];
static tRectangle sButtonRect[FP_POSITIONS];

bool front_panel_active(void) {
    return gFrontPanelMode;
}

void front_panel_set_active(bool on) {
    if (on && !gFrontPanelMode) {
        selection_clear();    // notes §6
        stop_dragging();
        palette_set_open(false);
    }
    gFrontPanelMode = on;
    synthlib_request_redraw();
}

void front_panel_toggle(void) {
    front_panel_set_active(!gFrontPanelMode);
#ifndef SYNTHLIB_PLUGIN_BUILD
    prefs_set_int(PREF_KEY_FRONT_PANEL, gFrontPanelMode ? 1 : 0);
#endif
}

void front_panel_load_preference(void) {
    gFrontPanelMode = (prefs_get_int(PREF_KEY_FRONT_PANEL, 0) != 0);
}

static uint32_t view_pages_index(void) {
    return (gFrontPanelView == eFrontPanelGlobal) ? 1u : 0u;
}

static uint32_t knob_index(uint32_t position) {
    uint32_t pages = view_pages_index();

    return (((gFrontPanelPage[pages] * NUM_BANKS_PER_PAGE) + gFrontPanelBank[pages]) * NUM_KNOBS_PER_BANK) + position;
}

static tKnobTarget position_target(uint32_t position) {
    return param_pages_knob_target(gFrontPanelView == eFrontPanelGlobal, (uint32_t)gSlot % MAX_SLOTS, knob_index(position));
}

// The G2 puts a button assignment on the button under the knob - notes §2
static bool position_is_button(uint32_t position) {
    uint32_t index = knob_index(position);

    if (gFrontPanelView == eFrontPanelGlobal) {
        return gGlobalKnobArray[index].isLed != 0u;
    }
    return gKnobArray[(uint32_t)gSlot % MAX_SLOTS].knob[index].isLed != 0u;
}

// ─── Click handlers ──────────────────────────────────────────────────────────

static uint32_t drag_value(void) {
    if (sDrag.isParam) {
        tModule * module = get_module(sDrag.target.key);

        return (module != NULL) ? module->param[sDrag.variation][sDrag.target.paramIndex].value : 0u;
    }
    return setting_value(&kSettings[sDrag.position].knob, sDrag.slot);
}

static void drag_set(uint32_t value) {
    if (!sDrag.isParam) {
        setting_set(&kSettings[sDrag.position].knob, sDrag.slot, value);
        return;
    }
    tModule * module = get_module(sDrag.target.key);

    if (module == NULL) {
        return;
    }
    module->param[sDrag.variation][sDrag.target.paramIndex].value = (uint8_t)value;
    send_param_value(sDrag.target.key.slot, sDrag.target.key, sDrag.target.paramIndex, sDrag.variation, value);
    send_param_value_to_links(sDrag.target.key.slot, sDrag.target.key, sDrag.target.paramIndex, sDrag.variation, value);
}

// The undo for the whole gesture; a parameter knob pressed and released without moving acts as a
// click on its canvas widget would (notes §9)
static void finish_drag(bool clicked, tCoord coord) {
    if (!sDrag.active) {
        return;
    }
    sDrag.active = false;

    if (sDrag.isParam) {
        if (sDrag.moved) {
            undo_push_param_change(sDrag.target.key, sDrag.target.paramIndex, sDrag.variation, sDrag.startValue, drag_value());
        } else if (clicked) {
            (void)param_pages_release_target(&sDrag.target, coord);
        }
        return;
    }
    setting_push_undo(&kSettings[sDrag.position].knob, sDrag.slot, sDrag.startValue, drag_value());
}

static void begin_drag(void) {
    sDrag.active     = true;
    sDrag.moved      = false;
    sDrag.unitAccum  = 0.0;
    sDrag.startValue = drag_value();
    click_region_capture_rect(&sDrag.rect);

    if (synthlib_dial_mode() != eDialModeRotary) {
        canvas_drag_begin();
    }
}

static void param_knob_click(tCoord coord, eClickPhase phase, void * userData) {
    uint32_t position = *(const uint32_t *)userData;

    if (phase == eClickPress) {
        sDrag.isParam   = true;
        sDrag.position  = position;
        sDrag.target    = position_target(position);

        if (!sDrag.target.assigned) {
            return;
        }
        sDrag.variation = gPatchDescr[sDrag.target.key.slot].activeVariation;
        sDrag.minimum   = 0u;
        sDrag.count     = paramLocationList[sDrag.target.paramRef].range;
        begin_drag();
    } else if ((phase == eClickRelease) || (phase == eClickReleaseOutside)) {
        finish_drag(phase == eClickRelease, coord);
    }
    synthlib_request_redraw();
}

static void setting_knob_click(tCoord coord, eClickPhase phase, void * userData) {
    uint32_t position = *(const uint32_t *)userData;
    uint32_t slot     = (uint32_t)gSlot % MAX_SLOTS;

    if (phase == eClickPress) {
        sDrag.isParam  = false;
        sDrag.position = position;
        sDrag.slot     = slot;
        sDrag.minimum  = kSettings[position].knob.minimum;
        sDrag.count    = kSettings[position].knob.count;
        begin_drag();
    } else if ((phase == eClickRelease) || (phase == eClickReleaseOutside)) {
        finish_drag(false, coord);
    }
    synthlib_request_redraw();
}

static void position_button_click(tCoord coord, eClickPhase phase, void * userData) {
    uint32_t position = *(const uint32_t *)userData;

    if (phase != eClickRelease) {
        return;
    }

    if (gFrontPanelView == eFrontPanelSettings) {
        const tSetting * setting = &kSettings[position].button;
        uint32_t         slot    = (uint32_t)gSlot % MAX_SLOTS;
        uint32_t         old     = setting_value(setting, slot);
        uint32_t         next    = setting->minimum + (((old - setting->minimum) + 1u) % setting->count);

        setting_set(setting, slot, next);
        setting_push_undo(setting, slot, old, next);
    } else {
        tKnobTarget target = position_target(position);

        (void)param_pages_release_target(&target, coord);
    }
    synthlib_request_redraw();
}

// notes §3
static void settings_button_click(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;
    (void)userData;

    if (phase != eClickRelease) {
        return;
    }
    double now         = get_time_ms();
    bool   doublePress = (now - sLastSettingsPress) < FP_DOUBLE_PRESS_MS;

    sLastSettingsPress = now;

    if (shift_modifier_held() || doublePress) {
        gFrontPanelView = (gFrontPanelView == eFrontPanelGlobal) ? eFrontPanelPages : eFrontPanelGlobal;
    } else {
        gFrontPanelView = (gFrontPanelView == eFrontPanelPages) ? eFrontPanelSettings : eFrontPanelPages;
    }
    synthlib_request_redraw();
}

// A page button leaves Patch Settings for the pages, as on the G2
static void row_click(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;

    if (phase != eClickRelease) {
        return;
    }

    if (gFrontPanelView == eFrontPanelSettings) {
        gFrontPanelView = eFrontPanelPages;
    }
    gFrontPanelPage[view_pages_index()] = *(const uint32_t *)userData;
    synthlib_request_redraw();
}

static void column_click(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;

    if (phase != eClickRelease) {
        return;
    }

    if (gFrontPanelView == eFrontPanelSettings) {
        gFrontPanelView = eFrontPanelPages;
    }
    gFrontPanelBank[view_pages_index()] = *(const uint32_t *)userData;
    synthlib_request_redraw();
}

// ─── Drag ────────────────────────────────────────────────────────────────────

bool front_panel_drag_motion(tCoord coord, double rawX, double rawY) {
    if (!sDrag.active) {
        return false;
    }
    int32_t index  = (int32_t)drag_value() - (int32_t)sDrag.minimum;
    double  pixels = 0.0;

    if (synthlib_dial_mode() == eDialModeRotary) {
        index = (int32_t)angle_to_value(calculate_mouse_angle(coord, sDrag.rect), sDrag.count);
    } else {
        if (synthlib_dial_mode() == eDialModeHorizontal) {
            pixels     = rawX - gDragPrevX;
            gDragPrevX = rawX;
        } else {
            pixels     = gDragPrevY - rawY;
            gDragPrevY = rawY;
        }
        sDrag.unitAccum += pixels * (double)sDrag.count / dial_drag_pixels_for_full_range(sDrag.count);

        int32_t step = (int32_t)sDrag.unitAccum;

        sDrag.unitAccum -= (double)step;
        index           += step;
    }

    if (index < 0) {
        index = 0;
    }

    if (index >= (int32_t)sDrag.count) {
        index = (int32_t)sDrag.count - 1;
    }
    uint32_t value = sDrag.minimum + (uint32_t)index;

    if (value != drag_value()) {
        drag_set(value);
        sDrag.moved = true;
    }
    synthlib_request_redraw();
    return true;
}

void front_panel_drag_cancel(void) {
    finish_drag(false, (tCoord){0.0, 0.0});
}

// ─── Right click ─────────────────────────────────────────────────────────────

bool front_panel_right_click(tCoord coord) {
    const tCanvasWidget * widget = canvas_widget_at(coord);

    if ((widget != NULL) && (widget->kind == eCanvasWidgetParam)) {
        open_param_context_menu(coord, widget->key, canvas_widget_index(widget));
        return true;
    }

    if (gFrontPanelView != eFrontPanelSettings) {
        for (uint32_t position = 0; position < FP_POSITIONS; position++) {
            if (within_rectangle(coord, sButtonRect[position])) {
                tKnobTarget target = position_target(position);

                if (target.assigned) {
                    open_param_context_menu(coord, target.key, target.paramIndex);
                }
                break;
            }
        }
    }
    return true;
}

// ─── Rendering ───────────────────────────────────────────────────────────────

static void fit_text(char * dst, size_t dstSize, const char * src, double maxWidth) {
    size_t length = strlen(src);

    if (length >= dstSize) {
        length = dstSize - 1;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';

    while ((length > 0) && (get_text_width(dst, STANDARD_TEXT_HEIGHT, eCache) > maxWidth)) {
        length--;
        dst[length] = '\0';
    }
}

// Centred on its position's centre line and cut to the width it has - notes §11
static void display_line(double centreX, double y, double width, const char * text) {
    char   fitted[64] = {0};
    double textW      = 0.0;

    fit_text(fitted, sizeof(fitted), text, width);
    textW = get_text_width(fitted, STANDARD_TEXT_HEIGHT, eCache);
    render_text(mainArea, (tRectangle){{centreX - (textW / 2.0), y}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, fitted);
}

// A grey button with its LED above it, as on the G2 - notes §12
static tRectangle panel_button(double x, double y, double width, const char * text, bool lit, tRgb litColour) {
    tRectangle rect = draw_button(mainArea, (tRectangle){{x, y}, {width, STANDARD_BUTTON_TEXT_HEIGHT}}, text, (tRgb)RGB_BACKGROUND_GREY);

    set_rgb_colour(lit ? litColour : kLedOff);
    render_rectangle(mainArea, (tRectangle){{rect.coord.x + ((rect.size.w - FP_LED_W) / 2.0), rect.coord.y - FP_LED_H - FP_LED_GAP},
                                            {FP_LED_W, FP_LED_H}
                     });
    return rect;
}

// A position's button, FP_BUTTON_W wide as drawn, centred under its dial
static tRectangle position_button(double centreX, double y, const char * text, bool lit) {
    tRectangle bounds = draw_button_bounds((tRectangle){{0.0, 0.0}, {FP_BUTTON_W, STANDARD_BUTTON_TEXT_HEIGHT}});

    return panel_button(centreX - (bounds.size.w / 2.0), y, FP_BUTTON_W, text, lit, kLedLit);
}

// Three lines of text and a margin; the dials sit FP_DIAL_BELOW_DISPLAY under it
static double display_bottom(double displayTop) {
    return displayTop + 4.0 + (STANDARD_TEXT_HEIGHT * 3.0) + 4.0;
}

static double display_line_y(double displayTop, uint32_t line) {
    return displayTop + 4.0 + (STANDARD_TEXT_HEIGHT * (double)line);
}

// The value's name where the parameter has a list of them, else the number
static void param_value_name(const tKnobTarget * target, uint32_t value, char * out, size_t outSize) {
    const tParamLocation * location = &paramLocationList[target->paramRef];

    if ((location->strMap != NULL) && (value < array_size_str_map(location->strMap))) {
        snprintf(out, outSize, "%s", location->strMap[value]);
    } else {
        snprintf(out, outSize, "%u", (unsigned)value);
    }
}

// The kinds the canvas drags as a dial, and which render_param_common() draws as one
static bool canvas_drags(const tKnobTarget * target) {
    switch (paramLocationList[target->paramRef].type) {
        case paramTypeToggle:
        case paramTypeMenu:
        case paramTypeCustomData:
        case paramTypeBypass:
        case paramTypeEnable:
        case paramTypePush:
        case paramTypeRadioEdit:
            return false;

        default:
            return !param_drawn_by_graph(target->module->type, target->paramIndex);
    }
}

// One knob/button position on the Parameter Pages - notes §2
static void render_page_position(uint32_t position, double cellX, double cellW, double displayTop, double dialY, double buttonY) {
    tKnobTarget target                     = position_target(position);
    double      centreX                    = cellX + (cellW / 2.0);
    double      textW                      = cellW - 8.0;
    tRectangle  dialRect                   = {{centreX - (FP_DIAL / 2.0), dialY}, {FP_DIAL, FP_DIAL}};
    char        name[CLAVIA_NAME_SIZE + 8] = {0};
    char        valueText[32]              = {0};
    uint32_t    variation                  = 0;
    uint32_t    value                      = 0;

    set_rgb_colour((tRgb)RGB_BLACK);

    if (!target.assigned) {
        display_line(centreX, display_line_y(displayTop, 0), textW, "--");
        (void)render_dial(mainArea, dialRect, 0, 128, 0, (tRgb)RGB_GREY_5);
        sButtonRect[position] = position_button(centreX, buttonY, "", false);
        return;
    }
    variation = gPatchDescr[target.key.slot].activeVariation;
    value     = target.module->param[variation][target.paramIndex].value;

    // notes §7 - line 1, the module; dashes when the position before is on the same module
    {
        tKnobTarget before = (position > 0) ? position_target(position - 1) : (tKnobTarget){
            0
        };

        if (before.assigned && (memcmp(&before.key, &target.key, sizeof(tModuleKey)) == 0)) {
            snprintf(name, sizeof(name), "----------------");
        } else if (gFrontPanelView == eFrontPanelGlobal) {
            snprintf(name, sizeof(name), "%s:%s", kSlotLabel[target.key.slot % MAX_SLOTS], param_pages_module_display_name(target.module));
        } else {
            snprintf(name, sizeof(name), "%s", param_pages_module_display_name(target.module));
        }
        display_line(centreX, display_line_y(displayTop, 0), textW, name);
    }

    if (position_is_button(position)) {
        // A button assignment: the knob is blank, the lit button carries it
        param_value_name(&target, value, valueText, sizeof(valueText));
        (void)render_dial(mainArea, dialRect, 0, 128, 0, (tRgb)RGB_GREY_5);
        sButtonRect[position] = position_button(centreX, buttonY, valueText, value != 0u);
        register_click_region(sButtonRect[position], eClickLayerPanel, position_button_click, &sButtonCtx[position]);
    } else if (canvas_drags(&target)) {
        // notes §2 - the canvas's own dial, its text drawn here instead, centred
        set_param_render_area(mainArea);
        set_dial_text_hidden(true);
        (void)render_param_common(dialRect, target.module, target.paramRef, target.paramIndex);
        set_dial_text_hidden(false);
        set_param_render_area(moduleArea);
        snprintf(valueText, sizeof(valueText), "%s", render_param_last_value_text());
        sButtonRect[position] = position_button(centreX, buttonY, "", false);
    } else {
        // notes §9 - a switch or a list on the knob: the panel's dial, stepping through its values
        param_value_name(&target, value, valueText, sizeof(valueText));
        dialRect              = render_dial(mainArea, dialRect, value, paramLocationList[target.paramRef].range, 0, (tRgb)RGB_GREY_5);
        register_click_region(dialRect, eClickLayerPanel, param_knob_click, &sKnobCtx[position]);
        sButtonRect[position] = position_button(centreX, buttonY, "", false);
    }
    set_rgb_colour((tRgb)RGB_BLACK);
    display_line(centreX, display_line_y(displayTop, 1), textW, param_pages_knob_param_label(&target));
    display_line(centreX, display_line_y(displayTop, 2), textW, valueText);
}

static void render_settings_position(uint32_t position, double cellX, double cellW, double displayTop, double dialY, double buttonY) {
    const tSettingPair * pair           = &kSettings[position];
    uint32_t             slot           = (uint32_t)gSlot % MAX_SLOTS;
    uint32_t             knobValue      = setting_value(&pair->knob, slot);
    uint32_t             buttonValue    = setting_value(&pair->button, slot);
    double               centreX        = cellX + (cellW / 2.0);
    double               textW          = cellW - 8.0;
    tRectangle           dialRect       = {{centreX - (FP_DIAL / 2.0), dialY}, {FP_DIAL, FP_DIAL}};
    char                 knobText[32]   = {0};
    char                 buttonText[32] = {0};

    setting_text(&pair->knob, knobValue, knobText, sizeof(knobText));

    // as the topbar counts them: a Mono or Legato patch plays one voice whatever it stores
    if ((pair->knob.kind == eSettingVoices) && (gPatchDescr[slot].monoPoly != monoPolyPoly)) {
        snprintf(knobText, sizeof(knobText), "1");
    }
    setting_text(&pair->button, buttonValue, buttonText, sizeof(buttonText));

    set_rgb_colour((tRgb)RGB_BLACK);
    display_line(centreX, display_line_y(displayTop, 0), textW, pair->title);
    display_line(centreX, display_line_y(displayTop, 1), textW, knobText);
    display_line(centreX, display_line_y(displayTop, 2), textW, buttonText);

    dialRect              = render_dial(mainArea, dialRect,
                                        (knobValue >= pair->knob.minimum) ? (knobValue - pair->knob.minimum) : 0u, pair->knob.count, 0, (tRgb)RGB_GREY_5);
    register_click_region(dialRect, eClickLayerPanel, setting_knob_click, &sKnobCtx[position]);

    sButtonRect[position] = position_button(centreX, buttonY, buttonText, buttonValue != pair->button.minimum);
    register_click_region(sButtonRect[position], eClickLayerPanel, position_button_click, &sButtonCtx[position]);
}

// notes §8
void front_panel_render(void) {
    double renderW     = get_render_width() / gGlobalGuiScale;
    double renderH     = get_render_height() / gGlobalGuiScale;
    double top         = MENU_BAR_HEIGHT + TOP_BAR_HEIGHT + palette_band_height();
    double available   = renderW - (FP_MARGIN * 2.0) - FP_PAGE_COLUMN_W - (FP_GAP * 2.0);
    double displayW    = fmin(FP_DISPLAY_MAX_W, fmax(FP_DISPLAY_MIN_W, (available - (FP_GAP * 3.0)) / 4.0));
    double totalW      = (displayW * 4.0) + (FP_GAP * 5.0) + FP_PAGE_COLUMN_W;
    double x0          = fmax(FP_MARGIN, (renderW - totalW) / 2.0);
    double captionY    = top + 14.0;
    double displayTop  = captionY + STANDARD_TEXT_HEIGHT + 10.0;
    double dialY       = display_bottom(displayTop) + FP_DIAL_BELOW_DISPLAY;
    double buttonY     = dialY + FP_DIAL + 12.0;
    char   caption[48] = {0};

    for (uint32_t i = 0; i < FP_POSITIONS; i++) {
        sKnobCtx[i]   = i;
        sButtonCtx[i] = i;
    }

    set_click_region_clip(NULL);

    set_rgb_colour(kFrameColour);
    render_rectangle(mainArea, (tRectangle){{0.0, top}, {renderW, renderH - top}});
    set_rgb_colour(kPanelColour);
    render_rectangle(mainArea, (tRectangle){{FP_FRAME, top + FP_FRAME}, {renderW - (FP_FRAME * 2.0), renderH - top - (FP_FRAME * 2.0)}});

    // The caption: which page, or which settings, the displays are showing
    if (gFrontPanelView == eFrontPanelSettings) {
        snprintf(caption, sizeof(caption), "Slot %s - Patch Settings", kSlotLabel[(uint32_t)gSlot % MAX_SLOTS]);
    } else {
        uint32_t pages = view_pages_index();

        snprintf(caption, sizeof(caption), "%s - Page %s%s",
                 (gFrontPanelView == eFrontPanelGlobal) ? "Global" : kSlotLabel[(uint32_t)gSlot % MAX_SLOTS],
                 kRowLabel[gFrontPanelPage[pages] % NUM_PARAM_PAGES], kColumnLabel[gFrontPanelBank[pages] % NUM_BANKS_PER_PAGE]);
    }
    set_rgb_colour(kPanelText);
    render_text(mainArea, (tRectangle){{x0, captionY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, caption);

    // The four displays, each over two knobs and two buttons
    for (uint32_t display = 0; display < (FP_POSITIONS / 2u); display++) {
        double displayX = x0 + (display * (displayW + FP_GAP));

        set_rgb_colour(kBezelColour);
        render_rectangle(mainArea, (tRectangle){{displayX - FP_BEZEL, displayTop - FP_BEZEL},
                                                {displayW + (FP_BEZEL * 2.0), (display_bottom(displayTop) - displayTop) + (FP_BEZEL * 2.0)}
                         });
        set_rgb_colour(kDisplayColour);
        render_rectangle(mainArea, (tRectangle){{displayX, displayTop}, {displayW, display_bottom(displayTop) - displayTop}});

        for (uint32_t half = 0; half < 2u; half++) {
            uint32_t position = (display * 2u) + half;
            double   cellX    = displayX + (half * (displayW / 2.0));

            if (gFrontPanelView == eFrontPanelSettings) {
                render_settings_position(position, cellX, displayW / 2.0, displayTop, dialY, buttonY);
            } else {
                render_page_position(position, cellX, displayW / 2.0, displayTop, dialY, buttonY);
            }
        }
    }

    // The PATCH SETTINGS / GLOBAL PANEL button and the PARAMETER PAGES row and column buttons
    {
        double     columnX  = x0 + (displayW * 4.0) + (FP_GAP * 5.0);
        double     rowStep  = STANDARD_BUTTON_TEXT_HEIGHT + FP_LED_H + FP_LED_GAP + 8.0;
        double     y        = displayTop + FP_LED_H + FP_LED_GAP;
        uint32_t   pages    = view_pages_index();
        bool       onPages  = (gFrontPanelView != eFrontPanelSettings);
        tRgb       lit      = kLedLit;
        tRectangle settings = panel_button(columnX, y, FP_PAGE_COLUMN_W - 10.0,
                                           (gFrontPanelView == eFrontPanelGlobal) ? "Global Panel" : "Patch Settings",
                                           gFrontPanelView != eFrontPanelPages, lit);

        register_click_region(settings, eClickLayerPanel, settings_button_click, NULL);
        y += rowStep + 6.0;

        set_rgb_colour(kPanelText);
        render_text(mainArea, (tRectangle){{columnX, y}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Parameter Pages");
        y += STANDARD_TEXT_HEIGHT + 6.0 + FP_LED_H + FP_LED_GAP;

        // As on the G2: A-E down the right, 1-3 across to the left of E (notes §10)
        double     rowX     = columnX + FP_PAGE_COLUMN_W - 10.0 - FP_PAGE_BUTTON_W;
        double     rowE     = y + ((NUM_PARAM_PAGES - 1u) * rowStep);

        for (uint32_t row = 0; row < NUM_PARAM_PAGES; row++) {
            tRectangle rect = panel_button(rowX, y + (row * rowStep), FP_PAGE_BUTTON_W, kRowLabel[row],
                                           onPages && (gFrontPanelPage[pages] == row), lit);

            sRowCtx[row] = row;
            register_click_region(rect, eClickLayerPanel, row_click, &sRowCtx[row]);
        }

        for (uint32_t column = 0; column < NUM_BANKS_PER_PAGE; column++) {
            double     x    = rowX - ((NUM_BANKS_PER_PAGE - column) * (FP_PAGE_BUTTON_W + FP_PAGE_BUTTON_GAP));
            tRectangle rect = panel_button(x, rowE, FP_PAGE_BUTTON_W, kColumnLabel[column],
                                           onPages && (gFrontPanelBank[pages] == column), lit);

            sColumnCtx[column] = column;
            register_click_region(rect, eClickLayerPanel, column_click, &sColumnCtx[column]);
        }
    }
}

#ifdef __cplusplus
}
#endif
