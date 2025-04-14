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

#ifndef __MODULE_RESOURCES_H__
#define __MODULE_RESOURCES_H__

#include "sysIncludes.h"

const char * emptyMap[]        = {NULL};
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

//TODO: see if we can make this const and only reference in one source file
tModuleProperties gModuleProperties[] = {
    // Array index = module type for access speed
    {"Unknown",      0,  0, 0, 0},
    {"Keyboard",     2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"4-Out",        2,  0, 0, 0},
    {"2-Out",        2,  0, 0, 0},
    {"Invert",       2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"OscB",         5,  0, 0, 0},
    {"OscShpB",      4, 10, 6, 1},
    {"OscC",         3,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Reverb",       3,  0, 0, 0},
    {"OscString",    3,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Sw8-1",        4,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"ValSw1-2",     2,  0, 0, 0},
    {"X-Fade",       2,  0, 0, 0},
    {"Mix4-1B",      2,  0, 0, 0},
    {"EnvADSR",      4,  8, 5, 0},
    {"Mux1-8",       2,  0, 0, 0},
    {"PartQuant",    2,  0, 0, 0},
    {"ModADSR",      5,  0, 0, 0},
    {"LfoC",         2,  0, 0, 0},
    {"LfoShpA",      5,  0, 0, 0},
    {"LfoA",         3,  0, 0, 0},
    {"OscMaster",    3,  0, 0, 0},
    {"Saturate",     2,  0, 0, 0},
    {"MetNoise",     2,  0, 0, 0},
    {"Device",       3,  0, 0, 0},
    {"Noise",        2,  0, 0, 0},
    {"Eq2Band",      3,  0, 0, 0},
    {"Eq3band",      4,  0, 0, 0},
    {"ShpExp",       2,  0, 0, 0},
    {"Driver",       3,  0, 0, 0},
    {"SwOnOffM",     2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Pulse",        2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Mix8-1B",      4,  0, 0, 0},
    {"EnvH",         2,  0, 0, 0},
    {"Delay",        2,  0, 0, 0},
    {"Constant",     2,  0, 0, 0},
    {"LevMult",      2,  0, 0, 0},
    {"FltVoice",     4,  0, 0, 0},
    {"EnvAHD",       4,  0, 0, 0},
    {"Pan",          2,  0, 0, 0},
    {"MixStereo",    5,  0, 0, 0},
    {"FltMulti",     4,  7, 6, 0},
    {"ConstSwT",     2,  0, 0, 0},
    {"FltNord",      5,  0, 0, 0},
    {"EnvMulti",     6,  0, 0, 0},
    {"SandH",        2,  0, 0, 0},
    {"FltStatic",    3,  0, 0, 0},
    {"EnvD",         2,  0, 0, 0},
    {"Resonator",    5,  0, 0, 0},
    {"Automate",     2,  0, 0, 0},
    {"DrumSynth",    8,  0, 0, 0},
    {"CompLev",      2,  0, 0, 0},
    {"Mux8-1X",      3,  0, 0, 0},
    {"Clip",         2,  0, 0, 0},
    {"Overdrive",    2,  0, 0, 0},
    {"Scratch",      3,  0, 0, 0},
    {"Gate",         2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Mix2-1B",      2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"ClkGen",       4,  0, 0, 0},
    {"ClkDiv",       2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"EnvFollow",    2,  0, 0, 0},
    {"NoteScaler",   2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"WaveWrap",     2,  0, 0, 0},
    {"NoteQuant",    2,  0, 0, 0},
    {"SwOnOffT",     2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Sw1-8",        4,  0, 0, 0},
    {"Sw4-1",        3,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"LevAmp",       2,  0, 0, 0},
    {"Rect",         2,  0, 0, 0},
    {"ShpStatic",    2,  0, 0, 0},
    {"EnvAD",        3,  0, 0, 0},
    {"WindSw",       2,  0, 0, 0},
    {"8Counter",     2,  0, 0, 0},
    {"FltLP",        2,  0, 0, 0},
    {"Sw1-4",        3,  0, 0, 0},
    {"Flanger",      3,  0, 0, 0},
    {"Sw1-2",        2,  0, 0, 0},
    {"FlipFlop",     2,  0, 0, 0},
    {"FltClassic",   4,  6, 4, 0},
    {"Unknown",      0,  0, 0, 0},
    {"StChorus",     3,  3, 3, 0},
    {"Unknown",      0,  0, 0, 0},
    {"OscD",         2,  0, 0, 0},
    {"OscA",         3,  0, 0, 0},
    {"FreqShift",    3,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Sw2-1",        2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"FltPhase",     5,  0, 0, 0},
    {"EqPeak",       4,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"ValSw2-1",     2,  0, 0, 0},
    {"OscNoise",     3,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Vocoder",      8,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"LevAdd",       2,  0, 0, 0},
    {"Fade1-2",      2,  0, 0, 0},
    {"Fade2-1",      2,  0, 0, 0},
    {"LevScaler",    3,  0, 0, 0},
    {"Mix8-1A",      2,  0, 0, 0},
    {"LevMod",       3,  0, 0, 0},
    {"Digitizer",    3,  0, 0, 0},
    {"EnvADDSR",     5,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"SeqNote",      9,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Mix4-1C",      4, 10, 5, 0},
    {"Mux8-1",       2,  0, 0, 0},
    {"WahWah",       2,  0, 0, 0},
    {"Name",         1,  0, 0, 0},
    {"Fx-In",        2,  0, 0, 0},
    {"MinMax",       2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"BinCounter",   2,  0, 0, 0},
    {"ADConv",       2,  0, 0, 0},
    {"DAConv",       2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"FltHP",        2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"T&H",          2,  0, 0, 0},
    {"Mix4-1S",      4,  0, 0, 0},
    {"CtrlSend",     2,  0, 0, 0},
    {"PCSend",       2,  0, 0, 0},
    {"NoteSend",     2,  0, 0, 0},
    {"SeqEvent",     5,  0, 0, 0},
    {"SeqVal",       8,  0, 0, 0},
    {"SeqLev",       8,  0, 0, 0},
    {"CtrlRcv",      2,  0, 0, 0},
    {"NoteRcv",      2,  0, 0, 0},
    {"NoteZone",     3,  0, 0, 0},
    {"Compress",     5,  7, 2, 0},
    {"Unknown",      0,  0, 0, 0},
    {"KeyQuant",     2,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"SeqCtr",       8,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"NoteDet",      2,  0, 0, 0},
    {"LevConv",      2,  0, 0, 0},
    {"Glide",        2,  0, 0, 0},
    {"CompSig",      2,  0, 0, 0},
    {"ZeroCnt",      2,  0, 0, 0},
    {"MixFader",     9,  0, 0, 0},
    {"FltComb",      4,  0, 0, 0},
    {"OscShpA",      5,  0, 0, 0},
    {"OscDual",      5,  0, 0, 0},
    {"DXRouter",     6,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"PShift",       3,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"ModAHD",       5,  0, 0, 0},
    {"2-In",         2,  0, 0, 0},
    {"4-In",         2,  0, 0, 0},
    {"DlySingleA",   2,  0, 0, 0},
    {"DlySingleB",   2,  0, 0, 0},
    {"DelayDual",    3,  0, 0, 0},
    {"DelayQuad",    5,  0, 0, 0},
    {"DelayA",       3,  0, 0, 0},
    {"DelayB",       4,  0, 0, 0},
    {"DlyClock",     2,  0, 0, 0},
    {"DlyShiftReg",  2,  0, 0, 0},
    {"Operator",    12,  0, 0, 0},
    {"DlyEight",     3,  0, 0, 0},
    {"DlyStereo",    5,  0, 0, 0},
    {"OscPM",        3,  0, 0, 0},
    {"Mix1-1A",      2,  0, 0, 0},
    {"Mix1-1S",      2,  0, 0, 0},
    {"Sw1-2M",       2,  0, 0, 0},
    {"Sw2-1M",       2,  0, 0, 0},
    {"ConstSwM",     2,  0, 0, 0},
    {"NoiseGate",    3,  0, 0, 0},
    {"LfoB",         4,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"Phaser",       2,  0, 0, 0},
    {"Mix4-1A",      2,  0, 0, 0},
    {"Mix2-1A",      2,  0, 0, 0},
    {"ModAmt",       2,  0, 0, 0},
    {"OscPerc",      3,  0, 0, 0},
    {"Status",       2,  0, 0, 0},
    {"PitchTrack",   2,  0, 0, 0},
    {"MonoKey",      2,  0, 0, 0},
    {"RandomA",      2,  0, 0, 0},
    {"Red2Blue",     2,  0, 0, 0},
    {"RandomB",      3,  0, 0, 0},
    {"Blue2Red",     2,  0, 0, 0},
    {"RndClkA",      2,  0, 0, 0},
    {"RndTrig",      2,  0, 0, 0},
    {"RndClkB",      3,  0, 0, 0},
    {"Unknown",      0,  0, 0, 0},
    {"RndPattern",   3,  0, 0, 0}, };

// Todo - I think we need a moduleResources.c for accessing these structures from anywhere, including getting size / number of items etc.
// Also, add text mapping reference in here! E.g. Keyboard track
void render_param_common(tCoord coord, uint32_t paramRef, uint32_t param, tModule * module);

// moduleType, paramType, offsetX, offsetY, label, range, defaultValue, string map
tParamLocation paramLocationList[] = {
    {moduleTypeOscShpB,    paramTypeCommonDial,            paramType2Dial,   32, 70, NULL,      128, 64, emptyMap       },
    {moduleTypeOscShpB,    paramTypeCommonDial,            paramType2Dial,   45, 60, "Cent",    128, 64, emptyMap       },
    {moduleTypeOscShpB,    paramTypeOffOnKeyboardTrack,    paramType2Toggle, 22, 60, "Kbt",       2,  1, offOnKbMap     },
    {moduleTypeOscShpB,    paramTypeCommonDial,            paramType2Dial,   12, 60, "Env",     128,  0, emptyMap       },
    {moduleTypeOscShpB,    paramTypePitchType,             paramType2Toggle, 30, 45, NULL,        4,  0, pitchTypeMap   },
    {moduleTypeOscShpB,    paramTypeCommonDial,            paramType2Dial,   62, 70, NULL,      128,  0, emptyMap       },
    {moduleTypeOscShpB,    paramTypeCommonDial,            paramType2Dial,   80, 15, "Shape",   128,  0, emptyMap       }, // 50% to 99%, not decimals
    {moduleTypeOscShpB,    paramTypeCommonDial,            paramType2Dial,   82, 60, "ShpCtrl", 128,  0, emptyMap       },
    {moduleTypeOscShpB,    paramTypeBypass,                paramType2Toggle, 95, 70, "Bypass",    2,  0, emptyMap       },
    {moduleTypeOscShpB,    paramTypeFmType,                paramType2Toggle, 60, 45, NULL,        2,  0, fmTypeMap      },
    {moduleTypeFltClassic, paramTypeFreq,                  paramType2Dial,   40, 60, "Freq",    128,  0, emptyMap       },
    {moduleTypeFltClassic, paramTypePitch,                 paramType2Dial,   10, 60, "Env",     128,  0, emptyMap       },
    {moduleTypeFltClassic, paramTypeOffTo100KeyboardTrack, paramType2Toggle, 25, 60, "Kbt",       5,  4, offTo100KbMap  },
    {moduleTypeFltClassic, paramTypeResonance,             paramType2Dial,   55, 60, "Res",     128,  0, emptyMap       },
    {moduleTypeFltClassic, paramTypeFltClassicDb,          paramType2Toggle, 70, 60, NULL,        3,  0, fltClassicDbMap},
    {moduleTypeFltClassic, paramTypeBypass,                paramType2Toggle, 95, 60, NULL,        2,  1, emptyMap       },
    {moduleTypeFltMulti,   paramTypeFreq,                  paramType2Dial,   40, 60, "Freq",    128,  0, emptyMap       },
    {moduleTypeFltMulti,   paramTypePitch,                 paramType2Dial,   12, 60, "Env",     128,  0, emptyMap       },
    {moduleTypeFltMulti,   paramTypeOffTo100KeyboardTrack, paramType2Toggle, 25, 60, "Kbt",       5,  4, offTo100KbMap  },
    {moduleTypeFltMulti,   paramTypeResonance,             paramType2Dial,   55, 60, "Res",     128,  0, emptyMap       },
    {moduleTypeFltMulti,   paramTypeGainControl,           paramType2Toggle, 55, 20, NULL,        2,  0, emptyMap       },
    {moduleTypeFltMulti,   paramTypeFltMultiDb,            paramType2Toggle, 70, 20, NULL,        2,  0, fltMultiDbMap  },
    {moduleTypeFltMulti,   paramTypeBypass,                paramType2Toggle, 80, 80, NULL,        2,  1, emptyMap       },
    {moduleTypeEnvADSR,    paramTypeCommonDial,            paramType2Dial,   20, 60, "Attack",  128,  0, emptyMap       },
    {moduleTypeEnvADSR,    paramTypeCommonDial,            paramType2Dial,   35, 60, "Delay",   128,  0, emptyMap       },
    {moduleTypeEnvADSR,    paramTypeCommonDial,            paramType2Dial,   50, 60, "Sus",     128,  0, emptyMap       },
    {moduleTypeEnvADSR,    paramTypeCommonDial,            paramType2Dial,   65, 60, "Rel",     128,  0, emptyMap       }, };

void render_connector(tModule * module, uint32_t connectorIndex, tConnectorDir dir, tConnectorType type, tCoord coord);

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
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   95, 15}, };

#endif // __MODULE_RESOURCES_H__
