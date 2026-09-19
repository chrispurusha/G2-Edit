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
// Notes: Docs/code-notes/moduleGraphics.c.md - "// notes §k" refers there.

#ifdef __cplusplus
extern "C" {
#endif

// Disable warnings from external library headers etc.
#include <math.h>
#include <stdint.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"
#include "waveModels.h"
#include "moduleGraphics.h"
#include "soundEngine.h"    // sound_engine_module_meter() - see volume_source()
#include "splitView.h"
#include "globalVars.h"
#include "renderParams.h"
#include "paramCurves.h"
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

// notes §1

// The leading {kind, key} pair is the shared prefix declared in moduleGraphics.h — see the comment
// there before reordering or adding a member.
typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
    uint32_t          paramIndex;
} tParamClickCtx;

static tParamClickCtx     sParamClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS];

// Where the Channel Select group being pressed was drawn. One press is in flight at a time, so one
// rectangle is enough — and it is only meaningful between this press and its own release, which is
// exactly the window the click registry's capture covers.
static tRectangle         sRadioPressRect;

typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
} tModuleClickCtx;

static tModuleClickCtx    sModuleClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES];

typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
    uint32_t          modeIndex;
} tModeClickCtx;

static tModeClickCtx      sModeClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_MODES];

typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
    uint32_t          connectorIndex;
} tConnectorClickCtx;

static tConnectorClickCtx sConnectorClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_CONNECTORS];

// The drag fields every parameter drag starts with - from the widget or from a graph handle.
static void param_drag_start(tModule * module, uint32_t paramIndex) {
    tParam * param = &module->param[gPatchDescr[module->key.slot].activeVariation][paramIndex];

    gParamDragging.moduleKey       = module->key;
    gParamDragging.type3           = paramType3Param;
    gParamDragging.param           = paramIndex;
    gParamDragging.startValue      = param->value;
    gParamDragging.active          = true;
    gParamDragging.startMorphRange = param->morphRange[gMorphGroupFocus];
}

// notes §2
static void param_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    tParamClickCtx * ctx       = (tParamClickCtx *)userData;
    tModule *        module    = get_module(ctx->key);
    uint32_t         slot      = ctx->key.slot;
    uint32_t         variation = gPatchDescr[slot].activeVariation;
    tParam *         param     = &module->param[variation][ctx->paramIndex];
    tParamType       paramType = paramLocationList[param->paramRef].type;

    if (phase == eClickPress) {
        // notes §3
        gParamFocus.valid      = true;
        gParamFocus.moduleKey  = module->key;
        gParamFocus.paramIndex = ctx->paramIndex;
        LOG_INFO("Param focus: slot %u location %u module %u param %u (type %u)\n",
                 module->key.slot, module->key.location, module->key.index, ctx->paramIndex, paramType);

        if (  paramType != paramTypeToggle && paramType != paramTypeMenu
           && paramType != paramTypeBypass && paramType != paramTypeEnable
           && paramType != paramTypePush && paramType != paramTypeCustomData
           && paramType != paramTypeRadioEdit) {
            param_drag_start(module, ctx->paramIndex);
            // notes §4
            click_region_capture_rect(&gParamDragging.rect);

            if ((synthlib_dial_mode() != eDialModeRotary) || (paramType == paramTypeSlider)) {
                canvas_drag_begin();
            }
        } else if (paramType == paramTypeRadioEdit) {
            // notes §5
            sRadioPressRect = (tRectangle){
                0
            };
            click_region_capture_rect(&sRadioPressRect);
        } else if (paramType == paramTypePush) {
            // notes §6
            uint32_t listSize = array_size_param_location_list();

            for (uint32_t ref = 0; ref < listSize; ref++) {
                if ((paramLocationList[ref].moduleType == module->type) && (paramLocationList[ref].type == paramTypeCustomData)) {
                    send_custom_data_value(slot, module->key);
                    break;
                }
            }

            send_param_value(slot, module->key, ctx->paramIndex, variation, 1);
            param->value = 0;  // Momentary: there is no pressed state to draw
        }
    } else if (phase == eClickRelease) {
        if ((paramType == paramTypeMenu) || (paramType == paramTypeCustomData)) {
            open_toggle_menu(coord, module->key, ctx->paramIndex, param->paramRef);
        } else if (paramType == paramTypeRadioEdit) {
            // notes §7
            uint32_t range       = paramLocationList[param->paramRef].range;
            uint32_t oldParamVal = param->value;
            int32_t  button      = radio_button_at(sRadioPressRect, range, coord);

            if ((button < 0) || ((uint32_t)button == oldParamVal)) {
                return;
            }
            param->value = (uint32_t)button;
            send_param_value(slot, module->key, ctx->paramIndex, variation, param->value);
            undo_push_param_change(module->key, ctx->paramIndex, variation, oldParamVal, param->value);
            send_param_value_to_links(slot, module->key, ctx->paramIndex, variation, param->value);
        } else if ((paramType == paramTypeToggle) || (paramType == paramTypeBypass) || (paramType == paramTypeEnable)) {
            uint32_t range       = paramLocationList[param->paramRef].range;
            uint32_t oldParamVal = param->value;

            param->value = (param->value + 1) % range;
            send_param_value(slot, module->key, ctx->paramIndex, variation, param->value);
            undo_push_param_change(module->key, ctx->paramIndex, variation, oldParamVal, param->value);
            send_param_value_to_links(slot, module->key, ctx->paramIndex, variation, param->value);
        } else if (paramType == paramTypePush) {
            // The button coming back up. The trigger itself went out on press — see the comment there.
            send_param_value(slot, module->key, ctx->paramIndex, variation, 0);
            param->value = 0;
        }
    }
}

// notes §8
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

// notes §9
static void connector_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    if (phase != eClickPress) {
        return;
    }
    tConnectorClickCtx * ctx    = (tConnectorClickCtx *)userData;
    tModule *            module = get_module(ctx->key);

    gCableDrag.rerouting = false;

    // notes §10
    if (ctrl_modifier_held()) {
        tConnectorDir dir              = module->connector[ctx->connectorIndex].dir;
        int           ioCount          = find_io_count_from_index(module, dir, (int)ctx->connectorIndex);
        tCableKey     key              = {0};
        uint32_t      otherModuleIndex = 0;
        uint32_t      otherIoCount     = 0;
        tConnectorDir otherDir         = connectorDirIn;

        // Finding ONE cable here is only about where to draw the rubber band from — the move itself
        // takes every cable on this hole (handle_cable_reroute()), which is why what gets recorded is
        // the hole and not that cable.
        if (  (ioCount >= 0)
           && find_cable_at_connector(module->key.slot, module->key.location, module->key.index,
                                      (uint32_t)ioCount, dir, &key, &otherModuleIndex, &otherIoCount, &otherDir)) {
            tModule * otherModule = get_module_slot(module->key.slot, module->key.location, otherModuleIndex);
            int       otherIndex  = (otherModule != NULL) ? find_index_from_io_count(otherModule, otherDir, (int)otherIoCount) : -1;

            if (otherIndex >= 0) {
                gCableDrag.rerouting          = true;
                gCableDrag.rerouteModuleIndex = module->key.index;
                gCableDrag.rerouteIoCount     = (uint32_t)ioCount;
                gCableDrag.rerouteDir         = dir;
                gCableDrag.fromModuleKey      = otherModule->key;
                gCableDrag.fromConnectorIndex = (uint32_t)otherIndex;
                cable_drag_set_end(coord);
                gCableDrag.active             = true;
                return;
            }
        }
    }
    gCableDrag.fromModuleKey      = module->key;
    gCableDrag.fromConnectorIndex = ctx->connectorIndex;
    cable_drag_set_end(coord);   // same placement the motion uses, so the end doesn't jump on the first move
    gCableDrag.active             = true;
}

// notes §11
static void drag_area_click_handler(tCoord coord, eClickPhase phase, void * userData) {
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

    // notes §12
    convert_mouse_coord_to_module_column_row(&gModuleDrag.prevColumn, &gModuleDrag.prevRow, coord);
    gModuleDrag.active        = true;
    gModuleDrag.snapshotCount = 0;

    // notes §13
    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * walk = get_module_slot(module->key.slot, module->key.location, i);

        if ((walk == NULL) || !walk->active) {
            continue;
        }
        gModuleDrag.snapshotKeys[gModuleDrag.snapshotCount]   = walk->key;
        gModuleDrag.snapshotColumn[gModuleDrag.snapshotCount] = walk->column;
        gModuleDrag.snapshotRow[gModuleDrag.snapshotCount]    = walk->row;
        gModuleDrag.snapshotCount++;
    }
}

// notes §14
static void * morph_click_ctx(const tModule * module, uint32_t index) {
    tParamClickCtx * ctx = &sParamClickCtx[module->key.slot][module->key.location][module->key.index][index];

    *ctx = (tParamClickCtx){
        eCanvasWidgetMorph, module->key, index
    };

    return ctx;
}

// Defined further down, beside the wave models they draw from.
static double module_wave_sample(uint32_t moduleType, uint32_t waveValue, double phase, double shape);
bool module_wave_picker_mode(uint32_t moduleType, uint32_t modeIndex);

static void morph_param_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;
    uint32_t  i         = ((const tParamClickCtx *)userData)->paramIndex;
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
            send_param_value_to_links(gSlot, module->key, i, variation, param->value);
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

// notes §15
static void render_param_focus_marks(tRectangle rect) {
    double x         = rect.coord.x;
    double y         = rect.coord.y;
    double w         = rect.size.w;
    double h         = rect.size.h;
    // notes §16
    double len       = fmin(w, h) * 0.175;
    double thickness = fmax(fmin(w, h) * 0.06, 1.0);

    if ((w <= 0.0) || (h <= 0.0) || (len < 1.0)) {
        return;
    }
    // notes §17
    set_rgb_colour((tRgb)RGB_BLACK);
    render_line(mainArea, (tCoord){x, y}, (tCoord){x + len, y}, thickness);                          // top left
    render_line(mainArea, (tCoord){x, y}, (tCoord){x, y + len}, thickness);
    render_line(mainArea, (tCoord){x + w, y}, (tCoord){x + w - len, y}, thickness);                  // top right
    render_line(mainArea, (tCoord){x + w, y}, (tCoord){x + w, y + len}, thickness);
    render_line(mainArea, (tCoord){x + w, y + h}, (tCoord){x + w - len, y + h}, thickness);          // bottom right
    render_line(mainArea, (tCoord){x + w, y + h}, (tCoord){x + w, y + h - len}, thickness);
    render_line(mainArea, (tCoord){x, y + h}, (tCoord){x + len, y + h}, thickness);                  // bottom left
    render_line(mainArea, (tCoord){x, y + h}, (tCoord){x, y + h - len}, thickness);
}

#define KEYQUANT_FIRST_NOTE_PARAM    (2)

// notes §84
static bool param_drawn_by_graph(uint32_t moduleType, uint32_t paramIndex) {
    return (moduleType == moduleTypeKeyQuant) && (paramIndex >= KEYQUANT_FIRST_NOTE_PARAM);
}

tRectangle render_param_common(tRectangle rectangle, tModule * module, uint32_t paramRef, uint32_t paramIndex) {
    // notes §18
    tRectangle widgetRect                  = {0};
    char       buff[16]                    = {0};
    char       label[CLAVIA_NAME_SIZE + 1] = {0};
    // notes §19
    uint32_t   slot                        = module->key.slot;
    uint32_t   variation                   = gPatchDescr[slot].activeVariation;
    uint32_t   paramValue                  = module->param[variation][paramIndex].value;
    uint32_t   morphRange                  = module->param[variation][paramIndex].morphRange[gMorphGroupFocus];

    if (paramValue >= paramLocationList[paramRef].range) {
        LOG_ERROR("Module index %u name %s ParamRef %u ParamIndex %u Value %u > Range %u\n", module->key.index, module->name, paramRef, paramIndex, paramValue, paramLocationList[paramRef].range);
        paramValue = 0;  // If we hit this, the module config needs fixing, but letting it through for now
    }

    // A renamed parameter is drawn under its new name — EXCEPT a Channel Select group, where name 0
    // is the first BUTTON's caption, not a name for the group. Taking it as the group label drew
    // "Box1" both above the group and on the button.
    if ((strlen(module->paramName[paramIndex][0]) > 0) && (paramLocationList[paramRef].type != paramTypeRadioEdit)) {
        COPY_STRING(label, module->paramName[paramIndex][0]);
    } else if (paramLocationList[paramRef].label != NULL) {
        COPY_STRING(label, paramLocationList[paramRef].label);
    }
    label[sizeof(label) - 1]                      = '\0';

    module->param[variation][paramIndex].paramRef = paramRef;

    if (param_drawn_by_graph(module->type, paramIndex)) {
        return widgetRect;
    }
    //LOG_DEBUG("param %u\n", paramValue);

    switch (paramLocationList[paramRef].type) {
        case paramTypeCustomData:
        case paramTypeToggle:
        case paramTypeMenu:
        {
            // notes §20
            static const char * blankCaptions[] = {
                WAVE_MENU_CAPTION, WAVE_MENU_CAPTION, WAVE_MENU_CAPTION,
                WAVE_MENU_CAPTION, WAVE_MENU_CAPTION, WAVE_MENU_CAPTION,
                WAVE_MENU_CAPTION, WAVE_MENU_CAPTION, NULL
            };
            bool                isWavePicker    = module_wave_picker_param(module->type, paramIndex);
            const char **       captions        = isWavePicker ? blankCaptions : paramLocationList[paramRef].strMap;

            widgetRect = render_paramType1StandardToggle(module, rectangle, label, buff, sizeof(buff), paramValue,
                                                         paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5,
                                                         paramIndex, paramRef, captions);

            if (isWavePicker == true) {
                // notes §21
                render_wave_icon(widgetRect, module->type, (uint32_t)paramValue,
                                 module_wave_icon_shape(module->type));
            }
            break;
        }
        case paramTypeRadioEdit:
        {
            widgetRect = render_paramType1RadioEdit(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            break;
        }
        case paramTypeBypass:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Bypass;

            if (render_param_function != NULL) {
                widgetRect = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        case paramTypeEnable:
        case paramTypePush:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Enable;

            if (render_param_function != NULL) {
                widgetRect = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
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
                case paramTypeLfoShape:       render_param_function      = &render_paramType1LfoShape;
                    break;
                case paramTypeDrumSlaveRatio: render_param_function      = &render_paramType1DrumSlaveRatio;
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
                case paramTypeUniPolShort:    render_param_function      = &render_paramType1UniPolShort;
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
                widgetRect = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramRef);
            }
            break;
        }
    }
    sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex] = (tParamClickCtx){
        eCanvasWidgetParam, module->key, paramIndex
    };
    register_click_region(widgetRect, eClickLayerCanvas, param_click_handler,
                          &sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex]);
    param_overlay_note_param(module, paramIndex, rectangle, buff);

    if (  gParamFocus.valid
       && (gParamFocus.moduleKey.slot == module->key.slot)
       && (gParamFocus.moduleKey.location == module->key.location)
       && (gParamFocus.moduleKey.index == module->key.index)
       && (gParamFocus.paramIndex == paramIndex)) {
        render_param_focus_marks(widgetRect);
    }
    // Hand back where the widget actually went, for a caller that needs it for its own hit-testing.
    return widgetRect;
}

// notes §22
const tCanvasWidget * canvas_widget_at_any_layer(tCoord coord) {
    return (const tCanvasWidget *)click_region_at(coord);
}

const tCanvasWidget * canvas_widget_at(tCoord coord) {
    // Canvas layer only — see moduleGraphics.h. The morph dials sit at eClickLayerPanel and register
    // an INTEGER cast to a pointer as their user data, so reading a tag off one would not merely be
    // the wrong answer, it would dereference a small integer.
    return (const tCanvasWidget *)click_region_at_layer(coord, eClickLayerCanvas);
}

uint32_t canvas_widget_index(const tCanvasWidget * widget) {
    if (widget == NULL) {
        return 0;
    }

    switch (widget->kind) {
        case eCanvasWidgetParam:
            return ((const tParamClickCtx *)widget)->paramIndex;

        case eCanvasWidgetMode:
            return ((const tModeClickCtx *)widget)->modeIndex;

        case eCanvasWidgetConnector:
            return ((const tConnectorClickCtx *)widget)->connectorIndex;

        default:
            return 0;
    }
}

// notes §23
bool param_is_under_cursor(const tModule * module, uint32_t paramIndex, tCoord coord) {
    if ((module == NULL) || (paramIndex >= MAX_NUM_PARAMETERS)) {
        return false;
    }
    return click_region_at(coord) == &sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex];
}

void render_mode_common(tRectangle rectangle, tModule * module, uint32_t modeRef, uint32_t modeIndex) {
    // notes §24
    if (modeIndex >= MAX_NUM_MODES) {
        LOG_ERROR("MAX_NUM_MODES needs increasing to >= %u (module type %u)\n", modeIndex + 1, module->type);
        EXIT_IN_DEBUG();
        return;
    }
    uint32_t modeValue = module->mode[modeIndex].value;

    // notes §25
    module->mode[modeIndex].modeRef = modeRef;

    switch (modeLocationList[modeRef].type) {
        case paramTypeOscWave:
        {
            char       buff[16]     = {0};

            snprintf(buff, sizeof(buff), "%u", modeValue);
            // notes §26
            double     modeLabelH   = rectangle.size.h / 4.0;
            tRectangle modeDialRect = rectangle;

            modeDialRect.coord.y                                                               += (modeLocationList[modeRef].label != NULL) ? (modeLabelH * 2.0) : modeLabelH;
            modeDialRect.size.h                                                                 = modeDialRect.size.w;
            module->mode[modeIndex].rectangle                                                   = render_dial_with_text(moduleArea, modeDialRect, (char *)modeLocationList[modeRef].label, buff, modeLabelH, modeValue, modeLocationList[modeRef].range, 0, (tRgb)RGB_GREY_5); // TODO: Check if Mode can be morphed
            sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex] = (tModeClickCtx){
                eCanvasWidgetMode, module->key, modeIndex
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
                draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {30, textHeight}}, debug, (tRgb)RGB_BACKGROUND_GREY);
                return;
            }
            //if (paramLocationList[paramRef].colourMap != NULL) {
            //    set_rgb_colour(paramLocationList[paramRef].colourMap[paramValue]);
            //} else {
            //    set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
            //}

            // notes §27
            if (modeLocationList[modeRef].label != NULL) {
                set_rgb_colour((tRgb)RGB_BLACK);
                render_text(moduleArea,
                            (tRectangle){{rectangle.coord.x, y - textHeight}, {rectangle.size.w, textHeight}},
                            (char *)modeLocationList[modeRef].label);
            }
            // notes §28
            bool isWaves = module_wave_picker_mode(module->type, modeIndex);

            module->mode[modeIndex].rectangle                                                   = draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {isWaves ? get_text_width(WAVE_MENU_CAPTION, textHeight, eNoCache) : largest_text_width(modeLocationList[modeRef].range, strMap, textHeight, eCache), textHeight}}, isWaves ? "" : strMap[modeValue], (tRgb)RGB_BACKGROUND_GREY);

            if (isWaves == true) {
                render_wave_icon(module->mode[modeIndex].rectangle, module->type, modeValue,
                                 module_wave_icon_shape(module->type));
            }
            sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex] = (tModeClickCtx){
                eCanvasWidgetMode, module->key, modeIndex
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

// notes §29
static uint32_t volume_source(tModule * module, uint32_t i) {
    uint32_t fromEngine = 0;

    if (sound_engine_module_meter((uint32_t)module->key.location, (uint32_t)module->key.index,
                                  i, &fromEngine) == true) {
        return fromEngine;
    }
    return module->volume.value[i];
}

void render_volume_common(tRectangle rectangle, tModule * module, uint32_t volumeRef, uint32_t volumeIndex) {
    module->volume.volumeRef = volumeRef;

    switch (volumeLocationList[volumeRef].volumeType) {
        case volumeTypeMono:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, volume_source(module, 0));
        }
        break;
        case volumeTypeStereo:
        {
            double space = find_volume_meter_config(volumeTypeStereo)->space;

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, volume_source(module, 0)); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, volume_source(module, 1));
        }
        break;
        case volumeTypeSequencer:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, volume_source(module, 0)); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
        }
        break;
        case volumeTypeQuad:
        {
            double space = find_volume_meter_config(volumeTypeQuad)->space;

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, volume_source(module, 0)); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, volume_source(module, 1));
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[2]);
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[3]);
        }
        break;
        case volumeTypeCompress:
        {
            // The gain-reduction scale the original editor prints beside these LEDs, top to bottom:
            // each mark's dB and the LED row (from the top, fractional) it sits against.
            static const char *        kMark[]  = {"1", "4", "9", "15", "24", "30"};
            static const double        kAtLed[] = {0.0, 1.67, 3.33, 5.17, 6.83, 8.5};
            const tVolumeMeterConfig * config   = find_volume_meter_config(volumeTypeCompress);
            double                     pitch    = rectangle.size.h / (double)config->segments;
            double                     textH    = STANDARD_TEXT_HEIGHT * 0.75;

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, volume_source(module, 0));
            set_rgba_colour((tRgba)RGBA_BLACK_ON_TRANSPARENT);

            for (uint32_t i = 0; i < (sizeof(kMark) / sizeof(kMark[0])); i++) {
                double width = get_text_width(kMark[i], textH, eNoCache);
                double y     = rectangle.coord.y + ((kAtLed[i] + 0.5) * pitch) - (textH / 2.0);

                render_text(moduleArea, (tRectangle){{rectangle.coord.x - width - 3.0, y}, {BLANK_SIZE, textH}}, kMark[i]);
            }
        }
        break;
        default:
        {
        }
        break;
    }
}

// A read-only readout — see tDisplayLocation. No click region is registered for it: it is not a
// control, and giving it one would put a dead target over the module body where a right-click should
// still reach the module's own menu.
static tModuleClickCtx sDrumPresetClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES];

static void drum_preset_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    if (phase == eClickRelease) {
        open_drum_preset_menu(coord, ((tModuleClickCtx *)userData)->key);
    }
}

void render_display_common(tRectangle rectangle, tModule * module, uint32_t displayRef) {
    uint32_t     slot       = module->key.slot;
    uint32_t     variation  = gPatchDescr[slot].activeVariation;
    uint32_t     source     = displayLocationList[displayRef].sourceParam;
    const char * label      = displayLocationList[displayRef].label;
    char         buff[16]   = {0};
    double       textHeight = (double)STANDARD_TEXT_HEIGHT;

    if (source >= MAX_NUM_PARAMETERS) {
        return;
    }

    switch (displayLocationList[displayRef].displayType) {
        case displayTypeSwitchCtrl:
        {
            // Four per step, which is the whole point of the number: it is what the Mux modules
            // decode back into a channel. Calculated, not tabulated — the rule is the same for a
            // two-way switch as for an eight-way one, which is exactly why the G2 uses it.
            snprintf(buff, sizeof(buff), "%u", module->param[variation][source].value * SWITCH_CTRL_STEP);
            break;
        }

        case displayTypeDrumPreset:
        {
            tModuleClickCtx * ctx    = &sDrumPresetClickCtx[slot][module->key.location][module->key.index];
            double            width  = 0.0;

            // As wide as the widest name, so the button does not change size as the dials move.
            for (int32_t preset = -1; preset < (int32_t)drum_synth_preset_count(); preset++) {
                double w = get_text_width(drum_synth_preset_name((uint32_t)preset), STANDARD_BUTTON_TEXT_HEIGHT, eCache);

                width = (w > width) ? w : width;
            }

            tRectangle        button = draw_button(moduleArea,
                                                   (tRectangle){{rectangle.coord.x, rectangle.coord.y}, {width, STANDARD_BUTTON_TEXT_HEIGHT}},
                                                   drum_synth_preset_name((uint32_t)drum_synth_preset_matching(module->param[variation])),
                                                   (tRgb)RGB_BACKGROUND_GREY);

            *ctx = (tModuleClickCtx){
                eCanvasWidgetModule, module->key
            };
            register_click_region(button, eClickLayerCanvas, drum_preset_click_handler, ctx);
            break;
        }

        default:
            return;
    }
    // A readout is PLAIN TEXT ON THE FACE, not a button - drawing it as a button invited the eye to
    // try, and with no click region a right-click over it still reaches the module's own menu. The
    // drum preset is the exception: it is a button because it can be clicked.
    set_rgb_colour((tRgb)RGB_BLACK);

    if (buff[0] != '\0') {
        render_text(moduleArea, (tRectangle){{rectangle.coord.x, rectangle.coord.y}, {BLANK_SIZE, textHeight}}, buff);
    }

    if (label == NULL) {
        return;
    }
    tRectangle labelRect = {{rectangle.coord.x, rectangle.coord.y}, {BLANK_SIZE, textHeight}};

    // Placed the same four ways a connector's label is, and for the same reasons — see
    // render_connector_common(), which this deliberately mirrors rather than inventing its own rule.
    switch (displayLocationList[displayRef].labelLoc) {
        case labelLocUp:
            labelRect.coord.y -= textHeight;
            break;

        case labelLocDown:
            labelRect.coord.y += (rectangle.size.h + 2.0);
            break;

        case labelLocLeft:
            labelRect.coord.x -= (get_text_width((char *)label, textHeight, eCache) + 2.0);
            break;

        case labelLocRight:
            labelRect.coord.x += (rectangle.size.w + 2.0);
            break;
    }
    render_text(moduleArea, labelRect, (char *)label);
}

void render_led_common(tRectangle rectangle, tModule * module, uint32_t ledRef, uint32_t ledIndex) {
    switch (ledLocationList[ledRef].ledType) {
        case ledTypeMultiBit:  // one bit of a group value rather than a 2-bit value of its own
        case ledTypeYes:
        {
            // Same bound the parser applies, from the other end: the caller's loop counts rows in
            // ledLocationList, and nothing stops that table growing a ninth row for a type.
            if (ledIndex >= MAX_LEDS_PER_MODULE) {
                LOG_ERROR("MAX_LEDS_PER_MODULE needs increasing to >= %u (module type %u)\n", ledIndex + 1, module->type);
                EXIT_IN_DEBUG();
                break;
            }
            // notes §30
            uint32_t ledVal = module->led.value[ledIndex];
            uint32_t fromEngine;

            if (sound_engine_module_led((uint32_t)module->key.location, (uint32_t)module->key.index,
                                        ledIndex, &fromEngine) == true) {
                ledVal = fromEngine;
            }
            bool     green  = ledVal & 1;
            bool     red    = (ledVal >> 1) & 1;

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
    // notes §31

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
                // notes §32
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
    // notes §33
    set_rgb_colour(connectorColourMap[effective_connector_type(type, module->upRate)]);  // Note, was using "module->connector[connectorIndex].type", check that this type param is OK

    // The passed-in dir, not module->connector[].dir — they hold the same value from the same
    // connectorLocationList entry, and using the parameter keeps this drawing code independent of
    // when the array happens to have been filled.
    if (dir == connectorDirIn) {
        module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, (tCoord){rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 2.0, 10.0, 0.0, 10.0);
    } else {
        module->connector[connectorIndex].rectangle = render_rectangle(moduleArea, (tRectangle){rectangle.coord, {rectangle.size.w, rectangle.size.h}});
    }
    // notes §34
    module->connector[connectorIndex].rectangle.coord.x                                          -= CONNECTOR_HIT_PADDING;
    module->connector[connectorIndex].rectangle.coord.y                                          -= CONNECTOR_HIT_PADDING;
    module->connector[connectorIndex].rectangle.size.w                                           += 2.0 * CONNECTOR_HIT_PADDING;
    module->connector[connectorIndex].rectangle.size.h                                           += 2.0 * CONNECTOR_HIT_PADDING;

    sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex] = (tConnectorClickCtx){
        eCanvasWidgetConnector, module->key, connectorIndex
    };
    register_click_region(module->connector[connectorIndex].rectangle, eClickLayerCanvas, connector_click_handler,
                          &sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex]);
    set_rgb_colour((tRgb)RGB_BLACK);
    render_circle_part(moduleArea, (tCoord){rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 4.0, 10.0, 0.0, 10.0);
}

// notes §35

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

// notes §36
static tRectangle centre_on_drawn_height(tRectangle relative, tAnchor anchor, double drawnHeight) {
    if ((anchor == anchorMiddle) || (anchor == anchorMiddleLeft) || (anchor == anchorMiddleRight)) {
        relative.coord.y += (relative.size.h - drawnHeight) / 2.0;
    }
    return relative;
}

// Both heights are percentages of MODULE_WIDTH, the units every row in these tables is written in.
static double param_drawn_height(tRectangle relative, tParamType type) {
    switch (type) {
        case paramTypeCustomData:
        case paramTypeToggle:
        case paramTypeMenu:
        case paramTypeEnable:
        case paramTypePush:
            return (STANDARD_BUTTON_TEXT_HEIGHT * 100.0) / MODULE_WIDTH;

        default:
            return relative.size.h;
    }
}

static double mode_drawn_height(tRectangle relative, tParamType type) {
    switch (type) {
        case paramTypeToggle:
        case paramTypeMenu:
            return relative.size.h / 2.0;

        default:
            return relative.size.h;
    }
}

// notes §37
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

// notes §38
static double skewed_ramp_zero_start(double phase, double peak) {
    double p = fmod(((peak + 1.0) * 0.5) + phase, 1.0);
    double y = (p < peak) ? (((p / peak) * 2.0) - 1.0) : ((((1.0 - p) / (1.0 - peak)) * 2.0) - 1.0);

    return -y;
}

// notes §39
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

// notes §40
static double oscshpb_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    // notes §41
    switch (waveformIndex) {
        case 0:
        case 1:
        case 2:
        case 3:
            // Closed-form and continuous: nothing to decide, so the shared value is used as it is.
            return wave_sine_by_index(waveformIndex, phase, shape);

        case 4: // TriSaw
        {
            return skewed_ramp_zero_start(phase, wave_trisaw_peak(shape));
        }
        case 5: // DblSaw — two near-full sawtooths, the second detuned
        {
            double detune = wave_dblsaw_detune(shape);

            return -(skewed_ramp_zero_start(phase, WAVE_DBLSAW_PEAK)
                     + skewed_ramp_zero_start(phase + detune, WAVE_DBLSAW_PEAK)) * 0.5;
        }
        case 6: // Pulse — asymmetric width
        {
            double duty      = wave_shpb_pulse_duty(shape);
            double edgeWidth = pulse_edge_width(fmin(duty, 1.0 - duty));

            if (phase < edgeWidth) {
                return ramp_from_zero(phase, edgeWidth);
            }

            if (phase >= (1.0 - edgeWidth)) {
                return ramp_to_zero(phase - (1.0 - edgeWidth), edgeWidth);
            }
            return (phase < duty) ? 1.0 : -1.0;
        }
        case 7: // SymPulse — High, then Low, then silence for the remainder of the cycle
        {
            double halfSeg   = wave_sympulse_half_segment(shape);
            double edgeWidth = pulse_edge_width(halfSeg);

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

// notes §42
static double lfoshpa_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    switch (waveformIndex) {
        case 0:
        {
            // notes §43
            double b = 0.02 + (0.46 * shape);
            double w = 0.0;

            if (b < 0.02) {
                b = 0.02;
            }

            if (phase < b) {
                w = 0.25 * (phase / b);
            } else if (phase < (1.0 - b)) {
                w = 0.25 + (0.5 * ((phase - b) / (1.0 - (2.0 * b))));
            } else {
                w = 0.75 + (0.25 * ((phase - (1.0 - b)) / b));
            }
            return sin(2.0 * M_PI * w);
        }
        case 1:
        case 2:
        {
            // notes §44
            double floorWidth = (waveformIndex == 1) ? 0.08 : 0.05;
            double width      = shape;
            double y          = 0.0;

            if (width < floorWidth) {
                width = floorWidth;
            }

            if (phase < width) {
                double u = phase / width;

                y = (waveformIndex == 1) ? (0.5 * (1.0 - cos(2.0 * M_PI * u)))
                    : ((u < 0.5) ? (u * 2.0) : ((1.0 - u) * 2.0));
            }
            double mean       = 0.5 * width;
            double peak       = fmax(1.0 - mean, mean);

            return (y - mean) / peak;
        }
        case 3:
        {
            // Saw2Tri - a triangle whose APEX slides right across the cycle, so it is a falling saw at
            // one end of the dial, a symmetric triangle at the centre and a rising saw at the other.
            // Measured apex 0.02, 0.26, 0.50, 0.74, 0.98 - the dial again, near enough exactly.
            double apex = 0.02 + (0.96 * shape);

            return (phase < apex) ? ((phase / apex) * 2.0 - 1.0)
                    : (((1.0 - phase) / (1.0 - apex)) * 2.0 - 1.0);
        }
        case 4:
        {
            // Sqr2Tri - a symmetric triangle driven progressively harder into a clip, so it fills out
            // from triangle through trapezoid towards a square. Measured gain 1.0, 2.0, 3.0, 3.9, 4.9,
            // which is 1 + 3.9 * Shape and reaches a trapezoid rather than a true square at the top.
            double gain = 1.0 + (3.9 * shape);
            double tri  = (phase < 0.5) ? ((phase / 0.5) * 2.0 - 1.0) : (((1.0 - phase) / 0.5) * 2.0 - 1.0);
            double y    = gain * tri;

            return (y > 1.0) ? 1.0 : ((y < -1.0) ? -1.0 : y);
        }
        default:
        {
            // notes §45
            double duty = 0.024 + (0.936 * shape);

            if (duty < 0.08) {
                duty = 0.08;
            }
            return (phase < duty) ? 1.0 : -1.0;
        }
    }
}

// notes §46
static double basic_sine(double phase) {
    return sin(2.0 * M_PI * phase);
}

// Symmetric triangle, apex at the quarter cycle so it starts at a rising zero.
static double basic_triangle(double phase) {
    return (phase < 0.25) ? (phase * 4.0)
           : ((phase < 0.75) ? (2.0 - (phase * 4.0)) : ((phase * 4.0) - 4.0));
}

// FALLS across the cycle, the same direction as TriSaw's ramp — and the opposite of DblSaw's and of
// OscA's, both of which the captures show rise. THREE MEASUREMENTS, TWO DIRECTIONS: saw polarity is
// per-module on this instrument and must never be carried from one module to another unmeasured.
static double basic_falling_saw(double phase) {
    return 1.0 - (2.0 * phase);
}

// RISES across the cycle. OscA's saw, measured 2026-08-24: correlating the captured cycle against the
// falling form scored 0.516 and against the rising one 0.988 — the same test, and the same size of
// gap, that caught DblSaw drawn upside down.
static double basic_rising_saw(double phase) {
    return (2.0 * phase) - 1.0;
}

// High for the first `duty` of the cycle, low for the rest. Full swing, with no DC removal: that is
// what LfoB's Squ measured as, and what OscShpB's measured Pulse draws at every width.
static double basic_pulse(double phase, double duty) {
    return (phase < duty) ? 1.0 : -1.0;
}

static double lfob_waveform_sample(uint32_t waveformIndex, double phase) {
    switch (waveformIndex) {
        case 0:
            return basic_sine(phase);              // Sin

        case 1:
            return basic_triangle(phase);          // Tri

        case 2:
            return basic_falling_saw(phase);       // Saw

        default:
            return basic_pulse(phase, 0.5);        // Squ
    }
}

// notes §47
static double shaper_transfer_sample(uint32_t moduleType, uint32_t modeValue, double input) {
    if (moduleType == moduleTypeRect) {
        switch (modeValue) {
            case 0:
                return (input > 0.0) ? input : 0.0;         // HalfPos — discards negatives

            case 1:
                return (input < 0.0) ? input : 0.0;         // HalfNeg — discards positives

            case 2:
                return fabs(input);                         // FullPos — mirrors negatives up

            default:
                return -fabs(input);                        // FullNeg — mirrors positives down
        }
    }
    {
        // The engine's own ShpStatic curves, so the picker cannot drift from what is heard.
        tShaperSettings shaper = {
            .kind = eShaperShpStatic, .curve = modeValue, .sym = true, .amount = 1.0, .mod = 0.0, .signalLeg = 0, .active = true
        };

        return shaper_transfer(&shaper, 1.0, input);
    }
}

// notes §48
static bool module_wave_is_transfer(uint32_t moduleType) {
    return (moduleType == moduleTypeRect) || (moduleType == moduleTypeShpStatic);
}

// notes §49
#define LFO_RANDOM_STEPS    (6)

static double lfo_random_sample(double phase, bool stepped) {
    static const double levels[LFO_RANDOM_STEPS] = {0.55, -0.30, 0.90, -0.75, 0.20, -0.60};
    double              scaled                   = phase * (double)LFO_RANDOM_STEPS;
    uint32_t            step                     = (uint32_t)scaled;
    double              within                   = scaled - (double)step;

    if (step >= LFO_RANDOM_STEPS) {
        step   = LFO_RANDOM_STEPS - 1;
        within = 1.0;
    }

    if (stepped == true) {
        return levels[step];
    }
    // Glides to the NEXT level, wrapping at the end so the drawn cycle closes on itself rather than
    // stepping across the seam — the same rule the seam logic in render_wave_icon() applies.
    return levels[step] + (within * (levels[(step + 1) % LFO_RANDOM_STEPS] - levels[step]));
}

// LfoA keeps its waveform in a parameter (index 4) and LfoC in a mode (index 0); the wave itself is
// the same either way, which is why one function serves both.
static double lfoa_waveform_sample(uint32_t waveformIndex, double phase) {
    switch (waveformIndex) {
        case 4:
            return lfo_random_sample(phase, true);      // RndSt — held between steps

        case 5:
            return lfo_random_sample(phase, false);     // Rnd — glides between the same values

        default:
            return lfob_waveform_sample(waveformIndex, phase);   // Sin, Tri, Saw, Squ — measured
    }
}

// notes §50
static double osc_a_waveform_sample(uint32_t waveformIndex, double phase) {
    switch (waveformIndex) {
        case 0:
            return basic_sine(phase);              // Sine

        case 1:
            return basic_triangle(phase);          // Tri

        case 2:
            return basic_rising_saw(phase);        // Saw — RISES, measured; see above

        case 3:
            return basic_pulse(phase, 0.5000);     // Sqr50 — measured 0.4975

        case 4:
            return basic_pulse(phase, 0.2500);     // Sqr25 — measured 0.2500

        default:
            return basic_pulse(phase, 0.0625);     // Sqr10 — measured 1/16, NOT the manual's 10%
    }
}

// notes §51
static double osc_b_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    switch (waveformIndex) {
        case 0:
            return basic_sine(phase);                            // Sine — Shape does nothing

        case 1:
            return basic_triangle(phase);                        // Tri — Shape does nothing

        case 2:
            return basic_rising_saw(phase);                      // Saw — Shape does nothing

        case 3:
            return oscshpb_waveform_sample(6, phase, shape);     // Sqr — OscShpB's Pulse

        default:
            return oscshpb_waveform_sample(5, phase, shape);     // DualSaw — OscShpB's DblSaw
    }
}

// notes §52
bool module_wave_picker_mode(uint32_t moduleType, uint32_t modeIndex) {
    return ((moduleType == moduleTypeOscShpB) && (modeIndex == 0))
           || ((moduleType == moduleTypeOscC) && (modeIndex == 0))
           || ((moduleType == moduleTypeOscD) && (modeIndex == 0))
           || ((moduleType == moduleTypeLfoC) && (modeIndex == 0));
}

bool module_wave_picker_param(uint32_t moduleType, uint32_t paramIndex) {
    return ((moduleType == moduleTypeOscShpA) && (paramIndex == 9))
           || ((moduleType == moduleTypeLfoShpA) && (paramIndex == 11))
           || ((moduleType == moduleTypeLfoB) && (paramIndex == 4))
           || ((moduleType == moduleTypeOscA) && (paramIndex == 4))
           || ((moduleType == moduleTypeOscB) && (paramIndex == 8))
           || ((moduleType == moduleTypeLfoA) && (paramIndex == 4))
           || ((moduleType == moduleTypeRect) && (paramIndex == 0))
           || ((moduleType == moduleTypeShpStatic) && (paramIndex == 0));
}

// One sample of whichever wave family the module in hand belongs to. The four waveform pickers reach
// three different sample functions and keep their waveform in three different places, so the choice
// is made once here rather than at each call site.
static double module_wave_sample(uint32_t moduleType, uint32_t waveValue, double phase, double shape) {
    if (moduleType == moduleTypeLfoB) {
        return lfob_waveform_sample(waveValue, phase);
    }

    // OscA, OscC and OscD share one waveform set, and it reaches them by two different routes: OscA
    // keeps it in a parameter, the other two in a mode. That difference belongs to the pickers, not
    // to the wave, so all three arrive here together.
    if ((moduleType == moduleTypeOscA) || (moduleType == moduleTypeOscC) || (moduleType == moduleTypeOscD)) {
        return osc_a_waveform_sample(waveValue, phase);
    }

    if (moduleType == moduleTypeLfoShpA) {
        return lfoshpa_waveform_sample(waveValue, phase, shape);
    }

    if (moduleType == moduleTypeOscB) {
        return osc_b_waveform_sample(waveValue, phase, shape);
    }

    if ((moduleType == moduleTypeLfoA) || (moduleType == moduleTypeLfoC)) {
        return lfoa_waveform_sample(waveValue, phase);
    }

    // The box's horizontal axis is the INPUT for these two, not phase — see shaper_transfer_sample().
    if (module_wave_is_transfer(moduleType) == true) {
        return shaper_transfer_sample(moduleType, waveValue, (phase * 2.0) - 1.0);
    }

    if (moduleType == moduleTypeOscShpA) {
        static const uint32_t shpAToShpB[] = {0, 1, 2, 3, 4, 7};

        if (waveValue >= (sizeof(shpAToShpB) / sizeof(shpAToShpB[0]))) {
            waveValue = 0;
        }
        return oscshpb_waveform_sample(shpAToShpB[waveValue], phase, shape);
    }
    return oscshpb_waveform_sample(waveValue, phase, shape);
}

// notes §53
static uint32_t module_wave_value(tModule * module, uint32_t variation) {
    switch (module->type) {
        case moduleTypeOscShpB:                     // a MODE on these, a parameter on the rest
        case moduleTypeOscC:
        case moduleTypeOscD:
        case moduleTypeLfoC:
            return module->mode[0].value;

        case moduleTypeOscShpA:
            return module->param[variation][9].value;

        case moduleTypeLfoShpA:
            return module->param[variation][11].value;

        case moduleTypeLfoB:
        case moduleTypeOscA:
        case moduleTypeLfoA:
            return module->param[variation][4].value;

        case moduleTypeOscB:
            return module->param[variation][8].value;

        case moduleTypeRect:                        // a transfer curve, not a wave — see
        case moduleTypeShpStatic:                   // shaper_transfer_sample()
            return module->param[variation][0].value;

        default:
            return 0;
    }
}

// 0..1, or the icons' fixed stand-in for a module that has no Shape dial at all. The sample functions
// for those modules ignore the argument, so the value only has to be harmless.
static double module_shape_value(tModule * module, uint32_t variation) {
    uint32_t index = 0;

    switch (module->type) {
        case moduleTypeOscShpB:
        case moduleTypeOscB:
            index = 6;
            break;

        case moduleTypeOscShpA:
            index = 7;
            break;

        case moduleTypeLfoShpA:
            index = 5;
            break;

        default:
            return WAVE_ICON_FIXED_SHAPE;           // LfoB, OscA, OscC, OscD — no Shape dial
    }
    return (double)module->param[variation][index].value / 127.0;
}

// notes §54
double module_wave_icon_shape(uint32_t moduleType) {
    // See the note at the picker's render site: a FIXED shape, chosen so the waves are told apart.
    return (moduleType == moduleTypeLfoShpA) ? WAVE_ICON_LFOSHPA_SHAPE : WAVE_ICON_FIXED_SHAPE;
}

void render_wave_icon(tRectangle buttonRect, uint32_t moduleType, uint32_t waveValue, double shape) {
    const int    numSamples = 48;   // enough for a 30-pixel-wide button; the big graph uses 200
    const double inset      = 2.0;
    double       left       = buttonRect.coord.x + inset;
    double       width      = buttonRect.size.w - (2.0 * inset);
    double       midY       = buttonRect.coord.y + (buttonRect.size.h / 2.0);
    double       halfHeight = (buttonRect.size.h / 2.0) - inset;
    tCoord       previous   = {0};
    tCoord       first      = {0};
    // The face is already zoomed, so the trace's own width has to be too, or it stays hairline-thin
    // when the canvas is zoomed in and coarse when zoomed out.
    double       thickness  = fmax(1.0, buttonRect.size.h / 12.0);

    if ((width <= 0.0) || (halfHeight <= 0.0)) {
        return;
    }

    for (int i = 0; i <= numSamples; i++) {
        double xFraction = (double)i / (double)numSamples;
        double sample    = module_wave_sample(moduleType, waveValue, xFraction, shape);
        tCoord point     = {left + (xFraction * width), midY - (sample * halfHeight)};

        if (i == 0) {
            first = point;
        } else {
            // notes §55
            render_line(mainArea, previous, point, thickness);
        }
        previous = point;
    }

    // notes §56
    if (  (module_wave_is_transfer(moduleType) == false)
       && (fabs(previous.y - first.y) > (buttonRect.size.h * 0.05))) {
        // notes §57
        render_line(mainArea, previous, (tCoord){previous.x, first.y}, thickness);
        render_line(mainArea, first, (tCoord){first.x, previous.y}, thickness);
    }
}

static void render_oscshpb_waveform_graph(tRectangle rectangle, tModule * module) {
    // notes §58
    uint32_t               slot          = module->key.slot;
    uint32_t               variation     = gPatchDescr[slot].activeVariation;
    uint32_t               waveformValue = module_wave_value(module, variation);
    double                 shape         = module_shape_value(module, variation);
    const tGraphLocation * graphLoc      = find_graph_location(module->type);
    tRectangle             graphRect     = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 midY          = graphRect.coord.y + (graphRect.size.h / 2.0);
    const int              numSamples    = 200;     // fine enough to resolve Pulse/SymPulse's narrow
                                                    // sub-sample-width edge ramps, not just the coarser
                                                    // per-cycle shapes
    const int              numCycles     = 1;       // one period across the box, matching the original editor
    tCoord                 prev          = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, midY}, (tCoord){graphRect.coord.x + graphRect.size.w, midY}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    tCoord                 firstPoint    = {0};
    double                 preWrapY      = 0.0;

    for (int i = 0; i <= numSamples; i++) {
        double xFraction = (double)i / (double)numSamples;                 // raw position across the box, 0..1
        double phase     = fmod(xFraction * numCycles, 1.0);               // wrapped per-cycle phase for the sample
        // THE SAME CALL THE ICON MAKES. Which family a module belongs to, and any index remapping
        // between families, is decided once in module_wave_sample() — so a law corrected there
        // corrects the big graph and the little button together, and a module added there gets both.
        double sample    = module_wave_sample(module->type, waveformValue, phase, shape);
        tCoord point     = {
            graphRect.coord.x + (xFraction * graphRect.size.w),
            graphRect.coord.y + (graphRect.size.h / 2.0) - (sample * graphRect.size.h * 0.45)
        };

        if (i == 0) {
            firstPoint = point;
        } else {
            render_line(moduleArea, prev, point, 1.5);
        }

        if (i == numSamples) {
            preWrapY = prev.y;      // the sample BEFORE the trace wraps back to where it began
        }
        prev = point;
    }

    // notes §59
    if (fabs(preWrapY - firstPoint.y) > (graphRect.size.h * 0.05)) {
        render_line(moduleArea, firstPoint, (tCoord){firstPoint.x, preWrapY}, 1.5);
    }
}

// notes §60
static double envadsr_decay_level(double t, double levelStart, double levelEnd, uint32_t envShapeIndex) {
    return levelEnd + ((levelStart - levelEnd) * env_fall_level(envShapeIndex, t));
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

#define GRAPH_HANDLE_SIZE    (7.0)          // module units, drawn
#define GRAPH_HANDLE_HIT     (2.0)          // times the drawn size, for the click region
#define GRAPH_HANDLES_MAX    (8)            // per module

// A graph handle's pointer-to-value law; item says which point of its graph the handle is.
typedef uint32_t (*tGraphValueFromPointer)(tModuleKey key, tRectangle graphBox, tCoord pointer, int32_t item);

typedef struct {
    tParamClickCtx         param;           // FIRST: to the registry, a menu or a hover this IS the parameter
    tGraphValueFromPointer value;
    tGraphValueFromPointer value2;          // the handle's other axis, moving param2 - or NULL
    uint32_t               param2;
    int32_t                item;
    tRectangle             graphBox;        // on screen
} tGraphHandleCtx;

static tGraphHandleCtx sGraphHandleCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][GRAPH_HANDLES_MAX];

// notes §87: a press on a handle starts the parameter's own drag - focus, undo, links, morph - but
// without capturing the cursor, which a pointer-following value needs to see.
static void graph_handle_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    const tGraphHandleCtx * handle = (const tGraphHandleCtx *)userData;
    tModule *               module = get_module(handle->param.key);

    (void)coord;

    if ((phase != eClickPress) || (module == NULL)) {
        return;
    }
    gParamFocus.valid          = true;
    gParamFocus.moduleKey      = module->key;
    gParamFocus.paramIndex     = handle->param.paramIndex;
    param_drag_start(module, handle->param.paramIndex);
    gParamDragging.graphValue  = handle->value;
    gParamDragging.graphValue2 = handle->value2;
    gParamDragging.param2      = handle->param2;
    gParamDragging.startValue2 = module->param[gPatchDescr[module->key.slot].activeVariation][handle->param2].value;
    gParamDragging.graphItem   = handle->item;
    gParamDragging.graphBox    = handle->graphBox;
}

// Register a handle's click region: the drawn square, enlarged, mapped from the graph's module
// rectangle onto the screen rectangle it was drawn at. value2 with param2 is its other axis, or NULL.
static void graph_handle_register(tModule * module, uint32_t handleIndex, uint32_t paramIndex, tGraphValueFromPointer value,
                                  uint32_t param2, tGraphValueFromPointer value2, int32_t item,
                                  tRectangle graphRect, tRectangle screenBox, tCoord centre) {
    if (handleIndex >= GRAPH_HANDLES_MAX) {
        return;
    }
    tGraphHandleCtx * ctx   = &sGraphHandleCtx[module->key.slot][module->key.location][module->key.index][handleIndex];
    double            scale = screenBox.size.w / graphRect.size.w;
    double            hit   = GRAPH_HANDLE_SIZE * GRAPH_HANDLE_HIT * scale;
    tRectangle        area  = {{
                                   screenBox.coord.x + ((centre.x - graphRect.coord.x) * scale) - (hit / 2.0),
                                   screenBox.coord.y + ((centre.y - graphRect.coord.y) * scale) - (hit / 2.0)
                               },      {hit,hit}};

    *ctx = (tGraphHandleCtx){
        {
            eCanvasWidgetParam, module->key, paramIndex
        }, value, value2, param2, item, screenBox
    };
    register_click_region(area, eClickLayerCanvas, graph_handle_click_handler, ctx);
}

static void graph_handle_draw(tCoord centre, bool live) {
    tRectangle square = {{centre.x - (GRAPH_HANDLE_SIZE / 2.0), centre.y - (GRAPH_HANDLE_SIZE / 2.0)}, {GRAPH_HANDLE_SIZE, GRAPH_HANDLE_SIZE}};

    set_rgb_colour(live ? (tRgb)RGB_GREY_9 : (tRgb)RGB_GREY_5);
    render_rectangle(moduleArea, square);
}


// notes §62: where a graph's zero line and full swing sit in its box, and how its widths scale to fit.
typedef struct {
    double zeroY;
    double fullSwing;
    double scale;
} tEnvGraphFrame;

static tEnvGraphFrame env_graph_frame(const tEnvGraph * graph, tRectangle box) {
    bool   isBip       = (graph->outputType == 4) || (graph->outputType == 5);
    bool   isNegFamily = (graph->outputType == 2) || (graph->outputType == 3);
    double totalWidth  = 0.0;

    for (uint32_t i = 0; i < graph->count; i++) {
        totalWidth += graph->segment[i].width;
    }

    return (tEnvGraphFrame){
        isBip ? (box.coord.y + (box.size.h * 0.5)) : (isNegFamily ? box.coord.y : (box.coord.y + box.size.h)),
        isBip ? (box.size.h * 0.5) : box.size.h,
        (totalWidth > 1.0) ? (1.0 / totalWidth) : 1.0
    };
}

// Where a segment ends in the box - shared by the drawing and the handles, so a handle sits on the curve.
static tCoord env_graph_segment_end(const tEnvGraph * graph, tRectangle box, uint32_t index) {
    tEnvGraphFrame frame = env_graph_frame(graph, box);
    double         x     = box.coord.x;

    for (uint32_t i = 0; i <= index; i++) {
        x += graph->segment[i].width * frame.scale * box.size.w;
    }

    return (tCoord){
        x, env_level_to_y(graph->segment[index].level, graph->outputType, frame.zeroY, frame.fullSwing)
    };
}

// notes §87: widths are scaled to fit the box, so there is no closed-form inverse. The handle tries
// every value of its parameter through the same layout, on a copy of the parameters, and keeps the
// one whose breakpoint lands nearest the pointer - the current value unless another is closer.
static uint32_t env_handle_value(tModuleKey key, tRectangle graphBox, tCoord pointer, int32_t item, bool vertical) {
    tModule * module   = get_module(key);
    tParam    params[MAX_NUM_PARAMETERS];
    tEnvGraph graph    = {0};

    if (module == NULL) {
        return 0;
    }
    memcpy(params, module->param[gPatchDescr[key.slot].activeVariation], sizeof(params));

    if ((env_stage_map(module->type, params, &graph, true) == false) || (item < 0) || ((uint32_t)item >= graph.count)) {
        return 0;
    }
    int32_t   which    = vertical ? graph.segment[item].levelParam : graph.segment[item].timeParam;

    if (which == ENV_NO_PARAM) {
        return 0;
    }
    uint32_t  range    = paramLocationList[params[which].paramRef].range;
    uint32_t  best     = params[which].value;
    double    bestMiss = INFINITY;

    for (uint32_t candidate = 0; candidate < range; candidate++) {
        tCoord end;
        double miss;

        params[which].value = candidate;
        env_stage_map(module->type, params, &graph, true);
        end                 = env_graph_segment_end(&graph, graphBox, (uint32_t)item);
        miss                = vertical ? fabs(end.y - pointer.y) : fabs(end.x - pointer.x);

        if ((miss < bestMiss - 1.0e-9) || ((fabs(miss - bestMiss) <= 1.0e-9) && (candidate == module->param[gPatchDescr[key.slot].activeVariation][which].value))) {
            best     = candidate;
            bestMiss = miss;
        }
    }

    return best;
}

static uint32_t env_time_from_pointer(tModuleKey key, tRectangle graphBox, tCoord pointer, int32_t item) {
    return env_handle_value(key, graphBox, pointer, item, false);
}

static uint32_t env_level_from_pointer(tModuleKey key, tRectangle graphBox, tCoord pointer, int32_t item) {
    return env_handle_value(key, graphBox, pointer, item, true);
}

static void render_envelope_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc      = find_graph_location(module->type);
    uint32_t               variation     = gPatchDescr[module->key.slot].activeVariation;
    tEnvGraph              graph         = {0};

    if ((graphLoc == NULL) || (env_stage_map(module->type, module->param[variation], &graph, true) == false)) {
        return;
    }
    tRectangle             graphRect     = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    uint32_t               outputType    = graph.outputType;
    tEnvGraphFrame         frame         = env_graph_frame(&graph, graphRect);
    const int              numCurveSteps = 12;
    double                 level         = graph.startLevel;
    tCoord                 prev          = {graphRect.coord.x, env_level_to_y(level, outputType, frame.zeroY, frame.fullSwing)};
    tRectangle             screen        = {0};
    uint32_t               handle        = 0;

    set_rgb_colour((tRgb)RGB_GREY_2);
    screen = render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_YELLOW_7);
    render_line(moduleArea, (tCoord){graphRect.coord.x, frame.zeroY}, (tCoord){graphRect.coord.x + graphRect.size.w, frame.zeroY}, 1.0);

    for (uint32_t i = 0; i < graph.count; i++) {
        const tEnvGraphSegment * segment = &graph.segment[i];
        double                   width   = segment->width * frame.scale * graphRect.size.w;
        double                   startX  = prev.x;

        if (segment->sustain || (segment->level == level) || (width <= 0.0)) {
            tCoord point = {startX + width, env_level_to_y(segment->level, outputType, frame.zeroY, frame.fullSwing)};

            // The sustain plateau is orange - matches the original editor's own colouring (manual).
            set_rgb_colour(segment->sustain ? (tRgb)RGB_ORANGE_1 : (tRgb)RGB_GREEN_ON);
            render_line(moduleArea, prev, point, 1.5);
            prev = point;
        } else {
            set_rgb_colour((tRgb)RGB_GREEN_ON);

            for (int step = 1; step <= numCurveSteps; step++) {
                double t     = (double)step / (double)numCurveSteps;
                double value = (segment->level > level)
                               ? level + ((segment->level - level) * env_attack_level(graph.shape, t))
                               : envadsr_decay_level(t, level, segment->level, graph.shape);
                tCoord point = {startX + (t * width), env_level_to_y(value, outputType, frame.zeroY, frame.fullSwing)};

                render_line(moduleArea, prev, point, 1.5);
                prev = point;
            }
        }
        level = segment->level;
    }

    // notes §87: a handle on every breakpoint that has a time - sideways for the time and, where the
    // level is a parameter too, up and down for that.
    for (uint32_t i = 0; i < graph.count; i++) {
        const tEnvGraphSegment * segment = &graph.segment[i];
        bool                     leveled = (segment->levelParam != ENV_NO_PARAM);
        tCoord                   end;

        if (segment->timeParam == ENV_NO_PARAM) {
            continue;
        }
        end = env_graph_segment_end(&graph, graphRect, i);
        graph_handle_draw(end, true);
        graph_handle_register(module, handle++, (uint32_t)segment->timeParam, env_time_from_pointer,
                              leveled ? (uint32_t)segment->levelParam : 0u, leveled ? env_level_from_pointer : NULL,
                              (int32_t)i, graphRect, screen, end);
    }
}

// notes §63
typedef struct {
    int             freq;
    int             res;
    int             slope;         // parameter index
    int             slopeMode;     // mode index, for the two that keep it there
    int             shape;         // FilterType parameter, where the module is multi-mode
    int             gc;            // FltNord's Gain Control toggle, or -1 where there is none
    tFilterTopology topology;
} tFilterGraph;

static bool filter_graph_map(uint32_t moduleType, tFilterGraph * out) {
    // notes §64
    switch (moduleType) {
        case moduleTypeFltClassic:
            *out = (tFilterGraph){
                .freq     = 0, .res = 3, .slope = 4, .slopeMode = -1, .shape = -1, .gc = -1,
                .topology = eFilterTopologyLadder
            };
            return true;

        case moduleTypeFltNord:
            // dB/Oct is param 5 and FilterType param 8 - see param-validation.md.
            *out = (tFilterGraph){
                .freq     = 0, .res = 4, .slope = 5, .slopeMode = -1, .shape = 8, .gc = 3,
                .topology = eFilterTopologyNord
            };
            return true;

        case moduleTypeFltLP:
            *out = (tFilterGraph){
                .freq     = 0, .res = -1, .slope = -1, .slopeMode = 0, .shape = -1, .gc = -1,
                .topology = eFilterTopologyCascadeLP
            };
            return true;

        case moduleTypeFltHP:
            *out = (tFilterGraph){
                .freq     = 0, .res = -1, .slope = -1, .slopeMode = 0, .shape = -1, .gc = -1,
                .topology = eFilterTopologyCascadeHP
            };
            return true;

        case moduleTypeFltStatic:
            // Freq 0, Res 1, FilterType 2 (LP/BP/HP) - see param-validation.md.
            *out = (tFilterGraph){
                .freq     = 0, .res = 1, .slope = -1, .slopeMode = -1, .shape = 2, .gc = -1,
                .topology = eFilterTopologyBiquad
            };
            return true;

        default:
            return false;
    }
}

#define FILTER_GRAPH_FINE_SPAN     (2.0)     // grid steps either side of the cutoff
#define FILTER_GRAPH_FINE_STEPS    (16.0)

static void render_filter_response_graph(tRectangle rectangle, tModule * module) {
    tFilterGraph           map            = {0};

    if (filter_graph_map(module->type, &map) == false) {
        return;
    }
    const uint32_t         freqParamIndex = (uint32_t)map.freq;
    uint32_t               slot           = module->key.slot;
    uint32_t               variation      = gPatchDescr[slot].activeVariation;
    double                 cutoffKnob     = (double)module->param[variation][freqParamIndex].value / 127.0;

    // notes §65
    const double           kCutoffMinX    = 0.15;                                             // min-Freq cutoff sits 15% in from the left edge
    const double           kCutoffMaxX    = 0.90;                                             // max-Freq cutoff sits 10% in from the right edge
    double                 cutoffX        = kCutoffMinX + (cutoffKnob * (kCutoffMaxX - kCutoffMinX));
    uint32_t               slopeIndex     = (map.slope >= 0)
                                             ? module->param[variation][map.slope].value
                                             : ((map.slopeMode >= 0) ? module->mode[map.slopeMode].value : 0);
    tFilterShape           shape          = (map.shape >= 0)
                                             ? (tFilterShape)module->param[variation][map.shape].value
                                             : eFilterShapeLowPass;

    // notes §66
    double                 resKnob        = (map.res >= 0) ? (double)module->param[variation][map.res].value : 0.0;
    double                 feedback       = flt_ladder_feedback(resKnob);
    uint32_t               tap            = (module->type == moduleTypeFltNord)
                                             ? flt_nord_tap(slopeIndex) : flt_ladder_tap(slopeIndex);
    uint32_t               poles          = flt_cascade_poles(slopeIndex);
    double                 staticQ        = flt_static_q(resKnob);

    // notes §67
    double                 nordGain       = 1.0;
    double                 nordQ          = 0.5;
    uint32_t               nordStages     = 1;

    if (module->type == moduleTypeFltNord) {
        bool   gcOn    = (map.gc >= 0) && (module->param[variation][map.gc].value != 0);
        bool   slope24 = (slopeIndex != 0u);
        double r       = (resKnob >= 127.0) ? 1.0 : (resKnob / 128.0);
        double d       = 1.0 - (((shape == eFilterShapeBandReject) ? 0.5 : 0.99) * r);
        double qb      = slope24 ? fmax(d * d, M_SQRT1_2 * d) : (d * d);

        nordQ      = 0.5 / qb;
        nordStages = (slope24 && (shape != eFilterShapeBandReject)) ? 2u : 1u;
        nordGain   = gcOn ? d : 1.0;
    }
    const tGraphLocation * graphLoc       = find_graph_location(module->type);

    if (graphLoc == NULL) {
        return;   // mapped for its maths, but the original draws no box here
    }
    tRectangle             graphRect      = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 baseY          = graphRect.coord.y + (graphRect.size.h * 0.6);  // 0dB reference, leaving
                                                                                           // headroom above for the
                                                                                           // resonance peak to rise into
    const double           step           = 1.0 / 100.0;
    tCoord                 prev           = {0};
    double                 x              = 0.0;
    bool                   started        = false;   // has the curve been inside the box yet?
    bool                   wasOnPage      = false;

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, baseY}, (tCoord){graphRect.coord.x + graphRect.size.w, baseY}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; ; i++) {
        double octaves   = (x - cutoffX) * 10.0;   // ~10 octaves span the box, matching Freq's own real range
        double ratio     = pow(2.0, octaves);      // f/fc
        double magnitude = 0.0;

        switch (map.topology) {
            case eFilterTopologyCascadeLP:
            case eFilterTopologyCascadeHP:
                magnitude = flt_cascade_magnitude(ratio, poles, map.topology == eFilterTopologyCascadeHP);
                break;

            case eFilterTopologyBiquad:
                magnitude = flt_biquad_magnitude(ratio, staticQ, shape);
                break;

            case eFilterTopologyNord:
            {
                // reference §23: the instrument's band-pass peaks at Q rather than at one
                double stage = flt_biquad_magnitude(ratio, nordQ, shape) * ((shape == eFilterShapeBandPass) ? nordQ : 1.0);

                magnitude = ((nordStages == 2u) ? (stage * stage) : stage) * nordGain;
                break;
            }

            default:
                magnitude = flt_ladder_magnitude(ratio, feedback, tap) * nordGain;
                break;
        }
        double levelDb = 20.0 * log10(fmax(magnitude, 1e-4));
        double level   = fmax(-1.0, fmin(1.0, levelDb / 24.0));   // +-24dB fills the box vertically

        // notes §68
        tCoord point   = {graphRect.coord.x + (x * graphRect.size.w), baseY - (level * graphRect.size.h * 0.38)};

        // notes §69
        bool   onPage  = (level > -1.0);

        if (onPage) {
            started = true;
        }

        if ((i > 0) && started && (onPage || wasOnPage)) {
            render_line(moduleArea, prev, point, 1.5);
        }

        if ((started && !onPage) || (x >= 1.0)) {
            break;   // came down and left the box - nothing further is worth drawing
        }
        wasOnPage = onPage;
        prev      = point;

        // notes §68a - on a grid through the cutoff, finer around it, so a sharp peak is always sampled
        double here    = (fabs(x - cutoffX) < (FILTER_GRAPH_FINE_SPAN * step)) ? (step / FILTER_GRAPH_FINE_STEPS) : step;

        x         = fmin(cutoffX + ((floor(((x - cutoffX) / here) + 1e-9) + 1.0) * here), 1.0);
    }
}

static double graph_param_raw(tModule * module, uint32_t variation, uint32_t index) {
    return (double)module->param[variation][index].value;
}

// notes §78
static void render_shaper_transfer_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc   = find_graph_location(module->type);
    uint32_t               variation  = gPatchDescr[module->key.slot].activeVariation;
    tShaperSettings        shaper     = {0};

    if ((graphLoc == NULL) || (shaper_settings_build(module, variation, graph_param_raw, &shaper) == false)) {
        return;
    }
    tRectangle             graphRect  = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 midX       = graphRect.coord.x + (graphRect.size.w / 2.0);
    double                 midY       = graphRect.coord.y + (graphRect.size.h / 2.0);
    double                 halfWidth  = graphRect.size.w * 0.45;
    double                 halfHeight = graphRect.size.h * 0.45;
    const int              numSamples = 200;    // WaveWrap at full Amount folds nine times across the box
    tCoord                 prev       = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, midY}, (tCoord){graphRect.coord.x + graphRect.size.w, midY}, 1.0);
    render_line(moduleArea, (tCoord){midX, graphRect.coord.y}, (tCoord){midX, graphRect.coord.y + graphRect.size.h}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double input  = ((2.0 * (double)i) / (double)numSamples) - 1.0;
        double output = shaper_transfer(&shaper, shaper.amount, input);
        tCoord point  = {midX + (input * halfWidth), midY - (output * halfHeight)};

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

// notes §79
static void render_eq_response_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc    = find_graph_location(module->type);
    uint32_t               variation   = gPatchDescr[module->key.slot].activeVariation;
    tEqBands               bands       = {0};

    if ((graphLoc == NULL) || (eq_bands_build(module, variation, graph_param_raw, &bands) == false)) {
        return;
    }
    tRectangle             graphRect   = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 zeroDbY     = graphRect.coord.y + (graphRect.size.h / 2.0);
    const double           lowestHz    = 20.0;
    const double           octaves     = 10.0;  // 20 Hz to 20.5 kHz
    const double           fullScaleDb = 20.0;  // the gain dials reach +-18
    const int              numSamples  = 100;
    tCoord                 prev        = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, zeroDbY}, (tCoord){graphRect.coord.x + graphRect.size.w, zeroDbY}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double x       = (double)i / (double)numSamples;
        double levelDb = 20.0 * log10(fmax(eq_magnitude(&bands, lowestHz * exp2(x * octaves)), 1e-4));
        double level   = fmax(-1.0, fmin(1.0, levelDb / fullScaleDb));
        tCoord point   = {graphRect.coord.x + (x * graphRect.size.w), zeroDbY - (level * graphRect.size.h * 0.45)};

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

// The box every response graph sits in, with its 0 dB line across the middle.
static void render_response_graph_box(tRectangle graphRect) {
    double zeroDbY = graphRect.coord.y + (graphRect.size.h / 2.0);

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, zeroDbY}, (tCoord){graphRect.coord.x + graphRect.size.w, zeroDbY}, 1.0);
}

// A gain as a height in that box: 0 dB across the middle, +-fullScaleDb at 45% either side of it.
static double response_graph_y(tRectangle graphRect, double magnitude, double fullScaleDb) {
    double levelDb = 20.0 * log10(fmax(magnitude, 1e-4));
    double level   = fmax(-1.0, fmin(1.0, levelDb / fullScaleDb));

    return graphRect.coord.y + (graphRect.size.h / 2.0) - (level * graphRect.size.h * 0.45);
}

#define FLTCOMB_GRAPH_FREQ    (0)    // §13.1
#define FLTCOMB_GRAPH_FB      (3)
#define FLTCOMB_GRAPH_TYPE    (5)

// notes §80
static void render_comb_response_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc   = find_graph_location(module->type);

    if ((module->type != moduleTypeFltComb) || (graphLoc == NULL)) {
        return;
    }
    uint32_t               variation  = gPatchDescr[module->key.slot].activeVariation;
    const tCombShape *     shape      = flt_comb_shape(module->param[variation][FLTCOMB_GRAPH_TYPE].value);
    double                 g          = flt_comb_feedback(graph_param_raw(module, variation, FLTCOMB_GRAPH_FB));
    double                 delay      = flt_comb_delay_samples(graph_param_raw(module, variation, FLTCOMB_GRAPH_FREQ), shape, FLTCOMB_REFERENCE_RATE);
    tRectangle             graphRect  = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    const double           teeth      = 4.0;
    const int              numSamples = 200;
    tCoord                 prev       = {0};

    render_response_graph_box(graphRect);
    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double x         = (double)i / (double)numSamples;
        double magnitude = flt_comb_magnitude(shape, g, delay, x * teeth / delay);
        tCoord point     = {graphRect.coord.x + (x * graphRect.size.w), response_graph_y(graphRect, magnitude, 24.0)};

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

// notes §81
static void render_phaser_response_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc   = find_graph_location(module->type);
    uint32_t               variation  = gPatchDescr[module->key.slot].activeVariation;
    tPhaserSettings        phaser     = {0};

    if ((graphLoc == NULL) || (flt_phase_settings_build(module, variation, graph_param_raw, &phaser) == false)) {
        return;
    }
    tRectangle             graphRect  = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    const double           lowestHz   = 20.0;
    const double           octaves    = 10.0;
    const int              numSamples = 200;
    tCoord                 prev       = {0};

    render_response_graph_box(graphRect);
    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double x         = (double)i / (double)numSamples;
        double magnitude = flt_phase_magnitude(&phaser, lowestHz * exp2(x * octaves));
        tCoord point     = {graphRect.coord.x + (x * graphRect.size.w), response_graph_y(graphRect, magnitude, 24.0)};

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

#define VOCODER_BANDS    (16)

// notes §82
static void render_vocoder_routing_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc  = find_graph_location(module->type);

    if ((module->type != moduleTypeVocoder) || (graphLoc == NULL)) {
        return;
    }
    uint32_t               variation = gPatchDescr[module->key.slot].activeVariation;
    tRectangle             graphRect = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 bandWidth = graphRect.size.w / (double)VOCODER_BANDS;
    double                 topY      = graphRect.coord.y + (graphRect.size.h * 0.12);
    double                 bottomY   = graphRect.coord.y + (graphRect.size.h * 0.88);

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);

    for (uint32_t band = 0; band < VOCODER_BANDS; band++) {
        double x = graphRect.coord.x + (((double)band + 0.5) * bandWidth);

        render_line(moduleArea, (tCoord){x, graphRect.coord.y + (graphRect.size.h * 0.04)}, (tCoord){x, topY}, 1.0);
        render_line(moduleArea, (tCoord){x, bottomY}, (tCoord){x, graphRect.coord.y + (graphRect.size.h * 0.96)}, 1.0);
    }

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (uint32_t synthesis = 0; synthesis < VOCODER_BANDS; synthesis++) {
        uint32_t analysis = module->param[variation][synthesis].value;   // vocoderStrMap: 0 is Off, then 1..16

        if ((analysis == 0) || (analysis > VOCODER_BANDS)) {
            continue;
        }
        render_line(moduleArea,
                    (tCoord){graphRect.coord.x + (((double)analysis - 0.5) * bandWidth), topY},
                    (tCoord){graphRect.coord.x + (((double)synthesis + 0.5) * bandWidth), bottomY}, 1.5);
    }
}

#define DX_COLUMNS    (6)
#define DX_LEVELS     (4)

typedef struct {
    double   x[DX_OPERATORS];       // in columns
    uint32_t level[DX_OPERATORS];   // carriers 0
    uint32_t levels;
    double   columns;
} tDxLayout;

static bool dx_single_target(uint8_t target) {
    return (target != 0) && ((target & (target - 1)) == 0);
}

static void dx_place(const tDxAlgorithm * alg, const double width[DX_OPERATORS], uint32_t op, double left, tDxLayout * layout) {
    layout->x[op] = left + (width[op] / 2.0);

    for (uint32_t child = op + 1; child < DX_OPERATORS; child++) {
        if (alg->target[child] == (1u << op)) {
            dx_place(alg, width, child, left, layout);
            left += width[child];
        }
    }
}

// notes §85
static void dx_layout(const tDxAlgorithm * alg, tDxLayout * layout) {
    double width[DX_OPERATORS]    = {0};
    double childSum[DX_OPERATORS] = {0};
    double left                   = 0.0;

    layout->levels = 1;

    for (uint32_t op = 0; op < DX_OPERATORS; op++) {
        layout->level[op] = 0;

        for (uint32_t t = 0; t < op; t++) {
            if (((alg->target[op] & (1u << t)) != 0) && (layout->level[t] >= layout->level[op])) {
                layout->level[op] = layout->level[t] + 1;
            }
        }

        if (layout->level[op] + 1 > layout->levels) {
            layout->levels = layout->level[op] + 1;
        }
    }

    for (int32_t op = DX_OPERATORS - 1; op >= 0; op--) {
        width[op] = fmax(1.0, childSum[op]);

        if (dx_single_target(alg->target[op])) {
            childSum[__builtin_ctz(alg->target[op])] += width[op];
        }
    }

    for (uint32_t op = 0; op < DX_OPERATORS; op++) {
        if (alg->target[op] == 0) {
            dx_place(alg, width, op, left, layout);
            left += width[op];
        }
    }

    layout->columns = left;

    for (uint32_t op = 0; op < DX_OPERATORS; op++) {
        if ((alg->target[op] != 0) && !dx_single_target(alg->target[op])) {
            double   sum     = 0.0;
            uint32_t targets = 0;

            for (uint32_t t = 0; t < op; t++) {
                if ((alg->target[op] & (1u << t)) != 0) {
                    sum += layout->x[t];
                    targets++;
                }
            }

            layout->x[op] = sum / (double)targets;
        }
    }
}

static void render_dxrouter_algorithm_graph(tRectangle rectangle, tModule * module) {
    static const char *    kOpName[DX_OPERATORS] = {"1", "2", "3", "4", "5", "6"};
    const tGraphLocation * graphLoc              = find_graph_location(module->type);

    if ((module->type != moduleTypeDXRouter) || (graphLoc == NULL)) {
        return;
    }
    uint32_t               variation             = gPatchDescr[module->key.slot].activeVariation;
    uint32_t               algorithm             = module->param[variation][0].value;
    uint32_t               feedback              = module->param[variation][1].value;
    const tDxAlgorithm *   alg                   = dx_algorithm(algorithm);
    tRectangle             graphRect             = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    tDxLayout              layout                = {0};

    dx_layout(alg, &layout);

    // A fixed grid, half a level spare at the foot for the output line, and the algorithm centred in
    // it both ways - so stepping through algorithms moves boxes rather than resizing them.
    double                 cellW                 = graphRect.size.w / (double)DX_COLUMNS;
    double                 cellH                 = graphRect.size.h / ((double)DX_LEVELS + 0.5);
    double                 boxW                  = cellW * 0.5;
    double                 boxH                  = cellH * 0.55;
    double                 gap                   = (cellH - boxH) / 2.0;
    double                 textH                 = boxH * 0.8;
    double                 originX               = graphRect.coord.x + ((((double)DX_COLUMNS - layout.columns) * cellW) / 2.0);
    double                 footY                 = graphRect.coord.y + graphRect.size.h - (((double)(DX_LEVELS - layout.levels) * cellH) / 2.0);
    double                 busY                  = footY - (cellH * 0.3);
    double                 busLeft               = graphRect.coord.x + graphRect.size.w;
    double                 busRight              = graphRect.coord.x;
    tCoord                 centre[DX_OPERATORS];

    for (uint32_t op = 0; op < DX_OPERATORS; op++) {
        centre[op] = (tCoord){
            originX + (layout.x[op] * cellW), footY - (cellH * 0.5) - (((double)layout.level[op] + 0.5) * cellH)
        };
    }

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    // Lines first and boxes over them, so a line ends at a box's edge whatever angle it comes in at.
    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (uint32_t op = 0; op < DX_OPERATORS; op++) {
        tCoord bottom = {centre[op].x, centre[op].y + (boxH / 2.0)};

        if (alg->target[op] == 0) {
            render_line(moduleArea, bottom, (tCoord){bottom.x, busY}, 1.5);
            busLeft  = fmin(busLeft, bottom.x - (boxW / 2.0));
            busRight = fmax(busRight, bottom.x + (boxW / 2.0));
            continue;
        }

        for (uint32_t t = 0; t < op; t++) {
            if ((alg->target[op] & (1u << t)) != 0) {
                render_line(moduleArea, bottom, (tCoord){centre[t].x, centre[t].y - (boxH / 2.0)}, 1.5);
            }
        }
    }

    render_line(moduleArea, (tCoord){busLeft, busY}, (tCoord){busRight, busY}, 1.5);

    // notes §85: out of the bottom of one end, round to the right of both, into the top of the other.
    uint32_t from       = alg->feedbackFrom - 1u;
    uint32_t to         = alg->feedbackTo - 1u;
    double   loopX      = fmax(centre[from].x, centre[to].x) + (boxW / 2.0) + (cellW * 0.18);
    double   loopBottom = centre[from].y + (boxH / 2.0) + (gap * 0.6);
    double   loopTop    = centre[to].y - (boxH / 2.0) - (gap * 0.6);

    set_rgb_colour((feedback > 0) ? (tRgb)RGB_ORANGE_1 : (tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){centre[from].x, centre[from].y + (boxH / 2.0)}, (tCoord){centre[from].x, loopBottom}, 1.5);
    render_line(moduleArea, (tCoord){centre[from].x, loopBottom}, (tCoord){loopX, loopBottom}, 1.5);
    render_line(moduleArea, (tCoord){loopX, loopBottom}, (tCoord){loopX, loopTop}, 1.5);
    render_line(moduleArea, (tCoord){loopX, loopTop}, (tCoord){centre[to].x, loopTop}, 1.5);
    render_line(moduleArea, (tCoord){centre[to].x, loopTop}, (tCoord){centre[to].x, centre[to].y - (boxH / 2.0)}, 1.5);

    for (uint32_t op = 0; op < DX_OPERATORS; op++) {
        double textW = get_text_width(kOpName[op], textH, eNoCache);

        set_rgb_colour((tRgb)RGB_GREY_7);
        render_rectangle(moduleArea, (tRectangle){{centre[op].x - (boxW / 2.0), centre[op].y - (boxH / 2.0)}, {boxW, boxH}});
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(moduleArea, (tRectangle){{centre[op].x - (textW / 2.0), centre[op].y - (textH / 2.0)}, {BLANK_SIZE, textH}}, kOpName[op]);
    }
}

#define COMPRESS_AXIS_MIN_DB    (-36.0)
#define COMPRESS_AXIS_MAX_DB    (12.0)
#define COMPRESS_GRAPH_STEPS    (48)
enum {
    eCompressHandleThreshold,
    eCompressHandleRatio,
    eCompressHandleRef
};

static double compress_db_from_x(tRectangle box, double x) {
    return COMPRESS_AXIS_MIN_DB + (((x - box.coord.x) / box.size.w) * (COMPRESS_AXIS_MAX_DB - COMPRESS_AXIS_MIN_DB));
}

static double compress_db_from_y(tRectangle box, double y) {
    return COMPRESS_AXIS_MAX_DB - (((y - box.coord.y) / box.size.h) * (COMPRESS_AXIS_MAX_DB - COMPRESS_AXIS_MIN_DB));
}

static tCoord compress_point(tRectangle box, double inDb, double outDb) {
    double span = COMPRESS_AXIS_MAX_DB - COMPRESS_AXIS_MIN_DB;

    inDb  = fmin(fmax(inDb, COMPRESS_AXIS_MIN_DB), COMPRESS_AXIS_MAX_DB);
    outDb = fmin(fmax(outDb, COMPRESS_AXIS_MIN_DB), COMPRESS_AXIS_MAX_DB);
    return (tCoord){
        box.coord.x + (((inDb - COMPRESS_AXIS_MIN_DB) / span) * box.size.w),
        box.coord.y + (((COMPRESS_AXIS_MAX_DB - outDb) / span) * box.size.h)
    };
}

static uint32_t compress_level_raw(double db, uint32_t maxRaw) {
    double raw = round(db + COMPRESS_DB_OFFSET);

    return (raw <= 0.0) ? 0u : (((uint32_t)raw > maxRaw) ? maxRaw : (uint32_t)raw);
}

// The handles' pointer-to-value laws - the inverse of how the graph places them.
static uint32_t compress_threshold_from_pointer(tModuleKey key, tRectangle graphBox, tCoord pointer, int32_t item) {
    (void)item;
    (void)key;
    return compress_level_raw(compress_db_from_x(graphBox, pointer.x), COMPRESS_THRESHOLD_OFF_RAW);   // past +11 dB: Off
}

static uint32_t compress_ref_from_pointer(tModuleKey key, tRectangle graphBox, tCoord pointer, int32_t item) {
    (void)item;
    (void)key;
    return compress_level_raw(compress_db_from_y(graphBox, pointer.y), COMPRESS_LEVEL_RAW_MAX);
}

static uint32_t compress_ratio_from_pointer(tModuleKey key, tRectangle graphBox, tCoord pointer, int32_t item) {
    (void)item;
    tModule * module   = get_module(key);

    if (module == NULL) {
        return 0;
    }
    tParam *  p        = module->param[gPatchDescr[key.slot].activeVariation];
    double    thrDb    = (double)p[COMPRESS_PARAM_THRESHOLD].value - COMPRESS_DB_OFFSET;
    double    targetDb = fmax((double)p[COMPRESS_PARAM_REFLVL].value - COMPRESS_DB_OFFSET, thrDb);
    double    headroom = COMPRESS_AXIS_MAX_DB - targetDb;   // how far the right-hand edge input is above the target
    double    rise     = compress_db_from_y(graphBox, pointer.y) - targetDb;

    if ((p[COMPRESS_PARAM_THRESHOLD].value >= COMPRESS_THRESHOLD_OFF_RAW) || (headroom <= 0.5)) {
        return p[COMPRESS_PARAM_RATIO].value;   // no slope to take hold of
    }
    return compress_ratio_raw((rise >= headroom) ? 1.0 : ((rise <= (headroom / 100.0)) ? 100.0 : (headroom / rise)));
}

// notes §87
static void render_compress_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc = find_graph_location(module->type);

    if ((module->type != moduleTypeCompress) || (graphLoc == NULL)) {
        return;
    }
    tParam *               p        = module->param[gPatchDescr[module->key.slot].activeVariation];
    tRectangle             box      = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    uint32_t               thrRaw   = p[COMPRESS_PARAM_THRESHOLD].value;
    uint32_t               refRaw   = p[COMPRESS_PARAM_REFLVL].value;
    uint32_t               ratioRaw = p[COMPRESS_PARAM_RATIO].value;
    bool                   off      = (thrRaw >= COMPRESS_THRESHOLD_OFF_RAW);
    double                 thrDb    = off ? COMPRESS_AXIS_MAX_DB : ((double)thrRaw - COMPRESS_DB_OFFSET);
    double                 refDb    = (double)refRaw - COMPRESS_DB_OFFSET;
    tCoord                 prev     = compress_point(box, COMPRESS_AXIS_MIN_DB, compress_out_db(COMPRESS_AXIS_MIN_DB, thrRaw, refRaw, ratioRaw));
    tRectangle             screen   = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    screen = render_rectangle(moduleArea, box);

    set_rgb_colour((tRgb)RGB_GREY_3);   // unity: what no compression would give
    render_line(moduleArea, compress_point(box, COMPRESS_AXIS_MIN_DB, COMPRESS_AXIS_MIN_DB), compress_point(box, COMPRESS_AXIS_MAX_DB, COMPRESS_AXIS_MAX_DB), 1.0);
    set_rgb_colour((tRgb)RGB_YELLOW_7); // the level it compresses towards
    render_line(moduleArea, compress_point(box, COMPRESS_AXIS_MIN_DB, refDb), compress_point(box, COMPRESS_AXIS_MAX_DB, refDb), 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int step = 1; step <= COMPRESS_GRAPH_STEPS; step++) {
        double inDb  = COMPRESS_AXIS_MIN_DB + (((double)step / (double)COMPRESS_GRAPH_STEPS) * (COMPRESS_AXIS_MAX_DB - COMPRESS_AXIS_MIN_DB));
        tCoord point = compress_point(box, inDb, compress_out_db(inDb, thrRaw, refRaw, ratioRaw));

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }

    // notes §87: where it is working now, from the gain-reduction LEDs (one bit each).
    uint32_t lit     = (uint32_t)__builtin_popcount(volume_source(module, 0) & 0xffu);

    if ((lit > 0u) && (off == false) && (compress_ratio(ratioRaw) > 1.0)) {
        // the meter shows reduction, over x (1 - 1/ratio): back to how far over Thr the input is
        double inDb = thrDb + (compress_meter_reduction_db(lit) / (1.0 - (1.0 / compress_ratio(ratioRaw))));
        tCoord live = compress_point(box, inDb, compress_out_db(inDb, thrRaw, refRaw, ratioRaw));

        set_rgb_colour((tRgb)RGB_ORANGE_1);
        render_rectangle(moduleArea, (tRectangle){{live.x - 2.5, live.y - 2.5}, {5.0, 5.0}});
    }
    tCoord   thrAt   = compress_point(box, thrDb, compress_out_db(thrDb, thrRaw, refRaw, ratioRaw));
    tCoord   ratioAt = compress_point(box, COMPRESS_AXIS_MAX_DB, compress_out_db(COMPRESS_AXIS_MAX_DB, thrRaw, refRaw, ratioRaw));
    tCoord   refAt   = compress_point(box, COMPRESS_AXIS_MIN_DB + 3.0, refDb);

    graph_handle_draw(refAt, true);
    graph_handle_draw(ratioAt, off == false);
    graph_handle_draw(thrAt, off == false);
    graph_handle_register(module, eCompressHandleRef, COMPRESS_PARAM_REFLVL, compress_ref_from_pointer, 0u, NULL, 0, box, screen, refAt);
    graph_handle_register(module, eCompressHandleRatio, COMPRESS_PARAM_RATIO, compress_ratio_from_pointer, 0u, NULL, 0, box, screen, ratioAt);
    graph_handle_register(module, eCompressHandleThreshold, COMPRESS_PARAM_THRESHOLD, compress_threshold_from_pointer, 0u, NULL, 0, box, screen, thrAt);
}

#define KBSCALE_LOWEST_NOTE    (9.0)      // A-1, the manual's range for BrPt and the graph
#define KBSCALE_NOTE_SPAN      (99.0)     // to C8
#define KBSCALE_STEPS          (24)
#define KBSCALE_FULL_SPAN      (0.5)      // of the axis: a depth reaches its full offset this far from BrPt

// DX level-scaling curves, by the menu: -Lin, -Exp, +Exp, +Lin.
static double kbscale_offset(uint32_t curve, double depth, double distance) {
    double sign  = (curve < 2) ? -1.0 : 1.0;
    bool   isExp = (curve == 1) || (curve == 2);
    double shape = isExp ? ((exp(4.0 * distance) - 1.0) / (exp(4.0) - 1.0)) : distance;

    return sign * depth * shape;
}

// notes §86
// notes §91 - the Drum Synth's picture: how long the drum rings, and how far it bends.
#define DRUM_GRAPH_MASTER_DCY    (2)
#define DRUM_GRAPH_SLAVE_DCY     (3)
#define DRUM_GRAPH_MASTER_LEV    (4)
#define DRUM_GRAPH_SLAVE_LEV     (5)
#define DRUM_GRAPH_NOISE_DCY     (9)
#define DRUM_GRAPH_BEND_AMT      (11)
#define DRUM_GRAPH_BEND_DCY      (12)
#define DRUM_GRAPH_NOISE_LEV     (14)
#define DRUM_GRAPH_STEPS         (48)
#define DRUM_GRAPH_HEADROOM      (0.9)   // leave the top of the box clear of the border

// One decaying voice at time t, as a fraction of where it started. Shape 0 is EnvADSR's own fall.
static double drum_decay_at(double t, double seconds) {
    double progress = (seconds > 0.0) ? (t / seconds) : 1.0;

    return (progress >= 1.0) ? 0.0 : env_fall_level(0u, progress);
}

static void render_drum_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc  = find_graph_location(module->type);

    if ((graphLoc == NULL) || (module->type != moduleTypeDrumSynth)) {
        return;
    }
    uint32_t               variation = gPatchDescr[module->key.slot].activeVariation;
    tRectangle             graphRect = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);

    double                 mLev      = graph_param_raw(module, variation, DRUM_GRAPH_MASTER_LEV) / 127.0;
    double                 sLev      = graph_param_raw(module, variation, DRUM_GRAPH_SLAVE_LEV) / 127.0;
    double                 nLev      = graph_param_raw(module, variation, DRUM_GRAPH_NOISE_LEV) / 127.0;
    double                 mDcy      = adr_time_seconds(graph_param_raw(module, variation, DRUM_GRAPH_MASTER_DCY));
    double                 sDcy      = adr_time_seconds(graph_param_raw(module, variation, DRUM_GRAPH_SLAVE_DCY));
    double                 nDcy      = adr_time_seconds(graph_param_raw(module, variation, DRUM_GRAPH_NOISE_DCY));
    double                 bend      = graph_param_raw(module, variation, DRUM_GRAPH_BEND_AMT) / 127.0;
    double                 bDcy      = adr_time_seconds(graph_param_raw(module, variation, DRUM_GRAPH_BEND_DCY));

    // The picture spans the drum's AUDIBLE length - the longest of the three voices - not the longest
    // thing on it. A fixed span would put a short kick in the first pixel and run a long tom off the
    // end; spanning the bend instead squashes the sound into a corner whenever the bend outlasts it,
    // which is most kicks. The bend simply runs to the right edge still falling, which is the truth:
    // it is still bending something that has already died away.
    double                 span      = fmax(mDcy, fmax(sDcy, nDcy));
    double                 peak      = mLev + sLev + nLev;
    double                 baseY     = graphRect.coord.y + graphRect.size.h;
    double                 height    = graphRect.size.h * DRUM_GRAPH_HEADROOM;

    if (span <= 0.0) {
        span = 1.0;
    }

    if (peak <= 0.0) {
        peak = 1.0;      // everything silenced: draw the shape it would have, flat on the floor
    }
    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, baseY},
                (tCoord){graphRect.coord.x + graphRect.size.w, baseY}, 1.0);

    // The bend first, so the amplitude reads over it where they cross.
    if (bend > 0.0) {
        tCoord prev = {graphRect.coord.x, baseY - (bend * height)};

        set_rgb_colour((tRgb)RGB_ORANGE_1);

        for (uint32_t i = 1; i <= DRUM_GRAPH_STEPS; i++) {
            double fraction = (double)i / (double)DRUM_GRAPH_STEPS;
            tCoord point    = {
                graphRect.coord.x + (fraction * graphRect.size.w),
                baseY - (bend * drum_decay_at(fraction * span, bDcy) * height)
            };

            render_line(moduleArea, prev, point, 1.0);
            prev = point;
        }
    }
    // The three voices summed at their own levels, against their own peak: the drum's own shape.
    tCoord prev = {graphRect.coord.x, baseY - height};

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (uint32_t i = 1; i <= DRUM_GRAPH_STEPS; i++) {
        double fraction = (double)i / (double)DRUM_GRAPH_STEPS;
        double t        = fraction * span;
        double level    = ((mLev * drum_decay_at(t, mDcy)) + (sLev * drum_decay_at(t, sDcy))
                           + (nLev * drum_decay_at(t, nDcy))) / peak;
        tCoord point    = {graphRect.coord.x + (fraction * graphRect.size.w), baseY - (level * height)};

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }
}

// notes §92 - OscNoise's band. Width alone shapes it, which is what the original's graph depends on
// too; the pitch only moves it, and a picture with no frequency axis cannot show that.
#define OSCNOISE_GRAPH_WIDTH       (6)   // §8.1 - the INSTRUMENT's numbering, not the face's old one
#define OSCNOISE_GRAPH_STEPS       (56)
#define OSCNOISE_GRAPH_OCTAVES     (2.5) // either side of centre
#define OSCNOISE_GRAPH_FLOOR_DB    (-42.0)

static void render_oscnoise_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc  = find_graph_location(module->type);

    if ((graphLoc == NULL) || (module->type != moduleTypeOscNoise)) {
        return;
    }
    uint32_t               variation = gPatchDescr[module->key.slot].activeVariation;
    tRectangle             graphRect = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 width     = graph_param_raw(module, variation, OSCNOISE_GRAPH_WIDTH);
    // §8.3 - Q per resonator, two of them in series
    double                 q         = osc_noise_resonator_q(width);
    double                 baseY     = graphRect.coord.y + graphRect.size.h;
    tCoord                 prev      = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (uint32_t i = 0; i <= OSCNOISE_GRAPH_STEPS; i++) {
        double fraction = (double)i / (double)OSCNOISE_GRAPH_STEPS;
        // Log frequency about the centre, so a narrow band stays a symmetrical spike
        double octaves  = ((fraction * 2.0) - 1.0) * OSCNOISE_GRAPH_OCTAVES;
        double ratio    = exp2(octaves);
        double detune   = (ratio) - (1.0 / ratio);
        double single   = 1.0 / sqrt(1.0 + ((q * detune) * (q * detune)));
        double both     = single * single;                       // §8.2 - two in series
        double levelDb  = 20.0 * log10(fmax(both, 1e-6));
        double level    = fmax(0.0, 1.0 - (levelDb / OSCNOISE_GRAPH_FLOOR_DB));
        tCoord point    = {
            graphRect.coord.x + (fraction * graphRect.size.w),
            baseY - (level * graphRect.size.h * 0.92)
        };

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

static void render_operator_kbscale_graph(tRectangle rectangle, tModule * module) {
    const tGraphLocation * graphLoc   = find_graph_location_nth(module->type, 1);

    if ((module->type != moduleTypeOperator) || (graphLoc == NULL)) {
        return;
    }
    uint32_t               variation  = gPatchDescr[module->key.slot].activeVariation;
    tParam *               p          = module->param[variation];
    tRectangle             graphRect  = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 zeroY      = graphRect.coord.y + (graphRect.size.h / 2.0);
    double                 swing      = graphRect.size.h * 0.45;
    double                 breakAt    = fmin(fmax(((double)p[17].value - KBSCALE_LOWEST_NOTE) / KBSCALE_NOTE_SPAN, 0.0), 1.0);
    double                 breakX     = graphRect.coord.x + (breakAt * graphRect.size.w);
    // The depths are menus whose top value is full depth, whatever the table gives as their range.
    double                 leftMax    = (double)paramLocationList[p[19].paramRef].range - 1.0;
    double                 rightMax   = (double)paramLocationList[p[21].paramRef].range - 1.0;
    double                 leftDepth  = (leftMax > 0.0) ? ((double)p[19].value / leftMax) : 0.0;
    double                 rightDepth = (rightMax > 0.0) ? ((double)p[21].value / rightMax) : 0.0;
    tCoord                 prev       = {graphRect.coord.x, 0.0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);
    set_rgb_colour((tRgb)RGB_YELLOW_7);
    render_line(moduleArea, (tCoord){graphRect.coord.x, zeroY}, (tCoord){graphRect.coord.x + graphRect.size.w, zeroY}, 1.0);
    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){breakX, graphRect.coord.y}, (tCoord){breakX, graphRect.coord.y + graphRect.size.h}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int step = 0; step <= (2 * KBSCALE_STEPS); step++) {
        double x        = (double)step / (double)(2 * KBSCALE_STEPS);
        bool   left     = (x < breakAt);
        double distance = fmin((left ? (breakAt - x) : (x - breakAt)) / KBSCALE_FULL_SPAN, 1.0);
        double offset   = left ? kbscale_offset(p[18].value, leftDepth, distance) : kbscale_offset(p[20].value, rightDepth, distance);
        tCoord point    = {graphRect.coord.x + (x * graphRect.size.w), zeroY - (fmin(fmax(offset, -1.0), 1.0) * swing)};

        if (step > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }
        prev = point;
    }
}

#define KEYBOARD_WHITE_KEYS    (7)
#define KEYBOARD_BLACK_KEYS    (5)

// notes §84
static void render_keyquant_keyboard(tRectangle rectangle, tModule * module) {
    static const uint32_t  kWhiteParam[KEYBOARD_WHITE_KEYS] = {10, 12, 2, 3, 5, 7, 9};   // C D E F G A B
    static const uint32_t  kBlackParam[KEYBOARD_BLACK_KEYS] = {11, 13, 4, 6, 8};         // C# D# F# G# A#
    static const double    kBlackAt[KEYBOARD_BLACK_KEYS]    = {1.0, 2.0, 4.0, 5.0, 6.0}; // white-key boundary each sits on
    const tGraphLocation * graphLoc                         = find_graph_location(module->type);

    if ((module->type != moduleTypeKeyQuant) || (graphLoc == NULL)) {
        return;
    }
    uint32_t               variation                        = gPatchDescr[module->key.slot].activeVariation;
    tRectangle             box                              = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 whiteWidth                       = box.size.w / (double)KEYBOARD_WHITE_KEYS;
    double                 blackWidth                       = whiteWidth * 0.6;
    double                 blackHeight                      = box.size.h * 0.6;
    tParamClickCtx *       ctx                              = sParamClickCtx[module->key.slot][module->key.location][module->key.index];

    // White keys first and black keys after, so the black ones are painted - and, because the most
    // recently registered click region wins, clicked - on top. Each key hands the click to its own
    // note parameter's context, so it toggles exactly as that parameter's on/off widget does.
    for (uint32_t key = 0; key < KEYBOARD_WHITE_KEYS; key++) {
        uint32_t   param = kWhiteParam[key];
        tRectangle shape = {{box.coord.x + (key * whiteWidth), box.coord.y}, {whiteWidth, box.size.h}};

        ctx[param] = (tParamClickCtx){
            eCanvasWidgetParam, module->key, param
        };
        set_rgb_colour((module->param[variation][param].value != 0) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_GREY_9);
        register_click_region(render_rectangle(moduleArea, shape), eClickLayerCanvas, param_click_handler, &ctx[param]);
        set_rgb_colour((tRgb)RGB_BLACK);
        render_line(moduleArea, shape.coord, (tCoord){shape.coord.x, shape.coord.y + shape.size.h}, 1.0);
    }

    for (uint32_t key = 0; key < KEYBOARD_BLACK_KEYS; key++) {
        uint32_t   param = kBlackParam[key];
        tRectangle shape = {{box.coord.x + (kBlackAt[key] * whiteWidth) - (blackWidth / 2.0), box.coord.y}, {blackWidth, blackHeight}};

        ctx[param] = (tParamClickCtx){
            eCanvasWidgetParam, module->key, param
        };
        set_rgb_colour((module->param[variation][param].value != 0) ? (tRgb)RGB_GREEN_3 : (tRgb)RGB_GREY_2);
        register_click_region(render_rectangle(moduleArea, shape), eClickLayerCanvas, param_click_handler, &ctx[param]);
    }

    set_rgb_colour((tRgb)RGB_BLACK);
    render_line(moduleArea, box.coord, (tCoord){box.coord.x + box.size.w, box.coord.y}, 1.0);
    render_line(moduleArea, (tCoord){box.coord.x, box.coord.y + box.size.h}, (tCoord){box.coord.x + box.size.w, box.coord.y + box.size.h}, 1.0);
    render_line(moduleArea, (tCoord){box.coord.x + box.size.w, box.coord.y}, (tCoord){box.coord.x + box.size.w, box.coord.y + box.size.h}, 1.0);
}

static tModuleClickCtx sGraphAreaCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES];

static void graph_area_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;
    (void)phase;
    (void)userData;   // swallowed: see guard_graph_areas()
}

// notes §88: a press on a graph but off its handles does nothing, rather than dragging the module.
// notes §90 - a colour a given fraction of the way from this one to white. Derived from the module's
// OWN colour rather than being a constant, since the user can set any of the 25 body colours and a
// fixed grey band would look pasted on over half of them.
static tRgb lighten_rgb(tRgb rgb, double fraction) {
    return (tRgb){
        rgb.red + ((1.0 - rgb.red) * fraction),
        rgb.green + ((1.0 - rgb.green) * fraction),
        rgb.blue + ((1.0 - rgb.blue) * fraction)
    };
}

// Registered after the module body and before the graphs' own handles and keys, which therefore win.
static void guard_graph_areas(tRectangle rectangle, tModule * module) {
    double                 scale = module->rectangle.size.w / rectangle.size.w;
    const tGraphLocation * graphLoc;

    sGraphAreaCtx[module->key.slot][module->key.location][module->key.index] = (tModuleClickCtx){
        eCanvasWidgetNone, module->key
    };

    for (uint32_t n = 0; (graphLoc = find_graph_location_nth(module->type, n)) != NULL; n++) {
        tRectangle box    = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
        tRectangle screen = {{
                                 module->rectangle.coord.x + ((box.coord.x - rectangle.coord.x) * scale),
                                 module->rectangle.coord.y + ((box.coord.y - rectangle.coord.y) * scale)
                             },
                             {box.size.w * scale, box.size.h * scale}};

        register_click_region(screen, eClickLayerCanvas, graph_area_click_handler,
                              &sGraphAreaCtx[module->key.slot][module->key.location][module->key.index]);
    }
}

static bool sShowNameBand = false;

// Backdoor NAMEBAND (backdoor.c's notes §28): a view setting, never saved.
void set_module_name_band_visible(bool visible) {
    sShowNameBand = visible;
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
            tRectangle relative = centre_on_drawn_height(paramLocationList[i].rectangle, paramLocationList[i].anchor,
                                                         param_drawn_height(paramLocationList[i].rectangle, paramLocationList[i].type));
            tRectangle adjusted = adjust_rectangle(rectangle, relative, paramLocationList[i].anchor, module);
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
            tRectangle relative = centre_on_drawn_height(modeLocationList[i].rectangle, modeLocationList[i].anchor,
                                                         mode_drawn_height(modeLocationList[i].rectangle, modeLocationList[i].type));
            tRectangle adjusted = adjust_rectangle(rectangle, relative, modeLocationList[i].anchor, module);
            render_mode_common(adjusted, module, i, mode++);

            if (mode >= module_mode_count(module->type)) {
                break;
            }
        }
    }

    render_module_connectors(rectangle, module);
    guard_graph_areas(rectangle, module);

    if (  (module->type == moduleTypeOscShpB) || (module->type == moduleTypeOscShpA)
       || (module->type == moduleTypeLfoShpA) || (module->type == moduleTypeLfoB)
       || (module->type == moduleTypeOscB) || (module->type == moduleTypeOscA)) {
        render_oscshpb_waveform_graph(rectangle, module);
    }
    render_envelope_graph(rectangle, module);
    render_filter_response_graph(rectangle, module);
    render_shaper_transfer_graph(rectangle, module);
    render_eq_response_graph(rectangle, module);
    render_comb_response_graph(rectangle, module);
    render_phaser_response_graph(rectangle, module);
    render_vocoder_routing_graph(rectangle, module);
    render_dxrouter_algorithm_graph(rectangle, module);
    render_operator_kbscale_graph(rectangle, module);
    render_compress_graph(rectangle, module);
    render_drum_graph(rectangle, module);
    render_oscnoise_graph(rectangle, module);
    render_keyquant_keyboard(rectangle, module);

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

    // No index cache for this one, unlike the loops above: displayLocationList has ten entries where
    // paramLocationList has a thousand, so a scan costs nothing and there is no cache to go stale.
    for (uint32_t i = 0; i < array_size_display_location_list(); i++) {
        if (displayLocationList[i].moduleType == module->type) {
            tRectangle adjusted = adjust_rectangle(rectangle, displayLocationList[i].rectangle, displayLocationList[i].anchor, module);

            render_display_common(adjusted, module, i);
        }
    }

    // Section headings. No index cache and no hit-testing: there are a handful of rows in the whole
    // table, they never respond to the mouse, and nothing about them depends on the module's state.
    // notes §90 - each sits on a band of its module's own colour, lightened.
    for (uint32_t i = 0; i < array_size_label_location_list(); i++) {
        if (labelLocationList[i].moduleType == module->type) {
            tRectangle adjusted = adjust_rectangle(rectangle, labelLocationList[i].rectangle, labelLocationList[i].anchor, module);

            adjusted.size.w = BLANK_SIZE;
            adjusted.size.h = STANDARD_TEXT_HEIGHT;

            tRectangle band     = adjusted;

            band.size.w     = get_text_width((char *)labelLocationList[i].text, adjusted.size.h, eNoCache)
                              + (2.0 * LABEL_BAND_PAD);
            band.coord.x   -= LABEL_BAND_PAD;
            set_rgb_colour(lighten_rgb(gModuleColourMap[module->colour], LABEL_BAND_LIGHTEN));
            render_rectangle(moduleArea, band);

            set_rgb_colour((tRgb)RGB_BLACK);
            render_text(moduleArea, adjusted, (char *)labelLocationList[i].text);
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

            // Rows, not stream slots — see module_led_row_count().
            if (led >= module_led_row_count(module->type)) {
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
        eCanvasWidgetModule, module->key
    };
    // notes §70
    register_click_region(module->rectangle, eClickLayerCanvas, drag_area_click_handler,
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
    // NO TITLE STRIP. A lighter-coloured bar used to be drawn across the top as the drag handle;
    // the whole face drags now, so it marked nothing, and the original editor has no such bar. The
    // module name still draws in that space - it is just no longer fenced off by a colour change.
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
        // notes §71
        COPY_STRING(buff, module->name);
        double nameX = moduleRectangle.coord.x + 5.0;

        if (module->type == moduleTypeName) {
            double nameWidth = get_text_width(buff, STANDARD_TEXT_HEIGHT, eNoCache);
            double centred   = moduleRectangle.coord.x + ((moduleRectangle.size.w - nameWidth) / 2.0);

            if (centred > nameX) {
                nameX = centred;    // a name too wide to centre stays hard left
            }
        }
        set_rgba_colour((tRgba)RGBA_BLACK_ON_TRANSPARENT);
        render_text(moduleArea, (tRectangle){{nameX, moduleRectangle.coord.y + 5.0},
                                             {BLANK_SIZE, STANDARD_TEXT_HEIGHT}
                    }, buff);
    }

    if (sShowNameBand) {
        // Where the widest name a module can carry would go - Docs/module-layout-rules.md, rule 16.
        set_rgba_colour((tRgba){1.0, 0.2, 0.2, 0.35});
        render_rectangle(moduleArea, (tRectangle){{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0},
                                                  {get_text_width(LONGEST_MODULE_NAME, STANDARD_TEXT_HEIGHT, eNoCache), STANDARD_TEXT_HEIGHT}
                         });
    }

    // notes §89
    if (sound_engine_active() && (sound_engine_models_module(module) == false)) {
        set_rgba_colour((tRgba){0.5, 0.5, 0.5, 0.6});
        render_rectangle(moduleArea, moduleRectangle);
    }
    // notes §72

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
            // notes §73
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

    // notes §74
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

    // No blend enable/disable: blending is on for the whole session (render_backend_init()).
    // The pair that used to sit here was compensating for render_text() disabling it.
    if (alpha < 1.0) {
        set_rgba_colour((tRgba){colour.red, colour.green, colour.blue, alpha});
    } else {
        set_rgb_colour(colour);
    }
    int       fromConnectorIndex = find_index_from_io_count(moduleFrom, (tConnectorDir)cable->key.linkType, cable->key.connectorFromIoCount);

    int       toConnectorIndex   = find_index_from_io_count(moduleTo, connectorDirIn, cable->key.connectorToIoCount);

    if (fromConnectorIndex != -1 && toConnectorIndex != -1) {
        render_cable_from_to(moduleFrom->connector[fromConnectorIndex], moduleTo->connector[toConnectorIndex], 4.0);
    }
}

// notes §75
static bool cable_is_being_rerouted(const tCable * cable) {
    if (!gCableDrag.active || !gCableDrag.rerouting) {
        return false;
    }
    return cable_touches_connector(cable, gCableDrag.rerouteModuleIndex,
                                   gCableDrag.rerouteIoCount, gCableDrag.rerouteDir);
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

        if (cable == NULL || !cable->active || cable_is_being_rerouted(cable)) {
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

            if (cable == NULL || !cable->active || cable_is_being_rerouted(cable)) {
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

// notes §76
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
    double     textHeight       = 0.0;
    bool       isKnob           = false;
    uint8_t    dialValue        = 0;
    uint32_t   slot             = gSlot;
    uint32_t   variation        = gPatchDescr[slot].activeVariation;

    tModule *  module           = get_module((tModuleKey){slot, (uint32_t)locationMorph, 1});

    if (module != NULL) {
        // notes §77

        for (i = 0; i < NUM_MORPHS; i++) {
            isKnob     = !(module->param[variation][i + NUM_MORPHS].value != 0);
            dialValue  = module->param[variation][i].value;

            snprintf(dialValueStr, sizeof(dialValueStr), "%u", dialValue);

            if (isKnob) {
                snprintf(label, sizeof(label), "%s", module->paramName[i + NUM_MORPHS][0]);

                if (label[0] == '\0') {
                    snprintf(label, sizeof(label), "Knob");
                }
            } else {
                snprintf(label, sizeof(label), "%s", morph_source_name(i, module->param[variation][i + NUM_MORPHS].value));
            }
            textHeight = rectangle.size.h / 4.0;

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
            tRectangle dialRect = render_dial_with_text(mainArea, (tRectangle){{rectangle.coord.x, rectangle.coord.y + 16 + textHeight}, {rectangle.size.w, rectangle.size.w}}, NULL, dialValueStr, textHeight, module->param[variation][i].value, 128, module->param[variation][i].morphRange[gMorphGroupFocus], dialColour);

            register_click_region(dialRect, eClickLayerPanel, morph_param_click_handler,
                                  morph_click_ctx(module, i));

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
            register_click_region(gMorphLabelRect[i], eClickLayerPanel, morph_param_click_handler,
                                  morph_click_ctx(module, (uint32_t)(i + NUM_MORPHS)));

            rectangle.coord.x += (STANDARD_TEXT_HEIGHT * 4) + 5;
        }
    }
}

#ifdef __cplusplus
}
#endif

