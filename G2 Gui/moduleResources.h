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

// NOTE - Only moduleResourcesAccess.c should be including this!

#ifndef __MODULE_RESOURCES_H__
#define __MODULE_RESOURCES_H__

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"

//const char * emptyMap[]        = {NULL};
const char * fltClassicDbMap[] = {"12db", "18db", "24db"};
const char * fltMultiDbMap[]   = {"6db", "12db"};
const char * offTo100KbMap[]   = {"Off", "25%", "50%", "75%", "100%"};
const char * offOnKbMap[]      = {"Off", "On"};
const char * pitchTypeMap[]    = {"Semi", "Freq", "Factor", "Partial"};
const char * fmTypeMap[]       = {"FM Lin", "FM Trk"};

const tRgb   cableColourMap[] = {
    {0.7, 0.1, 0.1},       // red
    {0.1, 0.1, 0.7},       // blue
    {0.7, 0.7, 0.1},       // yellow
    {0.8, 0.3, 0.2},       // orange
    {0.1, 0.7, 0.1},       // green
    {0.7, 0.1, 0.7},       // purple
    {0.9, 0.9, 0.9},       // white
};

const tRgb   gModuleColourMap[] = {
    {0.7, 0.7, 0.7},       // standard grey
    {0.8, 0.6, 0.6},       // red 4
    {0.6, 0.8, 0.6},       // green 4
    {0.6, 0.6, 0.8},       // blue 4
    {0.8, 0.8, 0.6},       // yellow 4
    {0.3, 0.3, 0.8},       // blue 1
    {0.8, 0.3, 0.3},       // red 1
    {0.4, 0.8, 0.8},       // cyan 2
    {0.4, 0.8, 0.4},       // green 2
    {0.8, 0.8, 0.3},       // yellow 1
    {0.3, 0.8, 0.3},       // green 1
    {0.8, 0.8, 0.4},       // yellow 2
    {0.5, 0.5, 0.8},       // blue 3
    {0.8, 0.4, 0.4},       // red 2
    {0.8, 0.5, 0.5},       // red 3
    {0.8, 0.8, 0.5},       // yellow 3
    {0.5, 0.8, 0.5},       // green 3
    {0.3, 0.8, 0.8},       // cyan 1
    {0.5, 0.8, 0.8},       // cyan 3
    {0.6, 0.8, 0.8},       // cyan 4
    {0.4, 0.4, 0.8},       // blue 2
    {0.8, 0.3, 0.8},       // purple 1
    {0.8, 0.4, 0.8},       // purple 2
    {0.8, 0.5, 0.8},       // purple 3
    {0.8, 0.6, 0.8},       // purple 4
};

const tRgb   connectorColourMap[] =  // TODO - add yellow for gate
{
    {0.7, 0.2, 0.2},
    {0.2, 0.2, 0.7}};


const tModuleProperties gModuleProperties[] = {
    {"Unknown",      0,   volumeTypeNone    },
    {"Keyboard",     2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"4-Out",        2,   volumeTypeNone    },
    {"2-Out",        2,   volumeTypeStereo  },
    {"Invert",       2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"OscB",         5,   volumeTypeNone    },
    {"OscShpB",      4,   volumeTypeNone    },
    {"OscC",         3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Reverb",       3,   volumeTypeNone    },
    {"OscString",    3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Sw8-1",        4,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"ValSw1-2",     2,   volumeTypeNone    },
    {"X-Fade",       2,   volumeTypeNone    },
    {"Mix4-1B",      2,   volumeTypeNone    },
    {"EnvADSR",      4,   volumeTypeNone    },
    {"Mux1-8",       2,   volumeTypeNone    },
    {"PartQuant",    2,   volumeTypeNone    },
    {"ModADSR",      5,   volumeTypeNone    },
    {"LfoC",         2,   volumeTypeNone    },
    {"LfoShpA",      5,   volumeTypeNone    },
    {"LfoA",         3,   volumeTypeNone    },
    {"OscMaster",    3,   volumeTypeNone    },
    {"Saturate",     2,   volumeTypeNone    },
    {"MetNoise",     2,   volumeTypeNone    },
    {"Device",       3,   volumeTypeNone    },
    {"Noise",        2,   volumeTypeNone    },
    {"Eq2Band",      3,   volumeTypeNone    },
    {"Eq3band",      4,   volumeTypeNone    },
    {"ShpExp",       2,   volumeTypeNone    },
    {"Driver",       3,   volumeTypeNone    },
    {"SwOnOffM",     2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Pulse",        2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Mix8-1B",      4,   volumeTypeNone    },
    {"EnvH",         2,   volumeTypeNone    },
    {"Delay",        2,   volumeTypeNone    },
    {"Constant",     2,   volumeTypeNone    },
    {"LevMult",      2,   volumeTypeNone    },
    {"FltVoice",     4,   volumeTypeNone    },
    {"EnvAHD",       4,   volumeTypeNone    },
    {"Pan",          2,   volumeTypeNone    },
    {"MixStereo",    5,   volumeTypeNone    },
    {"FltMulti",     4,   volumeTypeNone    },
    {"ConstSwT",     2,   volumeTypeNone    },
    {"FltNord",      5,   volumeTypeNone    },
    {"EnvMulti",     6,   volumeTypeNone    },
    {"SandH",        2,   volumeTypeNone    },
    {"FltStatic",    3,   volumeTypeNone    },
    {"EnvD",         2,   volumeTypeNone    },
    {"Resonator",    5,   volumeTypeNone    },
    {"Automate",     2,   volumeTypeNone    },
    {"DrumSynth",    8,   volumeTypeNone    },
    {"CompLev",      2,   volumeTypeNone    },
    {"Mux8-1X",      3,   volumeTypeNone    },
    {"Clip",         2,   volumeTypeNone    },
    {"Overdrive",    2,   volumeTypeNone    },
    {"Scratch",      3,   volumeTypeNone    },
    {"Gate",         2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Mix2-1B",      2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"ClkGen",       4,   volumeTypeNone    },
    {"ClkDiv",       2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"EnvFollow",    2,   volumeTypeNone    },
    {"NoteScaler",   2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"WaveWrap",     2,   volumeTypeNone    },
    {"NoteQuant",    2,   volumeTypeNone    },
    {"SwOnOffT",     2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Sw1-8",        4,   volumeTypeNone    },
    {"Sw4-1",        3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"LevAmp",       2,   volumeTypeNone    },
    {"Rect",         2,   volumeTypeNone    },
    {"ShpStatic",    2,   volumeTypeNone    },
    {"EnvAD",        3,   volumeTypeNone    },
    {"WindSw",       2,   volumeTypeNone    },
    {"8Counter",     2,   volumeTypeNone    },
    {"FltLP",        2,   volumeTypeNone    },
    {"Sw1-4",        3,   volumeTypeNone    },
    {"Flanger",      3,   volumeTypeNone    },
    {"Sw1-2",        2,   volumeTypeNone    },
    {"FlipFlop",     2,   volumeTypeNone    },
    {"FltClassic",   4,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"StChorus",     3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"OscD",         2,   volumeTypeNone    },
    {"OscA",         3,   volumeTypeNone    },
    {"FreqShift",    3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Sw2-1",        2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"FltPhase",     5,   volumeTypeNone    },
    {"EqPeak",       4,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"ValSw2-1",     2,   volumeTypeNone    },
    {"OscNoise",     3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Vocoder",      8,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"LevAdd",       2,   volumeTypeNone    },
    {"Fade1-2",      2,   volumeTypeNone    },
    {"Fade2-1",      2,   volumeTypeNone    },
    {"LevScaler",    3,   volumeTypeNone    },
    {"Mix8-1A",      2,   volumeTypeNone    },
    {"LevMod",       3,   volumeTypeNone    },
    {"Digitizer",    3,   volumeTypeNone    },
    {"EnvADDSR",     5,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"SeqNote",      9,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Mix4-1C",      4,  volumeTypeMono    },
    {"Mux8-1",       2,   volumeTypeNone    },
    {"WahWah",       2,   volumeTypeNone    },
    {"Name",         1,   volumeTypeNone    },
    {"Fx-In",        2,   volumeTypeNone    },
    {"MinMax",       2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"BinCounter",   2,   volumeTypeNone    },
    {"ADConv",       2,   volumeTypeNone    },
    {"DAConv",       2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"FltHP",        2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"T&H",          2,   volumeTypeNone    },
    {"Mix4-1S",      4,   volumeTypeNone    },
    {"CtrlSend",     2,   volumeTypeNone    },
    {"PCSend",       2,   volumeTypeNone    },
    {"NoteSend",     2,   volumeTypeNone    },
    {"SeqEvent",     5,   volumeTypeNone    },
    {"SeqVal",       8,   volumeTypeNone    },
    {"SeqLev",       8,   volumeTypeNone    },
    {"CtrlRcv",      2,   volumeTypeNone    },
    {"NoteRcv",      2,   volumeTypeNone    },
    {"NoteZone",     3,   volumeTypeNone    },
    {"Compress",     5,   volumeTypeCompress},
    {"Unknown",      0,   volumeTypeNone    },
    {"KeyQuant",     2,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"SeqCtr",       8,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"NoteDet",      2,   volumeTypeNone    },
    {"LevConv",      2,   volumeTypeNone    },
    {"Glide",        2,   volumeTypeNone    },
    {"CompSig",      2,   volumeTypeNone    },
    {"ZeroCnt",      2,   volumeTypeNone    },
    {"MixFader",     9,   volumeTypeNone    },
    {"FltComb",      4,   volumeTypeNone    },
    {"OscShpA",      5,   volumeTypeNone    },
    {"OscDual",      5,   volumeTypeNone    },
    {"DXRouter",     6,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"PShift",       3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"ModAHD",       5,   volumeTypeNone    },
    {"2-In",         2,   volumeTypeNone    },
    {"4-In",         2,   volumeTypeNone    },
    {"DlySingleA",   2,   volumeTypeNone    },
    {"DlySingleB",   2,   volumeTypeNone    },
    {"DelayDual",    3,   volumeTypeNone    },
    {"DelayQuad",    5,   volumeTypeNone    },
    {"DelayA",       3,   volumeTypeNone    },
    {"DelayB",       4,   volumeTypeNone    },
    {"DlyClock",     2,   volumeTypeNone    },
    {"DlyShiftReg",  2,   volumeTypeNone    },
    {"Operator",    12,   volumeTypeNone    },
    {"DlyEight",     3,   volumeTypeNone    },
    {"DlyStereo",    5,   volumeTypeNone    },
    {"OscPM",        3,   volumeTypeNone    },
    {"Mix1-1A",      2,   volumeTypeNone    },
    {"Mix1-1S",      2,   volumeTypeNone    },
    {"Sw1-2M",       2,   volumeTypeNone    },
    {"Sw2-1M",       2,   volumeTypeNone    },
    {"ConstSwM",     2,   volumeTypeNone    },
    {"NoiseGate",    3,   volumeTypeNone    },
    {"LfoB",         4,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"Phaser",       2,   volumeTypeNone    },
    {"Mix4-1A",      2,   volumeTypeNone    },
    {"Mix2-1A",      2,   volumeTypeNone    },
    {"ModAmt",       2,   volumeTypeNone    },
    {"OscPerc",      3,   volumeTypeNone    },
    {"Status",       2,   volumeTypeNone    },
    {"PitchTrack",   2,   volumeTypeNone    },
    {"MonoKey",      2,   volumeTypeNone    },
    {"RandomA",      2,   volumeTypeNone    },
    {"Red2Blue",     2,   volumeTypeNone    },
    {"RandomB",      3,   volumeTypeNone    },
    {"Blue2Red",     2,   volumeTypeNone    },
    {"RndClkA",      2,   volumeTypeNone    },
    {"RndTrig",      2,   volumeTypeNone    },
    {"RndClkB",      3,   volumeTypeNone    },
    {"Unknown",      0,   volumeTypeNone    },
    {"RndPattern",   3,   volumeTypeNone    }, };

// moduleType, paramType, offsetX, offsetY, label, range, defaultValue, string map
const tParamLocation paramLocationList[] = {
    {moduleTypeOscShpB,    paramType1CommonDial,            paramType2Dial,   32, 70, NULL,      128, 64, NULL           },
    {moduleTypeOscShpB,    paramType1CommonDial,            paramType2Dial,   45, 60, "Cent",    128, 64, NULL           },
    {moduleTypeOscShpB,    paramType1OffOnKeyboardTrack,    paramType2Toggle, 22, 60, "Kbt",       2,  1, offOnKbMap     },
    {moduleTypeOscShpB,    paramType1CommonDial,            paramType2Dial,   12, 60, "Env",     128,  0, NULL           },
    {moduleTypeOscShpB,    paramType1PitchType,             paramType2Toggle, 30, 45, NULL,        4,  0, pitchTypeMap   },
    {moduleTypeOscShpB,    paramType1CommonDial,            paramType2Dial,   62, 70, NULL,      128,  0, NULL           },
    {moduleTypeOscShpB,    paramType1CommonDial,            paramType2Dial,   80, 15, "Shape",   128,  0, NULL           }, // 50% to 99%, not decimals
    {moduleTypeOscShpB,    paramType1CommonDial,            paramType2Dial,   82, 60, "ShpCtrl", 128,  0, NULL           },
    {moduleTypeOscShpB,    paramType1Bypass,                paramType2Toggle, 95, 70, "Bypass",    2,  0, NULL           },
    {moduleTypeOscShpB,    paramType1FmType,                paramType2Toggle, 60, 45, NULL,        2,  0, fmTypeMap      },
    {moduleTypeFltClassic, paramType1Freq,                  paramType2Dial,   40, 60, "Freq",    128,  0, NULL           },
    {moduleTypeFltClassic, paramType1Pitch,                 paramType2Dial,   10, 60, "Env",     128,  0, NULL           },
    {moduleTypeFltClassic, paramType1OffTo100KeyboardTrack, paramType2Toggle, 25, 60, "Kbt",       5,  4, offTo100KbMap  },
    {moduleTypeFltClassic, paramType1Resonance,             paramType2Dial,   55, 60, "Res",     128,  0, NULL           },
    {moduleTypeFltClassic, paramType1FltClassicDb,          paramType2Toggle, 70, 60, NULL,        3,  0, fltClassicDbMap},
    {moduleTypeFltClassic, paramType1Bypass,                paramType2Toggle, 95, 60, NULL,        2,  1, NULL           },
    {moduleTypeFltMulti,   paramType1Freq,                  paramType2Dial,   40, 60, "Freq",    128,  0, NULL           },
    {moduleTypeFltMulti,   paramType1Pitch,                 paramType2Dial,   12, 60, "Env",     128,  0, NULL           },
    {moduleTypeFltMulti,   paramType1OffTo100KeyboardTrack, paramType2Toggle, 25, 60, "Kbt",       5,  4, offTo100KbMap  },
    {moduleTypeFltMulti,   paramType1Resonance,             paramType2Dial,   55, 60, "Res",     128,  0, NULL           },
    {moduleTypeFltMulti,   paramType1GainControl,           paramType2Toggle, 55, 20, NULL,        2,  0, NULL           },
    {moduleTypeFltMulti,   paramType1FltMultiDb,            paramType2Toggle, 70, 20, NULL,        2,  0, fltMultiDbMap  },
    {moduleTypeFltMulti,   paramType1Bypass,                paramType2Toggle, 80, 80, NULL,        2,  1, NULL           },
    {moduleTypeEnvADSR,    paramType1CommonDial,            paramType2Dial,   20, 60, "Attack",  128,  0, NULL           },
    {moduleTypeEnvADSR,    paramType1CommonDial,            paramType2Dial,   35, 60, "Delay",   128,  0, NULL           },
    {moduleTypeEnvADSR,    paramType1CommonDial,            paramType2Dial,   50, 60, "Sus",     128,  0, NULL           },
    {moduleTypeEnvADSR,    paramType1CommonDial,            paramType2Dial,   65, 60, "Rel",     128,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,    0, 60, "Thr",      43,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   10, 60, "2",        67,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   20, 60, "Att",     128,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   30, 60, "Rel",     128,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   40, 60, "RefLvl",   43,  0, NULL           },
    {moduleTypeCompress,   paramType1Sidechain,             paramType2Toggle, 40, 20, NULL,        2,  0, NULL           },
    {moduleTypeCompress,   paramType1Bypass,                paramType2Toggle, 80, 80, NULL,        2,  1, NULL           }, };

// moduleType, paramType, offsetX, offsetY, label, range, defaultValue, string map
const tParamLocation modeLocationList[] = {
    {moduleTypeOscShpB, paramType1OscWave, paramType2Dial, 20, 15, "Wave", 8, 0, NULL}, };

// Note these need to be in same order of connectors which are referenced by the hardware
const tConnectorLocation connectorLocationList[] = {
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeAudio,   95, 20},
    {moduleTypeFltClassic, connectorDirOut, connectorTypeAudio,   95, 95},
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl,  0, 95},
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl,  0, 50},
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeAudio,   95, 20},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   95, 50},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   95, 70},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   95, 95},
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl,  0, 90},
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl,  0, 50},
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl, 95, 20},
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl,  0, 70},
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl,  0, 95},
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl, 85, 95},
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl, 95, 95},
    {moduleTypeMix4to1C,   connectorDirOut, connectorTypeControl, 95, 95},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, 45, 70},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, 55, 70},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, 65, 70},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, 75, 70},
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeControl,  0, 70},
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeControl,  0, 95},
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,    0, 45},
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   55, 95},
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   75, 95},
    {moduleTypeOscShpB,    connectorDirOut, connectorTypeAudio,   95, 95},
    {moduleTypeStChorus,   connectorDirIn,  connectorTypeAudio,   95, 20},
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   85, 95},
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   95, 95},
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   85, 15},
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   95, 15},
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   40, 15},
    {moduleTypeCompress,   connectorDirOut, connectorTypeAudio,   95, 90},
    {moduleTypeCompress,   connectorDirOut, connectorTypeAudio,   85, 90},
    {moduleType2toOut,     connectorDirIn,  connectorTypeAudio,   85, 30},
    {moduleType2toOut,     connectorDirIn,  connectorTypeAudio,   95, 30}, };

#endif // __MODULE_RESOURCES_H__
