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
#include "defs.h"

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
    moduleTypeEnvADR,
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
    moduleTypeFltPhase,
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
} tModuleType;

typedef enum {
    mouseButtonNone,
    mouseButtonLeftDown,
    mouseButtonLeftUp,
    mouseButtonRightDown,
    mouseButtonRightUp
} tMouseButton;

typedef enum {
    cableColourRed,
    cableColourBlue,
    cableColourYellow,
    cableColourOrange,
    cableColourGreen,
    cableColourPurple,
    cableColourWhite,
    cableColourMax
} tCableColour;

typedef enum {
    patchTypeNoCat,
    patchTypeAcoustic,
    patchTypeSequencer,
    patchTypeBass,
    patchTypeClassic,
    patchTypeDrum,
    patchTypeFantasy,
    patchTypeFx,
    patchTypeLead,
    patchTypeOrgan,
    patchTypePad,
    patchTypePiano,
    patchTypeSynth,
    patchTypeAudioIn,
    patchTypeUser1,
    patchTypeUser2,
    patchTypeUserMax
} tPatchType;

typedef enum {
    monoPolyPoly,
    monoPolyMono,
    monoPolyLegato,
    monoPolyMax
} tMonoPoly;

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
    anchorTopLeft,
    anchorTopMiddle,
    anchorTopRight,
    anchorMiddleLeft,
    anchorMiddleRight,
    anchorMiddle,
    anchorBottomLeft,
    anchorBottomMiddle,
    anchorBottomRight
} tAnchor;

typedef enum {
    labelLocUp,
    labelLocDown,
    labelLocLeft,
    labelLocRight,
} tLabelLoc;

typedef enum {
    paramType1None,
    paramType1Freq,
    paramType1OscFreq,
    paramType1Fine,
    paramType1GeneralFreq,
    paramType1FreqDrum,
    paramType1Int,
    paramType1LFORate,
    paramType1Shape,
    paramType1dB,
    paramType1ADRTime,
    paramType1PulseTime,
    paramType1Time,
    paramType1TimeClk,
    paramType1Resonance,
    paramType1Pitch,
    paramType1Bypass,
    paramType1BipLevel,
    paramType1MixLevel,
    paramType1LevAmpDial,
    paramType1NoteDial,
    paramType1Pan,
    paramType1Slider,
    paramType1CommonDial,
    paramType1LRDial,
    paramType1OscWave,
    paramType1Enable,
    paramType1StrMap,
    paramType1FreqShift,
    paramType1StandardToggle,
} tParamType1;

typedef enum {
    paramType2Dial,
    paramType2Toggle,
    paramType2Menu,
    paramType2Other // Could add button, slider etc.
} tParamType2;

typedef enum {
    volumeTypeNone,
    volumeTypeStereo,
    volumeTypeQuad,
    volumeTypeMono,
    volumeTypeCompress,
    volumeTypeSequencer, // Step-position indicator: 2 bytes in wire stream, not a volume level
} tVolumeType;

typedef enum {
    ledTypeNo,
    ledTypeYes,
    ledTypePark,  // display-only LED: rendered but never counted in LED stream
} tLedType;

typedef enum {
    paramType3Param,
    paramType3Mode
} tParamType3;

typedef enum {
    locationFx,
    locationVa,
    locationMorph,
    locationMax
} tLocation;

typedef struct {
    uint32_t paramRef;
    uint8_t  value;
    uint8_t  morphRange[NUM_MORPHS];   // 8 seems to be number of morphs. Not sure we can go higher, but
    uint8_t  midiCC;                   // MIDI CC number assigned to this param (0 if unassigned)
    bool     hasMidiCC;                // true if a CC is assigned to this param
} tParam;

typedef struct {
    uint32_t   modeRef;
    tRectangle rectangle;
    uint32_t   value;
} tMode;

typedef struct {
    uint32_t   volumeRef;
    tRectangle rectangle;
    uint32_t   value[4];
    //uint32_t   value2;
    //uint32_t   value3;
    //uint32_t   value4;
} tVolume;

typedef struct {
    uint32_t   ledRef;
    tRectangle rectangle;
    uint32_t   value;
} tLed;

typedef struct {
    uint32_t slot;
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

typedef enum {
    connectorTypeAudio,
    connectorTypeControl,
    connectorTypeLogic
} tConnectorType;

typedef enum {
    connectorDirIn,
    connectorDirOut
} tConnectorDir;

typedef enum {
    topbarControlTypeButton,
    topbarControlTypeSpecial,
} tTopbarControlType;

typedef enum {
    // Standard buttons (IDs 0..topbarVariationInitId) — rendered generically from resource table
    topbarVaId = 0,
    topbarFxId,
    topbarOpenReadFileId,
    topbarOpenWriteFileId,
    topbarNewPatchId,
    topbarSlotAId,
    topbarSlotBId,
    topbarSlotCId,
    topbarSlotDId,
    topbarVariation1Id,
    topbarVariation2Id,
    topbarVariation3Id,
    topbarVariation4Id,
    topbarVariation5Id,
    topbarVariation6Id,
    topbarVariation7Id,
    topbarVariation8Id,
    topbarVariationInitId,
    // Special controls — dynamic text or custom rendering
    topbarPatchNameId,
    topbarPatchTypeId,
    topbarMonoPolyId,
    topbarVoiceCountId,
    topbarPatchNotesId,
    topbarSettingsId,
    topbarClockRunStopId,
    topbarTempoDialId,
    topbarPatchVolumeId,
    topbarPerfModeId,
    topbarHideAllCablesId,
    topbarTransparentCablesId,
    topbarCableColourToggle0Id,
    topbarCableColourToggle1Id,
    topbarCableColourToggle2Id,
    topbarCableColourToggle3Id,
    topbarCableColourToggle4Id,
    topbarCableColourToggle5Id,
    topbarCableColourToggle6Id,
    topbarCableColourSelect0Id,
    topbarCableColourSelect1Id,
    topbarCableColourSelect2Id,
    topbarCableColourSelect3Id,
    topbarCableColourSelect4Id,
    topbarCableColourSelect5Id,
    topbarCableColourSelect6Id,
    topbarControlMax
} tTopbarControlId;

typedef struct {
    tRectangle     rectangle;
    tCoord         coord;
    tConnectorDir  dir;  // Should pull from the location list
    tConnectorType type; // Should pull from the location list
} tConnector;

typedef struct {
    uint32_t slot;
    uint32_t location;
    uint32_t index;
} tModuleKey;

typedef struct _struct_module {
    tModuleKey              key;
    tModuleType             type;              // Review this. Is it used!?
    uint32_t                row;
    uint32_t                column;
    tRectangle              dragArea;         // For mouse-click recognition for dragging and menu
    tRectangle              rectangle;        // Full size of module rectangle, in case we need it
    uint32_t                colour;
    uint32_t                upRate;
    uint32_t                newUpRate;           // Only used for mass uprate re-assessing
    uint32_t                isLed;
    uint32_t                unknown1;            // Guess we should store this, to write back if necessary. Might not be needed
    uint32_t                modeCount;           // Don't yet know what this is for. Might need modes array adding
    uint32_t                actualParamCount;
    tMode                   mode[MAX_NUM_MODES]; // Might not need to be an array, since only seeing one mode so far
    char                    name[CLAVIA_NAME_SIZE + 1];
    tParam                  param[NUM_VARIATIONS_USB][MAX_NUM_PARAMETERS];
    uint32_t                paramNumLabels[MAX_NUM_PARAMETERS];
    bool                    paramNameSet[MAX_NUM_PARAMETERS][MAX_NUM_LABELS];
    char                    paramName[MAX_NUM_PARAMETERS][MAX_NUM_LABELS][PROTOCOL_PARAM_NAME_SIZE + 1];
    tConnector              connector[MAX_NUM_CONNECTORS];
    bool                    gotParamIndexCache;
    uint32_t                paramIndexCache;
    bool                    gotModeIndexCache;
    uint32_t                modeIndexCache;
    bool                    gotConnectorIndexCache;
    uint32_t                connectorIndexCache;
    bool                    gotVolumeIndexCache;
    uint32_t                volumeIndexCache;
    bool                    gotLedIndexCache;
    uint32_t                ledIndexCache;
    //uint32_t                volume[2];
    tVolume                 volume;
    tLed                    led;
    //uint32_t                led;
    struct _struct_module * prev;
    struct _struct_module * next;
} tModule;

typedef struct {
    double     xBar;
    bool       xBarDragging;
    double     xGrabOffset;
    tRectangle xThumb;
    double     yBar;
    bool       yBarDragging;
    double     yGrabOffset;
    tRectangle yThumb;
} tScrollState;

typedef struct {
    bool       active;
    tModuleKey moduleKey;
} tModuleDragging;

typedef struct {
    bool        active;
    tModuleKey  moduleKey;
    tParamType3 type3;      // Denotes param or mode, which should maybe be union-ed?
    uint32_t    param;
    uint32_t    mode;
} tParamDragging;

typedef struct {
    bool       active;
    tModuleKey fromModuleKey;
    uint32_t   fromConnectorIndex;
    tConnector toConnector;
} tCableDragging;

typedef struct {
    bool          active;
    uint32_t      slot;
    uint32_t      location;
    uint32_t      moduleIndex;
    uint32_t      ioCount;
    tConnectorDir dir;
} tHoverConnector;

typedef struct _struct_menuItem {
    char *                    label;
    tRgb                      colour;
    void (*action)(int index);
    uint32_t                  param;
    struct _struct_menuItem * subMenu;
} tMenuItem;

typedef struct {
    bool          active;       // Is the menu currently visible?
    tCoord        coord;        // Position of the menu (may be clamped)
    tCoord        originCoord;  // Original click position, unmodified by clamping
    tMenuItem *   items;        // Pointer to an array of menu items
    uint32_t      columns;      // 0 or 1 = single column; >1 = multi-column grid
    double        cellWidth;    // Override cell width when non-zero
    tModuleKey    moduleKey;
    tConnectorDir connectorDir;
    uint32_t      connectorIndex;
    uint32_t      paramIndex;
} tContextMenu;

typedef struct {
    const char *      name;
    const uint32_t    height;
    const tVolumeType volumeType;
    const tLedType    ledType;
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
    const tRectangle  rectangle;
    const tAnchor     anchor;
    const char *      label;
    const uint32_t    range;
    const uint32_t    defaultValue;
    const char **     strMap;
    const tRgb *      colourMap;
} tParamLocation;

typedef struct {
    const tModuleType    moduleType;
    const tConnectorDir  direction;
    const tConnectorType type;
    const tRectangle     rectangle;
    const tAnchor        anchor;
    const char *         label;
    const tLabelLoc      labelLoc;
} tConnectorLocation;

typedef struct {
    const tModuleType moduleType;
    const tParamType1 type1;
    const tParamType2 type2;
    const tRectangle  rectangle;
    const tAnchor     anchor;
    const char *      label;
    const uint32_t    range;
    const uint32_t    defaultValue;
    const char **     strMap;
} tModeLocation;

typedef struct {
    const tModuleType moduleType;
    const tVolumeType volumeType;
    const tRectangle  rectangle;
    const tAnchor     anchor;
} tVolumeLocation;

typedef struct {
    const tModuleType moduleType;
    const tLedType    ledType;
    const tRectangle  rectangle;
    const tAnchor     anchor;
} tLedLocation;

typedef struct {
    tRectangle rectangle;
    tRgb       colour;
    bool       isPressed;
} tTopbarControl;

typedef struct {
    tTopbarControlId   id;
    tCoord             coord;
    tAnchor            anchor;
    const char *       text;
    tRgb               defaultColour;
    tTopbarControlType type;
} tTopbarControlDef;

typedef struct {
    uint32_t unknown1;
    uint32_t unknown2;
    uint8_t  voiceCount;
    uint16_t barPosition;
    uint8_t  unknown3;
    uint8_t  visible[cableColourMax];
    uint8_t  monoPoly;
    uint8_t  activeVariation;
    uint8_t  category;
    uint16_t unknown4;
} tPatchDescr;

typedef struct {
    bool     assigned;
    uint32_t location;
    uint32_t moduleIndex;
    uint32_t isLed;            // 0 = regular param, 1 = LED/button param
    uint32_t paramIndex;
} tKnob;

typedef struct {
    tKnob knob[MAX_NUM_KNOBS];
} tKnobArray;

typedef struct {
    bool     assigned;
    uint32_t location;
    uint32_t moduleIndex;
    uint32_t isLed;
    uint32_t paramIndex;
    uint32_t slotIndex;
} tGlobalKnob;

typedef struct {
    uint32_t location;
    uint32_t moduleIndex;
    uint32_t paramIndex;
} tSelectedParam;

typedef struct {
    uint8_t  midiCC;
    uint32_t location;
    uint32_t moduleIndex;
    uint32_t paramIndex;
} tController;

typedef struct {
    tController controller[MAX_NUM_CONTROLLERS];
} tControllerArray;

typedef enum {
    eCommsNeverConnected,
    eCommsReconnecting,
    eCommsInitialising,
    eCommsOnLine
} tCommsState;

typedef enum {
    ePollNo,
    ePollYes
} tPoll;

typedef struct {
    bool     active;
    char     buffer[CLAVIA_NAME_SIZE + 1];
    uint32_t slot;
} tNameEdit;  // Todo - rename to patch name edit

typedef struct {
    bool       active;
    tModuleKey moduleKey;
    char       buffer[CLAVIA_NAME_SIZE + 1];
} tModuleNameEdit;

typedef struct {
    bool       active;
    tModuleKey moduleKey;
    uint32_t   paramIndex;
    char       buffer[PROTOCOL_PARAM_NAME_SIZE + 1];
} tParamNameEdit;

typedef struct {
    float cycles[locationMax];
    float mem[locationMax];
} tResourceAlloc;

typedef struct {
    bool     active;
    uint32_t slot;
    uint32_t cursorPos;
    char     buffer[PATCH_NOTES_SIZE + 1];
    char     original[PATCH_NOTES_SIZE + 1];
} tPatchNotesEdit;

typedef struct {
    bool     active;
    uint32_t slot;
} tPatchSettingsEdit;

typedef struct {
    char    name[CLAVIA_NAME_SIZE + 1];
    uint8_t midiChanSlot[4];  // 0x00-0x0F = ch 1-16, 0x10 = off
    uint8_t globalChan;       // same encoding
    uint8_t sysexId;          // same encoding
    uint8_t localOn;
    uint8_t progChangeRcv;
    uint8_t progChangeSnd;
    uint8_t controllersRcv;
    uint8_t controllersSnd;
    uint8_t receiveClock;      // 1=off, 0=om
    uint8_t sendClock;         // 0=on, 1=off
    int8_t  tuneCent;          // signed, -50..+50 cents
    uint8_t globalShiftActive;
    int8_t  globalOctaveShift; // signed, -2..+2
    int8_t  tuneSemi;          // signed, -12..+12 semitones
    uint8_t pedalPolarity;
    uint8_t pedalGain;
    uint8_t memoryProtect;
    uint8_t perfMode;
    uint8_t perfBank;
    uint8_t perfLocation;
    //uint8_t vibratoRate; // Comes from a parameter, not synth settings
    uint8_t patchSortMode;
    uint8_t perfSortMode;
} tSynthSettings;

typedef struct {
    tRectangle close;
    tRectangle midiChan[4];
    tRectangle globalChan;
    tRectangle sysexId;
    tRectangle localOn;
    tRectangle memoryProtect;
    tRectangle progChangeRcv;
    tRectangle progChangeSnd;
    tRectangle controllersRcv;
    tRectangle controllersSnd;
    tRectangle sendClock;
    tRectangle receiveClock;
    tRectangle tuneCent;
    tRectangle tuneSemi;
    tRectangle globalShiftActive;
    tRectangle globalOctaveShift;
    tRectangle pedalPolarity;
    tRectangle pedalGain;
    tRectangle patchSortMode;
    tRectangle perfSortMode;
    tRectangle synthName;
} tSettingsPanelRects;

typedef enum {
    eNoCache,
    eCache,
} tCache;

#endif // __TYPES_H__

