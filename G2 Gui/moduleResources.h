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

// mapping of values to strings etc.
#define MAP_NUM_ITEMS(map)    (sizeof(map) / sizeof(map[0]))

char *            filterDbMap[]   = {"12db", "18db", "24db"};
char *            fltMultiDbMap[] = {"6db", "12db"};
char *            filterKbMap[]   = {"Off", "25%", "50%", "75%", "100%"};

tRgb              cableColourMap[] = {
    {0.7, 0.1, 0.1},       // red
    {0.1, 0.1, 0.7},       // blue
    {0.7, 0.7, 0.1},       // yellow
    {0.8, 0.3, 0.2},       // orange
    {0.1, 0.7, 0.1},       // green
    {0.7, 0.1, 0.7},       // purple
    {0.9, 0.9, 0.9}        // white
};

tRgb              gModuleColourMap[] = {
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

void              render_OscShpB(tRectangle, tModule *);
void              render_EnvAdsr(tRectangle, tModule *);
void              render_FltMulti(tRectangle, tModule *);
void              render_FltClassic(tRectangle, tModule *);
void              render_StChorus(tRectangle, tModule *);
void              render_Mix4to1c(tRectangle, tModule *);
void              render_Compress(tRectangle, tModule *);

tModuleProperties gModuleProperties[] = {
    // Array index = module type for access speed
    {"Unknown",      0, 0, NULL             },
    {"Keyboard",     2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"4-Out",        2, 0, NULL             },
    {"2-Out",        2, 0, NULL             },
    {"Invert",       2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"OscB",         5, 0, NULL             },
    {"OscShpB",      4, 1, render_OscShpB   },
    {"OscC",         3, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Reverb",       3, 0, NULL             },
    {"OscString",    3, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Sw8-1",        4, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"ValSw1-2",     2, 0, NULL             },
    {"X-Fade",       2, 0, NULL             },
    {"Mix4-1B",      2, 0, NULL             },
    {"EnvADSR",      4, 5, render_EnvAdsr   },
    {"Mux1-8",       2, 0, NULL             },
    {"PartQuant",    2, 0, NULL             },
    {"ModADSR",      5, 0, NULL             },
    {"LfoC",         2, 0, NULL             },
    {"LfoShpA",      5, 0, NULL             },
    {"LfoA",         3, 0, NULL             },
    {"OscMaster",    3, 0, NULL             },
    {"Saturate",     2, 0, NULL             },
    {"MetNoise",     2, 0, NULL             },
    {"Device",       3, 0, NULL             },
    {"Noise",        2, 0, NULL             },
    {"Eq2Band",      3, 0, NULL             },
    {"Eq3band",      4, 0, NULL             },
    {"ShpExp",       2, 0, NULL             },
    {"Driver",       3, 0, NULL             },
    {"SwOnOffM",     2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Pulse",        2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Mix8-1B",      4, 0, NULL             },
    {"EnvH",         2, 0, NULL             },
    {"Delay",        2, 0, NULL             },
    {"Constant",     2, 0, NULL             },
    {"LevMult",      2, 0, NULL             },
    {"FltVoice",     4, 0, NULL             },
    {"EnvAHD",       4, 0, NULL             },
    {"Pan",          2, 0, NULL             },
    {"MixStereo",    5, 0, NULL             },
    {"FltMulti",     4, 6, render_FltMulti  },
    {"ConstSwT",     2, 0, NULL             },
    {"FltNord",      5, 0, NULL             },
    {"EnvMulti",     6, 0, NULL             },
    {"SandH",        2, 0, NULL             },
    {"FltStatic",    3, 0, NULL             },
    {"EnvD",         2, 0, NULL             },
    {"Resonator",    5, 0, NULL             },
    {"Automate",     2, 0, NULL             },
    {"DrumSynth",    8, 0, NULL             },
    {"CompLev",      2, 0, NULL             },
    {"Mux8-1X",      3, 0, NULL             },
    {"Clip",         2, 0, NULL             },
    {"Overdrive",    2, 0, NULL             },
    {"Scratch",      3, 0, NULL             },
    {"Gate",         2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Mix2-1B",      2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"ClkGen",       4, 0, NULL             },
    {"ClkDiv",       2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"EnvFollow",    2, 0, NULL             },
    {"NoteScaler",   2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"WaveWrap",     2, 0, NULL             },
    {"NoteQuant",    2, 0, NULL             },
    {"SwOnOffT",     2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Sw1-8",        4, 0, NULL             },
    {"Sw4-1",        3, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"LevAmp",       2, 0, NULL             },
    {"Rect",         2, 0, NULL             },
    {"ShpStatic",    2, 0, NULL             },
    {"EnvAD",        3, 0, NULL             },
    {"WindSw",       2, 0, NULL             },
    {"8Counter",     2, 0, NULL             },
    {"FltLP",        2, 0, NULL             },
    {"Sw1-4",        3, 0, NULL             },
    {"Flanger",      3, 0, NULL             },
    {"Sw1-2",        2, 0, NULL             },
    {"FlipFlop",     2, 0, NULL             },
    {"FltClassic",   4, 4, render_FltClassic},
    {"Unknown",      0, 0, NULL             },
    {"StChorus",     3, 3, render_StChorus  },
    {"Unknown",      0, 0, NULL             },
    {"OscD",         2, 0, NULL             },
    {"OscA",         3, 0, NULL             },
    {"FreqShift",    3, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Sw2-1",        2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"FltPhase",     5, 0, NULL             },
    {"EqPeak",       4, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"ValSw2-1",     2, 0, NULL             },
    {"OscNoise",     3, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Vocoder",      8, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"LevAdd",       2, 0, NULL             },
    {"Fade1-2",      2, 0, NULL             },
    {"Fade2-1",      2, 0, NULL             },
    {"LevScaler",    3, 0, NULL             },
    {"Mix8-1A",      2, 0, NULL             },
    {"LevMod",       3, 0, NULL             },
    {"Digitizer",    3, 0, NULL             },
    {"EnvADDSR",     5, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"SeqNote",      9, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Mix4-1C",      4, 5, render_Mix4to1c  },
    {"Mux8-1",       2, 0, NULL             },
    {"WahWah",       2, 0, NULL             },
    {"Name",         1, 0, NULL             },
    {"Fx-In",        2, 0, NULL             },
    {"MinMax",       2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"BinCounter",   2, 0, NULL             },
    {"ADConv",       2, 0, NULL             },
    {"DAConv",       2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"FltHP",        2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"T&H",          2, 0, NULL             },
    {"Mix4-1S",      4, 0, NULL             },
    {"CtrlSend",     2, 0, NULL             },
    {"PCSend",       2, 0, NULL             },
    {"NoteSend",     2, 0, NULL             },
    {"SeqEvent",     5, 0, NULL             },
    {"SeqVal",       8, 0, NULL             },
    {"SeqLev",       8, 0, NULL             },
    {"CtrlRcv",      2, 0, NULL             },
    {"NoteRcv",      2, 0, NULL             },
    {"NoteZone",     3, 0, NULL             },
    {"Compress",     5, 2, render_Compress  },
    {"Unknown",      0, 0, NULL             },
    {"KeyQuant",     2, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"SeqCtr",       8, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"NoteDet",      2, 0, NULL             },
    {"LevConv",      2, 0, NULL             },
    {"Glide",        2, 0, NULL             },
    {"CompSig",      2, 0, NULL             },
    {"ZeroCnt",      2, 0, NULL             },
    {"MixFader",     9, 0, NULL             },
    {"FltComb",      4, 0, NULL             },
    {"OscShpA",      5, 0, NULL             },
    {"OscDual",      5, 0, NULL             },
    {"DXRouter",     6, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"PShift",       3, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"ModAHD",       5, 0, NULL             },
    {"2-In",         2, 0, NULL             },
    {"4-In",         2, 0, NULL             },
    {"DlySingleA",   2, 0, NULL             },
    {"DlySingleB",   2, 0, NULL             },
    {"DelayDual",    3, 0, NULL             },
    {"DelayQuad",    5, 0, NULL             },
    {"DelayA",       3, 0, NULL             },
    {"DelayB",       4, 0, NULL             },
    {"DlyClock",     2, 0, NULL             },
    {"DlyShiftReg",  2, 0, NULL             },
    {"Operator",    12, 0, NULL             },
    {"DlyEight",     3, 0, NULL             },
    {"DlyStereo",    5, 0, NULL             },
    {"OscPM",        3, 0, NULL             },
    {"Mix1-1A",      2, 0, NULL             },
    {"Mix1-1S",      2, 0, NULL             },
    {"Sw1-2M",       2, 0, NULL             },
    {"Sw2-1M",       2, 0, NULL             },
    {"ConstSwM",     2, 0, NULL             },
    {"NoiseGate",    3, 0, NULL             },
    {"LfoB",         4, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"Phaser",       2, 0, NULL             },
    {"Mix4-1A",      2, 0, NULL             },
    {"Mix2-1A",      2, 0, NULL             },
    {"ModAmt",       2, 0, NULL             },
    {"OscPerc",      3, 0, NULL             },
    {"Status",       2, 0, NULL             },
    {"PitchTrack",   2, 0, NULL             },
    {"MonoKey",      2, 0, NULL             },
    {"RandomA",      2, 0, NULL             },
    {"Red2Blue",     2, 0, NULL             },
    {"RandomB",      3, 0, NULL             },
    {"Blue2Red",     2, 0, NULL             },
    {"RndClkA",      2, 0, NULL             },
    {"RndTrig",      2, 0, NULL             },
    {"RndClkB",      3, 0, NULL             },
    {"Unknown",      0, 0, NULL             },
    {"RndPattern",   3, 0, NULL             }, };

tRgb              connectorColourMap[2][2] =  // First index = direction, second = type
{
    {{0.5, 0.1, 0.1}, {0.5, 0.1, 0.5}},         // In: { Audio, Control }
    {{0.7, 0.1, 0.1}, {0.7, 0.1, 0.7}}          // Out: { Audio, Control }
};

tConstParameter parameter[] = {   // Experimental method of defining parameter location on a module
    {moduleTypeFltClassic, paramTypeFreq, {105.0 + FILTER_FREQ_RADIUS, 80.0}},
    {moduleTypeFltClassic, paramTypePitch, {15.0 + FILTER_FREQ_RADIUS, 80.0}},
    {moduleTypeFltClassic, paramTypeKeyboardTrack, {75.0, 80.0}},
    {moduleTypeFltClassic, paramTypeResonance, {160.0 + FILTER_FREQ_RADIUS, 80.0}},
    {moduleTypeFltClassic, paramTypeFltClassicDb, {210.0 + FILTER_FREQ_RADIUS, 80.0}},
    {moduleTypeFltClassic, paramTypeBypass, {250.0 + FILTER_FREQ_RADIUS, 50.0}},
};

tConstConnector connector[] = {    // Experimental method of defining connector location on a module
    {moduleTypeFltClassic, connectorDirIn, connectorTypeAudio, {250.0 + FILTER_FREQ_RADIUS, 20.0}},
    {moduleTypeFltClassic, connectorDirIn, connectorTypeAudio, {250.0 + FILTER_FREQ_RADIUS, 80.0}},
    {moduleTypeFltClassic, connectorDirIn, connectorTypeAudio, {15.0 + FILTER_FREQ_RADIUS, 20.0}},
    {moduleTypeFltClassic, connectorDirIn, connectorTypeAudio, {15.0 + FILTER_FREQ_RADIUS, 50.0}},
};

#endif // __MODULE_RESOURCES_H__
