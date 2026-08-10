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

#ifndef __TYPES_H__
#define __TYPES_H__

#include "sysIncludes.h"
#include "defs.h"
#include "geometry.h"

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
    paramTypeNone,
    paramTypeFreq,
    paramTypeOscFreq,
    paramTypeFine,
    paramTypeGeneralFreq,
    paramTypeFreqDrum,
    paramTypeInt,
    paramTypeLFORate,
    paramTypeShape,
    paramTypedB,
    paramTypeADRTime,
    paramTypePulseTime,
    paramTypeTime,
    paramTypeTimeClk,
    paramTypeResonance,
    paramTypePitch,
    paramTypeBypass,
    paramTypeBipLevel,
    paramTypePartials,   // PartQuant Range: bipolar partial count -64..+63 (0 centre), '*' when |v| > 32
    paramTypeUniPol,     // 0 to 64 'units' level (raw/2 as N.0/N.5, 127 = 64.0) — e.g. EnvADSR Sustain
    paramTypeMixLevel,
    paramTypeLevAmpDial,
    paramTypeNoteDial,
    paramTypePan,
    paramTypePhase,
    paramTypeBipolar,
    paramTypePShiftSemi,
    paramTypeBipolarPinned,
    paramTypePlusMinusUnits,
    paramTypeOffNum,
    paramTypeScratchRatio,
    paramTypeSampleRate,
    paramTypeThresholdDb,

    paramTypeResonanceQ,
    paramTypeFlangerRate,
    paramTypePhaserRate,
    paramTypeSwing,
    paramTypeBandwidth,
    paramTypeSlider,
    paramTypeCommonDial,
    paramTypeLRDial,
    paramTypeOscWave,
    paramTypeEnable,
    paramTypePush,
    paramTypeStrMap,
    paramTypeFreqShift,
    paramTypeToggle,
    paramTypeMenu,
    paramTypeCustomData, // value lives in param[] like other params; sent via custom data protocol (0x42)
} tParamType;

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

// Module indices within the patch settings location (locationMorph, location 2)
typedef enum {
    patchModuleMorph = 1,
    patchModuleVolume,
    patchModuleGlide,
    patchModuleBend,
    patchModuleVibrato,
    patchModuleArpeggiator,
    patchModuleSustain,
    patchModuleMasterClock,
    patchModuleVoices
} tPatchModuleIndex;

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
    uint32_t         ledRef;
    tRectangle       rectangle;
    _Atomic uint32_t value;
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

typedef struct {
    tCableKey key;
    uint32_t  colour;
    bool      active;
} tCable;

typedef enum {
    connectorTypeAudio,
    connectorTypeControl,
    connectorTypeLogic,
    connectorTypeTurboLogic // Logic connector on an up-rated module — orange, not plain yellow (see
                            // the upRate promotion in render_connector_common(), moduleGraphics.cpp)
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
#define TOPBAR_COL(r, \
                   g, \
                   b)    /* colour ignored when building enum */
#define TOPBAR_COL_BG_GREY     TOPBAR_COL(0.8, \
                                          0.8, \
                                          0.8)
#define TOPBAR_COL_GREEN_ON    TOPBAR_COL(0.3, \
                                          0.7, \
                                          0.3)
#define TOPBAR_COL_BLACK       TOPBAR_COL(0.0, \
                                          0.0, \
                                          0.0)
#define X(id, \
          ...)    id,
#include "topbarControls.def"
#undef X
#undef TOPBAR_COL_BLACK
#undef TOPBAR_COL_GREEN_ON
#undef TOPBAR_COL_BG_GREY
#undef TOPBAR_COL
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


typedef struct {
    bool        active;
    tModuleKey  key;
    tModuleType type;                          // Review this. Is it used!?
    uint32_t    row;
    uint32_t    column;
    tRectangle  dragArea;                     // For mouse-click recognition for dragging and menu
    tRectangle  rectangle;                    // Full size of module rectangle, in case we need it
    uint32_t    colour;
    uint32_t    upRate;
    uint32_t    newUpRate;                       // Only used for mass uprate re-assessing
    uint32_t    excludeFromMutation;             // Patch Mutator "Exclude From Mutation" flag (wire bit long
                                                 // mislabeled isLed - confirmed against decompiled original
                                                 // editor source and real captured patches, see mutator.c)
    uint32_t    unknown1;                        // Guess we should store this, to write back if necessary. Might not be needed
    uint32_t    modeCount;                       // Don't yet know what this is for. Might need modes array adding
    uint32_t    actualParamCount;
    tMode       mode[MAX_NUM_MODES];             // Might not need to be an array, since only seeing one mode so far
    char        name[CLAVIA_NAME_SIZE + 1];
    tParam      param[NUM_VARIATIONS_USB][MAX_NUM_PARAMETERS];
    uint32_t    paramNumLabels[MAX_NUM_PARAMETERS];
    bool        paramNameSet[MAX_NUM_PARAMETERS][MAX_NUM_LABELS];
    char        paramName[MAX_NUM_PARAMETERS][MAX_NUM_LABELS][PROTOCOL_PARAM_NAME_SIZE + 1];
    tConnector  connector[MAX_NUM_CONNECTORS];
    bool        gotParamIndexCache;
    uint32_t    paramIndexCache;
    bool        gotModeIndexCache;
    uint32_t    modeIndexCache;
    bool        gotConnectorIndexCache;
    uint32_t    connectorIndexCache;
    bool        gotVolumeIndexCache;
    uint32_t    volumeIndexCache;
    bool        gotLedIndexCache;
    uint32_t    ledIndexCache;
    //uint32_t                volume[2];
    tVolume     volume;
    tLed        led;
} tModule;

typedef struct {
    bool       active;
    tModuleKey moduleKey;
    bool       isMulti;
    uint32_t   prevColumn;
    uint32_t   prevRow;
    uint32_t   snapshotCount;
    tModuleKey snapshotKeys[MAX_NUM_MODULES];
    uint32_t   snapshotColumn[MAX_NUM_MODULES];
    uint32_t   snapshotRow[MAX_NUM_MODULES];
} tModuleDragging;

typedef struct {
    tModuleKey keys[MAX_NUM_MODULES];
    uint32_t   count;
} tSelection;

typedef struct {
    bool   active;
    tCoord start;    // module-space coordinates
    tCoord current;  // module-space coordinates
} tRubberBand;

typedef struct {
    bool        active;
    tModuleKey  moduleKey;
    tParamType3 type3;           // Denotes param or mode, which should maybe be union-ed?
    uint32_t    param;
    uint32_t    mode;
    uint32_t    startValue;      // value at drag-start, for undo
    uint32_t    startMorphRange; // morphRange[gMorphGroupFocus] at drag-start, so an Alt-drag
                                 // continues from the existing offset instead of resetting it

    // SUB-UNIT REMAINDER CARRIED BETWEEN MOUSE-MOVE EVENTS, and the reason slow dragging used to do
    // nothing at all. An incremental drag converts the movement since the PREVIOUS event into whole
    // parameter units and then advances its reference point — so a movement worth less than one unit
    // truncated to zero and was thrown away, every event, no matter how far the pointer travelled in
    // total. Moving slowly therefore changed nothing, and a fine (Shift) drag changed nothing at all,
    // because dividing by ten times as many pixels makes almost every event sub-unit.
    //
    // Keeping the fraction here and adding it to the next event's makes a slow drag advance smoothly
    // instead of not at all. It lives in this struct rather than as a file static so it is zeroed for
    // free by the memset that arms every drag — a leftover fraction from the last drag would otherwise
    // be spent on the first event of the next one.
    double unitAccum;
} tParamDragging;

// The parameter last clicked on the canvas. The original editor's MIDI Learn is "click a knob, then
// press L", so the L key needs a target that outlives the click — gParamDragging is cleared on
// mouse-up and only ever covers the draggable param types anyway.
typedef struct {
    bool       valid;
    tModuleKey moduleKey;
    uint32_t   paramIndex;
} tParamFocus;

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

// tMenuItem/tMenuFrame/tContextMenu now live in SynthLib's synthlibTypes.h
// (pulled in transitively via geometry.h below) — the mechanism itself is
// generic over any app. See tMenuContext further down for the G2-Edit-only
// state (moduleKey/paramIndex/etc) that this app's action callbacks need.

typedef struct {
    const char *      name;
    const uint32_t    height;
    const tVolumeType volumeType;
    const tLedType    ledType;
} tModuleProperties;

typedef enum {
    cableLinkTypeFromInput,
    cableLinkTypeFromOutput
} tCableLinkType;

typedef enum {
    doFreeYes,
    doFreeNo
} tDoFree;

typedef struct {
    tModuleType type;
    int32_t     dColumn;
    int32_t     dRow;
    uint32_t    origIndex;
    uint32_t    origColumn;
    uint32_t    origRow;
    uint32_t    colour;
    uint32_t    upRate;
    uint32_t    excludeFromMutation;
    char        name[CLAVIA_NAME_SIZE + 1];
    tParam      param[NUM_VARIATIONS_USB][MAX_NUM_PARAMETERS];
    uint32_t    mode[MAX_NUM_MODES];
    uint32_t    paramNumLabels[MAX_NUM_PARAMETERS];
    bool        paramNameSet[MAX_NUM_PARAMETERS][MAX_NUM_LABELS];
    char        paramName[MAX_NUM_PARAMETERS][MAX_NUM_LABELS][PROTOCOL_PARAM_NAME_SIZE + 1];
} tClipboardModule;

typedef struct {
    uint32_t fromOrigIndex;
    uint32_t fromIoCount;
    uint32_t toOrigIndex;
    uint32_t toIoCount;
    uint32_t linkType;
    uint32_t colour;
} tClipboardCable;

typedef struct {
    bool             active;
    uint32_t         location;
    tClipboardModule modules[MAX_NUM_MODULES];
    uint32_t         moduleCount;
    tClipboardCable  cables[MAX_NUM_CABLES];
    uint32_t         cableCount;
} tClipboard;

typedef struct {
    const tModuleType moduleType;
    const tParamType  type;
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
    const tParamType  type;
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
    const tParamType  type;
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

typedef enum {
    volumeMeterStyleMaskLeds,  // Independent per-bit LEDs, stacked vertically (Compress)
    volumeMeterStyleSingleLed, // Exactly one LED lit, at index == value, stacked horizontally (Sequencer)
    volumeMeterStyleLevelBar   // Continuous multi-band coloured bar with clip indicator (Mono/Stereo/Quad)
} tVolumeMeterStyle;

typedef struct {
    const tVolumeType       volumeType;
    const tVolumeMeterStyle style;
    const uint32_t          segments;  // LED count (LED styles) or level-bar step count (level-bar style)
    const double            space;     // gap between LEDs (LED styles), or between repeated per-channel
                                       // meters for multi-channel level bars (Stereo/Quad); unused (0) otherwise
    const tRgb              onColour;  // lit LED colour (LED styles only)
    const tRgb              offColour; // unlit LED colour (LED styles only)
} tVolumeMeterConfig;

typedef struct {
    const tModuleType moduleType;
    const tLedType    ledType;
    const tRectangle  rectangle;
    const tAnchor     anchor;
} tLedLocation;

// Placement for the per-module custom preview graphs (render_oscshpb_waveform_graph(),
// render_envadsr_graph(), render_fltclassic_response_graph(), moduleGraphics.cpp) - each graph's
// own drawing/curve logic stays in its dedicated function, only the rect+anchor is table-driven.
typedef struct {
    const tModuleType moduleType;
    const tRectangle  rectangle;
    const tAnchor     anchor;
} tGraphLocation;

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
    eCommsWaitingReady,
    eCommsAwaitingSyncDecision,  // Device is ready, but offline edits diverged — waiting on the user
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
    uint32_t cursorPos;
} tNameEdit;  // Todo - rename to patch name edit

typedef struct {
    bool       active;
    tModuleKey moduleKey;
    char       buffer[CLAVIA_NAME_SIZE + 1];
    uint32_t   cursorPos;
} tModuleNameEdit;

typedef struct {
    bool       active;
    tModuleKey moduleKey;
    uint32_t   paramIndex;
    char       buffer[PROTOCOL_PARAM_NAME_SIZE + 1];
    uint32_t   cursorPos;
} tParamNameEdit;

// G2-Edit's own record of what the currently open context menu was raised
// against — deliberately kept out of the generic tContextMenu (see menus.c),
// which knows nothing about modules/connectors/params, only about menu items
// and screen positions. Set by whichever open_*_context_menu() raised the
// menu; read back by that same menu's action(index) callbacks.
typedef struct {
    tModuleKey    moduleKey;
    tConnectorDir connectorDir;
    uint32_t      connectorIndex;
    uint32_t      paramIndex;
} tMenuContext;

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

typedef enum {
    pPSustainPedal = 0,
    pPOctaveShift,
    pPArpEnabled,
    pPArpRate,
    pPArpDirection,
    pPArpOctaves,
    pPVibratoAmount,
    pPVibratoSource,
    pPVibratoRate,
    pPGlideTime,
    pPGlideMode,
    pPBendRange,
    pPBendEnabled,
    pPCount
} tPatchParamRectId;

typedef struct {                     // Note - should reflect settings in the G2
    char            name[CLAVIA_NAME_SIZE + 1];
    _Atomic uint8_t midiChanSlot[4]; // 0x00-0x0F = ch 1-16, 0x10 = off
    _Atomic uint8_t globalChan;      // same encoding
    _Atomic uint8_t sysexId;         // same encoding
    _Atomic uint8_t localOn;
    _Atomic uint8_t progChangeRcv;
    _Atomic uint8_t progChangeSnd;
    _Atomic uint8_t controllersRcv;
    _Atomic uint8_t controllersSnd;
    _Atomic uint8_t receiveClock;      // 1=off, 0=om
    _Atomic uint8_t sendClock;         // 0=on, 1=off
    _Atomic int8_t  tuneCent;          // signed, -50..+50 cents
    _Atomic uint8_t globalShiftActive;
    _Atomic int8_t  globalOctaveShift; // signed, -2..+2
    _Atomic int8_t  tuneSemi;          // signed, -12..+12 semitones
    _Atomic uint8_t pedalPolarity;
    _Atomic uint8_t pedalGain;
    _Atomic uint8_t memoryProtect;
    _Atomic uint8_t perfBank;
    _Atomic uint8_t perfLocation;
    _Atomic uint8_t patchSortMode;
    _Atomic uint8_t perfSortMode;
} tSynthSettings;

// One bank/location's worth of what SUB_COMMAND_LIST_NAMES (0x14) reports — see
// gPatchNameTable/gPerfNameTable in globalVars.h. populated distinguishes a real (possibly
// zero-length-named) entry from a location the device never mentioned at all (List Names is a
// sparse listing — unpopulated locations are never sent on the wire, not represented by a
// placeholder entry).
typedef struct {
    bool    populated;
    char    name[CLAVIA_NAME_SIZE + 1];
    uint8_t category; // 0-15, see patchTypeStrMap in globalVars.c
} tNameTableEntry;

typedef struct {  // Note - should reflect settings in the G2
    _Atomic uint8_t globalMode;
    _Atomic uint8_t rangeAndFlags;
    _Atomic uint8_t keyboardRange;
    struct {
        _Atomic uint8_t keyboardEnabled;
        _Atomic uint8_t holdEnabled;
        _Atomic uint8_t rangeLower;
        _Atomic uint8_t rangeUpper;
    } slot[MAX_SLOTS];
} tPerfSettings;

typedef struct {  // Note - should reflect settings in the G2
    _Atomic uint8_t perfMode;
    char            perfName[CLAVIA_NAME_SIZE + 1];
    _Atomic uint8_t perfVersion;
    _Atomic uint8_t masterClock;
    _Atomic uint8_t masterClockRunning;
    _Atomic uint8_t masterVolume;
    _Atomic uint8_t selectedSlot;
    struct {
        char            patchName[CLAVIA_NAME_SIZE + 1];
        _Atomic uint8_t patchVersion;
        _Atomic uint8_t enabled;     // a.k.a. active
    } slot[MAX_SLOTS];
} tGlobalSettings;

typedef struct {
    tRectangle close;
    bool       closePressed;
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

typedef struct {
    bool active;
} tPerfSettingsEdit;

typedef struct {
    tRectangle close;
    bool       closePressed;
    tRectangle masterClock;
    tRectangle masterClockRunning;
    tRectangle keyboardRange;
    tRectangle slotEnabled[MAX_SLOTS];
    tRectangle slotKeyboard[MAX_SLOTS];
    tRectangle slotHold[MAX_SLOTS];
    tRectangle rangeLower[MAX_SLOTS];
    tRectangle rangeUpper[MAX_SLOTS];
} tPerfSettingsPanelRects;

// tDialMode now lives in SynthLib's synthlibTypes.h (identical across all three apps) — pulled in
// transitively via geometry.h above.

#endif // __TYPES_H__

