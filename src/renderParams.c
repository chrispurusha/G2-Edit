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
// Notes: Docs/code-notes/renderParams.c.md - "// notes §k" refers there.

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
#include "synthlibDefs.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"
#include "moduleGraphics.h"
#include "globalVars.h"
#include "moduleGraphics.h"
#include "renderParams.h"

// notes §1
static tArea gParamRenderArea = moduleArea;

void set_param_render_area(tArea area) {
    gParamRenderArea = area;
}

tRectangle render_paramType1Freq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double freq = 0.0;

    freq = round(flt_cutoff_hz(paramValue) * 100.0) / 100.0;

    if (freq < 100) {
        snprintf(buff, buffSize, "%.2fHz", freq);
    } else if (freq < 1000) {
        snprintf(buff, buffSize, "%.1fHz", freq);
    } else if (freq < 10000) {
        snprintf(buff, buffSize, "%.2fkHz", freq / 1000.0);
    } else {
        snprintf(buff, buffSize, "%.1fkHz", freq / 1000.0);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1OscFreq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // Frequency dial for oscillators. Uses PitchType param to control display of Tune
    int      pitchTypeParamIndex = osc_pitch_type_param_index(module);
    uint32_t slot                = module->key.slot;
    //uint32_t location            = gLocation;
    uint32_t variation           = gPatchDescr[slot].activeVariation;

    if (pitchTypeParamIndex < 0) {
        LOG_ERROR("paramType1OscFreq missing module->type implementation %d\n", module->type);
        pitchTypeParamIndex = 0;
    }

    switch (module->param[variation][pitchTypeParamIndex].value) {
        case 0:   // Semi. -64 to 63
        {
            double res = osc_freq_semitones(paramValue);

            snprintf(buff, buffSize, "%.1f", res);
            break;
        }
        case 1:   // Freq. 8.1758 Hz to 12.55 kHz
        {
            double res = osc_freq_hz(paramValue);

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
            double res = osc_freq_factor(paramValue);

            snprintf(buff, buffSize, "%.4fx", res);
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
        case 4:   // Sub. The Semi note scale, eleven octaves down; the bottom step is silence.
        {
            // The Cent dial's offset is not applied: which parameter carries it varies by module
            // and there is no per-module map for it here yet. It is zero at Cent's default of 64.
            double res = osc_sub_freq_hz(paramValue, 0.0);

            if (res == 0.0) {
                snprintf(buff, buffSize, "0 Hz");
            } else {
                snprintf(buff, buffSize, "%.4fHz", res);
            }
            break;
        }
        default:
        {
            // No module offers a fifth Pitch Type yet, but a patch written on the synth can carry
            // one. Print the raw value rather than leaving buff holding whatever was in it last.
            snprintf(buff, buffSize, "%.0f", paramValue);
            break;
        }
    }
    return render_dial_with_text(gParamRenderArea, rectangle, label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Fine(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double res = osc_fine_cents(paramValue);

    snprintf(buff, buffSize, "%.1f", res);
    return render_dial_with_text(gParamRenderArea, rectangle, label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
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
    return render_dial_with_text(gParamRenderArea, rectangle, label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Shape(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 50.0->99.9
    int val = (int)osc_shape_percent(paramValue);

    snprintf(buff, buffSize, "%u%%", val);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1LfoShape(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 1 -> 99, neutral at the centre - see lfo_shape_percent().
    int val = (int)lfo_shape_percent(paramValue);

    snprintf(buff, buffSize, "%u%%", val);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
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
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1LFORate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double   rate;
    int      rateModeParamIndex;
    uint32_t slot      = module->key.slot;
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
        case moduleTypeLfoB:
        {
            rateModeParamIndex = 2;
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
            rate = 1.0 / lfo_rate_hz(0, (double)paramValue);

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
            rate = 1.0 / lfo_rate_hz(1, (double)paramValue);

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
            double freq = lfo_rate_hz(2, (double)paramValue);

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
            int bpm = (int)round(lfo_rate_hz(3, (double)paramValue) * 60.0);

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
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Int(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int val = 0;

    val = paramValue;
    snprintf(buff, buffSize, "%u", val);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// notes §2
tRectangle render_paramType1UniPolShort(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int raw = (int)paramValue;

    if (raw >= 63) {
        snprintf(buff, buffSize, "64");
    } else {
        snprintf(buff, buffSize, "%d", raw);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1dB(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double dB       = 0.0;
    double dB_range = 1.0;

    switch (module->type) {
        case moduleTypeEq2Band:
        case moduleTypeEq3band:
        case moduleTypeEqPeak:
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
    // notes §3
    dB = ((paramValue >= 127.0) ? 64.0 : ((double)paramValue - 64.0)) / 64.0 * dB_range;
    snprintf(buff, buffSize, "%.1fdB", dB);

    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1MixLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    //double       level      = 0.0;

    int      expLinDBparam = 0;
    uint32_t slot          = module->key.slot;
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
        // notes §4
        int raw = (int)paramValue;

        if (raw <= 0) {
            snprintf(buff, buffSize, "-oo");
        } else if (raw == 1) {
            snprintf(buff, buffSize, "-99.9dB");
        } else if (raw == 2) {
            snprintf(buff, buffSize, "-99.0dB");
        } else if (raw >= 127) {
            snprintf(buff, buffSize, "-0dB");
        } else {
            snprintf(buff, buffSize, "%.1fdB", mix_level_db(paramValue));
        }
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
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
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1TimeClk(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // In Clk mode the delay follows the master clock, so a millisecond figure derived from the
    // Range would be meaningless — show the division the dial actually selects.
    {
        int clkIndex = delay_time_clk_param_index(module->type);

        if (clkIndex >= 0) {
            uint32_t clkSlot      = module->key.slot;
            uint32_t clkVariation = gPatchDescr[clkSlot].activeVariation;

            if (module->param[clkVariation][clkIndex].value != 0) {
                snprintf(buff, buffSize, "%s", clkSyncStrMap[clk_sync_index(paramValue)]);
                return render_dial_with_text(gParamRenderArea, rectangle,
                                             (char *)paramLocationList[paramRef].label, buff,
                                             (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue,
                                             paramLocationList[paramRef].range, morphRange, colour);
            }
        }
    }

    double time     = 0.0;
    double max_time = 0.0;

    switch (module->type) {
        case moduleTypeDelayQuad:
        case moduleTypeDelayA:
        case moduleTypeDelayB:
        case moduleTypeDlyStereo:
        {
            max_time = delay_range_max_seconds(module->type, module->mode[0].value);
            break;
        }
        default:
        {
            LOG_ERROR("paramType1TimeClk missing module->type implementation, %u", module->type);
        }
    }
    time = delay_time_seconds(max_time, paramValue);

    // "m", not "ms" - and the precision falls as the value grows, so the readout stays about the
    // same width whatever it holds. Seconds only once the time actually reaches 1.0, which on a
    // Range of 1.0s is the single topmost value of the dial.
    if (time < 0.01) {
        snprintf(buff, buffSize, "%.2fm", time * 1000.0);
    } else if (time < 0.1) {
        snprintf(buff, buffSize, "%.1fm", time * 1000.0);
    } else if (time < 1.0) {
        snprintf(buff, buffSize, "%.0fm", time * 1000.0);
    } else {
        snprintf(buff, buffSize, "%.3fs", time);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1ADRTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // notes §5
    double time = adr_time_seconds(paramValue);

    if (time < 0.1) {
        snprintf(buff, buffSize, "%.1fms", time * 1000.0);
    } else if (time < 1.0) {
        snprintf(buff, buffSize, "%.0fms", time * 1000.0);
    } else if (time < 10.0) {
        snprintf(buff, buffSize, "%.2fs", time);
    } else {
        snprintf(buff, buffSize, "%.1fs", time);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1PulseTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
#if 0
    // Declared inside the disabled block, not above it: outside, they are three unused variables and
    // three warnings, and the block is meant to be revivable in one step rather than needing its
    // declarations put back by hand.
    double   time_to_display;
    uint32_t slot      = module->key.slot;
    uint32_t variation = gPatchDescr[slot].activeVariation;

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
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
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
    return render_dial_with_text(gParamRenderArea, rectangle, label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1BipLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // -64 to 63
    double   res            = 0.0;
    double   maxVal         = 64.0;
    int      typeParamIndex = 0;
    uint32_t slot           = module->key.slot;
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
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Partials(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // notes §6
    int          v    = (int)paramValue - 64;
    const char * star = ((v > 32) || (v < -32)) ? "*" : "";

    if (v > 0) {
        snprintf(buff, buffSize, "+%d%s", v, star);
    } else {
        snprintf(buff, buffSize, "%d%s", v, star);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1UniPol(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 0 to 64 'units' level, as the original editor shows it: raw/2
    // shown as N.0 (even raw) or N.5 (odd raw), with raw 127 special-cased to
    // "64.0". Used for EnvADSR Sustain etc. (manual: "Range: 0 to 64 units").
    int raw = (int)paramValue;

    if (raw >= 127) {
        snprintf(buff, buffSize, "64.0");
    } else if ((raw & 1) == 0) {
        snprintf(buff, buffSize, "%d.0", raw >> 1);
    } else {
        snprintf(buff, buffSize, "%d.5", raw >> 1);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1LevAmpDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 0.25 to 4.0
    double lev = round(lev_amp_gain((double)paramValue) * 100.0) / 100.0;

    snprintf(buff, buffSize, "%.2fx", lev);

    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// notes §7
tRectangle render_paramType1Phase(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    snprintf(buff, buffSize, "%.0f", paramValue * 360.0 / 128.0);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// PShift's Semi, in semitones: a quarter of one per dial step, so ±16 rather than the ±64 the name
// suggests. The top step is not rounded up - the panel reads +15.8 there.
tRectangle render_paramType1PShiftSemi(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    snprintf(buff, buffSize, "%+.1f", pshift_semitones(paramValue));
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// notes §8
tRectangle render_paramType1BipolarPinned(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int value = (paramValue >= 127.0) ? 64 : ((int)paramValue - 64);

    snprintf(buff, buffSize, "%d", value);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// A SYMMETRIC range: the 0..64 units scale, but shown as the ± span it sets rather than as a single
// bound, so a quantiser's Range reads "+/-32.0" and covers that far either side of centre.
tRectangle render_paramType1PlusMinusUnits(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int raw = (int)paramValue;

    if (raw >= 127) {
        snprintf(buff, buffSize, "+/-64.0");
    } else {
        snprintf(buff, buffSize, "+/-%d.%c", raw >> 1, ((raw & 1) == 0) ? '0' : '5');
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// A count whose zero is a state rather than a quantity - "Off", not "0".
tRectangle render_paramType1OffNum(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int raw = (int)paramValue;

    if (raw <= 0) {
        snprintf(buff, buffSize, "Off");
    } else {
        snprintf(buff, buffSize, "%d", raw);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// Scratch's Ratio: playback speed as a signed multiple through a standstill at the centre. The synth
// writes the sign ahead of the 'x' and drops the decimals at zero entirely - "- x4.00", "x0",
// "x4.00" - so this is not simply a signed number with a prefix.
tRectangle render_paramType1ScratchRatio(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double ratio = scratch_ratio(paramValue);

    if (ratio == 0.0) {
        snprintf(buff, buffSize, "x0");
    } else if (ratio < 0.0) {
        snprintf(buff, buffSize, "- x%.2f", -ratio);
    } else {
        snprintf(buff, buffSize, "x%.2f", ratio);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// The Digitizer's Sample Rate: a pitch scale in disguise, twelve dial steps to a doubling.
tRectangle render_paramType1SampleRate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double hz = digitizer_rate_hz(paramValue);

    // Three significant figures the whole way up, so the reading loses a decimal each time the
    // number gains a digit: 32.70 Hz, 1.32 kHz, 50.2 kHz.
    if (hz < 1000.0) {
        snprintf(buff, buffSize, "%.2fHz", hz);
    } else if (hz < 10000.0) {
        snprintf(buff, buffSize, "%.2fkHz", hz / 1000.0);
    } else {
        snprintf(buff, buffSize, "%.1fkHz", hz / 1000.0);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// PitchTrack's Threshold in dB, silent at the bottom of the dial. The panel spells that step out as
// "- Infinity" rather than showing a very large negative number.
tRectangle render_paramType1ThresholdDb(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int raw = (int)paramValue;

    if (raw <= 0) {
        snprintf(buff, buffSize, "-oo");
    } else if (raw >= 127) {
        snprintf(buff, buffSize, "-0dB");
    } else {
        snprintf(buff, buffSize, "%.1fdB", pitchtrack_threshold_db(paramValue));
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// notes §9
tRectangle render_paramType1Bipolar(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    int value = (int)paramValue - 64;

    snprintf(buff, buffSize, (value > 0) ? "+%d" : "%d", value);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// A filter's resonance as Q, for the filters that read in Q rather than as a percentage. The synth
// drops the decimals once Q reaches 10, which keeps the reading three characters wide across the
// whole dial.
tRectangle render_paramType1ResonanceQ(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double q = flt_resonance_q(paramValue);

    snprintf(buff, buffSize, (q >= 10.0) ? "%.0f" : "%.2f", q);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1FlangerRate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    snprintf(buff, buffSize, "%.2fHz", flanger_rate_hz(paramValue));
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// The phaser's rate loses a decimal from raw 119 up, where the reading passes 10 Hz.
#define PHASER_RATE_ONE_DECIMAL_FROM    (119.0)

tRectangle render_paramType1PhaserRate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    snprintf(buff, buffSize, (paramValue >= PHASER_RATE_ONE_DECIMAL_FROM) ? "%.1fHz" : "%.2fHz", phaser_rate_hz(paramValue));
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// ClkGen's Swing, as a percentage. 50% is no swing - every second step lands exactly halfway - and
// the dial reaches 75% at the top, which is a hard shuffle. The scale is raw/5.08 + 50, so the
// useful half of the control is the bottom third of the knob's travel.
tRectangle render_paramType1Swing(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    snprintf(buff, buffSize, "%.1f%%", (paramValue / 5.08) + 50.0);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

// EqPeak's bandwidth, in OCTAVES, and it runs BACKWARDS: (128 - raw)/64, so the bottom of the dial
// is the widest bell at 2.00 octaves and the top is the narrowest at 0.02. Turning the knob up
// narrows the band rather than widening it, which the raw number gave no hint of.
tRectangle render_paramType1Bandwidth(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    snprintf(buff, buffSize, "%.2fOct", (128.0 - paramValue) / 64.0);
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Pan(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    return render_dial(gParamRenderArea, rectangle, paramValue, paramLocationList[paramRef].range, morphRange, colour);
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

    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1Resonance(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // notes §10
    int    raw    = (int)paramValue;
    double maxVal = 100.0;
    double res    = 0.0;

    if (raw < 127) {
        res = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        res = maxVal;             // Clip
    }

    if ((raw == 0) || (raw == 32) || (raw == 64) || (raw == 96) || (raw >= 127)) {
        snprintf(buff, buffSize, "%.0f", res);
    } else {
        snprintf(buff, buffSize, "%.1f", res);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

static const char * gNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

tRectangle render_paramType1Slider(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double     textH        = 8.0;
    tRgb       black        = {0.0, 0.0, 0.0};
    tRectangle textRect     = {0};
    tRectangle topRect      = {0};
    char       topStr[8]    = {0};
    char       bottomStr[8] = {0};
    bool       twoRow       = false;

    textRect.coord.x = rectangle.coord.x;
    textRect.coord.y = rectangle.coord.y - textH;
    textRect.size.w  = BLANK_SIZE;
    textRect.size.h  = textH;

    uint32_t   slot         = module->key.slot;
    uint32_t   variation    = gPatchDescr[slot].activeVariation;

    if (module->type == moduleTypeSeqNote) {
        int octave = (int)((uint32_t)paramValue / 12) - 1;
        int note   = (int)((uint32_t)paramValue % 12);
        twoRow = true;
        snprintf(topStr, sizeof(topStr), "%s", gNoteNames[note]);
        snprintf(bottomStr, sizeof(bottomStr), "%d", octave);
    } else if (module->type == moduleTypeMixFader) {
        // Display 0-100, matching the CommonDial-type mix level dials (e.g. Mix4-1C)
        double level = (paramValue < 127) ? round(((double)paramValue * 100.0 * 10.0) / 128.0) / 10.0 : 100.0;
        snprintf(buff, buffSize, "%.1f", level);
    } else {
        uint32_t bipParamIdx = 0;
        bool     hasBipolar  = false;

        if (module->type == moduleTypeSeqVal || module->type == moduleTypeSeqLev) {
            bipParamIdx = 34;
            hasBipolar  = true;
        } else if (module->type == moduleTypeSeqCtr) {
            bipParamIdx = 33;
            hasBipolar  = true;
        }

        // notes §11
        if (hasBipolar && module->param[variation][bipParamIdx].value == 0) {
            int displayValue = (paramValue >= 127.0) ? 64 : ((int)paramValue - 64);

            snprintf(buff, buffSize, "%d", displayValue);
        } else if (hasBipolar) {
            int raw = (int)paramValue;

            twoRow = true;
            snprintf(topStr, sizeof(topStr), "%d", (raw >= 127) ? 64 : (raw >> 1));
            snprintf(bottomStr, sizeof(bottomStr), ".%c", ((raw >= 127) || ((raw & 1) == 0)) ? '0' : '5');
        } else {
            snprintf(buff, buffSize, "%u", (uint32_t)paramValue);
        }
    }
    set_rgb_colour(black);

    if (twoRow) {
        topRect.coord.x = textRect.coord.x;
        topRect.coord.y = textRect.coord.y - textH;
        topRect.size.w  = BLANK_SIZE;
        topRect.size.h  = textH;

        render_text(gParamRenderArea, topRect, topStr);
        render_text(gParamRenderArea, textRect, bottomStr);
    } else {
        render_text(gParamRenderArea, textRect, buff);
    }
    return draw_slider(gParamRenderArea, rectangle, (uint32_t)paramValue, range, morphRange, colour);
}

tRectangle render_paramType1StrMap(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    const char ** strMap = paramLocationList[paramRef].strMap;

    if (strMap && (uint32_t)paramValue < array_size_str_map(strMap)) {
        snprintf(buff, buffSize, "%s", strMap[(int)paramValue]);
    } else {
        snprintf(buff, buffSize, "%d", (int)paramValue);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1FreqShift(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    uint32_t     slot      = module->key.slot;
    uint32_t     variation = gPatchDescr[slot].activeVariation;
    const char * s         = NULL;

    // notes §12
    switch (module->param[variation][2].value) {
        case 0:  s = freq_shift_subStrMap[(int)paramValue];
            break;

        case 1:  s = freq_shift_loStrMap[(int)paramValue];
            break;

        case 2:  s = freq_shift_hiStrMap[(int)paramValue];
            break;
    }

    if (s) {
        snprintf(buff, buffSize, "%s", s);
    } else {
        snprintf(buff, buffSize, "%d", (int)paramValue);
    }
    return render_dial_with_text(gParamRenderArea, rectangle, label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, range, morphRange, colour);
}

tRectangle render_paramType1StandardToggle(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    double y                      = rectangle.coord.y;
    double textHeight             = (double)STANDARD_BUTTON_TEXT_HEIGHT;
    tRgb   buttonBackgroundColour = RGB_BACKGROUND_GREY;

    if (strMap == NULL || paramValue >= array_size_str_map(strMap)) {
        if (strMap != NULL) {
            LOG_ERROR("Bad strMap for module type %d %s ParamRef %u ParamIndex %u, map pointer = 0x%lx, Value %u >= Map array size %u\n", module->type, gModuleProperties[module->type].name, paramRef, paramIndex, (unsigned long)strMap, (int)paramValue, array_size_str_map(strMap));
        }
        char       debug[64]      = {0};
        snprintf(debug, sizeof(debug), "%u", (int)paramValue);

        tRectangle text_rectangle = {{rectangle.coord.x, y}, {30, textHeight}};
        return draw_button(gParamRenderArea, text_rectangle, debug, (tRgb)RGB_BACKGROUND_GREY);
    }

    // notes §13
    if (strlen(label) > 0) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(gParamRenderArea, (tRectangle){{rectangle.coord.x, y - textHeight}, {BLANK_SIZE, textHeight}}, label);
    }

    if (paramLocationList[paramRef].colourMap != NULL) {
        buttonBackgroundColour = paramLocationList[paramRef].colourMap[(int)paramValue];
    }
    return draw_button(gParamRenderArea, (tRectangle){{rectangle.coord.x, y}, {largest_text_width(paramLocationList[paramRef].range, strMap, textHeight, eCache), textHeight}}, strMap[(int)paramValue], buttonBackgroundColour);
}

tRectangle radio_button_rect(tRectangle groupRect, uint32_t buttonCount, uint32_t buttonIndex) {
    uint32_t   columns = radio_columns(buttonCount);
    uint32_t   rows    = radio_rows(buttonCount);
    tRectangle rect    = groupRect;

    if ((columns == 0) || (rows == 0)) {
        return rect;
    }
    rect.size.w   = groupRect.size.w / (double)columns;
    rect.size.h   = groupRect.size.h / (double)rows;
    rect.coord.x += rect.size.w * (double)(buttonIndex % columns);
    rect.coord.y += rect.size.h * (double)(buttonIndex / columns);

    return rect;
}

// Which button of a Channel Select group a coordinate landed on, or -1 for none. The inverse of
// radio_button_rect(), and it has to stay that way — the hit test and the drawing are the same
// geometry read in opposite directions.
int32_t radio_button_at(tRectangle groupRect, uint32_t buttonCount, tCoord coord) {
    uint32_t i = 0;

    for (i = 0; i < buttonCount; i++) {
        tRectangle buttonRect = radio_button_rect(groupRect, buttonCount, i);

        if (  (coord.x >= buttonRect.coord.x) && (coord.x < (buttonRect.coord.x + buttonRect.size.w))
           && (coord.y >= buttonRect.coord.y) && (coord.y < (buttonRect.coord.y + buttonRect.size.h))) {
            return (int32_t)i;
        }
    }

    return -1;
}

// notes §14
tRectangle render_paramType1RadioEdit(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    double     textHeight       = (double)STANDARD_BUTTON_TEXT_HEIGHT;
    uint32_t   columns          = radio_columns(range);
    uint32_t   rows             = radio_rows(range);
    double     buttonWidth      = 0.0;
    double     rowHeight        = 0.0;
    tRectangle groupRect        = {{rectangle.coord.x, rectangle.coord.y}, {0.0, 0.0}};
    tRectangle drawnBounds      = {0};
    uint32_t   i                = 0;

    if ((range == 0) || (columns == 0)) {
        return groupRect;
    }
    // notes §15
    double     buttonMargin     = draw_button_bounds((tRectangle){{0.0, 0.0}, {0.0, 0.0}}).size.w / 2.0;

    // notes §16
    buttonWidth      = get_text_width(RADIO_WIDEST_CAPTION, textHeight, eCache) + RADIO_BUTTON_PADDING + (2.0 * buttonMargin);
    rowHeight        = textHeight + (2.0 * buttonMargin);

    // notes §17
    double     availablePercent = RADIO_FACE_WIDTH_PERCENT - paramLocationList[paramRef].rectangle.coord.x;

    if (availablePercent > 0.0) {
        double maxButtonWidth = scale_from_percent(availablePercent) / (double)columns;

        if (buttonWidth > maxButtonWidth) {
            buttonWidth = maxButtonWidth;
        }
    }
    groupRect.size.w = buttonWidth * (double)columns;
    groupRect.size.h = rowHeight * (double)rows;

    if (strlen(label) > 0) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(gParamRenderArea, (tRectangle){{rectangle.coord.x, rectangle.coord.y - textHeight}, {BLANK_SIZE, textHeight}}, label);
    }

    for (i = 0; i < range; i++) {
        tRectangle buttonRect = radio_button_rect(groupRect, range, i);
        tRectangle drawnRect  = {0};
        // draw_button() adds the margin back, so this lands at exactly buttonRect.
        tRectangle drawRect   = {
            {buttonRect.coord.x + buttonMargin,        buttonRect.coord.y + buttonMargin       },
            {buttonRect.size.w - (2.0 * buttonMargin), buttonRect.size.h - (2.0 * buttonMargin)}
        };
        bool       selected   = ((uint32_t)paramValue == i);

        // The button being renamed shows the edit buffer with a caret, in place, the same way an
        // Enable button does. Renaming a Channel Select button is the one case where the parameter
        // being edited is not enough to say WHICH box is in edit — hence labelIndex.
        if (  gParamNameEdit.active
           && (gParamNameEdit.moduleKey.slot == module->key.slot)
           && (gParamNameEdit.moduleKey.location == module->key.location)
           && (gParamNameEdit.moduleKey.index == module->key.index)
           && (gParamNameEdit.paramIndex == paramIndex)
           && (gParamNameEdit.labelIndex == i)) {
            char     editBuf[PROTOCOL_PARAM_NAME_SIZE + 2] = {0};
            uint32_t cp                                    = gParamNameEdit.cursorPos;

            memcpy(editBuf, gParamNameEdit.buffer, cp);
            editBuf[cp] = '|';
            memcpy(&editBuf[cp + 1], &gParamNameEdit.buffer[cp], strlen(gParamNameEdit.buffer) - cp + 1);
            drawnRect   = draw_button(gParamRenderArea, drawRect, editBuf, (tRgb)RGB_WHITE);
        } else {
            char caption[PROTOCOL_PARAM_NAME_SIZE + 1] = {0};

            COPY_STRING(caption, radio_caption(module, paramIndex, i, strMap));

            // Truncated to fit rather than allowed to spill over the button's edge and into its
            // neighbour. eNoCache throughout: this is a runtime buffer and the width cache is keyed
            // on the string POINTER, so caching would pin the first caption ever measured.
            while (  (strlen(caption) > 1)
                  && (get_text_width(caption, textHeight, eNoCache) > (drawRect.size.w - RADIO_BUTTON_PADDING))) {
                caption[strlen(caption) - 1] = '\0';
            }
            drawnRect = draw_button(gParamRenderArea, drawRect, caption,
                                    selected ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        }

        // notes §18
        if (i == 0) {
            drawnBounds = drawnRect;
        } else {
            double right  = drawnBounds.coord.x + drawnBounds.size.w;
            double bottom = drawnBounds.coord.y + drawnBounds.size.h;

            if (drawnRect.coord.x < drawnBounds.coord.x) {
                drawnBounds.coord.x = drawnRect.coord.x;
            }

            if (drawnRect.coord.y < drawnBounds.coord.y) {
                drawnBounds.coord.y = drawnRect.coord.y;
            }

            if ((drawnRect.coord.x + drawnRect.size.w) > right) {
                right = drawnRect.coord.x + drawnRect.size.w;
            }

            if ((drawnRect.coord.y + drawnRect.size.h) > bottom) {
                bottom = drawnRect.coord.y + drawnRect.size.h;
            }
            drawnBounds.size.w = right - drawnBounds.coord.x;
            drawnBounds.size.h = bottom - drawnBounds.coord.y;
        }
    }

    // The grid divides the DRAWN bounds the same way it divided the layout rect: the adjustment is a
    // uniform scale and translate, so equal cells stay equal cells. That is what lets the hit test
    // work from this one rectangle.
    return drawnBounds;
}

tRectangle render_paramType1Bypass(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    return draw_power_button(gParamRenderArea, rectangle, paramValue != 0);
}

tRectangle render_paramType1Enable(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap) {
    tRgb       buttonBackgroundColour = RGB_BACKGROUND_GREY;
    tRectangle buttonRect             = {{rectangle.coord.x, rectangle.coord.y}, {rectangle.size.w, STANDARD_BUTTON_TEXT_HEIGHT}};

    if (  gParamNameEdit.active
       && gParamNameEdit.moduleKey.slot == module->key.slot
       && gParamNameEdit.moduleKey.location == module->key.location
       && gParamNameEdit.moduleKey.index == module->key.index
       && gParamNameEdit.paramIndex == paramIndex) {
        char     editBuf[PROTOCOL_PARAM_NAME_SIZE + 2] = {0};
        uint32_t cp                                    = gParamNameEdit.cursorPos;
        memcpy(editBuf, gParamNameEdit.buffer, cp);
        editBuf[cp] = '|';
        memcpy(&editBuf[cp + 1], &gParamNameEdit.buffer[cp], strlen(gParamNameEdit.buffer) - cp + 1);
        return draw_button(gParamRenderArea, buttonRect, editBuf, (tRgb)RGB_WHITE);
    }

    if (paramLocationList[paramRef].colourMap != NULL) {
        buttonBackgroundColour = paramLocationList[paramRef].colourMap[(int)paramValue];
    }
    return draw_button(gParamRenderArea, buttonRect, label, buttonBackgroundColour);
}

#ifdef __cplusplus
}
#endif
