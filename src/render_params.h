//
//  render_params.h
//  G2-edit
//
//  Created by Øyvind Jacobsen Bjørkås on 25/06/2025.
//

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



#endif // __RENDER_PARAMS_H__
