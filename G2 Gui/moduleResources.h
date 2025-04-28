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
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Keyboard",     2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"4-Out",        2,  0, 0, 0, volumeTypeNone},
    {"2-Out",        2,  0, 0, 0, volumeTypeStereo},
    {"Invert",       2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"OscB",         5,  0, 0, 0, volumeTypeNone},
    {"OscShpB",      4, 10, 6, 1, volumeTypeNone},
    {"OscC",         3,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Reverb",       3,  0, 0, 0, volumeTypeNone},
    {"OscString",    3,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Sw8-1",        4,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"ValSw1-2",     2,  0, 0, 0, volumeTypeNone},
    {"X-Fade",       2,  0, 0, 0, volumeTypeNone},
    {"Mix4-1B",      2,  0, 0, 0, volumeTypeNone},
    {"EnvADSR",      4,  8, 5, 0, volumeTypeNone},
    {"Mux1-8",       2,  0, 0, 0, volumeTypeNone},
    {"PartQuant",    2,  0, 0, 0, volumeTypeNone},
    {"ModADSR",      5,  0, 0, 0, volumeTypeNone},
    {"LfoC",         2,  0, 0, 0, volumeTypeNone},
    {"LfoShpA",      5,  0, 0, 0, volumeTypeNone},
    {"LfoA",         3,  0, 0, 0, volumeTypeNone},
    {"OscMaster",    3,  0, 0, 0, volumeTypeNone},
    {"Saturate",     2,  0, 0, 0, volumeTypeNone},
    {"MetNoise",     2,  0, 0, 0, volumeTypeNone},
    {"Device",       3,  0, 0, 0, volumeTypeNone},
    {"Noise",        2,  0, 0, 0, volumeTypeNone},
    {"Eq2Band",      3,  0, 0, 0, volumeTypeNone},
    {"Eq3band",      4,  0, 0, 0, volumeTypeNone},
    {"ShpExp",       2,  0, 0, 0, volumeTypeNone},
    {"Driver",       3,  0, 0, 0, volumeTypeNone},
    {"SwOnOffM",     2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Pulse",        2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Mix8-1B",      4,  0, 0, 0, volumeTypeNone},
    {"EnvH",         2,  0, 0, 0, volumeTypeNone},
    {"Delay",        2,  0, 0, 0, volumeTypeNone},
    {"Constant",     2,  0, 0, 0, volumeTypeNone},
    {"LevMult",      2,  0, 0, 0, volumeTypeNone},
    {"FltVoice",     4,  0, 0, 0, volumeTypeNone},
    {"EnvAHD",       4,  0, 0, 0, volumeTypeNone},
    {"Pan",          2,  0, 0, 0, volumeTypeNone},
    {"MixStereo",    5,  0, 0, 0, volumeTypeNone},
    {"FltMulti",     4,  7, 6, 0, volumeTypeNone},
    {"ConstSwT",     2,  0, 0, 0, volumeTypeNone},
    {"FltNord",      5,  0, 0, 0, volumeTypeNone},
    {"EnvMulti",     6,  0, 0, 0, volumeTypeNone},
    {"SandH",        2,  0, 0, 0, volumeTypeNone},
    {"FltStatic",    3,  0, 0, 0, volumeTypeNone},
    {"EnvD",         2,  0, 0, 0, volumeTypeNone},
    {"Resonator",    5,  0, 0, 0, volumeTypeNone},
    {"Automate",     2,  0, 0, 0, volumeTypeNone},
    {"DrumSynth",    8,  0, 0, 0, volumeTypeNone},
    {"CompLev",      2,  0, 0, 0, volumeTypeNone},
    {"Mux8-1X",      3,  0, 0, 0, volumeTypeNone},
    {"Clip",         2,  0, 0, 0, volumeTypeNone},
    {"Overdrive",    2,  0, 0, 0, volumeTypeNone},
    {"Scratch",      3,  0, 0, 0, volumeTypeNone},
    {"Gate",         2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Mix2-1B",      2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"ClkGen",       4,  0, 0, 0, volumeTypeNone},
    {"ClkDiv",       2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"EnvFollow",    2,  0, 0, 0, volumeTypeNone},
    {"NoteScaler",   2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"WaveWrap",     2,  0, 0, 0, volumeTypeNone},
    {"NoteQuant",    2,  0, 0, 0, volumeTypeNone},
    {"SwOnOffT",     2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Sw1-8",        4,  0, 0, 0, volumeTypeNone},
    {"Sw4-1",        3,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"LevAmp",       2,  0, 0, 0, volumeTypeNone},
    {"Rect",         2,  0, 0, 0, volumeTypeNone},
    {"ShpStatic",    2,  0, 0, 0, volumeTypeNone},
    {"EnvAD",        3,  0, 0, 0, volumeTypeNone},
    {"WindSw",       2,  0, 0, 0, volumeTypeNone},
    {"8Counter",     2,  0, 0, 0, volumeTypeNone},
    {"FltLP",        2,  0, 0, 0, volumeTypeNone},
    {"Sw1-4",        3,  0, 0, 0, volumeTypeNone},
    {"Flanger",      3,  0, 0, 0, volumeTypeNone},
    {"Sw1-2",        2,  0, 0, 0, volumeTypeNone},
    {"FlipFlop",     2,  0, 0, 0, volumeTypeNone},
    {"FltClassic",   4,  6, 4, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"StChorus",     3,  3, 3, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"OscD",         2,  0, 0, 0, volumeTypeNone},
    {"OscA",         3,  0, 0, 0, volumeTypeNone},
    {"FreqShift",    3,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Sw2-1",        2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"FltPhase",     5,  0, 0, 0, volumeTypeNone},
    {"EqPeak",       4,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"ValSw2-1",     2,  0, 0, 0, volumeTypeNone},
    {"OscNoise",     3,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Vocoder",      8,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"LevAdd",       2,  0, 0, 0, volumeTypeNone},
    {"Fade1-2",      2,  0, 0, 0, volumeTypeNone},
    {"Fade2-1",      2,  0, 0, 0, volumeTypeNone},
    {"LevScaler",    3,  0, 0, 0, volumeTypeNone},
    {"Mix8-1A",      2,  0, 0, 0, volumeTypeNone},
    {"LevMod",       3,  0, 0, 0, volumeTypeNone},
    {"Digitizer",    3,  0, 0, 0, volumeTypeNone},
    {"EnvADDSR",     5,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"SeqNote",      9,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Mix4-1C",      4, 10, 5, 0, volumeTypeMono},
    {"Mux8-1",       2,  0, 0, 0, volumeTypeNone},
    {"WahWah",       2,  0, 0, 0, volumeTypeNone},
    {"Name",         1,  0, 0, 0, volumeTypeNone},
    {"Fx-In",        2,  0, 0, 0, volumeTypeNone},
    {"MinMax",       2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"BinCounter",   2,  0, 0, 0, volumeTypeNone},
    {"ADConv",       2,  0, 0, 0, volumeTypeNone},
    {"DAConv",       2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"FltHP",        2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"T&H",          2,  0, 0, 0, volumeTypeNone},
    {"Mix4-1S",      4,  0, 0, 0, volumeTypeNone},
    {"CtrlSend",     2,  0, 0, 0, volumeTypeNone},
    {"PCSend",       2,  0, 0, 0, volumeTypeNone},
    {"NoteSend",     2,  0, 0, 0, volumeTypeNone},
    {"SeqEvent",     5,  0, 0, 0, volumeTypeNone},
    {"SeqVal",       8,  0, 0, 0, volumeTypeNone},
    {"SeqLev",       8,  0, 0, 0, volumeTypeNone},
    {"CtrlRcv",      2,  0, 0, 0, volumeTypeNone},
    {"NoteRcv",      2,  0, 0, 0, volumeTypeNone},
    {"NoteZone",     3,  0, 0, 0, volumeTypeNone},
    {"Compress",     5,  7, 2, 0, volumeTypeCompress},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"KeyQuant",     2,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"SeqCtr",       8,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"NoteDet",      2,  0, 0, 0, volumeTypeNone},
    {"LevConv",      2,  0, 0, 0, volumeTypeNone},
    {"Glide",        2,  0, 0, 0, volumeTypeNone},
    {"CompSig",      2,  0, 0, 0, volumeTypeNone},
    {"ZeroCnt",      2,  0, 0, 0, volumeTypeNone},
    {"MixFader",     9,  0, 0, 0, volumeTypeNone},
    {"FltComb",      4,  0, 0, 0, volumeTypeNone},
    {"OscShpA",      5,  0, 0, 0, volumeTypeNone},
    {"OscDual",      5,  0, 0, 0, volumeTypeNone},
    {"DXRouter",     6,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"PShift",       3,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"ModAHD",       5,  0, 0, 0, volumeTypeNone},
    {"2-In",         2,  0, 0, 0, volumeTypeNone},
    {"4-In",         2,  0, 0, 0, volumeTypeNone},
    {"DlySingleA",   2,  0, 0, 0, volumeTypeNone},
    {"DlySingleB",   2,  0, 0, 0, volumeTypeNone},
    {"DelayDual",    3,  0, 0, 0, volumeTypeNone},
    {"DelayQuad",    5,  0, 0, 0, volumeTypeNone},
    {"DelayA",       3,  0, 0, 0, volumeTypeNone},
    {"DelayB",       4,  0, 0, 0, volumeTypeNone},
    {"DlyClock",     2,  0, 0, 0, volumeTypeNone},
    {"DlyShiftReg",  2,  0, 0, 0, volumeTypeNone},
    {"Operator",    12,  0, 0, 0, volumeTypeNone},
    {"DlyEight",     3,  0, 0, 0, volumeTypeNone},
    {"DlyStereo",    5,  0, 0, 0, volumeTypeNone},
    {"OscPM",        3,  0, 0, 0, volumeTypeNone},
    {"Mix1-1A",      2,  0, 0, 0, volumeTypeNone},
    {"Mix1-1S",      2,  0, 0, 0, volumeTypeNone},
    {"Sw1-2M",       2,  0, 0, 0, volumeTypeNone},
    {"Sw2-1M",       2,  0, 0, 0, volumeTypeNone},
    {"ConstSwM",     2,  0, 0, 0, volumeTypeNone},
    {"NoiseGate",    3,  0, 0, 0, volumeTypeNone},
    {"LfoB",         4,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"Phaser",       2,  0, 0, 0, volumeTypeNone},
    {"Mix4-1A",      2,  0, 0, 0, volumeTypeNone},
    {"Mix2-1A",      2,  0, 0, 0, volumeTypeNone},
    {"ModAmt",       2,  0, 0, 0, volumeTypeNone},
    {"OscPerc",      3,  0, 0, 0, volumeTypeNone},
    {"Status",       2,  0, 0, 0, volumeTypeNone},
    {"PitchTrack",   2,  0, 0, 0, volumeTypeNone},
    {"MonoKey",      2,  0, 0, 0, volumeTypeNone},
    {"RandomA",      2,  0, 0, 0, volumeTypeNone},
    {"Red2Blue",     2,  0, 0, 0, volumeTypeNone},
    {"RandomB",      3,  0, 0, 0, volumeTypeNone},
    {"Blue2Red",     2,  0, 0, 0, volumeTypeNone},
    {"RndClkA",      2,  0, 0, 0, volumeTypeNone},
    {"RndTrig",      2,  0, 0, 0, volumeTypeNone},
    {"RndClkB",      3,  0, 0, 0, volumeTypeNone},
    {"Unknown",      0,  0, 0, 0, volumeTypeNone},
    {"RndPattern",   3,  0, 0, 0, volumeTypeNone}, };

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
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   0, 60,  "Thr",     43,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   10, 60, "2",     67,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   20, 60, "Att",     128,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   30, 60, "Rel",     128,  0, NULL           },
    {moduleTypeCompress,   paramType1CommonDial,            paramType2Dial,   40, 60, "RefLvl",    43,  0, NULL           },
    {moduleTypeCompress,   paramType1Sidechain,            paramType2Toggle,  40, 20, NULL,     2,  0, NULL           },
    {moduleTypeCompress,   paramType1Bypass,                paramType2Toggle,   80, 80, NULL,  2,  1, NULL } };

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
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   95, 15}, };

#endif // __MODULE_RESOURCES_H__
