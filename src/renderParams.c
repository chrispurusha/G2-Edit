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

//  Created by Øyvind Jacobsen Bjørkås on 25/06/2025.

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

#include "defs.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"
#include "moduleGraphics.h"
#include "globalVars.h"
#include "moduleGraphics.h"

tRectangle render_paramType1Freq(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
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
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1OscFreq(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // Frequency dial for oscillators. Uses PitchType param to control display of Tune
    int pitchTypeParamIndex = 0;
    
    switch (module->type) {
        case moduleTypeOscMaster:
        {
            pitchTypeParamIndex = 3;
            break;
        }
        case moduleTypeOscB:
        case moduleTypeResonator:
        {
            pitchTypeParamIndex = 4;
            break;
        }
        default:
        {
            LOG_ERROR("paramType1OscFreq missing module->type implementation");
        }
    }
    
    switch (module->param[gVariation][pitchTypeParamIndex].value) {
        case 0:   // Semi. -64 to 63
        {
            double res;
            
            if (paramValue < 127) {
                res = round((((double)paramValue - 64.0) * 10.0)) / 10.0;
            } else {
                res = 63.0;             // Clip
            }
            snprintf(buff, sizeof(buff), "%.1f", res);
            break;
        }
        case 1:   // Freq. 8.1758 Hz to 12.55 kHz
        {
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
        case 2:   // Factor. 0->0.0248, 127 -> 38.072
        {
            double res;
            double min_factor = .0248;
            double max_factor = 38.072;
            res = exp((double)paramValue / 127.0 * log(max_factor / min_factor)) * min_factor;
            snprintf(buff, sizeof(buff), "%.4fHz", res);
            break;
        }
        case 3:   // Partial. Displays partials for values from 33 upwards, Hz below.
        {
            double res;
            
            if (paramValue == 0.0) {
                snprintf(buff, sizeof(buff), "0 Hz");
            } else if (paramValue < 33.0) { // show value as Hz
                double min_freq = 0.005;
                double max_freq = 5.153;
                res = exp(((double)paramValue - 1.0) / 31.0 * log(max_freq / min_freq)) * min_freq;
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
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Fine(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double res = ((double)paramValue - 64.0) / 64.0 * 50.0;
    snprintf(buff, sizeof(buff), "%.1f", res);
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1GeneralFreq(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double minFreq, maxFreq, freq;
    
    switch (module->type) {
        case moduleTypeEq3band:
        {
            minFreq = 100.0;
            maxFreq = 8000.0;
            break;
        }
        default:
        {
            minFreq = 1.0;
            maxFreq = 1.0;
            LOG_ERROR("paramType1GeneralFreq missing module->type implementation, %u", module->type);
        }
    }
    freq = minFreq * exp((double)paramValue * log(maxFreq / minFreq) / 127.0);
    
    if (freq < 1000.0) {
        snprintf(buff, sizeof(buff), "%.0fHz", freq);
    } else {
        snprintf(buff, sizeof(buff), "%.2fkHz", freq / 1000.0);
    }
    return  render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Shape(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 50.0->99.9
    int val = 0;
    val = (int)(paramValue * 49.0 / 127.0 + 50.0);
    snprintf(buff, sizeof(buff), "%u", val);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1FreqDrum(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double freq = 0.0;
    
    // 0 -> 20 Hz, 127 -> 784 Hz
    freq = round(20.0 * pow(2, (double)paramValue * 0.041675) * 100.0) / 100.0;
    
    if (freq < 100) {
        snprintf(buff, sizeof(buff), "%.2fHz", freq);
    } else {
        snprintf(buff, sizeof(buff), "%.1fHz", freq);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
    
tRectangle render_paramType1LFORate(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double rate;
    int    rateModeParamIndex;
    
    switch (module->type) {
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
            rateModeParamIndex = NULL;
            LOG_ERROR("paramType1LFORate missing module->type implementation");
        }
    }
    switch (module->param[gVariation][rateModeParamIndex].value) {
        case 0: // Sub - compute range in s
        {
            double range_start = 699.0;
            double range_end   = 5.46;
            rate = range_start * exp((double)paramValue * log(range_end / range_start) / 127.0);
            
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
            double range_end   = 0.04098; // 24.4 Hz
            rate = range_start * exp((double)paramValue * log(range_end / range_start) / 127.0);
            
            if (rate > 10.0) {
                snprintf(buff, sizeof(buff), "%.1fs", rate);
            } else if (rate > 0.1) {
                snprintf(buff, sizeof(buff), "%.2fHz", 1.0 / rate);
            } else {
                snprintf(buff, sizeof(buff), "%.1fHz", 1.0 / rate);
            }
            break;
        }
        case 2: // Rate Hi - compute rate in Hz
        {
            double min_freq = 0.26;
            double max_freq = 392.0;
            double freq     = min_freq * exp((double)paramValue * log(max_freq / min_freq) / 127.0);
            
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
            
            if (paramValue < 33) {        // 0->24, 32->88 in steps of 2
                bpm = 24 + round(2 * paramValue);
            } else if (paramValue < 97) { // 33 -> 89, 96 -> 152 in steps of 1
                bpm = 56 + round(paramValue);
            } else {                      // 97 -> 154, 127 -> 214 in steps of 2
                bpm = 154 + round(2 * (paramValue - 97.0));
            }
            snprintf(buff, sizeof(buff), "%u", bpm);
            break;
        case 4: // ClkSync. 32 values
            const char * clkSyncStrMap[] = {"64/1", "48/1",  "32/1",  "24/1", "16/1",  "12/1",  "8/1",  "6/1",  "4/1",  "3/1", "2/1",   "1/1D",
                "1/1",  "1/2D",  "1/1T",  "1/2",  "1/4D",  "1/2T",  "1/4",  "1/8D", "1/4T", "1/8", "1/16D", "1/8T",
                "1/16", "1/32D", "1/16T", "1/32", "1/64D", "1/32T", "1/64", "1/64T"};
            int          posClkSyncStrMap = floor(paramValue / 4);
            snprintf(buff, sizeof(buff), "%s\n", clkSyncStrMap[posClkSyncStrMap]);
            break;
        }
        default:
        {
            LOG_ERROR("Wrong case %u in paramTypeLFORate\n", module->param[gVariation][rateModeParamIndex].value);
        }
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1Int(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int val = 0;
    val = paramValue;
    snprintf(buff, sizeof(buff), "%u", val);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1dB(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
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
            dB_range = 0.0;
            LOG_ERROR("paramType1dB missing module->type implementation");
        }
    }
    dB = round(((double)paramValue - 64.0) / 64.0 * dB_range);
    snprintf(buff, sizeof(buff), "%+.0fdB", dB);
    
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1MixLevel(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double       level      = 0.0;
    const double dbLvlMap[] = {-100.0, -99.9, -99.0, -72.1, -69.2, -66.9, -64.8, -62.9,
        -61.1,    -59,  -57.9, -56.4, -55.0, -53.6, -52.3,-51.0,
        -49.8,  -48.6, -47.5, -46.4, -45.3, -44.3, -43.3, -42.3,
        -41.3,  -40.4, -39.5, -38.7, -37.8, -37.0, -36.2, -35.5,
        -34.7,  -34.0, -33.3, -32.6, -31.9, -31.2, -30.6, -30.0,
        -29.3,  -28.7, -28.2, -27.6, -27.0, -26.5, -25.9, -25.4,
        -24.8,  -24.3, -23.8, -23.3, -22.8, -22.4, -21.9, -21.4,
        -21.0,  -20.5, -20.1, -19.7, -19.2, -18.8, -18.4, -18.0,
        -17.6,  -17.2, -16.8, -16.4, -16.1, -15.7, -15.3, -15.0,
        -14.6,  -14.3, -13.9, -13.6, -13.2, -12.9, -12.6, -12.2,
        -11.9,  -11.6, -11.3, -11.0, -10.7, -10.4, -10.1,  -9.8,
        -9.5,    -9.2,  -8.9,  -8.6,  -8.3,  -8.0,  -7.8,  -7.5,
        -7.2,    -7.0,  -6.7,  -6.4,  -6.2,  -5.9,  -5.7,  -5.4,
        -5.2,    -4.9,  -4.7,  -4.4,  -4.2,  -4.0,  -3.7,  -3.5,
        -3.3,    -3.0,  -2.8,  -2.6,  -2.3,  -2.1,  -1.9,  -1.7,
        -1.5,    -1.3,  -1.0,  -0.8,  -0.6,  -0.4,  -0.2, 0.0};
    int          expLinDBparam;
    
    switch (module->type) {
        case moduleTypeMix8to1B:
        {
            expLinDBparam = 8;
            break;
        }
    }
    level = paramValue;
    
    if (module->param[gVariation][expLinDBparam].value == 2) { // display dB
        if (paramValue == 0.0) {
            snprintf(buff, sizeof(buff), "-oodB");
        } else {
            snprintf(buff, sizeof(buff), "%.1f", dbLvlMap[(int)paramValue]);
        }
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
    
    
tRectangle render_paramType1Time(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
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
            max_time = 0.0;
            LOG_ERROR("paramType1Time missing module->type implementation");
        }
    }
    // scale 0 -> min_time and 127 -> max_time, exponentially
    time = exp((double)paramValue / 127 * log(max_time / min_time)) * min_time;
    
    if (time < 1.0) {
        snprintf(buff, sizeof(buff), "%.0fms", time * 1000);
    } else {
        snprintf(buff, sizeof(buff), "%.1fs", time);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1TimeClk(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double time = 0.0;
    double min_time, max_time;
    
    switch (module->type) {
        case moduleTypeDelayQuad:
        case moduleTypeDelayA:
        case moduleTypeDelayB:
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
                default:
                {
                    max_time = 0.0;
                    LOG_ERROR("paramType1TimeClk module[0]->value wrong value, %u", module->type);
                }
            }
            break;
        }
        default:
        {
            min_time = 0.0;
            max_time = 0.0;
            LOG_ERROR("paramType1TimeClk missing module->type implementation, %u", module->type);
        }
    }
    // scale 0 -> min_time and 127 -> max_time, exponentially
    time = exp((double)paramValue / 127 * log(max_time / min_time)) * min_time;
    
    if (time < 1.0) {
        snprintf(buff, sizeof(buff), "%.0fms", time * 1000);
    } else {
        snprintf(buff, sizeof(buff), "%.1fs", time);
    }
    return  render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1ADRTime(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double       time         = 0.0;
    const double ADRTimeMap[] = {0.0005, 0.0006, 0.0007, 0.0009, 0.0011, 0.0013, 0.0015, 0.0018, 0.0021, 0.0025, 0.0030, 0.0035, 0.0040, 0.0047, 0.0055, 0.0063,
        0.0073, 0.0084, 0.0097, 0.0111, 0.0127, 0.0145, 0.0165, 0.0187, 0.0212, 0.0240, 0.0271, 0.0306, 0.0344, 0.0387, 0.0434, 0.0486,
        0.0543, 0.0606, 0.0676, 0.0752, 0.0836, 0.0928,  0.103,  0.114,  0.126,  0.139,  0.153,  0.169,  0.186,  0.204,  0.224,  0.246,
        0.269,   0.295,  0.322,  0.352,  0.384,  0.419,  0.456,  0.496,  0.540,  0.586,  0.636,  0.690,  0.748,  0.810,  0.876,  0.947,
        1.02,     1.10,   1.19,   1.28,   1.38,   1.49,   1.60,   1.72,   1.85,   1.99,   2.13,   2.28,   2.45,   2.62,   2.81,   3.00,
        3.21,     3.43,   3.66,   2.91,   4.17,   4.45,   4.74,   5.05,   5.37,   5.72,   6.08,   6.47,   6.87,   7.30,   7.75,   8.22,
        8.72,     9.25,   9.80,   10.4,   11.0,   11.6,   12.3,   13.0,   13.8,   14.6,   15.4,   16.2,   17.1,   18.1,   19.1,   20.1,
        21.2,     22.4,   23.5,   24.8,   26.1,   27.5,   28.9,   30.4,   32.0,   33.6,   35.3,   37.1,   38.9,   40.9,   42.9, 45.0};
    // use table
    time = ADRTimeMap[(int)paramValue];
    
    if (time < 0.1) {
        snprintf(buff, sizeof(buff), "%.1fms", time * 1000);
    } else if (time < 1.0) {
        snprintf(buff, sizeof(buff), "%.0fms", time * 1000);
    } else if (time < 10.0) {
        snprintf(buff, sizeof(buff), "%.2fs", time);
    } else   {
        snprintf(buff, sizeof(buff), "%.1fs", time);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1PulseTime(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double       time_to_display;
    const double pulseLoTime[] = {.00104, .00111, .00119, .00128, .00137, .00147, .00157, .00169,
        .00181, .00194, .00208, .00223, .00239, .00256, .00275, .00294,
        .00316, .00338, .00363, .00389, .00417, .00448, .00480, .00515,
        .00552, .00593, .00636, .00682, .00732, .00785, .00842, .00904,
        .00970, .01040, .01120, .01200, .01290, .01380, .01480, .01590,
        .0171,   .0183,  .0197,  .0211,  .0227,  .0244,  .0262,  .0281,
        .0302,   .0324,  .0348,  .0374,  .0402,  .0432,  .0464,  .0498,
        .0535,   .0575,  .0618,  .0664,  .0713,  .0767,  .0824,  .0886,
        .0952,   .1020,  .1100,  .1180,  .1270,  .1370,  .1470,  .1580,
        .170,     .183,   .196,   .211,   .227,   .244,   .263,   .283,
        .304,     .327,   .352,   .379,   .408,   .439,   .472,   .508,
        .547,     .588,   .633,   .681,   .734,   .790,   .850,   .915,
        .985,     1.07,   1.15,   1.24,   1.33,   1.43,   1.54,   1.66,
        1.79,     1.93,   2.07,   2.23,   2.41,   2.59,   2.79,   3.01,
        3.24,     3.49,   3.76,   4.06,   4.37,   4.71,   5.08,   5.48,
        5.90,     6.36,   6.86,   7.40,   7.98,   8.60,   9.28, 10.0};
    time_to_display = pulseLoTime[(int)paramValue]; // in s
    
    switch (module->param[gVariation][2].value) {
        case 0:   // Sub
        {
            time_to_display /= 10.0;
            break;
        }
        case 1:   // Lo
        {
            break;
        }
        case 2:   // Hi
        {
            time_to_display *= 10.0;
            break;
        }
        default:
        {
            LOG_ERROR("Wrong range in paramType1PulseTime: %u", module->param[2]->value);
        }
    }
    
    if (time_to_display < 0.01) {
        snprintf(buff, sizeof(buff), "%.2fms", time_to_display * 1000);
    } else if (time_to_display < 0.1) {
        snprintf(buff, sizeof(buff), "%.1fms", time_to_display * 1000);
    } else if (time_to_display < 1.0) {
        snprintf(buff, sizeof(buff), "%.0fms", time_to_display * 1000);
    } else if (time_to_display < 10.0) {
        snprintf(buff, sizeof(buff), "%.2fs", time_to_display);
    } else   {
        snprintf(buff, sizeof(buff), "%.1fs", time_to_display);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1Pitch(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double percent = 0.0;
    double maxVal  = 200.0;
    
    if (paramValue < 127) {
        percent = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        percent = maxVal;             // Clip
    }
    snprintf(buff, sizeof(buff), "%.1f%%", percent);
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1BipLevel(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // -64 to 63
    double res    = 0.0;
    double maxVal = 64.0;
    int typeParamIndex;
    
    switch (module->type) {
        case moduleTypeConstant:
        {
            typeParamIndex = 1;
            break;
        }
        default:
        {
            typeParamIndex = -1; // Only bipolar values
        }
    }
    if (typeParamIndex == -1) {
        if (paramValue < 127) {
            res = round((((double)paramValue - 64.0) * maxVal * 10.0) / 64.0) / 10.0;
        } else {
            res = maxVal;             // Clip
        }
    } else {
        switch (module->param[gVariation][typeParamIndex].value) {
            case 0: // Bip
            {
                if (paramValue < 127) {
                    res = round((((double)paramValue - 64.0) * maxVal * 10.0) / 64.0) / 10.0;
                } else {
                    res = maxVal;             // Clip
                }
                break;
            }
            case 1:
            {
                if (paramValue < 127) {
                    res = round((double)paramValue / 2.0 * 10.0) / 10.0;
                } else {
                    res = maxVal;
                }
            }
        }
    }
    snprintf(buff, sizeof(buff), "%.1f", res);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1LevAmpDial(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 0.25 to 4.0
    double lev = 1.0;
    // scale 0 -> 0.25x and 127 -> 4.0x, exponentially
    lev = round(exp((double)paramValue * 0.0218) * 0.25 * 100.0) / 100.0;
    
    snprintf(buff, sizeof(buff), "%.2fx", lev);
    
    return  render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1Pan(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    return render_dial(moduleArea, rectangle, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
tRectangle render_paramType1NoteDial(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // C-1 to G9
    int    noteoctave;
    int    noteval;
    char * noteNameStrMap[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}; // TODO: should be in the module resources file
    char * noteName;
    
    noteoctave = ((int)paramValue) / 12 - 1;
    noteval    = ((int)paramValue) % 12;
    noteName   = noteNameStrMap[noteval];
    
    snprintf(buff, sizeof(buff), "%s%i", noteName, noteoctave);
    
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}
    
    
    
tRectangle render_paramType1Resonance(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double res    = 0.0;
    double maxVal = 100.0;
    
    if (paramValue < 127) {
        res = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        res = maxVal;             // Clip
    }
    snprintf(buff, sizeof(buff), "%.1f", res);
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1StandardToggle(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    double  y          = rectangle.coord.y;
    double  textHeight = rectangle.size.h / 2.0;
        
    if (paramValue >= array_size_str_map(strMap)) {
        LOG_ERROR("Bad strMap for module type %s ParamRef %u ParamIndex %u, map pointer = 0x%lx, Value %u >= Map array size %u\n", gModuleProperties[module->type].name, paramRef, paramIndex, (unsigned long)strMap, (int)paramValue, array_size_str_map(strMap));
        
        //Debug help for value
        char debug[64] = {0};
        snprintf(debug, sizeof(debug), "%u", (int)paramValue);
    
        tRectangle text_rectangle = {{rectangle.coord.x, y}, {30, textHeight}};
        set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
        return draw_button(moduleArea, text_rectangle, debug);
    }
    
    if (strlen(label) > 0) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(moduleArea, (tRectangle){{rectangle.coord.x, y}, {BLANK_SIZE, textHeight}}, label);
        y += textHeight;
    }
    
    if (paramLocationList[paramRef].colourMap != NULL) {
        set_rgb_colour(paramLocationList[paramRef].colourMap[(int)paramValue]);
    } else {
        set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
    }
    return draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {largest_text_width(paramLocationList[paramRef].range, strMap, textHeight), textHeight}}, strMap[(int)paramValue]);
}

tRectangle render_paramType1UpDown(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    double  y          = rectangle.coord.y;
    double  textHeight = rectangle.size.h / 2.0;
    
    if (strMap == NULL) {
        LOG_ERROR("No strMap for module type %s\n", gModuleProperties[module->type].name);
        
        //Debug help for value
        char debug[64] = {0};
        snprintf(debug, sizeof(debug), "%u", paramValue);
        set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
        return draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {30, textHeight}}, debug);
    }
    
    if (strlen(label) > 0) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(moduleArea, (tRectangle){{rectangle.coord.x, y}, {BLANK_SIZE, textHeight / 2}}, label);
        y += textHeight;
    }
    
    if (paramLocationList[paramRef].colourMap != NULL) {
        set_rgb_colour(paramLocationList[paramRef].colourMap[(int)paramValue]);
    } else {
        set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
    }
    return draw_updown(moduleArea, (tRectangle){{rectangle.coord.x, y}, {largest_text_width(paramLocationList[paramRef].range, strMap, textHeight), textHeight}}, strMap[(int)paramValue]);
}
    
    
tRectangle render_paramType1Bypass(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    
    return draw_power_button(moduleArea, rectangle, paramValue != 0);
}

tRectangle render_paramType1Enable(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    if (paramLocationList[paramRef].colourMap != NULL) {
        set_rgb_colour(paramLocationList[paramRef].colourMap[(int)paramValue]);
    } else {
        set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);          // Grey when OFF
    }
    return draw_button(moduleArea, rectangle, NULL);  // TODO: Add label!
}

#ifdef __cplusplus
}
#endif
