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


#ifndef __MODULE_RESOURCES_H__
#define __MODULE_RESOURCES_H__

#include "sysIncludes.h"

// mapping of values to strings etc.

const char * filterDbMap[] = {"12db", "18db", "24db"};
const char * fltMultiDbMap[] = {"6db", "12db"};
const char * filterKbMap[] = {"Off", "25%", "50%", "75%", "100%"};


tRgb cableColourMap[] =
{
    {0.7, 0.1, 0.1},     // red
    {0.8, 0.3, 0.2},     // orange
    {0.1, 0.1, 0.7},     // blue
    {0.7, 0.7, 0.1},     // yellow
    {0.8, 0.3, 0.2},     // orange
    {0.1, 0.7, 0.1},     // green
    {0.7, 0.1, 0.7},     // purple
    {0.7, 0.7, 0.7}      // white
};

tRgb gModuleColourMap[] =
{
    {0.7, 0.7, 0.7},     // standard grey
    {0.8, 0.6, 0.6},     // red 4
    {0.6, 0.8, 0.6},     // green 4
    {0.6, 0.6, 0.8},     // blue 4
    {0.8, 0.8, 0.6},     // yellow 4
    {0.3, 0.3, 0.8},     // blue 1
    {0.8, 0.3, 0.3},     // red 1
    {0.4, 0.8, 0.8},     // cyan 2
    {0.4, 0.8, 0.4},     // green 2
    {0.8, 0.8, 0.3},     // yellow 1
    {0.3, 0.8, 0.3},     // green 1
    {0.8, 0.8, 0.4},     // yellow 2
    {0.5, 0.5, 0.8},     // blue 3
    {0.8, 0.4, 0.4},     // red 2
    {0.8, 0.5, 0.5},     // red 3
    {0.8, 0.8, 0.5},     // yellow 3
    {0.5, 0.8, 0.5},     // green 3
    {0.3, 0.8, 0.8},     // cyan 1
    {0.5, 0.8, 0.8},     // cyan 3
    {0.6, 0.8, 0.8},     // cyan 4
    {0.4, 0.4, 0.8},     // blue 2
    {0.8, 0.3, 0.8},     // purple 1
    {0.8, 0.4, 0.8},     // purple 2
    {0.8, 0.5, 0.8},     // purple 3
    {0.8, 0.6, 0.8},     // purple 4
};

void render_OscShpB(tRectangle, tModule *);
void render_EnvAdsr(tRectangle, tModule *);
void render_FltMulti(tRectangle, tModule *);
void render_FltClassic(tRectangle, tModule *);
void render_StChorus(tRectangle, tModule *);
void render_Mix4to1c(tRectangle, tModule *);
void render_Compress(tRectangle, tModule *);

typedef struct {
    const char *   name;
    const uint32_t height;
    void (*renderFunction)(tRectangle, tModule *);
} tModuleProperties;

tModuleProperties gModuleProperties[] =
{
    // Array index = module type for access speed
    {"Unknown",      0, NULL},
    {"Keyboard",     2, NULL},
    {"Unknown",      0, NULL},
    {"4-Out",        2, NULL},
    {"2-Out",        2, NULL},
    {"Invert",       2, NULL},
    {"Unknown",      0, NULL},
    {"OscB",         5, NULL},
    {"OscShpB",      4, render_OscShpB},
    {"OscC",         3, NULL},
    {"Unknown",      0, NULL},
    {"Unknown",      0, NULL},
    {"Reverb",       3, NULL},
    {"OscString",    3, NULL},
    {"Unknown",      0, NULL},
    {"Sw8-1",        4, NULL},
    {"Unknown",      0, NULL},
    {"ValSw1-2",     2, NULL},
    {"X-Fade",       2, NULL},
    {"Mix4-1B",      2, NULL},
    {"EnvADSR",      4, render_EnvAdsr},
    {"Mux1-8",       2, NULL},
    {"PartQuant",    2, NULL},
    {"ModADSR",      5, NULL},
    {"LfoC",         2, NULL},
    {"LfoShpA",      5, NULL},
    {"LfoA",         3, NULL},
    {"OscMaster",    3, NULL},
    {"Saturate",     2, NULL},
    {"MetNoise",     2, NULL},
    {"Device",       3, NULL},
    {"Noise",        2, NULL},
    {"Eq2Band",      3, NULL},
    {"Eq3band",      4, NULL},
    {"ShpExp",       2, NULL},
    {"Driver",       3, NULL},
    {"SwOnOffM",     2, NULL},
    {"Unknown",      0, NULL},
    {"Pulse",        2, NULL},
    {"Unknown",      0, NULL},
    {"Mix8-1B",      4, NULL},
    {"EnvH",         2, NULL},
    {"Delay",        2, NULL},
    {"Constant",     2, NULL},
    {"LevMult",      2, NULL},
    {"FltVoice",     4, NULL},
    {"EnvAHD",       4, NULL},
    {"Pan",          2, NULL},
    {"MixStereo",    5, NULL},
    {"FltMulti",     4, render_FltMulti},
    {"ConstSwT",     2, NULL},
    {"FltNord",      5, NULL},
    {"EnvMulti",     6, NULL},
    {"SandH",        2, NULL},
    {"FltStatic",    3, NULL},
    {"EnvD",         2, NULL},
    {"Resonator",    5, NULL},
    {"Automate",     2, NULL},
    {"DrumSynth",    8, NULL},
    {"CompLev",      2, NULL},
    {"Mux8-1X",      3, NULL},
    {"Clip",         2, NULL},
    {"Overdrive",    2, NULL},
    {"Scratch",      3, NULL},
    {"Gate",         2, NULL},
    {"Unknown",      0, NULL},
    {"Mix2-1B",      2, NULL},
    {"Unknown",      0, NULL},
    {"ClkGen",       4, NULL},
    {"ClkDiv",       2, NULL},
    {"Unknown",      0, NULL},
    {"EnvFollow",    2, NULL},
    {"NoteScaler",   2, NULL},
    {"Unknown",      0, NULL},
    {"WaveWrap",     2, NULL},
    {"NoteQuant",    2, NULL},
    {"SwOnOffT",     2, NULL},
    {"Unknown",      0, NULL},
    {"Sw1-8",        4, NULL},
    {"Sw4-1",        3, NULL},
    {"Unknown",      0, NULL},
    {"LevAmp",       2, NULL},
    {"Rect",         2, NULL},
    {"ShpStatic",    2, NULL},
    {"EnvAD",        3, NULL},
    {"WindSw",       2, NULL},
    {"8Counter",     2, NULL},
    {"FltLP",        2, NULL},
    {"Sw1-4",        3, NULL},
    {"Flanger",      3, NULL},
    {"Sw1-2",        2, NULL},
    {"FlipFlop",     2, NULL},
    {"FltClassic",   4, render_FltClassic},
    {"Unknown",      0, NULL},
    {"StChorus",     3, render_StChorus},
    {"Unknown",      0, NULL},
    {"OscD",         2, NULL},
    {"OscA",         3, NULL},
    {"FreqShift",    3, NULL},
    {"Unknown",      0, NULL},
    {"Sw2-1",        2, NULL},
    {"Unknown",      0, NULL},
    {"FltPhase",     5, NULL},
    {"EqPeak",       4, NULL},
    {"Unknown",      0, NULL},
    {"ValSw2-1",     2, NULL},
    {"OscNoise",     3, NULL},
    {"Unknown",      0, NULL},
    {"Vocoder",      8, NULL},
    {"Unknown",      0, NULL},
    {"Unknown",      0, NULL},
    {"Unknown",      0, NULL},
    {"LevAdd",       2, NULL},
    {"Fade1-2",      2, NULL},
    {"Fade2-1",      2, NULL},
    {"LevScaler",    3, NULL},
    {"Mix8-1A",      2, NULL},
    {"LevMod",       3, NULL},
    {"Digitizer",    3, NULL},
    {"EnvADDSR",     5, NULL},
    {"Unknown",      0, NULL},
    {"SeqNote",      9, NULL},
    {"Unknown",      0, NULL},
    {"Mix4-1C",      4, render_Mix4to1c},
    {"Mux8-1",       2, NULL},
    {"WahWah",       2, NULL},
    {"Name",         1, NULL},
    {"Fx-In",        2, NULL},
    {"MinMax",       2, NULL},
    {"Unknown",      0, NULL},
    {"BinCounter",   2, NULL},
    {"ADConv",       2, NULL},
    {"DAConv",       2, NULL},
    {"Unknown",      0, NULL},
    {"FltHP",        2, NULL},
    {"Unknown",      0, NULL},
    {"Unknown",      0, NULL},
    {"Unknown",      0, NULL},
    {"Unknown",      0, NULL},
    {"T&H",          2, NULL},
    {"Mix4-1S",      4, NULL},
    {"CtrlSend",     2, NULL},
    {"PCSend",       2, NULL},
    {"NoteSend",     2, NULL},
    {"SeqEvent",     5, NULL},
    {"SeqVal",       8, NULL},
    {"SeqLev",       8, NULL},
    {"CtrlRcv",      2, NULL},
    {"NoteRcv",      2, NULL},
    {"NoteZone",     3, NULL},
    {"Compress",     5, render_Compress},
    {"Unknown",      0, NULL},
    {"KeyQuant",     2, NULL},
    {"Unknown",      0, NULL},
    {"SeqCtr",       8, NULL},
    {"Unknown",      0, NULL},
    {"NoteDet",      2, NULL},
    {"LevConv",      2, NULL},
    {"Glide",        2, NULL},
    {"CompSig",      2, NULL},
    {"ZeroCnt",      2, NULL},
    {"MixFader",     9, NULL},
    {"FltComb",      4, NULL},
    {"OscShpA",      5, NULL},
    {"OscDual",      5, NULL},
    {"DXRouter",     6, NULL},
    {"Unknown",      0, NULL},
    {"PShift",       3, NULL},
    {"Unknown",      0, NULL},
    {"ModAHD",       5, NULL},
    {"2-In",         2, NULL},
    {"4-In",         2, NULL},
    {"DlySingleA",   2, NULL},
    {"DlySingleB",   2, NULL},
    {"DelayDual",    3, NULL},
    {"DelayQuad",    5, NULL},
    {"DelayA",       3, NULL},
    {"DelayB",       4, NULL},
    {"DlyClock",     2, NULL},
    {"DlyShiftReg",  2, NULL},
    {"Operator",    12, NULL},
    {"DlyEight",     3, NULL},
    {"DlyStereo",    5, NULL},
    {"OscPM",        3, NULL},
    {"Mix1-1A",      2, NULL},
    {"Mix1-1S",      2, NULL},
    {"Sw1-2M",       2, NULL},
    {"Sw2-1M",       2, NULL},
    {"ConstSwM",     2, NULL},
    {"NoiseGate",    3, NULL},
    {"LfoB",         4, NULL},
    {"Unknown",      0, NULL},
    {"Phaser",       2, NULL},
    {"Mix4-1A",      2, NULL},
    {"Mix2-1A",      2, NULL},
    {"ModAmt",       2, NULL},
    {"OscPerc",      3, NULL},
    {"Status",       2, NULL},
    {"PitchTrack",   2, NULL},
    {"MonoKey",      2, NULL},
    {"RandomA",      2, NULL},
    {"Red2Blue",     2, NULL},
    {"RandomB",      3, NULL},
    {"Blue2Red",     2, NULL},
    {"RndClkA",      2, NULL},
    {"RndTrig",      2, NULL},
    {"RndClkB",      3, NULL},
    {"Unknown",      0, NULL},
    {"RndPattern",   3, NULL},
    {"MaxVal",       0}
};

tRgb connectorColourMap[] = {
    {0.5, 0.1, 0.1},  // Audio In
    {0.7, 0.1, 0.1},  // Audio Out
    {0.5, 0.1, 0.5},  // Control In
    {0.7, 0.1, 0.7}   // Control Out
};

#endif // __MODULE_RESOURCES_H__
