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

#ifndef __TYPES_H__
#define __TYPES_H__

#include "sysIncludes.h"

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
    moduleTypeRndPattern
} tModuleType;

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

typedef enum {
    paramType1None,
    paramType1Freq,
    paramType1Resonance,
    paramType1Pitch,
    paramType1PitchType,
    paramType1FmType,
    paramType1OffTo100KeyboardTrack,
    paramType1OffOnKeyboardTrack,
    paramType1GainControl,
    paramType1Bypass,
    paramType1FltClassicDb,
    paramType1FltMultiDb,
    paramType1CommonDial,
    paramType1OscWave
} tParamType1;

typedef enum {
    paramType2Dial,
    paramType2Toggle,
    paramType2Other // Could add button, slider etc.
} tParamType2;

typedef enum {
    paramType3Param,
    paramType3Mode
} tParamType3;

typedef enum {
    locationFx = 0,
    locationVa = 1
} tLocation;

typedef struct {
    uint32_t   paramRef;
    tRectangle rectangle;
    uint32_t   value;
} tParam;

typedef struct {
    uint32_t location;
    uint32_t moduleFromIndex;
    uint32_t connectorFromIoCount;
    uint32_t linkType;
    uint32_t moduleToIndex;
    uint32_t connectorToIoCount;
} tCableKey;

typedef struct _struct_cable {
    tCableKey              key;
    uint32_t               colour;
    struct _struct_cable * prev;
    struct _struct_cable * next;       // This can go, when we attach to modules rather than separate linked list
} tCable;

typedef enum {   // Todo: add a new type for e.g. yellow gate
    connectorTypeAudio,
    connectorTypeControl
} tConnectorType;

typedef enum {
    connectorDirIn,
    connectorDirOut
} tConnectorDir;

typedef struct {
    tConnectorDir  dir;
    tConnectorType type;
    tCoord         coord;
    tRectangle     rectangle;
} tConnector;

typedef struct {
    uint32_t location;
    uint32_t index;
} tModuleKey;

typedef struct _struct_module {
    tModuleKey              key;
    tModuleType             type;              // Review this. Is it used!?
    uint32_t                row;
    uint32_t                column;
    tRectangle              rectangle;                                // Purely for mouse-click recognition
    uint32_t                colour;
    uint32_t                upRate;
    uint32_t                newUpRate; // Only used for mass uprate re-assessing
    uint32_t                isLed;
    uint32_t                unknown1;  // Guess we should store this, to write back if necessary. Might not be needed
    uint32_t                modeCount; // Don't yet know what this is for. Might need modes array adding
    tParam                  mode[NUM_MODES];
    char                    name[MODULE_NAME_SIZE + 1];
    uint32_t                allocatedParams;
    tParam *                param[VARIATIONS];
    uint32_t                allocatedConnectors;
    tConnector *            connector;
    bool                    gotParamIndexCache;
    uint32_t                paramIndexCache;
    bool                    gotModeIndexCache;
    uint32_t                modeIndexCache;
    bool                    gotConnectorIndexCache;
    uint32_t                connectorIndexCache;
    struct _struct_module * prev;
    struct _struct_module * next;
} tModule;

typedef struct {
    double xBar;
    bool   xBarDragging;
    double yBar;
    bool   yBarDragging;
} tScrollState;

typedef struct {
    bool       active;
    tModuleKey moduleKey;
} tModuleDragging;     // Parameter value dragging - Todo: rename

typedef struct {
    bool        active;
    tModuleKey  moduleKey;
    tParamType3 type3;
    uint32_t    variation;
    uint32_t    param;
    uint32_t    mode;
} tDialDragging;     // Parameter value dragging - Todo: rename

typedef struct {
    bool       active;
    tModuleKey fromModuleKey;
    uint32_t   fromConnectorIndex;
    tConnector toConnector;
} tCableDragging;

typedef struct _struct_menuItem {
    char *                    label;
    void (*action)(int index);
    uint32_t                  param;
    struct _struct_menuItem * subMenu;
} tMenuItem;

typedef struct {
    bool          active; // Is the menu currently visible?
    tCoord        coord;  // Position of the menu
    tMenuItem *   items;  // Pointer to an array of menu items
    tModuleKey    moduleKey;
    tConnectorDir connectorDir;
    uint32_t      connectorIndex;
} tContextMenu;

typedef struct {
    const char *   name;
    const uint32_t height;
    const uint32_t numParameters;
    const uint32_t numConnectors;
    const uint32_t modeCount;
} tModuleProperties;

typedef enum {
    mainArea,
    moduleArea
} tArea;

typedef enum {
    cableLinkTypeFromInput,
    cableLinkTypeFromOutput
} tCableLinkType;

typedef enum {
    doFreeYes,
    doFreeNo
} tDoFree;

typedef struct {
    double u1;         // Top-left texture coordinates
    double v1;
    double u2;         // Bottom-right texture coordinates
    double v2;
    double advance_x;  // Horizontal advance
    int    width;      // Width and height of the glyph
    int    height;
    int    offset_x;   // Offset from the origin
    int    offset_y;
} GlyphInfo;

typedef struct {
    const tModuleType moduleType;
    const tParamType1 type;
    const tCoord      coord;
} tConstParameter;

typedef struct {
    const tModuleType    moduleType;
    const tConnectorDir  dir;
    const tConnectorType type;
    const tCoord         coord;
} tConstConnector;

typedef struct {
    const tModuleType moduleType;
    const tParamType1 type1;
    const tParamType2 type2;
    const double      offsetX;
    const double      offsetY;
    const char *      label;
    const uint32_t    range;
    const uint32_t    defaultValue;
    const char **     strMap;
} tParamLocation;

typedef struct {
    const tModuleType    moduleType;
    const tConnectorDir  direction;
    const tConnectorType type;
    const double         offsetX;
    const double         offsetY;
} tConnectorLocation;

typedef struct {
    tRectangle rectangle;
    char *     text;
    void (*function)(void);
} tButton;

#endif // __TYPES_H__
