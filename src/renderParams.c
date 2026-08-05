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

// Which drawing area every param widget below renders into. The patch canvas draws through
// moduleArea (canvas zoom + scroll applied); panels that reuse these same widgets - the
// Parameter Pages panel - draw into mainArea instead. It's a mode set around the call rather
// than a parameter, so that reusing the widgets elsewhere doesn't mean threading an extra
// argument through all ~30 renderer signatures and their function-pointer types in
// render_param_common().
static tArea gParamRenderArea = moduleArea;

void set_param_render_area(tArea area) {
    gParamRenderArea = area;
}

// The oscillator dial curves, kept apart from the renderers that print them so the sound engine
// can share them. See renderParams.h for why. Nothing here touches global state, so both the
// UI thread and the audio thread's parameter snapshot can call them freely.

int osc_pitch_type_param_index(tModule * module) {
    switch (module->type) {
        case moduleTypeOscB:
        case moduleTypeResonator:
        case moduleTypeOscShpB:
        case moduleTypeOscString:
        case moduleTypeOscNoise:
        case moduleTypeOscShpA:
        case moduleTypeOscDual:
        {
            return 4;
        }
        case moduleTypeOscMaster:
        case moduleTypeOscC:
        case moduleTypeOscPM:
        {
            return 3;
        }
        case moduleTypeOscPerc:
        {
            return 2;
        }
        case moduleTypeOscA:
        {
            return 6;
        }
        default:
        {
            return -1;
        }
    }
}

double osc_freq_semitones(double paramValue) {
    if (paramValue >= 127.0) {
        return 63.0;    // Clip - the dial's top step is +63, not +64
    }
    return paramValue - 64.0;
}

double osc_freq_hz(double paramValue) {
    double minFreq = 8.1758;    // A concert-pitch C-1
    double maxFreq = 12550.0;

    return exp(paramValue / 127.0 * log(maxFreq / minFreq)) * minFreq;
}

double osc_freq_factor(double paramValue) {
    double minFactor = 0.0248;
    double maxFactor = 38.072;

    return exp(paramValue / 127.0 * log(maxFactor / minFactor)) * minFactor;
}

double osc_fine_cents(double paramValue) {
    return (paramValue - 64.0) / 64.0 * 50.0;
}

double osc_shape_percent(double paramValue) {
    return paramValue * 49.0 / 127.0 + 50.0;
}

// 13.75 Hz is A-1, so like the oscillators' Tune the filter's Freq dial is really a pitch — its
// value counts semitones up from there, reaching about 21 kHz at 127 (manual: "13.76 Hz to 21.1 kHz").
double flt_cutoff_hz(double paramValue) {
    return 13.75 * pow(2.0, paramValue / 12.0);
}

// 0.5 at the bottom of the dial up to 50 at the top, which is the range filter_resonanceStrMap
// prints.
double flt_resonance_q(double paramValue) {
    int    index = (int)paramValue;
    double value = 0.0;

    if (index < 0) {
        index = 0;
    } else if (index > 127) {
        index = 127;
    }

    // Read the dial's own table. It was an exponential between the endpoints, which matched at 0.5
    // and 50 and was wrong by as much as 245% in between — the table reads Q 3.16 where the curve
    // gave 10.9 — so the engine had far more resonance than the module was showing across most of
    // the knob's travel. The Res dial is a paramTypeStrMap, i.e. it PRINTS this table, so the two
    // were visibly disagreeing.
    if (filter_resonanceStrMap[index] != NULL) {
        value = atof(filter_resonanceStrMap[index]);
    }
    return (value > 0.0) ? value : 0.5;
}

// The dB scroll button selects how many one-pole stages sit on top of the base two: 12, 18 or 24 dB
// per octave.
uint32_t flt_slope_extra_poles(uint32_t slopeValue) {
    return (slopeValue > 2) ? 2 : slopeValue;
}

// The Kbt scroll button is Off / 25% / 50% / 75% / 100% keyboard tracking (manual p.196).
double flt_kbt_amount(uint32_t kbtValue) {
    return (kbtValue > 4) ? 1.0 : ((double)kbtValue * 0.25);
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

// An LFO's speed in Hz, for a given Range setting. The four continuous ranges are exponential
// sweeps between the endpoints the dial displays; BPM is beats per minute, so a sixth of a hertz per
// unit. Shared with the sound engine so the rate heard and the rate shown cannot drift apart.
//
// ClkSync needs the patch's master clock, which the engine has no notion of, so it falls back to the
// slow end of Rate Lo rather than pretending to be in time with something.
double lfo_rate_hz(uint32_t rangeMode, double paramValue) {
    switch (rangeMode) {
        case 0:   // Rate Sub, expressed as a period of 699 s down to 5.46 s
        {
            return 1.0 / (699.0 * exp(paramValue * log(5.46 / 699.0) / 127.0));
        }
        case 1:   // Rate Lo, a period of 62.9 s down to 0.04098 s (24.4 Hz)
        {
            return 1.0 / (62.9 * exp(paramValue * log(0.04098 / 62.9) / 127.0));
        }
        case 2:   // Rate Hi, stated directly in Hz
        {
            return 0.26 * exp(paramValue * log(392.0 / 0.26) / 127.0);
        }
        case 3:   // BPM
        {
            double bpm = (paramValue < 33.0) ? (24.0 + round(2.0 * paramValue))
                         : (paramValue < 97.0) ? (56.0 + round(paramValue))
                         : (154.0 + round(2.0 * (paramValue - 97.0)));

            return bpm / 60.0;
        }
        default:
        {
            return 1.0;
        }
    }
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
        snprintf(buff, buffSize, "%s", dbLvlStrMap[(int)paramValue]);
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
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
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
    return render_dial_with_text(gParamRenderArea, rectangle, (char *)paramLocationList[paramRef].label, buff, (double)STANDARD_BUTTON_TEXT_HEIGHT, paramValue, paramLocationList[paramRef].range, morphRange, colour);
}

tRectangle render_paramType1PulseTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    double   time_to_display;
    uint32_t slot      = module->key.slot;
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
    // PartQuant Range — bipolar partial count. Per the original editor's
    // ParamText::Sym: value = raw - 64 (raw 64 = 0 centre, 127 = +63, 0 = -64),
    // with a leading '+' on positives. The manual (PARTQUANT / RANGE KNOB) adds a
    // '*' once the magnitude exceeds +/-32, flagging that the practical output
    // limit (the 32nd harmonic) has been passed.
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
    // 0 to 64 'units' level, per the original editor's ParamText::UniPol: raw/2
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

// LevAmp's amplification, as a multiplier. The manual (p.227) gives the range as "0.25 to 4.0 times
// the input level"; the dial walks that range exponentially, passing through unity at 64. Shared
// with the sound engine so what is heard and what the dial reads cannot drift apart.
double lev_amp_gain(double paramValue) {
    return exp(paramValue * 0.0218) * 0.25;
}

tRectangle render_paramType1LevAmpDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef) {
    // 0.25 to 4.0
    double lev = round(lev_amp_gain((double)paramValue) * 100.0) / 100.0;

    snprintf(buff, buffSize, "%.2fx", lev);

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
    double res    = 0.0;
    double maxVal = 100.0;

    if (paramValue < 127) {
        res = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        res = maxVal;             // Clip
    }
    snprintf(buff, buffSize, "%.1f", res);
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

        if (hasBipolar && module->param[variation][bipParamIdx].value == 0) {
            int displayValue = (int)paramValue - 64;

            if (displayValue == 0) {
                snprintf(buff, buffSize, "0");
            } else {
                snprintf(buff, buffSize, "%+d", displayValue);
            }
        } else {
            snprintf(buff, buffSize, "%u", (uint32_t)paramValue);

            // Unipolar SeqVal/SeqLev/SeqCtr values of 100-127 are the only 3-digit case here
            if (hasBipolar && buff[0] == '1' && buff[1] >= '0' && buff[1] <= '9' && buff[2] >= '0' && buff[2] <= '9' && buff[3] == '\0') {
                twoRow       = true;
                topStr[0]    = buff[0];
                bottomStr[0] = buff[1];
                bottomStr[1] = buff[2];
            }
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

    // BUTTON-ANCHORED, the same rule render_dial_with_text() follows: the rectangle IS the button,
    // and the label is drawn in the row ABOVE it. It used to be the other way round - the label at
    // the rectangle and the button pushed a row below it - which made the button's position depend
    // on whether the param happened to have a label, exactly the problem the dials had. It also
    // moved the button whenever a patch RENAMED the param, since that name arrives at runtime.
    if (strlen(label) > 0) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(gParamRenderArea, (tRectangle){{rectangle.coord.x, y - textHeight}, {BLANK_SIZE, textHeight}}, label);
    }

    if (paramLocationList[paramRef].colourMap != NULL) {
        buttonBackgroundColour = paramLocationList[paramRef].colourMap[(int)paramValue];
    }
    return draw_button(gParamRenderArea, (tRectangle){{rectangle.coord.x, y}, {largest_text_width(paramLocationList[paramRef].range, strMap, textHeight, eCache), textHeight}}, strMap[(int)paramValue], buttonBackgroundColour);
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
