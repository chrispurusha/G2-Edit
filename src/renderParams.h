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

#ifndef __RENDER_PARAMS_H__
#define __RENDER_PARAMS_H__

#include "sysIncludes.h"
#include "types.h"
#include "synthlibTypes.h"
#include "paramCurves.h"

// Selects the drawing area the widgets below render into: moduleArea (the default) for the
// scrolled/zoomed patch canvas, mainArea for a panel reusing the same widgets. Set it around
// the panel's own render pass and put it back afterwards.
void set_param_render_area(tArea area);


tRectangle render_paramType1Freq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1OscFreq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Fine(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1GeneralFreq(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1LfoShape(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Shape(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1FreqDrum(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1LFORate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Int(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1dB(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1MixLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Time(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1TimeClk(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1ADRTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1PulseTime(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Pitch(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1BipLevel(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Partials(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1UniPol(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1UniPolShort(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1LevAmpDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1PShiftSemi(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1BipolarPinned(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1PlusMinusUnits(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1OffNum(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1ScratchRatio(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1SampleRate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1ThresholdDb(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Bipolar(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1ResonanceQ(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1FlangerRate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1PhaserRate(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Swing(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Bandwidth(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Phase(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Pan(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1NoteDial(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Resonance(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1Slider(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1StrMap(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);
tRectangle render_paramType1FreqShift(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphRange, tRgb colour, uint32_t paramRef);

tRectangle render_paramType1StandardToggle(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1Bypass(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1Enable(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
tRectangle render_paramType1RadioEdit(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
// Where button `buttonIndex` of a Channel Select group drew, given where the group as a whole drew.
// The renderer and the hit test have to agree on this, so they share it rather than each doing the
// arithmetic; buttons are uniform, so the group rectangle is all it takes.
tRectangle radio_button_rect(tRectangle groupRect, uint32_t buttonCount, uint32_t buttonIndex);
int32_t radio_button_at(tRectangle groupRect, uint32_t buttonCount, tCoord coord);


#endif // __RENDER_PARAMS_H__
