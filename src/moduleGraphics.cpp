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

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include <math.h>
#include <stdint.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"
#include "moduleGraphics.h"
#include "globalVars.h"
#include "renderParams.h"
#include "mouseHandle.h"
#include "menus.h"
#include "selection.h"
#include "mutatorUI.h"
#include "protocol.h"
#include "undo.h"
#include "clickRegion.h"

// ── Click-region registration ────────────────────────────────────────────────
//
// Every clickable widget on the canvas — module params, mode toggles,
// connectors, the module body/drag-handle strip, and the morph group
// overlay — registers a clickable rect each frame right where its render
// function already computes it, instead of mouseHandle.c re-deriving
// hit-testing over every active module. Morph group dials register at
// eClickLayerPanel (a fixed on-screen overlay, unlike everything else here,
// which is eClickLayerCanvas and scrolls with the module area) — dispatch
// checks Panel before Canvas unconditionally, which is what lets a scrolled
// regular module sit visually underneath the morph overlay without stealing
// its clicks (see mouse_button()'s own comment on this).

typedef struct {
    tModuleKey key;
    uint32_t   paramIndex;
} tParamClickCtx;

static tParamClickCtx     sParamClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS];

typedef struct {
    tModuleKey key;
} tModuleClickCtx;

static tModuleClickCtx    sModuleClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES];

typedef struct {
    tModuleKey key;
    uint32_t   modeIndex;
} tModeClickCtx;

static tModeClickCtx      sModeClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_MODES];

typedef struct {
    tModuleKey key;
    uint32_t   connectorIndex;
} tConnectorClickCtx;

static tConnectorClickCtx sConnectorClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_CONNECTORS];

// Mirrors the params loop previously in mouseHandle.c's
// handle_module_press_for_module()/handle_module_release_for_module() — see
// git history for that code. Only reachable for non-morph modules (this
// handler is only ever registered from render_param_common(), which morph
// groups don't go through), so the paramType-by-location branch those
// functions needed is gone here.
static void param_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    tParamClickCtx * ctx       = (tParamClickCtx *)userData;
    tModule *        module    = get_module(ctx->key);
    uint32_t         slot      = ctx->key.slot;
    uint32_t         variation = gPatchDescr[slot].activeVariation;
    tParam *         param     = &module->param[variation][ctx->paramIndex];
    tParamType       paramType = paramLocationList[param->paramRef].type;

    if (phase == eClickPress) {
        if (  paramType != paramTypeToggle && paramType != paramTypeMenu
           && paramType != paramTypeBypass && paramType != paramTypeEnable
           && paramType != paramTypePush && paramType != paramTypeCustomData) {
            gParamDragging.moduleKey       = module->key;
            gParamDragging.type3           = paramType3Param;
            gParamDragging.param           = ctx->paramIndex;
            gParamDragging.startValue      = param->value;
            gParamDragging.active          = true;
            gParamDragging.startMorphRange = param->morphRange[gMorphGroupFocus];

            if ((synthlib_dial_mode() != eDialModeRotary) || (paramType == paramTypeSlider)) {
                start_cursor_drag();
            }
        } else if (paramType == paramTypePush) {
            send_param_value(slot, module->key, ctx->paramIndex, variation, 0);
            param->value = 0;
        }
    } else if (phase == eClickRelease) {
        if ((paramType == paramTypeMenu) || (paramType == paramTypeCustomData)) {
            open_toggle_menu(coord, module->key, ctx->paramIndex, param->paramRef);
        } else if ((paramType == paramTypeToggle) || (paramType == paramTypeBypass) || (paramType == paramTypeEnable)) {
            uint32_t range       = paramLocationList[param->paramRef].range;
            uint32_t oldParamVal = param->value;

            param->value = (param->value + 1) % range;
            send_param_value(slot, module->key, ctx->paramIndex, variation, param->value);
            undo_push_param_change(module->key, ctx->paramIndex, variation, oldParamVal, param->value);
        } else if (paramType == paramTypePush) {
            uint32_t listSize = array_size_param_location_list();

            for (uint32_t ref = 0; ref < listSize; ref++) {
                if ((paramLocationList[ref].moduleType == module->type) && (paramLocationList[ref].type == paramTypeCustomData)) {
                    send_custom_data_value(slot, module->key);
                    break;
                }
            }

            send_param_value(slot, module->key, ctx->paramIndex, variation, 1);
            param->value = 0;
        }
    }
}

// Mirrors the modes loop previously in mouseHandle.c's
// handle_module_press_for_module()/handle_module_release_for_module() — see
// git history for that code. Modes only ever have two release-relevant
// types (Menu, Toggle); anything else (e.g. paramTypeOscWave) is a plain
// drag, armed on press.
static void mode_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    tModeClickCtx * ctx      = (tModeClickCtx *)userData;
    tModule *       module   = get_module(ctx->key);
    tMode *         mode     = &module->mode[ctx->modeIndex];
    tParamType      modeType = modeLocationList[mode->modeRef].type;

    if (phase == eClickPress) {
        if ((modeType != paramTypeToggle) && (modeType != paramTypeMenu)) {
            memset(&gParamDragging, 0, sizeof(gParamDragging));
            gParamDragging.moduleKey  = module->key;
            gParamDragging.type3      = paramType3Mode;
            gParamDragging.mode       = ctx->modeIndex;
            gParamDragging.startValue = mode->value;
            gParamDragging.active     = true;

            if (synthlib_dial_mode() != eDialModeRotary) {
                start_cursor_drag();
            }
        }
    } else if (phase == eClickRelease) {
        if (modeType == paramTypeMenu) {
            open_mode_toggle_menu(coord, module->key, ctx->modeIndex, mode->modeRef);
        } else if (modeType == paramTypeToggle) {
            uint32_t oldModeVal = mode->value;

            mode->value = (mode->value + 1) % modeLocationList[mode->modeRef].range;
            send_mode_value(ctx->key.slot, module->key, ctx->modeIndex, mode->value);
            undo_push_mode_change(module->key, ctx->modeIndex, oldModeVal, mode->value);
        }
    }
}

// Mirrors the connectors loop previously in mouseHandle.c's
// handle_module_press_for_module() — see git history for that code. Press
// only: connector release (completing a cable) is handled entirely
// separately, by handle_cable_connect() re-scanning every connector against
// the release coord directly — it doesn't care which connector (if any) was
// originally pressed, so it isn't a per-widget dispatch target.
static void connector_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    if (phase != eClickPress) {
        return;
    }
    tConnectorClickCtx * ctx    = (tConnectorClickCtx *)userData;
    tModule *            module = get_module(ctx->key);

    gCableDrag.fromModuleKey      = module->key;
    gCableDrag.fromConnectorIndex = ctx->connectorIndex;
    convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, coord);
    gCableDrag.active             = true;
}

// Mirrors the final "clicking anywhere else on the module body selects
// without starting a drag" fallback previously in mouseHandle.c's
// handle_module_press_for_module() — see git history for that code.
static void module_body_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;

    if (phase != eClickPress) {
        return;
    }
    tModuleClickCtx * ctx             = (tModuleClickCtx *)userData;
    tModule *         module          = get_module(ctx->key);
    bool              multiSelectHeld = glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                                        || glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS
                                        || glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_LEFT_SUPER) == GLFW_PRESS
                                        || glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;

    if (multiSelectHeld) {
        selection_toggle(module->key);
    } else {
        selection_set_single(module->key);
    }
}

// Mirrors the "module->dragArea" branch previously in mouseHandle.c's
// handle_module_press_for_module() — see git history for that code. Registered
// on module->dragArea *after* module_body_click_handler is registered on the
// (larger, overlapping) module->rectangle, so this wins for clicks landing in
// the drag-handle strip, exactly like the old first-match-wins loop order.
static void drag_area_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;

    if (phase != eClickPress) {
        return;
    }
    tModuleClickCtx * ctx             = (tModuleClickCtx *)userData;
    tModule *         module          = get_module(ctx->key);
    bool              multiSelectHeld = glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                                        || glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS
                                        || glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_LEFT_SUPER) == GLFW_PRESS
                                        || glfwGetKey((GLFWwindow *)synthlib_window(), GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;

    if (multiSelectHeld) {
        selection_toggle(module->key);
    } else if (!is_selected(module->key)) {
        selection_set_single(module->key);
    }
    gModuleDrag.moduleKey     = module->key;
    gModuleDrag.isMulti       = is_selected(module->key) && gSelection.count > 1;
    gModuleDrag.prevColumn    = module->column;
    gModuleDrag.prevRow       = module->row;
    gModuleDrag.active        = true;
    gModuleDrag.snapshotCount = 0;

    if (gModuleDrag.isMulti) {
        for (uint32_t si = 0; si < gSelection.count && si < MAX_NUM_MODULES; si++) {
            tModule * sel = get_module(gSelection.keys[si]);

            if (!sel) {
                continue;
            }
            gModuleDrag.snapshotKeys[gModuleDrag.snapshotCount]   = gSelection.keys[si];
            gModuleDrag.snapshotColumn[gModuleDrag.snapshotCount] = sel->column;
            gModuleDrag.snapshotRow[gModuleDrag.snapshotCount]    = sel->row;
            gModuleDrag.snapshotCount++;
        }
    } else {
        gModuleDrag.snapshotKeys[0]   = module->key;
        gModuleDrag.snapshotColumn[0] = module->column;
        gModuleDrag.snapshotRow[0]    = module->row;
        gModuleDrag.snapshotCount     = 1;
    }
}

// Mirrors the morph-specific branch of the params loop previously in
// mouseHandle.c's handle_module_press_for_module()/
// handle_module_release_for_module() (location == locationMorph) — see git
// history for that code. Registered at eClickLayerPanel by
// render_morph_groups() below, not eClickLayerCanvas like every other
// handler in this file — see this file's own top-of-file comment for why.
// userData carries the param index (0..NUM_MORPHS*2-1) as a plain integer,
// not a pointer — the morph module is a fixed singleton ({gSlot,
// locationMorph, 1}), so there's no per-instance context to point to the way
// a regular module's tModuleKey needs.
//
// Unlike a regular module param, morph's own paramType is derived purely
// from which half of the index range i falls in (i < NUM_MORPHS = the dial
// itself, always paramTypeCommonDial; i >= NUM_MORPHS = the knob/morph-name
// label underneath it, always paramTypeToggle) — never from
// paramLocationList[param->paramRef].type the way param_click_handler reads
// it. That collapses the original 3-way paramType branch down to: the dial
// half only ever arms a drag (on press), the label half only ever toggles
// (on release, flipping the isKnob flag render_morph_groups() reads).
static void morph_param_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;
    uint32_t  i         = (uint32_t)(intptr_t)userData;
    tModule * module    = get_module((tModuleKey){gSlot, (uint32_t)locationMorph, 1});
    uint32_t  variation = gPatchDescr[gSlot].activeVariation;
    tParam *  param     = &module->param[variation][i];

    if (phase == eClickPress) {
        if (i < NUM_MORPHS) {
            gParamDragging.moduleKey       = module->key;
            gParamDragging.type3           = paramType3Param;
            gParamDragging.param           = i;
            gParamDragging.startValue      = param->value;
            gParamDragging.active          = true;
            gMorphGroupFocus               = i;
            gParamDragging.startMorphRange = param->morphRange[gMorphGroupFocus];

            if (synthlib_dial_mode() != eDialModeRotary) {
                start_cursor_drag();
            }
        }
    } else if (phase == eClickRelease) {
        if (i >= NUM_MORPHS) {
            uint32_t oldParamVal = param->value;

            param->value = (param->value + 1) % 2;
            send_param_value(gSlot, module->key, i, variation, param->value);
            undo_push_param_change(module->key, i, variation, oldParamVal, param->value);
        }
    }
}

void render_volume_meter(tRectangle rectangle, tVolumeType volumeType, uint32_t value) { // TODO: move to utilsgraphics!?
    switch (volumeType) {
        case volumeTypeCompress:
        {
            tRectangle smallRectangle = rectangle;
            double     space          = 2.0; // TODO: Possibly make a percentage of width
            uint32_t   leds           = 10;

            smallRectangle.coord.y += space;
            smallRectangle.coord.x += space;
            smallRectangle.size.h   = (smallRectangle.size.h - (space * (double)(leds + 1))) / (double)leds;
            smallRectangle.size.w  -= space * 2;

            set_rgb_colour(RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            value                  &= 0x0ff;                         // There's a value of 3 in the high nibble, which is unknown use. Might be an indication of this being individual bit per LED?

            for (int i = 0; i < leds; i++) {
                if ((value >> i) & 0x01) {
                    set_rgb_colour(RGB_GREEN_7);
                } else {
                    set_rgb_colour(RGB_GREEN_3);
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.y += smallRectangle.size.h + space;
            }

            break;
        }

        case volumeTypeMono:
        case volumeTypeStereo:
        case volumeTypeQuad:
        {
            uint32_t level             = value & 0x0f;
            //bool     yellowHold        = ((value >> 4) & 0x03) != 0;
            //bool     hold        = ((value >> 5) & 0x01) != 0;
            bool     clip              = ((value >> 6) & 0x01) != 0;

            double   fullHeight        = rectangle.size.h;
            double   stepHeight        = fullHeight / 12.0;
            int      valueThresholds[] = {7, 11, 12}; // exclusive upper bounds: green/yellow/red
            tRgb     colours[]         = {RGB_GREEN_7, RGB_YELLOW_7, RGB_RED_7};

            set_rgb_colour(RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            double   previousHeight    = 0;

            for (int i = 0; i < 3; i++) {
                int    segmentTopVal     = valueThresholds[i];
                int    segmentBottomVal  = (i == 0) ? 0 : valueThresholds[i - 1];
                int    segmentRange      = segmentTopVal - segmentBottomVal;
                double segmentDrawHeight = 0;

                if ((int)level >= segmentBottomVal) {
                    int drawSteps = ((int)level < segmentTopVal) ? (int)level - segmentBottomVal : segmentRange;
                    segmentDrawHeight = (drawSteps * fullHeight) / 12.0;

                    set_rgb_colour(colours[i]);
                    render_rectangle(
                        moduleArea,
                        {{rectangle.coord.x,
                            rectangle.coord.y + fullHeight - previousHeight - segmentDrawHeight},
                            {rectangle.size.w,
                             segmentDrawHeight}});
                    previousHeight   += segmentDrawHeight;
                }
            }

            // Clip: bright red stripe at the very top of the meter (red zone) when clipping.
            if (clip) {
                set_rgb_colour(RGB_RED_7);
                render_rectangle(moduleArea,
                                 {{rectangle.coord.x, rectangle.coord.y},
                                     {rectangle.size.w, stepHeight}});
            }
            break;
        }

        case volumeTypeSequencer:
        {
            tRectangle smallRectangle = rectangle;
            double     space          = 2.0; // TODO: Possibly make a percentage of width
            uint32_t   leds           = 16;

            smallRectangle.coord.y += space;
            smallRectangle.coord.x += space;
            smallRectangle.size.w   = (smallRectangle.size.w - (space * (double)(leds + 1))) / (double)leds;
            smallRectangle.size.h  -= space * 2;

            set_rgb_colour(RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            value                  &= 0x0ff;                         // There's a value of 3 in the high nibble, which is unknown use. Might be an indication of this being individual bit per LED?

            for (int i = 0; i < leds; i++) {
                if (i == value) {
                    set_rgb_colour(RGB_GREEN_7);
                } else {
                    set_rgb_colour(RGB_GREEN_3);
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.x += smallRectangle.size.w + space;
            }

            break;
        }
        default:
            break;
    }
}

// The assignment label(s) are recorded here (during per-module rendering)
// but only actually painted by render_knob_assignment_overlay(), called once
// the whole frame's modules/cables are drawn — otherwise later components
// (this module's own name/text, or modules drawn afterwards) paint over it.
// A param can be assigned to a local (patch) knob, a Global Parameter Page
// knob, and a MIDI CC all at once — they're independent — so up to one
// overlay row of each is queued and shown stacked.
#define MAX_KNOB_OVERLAYS    3

static int        gKnobOverlayCount                        = 0;
static tRectangle gKnobOverlayRect[MAX_KNOB_OVERLAYS]      = {0};
static char       gKnobOverlayLabel[MAX_KNOB_OVERLAYS][32] = {0};

void render_knob_assignment_overlay(void) {
    for (int i = 0; i < gKnobOverlayCount; i++) {
        draw_button(moduleArea, gKnobOverlayRect[i], gKnobOverlayLabel[i], RGB_GREY_9);
    }
}

// Queues one hover overlay row below the given param rectangle with the
// given (already-formatted) label; additional rows stack further down.
static void queue_overlay_row(tRectangle rectangle, const char * label) {
    if (gKnobOverlayCount >= MAX_KNOB_OVERLAYS) {
        return;
    }
    double     labelWidth = get_text_width(label, (double)STANDARD_BUTTON_TEXT_HEIGHT * 0.8, eCache);
    double     rowHeight  = ((double)STANDARD_TEXT_HEIGHT * 0.8) + 2.0;
    tRectangle labelRect  = {{rectangle.coord.x + (rectangle.size.w - labelWidth) / 2.0,
        rectangle.coord.y + rectangle.size.h + 2.0 + (rowHeight * (double)gKnobOverlayCount)},
        {labelWidth,                                               (double)STANDARD_TEXT_HEIGHT * 0.8}};

    COPY_STRING(gKnobOverlayLabel[gKnobOverlayCount], label);
    gKnobOverlayRect[gKnobOverlayCount] = labelRect;
    gKnobOverlayCount++;
}

// Local/Global knob labels ("A 1 3" / "G A 1 3") from a 0-119 knob index.
static void queue_knob_overlay(tRectangle rectangle, int32_t knobIdx, bool isGlobal) {
    int  page = knobIdx / 24;
    int  bank = (knobIdx % 24) / 8;
    int  pos  = knobIdx % 8;
    char label[16];

    if (isGlobal) {
        snprintf(label, sizeof(label), "G %c %d %d", 'A' + page, bank + 1, pos + 1);
    } else {
        snprintf(label, sizeof(label), "%c %d %d", 'A' + page, bank + 1, pos + 1);
    }
    queue_overlay_row(rectangle, label);
}

// This might be too generic and won't be able to use, or we add extra params!
// TODO: possibly move all the type cases into functions in a new source file, references by function pointer?
void render_param_common(tRectangle rectangle, tModule * module, uint32_t paramRef, uint32_t paramIndex) {
    char     buff[16]                    = {0};
    char     label[CLAVIA_NAME_SIZE + 1] = {0};
    uint32_t slot                        = gSlot;
    uint32_t variation                   = gPatchDescr[slot].activeVariation;
    uint32_t paramValue                  = module->param[variation][paramIndex].value;
    uint32_t morphRange                  = module->param[variation][paramIndex].morphRange[gMorphGroupFocus];

    if (paramValue >= paramLocationList[paramRef].range) {
        LOG_ERROR("Module index %u name %s ParamRef %u ParamIndex %u Value %u > Range %u\n", module->key.index, module->name, paramRef, paramIndex, paramValue, paramLocationList[paramRef].range);
        paramValue = 0;  // If we hit this, the module config needs fixing, but letting it through for now
    }

    if (strlen(module->paramName[paramIndex][0]) > 0) {  // TODO - Work out how labels array works
        COPY_STRING(label, module->paramName[paramIndex][0]);
    } else if (paramLocationList[paramRef].label != NULL) {
        COPY_STRING(label, paramLocationList[paramRef].label);
    }
    label[sizeof(label) - 1]                      = '\0';

    module->param[variation][paramIndex].paramRef = paramRef;

    //LOG_DEBUG("param %u\n", paramValue);

    switch (paramLocationList[paramRef].type) {
        case paramTypeCustomData:
        case paramTypeToggle:
        case paramTypeMenu:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1StandardToggle;

            if (render_param_function != NULL) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        case paramTypeBypass:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Bypass;

            if (render_param_function != NULL) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        case paramTypeEnable:
        case paramTypePush:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Enable;

            if (render_param_function != NULL) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        default:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
            render_param_function = NULL;

            switch (paramLocationList[paramRef].type) {
                case paramTypeFreq:           render_param_function = &render_paramType1Freq;
                    break;
                case paramTypeOscFreq:        render_param_function = &render_paramType1OscFreq;
                    break;
                case paramTypeFine:           render_param_function = &render_paramType1Fine;
                    break;
                case paramTypeGeneralFreq:    render_param_function = &render_paramType1GeneralFreq;
                    break;
                case paramTypeShape:          render_param_function = &render_paramType1Shape;
                    break;
                case paramTypeFreqDrum:       render_param_function = &render_paramType1FreqDrum;
                    break;
                case paramTypeLFORate:        render_param_function = &render_paramType1LFORate;
                    break;
                case paramTypeInt:            render_param_function = &render_paramType1Int;
                    break;
                case paramTypedB:             render_param_function = &render_paramType1dB;
                    break;
                case paramTypeMixLevel:       render_param_function = &render_paramType1MixLevel;
                    break;
                case paramTypeTime:           render_param_function = &render_paramType1Time;
                    break;
                case paramTypeTimeClk:        render_param_function = &render_paramType1TimeClk;
                    break;
                case paramTypeADRTime:        render_param_function = &render_paramType1ADRTime;
                    break;
                case paramTypePulseTime:      render_param_function = &render_paramType1PulseTime;
                    break;
                case paramTypePitch:          render_param_function = &render_paramType1Pitch;
                    break;
                case paramTypeBipLevel:       render_param_function = &render_paramType1BipLevel;
                    break;
                case paramTypeLevAmpDial:     render_param_function = &render_paramType1LevAmpDial;
                    break;
                case paramTypePan:            render_param_function = &render_paramType1Pan;
                    break;
                case paramTypeNoteDial:       render_param_function = &render_paramType1NoteDial;
                    break;
                case paramTypeCommonDial:     // default percent dial
                case paramTypeLRDial:         // pan-type dial
                case paramTypeResonance:      render_param_function = &render_paramType1Resonance;
                    break;
                case paramTypeSlider:         render_param_function = &render_paramType1Slider;
                    break;
                case paramTypeStrMap:         render_param_function = &render_paramType1StrMap;
                    break;
                case paramTypeFreqShift:      render_param_function = &render_paramType1FreqShift;
                    break;
                default:                      LOG_ERROR("Unrecognised paramType %d\n", paramLocationList[paramRef].type);
                    break;
            }

            if (render_param_function != NULL) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5, paramRef);
            }
            break;
        }
    }
    sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex] = (tParamClickCtx){
        module->key, paramIndex
    };
    register_click_region(gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex],
                          eClickLayerCanvas, param_click_handler, &sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex]);
    {
        // A param can be assigned to a local (patch) knob, a Global
        // Parameter Page knob, and a MIDI CC all at the same time — they're
        // independent (the manual counts 120 per Slot plus 120 Global as
        // fully additive, and MIDI CC deassign is keyed purely by CC
        // number, not by param) — so show all rows that apply.
        int32_t localKnobIdx  = find_knob_for_param(module->key.slot, module->key.location,
                                                    module->key.index, paramIndex);
        int32_t globalKnobIdx = find_global_knob_for_param(module->key.slot, module->key.location,
                                                           module->key.index, paramIndex);
        int32_t ccIdx         = find_controller_for_param(module->key.slot, module->key.location,
                                                          module->key.index, paramIndex);

        // Skip the hover check entirely while a cursor-hiding drag (param,
        // tempo, vibrato, glide-time...) is active: the reported cursor
        // position during those is a virtual/relative-delta accumulator,
        // not a real on-screen point, and can drift over an unrelated
        // param — showing its knob/CC overlay instead of (or as well as)
        // the one actually being dragged.
        if ((localKnobIdx >= 0 || globalKnobIdx >= 0 || ccIdx >= 0) && !is_cursor_hidden_dragging()) {
            tCoord mouseCoord = {0};

            get_global_gui_scaled_mouse_coord(&mouseCoord);

            // Don't show a knob/CC overlay for a param that's actually hidden behind the Mutator
            // floater right now.
            if ((gMutator.active && within_rectangle(mouseCoord, gMutator.panelRect))) {
                return;
            }

            if (within_rectangle(mouseCoord, gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex])) {
                if (localKnobIdx >= 0) {
                    queue_knob_overlay(rectangle, localKnobIdx, false);
                }

                if (globalKnobIdx >= 0) {
                    queue_knob_overlay(rectangle, globalKnobIdx, true);
                }

                if (ccIdx >= 0) {
                    char ccLabel[16];

                    snprintf(ccLabel, sizeof(ccLabel), "CC %u", gControllerArray[module->key.slot].controller[ccIdx].midiCC);
                    queue_overlay_row(rectangle, ccLabel);
                }
            }
        }
    }
}

void render_mode_common(tRectangle rectangle, tModule * module, uint32_t modeRef, uint32_t modeIndex) {
    uint32_t modeValue = module->mode[modeIndex].value;

    module->mode[0].modeRef = modeRef;

    switch (modeLocationList[modeRef].type) {
        case paramTypeOscWave:
        {
            char buff[16] = {0};

            snprintf(buff, sizeof(buff), "%u", module->mode[0].value);
            module->mode[modeIndex].rectangle                                                   = render_dial_with_text(moduleArea, rectangle, (char *)modeLocationList[modeRef].label, buff, rectangle.size.h / 4.0, module->mode[0].value, modeLocationList[modeRef].range, 0, RGB_GREY_5); // TODO: Check if Mode can be morphed
            sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex] = (tModeClickCtx){
                module->key, modeIndex
            };
            register_click_region(module->mode[modeIndex].rectangle, eClickLayerCanvas, mode_click_handler,
                                  &sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex]);
            break;
        }
        case paramTypeToggle:
        case paramTypeMenu:
        {
            const char ** strMap     = modeLocationList[modeRef].strMap;
            double        y          = rectangle.coord.y;
            double        textHeight = rectangle.size.h / 2.0;

            //LOG_DEBUG("Mode for module %s\n", gModuleProperties[module->type].name);
            if (strMap == NULL) {
                LOG_ERROR("No strMap for module type %s\n", gModuleProperties[module->type].name);

                //Debug help for value
                char debug[64] = {0};
                snprintf(debug, sizeof(debug), "modeRef %u", modeRef);
                gParamRectangle[module->key.slot][module->key.location][module->key.index][modeIndex] = draw_button(moduleArea, {{rectangle.coord.x, y}, {30, textHeight}}, debug, RGB_BACKGROUND_GREY);
                return;
            }
            //if (paramLocationList[paramRef].colourMap != NULL) {
            //    set_rgb_colour(paramLocationList[paramRef].colourMap[paramValue]);
            //} else {
            //    set_rgb_colour(RGB_BACKGROUND_GREY);
            //}

            module->mode[modeIndex].rectangle                                                   = draw_button(moduleArea, {{rectangle.coord.x, y}, {largest_text_width(modeLocationList[modeRef].range, strMap, textHeight, eCache), textHeight}}, strMap[modeValue], RGB_BACKGROUND_GREY);
            sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex] = (tModeClickCtx){
                module->key, modeIndex
            };
            register_click_region(module->mode[modeIndex].rectangle, eClickLayerCanvas, mode_click_handler,
                                  &sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex]);
            break;
        }
        default:
        {
        }
        break;
    }
}

void render_volume_common(tRectangle rectangle, tModule * module, uint32_t volumeRef, uint32_t volumeIndex) {
    module->volume.volumeRef = volumeRef;

    switch (volumeLocationList[volumeRef].volumeType) {
        case volumeTypeMono:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]);
        }
        break;
        case volumeTypeStereo:
        {
            double space = 2.0;                                                                                // TODO: Possibly make a percentage of width

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[1]);
        }
        break;
        case volumeTypeSequencer:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
        }
        break;
        case volumeTypeQuad:
        {
            double space = 2.0;                                                                                // TODO: Possibly make a percentage of width

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[1]);
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[2]);
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[3]);
        }
        break;
        case volumeTypeCompress:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]);
        }
        break;
        default:
        {
        }
        break;
    }
}

void render_led_common(tRectangle rectangle, tModule * module, uint32_t ledRef, uint32_t ledIndex) {
    module->led.ledRef = ledRef;

    switch (ledLocationList[ledRef].ledType) {
        case ledTypeYes:
        {
            uint32_t ledVal = module->led.value;
            bool     green  = (ledVal >> 1) & 1;
            bool     red    = ledVal & 1;

            if (green && red) {
                set_rgb_colour(RGB_YELLOW_7);
            } else if (green) {
                set_rgb_colour(RGB_GREEN_7);
            } else if (red) {
                set_rgb_colour(RGB_RED_7);
            } else {
                set_rgb_colour(RGB_BLACK);
            }
            render_rectangle(moduleArea, rectangle);
            break;
        }
        case ledTypePark:
        {
            set_rgb_colour(RGB_GREEN_3);
            render_rectangle(moduleArea, rectangle);
            break;
        }
        default:
        {
        }
        break;
    }
}

void render_connector_common(tRectangle rectangle, tModule * module, tConnectorDir dir, tConnectorType type, uint32_t connectorListIndex, uint32_t connectorIndex) {
    if (connectorIndex >= MAX_NUM_CONNECTORS) {
        LOG_ERROR("MAX_NUM_CONNECTORS needs increasing to >= %u\n", connectorIndex + 1);
        exit(1);
    }
    module->connector[connectorIndex].coord = rectangle.coord;  // Register where we're rendering this connector, for cable connecting
    module->connector[connectorIndex].dir   = dir;
    module->connector[connectorIndex].type  = type;

    if (connectorLocationList[connectorListIndex].label != NULL) {
        tRectangle textRectangle = rectangle;
        textRectangle.size.w = BLANK_SIZE;
        textRectangle.size.h = STANDARD_TEXT_HEIGHT;

        set_rgb_colour(RGB_BLACK);

        switch (connectorLocationList[connectorListIndex].labelLoc) {
            case labelLocUp:
                textRectangle.coord.y -= STANDARD_TEXT_HEIGHT; // May need scaling
                break;
            case labelLocDown:
                textRectangle.coord.y += STANDARD_TEXT_HEIGHT;
                break;
            case labelLocLeft:
                textRectangle.coord.x -= (get_text_width((char *)connectorLocationList[connectorListIndex].label, textRectangle.size.h, eCache) + 2);
                textRectangle.coord.y += 2;
                break;
            case labelLocRight:
                textRectangle.coord.x += (rectangle.size.w + 2);
                textRectangle.coord.y += 2;
                break;
        }
        render_text(moduleArea, textRectangle, (char *)connectorLocationList[connectorListIndex].label);
    }
    // Per the G2 manual ("Control signals, blue connectors" / "Logic or gate signals, yellow and
    // orange connectors", g2manual.txt p.135) and confirmed against the original decompiled editor
    // (Original Editor/G2Editor.c — CPnlControlInHole/OutHole::GetColor() and
    // CPnlLogicInHole/OutHole::GetColor(), both bandwidth-dependent; ECableColor's own
    // TurboLogic entry, RGB (1.0, 0.75, 0.31), confirms the exact orange): a module running at the
    // higher (audio) bandwidth promotes its blue (control) connectors to red/Audio, but a yellow
    // (logic) connector instead becomes orange/TurboLogic — still a logic signal, just the
    // higher-bandwidth variant, never plain red. NOT stored back into
    // module->connector[connectorIndex].type above — that field is the connector's permanent
    // declared type, used by protocol.c's own upRate-propagation walk (see its own comment), and
    // must never reflect this purely-cosmetic promotion.
    set_rgb_colour(connectorColourMap[effective_connector_type(type, module->upRate)]);  // Note, was using "module->connector[connectorIndex].type", check that this type param is OK

    if (module->connector[connectorIndex].dir == connectorDirIn) {
        module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, {rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 2.0, 10.0, 0.0, 10.0);
    } else {
        module->connector[connectorIndex].rectangle = render_rectangle(moduleArea, {rectangle.coord, {rectangle.size.w, rectangle.size.h}});
    }
    sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex] = (tConnectorClickCtx){
        module->key, connectorIndex
    };
    register_click_region(module->connector[connectorIndex].rectangle, eClickLayerCanvas, connector_click_handler,
                          &sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex]);
    set_rgb_colour(RGB_BLACK);
    render_circle_part(moduleArea, {rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 4.0, 10.0, 0.0, 10.0);
}

tRectangle adjust_rectangle(tRectangle moduleBase, tRectangle relative, tAnchor anchor, tModule * module) {
    relative = rectangle_scale_from_percent(relative);

    switch (anchor) {
        case anchorTopLeft:
            relative.coord.x = moduleBase.coord.x + relative.coord.x;
            relative.coord.y = moduleBase.coord.y + relative.coord.y;
            break;
        case anchorTopRight:
            relative.coord.x = ((moduleBase.coord.x + moduleBase.size.w) + relative.coord.x) - relative.size.w;
            relative.coord.y = moduleBase.coord.y + relative.coord.y;
            break;
        case anchorTopMiddle:
            relative.coord.x = ((moduleBase.coord.x + (moduleBase.size.w / 2.0)) + relative.coord.x) - (relative.size.w / 2.0);
            relative.coord.y = moduleBase.coord.y + relative.coord.y;
            break;
        case anchorMiddleLeft:
            relative.coord.x = moduleBase.coord.x + relative.coord.x;
            relative.coord.y = ((moduleBase.coord.y + (moduleBase.size.h / 2.0)) + relative.coord.y) - (relative.size.h / 2.0);
            break;
        case anchorMiddleRight:
            relative.coord.x = ((moduleBase.coord.x + moduleBase.size.w) + relative.coord.x) - relative.size.w;
            relative.coord.y = ((moduleBase.coord.y + (moduleBase.size.h / 2.0)) + relative.coord.y) - (relative.size.h / 2.0);
            break;
        case anchorMiddle:
            relative.coord.x = ((moduleBase.coord.x + (moduleBase.size.w / 2.0)) + relative.coord.x) - (relative.size.w / 2.0);
            relative.coord.y = ((moduleBase.coord.y + (moduleBase.size.h / 2.0)) + relative.coord.y) - (relative.size.h / 2.0);
            break;
        case anchorBottomLeft:
            relative.coord.x = moduleBase.coord.x + relative.coord.x;
            relative.coord.y = ((moduleBase.coord.y + moduleBase.size.h) + relative.coord.y) - relative.size.h;
            break;
        case anchorBottomMiddle:
            relative.coord.x = ((moduleBase.coord.x + (moduleBase.size.w / 2.0)) + relative.coord.x) - (relative.size.w / 2.0);
            relative.coord.y = ((moduleBase.coord.y + moduleBase.size.h) + relative.coord.y) - relative.size.h;
            break;
        case anchorBottomRight:
            relative.coord.x = ((moduleBase.coord.x + moduleBase.size.w) + relative.coord.x) - relative.size.w;
            relative.coord.y = ((moduleBase.coord.y + moduleBase.size.h) + relative.coord.y) - relative.size.h;
            break;
    }
    return relative;
}

// Registers module->connector[i].coord (logical, moduleArea-local — the same space cables
// read it back in, applying scale/scroll themselves at draw time) and draws the small
// connector glyphs. Split out of render_module_common() so it can run on its own for a
// module that's currently scrolled off-screen: cables reference connector positions on
// BOTH their endpoint modules regardless of which one (if either) is actually visible right
// now, so this must stay up to date even when the rest of that module's rendering is skipped.
static void render_module_connectors(tRectangle rectangle, tModule * module) {
    uint32_t connector = 0;

    for (uint32_t i = module->connectorIndexCache; i < array_size_connector_location_list(); i++) {
        if (connectorLocationList[i].moduleType == module->type) {
            if (module->gotConnectorIndexCache == false) {
                module->connectorIndexCache    = i;
                module->gotConnectorIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, connectorLocationList[i].rectangle, connectorLocationList[i].anchor, module);
            adjusted.size.h = adjusted.size.w; // We want this one to be square
            render_connector_common(adjusted, module, connectorLocationList[i].direction, connectorLocationList[i].type, i, connector++);

            if (connector >= module_connector_count(module->type)) {
                break;
            }
        }
    }
}

// ── OscShpB waveform preview ─────────────────────────────────────────────────
//
// The manual describes "Waveform Drop-Down Selectors With Graphs" on the G2's
// Shape Oscillators. This was originally built against OscB, using per-
// waveform routines recovered from the decompiled binary
// (CPnlWaveformGraphABC::DrawSine/DrawTri/DrawSaw/DrawSquare/DrawDsf) - but
// on real hardware, Shape turned out to make no audible difference to OscB's
// sin/tri/saw at all (only squ and sup). CPnlOscSinShapeGraph, the one
// decompiled graph-widget class with "Shape" in its name, also never quite
// matched OscB's 5 waveforms - its Draw() dispatches to DrawDualSine/
// DrawDsf/DrawTweekTri/DrawPulse, which fits OscShpB's 8-option waveform
// mode (Sine1-4, TriSaw, DblSaw, Pulse, SymPulse - oscShpBStrMap) far
// better. Moved here on that basis; still an approximation distilled into
// simple closed-form functions rather than a byte-exact port, so treat this
// as a starting point to verify against real hardware, same as before.
// A single cycle that starts at a rising zero-crossing and runs -1..+1..-1..(back to 0), with
// "peak" (0..1) setting where the top of the ramp falls: 0.5 is a symmetric triangle, near 1.0 a
// near-full sawtooth ramp. This is exactly TriSaw's math below, factored out so DblSaw can reuse
// it rather than duplicate a subtly different version of the same thing - both the phase-shift
// (landing a rising, not falling, zero-crossing at phase 0) and the sign flip (matching the real
// editor's orientation) were only worked out and confirmed via TriSaw.
static double skewed_ramp_zero_start(double phase, double peak) {
    double p = fmod(((peak + 1.0) * 0.5) + phase, 1.0);
    double y = (p < peak) ? (((p / peak) * 2.0) - 1.0) : ((((1.0 - p) / (1.0 - peak)) * 2.0) - 1.0);

    return -y;
}

// Shape is always the raw 0-127 param value normalised to 0..1 - but the dial itself only
// *displays* 50%..99% of that (render_paramType1Shape), so Shape 0 is the dial's displayed
// minimum (50%) and Shape 1 its displayed maximum (99%), not "no shaping"/"full shaping" in the
// usual 0-100% sense. TriSaw (case 4, below) is the one case confirmed against the real original
// editor across that whole range; the rest follow its two lessons - Shape 0 (displayed 50%)
// should be the "basic"/symmetric member of the waveform family, Shape 1 (displayed 99%) the
// most extreme one, and the cycle should start at a rising zero-crossing (or, for the pulses,
// at the rising edge itself) rather than a trough - but are otherwise unconfirmed guesses, same
// as TriSaw was before checking it against real hardware.
static double oscshpb_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    switch (waveformIndex) {
        case 0: // Sine1 - plain sine, Shape mostly cosmetic
        {
            return sin(2.0 * M_PI * phase);
        }
        case 1: // Sine2 - a literal "dual sine": a second copy of the same sine, detuned in phase
                // by Shape and summed in - mirrors DblSaw's "dual" construction below rather than
                // blending in a different harmonic
        {
            double detune = shape * 0.15;
            double s1     = sin(2.0 * M_PI * phase);
            double s2     = sin(2.0 * M_PI * (phase + detune));

            return (s1 + s2) * 0.5;
        }
        case 2: // Sine3 - Discrete Summation Formula (Moorer "buzz"), N=1: matches
                // CPnlWaveformGraphABC::DrawDsf's y = sin(theta)/(1-2r*cos(theta)+r^2). Already
                // starts at a rising zero-crossing (numerator sin(theta) is 0 and rising at
                // theta=0 for any r<1) with no extra phase-shift needed.
        {
            double theta = 2.0 * M_PI * phase;
            double r     = fmin(shape, 0.97);
            double denom = 1.0 - (2.0 * r * cos(theta)) + (r * r);
            double y     = (denom > 0.0001) ? ((sin(theta) / denom) * (1.0 - r)) : 0.0;

            return fmax(-1.0, fmin(1.0, y));
        }
        case 3: // Sine4 - same DSF formula as Sine3, but N=2 (matches DrawDsf's param_2=1 vs 2
                // dispatch), so it sharpens into twice as many buzz peaks per cycle
        {
            double theta = 2.0 * M_PI * phase;
            double r     = fmin(shape, 0.97);
            double denom = 1.0 - (2.0 * r * cos(2.0 * theta)) + (r * r);
            double y     = (denom > 0.0001) ? ((sin(theta) / denom) * (1.0 - r)) : 0.0;

            return fmax(-1.0, fmin(1.0, y));
        }
        case 4: // TriSaw - Shape skews the breakpoint from a symmetric triangle towards a sawtooth.
                // Confirmed against the real original editor: Shape at its displayed minimum
                // (50%) is a single-cycle symmetric triangle, and at its displayed maximum (99%)
                // a single-cycle ramp, both starting at a rising zero-crossing.
        {
            double peak = 0.5 + (shape * 0.47); // shape 0 (displayed 50%) -> 0.5 (triangle),
                                                // shape 1 (displayed 99%) -> 0.97 (near-full ramp)

            return skewed_ramp_zero_start(phase, peak);
        }
        case 5: // DblSaw - two near-full ramps summed, the second detuned in phase by Shape (a
                // classic "double saw" richness/detune effect); each ramp individually starts at
                // a rising zero-crossing like TriSaw, though the detuned sum only does so exactly
                // at Shape 0.
        {
            const double peak   = 0.97; // both ramps are near-full sawtooths, not triangles
            double       detune = shape * 0.15;

            return (skewed_ramp_zero_start(phase, peak) + skewed_ramp_zero_start(phase + detune, peak)) * 0.5;
        }
        case 6: // Pulse - pulse width (duty cycle) widens with Shape: Shape 0 (displayed 50%) is
                // a plain symmetric square, Shape 1 (displayed 99%) spends most of the cycle
                // high (the falling edge moves right, cutting into the LOW time, not the high
                // time - confirmed against the real original editor). Already starts right at
                // the rising edge (phase 0 is the first sample of the high part of the cycle).
        {
            double duty = 0.5 + (shape * 0.45); // 50%..95%

            return (phase < duty) ? 1.0 : -1.0;
        }
        case 7: // SymPulse - a duty-symmetric pulse: two equal, evenly-spaced Shape-width pulses
                // per cycle, narrowing with Shape the same way Pulse does (Shape 0 -> two 25%
                // pulses, i.e. 50% total duty; Shape 1 -> two narrow ones)
        {
            double halfDuty = 0.25 - (shape * 0.225); // 25%..2.5%

            return ((phase < halfDuty) || ((phase >= 0.5) && (phase < 0.5 + halfDuty))) ? 1.0 : -1.0;
        }
        default:
            return 0.0;
    }
}

static void render_oscshpb_waveform_graph(tRectangle rectangle, tModule * module) {
    // Shape (param index 6) - fixed position for moduleTypeOscShpB's entries in
    // paramLocationList. Waveform is a MODE here (not a param, unlike OscB) - OscShpB's only
    // mode entry, "Wave" (modeLocationList, oscShpBStrMap), so index 0.
    const uint32_t shapeParamIndex   = 6;
    const uint32_t waveformModeIndex = 0;
    uint32_t       slot              = module->key.slot;
    uint32_t       variation         = gPatchDescr[slot].activeVariation;
    uint32_t       waveformValue     = module->mode[waveformModeIndex].value;
    double         shape             = (double)module->param[variation][shapeParamIndex].value / 127.0;
    tRectangle     graphRect         = adjust_rectangle(rectangle, (tRectangle){{-2, 6}, {30, 10}}, anchorTopRight, module);
    double         midY              = graphRect.coord.y + (graphRect.size.h / 2.0);
    const int      numSamples        = 48;
    const int      numCycles         = 1; // one period across the box, matching the original editor
    tCoord         prev              = {0};

    set_rgb_colour(RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour(RGB_GREY_5);
    render_line(moduleArea, {graphRect.coord.x, midY}, {graphRect.coord.x + graphRect.size.w, midY}, 1.0);

    set_rgb_colour(RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double xFraction = (double)i / (double)numSamples;                 // raw position across the box, 0..1
        double phase     = fmod(xFraction * numCycles, 1.0);               // wrapped per-cycle phase for the sample
        double sample    = oscshpb_waveform_sample(waveformValue, phase, shape);
        tCoord point     = {graphRect.coord.x + (xFraction * graphRect.size.w),
                            graphRect.coord.y + (graphRect.size.h / 2.0) - (sample * graphRect.size.h * 0.45)};

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

void render_module_common(tRectangle rectangle, tModule * module) {
    if (module == NULL) {
        return;
    }
    uint32_t param  = 0;
    uint32_t mode   = 0;
    uint32_t volume = 0;
    uint32_t led    = 0;

    for (uint32_t i = module->paramIndexCache; i < array_size_param_location_list(); i++) {
        if (paramLocationList[i].moduleType == module->type) {
            if (module->gotParamIndexCache == false) {
                module->paramIndexCache    = i;
                module->gotParamIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, paramLocationList[i].rectangle, paramLocationList[i].anchor, module);
            render_param_common(adjusted, module, i, param++);

            if (param >= module_param_count(module->type)) {
                break;
            }
        }
    }

    for (uint32_t i = module->modeIndexCache; i < array_size_mode_location_list(); i++) {
        if (modeLocationList[i].moduleType == module->type) {
            if (module->gotModeIndexCache == false) {
                module->modeIndexCache    = i;
                module->gotModeIndexCache = true;
            }
            //render_mode_common(
            //    {rectangle.coord.x + x_param_pos_from_percent(modeLocationList[i].offsetX), rectangle.coord.y + y_param_pos_from_percent(module->type, modeLocationList[i].offsetY)}, //module, i,
            //    mode++);
            tRectangle adjusted = adjust_rectangle(rectangle, modeLocationList[i].rectangle, modeLocationList[i].anchor, module);
            render_mode_common(adjusted, module, i, mode++);

            if (mode >= module_mode_count(module->type)) {
                break;
            }
        }
    }

    render_module_connectors(rectangle, module);

    if (module->type == moduleTypeOscShpB) {
        render_oscshpb_waveform_graph(rectangle, module);
    }

    for (uint32_t i = module->volumeIndexCache; i < array_size_volume_location_list(); i++) {
        if (volumeLocationList[i].moduleType == module->type) {
            if (module->gotVolumeIndexCache == false) {
                module->volumeIndexCache    = i;
                module->gotVolumeIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, volumeLocationList[i].rectangle, volumeLocationList[i].anchor, module);
            render_volume_common(adjusted, module, i, volume++);

            if (volume >= module_volume_count(module->type)) {
                break;
            }
        }
    }

    for (uint32_t i = module->ledIndexCache; i < array_size_led_location_list(); i++) {
        if (ledLocationList[i].moduleType == module->type) {
            if (module->gotLedIndexCache == false) {
                module->ledIndexCache    = i;
                module->gotLedIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, ledLocationList[i].rectangle, ledLocationList[i].anchor, module);
            adjusted.size.h = adjusted.size.w; // We want this one to be square
            render_led_common(adjusted, module, i, led++);

            if (led >= module_led_count(module->type)) {
                break;
            }
        }
    }
}

void render_module(tModule * module) {
    double     moduleHeight               = gModuleProperties[module->type].height;
    double     xPos                       = module->column * MODULE_X_SPAN;
    double     yPos                       = module->row * MODULE_Y_SPAN;
    double     xWidth                     = MODULE_WIDTH;
    double     yHeight                    = (moduleHeight * MODULE_Y_SPAN) - MODULE_Y_GAP;
    char       buff[CLAVIA_NAME_SIZE + 1] = {0};
    tRgb       rgb                        = {0};

    tRectangle moduleRectangle            = {{xPos, yPos}, {xWidth, yHeight}};

    rgb                                                                        = gModuleColourMap[module->colour];
    set_rgb_colour(rgb);
    module->rectangle                                                          = render_rectangle_with_border(moduleArea, moduleRectangle);

    sModuleClickCtx[module->key.slot][module->key.location][module->key.index] = (tModuleClickCtx){
        module->key
    };
    register_click_region(module->rectangle, eClickLayerCanvas, module_body_click_handler,
                          &sModuleClickCtx[module->key.slot][module->key.location][module->key.index]);

    if (is_selected(module->key)) {
        double t = 2.0;
        double x = moduleRectangle.coord.x;
        double y = moduleRectangle.coord.y;
        double w = moduleRectangle.size.w;
        double h = moduleRectangle.size.h;

        set_rgb_colour(RGB_YELLOW_7);
        render_line(moduleArea, {x, y}, {x + w, y}, t);                 // top
        render_line(moduleArea, {x + w, y}, {x + w, y + h}, t);         // right
        render_line(moduleArea, {x + w, y + h}, {x, y + h}, t);         // bottom
        render_line(moduleArea, {x, y + h}, {x, y}, t);                 // left
    }

    // Patch Mutator: mark excluded modules with a thin red frame, but only while the panel is
    // open - matches the original editor's SetMutaLockVisible (pure display state, not persisted).
    if (gMutator.active && module->excludeFromMutation) {
        double t = 1.0;
        double x = moduleRectangle.coord.x;
        double y = moduleRectangle.coord.y;
        double w = moduleRectangle.size.w;
        double h = moduleRectangle.size.h;

        set_rgb_colour(RGB_RED_7);
        render_line(moduleArea, {x, y}, {x + w, y}, t);                 // top
        render_line(moduleArea, {x + w, y}, {x + w, y + h}, t);         // right
        render_line(moduleArea, {x + w, y + h}, {x, y + h}, t);         // bottom
        render_line(moduleArea, {x, y + h}, {x, y}, t);                 // left
    }
    rgb              = {rgb.red * 1.05, rgb.green * 1.05, rgb.blue * 1.05};
    set_rgb_colour(rgb);
    module->dragArea = render_rectangle(moduleArea, {{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3}, {moduleRectangle.size.w - 6, STANDARD_TEXT_HEIGHT + 2}});
    register_click_region(module->dragArea, eClickLayerCanvas, drag_area_click_handler,
                          &sModuleClickCtx[module->key.slot][module->key.location][module->key.index]);

    render_module_common(moduleRectangle, module);

    if (  gModuleNameEdit.active
       && gModuleNameEdit.moduleKey.slot == module->key.slot
       && gModuleNameEdit.moduleKey.location == module->key.location
       && gModuleNameEdit.moduleKey.index == module->key.index) {
        char     editBuf[CLAVIA_NAME_SIZE + 2] = {0};
        uint32_t cp                            = gModuleNameEdit.cursorPos;
        memcpy(editBuf, gModuleNameEdit.buffer, cp);
        editBuf[cp] = '|';
        memcpy(&editBuf[cp + 1], &gModuleNameEdit.buffer[cp], strlen(gModuleNameEdit.buffer) - cp + 1);

        // Highlight the drag area to show edit mode
        set_rgb_colour(RGB_WHITE);
        render_rectangle(moduleArea, {{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3},
                             {get_text_width(LONGEST_MODULE_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache) + 5, STANDARD_BUTTON_TEXT_HEIGHT + 2}});

        set_rgba_colour(RGBA_BLACK_ON_TRANSPARENT);
        render_text(moduleArea, {{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0},
                        {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, editBuf);
    } else {
        snprintf(buff, sizeof(buff), "%s", module->name);
        set_rgba_colour(RGBA_BLACK_ON_TRANSPARENT);
        render_text(moduleArea, {{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0},
                        {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    }
    // Temporary items purely for development debug
    snprintf(buff, sizeof(buff), "(%s)", gModuleProperties[module->type].name);

    render_text(moduleArea, {{moduleRectangle.coord.x + 180.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    snprintf(buff, sizeof(buff), "%u", module->key.index);
    render_text(moduleArea, {{moduleRectangle.coord.x + moduleRectangle.size.w - 20.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    if (module->modeCount > 0) {
        snprintf(buff, sizeof(buff), "Modes %u", module->modeCount);
        render_text(moduleArea, {{moduleRectangle.coord.x + 250.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    }
}

void render_modules(void) {
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    gKnobOverlayCount = 0; // re-armed below only if a knob-assigned param is under the mouse this frame

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && module->type != moduleTypeUnknown0) {
            // Skip the (relatively expensive, many-sub-element) render for modules currently
            // scrolled entirely outside the visible canvas. Rect here mirrors render_module()'s
            // own moduleRectangle exactly; rectangle_visible_in_module_area() applies the real
            // scale/scroll transform so a module straddling the viewport edge still renders.
            double     moduleHeight    = gModuleProperties[module->type].height;
            tRectangle moduleRectangle = {{module->column * MODULE_X_SPAN, module->row * MODULE_Y_SPAN                  },
                {MODULE_WIDTH,                   (moduleHeight * MODULE_Y_SPAN) - MODULE_Y_GAP}};

            if (!rectangle_visible_in_module_area(moduleRectangle)) {
                // Still off-screen — but cables reference this module's connector positions
                // regardless of whether it's currently visible, so those must stay registered.
                render_module_connectors(moduleRectangle, module);
                continue;
            }
            render_module(module);
        }
    }

    if (gRubberBand.active) {
        double x1 = gRubberBand.start.x < gRubberBand.current.x ? gRubberBand.start.x : gRubberBand.current.x;
        double y1 = gRubberBand.start.y < gRubberBand.current.y ? gRubberBand.start.y : gRubberBand.current.y;
        double x2 = gRubberBand.start.x > gRubberBand.current.x ? gRubberBand.start.x : gRubberBand.current.x;
        double y2 = gRubberBand.start.y > gRubberBand.current.y ? gRubberBand.start.y : gRubberBand.current.y;

        set_rgb_colour(RGB_YELLOW_7);
        render_line(moduleArea, {x1, y1}, {x2, y1}, 1.5); // top
        render_line(moduleArea, {x2, y1}, {x2, y2}, 1.5); // right
        render_line(moduleArea, {x2, y2}, {x1, y2}, 1.5); // bottom
        render_line(moduleArea, {x1, y2}, {x1, y1}, 1.5); // left
    }
    // Draw background areas
    //set_rgb_colour(RGB_RED_7/*RGB_BACKGROUND_GREY*/);
    //tRectangle area        = module_area();
    //render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
    //render_rectangle(mainArea, {{area.coord.x + area.size.w, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, {{0.0, area.coord.y + area.size.h}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
}

void render_cable_from_to(tConnector from, tConnector to, double thickness) {
    tCoord control   = {0};

    from.coord.x += scale_from_percent(CONNECTOR_SIZE / 2.0);
    from.coord.y += scale_from_percent(CONNECTOR_SIZE / 2.0);
    to.coord.x   += scale_from_percent(CONNECTOR_SIZE / 2.0);
    to.coord.y   += scale_from_percent(CONNECTOR_SIZE / 2.0);

    double dy        = to.coord.y - from.coord.y;
    double bowAmount = fmin(fabs(dy) * 0.3, 80.0);

    if (from.coord.x == to.coord.x) {
        // Exactly vertical — bow horizontally so the cable is visible
        control.x = fmax(from.coord.x, to.coord.x) + bowAmount;
    } else {
        // All other cables — gravity sag downward
        control.x = (from.coord.x + to.coord.x) / 2.0;
    }
    control.y     = fmax(from.coord.y, to.coord.y) + 40.0;

    // A quadratic bezier curve is always fully contained within the bounding box of its 3
    // control points, so this bbox — built from the endpoints AND the sag point, not just
    // the endpoints — is a correct (if slightly loose) bound on where the curve can actually
    // fall. Skip the draw if none of that box is visible.
    double minX      = fmin(fmin(from.coord.x, to.coord.x), control.x);
    double maxX      = fmax(fmax(from.coord.x, to.coord.x), control.x);
    double minY      = fmin(fmin(from.coord.y, to.coord.y), control.y);
    double maxY      = fmax(fmax(from.coord.y, to.coord.y), control.y);

    if (!rectangle_visible_in_module_area({{minX, minY}, {maxX - minX, maxY - minY}})) {
        return;
    }
    render_bezier_curve(moduleArea, from.coord, control, to.coord, thickness, 15);
}

static bool cable_touches_hover_connector(tCable * cable) {
    if (!gHoverConnector.active) {
        return false;
    }

    if (cable->key.slot != gHoverConnector.slot || cable->key.location != gHoverConnector.location) {
        return false;
    }

    if (  cable->key.moduleFromIndex == gHoverConnector.moduleIndex
       && cable->key.connectorFromIoCount == gHoverConnector.ioCount
       && cable->key.linkType == (uint32_t)gHoverConnector.dir) {
        return true;
    }

    if (  cable->key.moduleToIndex == gHoverConnector.moduleIndex
       && cable->key.connectorToIoCount == gHoverConnector.ioCount
       && gHoverConnector.dir == connectorDirIn) {
        return true;
    }
    return false;
}

void render_cable(tCable * cable, double alpha) {
    tRgb      colour             = gCableColourMap[cable->colour];

    tModule * moduleFrom         = get_module({cable->key.slot, cable->key.location, cable->key.moduleFromIndex});

    if (moduleFrom == NULL) {
        return;
    }
    tModule * moduleTo           = get_module({cable->key.slot, cable->key.location, cable->key.moduleToIndex});

    if (moduleTo == NULL) {
        return;
    }

    if (alpha < 1.0) {
        glEnable(GL_BLEND);  // TODO - move blend enables to graphics routines
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        set_rgba_colour({colour.red, colour.green, colour.blue, alpha});
    } else {
        set_rgb_colour(colour);
    }
    int       fromConnectorIndex = find_index_from_io_count(moduleFrom, (tConnectorDir)cable->key.linkType, cable->key.connectorFromIoCount);

    int       toConnectorIndex   = find_index_from_io_count(moduleTo, connectorDirIn, cable->key.connectorToIoCount);

    if (fromConnectorIndex != -1 && toConnectorIndex != -1) {
        render_cable_from_to(moduleFrom->connector[fromConnectorIndex], moduleTo->connector[toConnectorIndex], 4.0);
    }

    if (alpha < 1.0) {
        glDisable(GL_BLEND);  // TODO - move blend disable to graphics routines
    }
}

void render_cables(void) {
    uint32_t slot           = gSlot;
    uint32_t location       = gLocation;
    bool     hideAll        = gCablesHideAll;
    bool     allTransparent = gCablesTransparent;
    bool     hoverActive    = gHoverConnector.active;
    double   normalAlpha    = allTransparent ? 0.5 : 1.0;

    if (hideAll) {
        return;
    }

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable         = get_cable_slot(slot, location, i);

        if (cable == NULL || !cable->active) {
            continue;
        }
        bool     colourVisible = gPatchDescr[slot].visible[cable->colour];
        bool     isHovered     = cable_touches_hover_connector(cable);

        if (!colourVisible || (hoverActive && isHovered)) {
            continue;
        }
        render_cable(cable, hoverActive ? 0.2 : normalAlpha);
    }

    if (hoverActive) {
        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable * cable         = get_cable_slot(slot, location, i);

            if (cable == NULL || !cable->active) {
                continue;
            }
            bool     colourVisible = gPatchDescr[slot].visible[cable->colour];
            bool     isHovered     = cable_touches_hover_connector(cable);

            if (!colourVisible || !isHovered) {
                continue;
            }
            render_cable(cable, 1.0);
        }
    }
}

void render_morph_groups(void) {
    tRectangle rectangle        = {{840, 4 + MENU_BAR_HEIGHT}, {STANDARD_TEXT_HEIGHT *2, STANDARD_TEXT_HEIGHT * 4}};
    char       dialValueStr[16] = {0};
    char       label[16]        = {0};
    tRgb       dialColour       = RGB_BACKGROUND_GREY;
    uint32_t   i                = 0;
    uint32_t   j                = 0;
    double     textHeight       = 0.0;
    bool       isKnob           = false;
    uint8_t    dialValue        = 0;
    uint32_t   slot             = gSlot;
    uint32_t   variation        = gPatchDescr[slot].activeVariation;

    tModule *  module           = get_module({slot, (uint32_t)locationMorph, 1});

    if (module != NULL) {
        // Make sure all rectangles (for mouse click) are nullified
        for (i = 0; i < NUM_VARIATIONS_USB; i++) {
            for (j = 0; j < (NUM_MORPHS * 2); j++) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][j] = NULL_RECTANGLE;
            }
        }

        for (i = 0; i < NUM_MORPHS; i++) {
            isKnob                                                                                     = !(module->param[variation][i + NUM_MORPHS].value != 0);
            dialValue                                                                                  = module->param[variation][i].value;

            snprintf(dialValueStr, sizeof(dialValueStr), "%u", dialValue);

            if (isKnob) {
                snprintf(label, sizeof(label), "%s", module->paramName[i + NUM_MORPHS][0]);

                if (label[0] == '\0') {
                    snprintf(label, sizeof(label), "Knob");
                }
            } else {
                snprintf(label, sizeof(label), "%s", morphStrMap[i]);
            }
            textHeight                                                                                 = rectangle.size.h / 4.0;

            set_rgb_colour(RGB_BLACK);
            render_text(mainArea, {{rectangle.coord.x - 3, rectangle.coord.y}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, (char *)morphStrMap[i]);

            if (i == gMorphGroupFocus) {
                dialColour = isKnob ? (tRgb)RGB_ORANGE_0 : (tRgb)RGB_ORANGE_2;
            } else {
                dialColour = RGB_GREY_3;
            }
            gParamRectangle[module->key.slot][module->key.location][module->key.index][i]              = render_dial_with_text(mainArea, {{rectangle.coord.x, rectangle.coord.y + 16}, {rectangle.size.w, rectangle.size.h}}, NULL, dialValueStr, rectangle.size.h / 4.0, module->param[variation][i].value, 128, module->param[variation][i].morphRange[gMorphGroupFocus], dialColour);
            register_click_region(gParamRectangle[module->key.slot][module->key.location][module->key.index][i],
                                  eClickLayerPanel, morph_param_click_handler, (void *)(intptr_t)i);

            if (  gParamNameEdit.active
               && gParamNameEdit.moduleKey.slot == module->key.slot
               && gParamNameEdit.moduleKey.location == module->key.location
               && gParamNameEdit.moduleKey.index == module->key.index
               && gParamNameEdit.paramIndex == i + NUM_MORPHS) {
                char     editBuf[PROTOCOL_PARAM_NAME_SIZE + 2] = {0};
                uint32_t cp                                    = gParamNameEdit.cursorPos;
                memcpy(editBuf, gParamNameEdit.buffer, cp);
                editBuf[cp]        = '|';
                memcpy(&editBuf[cp + 1], &gParamNameEdit.buffer[cp], strlen(gParamNameEdit.buffer) - cp + 1);
                gMorphLabelRect[i] = draw_button(mainArea, {{rectangle.coord.x - 5, rectangle.coord.y + 57}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, editBuf, RGB_WHITE);
            } else {
                gMorphLabelRect[i] = draw_button(mainArea, {{rectangle.coord.x - 5, rectangle.coord.y + 57}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, label, RGB_BACKGROUND_GREY);
            }
            gParamRectangle[module->key.slot][module->key.location][module->key.index][i + NUM_MORPHS] = gMorphLabelRect[i];
            register_click_region(gParamRectangle[module->key.slot][module->key.location][module->key.index][i + NUM_MORPHS],
                                  eClickLayerPanel, morph_param_click_handler, (void *)(intptr_t)(i + NUM_MORPHS));

            rectangle.coord.x                                                                         += (STANDARD_TEXT_HEIGHT * 4) + 5;
        }
    }
}

#ifdef __cplusplus
}
#endif

