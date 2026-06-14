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

tRectangle render_paramType1Freq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double freq = 0.0;

    freq = round(13.75 * pow(2, (double)paramValue / 12.0) * 100.0) / 100.0;

    if (freq < 100) {
        snprintf(buff, buffSize, "%.2fHz", freq);
    } else if (freq < 1000) {
        snprintf(buff, buffSize, "%.1fHz", freq);
    } else if (freq < 10000) {
        snprintf(buff, buffSize, "%.2fkHz", freq / 1000.0);
    } else {
        snprintf(buff, buffSize, "%.1fkHz", freq / 1000.0);
    }
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1OscFreq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // Frequency dial for oscillators. Uses PitchType param to control display of Tune
    int      pitchTypeParamIndex = 0;
    uint32_t slot                = atomic_load(&gSlot);
    //uint32_t location            = atomic_load(&gLocation);
    uint32_t variation           = gPatchDescr[slot].activeVariation;

    switch (module->type) {
        case moduleTypeOscMaster:
        {
            pitchTypeParamIndex = 3;
            break;
        }
        case moduleTypeOscB:
        case moduleTypeResonator:
        case moduleTypeOscShpB:
        {
            pitchTypeParamIndex = 4;
            break;
        }
        case moduleTypeOscC:
        {
            pitchTypeParamIndex = 3;
            break;
        }
        default:
        {
            LOG_ERROR("paramType1OscFreq missing module->type implementation %d\n", module->type);
        }
    }

    switch (module->param[variation][pitchTypeParamIndex].value) {
        case 0:   // Semi. -64 to 63
        {
            double res;

            if (paramValue < 127) {
                res = round((((double)paramValue - 64.0) * 10.0)) / 10.0;
            } else {
                res = 63.0;             // Clip
            }
            snprintf(buff, buffSize, "%.1f", res);
            break;
        }
        case 1:   // Freq. 8.1758 Hz to 12.55 kHz
        {
            double res;
            double min_freq = 8.1758;
            double max_freq = 12550.0;
            res = exp((double)paramValue / 127.0 * log(max_freq / min_freq)) * min_freq;

            if (res < 100) {
                snprintf(buff, buffSize, "%.2fHz", res);
            } else if (res < 1000) {
                snprintf(buff, buffSize, "%.1fHz", res);
            } else if (res < 10000) {
                snprintf(buff, buffSize, "%.2fkHz", res / 1000.0);
            } else {
                snprintf(buff, buffSize, "%.1fkHz", res / 1000.0);
            }
            break;
        }
        case 2:   // Factor. 0->0.0248, 127 -> 38.072
        {
            double res;
            double min_factor = .0248;
            double max_factor = 38.072;
            res = exp((double)paramValue / 127.0 * log(max_factor / min_factor)) * min_factor;
            snprintf(buff, buffSize, "%.4fHz", res);
            break;
        }
        case 3:   // Partial. Displays partials for values from 33 upwards, Hz below.
        {
            double res;

            if (paramValue == 0.0) {
                snprintf(buff, buffSize, "0 Hz");
            } else if (paramValue < 33.0) { // show value as Hz
                double min_freq = 0.005;
                double max_freq = 5.153;
                res = exp(((double)paramValue - 1.0) / 31.0 * log(max_freq / min_freq)) * min_freq;
                snprintf(buff, buffSize, "%.3fHz", res);
            } else if (paramValue < 64.0) {
                res = 64.0 - paramValue + 1.0;
                snprintf(buff, buffSize, "1:%.0f", res);
            } else {
                res = paramValue - 64.0 + 1.0;
                snprintf(buff, buffSize, "%.0f:1", res);
            }
            break;
        }
    }
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Fine(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double res = ((double)paramValue - 64.0) / 64.0 * 50.0;

    snprintf(buff, buffSize, "%.1f", res);
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1GeneralFreq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
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
        snprintf(buff, buffSize, "%.0fHz", freq);
    } else {
        snprintf(buff, buffSize, "%.2fkHz", freq / 1000.0);
    }
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Shape(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 50.0->99.9
    int val = 0;

    val = (int)(paramValue * 49.0 / 127.0 + 50.0);
    snprintf(buff, buffSize, "%u", val);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1FreqDrum(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double freq = 0.0;

    // 0 -> 20 Hz, 127 -> 784 Hz
    freq = round(20.0 * pow(2, (double)paramValue * 0.041675) * 100.0) / 100.0;

    if (freq < 100) {
        snprintf(buff, buffSize, "%.2fHz", freq);
    } else {
        snprintf(buff, buffSize, "%.1fHz", freq);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1LFORate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double   rate;
    int      rateModeParamIndex;
    uint32_t slot      = atomic_load(&gSlot);
    uint32_t variation = gPatchDescr[slot].activeVariation;

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
            rateModeParamIndex = 0;
            LOG_ERROR("paramType1LFORate missing module->type implementation");
        }
    }

    switch (module->param[variation][rateModeParamIndex].value) {
        case 0: // Sub - compute range in s
        {
            double range_start = 699.0;
            double range_end   = 5.46;
            rate = range_start * exp((double)paramValue * log(range_end / range_start) / 127.0);

            if (rate > 100.0) {
                snprintf(buff, buffSize, "%.0fs", rate);
            } else if (rate > 10.0) {
                snprintf(buff, buffSize, "%.1fs", rate);
            } else {
                snprintf(buff, buffSize, "%.2fs", rate);
            }
            break;
        }

        case 1: // Rate Lo -- compute rate in s
        {
            double range_start = 62.9;
            double range_end   = 0.04098; // 24.4 Hz
            rate = range_start * exp((double)paramValue * log(range_end / range_start) / 127.0);

            if (rate > 10.0) {
                snprintf(buff, buffSize, "%.1fs", rate);
            } else if (rate > 0.1) {
                snprintf(buff, buffSize, "%.2fHz", 1.0 / rate);
            } else {
                snprintf(buff, buffSize, "%.1fHz", 1.0 / rate);
            }
            break;
        }
        case 2: // Rate Hi - compute rate in Hz
        {
            double min_freq = 0.26;
            double max_freq = 392.0;
            double freq     = min_freq * exp((double)paramValue * log(max_freq / min_freq) / 127.0);

            if (freq < 10.0) {
                snprintf(buff, buffSize, "%.2fHz", freq);
            } else if (freq < 100.0) {
                snprintf(buff, buffSize, "%.1fHz", freq);
            } else {
                snprintf(buff, buffSize, "%.0fHz", freq);
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
            snprintf(buff, buffSize, "%u", bpm);
            break;
        }
        case 4: // ClkSync. 32 values
        {
            int posClkSyncStrMap = (int)(paramValue / 4.0);

            if (posClkSyncStrMap > 31) {
                posClkSyncStrMap = 31;
            }
            snprintf(buff, buffSize, "%s\n", clkSyncStrMap[posClkSyncStrMap]);
            break;
        }
        default:
        {
            LOG_ERROR("Wrong case %u in paramTypeLFORate\n", module->param[variation][rateModeParamIndex].value);
        }
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Int(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int val = 0;

    val = paramValue;
    snprintf(buff, buffSize, "%u", val);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1dB(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
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
    snprintf(buff, buffSize, "%+.0fdB", dB);

    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1MixLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    //double       level      = 0.0;

    int      expLinDBparam = 0;
    uint32_t slot          = atomic_load(&gSlot);
    uint32_t variation     = gPatchDescr[slot].activeVariation;

    switch (module->type) {
        case moduleTypeMix8to1B:
        {
            expLinDBparam = 8;
            break;
        }
        default:
        {
            break;
        }
    }
    //level = paramValue;

    if (module->param[variation][expLinDBparam].value == 2) { // display dB
        snprintf(buff, buffSize, "%s", dbLvlStrMap[(int)paramValue]);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Time(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double time     = 0.0;
    double min_time = 0;
    double max_time = 0.0;

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
    if (min_time <= 0.0 || max_time <= 0.0) {
        snprintf(buff, buffSize, "???");
    } else {
        time = exp((double)paramValue / 127 * log(max_time / min_time)) * min_time;
    }

    if (time < 1.0) {
        snprintf(buff, buffSize, "%.0fms", time * 1000);
    } else {
        snprintf(buff, buffSize, "%.1fs", time);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1TimeClk(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
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
        snprintf(buff, buffSize, "%.0fms", time * 1000);
    } else {
        snprintf(buff, buffSize, "%.1fs", time);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1ADRTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double time = 0.0;


    // use table
    //time = ADRTimeMap[(int)paramValue];

    //if (time < 0.1) {
    //    snprintf(buff, buffSize, "%.1fms", time * 1000);
    //} else if (time < 1.0) {
    //    snprintf(buff, buffSize, "%.0fms", time * 1000);
    //} else if (time < 10.0) {
    //    snprintf(buff, buffSize, "%.2fs", time);
    //} else {
    //    snprintf(buff, buffSize, "%.1fs", time);
    //}
    snprintf(buff, buffSize, "%s", ADRTimeStrMap[(int)paramValue]);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1PulseTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double   time_to_display;
    uint32_t slot      = atomic_load(&gSlot);
    uint32_t variation = gPatchDescr[slot].activeVariation;

#if 0
    time_to_display = pulseLoTime[(int)paramValue]; // in s

    switch (module->param[variation][2].value) {
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
            LOG_ERROR("Wrong range in paramType1PulseTime: %u", module->param[variation][2].value);
        }
    }

    if (time_to_display < 0.01) {
        snprintf(buff, buffSize, "%.2fms", time_to_display * 1000);
    } else if (time_to_display < 0.1) {
        snprintf(buff, buffSize, "%.1fms", time_to_display * 1000);
    } else if (time_to_display < 1.0) {
        snprintf(buff, buffSize, "%.0fms", time_to_display * 1000);
    } else if (time_to_display < 10.0) {
        snprintf(buff, buffSize, "%.2fs", time_to_display);
    } else {
        snprintf(buff, buffSize, "%.1fs", time_to_display);
    }
#endif
    snprintf(buff, buffSize, "%s", pulseLoTimeStrMap[(int)paramValue]);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Pitch(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double percent = 0.0;
    double maxVal  = 200.0;

    if (paramValue < 127) {
        percent = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        percent = maxVal;             // Clip
    }
    snprintf(buff, buffSize, "%.1f%%", percent);
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1BipLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // -64 to 63
    double   res            = 0.0;
    double   maxVal         = 64.0;
    int      typeParamIndex = 0;
    uint32_t slot           = atomic_load(&gSlot);
    uint32_t variation      = gPatchDescr[slot].activeVariation;

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
        switch (module->param[variation][typeParamIndex].value) {
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
    snprintf(buff, buffSize, "%.1f", res);
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1LevAmpDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 0.25 to 4.0
    double lev = 1.0;

    // scale 0 -> 0.25x and 127 -> 4.0x, exponentially
    lev = round(exp((double)paramValue * 0.0218) * 0.25 * 100.0) / 100.0;

    snprintf(buff, buffSize, "%.2fx", lev);

    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Pan(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    return render_dial(moduleArea, rectangle, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1NoteDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // C-1 to G9
    int          noteoctave;
    int          noteval;
    const char * noteName;

    noteoctave = ((int)paramValue) / 12 - 1;
    noteval    = ((int)paramValue) % 12;
    noteName   = noteNameStrMap[noteval];

    snprintf(buff, buffSize, "%s%i", noteName, noteoctave);

    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Resonance(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double res    = 0.0;
    double maxVal = 100.0;

    if (paramValue < 127) {
        res = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        res = maxVal;             // Clip
    }
    snprintf(buff, buffSize, "%.1f", res);
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1StrMap(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    const char ** strMap = paramLocationList[paramRef].strMap;

    if (strMap && (uint32_t)paramValue < array_size_str_map((char **)strMap)) {
        snprintf(buff, buffSize, "%s", strMap[(int)paramValue]);
    } else {
        snprintf(buff, buffSize, "%d", (int)paramValue);
    }
    return render_dial_with_text(moduleArea, rectangle, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1FreqShift(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    uint32_t     slot      = atomic_load(&gSlot);
    uint32_t     variation = gPatchDescr[slot].activeVariation;
    const char * s         = NULL;

    switch (module->param[variation][2].value) {
        case 0:  s = freq_shift_hiStrMap[(int)paramValue];
            break;

        case 1:  s = freq_shift_loStrMap[(int)paramValue];
            break;

        case 2:  s = freq_shift_subStrMap[(int)paramValue];
            break;
    }

    if (s) {
        snprintf(buff, buffSize, "%s", s);
    } else {
        snprintf(buff, buffSize, "%d", (int)paramValue);
    }
    return render_dial_with_text(moduleArea, rectangle, label, buff, paramValue, range, morphRange, colour);
}

tRectangle render_paramType1StandardToggle(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, char ** strMap) {
    double y                      = rectangle.coord.y;
    double textHeight             = rectangle.size.h / 2.0;
    tRgb   buttonBackgroundColour = RGB_BACKGROUND_GREY;

    if (paramValue >= array_size_str_map(strMap)) {
        LOG_ERROR("Bad strMap for module type %d %s ParamRef %u ParamIndex %u, map pointer = 0x%lx, Value %u >= Map array size %u\n", module->type, gModuleProperties[module->type].name, paramRef, paramIndex, (unsigned long)strMap, (int)paramValue, array_size_str_map(strMap));

        //Debug help for value
        char       debug[64]      = {0};
        snprintf(debug, sizeof(debug), "%u", (int)paramValue);

        tRectangle text_rectangle = {{rectangle.coord.x, y}, {30, textHeight}};
        //set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
        return draw_button(moduleArea, text_rectangle, debug, (tRgb)RGB_BACKGROUND_GREY);
    }

    if (strlen(label) > 0) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(moduleArea, (tRectangle){{rectangle.coord.x, y}, {BLANK_SIZE, textHeight}}, label);
        y += textHeight;
    }

    if (paramLocationList[paramRef].colourMap != NULL) {
        buttonBackgroundColour = paramLocationList[paramRef].colourMap[(int)paramValue];
    }
    return draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {largest_text_width(paramLocationList[paramRef].range, strMap, textHeight), textHeight}}, strMap[(int)paramValue], buttonBackgroundColour);
}

tRectangle render_paramType1UpDown(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, char ** strMap) {
    double y          = rectangle.coord.y;
    double textHeight = rectangle.size.h / 2.0;

    if (strMap == NULL) {
        LOG_ERROR("No strMap for module type %s\n", gModuleProperties[module->type].name);

        //Debug help for value
        char debug[64] = {0};
        snprintf(debug, sizeof(debug), "%f", paramValue);
        return draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {30, textHeight}}, debug, (tRgb)RGB_BACKGROUND_GREY);
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

tRectangle render_paramType1Bypass(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    return draw_power_button(moduleArea, rectangle, paramValue != 0);
}

tRectangle render_paramType1Enable(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    tRgb buttonBackgroundColour = RGB_BACKGROUND_GREY;

    if (paramLocationList[paramRef].colourMap != NULL) {
        buttonBackgroundColour = paramLocationList[paramRef].colourMap[(int)paramValue];
    }
    return draw_button(moduleArea, rectangle, NULL, buttonBackgroundColour);  // TODO: Add label!
}

#ifdef __cplusplus
}
#endif
