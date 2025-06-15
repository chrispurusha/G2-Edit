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

const char * fltClassicDbStrMap[] = {"12db", "18db", "24db"};
const char * fltMultiDbStrMap[]   = {"6db", "12db", NULL};
const char * fltNordDbStrMap[]   = {"12dB", "24dB"};
const char * offTo100KbStrMap[]   = {"Off", "25%", "50%", "75%", "100%"};
const char * offOnStrMap[]        = {"Off", "On"};
const char * expStrMap[]          = {"Exp", "Lin", "dB"};
const char * logStrMap[]          = {"Log", "Lin"};
const char * padStrMap[]          = {"0dB", "+6dB"};
const char * db12PadStrMap[]      = {"+6dB", "0dB", "-6dB", "-12dB"};
const char * db12BPadStrMap[]     = {"0dB", "-6dB", "-12dB"};
const char * gcStrMap[]           = {"GC", "GC"};
const char * kbStrMap[]           = {"KB", "KB"};
const char * sideChainStrMap[]    = {"Side Chain", "Side Chain"};
const char * pitchTypeStrMap[]    = {"Semi", "Freq", "Factor", "Partial"};
const char * fmTypeStrMap[]       = {"FM Lin", "FM Trk"};
const char * envShapeStrMap[]     = {"LogExp", "LinExp", "ExpExp", "LinLin"};
const char * normalResetStrMap[]  = {"Normal", "Reset"};
const char * posStrMap[]          = {"Pos", "PosInv", "Neg", "NegInv", "Bip", "BipInv"};
const char * outToStrMap[]        = {"Main 1/2", "Main 3/4", "FX 1/2", "FX 3/4", "Bus 1/2", "Bus 3/4", };
const char * outTo4OutStrMap[]    = {"Main", "Fx", "Bus"};
const char * inFxStrMap[]         = {"1/2", "3/4"};
const char * shapeTypeStrMap[]    = {"sin", "tri", "saw", "squ", "sup"};
const char * shapeOscATypeStrMap[]= {"sin", "tri", "saw", "squ", "p25", "p10"};
const char * reverbTypeStrMap[]   = {"Small", "Medium", "Large", "Hall"};
const char * polyMonoStrMap[]     = {"Poly", "Mono"};
const char * rangeStrMap[]        = {"Rate Lo", "Rate Hi", "BPM", "Clk", "Rate Sub"};
const char * lfoWaveStrMap[]      = {"Sin", "Tri", "Saw", "Squ", "RndSt", "Rnd"};
const char * lfoAltWaveStrMap[]   = {"Sine", "CosBell", "TriBell", "Saw>Tri", "Tri>Squ", "Pulse"};
const char * saturateCurveStrMap[]= {"1", "2", "3", "4"};
const char * shpExpCurveStrMap[]  = {"x2", "x3", "x4", "x5"};
const char * pulseRangeStrMap[]   = {"Sub", "Lo", "Hi"};
const char * pulseModeStrMap[]    = {"Plus", "Minus"};
const char * bipUniStrMap[]       = {"Bip", "Uni"};
const char * vowelStrMap[]        = {"A", "E", "I", "O", "U", "Y", "AA", "AE", "OE"};
const char * nordFilterTypeStrMap[]={"LP", "BP", "HP", "BR"};
const char * staticFilterTypeStrMap[]={"LP", "BP", "HP"};
const char * multiEnvSustainStrMap[]= {"None", "L1", "L2", "L3", "L4"};
const char * drSynthPresetStrMap[]= {"Kick 1"}; // *** Don't have the list
const char * asymSymStrMap[]      = {"Asym", "Sym"};
const char * odTypeStrMap[]       = {"Soft", "Hard", "Heavy", "Fat"};
const char * delayStrMap[]        = {"50ms", "?", "?", "?"}; // *** For Scratch module; don't have the list
const char * gateTypeStrMap[]     = {"AND", "NAND", "OR", "NOR", "XOR", "NXOR"};
const char * invStrMap[]          = {"+", "Inv"};
const char * clkSrcStrMap[]       = {"Internal", "Master"};
const char * int16StrMap[]     = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
const char * divModeStrMap[]      = {"Gated", "Toggled"};
const char * out8StrMap[]         = {"Out1", "Out2", "Out3", "Out4", "Out5", "Out6", "Out7", "Out8"};
const char * in8StrMap[]          = {"In1", "In2", "In3", "In4", "In5", "In6", "In7", "In8"};
const char * rectStrMap[]         = {"Half wave pos.", "Half wave neg.", "Full wave pos.", "Full wave neg."};
const char * shpStaticStrMap[]    = {"Inv x3", "Inv x2", "x2", "x3"};
const char * trigGateStrMap[]     = {"Trig", "Gate"};
const char * decayReleaseStrMap[] = {"Decay", "Release"};
const char * fltLPSlopeStrMap[]   = {"6db", "12db", "18db", "24db", "30db", "36db"};
const char * flipFlopStrMap[]     = {"D-type", "RS-type"};
const char * freqShiftRangeStrMap[]= {"Hi", "Lo", "Sub"};
const char * fltPhaseTypeStrMap[] = {"Notch", "Peak", "Deep"};
const char * eq2BandLoStrMap[]    = {"80Hz"};
const char * eq2BandHiStrMap[]    = {"12kHz"};
const char * presetStrMap[]       = {"Set"};
const char * bitsStrMap[]         = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Off"};
const char * sustainStrMap[]      = {"L1", "L2"};
const char * midiChanStrMap[]     = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "This", "SlotA", "SlotB", "SlotC", "SlotD"};
const char * midiChanRcvStrMap[]     = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "This", "Keyb"};
const char * thruStrMap[]         = {"Notes only", "Notes+Ctrls"};
const char * levConvStrMap[]      = {"Bip", "Pos", "Neg"};
const char * oscShpAStrMap[]      = {"Sine1", "Sine2", "Sine3", "Sine4", "TriSaw", "SymPulse"};
const char * oscShpBStrMap[]      = {"Sine1", "Sine2", "Sine3", "Sine4", "TriSaw", "DblSaw", "Pulse", "SymPulse"};

const tRgb   offOnColourMap[] = {RGB_BACKGROUND_GREY, RGB_GREEN_ON};

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

const tRgb   connectorColourMap[] = {
    {0.7, 0.2, 0.2},
    {0.2, 0.2, 0.7},
    {0.9, 0.9, 0.0}, };


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
    {"EnvADR",       3, volumeTypeNone,     ledTypeNo },
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
    {"Fx-In",        2, volumeTypeStereo,   ledTypeNo },
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
    {"Mix4-1S",      4, volumeTypeStereo,   ledTypeNo },
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

// moduleType, paramType1, paramType2, position rectangle, anchor, label, range, defaultValue, string map, colour map
const tParamLocation paramLocationList[] = {
    //  0 Unknown
    //  1 Keyboard
    //  2 Unknown
    //  3 4-Out
    {moduleType4toOut,     paramType1StandardToggle, paramType2Toggle, {{ 25,  -3}, {7,  7}}, anchorBottomLeft,  "Out to",    3,  0, outTo4OutStrMap,    NULL          },  // 3 4-Out
    {moduleType4toOut,     paramType1Bypass,         paramType2Toggle, {{ 45,  -3}, {5,  5}}, anchorBottomLeft,  "Bypass",    2,  0, NULL,               NULL          },
    {moduleType4toOut,     paramType1StandardToggle, paramType2Toggle, {{  3,  -3}, {7,  7}}, anchorBottomLeft,  "Pad",       2,  0, padStrMap,          NULL          },
    // 4 2-Out
    {moduleType2toOut,     paramType1StandardToggle, paramType2Toggle, {{ 25,  -3}, {7,  7}}, anchorBottomLeft,  "Out to",    6,  0, outToStrMap,        NULL          },  // 4 2-Out
    {moduleType2toOut,     paramType1Bypass,         paramType2Toggle, {{ 45,  -3}, {5,  5}}, anchorBottomLeft,  "Bypass",    2,  0, NULL,               NULL          },
    {moduleType2toOut,     paramType1StandardToggle, paramType2Toggle, {{  3,  -3}, {7,  7}}, anchorBottomLeft,  "Pad",       2,  0, padStrMap,          NULL          },
    //  5 Invert
    //  6 Unknown
    //  7 OscB
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Tune",    128, 64, NULL,               NULL          }, // 7 OscB
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          },
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap},
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128,  0, NULL,               NULL          },
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Toggle, {{ 30,  15}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          },
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  "FM",      128,  0, NULL,               NULL          },
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{-15,   5}, {7, 14}}, anchorTopRight,    "Shape",   128,  0, NULL,               NULL          },     // 50% to 99%, not decimals
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "ShpM",    128,  0, NULL,               NULL          },
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Toggle, {{-30,   5}, {7,  7}}, anchorTopRight,    NULL,        5,  0, shapeTypeStrMap,    NULL          },     // Waveform
    {moduleTypeOscB,       paramType1Bypass,         paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, "Bypass",    2,  0, NULL,               NULL          },
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Toggle, {{ 60,  15}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, fmTypeStrMap,       NULL          },
    // 8 OscShpB
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128, 64, NULL,               NULL          },  // 8 OscShapeB
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap},
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Env",     128,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1StandardToggle, paramType2Toggle, {{ 30,  15}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          },
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  "FM",      128,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{-15,   5}, {7, 14}}, anchorTopRight,    "Shape",   128,  0, NULL,               NULL          },     // 50% to 99%, not decimals
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "ShpCtrl", 128,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1Bypass,         paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, "Bypass",    2,  0, NULL,               NULL          },
    {moduleTypeOscShpB,    paramType1StandardToggle, paramType2Toggle, {{ 60,  15}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, fmTypeStrMap,       NULL          },
    //  9 OscC
    {moduleTypeOscC,       paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Tune",    128, 64, NULL,               NULL          }, // 9 Coarse
    {moduleTypeOscC,       paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          }, // 9 Fine
    {moduleTypeOscC,       paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 9 Kbt
    {moduleTypeOscC,    paramType1StandardToggle, paramType2Toggle, {{ 30,  15}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          }, // 9 Tune Mode
    {moduleTypeOscC,       paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  "FM",      128,  0, NULL,               NULL          }, // FM
    {moduleTypeOscC,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 9 Bypass
    {moduleTypeOscC,       paramType1StandardToggle, paramType2Toggle, {{ 60,  15}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, fmTypeStrMap,       NULL          }, // 9 FM Type
    {moduleTypeOscC,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128,  0, NULL,               NULL          }, // Pitch Mode

    // 10 Unknown
    // 11 Unknown
    // 12 Reverb
    {moduleTypeReverb,     paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Time",    128, 64, NULL,               NULL          },  // 12 Reverb
    {moduleTypeReverb,     paramType1CommonDial,     paramType2Dial,   {{ 37,  -3}, {7, 14}}, anchorBottomLeft,  "Brightness", 128, 64, NULL,            NULL          },
    {moduleTypeReverb,     paramType1CommonDial,     paramType2Dial,   {{ 57,  -3}, {7, 14}}, anchorBottomLeft,  "DryWet",   128, 64, NULL,              NULL          },
    {moduleTypeReverb,     paramType1Bypass,         paramType2Toggle, {{-20,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          },
    // 13 OscString
    {moduleTypeOscString,  paramType1Freq,           paramType2Dial,   {{ 30,  -3}, { 7,14}}, anchorBottomLeft,  NULL,     128, 64, NULL,              NULL          }, // 13 OscString Coarse
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 47,  -3}, { 7,14}}, anchorBottomLeft,  "Cent",     128, 64, NULL,              NULL          }, // 13 OscString Fine
    {moduleTypeOscString,  paramType1StandardToggle, paramType2Toggle, {{ 22,  -3}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 13 OscString Kbt
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128,  0, NULL,               NULL          }, // 13 OscString Pitch modulation
    {moduleTypeOscString,  paramType1StandardToggle, paramType2Toggle, {{ 30,  10}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          }, // 13 OscString Pitch mode
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 59,  -3}, {7, 14}}, anchorBottomLeft,  "Decay",   128, 64, NULL,               NULL          }, // 13 OscString Decay
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 71,  -3}, {7, 14}}, anchorBottomLeft,  "Damp",    128, 64, NULL,               NULL          }, // 13 OscString Damp
    {moduleTypeOscString,  paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 13 OscString Bypass
    // 14 Unknown
    // 15 Sw8-1 *** Needs more resources
    // 16 Unknown
    // 17 ValSw1-2 *** Could need more resources
    // 18 X-Fade
    {moduleTypeXtoFade,    paramType1CommonDial,     paramType2Dial,   {{ 40, -3}, {7, 14}},  anchorBottomLeft,   NULL,   128, 0, NULL, NULL},  // 18 Mod
    {moduleTypeXtoFade,    paramType1CommonDial,     paramType2Dial,   {{ 55, -3}, {7, 14}},  anchorBottomLeft,   NULL,   128, 64, NULL, NULL}, // 18 X-fade *** should be another type of dial
    {moduleTypeXtoFade,    paramType1StandardToggle, paramType2Toggle, {{ 20, -3}, {7,  7}},  anchorBottomLeft,   NULL,   2, 0, logStrMap, NULL},
    // 19 Mix4-1B
    // 20 EnvvADSR
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Toggle, {{-26,  -3}, {7,  7}}, anchorBottomRight, NULL,        4,  0, envShapeStrMap,     NULL          },  // 20 EnvADSR
    {moduleTypeEnvADSR,    paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Attack",  128,  0, NULL,               NULL          },
    {moduleTypeEnvADSR,    paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Delay",   128,  0, NULL,               NULL          },
    {moduleTypeEnvADSR,    paramType1CommonDial,     paramType2Dial,   {{ 44,  -3}, {7, 14}}, anchorBottomLeft,  "Sus",     128,  0, NULL,               NULL          },
    {moduleTypeEnvADSR,    paramType1CommonDial,     paramType2Dial,   {{ 56,  -3}, {7, 14}}, anchorBottomLeft,  "Rel",     128,  0, NULL,               NULL          },
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Toggle, {{-26, -10}, {7,  7}}, anchorBottomRight, NULL,        6,  0, posStrMap,          NULL          },
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, kbStrMap,           offOnColourMap},
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Toggle, {{  5,  -8}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, normalResetStrMap,  NULL          },
    // 21 Mux1-8
    // 22 PartQuant
    // 23 ModADSR
    // 24 LfoC
    {moduleTypeLfoC,    paramType1CommonDial,  paramType2Dial, {{50, -3}, {7, 14}}, anchorBottomLeft,  NULL,  128, 64,  NULL,  NULL},  // 24 Rate  *** Could have Freq but tied to Range
    {moduleTypeLfoC,    paramType1StandardToggle,  paramType2Toggle, {{10, -3}, {7, 7}}, anchorBottomLeft,  NULL,  2, 0,  polyMonoStrMap,  NULL},  // 24 Mode
    {moduleTypeLfoC,    paramType1StandardToggle,  paramType2Toggle, {{76, -3}, {7, 7}}, anchorBottomLeft,  NULL,  6, 0,  posStrMap,  NULL},  // 24 OutType
    {moduleTypeLfoC,    paramType1StandardToggle,  paramType2Toggle, {{30, -3}, {7, 7}}, anchorBottomLeft,  NULL,  5, 0,  rangeStrMap,  NULL},  // 24 Range
    {moduleTypeLfoC,    paramType1StandardToggle,  paramType2Toggle, {{64, -3}, {7, 7}}, anchorBottomLeft,  NULL,  6, 0,  lfoWaveStrMap,  NULL},  // 24 Wave
    // 25 LfoShpA
    {moduleTypeLfoShpA, paramType1CommonDial,  paramType2Dial, {{36, -3}, {7, 14}}, anchorBottomLeft,  NULL,  128, 64,  NULL,  NULL},  // 25 Rate  *** Could have Freq but tied to Range
    {moduleTypeLfoShpA, paramType1StandardToggle,  paramType2Toggle, {{36, -17}, {7, 7}}, anchorBottomLeft,  NULL,  5, 0,  rangeStrMap,  NULL},  // 25 Range
    {moduleTypeLfoShpA, paramType1StandardToggle,  paramType2Toggle, {{25, -28}, {7, 7}}, anchorBottomLeft,  "Kbt",  5, 0,  offTo100KbStrMap,  NULL},  // 25 Kbt
    {moduleTypeLfoShpA, paramType1CommonDial,  paramType2Dial, {{25, -3}, {7, 14}}, anchorBottomLeft,  NULL,  128, 0,  NULL,  NULL},  // 25 Rate M
    {moduleTypeLfoShpA,  paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 25 Bypass
    {moduleTypeLfoShpA, paramType1CommonDial,  paramType2Dial, {{57, -17}, {7, 14}}, anchorBottomLeft,  "Shape",  128, 0,  NULL,  NULL},  // 25 Shape
    {moduleTypeLfoShpA, paramType1CommonDial,  paramType2Dial, {{74, -3}, {7, 14}}, anchorBottomLeft,  NULL,  128, 0,  NULL,  NULL},  // 25 Phase M
    {moduleTypeLfoShpA, paramType1CommonDial,  paramType2Dial, {{74, -17}, {7, 14}}, anchorBottomLeft,  "Phase",  128, 64,  NULL,  NULL},  // 25 Phase
    {moduleTypeLfoShpA, paramType1CommonDial,  paramType2Dial, {{57, -3}, {7, 14}}, anchorBottomLeft,  NULL,  128, 0,  NULL,  NULL},  // 25 Shape M
    {moduleTypeLfoShpA, paramType1StandardToggle,  paramType2Toggle, {{25,-17}, {7, 7}}, anchorBottomLeft,  NULL,  2, 0,  polyMonoStrMap,  NULL},  // 25 Mode
    {moduleTypeLfoShpA, paramType1StandardToggle,  paramType2Toggle, {{ -10, -10}, {7, 7}}, anchorBottomRight,  NULL,  6, 0,  posStrMap,  NULL},  // 25 OutType
    {moduleTypeLfoShpA,    paramType1StandardToggle,  paramType2Toggle, {{57, -32}, {7, 7}}, anchorBottomLeft,  NULL,  6, 0,  lfoAltWaveStrMap,  NULL},  // 25 Wave
    // 26 LfoA
    {moduleTypeLfoA,    paramType1CommonDial,  paramType2Dial, {{45, -3}, {7, 14}}, anchorBottomLeft,  NULL,  128, 64,  NULL,  NULL},  // 26 Rate  *** Could have Freq but tied to Range
    {moduleTypeLfoA,    paramType1StandardToggle,  paramType2Toggle, {{17, -10}, {7, 7}}, anchorBottomLeft,  NULL,  2, 0,  polyMonoStrMap,  NULL},  // 26 Mode
    {moduleTypeLfoA,    paramType1StandardToggle,  paramType2Toggle, {{65,  -3}, {7, 7}}, anchorBottomLeft,  "Kbt",  5, 0,  offTo100KbStrMap,  NULL},  // 26 Kbt
    {moduleTypeLfoA, paramType1CommonDial,  paramType2Dial, {{10, -3}, {7, 14}}, anchorBottomLeft,  NULL,  128, 0,  NULL,  NULL},  // 26 Rate M
    {moduleTypeLfoA,    paramType1StandardToggle,  paramType2Toggle, {{65, -10}, {7, 7}}, anchorBottomLeft,  NULL,  6, 0,  lfoWaveStrMap,  NULL},  // 26 Wave
    {moduleTypeLfoA,     paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 26 Bypass
    {moduleTypeLfoA,    paramType1StandardToggle,  paramType2Toggle, {{ -10, -10}, {7, 7}}, anchorBottomRight,  NULL,  6, 0,  posStrMap,  NULL},  // 26 OutType
    {moduleTypeLfoA,    paramType1StandardToggle,  paramType2Toggle, {{25, -3}, {7, 7}}, anchorBottomLeft,  NULL,  5, 0,  rangeStrMap,  NULL},  // 26 Range
    // 27 OscMaster
    {moduleTypeOscMaster,    paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128, 64, NULL,               NULL          },  // 27 Pitch
    {moduleTypeOscMaster,    paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          },  // 27 Fine
    {moduleTypeOscMaster,    paramType1StandardToggle, paramType2Toggle, {{ 22, -3}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap},  // 27 Kbt
    {moduleTypeOscMaster,    paramType1StandardToggle, paramType2Toggle, {{ 30,  15}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          },  // 27 Tune mode
    {moduleTypeOscMaster,  paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch M",   128,  0, NULL,               NULL          }, // 27 Pitch M
    // 28 Saturate
    {moduleTypeSaturate,  paramType1CommonDial,     paramType2Dial,   {{ 54,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 0, NULL,               NULL          },  // 28 Amount
    {moduleTypeSaturate,  paramType1CommonDial,     paramType2Dial,   {{ 41,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 0, NULL,               NULL          },  // 28 AmountMod
    {moduleTypeSaturate,     paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 28 Bypass
    {moduleTypeSaturate,    paramType1StandardToggle, paramType2Toggle, {{ 70, -6}, {7,  7}}, anchorBottomLeft,  "Curve",       4,  0, saturateCurveStrMap, NULL},  // 28 Curve
    // 29 MetNoise
    {moduleTypeMetNoise, paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "Color",   128, 64, NULL,               NULL          },  // 29 Color
    {moduleTypeMetNoise, paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",   128, 64, NULL,               NULL          },  // 29 Freq
    {moduleTypeMetNoise,     paramType1Bypass,         paramType2Toggle, {{-3,  -10}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 28 Bypass
    {moduleTypeMetNoise, paramType1CommonDial,     paramType2Dial,   {{ 70,  -3}, {7, 14}}, anchorBottomLeft,  NULL,   128, 64, NULL,               NULL          },  // 29 Color M
    {moduleTypeMetNoise, paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  NULL,   128, 64, NULL,               NULL          },  // 29 Freq M
    // 30 Device - No controls
    // 31 Noise
    {moduleTypeNoise, paramType1CommonDial,     paramType2Dial,   {{ 70,  -3}, {7, 14}}, anchorBottomLeft,  "Color",   128, 0, NULL,               NULL          },  // 31 Color  *** Or special dial
    {moduleTypeNoise,     paramType1Bypass,         paramType2Toggle, {{-3,  -10}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 31 Bypass
    // 32 Eq2Band
//    {moduleTypeEq2Band,    paramType1StandardToggle, paramType2Toggle, {{ 10, -6}, {7,  7}}, anchorBottomLeft,  "LoFreq",       1,  0, eq2BandLoStrMap, NULL},  // 32 Lo freq
//    {moduleTypeEq2Band,    paramType1StandardToggle, paramType2Toggle, {{ 20, -6}, {7,  7}}, anchorBottomLeft,  "HiFreq",       1,  0, eq2BandHiStrMap, NULL},  // 32 Hi freq
    {moduleTypeEq2Band, paramType1dB,     paramType2Dial,   {{ 30,  -3}, {7, 14}}, anchorBottomLeft,  "???",   128, 64, NULL,               NULL          },  // 32 Lo Gain
    {moduleTypeEq2Band, paramType1dB,     paramType2Dial,   {{ 50,  -3}, {7, 14}}, anchorBottomLeft,  "???",   128, 64, NULL,               NULL          },  // 32 Lo Gain
    {moduleTypeEq2Band, paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "Level",   128, 127, NULL,               NULL          },  // 32 Level
    {moduleTypeEq2Band,     paramType1Bypass,         paramType2Toggle, {{-3,  -9}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 32 Bypass
    {moduleTypeEq2Band, paramType1dB,     paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Lo (80Hz)",   128, 64, NULL,               NULL          },  // 32 Lo Gain
    {moduleTypeEq2Band, paramType1dB,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  "Hi (12kHz)",   128, 64, NULL,               NULL          },  // 32 Hi Gain
    // 33 Eq3Band
    {moduleTypeEq3band, paramType1dB,     paramType2Dial,   {{ 30,  -3}, {7, 14}}, anchorBottomLeft,  "???",   128, 64, NULL,               NULL          },  // 32 Lo Gain
    {moduleTypeEq3band, paramType1dB,     paramType2Dial,   {{ 50,  -3}, {7, 14}}, anchorBottomLeft,  "???",   128, 64, NULL,               NULL          },  // 32 Lo Gain
    {moduleTypeEq3band, paramType1dB,     paramType2Dial,   {{ 5,  -3}, {7, 14}}, anchorBottomLeft,  "Lo (80Hz)",   128, 64, NULL,               NULL          },  // 33 Lo Gain
    {moduleTypeEq3band, paramType1dB,     paramType2Dial,   {{ 24,  -3}, {7, 14}}, anchorBottomLeft,  "MidGn",   128, 64, NULL,               NULL          },  // 33 Mid Gain
    {moduleTypeEq3band, paramType1Freq,     paramType2Dial,   {{ 36,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",   128, 64, NULL,               NULL          },  // 33 Mid Freq
    {moduleTypeEq3band, paramType1dB,     paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  "Hi (12kHz)",   128, 64, NULL,               NULL          },  // 33 Hi Gain
    {moduleTypeEq3band, paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "Level",   128, 127, NULL,               NULL          },  // 33 Level
    {moduleTypeEq3band,     paramType1Bypass,         paramType2Toggle, {{-3,  -9}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 33 Bypass
    // 34 ShpExp
    {moduleTypeShpExp,  paramType1CommonDial,     paramType2Dial,   {{ 54,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 0, NULL,               NULL          },  // 34 Amount
    {moduleTypeShpExp,  paramType1CommonDial,     paramType2Dial,   {{ 41,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 0, NULL,               NULL          },  // 34 AmountMod
    {moduleTypeShpExp,     paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 34 Bypass
    {moduleTypeShpExp,    paramType1StandardToggle, paramType2Toggle, {{ 70, -6}, {7,  7}}, anchorBottomLeft,  "Curve",       4,  0, shpExpCurveStrMap, NULL},  // 34 Curve
    // 35 Driver *** What module is this?
    // 36 SwOnOffM
    {moduleTypeSwOnOffM,    paramType1StandardToggle, paramType2Toggle, {{ 60, -3}, {7,  7}}, anchorBottomLeft,  NULL,       2,  0, offOnStrMap, NULL}, // 36 On
    // 37 Unknown
    // 38 Pulse
    {moduleTypePulse,  paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  "Time",    128, 0, NULL,               NULL          },  // 38 Time
    {moduleTypePulse,  paramType1CommonDial,     paramType2Dial,   {{ 42,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 0, NULL,               NULL          },  // 38 TimeMod
    {moduleTypePulse,    paramType1StandardToggle, paramType2Toggle, {{ 52, -3}, {7,  7}}, anchorBottomLeft,  NULL,       3,  0, pulseRangeStrMap, NULL},  // 38 Range
    // 39 Unknown
    // 40 Mix8-1B
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In1
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 29,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In2
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 38,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In3
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 47,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In4
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 56,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In5
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 65,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In6
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 74,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In7
    {moduleTypeMix8to1B,  paramType1CommonDial,     paramType2Dial,   {{ 83,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 40 In8
    {moduleTypeMix8to1B,  paramType1StandardToggle,     paramType2Toggle,   {{ 3,  -17}, {7, 7}}, anchorBottomLeft,  NULL,    3, 0, expStrMap,               NULL          },  // 40 ExpLinDB
    {moduleTypeMix8to1B,  paramType1StandardToggle,     paramType2Toggle,   {{ 3,  -10}, {7, 7}}, anchorBottomLeft,  "Pad",    2, 0, db12BPadStrMap,               NULL          },  // 40 Pad *** Check range of Pad
    // 41 EnvH
    {moduleTypeEnvH,    paramType1CommonDial, paramType2Dial, {{50,  -3}, {7,  14}}, anchorBottomLeft, NULL,        128, 64, NULL,     NULL          },  // 41 Hold
    {moduleTypeEnvH,    paramType1StandardToggle, paramType2Toggle, {{-32, -3}, {7,  7}}, anchorBottomRight, NULL,        6,  0, posStrMap,          NULL          }, // 41 Pos
    // 42 Delay (Logic Delay)
    {moduleTypeDelay,  paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  "Time",    128, 0, NULL,               NULL          },  // 42 Time
    {moduleTypeDelay,  paramType1CommonDial,     paramType2Dial,   {{ 42,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 0, NULL,               NULL          },  // 42 TimeMod
    {moduleTypeDelay,    paramType1StandardToggle, paramType2Toggle, {{ 52, -3}, {7,  7}}, anchorBottomLeft,  NULL,       3,  0, pulseRangeStrMap, NULL},  // 42 Range
    // 43 Constant
    {moduleTypeConstant,    paramType1CommonDial, paramType2Dial, {{80,  -3}, {7,  14}}, anchorBottomLeft, NULL,        128, 64, NULL,     NULL          },  // 43 Value
    {moduleTypeConstant,    paramType1StandardToggle, paramType2Toggle, {{ 40, -3}, {7,  7}}, anchorBottomLeft,  NULL,       2,  0, bipUniStrMap, NULL},  // 43 Bip/Uni
    // 44 LevMult  - No Controls
    // 45 FltVoice
    {moduleTypeFltVoice, paramType1StandardToggle, paramType2Toggle, {{ 40, -17}, {7,  7}}, anchorBottomLeft,  NULL,       9,  0, vowelStrMap, NULL},  // 45 Vowel1
    {moduleTypeFltVoice, paramType1StandardToggle, paramType2Toggle, {{ 50, -17}, {7,  7}}, anchorBottomLeft,  NULL,       9,  1, vowelStrMap, NULL},  // 45 Vowel2
    {moduleTypeFltVoice, paramType1StandardToggle, paramType2Toggle, {{ 60, -17}, {7,  7}}, anchorBottomLeft,  NULL,       9,  2, vowelStrMap, NULL},  // 45 Vowel3
    {moduleTypeFltVoice, paramType1CommonDial, paramType2Dial, {{-15, -17}, {7,  14}}, anchorBottomRight, NULL,        128, 80, NULL,     NULL          },  // 45 Level
    {moduleTypeFltVoice, paramType1CommonDial, paramType2Dial, {{55, -3}, {7,  14}}, anchorBottomLeft, NULL,        128, 80, NULL,     NULL          },  // 45 Vowel
    {moduleTypeFltVoice, paramType1CommonDial, paramType2Dial, {{45, -3}, {7,  14}}, anchorBottomLeft, NULL,        128, 0, NULL,     NULL          },  // 45 VowelMod
    {moduleTypeFltVoice, paramType1CommonDial, paramType2Dial, {{20, -3}, {7,  14}}, anchorBottomLeft, "Freq",        128, 80, NULL,     NULL          },  // 45 Freq
    {moduleTypeFltVoice, paramType1CommonDial, paramType2Dial, {{10, -3}, {7,  14}}, anchorBottomLeft, NULL,        128, 0, NULL,     NULL          },  // 45 FreqMod
    {moduleTypeFltVoice, paramType1CommonDial, paramType2Dial, {{30, -3}, {7,  14}}, anchorBottomLeft, "Res",        128, 0, NULL,     NULL          },  // 45 Res
    {moduleTypeFltVoice, paramType1Bypass,         paramType2Toggle, {{-3,  -10}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 45 Bypass
    // 46 EnvAHD
    {moduleTypeEnvAHD,    paramType1StandardToggle, paramType2Toggle, {{-26,  -3}, {7,  7}}, anchorBottomRight, NULL,        4,  0, envShapeStrMap,     NULL          },  // 46 Shape
    {moduleTypeEnvAHD,    paramType1ADRTime,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Attack",  128,  0, NULL,               NULL          },  // 46 Attack
    {moduleTypeEnvAHD,    paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Hold",   128,  0, NULL,               NULL          }, // 46 Hold
    {moduleTypeEnvAHD,    paramType1StandardToggle, paramType2Toggle, {{  5,  -15}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, normalResetStrMap,  NULL          },  // 46 Reset
    {moduleTypeEnvAHD,    paramType1ADRTime,     paramType2Dial,   {{ 44,  -3}, {7, 14}}, anchorBottomLeft,  "Decay",     128,  0, NULL,               NULL          }, // 46 Decay
    {moduleTypeEnvAHD,    paramType1StandardToggle, paramType2Toggle, {{-26, -10}, {7,  7}}, anchorBottomRight, NULL,        6,  0, posStrMap,          NULL          },
    {moduleTypeEnvAHD,    paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, kbStrMap,           offOnColourMap},
    // 47 Pan
    {moduleTypePan,    paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  NULL,   128,  0, NULL,               NULL          }, // 47 PanMod
    {moduleTypePan,    paramType1LRDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  NULL,   128,  64, NULL,               NULL          }, // 47 Pan
    {moduleTypePan,    paramType1StandardToggle, paramType2Toggle, {{ 10, -5}, {7,  7}}, anchorBottomLeft, NULL,        2,  0, logStrMap,          NULL          }, // 47 LogLin
    // 48 MixStereo
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 48 Lvl1
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 22,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 48 Lvl2
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 34,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 48 Lvl3
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 48,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 48 Lvl4
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 48 Lvl5
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 72,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 80, NULL,               NULL          },  // 48 Lvl6
    {moduleTypeMixStereo,    paramType1LRDial,     paramType2Dial,   {{ 10,  -17}, {7, 14}}, anchorBottomLeft,  NULL,   128,  64, NULL,               NULL          }, // 48 Pan1
    {moduleTypeMixStereo,    paramType1LRDial,     paramType2Dial,   {{ 22,  -17}, {7, 14}}, anchorBottomLeft,  NULL,   128,  64, NULL,               NULL          }, // 48 Pan2
    {moduleTypeMixStereo,    paramType1LRDial,     paramType2Dial,   {{ 34,  -17}, {7, 14}}, anchorBottomLeft,  NULL,   128,  64, NULL,               NULL          }, // 48 Pan3
    {moduleTypeMixStereo,    paramType1LRDial,     paramType2Dial,   {{ 46,  -17}, {7, 14}}, anchorBottomLeft,  NULL,   128,  64, NULL,               NULL          }, // 48 Pan4
    {moduleTypeMixStereo,    paramType1LRDial,     paramType2Dial,   {{ 58,  -17}, {7, 14}}, anchorBottomLeft,  NULL,   128,  64, NULL,               NULL          }, // 48 Pan5
    {moduleTypeMixStereo,    paramType1LRDial,     paramType2Dial,   {{ 70,  -17}, {7, 14}}, anchorBottomLeft,  NULL,   128,  64, NULL,               NULL          }, // 48 Pan6
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ -7,  -25}, {7, 14}}, anchorBottomRight,  NULL,    128, 80, NULL,               NULL          },  // 48 LvlMaster
    // 49 FltMulti
    {moduleTypeFltMulti,   paramType1Freq,           paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },  // 49 Freq
    {moduleTypeFltMulti,   paramType1Pitch,          paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",     128,  0, NULL,               NULL          }, // 49 Pitch M
    {moduleTypeFltMulti,   paramType1StandardToggle, paramType2Toggle, {{ 25, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          }, // 49 Kbt
    {moduleTypeFltMulti,   paramType1StandardToggle, paramType2Toggle, {{ 55,   5}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, gcStrMap,           offOnColourMap}, // 49 GC
    {moduleTypeFltMulti,   paramType1Resonance,      paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  "Res",     128,  0, NULL,               NULL          }, // 49 Res
    {moduleTypeFltMulti,   paramType1StandardToggle, paramType2Toggle, {{ 70, -10}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, fltMultiDbStrMap,   NULL          }, // 49 dB
    {moduleTypeFltMulti,   paramType1Bypass,         paramType2Toggle, {{-17,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 49 Bypass
    // 50 ConstSwT
    {moduleTypeConstSwT,    paramType1CommonDial, paramType2Dial, {{80,  -3}, {7,  14}}, anchorBottomLeft, NULL,        128, 64, NULL,     NULL          },  // 50 Value
    {moduleTypeConstSwT,   paramType1StandardToggle, paramType2Toggle, {{ 60, -3}, {7,  7}}, anchorBottomLeft,  NULL,       2,  0, offOnStrMap,   offOnColourMap          }, // 50 Switch
    {moduleTypeConstSwT,    paramType1StandardToggle, paramType2Toggle, {{ 40, -3}, {7,  7}}, anchorBottomLeft,  NULL,       2,  0, bipUniStrMap, NULL},  // 50 Bip/Uni
    // 51 FltNord
    {moduleTypeFltNord,   paramType1Freq,           paramType2Dial,   {{ 30,  -25}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },  // 51 Freq
    {moduleTypeFltNord,   paramType1CommonDial,          paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",     128,  0, NULL,               NULL          }, // 51 Pitch M
    {moduleTypeFltNord,   paramType1StandardToggle, paramType2Toggle, {{ 15, -25}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          }, // 51 Kbt
    {moduleTypeFltNord,   paramType1StandardToggle, paramType2Toggle, {{ 40,   -15}, {7,  7}}, anchorBottomLeft,     NULL,        2,  0, gcStrMap,           offOnColourMap}, // 51 GC
    {moduleTypeFltNord,   paramType1Resonance,      paramType2Dial,   {{ 55,  -25}, {7, 14}}, anchorBottomLeft,  "Res",     128,  0, NULL,               NULL          }, // 51 Res
    {moduleTypeFltNord,   paramType1StandardToggle, paramType2Toggle, {{ 70, -10}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, fltNordDbStrMap,   NULL          }, // 51 dB/Oct
    {moduleTypeFltNord,   paramType1Bypass,         paramType2Toggle, {{-17,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 51 Bypass
    {moduleTypeFltNord,   paramType1CommonDial,          paramType2Dial,   {{ 31,  -3}, {7, 14}}, anchorBottomLeft,  "Fm Lin",     128,  0, NULL,               NULL          }, // 51 Fm Lin
    {moduleTypeFltNord,   paramType1StandardToggle, paramType2Toggle, {{ 70, -20}, {7,  7}}, anchorBottomLeft,  NULL,        4,  0, nordFilterTypeStrMap,   NULL          }, // 51 FilterType
    {moduleTypeFltNord,   paramType1CommonDial,          paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  "Res M",     128,  0, NULL,               NULL          }, // 51 Res M
    // 52 EnvMulti
    {moduleTypeEnvMulti,   paramType1CommonDial,          paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "L1",     128,  127   , NULL,               NULL          }, // 52 Lvl1
    {moduleTypeEnvMulti,   paramType1CommonDial,          paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "L1",     128,  127   , NULL,               NULL          }, // 52 Lvl2
    {moduleTypeEnvMulti,   paramType1CommonDial,          paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  "L1",     128,  127   , NULL,               NULL          }, // 52 Lvl3
    {moduleTypeEnvMulti,   paramType1CommonDial,          paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "L1",     128,  127   , NULL,               NULL          }, // 52 Lvl4
    {moduleTypeEnvMulti,    paramType1ADRTime,     paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "T1",     128,  0, NULL,               NULL          }, // 52 Time1
    {moduleTypeEnvMulti,    paramType1ADRTime,     paramType2Dial,   {{ 30,  -3}, {7, 14}}, anchorBottomLeft,  "T2",     128,  0, NULL,               NULL          }, // 52 Time2
    {moduleTypeEnvMulti,    paramType1ADRTime,     paramType2Dial,   {{ 50,  -3}, {7, 14}}, anchorBottomLeft,  "T3",     128,  0, NULL,               NULL          }, // 52 Time3
    {moduleTypeEnvMulti,    paramType1ADRTime,     paramType2Dial,   {{ 70,  -3}, {7, 14}}, anchorBottomLeft,  "T4",     128,  0, NULL,               NULL          }, // 52 Time4
    {moduleTypeEnvMulti,    paramType1StandardToggle, paramType2Toggle, {{  3,  -22}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, normalResetStrMap,  NULL          },  // 52 Reset
    {moduleTypeEnvMulti,    paramType1StandardToggle, paramType2Toggle, {{  30,  -22}, {7,  7}}, anchorBottomLeft,  "Sust",        5,  1, multiEnvSustainStrMap,  NULL          },  // 52 SustainPlacement
    {moduleTypeEnvMulti,    paramType1StandardToggle, paramType2Toggle, {{-17, -29}, {7,  7}}, anchorBottomRight, NULL,        6,  0, posStrMap,          NULL          }, // 52 OutType
    {moduleTypeEnvMulti,    paramType1StandardToggle, paramType2Toggle, {{ 10,   17}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, kbStrMap,           offOnColourMap}, // 52 Kbt
    {moduleTypeEnvMulti,    paramType1StandardToggle, paramType2Toggle, {{-26,  -17}, {7,  7}}, anchorBottomRight, NULL,        4,  0, envShapeStrMap,     NULL          },  // 52 Shape
    // 53 SandH -- No parameters
    // 54 FltStatic
    {moduleTypeFltStatic,   paramType1Freq,           paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },  // 54 Freq
    {moduleTypeFltStatic,   paramType1Resonance,      paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Res",     128,  0, NULL,               NULL          }, // 54 Res
    {moduleTypeFltStatic,   paramType1StandardToggle, paramType2Toggle, {{ 60, -3}, {7,  7}}, anchorBottomLeft,  NULL,        3,  0, staticFilterTypeStrMap,   NULL          }, // 54 FilterType
    {moduleTypeFltStatic,   paramType1Bypass,         paramType2Toggle, {{-3,   2}, {5,  5}}, anchorMiddleRight, NULL,        2,  1, NULL,               NULL          }, // 54 Bypass
    {moduleTypeFltStatic,   paramType1StandardToggle, paramType2Toggle, {{ 80,   -3}, {7,  7}}, anchorBottomLeft,     NULL,        2,  0, gcStrMap,           offOnColourMap}, // 54 GC
    // 55 EnvD
    {moduleTypeEnvD,    paramType1ADRTime,     paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Decay",     128,  0, NULL,               NULL          }, // 55 Decay
    {moduleTypeEnvD,    paramType1StandardToggle, paramType2Toggle, {{55, -3}, {7,  7}}, anchorBottomLeft, NULL,        6,  0, posStrMap,          NULL          }, // 55 OutType
    // 56 Resonator *** Which module is this?
    // 57 Automate *** We lack an UI element for the CTRL selector
    // 58 Drumsynth
    {moduleTypeDrumSynth,   paramType1FreqDrum,           paramType2Dial,   {{ 20,  -22}, {7, 14}}, anchorBottomLeft,  NULL,    128,  100, NULL,               NULL          },  // 58 Master Freq
    {moduleTypeDrumSynth,   paramType1CommonDial,           paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 58 Slave Ratio
    {moduleTypeDrumSynth,    paramType1ADRTime,     paramType2Dial,   {{ 30,  -22}, {7, 14}}, anchorBottomLeft,  "Dcy",     128,  0, NULL,               NULL          }, // 58 MasterDecay
    {moduleTypeDrumSynth,    paramType1ADRTime,     paramType2Dial,   {{ 30,  -3}, {7, 14}}, anchorBottomLeft,  "Dcy",     128,  0, NULL,               NULL          }, // 58 Slave Decay
    {moduleTypeDrumSynth,   paramType1CommonDial,           paramType2Dial,   {{ 40,  -22}, {7, 14}}, anchorBottomLeft,  "Lvl",    128,  100, NULL,               NULL          },  // 58 Master Level
    {moduleTypeDrumSynth,   paramType1CommonDial,           paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Lvl",    128,  100, NULL,               NULL          },  // 58 Slave Level
    {moduleTypeDrumSynth,   paramType1Freq,           paramType2Dial,   {{ 50,  -22}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },  // 58 Noise Filter Freq
    {moduleTypeDrumSynth,   paramType1Resonance,      paramType2Dial,   {{ 60,  -22}, {7, 14}}, anchorBottomLeft,  "Res",     128,  0, NULL,               NULL          }, // 54 Noise Filter Res
    {moduleTypeDrumSynth,   paramType1CommonDial,           paramType2Dial,   {{ 70,  -22}, {7, 14}}, anchorBottomLeft,  "Swp",    128,  0, NULL,               NULL          },  // 58 Noise Filter Sweep
    {moduleTypeDrumSynth,    paramType1ADRTime,     paramType2Dial,   {{ 80,  -22}, {7, 14}}, anchorBottomLeft,  "Dcy",     128,  0, NULL,               NULL          }, // 58 Noise Filter Decay
    {moduleTypeDrumSynth,   paramType1StandardToggle, paramType2Toggle, {{ 90, -22}, {7,  7}}, anchorBottomLeft,  NULL,        3,  0, staticFilterTypeStrMap,   NULL          }, // 58 Noise FilterType
    {moduleTypeDrumSynth,   paramType1CommonDial,           paramType2Dial,   {{ 50,  -3}, {7, 14}}, anchorBottomLeft,  "Bend",    128,  0, NULL,               NULL          },  // 58 Bend Amount
    {moduleTypeDrumSynth,    paramType1ADRTime,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  "Dcy",     128,  0, NULL,               NULL          }, // 58 BendDecay
    {moduleTypeDrumSynth,   paramType1CommonDial,           paramType2Dial,   {{ 70,  -3}, {7, 14}}, anchorBottomLeft,  "Click",    128,  0, NULL,               NULL          },  // 58 Click Amount
    {moduleTypeDrumSynth,   paramType1CommonDial,           paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "Noise",    128,  0, NULL,               NULL          },  // 58 Noise Amount
    {moduleTypeDrumSynth,   paramType1Bypass,         paramType2Toggle, {{-3,   -10}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 58 Bypass
//    {moduleTypeDrumSynth,   paramType1StandardToggle, paramType2Toggle, {{ 60, -35}, {7,  7}}, anchorBottomLeft,  NULL,        1,  0, drSynthPresetStrMap,   NULL          }, // 58 Preset *** Length must be updated
    // 59 CompLev
    {moduleTypeCompLev,   paramType1BipLevel,           paramType2Dial,   {{ 70,  -3}, {7, 14}}, anchorBottomLeft,  "C",    128,  64, NULL,               NULL          },  // 59 Compare Level
    // 60 Mux8-1X
    {moduleTypeMux8to1X,   paramType1CommonDial,           paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "X-fade",    128,  0, NULL,               NULL          },  // 58 X-fade
    // 61 Clip
    {moduleTypeClip,   paramType1CommonDial,           paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 61 Clip level M
    {moduleTypeClip,   paramType1CommonDial,           paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 61 Clip level
    {moduleTypeClip,   paramType1StandardToggle, paramType2Toggle, {{ 10, -3}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, asymSymStrMap,   NULL          }, // 61 Shape
    {moduleTypeClip,   paramType1Bypass,         paramType2Toggle, {{-10,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 61 Bypass
    // 62 OverDrive
    {moduleTypeOverdrive,   paramType1CommonDial,           paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 62 Amount M
    {moduleTypeOverdrive,   paramType1CommonDial,           paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 62 Amount
    {moduleTypeOverdrive,   paramType1Bypass,         paramType2Toggle, {{-10,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 62 Bypass
    {moduleTypeOverdrive,   paramType1StandardToggle, paramType2Toggle, {{ 15, -3}, {7,  7}}, anchorBottomLeft,  NULL,        4,  0, odTypeStrMap,   NULL          }, // 62 Type
    {moduleTypeOverdrive,   paramType1StandardToggle, paramType2Toggle, {{ 3, -3}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, asymSymStrMap,   NULL          }, // 62 Shape
    // 63 Scratch
    {moduleTypeScratch,   paramType1CommonDial,           paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 63 Ratio
    {moduleTypeScratch,   paramType1CommonDial,           paramType2Dial,   {{ 15,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 63 Ratio Mod
    {moduleTypeScratch,   paramType1StandardToggle, paramType2Toggle, {{ 75, -3}, {7,  7}}, anchorBottomLeft,  NULL,        4,  0, delayStrMap,   NULL          }, // 63 Delay
    {moduleTypeScratch,   paramType1Bypass,         paramType2Toggle, {{-3,   -10}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 63 Bypass
    // 64 Gate (Logic Gate) -- no parameters, 2 modes
    // 65 Unknown
    // 66 Mix2-1B
    {moduleTypeMix2to1B,   paramType1StandardToggle,         paramType2Toggle, {{40,   -3}, {7,  7}}, anchorBottomLeft, NULL,        2,  0, invStrMap,               NULL          }, // 66 Inv1
    {moduleTypeMix2to1B,   paramType1CommonDial,           paramType2Dial,   {{ 50,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  100, NULL,               NULL          },  // 66 Lvl1
    {moduleTypeMix2to1B,   paramType1StandardToggle,         paramType2Toggle, {{65,   -3}, {7,  7}}, anchorBottomLeft, NULL,        2,  0, invStrMap,               NULL          }, // 66 Inv2
    {moduleTypeMix2to1B,   paramType1CommonDial,           paramType2Dial,   {{ 75,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  100, NULL,               NULL          },  // 66 Lvl2
    {moduleTypeMix2to1B,  paramType1StandardToggle,     paramType2Toggle,   {{ 20,  -3}, {7, 7}}, anchorBottomLeft,  NULL,    3, 0, expStrMap,               NULL          },  // 66 ExpLinDB
    // 67 Unknown
    // 68 ClkGen
    {moduleTypeClkGen,   paramType1CommonDial,           paramType2Dial,   {{60,  -17}, {7, 14}}, anchorBottomLeft,  NULL,    128,  100, NULL,               NULL          },  // 68 Tempo *** needs new dial 24-214 BPM
    {moduleTypeClkGen,   paramType1Bypass,         paramType2Toggle, {{70,   -17}, {5,  5}}, anchorBottomLeft, NULL,        2,  1, NULL,               NULL          }, // 68 Bypass
    {moduleTypeClkGen,  paramType1StandardToggle,     paramType2Toggle,   {{ 3,  -3}, {7, 7}}, anchorBottomLeft,  "Source",    2, 0, clkSrcStrMap,               NULL          },  // 66 Clk Source
    {moduleTypeClkGen,  paramType1StandardToggle,     paramType2Toggle,   {{ 60,  -3}, {7, 7}}, anchorBottomLeft,  "Sync every",    16, 3, int16StrMap,               NULL          },  // 66 BeatSync
    {moduleTypeClkGen,   paramType1CommonDial,           paramType2Dial,   {{25,  -3}, {7, 14}}, anchorBottomLeft,  "Swing",    128,  0, NULL,               NULL          },  // 68 Swing
    // 69 ClkDiv
    {moduleTypeClkDiv,   paramType1CommonDial,           paramType2Dial,   {{60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 69 Divider *** needs other UI element
    // 70 Unknown
    // 71 EnvFollow
    {moduleTypeEnvFollow,    paramType1ADRTime,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Attack",     128,  0, NULL,               NULL          }, // 71 Attack *** check values
    {moduleTypeEnvFollow,    paramType1ADRTime,     paramType2Dial,   {{ 35,  -3}, {7, 14}}, anchorBottomLeft,  "Release",     128,  0, NULL,               NULL          }, // 71 Attack *** check values
    // 72 NoteScaler
    {moduleTypeNoteScaler,   paramType1CommonDial,           paramType2Dial,   {{40,  -3}, {7, 14}}, anchorBottomLeft,  "Range",    128,  0, NULL,               NULL          },  // 72 Range *** needs UI element
    // 73 Unknown
    // 74 WaveWrap
    {moduleTypeWaveWrap,   paramType1CommonDial,           paramType2Dial,   {{40,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 74 Amount Mod
    {moduleTypeWaveWrap,   paramType1CommonDial,           paramType2Dial,   {{55,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  0, NULL,               NULL          },  // 74 Amount
    {moduleTypeWaveWrap,   paramType1Bypass,         paramType2Toggle, {{-10,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 74 Bypass
    // 75 NoteQuant
    {moduleTypeNoteQuant,   paramType1CommonDial,           paramType2Dial,   {{25,  -3}, {7, 14}}, anchorBottomLeft,  "Range",    128,  127, NULL,               NULL          },  // 75 Range
    {moduleTypeNoteQuant,   paramType1CommonDial,           paramType2Dial,   {{40,  -3}, {7, 14}}, anchorBottomLeft,  "Notes",    128,  0, NULL,               NULL          },  // 75 Notes
    // 76 SwOnOffT
    {moduleTypeSwOnOffT,    paramType1StandardToggle, paramType2Toggle, {{ 60, -3}, {7,  7}}, anchorBottomLeft,  NULL,       2,  0, offOnStrMap, NULL}, // 76 On
    // 77 Unknown
    // 78 Sw1-8
    {moduleTypeSw1to8,   paramType1StandardToggle, paramType2Toggle, {{ 40, -17}, {7,  7}}, anchorBottomLeft,  NULL,        8,  0, out8StrMap,   NULL          }, // 78 Selector
    // 79 Sw4-1
    {moduleTypeSw4to1,   paramType1StandardToggle, paramType2Toggle, {{ 40, -17}, {7,  7}}, anchorBottomLeft,  NULL,        4,  0, in8StrMap,   NULL          }, // 79 Selector
    // 80 Unknown
    // 81 LevAmp
    {moduleTypeLevAmp,   paramType1LevAmpDial,           paramType2Dial,   {{70,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 81 Gain
    {moduleTypeLevAmp,    paramType1StandardToggle, paramType2Toggle, {{ 60, -3}, {7,  7}}, anchorBottomLeft,  NULL,       2,  1, expStrMap, NULL}, // 81 Type
    // 82 Rect
    {moduleTypeRect,   paramType1StandardToggle, paramType2Toggle, {{ 40, -3}, {7,  7}}, anchorBottomLeft,  NULL,        4,  0, rectStrMap,   NULL          }, // 82 Mode
    {moduleTypeRect,   paramType1Bypass,         paramType2Toggle, {{-10,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 82 Bypass
    // 83 ShpStatic
    {moduleTypeShpStatic,   paramType1StandardToggle, paramType2Toggle, {{ 40, -3}, {7,  7}}, anchorBottomLeft,  NULL,        4,  0, shpStaticStrMap,   NULL          }, // 83 Mode
    {moduleTypeShpStatic,   paramType1Bypass,         paramType2Toggle, {{-10,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 83 Bypass
    // 84 EnvADR
    {moduleTypeEnvADR,    paramType1StandardToggle, paramType2Toggle, {{70,  -17}, {7,  7}}, anchorBottomLeft, NULL,        4,  0, envShapeStrMap,     NULL          },  // 84 Shape
    {moduleTypeEnvADR,    paramType1ADRTime,     paramType2Dial,   {{ 35,  -3}, {7, 14}}, anchorBottomLeft,  "Attack",  128,  0, NULL,               NULL          },  // 84 Attack
    {moduleTypeEnvADR,    paramType1StandardToggle, paramType2Toggle, {{  28,  -17}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, normalResetStrMap,  NULL          },  // 84 Reset
    {moduleTypeEnvADR,    paramType1ADRTime,     paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  NULL,     128,  0, NULL,               NULL          }, // 84 Release/Decay time
    {moduleTypeEnvADR,    paramType1StandardToggle, paramType2Toggle, {{  10,  -3}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, trigGateStrMap,  NULL          },  // 84 Trig/Gate
    {moduleTypeEnvADR,    paramType1StandardToggle, paramType2Toggle, {{70, -10}, {7,  7}}, anchorBottomLeft, NULL,        6,  0, posStrMap,          NULL          },
    {moduleTypeEnvADR,    paramType1StandardToggle, paramType2Toggle, {{ 3,   -10}, {7,  7}}, anchorBottomLeft,     NULL,        2,  0, kbStrMap,           offOnColourMap},
    {moduleTypeEnvADR,    paramType1StandardToggle, paramType2Toggle, {{  52,  -12}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, decayReleaseStrMap,  NULL          },  // 84 Decay/Release toggle
    // 85 WindSw
    {moduleTypeWindSw,   paramType1CommonDial,           paramType2Dial,   {{40,  -3}, {7, 14}}, anchorBottomLeft,  "From",    128,  0, NULL,               NULL          },  // 85 From
    {moduleTypeWindSw,   paramType1CommonDial,           paramType2Dial,   {{60,  -3}, {7, 14}}, anchorBottomLeft,  "To",    128,  0, NULL,               NULL          },  // 85 To
    // 86 8Counter -- No controls
    // 87 FltLP
    {moduleTypeFltLP,   paramType1Freq,           paramType2Dial,   {{50,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 87 Freq
    {moduleTypeFltLP,   paramType1CommonDial,           paramType2Dial,   {{35,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 87 FreqMod
    {moduleTypeFltLP, paramType1StandardToggle, paramType2Toggle, {{ 10, -3}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          },
    //{moduleTypeFltLP, paramType1StandardToggle, paramType2Toggle, {{ 67, -3}, {7,  7}}, anchorBottomLeft,  "Slope",       6,  2, fltLPSlopeStrMap,   NULL          },
    {moduleTypeFltLP,   paramType1Bypass,         paramType2Toggle, {{-10,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 83 Bypass
    // 88 Sw1-4
    {moduleTypeSw1to4,   paramType1StandardToggle, paramType2Toggle, {{ 37, -3}, {7,  7}}, anchorBottomLeft,  NULL,        4,  0, out8StrMap,   NULL          }, // 88 Selector
    // 89 Flanger
    {moduleTypeFlanger,   paramType1CommonDial,           paramType2Dial,   {{35,  -3}, {7, 14}}, anchorBottomLeft,  "Rate",    128,  64, NULL,               NULL          },  // 89 Rate
    {moduleTypeFlanger,   paramType1CommonDial,           paramType2Dial,   {{55,  -3}, {7, 14}}, anchorBottomLeft,  "Range",    128,  64, NULL,               NULL          },  // 89 Range
    {moduleTypeFlanger,   paramType1CommonDial,           paramType2Dial,   {{75,  -3}, {7, 14}}, anchorBottomLeft,  "Feedback",    128,  64, NULL,               NULL          },  // 89 Feedback
    {moduleTypeFlanger,   paramType1Bypass,         paramType2Toggle, {{-3,   -8}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 89 Bypass
    // 90 Sw1-2
    {moduleTypeSw1to2,   paramType1StandardToggle, paramType2Toggle, {{ 37, -3}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, out8StrMap,   NULL          }, // 90 Selector
    // 91 FlipFlop
    // 92 FltClassic
    {moduleTypeFltClassic, paramType1Freq,           paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },  // 92
        {moduleTypeFltClassic, paramType1Pitch,          paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "Env",     128,  0, NULL,               NULL          },
        {moduleTypeFltClassic, paramType1StandardToggle, paramType2Toggle, {{ 25, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          },
        {moduleTypeFltClassic, paramType1Resonance,      paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  "Res",     128,  0, NULL,               NULL          },
        {moduleTypeFltClassic, paramType1StandardToggle, paramType2Toggle, {{ 70, -10}, {7,  7}}, anchorBottomLeft,  NULL,        3,  0, fltClassicDbStrMap, NULL          },
        {moduleTypeFltClassic, paramType1Bypass,         paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, NULL,        2,  1, NULL,               NULL          },
    // 93 Unknown
    // 94 StChorus
        {moduleTypeStChorus,   paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Detune",  128,  0, NULL,               NULL          },  // 94
        {moduleTypeStChorus,   paramType1CommonDial,     paramType2Dial,   {{ 35,  -3}, {7, 14}}, anchorBottomLeft,  "Amount",  128,  0, NULL,               NULL          },
        {moduleTypeStChorus,   paramType1Bypass,         paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, NULL,        2,  1, NULL,               NULL          },
    // 95 Unknown
    // 96 OscD
    {moduleTypeOscD,       paramType1Freq,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Tune",    128, 64, NULL,               NULL          }, // 96 Tune
    {moduleTypeOscD,       paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          }, // 96 Fine
    {moduleTypeOscD,       paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 96 Kbt
    {moduleTypeOscD,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128,  0, NULL,               NULL          }, // 96 Pitch M
    {moduleTypeOscD,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 96 Bypass
    // 97 OscA
    {moduleTypeOscA,       paramType1Freq,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Tune",    128, 64, NULL,               NULL          }, // 97 Tune
    {moduleTypeOscA,       paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          }, // 97 Fine
    {moduleTypeOscA,       paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 97 Kbt
    {moduleTypeOscA,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128,  0, NULL,               NULL          }, // 97 Pitch M
    {moduleTypeOscA,       paramType1StandardToggle, paramType2Toggle, {{-20,   5}, {7,  7}}, anchorTopRight,    NULL,        6,  0, shapeOscATypeStrMap,    NULL   },     // 97 Waveform
    {moduleTypeOscA,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 97 Bypass
    {moduleTypeOscA,       paramType1StandardToggle, paramType2Toggle, {{ 30,  15}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          }, // 97 Pitch Type
    // 98 FreqShift
    {moduleTypeFreqShift,       paramType1Freq,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "FreqShift",    128, 64, NULL,               NULL          }, // 98 FreqShift
    {moduleTypeFreqShift,       paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 98 Mod
    {moduleTypeFreqShift,       paramType1StandardToggle, paramType2Toggle, {{60,   -10}, {7,  7}}, anchorBottomLeft,    NULL,        3,  0, freqShiftRangeStrMap,    NULL   },     // 98 Range
    {moduleTypeFreqShift,       paramType1Bypass,         paramType2Toggle, {{-10,  -10}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 98 Bypass
    // 99 Unknown
    // 100 Sw2-1
    {moduleTypeSw2to1,   paramType1StandardToggle, paramType2Toggle, {{ 50, -3}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, in8StrMap,   NULL          }, // 100 Selector
    // 101 Unknown
    // 102 FltPhase
    {moduleTypeFltPhase,   paramType1CommonDial,          paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",     128,  0, NULL,               NULL          }, // 102 Pitch M
    {moduleTypeFltPhase,   paramType1Freq,           paramType2Dial,   {{ 22,  -10}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },  // 102 Freq
    {moduleTypeFltPhase,   paramType1CommonDial,          paramType2Dial,   {{ 42,  -3}, {7, 14}}, anchorBottomLeft,  NULL,     128,  0, NULL,               NULL          }, // 102 Spread M
    {moduleTypeFltPhase,   paramType1CommonDial,          paramType2Dial,   {{ 57,  -17}, {7, 14}}, anchorBottomLeft,  "FB",     128,  64, NULL,               NULL          }, // 102 FB
    {moduleTypeFltPhase,   paramType1StandardToggle, paramType2Toggle, {{ 70, -3}, {7,  7}}, anchorBottomLeft,  "Notch",        6,  4, int16StrMap,   NULL          },// 102 Notch count
    {moduleTypeFltPhase,   paramType1CommonDial,          paramType2Dial,   {{ 38,  -17}, {7, 14}}, anchorBottomLeft,  "Spread",     128,  64, NULL,               NULL          }, // 102 Spread
    {moduleTypeFltPhase,   paramType1Bypass,         paramType2Toggle, {{-3,   -10}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 102 Bypass
    {moduleTypeFltPhase,   paramType1CommonDial,          paramType2Dial,   {{ 83,  -3}, {7, 14}}, anchorBottomLeft,  "Level",     128,  0, NULL,               NULL          },// 102 Level
    {moduleTypeFltPhase,   paramType1CommonDial,          paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  NULL,     128,  0, NULL,               NULL          }, // 102 FB Mod
    {moduleTypeFltPhase,   paramType1StandardToggle, paramType2Toggle, {{ 68, -12}, {7,  7}}, anchorBottomLeft,  NULL,        3,  0, fltPhaseTypeStrMap,   NULL          },// 102 Type
    {moduleTypeFltPhase,   paramType1StandardToggle, paramType2Toggle, {{ 3, -22}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          }, // 102 Kbt
    // 103 EqPeak
    {moduleTypeEqPeak,   paramType1Freq,           paramType2Dial,   {{ 22,  -3}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  64, NULL,               NULL          },  // 103 Freq
    {moduleTypeEqPeak,   paramType1BipLevel,           paramType2Dial,   {{ 42,  -3}, {7, 14}}, anchorBottomLeft,  "Gain",    128,  64, NULL,               NULL          },  // 103 Gain
    {moduleTypeEqPeak,   paramType1CommonDial,           paramType2Dial,   {{ 62,  -10}, {7, 14}}, anchorBottomLeft,  "BW",    128,  64, NULL,               NULL          },  // 103 BW *** needs UI element
    {moduleTypeEqPeak,   paramType1Bypass,         paramType2Toggle, {{-3,   -10}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 103 Bypass
    {moduleTypeEqPeak,   paramType1CommonDial,          paramType2Dial,   {{ 83,  -3}, {7, 14}}, anchorBottomLeft,  "Level",     128,  127, NULL,               NULL          },// 102 Level
    // 104 Unknown
    // 105 ValSw2-1
    {moduleTypeValSw2to1,   paramType1CommonDial,           paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "Ctrl Value",    64,  0, NULL,               NULL          },  // 105 Value
    // 106 OscNoise
    {moduleTypeOscNoise,       paramType1Freq,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Tune",    128, 64, NULL,               NULL          }, // 106 Tune
    {moduleTypeOscNoise,       paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          }, // 106 Fine
    {moduleTypeOscNoise,       paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 97 Kbt
    {moduleTypeOscNoise,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128,  0, NULL,               NULL          }, // 97 Pitch M
    {moduleTypeOscNoise,       paramType1StandardToggle, paramType2Toggle, {{ 30,  6}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          }, // 97 Pitch Type
    {moduleTypeOscNoise,       paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, {7, 14}}, anchorBottomLeft,  "Width",    128, 64, NULL,               NULL          }, // 106 Width
    {moduleTypeOscNoise,       paramType1CommonDial,     paramType2Dial,   {{ 65,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 106 Width M
    {moduleTypeOscNoise,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 106 Bypass

    // 107 Unknown
    // 108 Vocoder
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 10,  -3}, {5,  7}}, anchorBottomLeft,     "1",        16,  0, int16StrMap,    NULL          }, // 108 BandSel 1
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 15,  -3}, {5,  7}}, anchorBottomLeft,     "2",        16,  1, int16StrMap,    NULL          }, // 108 BandSel 2
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 20,  -3}, {5,  7}}, anchorBottomLeft,     "3",        16,  2, int16StrMap,    NULL          }, // 108 BandSel 3
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 25,  -3}, {5,  7}}, anchorBottomLeft,     "4",        16,  3, int16StrMap,    NULL         }, // 108 BandSel 4
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 30,  -3}, {5,  7}}, anchorBottomLeft,     "5",        16,  4, int16StrMap,    NULL          }, // 108 BandSel 5
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 35,  -3}, {5,  7}}, anchorBottomLeft,     "6",        16,  5, int16StrMap,    NULL          }, // 108 BandSel 6
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 40,  -3}, {5,  7}}, anchorBottomLeft,     "7",        16,  6, int16StrMap,    NULL          }, // 108 BandSel 7
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 45,  -3}, {5,  7}}, anchorBottomLeft,     "8",        16,  7, int16StrMap,    NULL          }, // 108 BandSel 8
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 50,  -3}, {5,  7}}, anchorBottomLeft,     "9",        16,  8, int16StrMap,    NULL          }, // 108 BandSel 9
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 55,  -3}, {5,  7}}, anchorBottomLeft,     "10",        16,  9, int16StrMap,    NULL          }, // 108 BandSel 10
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 60,  -3}, {5,  7}}, anchorBottomLeft,     "11",        16,  10, int16StrMap,    NULL          }, // 108 BandSel 11
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 65,  -3}, {5,  7}}, anchorBottomLeft,     "12",        16,  11, int16StrMap,    NULL          }, // 108 BandSel 12
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 70,  -3}, {5,  7}}, anchorBottomLeft,     "13",        16,  12, int16StrMap,    NULL          }, // 108 BandSel 13
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 75,  -3}, {5,  7}}, anchorBottomLeft,     "14",        16,  13, int16StrMap,    NULL          }, // 108 BandSel 14
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 80,  -3}, {5,  7}}, anchorBottomLeft,     "15",        16,  14, int16StrMap,    NULL          }, // 108 BandSel 15
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 85,  -3}, {5,  7}}, anchorBottomLeft,     "16",        16,  15, int16StrMap,    NULL          }, // 108 BandSel 16
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 3,  -12}, {7,  7}}, anchorBottomLeft,     "Emp",        2,  0, offOnStrMap,    NULL          }, // 108 Emphasis
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 3,  -24}, {7,  7}}, anchorBottomLeft,     "Mon",        2,  0, offOnStrMap,    NULL          }, // 108 Monitor
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 10,  -24}, {7,  7}}, anchorBottomLeft,     "-2",        1,  0, presetStrMap,    NULL          }, // 108 Preset -2
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{ 17,  -24}, {7,  7}}, anchorBottomLeft,     "-1",        1,  0, presetStrMap,    NULL          }, // 108 Preset -1
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{24,  -24}, {7,  7}}, anchorBottomLeft,     "-0",        1,  0, presetStrMap,    NULL          }, // 108 Preset 0
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{31,  -24}, {7,  7}}, anchorBottomLeft,     "+1",        1,  0, presetStrMap,    NULL          }, // 108 Preset +1
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{38,  -24}, {7,  7}}, anchorBottomLeft,     "+2",        1,  0, presetStrMap,    NULL          }, // 108 Preset +2
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{45,  -24}, {7,  7}}, anchorBottomLeft,     "Inv",        1,  0, presetStrMap,    NULL          }, // 108 Preset Inv
    {moduleTypeVocoder,       paramType1StandardToggle, paramType2Toggle, {{52,  -24}, {7,  7}}, anchorBottomLeft,     "Rnd",        1,  0, presetStrMap,    NULL          }, // 108 Preset Rnd
    // 109 Unknown
    // 110 Unknown
    // 111 Unknown
    // 112 LevAdd
    {moduleTypeLevAdd,       paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 112 Constant
    {moduleTypeLevAdd,    paramType1StandardToggle, paramType2Toggle, {{ 40, -3}, {7,  7}}, anchorBottomLeft,  NULL,       2,  0, bipUniStrMap, NULL},  // 112 Bip/Uni
    // 113 Fade1-2
    {moduleTypeFade1to2,       paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 113 Mix
    {moduleTypeFade1to2,       paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 113 Mix M
    // 114 Fade2-1
    {moduleTypeFade2to1,       paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 114 Mix
    {moduleTypeFade2to1,       paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 114 Mix M
    // 115 LevScaler
    {moduleTypeLevScaler, paramType1dB,     paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  "L",   128, 64, NULL,               NULL          },  // 115 Left slope +- 8 db/oct
    {moduleTypeLevScaler,       paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  "BP",    128, 64, NULL,               NULL          }, // 115 BreakPoint
    {moduleTypeLevScaler, paramType1dB,     paramType2Dial,   {{ 70,  -3}, {7, 14}}, anchorBottomLeft,  "R",   128, 64, NULL,               NULL          },  // 115 Right slope +- 8 db/oct
    {moduleTypeLevScaler,       paramType1StandardToggle, paramType2Toggle, {{ 22, -3}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 115 Kbt
    // 116 Mix8-1A
    {moduleTypeMix8to1A,  paramType1StandardToggle,     paramType2Toggle,   {{ 3,  -3}, {7, 7}}, anchorBottomLeft,  "Pad",    3, 0, db12BPadStrMap,               NULL          },  // 116 Pad
    // 117 LevMod
    {moduleTypeLevMod,       paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 0, NULL,               NULL          }, // 117 ModDepth
    {moduleTypeLevMod,       paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 64, NULL,               NULL          }, // 117 ModType
    // 118 Digitizer
    {moduleTypeDigitizer,  paramType1StandardToggle,     paramType2Toggle,   {{80,  -3}, {7, 7}}, anchorBottomLeft,  "Bits",    13, 11, bitsStrMap,               NULL          },  // 118 Bits
    {moduleTypeDigitizer,       paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  "Sample Rate",    128, 64, NULL,               NULL          }, // 118 Rate
    {moduleTypeDigitizer,       paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "Rate",    128, 0, NULL,               NULL          }, // 118 Rate M
    {moduleTypeDigitizer,       paramType1Bypass,         paramType2Toggle, {{-3,  -10}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 106 Bypass
    // 119 EnvADDSR
    {moduleTypeEnvADDSR,    paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, kbStrMap,           offOnColourMap}, // 119 Kb
    {moduleTypeEnvADDSR,    paramType1StandardToggle, paramType2Toggle, {{-30,  -31}, {7,  7}}, anchorBottomRight, NULL,        4,  0, envShapeStrMap,     NULL          },  // 119 Shape
    {moduleTypeEnvADDSR,    paramType1ADRTime,     paramType2Dial,   {{ 20,  -3}, {7, 14}}, anchorBottomLeft,  "A",  128,  0, NULL,               NULL          }, // 119 A
    {moduleTypeEnvADDSR,    paramType1ADRTime,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "D1",   128,  0, NULL,               NULL          }, // 119 D1
    {moduleTypeEnvADDSR,    paramType1CommonDial,     paramType2Dial,   {{ 44,  -3}, {7, 14}}, anchorBottomLeft,  "L1",     128,  0, NULL,               NULL          }, // 119 L1
    {moduleTypeEnvADDSR,    paramType1ADRTime,     paramType2Dial,   {{ 56,  -3}, {7, 14}}, anchorBottomLeft,  "D2",   128,  0, NULL,               NULL          }, // 119 D2
    {moduleTypeEnvADDSR,    paramType1CommonDial,     paramType2Dial,   {{ 68,  -3}, {7, 14}}, anchorBottomLeft,  "L2",     128,  0, NULL,               NULL          }, // 119 L2
    {moduleTypeEnvADDSR,    paramType1ADRTime,     paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "R",     128,  0, NULL,               NULL          }, // 119 R
    {moduleTypeEnvADDSR,    paramType1StandardToggle, paramType2Toggle, {{20, -24}, {7,  7}}, anchorBottomLeft, "Sustain",        2,  0, sustainStrMap,          NULL          }, // 119 Sustainmode
    {moduleTypeEnvADDSR,    paramType1StandardToggle, paramType2Toggle, {{-30, -24}, {7,  7}}, anchorBottomRight, NULL,        6,  0, posStrMap,          NULL          }, // 119 Pos
    {moduleTypeEnvADDSR,    paramType1StandardToggle, paramType2Toggle, {{  5,  -8}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, normalResetStrMap,  NULL          }, // 119 Reset
 
    // 120 Unknown
    // 121 SeqNote *** Postpone
    // 122 Unknown
    // 123 Mix4-1C
        {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 20,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L1
        {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 35,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L2
        {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 50,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L3
        {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 65,  15}, {7, 14}}, anchorTopLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L4
        {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{ 26,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap}, // 123 Enable1
        {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{ 41,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap},  // 123 Enable2
        {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{ 56,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap},  // 123 Enable3
        {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{ 71,  10}, {3,  3}}, anchorTopLeft,     NULL,        2,  0, NULL,               offOnColourMap}, // 123 Enable4
        {moduleTypeMix4to1C,   paramType1StandardToggle, paramType2Toggle, {{  3,  20}, {7,  7}}, anchorTopLeft,     "Pad",       2,  0, padStrMap,          NULL          }, // 123 Pad
        {moduleTypeMix4to1C,   paramType1StandardToggle, paramType2Toggle, {{  3,  10}, {7,  7}}, anchorTopLeft,     NULL,        3,  0, expStrMap,          NULL          }, // 123 Exp
    // 124 Mux8-1 -- No params
    // 125 WahWah
    {moduleTypeWahWah,   paramType1Freq,           paramType2Dial,   {{35,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 125 Sweep M
    {moduleTypeWahWah,   paramType1CommonDial,           paramType2Dial,   {{50,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 125 Sweep
    {moduleTypeWahWah,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          }, // 125 Bypass
    // 126 Name
    // 127 Fx-In
        {moduleTypeFxtoIn,     paramType1StandardToggle, paramType2Toggle, {{ 25,  -3}, {7,  7}}, anchorBottomLeft,  "In FX",     2,  0, inFxStrMap,         NULL          },  // 127 In
        {moduleTypeFxtoIn,     paramType1Bypass,         paramType2Toggle, {{-35,  -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 127 Bypass
        {moduleTypeFxtoIn,     paramType1StandardToggle, paramType2Toggle, {{  3,  -3}, {7,  7}}, anchorBottomLeft,  "Pad",       4,  0, db12PadStrMap,      NULL          }, // 127 Pad
    // 128 MinMax -- No params
    // 128 Unknown
    // 130 BinCounter -- No params
    // 131 ADConv  -- No params
    // 132 DAConv -- No params
    // 133 Unknown
    // 134 FltHP
    {moduleTypeFltHP,   paramType1Freq,           paramType2Dial,   {{50,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 134 Freq
    {moduleTypeFltHP,   paramType1CommonDial,           paramType2Dial,   {{35,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128,  64, NULL,               NULL          },  // 134 FreqMod
    {moduleTypeFltHP, paramType1StandardToggle, paramType2Toggle, {{ 10, -3}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          }, // 134 Kbt
    {moduleTypeFltHP,   paramType1Bypass,         paramType2Toggle, {{-10,   -3}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 134 Bypass
    // 135 Unknown
    // 136 Unknown
    // 137 Unknown
    // 138 Unknown
    // 139 T&H -- No params
    // 140 Mix4-1S
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 15,  -10}, {7, 14}}, anchorBottomLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L1
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 35,  -10}, {7, 14}}, anchorBottomLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L2
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 55,  -10}, {7, 14}}, anchorBottomLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L3
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 75,  -10}, {7, 14}}, anchorBottomLeft,     NULL,      128,  0, NULL,               NULL          },  // 123 L4
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{ 3,  -3}, {14,  4}}, anchorBottomLeft,     NULL,        2,  0, NULL,               offOnColourMap}, // 123 Enable1
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{ 23,  -3}, {14,  4}}, anchorBottomLeft,     NULL,        2,  0, NULL,               offOnColourMap},  // 123 Enable2
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{ 43,  -3}, {14,  4}}, anchorBottomLeft,     NULL,        2,  0, NULL,               offOnColourMap},  // 123 Enable3
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{ 63,  -3}, {14,  4}}, anchorBottomLeft,     NULL,        2,  0, NULL,               offOnColourMap}, // 123 Enable4
    {moduleTypeMix4to1S,   paramType1StandardToggle, paramType2Toggle, {{  40,  -25}, {7,  7}}, anchorBottomLeft,     NULL,        3,  0, expStrMap,          NULL          }, // 123 Exp
    // 141 CtrlSend
    {moduleTypeCtrlSend, paramType1CommonDial, paramType2Dial, {{ 40, -3}, {7,  14}}, anchorBottomLeft,  "Ctrl",       128,  1, NULL,   NULL          }, // 141 Ctrl
    {moduleTypeCtrlSend, paramType1CommonDial, paramType2Dial, {{ 75, -3}, {7,  14}}, anchorBottomLeft,  "Value",       128,  1, NULL,   NULL          }, // 141 Ctrl
    {moduleTypeCtrlSend, paramType1StandardToggle, paramType2Toggle, {{ 85, -3}, {7,  7}}, anchorBottomLeft,  "Chan",       21,  0, midiChanStrMap,   NULL          }, // 141 Channel
    // 142 PCSend
    {moduleTypePCSend, paramType1CommonDial, paramType2Dial, {{ 75, -3}, {7,  14}}, anchorBottomLeft,  "Program",       128,  1, NULL,   NULL          }, // 142 Program
    {moduleTypePCSend, paramType1StandardToggle, paramType2Toggle, {{ 85, -3}, {7,  7}}, anchorBottomLeft,  "Chan",       21,  0, midiChanStrMap,   NULL          }, // 142 Channel
    // 143 NoteSend
    {moduleTypeNoteSend, paramType1CommonDial, paramType2Dial, {{ 42, -3}, {7,  14}}, anchorBottomLeft,  "Vel",       128,  100, NULL,   NULL          }, // 143 Vel
    {moduleTypeNoteSend, paramType1NoteDial, paramType2Dial, {{ 62, -3}, {7,  14}}, anchorBottomLeft,  "Note",       128,  64, NULL,   NULL          }, // 143 Ctrl
    {moduleTypeNoteSend, paramType1StandardToggle, paramType2Toggle, {{ 85, -3}, {7,  7}}, anchorBottomLeft,  "Chan",       21,  0, midiChanStrMap,   NULL          }, // 143 Channel
    // 144 SeqEvent
    // 145 SeqVal
    // 146 SeqLev
    // 147 CtrRcv
    {moduleTypeCtrlRcv, paramType1CommonDial, paramType2Dial, {{ 40, -3}, {7,  14}}, anchorBottomLeft,  "Ctrl",       128,  64, NULL,   NULL          }, // 147 Ctrl
    {moduleTypeCtrlRcv, paramType1StandardToggle, paramType2Toggle, {{ 60, -3}, {7,  7}}, anchorBottomLeft,  "Chan",       18,  17, midiChanRcvStrMap,   NULL          }, // 147 Channel
    // 148 NoteRcv
    {moduleTypeNoteRcv, paramType1NoteDial, paramType2Dial, {{ 40, -3}, {7,  14}}, anchorBottomLeft,  "Note",       128,  64, NULL,   NULL          }, // 148 Note
    {moduleTypeNoteRcv, paramType1StandardToggle, paramType2Toggle, {{ 60, -3}, {7,  7}}, anchorBottomLeft,  "Chan",       18,  17, midiChanRcvStrMap,   NULL          }, // 148 Channel
    // 149 NoteZone
    {moduleTypeNoteZone, paramType1StandardToggle, paramType2Toggle, {{ 3, -3}, {7,  7}}, anchorBottomLeft,  "RChan",       18,  17, midiChanRcvStrMap,   NULL          }, // 149 Receive Channel
    {moduleTypeNoteZone, paramType1NoteDial, paramType2Dial, {{ 23, -3}, {7,  14}}, anchorBottomLeft,  "Min",       128,  0, NULL,   NULL          }, // 149 Min note
    {moduleTypeNoteZone, paramType1NoteDial, paramType2Dial, {{ 43, -3}, {7,  14}}, anchorBottomLeft,  "Max",       128,  127, NULL,   NULL          }, // 149 Max note
    {moduleTypeNoteZone, paramType1CommonDial, paramType2Dial, {{ 63, -3}, {7,  14}}, anchorBottomLeft,  "Trans",       128,  64, NULL,   NULL          }, // 149 Transpose
    {moduleTypeNoteZone, paramType1StandardToggle, paramType2Toggle, {{ 83, -3}, {7,  7}}, anchorBottomLeft,  "SChan",       21,  0, midiChanStrMap,   NULL          }, // 149 Send Channel
    {moduleTypeNoteZone, paramType1StandardToggle, paramType2Toggle, {{ 60, -17}, {7,  7}}, anchorBottomLeft,  "Thru",       2,  0, thruStrMap,   NULL          }, // 149 Thru mode
    // 150 Compress
        {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "Thr",      43,  0, NULL,               NULL          },  // 150
        {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 24,  -3}, {7, 14}}, anchorBottomLeft,  "Ratio",    67,  0, NULL,               NULL          },
        {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 38,  -3}, {7, 14}}, anchorBottomLeft,  "Att",     128,  0, NULL,               NULL          },
        {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 52,  -3}, {7, 14}}, anchorBottomLeft,  "Rel",     128,  0, NULL,               NULL          },
        {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 66,  -3}, {7, 14}}, anchorBottomLeft,  "RefLvl",   43,  0, NULL,               NULL          },
        {moduleTypeCompress,   paramType1StandardToggle, paramType2Toggle, {{ 27,   5}, {7,  7}}, anchorTopLeft,     NULL,        2,  1, sideChainStrMap,    offOnColourMap},
        {moduleTypeCompress,   paramType1Bypass,         paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, NULL,        2,  1, NULL,               NULL          },
    // 151 Unknown
    // 152 KeyQuant *** Skip for now
    // 153 Unknown
    // 154 SeqCtr
    // 155 Unknown
    // 156 NoteDet
    {moduleTypeNoteDet, paramType1NoteDial, paramType2Dial, {{ 23, -3}, {7,  14}}, anchorBottomLeft,  "Note",       128,  64, NULL,   NULL          }, // 156 Note
    // 157 LevConv
    {moduleTypeLevConv,    paramType1StandardToggle, paramType2Toggle, {{-15, -1}, {7,  7}}, anchorBottomRight, NULL,        6,  0, posStrMap,          NULL          }, // 157 OutType
    {moduleTypeLevConv,    paramType1StandardToggle, paramType2Toggle, {{-57, -3}, {7,  7}}, anchorBottomRight, "Input",        3,  0, levConvStrMap,          NULL          }, // 157 In Signal Type
    // 158 Glide
    {moduleTypeGlide,    paramType1Time,     paramType2Dial,   {{ 60,  -3}, {7, 14}}, anchorBottomLeft,  "Time",  128,  0, NULL,               NULL          }, // 158 Time
    {moduleTypeGlide,    paramType1StandardToggle, paramType2Toggle, {{40, -3}, {7,  7}}, anchorBottomLeft, NULL,        2,  0, offOnStrMap,          NULL          }, // 158 Glide on/off
    {moduleTypeGlide,    paramType1StandardToggle, paramType2Toggle, {{70, -3}, {7,  7}}, anchorBottomLeft, NULL,        2,  0, logStrMap,          NULL          }, // 158 Shape
    // 159 CompSig
    // 160 ZeroCnt
    // 161 MixFader -- skip for now
    // 162 FltComb
    {moduleTypeFltComb,   paramType1Freq,           paramType2Dial,   {{ 22,  -10}, {7, 14}}, anchorBottomLeft,  "Freq",    128,  0, NULL,               NULL          },  // 102 Freq
    {moduleTypeFltComb,   paramType1CommonDial,          paramType2Dial,   {{ 10,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",     128,  0, NULL,               NULL          }, // 102 Pitch M
    {moduleTypeFltComb,   paramType1StandardToggle, paramType2Toggle, {{ 3, -22}, {7,  7}}, anchorBottomLeft,  "Kbt",       5,  4, offTo100KbStrMap,   NULL          }, // 102 Kbt
    {moduleTypeFltComb,   paramType1CommonDial,          paramType2Dial,   {{ 57,  -17}, {7, 14}}, anchorBottomLeft,  "FB",     128,  64, NULL,               NULL          }, // 102 FB
    {moduleTypeFltComb,   paramType1CommonDial,          paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  NULL,     128,  0, NULL,               NULL          }, // 102 FB Mod
    {moduleTypeFltComb,   paramType1StandardToggle, paramType2Toggle, {{ 68, -12}, {7,  7}}, anchorBottomLeft,  NULL,        3,  0, fltPhaseTypeStrMap,   NULL          },// 102 Type
    {moduleTypeFltComb,   paramType1CommonDial,          paramType2Dial,   {{ 83,  -3}, {7, 14}}, anchorBottomLeft,  "Level",     128,  0, NULL,               NULL          },// 102 Level
    {moduleTypeFltComb,   paramType1Bypass,         paramType2Toggle, {{-3,   -10}, {5,  5}}, anchorBottomRight, NULL,        2,  1, NULL,               NULL          }, // 102 Bypass
    // 163 OscShpA
    {moduleTypeOscShpA,    paramType1Freq,     paramType2Dial,   {{ 32,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128, 64, NULL,               NULL          },  // 163 Coarse
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          }, // 163 Fine
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 163 Kbt
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",     128,  0, NULL,               NULL          }, // 163 Pitch M
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Toggle, {{ 15,  15}, {7,  7}}, anchorTopLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          }, // 163 Tune M
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, {7, 14}}, anchorBottomLeft,  "FM",      128,  0, NULL,               NULL          }, // FM
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Toggle, {{ 60,  15}, {7,  7}}, anchorTopLeft,     NULL,        2,  0, fmTypeStrMap,       NULL          }, // FM Pitch Track
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{-15,   5}, {7, 14}}, anchorTopRight,    "Shape",   128,  0, NULL,               NULL          },     // 50% to 99%, not decimals
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, {7, 14}}, anchorBottomLeft,  "ShpCtrl", 128,  0, NULL,               NULL          }, // 163 Shape M
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Toggle, {{ 30,  15}, {7,  7}}, anchorTopLeft,     NULL,        6,  0, oscShpAStrMap,    NULL          }, // 163 Wave
    {moduleTypeOscShpA,    paramType1Bypass,         paramType2Toggle, {{ -3,   0}, {5,  5}}, anchorMiddleRight, "Bypass",    2,  0, NULL,               NULL          },
    // 164 OscDual
    {moduleTypeOscDual,    paramType1Freq,     paramType2Dial,   {{ 25,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",   128, 64, NULL,               NULL          },  // 164 Coarse
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 35,  -3}, {7, 14}}, anchorBottomLeft,  "Cent",    128, 64, NULL,               NULL          }, // 164 Fine
    {moduleTypeOscDual,    paramType1StandardToggle, paramType2Toggle, {{ 12, -20}, {7,  7}}, anchorBottomLeft,  "Kbt",       2,  1, offOnStrMap,        offOnColourMap}, // 164 Kbt
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, {7, 14}}, anchorBottomLeft,  "Pitch",     128,  0, NULL,               NULL          }, // 164 Pitch M
    {moduleTypeOscDual,    paramType1StandardToggle, paramType2Toggle, {{ 25,  -17}, {7,  7}}, anchorBottomLeft,     NULL,        4,  0, pitchTypeStrMap,    NULL          }, // 164 Tune M
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 75,  -17}, {7, 14}}, anchorBottomLeft,  "SqrLvl",    128, 100, NULL,               NULL          }, // 164 SqrLvl
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 65,  -17}, {7, 14}}, anchorBottomLeft,  "PW",    128, 0, NULL,               NULL          }, // 164 PW
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, {7, 14}}, anchorBottomLeft,  "SawLvl",    128, 100, NULL,               NULL          }, // 164 SawLvl
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 65,  -3}, {7, 14}}, anchorBottomLeft,  "Phase",    128, 0, NULL,               NULL          }, // 164 Phase
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 90,  -17}, {7, 14}}, anchorBottomLeft,  "ubLvl",    128, 100, NULL,               NULL          }, // 164 Suboct Lvl
    {moduleTypeOscDual,    paramType1Bypass,         paramType2Toggle, {{ -10,   -3}, {5,  5}}, anchorBottomRight, "Bypass",    2,  0, NULL,               NULL          },
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 55,  -17}, {7, 14}}, anchorBottomLeft,  NULL,    128, 100, NULL,               NULL          }, // 164 Sqr M
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, {7, 14}}, anchorBottomLeft,  NULL,    128, 100, NULL,               NULL          }, // 164 Saw M
    {moduleTypeOscDual,    paramType1StandardToggle, paramType2Toggle, {{ -3,  -10}, {7,  7}}, anchorBottomRight,     "Soft",        2,  0, offOnStrMap,    NULL          }, // 164 Tune M


    // 165 DXRouter
    // 166 Unknown
    // 167 PShift
    // 168 Unknown
    // 169 ModAHD
    // 170 2-In
    // 171 4-In
    // 172 DlySingleA
    // 173 DlySingleB
    // 174 DelayDual
    // 175 DelayQuad
    // 176 DelayA
    // 177 DelayB
    // 178 DlyClock
    // 179 DlyShiftReg
    // 180 Operator
    // 181 DlyEight
    // 182 DlyStereo
    // 183 OscPM
    // 184 Mix1-1A
    // 185 Mix1-1S
    // 186 Sw1-2M
    // 187 Sw2-1M
    // 177 ConstSwM
    // 189 NoiseGate
    // 190 LfoB
    // 191 Unknown
    // 192 Phaser
    // 193 Mix4-1A
    // 194 Mix2-1A
    // 195 ModAmt
    // 196 OscPerc
    // 197 Status
    // 198 PitchTrack
    // 199 MonoKey
    // 200 RandomA
    // 201 Red2Blue
    // 202 RandomB
    // 203 Blue2Red
    // 204 RndClkA
    // 205 RndTrig
    // 206 RndClkB
    // 207 Unknown
    // 208 RndPattern
    
    };

// Note these need to be in same order of connectors which are referenced by the hardware
const tConnectorLocation connectorLocationList[] = {
    //  0 Unknown
    //  1 Keyboard
    {moduleTypeKeyboard,   connectorDirOut, connectorTypeControl, {{ 10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Pitch", labelLocLeft },  // 1
    {moduleTypeKeyboard,   connectorDirOut, connectorTypeLogic,   {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Gate",  labelLocLeft },
    {moduleTypeKeyboard,   connectorDirOut, connectorTypeControl, {{ 53,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Lin",   labelLocLeft },
    {moduleTypeKeyboard,   connectorDirOut, connectorTypeControl, {{ 93,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rel",   labelLocLeft },
    {moduleTypeKeyboard,   connectorDirOut, connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Note",  labelLocLeft },
    {moduleTypeKeyboard,   connectorDirOut, connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Exp",   labelLocLeft },
    // 2 Unknown
    // 3 4-Out
    {moduleType4toOut,     connectorDirIn,  connectorTypeAudio,   {{-24,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "1",     labelLocUp   }, // 3 4-out
    {moduleType4toOut,     connectorDirIn,  connectorTypeAudio,   {{-17,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "2",     labelLocUp   },
    {moduleType4toOut,     connectorDirIn,  connectorTypeAudio,   {{-10,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "3",     labelLocUp   },
    {moduleType4toOut,     connectorDirIn,  connectorTypeAudio,   {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "4",     labelLocUp   },
    //  4 2-Out
    {moduleType2toOut,     connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   }, // 4
    {moduleType2toOut,     connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },
    //  5 Invert
    {moduleTypeInvert,     connectorDirIn,  connectorTypeLogic,   {{-24,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, NULL, labelLocLeft}, // 5 Invert
    {moduleTypeInvert,     connectorDirOut, connectorTypeLogic,   {{-17,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, NULL, labelLocLeft}, // 5 Invert
    {moduleTypeInvert,     connectorDirIn,  connectorTypeLogic,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, NULL, labelLocLeft}, // 5 Invert
    {moduleTypeInvert,     connectorDirOut, connectorTypeLogic,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, NULL, labelLocLeft}, // 5 Invert
    //  6 Unknown
    //  7 OscB
    {moduleTypeOscB,       connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 7 OscB Pitch
    {moduleTypeOscB,       connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // PitchVar
    {moduleTypeOscB,       connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",  labelLocRight}, // Sync
    {moduleTypeOscB,       connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // FmMod
    {moduleTypeOscB,       connectorDirIn,  connectorTypeAudio,   {{ 73,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // Shape Mod
    {moduleTypeOscB,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // Out
    //  8 OscShpB
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 8 OscShpB
    //{moduleTypeOscShpB,    connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 8
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },
    {moduleTypeOscShpB,    connectorDirIn,  connectorTypeAudio,   {{ 73,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },
    {moduleTypeOscShpB,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    //  9 OscC
    {moduleTypeOscC,       connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 9 OscC PitchVar
    {moduleTypeOscC,       connectorDirIn,  connectorTypeAudio,   {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",  labelLocRight}, // Sync
    {moduleTypeOscC,       connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // FmMod
    {moduleTypeOscC,       connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // Pitch
    {moduleTypeOscC,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // Out
    // 10 Unknown
    // 11 Unknown
    // 12 Reverb
    {moduleTypeReverb,     connectorDirIn,  connectorTypeAudio,   {{-12,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "L",     labelLocLeft },  // 12 Reberb InL
    {moduleTypeReverb,     connectorDirIn,  connectorTypeAudio,   {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "R",     labelLocLeft },  // 12 Reberb InR
    {moduleTypeReverb,     connectorDirOut, connectorTypeAudio,   {{-12,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft },  // 12 Reberb OutL
    {moduleTypeReverb,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft },  // 12 Reberb InL
    // 13 OscString
    {moduleTypeOscString,  connectorDirIn,  connectorTypeAudio,   {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocLeft },  // 13 OscString Audio In
    {moduleTypeOscString,  connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocLeft },  // 13 OscString Pitch
    {moduleTypeOscString,  connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocLeft },  // 13 OscString Pitch Var
    {moduleTypeOscString,  connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft},   // 13 OscString Audio Out
    // 14 Unknown
    // 15 Sw8-1
    // 16 Unknown
    // 17 ValSw1-2
    // 18 X-Fade
    {moduleTypeXtoFade,    connectorDirIn,  connectorTypeControl, {{ 70,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",     labelLocLeft},    // 18 In1
    {moduleTypeXtoFade,    connectorDirIn,  connectorTypeControl, {{ 80,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",     labelLocLeft},    // 18 In2
    {moduleTypeXtoFade,    connectorDirIn,  connectorTypeControl, {{ 33,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp},    // 18 Mod
    {moduleTypeXtoFade,    connectorDirOut, connectorTypeControl, {{ 92,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp},    // 18 Out
    // 19 Mix4-1B
    // 20 EnvADSR
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl, {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },  // 20
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeLogic,   {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Gate",  labelLocRight},
    {moduleTypeEnvADSR,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",    labelLocRight},
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",   labelLocUp   },
    {moduleTypeEnvADSR,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    // 21 Mux1-8
    // 22 PartQuant
    // 23 ModADSR
    // 24 LfoC
    {moduleTypeLfoC,      connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   },
    {moduleTypeLfoC,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    // 25 LfoShpA
    {moduleTypeLfoShpA,   connectorDirIn,  connectorTypeControl, {{  10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   }, // Rate
    {moduleTypeLfoShpA,   connectorDirIn,  connectorTypeControl, {{  17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   }, // RateVar
    {moduleTypeLfoShpA,   connectorDirIn,  connectorTypeControl, {{  3,  -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "Rst",    labelLocUp   }, // Rst
    {moduleTypeLfoShpA,   connectorDirIn,  connectorTypeControl, {{  50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   }, // Shape M
    {moduleTypeLfoShpA,   connectorDirIn,  connectorTypeControl, {{  67,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   }, // Phase M
    {moduleTypeLfoShpA,   connectorDirIn,  connectorTypeControl, {{  10,  -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "Dir",    labelLocUp   }, // Dir
    {moduleTypeLfoShpA,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // Out
    {moduleTypeLfoShpA,   connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "Snc",    labelLocUp   }, // Snc
    // 26 LfoA
    {moduleTypeLfoA,   connectorDirIn,  connectorTypeControl, {{  3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   }, // Rate
    {moduleTypeLfoA,   connectorDirIn,  connectorTypeControl, {{  3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   }, // Rate
    {moduleTypeLfoA,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // Out
    // 27 OscMaster
    {moduleTypeOscMaster,       connectorDirIn,  connectorTypeControl, {{  3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 9 OscC Pitch
    {moduleTypeOscMaster,       connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 9 OscC PitchVar
    {moduleTypeOscMaster,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // Out
    // 28 Saturate
    {moduleTypeSaturate,    connectorDirIn,  connectorTypeControl, {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 28 In
    {moduleTypeSaturate,    connectorDirIn,  connectorTypeControl, {{ 35,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 28 Mod
    {moduleTypeSaturate,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 28 Out
    // 29 MetNoise
    {moduleTypeMetNoise,    connectorDirIn,  connectorTypeControl, {{ 32,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 29 FreqMod
    {moduleTypeMetNoise,    connectorDirIn,  connectorTypeControl, {{ 62,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 29 ColorMod
    {moduleTypeMetNoise,    connectorDirOut,  connectorTypeAudio, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 29 Out
    // 30 Device
    {moduleTypeDevice,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Wheel",    labelLocUp   },  // 30 Wheel
    {moduleTypeDevice,    connectorDirOut,  connectorTypeControl, {{ 18,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "AftTouch",    labelLocUp   },  // 30 AfterTouch
    {moduleTypeDevice,    connectorDirOut,  connectorTypeControl, {{ 33,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "CtrlPedal",    labelLocUp   },  // 30 ControlPedal
    {moduleTypeDevice,    connectorDirOut,  connectorTypeLogic, {{ 48,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Sustain",    labelLocUp   },  // 30 SustainPedal
    {moduleTypeDevice,    connectorDirOut,  connectorTypeControl, {{ 63,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Stick",    labelLocUp   },  // 30 PitchStick
    {moduleTypeDevice,    connectorDirOut,  connectorTypeControl, {{ 78,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "GWh1",    labelLocUp   },  // 30 Global Wheel 1
    {moduleTypeDevice,    connectorDirOut,  connectorTypeControl, {{ 88,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "GWh2",    labelLocUp   },  // 30 Global Wheel 2
    // 31 Noise
    {moduleTypeNoise,    connectorDirOut,  connectorTypeAudio, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 31 Out
    // 32 Eq2Band
    {moduleTypeEq2Band,    connectorDirIn,  connectorTypeAudio, {{ -3,   -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 32 In
    {moduleTypeEq2Band,    connectorDirOut,  connectorTypeAudio, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 32 Out
    // 33 Eq3Band
    {moduleTypeEq3band,    connectorDirIn,  connectorTypeAudio, {{ -3,   -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 33 In
    {moduleTypeEq3band,    connectorDirOut,  connectorTypeAudio, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 33 Out
    // 34 ShpExp
    {moduleTypeShpExp,    connectorDirIn,  connectorTypeControl, {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 34 In
    {moduleTypeShpExp,    connectorDirIn,  connectorTypeControl, {{ 35,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 34 Mod
    {moduleTypeShpExp,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 34 Out
    // 35 Driver *** What Module is this?
    // 36 SwOnOffM
    {moduleTypeSwOnOffM,    connectorDirIn,  connectorTypeControl, {{ -20,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 36 In
    {moduleTypeSwOnOffM,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 36 Out
    {moduleTypeSwOnOffM,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 36 Ctrl
    // 37 Unknown
    // 38 Pulse
    {moduleTypePulse,    connectorDirIn,  connectorTypeLogic, {{ -21,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 38 In
    {moduleTypePulse,    connectorDirIn,  connectorTypeControl, {{ 35,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 38 TimeM
    {moduleTypePulse,    connectorDirOut,  connectorTypeLogic, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 38 Out
    // 39 Unknown
    // 40 Mix8-1B
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 21,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "1",    labelLocUp   },  // 40 In1
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 30,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "2",    labelLocUp   },  // 40 In2
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 39,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "3",    labelLocUp   },  // 40 In3
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 48,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "4",    labelLocUp   },  // 40 In4
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 57,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "5",    labelLocUp   },  // 40 In5
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 66,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "6",    labelLocUp   },  // 40 In6
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 75,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "7",    labelLocUp   },  // 40 In7
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ 84,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "8",    labelLocUp   },  // 40 In8
    {moduleTypeMix8to1B,  connectorDirIn,  connectorTypeControl, {{ -3,   -28}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    "Chain",    labelLocLeft   },  // 40 Chain
    {moduleTypeMix8to1B,  connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 40 Out
    // 41 EnvH
    {moduleTypeEnvH,    connectorDirIn,  connectorTypeLogic, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Trig",    labelLocRight   },  // 41 Trig
    {moduleTypeEnvH,    connectorDirIn,  connectorTypeControl, {{  25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",    labelLocRight},
    {moduleTypeEnvH,    connectorDirIn,  connectorTypeControl, {{  -12,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 41 In
    {moduleTypeEnvH,    connectorDirOut, connectorTypeControl, {{-20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",   labelLocUp   }, // 41 Env
    {moduleTypeEnvH,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 41 Out
    // 42 Delay
    {moduleTypeDelay,    connectorDirIn,  connectorTypeLogic, {{ -21,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 42 In
    {moduleTypeDelay,    connectorDirIn,  connectorTypeControl, {{ 35,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 42 TimeM
    {moduleTypeDelay,    connectorDirOut,  connectorTypeLogic, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 42 Out
    // 43 Constant
    {moduleTypeConstant,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 43 Out
    // 44 LevMult
    {moduleTypeLevMult,    connectorDirIn,  connectorTypeControl, {{ -30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 44 In
    {moduleTypeLevMult,    connectorDirIn,  connectorTypeControl, {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 44 Mod
    {moduleTypeLevMult,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 43 Out
    // 45 FltVoice
    {moduleTypeFltVoice,connectorDirIn,  connectorTypeAudio, {{ -3,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 45 In
    {moduleTypeFltVoice,connectorDirIn,  connectorTypeControl, {{ 38,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 45 Vowel
    {moduleTypeFltVoice,connectorDirIn,  connectorTypeAudio, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 45 FreqMod
    {moduleTypeFltVoice,connectorDirOut,  connectorTypeAudio, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 45 Out
    // 46 EnvAHD
    {moduleTypeEnvAHD,    connectorDirIn,  connectorTypeLogic,   {{  3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Trig",  labelLocRight}, // 46 Trig
    {moduleTypeEnvAHD,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",    labelLocRight}, // 46 AM
    {moduleTypeEnvAHD,    connectorDirIn,  connectorTypeControl, {{  -3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 46 In
    {moduleTypeEnvAHD,    connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",   labelLocUp   }, // 46 Env
    {moduleTypeEnvAHD,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 46 Out
    // 47 Pan
    {moduleTypePan,    connectorDirIn,  connectorTypeControl, {{  -25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 47 In
    {moduleTypePan,    connectorDirIn,  connectorTypeControl, {{  20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 47 Mod
    {moduleTypePan,    connectorDirOut,  connectorTypeControl, {{  -15,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  "L",    labelLocLeft}, // 47 OutL
    {moduleTypePan,    connectorDirOut,  connectorTypeControl, {{  -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  "R",    labelLocLeft}, // 46 OutR
   // 48 MixStereo
    {moduleTypeMixStereo,    connectorDirIn,  connectorTypeControl, {{  10,  -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 48 In1
    {moduleTypeMixStereo,    connectorDirIn,  connectorTypeControl, {{  22,  -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 48 In2
    {moduleTypeMixStereo,    connectorDirIn,  connectorTypeControl, {{  34,  -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 48 In3
    {moduleTypeMixStereo,    connectorDirIn,  connectorTypeControl, {{  46,  -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 48 In4
    {moduleTypeMixStereo,    connectorDirIn,  connectorTypeControl, {{  58,  -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 48 In5
    {moduleTypeMixStereo,    connectorDirIn,  connectorTypeControl, {{  70,  -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 48 In6
    {moduleTypeMixStereo,    connectorDirOut,  connectorTypeControl, {{  -10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  "L",    labelLocUp}, // 48 OutL
    {moduleTypeMixStereo,    connectorDirOut,  connectorTypeControl, {{  -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  "R",    labelLocUp}, // 48 OutR
    // 49 FltMulti
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   }, // 49 In
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "LP",    labelLocLeft  },
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "BP",    labelLocLeft   },
    {moduleTypeFltMulti,   connectorDirOut, connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "HP",    labelLocLeft  },
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // PitchVar
    {moduleTypeFltMulti,   connectorDirIn,  connectorTypeControl, {{  3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // Pitch
    // 50 ConstSwT
    {moduleTypeConstSwT,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 50 Out
    // 51 FltNord
    {moduleTypeFltNord,    connectorDirIn,  connectorTypeAudio,   {{ -3,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 51 In
    {moduleTypeFltNord,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // Out
    {moduleTypeFltNord,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // PitchVar
    {moduleTypeFltNord,    connectorDirIn,  connectorTypeControl, {{ 3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // Pitch
    {moduleTypeFltNord,    connectorDirIn,  connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // FMLin
    {moduleTypeFltNord,    connectorDirIn,  connectorTypeControl, {{ 47,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // Res
    // 52 EnvMulti
    {moduleTypeEnvMulti,    connectorDirIn,  connectorTypeLogic,   {{  3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Gate",  labelLocRight}, // 52 Gate
    {moduleTypeEnvMulti,    connectorDirIn,  connectorTypeControl, {{  17,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,  "AM",    labelLocRight}, // 52 AM
    {moduleTypeEnvMulti,    connectorDirIn,  connectorTypeControl, {{  -10,  7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,  NULL,    labelLocRight}, // 52 In
    {moduleTypeEnvMulti,    connectorDirOut, connectorTypeControl, {{-3, 14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, "Env",   labelLocLeft   }, // 52 Env
    {moduleTypeEnvMulti,    connectorDirOut, connectorTypeControl, {{ -3,  7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, NULL,    labelLocUp   }, // 52 Out
    // 53 SandH
    {moduleTypeSandH,    connectorDirIn,  connectorTypeControl, {{  80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 53 In
    {moduleTypeSandH,    connectorDirIn,  connectorTypeLogic,   {{  60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Ctrl",  labelLocLeft}, // 53 Ctrl
    {moduleTypeSandH,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 53 Out
    // 54 FltStatic
    {moduleTypeFltStatic,    connectorDirIn,  connectorTypeAudio,   {{ -3,   7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   }, // 54 In
    {moduleTypeFltStatic,    connectorDirOut, connectorTypeAudio, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 54 Out
    // 55 EnvD
    {moduleTypeEnvD,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Trig",  labelLocRight}, // 55 Trig
    {moduleTypeEnvD,    connectorDirIn,  connectorTypeControl, {{  17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",    labelLocRight}, // 55 AM
    {moduleTypeEnvD,    connectorDirIn,  connectorTypeControl, {{  -10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 55 In
    {moduleTypeEnvD,    connectorDirOut, connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",   labelLocLeft   }, // 55 Env
    {moduleTypeEnvD,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 55 Out
    // 56 Resonator
    // 57 Automate
    // 58 Drumsynth
    {moduleTypeDrumSynth,    connectorDirIn,  connectorTypeLogic,   {{  3,  5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Trig",  labelLocUp}, // 55 Trig
    {moduleTypeDrumSynth,    connectorDirIn,  connectorTypeControl, {{  3,  -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Pitch",    labelLocUp}, // 55 Pitch M
    {moduleTypeDrumSynth,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",    labelLocUp}, // 55 Velocity
    {moduleTypeDrumSynth,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 55 Out
    // 59 CompLev
    {moduleTypeCompLev,    connectorDirIn,  connectorTypeControl, {{  40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "A",    labelLocLeft}, // 55 Pitch M
    {moduleTypeCompLev,    connectorDirOut, connectorTypeLogic, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "A>=C",    labelLocLeft   }, // 55 Out
    // 60 Mux8-1X
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",    labelLocUp}, // 60 In1
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  18,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",    labelLocUp}, // 60 In2
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  26,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",    labelLocUp}, // 60 In3
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  34,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",    labelLocUp}, // 60 In4
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  42,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",    labelLocUp}, // 60 In5
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",    labelLocUp}, // 60 In6
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",    labelLocUp}, // 60 In7
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  66,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",    labelLocUp}, // 60 In8
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",    labelLocUp}, // 60 Ctrl
    {moduleTypeMux8to1X,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 60 Out
    // 61 Clip
    {moduleTypeClip,    connectorDirIn,  connectorTypeControl, {{  -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocUp}, // 61 In
    {moduleTypeClip,    connectorDirIn,  connectorTypeControl, {{  33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp}, // 61 Mod
    {moduleTypeClip,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 61 Out
    // 62 OverDrive
    {moduleTypeOverdrive,    connectorDirIn,  connectorTypeControl, {{  -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocUp}, // 62 In
    {moduleTypeOverdrive,    connectorDirIn,  connectorTypeControl, {{  33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp}, // 62 Mod
    {moduleTypeOverdrive,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 62 Out
    // 63 Scratch
    {moduleTypeScratch,    connectorDirIn,  connectorTypeControl, {{  -3,  -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocUp}, // 63 In
    {moduleTypeScratch,    connectorDirIn,  connectorTypeControl, {{  6,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp}, // 63 Mod
    {moduleTypeScratch,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 63 Out
    // 64 Gate
    {moduleTypeGate,    connectorDirIn,  connectorTypeLogic,   {{  35,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,  labelLocRight}, // 64 In1_1
    {moduleTypeGate,    connectorDirIn,  connectorTypeLogic,   {{  30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,  labelLocRight}, // 64 In1_2
    {moduleTypeGate,    connectorDirIn,  connectorTypeLogic,   {{  65,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,  labelLocRight}, // 64 In2_1
    {moduleTypeGate,    connectorDirIn,  connectorTypeLogic,   {{  60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,  labelLocRight}, // 64 In2_1
    {moduleTypeGate,    connectorDirOut, connectorTypeLogic, {{ 52,  -6}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocLeft   }, // 64 Out1
    {moduleTypeGate,    connectorDirOut, connectorTypeLogic, {{ 82,  -6}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocLeft   }, // 64 Out1
    // 65 Unknown
    // 66 Mix2-1B
    {moduleTypeMix2to1B,    connectorDirIn,  connectorTypeControl, {{  33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp}, // 66 In1
    {moduleTypeMix2to1B,    connectorDirIn,  connectorTypeControl, {{  58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp}, // 66 In2
    {moduleTypeMix2to1B,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Chain",    labelLocRight}, // 66 Chain
    {moduleTypeMix2to1B,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 66 Out
    // 67 Unknown
    // 68 ClkGen
    {moduleTypeClkGen,    connectorDirIn,  connectorTypeLogic,   {{  3,  -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Reset",  labelLocRight}, // 68 Reset
    {moduleTypeClkGen,    connectorDirOut, connectorTypeLogic, {{ -3,  -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "1/96",    labelLocLeft   }, // 68 1/96
    {moduleTypeClkGen,    connectorDirOut, connectorTypeLogic, {{ -3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "1/16",    labelLocLeft   }, // 68 1/16
    {moduleTypeClkGen,    connectorDirOut, connectorTypeLogic, {{ -3,  -24}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "ClkActive",    labelLocLeft   }, // 68 1/96
    {moduleTypeClkGen,    connectorDirOut, connectorTypeLogic, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Sync",    labelLocLeft   }, // 68 1/96
    // 69 ClkDiv
    {moduleTypeClkDiv,    connectorDirIn,  connectorTypeLogic,   {{  20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Clock",  labelLocRight}, // 69 Clock
    {moduleTypeClkDiv,    connectorDirIn,  connectorTypeLogic,   {{  40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Reset",  labelLocRight}, // 69 Reset
    {moduleTypeClkDiv,    connectorDirOut, connectorTypeLogic, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 69 Out
    // 70 Unknown
    // 71 EnvFollow
    {moduleTypeEnvFollow,    connectorDirIn,  connectorTypeControl, {{  -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 71 In
    {moduleTypeEnvFollow,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 71 Out
    // 72 NoteScaler
    {moduleTypeNoteScaler,    connectorDirIn,  connectorTypeControl, {{  -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 72 In
    {moduleTypeNoteScaler,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 72 Out
    // 73 Unknown
    // 74 WaveWrap
    {moduleTypeWaveWrap,    connectorDirIn,  connectorTypeControl, {{  30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 74 Mod
    {moduleTypeWaveWrap,    connectorDirIn,  connectorTypeControl, {{  -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 74 In
    {moduleTypeWaveWrap,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 74 Out
    // 75 NoteQuant
    {moduleTypeNoteQuant,    connectorDirIn,  connectorTypeControl, {{  -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 75 In
    {moduleTypeNoteQuant,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 75 Out
    // 76 SwOnOffT
    {moduleTypeSwOnOffT,    connectorDirIn,  connectorTypeControl, {{ -20,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 76 In
    {moduleTypeSwOnOffT,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 76 Out
    {moduleTypeSwOnOffT,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 76 Ctrl
    // 77 Unknown
    // 78 Sw1-8
    {moduleTypeSw1to8,    connectorDirIn,  connectorTypeControl, {{ 15,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 78 In
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 28,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "1",    labelLocLeft   },  // 78 Out1
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 36,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "2",    labelLocLeft   },  // 78 Out2
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 44,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "3",    labelLocLeft   },  // 78 Out3
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 52,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "4",    labelLocLeft   },  // 78 Out4
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 60,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "5",    labelLocLeft   },  // 78 Out5
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 68,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "6",    labelLocLeft   },  // 78 Out6
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 76,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "7",    labelLocLeft   },  // 78 Out7
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 84,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "8",    labelLocLeft   },  // 78 Out8
    {moduleTypeSw1to8,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Ctrl",    labelLocRight   },  // 78 Ctrl
    // 79 Sw4-1
    {moduleTypeSw4to1,    connectorDirIn,  connectorTypeControl, {{ 20,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "1",    labelLocLeft   },  // 79 In1
    {moduleTypeSw4to1,    connectorDirIn,  connectorTypeControl, {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "2",    labelLocLeft   },  // 79 In2
    {moduleTypeSw4to1,    connectorDirIn,  connectorTypeControl, {{ 40,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "3",    labelLocLeft   },  // 79 In3
    {moduleTypeSw4to1,    connectorDirIn,  connectorTypeControl, {{ 50,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "4",    labelLocLeft   },  // 79 In4
    {moduleTypeSw4to1,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 79 Out
    {moduleTypeSw4to1,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Ctrl",    labelLocRight   },  // 79 Ctrl
    // 80 Unknown
    // 81 LevAmp
    {moduleTypeLevAmp,    connectorDirIn,  connectorTypeControl, {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 81 In
    {moduleTypeLevAmp,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 81 Out
    // 82 Rect
    {moduleTypeRect,    connectorDirIn,  connectorTypeControl, {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 82 In
    {moduleTypeRect,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 82 Out
    // 83 ShpStatic
    {moduleTypeShpStatic,    connectorDirIn,  connectorTypeControl, {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 82 In
    {moduleTypeShpStatic,    connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 82 Out
    // 84 EnvADR
    {moduleTypeEnvADR,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,  labelLocRight}, // 84 Trig/Gate
    {moduleTypeEnvADR,    connectorDirIn,  connectorTypeControl, {{  -3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 84 In
    {moduleTypeEnvADR,    connectorDirIn,  connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",    labelLocUp}, // 84 AM
    {moduleTypeEnvADR,    connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",   labelLocUp   }, // 84 Env
    {moduleTypeEnvADR,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 84 Out
    {moduleTypeEnvADR,    connectorDirOut,  connectorTypeLogic,   {{  -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,     NULL,  labelLocLeft}, // 46 End
    // 85 WindSw
    {moduleTypeWindSw,    connectorDirIn,  connectorTypeControl, {{  -10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocRight}, // 85 In
    {moduleTypeWindSw,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",    labelLocRight}, // 85 In
    {moduleTypeWindSw,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 85 Out
    {moduleTypeWindSw,    connectorDirOut, connectorTypeLogic, {{ -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 85 Gate
    // 86 8Counter
    {moduleType8Counter,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Clk",  labelLocRight}, // 86 Clock
    {moduleType8Counter,    connectorDirIn,  connectorTypeLogic,   {{  10,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Rst",  labelLocRight}, // 86 Reset
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "1",    labelLocUp   }, // 86 Out1
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "2",    labelLocUp   }, // 86 Out2
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "3",    labelLocUp   }, // 86 Out3
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "4",    labelLocUp   }, // 86 Out4
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "5",    labelLocUp   }, // 86 Out5
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "6",    labelLocUp   }, // 86 Out6
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "7",    labelLocUp   }, // 86 Out7
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic, {{ 90,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "8",    labelLocUp   }, // 86 Out8
    // 87 FltLP
    {moduleTypeFltLP, connectorDirIn,  connectorTypeAudio,   {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 87 In
    {moduleTypeFltLP, connectorDirIn,  connectorTypeControl, {{  28,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },  // 87 Mod
    {moduleTypeFltLP, connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 87 Out
    // 88 Sw1-4
    {moduleTypeSw1to4,    connectorDirIn,  connectorTypeControl, {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 88 In
    {moduleTypeSw1to4,    connectorDirOut,  connectorTypeControl, {{ 50,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "1",    labelLocLeft   },  // 88 Out1
    {moduleTypeSw1to4,    connectorDirOut,  connectorTypeControl, {{ 60,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "2",    labelLocLeft   },  // 88 Out2
    {moduleTypeSw1to4,    connectorDirOut,  connectorTypeControl, {{ 70,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "3",    labelLocLeft   },  // 88 Out3
    {moduleTypeSw1to4,    connectorDirOut,  connectorTypeControl, {{ 80,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "4",    labelLocLeft   },  // 88 Out4
    {moduleTypeSw1to4,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Ctrl",    labelLocRight   },  // 88 Ctrl
    // 89 Flanger
    {moduleTypeFlanger, connectorDirIn,  connectorTypeAudio,   {{ -3,   -13}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 89 In
    {moduleTypeFlanger, connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 89 Out
    // 90 Sw1-2
    {moduleTypeSw1to2,    connectorDirIn,  connectorTypeControl, {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 88 In
    {moduleTypeSw1to2,    connectorDirOut,  connectorTypeControl, {{ 50,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "1",    labelLocLeft   },  // 88 Out1
    {moduleTypeSw1to2,    connectorDirOut,  connectorTypeControl, {{ 60,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "2",    labelLocLeft   },  // 88 Out2
    {moduleTypeSw1to2,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Ctrl",    labelLocRight   },  // 88 Ctrl
    // 91 FlipFlop
    {moduleTypeFlipFlop,    connectorDirIn,  connectorTypeLogic,   {{  30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Clk",  labelLocLeft}, // 91 Clock
    {moduleTypeFlipFlop,    connectorDirIn,  connectorTypeLogic,   {{  15,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Rst",  labelLocLeft}, // 91 Reset
    {moduleTypeFlipFlop,    connectorDirIn,  connectorTypeLogic,   {{  35,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "In",  labelLocLeft}, // 91 In
    {moduleTypeFlipFlop,    connectorDirOut,  connectorTypeLogic,   {{  70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "NotQ",  labelLocRight}, // 91 NotQ
    {moduleTypeFlipFlop,    connectorDirOut,  connectorTypeLogic,   {{  80,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     "Q",  labelLocRight}, // 91 Q
    // 92 FltClassic
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },  // 92
    {moduleTypeFltClassic, connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },
    {moduleTypeFltClassic, connectorDirIn,  connectorTypeControl, {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,    labelLocUp   },
    // 93 Unknown
    // 94 StChorus
    {moduleTypeStChorus,   connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },  // 94
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    {moduleTypeStChorus,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    // 95 Unknown
    // 96 OscD
    {moduleTypeOscD,       connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocRight}, // 96 Pitch
    {moduleTypeOscD,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 96 Out
    // 97 OscA
    {moduleTypeOscA,       connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 97 Pitch
    {moduleTypeOscA,       connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 97  PitchVar
    {moduleTypeOscA,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 97 Out
    // 98 FreqShift
    {moduleTypeFreqShift,       connectorDirIn,  connectorTypeControl, {{  3, -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 98 Mod
    {moduleTypeFreqShift,   connectorDirIn,  connectorTypeAudio,   {{ -10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },  // 98 In
    {moduleTypeFreqShift,   connectorDirOut, connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Down",    labelLocLeft   }, //98 Down
    {moduleTypeFreqShift,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Up",    labelLocLeft   }, // 98 Up
    // 99 Unknown
    // 100 Sw2-1
    {moduleTypeSw2to1,    connectorDirIn,  connectorTypeControl, {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   },  // 100 In1
    {moduleTypeSw2to1,    connectorDirIn,  connectorTypeControl, {{ 40,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "1",    labelLocLeft   },  // 100 In2
    {moduleTypeSw2to1,    connectorDirOut,  connectorTypeControl, {{ 60,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "2",    labelLocLeft   },  // 100 Out
    {moduleTypeSw2to1,    connectorDirOut,  connectorTypeControl, {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Ctrl",    labelLocRight   },  // 100 Ctrl
    // 101 Unknown
    // 102 FltPhase
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeAudio,   {{ -3,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 102 In
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 102 PitchVar
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{  35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 102 Spr
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // 102 FM
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{ 3,  -12}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // 102 Pitch
    {moduleTypeFltPhase,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 102 Out
    // 103 EqPeak
    {moduleTypeEqPeak,    connectorDirIn,  connectorTypeAudio,   {{ -3,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 103 In
    {moduleTypeEqPeak,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 103 Out
    // 104 Unknown
    // 105 ValSw2-1
    {moduleTypeValSw2to1,    connectorDirIn,  connectorTypeControl,   {{ -24,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 105 In1
    {moduleTypeValSw2to1,    connectorDirIn,  connectorTypeControl,   {{ -10,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    "On",    labelLocLeft   }, // 105 In2
    {moduleTypeValSw2to1,    connectorDirIn,  connectorTypeControl,   {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Ctrl",    labelLocRight   }, // 105 Ctrl
    {moduleTypeValSw2to1,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 105 Out
    // 106 OscNoise
    {moduleTypeOscNoise,       connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 1106 Pitch
    {moduleTypeOscNoise,       connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 106  PitchVar
    {moduleTypeOscNoise,       connectorDirIn,  connectorTypeControl, {{  58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 106  Width
    {moduleTypeOscNoise,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 106 Out
    // 107 Unknown
    // 108 Vocoder
    {moduleTypeVocoder,       connectorDirIn, connectorTypeAudio,   {{ 3,  -35}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, "Ctrl",    labelLocRight   }, // 108 Ctrl
    {moduleTypeVocoder,       connectorDirIn, connectorTypeAudio,   {{ -3,  -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 108 In
    {moduleTypeVocoder,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 108 Out
    // 109 Unknown
    // 110 Unknown
    // 111 Unknown
    // 112 LevAdd
    {moduleTypeLevAdd,    connectorDirIn,  connectorTypeControl,   {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 112 In
    {moduleTypeLevAdd,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 112 Out
    // 113 Fade1-2
    {moduleTypeFade1to2,    connectorDirIn,  connectorTypeControl,   {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 113 In
    {moduleTypeFade1to2,    connectorDirIn,  connectorTypeControl,   {{ 33,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   }, // 113 Ctrl
    {moduleTypeFade1to2,    connectorDirOut, connectorTypeControl,   {{ -10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 113 Out1
    {moduleTypeFade1to2,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 113 Out2
    // 114 Fade2-1
    {moduleTypeFade2to1,    connectorDirIn,  connectorTypeControl,   {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 114 In1
    {moduleTypeFade2to1,    connectorDirIn, connectorTypeControl,   {{ -10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 114 In2
    {moduleTypeFade2to1,    connectorDirIn,  connectorTypeControl,   {{ 33,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   }, // 114 Ctrl
    {moduleTypeFade2to1,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 114 Out
    // 115 LevScaler
    {moduleTypeLevScaler,    connectorDirIn,  connectorTypeControl,   {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Note",    labelLocRight   }, // 115 Note
    {moduleTypeLevScaler,    connectorDirIn,  connectorTypeControl,   {{ -3,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 115 In
    {moduleTypeLevScaler,    connectorDirOut, connectorTypeControl,   {{ -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Level",    labelLocUp  }, // 115 Level
    {moduleTypeLevScaler,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 115 Out
    // 116 Mix8-1A
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 21,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "1",    labelLocUp   },  // 116 In1
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "2",    labelLocUp   },  // 116 In2
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 39,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "3",    labelLocUp   },  // 116 In3
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 48,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "4",    labelLocUp   },  // 116 In4
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 57,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "5",    labelLocUp   },  // 116 In5
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 66,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "6",    labelLocUp   },  // 116 In6
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 75,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "7",    labelLocUp   },  // 116 In7
    {moduleTypeMix8to1A,  connectorDirIn,  connectorTypeControl, {{ 84,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "8",    labelLocUp   },  // 116 In8
    {moduleTypeMix8to1A,  connectorDirOut,  connectorTypeControl, {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 116 Out
    // 117 LevMod
    {moduleTypeLevMod,    connectorDirIn,  connectorTypeControl,   {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocRight   }, // 117 In
    {moduleTypeLevMod,    connectorDirIn,  connectorTypeControl,   {{ -10,   -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    "Mod",    labelLocLeft  }, // 117 Mod
    {moduleTypeLevMod,    connectorDirIn, connectorTypeControl,   {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp  }, // 117 ModDepth
    {moduleTypeLevMod,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 115 Out
    // 118 Digitizer
    {moduleTypeDigitizer,    connectorDirIn,  connectorTypeControl,   {{ -3,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocRight   }, // 118 In
    {moduleTypeDigitizer,    connectorDirIn, connectorTypeControl,   {{ 13,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp  }, // 118 Rate M
    {moduleTypeDigitizer,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 118 Out
    // 119 EnvADDSR
    {moduleTypeEnvADDSR,    connectorDirIn,  connectorTypeLogic,   {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Gate",  labelLocRight}, // 119 Gate
    {moduleTypeEnvADDSR,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",    labelLocRight}, // 119 AM
    {moduleTypeEnvADDSR,    connectorDirIn,  connectorTypeControl, {{ -17,   10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },  // 119 In
    {moduleTypeEnvADDSR,    connectorDirOut, connectorTypeControl, {{-3,  24}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, "Env",   labelLocLeft  }, // 119 Env
    {moduleTypeEnvADDSR,    connectorDirOut, connectorTypeControl, {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight, NULL,    labelLocUp   }, // 119 Out
    // 120 Unknown
    // 121 SeqNote
    // 122 Unknown
    // 123 Mix4-1C
    {moduleTypeMix4to1C,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 123
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 20,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,    labelLocUp   },
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 35,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,    labelLocUp   },
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 50,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,    labelLocUp   },
    {moduleTypeMix4to1C,   connectorDirIn,  connectorTypeControl, {{ 65,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,    labelLocUp   },
    // 124 Mux8-1
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",    labelLocUp}, // 60 In1
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  18,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",    labelLocUp}, // 60 In2
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  26,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",    labelLocUp}, // 60 In3
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  34,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",    labelLocUp}, // 60 In4
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  42,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",    labelLocUp}, // 60 In5
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",    labelLocUp}, // 60 In6
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",    labelLocUp}, // 60 In7
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  66,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",    labelLocUp}, // 60 In8
    {moduleTypeMux8to1,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",    labelLocUp}, // 60 Ctrl
    {moduleTypeMux8to1,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocLeft   }, // 60 Out
    // 125 WahWah
    {moduleTypeWahWah,    connectorDirIn,  connectorTypeAudio,   {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 112 In
    {moduleTypeWahWah,    connectorDirIn,  connectorTypeControl,   {{ 28,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocUp   }, // 112 Sweep
    {moduleTypeWahWah,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 112 Out
    // 126 Name
    // 127 Fx-In
    {moduleTypeFxtoIn,     connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 127
    {moduleTypeFxtoIn,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    // 128 MinMax
    {moduleTypeMinMax,    connectorDirIn,  connectorTypeControl,   {{ 40,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "A",    labelLocLeft   }, // 128 In A
    {moduleTypeMinMax,    connectorDirIn,  connectorTypeControl,   {{ 55,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "B",    labelLocLeft   }, // 128 In B
    {moduleTypeMinMax,    connectorDirOut, connectorTypeControl,   {{ -17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Min",    labelLocLeft   }, // 128 Min Out
    {moduleTypeMinMax,    connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Max",    labelLocLeft   }, // 128 Max Out
    // 128 Unknown
    // 130 BinCounter
    {moduleTypeBinCounter,    connectorDirIn,  connectorTypeLogic,   {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Clk",    labelLocRight   }, // 130 Clk
    {moduleTypeBinCounter,    connectorDirIn,  connectorTypeLogic,   {{ 17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Rst",    labelLocRight   }, // 130 Rst
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "001",    labelLocUp   }, // 130 Out 001
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 38,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "002",    labelLocUp   }, // 130 Out 002
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 46,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "004",    labelLocUp   }, // 130 Out 004
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 54,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "008",    labelLocUp   }, // 130 Out 008
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 62,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "016",    labelLocUp   }, // 130 Out 016
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 70,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "032",    labelLocUp   }, // 130 Out 032
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 78,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "064",    labelLocUp   }, // 130 Out 064
    {moduleTypeBinCounter,    connectorDirOut,  connectorTypeLogic,   {{ 86,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "128",    labelLocUp   }, // 130 Out 128
    // 131 ADConv
    {moduleTypeADConv,    connectorDirIn,  connectorTypeControl,   {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Input",    labelLocRight   }, // 131 Input
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D0",    labelLocUp   }, // 131 Out D0
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 38,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D1",    labelLocUp   }, // 131 Out D1
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 46,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D2",    labelLocUp   }, // 131 Out D2
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 54,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D3",    labelLocUp   }, // 131 Out D3
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 62,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D4",    labelLocUp   }, // 131 Out D4
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 70,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D5",    labelLocUp   }, // 131 Out D5
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 78,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D6",    labelLocUp   }, // 131 Out D6
    {moduleTypeADConv,    connectorDirOut,  connectorTypeLogic,   {{ 86,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D7",    labelLocUp   }, // 131 Out D7
    // 132 DAConv
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 30,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D0",    labelLocUp   }, // 132 In D0
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 38,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D1",    labelLocUp   }, // 132 In D1
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 46,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D2",    labelLocUp   }, // 132 In D2
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 54,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D3",    labelLocUp   }, // 132 In D3
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 62,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D4",    labelLocUp   }, // 132 In D4
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 70,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D5",    labelLocUp   }, // 132 In D5
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 78,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D6",    labelLocUp   }, // 132 In D6
    {moduleTypeDAConv,    connectorDirIn,  connectorTypeLogic,   {{ 86,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "D7",    labelLocUp   }, // 132 In D7
    {moduleTypeDAConv,    connectorDirOut,  connectorTypeControl,   {{ -3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    "Out",    labelLocUp   }, // 132 Output
    // 133 Unknown
    // 134 FltHP
    {moduleTypeFltHP, connectorDirIn,  connectorTypeAudio,   {{ -17,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   },  // 134 In
    {moduleTypeFltHP, connectorDirIn,  connectorTypeControl, {{  28,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },  // 134 Mod
    {moduleTypeFltHP, connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 134 Out
    // 135 Unknown
    // 136 Unknown
    // 137 Unknown
    // 138 Unknown
    // 139 T&H
    {moduleTypeTandH,    connectorDirIn,  connectorTypeLogic,   {{ 60,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Ctrl",    labelLocLeft   }, // 139 Ctrl
    {moduleTypeTandH, connectorDirIn,  connectorTypeControl, {{  80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },  // 139 In
    {moduleTypeTandH, connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 139 Out

    // 140 Mix4-1S
   {moduleTypeMix4to1S,   connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 140 OutL
    {moduleTypeMix4to1S,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 140 OutR
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 3,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // 140 In1L
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 10,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   },// 140 In1R
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 23,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   },// 140 In2L
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 30,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   },// 140 In2R
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 43,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   },// 140 In3L
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 50,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   },// 140 In3R
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 63,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   },// 140 In4L
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ 70,  -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   },// 140 In4R
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },// 140 ChainL
    {moduleTypeMix4to1S,   connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },// 140 ChainR
    // 141 CtrlSend
    {moduleTypeCtrlSend,    connectorDirIn,  connectorTypeLogic,   {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Send",    labelLocRight   }, // 141 Send In
    {moduleTypeCtrlSend,    connectorDirOut,  connectorTypeLogic,   {{ 20,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocRight   }, // 141 Send Out
    {moduleTypeCtrlSend, connectorDirIn,  connectorTypeControl, {{  65,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },  // 141 Value
    // 142 PCSend
    {moduleTypePCSend,    connectorDirIn,  connectorTypeLogic,   {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Send",    labelLocRight   }, // 142 Send In
    {moduleTypePCSend,    connectorDirOut,  connectorTypeLogic,   {{ 20,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    NULL,    labelLocRight   }, // 142 Send Out
    {moduleTypePCSend, connectorDirIn,  connectorTypeControl, {{  65,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },  // 142 Program
    // 143 NoteSend
    {moduleTypeNoteSend,    connectorDirIn,  connectorTypeLogic,   {{ 3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Gate",    labelLocRight   }, // 143 Gate
    {moduleTypeNoteSend, connectorDirIn,  connectorTypeControl, {{  35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",    labelLocUp   },  // 143 Velocity
    {moduleTypeNoteSend, connectorDirIn,  connectorTypeControl, {{  55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Note",    labelLocUp   },  // 143 Note
    // 144 SeqEvent
    // 145 SeqVal
    // 146 SeqLev
    // 147 CtrRcv
    {moduleTypeCtrlRcv,    connectorDirOut,  connectorTypeLogic,   {{80,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Rcv",    labelLocUp   }, // 147 Rcv
    {moduleTypeCtrlRcv, connectorDirOut,  connectorTypeControl, {{  92,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Val",    labelLocUp   },  // 147 Value
    // 148 NoteRcv
    {moduleTypeNoteRcv,    connectorDirOut,  connectorTypeLogic,   {{74,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Rcv",    labelLocUp   }, // 148 Rcv
    {moduleTypeNoteRcv, connectorDirOut,  connectorTypeControl, {{  81,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",    labelLocUp   },  // 148 Vel
    {moduleTypeNoteRcv, connectorDirOut,  connectorTypeControl, {{  88,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "RVel",    labelLocUp   },  // 148 Release vel
    // 149 NoteZone
    // 150 Compress
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },  // 150
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },
    {moduleTypeCompress,   connectorDirIn,  connectorTypeAudio,   {{ 20,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,    labelLocUp   },
    {moduleTypeCompress,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    {moduleTypeCompress,   connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
 
    // 151 Unknown
    // 152 KeyQuant
    // 153 Unknown
    // 154 SeqCtr
    // 155 Unknown
    // 156 NoteDet
    {moduleTypeNoteDet,    connectorDirOut,  connectorTypeLogic,   {{70,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Gate",    labelLocUp   }, // 156 Gate
    {moduleTypeNoteDet, connectorDirOut,  connectorTypeControl, {{  80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",    labelLocUp   },  // 156 Vel
    {moduleTypeNoteDet, connectorDirOut,  connectorTypeControl, {{  90,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "RVel",    labelLocUp   },  // 156 Release vel
    // 157 LevConv
    {moduleTypeLevConv, connectorDirIn,  connectorTypeControl, {{  -23,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,  NULL,    labelLocUp   },  // 157 In
    {moduleTypeLevConv, connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 157 Out
    // 158 Glide
    {moduleTypeGlide, connectorDirIn,  connectorTypeControl, {{  80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },  // 139 In
    {moduleTypeGlide,    connectorDirIn,  connectorTypeLogic,   {{3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,    "Glide on",    labelLocRight  }, // 158 Glide On
    {moduleTypeGlide, connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 139 Out
    // 159 CompSig
    {moduleTypeCompSig, connectorDirIn,  connectorTypeControl, {{  60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "A",    labelLocLeft   },  // 159 A
    {moduleTypeCompSig, connectorDirIn,  connectorTypeControl, {{  75,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "B",    labelLocLeft   },  // 159 B
    {moduleTypeCompSig,    connectorDirOut,  connectorTypeLogic,   {{-3,   -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    "A>=B",    labelLocLeft  }, // 159 Cmp
    // 160 ZeroCnt
    {moduleTypeZeroCnt, connectorDirIn,  connectorTypeControl, {{  80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   },  // 160 In
    {moduleTypeZeroCnt, connectorDirOut, connectorTypeControl,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },  // 160 Out
    // 161 MixFader // Skip
    // 162 FltComb
    {moduleTypeFltComb,    connectorDirIn,  connectorTypeAudio,   {{ -3,   -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight,    NULL,    labelLocUp   }, // 162 In
    {moduleTypeFltComb,    connectorDirIn,  connectorTypeControl, {{ 3,  -12}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // 162 Pitch
    {moduleTypeFltComb,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 162 PitchVar
    {moduleTypeFltComb,    connectorDirIn,  connectorTypeControl, {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,     NULL,    labelLocUp   }, // 162 FM
    {moduleTypeFltComb,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 162 Out
    // 163 OscShpA
    {moduleTypeOscShpA,    connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 163 Pitch
    {moduleTypeOscShpA,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 163 PitchVar
    {moduleTypeOscShpA,    connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",    labelLocUp   }, // 163 Sync
    {moduleTypeOscShpA,    connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // FM
    {moduleTypeOscShpA,    connectorDirIn,  connectorTypeAudio,   {{ 73,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 163 Shape
    {moduleTypeOscShpA,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // Out
    // 164 OscDual
    {moduleTypeOscDual,    connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 164 Pitch
    {moduleTypeOscDual,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 164 PitchVar
    {moduleTypeOscDual,    connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",    labelLocUp   }, // 164 Sync
    {moduleTypeOscDual,    connectorDirIn,  connectorTypeAudio,   {{  48, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,    labelLocUp   }, // 164 PW
    {moduleTypeOscDual,    connectorDirIn,  connectorTypeAudio,   {{  48, -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft, NULL,    labelLocUp   }, // 164 Phase
    {moduleTypeOscDual,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   }, // 164 Out
    // 165 DXRouter
    // 166 Unknown
    // 167 PShift
    // 168 Unknown
    // 169 ModAHD
    // 170 2-In
    // 171 4-In
    // 172 DlySingleA
    // 173 DlySingleB
    // 174 DelayDual
    // 175 DelayQuad
    // 176 DelayA
    // 177 DelayB
    {moduleTypeDelayB,     connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,    labelLocUp   },  // 177
    {moduleTypeDelayB,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,    labelLocUp   },
    // 178 DlyClock
    // 179 DlyShiftReg
    // 180 Operator
    // 181 DlyEight
    // 182 DlyStereo
    // 183 OscPM
    // 184 Mix1-1A
    // 185 Mix1-1S
    // 186 Sw1-2M
    // 187 Sw2-1M
    // 177 ConstSwM
    // 189 NoiseGate
    // 190 LfoB
    // 191 Unknown
    // 192 Phaser
    // 193 Mix4-1A
    // 194 Mix2-1A
    // 195 ModAmt
    // 196 OscPerc
    // 197 Status
    // 198 PitchTrack
    // 199 MonoKey
    // 200 RandomA
    // 201 Red2Blue
    // 202 RandomB
    // 203 Blue2Red
    // 204 RndClkA
    // 205 RndTrig
    // 206 RndClkB
    // 207 Unknown
    // 208 RndPattern


};
// moduleType, paramType, offsetX, offsetY, label, range, defaultValue, string map
const tModeLocation   modeLocationList[] = {
    {moduleTypeOscShpB, paramType1StandardToggle, paramType2Dial, {{20, 5}, {7, 14}}, anchorTopLeft, "Wave", 8, 0, oscShpBStrMap}, // 8 OscShpB
    {moduleTypeOscC, paramType1StandardToggle, paramType2Toggle, {{-30,   5}, {7,  7}}, anchorTopRight, "Wave", 6, 0, shapeOscATypeStrMap}, // 9 Wave
    {moduleTypeReverb, paramType1StandardToggle, paramType2Toggle, {{3, -3}, {14, 7}}, anchorBottomLeft, "Type", 4, 0, reverbTypeStrMap}, // 12 Reverb
    //{moduleTypeLfoC,    paramType1StandardToggle,  paramType2Toggle, {{64, -3}, {7, 7}}, anchorBottomLeft, "Wave", 6, 0, lfoWaveStrMap}, // 24 LfoC
    {moduleTypePulse,    paramType1StandardToggle, paramType2Toggle, {{ -13, -3}, {7,  7}}, anchorBottomRight, "Mode", 2, 0, pulseModeStrMap}, // 38 Mode
    {moduleTypeDelay,    paramType1StandardToggle, paramType2Toggle, {{ -13, -3}, {7,  7}}, anchorBottomRight, "Mode", 2, 0, pulseModeStrMap}, // 42 Mode
    {moduleTypeGate,   paramType1StandardToggle, paramType2Toggle, {{ 40, -3}, {7,  7}}, anchorBottomLeft, "G1", 6, 0, gateTypeStrMap}, // 64 Gate 1 Type
    {moduleTypeGate,   paramType1StandardToggle, paramType2Toggle, {{ 70, -3}, {7,  7}}, anchorBottomLeft, "G2", 6, 0, gateTypeStrMap}, // 64 Gate 2 Type
    {moduleTypeClkDiv,  paramType1StandardToggle,     paramType2Toggle,   {{ 80,  -3}, {7, 7}}, anchorBottomLeft, "Mode", 2, 0, divModeStrMap}, // 69 DivMode
    {moduleTypeFltLP, paramType1StandardToggle, paramType2Toggle, {{ 67, -3}, {7,  7}}, anchorBottomLeft, "Slope", 6, 2, fltLPSlopeStrMap}, // 87 FltLP
    {moduleTypeFlipFlop,   paramType1StandardToggle, paramType2Toggle, {{ 45, -3}, {7,  7}}, anchorBottomLeft,  "Type",        2,  0, flipFlopStrMap}, // 91 Selector
    {moduleTypeOscD, paramType1StandardToggle, paramType2Toggle, {{-30,   5}, {7,  7}}, anchorTopRight, "Wave", 6, 0, shapeOscATypeStrMap}, // 96 Wave
    {moduleTypeFltHP, paramType1StandardToggle, paramType2Toggle, {{ 67, -3}, {7,  7}}, anchorBottomLeft, "Slope", 6, 2, fltLPSlopeStrMap}, // 134 FltLP
};

//{moduleTypeReverb,     paramType1StandardToggle, paramType2Toggle, {{  3,  -3}, {14, 7}}, anchorBottomLeft,  "Type",      4, 0, reverbTypeStrMap,    NULL          },
//{moduleTypeOscC,       paramType1StandardToggle, paramType2Toggle, {{-30,   5}, {7,  7}}, anchorTopRight,    NULL,        5,  0, shapeTypeStrMap,    NULL          },     // 9 Waveform
//{moduleTypeLfoC,    paramType1StandardToggle,  paramType2Toggle, {{64, -3}, {7, 7}}, anchorBottomLeft,  NULL,  6, 0,  lfoWaveStrMap,  NULL},  // 24 Wave
//    {moduleTypePulse,    paramType1StandardToggle, paramType2Toggle, {{ -13, -3}, {7,  7}}, anchorBottomRight,  NULL,       2,  0, pulseModeStrMap, NULL},  // 38 Range
//    {moduleTypeDelay,    paramType1StandardToggle, paramType2Toggle, {{ -13, -3}, {7,  7}}, anchorBottomRight,  NULL,       2,  0, pulseModeStrMap, NULL},  // 42 Mode
// {moduleTypeGate,   paramType1StandardToggle, paramType2Toggle, {{ 40, -3}, {7,  7}}, anchorBottomLeft,  NULL,        6,  0, gateTypeStrMap,   NULL          }, // 64 Gate 1 Type
// {moduleTypeGate,   paramType1StandardToggle, paramType2Toggle, {{ 70, -3}, {7,  7}}, anchorBottomLeft,  NULL,        6,  0, gateTypeStrMap,   NULL          }, // 64 Gate 2 Type
//    {moduleTypeClkDiv,  paramType1StandardToggle,     paramType2Toggle,   {{ 80,  -3}, {7, 7}}, anchorBottomLeft,  NULL,    2, 0, divModeStrMap,               NULL          },  // 69 DivMode
// {moduleTypeFltLP, paramType1StandardToggle, paramType2Toggle, {{ 67, -3}, {7,  7}}, anchorBottomLeft,  "Slope",       6,  2, fltLPSlopeStrMap,   NULL          },
// {moduleTypeFlipFlop,   paramType1StandardToggle, paramType2Toggle, {{ 45, -3}, {7,  7}}, anchorBottomLeft,  NULL,        2,  0, flipFlopStrMap,   NULL          }, // 91 Selector
//    {moduleTypeOscD,       paramType1StandardToggle, paramType2Toggle, {{-20,   5}, {7,  7}}, anchorTopRight,    NULL,        5,  0, shapeTypeStrMap,    NULL          },     // 96 Waveform


const tVolumeLocation volumeLocationList[] = {
    {moduleTypeMix4to1C, volumeTypeMono,     {{-15, 6}, {3, 25}}, anchorTopRight},
    {moduleTypeCompress, volumeTypeCompress, {{-18, 6}, {4, 38}}, anchorTopRight},
    {moduleType2toOut,   volumeTypeStereo,   {{-25, 6}, {3, 10}}, anchorTopRight},
    {moduleTypeFxtoIn,   volumeTypeStereo,   {{-25, 6}, {3, 10}}, anchorTopRight},
    {moduleTypeMix4to1S, volumeTypeStereo,   {{-10, 13}, {3, 15}}, anchorTopRight}, };

const tLedLocation    ledLocationList[] = {
    {moduleTypeEnvADSR, ledTypeYes, {{3, 8}, {3, 3}}, anchorTopLeft}, };

#endif // __MODULE_RESOURCES_H__
