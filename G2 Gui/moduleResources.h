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

const char * emptyMap[]        = {""};
const char * fltClassicDbMap[] = {"12db", "18db", "24db"};
const char * fltMultiDbMap[]   = {"6db", "12db"};
const char * filterKbMap[]     = {"Off", "25%", "50%", "75%", "100%"};

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
    {0.2, 0.2, 0.7}
};

//TODO: see if we can make this const and only reference in one source file
tModuleProperties gModuleProperties[] = {
    // Array index = module type for access speed
    {"Unknown",      0,  0, 0},
    {"Keyboard",     2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"4-Out",        2,  0, 0},
    {"2-Out",        2,  0, 0},
    {"Invert",       2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"OscB",         5,  0, 0},
    {"OscShpB",      4, 10, 1},
    {"OscC",         3,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Reverb",       3,  0, 0},
    {"OscString",    3,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Sw8-1",        4,  0, 0},
    {"Unknown",      0,  0, 0},
    {"ValSw1-2",     2,  0, 0},
    {"X-Fade",       2,  0, 0},
    {"Mix4-1B",      2,  0, 0},
    {"EnvADSR",      4,  8, 5},
    {"Mux1-8",       2,  0, 0},
    {"PartQuant",    2,  0, 0},
    {"ModADSR",      5,  0, 0},
    {"LfoC",         2,  0, 0},
    {"LfoShpA",      5,  0, 0},
    {"LfoA",         3,  0, 0},
    {"OscMaster",    3,  0, 0},
    {"Saturate",     2,  0, 0},
    {"MetNoise",     2,  0, 0},
    {"Device",       3,  0, 0},
    {"Noise",        2,  0, 0},
    {"Eq2Band",      3,  0, 0},
    {"Eq3band",      4,  0, 0},
    {"ShpExp",       2,  0, 0},
    {"Driver",       3,  0, 0},
    {"SwOnOffM",     2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Pulse",        2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Mix8-1B",      4,  0, 0},
    {"EnvH",         2,  0, 0},
    {"Delay",        2,  0, 0},
    {"Constant",     2,  0, 0},
    {"LevMult",      2,  0, 0},
    {"FltVoice",     4,  0, 0},
    {"EnvAHD",       4,  0, 0},
    {"Pan",          2,  0, 0},
    {"MixStereo",    5,  0, 0},
    {"FltMulti",     4,  7, 6},
    {"ConstSwT",     2,  0, 0},
    {"FltNord",      5,  0, 0},
    {"EnvMulti",     6,  0, 0},
    {"SandH",        2,  0, 0},
    {"FltStatic",    3,  0, 0},
    {"EnvD",         2,  0, 0},
    {"Resonator",    5,  0, 0},
    {"Automate",     2,  0, 0},
    {"DrumSynth",    8,  0, 0},
    {"CompLev",      2,  0, 0},
    {"Mux8-1X",      3,  0, 0},
    {"Clip",         2,  0, 0},
    {"Overdrive",    2,  0, 0},
    {"Scratch",      3,  0, 0},
    {"Gate",         2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Mix2-1B",      2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"ClkGen",       4,  0, 0},
    {"ClkDiv",       2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"EnvFollow",    2,  0, 0},
    {"NoteScaler",   2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"WaveWrap",     2,  0, 0},
    {"NoteQuant",    2,  0, 0},
    {"SwOnOffT",     2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Sw1-8",        4,  0, 0},
    {"Sw4-1",        3,  0, 0},
    {"Unknown",      0,  0, 0},
    {"LevAmp",       2,  0, 0},
    {"Rect",         2,  0, 0},
    {"ShpStatic",    2,  0, 0},
    {"EnvAD",        3,  0, 0},
    {"WindSw",       2,  0, 0},
    {"8Counter",     2,  0, 0},
    {"FltLP",        2,  0, 0},
    {"Sw1-4",        3,  0, 0},
    {"Flanger",      3,  0, 0},
    {"Sw1-2",        2,  0, 0},
    {"FlipFlop",     2,  0, 0},
    {"FltClassic",   4,  6, 4},
    {"Unknown",      0,  0, 0},
    {"StChorus",     3,  3, 3},
    {"Unknown",      0,  0, 0},
    {"OscD",         2,  0, 0},
    {"OscA",         3,  0, 0},
    {"FreqShift",    3,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Sw2-1",        2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"FltPhase",     5,  0, 0},
    {"EqPeak",       4,  0, 0},
    {"Unknown",      0,  0, 0},
    {"ValSw2-1",     2,  0, 0},
    {"OscNoise",     3,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Vocoder",      8,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Unknown",      0,  0, 0},
    {"LevAdd",       2,  0, 0},
    {"Fade1-2",      2,  0, 0},
    {"Fade2-1",      2,  0, 0},
    {"LevScaler",    3,  0, 0},
    {"Mix8-1A",      2,  0, 0},
    {"LevMod",       3,  0, 0},
    {"Digitizer",    3,  0, 0},
    {"EnvADDSR",     5,  0, 0},
    {"Unknown",      0,  0, 0},
    {"SeqNote",      9,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Mix4-1C",      4, 10, 5},
    {"Mux8-1",       2,  0, 0},
    {"WahWah",       2,  0, 0},
    {"Name",         1,  0, 0},
    {"Fx-In",        2,  0, 0},
    {"MinMax",       2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"BinCounter",   2,  0, 0},
    {"ADConv",       2,  0, 0},
    {"DAConv",       2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"FltHP",        2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Unknown",      0,  0, 0},
    {"T&H",          2,  0, 0},
    {"Mix4-1S",      4,  0, 0},
    {"CtrlSend",     2,  0, 0},
    {"PCSend",       2,  0, 0},
    {"NoteSend",     2,  0, 0},
    {"SeqEvent",     5,  0, 0},
    {"SeqVal",       8,  0, 0},
    {"SeqLev",       8,  0, 0},
    {"CtrlRcv",      2,  0, 0},
    {"NoteRcv",      2,  0, 0},
    {"NoteZone",     3,  0, 0},
    {"Compress",     5,  7, 2},
    {"Unknown",      0,  0, 0},
    {"KeyQuant",     2,  0, 0},
    {"Unknown",      0,  0, 0},
    {"SeqCtr",       8,  0, 0},
    {"Unknown",      0,  0, 0},
    {"NoteDet",      2,  0, 0},
    {"LevConv",      2,  0, 0},
    {"Glide",        2,  0, 0},
    {"CompSig",      2,  0, 0},
    {"ZeroCnt",      2,  0, 0},
    {"MixFader",     9,  0, 0},
    {"FltComb",      4,  0, 0},
    {"OscShpA",      5,  0, 0},
    {"OscDual",      5,  0, 0},
    {"DXRouter",     6,  0, 0},
    {"Unknown",      0,  0, 0},
    {"PShift",       3,  0, 0},
    {"Unknown",      0,  0, 0},
    {"ModAHD",       5,  0, 0},
    {"2-In",         2,  0, 0},
    {"4-In",         2,  0, 0},
    {"DlySingleA",   2,  0, 0},
    {"DlySingleB",   2,  0, 0},
    {"DelayDual",    3,  0, 0},
    {"DelayQuad",    5,  0, 0},
    {"DelayA",       3,  0, 0},
    {"DelayB",       4,  0, 0},
    {"DlyClock",     2,  0, 0},
    {"DlyShiftReg",  2,  0, 0},
    {"Operator",    12,  0, 0},
    {"DlyEight",     3,  0, 0},
    {"DlyStereo",    5,  0, 0},
    {"OscPM",        3,  0, 0},
    {"Mix1-1A",      2,  0, 0},
    {"Mix1-1S",      2,  0, 0},
    {"Sw1-2M",       2,  0, 0},
    {"Sw2-1M",       2,  0, 0},
    {"ConstSwM",     2,  0, 0},
    {"NoiseGate",    3,  0, 0},
    {"LfoB",         4,  0, 0},
    {"Unknown",      0,  0, 0},
    {"Phaser",       2,  0, 0},
    {"Mix4-1A",      2,  0, 0},
    {"Mix2-1A",      2,  0, 0},
    {"ModAmt",       2,  0, 0},
    {"OscPerc",      3,  0, 0},
    {"Status",       2,  0, 0},
    {"PitchTrack",   2,  0, 0},
    {"MonoKey",      2,  0, 0},
    {"RandomA",      2,  0, 0},
    {"Red2Blue",     2,  0, 0},
    {"RandomB",      3,  0, 0},
    {"Blue2Red",     2,  0, 0},
    {"RndClkA",      2,  0, 0},
    {"RndTrig",      2,  0, 0},
    {"RndClkB",      3,  0, 0},
    {"Unknown",      0,  0, 0},
    {"RndPattern",   3,  0, 0}, };

void render_param_common(tCoord coord, uint32_t paramRef, uint32_t param, tModule * module);

tParamLocation paramLocationList[] = {
    {moduleTypeFltClassic, paramTypeFreq,          120.0, 80.0, "Freq",   128},
    {moduleTypeFltClassic, paramTypePitch,          30.0, 80.0, "Env",    128},
    {moduleTypeFltClassic, paramTypeKeyboardTrack,  75.0, 80.0, "Kbt",      5},
    {moduleTypeFltClassic, paramTypeResonance,     175.0, 80.0, "Res",    128},
    {moduleTypeFltClassic, paramTypeFltClassicDb,  210.0, 80.0, "",         3},
    {moduleTypeFltClassic, paramTypeBypass,        245.0, 50.0, "",         2},
    {moduleTypeFltMulti,   paramTypeFreq,          140.0, 80.0, "Freq",   128},
    {moduleTypeFltMulti,   paramTypePitch,          30.0, 80.0, "Env",    128},
    {moduleTypeFltMulti,   paramTypeKeyboardTrack,  75.0, 80.0, "Kbt",      5},
    {moduleTypeFltMulti,   paramTypeResonance,     200.0, 80.0, "Res",    128},
    {moduleTypeFltMulti,   paramTypeGainControl,   180.0, 25.0, "",         2},
    {moduleTypeFltMulti,   paramTypeFltMultiDb,    210.0, 25.0, "",         2},
    {moduleTypeFltMulti,   paramTypeBypass,        230.0, 80.0, "",         2},
    {moduleTypeEnvADSR,    paramTypeCommonDial,     40.0, 80.0, "Attack", 128},
    {moduleTypeEnvADSR,    paramTypeCommonDial,     80.0, 80.0, "Delay",  128},
    {moduleTypeEnvADSR,    paramTypeCommonDial,    120.0, 80.0, "Sus",    128},
    {moduleTypeEnvADSR,    paramTypeCommonDial,    160.0, 80.0, "Rel",    128}, };

void render_connector(tModule * module, uint32_t connectorIndex, tConnectorDir dir, tConnectorType type, tCoord coord);

const tConnectorLocation connectorLocationList[] = {
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeAudio,   255.0,  20.0},
    {moduleTypeFltClassic, connectorDirOut, connectorTypeAudio,   255.0,  95.0},
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl,  15.0,  95.0},
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl,  15.0,  50.0},
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeAudio,   255.0,  20.0},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   255.0,  60.0},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   255.0,  80.0},
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   255.0, 100.0},
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl,  15.0,  95.0},
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl,  15.0,  50.0},
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl,   255.0,  20.0},
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl,  15.0,  75.0},
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl,  15.0,  95.0},
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl, 230.0,  95.0},
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl,   255.0,  95.0},
    {moduleTypeMix4to1C,   connectorDirOut, connectorTypeControl,   255.0,  95.0},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl,    60.0,  80.0},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl,    80.0,  80.0},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl,   100.0,  80.0},
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl,   120.0,  80.0},
    {moduleTypeOscShpB,    connectorDirOut, connectorTypeAudio,   150.0,  80.0},
    {moduleTypeStChorus,   connectorDirIn,  connectorTypeAudio,   255.0,  20.0},
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   235.0,  65.0},
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   255.0,  65.0},
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   235.0,  20.0},
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   255.0,  20.0}, };

#endif // __MODULE_RESOURCES_H__
