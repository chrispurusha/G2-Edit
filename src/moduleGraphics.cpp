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

            value  = ((value & 0xff00) >> 8) | ((value & 0xff) << 8); // Swap bytes
            value &= 0x03ff;                                          // There's a value of 3 in the high nibble, which is unknown use. Might be an indication of this being individual bit per LED?

            for (int i = 0; i < leds; i++) {
                if ((value >> i) & 0x01) {
                    set_rgb_colour(RGB_GREEN_7);
                } else {
                    set_rgb_colour(RGB_GREEN_3);
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.y += smallRectangle.size.h + space;
            }
        }
        break;

        case volumeTypeMono:
        case volumeTypeStereo:
        {
            uint32_t top3Bits = 0;

            value    = ((value & 0xff00) >> 8) | ((value & 0xff) << 8); // Swap bytes
            top3Bits = (value >> 5) & 0x7;
            value   &= 0x1f;
            //LOG_DEBUG("Top 3 bits = %u val = %u\n", top3Bits, value); // Val of 10 or 11 = Yellow, 12 = red?, top bits 3 = clip?

            double fullHeight  = rectangle.size.h;
            double scaledValue = (rectangle.size.h * value) / 12.0; // 128 usually, but one example of 300!? Maybe the leading nibble denotes a type? val of 1 changes scale!?

            int    valueThresholds[] = {7, 11, 12};                 // Exclusive upper bounds for green/yellow/red
            tRgb   colours[]         = {RGB_GREEN_7, RGB_YELLOW_7, RGB_RED_7};

            set_rgb_colour(RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            double previousHeight = 0;

            for (int i = 0; i < 3; i++) {
                int    segmentTopVal    = valueThresholds[i];
                int    segmentBottomVal = (i == 0) ? 0 : valueThresholds[i - 1];
                int    segmentRange     = segmentTopVal - segmentBottomVal;

                double segmentHeight = (segmentRange * fullHeight) / 12.0;

                // Determine how much of this segment to draw
                double segmentDrawHeight = 0;

                if (value >= segmentBottomVal) {
                    int drawSteps = (value < segmentTopVal) ? value - segmentBottomVal : segmentRange;
                    segmentDrawHeight = (drawSteps * fullHeight) / 12.0;

                    set_rgb_colour(colours[i]);
                    render_rectangle(
                        moduleArea,
                        {{rectangle.coord.x,
                            rectangle.coord.y + fullHeight - previousHeight - segmentDrawHeight},
                            {rectangle.size.w,
                             segmentDrawHeight}});
                    previousHeight += segmentDrawHeight;
                }
            }
        }

        break;
        default:
            break;
    }
}

tRectangle render_dial(tArea area, tRectangle rectangle, uint32_t value, uint32_t range, uint32_t morphRange, tRgb colour) {  // Drop down into utilsGraphics?
    double  angle            = 0.0;
    double  morphAngle       = 0.0;
    double  radius           = 0.0;
    double  x                = 0;
    double  y                = 0;
    int32_t signedMorphRange = 0;
    int32_t signedValue      = 0;
    int32_t morphPos         = 0;

    radius = (rectangle.size.w / 2.0);
    x      = rectangle.coord.x + radius;
    y      = rectangle.coord.y + radius;
    angle  = value_to_angle(value, range);

    //debug
    //{
    //    char buff[256] = {0};
    //    set_rgb_colour(RGB_GREY_5);
    //    snprintf(buff, sizeof(buff), "%u", value);
    //    render_text(moduleArea, {{x, y + 20}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    //}

    if (morphRange == 0) {
        set_rgb_colour(colour);
    } else {
        set_rgb_colour(RGB_ORANGE_2);
    }
    render_circle_part_angle(area, {x, y}, radius, 0.0, 360.0, 25);

    if (morphRange != 0) {
        signedValue = (int32_t)value;

        if (morphRange < 127) {
            signedMorphRange = (int32_t)morphRange;
        } else {
            signedMorphRange = (int32_t)morphRange - 256;
        }
        morphPos = signedValue + signedMorphRange;

        if (morphPos < 0) {
            morphPos = 0;
        } else if (morphPos > (range - 1)) {
            morphPos = range - 1;
        }
        morphAngle = value_to_angle((uint32_t)morphPos, range);
        set_rgb_colour(RGB_ORANGE_1);

        if (morphAngle > angle) {
            render_circle_part_angle(area, {x, y}, radius, angle, morphAngle, 25);
        } else {
            render_circle_part_angle(area, {x, y}, radius, morphAngle, angle, 25);
        }
    }
    set_rgb_colour(RGB_BLACK);
    render_radial_line(area, {x, y}, radius, angle, 2.0);
    set_rgb_colour(RGB_BLACK);
    return render_circle_line(area, {x, y}, radius, 25, 1.0);
}

tRectangle render_dial_with_text(tArea area, tRectangle rectangle, char * label, char * buff, uint32_t value, uint32_t range, uint32_t morphRange, tRgb colour) {
    double textHeight = rectangle.size.h / 4.0;

    set_rgb_colour(RGB_BLACK);

    if (label != NULL) {
        render_text(area, {{rectangle.coord.x, rectangle.coord.y}, {BLANK_SIZE, textHeight}}, label);
    }

    if (buff != NULL) {
        render_text(area, {{rectangle.coord.x, rectangle.coord.y + textHeight}, {BLANK_SIZE, textHeight}}, buff);
    }
    //{
    //    char debug[64] = {0};
    //
    //    snprintf(debug, sizeof(debug), "VAL %u\n", value);
    //    render_text(moduleArea, {{rectangle.coord.x, rectangle.coord.y + textHeight}, {BLANK_SIZE, textHeight}}, debug);
    //}

    //set_rgb_colour(RGB_GREY_2);
    return render_dial(area, {{rectangle.coord.x, rectangle.coord.y + (textHeight * 2.0)}, {rectangle.size.w, rectangle.size.w}}, value, range, morphRange, colour);
}

// This might be too generic and won't be able to use, or we add extra params!
void render_param_common(tRectangle rectangle, tModule * module, uint32_t paramRef, uint32_t paramIndex) {
    char     buff[16]                   = {0};
    char     label[PARAM_NAME_SIZE + 1] = {0};
    uint32_t paramValue                 = module->param[gVariation][paramIndex].value;
    uint32_t morphRange                 = module->param[gVariation][paramIndex].morphRange[gMorphGroupFocus];

    if (paramValue >= paramLocationList[paramRef].range) {
        LOG_ERROR("Module index %u name %s ParamRef %u ParamIndex %u Value %u > Range %u\n", module->key.index, module->name, paramRef, paramIndex, paramValue, paramLocationList[paramRef].range);
        exit(1);
    }

    if (strlen(module->paramName[paramIndex]) > 0) {
        strncpy(label, module->paramName[paramIndex], sizeof(label));
    } else if (paramLocationList[paramRef].label != NULL) {
        strncpy(label, paramLocationList[paramRef].label, sizeof(label));
    }
    label[sizeof(label) - 1] = '\0';

    module->param[gVariation][paramIndex].paramRef = paramRef;

    switch (paramLocationList[paramRef].type1) {
            LOG_DEBUG("param %u", paramValue);
        case paramType1Freq:
        {
            double freq = 0.0;

            freq = round(13.75 * pow(2, (double)paramValue / 12.0) * 100.0) / 100.0;

            if (freq < 100) {
                snprintf(buff, sizeof(buff), "%.2fHz", freq);
            } else if (freq < 1000) {
                snprintf(buff, sizeof(buff), "%.1fHz", freq);
            } else if (freq < 10000) {
                snprintf(buff, sizeof(buff), "%.2fkHz", freq / 1000.0);
            } else {
                snprintf(buff, sizeof(buff), "%.1fkHz", freq / 1000.0);
            }
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1OscFreq:
        {
            // Frequency dial for oscillators. Uses PitchType param to control display of Tune
            int pitchTypeParamIndex = 0;
            
            switch(module->type) {
                case moduleTypeOscMaster:
                {
                    pitchTypeParamIndex = 3;
                    break;
                }
                case moduleTypeOscB:
                    pitchTypeParamIndex = 4;
                    break;
                default: {
                }
            }
            switch(module->param[gVariation][pitchTypeParamIndex].value) {
                case 0: { // Semi. -64 to 63
                    double res;

                    if (paramValue < 127) {
                        res = round((((double)paramValue-64.0) *10.0)) / 10.0;
                    } else {
                        res = 63.0;             // Clip
                    }
                    snprintf(buff, sizeof(buff), "%.1f", res);
                    break;
                }
                case 1: { // Freq. 8.1758 Hz to 12.55 kHz
                    double res;
                    double min_freq = 8.1758;
                    double max_freq = 12550.0;
                    res = exp((double)paramValue / 127.0 * log(max_freq / min_freq)) * min_freq;
                    if (res < 100) {
                        snprintf(buff, sizeof(buff), "%.2fHz", res);
                    } else if (res < 1000) {
                        snprintf(buff, sizeof(buff), "%.1fHz", res);
                    } else if (res < 10000) {
                        snprintf(buff, sizeof(buff), "%.2fkHz", res / 1000.0);
                    } else {
                        snprintf(buff, sizeof(buff), "%.1fkHz", res / 1000.0);
                    }
                    break;
                }
                case 2: { // Factor. 0->0.0248, 127 -> 38.072
                    double res;
                    double min_factor = .0248;
                    double max_factor = 38.072;
                    res = exp((double)paramValue / 127.0 * log(max_factor / min_factor)) * min_factor;
                    snprintf(buff, sizeof(buff), "%.4fHz", res);
                    break;
                }
                case 3: { // Partial. Displays partials for values from 33 upwards, Hz below.
                    double res;
                    if(paramValue == 0.0) {
                        snprintf(buff, sizeof(buff), "0 Hz");
                    } else if(paramValue < 33.0) { // show value as Hz
                        double min_freq = 0.005;
                        double max_freq = 5.153;
                        res = exp(((double)paramValue-1.0) / 31.0 * log(max_freq / min_freq)) * min_freq;
                        snprintf(buff, sizeof(buff), "%.3fHz", res);
                    } else if (paramValue < 64.0) {
                        res = 64.0 - paramValue + 1.0;
                        snprintf(buff, sizeof(buff), "1:%.0f", res);
                    } else {
                        res = paramValue - 64.0 + 1.0;
                        snprintf(buff, sizeof(buff), "%.0f:1", res);
                    }
                    break;
                }
            }
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1Fine:
        {
            double res = ((double)paramValue - 64.0)/64.0*50.0;
            snprintf(buff, sizeof(buff), "%.1f", res);
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1Shape: // 50.0->99.0
        {
            int val = 0;
            val = (int)(paramValue*49.0/127.0 + 50.0);
            snprintf(buff, sizeof(buff), "%u", val);
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }

        case paramType1FreqDrum:
        {
            double freq = 0.0;

            // 0 -> 20 Hz, 127 -> 784 Hz
            freq = round(20.0 * pow(2, (double)paramValue * 0.041675) * 100.0) / 100.0;

            if (freq < 100) {
                snprintf(buff, sizeof(buff), "%.2fHz", freq);
            } else {
                snprintf(buff, sizeof(buff), "%.1fHz", freq);
            }
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1LFORate:
        {
            double rate;
            int rateModeParamIndex;
            switch(module->type) {
                case moduleTypeLfoShpA:
                {
                    rateModeParamIndex = 1;
                    break;
                }
                case moduleTypeLfoC:
                {
                    rateModeParamIndex = 3;
                    break;
                }
                case moduleTypeLfoA:
                {
                    rateModeParamIndex = 7;
                    break;
                }
                default:
                {
                }
            }
            switch(module->param[gVariation][rateModeParamIndex].value) {
                case 0: // Sub - compute range in s
                {
                    double range_start = 699.0;
                    double range_end = 5.46;
                    rate = range_start * exp((double)paramValue * log(range_end/range_start)/127.0);
                    if (rate > 100.0) {
                        snprintf(buff, sizeof(buff), "%.0fs", rate);
                    } else if (rate > 10.0) {
                        snprintf(buff, sizeof(buff), "%.1fs", rate);
                    } else {
                        snprintf(buff, sizeof(buff), "%.2fs", rate);
                    }
                    break;
                }

                case 1: // Rate Lo -- compute rate in s
                {
                    double range_start = 62.9;
                    double range_end = 0.04098; // 24.4 Hz
                    rate = range_start * exp((double)paramValue * log(range_end/range_start)/127.0);
                    if (rate > 10.0) {
                        snprintf(buff, sizeof(buff), "%.1fs", rate);
                    } else if (rate > 0.1) {
                        snprintf(buff, sizeof(buff), "%.2fHz", 1.0/rate);
                    } else {
                        snprintf(buff, sizeof(buff), "%.1fHz", 1.0/rate);
                    }
                    break;
                }
                case 2: // Rate Hi - compute rate in Hz
                {
                    double min_freq=0.26;
                    double max_freq=392.0;
                    double freq = min_freq * exp((double) paramValue * log(max_freq/min_freq)/127.0);
                    if (freq < 10.0) {
                        snprintf(buff, sizeof(buff), "%.2fHz", freq);
                    } else if (freq < 100.0) {
                        snprintf(buff, sizeof(buff), "%.1fHz", freq);
                    } else {
                        snprintf(buff, sizeof(buff), "%.0fHz", freq);
                    }
                    break;
                }
                case 3: // BPM
                {
                    int bpm;
                    if (paramValue < 33) { // 0->24, 32->88 in steps of 2
                        bpm = 24 + round(2 * paramValue);
                    } else if (paramValue < 97) { // 33 -> 89, 96 -> 152 in steps of 1
                        bpm = 56 + round(paramValue);
                    } else { // 97 -> 154, 127 -> 214 in steps of 2
                        bpm = 154 + round(2 * (paramValue - 97.0));
                    }
                    snprintf(buff, sizeof(buff), "%u", bpm);
                    break;
                case 4: // ClkSync. 32 values
                    const char * clkSyncStrMap[] = {
                        "64/1", "48/1", "32/1", "24/1",  "16/1", "12/1", "8/1", "6/1",   "4/1", "3/1", "2/1", "1/1D",
                        "1/1", "1/2D", "1/1T", "1/2",    "1/4D", "1/2T", "1/4", "1/8D",  "1/4T", "1/8", "1/16D", "1/8T",
                        "1/16", "1/32D", "1/16T", "1/32",  "1/64D", "1/32T", "1/64", "1/64T"
                    };
                    int posClkSyncStrMap = floor(paramValue/4);
                    snprintf(buff, sizeof(buff), "%s\n", clkSyncStrMap[posClkSyncStrMap]);
                    break;
                }
                default:
                {
                    LOG_ERROR("Wrong case %u in paramTypeLFORate\n", module->param[gVariation][rateModeParamIndex].value);
                }
            }
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }

        case paramType1Int:
        {
            int val = 0;
            val = paramValue;
            snprintf(buff, sizeof(buff), "%u", val);
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1dB:
        {
            double dB       = 0.0;
            double dB_range = 1.0;

            switch (module->type) {
                case moduleTypeEq2Band:
                case moduleTypeEq3band:
                {
                    dB_range = 18.0;
                    break;
                }
                case moduleTypeLevScaler:
                {
                    dB_range = 8.0;
                    break;
                }
                default:
                {
                }
            }
            dB = round(((double)paramValue - 64.0) / 64.0 * dB_range);
            snprintf(buff, sizeof(buff), "%+.0fdB", dB);

            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1Time:
        {
            double time = 0.0;
            double min_time, max_time;

            switch (module->type) {
                case moduleTypeGlide:
                {
                    min_time = 0.002;
                    max_time = 22.4;
                    break;
                }
                case moduleTypeDlySingleA:
                case moduleTypeDlySingleB:
                case moduleTypeDelayDual:
                case moduleTypeDlyEight:
                case moduleTypeDlyStereo:
                {
                    min_time = 0.001;

                    switch (module->mode[0].value) {
                        case 0:
                        {
                            max_time = 0.005;
                            break;
                        }
                        case 1:
                        {
                            max_time = 0.025;
                            break;
                        }
                        case 2:
                        {
                            max_time = 0.100;
                            break;
                        }
                        case 3:
                        {
                            max_time = 0.500;
                            break;
                        }
                        case 4:
                        {
                            max_time = 1.0;
                            break;
                        }
                        case 5:
                        {
                            max_time = 2.0;
                            break;
                        }
                        case 6:
                        {
                            max_time = 2.7;
                            break;
                        }
                    }
                    break;
                }
                default:
                {
                }
            }
            // scale 0 -> min_time and 127 -> max_time, exponentially
            time = exp((double)paramValue / 127 * log(max_time / min_time)) * min_time;

            if (time < 1.0) {
                snprintf(buff, sizeof(buff), "%.0fms", time * 1000);
            } else {
                snprintf(buff, sizeof(buff), "%.1fs", time);
            }
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1TimeClk:
        {
            double time = 0.0;
            double min_time, max_time;

            switch (module->type) {
                case moduleTypeDelayQuad:
                case moduleTypeDelayA:
                case moduleTypeDelayB:
                {
                    min_time = 0.001;

                    switch (module->mode[0].value) {
                        case 0:
                        {
                            max_time = 0.005;
                            break;
                        }
                        case 1:
                        {
                            max_time = 0.025;
                            break;
                        }
                        case 2:
                        {
                            max_time = 0.100;
                            break;
                        }
                        case 3:
                        {
                            max_time = 0.500;
                            break;
                        }
                        case 4:
                        {
                            max_time = 1.0;
                            break;
                        }
                        case 5:
                        {
                            max_time = 2.0;
                            break;
                        }
                        case 6:
                        {
                            max_time = 2.7;
                            break;
                        }
                    }
                    break;
                }
                default:
                {
                }
            }
            // scale 0 -> min_time and 127 -> max_time, exponentially
            time = exp((double)paramValue / 127 * log(max_time / min_time)) * min_time;

            if (time < 1.0) {
                snprintf(buff, sizeof(buff), "%.0fms", time * 1000);
            } else {
                snprintf(buff, sizeof(buff), "%.1fs", time);
            }
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1ADRTime:
        {
            double time = 0.0;
            const double ADRTimeMap[] = {0.0005, 0.0006, 0.0007, 0.0009,  0.0011, 0.0013, 0.0015, 0.0018,  0.0021, 0.0025, 0.0030, 0.0035,  0.0040, 0.0047, 0.0055, 0.0063,
                                         0.0073, 0.0084, 0.0097, 0.0111,  0.0127, 0.0145, 0.0165, 0.0187,  0.0212, 0.0240, 0.0271, 0.0306,  0.0344, 0.0387, 0.0434, 0.0486,
                                         0.0543, 0.0606, 0.0676, 0.0752,  0.0836, 0.0928, 0.103,  0.114,   0.126,  0.139,  0.153,  0.169,   0.186,  0.204,  0.224,  0.246,
                                         0.269,  0.295,  0.322,  0.352,   0.384,  0.419,  0.456,  0.496,   0.540,  0.586,  0.636,  0.690,   0.748,  0.810,  0.876,  0.947,
                                         1.02, 1.10, 1.19, 1.28,  1.38, 1.49, 1.60, 1.72,  1.85, 1.99, 2.13, 2.28,  2.45, 2.62, 2.81, 3.00,
                                         3.21, 3.43, 3.66, 2.91,  4.17, 4.45, 4.74, 5.05,  5.37, 5.72, 6.08, 6.47,  6.87, 7.30, 7.75, 8.22,
                                         8.72, 9.25, 9.80, 10.4,  11.0, 11.6, 12.3, 13.0,  13.8, 14.6, 15.4, 16.2,  17.1, 18.1, 19.1, 20.1,
                                         21.2, 22.4, 23.5, 24.8,  26.1, 27.5, 28.9, 30.4,  32.0, 33.6, 35.3, 37.1,  38.9, 40.9, 42.9, 45.0
            };
            // use table
            time = ADRTimeMap[paramValue];

            if (time < 0.1) {
                snprintf(buff, sizeof(buff), "%.1fms", time * 1000);
            } else if (time < 1.0) {
                snprintf(buff, sizeof(buff), "%.0fms", time * 1000);
            } else if (time < 10.0) {
                snprintf(buff, sizeof(buff), "%.2fs", time);
            }
            else {
                snprintf(buff, sizeof(buff), "%.1fs", time);
            }
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1Pitch:
        {
            double percent = 0.0;
            double maxVal  = 200.0;

            if (paramValue < 127) {
                percent = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
            } else {
                percent = maxVal;             // Clip
            }
            snprintf(buff, sizeof(buff), "%.1f%%", percent);
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1BipLevel: // -64 to 63
        {
            double res    = 0.0;
            double maxVal = 64.0;

            if (paramValue < 127) {
                res = round((((double)paramValue - 64.0) * maxVal * 10.0) / 64.0) / 10.0;
            } else {
                res = maxVal;             // Clip
            }
            snprintf(buff, sizeof(buff), "%.1f", res);
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1LevAmpDial: // 0.25 to 4.0
        {
            double lev = 1.0;
            // scale 0 -> 0.25x and 127 -> 4.0x, exponentially
            lev = round(exp((double)paramValue * 0.0218) * 0.25 * 100.0) / 100.0;

            snprintf(buff, sizeof(buff), "%.2fx", lev);

            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1Pan:
        {
            module->param[gVariation][paramIndex].rectangle = render_dial(moduleArea, rectangle, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1NoteDial: // C-1 to G9
        {
            int    noteoctave;
            int    noteval;
            char * noteNameStrMap[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            char * noteName;

            noteoctave = paramValue / 12 - 1;
            noteval    = paramValue % 12;
            noteName   = noteNameStrMap[noteval];

            snprintf(buff, sizeof(buff), "%s%i", noteName, noteoctave);

            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1CommonDial: // Ultimately might not be a common dial, or could just be a default percent dial!?
        case paramType1LRDial:     // Pan type dial, perhaps with reset triangle
        case paramType1Resonance:
        {
            double res    = 0.0;
            double maxVal = 100.0;

            if (paramValue < 127) {
                res = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
            } else {
                res = maxVal;             // Clip
            }
            snprintf(buff, sizeof(buff), "%.1f", res);
            module->param[gVariation][paramIndex].rectangle = render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, RGB_GREY_5);
            break;
        }
        case paramType1StandardToggle:
        {
            char ** strMap     = (char **)paramLocationList[paramRef].strMap;
            double  y          = rectangle.coord.y;
            double  textHeight = rectangle.size.h / 2.0;

            if (strMap == NULL) {
                LOG_ERROR("No strMap for module type %s\n", gModuleProperties[module->type].name);

                //Debug help for value
                char debug[64] = {0};
                snprintf(debug, sizeof(debug), "%u", paramValue);
                set_rgb_colour(RGB_BACKGROUND_GREY);
                module->param[gVariation][paramIndex].rectangle = draw_button(moduleArea, {{rectangle.coord.x, y}, {30, textHeight}}, debug);
                return;
            }

            if (strlen(label) > 0) {
                set_rgb_colour(RGB_BLACK);
                render_text(moduleArea, {{rectangle.coord.x, y}, {BLANK_SIZE, textHeight}}, label);
                y += textHeight;
            }

            if (paramLocationList[paramRef].colourMap != NULL) {
                set_rgb_colour(paramLocationList[paramRef].colourMap[paramValue]);
            } else {
                set_rgb_colour(RGB_BACKGROUND_GREY);
            }
            module->param[gVariation][paramIndex].rectangle = draw_button(moduleArea, {{rectangle.coord.x, y}, {largest_text_width(paramLocationList[paramRef].range, strMap, textHeight), textHeight}}, strMap[paramValue]);
            break;
        }
        case paramType1Bypass:
        {
            module->param[gVariation][paramIndex].rectangle = draw_power_button(moduleArea, rectangle, paramValue != 0);
            return;
        }
        case paramType1Enable:
        {
            //module->param[gVariation][paramIndex].rectangle = draw_power_button(moduleArea, rectangle, paramValue != 0);

            if (paramLocationList[paramRef].colourMap != NULL) {
                set_rgb_colour(paramLocationList[paramRef].colourMap[paramValue]);
            } else {
                set_rgb_colour(RGB_BACKGROUND_GREY);          // Grey when OFF
            }
            module->param[gVariation][paramIndex].rectangle = draw_button(moduleArea, rectangle, NULL);  // TODO: Add label!
            return;
        }
        default:
        {
        }
        break;
    }
    //{
    //    char debug[64] = {0};
    //
    //    snprintf(debug, sizeof(debug), "%u %u\n", module->param[gVariation][paramIndex].range[0], module->param[gVariation][paramIndex].range[1]);
    //    render_text(moduleArea, {{rectangle.coord.x, rectangle.coord.y + 40}, {BLANK_SIZE, 12}}, debug);
    //}
}

void render_mode_common(tRectangle rectangle, tModule * module, uint32_t modeRef, uint32_t modeIndex) {
    char     buff[16]                   = {0};
    char     label[PARAM_NAME_SIZE + 1] = {0};
    uint32_t modeValue                  = module->mode[modeIndex].value;


    module->mode[0].modeRef = modeRef;

    switch (modeLocationList[modeRef].type1) {
        case paramType1OscWave:
        {
            char buff[16] = {0};

            snprintf(buff, sizeof(buff), "%u", module->mode[0].value);
            module->mode[modeIndex].rectangle = render_dial_with_text(moduleArea, rectangle, (char *)modeLocationList[modeRef].label, buff, module->mode[0].value, modeLocationList[modeRef].range, 0, RGB_GREY_5);  // TODO: Check if Mode can be morphed
            break;
        }
        case paramType1StandardToggle:
        {
            char ** strMap     = (char **)modeLocationList[modeRef].strMap;
            double  y          = rectangle.coord.y;
            double  textHeight = rectangle.size.h / 2.0;

            if (strMap == NULL) {
                LOG_ERROR("No strMap for module type %s\n", gModuleProperties[module->type].name);

                //Debug help for value
                char debug[64] = {0};
                snprintf(debug, sizeof(debug), "modeRef %u", modeRef);
                set_rgb_colour(RGB_BACKGROUND_GREY);
                module->param[gVariation][modeIndex].rectangle = draw_button(moduleArea, {{rectangle.coord.x, y}, {30, textHeight}}, debug);
                return;
            }
            //if (paramLocationList[paramRef].colourMap != NULL) {
            //    set_rgb_colour(paramLocationList[paramRef].colourMap[paramValue]);
            //} else {
            //    set_rgb_colour(RGB_BACKGROUND_GREY);
            //}
            set_rgb_colour(RGB_BACKGROUND_GREY);
            
            module->mode[modeIndex].rectangle = draw_button(moduleArea, {{rectangle.coord.x, y}, {largest_text_width(modeLocationList[modeRef].range, strMap, textHeight), textHeight}}, strMap[modeValue]);
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
            // Debug
            //snprintf(buff, sizeof(buff), "Vol %u", module->volume.value1);
            //set_rgb_colour(RGB_BLACK);
            //render_text(moduleArea, {{coord.x + x_param_size_from_percent(5), coord.y}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value1);
        }
        break;
        case volumeTypeStereo:
        {
            double space = 2.0;                                                                              // TODO: Possibly make a percentage of width

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value1); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value2);
        }
        break;
        case volumeTypeCompress:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value1);
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
            if (module->led.value != 0) {
                set_rgb_colour(RGB_GREEN_7);
            } else{
                set_rgb_colour(RGB_BLACK);
            }
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
                textRectangle.coord.x -= (get_text_width((char *)connectorLocationList[connectorListIndex].label, textRectangle.size.h) + 2);
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
    char       buff[MODULE_NAME_SIZE + 1] = {0};
    tRgb       rgb                        = {0};

    tRectangle moduleRectangle = {{xPos, yPos}, {xWidth, yHeight}};

    rgb = gModuleColourMap[module->colour];
    set_rgb_colour(rgb);
    module->rectangle = render_rectangle_with_border(moduleArea, moduleRectangle);

    rgb = {rgb.red * 1.05, rgb.green * 1.05, rgb.blue * 1.05};
    set_rgb_colour(rgb);
    module->dragArea = render_rectangle(moduleArea, {{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3}, {moduleRectangle.size.w - 6, STANDARD_TEXT_HEIGHT + 2}});

    render_module_common(moduleRectangle, module);
    write_module(module->key, module);                                             // Save calculated coords

    snprintf(buff, sizeof(buff), "%s", module->name);
    set_rgba_colour(RGBA_BLACK_ON_TRANSPARENT);
    render_text(moduleArea, {{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

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
    tModule module      = {0};
    bool    validModule = false;

    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if (validModule && module.key.slot == gSlot && module.key.location == gLocation && module.type != moduleTypeUnknown0) {
            render_module(&module);
        }
    } while (validModule);

    finish_walk_module();

    // Draw background areas
    //set_rgb_colour(RGB_RED_7/*RGB_BACKGROUND_GREY*/);
    //tRectangle area        = module_area();
    //render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
    //render_rectangle(mainArea, {{area.coord.x + area.size.w, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, {{0.0, area.coord.y + area.size.h}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
}

void render_cable_from_to(tConnector from, tConnector to) {
    tCoord control = {0};

    from.coord.x += scale_from_percent(CONNECTOR_SIZE / 2.0);
    from.coord.y += scale_from_percent(CONNECTOR_SIZE / 2.0);
    to.coord.x   += scale_from_percent(CONNECTOR_SIZE / 2.0);
    to.coord.y   += scale_from_percent(CONNECTOR_SIZE / 2.0);

    if (from.coord.x == to.coord.x) {
        control.x = from.coord.x;
        control.y = fmin(from.coord.y, to.coord.y);
    } else {
        control.x = ((from.coord.x + to.coord.x) / 2.0);
        control.y = fmax(from.coord.y, to.coord.y) + 40.0;
    }
    render_bezier_curve(moduleArea, from.coord, control, to.coord, 4.0, 15);
}

void render_cable(tCable * cable) {
    tModule moduleFrom = {0};
    tModule moduleTo   = {0};

    if (read_module({cable->key.slot, cable->key.location, cable->key.moduleFromIndex}, &moduleFrom) == false) {
        return;
    }

    if (read_module({cable->key.slot, cable->key.location, cable->key.moduleToIndex}, &moduleTo) == false) {
        return;
    }
    set_rgb_colour(cableColourMap[cable->colour]);

    int fromConnectorIndex = find_index_from_io_count(&moduleFrom, (tConnectorDir)cable->key.linkType, cable->key.connectorFromIoCount);

    int toConnectorIndex = find_index_from_io_count(&moduleTo, connectorDirIn, cable->key.connectorToIoCount);

    if (fromConnectorIndex != -1 && toConnectorIndex != -1) {
        render_cable_from_to(moduleFrom.connector[fromConnectorIndex], moduleTo.connector[toConnectorIndex]);
    }
}

void render_cables(void) {
    tCable cable      = {0};
    bool   validCable = false;

    reset_walk_cable();

    do {
        validCable = walk_next_cable(&cable);

        if (validCable && cable.key.slot == gSlot && cable.key.location == gLocation) {
            render_cable(&cable);
        }
    } while (validCable);

    finish_walk_cable();
}

void render_morph_groups(void) {
    tModule    module      = {0};
    tRectangle rectangle   = {{((get_render_width() / 2) - SCROLLBAR_MARGIN) - (((STANDARD_TEXT_HEIGHT * 4) + 5) * 8), 26}, {STANDARD_TEXT_HEIGHT *2, STANDARD_TEXT_HEIGHT * 4}};
    char       buff[16]    = {0};
    char       label[16]   = {0};
    tRgb       dialColour  = RGB_BACKGROUND_GREY;
    uint32_t   i           = 0;
    uint32_t   j           = 0;
    double     textHeight  = 0.0;
    bool       validModule = false;

    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if ((validModule && module.key.slot == gSlot && module.key.location == locationMorph) && (module.key.index == 1)) {
            // Make sure all rectangles (for mouse click) are nullified
            for (i = 0; i < NUM_VARIATIONS; i++) {
                for (j = 0; j < (NUM_MORPHS * 2); j++) {
                    module.param[i][j].rectangle = NULL_RECTANGLE;
                }
            }

            for (i = 0; i < NUM_MORPHS; i++) {
                snprintf(buff, sizeof(buff), "%u", module.param[gVariation][i].value);

                if (module.param[gVariation][i + NUM_MORPHS].value != 0) {
                    snprintf(label, sizeof(label), "%s", module.paramName[i + NUM_MORPHS]);
                } else {
                    snprintf(label, sizeof(label), "Knob");
                }

                if (i == gMorphGroupFocus) {
                    dialColour = RGB_ORANGE_2;
                } else {
                    dialColour = RGB_GREY_3;
                }
                module.param[gVariation][i].rectangle = render_dial_with_text(mainArea, rectangle, NULL, buff, module.param[gVariation][i].value, 128, module.param[gVariation][i].morphRange[gMorphGroupFocus], dialColour);

                textHeight = rectangle.size.h / 4.0;

                set_rgb_colour(RGB_BACKGROUND_GREY);
                module.param[gVariation][i + NUM_MORPHS].rectangle = draw_button(mainArea, {{rectangle.coord.x - 5, rectangle.coord.y - 8}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, label);

                rectangle.coord.x += (STANDARD_TEXT_HEIGHT * 4) + 5;
            }

            write_module(module.key, &module);
        }
    } while (validModule);

    finish_walk_module();
}

#ifdef __cplusplus
}
#endif
