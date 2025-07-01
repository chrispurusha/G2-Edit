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

#ifndef __RENDER_PARAMS_H__
#define __RENDER_PARAMS_H__

#include "sysIncludes.h"
#include "types.h"

tRectangle render_paramType1Freq(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1OscFreq(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Fine(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1GeneralFreq(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Shape(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1FreqDrum(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1LFORate(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Int(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1dB(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1MixLevel(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Time(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1TimeClk(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1ADRTime(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1PulseTime(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Pitch(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1BipLevel(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1LevAmpDial(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Pan(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1NoteDial(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Resonance(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);

tRectangle render_paramType1StandardToggle(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1UpDown(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1Bypass(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1Enable(tModule * module, tRectangle rectangle, char* label, char* buff, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);



#endif // __RENDER_PARAMS_H__
