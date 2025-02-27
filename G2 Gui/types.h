/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
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


#ifndef __TYPES_H__
#define __TYPES_H__

#include "sysIncludes.h"

typedef struct {
    uint8_t * buffPtr;
    uint32_t  buffSize;
} tString;

typedef struct {
    double red;
    double green;
    double blue;
} tRgb;

typedef struct {
    double red;
    double green;
    double blue;
    double alpha;
} tRgba;

typedef struct {
    double x;
    double y;
} tCoord;

typedef struct {
    double w;
    double h;
} tSize;

typedef struct {
    tCoord coord;
    tSize  size;
} tRectangle;

typedef struct {
    int32_t x;
    int32_t y;
} tIntCoord;

typedef struct {
    int32_t w;
    int32_t h;
} tIntSize;

typedef struct {
    tIntCoord coord;
    tIntSize  size;
} tIntRectangle;

typedef struct {
    tCoord coord1;
    tCoord coord2rel;
    tCoord coord3rel;
} tTriangle;

typedef struct {
    double xBar;
    bool   xBarDragging;
    double yBar;
    bool   yBarDragging;
} tScrollState;

typedef struct {
    bool     active;
    uint32_t location;     //Todo = replace location and index with tModuleKey
    uint32_t index;
    uint32_t variation;
    uint32_t param;
} tDragging;

typedef enum {
    moduleTypeUnknown0,
    moduleTypeKeyboard,
    moduleTypeUnknown2,
    moduleType4toOut,
    moduleType2toOut,
    moduleTypeInvert,
    moduleTypeUnknown6,
    moduleTypeOscB,
    moduleTypeOscShpB,
    moduleTypeOscC,
    moduleTypeUnknown10,
    moduleTypeUnknown11,
    moduleTypeReverb,
    moduleTypeOscString,
    moduleTypeUnknown14,
    moduleTypeSw8to1,
    moduleTypeUnknown16,
    moduleTypeValSw1to2,
    moduleTypeXtoFade,
    moduleTypeMix4to1B,
    moduleTypeEnvADSR,
    moduleTypeMux1to8,
    moduleTypePartQuant,
    moduleTypeModADSR,
    moduleTypeLfoC,
    moduleTypeLfoShpA,
    moduleTypeLfoA,
    moduleTypeOscMaster,
    moduleTypeSaturate,
    moduleTypeMetNoise,
    moduleTypeDevice,
    moduleTypeNoise,
    moduleTypeEq2Band,
    moduleTypeEq3band,
    moduleTypeShpExp,
    moduleTypeDriver,
    moduleTypeSwOnOffM,
    moduleTypeUnknown37,
    moduleTypePulse,
    moduleTypeUnknown39,
    moduleTypeMix8to1B,
    moduleTypeEnvH,
    moduleTypeDelay,
    moduleTypeConstant,
    moduleTypeLevMult,
    moduleTypeFltVoice,
    moduleTypeEnvAHD,
    moduleTypePan,
    moduleTypeMixStereo,
    moduleTypeFltMulti,
    moduleTypeConstSwT,
    moduleTypeFltNord,
    moduleTypeEnvMulti,
    moduleTypeSandH,
    moduleTypeFltStatic,
    moduleTypeEnvD,
    moduleTypeResonator,
    moduleTypeAutomate,
    moduleTypeDrumSynth,
    moduleTypeCompLev,
    moduleTypeMux8to1X,
    moduleTypeClip,
    moduleTypeOverdrive,
    moduleTypeScratch,
    moduleTypeGate,
    moduleTypeUnknown65,
    moduleTypeMix2to1B,
    moduleTypeUnknown67,
    moduleTypeClkGen,
    moduleTypeClkDiv,
    moduleTypeUnknown70,
    moduleTypeEnvFollow,
    moduleTypeNoteScaler,
    moduleTypeUnknown73,
    moduleTypeWaveWrap,
    moduleTypeNoteQuant,
    moduleTypeSwOnOffT,
    moduleTypeUnknown77,
    moduleTypeSw1to8,
    moduleTypeSw4to1,
    moduleTypeUnknown80,
    moduleTypeLevAmp,
    moduleTypeRect,
    moduleTypeShpStatic,
    moduleTypeEnvAD,
    moduleTypeWindSw,
    moduleType8Counter,
    moduleTypeFltLP,
    moduleTypeSw1to4,
    moduleTypeFlanger,
    moduleTypeSw1to2,
    moduleTypeFlipFlop,
    moduleTypeFltClassic,
    moduleTypeUnknown93,
    moduleTypeStChorus,
    moduleTypeUnknown95,
    moduleTypeOscD,
    moduleTypeOscA,
    moduleTypeFreqShift,
    moduleTypeUnknown99,
    moduleTypeSw2to1,
    moduleTypeUnknown101,
    moduleType1FltPhase,
    moduleTypeEqPeak,
    moduleTypeUnknown104,
    moduleTypeValSw2to1,
    moduleTypeOscNoise,
    moduleTypeUnknown107,
    moduleTypeVocoder,
    moduleTypeUnknown109,
    moduleTypeUnknown110,
    moduleTypeUnknown111,
    moduleTypeLevAdd,
    moduleTypeFade1to2,
    moduleTypeFade2to1,
    moduleTypeLevScaler,
    moduleTypeMix8to1A,
    moduleTypeLevMod,
    moduleTypeDigitizer,
    moduleTypeEnvADDSR,
    moduleTypeUnknown120,
    moduleTypeSeqNote,
    moduleTypeUnknown122,
    moduleTypeMix4to1C,
    moduleTypeMux8to1,
    moduleTypeWahWah,
    moduleTypeName,
    moduleTypeFxtoIn,
    moduleTypeMinMax,
    moduleTypeUnknown129,
    moduleTypeBinCounter,
    moduleTypeADConv,
    moduleTypeDAConv,
    moduleTypeUnknown133,
    moduleTypeFltHP,
    moduleTypeUnknown135,
    moduleTypeUnknown136,
    moduleTypeUnknown137,
    moduleTypeUnknown138,
    moduleTypeTandH,
    moduleTypeMix4to1S,
    moduleTypeCtrlSend,
    moduleTypePCSend,
    moduleTypeNoteSend,
    moduleTypeSeqEvent,
    moduleTypeSeqVal,
    moduleTypeSeqLev,
    moduleTypeCtrlRcv,
    moduleTypeNoteRcv,
    moduleTypeNoteZone,
    moduleTypeCompress,
    moduleTypeUnknown151,
    moduleTypeKeyQuant,
    moduleTypeUnknown153,
    moduleTypeSeqCtr,
    moduleTypeUnknown155,
    moduleTypeNoteDet,
    moduleTypeLevConv,
    moduleTypeGlide,
    moduleTypeCompSig,
    moduleTypeZeroCnt,
    moduleTypeMixFader,
    moduleTypeFltComb,
    moduleTypeOscShpA,
    moduleTypeOscDual,
    moduleTypeDXRouter,
    moduleTypeUnknown166,
    moduleTypePShift,
    moduleTypeUnknown168,
    moduleTypeModAHD,
    moduleType2toIn,
    moduleType4toIn,
    moduleTypeDlySingleA,
    moduleTypeDlySingleB,
    moduleTypeDelayDual,
    moduleTypeDelayQuad,
    moduleTypeDelayA,
    moduleTypeDelayB,
    moduleTypeDlyClock,
    moduleTypeDlyShiftReg,
    moduleTypeOperator,
    moduleTypeDlyEight,
    moduleTypeDlyStereo,
    moduleTypeOscPM,
    moduleTypeMix1to1A,
    moduleTypeMix1to1S,
    moduleTypeSw1to2M,
    moduleTypeSw2to1M,
    moduleTypeConstSwM,
    moduleTypeNoiseGate,
    moduleTypeLfoB,
    moduleTypeUnknown191,
    moduleTypePhaser,
    moduleTypeMix4to1A,
    moduleTypeMix2to1A,
    moduleTypeModAmt,
    moduleTypeOscPerc,
    moduleTypeStatus,
    moduleTypePitchTrack,
    moduleTypeMonoKey,
    moduleTypeRandomA,
    moduleTypeRed2Blue,
    moduleTypeRandomB,
    moduleTypeBlue2Red,
    moduleTypeRndClkA,
    moduleTypeRndTrig,
    moduleTypeRndClkB,
    moduleTypeUnknown207,
    moduleTypeRndPattern,
    moduleTypeMax
} tmoduleType;

typedef enum {
    mainArea,
    moduleArea
} tArea;

#endif // __TYPES_H__