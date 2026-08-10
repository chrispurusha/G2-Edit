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
#include "splitView.h"
#include "globalVars.h"
#include "renderParams.h"
#include "mouseHandle.h"
#include "menus.h"
#include "selection.h"
#include "mutatorUI.h"
#include "paramPages.h"
#include "paramOverlay.h"
#include "protocol.h"
#include "undo.h"
#include "canvasDrag.h"
#include "clickRegion.h"
#include "inputState.h"

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
        // The parameter MIDI Learn will act on. Every param type, not just the draggable ones — a
        // button can carry a CC too. This has to live here rather than in mouseHandle.c's
        // handle_module_press_for_module(), which looks like the click path but is legacy fallback:
        // every widget registers a click region at render time and dispatch_click_region() gets
        // there first, so that function no longer runs for a canvas parameter.
        gParamFocus.valid      = true;
        gParamFocus.moduleKey  = module->key;
        gParamFocus.paramIndex = ctx->paramIndex;
        LOG_INFO("Param focus: slot %u location %u module %u param %u (type %u)\n",
                 module->key.slot, module->key.location, module->key.index, ctx->paramIndex, paramType);

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
                canvas_drag_begin();
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
                canvas_drag_begin();
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
    cable_drag_set_end(coord);   // same placement the motion uses, so the end doesn't jump on the first move
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
    bool              multiSelectHeld = multi_select_modifier_held();

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
    bool              multiSelectHeld = multi_select_modifier_held();

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
                canvas_drag_begin();
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
    const tVolumeMeterConfig * config = find_volume_meter_config(volumeType);

    if (config == NULL) {
        return;
    }

    switch (config->style) {
        case volumeMeterStyleMaskLeds:
        {
            tRectangle smallRectangle = rectangle;
            double     space          = config->space;
            uint32_t   leds           = config->segments;

            smallRectangle.coord.y += space;
            smallRectangle.coord.x += space;
            smallRectangle.size.h   = (smallRectangle.size.h - (space * (double)(leds + 1))) / (double)leds;
            smallRectangle.size.w  -= space * 2;

            set_rgb_colour((tRgb)RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            value                  &= 0x0ff;                         // There's a value of 3 in the high nibble, which is unknown use. Might be an indication of this being individual bit per LED?

            for (int i = 0; i < leds; i++) {
                if ((value >> i) & 0x01) {
                    set_rgb_colour(config->onColour);
                } else {
                    set_rgb_colour(config->offColour);
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.y += smallRectangle.size.h + space;
            }

            break;
        }

        case volumeMeterStyleLevelBar:
        {
            uint32_t level             = value & 0x0f;
            //bool     yellowHold        = ((value >> 4) & 0x03) != 0;
            //bool     hold        = ((value >> 5) & 0x01) != 0;
            bool     clip              = ((value >> 6) & 0x01) != 0;

            double   fullHeight        = rectangle.size.h;
            double   stepHeight        = fullHeight / (double)config->segments;
            int      valueThresholds[] = {7, 11, 12}; // exclusive upper bounds: green/yellow/red
            tRgb     colours[]         = {RGB_GREEN_7, (tRgb)RGB_YELLOW_7, RGB_RED_7};

            set_rgb_colour((tRgb)RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            double   previousHeight    = 0;

            for (int i = 0; i < 3; i++) {
                int    segmentTopVal     = valueThresholds[i];
                int    segmentBottomVal  = (i == 0) ? 0 : valueThresholds[i - 1];
                int    segmentRange      = segmentTopVal - segmentBottomVal;
                double segmentDrawHeight = 0;

                if ((int)level >= segmentBottomVal) {
                    int drawSteps = ((int)level < segmentTopVal) ? (int)level - segmentBottomVal : segmentRange;
                    segmentDrawHeight = (drawSteps * fullHeight) / (double)config->segments;

                    set_rgb_colour(colours[i]);
                    render_rectangle(
                        moduleArea,
                        (tRectangle){{rectangle.coord.x,
                                      rectangle.coord.y + fullHeight - previousHeight - segmentDrawHeight},
                                     {rectangle.size.w,
                                      segmentDrawHeight}
                        });
                    previousHeight   += segmentDrawHeight;
                }
            }

            // Clip: bright red stripe at the very top of the meter (red zone) when clipping.
            if (clip) {
                set_rgb_colour((tRgb)RGB_RED_7);
                render_rectangle(moduleArea,
                                 (tRectangle){{rectangle.coord.x, rectangle.coord.y},
                                              {rectangle.size.w, stepHeight}
                                 });
            }
            break;
        }

        case volumeMeterStyleSingleLed:
        {
            tRectangle smallRectangle = rectangle;
            double     space          = config->space;
            uint32_t   leds           = config->segments;

            smallRectangle.coord.y += space;
            smallRectangle.coord.x += space;
            smallRectangle.size.w   = (smallRectangle.size.w - (space * (double)(leds + 1))) / (double)leds;
            smallRectangle.size.h  -= space * 2;

            set_rgb_colour((tRgb)RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            value                  &= 0x0ff;                         // There's a value of 3 in the high nibble, which is unknown use. Might be an indication of this being individual bit per LED?

            for (int i = 0; i < leds; i++) {
                if (i == value) {
                    set_rgb_colour(config->onColour);
                } else {
                    set_rgb_colour(config->offColour);
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.x += smallRectangle.size.w + space;
            }

            break;
        }
    }
}

// This might be too generic and won't be able to use, or we add extra params!
// TODO: possibly move all the type cases into functions in a new source file, references by function pointer?
void render_param_common(tRectangle rectangle, tModule * module, uint32_t paramRef, uint32_t paramIndex) {
    char     buff[16]                    = {0};
    char     label[CLAVIA_NAME_SIZE + 1] = {0};
    // The module's own Slot, not gSlot: identical while rendering the canvas (which only ever
    // draws the selected Slot), but the Parameter Pages panel reuses these widgets to draw a
    // Global page's knobs, and those can point at a module in any of the four Slots - each with
    // its own active Variation. Same reason the renderParams.c widgets read module->key.slot.
    uint32_t slot                        = module->key.slot;
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
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        case paramTypeBypass:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Bypass;

            if (render_param_function != NULL) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        case paramTypeEnable:
        case paramTypePush:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Enable;

            if (render_param_function != NULL) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        default:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
            render_param_function = NULL;

            switch (paramLocationList[paramRef].type) {
                case paramTypeFreq:           render_param_function      = &render_paramType1Freq;
                    break;
                case paramTypeOscFreq:        render_param_function      = &render_paramType1OscFreq;
                    break;
                case paramTypeFine:           render_param_function      = &render_paramType1Fine;
                    break;
                case paramTypeGeneralFreq:    render_param_function      = &render_paramType1GeneralFreq;
                    break;
                case paramTypeShape:          render_param_function      = &render_paramType1Shape;
                    break;
                case paramTypeFreqDrum:       render_param_function      = &render_paramType1FreqDrum;
                    break;
                case paramTypeLFORate:        render_param_function      = &render_paramType1LFORate;
                    break;
                case paramTypeInt:            render_param_function      = &render_paramType1Int;
                    break;
                case paramTypedB:             render_param_function      = &render_paramType1dB;
                    break;
                case paramTypeMixLevel:       render_param_function      = &render_paramType1MixLevel;
                    break;
                case paramTypeTime:           render_param_function      = &render_paramType1Time;
                    break;
                case paramTypeTimeClk:        render_param_function      = &render_paramType1TimeClk;
                    break;
                case paramTypeADRTime:        render_param_function      = &render_paramType1ADRTime;
                    break;
                case paramTypePulseTime:      render_param_function      = &render_paramType1PulseTime;
                    break;
                case paramTypePitch:          render_param_function      = &render_paramType1Pitch;
                    break;
                case paramTypeBipLevel:       render_param_function      = &render_paramType1BipLevel;
                    break;
                case paramTypePartials:       render_param_function      = &render_paramType1Partials;
                    break;
                case paramTypeUniPol:         render_param_function      = &render_paramType1UniPol;
                    break;
                case paramTypeLevAmpDial:     render_param_function      = &render_paramType1LevAmpDial;
                    break;
                case paramTypeResonanceQ:         render_param_function  = &render_paramType1ResonanceQ;
                    break;

                case paramTypeFlangerRate:        render_param_function  = &render_paramType1FlangerRate;
                    break;

                case paramTypePhaserRate:         render_param_function  = &render_paramType1PhaserRate;
                    break;

                case paramTypeSwing:          render_param_function      = &render_paramType1Swing;
                    break;

                case paramTypeBandwidth:      render_param_function      = &render_paramType1Bandwidth;
                    break;

                case paramTypePhase:          render_param_function      = &render_paramType1Phase;
                    break;

                case paramTypePan:            render_param_function      = &render_paramType1Pan;
                    break;
                case paramTypeNoteDial:       render_param_function      = &render_paramType1NoteDial;
                    break;
                case paramTypePShiftSemi:          render_param_function = &render_paramType1PShiftSemi;
                    break;

                case paramTypeBipolarPinned:       render_param_function = &render_paramType1BipolarPinned;
                    break;

                case paramTypePlusMinusUnits:      render_param_function = &render_paramType1PlusMinusUnits;
                    break;

                case paramTypeOffNum:              render_param_function = &render_paramType1OffNum;
                    break;

                case paramTypeScratchRatio:        render_param_function = &render_paramType1ScratchRatio;
                    break;

                case paramTypeSampleRate:          render_param_function = &render_paramType1SampleRate;
                    break;

                case paramTypeThresholdDb:         render_param_function = &render_paramType1ThresholdDb;
                    break;

                case paramTypeBipolar:        // -64..+63, zero at the centre
                case paramTypeLRDial:         // pan dial, same bipolar reading
                    render_param_function                                = &render_paramType1Bipolar;
                    break;

                case paramTypeCommonDial:     // default percent dial
                case paramTypeResonance:      render_param_function      = &render_paramType1Resonance;
                    break;
                case paramTypeSlider:         render_param_function      = &render_paramType1Slider;
                    break;
                case paramTypeStrMap:         render_param_function      = &render_paramType1StrMap;
                    break;
                case paramTypeFreqShift:      render_param_function      = &render_paramType1FreqShift;
                    break;
                default:                      LOG_ERROR("Unrecognised paramType %d\n", paramLocationList[paramRef].type);
                    break;
            }

            if (render_param_function != NULL) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex] = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramRef);
            }
            break;
        }
    }
    sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex] = (tParamClickCtx){
        module->key, paramIndex
    };
    register_click_region(gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex],
                          eClickLayerCanvas, param_click_handler, &sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex]);
    param_overlay_note_param(module, paramIndex, rectangle, buff);
}

void render_mode_common(tRectangle rectangle, tModule * module, uint32_t modeRef, uint32_t modeIndex) {
    uint32_t modeValue = module->mode[modeIndex].value;

    module->mode[0].modeRef = modeRef;

    switch (modeLocationList[modeRef].type) {
        case paramTypeOscWave:
        {
            char       buff[16]     = {0};

            snprintf(buff, sizeof(buff), "%u", module->mode[0].value);
            // render_dial_with_text() is dial-anchored and draws its text upwards, so shift the
            // rect down by the rows this mode will use to keep the block where it was. No entry
            // in modeLocationList is currently an OscWave, so this path is unexercised - it is
            // converted for correctness rather than because anything renders through it today.
            double     modeLabelH   = rectangle.size.h / 4.0;
            tRectangle modeDialRect = rectangle;

            modeDialRect.coord.y                                                               += (modeLocationList[modeRef].label != NULL) ? (modeLabelH * 2.0) : modeLabelH;
            modeDialRect.size.h                                                                 = modeDialRect.size.w;
            module->mode[modeIndex].rectangle                                                   = render_dial_with_text(moduleArea, modeDialRect, (char *)modeLocationList[modeRef].label, buff, modeLabelH, module->mode[0].value, modeLocationList[modeRef].range, 0, (tRgb)RGB_GREY_5); // TODO: Check if Mode can be morphed
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
                gParamRectangle[module->key.slot][module->key.location][module->key.index][modeIndex] = draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {30, textHeight}}, debug, (tRgb)RGB_BACKGROUND_GREY);
                return;
            }
            //if (paramLocationList[paramRef].colourMap != NULL) {
            //    set_rgb_colour(paramLocationList[paramRef].colourMap[paramValue]);
            //} else {
            //    set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
            //}

            // The label sits ABOVE the button, and the button does not move to make room. The dial
            // branch above does the opposite — it offsets the dial downwards — but every position in
            // modeLocationList was laid out against a renderer that drew no label at all, so pushing
            // the buttons down would shift every mode dropdown in the app. Drawing upwards into the
            // space the module already leaves keeps those positions meaning what they always did.
            if (modeLocationList[modeRef].label != NULL) {
                set_rgb_colour((tRgb)RGB_BLACK);
                render_text(moduleArea,
                            (tRectangle){{rectangle.coord.x, y - textHeight}, {rectangle.size.w, textHeight}},
                            (char *)modeLocationList[modeRef].label);
            }
            module->mode[modeIndex].rectangle                                                   = draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {largest_text_width(modeLocationList[modeRef].range, strMap, textHeight, eCache), textHeight}}, strMap[modeValue], (tRgb)RGB_BACKGROUND_GREY);
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
            double space = find_volume_meter_config(volumeTypeStereo)->space;

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
            double space = find_volume_meter_config(volumeTypeQuad)->space;

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
                set_rgb_colour((tRgb)RGB_YELLOW_7);
            } else if (green) {
                set_rgb_colour((tRgb)RGB_GREEN_7);
            } else if (red) {
                set_rgb_colour((tRgb)RGB_RED_7);
            } else {
                set_rgb_colour((tRgb)RGB_BLACK);
            }
            render_rectangle(moduleArea, rectangle);
            break;
        }
        case ledTypePark:
        {
            set_rgb_colour((tRgb)RGB_GREEN_3);
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
    // .dir and .type are deliberately NOT written here any more. They are static per module type
    // and are now filled by populate_module_connectors() when the module enters the database, so
    // they are correct for code that never draws — the sound engine's cable lookups above all.
    // Setting them here as well would restore two owners for one fact, which is how they came to be
    // available only after rendering in the first place. Geometry stays ours; the rest is the
    // resource list's.

    if (connectorLocationList[connectorListIndex].label != NULL) {
        tRectangle textRectangle = rectangle;
        textRectangle.size.w = BLANK_SIZE;
        textRectangle.size.h = STANDARD_TEXT_HEIGHT;

        set_rgb_colour((tRgb)RGB_BLACK);

        switch (connectorLocationList[connectorListIndex].labelLoc) {
            case labelLocUp:
                textRectangle.coord.y -= STANDARD_TEXT_HEIGHT; // May need scaling
                break;
            case labelLocDown:
                // PAST THE CONNECTOR, not by a text height. This used to add STANDARD_TEXT_HEIGHT, which
                // is a measurement of the LABEL and says nothing about how tall the thing it has to clear
                // is — so the glyphs landed on the bottom of the connector circle. labelLocLeft and
                // labelLocRight already step over the connector by its own size.w; this is the same rule
                // on the other axis, and it stays correct whatever CONNECTOR_SIZE or the zoom become.
                //
                // labelLocUp is left as it is: going UP, the distance to clear is the label's own height,
                // because coord.y is the top of the text box.
                textRectangle.coord.y += (rectangle.size.h + 2);
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
    // orange connectors", g2manual.txt p.135) and confirmed against the original editor's behaviour
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

    // The passed-in dir, not module->connector[].dir — they hold the same value from the same
    // connectorLocationList entry, and using the parameter keeps this drawing code independent of
    // when the array happens to have been filled.
    if (dir == connectorDirIn) {
        module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, (tCoord){rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 2.0, 10.0, 0.0, 10.0);
    } else {
        module->connector[connectorIndex].rectangle = render_rectangle(moduleArea, (tRectangle){rectangle.coord, {rectangle.size.w, rectangle.size.h}});
    }
    sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex] = (tConnectorClickCtx){
        module->key, connectorIndex
    };
    register_click_region(module->connector[connectorIndex].rectangle, eClickLayerCanvas, connector_click_handler,
                          &sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex]);
    set_rgb_colour((tRgb)RGB_BLACK);
    render_circle_part(moduleArea, (tCoord){rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 4.0, 10.0, 0.0, 10.0);
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
// waveform routines recovered by inspection
// (CPnlWaveformGraphABC::DrawSine/DrawTri/DrawSaw/DrawSquare/DrawDsf) - but
// on real hardware, Shape turned out to make no audible difference to OscB's
// sin/tri/saw at all (only squ and sup). CPnlOscSinShapeGraph, the one
// reference graph-widget class with "Shape" in its name, also never quite
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

// Ramp width for the Pulse/SymPulse zero-crossing ramps below: half a sample-to-sample step at
// the render loop's 200-sample resolution (must stay narrower than that step or the ramp isn't
// actually sampled at all), capped further so it never eats more than half of whatever room is
// actually available on either side of it (relevant once High/Low get thin near Shape's
// extremes - see SymPulse below).
static double pulse_edge_width(double roomAvailable) {
    return fmin(0.0025, roomAvailable * 0.5);
}

// The two ends of a Pulse/SymPulse ramp: a hard step never actually produces a sample AT zero,
// so both add these explicit narrow ramps at their zero-crossing points instead of relying on
// the step happening to land on a sample.
static double ramp_from_zero(double phaseIntoRamp, double edgeWidth) {
    return phaseIntoRamp / edgeWidth; // 0 -> +1
}

static double ramp_to_zero(double phaseIntoRamp, double edgeWidth) {
    return -1.0 + (phaseIntoRamp / edgeWidth); // -1 -> 0
}

// Shape is always the raw 0-127 param value normalised to 0..1 - but the dial itself only
// *displays* 50%..99% of that (render_paramType1Shape), so Shape 0 is the dial's displayed
// minimum (50%) and Shape 1 its displayed maximum (99%), not "no shaping"/"full shaping" in the
// usual 0-100% sense. Sine1-4/TriSaw/Pulse/SymPulse are all taken directly from the manual's
// "WAVEFORMS AND SHAPES" section for OscShpB (g2manual.txt), which spells out each one's exact
// Shape 50%/75%/99% appearance - not guesses. DblSaw's manual description hasn't been
// reconciled with what's actually implemented below yet (see its own comment).
static double oscshpb_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    switch (waveformIndex) {
        case 0: // Sine1 - "a phase modulated sine wave. At 50% Shape setting, the signal is a
                // perfect sine wave and at 99% similar to a sawtooth wave" (manual). Classic
                // Casio CZ-style phase distortion: warp the phase fed into sin() using a single
                // breakpoint "a" - the SHORT portion compresses the sine's rising lobe into a
                // quick rise near the start (mimicking a saw's sharp edge), the LONG portion
                // stretches the falling lobe into a slow, broad fall (mimicking a saw's long
                // ramp) - confirmed against the real original editor (a fast rise then a long
                // smooth fall, not the other way round). Built from smooth sine curves
                // throughout, so "similar to" rather than an exact sawtooth, matching the
                // manual's wording (contrast TriSaw, which it calls "a perfect Sawtooth" at
                // 99%). "a" only reaches a moderate minimum (not a hard spike) so it stays a
                // "softened saw" - Sine2 below is the one that pushes all the way to an actual
                // narrow spike.
        {
            double a      = 0.5 - (shape * 0.35); // 0.5 (no distortion) .. 0.15 (moderate distortion)
            double warped = (phase < a) ? ((phase / a) * 0.5) : (0.5 + (((phase - a) / (1.0 - a)) * 0.5));

            return sin(2.0 * M_PI * warped);
        }
        case 1: // Sine2 - "a Sine -> Double Sine signal. At 50% Shape setting, the signal is a
                // pure sine wave and at 99% Shape setting, the first half of the period almost
                // covers the entire period length and the second half is a very narrow spike"
                // (manual). Same phase-warp idea as Sine1, but the breakpoint splits the cycle
                // into the sine's own positive/negative lobes rather than splitting where within
                // one continuous curve the warp lands, and is pushed to a much more extreme
                // ratio - the manual explicitly calls for an actual narrow "spike", not just a
                // softened asymmetry like Sine1.
        {
            double lobeWidth = 0.5 + (shape * 0.485); // 0.5 (symmetric) .. 0.985 (a narrow spike)
            double warped    = (phase < lobeWidth) ? ((phase / lobeWidth) * 0.5) : (0.5 + (((phase - lobeWidth) / (1.0 - lobeWidth)) * 0.5));

            return sin(2.0 * M_PI * warped);
        }
        case 2: // Sine3 - "a Sine -> Even harmonics signal. At 50% Shape setting, the signal is
                // a perfect sine wave and at 99% a lot of even harmonics have been added"
                // (manual). A few additive even harmonics (2nd, 4th), growing with Shape.
        {
            double theta = 2.0 * M_PI * phase;
            double y     = sin(theta) + (shape * 0.5 * sin(2.0 * theta)) + (shape * 0.25 * sin(4.0 * theta));

            return y / (1.0 + (shape * 0.75));
        }
        case 3: // Sine4 - "a Sine -> Odd harmonics signal...at 99% a lot of odd harmonics have
                // been added" (manual). Same idea as Sine3, but 3rd/5th harmonics instead of
                // 2nd/4th - odd harmonics preserve half-wave symmetry, so this trends towards a
                // square-ish richness rather than Sine3's saw-ish one.
        {
            double theta = 2.0 * M_PI * phase;
            double y     = sin(theta) + (shape * 0.5 * sin(3.0 * theta)) + (shape * 0.25 * sin(5.0 * theta));

            return y / (1.0 + (shape * 0.75));
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
        case 5: // DblSaw - "Double Saw signal. At 50% Shape setting, the signal consists of two
                // Sawtooth waves in phase with each other, at 75% two Sawtooth waves slightly
                // phase shifted and at 99% two Sawtooth waves 90 degrees phase shifted" (manual)
                // - a pure phase detune between two same-frequency ramps, capped at a quarter
                // cycle, not a frequency change.
        {
            const double peak   = 0.97;         // both saws are near-full sawtooths, not triangles
            double       detune = shape * 0.25; // 0 (in phase) .. 0.25 (90 degrees)

            return (skewed_ramp_zero_start(phase, peak) + skewed_ramp_zero_start(phase + detune, peak)) * 0.5;
        }
        case 6: // Pulse - "a Pulse with selectable ASYMMETRIC pulse width...at 50% Shape
                // setting, the signal is a perfect Square, at 75% a Pulse with 25%/75% pulse
                // width and at 99% a Pulse with 1%/99% pulse width" (manual) - duty is simply
                // the Shape dial's own displayed percentage. The falling edge moves right,
                // cutting into the LOW time, not the high time (confirmed against the real
                // original editor). The High/Low step itself is a hard step that never actually
                // produces a sample AT zero, so the rising crossing that belongs at the wrap
                // seam was invisible - phase 0 and 1 ramp to/from 0.0 explicitly so that
                // crossing is a real plotted point at both ends of the display, instead of
                // starting already at the top. Those two ramps only span half the vertical
                // distance of the middle (high-to-low) transition (0->+1 rather than +1->-1),
                // so they're given half its width too, to come out the same slope rather than
                // looking shallower.
        {
            double duty      = 0.5 + (shape * 0.49); // 50%..99%, matches the Shape dial's own
                                                     // displayed percentage exactly
            double edgeWidth = pulse_edge_width(fmin(duty, 1.0 - duty));

            if (phase < edgeWidth) {
                return ramp_from_zero(phase, edgeWidth);
            }

            if (phase >= (1.0 - edgeWidth)) {
                return ramp_to_zero(phase - (1.0 - edgeWidth), edgeWidth);
            }
            return (phase < duty) ? 1.0 : -1.0;
        }
        case 7: // SymPulse - single cycle: 0 (start) -> ramp -> High (hold) -> Low (hold, a
                // direct step from High, same as Pulse's middle transition) -> ramp -> Zero
                // (hold for the remainder) -> 0 (end). "A Pulse with selectable SYMMETRIC pulse
                // width. At 50% Shape setting, the signal is a perfect Square, at 75% a Pulse
                // with 25% symmetric pulse width and at 99% a Pulse with 1% symmetric pulse
                // width" (manual) - matches this exactly (see the halfSeg rescale below). Same
                // edge-ramp treatment as Pulse above and for the same reason: a hard step never
                // produces an actual sample AT zero, and the half-magnitude ramps (0->+1, -1->0)
                // need half the width of the full-magnitude High->Low step to come out the same
                // slope.
        {
            // High/Low segment length each: rescaled (not clamped) from 0.5 (Shape 0, a perfect
            // square) to 0.01 (Shape 1, a 1% pulse - manual's own number), staying linear the
            // whole way rather than bending flat once it hits the floor. This also reproduces
            // the manual's 75% figure almost exactly (~0.25 at Shape ~0.51, the displayed-75%
            // point) without needing a separate calibration constant.
            const double floor     = 0.01;
            double       halfSeg   = floor + ((0.5 - floor) * (1.0 - shape));
            double       edgeWidth = pulse_edge_width(halfSeg);

            if (phase < edgeWidth) {
                return ramp_from_zero(phase, edgeWidth);
            }

            if (phase < halfSeg) {
                return 1.0;
            }

            if (phase < (2.0 * halfSeg)) {
                return -1.0;
            }

            if (phase < ((2.0 * halfSeg) + edgeWidth)) {
                return ramp_to_zero(phase - (2.0 * halfSeg), edgeWidth);
            }
            return 0.0;
        }
        default:
            return 0.0;
    }
}

static void render_oscshpb_waveform_graph(tRectangle rectangle, tModule * module) {
    // Shape (param index 6) - fixed position for moduleTypeOscShpB's entries in
    // paramLocationList. Waveform is a MODE here (not a param, unlike OscB) - OscShpB's only
    // mode entry, "Wave" (modeLocationList, oscShpBStrMap), so index 0.
    const uint32_t         shapeParamIndex   = 6;
    const uint32_t         waveformModeIndex = 0;
    uint32_t               slot              = module->key.slot;
    uint32_t               variation         = gPatchDescr[slot].activeVariation;
    uint32_t               waveformValue     = module->mode[waveformModeIndex].value;
    double                 shape             = (double)module->param[variation][shapeParamIndex].value / 127.0;
    const tGraphLocation * graphLoc          = find_graph_location(module->type);
    tRectangle             graphRect         = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 midY              = graphRect.coord.y + (graphRect.size.h / 2.0);
    const int              numSamples        = 200; // fine enough to resolve Pulse/SymPulse's narrow
                                                    // sub-sample-width edge ramps, not just the coarser
                                                    // per-cycle shapes
    const int              numCycles         = 1;   // one period across the box, matching the original editor
    tCoord                 prev              = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, midY}, (tCoord){graphRect.coord.x + graphRect.size.w, midY}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double xFraction = (double)i / (double)numSamples;                 // raw position across the box, 0..1
        double phase     = fmod(xFraction * numCycles, 1.0);               // wrapped per-cycle phase for the sample
        double sample    = oscshpb_waveform_sample(waveformValue, phase, shape);
        tCoord point     = {
            graphRect.coord.x + (xFraction * graphRect.size.w),
            graphRect.coord.y + (graphRect.size.h / 2.0) - (sample * graphRect.size.h * 0.45)
        };

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

// ── EnvADSR envelope preview ─────────────────────────────────────────────────
//
// A small live graph of the classic Attack/Decay/Sustain/Release envelope shape, same spirit
// and roughly the same size as the OscShpB waveform preview above. Segment WIDTHS are drawn
// from each knob's own raw value independently (not real time - Attack/Decay/Release span
// 0.5ms to 45s each, far too wide a dynamic range to draw to scale in a
// small box; and not normalised against each other either, since dividing by the sum of all
// three made each segment's width depend on the OTHER two as well as its own knob, saturating
// quickly enough that sweeping one knob only looked like it had two states). The Sustain
// plateau's HEIGHT reflects its level directly (it's a level, not a timed phase, so it gets a
// fixed display width just to show the hold clearly).
//
// Both Env Shape and Output Type are taken from the manual's "COMMON ENVELOPE GENERATOR
// PARAMETERS" section (g2manual.txt), not guesses:
//
// - SHAPE SCROLL BUTTON (envShapeStrMap - "there are four alternatives: Logarithmic Attack &
//   Exponential Decay/Release, Linear Attack & Exponential Decay/Release, Exponential Attack &
//   Decay/Release and Linear Attack & Decay/Release") - so the first word of each is Attack's
//   curve, the second is Decay+Release's. Log (the default) is concave (fast then levelling -
//   confirmed against the real original editor); Exp attack is its mirror, convex (slow then a
//   fast final approach); Decay/Release's Exp is concave, matching a capacitor discharging.
// - OUTPUT TYPE SCROLL BUTTON ("Pos: 0 up to +64 then down to 0. PosInv: +64 down to 0 then up
//   to +64, i.e. inverted. Neg/NegInv mirror Pos/PosInv into negative range. Bip/BipInv: bipolar,
//   sustain level fixed at 0 (ignoring the Sustain knob)"). Implemented as one shape computed in
//   Pos's own convention, then optionally reflected (1-x, for PosInv/Neg) and/or negated (for
//   Neg/NegInv/BipInv) - Bip/BipInv additionally swap in a fixed 0 sustain and let Release run
//   on to -1 instead of stopping at 0.
// - GRAPHS ("Any sustain level is indicated with an orange line; the rest...are green. There is
//   also a yellow horizontal line which indicates the zero level") - colours below match this
//   directly.
static double envadsr_exp_decay(double t, double levelStart, double levelEnd) {
    const double k    = 4.0; // decay sharpness - normalised below so the curve still lands exactly
                             // on levelStart/levelEnd at t=0/1 despite exp() itself being asymptotic
    double       raw  = exp(-k * t) - exp(-k);
    double       norm = 1.0 - exp(-k);

    return levelEnd + ((levelStart - levelEnd) * (raw / norm));
}

static double envadsr_exp_accel(double t, double levelStart, double levelEnd) {
    const double k    = 4.0; // mirror of envadsr_exp_decay's normalisation - grows slowly at
                             // first, then accelerates near the end (Exp attack, not the Log
                             // default)
    double       raw  = exp(k * t) - 1.0;
    double       norm = exp(k) - 1.0;

    return levelStart + ((levelEnd - levelStart) * (raw / norm));
}

// Attack curve types (envShapeStrMap's first word): 0=Log (default), 1=Lin, 2=Exp, 3=Lin.
static double envadsr_attack_level(double t, uint32_t envShapeIndex) {
    switch (envShapeIndex) {
        case 1:
        case 3: return t;                               // Lin

        case 2: return envadsr_exp_accel(t, 0.0, 1.0);  // Exp - convex

        default: return envadsr_exp_decay(t, 0.0, 1.0); // Log - concave
    }
}

// Decay/Release curve types (envShapeStrMap's second word): 0-2=Exp, 3=Lin.
static double envadsr_decay_level(double t, double levelStart, double levelEnd, uint32_t envShapeIndex) {
    if (envShapeIndex == 3) {
        return levelStart + ((levelEnd - levelStart) * t); // Lin
    }
    return envadsr_exp_decay(t, levelStart, levelEnd); // Exp - concave
}

// Maps an envelope "shape" value (0 at the start, 1 at the attack peak) to a y coordinate.
// "shape" is always expressed in Pos's own convention; each Output Type converts it to what it
// actually outputs (per the manual's six descriptions) before the mapping.
static double env_level_to_y(double shape, uint32_t outputType, double zeroY, double fullSwing) {
    double actualLevel;

    switch (outputType) {
        case 1: actualLevel  = 1.0 - shape;
            break;                                   // PosInv

        case 2: actualLevel  = shape - 1.0;
            break;                                   // Neg

        case 3: actualLevel  = -shape;
            break;                                   // NegInv

        case 5: actualLevel  = -shape;
            break;                                   // BipInv

        default: actualLevel = shape;
            break;                                   // Pos, Bip
    }
    return zeroY - (actualLevel * fullSwing);
}

static void render_envadsr_graph(tRectangle rectangle, tModule * module) {
    // Env Shape (index 0), Attack (1), Decay (2), Sustain (3), Release (4), Output Type (5) -
    // fixed positions for moduleTypeEnvADSR's entries in paramLocationList, see moduleResources.h.
    const uint32_t         envShapeParamIndex   = 0;
    const uint32_t         attackParamIndex     = 1;
    const uint32_t         decayParamIndex      = 2;
    const uint32_t         sustainParamIndex    = 3;
    const uint32_t         releaseParamIndex    = 4;
    const uint32_t         outputTypeParamIndex = 5;
    uint32_t               slot                 = module->key.slot;
    uint32_t               variation            = gPatchDescr[slot].activeVariation;
    uint32_t               envShapeIndex        = module->param[variation][envShapeParamIndex].value;
    double                 attackVal            = (double)module->param[variation][attackParamIndex].value / 127.0;
    double                 decayVal             = (double)module->param[variation][decayParamIndex].value / 127.0;
    double                 sustainLevel         = (double)module->param[variation][sustainParamIndex].value / 127.0;
    double                 releaseVal           = (double)module->param[variation][releaseParamIndex].value / 127.0;
    uint32_t               outputType           = module->param[variation][outputTypeParamIndex].value; // 0=Pos,
                                                                                                        // 1=PosInv, 2=Neg,
                                                                                                        // 3=NegInv, 4=Bip,
                                                                                                        // 5=BipInv
    bool                   isBip                = (outputType == 4) || (outputType == 5);
    bool                   isNegFamily          = (outputType == 2) || (outputType == 3);

    // Bip/BipInv ignore the Sustain knob (fixed at the centre level instead) and Release
    // continues on past that centre to the opposite extreme, rather than stopping there.
    double                 effectiveSustain     = isBip ? 0.0 : sustainLevel;
    double                 releaseTarget        = isBip ? -1.0 : 0.0;

    // Centred horizontally, vertically aligned with the KB Active toggle's own y offset (8).
    const tGraphLocation * graphLoc             = find_graph_location(module->type);
    tRectangle             graphRect            = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);

    const double           holdWidth            = 0.24; // fixed width just to show the Sustain plateau clearly

    // Each segment's width comes from its OWN knob only, independent of the other two - 0.04
    // (raw minimum, still clearly visible rather than a zero-width vertical line) up to 0.24
    // (raw maximum). Whatever's left of the box after Attack+Decay+Release+the fixed Sustain
    // width is just background.
    double                 attackW              = 0.04 + (attackVal * 0.20);
    double                 decayW               = 0.04 + (decayVal * 0.20);
    double                 releaseW             = 0.04 + (releaseVal * 0.20);
    double                 x0                   = graphRect.coord.x;

    // Where "envelope output = 0" sits, and how much of the box height its full swing covers -
    // Pos/PosInv only ever go 0..+1 (manual: "0 units...up to +64"), so zero sits at the
    // bottom; Neg/NegInv only ever go 0..-1, so zero sits at the top; only Bip/BipInv are
    // genuinely bipolar, spanning -1..+1 around a centred zero.
    double                 zeroY                = isBip ? (graphRect.coord.y + (graphRect.size.h * 0.5)) : (isNegFamily ? graphRect.coord.y : (graphRect.coord.y + graphRect.size.h));
    double                 fullSwing            = isBip ? (graphRect.size.h * 0.5) : graphRect.size.h;

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_YELLOW_7);
    render_line(moduleArea, (tCoord){x0, zeroY}, (tCoord){x0 + graphRect.size.w, zeroY}, 1.0);

    // "shape" (0 at the start, 1 at the attack peak, effectiveSustain during hold, releaseTarget
    // at the end) is always expressed in Pos's own convention; convert it to what each Output
    // Type actually outputs (per the manual's six descriptions) before mapping to a y coordinate.

    tCoord                 p0                   = {x0, env_level_to_y(0.0, outputType, zeroY, fullSwing)};
    tCoord                 p1                   = {x0 + (attackW * graphRect.size.w), env_level_to_y(1.0, outputType, zeroY, fullSwing)};
    tCoord                 p2                   = {p1.x + (decayW * graphRect.size.w), env_level_to_y(effectiveSustain, outputType, zeroY, fullSwing)};
    tCoord                 p3                   = {p2.x + (holdWidth * graphRect.size.w), p2.y};
    tCoord                 p4                   = {p3.x + (releaseW * graphRect.size.w), env_level_to_y(releaseTarget, outputType, zeroY, fullSwing)};

    const int              numCurveSteps        = 12;
    tCoord                 prev                 = p0;

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 1; i <= numCurveSteps; i++) {
        double t     = (double)i / (double)numCurveSteps;
        double level = envadsr_attack_level(t, envShapeIndex);
        tCoord point = {p0.x + (t * (p1.x - p0.x)), env_level_to_y(level, outputType, zeroY, fullSwing)};

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }

    for (int i = 1; i <= numCurveSteps; i++) {
        double t     = (double)i / (double)numCurveSteps;
        double level = envadsr_decay_level(t, 1.0, effectiveSustain, envShapeIndex);
        tCoord point = {p1.x + (t * (p2.x - p1.x)), env_level_to_y(level, outputType, zeroY, fullSwing)};

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }

    set_rgb_colour((tRgb)RGB_ORANGE_1); // sustain segment - matches the original editor's own colouring
    render_line(moduleArea, p2, p3, 1.5);

    set_rgb_colour((tRgb)RGB_GREEN_ON);
    prev = p3;

    for (int i = 1; i <= numCurveSteps; i++) {
        double t     = (double)i / (double)numCurveSteps;
        double level = envadsr_decay_level(t, effectiveSustain, releaseTarget, envShapeIndex);
        tCoord point = {p3.x + (t * (p4.x - p3.x)), env_level_to_y(level, outputType, zeroY, fullSwing)};

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }
}

// ── FltClassic response preview ──────────────────────────────────────────────
//
// A small live graph of the classic lowpass filter's frequency response, using the real 2-pole
// resonant lowpass magnitude formula |H(f)|^2 = 1 / [(1-(f/fc)^2)^2 + (f/(fc*Q))^2] rather than
// stitching together a flat line, a Gaussian "bump" and a separate linear rolloff - passband,
// resonant peak (taller and narrower as Q increases, per the manual's "narrow resonance
// peak...similar to...analog ladder filters") and rolloff all fall out of the one formula as a
// single smooth curve. 18/24 dB/octave are modelled as extra plain one-pole rolloff stages
// cascaded onto the base 2-pole (12dB) response, which also naturally narrows the peak further
// at higher slopes, matching real higher-order filter behaviour. Q itself uses the real 0.5..50
// range from flt_resonance_q(). The
// X axis is ~10 octaves of relative frequency (roughly matching Freq's own real ~14Hz..21kHz
// exponential range) rather than a literal Hz-calibrated Bode plot - the box is too small to be
// literal about that regardless of curve shape. The cutoff is inset into a band across the box
// (not mapped edge-to-edge) so the resonance peak always keeps headroom either side - see the
// cutoff calc below.
static void render_fltclassic_response_graph(tRectangle rectangle, tModule * module) {
    // Freq (index 0), Res (index 3), dB/octave slope (index 4) - fixed positions for
    // moduleTypeFltClassic's entries in paramLocationList, see moduleResources.h.
    const uint32_t         freqParamIndex  = 0;
    const uint32_t         resParamIndex   = 3;
    const uint32_t         slopeParamIndex = 4;
    uint32_t               slot            = module->key.slot;
    uint32_t               variation       = gPatchDescr[slot].activeVariation;
    double                 cutoffKnob      = (double)module->param[variation][freqParamIndex].value / 127.0;

    // Inset the cutoff's on-screen position into a band rather than mapping the knob edge-to-edge, so
    // the resonance peak (and the passband/left flank below it) always keeps horizontal headroom. At
    // min Freq the peak used to sit hard against the left edge with its lower half off-screen. This is
    // a small horizontal zoom-out: the box now shows a slightly wider window than the knob's own
    // ~14Hz..21kHz range so the curve never runs off either edge.
    const double           kCutoffMinX     = 0.15;                                            // min-Freq cutoff sits 15% in from the left edge
    const double           kCutoffMaxX     = 0.90;                                            // max-Freq cutoff sits 10% in from the right edge
    double                 cutoffX         = kCutoffMinX + (cutoffKnob * (kCutoffMaxX - kCutoffMinX));
    uint32_t               slopeIndex      = module->param[variation][slopeParamIndex].value; // 0=12dB, 1=18dB, 2=24dB

    // Shared with the dial text and the sound engine — see renderParams.h. What the curve draws and
    // what the engine plays come from one definition.
    double                 q               = flt_resonance_q((double)module->param[variation][resParamIndex].value);
    int                    extraPoles      = (int)flt_slope_extra_poles(slopeIndex);           // 12/18/24 dB/octave

    const tGraphLocation * graphLoc        = find_graph_location(module->type);
    tRectangle             graphRect       = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 baseY           = graphRect.coord.y + (graphRect.size.h * 0.6); // 0dB reference, leaving
                                                                                           // headroom above for the
                                                                                           // resonance peak to rise into
    const int              numSamples      = 100;
    tCoord                 prev            = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, baseY}, (tCoord){graphRect.coord.x + graphRect.size.w, baseY}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double x         = (double)i / (double)numSamples;
        double octaves   = (x - cutoffX) * 10.0;   // ~10 octaves span the box, matching Freq's own real range
        double ratio     = pow(2.0, octaves);      // f/fc
        double ratioSq   = ratio * ratio;

        double denomSq   = ((1.0 - ratioSq) * (1.0 - ratioSq)) + ((ratio / q) * (ratio / q));
        double magnitude = 1.0 / sqrt(fmax(denomSq, 1e-6));

        if (extraPoles > 0) {
            magnitude /= sqrt(1.0 + pow(ratio, 2.0 * extraPoles));
        }
        double levelDb   = 20.0 * log10(fmax(magnitude, 1e-4));
        double level     = fmax(-1.0, fmin(1.0, levelDb / 24.0)); // +-24dB fills the box vertically

        // Scale must stay within min(baseY's own fraction, 1-that fraction) - 0.6/0.55 didn't
        // (0.6+0.55 = 1.15), which is exactly why the rolloff (level -> -1) was plotting below
        // the box's bottom edge; 0.38 fits both the 0.6-above and 0.4-below headroom baseY
        // leaves either side of it.
        tCoord point     = {graphRect.coord.x + (x * graphRect.size.w), baseY - (level * graphRect.size.h * 0.38)};

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }

        if (level <= -1.0) {
            break; // fully rolled off - stop rather than trailing a flat line along the bottom edge
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

    if (module->type == moduleTypeEnvADSR) {
        render_envadsr_graph(rectangle, module);
    }

    if (module->type == moduleTypeFltClassic) {
        render_fltclassic_response_graph(rectangle, module);
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

        set_rgb_colour((tRgb)RGB_YELLOW_7);
        render_line(moduleArea, (tCoord){x, y}, (tCoord){x + w, y}, t);                 // top
        render_line(moduleArea, (tCoord){x + w, y}, (tCoord){x + w, y + h}, t);         // right
        render_line(moduleArea, (tCoord){x + w, y + h}, (tCoord){x, y + h}, t);         // bottom
        render_line(moduleArea, (tCoord){x, y + h}, (tCoord){x, y}, t);                 // left
    }

    // Patch Mutator: mark excluded modules with a thin red frame, but only while the panel is
    // open - matches the original editor's SetMutaLockVisible (pure display state, not persisted).
    if (gMutator.active && module->excludeFromMutation) {
        double t = 1.0;
        double x = moduleRectangle.coord.x;
        double y = moduleRectangle.coord.y;
        double w = moduleRectangle.size.w;
        double h = moduleRectangle.size.h;

        set_rgb_colour((tRgb)RGB_RED_7);
        render_line(moduleArea, (tCoord){x, y}, (tCoord){x + w, y}, t);                 // top
        render_line(moduleArea, (tCoord){x + w, y}, (tCoord){x + w, y + h}, t);         // right
        render_line(moduleArea, (tCoord){x + w, y + h}, (tCoord){x, y + h}, t);         // bottom
        render_line(moduleArea, (tCoord){x, y + h}, (tCoord){x, y}, t);                 // left
    }
    rgb              = (tRgb){
        rgb.red * 1.05, rgb.green * 1.05, rgb.blue * 1.05
    };
    set_rgb_colour(rgb);
    module->dragArea = render_rectangle(moduleArea, (tRectangle){{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3}, {moduleRectangle.size.w - 6, STANDARD_TEXT_HEIGHT + 2}});
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
        set_rgb_colour((tRgb)RGB_WHITE);
        render_rectangle(moduleArea, (tRectangle){{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3},
                                                  {get_text_width(LONGEST_MODULE_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache) + 5, STANDARD_BUTTON_TEXT_HEIGHT + 2}
                         });

        set_rgba_colour((tRgba)RGBA_BLACK_ON_TRANSPARENT);
        render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0},
                                             {BLANK_SIZE, STANDARD_TEXT_HEIGHT}
                    }, editBuf);
    } else {
        snprintf(buff, sizeof(buff), "%s", module->name);
        set_rgba_colour((tRgba)RGBA_BLACK_ON_TRANSPARENT);
        render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0},
                                             {BLANK_SIZE, STANDARD_TEXT_HEIGHT}
                    }, buff);
    }
    // Temporary items purely for development debug
    snprintf(buff, sizeof(buff), "(%s)", gModuleProperties[module->type].name);

    render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 180.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    snprintf(buff, sizeof(buff), "%u", module->key.index);
    render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + moduleRectangle.size.w - 20.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    // Mode count — debug only, and the one of these three that says nothing a user would want:
    // the type name and the index both identify the module, this just counts its mode entries.
    // Commented rather than deleted so it can go back for protocol work on modes.
    //if (module->modeCount > 0) {
    //    snprintf(buff, sizeof(buff), "Modes %u", module->modeCount);
    //    render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 250.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    //}
}

void render_modules(void) {
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    // param_overlay_begin_frame() used to live here. It moved out to render_frame() when the canvas
    // gained panes: this function now runs once PER PANE, and resetting the overlay queue between
    // panes would throw away everything the first pane had just queued.

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

    // Only in the pane it was started in. This function runs once per pane now, and the band's
    // coordinates are module-space for ONE pane — drawing it in the other would put a second
    // rectangle at the same module coordinates in the wrong half of the window.
    if (gRubberBand.active && (module_pane() == split_view_focused_pane())) {
        double x1 = gRubberBand.start.x < gRubberBand.current.x ? gRubberBand.start.x : gRubberBand.current.x;
        double y1 = gRubberBand.start.y < gRubberBand.current.y ? gRubberBand.start.y : gRubberBand.current.y;
        double x2 = gRubberBand.start.x > gRubberBand.current.x ? gRubberBand.start.x : gRubberBand.current.x;
        double y2 = gRubberBand.start.y > gRubberBand.current.y ? gRubberBand.start.y : gRubberBand.current.y;

        set_rgb_colour((tRgb)RGB_YELLOW_7);
        render_line(moduleArea, (tCoord){x1, y1}, (tCoord){x2, y1}, 1.5); // top
        render_line(moduleArea, (tCoord){x2, y1}, (tCoord){x2, y2}, 1.5); // right
        render_line(moduleArea, (tCoord){x2, y2}, (tCoord){x1, y2}, 1.5); // bottom
        render_line(moduleArea, (tCoord){x1, y2}, (tCoord){x1, y1}, 1.5); // left
    }
    // Draw background areas
    //set_rgb_colour((tRgb)RGB_RED_7/*RGB_BACKGROUND_GREY*/);
    //tRectangle area        = module_area();
    //render_rectangle(mainArea, (tRectangle){{0.0, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, (tRectangle){{0.0, area.coord.y - MODULE_MARGIN}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
    //render_rectangle(mainArea, (tRectangle){{area.coord.x + area.size.w, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, (tRectangle){{0.0, area.coord.y + area.size.h}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
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

    if (!rectangle_visible_in_module_area((tRectangle){{minX, minY}, {maxX - minX, maxY - minY}})) {
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

    tModule * moduleFrom         = get_module((tModuleKey){cable->key.slot, cable->key.location, cable->key.moduleFromIndex});

    if (moduleFrom == NULL) {
        return;
    }
    tModule * moduleTo           = get_module((tModuleKey){cable->key.slot, cable->key.location, cable->key.moduleToIndex});

    if (moduleTo == NULL) {
        return;
    }

    if (alpha < 1.0) {
        glEnable(GL_BLEND);  // TODO - move blend enables to graphics routines
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        set_rgba_colour((tRgba){colour.red, colour.green, colour.blue, alpha});
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

// Which source a morph group is taking, given its selector value. Every group offers the knob at 0
// and one named alternative at 1 - except the FIFTH, which has a third: the first global wheel.
// Treating any non-zero value as "the" named source showed a group 5 morph assigned to G.Wh 1 as
// Sust.Pd instead.
#define MORPH_GROUP_WITH_THIRD_SOURCE    (4)     // zero-based, i.e. the fifth group

static const char * morph_source_name(uint32_t group, uint32_t sourceValue) {
    if ((group == MORPH_GROUP_WITH_THIRD_SOURCE) && (sourceValue > 1)) {
        return "G.Wh 1";
    }
    return morphStrMap[group];
}

void render_morph_groups(void) {
    tRectangle rectangle        = {{840, 4 + MENU_BAR_HEIGHT}, {STANDARD_TEXT_HEIGHT *2, STANDARD_TEXT_HEIGHT * 4}};
    char       dialValueStr[16] = {0};
    char       label[16]        = {0};
    tRgb       dialColour       = (tRgb)RGB_BACKGROUND_GREY;
    uint32_t   i                = 0;
    uint32_t   j                = 0;
    double     textHeight       = 0.0;
    bool       isKnob           = false;
    uint8_t    dialValue        = 0;
    uint32_t   slot             = gSlot;
    uint32_t   variation        = gPatchDescr[slot].activeVariation;

    tModule *  module           = get_module((tModuleKey){slot, (uint32_t)locationMorph, 1});

    if (module != NULL) {
        // Make sure all rectangles (for mouse click) are nullified
        for (i = 0; i < NUM_VARIATIONS_USB; i++) {
            for (j = 0; j < (NUM_MORPHS * 2); j++) {
                gParamRectangle[module->key.slot][module->key.location][module->key.index][j] = (tRectangle)NULL_RECTANGLE;
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
                snprintf(label, sizeof(label), "%s", morph_source_name(i, module->param[variation][i + NUM_MORPHS].value));
            }
            textHeight                                                                                 = rectangle.size.h / 4.0;

            set_rgb_colour((tRgb)RGB_BLACK);
            render_text(mainArea, (tRectangle){{rectangle.coord.x - 3, rectangle.coord.y}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, (char *)morphStrMap[i]);

            if (i == gMorphGroupFocus) {
                dialColour = isKnob ? (tRgb)RGB_ORANGE_0 : (tRgb)RGB_ORANGE_2;
            } else {
                dialColour = (tRgb)RGB_GREY_3;
            }
            // + textHeight on top of the existing + 16 because render_dial_with_text() is
            // dial-anchored: the rect is now the circle and the value string is drawn in the row
            // above it, where it previously started at the rect's own y.
            gParamRectangle[module->key.slot][module->key.location][module->key.index][i]              = render_dial_with_text(mainArea, (tRectangle){{rectangle.coord.x, rectangle.coord.y + 16 + textHeight}, {rectangle.size.w, rectangle.size.w}}, NULL, dialValueStr, textHeight, module->param[variation][i].value, 128, module->param[variation][i].morphRange[gMorphGroupFocus], dialColour);
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
                gMorphLabelRect[i] = draw_button(mainArea, (tRectangle){{rectangle.coord.x - 5, rectangle.coord.y + 57}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, editBuf, (tRgb)RGB_WHITE);
            } else {
                gMorphLabelRect[i] = draw_button(mainArea, (tRectangle){{rectangle.coord.x - 5, rectangle.coord.y + 57}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, label, (tRgb)RGB_BACKGROUND_GREY);
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

