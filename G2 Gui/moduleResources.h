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
const char * fltClassicDbStrMap[] = {"12db", "18db", "24db"};
const char * fltMultiDbStrMap[]   = {"6db", "12db"};
const char * offTo100KbStrMap[]   = {"Off", "25%", "50%", "75%", "100%"};
const char * offOnStrMap[]        = {"Off", "On"};
const tRgb   offOnColourMap[]     = {RGB_BACKGROUND_GREY, RGB_GREEN_ON};
const char * expStrMap[]          = {"Exp", "Lin", "dB"};
const char * padStrMap[]          = {"0dB", "-6dB"};
const char * gcStrMap[]           = {"GC", "GC"};
const char * pitchTypeStrMap[]    = {"Semi", "Freq", "Factor", "Partial"};
const char * fmTypeStrMap[]       = {"FM Lin", "FM Trk"};

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
    {0.2, 0.2, 0.7},
    {0.8, 0.8, 0.2},
};


const tModuleProperties gModuleProperties[] = {
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Keyboard",     2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"4-Out",        2, volumeTypeNone,     ledTypeNo },
    {"2-Out",        2, volumeTypeStereo,   ledTypeNo },
    {"Invert",       2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"OscB",         5, volumeTypeNone,     ledTypeNo },
    {"OscShpB",      4, volumeTypeNone,     ledTypeNo },
    {"OscC",         3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Reverb",       3, volumeTypeNone,     ledTypeNo },
    {"OscString",    3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Sw8-1",        4, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"ValSw1-2",     2, volumeTypeNone,     ledTypeNo },
    {"X-Fade",       2, volumeTypeNone,     ledTypeNo },
    {"Mix4-1B",      2, volumeTypeNone,     ledTypeNo },
    {"EnvADSR",      4, volumeTypeNone,     ledTypeYes},
    {"Mux1-8",       2, volumeTypeNone,     ledTypeNo },
    {"PartQuant",    2, volumeTypeNone,     ledTypeNo },
    {"ModADSR",      5, volumeTypeNone,     ledTypeNo },
    {"LfoC",         2, volumeTypeNone,     ledTypeNo },
    {"LfoShpA",      5, volumeTypeNone,     ledTypeNo },
    {"LfoA",         3, volumeTypeNone,     ledTypeNo },
    {"OscMaster",    3, volumeTypeNone,     ledTypeNo },
    {"Saturate",     2, volumeTypeNone,     ledTypeNo },
    {"MetNoise",     2, volumeTypeNone,     ledTypeNo },
    {"Device",       3, volumeTypeNone,     ledTypeNo },
    {"Noise",        2, volumeTypeNone,     ledTypeNo },
    {"Eq2Band",      3, volumeTypeNone,     ledTypeNo },
    {"Eq3band",      4, volumeTypeNone,     ledTypeNo },
    {"ShpExp",       2, volumeTypeNone,     ledTypeNo },
    {"Driver",       3, volumeTypeNone,     ledTypeNo },
    {"SwOnOffM",     2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Pulse",        2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Mix8-1B",      4, volumeTypeNone,     ledTypeNo },
    {"EnvH",         2, volumeTypeNone,     ledTypeNo },
    {"Delay",        2, volumeTypeNone,     ledTypeNo },
    {"Constant",     2, volumeTypeNone,     ledTypeNo },
    {"LevMult",      2, volumeTypeNone,     ledTypeNo },
    {"FltVoice",     4, volumeTypeNone,     ledTypeNo },
    {"EnvAHD",       4, volumeTypeNone,     ledTypeNo },
    {"Pan",          2, volumeTypeNone,     ledTypeNo },
    {"MixStereo",    5, volumeTypeNone,     ledTypeNo },
    {"FltMulti",     4, volumeTypeNone,     ledTypeNo },
    {"ConstSwT",     2, volumeTypeNone,     ledTypeNo },
    {"FltNord",      5, volumeTypeNone,     ledTypeNo },
    {"EnvMulti",     6, volumeTypeNone,     ledTypeNo },
    {"SandH",        2, volumeTypeNone,     ledTypeNo },
    {"FltStatic",    3, volumeTypeNone,     ledTypeNo },
    {"EnvD",         2, volumeTypeNone,     ledTypeNo },
    {"Resonator",    5, volumeTypeNone,     ledTypeNo },
    {"Automate",     2, volumeTypeNone,     ledTypeNo },
    {"DrumSynth",    8, volumeTypeNone,     ledTypeNo },
    {"CompLev",      2, volumeTypeNone,     ledTypeNo },
    {"Mux8-1X",      3, volumeTypeNone,     ledTypeNo },
    {"Clip",         2, volumeTypeNone,     ledTypeNo },
    {"Overdrive",    2, volumeTypeNone,     ledTypeNo },
    {"Scratch",      3, volumeTypeNone,     ledTypeNo },
    {"Gate",         2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Mix2-1B",      2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"ClkGen",       4, volumeTypeNone,     ledTypeNo },
    {"ClkDiv",       2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"EnvFollow",    2, volumeTypeNone,     ledTypeNo },
    {"NoteScaler",   2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"WaveWrap",     2, volumeTypeNone,     ledTypeNo },
    {"NoteQuant",    2, volumeTypeNone,     ledTypeNo },
    {"SwOnOffT",     2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Sw1-8",        4, volumeTypeNone,     ledTypeNo },
    {"Sw4-1",        3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"LevAmp",       2, volumeTypeNone,     ledTypeNo },
    {"Rect",         2, volumeTypeNone,     ledTypeNo },
    {"ShpStatic",    2, volumeTypeNone,     ledTypeNo },
    {"EnvAD",        3, volumeTypeNone,     ledTypeNo },
    {"WindSw",       2, volumeTypeNone,     ledTypeNo },
    {"8Counter",     2, volumeTypeNone,     ledTypeNo },
    {"FltLP",        2, volumeTypeNone,     ledTypeNo },
    {"Sw1-4",        3, volumeTypeNone,     ledTypeNo },
    {"Flanger",      3, volumeTypeNone,     ledTypeNo },
    {"Sw1-2",        2, volumeTypeNone,     ledTypeNo },
    {"FlipFlop",     2, volumeTypeNone,     ledTypeNo },
    {"FltClassic",   4, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"StChorus",     3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"OscD",         2, volumeTypeNone,     ledTypeNo },
    {"OscA",         3, volumeTypeNone,     ledTypeNo },
    {"FreqShift",    3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Sw2-1",        2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"FltPhase",     5, volumeTypeNone,     ledTypeNo },
    {"EqPeak",       4, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"ValSw2-1",     2, volumeTypeNone,     ledTypeNo },
    {"OscNoise",     3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Vocoder",      8, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"LevAdd",       2, volumeTypeNone,     ledTypeNo },
    {"Fade1-2",      2, volumeTypeNone,     ledTypeNo },
    {"Fade2-1",      2, volumeTypeNone,     ledTypeNo },
    {"LevScaler",    3, volumeTypeNone,     ledTypeNo },
    {"Mix8-1A",      2, volumeTypeNone,     ledTypeNo },
    {"LevMod",       3, volumeTypeNone,     ledTypeNo },
    {"Digitizer",    3, volumeTypeNone,     ledTypeNo },
    {"EnvADDSR",     5, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"SeqNote",      9, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Mix4-1C",      4, volumeTypeMono,     ledTypeNo },
    {"Mux8-1",       2, volumeTypeNone,     ledTypeNo },
    {"WahWah",       2, volumeTypeNone,     ledTypeNo },
    {"Name",         1, volumeTypeNone,     ledTypeNo },
    {"Fx-In",        2, volumeTypeNone,     ledTypeNo },
    {"MinMax",       2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"BinCounter",   2, volumeTypeNone,     ledTypeNo },
    {"ADConv",       2, volumeTypeNone,     ledTypeNo },
    {"DAConv",       2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"FltHP",        2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"T&H",          2, volumeTypeNone,     ledTypeNo },
    {"Mix4-1S",      4, volumeTypeNone,     ledTypeNo },
    {"CtrlSend",     2, volumeTypeNone,     ledTypeNo },
    {"PCSend",       2, volumeTypeNone,     ledTypeNo },
    {"NoteSend",     2, volumeTypeNone,     ledTypeNo },
    {"SeqEvent",     5, volumeTypeNone,     ledTypeNo },
    {"SeqVal",       8, volumeTypeNone,     ledTypeNo },
    {"SeqLev",       8, volumeTypeNone,     ledTypeNo },
    {"CtrlRcv",      2, volumeTypeNone,     ledTypeNo },
    {"NoteRcv",      2, volumeTypeNone,     ledTypeNo },
    {"NoteZone",     3, volumeTypeNone,     ledTypeNo },
    {"Compress",     5, volumeTypeCompress, ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"KeyQuant",     2, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"SeqCtr",       8, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"NoteDet",      2, volumeTypeNone,     ledTypeNo },
    {"LevConv",      2, volumeTypeNone,     ledTypeNo },
    {"Glide",        2, volumeTypeNone,     ledTypeNo },
    {"CompSig",      2, volumeTypeNone,     ledTypeNo },
    {"ZeroCnt",      2, volumeTypeNone,     ledTypeNo },
    {"MixFader",     9, volumeTypeNone,     ledTypeNo },
    {"FltComb",      4, volumeTypeNone,     ledTypeNo },
    {"OscShpA",      5, volumeTypeNone,     ledTypeNo },
    {"OscDual",      5, volumeTypeNone,     ledTypeNo },
    {"DXRouter",     6, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"PShift",       3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"ModAHD",       5, volumeTypeNone,     ledTypeNo },
    {"2-In",         2, volumeTypeNone,     ledTypeNo },
    {"4-In",         2, volumeTypeNone,     ledTypeNo },
    {"DlySingleA",   2, volumeTypeNone,     ledTypeNo },
    {"DlySingleB",   2, volumeTypeNone,     ledTypeNo },
    {"DelayDual",    3, volumeTypeNone,     ledTypeNo },
    {"DelayQuad",    5, volumeTypeNone,     ledTypeNo },
    {"DelayA",       3, volumeTypeNone,     ledTypeNo },
    {"DelayB",       4, volumeTypeNone,     ledTypeNo },
    {"DlyClock",     2, volumeTypeNone,     ledTypeNo },
    {"DlyShiftReg",  2, volumeTypeNone,     ledTypeNo },
    {"Operator",    12, volumeTypeNone,     ledTypeNo },
    {"DlyEight",     3, volumeTypeNone,     ledTypeNo },
    {"DlyStereo",    5, volumeTypeNone,     ledTypeNo },
    {"OscPM",        3, volumeTypeNone,     ledTypeNo },
    {"Mix1-1A",      2, volumeTypeNone,     ledTypeNo },
    {"Mix1-1S",      2, volumeTypeNone,     ledTypeNo },
    {"Sw1-2M",       2, volumeTypeNone,     ledTypeNo },
    {"Sw2-1M",       2, volumeTypeNone,     ledTypeNo },
    {"ConstSwM",     2, volumeTypeNone,     ledTypeNo },
    {"NoiseGate",    3, volumeTypeNone,     ledTypeNo },
    {"LfoB",         4, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"Phaser",       2, volumeTypeNone,     ledTypeNo },
    {"Mix4-1A",      2, volumeTypeNone,     ledTypeNo },
    {"Mix2-1A",      2, volumeTypeNone,     ledTypeNo },
    {"ModAmt",       2, volumeTypeNone,     ledTypeNo },
    {"OscPerc",      3, volumeTypeNone,     ledTypeNo },
    {"Status",       2, volumeTypeNone,     ledTypeNo },
    {"PitchTrack",   2, volumeTypeNone,     ledTypeNo },
    {"MonoKey",      2, volumeTypeNone,     ledTypeNo },
    {"RandomA",      2, volumeTypeNone,     ledTypeNo },
    {"Red2Blue",     2, volumeTypeNone,     ledTypeNo },
    {"RandomB",      3, volumeTypeNone,     ledTypeNo },
    {"Blue2Red",     2, volumeTypeNone,     ledTypeNo },
    {"RndClkA",      2, volumeTypeNone,     ledTypeNo },
    {"RndTrig",      2, volumeTypeNone,     ledTypeNo },
    {"RndClkB",      3, volumeTypeNone,     ledTypeNo },
    {"Unknown",      0, volumeTypeNone,     ledTypeNo },
    {"RndPattern",   3, volumeTypeNone,     ledTypeNo }, };

// moduleType, paramType, offsetX, offsetY, label, range, defaultValue, string map
const tParamLocation paramLocationList[] = {
    {moduleTypeOscShpB,    paramType1CommonDial,         paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128, 64, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1CommonDial,         paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1OffOnKeyboardTrack, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap},
    {moduleTypeOscShpB,    paramType1CommonDial,         paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Env",     128,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1PitchType,          paramType2Toggle, {{ 30,  15}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          },
    {moduleTypeOscShpB,    paramType1CommonDial,         paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  "FM",      128,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1CommonDial,         paramType2Dial,   {{-15,   5}, {7, 14}}, anchorTopRight,    "Shape",   128,  0, NULL,               NULL          }, // 50% to 99%, not decimals
    {moduleTypeOscShpB,    paramType1CommonDial,         paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "ShpCtrl", 128,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1Bypass,             paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, "Bypass",    2,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1FmType,             paramType2Toggle, {{ 60,  15}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, fmTypeStrMap,       NULL          },
    {moduleTypeFltClassic, paramType1Freq,               paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },
    {moduleTypeFltClassic, paramType1Pitch,              paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "Env",     128,  0, NULL,               NULL          },
    {moduleTypeFltClassic, paramType1OffTo100KbTrack,    paramType2Toggle, {{ 25, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          },
    {moduleTypeFltClassic, paramType1Resonance,          paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  "Res",     128,  0, NULL,               NULL          },
    {moduleTypeFltClassic, paramType1FltClassicDb,       paramType2Toggle, {{ 70, -10}, {7,  7}}, anchorBottomLeft,  NULL,        3,  0, fltClassicDbStrMap, NULL          },
    {moduleTypeFltClassic, paramType1Bypass,             paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, NULL,        2,  1, NULL,               NULL          },
    {moduleTypeFltMulti,   paramType1Freq,               paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },
    {moduleTypeFltMulti,   paramType1Pitch,              paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Env",     128,  0, NULL,               NULL          },
    {moduleTypeFltMulti,   paramType1OffTo100KbTrack,    paramType2Toggle, {{ 25, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          },
    {moduleTypeFltMulti,   paramType1Resonance,          paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  "Res",     128,  0, NULL,               NULL          },
    {moduleTypeFltMulti,   paramType1GainControl,        paramType2Toggle, {{ 55,   5}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, gcStrMap,           offOnColourMap},
    {moduleTypeFltMulti,   paramType1FltMultiDb,         paramType2Toggle, {{ 70, -10}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, fltMultiDbStrMap,   NULL          },
    {moduleTypeFltMulti,   paramType1Bypass,             paramType2Toggle, {{-10,   0}, {5,  5}}, anchorMiddleRight, NULL,        2,  1, NULL,               NULL          },
    {moduleTypeMix4to1C,   paramType1CommonDial,         paramType2Dial,   {{ 20,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },
    {moduleTypeMix4to1C,   paramType1CommonDial,         paramType2Dial,   {{ 35,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },
    {moduleTypeMix4to1C,   paramType1CommonDial,         paramType2Dial,   {{ 50,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },
    {moduleTypeMix4to1C,   paramType1CommonDial,         paramType2Dial,   {{ 65,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },
    {moduleTypeMix4to1C,   paramType1Enable,             paramType2Toggle, {{ 26,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap},
    {moduleTypeMix4to1C,   paramType1Enable,             paramType2Toggle, {{ 41,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap},
    {moduleTypeMix4to1C,   paramType1Enable,             paramType2Toggle, {{ 56,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap},
    {moduleTypeMix4to1C,   paramType1Enable,             paramType2Toggle, {{ 71,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap},
    {moduleTypeMix4to1C,   paramType1Pad,                paramType2Toggle, {{  3,  20}, {7,  7}}, anchorTopLeft,     "Pad",       2,  0, padStrMap,          NULL          },
    {moduleTypeMix4to1C,   paramType1Exp,                paramType2Toggle, {{  3,  10}, {7,  7}}, anchorTopLeft,     NULL,        3,  0, expStrMap,          NULL          },
    {moduleTypeEnvADSR,    paramType1CommonDial,         paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Attack",  128,  0, NULL,               NULL          },
    {moduleTypeEnvADSR,    paramType1CommonDial,         paramType2Dial,   {{ 35,  -3}, {7, 14}}, anchorBottomLeft,  "Delay",   128,  0, NULL,               NULL          },
    {moduleTypeEnvADSR,    paramType1CommonDial,         paramType2Dial,   {{ 50,  -3}, {7, 14}}, anchorBottomLeft,  "Sus",     128,  0, NULL,               NULL          },
    {moduleTypeEnvADSR,    paramType1CommonDial,         paramType2Dial,   {{ 65,  -3}, {7, 14}}, anchorBottomLeft,  "Rel",     128,  0, NULL,               NULL          },
    {moduleTypeCompress,   paramType1CommonDial,         paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "Thr",      43,  0, NULL,               NULL          },
    {moduleTypeCompress,   paramType1CommonDial,         paramType2Dial,   {{ 24,  -3}, {7, 14}}, anchorBottomLeft,  "2",        67,  0, NULL,               NULL          },
    {moduleTypeCompress,   paramType1CommonDial,         paramType2Dial,   {{ 38,  -3}, {7, 14}}, anchorBottomLeft,  "Att",     128,  0, NULL,               NULL          },
    {moduleTypeCompress,   paramType1CommonDial,         paramType2Dial,   {{ 52,  -3}, {7, 14}}, anchorBottomLeft,  "Rel",     128,  0, NULL,               NULL          },
    {moduleTypeCompress,   paramType1CommonDial,         paramType2Dial,   {{ 66,  -3}, {7, 14}}, anchorBottomLeft,  "RefLvl",   43,  0, NULL,               NULL          },
    {moduleTypeCompress,   paramType1Sidechain,          paramType2Toggle, {{ 45,  20}, {7, 14}}, anchorTopLeft,     NULL,        2,  0, NULL,               NULL          },
    {moduleTypeCompress,   paramType1Bypass,             paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, NULL,        2,  1, NULL,               NULL          }, };

// Note these need to be in same order of connectors which are referenced by the hardware
const tConnectorLocation connectorLocationList[] = {
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   {{ 73,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeOscShpB,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   },
    {moduleTypeFltClassic, connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl, {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   },
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl, {{  3,  30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeMix4to1C,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 20,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 35,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 50,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 65,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl, {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   },
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeLogic, {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft },
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeStChorus,   connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   },
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   },
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   },
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   {{ 20,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft    },
    {moduleTypeCompress,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleTypeCompress,   connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight},
    {moduleType2toOut,     connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   },
    {moduleType2toOut,     connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight   }};

// moduleType, paramType, offsetX, offsetY, label, range, defaultValue, string map
const tModeLocation   modeLocationList[] = {
    {moduleTypeOscShpB, paramType1OscWave, paramType2Dial, {{20, 5}, {7, 14}}, anchorTopLeft, "Wave", 8, 0, NULL}};

const tVolumeLocation volumeLocationList[] = {
    {moduleTypeMix4to1C, volumeTypeMono,     {{-15, 5}, {3, 25}}, anchorTopRight},
    {moduleTypeCompress, volumeTypeCompress, {{-18, 5}, {3, 38}}, anchorTopRight},
    {moduleType2toOut,   volumeTypeStereo,   {{-25, 5}, {3, 10}}, anchorTopRight}};

const tLedLocation    ledLocationList[] = {
    {moduleTypeEnvADSR, ledTypeYes, {{3, 8}, {3, 3}}, anchorTopLeft}, };

#endif // __MODULE_RESOURCES_H__
