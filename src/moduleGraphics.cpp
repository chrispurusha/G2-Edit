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

// System header files
#include <math.h>

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"
#include "moduleGraphics.h"
#include "globalVars.h"
#include "renderParams.h"
#include "mouseHandle.h"
#include "menus.h"
#include "selection.h"

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
    {
        int32_t knobIdx = find_knob_for_param(module->key.slot, module->key.location,
                                              module->key.index, paramIndex);

        if (knobIdx >= 0) {
            tCoord mouseCoord = {0};

            get_global_gui_scaled_mouse_coord(&mouseCoord);

            if (within_rectangle(mouseCoord, gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex])) {
                char       knobLabel[32] = {0};
                int        page          = knobIdx / 24;
                int        bank          = (knobIdx % 24) / 8;
                int        pos           = knobIdx % 8;
                double     labelWidth    = get_text_width("X X X", (double)STANDARD_BUTTON_TEXT_HEIGHT * 0.8, eCache);
                tRectangle labelRect     = {{rectangle.coord.x + (rectangle.size.w - labelWidth) / 2.0,
                    rectangle.coord.y + rectangle.size.h + 2.0},
                    {labelWidth,                                               (double)STANDARD_TEXT_HEIGHT * 0.8}};

                snprintf(knobLabel, sizeof(knobLabel), "%c %d %d", 'A' + page, bank + 1, pos + 1);
                draw_button(moduleArea, labelRect, knobLabel, RGB_GREY_9);
            }
        }
    }
}

void render_mode_common(tRectangle rectangle, tModule * module, uint32_t modeRef, uint32_t modeIndex) {
    uint32_t modeValue = module->mode[modeIndex].value;
    uint32_t slot      = gSlot;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    module->mode[0].modeRef = modeRef;

    switch (modeLocationList[modeRef].type) {
        case paramTypeOscWave:
        {
            char buff[16] = {0};

            snprintf(buff, sizeof(buff), "%u", module->mode[0].value);
            module->mode[modeIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)modeLocationList[modeRef].label, buff, rectangle.size.h / 4.0, module->mode[0].value, modeLocationList[modeRef].range, 0, RGB_GREY_5);  // TODO: Check if Mode can be morphed
            break;
        }
        case paramTypeToggle:
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

            module->mode[modeIndex].rectangle = draw_button(moduleArea, {{rectangle.coord.x, y}, {largest_text_width(modeLocationList[modeRef].range, strMap, textHeight, eCache), textHeight}}, strMap[modeValue], RGB_BACKGROUND_GREY);
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

    if (module->upRate) { // TODO: should only apply to connectors which carry audio
        type = connectorTypeAudio;
    }
    set_rgb_colour(connectorColourMap[type]);  // Note, was using "module->connector[connectorIndex].type", check that this type param is OK

    if (module->connector[connectorIndex].dir == connectorDirIn) {
        module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, {rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 2.0, 10.0, 0.0, 10.0);
    } else {
        module->connector[connectorIndex].rectangle = render_rectangle(moduleArea, {rectangle.coord, {rectangle.size.w, rectangle.size.h}});
    }
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

void render_module_common(tRectangle rectangle, tModule * module) {
    if (module == NULL) {
        return;
    }
    uint32_t param     = 0;
    uint32_t mode      = 0;
    uint32_t connector = 0;
    uint32_t volume    = 0;
    uint32_t led       = 0;

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

    rgb               = gModuleColourMap[module->colour];
    set_rgb_colour(rgb);
    module->rectangle = render_rectangle_with_border(moduleArea, moduleRectangle);

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
    rgb               = {rgb.red * 1.05, rgb.green * 1.05, rgb.blue * 1.05};
    set_rgb_colour(rgb);
    module->dragArea  = render_rectangle(moduleArea, {{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3}, {moduleRectangle.size.w - 6, STANDARD_TEXT_HEIGHT + 2}});

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

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && module->type != moduleTypeUnknown0) {
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
    tRectangle rectangle        = {{840, 4}, {STANDARD_TEXT_HEIGHT *2, STANDARD_TEXT_HEIGHT * 4}};
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

            rectangle.coord.x                                                                         += (STANDARD_TEXT_HEIGHT * 4) + 5;
        }
    }
}

#ifdef __cplusplus
}
#endif

