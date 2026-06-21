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

// Terminate the string maps with a NULL, so that we can work out array size
const char *             fltClassicDbStrMap[]                    = {"12db", "18db", "24db", NULL};
const char *             fltMultiDbStrMap[]                      = {"6db", "12db", NULL};
const char *             emptyStrMap[]                           = {" ", NULL};
const char *             driverTypeStrMap[]                      = {"Reed", "Bow", "-Lip-", "-Mallet-", NULL};
const char *             octaveStrMap[]                          = {"C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", NULL};
const char *             bip128StrMap[]                          = {"-64", "-63", "-62", "-61", "-60", "-59", "-58", "-57", "-56", "-55", "-54", "-53", "-52", "-51", "-50", "-49", "-48", "-47", "-46", "-45", "-44", "-43", "-42", "-41", "-40", "-39", "-38", "-37", "-36", "-35", "-34", "-33", "-32", "-31", "-30", "-29", "-28", "-27", "-26", "-25", "-24", "-23", "-22", "-21", "-20", "-19", "-18", "-17", "-16", "-15", "-14", "-13", "-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63", NULL};
const char *             u128StrMap[]                            = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111", "112", "113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127"};
const char *             captureStrMap[]                         = {"Closest", "Evenly", NULL};
const char *             fltNordDbStrMap[]                       = {"12dB", "24dB", NULL};
const char *             offTo100KbStrMap[]                      = {"Off", "25%", "50%", "75%", "100%", NULL};
const char *             offOnStrMap[]                           = {"Off", "On", NULL};
const char *             expStrMap[]                             = {"Exp", "Lin", "dB", NULL};
const char *             logStrMap[]                             = {"Log", "Lin", NULL};
const char *             padStrMap[]                             = {"0dB", "-6dB", NULL};
const char *             db12PadStrMap[]                         = {"+6dB", "0dB", "-6dB", "-12dB", NULL};
const char *             db12BPadStrMap[]                        = {"0dB", "-6dB", "-12dB", NULL};
const char *             gcStrMap[]                              = {"GC", "GC", NULL};
const char *             kbStrMap[]                              = {"KB", "KB", NULL};
const char *             sideChainStrMap[]                       = {"Side Chain", "Side Chain", NULL};
const char *             pitchTypeStrMap[]                       = {"Semi", "Freq", "Factor", "Partial", NULL};
const char *             fmTypeStrMap[]                          = {"FM Lin", "FM Trk", NULL};
const char *             envShapeStrMap[]                        = {"LogExp", "LinExp", "ExpExp", "LinLin", NULL};
const char *             normalResetStrMap[]                     = {"Normal", "Reset", NULL};
const char *             posStrMap[]                             = {"Pos", "PosInv", "Neg", "NegInv", "Bip", "BipInv", NULL};
const char *             posNegInvStrMap[]                       = {"Pos", "PosInv", "Neg", "NegInv", NULL};
const char *             posNegInvBipStrMap[]                    = {"Pos", "PosInv", "Neg", "NegInv", "Bip", NULL};
const char *             bipPosNegStrMap[]                       = {"BiPol", "Pos", "Neg", NULL};
const char *             resonAlgStrMap[]                        = {"String1", "String2", "Tube1", "Tube2", "Tube3", NULL};
const char *             outToStrMap[]                           = {"Main 1/2", "Main 3/4", "FX 1/2", "FX 3/4", "Bus 1/2", "Bus 3/4", NULL};
const char *             outTo4OutStrMap[]                       = {"Main", "Fx", "Bus", NULL};
const char *             inFxStrMap[]                            = {"1/2", "3/4", NULL};
const char *             shapeTypeStrMap[]                       = {"sin", "tri", "saw", "squ", "sup", NULL};
const char *             shapeOscATypeStrMap[]                   = {"sin", "tri", "saw", "squ", "p25", "p10", NULL};
const char *             reverbTypeStrMap[]                      = {"Small", "Medium", "Large", "Hall", NULL};
const char *             polyMonoStrMap[]                        = {"Poly", "Mono", NULL};
const char *             rangeStrMap[]                           = {"Rate Lo", "Rate Hi", "BPM", "Rate Sub", "Clk", NULL};
const char *             lfoWaveStrMap[]                         = {"Sin", "Tri", "Saw", "Squ", "RndSt", "Rnd", NULL};
const char *             lfoShpAWaveStrMap[]                     = {"Sine", "CosBell", "TriBell", "Saw>Tri", "Tri>Squ", "Pulse", NULL};
const char *             rangeLfoCStrMap[]                       = {"Rate Lo", "Rate Hi", "BPM", "Rate Sub", NULL};
const char *             saturateCurveStrMap[]                   = {"1", "2", "3", "4", NULL};
const char *             shpExpCurveStrMap[]                     = {"x2", "x3", "x4", "x5", NULL};
const char *             pulseRangeStrMap[]                      = {"Sub", "Lo", "Hi", NULL};
const char *             pulseModeStrMap[]                       = {"Plus", "Minus", NULL};
const char *             logicDelayModeStrMap[]                  = {"Pos", "Neg", "Cycle", NULL};
const char *             oscPmWaveStrMap[]                       = {"Sin", "Tri", NULL};
const char *             rndClkBCharStrMap[]                     = {"Rnd1", "Rnd2", NULL};
const char *             bipUniStrMap[]                          = {"Bip", "Uni", NULL};
const char *             vowelStrMap[]                           = {"A", "E", "I", "O", "U", "Y", "AA", "AE", "OE", NULL};
const char *             nordFilterTypeStrMap[]                  = {"LP", "BP", "HP", "BR", NULL};
const char *             staticFilterTypeStrMap[]                = {"LP", "BP", "HP", NULL};
const char *             multiEnvSustainStrMap[]                 = {"L1", "L2", "L3", "Trg", NULL};
const char *             drSynthPresetStrMap[]                   = {"Kick 1", "Kick 2", "Kick 3", "Kick 4", "Kick 5", "Snare 1", "Snare 2", "Snare 3", "Snare 4", "Snare 5", "Tom1 1", "Tom1 2", "Tom1 3", "Tom2 1", "Tom2 2", "Tom2 3", "Tom3 1", "Tom3 2", "Tom3 3", "Cymb 1", "Cymb 2", "Cymb 3", "Cymb 4", "Cymb 5", "Perc 1", "Perc 2", "Perc 3", "Perc 4", "Perc 5", "Perc 6", NULL}; // *** Don't have the list
const char *             asymSymStrMap[]                         = {"Asym", "Sym", NULL};
const char *             odTypeStrMap[]                          = {"Soft", "Hard", "Heavy", "Fat", NULL};
const char *             delayStrMap[]                           = {"12.5ms", "25ms", "50ms", "100ms", NULL};
const char *             gateTypeStrMap[]                        = {"AND", "NAND", "OR", "NOR", "XOR", "NXOR", NULL};
const char *             invStrMap[]                             = {"+", "Inv", NULL};
const char *             clkSrcStrMap[]                          = {"Internal", "Master", NULL};
const char *             int16StrMap[]                           = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", NULL};
const char *             divModeStrMap[]                         = {"Gated", "Toggled", NULL};
const char *             out8StrMap[]                            = {"Out1", "Out2", "Out3", "Out4", "Out5", "Out6", "Out7", "Out8", NULL};
const char *             in8StrMap[]                             = {"In1", "In2", "In3", "In4", "In5", "In6", "In7", "In8", NULL};
const char *             rectStrMap[]                            = {"Half wave pos.", "Half wave neg.", "Full wave pos.", "Full wave neg.", NULL};
const char *             shpStaticStrMap[]                       = {"Inv x3", "Inv x2", "x2", "x3", NULL};
const char *             trigGateStrMap[]                        = {"Trig", "Gate", NULL};
const char *             decayReleaseStrMap[]                    = {"Decay", "Release", NULL};
const char *             fltLPSlopeStrMap[]                      = {"6db", "12db", "18db", "24db", "30db", "36db", NULL};
const char *             flipFlopStrMap[]                        = {"D-type", "RS-type", NULL};
const char *             freqShiftRangeStrMap[]                  = {"Hi", "Lo", "Sub", NULL};
const char *             fltPhaseTypeStrMap[]                    = {"Notch", "Peak", "Deep", NULL};
const char *             eq2BandLoStrMap[]                       = {"80Hz", "110Hz", "160Hz", NULL};
const char *             eq2BandHiStrMap[]                       = {"6kHz", "8kHz", "12kHz", NULL};
const char *             presetStrMap[]                          = {"Set", NULL};
const char *             bitsStrMap[]                            = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "Off", NULL};
const char *             sustainStrMap[]                         = {"L1", "L2", NULL};
const char *             midiChanStrMap[]                        = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "This", "SlotA", "SlotB", "SlotC", "SlotD", NULL};
const char *             midiChanRcvStrMap[]                     = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "This", "Keyb", NULL};
const char *             thruStrMap[]                            = {"Notes only", "Notes+Ctrls", NULL};
const char *             levConvStrMap[]                         = {"Bip", "Pos", "Neg", NULL};
const char *             oscShpAStrMap[]                         = {"Sine1", "Sine2", "Sine3", "Sine4", "TriSaw", "SymPulse", NULL};
const char *             oscShpBStrMap[]                         = {"Sine1", "Sine2", "Sine3", "Sine4", "TriSaw", "DblSaw", "Pulse", "SymPulse", NULL};
const char *             dxFeedbackStrMap[]                      = {"0", "1", "2", "3", "4", "5", "6", "7", NULL};
const char *             dxAlgStrMap[]                           = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", NULL};
const char *             pShiftDelayStrMod[]                     = {"12.5ms", "25ms", "50ms", "100ms", NULL};
const char *             twoToInSourceStrMap[]                   = {"In1/2", "In3/4", "Bus1/2", "Bus3/4", NULL};
const char *             fourToInSourceStrMap[]                  = {"In", "Bus", NULL};
const char *             delayRangeStrMap[]                      = {"5ms", "25ms", "100ms", "500ms", "1.0s", "2.0s", "2.7s", NULL};
const char *             delayABRangeStrMap[]                    = {"500ms", "1.0s", "2.0s", "2.7s", NULL};
const char *             dlyStereoRangeStrMap[]                  = {"500ms", "1.0s", "1.35s", NULL};
const char *             timeClkStrMap[]                         = {"Time", "Clk", NULL};
const char *             ratioFixedStrMap[]                      = {"Ratio", "Fixed", NULL};
const char *             detuneStrMap[]                          = {"-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", NULL};
const char *             operator07StrMap[]                      = {"0", "1", "2", "3", "4", "5", "6", "7", NULL};
const char *             operatorDepthStrMap[]                   = {"-Lin", "-Exp", "+Exp", "+Lin", NULL};
const char *             phaserTypeStrMap[]                      = {"Type I", "Type II", NULL};
const char *             invertStrMap[]                          = {"m", "1-m", NULL};
const char *             monoKeyStrMap[]                         = {"Last", "Lo", "Hi", NULL};
const char *             edgeStepStrMap[]                        = {"100%", "75%", "50%", "25%", "0%", NULL};
const char *             vocoderStrMap[]                         = {"Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", NULL};
const char *             sw8to1SelStrMap[]                       = {"Lo", "64'", "32'", "16'", "8'", "4'", "2'", "1'", NULL};
const char *             seqXFadeStrMap[]                        = {"Off", "25%", "50%", "100%", NULL};
const char *             clkSyncStrMap[]                         = {"64/1", "48/1",  "32/1",  "24/1", "16/1",  "12/1",  "8/1",  "6/1",   "4/1",  "3/1", "2/1",   "1/1D",
                                                                    "1/1",                                      "1/2D",  "1/1T",  "1/2",  "1/4D",  "1/2T",  "1/4",  "1/8D",  "1/4T", "1/8", "1/16D", "1/8T",
                                                                    "1/16",                                     "1/32D", "1/16T", "1/32", "1/64D", "1/32T", "1/64", "1/64T", NULL};
const char *             noteNameStrMap[]                        = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", NULL};
const char *             morphStrMap[]                           = {"Wheel", "Vel", "Keyb", "Aft.Tch", "Sust.Pd", "Ctrl.Pd", "P.Stick", "G.Wh", NULL};
const char *             env_timesStrMap[]                       = {"0.0005ms", "0.0006ms", "0.0007ms", "0.0009ms", "0.0011ms", "0.0013ms", "0.0015ms", "0.0018ms", "0.0021ms", "0.0025ms", "0.0030ms", "0.0035ms", "0.0040ms", "0.0047ms", "0.0055ms", "0.0063ms", "0.0073ms", "0.0084ms", "0.0097ms", "0.0111ms", "0.0127ms", "0.0145ms", "0.0165ms", "0.0187ms", "0.0212ms", "0.0240ms", "0.0271ms", "0.0306ms", "0.0344ms", "0.0387ms", "0.0434ms", "0.0486ms", "0.0543ms", "0.0606ms", "0.0676ms", "0.0752ms", "0.0836ms", "0.0928ms", "0.1030ms", "0.1140ms", "0.1260ms", "0.1390ms", "0.1530ms", "0.1690ms", "0.1860ms", "0.2040ms", "0.2240ms", "0.2460ms", "0.2690ms", "0.2950ms", "0.3220ms", "0.3520ms", "0.3840ms", "0.4190ms", "0.4560ms", "0.4960ms", "0.5400ms", "0.5860ms", "0.6360ms", "0.6900ms", "0.7480ms", "0.8100ms", "0.8760ms", "0.9470ms", "1.0200ms", "1.1000ms", "1.1900ms", "1.2800ms", "1.3800ms", "1.4900ms", "1.6000ms", "1.7200ms", "1.8500ms", "1.9900ms", "2.1300ms", "2.2800ms", "2.4600ms", "2.6200ms", "2.8100ms", "3.0000ms", "3.2100ms", "3.4300ms", "3.6600ms", "3.9100ms", "4.1700ms", "4.4500ms", "4.7400ms", "5.0500ms", "5.3700ms", "5.7200ms", "6.0800ms", "6.4700ms", "6.8700ms", "7.3000ms", "7.7500ms", "8.2200ms", "8.7200ms", "9.2500ms", "9.8000ms", "10.400ms", "11.000ms", "11.600ms", "12.300ms", "13.000ms", "13.800ms", "14.600ms", "15.400ms", "16.200ms", "17.100ms", "18.100ms", "19.100ms", "20.100ms", "21.200ms", "22.400ms", "23.500ms", "24.800ms", "26.100ms", "27.500ms", "28.900ms", "30.400ms", "32.000ms", "33.600ms", "35.300ms", "37.100ms", "38.900ms", "40.900ms", "42.900ms", "45.000ms", NULL};
const char *             unknown_timesStrMap[]                   = {"0.2m", "0.3m", "0.4m", "0.5m", "0.6m", "0.8m", "0.9m", "1.0m", "1.2m", "1.4m", "1.6m", "1.7m", "2.0m", "2.2m", "2.4m", "2.6m", "2.9m", "3.1m", "3.4m", "3.7m", "4.0m", "4.3m", "4.6m", "4.9m", "5.3m", "5.6m", "6.0m", "6.3m", "6.7m", "7.1m", "7.5m", "7.9m", "8.4m", "8.8m", "9.3m", "9.7m", "10.2m", "10.7m", "11.2m", "11.7m", "12.2m", "12.7m", "13.3m", "13.8m", "14.4m", "14.9m", "15.5m", "16.1m", "16.7m", "17.4m", "18.0m", "18.6m", "19.3m", "19.9m", "20.6m", "21.3m", "22.0m", "22.7m", "23.4m", "24.1m", "24.9m", "25.6m", "26.4m", "27.2m", "28.0m", "28.8m", "29.6m", "30.4m", "31.2m", "32.1m", "32.9m", "33.8m", "34.6m", "35.5m", "36.4m", "37.3m", "38.3m", "39.2m", "40.1m", "41.1m", "42.0m", "43.0m", "44.0m", "45.0m", "46.0m", "47.0m", "48.1m", "49.1m", "50.2m", "51.2m", "52.3m", "53.4m", "54.5m", "55.6m", "56.7m", "57.9m", "59.0m", "60.2m", "61.3m", "62.5m", "63.7m", "64.9m", "66.1m", "67.3m", "68.6m", "69.8m", "71.1m", "72.3m", "73.6m", "74.9m", "76.2m", "77.5m", "78.8m", "80.2m", "81.5m", "82.9m", "84.2m", "85.6m", "87.0m", "88.4m", "89.8m", "91.2m", "92.7m", "94.1m", "95.6m", "97.0m", "98.5m", "100m", NULL};
const char *             compr_attack_timesStrMap[]              = {"Fast", "0.53m", "0.56m", "0.59m", "0.63m", "0.67m", "0.71m", "0.75m", "0.79m", "0.84m", "0.89m", "0.94m", "1.00m", "1.06m", "1.12m", "1.19m", "1.26m", "1.33m", "1.41m", "1.50m", "1.59m", "1.68m", "1.78m", "1.89m", "2.00m", "2.12m", "2.24m", "2.38m", "2.52m", "2.67m", "2.83m", "3.00m", "3.17m", "3.36m", "3.56m", "3.78m", "4.00m", "4.24m", "4.49m", "4.76m", "5.04m", "5.34m", "5.66m", "5.99m", "6.35m", "6.73m", "7.13m", "7.55m", "8.00m", "8.48m", "8.98m", "9.51m", "10.1m", "10.7m", "11.3m", "12.0m", "12.7m", "13.5m", "14.3m", "15.1m", "16.0m", "17.0m", "18.0m", "19.0m", "20.2m", "21.4m", "22.6m", "24.0m", "25.4m", "26.9m", "28.5m", "30.2m", "32.0m", "33.9m", "35.9m", "38.1m", "40.3m", "42.7m", "45.3m", "47.9m", "50.8m", "53.8m", "57.0m", "60.4m", "64.0m", "67.8m", "71.8m", "76.1m", "80.6m", "85.4m", "90.5m", "95.9m", " 102m", " 108m", " 114m", " 121m", " 128m", " 136m", " 144m", " 152m", " 161m", " 171m", " 181m", " 192m", " 203m", " 215m", " 228m", " 242m", " 256m", " 271m", " 287m", " 304m", " 323m", " 342m", " 362m", " 384m", " 406m", " 431m", " 456m", " 483m", " 512m", " 542m", " 575m", " 609m", " 645m", " 683m", " 724m", " 767m", NULL};
const char *             compr_release_timesStrMap[]             = {" 125m", " 129m", " 134m", " 139m", " 144m", " 149m", " 154m", " 159m", " 165m", " 171m", " 177m", " 183m", " 189m", " 196m", " 203m", " 210m", " 218m", " 225m", " 233m", " 241m", " 250m", " 259m", " 268m", " 277m", " 287m", " 297m", " 308m", " 319m", " 330m", " 342m", " 354m", " 366m", " 379m", " 392m", " 406m", " 420m", " 435m", " 451m", " 467m", " 483m", " 500m", " 518m", " 536m", " 555m", " 574m", " 595m", " 616m", " 637m", " 660m", " 683m", " 707m", " 732m", " 758m", " 785m", " 812m", " 841m", " 871m", " 901m", " 933m", " 966m", "1.00s", "1.04s", "1.07s", "1.11s", "1.15s", "1.19s", "1.23s", "1.27s", "1.32s", "1.37s", "1.41s", "1.46s", "1.52s", "1.57s", "1.62s", "1.68s", "1.74s", "1.80s", "1.87s", "1.93s", "2.00s", "2.07s", "2.14s", "2.22s", "2.30s", "2.38s", "2.46s", "2.55s", "2.64s", "2.73s", "2.83s", "2.93s", "3.03s", "3.14s", "3.25s", "3.36s", "3.48s", "3.61s", "3.73s", "3.86s", "4.00s", "4.14s", "4.29s", "4.44s", "4.59s", "4.76s", "4.92s", "5.10s", "5.28s", "5.46s", "5.66s", "5.86s", "6.06s", "6.28s", "6.50s", "6.73s", "6.96s", "7.21s", "7.46s", "7.73s", "8.00s", "8.28s", "8.57s", "8.88s", "9.19s", "9.51s", "9.85s", "10.2s", NULL};
const char *             filter_resonanceStrMap[]                = {"0.50", "0.51", "0.51", "0.52", "0.53", "0.54", "0.55", "0.55", "0.56", "0.57", "0.58", "0.59", "0.60", "0.61", "0.62", "0.63", "0.64", "0.64", "0.66", "0.67", "0.68", "0.69", "0.70", "0.71", "0.73", "0.74", "0.75", "0.76", "0.78", "0.79", "0.81", "0.82", "0.84", "0.84", "0.87", "0.88", "0.90", "0.92", "0.94", "0.95", "0.97", "0.99", "1.01", "1.03", "1.06", "1.08", "1.10", "1.12", "1.15", "1.17", "1.20", "1.23", "1.25", "1.28", "1.31", "1.34", "1.37", "1.41", "1.44", "1.48", "1.51", "1.55", "1.59", "1.63", "1.67", "1.72", "1.76", "1.81", "1.86", "1.91", "1.97", "2.03", "2.08", "2.15", "2.21", "2.28", "2.35", "2.42", "2.50", "2.58", "2.67", "2.76", "2.85", "2.95", "3.05", "3.16", "3.28", "3.40", "3.53", "3.67", "3.81", "3.96", "4.13", "4.30", "4.49", "4.68", "4.89", "5.12", "5.36", "5.61", "5.89", "6.19", "6.51", "6.85", "7.23", "7.64", "8.08", "8.56", "9.08", "9.66", "10", "11", "12", "13", "14", "15", "16", "17", "19", "20", "22", "25", "27", "30", "34", "38", "44", "50", NULL};
const char *             phaser_freqStrMap[]                     = {"0.05Hz", "0.05Hz", "0.05Hz", "0.05Hz", "0.06Hz", "0.06Hz", "0.07Hz", "0.08Hz", "0.09Hz", "0.10Hz", "0.12Hz", "0.13Hz", "0.15Hz", "0.17Hz", "0.19Hz", "0.21Hz", "0.23Hz", "0.25Hz", "0.28Hz", "0.30Hz", "0.33Hz", "0.36Hz", "0.39Hz", "0.42Hz", "0.46Hz", "0.49Hz", "0.53Hz", "0.57Hz", "0.61Hz", "0.65Hz", "0.69Hz", "0.73Hz", "0.78Hz", "0.82Hz", "0.87Hz", "0.92Hz", "0.97Hz", "1.02Hz", "1.08Hz", "1.13Hz", "1.19Hz", "1.25Hz", "1.31Hz", "1.37Hz", "1.43Hz", "1.49Hz", "1.56Hz", "1.63Hz", "1.69Hz", "1.76Hz", "1.83Hz", "1.91Hz", "1.98Hz", "2.05Hz", "2.13Hz", "2.21Hz", "2.29Hz", "2.37Hz", "2.45Hz", "2.53Hz", "2.62Hz", "2.71Hz", "2.80Hz", "2.88Hz", "2.98Hz", "3.07Hz", "3.16Hz", "3.26Hz", "3.35Hz", "3.45Hz", "3.55Hz", "3.65Hz", "3.75Hz", "3.86Hz", "3.96Hz", "4.07Hz", "4.18Hz", "4.29Hz", "4.40Hz", "4.51Hz", "4.62Hz", "4.74Hz", "4.86Hz", "4.97Hz", "5.09Hz", "5.21Hz", "5.34Hz", "5.46Hz", "5.58Hz", "5.71Hz", "5.84Hz", "5.97Hz", "6.10Hz", "6.23Hz", "6.37Hz", "6.50Hz", "6.64Hz", "6.77Hz", "6.92Hz", "7.06Hz", "7.20Hz", "7.34Hz", "7.49Hz", "7.63Hz", "7.78Hz", "7.93Hz", "8.08Hz", "8.23Hz", "8.39Hz", "8.54Hz", "8.70Hz", "8.86Hz", "9.02Hz", "9.18Hz", "9.34Hz", "9.50Hz", "9.67Hz", "9.84Hz", "10.0Hz", "10.2Hz", "10.3Hz", "10.5Hz", "10.7Hz", "10.9Hz", "11.0Hz", "11.2Hz", "11.4Hz", "11.6Hz", NULL};
const char *             flanger_rateStrMap[]                    = {"0.01Hz", "0.02Hz", "0.05Hz", "0.07Hz", "0.09Hz", "0.11Hz", "0.14Hz", "0.16Hz", "0.18Hz", "0.21Hz", "0.23Hz", "0.25Hz", "0.27Hz", "0.30Hz", "0.32Hz", "0.34Hz", "0.37Hz", "0.39Hz", "0.41Hz", "0.43Hz", "0.46Hz", "0.48Hz", "0.50Hz", "0.53Hz", "0.55Hz", "0.57Hz", "0.60Hz", "0.62Hz", "0.64Hz", "0.66Hz", "0.69Hz", "0.71Hz", "0.73Hz", "0.76Hz", "0.78Hz", "0.80Hz", "0.82Hz", "0.85Hz", "0.87Hz", "0.89Hz", "0.92Hz", "0.94Hz", "0.96Hz", "0.98Hz", "1.01Hz", "1.03Hz", "1.05Hz", "1.08Hz", "1.10Hz", "1.12Hz", "1.14Hz", "1.17Hz", "1.19Hz", "1.21Hz", "1.24Hz", "1.26Hz", "1.28Hz", "1.30Hz", "1.33Hz", "1.35Hz", "1.37Hz", "1.40Hz", "1.42Hz", "1.44Hz", "1.46Hz", "1.49Hz", "1.51Hz", "1.53Hz", "1.56Hz", "1.58Hz", "1.60Hz", "1.63Hz", "1.65Hz", "1.67Hz", "1.69Hz", "1.72Hz", "1.74Hz", "1.76Hz", "1.79Hz", "1.81Hz", "1.83Hz", "1.85Hz", "1.88Hz", "1.90Hz", "1.92Hz", "1.95Hz", "1.97Hz", "1.99Hz", "2.01Hz", "2.04Hz", "2.06Hz", "2.08Hz", "2.11Hz", "2.13Hz", "2.15Hz", "2.17Hz", "2.20Hz", "2.22Hz", "2.24Hz", "2.27Hz", "2.29Hz", "2.31Hz", "2.33Hz", "2.36Hz", "2.38Hz", "2.40Hz", "2.43Hz", "2.45Hz", "2.47Hz", "2.49Hz", "2.52Hz", "2.54Hz", "2.56Hz", "2.59Hz", "2.61Hz", "2.63Hz", "2.66Hz", "2.68Hz", "2.70Hz", "2.72Hz", "2.75Hz", "2.77Hz", "2.79Hz", "2.82Hz", "2.84Hz", "2.86Hz", "2.88Hz", "2.91Hz", NULL};
const char *             freq_shift_subStrMap[]                  = {"0.000Hz", "0.000Hz", "0.000Hz", "0.000Hz", "0.000Hz", "0.001Hz", "0.001Hz", "0.001Hz", "0.002Hz", "0.003Hz", "0.004Hz", "0.006Hz", "0.007Hz", "0.009Hz", "0.012Hz", "0.014Hz", "0.018Hz", "0.021Hz", "0.025Hz", "0.029Hz", "0.034Hz", "0.040Hz", "0.046Hz", "0.052Hz", "0.059Hz", "0.067Hz", "0.075Hz", "0.084Hz", "0.094Hz", "0.10Hz", "0.12Hz", "0.13Hz", "0.14Hz", "0.15Hz", "0.17Hz", "0.18Hz", "0.20Hz", "0.22Hz", "0.24Hz", "0.25Hz", "0.27Hz", "0.30Hz", "0.32Hz", "0.34Hz", "0.37Hz", "0.39Hz", "0.42Hz", "0.45Hz", "0.47Hz", "0.50Hz", "0.54Hz", "0.57Hz", "0.60Hz", "0.64Hz", "0.67Hz", "0.71Hz", "0.75Hz", "0.79Hz", "0.84Hz", "0.88Hz", "0.93Hz", "0.97Hz", "1.02Hz", "1.07Hz", "1.12Hz", "1.18Hz", "1.23Hz", "1.29Hz", "1.35Hz", "1.41Hz", "1.47Hz", "1.53Hz", "1.60Hz", "1.67Hz", "1.74Hz", "1.81Hz", "1.88Hz", "1.96Hz", "2.03Hz", "2.11Hz", "2.19Hz", "2.28Hz", "2.36Hz", "2.45Hz", "2.54Hz", "2.63Hz", "2.73Hz", "2.82Hz", "2.92Hz", "3.02Hz", "3.12Hz", "3.23Hz", "3.34Hz", "3.45Hz", "3.56Hz", "3.67Hz", "3.79Hz", "3.91Hz", "4.03Hz", "4.16Hz", "4.29Hz", "4.42Hz", "4.55Hz", "4.68Hz", "4.82Hz", "4.96Hz", "5.11Hz", "5.25Hz", "5.40Hz", "5.55Hz", "5.71Hz", "5.86Hz", "6.02Hz", "6.18Hz", "6.35Hz", "6.52Hz", "6.69Hz", "6.87Hz", "7.04Hz", "7.22Hz", "7.41Hz", "7.59Hz", "7.78Hz", "7.98Hz", "8.17Hz", "8.37Hz", "8.57Hz", "8.78Hz", NULL};
const char *             freq_shift_loStrMap[]                   = {"0.000Hz", "0.000Hz", "0.000Hz", "0.001Hz", "0.003Hz", "0.006Hz", "0.010Hz", "0.016Hz", "0.024Hz", "0.035Hz", "0.048Hz", "0.063Hz", "0.082Hz", "0.10Hz", "0.13Hz", "0.16Hz", "0.20Hz", "0.23Hz", "0.28Hz", "0.33Hz", "0.38Hz", "0.44Hz", "0.51Hz", "0.58Hz", "0.66Hz", "0.74Hz", "0.84Hz", "0.94Hz", "1.05Hz", "1.16Hz", "1.29Hz", "1.42Hz", "1.56Hz", "1.71Hz", "1.87Hz", "2.04Hz", "2.22Hz", "2.41Hz", "2.61Hz", "2.83Hz", "3.05Hz", "3.28Hz", "3.53Hz", "3.79Hz", "4.06Hz", "4.34Hz", "4.64Hz", "4.95Hz", "5.27Hz", "5.61Hz", "5.96Hz", "6.32Hz", "6.70Hz", "7.09Hz", "7.50Hz", "7.93Hz", "8.37Hz", "8.82Hz", "9.30Hz", "9.79Hz", "10.3Hz", "10.8Hz", "11.4Hz", "11.9Hz", "12.5Hz", "13.1Hz", "13.7Hz", "14.3Hz", "15.0Hz", "15.7Hz", "16.3Hz", "17.1Hz", "17.8Hz", "18.5Hz", "19.3Hz", "20.1Hz", "20.9Hz", "21.8Hz", "22.6Hz", "23.5Hz", "24.4Hz", "25.3Hz", "26.3Hz", "27.2Hz", "28.2Hz", "29.3Hz", "30.3Hz", "31.4Hz", "32.5Hz", "33.6Hz", "34.7Hz", "35.9Hz", "37.1Hz", "38.3Hz", "39.6Hz", "40.9Hz", "42.2Hz", "43.5Hz", "44.8Hz", "46.2Hz", "47.6Hz", "49.1Hz", "50.6Hz", "52.1Hz", "53.6Hz", "55.2Hz", "56.7Hz", "58.4Hz", "60.0Hz", "61.7Hz", "63.4Hz", "65.2Hz", "66.9Hz", "68.8Hz", "70.6Hz", "72.5Hz", "74.4Hz", "76.3Hz", "78.3Hz", "80.3Hz", "82.3Hz", "84.4Hz", "86.5Hz", "88.7Hz", "90.8Hz", "93.1Hz", "95.3Hz", "97.6Hz", NULL};
const char *             freq_shift_hiStrMap[]                   = {"0.000Hz", "0.001Hz", "0.006Hz", "0.021Hz", "0.049Hz", "0.096Hz", "0.17Hz", "0.26Hz", "0.39Hz", "0.56Hz", "0.77Hz", "1.02Hz", "1.32Hz", "1.68Hz", "2.10Hz", "2.58Hz", "3.14Hz", "3.76Hz", "4.46Hz", "5.25Hz", "6.12Hz", "7.09Hz", "8.15Hz", "9.31Hz", "10.6Hz", "12.0Hz", "13.5Hz", "15.1Hz", "16.8Hz", "18.7Hz", "20.7Hz", "22.8Hz", "25.1Hz", "27.5Hz", "30.1Hz", "32.8Hz", "35.7Hz", "38.8Hz", "42.0Hz", "45.4Hz", "49.0Hz", "52.8Hz", "56.7Hz", "60.9Hz", "65.2Hz", "69.8Hz", "74.5Hz", "79.5Hz", "84.7Hz", "90.1Hz", "95.7Hz", "102Hz", "108Hz", "114Hz", "121Hz", "127Hz", "134Hz", "142Hz", "149Hz", "157Hz", "165Hz", "174Hz", "182Hz", "191Hz", "201Hz", "210Hz", "220Hz", "230Hz", "241Hz", "251Hz", "263Hz", "274Hz", "286Hz", "298Hz", "310Hz", "323Hz", "336Hz", "349Hz", "363Hz", "377Hz", "392Hz", "407Hz", "422Hz", "438Hz", "454Hz", "470Hz", "487Hz", "504Hz", "522Hz", "540Hz", "558Hz", "577Hz", "596Hz", "616Hz", "636Hz", "656Hz", "677Hz", "699Hz", "720Hz", "743Hz", "765Hz", "789Hz", "812Hz", "836Hz", "861Hz", "886Hz", "912Hz", "938Hz", "964Hz", "991Hz", "1019Hz", "1047Hz", "1075Hz", "1105Hz", "1134Hz", "1164Hz", "1195Hz", "1226Hz", "1258Hz", "1290Hz", "1323Hz", "1356Hz", "1390Hz", "1424Hz", "1459Hz", "1495Hz", "1531Hz", "1568Hz", NULL};
const char *             pulse_delay_rangeStrMap[]               = {"10.4ms", "11.1ms", "11.9ms", "12.8ms", "13.7ms", "14.7ms", "15.8ms", "16.9ms", "18.1ms", "19.4ms", "20.8ms", "22.3ms", "23.9ms", "25.6ms", "27.5ms", "29.4ms", "31.6ms", "33.8ms", "36.3ms", "38.9ms", "41.7ms", "44.8ms", "48.0ms", "51.5ms", "55.2ms", "59.3ms", "63.6ms", "68.2ms", "73.2ms", "78.5ms", "84.2ms", "90.4ms", "97.0ms", "104ms", "112ms", "120ms", "129ms", "138ms", "148ms", "159ms", "171ms", "183ms", "197ms", "211ms", "227ms", "244ms", "262ms", "281ms", "302ms", "324ms", "348ms", "374ms", "402ms", "432ms", "464ms", "498ms", "535ms", "575ms", "618ms", "664ms", "713ms", "767ms", "824ms", "886ms", "952ms", "1030ms", "1100ms", "1190ms", "1280ms", "1370ms", "1470ms", "1580ms", "1700ms", "1830ms", "1970ms", "2120ms", "2280ms", "2450ms", "2630ms", "2830ms", "3050ms", "3280ms", "3530ms", "3790ms", "4080ms", "4390ms", "4720ms", "5080ms", "5470ms", "5890ms", "6340ms", "6820ms", "7340ms", "7900ms", "8500ms", "9160ms", "9860ms", "10700ms", "11500ms", "12400ms", "13300ms", "14300ms", "15400ms", "16600ms", "17900ms", "19300ms", "20700ms", "22300ms", "24100ms", "25900ms", "27900ms", "30100ms", "32400ms", "34900ms", "37600ms", "40600ms", "43700ms", "47100ms", "50800ms", "54800ms", "59000ms", "63600ms", "68600ms", "74000ms", "79800ms", "86000ms", "92800ms", "100000ms", NULL};
const char *             env_follow_attackStrMap[]               = {"0.00ms", "0.53ms", "0.56ms", "0.60ms", "0.64ms", "0.67ms", "0.72ms", "0.76ms", "0.81ms", "0.86ms", "0.91ms", "0.97ms", "1.03ms", "1.09ms", "1.16ms", "1.23ms", "1.30ms", "1.38ms", "1.47ms", "1.56ms", "1.66ms", "1.76ms", "1.87ms", "1.98ms", "2.10ms", "2.23ms", "2.37ms", "2.52ms", "2.67ms", "2.84ms", "3.01ms", "3.20ms", "3.39ms", "3.60ms", "3.83ms", "4.06ms", "4.31ms", "4.58ms", "4.86ms", "5.16ms", "5.48ms", "5.82ms", "6.18ms", "6.56ms", "6.96ms", "7.39ms", "7.85ms", "8.33ms", "8.84ms", "9.39ms", "9.97ms", "10.6ms", "11.2ms", "11.9ms", "12.7ms", "13.4ms", "14.3ms", "15.2ms", "16.1ms", "17.1ms", "18.1ms", "19.3ms", "20.4ms", "21.7ms", "23.0ms", "24.5ms", "26.0ms", "27.6ms", "29.3ms", "31.1ms", "33.0ms", "35.0ms", "37.2ms", "39.5ms", "41.9ms", "44.5ms", "47.3ms", "50.2ms", "53.3ms", "56.5ms", "60.0ms", "63.7ms", "67.7ms", "71.8ms", "76.3ms", "81.0ms", "86.0ms", "91.3ms", "96.9ms", "103ms", "109ms", "116ms", "123ms", "131ms", "139ms", "147ms", "156ms", "166ms", "176ms", "187ms", "199ms", "211ms", "224ms", "238ms", "253ms", "268ms", "285ms", "302ms", "321ms", "341ms", "362ms", "384ms", "408ms", "433ms", "459ms", "488ms", "518ms", "550ms", "584ms", "620ms", "659ms", "699ms", "742ms", "787ms", "837ms", "888ms", "942ms", "1000ms", NULL};
const char *             env_follow_releaseStrMap[]              = {"10.0ms", "10.5ms", "10.9ms", "11.4ms", "12.0ms", "12.5ms", "13.1ms", "13.7ms", "14.3ms", "15.0ms", "15.7ms", "16.4ms", "17.1ms", "17.9ms", "18.8ms", "19.6ms", "20.5ms", "21.5ms", "22.4ms", "23.5ms", "24.6ms", "25.7ms", "26.9ms", "28.1ms", "29.4ms", "30.7ms", "32.1ms", "33.6ms", "35.2ms", "36.8ms", "38.5ms", "40.2ms", "42.1ms", "44.0ms", "46.0ms", "48.2ms", "50.4ms", "52.7ms", "55.1ms", "57.6ms", "60.3ms", "63.1ms", "66.0ms", "69.0ms", "72.2ms", "75.5ms", "78.9ms", "82.6ms", "86.3ms", "90.3ms", "94.5ms", "98.8ms", "103ms", "108ms", "113ms", "118ms", "124ms", "129ms", "135ms", "142ms", "148ms", "155ms", "162ms", "169ms", "177ms", "185ms", "194ms", "203ms", "212ms", "222ms", "232ms", "243ms", "254ms", "265ms", "278ms", "291ms", "304ms", "318ms", "332ms", "347ms", "363ms", "380ms", "398ms", "416ms", "435ms", "455ms", "476ms", "498ms", "521ms", "545ms", "570ms", "596ms", "624ms", "652ms", "682ms", "713ms", "747ms", "780ms", "816ms", "854ms", "894ms", "934ms", "977ms", "1020ms", "1070ms", "1120ms", "1170ms", "1220ms", "1280ms", "1340ms", "1400ms", "1460ms", "1530ms", "1600ms", "1680ms", "1760ms", "1840ms", "1920ms", "2010ms", "2100ms", "2200ms", "2300ms", "2410ms", "2520ms", "2630ms", "2760ms", "2870ms", "3000ms", NULL};
const char *             noise_gate_attackStrMap[]               = {"0.2ms", "0.3ms", "0.4ms", "0.5ms", "0.6ms", "0.8ms", "0.9ms", "1.0ms", "1.2ms", "1.4ms", "1.6ms", "1.7ms", "2.0ms", "2.2ms", "2.4ms", "2.6ms", "2.9ms", "3.1ms", "3.4ms", "3.7ms", "4.0ms", "4.3ms", "4.6ms", "4.9ms", "5.3ms", "5.6ms", "6.0ms", "6.3ms", "6.7ms", "7.1ms", "7.5ms", "7.9ms", "8.4ms", "8.8ms", "9.3ms", "9.7ms", "10.2ms", "10.7ms", "11.2ms", "11.7ms", "12.2ms", "12.7ms", "13.3ms", "13.8ms", "14.4ms", "14.9ms", "15.5ms", "16.1ms", "16.7ms", "17.4ms", "18.0ms", "18.6ms", "19.3ms", "19.9ms", "20.6ms", "21.3ms", "22.0ms", "22.7ms", "23.4ms", "24.1ms", "24.9ms", "25.6ms", "26.4ms", "27.2ms", "28.0ms", "28.8ms", "29.6ms", "30.4ms", "31.2ms", "32.1ms", "32.9ms", "33.8ms", "34.6ms", "35.5ms", "36.4ms", "37.3ms", "38.3ms", "39.2ms", "40.1ms", "41.1ms", "42.0ms", "43.0ms", "44.0ms", "45.0ms", "46.0ms", "47.0ms", "48.1ms", "49.1ms", "50.2ms", "51.2ms", "52.3ms", "53.4ms", "54.5ms", "55.6ms", "56.7ms", "57.9ms", "59.0ms", "60.2ms", "61.3ms", "62.5ms", "63.7ms", "64.9ms", "66.1ms", "67.3ms", "68.6ms", "69.8ms", "71.1ms", "72.3ms", "73.6ms", "74.9ms", "76.2ms", "77.5ms", "78.8ms", "80.2ms", "81.5ms", "82.9ms", "84.2ms", "85.6ms", "87.0ms", "88.4ms", "89.8ms", "91.2ms", "92.7ms", "94.1ms", "95.6ms", "97.0ms", "98.5ms", "100ms", NULL};
const char *             noise_gate_releaseStrMap[]              = {"0.5ms", "0.59ms", "0.67ms", "0.76ms", "0.87ms", "0.98ms", "1.11ms", "1.25ms", "1.40ms", "1.57ms", "1.75ms", "1.95ms", "2.17ms", "2.41ms", "2.66ms", "2.94ms", "3.25ms", "3.57ms", "3.92ms", "4.30ms", "4.71ms", "5.15ms", "5.62ms", "6.12ms", "6.66ms", "7.24ms", "7.85ms", "8.51ms", "9.21ms", "9.96ms", "10.7ms", "11.6ms", "12.5ms", "13.4ms", "14.4ms", "15.5ms", "16.6ms", "17.8ms", "19.1ms", "20.4ms", "21.8ms", "23.3ms", "24.9ms", "26.5ms", "28.2ms", "30.0ms", "32.0ms", "34.0ms", "36.1ms", "38.3ms", "40.6ms", "43.0ms", "45.5ms", "48.2ms", "51.0ms", "53.9ms", "56.9ms", "60.1ms", "63.4ms", "66.8ms", "70.4ms", "74.2ms", "78.1ms", "82.2ms", "86.4ms", "90.9ms", "95.5ms", "100ms", "105ms", "110ms", "116ms", "121ms", "127ms", "133ms", "139ms", "146ms", "153ms", "160ms", "167ms", "174ms", "182ms", "190ms", "198ms", "207ms", "216ms", "225ms", "234ms", "244ms", "254ms", "265ms", "275ms", "286ms", "298ms", "310ms", "322ms", "335ms", "348ms", "361ms", "375ms", "389ms", "404ms", "419ms", "434ms", "450ms", "467ms", "484ms", "501ms", "519ms", "537ms", "556ms", "578ms", "596ms", "616ms", "638ms", "659ms", "682ms", "705ms", "728ms", "752ms", "777ms", "802ms", "828ms", "855ms", "883ms", "911ms", "940ms", "970ms", "1000ms", NULL};
const char *             noisegate_pitchtrack_threshholdStrMap[] = {"-100.0dB", "-42.1dB", "-36.1dB", "-32.5dB", "-30.0dB", "-28.1dB", "-26.5dB", "-25.2dB", "-24.0dB", "-23.0dB", "-22.1dB", "-21.2dB", "-20.5dB", "-19.8dB", "-19.2dB", "-18.6dB", "-18.0dB", "-17.5dB", "-17.0dB", "-16.5dB", "-16.1dB", "-15.6dB", "-15.2dB", "-14.8dB", "-14.5dB", "-14.1dB", "-13.8dB", "-13.4dB", "-13.1dB", "-12.8dB", "-12.5dB", "-12.2dB", "-12.0dB", "-11.7dB", "-11.4dB", "-11.2dB", "-11.0dB", "-10.7dB", "-10.5dB", "-10.3dB", "-10.0dB", "-9.8dB", "-9.6dB", "-9.4dB", "-9.2dB", "-9.0dB", "-8.8dB", "-8.6dB", "-8.5dB", "-8.3dB", "-8.1dB", "-7.9dB", "-7.8dB", "-7.6dB", "-7.4dB", "-7.3dB", "-7.1dB", "-7.0dB", "-6.8dB", "-6.7dB", "-6.5dB", "-6.4dB", "-6.2dB", "-6.1dB", "-6.0dB", "-5.8dB", "-5.7dB", "-5.6dB", "-5.4dB", "-5.3dB", "-5.2dB", "-5.1dB", "-4.9dB", "-4.8dB", "-4.7dB", "-4.6dB", "-4.5dB", "-4.3dB", "-4.2dB", "-4.1dB", "-4.0dB", "-3.9dB", "-3.8dB", "-3.7dB", "-3.6dB", "-3.5dB", "-3.4dB", "-3.3dB", "-3.2dB", "-3.1dB", "-3.0dB", "-2.9dB", "-2.8dB", "-2.7dB", "-2.6dB", "-2.5dB", "-2.4dB", "-2.3dB", "-2.3dB", "-2.2dB", "-2.1dB", "-2.0dB", "-1.9dB", "-1.8dB", "-1.7dB", "-1.7dB", "-1.6dB", "-1.5dB", "-1.4dB", "-1.3dB", "-1.2dB", "-1.2dB", "-1.1dB", "-1.0dB", "-0.9dB", "-0.9dB", "-0.8dB", "-0.7dB", "-0.6dB", "-0.6dB", "-0.5dB", "-0.4dB", "-0.3dB", "-0.3dB", "-0.2dB", "-0.1dB", "-0.1dB", "-0.0dB", NULL};
const char *             patchVolumeStrMap[]                     = {"-78dB", "-77dB", "-76dB", "-74dB", "-73dB", "-72dB", "-71dB", "-70dB", "-68dB", "-67dB", "-66dB", "-65dB", "-64dB", "-63dB", "-62dB", "-61dB", "-60dB", "-59dB", "-58dB", "-57dB", "-56dB", "-55dB", "-54dB", "-53dB", "-52dB", "-51dB", "-50dB", "-49dB", "-48dB", "-48dB", "-47dB", "-46dB", "-45dB", "-44dB", "-43dB", "-43dB", "-42dB", "-41dB", "-40dB", "-39dB", "-39dB", "-38dB", "-37dB", "-36dB", "-36dB", "-35dB", "-34dB", "-34dB", "-33dB", "-32dB", "-32dB", "-31dB", "-30dB", "-30dB", "-29dB", "-28dB", "-28dB", "-27dB", "-27dB", "-26dB", "-26dB", "-25dB", "-24dB", "-24dB", "-23dB", "-23dB", "-22dB", "-22dB", "-21dB", "-21dB", "-20dB", "-20dB", "-19dB", "-19dB", "-18dB", "-18dB", "-17dB", "-17dB", "-16dB", "-16dB", "-15dB", "-15dB", "-15dB", "-14dB", "-14dB", "-13dB", "-13dB", "-12dB", "-12dB", "-12dB", "-11dB", "-11dB", "-11dB", "-10dB", "-9.8dB", "-9.4dB", "-9.1dB", "-8.7dB", "-8.4dB", "-8.0dB", "-7.7dB", "-7.4dB", "-7.0dB", "-6.7dB", "-6.4dB", "-6.1dB", "-5.7dB", "-5.4dB", "-5.1dB", "-4.8dB", "-4.5dB", "-4.2dB", "-3.9dB", "-3.6dB", "-3.4dB", "-3.1dB", "-2.8dB", "-2.5dB", "-2.3dB", "-2.0dB", "-1.7dB", "-1.5dB", "-1.2dB", "-1.0dB", "-0.7dB", "-0.5dB", "-0.2dB", "-0.0dB", NULL};
const char *             glide_timeStrMap[]                      = {"0.2ms", "0.2ms", "0.3ms", "0.4ms", "0.5ms", "0.6ms", "0.7ms", "0.9ms", "1.0ms", "1.2ms", "1.4ms", "1.7ms", "2.0ms", "2.3ms", "2.7ms", "3.1ms", "3.6ms", "4.2ms", "4.8ms", "5.5ms", "6.3ms", "7.2ms", "8.2ms", "9.3ms", "11ms", "12ms", "14ms", "15ms", "17ms", "19ms", "22ms", "24ms", "27ms", "30ms", "34ms", "38ms", "42ms", "46ms", "51ms", "57ms", "63ms", "69ms", "77ms", "84ms", "93ms", "102ms", "112ms", "123ms", "135ms", "147ms", "161ms", "178ms", "192ms", "209ms", "228ms", "248ms", "270ms", "293ms", "318ms", "345ms", "374ms", "405ms", "438ms", "473ms", "511ms", "552ms", "595ms", "642ms", "691ms", "744ms", "800ms", "860ms", "924ms", "992ms", "1100ms", "1100ms", "1200ms", "1300ms", "1400ms", "1500ms", "1600ms", "1700ms", "1800ms", "2000ms", "2100ms", "2200ms", "2400ms", "2500ms", "2700ms", "2900ms", "3000ms", "3200ms", "3400ms", "3600ms", "3900ms", "4100ms", "4400ms", "4600ms", "4900ms", "5200ms", "5500ms", "5800ms", "6100ms", "6500ms", "6900ms", "7300ms", "7700ms", "8000ms", "8600ms", "9000ms", "9500ms", "10100ms", "10600ms", "11100ms", "11700ms", "12400ms", "13000ms", "13700ms", "14400ms", "15200ms", "15800ms", "16800ms", "17500ms", "18300ms", "19200ms", "20200ms", "21200ms", "22400ms", NULL};
const char *             patch_settings_glideStrMap[]            = {"19ms", "20ms", "21ms", "23ms", "24ms", "26ms", "27ms", "29ms", "31ms", "33ms", "35ms", "37ms", "40ms", "42ms", "45ms", "48ms", "51ms", "55ms", "58ms", "62ms", "66ms", "71ms", "75ms", "80ms", "86ms", "91ms", "97ms", "104ms", "110ms", "118ms", "125ms", "133ms", "142ms", "151ms", "160ms", "170ms", "181ms", "192ms", "203ms", "215ms", "227ms", "240ms", "253ms", "267ms", "281ms", "296ms", "311ms", "326ms", "342ms", "358ms", "374ms", "390ms", "406ms", "423ms", "440ms", "456ms", "473ms", "490ms", "506ms", "523ms", "540ms", "556ms", "573ms", "589ms", "605ms", "621ms", "637ms", "653ms", "669ms", "685ms", "701ms", "717ms", "733ms", "749ms", "765ms", "781ms", "798ms", "815ms", "832ms", "849ms", "867ms", "885ms", "903ms", "922ms", "942ms", "962ms", "983ms", "1000ms", "1030ms", "1050ms", "1070ms", "1100ms", "1130ms", "1150ms", "1180ms", "1210ms", "1240ms", "1280ms", "1310ms", "1350ms", "1390ms", "1430ms", "1470ms", "1520ms", "1570ms", "1620ms", "1680ms", "1740ms", "1800ms", "1870ms", "1950ms", "2030ms", "2120ms", "2220ms", "2330ms", "2450ms", "2580ms", "2720ms", "2890ms", "3070ms", "3280ms", "3520ms", "3800ms", "4120ms", "4510ms", "4980ms", "5550ms", "6270ms", NULL};
const char *             dbLvlStrMap[]                           = {"-100.0dB", "-99.9dB", "-99.0dB", "-72.1dB", "-69.2dB", "-66.9dB", "-64.8dB", "-62.9dB", "-61.1dB", "-59dB", "-57.9dB", "-56.4dB", "-55.0dB", "-53.6dB", "-52.3dB", "-51.0dB", "-49.8dB", "-48.6dB", "-47.5dB", "-46.4dB", "-45.3dB", "-44.3dB", "-43.3dB", "-42.3dB", "-41.3dB", "-40.4dB", "-39.5dB", "-38.7dB", "-37.8dB", "-37.0dB", "-36.2dB", "-35.5dB", "-34.7dB", "-34.0dB", "-33.3dB", "-32.6dB", "-31.9dB", "-31.2dB", "-30.6dB", "-30.0dB", "-29.3dB", "-28.7dB", "-28.2dB", "-27.6dB", "-27.0dB", "-26.5dB", "-25.9dB", "-25.4dB", "-24.8dB", "-24.3dB", "-23.8dB", "-23.3dB", "-22.8dB", "-22.4dB", "-21.9dB", "-21.4dB", "-21.0dB", "-20.5dB", "-20.1dB", "-19.7dB", "-19.2dB", "-18.8dB", "-18.4dB", "-18.0dB", "-17.6dB", "-17.2dB", "-16.8dB", "-16.4dB", "-16.1dB", "-15.7dB", "-15.3dB", "-15.0dB", "-14.6dB", "-14.3dB", "-13.9dB", "-13.6dB", "-13.2dB", "-12.9dB", "-12.6dB", "-12.2dB", "-11.9dB", "-11.6dB", "-11.3dB", "-11.0dB", "-10.7dB", "-10.4dB", "-10.1dB", "-9.8dB", "-9.5dB", "-9.2dB", "-8.9dB", "-8.6dB", "-8.3dB", "-8.0dB", "-7.8dB", "-7.5dB", "-7.2dB", "-7.0dB", "-6.7dB", "-6.4dB", "-6.2dB", "-5.9dB", "-5.7dB", "-5.4dB", "-5.2dB", "-4.9dB", "-4.7dB", "-4.4dB", "-4.2dB", "-4.0dB", "-3.7dB", "-3.5dB", "-3.3dB", "-3.0dB", "-2.8dB", "-2.6dB", "-2.3dB", "-2.1dB", "-1.9dB", "-1.7dB", "-1.5dB", "-1.3dB", "-1.0dB", "-0.8dB", "-0.6dB", "-0.4dB", "-0.2dB", "0.0dB", NULL};
const char *             ADRTimeStrMap[]                         = {"0.5ms", "0.6ms", "0.7ms", "0.9ms", "1.1ms", "1.3ms", "1.5ms", "1.8ms", "2.1ms", "2.5ms", "3.0ms", "3.5ms", "4.0ms", "4.7ms", "5.5ms", "6.3ms", "7.3ms", "8.4ms", "9.7ms", "11.1ms", "12.7ms", "14.5ms", "16.5ms", "18.7ms", "21.2ms", "24.0ms", "27.1ms", "30.6ms", "34.4ms", "38.7ms", "43.4ms", "48.6ms", "54.3ms", "60.6ms", "67.6ms", "75.2ms", "83.6ms", "92.8ms", "103ms", "114ms", "126ms", "139ms", "153ms", "169ms", "186ms", "204ms", "224ms", "246ms", "269ms", "295ms", "322ms", "352ms", "384ms", "419ms", "456ms", "496ms", "540ms", "586ms", "636ms", "690ms", "748ms", "810ms", "876ms", "947ms", "1.02s", "1.10s", "1.19s", "1.28s", "1.38s", "1.49s", "1.60s", "1.72s", "1.85s", "1.99s", "2.13s", "2.28s", "2.45s", "2.62s", "2.81s", "3.00s", "3.21s", "3.43s", "3.66s", "3.91s", "4.17s", "4.45s", "4.74s", "5.05s", "5.37s", "5.72s", "6.08s", "6.47s", "6.87s", "7.30s", "7.75s", "8.22s", "8.72s", "9.25s", "9.80s", "10.4s", "11.0s", "11.6s", "12.3s", "13.0s", "13.8s", "14.6s", "15.4s", "16.2s", "17.1s", "18.1s", "19.1s", "20.1s", "21.2s", "22.4s", "23.5s", "24.8s", "26.1s", "27.5s", "28.9s", "30.4s", "32.0s", "33.6s", "35.3s", "37.1s", "38.9s", "40.9s", "42.9s", "45.0s", NULL};

const char *             pulseLoTimeStrMap[]                     = {"1.04ms", "1.11ms", "1.19ms", "1.28ms", "1.37ms", "1.47ms", "1.57ms", "1.69ms", "1.81ms", "1.94ms", "2.08ms", "2.23ms", "2.39ms", "2.56ms", "2.75ms", "2.94ms", "3.16ms", "3.38ms", "3.63ms", "3.89ms", "4.17ms", "4.48ms", "4.80ms", "5.15ms", "5.52ms", "5.93ms", "6.36ms", "6.82ms", "7.32ms", "7.85ms", "8.42ms", "9.04ms", "9.70ms", "10.4ms", "11.2ms", "12.0ms", "12.9ms", "13.8ms", "14.8ms", "15.9ms", "17.1ms", "18.3ms", "19.7ms", "21.1ms", "22.7ms", "24.4ms", "26.2ms", "28.1ms", "30.2ms", "32.4ms", "34.8ms", "37.4ms", "40.2ms", "43.2ms", "46.4ms", "49.8ms", "53.5ms", "57.5ms", "61.8ms", "66.4ms", "71.3ms", "76.7ms", "82.4ms", "88.6ms", "95.2ms", "102ms", "110ms", "118ms", "127ms", "137ms", "147ms", "158ms", "170ms", "183ms", "196ms", "211ms", "227ms", "244ms", "263ms", "283ms", "304ms", "327ms", "352ms", "379ms", "408ms", "439ms", "472ms", "508ms", "547ms", "588ms", "633ms", "681ms", "734ms", "790ms", "850ms", "915ms", "985ms", "1.07s", "1.15s", "1.24s", "1.33s", "1.43s", "1.54s", "1.66s", "1.79s", "1.93s", "2.07s", "2.23s", "2.41s", "2.59s", "2.79s", "3.01s", "3.24s", "3.49s", "3.76s", "4.06s", "4.37s", "4.71s", "5.08s", "5.48s", "5.90s", "6.36s", "6.86s", "7.40s", "7.98s", "8.60s", "9.28s", "10.0s", NULL};

const tRgb               offOnColourMap[]                        = {RGB_BACKGROUND_GREY, RGB_GREEN_ON};

const tRgb               gCableColourMap[cableColourMax]         = {
    {0.7, 0.1, 0.1},                                    // red a.k.a Audio
    {0.3, 0.3, 0.7},                                    // blue a.k.a Control
    {0.7, 0.7, 0.1},                                    // yellow a.k.a Logic BG
    {0.8, 0.3, 0.2},                                    // orange a.k.a Logic FG
    {0.1, 0.7, 0.1},                                    // green a.k.a User 1
    {0.7, 0.1, 0.7},                                    // purple a.k.a User 2
    {0.9, 0.9, 0.9},                                    // white - apparently not user selectable
};

const tRgb               gModuleColourMap[]                      = {MODULE_STANDARD_GREY,
                                                                    MODULE_RED_4,
                                                                    MODULE_GREEN_4,
                                                                    MODULE_BLUE_4,
                                                                    MODULE_YELLOW_4,
                                                                    MODULE_BLUE_1,
                                                                    MODULE_RED_1,
                                                                    MODULE_CYAN_2,
                                                                    MODULE_GREEN_2,
                                                                    MODULE_YELLOW_1,
                                                                    MODULE_GREEN_1,
                                                                    MODULE_YELLOW_2,
                                                                    MODULE_BLUE_3,
                                                                    MODULE_RED_2,
                                                                    MODULE_RED_3,
                                                                    MODULE_YELLOW_3,
                                                                    MODULE_GREEN_3,
                                                                    MODULE_CYAN_1,
                                                                    MODULE_CYAN_3,
                                                                    MODULE_CYAN_4,
                                                                    MODULE_BLUE_2,
                                                                    MODULE_PURPLE_1,
                                                                    MODULE_PURPLE_2,
                                                                    MODULE_PURPLE_3,
                                                                    MODULE_PURPLE_4};

const tRgb               connectorColourMap[] = {
    {0.7, 0.2, 0.2},
    {0.2, 0.2, 0.7},
    {0.9, 0.9, 0.0}, };


const tModuleProperties  gModuleProperties[]  = {
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Keyboard",     2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"4-Out",        2, volumeTypeQuad,      ledTypeNo },
    {"2-Out",        2, volumeTypeStereo,    ledTypeNo },
    {"Invert",       2, volumeTypeNone,      ledTypeYes},
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"OscB",         5, volumeTypeNone,      ledTypeNo },
    {"OscShpB",      4, volumeTypeNone,      ledTypeNo },
    {"OscC",         3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Reverb",       3, volumeTypeNone,      ledTypeNo },
    {"OscString",    3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Sw8-1",        4, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"ValSw1-2",     2, volumeTypeNone,      ledTypeNo },
    {"X-Fade",       2, volumeTypeNone,      ledTypeNo },
    {"Mix4-1B",      2, volumeTypeMono,      ledTypeNo },
    {"EnvADSR",      4, volumeTypeNone,      ledTypeYes},
    {"Mux1-8",       2, volumeTypeSequencer, ledTypeNo },
    {"PartQuant",    2, volumeTypeNone,      ledTypeNo },
    {"ModADSR",      5, volumeTypeNone,      ledTypeNo },
    {"LfoC",         2, volumeTypeNone,      ledTypeNo },
    {"LfoShpA",      5, volumeTypeNone,      ledTypeNo },
    {"LfoA",         3, volumeTypeNone,      ledTypeNo },
    {"OscMaster",    3, volumeTypeNone,      ledTypeNo },
    {"Saturate",     2, volumeTypeNone,      ledTypeNo },
    {"MetNoise",     2, volumeTypeNone,      ledTypeNo },
    {"Device",       3, volumeTypeNone,      ledTypeNo },
    {"Noise",        2, volumeTypeNone,      ledTypeNo },
    {"Eq2Band",      3, volumeTypeMono,      ledTypeNo },
    {"Eq3band",      4, volumeTypeMono,      ledTypeNo },
    {"ShpExp",       2, volumeTypeNone,      ledTypeNo },
    {"Driver",       3, volumeTypeNone,      ledTypeNo },
    {"SwOnOffM",     2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Pulse",        2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Mix8-1B",      4, volumeTypeMono,      ledTypeNo },
    {"EnvH",         2, volumeTypeNone,      ledTypeNo },
    {"Delay",        2, volumeTypeNone,      ledTypeNo },
    {"Constant",     2, volumeTypeNone,      ledTypeNo },
    {"LevMult",      2, volumeTypeNone,      ledTypeNo },
    {"FltVoice",     4, volumeTypeMono,      ledTypeNo },
    {"EnvAHD",       4, volumeTypeNone,      ledTypeNo },
    {"Pan",          2, volumeTypeNone,      ledTypeNo },
    {"MixStereo",    5, volumeTypeStereo,    ledTypeNo },
    {"FltMulti",     4, volumeTypeNone,      ledTypeNo },
    {"ConstSwT",     2, volumeTypeNone,      ledTypeNo },
    {"FltNord",      5, volumeTypeNone,      ledTypeNo },
    {"EnvMulti",     6, volumeTypeNone,      ledTypeNo },
    {"SandH",        2, volumeTypeNone,      ledTypeNo },
    {"FltStatic",    3, volumeTypeNone,      ledTypeNo },
    {"EnvD",         2, volumeTypeNone,      ledTypeNo },
    {"Resonator",    5, volumeTypeNone,      ledTypeNo },
    {"Automate",     2, volumeTypeNone,      ledTypeNo },
    {"DrumSynth",    8, volumeTypeNone,      ledTypeNo },
    {"CompLev",      2, volumeTypeNone,      ledTypeNo },
    {"Mux8-1X",      3, volumeTypeSequencer, ledTypeNo },
    {"Clip",         2, volumeTypeNone,      ledTypeNo },
    {"Overdrive",    2, volumeTypeNone,      ledTypeNo },
    {"Scratch",      3, volumeTypeNone,      ledTypeNo },
    {"Gate",         2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Mix2-1B",      2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"ClkGen",       4, volumeTypeNone,      ledTypeNo },
    {"ClkDiv",       2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"EnvFollow",    2, volumeTypeNone,      ledTypeNo },
    {"NoteScaler",   2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"WaveWrap",     2, volumeTypeNone,      ledTypeNo },
    {"NoteQuant",    2, volumeTypeNone,      ledTypeNo },
    {"SwOnOffT",     2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Sw1-8",        4, volumeTypeNone,      ledTypeNo },
    {"Sw4-1",        3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"LevAmp",       2, volumeTypeNone,      ledTypeNo },
    {"Rect",         2, volumeTypeNone,      ledTypeNo },
    {"ShpStatic",    2, volumeTypeNone,      ledTypeNo },
    {"EnvADR",       3, volumeTypeNone,      ledTypeNo },
    {"WindSw",       2, volumeTypeNone,      ledTypeNo },
    {"8Counter",     2, volumeTypeSequencer, ledTypeNo },
    {"FltLP",        2, volumeTypeNone,      ledTypeNo },
    {"Sw1-4",        3, volumeTypeNone,      ledTypeNo },
    {"Flanger",      3, volumeTypeNone,      ledTypeNo },
    {"Sw1-2",        2, volumeTypeNone,      ledTypeNo },
    {"FlipFlop",     2, volumeTypeNone,      ledTypeNo },
    {"FltClassic",   4, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"StChorus",     3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"OscD",         2, volumeTypeNone,      ledTypeNo },
    {"OscA",         3, volumeTypeNone,      ledTypeNo },
    {"FreqShift",    3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Sw2-1",        2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"FltPhase",     5, volumeTypeMono,      ledTypeNo },
    {"EqPeak",       4, volumeTypeMono,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"ValSw2-1",     2, volumeTypeNone,      ledTypeNo },
    {"OscNoise",     3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Vocoder",      8, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"LevAdd",       2, volumeTypeNone,      ledTypeNo },
    {"Fade1-2",      2, volumeTypeNone,      ledTypeNo },
    {"Fade2-1",      2, volumeTypeNone,      ledTypeNo },
    {"LevScaler",    3, volumeTypeNone,      ledTypeNo },
    {"Mix8-1A",      2, volumeTypeMono,      ledTypeNo },
    {"LevMod",       3, volumeTypeNone,      ledTypeNo },
    {"Digitizer",    3, volumeTypeNone,      ledTypeNo },
    {"EnvADDSR",     5, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"SeqNote",      9, volumeTypeSequencer, ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Mix4-1C",      4, volumeTypeMono,      ledTypeNo },
    {"Mux8-1",       2, volumeTypeSequencer, ledTypeNo },
    {"WahWah",       2, volumeTypeNone,      ledTypeNo },
    {"Name",         1, volumeTypeNone,      ledTypeNo },
    {"Fx-In",        2, volumeTypeStereo,    ledTypeNo },
    {"MinMax",       2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"BinCounter",   2, volumeTypeSequencer, ledTypeNo },
    {"ADConv",       2, volumeTypeSequencer, ledTypeNo },
    {"DAConv",       2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"FltHP",        2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"T&H",          2, volumeTypeNone,      ledTypeNo },
    {"Mix4-1S",      4, volumeTypeStereo,    ledTypeNo },
    {"CtrlSend",     2, volumeTypeNone,      ledTypeNo },
    {"PCSend",       2, volumeTypeNone,      ledTypeNo },
    {"NoteSend",     2, volumeTypeNone,      ledTypeNo },
    {"SeqEvent",     5, volumeTypeSequencer, ledTypeNo },
    {"SeqVal",       8, volumeTypeSequencer, ledTypeNo },
    {"SeqLev",       8, volumeTypeSequencer, ledTypeNo },
    {"CtrlRcv",      2, volumeTypeNone,      ledTypeNo },
    {"NoteRcv",      2, volumeTypeNone,      ledTypeNo },
    {"NoteZone",     3, volumeTypeNone,      ledTypeNo },
    {"Compress",     5, volumeTypeCompress,  ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"KeyQuant",     2, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"SeqCtr",       8, volumeTypeSequencer, ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"NoteDet",      2, volumeTypeNone,      ledTypeNo },
    {"LevConv",      2, volumeTypeNone,      ledTypeNo },
    {"Glide",        2, volumeTypeNone,      ledTypeNo },
    {"CompSig",      2, volumeTypeNone,      ledTypeNo },
    {"ZeroCnt",      2, volumeTypeNone,      ledTypeNo },
    {"MixFader",     9, volumeTypeMono,      ledTypeNo },
    {"FltComb",      4, volumeTypeMono,      ledTypeNo },
    {"OscShpA",      5, volumeTypeNone,      ledTypeNo },
    {"OscDual",      5, volumeTypeNone,      ledTypeNo },
    {"DXRouter",     6, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"PShift",       3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"ModAHD",       5, volumeTypeNone,      ledTypeNo },
    {"2-In",         2, volumeTypeStereo,    ledTypeNo },
    {"4-In",         2, volumeTypeQuad,      ledTypeNo },
    {"DlySingleA",   2, volumeTypeNone,      ledTypeNo },
    {"DlySingleB",   2, volumeTypeNone,      ledTypeNo },
    {"DelayDual",    3, volumeTypeNone,      ledTypeNo },
    {"DelayQuad",    5, volumeTypeNone,      ledTypeNo },
    {"DelayA",       3, volumeTypeNone,      ledTypeNo },
    {"DelayB",       4, volumeTypeNone,      ledTypeNo },
    {"DlyClock",     2, volumeTypeNone,      ledTypeNo },
    {"DlyShiftReg",  2, volumeTypeNone,      ledTypeNo },
    {"Operator",    12, volumeTypeNone,      ledTypeNo },
    {"DlyEight",     3, volumeTypeNone,      ledTypeNo },
    {"DlyStereo",    5, volumeTypeNone,      ledTypeNo },
    {"OscPM",        3, volumeTypeNone,      ledTypeNo },
    {"Mix1-1A",      2, volumeTypeNone,      ledTypeNo },
    {"Mix1-1S",      2, volumeTypeNone,      ledTypeNo },
    {"Sw1-2M",       2, volumeTypeNone,      ledTypeNo },
    {"Sw2-1M",       2, volumeTypeNone,      ledTypeNo },
    {"ConstSwM",     2, volumeTypeNone,      ledTypeNo },
    {"NoiseGate",    3, volumeTypeNone,      ledTypeNo },
    {"LfoB",         4, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"Phaser",       2, volumeTypeNone,      ledTypeNo },
    {"Mix4-1A",      2, volumeTypeNone,      ledTypeNo },
    {"Mix2-1A",      2, volumeTypeNone,      ledTypeNo },
    {"ModAmt",       2, volumeTypeNone,      ledTypeNo },
    {"OscPerc",      3, volumeTypeNone,      ledTypeNo },
    {"Status",       2, volumeTypeNone,      ledTypeNo },
    {"PitchTrack",   2, volumeTypeNone,      ledTypeNo },
    {"MonoKey",      2, volumeTypeNone,      ledTypeNo },
    {"RandomA",      2, volumeTypeNone,      ledTypeNo },
    {"Red2Blue",     2, volumeTypeNone,      ledTypeNo },
    {"RandomB",      3, volumeTypeNone,      ledTypeNo },
    {"Blue2Red",     2, volumeTypeNone,      ledTypeNo },
    {"RndClkA",      2, volumeTypeNone,      ledTypeNo },
    {"RndTrig",      2, volumeTypeNone,      ledTypeNo },
    {"RndClkB",      3, volumeTypeNone,      ledTypeNo },
    {"Unknown",      0, volumeTypeNone,      ledTypeNo },
    {"RndPattern",   3, volumeTypeNone,      ledTypeNo }, };

// moduleType, paramType1, paramType2, position rectangle, anchor, label, range, defaultValue, string map, colour map
const tParamLocation     paramLocationList[] = {
    //  0 Unknown
    //  1 Keyboard -- No params
    //  2 Unknown
    //  3 4-Out
    {moduleType4toOut,     paramType1StandardToggle, paramType2Menu,   {{ 25,  -3}, { 7,  7}}, anchorBottomLeft,  "Out to",         3,   0, outTo4OutStrMap,                       NULL          }, // 3 4-Out
    {moduleType4toOut,     paramType1Bypass,         paramType2Toggle, {{ 45,  -3}, { 5,  5}}, anchorBottomLeft,  "Bypass",         2,   1, NULL,                                  NULL          },
    {moduleType4toOut,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  "Pad",            2,   0, padStrMap,                             NULL          },
    // 4 2-Out
    {moduleType2toOut,     paramType1StandardToggle, paramType2Menu,   {{ 25,  -3}, { 7,  7}}, anchorBottomLeft,  "Out to",         6,   0, outToStrMap,                           NULL          }, // 4 2-Out
    {moduleType2toOut,     paramType1Bypass,         paramType2Toggle, {{ 45,  -3}, { 5,  5}}, anchorBottomLeft,  "Bypass",         2,   1, NULL,                                  NULL          },
    {moduleType2toOut,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  "Pad",            2,   0, padStrMap,                             NULL          },
    //  5 Invert -- No params
    //  6 Unknown
    //  7 OscB
    {moduleTypeOscB,       paramType1OscFreq,        paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 7 Tune
    {moduleTypeOscB,       paramType1Fine,           paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 7 Cent
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, { 7,  7}}, anchorTopLeft,     NULL,             2,   1, kbStrMap,                              offOnColourMap}, // 7 Kbt
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 7 Pitch M
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Menu,   {{ 30,  15}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 7 Pitch Type
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "FM",           128,   0, NULL,                                  NULL          }, // FM M
    {moduleTypeOscB,       paramType1Shape,          paramType2Dial,   {{-15,   5}, { 7, 14}}, anchorTopRight,    "Shape",        128,   0, NULL,                                  NULL          }, // 7 Shape 50% to 99%, not decimals
    {moduleTypeOscB,       paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "ShpM",         128,   0, NULL,                                  NULL          }, // 7 Shape M
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Menu,   {{-30,   5}, { 7,  7}}, anchorTopRight,    NULL,             5,   2, shapeTypeStrMap,                       NULL          }, // 7 Waveform
    {moduleTypeOscB,       paramType1Bypass,         paramType2Toggle, {{ -3,   0}, { 5,  5}}, anchorMiddleRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 7 Bypass
    {moduleTypeOscB,       paramType1StandardToggle, paramType2Menu,   {{ 60,  15}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, fmTypeStrMap,                          NULL          }, // 7 FM Type
    // 8 OscShpB
    {moduleTypeOscShpB,    paramType1OscFreq,        paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 8 Tune
    {moduleTypeOscShpB,    paramType1Fine,           paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 8 Cent
    {moduleTypeOscShpB,    paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, { 7,  7}}, anchorTopLeft,     NULL,             2,   1, kbStrMap,                              offOnColourMap}, // 8 Kbt
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 8 Pitch M
    {moduleTypeOscShpB,    paramType1StandardToggle, paramType2Menu,   {{ 30,  15}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 8 Pitch Type
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "FM",           128,   0, NULL,                                  NULL          }, // FM M
    {moduleTypeOscShpB,    paramType1Shape,          paramType2Dial,   {{-15,   5}, { 7, 14}}, anchorTopRight,    "Shape",        128,   0, NULL,                                  NULL          }, // 8 Shape 50% to 99%, not decimals
    {moduleTypeOscShpB,    paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "ShpCtrl",      128,   0, NULL,                                  NULL          }, // 8 Shape M
    {moduleTypeOscShpB,    paramType1Bypass,         paramType2Toggle, {{ -3,   0}, { 5,  5}}, anchorMiddleRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 8 Bypass
    {moduleTypeOscShpB,    paramType1StandardToggle, paramType2Menu,   {{ 60,  15}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, fmTypeStrMap,                          NULL          }, // 8 FM Type
    //  9 OscC
    {moduleTypeOscC,       paramType1OscFreq,        paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 9 Coarse
    {moduleTypeOscC,       paramType1Fine,           paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 9 Fine
    {moduleTypeOscC,       paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, { 7,  7}}, anchorTopLeft,     NULL,             2,   1, kbStrMap,                              offOnColourMap}, // 9 Kbt
    {moduleTypeOscC,       paramType1StandardToggle, paramType2Menu,   {{ 30,  15}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 9 Tune Mode
    {moduleTypeOscC,       paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "FM",           128,   0, NULL,                                  NULL          }, // FM
    {moduleTypeOscC,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 9 Bypass
    {moduleTypeOscC,       paramType1StandardToggle, paramType2Menu,   {{ 70,  15}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, fmTypeStrMap,                          NULL          }, // 9 FM Type
    {moduleTypeOscC,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // Pitch Mode
    // 10 Unknown
    // 11 Unknown
    // 12 Reverb
    {moduleTypeReverb,     paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Time",         128,  64, NULL,                                  NULL          }, // 12 Time. Needs to check range
    {moduleTypeReverb,     paramType1CommonDial,     paramType2Dial,   {{ 37,  -3}, { 7, 14}}, anchorBottomLeft,  "Brightness",   128,  64, NULL,                                  NULL          }, // 12 Brightness
    {moduleTypeReverb,     paramType1CommonDial,     paramType2Dial,   {{ 57,  -3}, { 7, 14}}, anchorBottomLeft,  "DryWet",       128,  64, NULL,                                  NULL          }, // 12 DryWet
    {moduleTypeReverb,     paramType1Bypass,         paramType2Toggle, {{-20,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 12 Bypass
    // 13 OscString
    {moduleTypeOscString,  paramType1OscFreq,        paramType2Dial,   {{ 30,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 13 OscString Tune
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 47,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 13 OscString Fine
    {moduleTypeOscString,  paramType1StandardToggle, paramType2Toggle, {{ 22,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   1, kbStrMap,                              offOnColourMap}, // 13 OscString Kbt
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 13 OscString Pitch modulation
    {moduleTypeOscString,  paramType1StandardToggle, paramType2Menu,   {{ 30,  10}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 13 OscString Pitch mode
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 59,  -3}, { 7, 14}}, anchorBottomLeft,  "Decay",        128,  64, NULL,                                  NULL          }, // 13 OscString Decay
    {moduleTypeOscString,  paramType1CommonDial,     paramType2Dial,   {{ 71,  -3}, { 7, 14}}, anchorBottomLeft,  "Damp",         128,  64, NULL,                                  NULL          }, // 13 OscString Damp
    {moduleTypeOscString,  paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 13 OscString Bypass
    // 14 Unknown
    // 15 Sw8-1 *** Needs more resources
    {moduleTypeSw8to1,     paramType1StandardToggle, paramType2Menu,   {{ 10, -17}, { 7,  7}}, anchorBottomLeft,  "Sel",            8,   0, sw8to1SelStrMap,                       NULL          }, // 15 Sel
    // 16 Unknown
    // 17 ValSw1-2
    {moduleTypeValSw1to2,  paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, u128StrMap,                            NULL          }, // 17 Val
    // 18 X-Fade
    {moduleTypeXtoFade,    paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 18 Mod
    {moduleTypeXtoFade,    paramType1Pan,            paramType2Dial,   {{ 55,   4}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 18 X-fade
    {moduleTypeXtoFade,    paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, logStrMap,                             NULL          },
    // 19 Mix4-1B
    {moduleTypeMix4to1B,   paramType1CommonDial,     paramType2Dial,   {{ 37,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 19 Lev1
    {moduleTypeMix4to1B,   paramType1CommonDial,     paramType2Dial,   {{ 52,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 19 Lev2
    {moduleTypeMix4to1B,   paramType1CommonDial,     paramType2Dial,   {{ 67,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 19 Lev3
    {moduleTypeMix4to1B,   paramType1CommonDial,     paramType2Dial,   {{ 82,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 19 Lev4
    {moduleTypeMix4to1B,   paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 19 ExpLinDB
    // 20 EnvvADSR
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Menu,   {{-26,  -3}, { 7,  7}}, anchorBottomRight, NULL,             4,   0, envShapeStrMap,                        NULL          }, // 20 Env Shape
    {moduleTypeEnvADSR,    paramType1ADRTime,        paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Attack",       128,   0, NULL,                                  NULL          }, // 20 A
    {moduleTypeEnvADSR,    paramType1ADRTime,        paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Delay",        128,   0, NULL,                                  NULL          }, // 20 D
    {moduleTypeEnvADSR,    paramType1CommonDial,     paramType2Dial,   {{ 44,  -3}, { 7, 14}}, anchorBottomLeft,  "Sus",          128, 100, NULL,                                  NULL          }, // 20 S
    {moduleTypeEnvADSR,    paramType1ADRTime,        paramType2Dial,   {{ 56,  -3}, { 7, 14}}, anchorBottomLeft,  "Rel",          128,   0, NULL,                                  NULL          }, // 20 R
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Menu,   {{-26, -10}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          }, // 20 Output Type
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, kbStrMap,                              offOnColourMap}, // 20 KB Active
    {moduleTypeEnvADSR,    paramType1StandardToggle, paramType2Menu,   {{  5,  -8}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, normalResetStrMap,                     NULL          }, // 20 Normal/Reset
    // 21 Mux1-8 -- No params
    // 22 PartQuant
    {moduleTypePartQuant,  paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Range",        128, 100, NULL,                                  NULL          }, // 22 Range
    // 23 ModADSR
    {moduleTypeModADSR,    paramType1ADRTime,        paramType2Dial,   {{ 22, -17}, { 7, 14}}, anchorBottomLeft,  "A",            128,   0, NULL,                                  NULL          }, // 22 A
    {moduleTypeModADSR,    paramType1ADRTime,        paramType2Dial,   {{ 37, -17}, { 7, 14}}, anchorBottomLeft,  "D",            128,   0, NULL,                                  NULL          }, // 22 D
    {moduleTypeModADSR,    paramType1CommonDial,     paramType2Dial,   {{ 52, -17}, { 7, 14}}, anchorBottomLeft,  "S",            128, 100, NULL,                                  NULL          }, // 22 S
    {moduleTypeModADSR,    paramType1ADRTime,        paramType2Dial,   {{ 67, -17}, { 7, 14}}, anchorBottomLeft,  "R",            128,   0, NULL,                                  NULL          }, // 22 R
    {moduleTypeModADSR,    paramType1CommonDial,     paramType2Dial,   {{ 27,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 22 Attack M
    {moduleTypeModADSR,    paramType1CommonDial,     paramType2Dial,   {{ 42,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 22 Decay M
    {moduleTypeModADSR,    paramType1CommonDial,     paramType2Dial,   {{ 57,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 22 Sustain M
    {moduleTypeModADSR,    paramType1CommonDial,     paramType2Dial,   {{ 72,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 22 Release M
    {moduleTypeModADSR,    paramType1StandardToggle, paramType2Menu,   {{-15, -17}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          }, // 22 Output Type
    {moduleTypeModADSR,    paramType1StandardToggle, paramType2Toggle, {{  3, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, kbStrMap,                              offOnColourMap}, // 22 KB
    // 24 LfoC
    {moduleTypeLfoC,       paramType1LFORate,        paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 24 Rate  *** Could have Freq but tied to Range
    {moduleTypeLfoC,       paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, polyMonoStrMap,                        NULL          }, // 24 Mode
    {moduleTypeLfoC,       paramType1StandardToggle, paramType2Menu,   {{ 76,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             6,   0, posStrMap,                             NULL          }, // 24 OutType
    {moduleTypeLfoC,       paramType1StandardToggle, paramType2Menu,   {{ 30,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, rangeLfoCStrMap,                       NULL          }, // 24 Range
    {moduleTypeLfoC,       paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 24 Bypass
    // 25 LfoShpA
    {moduleTypeLfoShpA,    paramType1LFORate,        paramType2Dial,   {{ 36,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 25 Rate
    {moduleTypeLfoShpA,    paramType1StandardToggle, paramType2Menu,   {{ 36, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             5,   0, rangeStrMap,                           NULL          }, // 25 Range
    {moduleTypeLfoShpA,    paramType1StandardToggle, paramType2Menu,   {{ 25, -28}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   0, offTo100KbStrMap,                      NULL          }, // 25 Kbt
    {moduleTypeLfoShpA,    paramType1CommonDial,     paramType2Dial,   {{ 25,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 25 Rate M
    {moduleTypeLfoShpA,    paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 25 Bypass
    {moduleTypeLfoShpA,    paramType1CommonDial,     paramType2Dial,   {{ 57, -17}, { 7, 14}}, anchorBottomLeft,  "Shape",        128,   0, NULL,                                  NULL          }, // 25 Shape
    {moduleTypeLfoShpA,    paramType1CommonDial,     paramType2Dial,   {{ 74,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 25 Phase M
    {moduleTypeLfoShpA,    paramType1CommonDial,     paramType2Dial,   {{ 74, -17}, { 7, 14}}, anchorBottomLeft,  "Phase",        128,  64, NULL,                                  NULL          }, // 25 Phase
    {moduleTypeLfoShpA,    paramType1CommonDial,     paramType2Dial,   {{ 57,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 25 Shape M
    {moduleTypeLfoShpA,    paramType1StandardToggle, paramType2Menu,   {{ 25, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, polyMonoStrMap,                        NULL          }, // 25 Mode
    {moduleTypeLfoShpA,    paramType1StandardToggle, paramType2Menu,   {{-10, -10}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          }, // 25 OutType
    {moduleTypeLfoShpA,    paramType1StandardToggle, paramType2Menu,   {{ 57, -32}, { 7,  7}}, anchorBottomLeft,  NULL,             6,   0, lfoShpAWaveStrMap,                     NULL          }, // 25 Wave
    // 26 LfoA
    {moduleTypeLfoA,       paramType1LFORate,        paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 26 Rate
    {moduleTypeLfoA,       paramType1StandardToggle, paramType2Menu,   {{ 17, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, polyMonoStrMap,                        NULL          }, // 26 Mode
    {moduleTypeLfoA,       paramType1StandardToggle, paramType2Menu,   {{ 65,  -3}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   0, offTo100KbStrMap,                      NULL          }, // 26 Kbt
    {moduleTypeLfoA,       paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 26 Rate M
    {moduleTypeLfoA,       paramType1StandardToggle, paramType2Menu,   {{ 65, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             6,   0, lfoWaveStrMap,                         NULL          }, // 26 Wave
    {moduleTypeLfoA,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 26 Bypass
    {moduleTypeLfoA,       paramType1StandardToggle, paramType2Menu,   {{-10, -10}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          }, // 26 OutType
    {moduleTypeLfoA,       paramType1StandardToggle, paramType2Menu,   {{ 25,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, rangeStrMap,                           NULL          }, // 26 Range
    // 27 OscMaster
    {moduleTypeOscMaster,  paramType1OscFreq,        paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,  64, NULL,                                  NULL          }, // 27 Pitch
    {moduleTypeOscMaster,  paramType1Fine,           paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 27 Fine
    {moduleTypeOscMaster,  paramType1StandardToggle, paramType2Toggle, {{ 22,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   1, kbStrMap,                              offOnColourMap}, // 27 Kbt
    {moduleTypeOscMaster,  paramType1StandardToggle, paramType2Menu,   {{ 30,  15}, { 7,  7}}, anchorTopLeft,     NULL,             3,   0, pitchTypeStrMap,                       NULL          }, // 27 Tune mode
    {moduleTypeOscMaster,  paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch M",      128,   0, NULL,                                  NULL          }, // 27 Pitch M
    // 28 Saturate
    {moduleTypeSaturate,   paramType1CommonDial,     paramType2Dial,   {{ 54,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 28 Amount
    {moduleTypeSaturate,   paramType1CommonDial,     paramType2Dial,   {{ 41,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 28 AmountMod
    {moduleTypeSaturate,   paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 28 Bypass
    {moduleTypeSaturate,   paramType1StandardToggle, paramType2Menu,   {{ 70,  -6}, { 7,  7}}, anchorBottomLeft,  "Curve",          4,   0, saturateCurveStrMap,                   NULL          }, // 28 Curve
    // 29 MetNoise
    {moduleTypeMetNoise,   paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "Color",        128,  64, NULL,                                  NULL          }, // 29 Color
    {moduleTypeMetNoise,   paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,  64, NULL,                                  NULL          }, // 29 Freq
    {moduleTypeMetNoise,   paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 28 Bypass
    {moduleTypeMetNoise,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 29 Freq M
    {moduleTypeMetNoise,   paramType1CommonDial,     paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 29 Color M
    // 30 Device - No controls
    // 31 Noise
    {moduleTypeNoise,      paramType1CommonDial,     paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  "Color",        128,   0, NULL,                                  NULL          }, // 31 Color  *** Or special dial
    {moduleTypeNoise,      paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 31 Bypass
    // 32 Eq2Band
    {moduleTypeEq2Band,    paramType1dB,             paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Lo (80Hz)",    128,  64, NULL,                                  NULL          }, // 32 Lo Gain
    {moduleTypeEq2Band,    paramType1dB,             paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Hi (12kHz)",   128,  64, NULL,                                  NULL          }, // 32 Hi Gain
    {moduleTypeEq2Band,    paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  "Level",        128, 127, NULL,                                  NULL          }, // 32 Level
    {moduleTypeEq2Band,    paramType1Bypass,         paramType2Toggle, {{ -3,  -9}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 32 Bypass
    {moduleTypeEq2Band,    paramType1StandardToggle, paramType2Menu,   {{ 30, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, eq2BandLoStrMap,                       NULL          }, // 32 Lo Freq
    {moduleTypeEq2Band,    paramType1StandardToggle, paramType2Menu,   {{ 50, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   2, eq2BandHiStrMap,                       NULL          }, // 32 Hi Freq
    // 33 Eq3Band
    {moduleTypeEq3band,    paramType1dB,             paramType2Dial,   {{  5,  -3}, { 7, 14}}, anchorBottomLeft,  "Lo",           128,  64, NULL,                                  NULL          }, // 33 Lo Gain
    {moduleTypeEq3band,    paramType1dB,             paramType2Dial,   {{ 24,  -3}, { 7, 14}}, anchorBottomLeft,  "MidGn",        128,  64, NULL,                                  NULL          }, // 33 Mid Gain
    {moduleTypeEq3band,    paramType1GeneralFreq,    paramType2Dial,   {{ 36,  -3}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,  64, NULL,                                  NULL          }, // 33 Mid Freq
    {moduleTypeEq3band,    paramType1dB,             paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  "Hi",           128,  64, NULL,                                  NULL          }, // 33 Hi Gain
    {moduleTypeEq3band,    paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "Level",        128, 127, NULL,                                  NULL          }, // 33 Level
    {moduleTypeEq3band,    paramType1Bypass,         paramType2Toggle, {{ -3,  -9}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 33 Bypass
    {moduleTypeEq3band,    paramType1StandardToggle, paramType2Menu,   {{  3, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, eq2BandLoStrMap,                       NULL          }, // 33 Lo Freq
    {moduleTypeEq3band,    paramType1StandardToggle, paramType2Menu,   {{ 53, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   2, eq2BandHiStrMap,                       NULL          }, // 33 Hi Freq
    // 34 ShpExp
    {moduleTypeShpExp,     paramType1CommonDial,     paramType2Dial,   {{ 54,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 34 Amount
    {moduleTypeShpExp,     paramType1CommonDial,     paramType2Dial,   {{ 41,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 34 AmountMod
    {moduleTypeShpExp,     paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 34 Bypass
    {moduleTypeShpExp,     paramType1StandardToggle, paramType2Menu,   {{ 70,  -6}, { 7,  7}}, anchorBottomLeft,  "Curve",          4,   0, shpExpCurveStrMap,                     NULL          }, // 34 Curve
    // 35 Driver (Not in the Clavia editor)
    {moduleTypeDriver,     paramType1CommonDial,     paramType2Dial,   {{ 54,  -3}, { 7, 14}}, anchorBottomLeft,  "Stiffness",    128,   0, NULL,                                  NULL          }, // 35 Embouchure
    {moduleTypeDriver,     paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  "Embouchure",   128,   0, NULL,                                  NULL          }, // 35 Stiffness
    // 36 SwOnOffM
    {moduleTypeSwOnOffM,   paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, offOnStrMap,                           NULL          }, // 36 On
    // 37 Unknown
    // 38 Pulse
    {moduleTypePulse,      paramType1PulseTime,      paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, NULL,                                  NULL          }, // 38 Time
    {moduleTypePulse,      paramType1CommonDial,     paramType2Dial,   {{ 42,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 38 TimeMod
    {moduleTypePulse,      paramType1StandardToggle, paramType2Menu,   {{ 52,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, pulseRangeStrMap,                      NULL          }, // 38 Range
    // 39 Unknown
    // 40 Mix8-1B
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In1
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 29,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In2
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 38,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In3
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 47,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In4
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 56,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In5
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 65,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In6
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 74,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In7
    {moduleTypeMix8to1B,   paramType1MixLevel,       paramType2Dial,   {{ 83,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 40 In8
    {moduleTypeMix8to1B,   paramType1StandardToggle, paramType2Menu,   {{  3, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 40 ExpLinDB
    {moduleTypeMix8to1B,   paramType1StandardToggle, paramType2Menu,   {{  3, -10}, { 7,  7}}, anchorBottomLeft,  "Pad",            3,   0, db12BPadStrMap,                        NULL          }, // 40 Pad
    // 41 EnvH
    {moduleTypeEnvH,       paramType1ADRTime,        paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 41 Hold
    {moduleTypeEnvH,       paramType1StandardToggle, paramType2Menu,   {{-32,  -3}, { 7,  7}}, anchorBottomRight, NULL,             4,   0, posNegInvStrMap,                       NULL          }, // 41 Pos
    // 42 Delay (Logic Delay)
    {moduleTypeDelay,      paramType1PulseTime,      paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, NULL,                                  NULL          }, // 42 Time
    {moduleTypeDelay,      paramType1CommonDial,     paramType2Dial,   {{ 42,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 42 TimeMod
    {moduleTypeDelay,      paramType1StandardToggle, paramType2Menu,   {{ 52,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, pulseRangeStrMap,                      NULL          }, // 42 Range

    // 43 Constant
    {moduleTypeConstant,   paramType1BipLevel,       paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 43 Value
    {moduleTypeConstant,   paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, bipUniStrMap,                          NULL          }, // 43 Bip/Uni
    // 44 LevMult  - No Controls
    // 45 FltVoice
    {moduleTypeFltVoice,   paramType1StandardToggle, paramType2Menu,   {{ 40, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             9,   0, vowelStrMap,                           NULL          }, // 45 Vowel1
    {moduleTypeFltVoice,   paramType1StandardToggle, paramType2Menu,   {{ 50, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             9,   1, vowelStrMap,                           NULL          }, // 45 Vowel2
    {moduleTypeFltVoice,   paramType1StandardToggle, paramType2Menu,   {{ 60, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             9,   2, vowelStrMap,                           NULL          }, // 45 Vowel3
    {moduleTypeFltVoice,   paramType1CommonDial,     paramType2Dial,   {{-15, -17}, { 7, 14}}, anchorBottomRight, NULL,           128,  80, NULL,                                  NULL          }, // 45 Level
    {moduleTypeFltVoice,   paramType1BipLevel,       paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 45 Vowel
    {moduleTypeFltVoice,   paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 45 VowelMod
    {moduleTypeFltVoice,   paramType1BipLevel,       paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,  80, NULL,                                  NULL          }, // 45 Freq
    {moduleTypeFltVoice,   paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 45 FreqMod
    {moduleTypeFltVoice,   paramType1BipLevel,       paramType2Dial,   {{ 30,  -3}, { 7, 14}}, anchorBottomLeft,  "Res",          128,   0, NULL,                                  NULL          }, // 45 Res
    {moduleTypeFltVoice,   paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 45 Bypass
    // 46 EnvAHD
    {moduleTypeEnvAHD,     paramType1StandardToggle, paramType2Menu,   {{-26,  -3}, { 7,  7}}, anchorBottomRight, NULL,             4,   0, envShapeStrMap,                        NULL          }, // 46 Shape
    {moduleTypeEnvAHD,     paramType1ADRTime,        paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Attack",       128,   0, NULL,                                  NULL          }, // 46 Attack
    {moduleTypeEnvAHD,     paramType1ADRTime,        paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Hold",         128,   0, NULL,                                  NULL          }, // 46 Hold
    {moduleTypeEnvAHD,     paramType1StandardToggle, paramType2Menu,   {{  5, -15}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, normalResetStrMap,                     NULL          }, // 46 Reset
    {moduleTypeEnvAHD,     paramType1ADRTime,        paramType2Dial,   {{ 44,  -3}, { 7, 14}}, anchorBottomLeft,  "Release",      128,   0, NULL,                                  NULL          }, // 46 Release
    {moduleTypeEnvAHD,     paramType1StandardToggle, paramType2Menu,   {{-26, -10}, { 7,  7}}, anchorBottomRight, NULL,             4,   0, posNegInvStrMap,                       NULL          }, // 46 OutType
    {moduleTypeEnvAHD,     paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, kbStrMap,                              offOnColourMap},
    // 47 Pan
    {moduleTypePan,        paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 47 PanMod
    {moduleTypePan,        paramType1LRDial,         paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 47 Pan
    {moduleTypePan,        paramType1StandardToggle, paramType2Menu,   {{ 10,  -5}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, logStrMap,                             NULL          }, // 47 LogLin
    // 48 MixStereo
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 48 Lvl1
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 22,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 48 Lvl2
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 34,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 48 Lvl3
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 48,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 48 Lvl4
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 48 Lvl5
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ 72,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  80, NULL,                                  NULL          }, // 48 Lvl6
    {moduleTypeMixStereo,  paramType1LRDial,         paramType2Dial,   {{ 10, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 48 Pan1
    {moduleTypeMixStereo,  paramType1LRDial,         paramType2Dial,   {{ 22, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 48 Pan2
    {moduleTypeMixStereo,  paramType1LRDial,         paramType2Dial,   {{ 34, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 48 Pan3
    {moduleTypeMixStereo,  paramType1LRDial,         paramType2Dial,   {{ 46, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 48 Pan4
    {moduleTypeMixStereo,  paramType1LRDial,         paramType2Dial,   {{ 58, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 48 Pan5
    {moduleTypeMixStereo,  paramType1LRDial,         paramType2Dial,   {{ 70, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 48 Pan6
    {moduleTypeMixStereo,  paramType1CommonDial,     paramType2Dial,   {{ -7, -25}, { 7, 14}}, anchorBottomRight, NULL,           128,  80, NULL,                                  NULL          }, // 48 LvlMaster
    // 49 FltMulti
    {moduleTypeFltMulti,   paramType1Freq,           paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,   0, NULL,                                  NULL          }, // 49 Freq
    {moduleTypeFltMulti,   paramType1Pitch,          paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 49 Pitch M
    {moduleTypeFltMulti,   paramType1StandardToggle, paramType2Menu,   {{ 25, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   4, offTo100KbStrMap,                      NULL          }, // 49 Kbt
    {moduleTypeFltMulti,   paramType1StandardToggle, paramType2Toggle, {{ 55,   5}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, gcStrMap,                              offOnColourMap}, // 49 GC
    {moduleTypeFltMulti,   paramType1StrMap,         paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  "Res",          128,   0, filter_resonanceStrMap,                NULL          }, // 49 Res
    {moduleTypeFltMulti,   paramType1StandardToggle, paramType2Menu,   {{ 70, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, fltMultiDbStrMap,                      NULL          }, // 49 dB
    {moduleTypeFltMulti,   paramType1Bypass,         paramType2Toggle, {{-17,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 49 Bypass
    // 50 ConstSwT
    {moduleTypeConstSwT,   paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 50 Value
    {moduleTypeConstSwT,   paramType1StandardToggle, paramType2Toggle, {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, offOnStrMap,                           offOnColourMap}, // 50 Switch
    {moduleTypeConstSwT,   paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, bipUniStrMap,                          NULL          }, // 50 Bip/Uni
    // 51 FltNord
    {moduleTypeFltNord,    paramType1Freq,           paramType2Dial,   {{ 30, -25}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,   0, NULL,                                  NULL          }, // 51 Freq
    {moduleTypeFltNord,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 51 Pitch M
    {moduleTypeFltNord,    paramType1StandardToggle, paramType2Menu,   {{ 15, -25}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   4, offTo100KbStrMap,                      NULL          }, // 51 Kbt
    {moduleTypeFltNord,    paramType1StandardToggle, paramType2Toggle, {{ 40, -15}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, gcStrMap,                              offOnColourMap}, // 51 GC
    {moduleTypeFltNord,    paramType1StrMap,         paramType2Dial,   {{ 55, -25}, { 7, 14}}, anchorBottomLeft,  "Res",          128,   0, filter_resonanceStrMap,                NULL          }, // 51 Res
    {moduleTypeFltNord,    paramType1StandardToggle, paramType2Menu,   {{ 70, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, fltNordDbStrMap,                       NULL          }, // 51 dB/Oct
    {moduleTypeFltNord,    paramType1Bypass,         paramType2Toggle, {{-17,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 51 Bypass
    {moduleTypeFltNord,    paramType1CommonDial,     paramType2Dial,   {{ 31,  -3}, { 7, 14}}, anchorBottomLeft,  "Fm Lin",       128,   0, NULL,                                  NULL          }, // 51 Fm Lin
    {moduleTypeFltNord,    paramType1StandardToggle, paramType2Menu,   {{ 70, -20}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, nordFilterTypeStrMap,                  NULL          }, // 51 FilterType
    {moduleTypeFltNord,    paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  "Res M",        128,   0, NULL,                                  NULL          }, // 51 Res M
    // 52 EnvMulti
    {moduleTypeEnvMulti,   paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "L1",           128, 127, NULL,                                  NULL          }, // 52 Lvl1
    {moduleTypeEnvMulti,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "L1",           128, 127, NULL,                                  NULL          }, // 52 Lvl2
    {moduleTypeEnvMulti,   paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "L1",           128, 127, NULL,                                  NULL          }, // 52 Lvl3
    {moduleTypeEnvMulti,   paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "L1",           128, 127, NULL,                                  NULL          }, // 52 Lvl4
    {moduleTypeEnvMulti,   paramType1ADRTime,        paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  "T1",           128,   0, NULL,                                  NULL          }, // 52 Time1
    {moduleTypeEnvMulti,   paramType1ADRTime,        paramType2Dial,   {{ 30,  -3}, { 7, 14}}, anchorBottomLeft,  "T2",           128,   0, NULL,                                  NULL          }, // 52 Time2
    {moduleTypeEnvMulti,   paramType1ADRTime,        paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "T3",           128,   0, NULL,                                  NULL          }, // 52 Time3
    {moduleTypeEnvMulti,   paramType1ADRTime,        paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  "T4",           128,   0, NULL,                                  NULL          }, // 52 Time4
    {moduleTypeEnvMulti,   paramType1StandardToggle, paramType2Menu,   {{  3, -22}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, normalResetStrMap,                     NULL          }, // 52 Reset
    {moduleTypeEnvMulti,   paramType1StandardToggle, paramType2Menu,   {{ 30, -22}, { 7,  7}}, anchorBottomLeft,  "Sust",           4,   2, multiEnvSustainStrMap,                 NULL          }, // 52 SustainPlacement
    {moduleTypeEnvMulti,   paramType1StandardToggle, paramType2Menu,   {{-17, -29}, { 7,  7}}, anchorBottomRight, NULL,             5,   0, posNegInvBipStrMap,                    NULL          }, // 52 OutType
    {moduleTypeEnvMulti,   paramType1StandardToggle, paramType2Toggle, {{ 10,  17}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, kbStrMap,                              offOnColourMap}, // 52 Kbt
    {moduleTypeEnvMulti,   paramType1StandardToggle, paramType2Menu,   {{-26, -17}, { 7,  7}}, anchorBottomRight, NULL,             4,   0, envShapeStrMap,                        NULL          }, // 52 Shape
    // 53 SandH -- No parameters
    // 54 FltStatic
    {moduleTypeFltStatic,  paramType1Freq,           paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,   0, NULL,                                  NULL          }, // 54 Freq
    {moduleTypeFltStatic,  paramType1Resonance,      paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Res",          128,   0, NULL,                                  NULL          }, // 54 Res
    {moduleTypeFltStatic,  paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, staticFilterTypeStrMap,                NULL          }, // 54 FilterType
    {moduleTypeFltStatic,  paramType1Bypass,         paramType2Toggle, {{ -3,   2}, { 5,  5}}, anchorMiddleRight, NULL,             2,   1, NULL,                                  NULL          }, // 54 Bypass
    {moduleTypeFltStatic,  paramType1StandardToggle, paramType2Toggle, {{ 80,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, gcStrMap,                              offOnColourMap}, // 54 GC
    // 55 EnvD
    {moduleTypeEnvD,       paramType1ADRTime,        paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Decay",        128,   0, NULL,                                  NULL          }, // 55 Decay
    {moduleTypeEnvD,       paramType1StandardToggle, paramType2Menu,   {{ 55,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, posNegInvStrMap,                       NULL          }, // 55 OutType
    // 56 Resonator (Not in Clavia editor)
    {moduleTypeResonator,  paramType1OscFreq,        paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 56 Tune
    {moduleTypeResonator,  paramType1Fine,           paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 56 Cent
    {moduleTypeResonator,  paramType1StandardToggle, paramType2Toggle, {{ 15, -20}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   1, kbStrMap,                              offOnColourMap}, // 56 Kbt
    {moduleTypeResonator,  paramType1CommonDial,     paramType2Dial,   {{ 17,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 56 Pitch M
    {moduleTypeResonator,  paramType1StandardToggle, paramType2Menu,   {{ 30,  12}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 56 Pitch Type
    {moduleTypeResonator,  paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "Decay",        128,   0, NULL,                                  NULL          }, // 56 Decay
    {moduleTypeResonator,  paramType1CommonDial,     paramType2Dial,   {{ 62, -17}, { 7, 14}}, anchorBottomLeft,  "Damp",         128,   0, NULL,                                  NULL          }, // 56 Damp
    {moduleTypeResonator,  paramType1Bypass,         paramType2Toggle, {{ -3,  -3}, { 5,  5}}, anchorMiddleRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 56 Bypass
    {moduleTypeResonator,  paramType1CommonDial,     paramType2Dial,   {{ 75, -17}, { 7, 14}}, anchorBottomLeft,  "Pos",          128,   0, NULL,                                  NULL          }, // 56 Pos
    {moduleTypeResonator,  paramType1StandardToggle, paramType2Menu,   {{ 30,  20}, { 7,  7}}, anchorTopLeft,     "Alg",            5,   0, resonAlgStrMap,                        NULL          }, // 56 Algorithm
    // 57 Automate *** We lack an UI element for the CTRL selector
    {moduleTypeAutomate,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Ctrl",         128,   7, NULL,                                  NULL          }, // 57 Ctrl CC
    {moduleTypeAutomate,   paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  "Val",          128,  64, NULL,                                  NULL          }, // 57 Val
    {moduleTypeAutomate,   paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  "Ch",            17,   0, NULL,                                  NULL          }, // 57 Channel
    {moduleTypeAutomate,   paramType1StandardToggle, paramType2Menu,   {{ 33,  -3}, { 7,  7}}, anchorBottomLeft,  "Echo",           2,   0, offOnStrMap,                           NULL          }, // 57 Echo
    // 58 Drumsynth
    {moduleTypeDrumSynth,  paramType1FreqDrum,       paramType2Dial,   {{ 20, -22}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 58 Master Freq
    {moduleTypeDrumSynth,  paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 58 Slave Ratio
    {moduleTypeDrumSynth,  paramType1ADRTime,        paramType2Dial,   {{ 30, -22}, { 7, 14}}, anchorBottomLeft,  "Dcy",          128,   0, NULL,                                  NULL          }, // 58 MasterDecay
    {moduleTypeDrumSynth,  paramType1ADRTime,        paramType2Dial,   {{ 30,  -3}, { 7, 14}}, anchorBottomLeft,  "Dcy",          128,   0, NULL,                                  NULL          }, // 58 Slave Decay
    {moduleTypeDrumSynth,  paramType1CommonDial,     paramType2Dial,   {{ 40, -22}, { 7, 14}}, anchorBottomLeft,  "Lvl",          128, 100, NULL,                                  NULL          }, // 58 Master Level
    {moduleTypeDrumSynth,  paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Lvl",          128, 100, NULL,                                  NULL          }, // 58 Slave Level
    {moduleTypeDrumSynth,  paramType1Freq,           paramType2Dial,   {{ 50, -22}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,   0, NULL,                                  NULL          }, // 58 Noise Filter Freq
    {moduleTypeDrumSynth,  paramType1Resonance,      paramType2Dial,   {{ 60, -22}, { 7, 14}}, anchorBottomLeft,  "Res",          128,   0, NULL,                                  NULL          }, // 54 Noise Filter Res
    {moduleTypeDrumSynth,  paramType1CommonDial,     paramType2Dial,   {{ 70, -22}, { 7, 14}}, anchorBottomLeft,  "Swp",          128,   0, NULL,                                  NULL          }, // 58 Noise Filter Sweep
    {moduleTypeDrumSynth,  paramType1ADRTime,        paramType2Dial,   {{ 80, -22}, { 7, 14}}, anchorBottomLeft,  "Dcy",          128,   0, NULL,                                  NULL          }, // 58 Noise Filter Decay
    {moduleTypeDrumSynth,  paramType1StandardToggle, paramType2Menu,   {{ 90, -22}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, staticFilterTypeStrMap,                NULL          }, // 58 Noise FilterType
    {moduleTypeDrumSynth,  paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Bend",         128,   0, NULL,                                  NULL          }, // 58 Bend Amount
    {moduleTypeDrumSynth,  paramType1ADRTime,        paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Dcy",          128,   0, NULL,                                  NULL          }, // 58 BendDecay
    {moduleTypeDrumSynth,  paramType1CommonDial,     paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  "Click",        128,   0, NULL,                                  NULL          }, // 58 Click Amount
    {moduleTypeDrumSynth,  paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "Noise",        128,   0, NULL,                                  NULL          }, // 58 Noise Amount
    {moduleTypeDrumSynth,  paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 58 Bypass
    //{moduleTypeDrumSynth,   paramType1StandardToggle, paramType2Menu, {{ 60, -35}, {7,  7}}, anchorBottomLeft,  NULL,        1,  0, drSynthPresetStrMap,   NULL          }, // 58 Preset *** Length must be updated  // TODO: Only 16 parameters on this one, decide which one is wrong

    // 59 CompLev
    {moduleTypeCompLev,    paramType1BipLevel,       paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  "C",            128,  64, NULL,                                  NULL          }, // 59 Compare Level
    // 60 Mux8-1X
    {moduleTypeMux8to1X,   paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "X-fade",       128,   0, NULL,                                  NULL          }, // 58 X-fade
    // 61 Clip
    {moduleTypeClip,       paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 61 Clip level M
    {moduleTypeClip,       paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 61 Clip level
    {moduleTypeClip,       paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, asymSymStrMap,                         NULL          }, // 61 Shape
    {moduleTypeClip,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 61 Bypass
    // 62 OverDrive
    {moduleTypeOverdrive,  paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 62 Amount M
    {moduleTypeOverdrive,  paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 62 Amount
    {moduleTypeOverdrive,  paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 62 Bypass
    {moduleTypeOverdrive,  paramType1StandardToggle, paramType2Menu,   {{ 15,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, odTypeStrMap,                          NULL          }, // 62 Type
    {moduleTypeOverdrive,  paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, asymSymStrMap,                         NULL          }, // 62 Shape
    // 63 Scratch
    {moduleTypeScratch,    paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 63 Ratio
    {moduleTypeScratch,    paramType1CommonDial,     paramType2Dial,   {{ 15,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 63 Ratio Mod
    {moduleTypeScratch,    paramType1StandardToggle, paramType2Menu,   {{ 75,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, delayStrMap,                           NULL          }, // 63 Delay
    {moduleTypeScratch,    paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 63 Bypass
    // 64 Gate (Logic Gate) -- no parameters, 2 modes
    // 65 Unknown
    // 66 Mix2-1B
    {moduleTypeMix2to1B,   paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, invStrMap,                             NULL          }, // 66 Inv1
    {moduleTypeMix2to1B,   paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 66 Lvl1
    {moduleTypeMix2to1B,   paramType1StandardToggle, paramType2Menu,   {{ 65,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, invStrMap,                             NULL          }, // 66 Inv2
    {moduleTypeMix2to1B,   paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 66 Lvl2
    {moduleTypeMix2to1B,   paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 66 ExpLinDB
    // 67 Unknown
    // 68 ClkGen
    {moduleTypeClkGen,     paramType1CommonDial,     paramType2Dial,   {{ 60, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 68 Tempo *** needs new dial 24-214 BPM
    {moduleTypeClkGen,     paramType1Bypass,         paramType2Toggle, {{ 70, -17}, { 5,  5}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  NULL          }, // 68 Bypass
    {moduleTypeClkGen,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  "Source",         2,   0, clkSrcStrMap,                          NULL          }, // 66 Clk Source
    {moduleTypeClkGen,     paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  "Sync every",    16,   3, int16StrMap,                           NULL          }, // 66 BeatSync
    {moduleTypeClkGen,     paramType1CommonDial,     paramType2Dial,   {{ 25,  -3}, { 7, 14}}, anchorBottomLeft,  "Swing",        128,   0, NULL,                                  NULL          }, // 68 Swing
    // 69 ClkDiv
    {moduleTypeClkDiv,     paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 69 Divider *** needs other UI element

    // 70 Unknown
    // 71 EnvFollow
    {moduleTypeEnvFollow,  paramType1StrMap,         paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Attack",       128,   0, env_follow_attackStrMap,               NULL          }, // 71 Attack
    {moduleTypeEnvFollow,  paramType1StrMap,         paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  "Release",      128,   0, env_follow_releaseStrMap,              NULL          }, // 71 Release
    // 72 NoteScaler
    {moduleTypeNoteScaler, paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Range",        128,   0, NULL,                                  NULL          }, // 72 Range *** needs UI element
    // 73 Unknown
    // 74 WaveWrap
    {moduleTypeWaveWrap,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 74 Amount Mod
    {moduleTypeWaveWrap,   paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 74 Amount
    {moduleTypeWaveWrap,   paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 74 Bypass
    // 75 NoteQuant
    {moduleTypeNoteQuant,  paramType1CommonDial,     paramType2Dial,   {{ 25,  -3}, { 7, 14}}, anchorBottomLeft,  "Range",        128, 127, NULL,                                  NULL          }, // 75 Range
    {moduleTypeNoteQuant,  paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Notes",        128,   0, NULL,                                  NULL          }, // 75 Notes
    // 76 SwOnOffT
    {moduleTypeSwOnOffT,   paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, offOnStrMap,                           NULL          }, // 76 On
    // 77 Unknown
    // 78 Sw1-8
    {moduleTypeSw1to8,     paramType1StandardToggle, paramType2Menu,   {{ 40, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             8,   0, out8StrMap,                            NULL          }, // 78 Selector
    // 79 Sw4-1
    {moduleTypeSw4to1,     paramType1StandardToggle, paramType2Menu,   {{ 40, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, in8StrMap,                             NULL          }, // 79 Selector
    // 80 Unknown
    // 81 LevAmp
    {moduleTypeLevAmp,     paramType1LevAmpDial,     paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 81 Gain
    {moduleTypeLevAmp,     paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   1, expStrMap,                             NULL          }, // 81 Type
    // 82 Rect
    {moduleTypeRect,       paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, rectStrMap,                            NULL          }, // 82 Mode
    {moduleTypeRect,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 82 Bypass
    // 83 ShpStatic
    {moduleTypeShpStatic,  paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, shpStaticStrMap,                       NULL          }, // 83 Mode
    {moduleTypeShpStatic,  paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 83 Bypass
    // 84 EnvADR
    {moduleTypeEnvADR,     paramType1StandardToggle, paramType2Menu,   {{ 70, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, envShapeStrMap,                        NULL          }, // 84 Shape
    {moduleTypeEnvADR,     paramType1ADRTime,        paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  "Attack",       128,   0, NULL,                                  NULL          }, // 84 Attack
    {moduleTypeEnvADR,     paramType1StandardToggle, paramType2Menu,   {{ 28, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, normalResetStrMap,                     NULL          }, // 84 Reset
    {moduleTypeEnvADR,     paramType1ADRTime,        paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 84 Release/Decay time
    {moduleTypeEnvADR,     paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, trigGateStrMap,                        NULL          }, // 84 Trig/Gate
    {moduleTypeEnvADR,     paramType1StandardToggle, paramType2Menu,   {{ 70, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, posNegInvStrMap,                       NULL          }, // 84 OutType
    {moduleTypeEnvADR,     paramType1StandardToggle, paramType2Toggle, {{  3, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, kbStrMap,                              offOnColourMap},
    {moduleTypeEnvADR,     paramType1StandardToggle, paramType2Menu,   {{ 52, -12}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, decayReleaseStrMap,                    NULL          }, // 84 Decay/Release toggle
    // 85 WindSw
    {moduleTypeWindSw,     paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "From",         128,   0, NULL,                                  NULL          }, // 85 From
    {moduleTypeWindSw,     paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "To",           128,   0, NULL,                                  NULL          }, // 85 To
    // 86 8Counter -- No controls
    // 87 FltLP
    {moduleTypeFltLP,      paramType1Freq,           paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 87 Freq
    {moduleTypeFltLP,      paramType1CommonDial,     paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 87 FreqMod
    {moduleTypeFltLP,      paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   4, offTo100KbStrMap,                      NULL          },
    //{moduleTypeFltLP, paramType1StandardToggle, paramType2Menu, {{ 67, -3}, {7,  7}}, anchorBottomLeft,  "Slope",       6,  2, fltLPSlopeStrMap,   NULL          }, // TODO: Possibly one of these should be a mode - only expecting 5 params
    {moduleTypeFltLP,      paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 83 Bypass
    // 88 Sw1-4
    {moduleTypeSw1to4,     paramType1StandardToggle, paramType2Menu,   {{ 37,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, out8StrMap,                            NULL          }, // 88 Selector
    // 89 Flanger
    {moduleTypeFlanger,    paramType1StrMap,         paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  "Rate",         128,  64, flanger_rateStrMap,                    NULL          }, // 89 Rate
    {moduleTypeFlanger,    paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  "Range",        128,  64, NULL,                                  NULL          }, // 89 Range
    {moduleTypeFlanger,    paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  "Feedback",     128,  64, NULL,                                  NULL          }, // 89 Feedback
    {moduleTypeFlanger,    paramType1Bypass,         paramType2Toggle, {{ -3,  -8}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 89 Bypass
    // 90 Sw1-2
    {moduleTypeSw1to2,     paramType1StandardToggle, paramType2Menu,   {{ 37,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, out8StrMap,                            NULL          }, // 90 Selector
    // 91 FlipFlop
    // 92 FltClassic
    {moduleTypeFltClassic, paramType1Freq,           paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,   0, NULL,                                  NULL          }, // 92
    {moduleTypeFltClassic, paramType1Pitch,          paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  "Env",          128,   0, NULL,                                  NULL          },
    {moduleTypeFltClassic, paramType1StandardToggle, paramType2Menu,   {{ 25, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   4, offTo100KbStrMap,                      NULL          },
    {moduleTypeFltClassic, paramType1StrMap,         paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  "Res",          128,   0, filter_resonanceStrMap,                NULL          },
    {moduleTypeFltClassic, paramType1StandardToggle, paramType2Menu,   {{ 70, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, fltClassicDbStrMap,                    NULL          },
    {moduleTypeFltClassic, paramType1Bypass,         paramType2Toggle, {{ -3,   0}, { 5,  5}}, anchorMiddleRight, NULL,             2,   1, NULL,                                  NULL          },
    // 93 Unknown
    // 94 StChorus
    {moduleTypeStChorus,   paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Detune",       128,   0, NULL,                                  NULL          }, // 94
    {moduleTypeStChorus,   paramType1CommonDial,     paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  "Amount",       128,   0, NULL,                                  NULL          },
    {moduleTypeStChorus,   paramType1Bypass,         paramType2Toggle, {{ -3,   0}, { 5,  5}}, anchorMiddleRight, NULL,             2,   1, NULL,                                  NULL          },
    // 95 Unknown
    // 96 OscD
    {moduleTypeOscD,       paramType1Freq,           paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 96 Tune
    {moduleTypeOscD,       paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 96 Fine
    {moduleTypeOscD,       paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 96 Kbt
    {moduleTypeOscD,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 96 Pitch M
    {moduleTypeOscD,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 96 Bypass

    // 97 OscA
    {moduleTypeOscA,       paramType1Freq,           paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 97 Tune
    {moduleTypeOscA,       paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 97 Fine
    {moduleTypeOscA,       paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, { 7,  7}}, anchorTopLeft,     NULL,             2,   1, kbStrMap,                              offOnColourMap}, // 97 Kbt
    {moduleTypeOscA,       paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 97 Pitch M
    {moduleTypeOscA,       paramType1StandardToggle, paramType2Menu,   {{-20,   5}, { 7,  7}}, anchorTopRight,    NULL,             6,   2, shapeOscATypeStrMap,                   NULL          }, // 97 Waveform
    {moduleTypeOscA,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 97 Bypass
    {moduleTypeOscA,       paramType1StandardToggle, paramType2Menu,   {{ 30,  10}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 97 Pitch Type

    // 98 FreqShift
    {moduleTypeFreqShift,  paramType1FreqShift,      paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "FreqShift",    128,  64, NULL,                                  NULL          }, // 98 FreqShift
    {moduleTypeFreqShift,  paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 98 Mod
    {moduleTypeFreqShift,  paramType1StandardToggle, paramType2Menu,   {{ 60, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, freqShiftRangeStrMap,                  NULL          }, // 98 Range
    {moduleTypeFreqShift,  paramType1Bypass,         paramType2Toggle, {{-10, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 98 Bypass
    // 99 Unknown
    // 100 Sw2-1
    {moduleTypeSw2to1,     paramType1StandardToggle, paramType2Menu,   {{ 50,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, in8StrMap,                             NULL          }, // 100 Selector
    // 101 Unknown
    // 102 FltPhase
    {moduleTypeFltPhase,   paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 102 Pitch M
    {moduleTypeFltPhase,   paramType1Freq,           paramType2Dial,   {{ 22, -10}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,   0, NULL,                                  NULL          }, // 102 Freq
    {moduleTypeFltPhase,   paramType1CommonDial,     paramType2Dial,   {{ 42,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 102 Spread M
    {moduleTypeFltPhase,   paramType1CommonDial,     paramType2Dial,   {{ 57, -17}, { 7, 14}}, anchorBottomLeft,  "FB",           128,  64, NULL,                                  NULL          }, // 102 FB
    {moduleTypeFltPhase,   paramType1StandardToggle, paramType2Menu,   {{ 70,  -3}, { 7,  7}}, anchorBottomLeft,  "Notch",          6,   4, int16StrMap,                           NULL          }, // 102 Notch count
    {moduleTypeFltPhase,   paramType1CommonDial,     paramType2Dial,   {{ 38, -17}, { 7, 14}}, anchorBottomLeft,  "Spread",       128,  64, NULL,                                  NULL          }, // 102 Spread
    {moduleTypeFltPhase,   paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 102 Bypass
    {moduleTypeFltPhase,   paramType1CommonDial,     paramType2Dial,   {{ 83,  -3}, { 7, 14}}, anchorBottomLeft,  "Level",        128,   0, NULL,                                  NULL          }, // 102 Level
    {moduleTypeFltPhase,   paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 102 FB Mod
    {moduleTypeFltPhase,   paramType1StandardToggle, paramType2Menu,   {{ 68, -12}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, fltPhaseTypeStrMap,                    NULL          }, // 102 Type
    {moduleTypeFltPhase,   paramType1StandardToggle, paramType2Menu,   {{  3, -22}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   4, offTo100KbStrMap,                      NULL          }, // 102 Kbt
    // 103 EqPeak
    {moduleTypeEqPeak,     paramType1Freq,           paramType2Dial,   {{ 22,  -3}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,  64, NULL,                                  NULL          }, // 103 Freq
    {moduleTypeEqPeak,     paramType1BipLevel,       paramType2Dial,   {{ 42,  -3}, { 7, 14}}, anchorBottomLeft,  "Gain",         128,  64, NULL,                                  NULL          }, // 103 Gain
    {moduleTypeEqPeak,     paramType1CommonDial,     paramType2Dial,   {{ 62, -10}, { 7, 14}}, anchorBottomLeft,  "BW",           128,  64, NULL,                                  NULL          }, // 103 BW *** needs UI element
    {moduleTypeEqPeak,     paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 103 Bypass
    {moduleTypeEqPeak,     paramType1CommonDial,     paramType2Dial,   {{ 83,  -3}, { 7, 14}}, anchorBottomLeft,  "Level",        128, 127, NULL,                                  NULL          }, // 102 Level

    // 104 Unknown
    // 105 ValSw2-1
    {moduleTypeValSw2to1,  paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, u128StrMap,                            NULL          }, // 105 Val
    // 106 OscNoise
    {moduleTypeOscNoise,   paramType1Freq,           paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 106 Tune
    {moduleTypeOscNoise,   paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 106 Fine
    {moduleTypeOscNoise,   paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 97 Kbt
    {moduleTypeOscNoise,   paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 97 Pitch M
    {moduleTypeOscNoise,   paramType1StandardToggle, paramType2Menu,   {{ 30,   6}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 97 Pitch Type
    {moduleTypeOscNoise,   paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  "Width",        128,  64, NULL,                                  NULL          }, // 106 Width
    {moduleTypeOscNoise,   paramType1CommonDial,     paramType2Dial,   {{ 65,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 106 Width M
    {moduleTypeOscNoise,   paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 106 Bypass

    // 107 Unknown
    // 108 Vocoder
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 5,  7}}, anchorBottomLeft,  "1",             17,   1, vocoderStrMap,                         NULL          }, // 108 BandSel 1
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 15,  -3}, { 5,  7}}, anchorBottomLeft,  "2",             17,   2, vocoderStrMap,                         NULL          }, // 108 BandSel 2
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 5,  7}}, anchorBottomLeft,  "3",             17,   3, vocoderStrMap,                         NULL          }, // 108 BandSel 3
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 25,  -3}, { 5,  7}}, anchorBottomLeft,  "4",             17,   4, vocoderStrMap,                         NULL          }, // 108 BandSel 4
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 30,  -3}, { 5,  7}}, anchorBottomLeft,  "5",             17,   5, vocoderStrMap,                         NULL          }, // 108 BandSel 5
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 35,  -3}, { 5,  7}}, anchorBottomLeft,  "6",             17,   6, vocoderStrMap,                         NULL          }, // 108 BandSel 6
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 5,  7}}, anchorBottomLeft,  "7",             17,   7, vocoderStrMap,                         NULL          }, // 108 BandSel 7
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 45,  -3}, { 5,  7}}, anchorBottomLeft,  "8",             17,   8, vocoderStrMap,                         NULL          }, // 108 BandSel 8
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 50,  -3}, { 5,  7}}, anchorBottomLeft,  "9",             17,   9, vocoderStrMap,                         NULL          }, // 108 BandSel 9
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 55,  -3}, { 5,  7}}, anchorBottomLeft,  "10",            17,  10, vocoderStrMap,                         NULL          }, // 108 BandSel 10
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 5,  7}}, anchorBottomLeft,  "11",            17,  11, vocoderStrMap,                         NULL          }, // 108 BandSel 11
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 65,  -3}, { 5,  7}}, anchorBottomLeft,  "12",            17,  12, vocoderStrMap,                         NULL          }, // 108 BandSel 12
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 70,  -3}, { 5,  7}}, anchorBottomLeft,  "13",            17,  13, vocoderStrMap,                         NULL          }, // 108 BandSel 13
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 75,  -3}, { 5,  7}}, anchorBottomLeft,  "14",            17,  14, vocoderStrMap,                         NULL          }, // 108 BandSel 14
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 80,  -3}, { 5,  7}}, anchorBottomLeft,  "15",            17,  15, vocoderStrMap,                         NULL          }, // 108 BandSel 15
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{ 85,  -3}, { 5,  7}}, anchorBottomLeft,  "16",            17,  16, vocoderStrMap,                         NULL          }, // 108 BandSel 16
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{  3, -12}, { 7,  7}}, anchorBottomLeft,  "Emp",            2,   0, offOnStrMap,                           NULL          }, // 108 Emphasis
    {moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu,   {{  3, -24}, { 7,  7}}, anchorBottomLeft,  "Mon",            2,   0, offOnStrMap,                           NULL          }, // 108 Monitor
    //{moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu, {{ 10, -24}, { 7,  7}}, anchorBottomLeft,  "-2",             1,   0, presetStrMap,           NULL          }, // 108 Preset -2  // TODO: should only be 18 parameters on this!
    //{moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu, {{ 17, -24}, { 7,  7}}, anchorBottomLeft,  "-1",             1,   0, presetStrMap,           NULL          }, // 108 Preset -1
    //{moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu, {{ 24, -24}, { 7,  7}}, anchorBottomLeft,  "-0",             1,   0, presetStrMap,           NULL          }, // 108 Preset 0
    //{moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu, {{ 31, -24}, { 7,  7}}, anchorBottomLeft,  "+1",             1,   0, presetStrMap,           NULL          }, // 108 Preset +1
    //{moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu, {{ 38, -24}, { 7,  7}}, anchorBottomLeft,  "+2",             1,   0, presetStrMap,           NULL          }, // 108 Preset +2
    //{moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu, {{ 45, -24}, { 7,  7}}, anchorBottomLeft,  "Inv",            1,   0, presetStrMap,           NULL          }, // 108 Preset Inv
    //{moduleTypeVocoder,    paramType1StandardToggle, paramType2Menu, {{ 52, -24}, { 7,  7}}, anchorBottomLeft,  "Rnd",            1,   0, presetStrMap,           NULL          }, // 108 Preset Rnd
    // 109 Unknown
    // 110 Unknown
    // 111 Unknown
    // 112 LevAdd
    {moduleTypeLevAdd,     paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 112 Constant
    {moduleTypeLevAdd,     paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, bipUniStrMap,                          NULL          }, // 112 Bip/Uni
    // 113 Fade1-2
    {moduleTypeFade1to2,   paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 113 Mix
    {moduleTypeFade1to2,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 113 Mix M
    // 114 Fade2-1
    {moduleTypeFade2to1,   paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 114 Mix
    {moduleTypeFade2to1,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 114 Mix M
    // 115 LevScaler
    {moduleTypeLevScaler,  paramType1dB,             paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "L",            128,  64, NULL,                                  NULL          }, // 115 Left slope +- 8 db/oct
    {moduleTypeLevScaler,  paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  "BP",           128,  64, NULL,                                  NULL          }, // 115 BreakPoint
    {moduleTypeLevScaler,  paramType1dB,             paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  "R",            128,  64, NULL,                                  NULL          }, // 115 Right slope +- 8 db/oct
    {moduleTypeLevScaler,  paramType1StandardToggle, paramType2Toggle, {{ 22,  -3}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 115 Kbt
    // 116 Mix8-1A
    {moduleTypeMix8to1A,   paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  "Pad",            3,   0, db12BPadStrMap,                        NULL          }, // 116 Pad
    // 117 LevMod
    {moduleTypeLevMod,     paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 117 ModDepth
    {moduleTypeLevMod,     paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 117 ModType
    // 118 Digitizer
    {moduleTypeDigitizer,  paramType1StandardToggle, paramType2Menu,   {{ 80,  -3}, { 7,  7}}, anchorBottomLeft,  "Bits",          13,  11, bitsStrMap,                            NULL          }, // 118 Bits
    {moduleTypeDigitizer,  paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Sample Rate",  128,  64, NULL,                                  NULL          }, // 118 Rate
    {moduleTypeDigitizer,  paramType1CommonDial,     paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Rate",         128,   0, NULL,                                  NULL          }, // 118 Rate M
    {moduleTypeDigitizer,  paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 106 Bypass
    // 119 EnvADDSR
    {moduleTypeEnvADDSR,   paramType1StandardToggle, paramType2Toggle, {{ 10,   8}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, kbStrMap,                              offOnColourMap}, // 119 Kb
    {moduleTypeEnvADDSR,   paramType1StandardToggle, paramType2Menu,   {{-30, -31}, { 7,  7}}, anchorBottomRight, NULL,             4,   0, envShapeStrMap,                        NULL          }, // 119 Shape
    {moduleTypeEnvADDSR,   paramType1ADRTime,        paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "A",            128,   0, NULL,                                  NULL          }, // 119 A
    {moduleTypeEnvADDSR,   paramType1ADRTime,        paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "D1",           128,   0, NULL,                                  NULL          }, // 119 D1
    {moduleTypeEnvADDSR,   paramType1CommonDial,     paramType2Dial,   {{ 44,  -3}, { 7, 14}}, anchorBottomLeft,  "L1",           128,   0, NULL,                                  NULL          }, // 119 L1
    {moduleTypeEnvADDSR,   paramType1ADRTime,        paramType2Dial,   {{ 56,  -3}, { 7, 14}}, anchorBottomLeft,  "D2",           128,   0, NULL,                                  NULL          }, // 119 D2
    {moduleTypeEnvADDSR,   paramType1CommonDial,     paramType2Dial,   {{ 68,  -3}, { 7, 14}}, anchorBottomLeft,  "L2",           128,   0, NULL,                                  NULL          }, // 119 L2
    {moduleTypeEnvADDSR,   paramType1ADRTime,        paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "R",            128,   0, NULL,                                  NULL          }, // 119 R
    {moduleTypeEnvADDSR,   paramType1StandardToggle, paramType2Menu,   {{ 20, -24}, { 7,  7}}, anchorBottomLeft,  "Sustain",        2,   0, sustainStrMap,                         NULL          }, // 119 Sustainmode
    {moduleTypeEnvADDSR,   paramType1StandardToggle, paramType2Menu,   {{-30, -24}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          }, // 119 Pos
    {moduleTypeEnvADDSR,   paramType1StandardToggle, paramType2Menu,   {{  5,  -8}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, normalResetStrMap,                     NULL          }, // 119 Reset

    // 120 Unknown
    // 121 SeqNote
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  3,  10}, { 7,  7}}, anchorTopLeft,     "Cycl",           2,   0, offOnStrMap,                           offOnColourMap}, // 121 Cycle
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Menu,   {{  0, -60}, {14, 14}}, anchorBottomRight, NULL,             8,   5, octaveStrMap,                          NULL          }, // 121 Octave
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Menu,   {{-15,  15}, { 8,  8}}, anchorTopRight,    "Length",        16,  15, int16StrMap,                           NULL          }, // 121 Octave
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0, -40}, {10,  7}}, anchorBottomRight, "Clr",            1,   0, emptyStrMap,                           NULL          }, // 121 Clear
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0, -30}, {10,  7}}, anchorBottomRight, "Rnd",            1,   0, emptyStrMap,                           NULL          }, // 121 Randomize
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy for now
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy
    {moduleTypeSeqNote,    paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 121 Dummy
    // 122 Unknown
    // 123 Mix4-1C
    {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 11, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          },   // 123 L1
    {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 31, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          },   // 123 L2
    {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 51, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          },   // 123 L3
    {moduleTypeMix4to1C,   paramType1CommonDial,     paramType2Dial,   {{ 71, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          },   // 123 L4
    {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{  5,  -1}, {18,  7}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap},   // 123 Enable1
    {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{ 25,  -1}, {18,  7}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap},   // 123 Enable2
    {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{ 45,  -1}, {18,  7}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap},   // 123 Enable3
    {moduleTypeMix4to1C,   paramType1Enable,         paramType2Toggle, {{ 65,  -1}, {18,  7}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap},   // 123 Enable4
    {moduleTypeMix4to1C,   paramType1StandardToggle, paramType2Menu,   {{-12,   6}, { 7,  7}}, anchorTopRight,    "Pad",            2,   0, padStrMap,                             NULL          },   // 123 Pad
    {moduleTypeMix4to1C,   paramType1StandardToggle, paramType2Menu,   {{  3,   6}, { 7,  7}}, anchorTopLeft,     "Curve",          3,   0, expStrMap,                             NULL          },   // 123 Exp
    // 124 Mux8-1 -- No params
    // 125 WahWah
    {moduleTypeWahWah,     paramType1Freq,           paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 125 Sweep M
    {moduleTypeWahWah,     paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 125 Sweep
    {moduleTypeWahWah,     paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 125 Bypass
    // 126 Name
    // 127 Fx-In
    {moduleTypeFxtoIn,     paramType1StandardToggle, paramType2Menu,   {{ 25,  -3}, { 7,  7}}, anchorBottomLeft,  "In FX",          2,   0, inFxStrMap,                            NULL          }, // 127 In
    {moduleTypeFxtoIn,     paramType1Bypass,         paramType2Toggle, {{-35,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 127 Bypass
    {moduleTypeFxtoIn,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  "Pad",            4,   0, db12PadStrMap,                         NULL          }, // 127 Pad
    // 128 MinMax -- No params
    // 128 Unknown
    // 130 BinCounter -- No params
    // 131 ADConv  -- No params
    // 132 DAConv -- No params
    // 133 Unknown
    // 134 FltHP
    {moduleTypeFltHP,      paramType1Freq,           paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 134 Freq
    {moduleTypeFltHP,      paramType1CommonDial,     paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 134 FreqMod
    {moduleTypeFltHP,      paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   4, offTo100KbStrMap,                      NULL          }, // 134 Kbt
    {moduleTypeFltHP,      paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 134 Bypass
    // 135 Unknown
    // 136 Unknown
    // 137 Unknown
    // 138 Unknown
    // 139 T&H -- No params
    // 140 Mix4-1S
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 15, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 123 L1
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 35, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 123 L2
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 55, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 123 L3
    {moduleTypeMix4to1S,   paramType1CommonDial,     paramType2Dial,   {{ 75, -10}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 123 L4
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{  3,  -3}, {14,  4}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 123 Enable1
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{ 23,  -3}, {14,  4}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 123 Enable2
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{ 43,  -3}, {14,  4}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 123 Enable3
    {moduleTypeMix4to1S,   paramType1Enable,         paramType2Toggle, {{ 63,  -3}, {14,  4}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 123 Enable4
    {moduleTypeMix4to1S,   paramType1StandardToggle, paramType2Menu,   {{ 40, -25}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 123 Exp
    // 141 CtrlSend
    {moduleTypeCtrlSend,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Ctrl",         128,   1, NULL,                                  NULL          }, // 141 Ctrl
    {moduleTypeCtrlSend,   paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  "Value",        128,   1, NULL,                                  NULL          }, // 141 Ctrl
    {moduleTypeCtrlSend,   paramType1StandardToggle, paramType2Menu,   {{ 85,  -3}, { 7,  7}}, anchorBottomLeft,  "Chan",          21,   0, midiChanStrMap,                        NULL          }, // 141 Channel
    // 142 PCSend
    {moduleTypePCSend,     paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  "Program",      128,   1, NULL,                                  NULL          }, // 142 Program
    {moduleTypePCSend,     paramType1StandardToggle, paramType2Menu,   {{ 85,  -3}, { 7,  7}}, anchorBottomLeft,  "Chan",          21,   0, midiChanStrMap,                        NULL          }, // 142 Channel
    // 143 NoteSend
    {moduleTypeNoteSend,   paramType1CommonDial,     paramType2Dial,   {{ 42,  -3}, { 7, 14}}, anchorBottomLeft,  "Vel",          128, 100, NULL,                                  NULL          }, // 143 Vel
    {moduleTypeNoteSend,   paramType1NoteDial,       paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "Note",         128,  64, NULL,                                  NULL          }, // 143 Ctrl
    {moduleTypeNoteSend,   paramType1StandardToggle, paramType2Menu,   {{ 85,  -3}, { 7,  7}}, anchorBottomLeft,  "Chan",          21,   0, midiChanStrMap,                        NULL          }, // 143 Channel
    // 144 SeqEvent
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 20, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-1
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 24, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-2
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 28, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-3
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 32, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-4
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 36, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-5
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 40, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-6
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 44, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-7
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 48, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-8
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 52, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-9
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 56, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-10
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 60, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-11
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 64, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-12
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 68, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-13
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 72, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-14
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 76, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-15
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 80, -15}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step1-16
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 20,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-1
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 24,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-2
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 28,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-3
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 32,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-4
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 36,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-5
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 40,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-6
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 44,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-7
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 48,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-8
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 52,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-9
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 56,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-10
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 60,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-11
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 64,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-12
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 68,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-13
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 72,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-14
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 76,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-15
    {moduleTypeSeqEvent,   paramType1Enable,         paramType2Toggle, {{ 80,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-16
    {moduleTypeSeqEvent,   paramType1StandardToggle, paramType2Toggle, {{  3,  10}, { 7,  7}}, anchorTopLeft,     "Cycl",           2,   0, offOnStrMap,                           offOnColourMap}, // 121 Cycle
    {moduleTypeSeqEvent,   paramType1StandardToggle, paramType2Menu,   {{-15,  15}, { 8,  8}}, anchorTopRight,    "Length",        16,  15, int16StrMap,                           NULL          }, // 121 Length
    {moduleTypeSeqEvent,   paramType1StandardToggle, paramType2Menu,   {{ 88, -16}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, trigGateStrMap,                        NULL          }, // 144 T up
    {moduleTypeSeqEvent,   paramType1StandardToggle, paramType2Menu,   {{ 88,  -4}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, trigGateStrMap,                        NULL          }, // 144 T down
    // 145 SeqVal
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 20, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-1
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 29, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-2
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 38, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-3
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 47, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-4
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 56, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-5
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 65, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-6
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 74, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-7
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 83, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-8
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 20, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-1
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 29, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-2
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 38, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-3
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 47, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-4
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 56, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-5
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 65, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-6
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 74, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-7
    {moduleTypeSeqVal,     paramType1Slider,         paramType2Dial,   {{ 83, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-8
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 20, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-1
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 29, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-2
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 38, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-3
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 47, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-4
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 56, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-5
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 65, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-68
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 74, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-7
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 83, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-8
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 20,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-9
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 29,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-10
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 38,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-11
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 47,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-12
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 56,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-13
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 65,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-14
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 74,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-15
    {moduleTypeSeqVal,     paramType1Enable,         paramType2Toggle, {{ 83,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-16
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  3,  10}, { 7,  7}}, anchorTopLeft,     "Cycl",           2,   0, offOnStrMap,                           offOnColourMap}, // 144 Cycle
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Menu,   {{-15,  15}, { 8,  8}}, anchorTopRight,    "Length",        16,  15, int16StrMap,                           NULL          }, // 144 Length
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Menu,   {{  3,  20}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, bipUniStrMap,                          NULL          }, // 144 Cycle
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Menu,   {{  3,  -4}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, trigGateStrMap,                        NULL          }, // 144 T down
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{ 35, -60}, {10,  7}}, anchorBottomLeft,  "Clr",            1,   0, emptyStrMap,                           NULL          }, // 144 Clear
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{ 45, -60}, {10,  7}}, anchorBottomLeft,  "Rnd",            1,   0, emptyStrMap,                           NULL          }, // 144 Randomize
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 144 Dummy
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 144 Dummy
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 144 Dummy
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 144 Dummy
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 144 Dummy
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 144 Dummy
    {moduleTypeSeqVal,     paramType1StandardToggle, paramType2Toggle, {{  0,   0}, { 1,  1}}, anchorBottomRight, NULL,             1,   0, emptyStrMap,                           NULL          }, // 144 Dummy
    // 146 SeqLev
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 20, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-1
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 29, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-2
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 38, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-3
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 47, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-4
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 56, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-5
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 65, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-6
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 74, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-7
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 83, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step1-8
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 20, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-1
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 29, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-2
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 38, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-3
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 47, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-4
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 56, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-5
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 65, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-6
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 74, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-7
    {moduleTypeSeqLev,     paramType1Slider,         paramType2Dial,   {{ 83, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // Step2-8
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 20, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-1
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 29, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-2
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 38, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-3
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 47, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-4
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 56, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-5
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 65, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-68
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 74, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-7
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 83, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-8
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 20,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-9
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 29,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-10
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 38,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-11
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 47,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-12
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 56,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-13
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 65,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-14
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 74,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-15
    {moduleTypeSeqLev,     paramType1Enable,         paramType2Toggle, {{ 83,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 144 Step2-16
    {moduleTypeSeqLev,     paramType1StandardToggle, paramType2Toggle, {{  3,  10}, { 7,  7}}, anchorTopLeft,     "Cycl",           2,   0, offOnStrMap,                           offOnColourMap}, // 144 Cycle
    {moduleTypeSeqLev,     paramType1StandardToggle, paramType2Menu,   {{-15,  15}, { 8,  8}}, anchorTopRight,    "Length",        16,  15, int16StrMap,                           NULL          }, // 144 Length
    {moduleTypeSeqLev,     paramType1StandardToggle, paramType2Menu,   {{  3,  20}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, bipUniStrMap,                          NULL          }, // 144 Cycle
    {moduleTypeSeqLev,     paramType1StandardToggle, paramType2Menu,   {{  3,  -4}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, trigGateStrMap,                        NULL          }, // 144 T down
    {moduleTypeSeqLev,     paramType1StandardToggle, paramType2Toggle, {{ 35, -60}, {10,  7}}, anchorBottomLeft,  "Clr",            1,   0, emptyStrMap,                           NULL          }, // 144 Clear
    {moduleTypeSeqLev,     paramType1StandardToggle, paramType2Toggle, {{ 45, -60}, {10,  7}}, anchorBottomLeft,  "Rnd",            1,   0, emptyStrMap,                           NULL          }, // 144 Randomize

    // 147 CtrRcv
    {moduleTypeCtrlRcv,    paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Ctrl",         128,  64, NULL,                                  NULL          }, // 147 Ctrl
    {moduleTypeCtrlRcv,    paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  "Chan",          18,  17, midiChanRcvStrMap,                     NULL          }, // 147 Channel
    // 148 NoteRcv
    {moduleTypeNoteRcv,    paramType1NoteDial,       paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Note",         128,  64, NULL,                                  NULL          }, // 148 Note
    {moduleTypeNoteRcv,    paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  "Chan",          18,  17, midiChanRcvStrMap,                     NULL          }, // 148 Channel
    // 149 NoteZone
    {moduleTypeNoteZone,   paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  "RChan",         18,  17, midiChanRcvStrMap,                     NULL          }, // 149 Receive Channel
    {moduleTypeNoteZone,   paramType1NoteDial,       paramType2Dial,   {{ 23,  -3}, { 7, 14}}, anchorBottomLeft,  "Min",          128,   0, NULL,                                  NULL          }, // 149 Min note
    {moduleTypeNoteZone,   paramType1NoteDial,       paramType2Dial,   {{ 43,  -3}, { 7, 14}}, anchorBottomLeft,  "Max",          128, 127, NULL,                                  NULL          }, // 149 Max note
    {moduleTypeNoteZone,   paramType1CommonDial,     paramType2Dial,   {{ 63,  -3}, { 7, 14}}, anchorBottomLeft,  "Trans",        128,  64, NULL,                                  NULL          }, // 149 Transpose
    {moduleTypeNoteZone,   paramType1StandardToggle, paramType2Menu,   {{ 83,  -3}, { 7,  7}}, anchorBottomLeft,  "SChan",         21,   0, midiChanStrMap,                        NULL          }, // 149 Send Channel
    {moduleTypeNoteZone,   paramType1StandardToggle, paramType2Menu,   {{ 60, -17}, { 7,  7}}, anchorBottomLeft,  "Thru",           2,   0, thruStrMap,                            NULL          }, // 149 Thru mode
    // 150 Compress
    {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  "Thr",           43,   0, NULL,                                  NULL          }, // 150
    {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 24,  -3}, { 7, 14}}, anchorBottomLeft,  "Ratio",         67,   0, NULL,                                  NULL          },
    {moduleTypeCompress,   paramType1StrMap,         paramType2Dial,   {{ 38,  -3}, { 7, 14}}, anchorBottomLeft,  "Att",          128,   0, compr_attack_timesStrMap,              NULL          },
    {moduleTypeCompress,   paramType1StrMap,         paramType2Dial,   {{ 52,  -3}, { 7, 14}}, anchorBottomLeft,  "Rel",          128,   0, compr_release_timesStrMap,             NULL          },
    {moduleTypeCompress,   paramType1CommonDial,     paramType2Dial,   {{ 66,  -3}, { 7, 14}}, anchorBottomLeft,  "RefLvl",        43,   0, NULL,                                  NULL          },
    {moduleTypeCompress,   paramType1StandardToggle, paramType2Toggle, {{ 27,   5}, { 7,  7}}, anchorTopLeft,     NULL,             2,   1, sideChainStrMap,                       offOnColourMap},
    {moduleTypeCompress,   paramType1Bypass,         paramType2Toggle, {{ -3,   0}, { 5,  5}}, anchorMiddleRight, NULL,             2,   1, NULL,                                  NULL          },
    // 151 Unknown
    // 152 KeyQuant
    {moduleTypeKeyQuant,   paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Range",        128,   0, NULL,                                  NULL          }, // 152 Range
    {moduleTypeKeyQuant,   paramType1StandardToggle, paramType2Menu,   {{ 20,   5}, { 7,  7}}, anchorTopLeft,     "Capture",        2,   0, captureStrMap,                         NULL          },
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 60,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note E
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 64,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note F
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 66,  -7}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note F#
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 68,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note G
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 70,  -7}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note G#
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 72,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note A
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 74,  -7}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note A#
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 76,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note B
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 52,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note C
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 54,  -7}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note C#
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 56,  -3}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note D
    {moduleTypeKeyQuant,   paramType1Enable,         paramType2Toggle, {{ 58,  -7}, { 3,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 152 Note D#
    // 153 Unknown
    // 154 SeqCtr
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 20, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step1
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 29, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step2
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 38, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step3
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 47, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step4
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 56, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step5
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 65, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step6
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 74, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step7
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 83, -42}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step8
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 20, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step9
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 29, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step10
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 38, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step11
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 47, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step12
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 56, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step13
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 65, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step14
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 74, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step15
    {moduleTypeSeqCtr,     paramType1Slider,         paramType2Dial,   {{ 83, -25}, { 7, 16}}, anchorBottomLeft,  NULL,           128,   0, bip128StrMap,                          NULL          }, // 154 Step16
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 20, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt1
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 29, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt2
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 38, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt3
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 47, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt4
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 56, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt5
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 65, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt6
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 74, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt7
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 83, -15}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt8
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 20,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt9
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 29,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt10
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 38,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt11
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 47,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt12
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 56,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt13
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 65,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt14
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 74,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt15
    {moduleTypeSeqCtr,     paramType1Enable,         paramType2Toggle, {{ 83,  -3}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 154 Evnt16
    {moduleTypeSeqCtr,     paramType1StandardToggle, paramType2Menu,   {{ 35, -60}, {10,  7}}, anchorBottomLeft,  "Pulse",          2,   0, trigGateStrMap,                        NULL          }, // 154 Pulse
    {moduleTypeSeqCtr,     paramType1StandardToggle, paramType2Menu,   {{  3,  10}, { 7,  7}}, anchorTopLeft,     "Pol",            2,   0, bipUniStrMap,                          NULL          }, // 154 Pol
    {moduleTypeSeqCtr,     paramType1StandardToggle, paramType2Menu,   {{ -3, -50}, { 7,  7}}, anchorBottomRight, "XFade",          4,   0, seqXFadeStrMap,                        NULL          }, // 154 XFade
    {moduleTypeSeqCtr,     paramType1StandardToggle, paramType2Toggle, {{ 55, -60}, {10,  7}}, anchorBottomLeft,  "Rnd",            1,   0, emptyStrMap,                           NULL          }, // 154 Random
    {moduleTypeSeqCtr,     paramType1StandardToggle, paramType2Toggle, {{ 65, -60}, {10,  7}}, anchorBottomLeft,  "Clr",            1,   0, emptyStrMap,                           NULL          }, // 154 Clear
    // 155 Unknown
    // 156 NoteDet
    {moduleTypeNoteDet,    paramType1NoteDial,       paramType2Dial,   {{ 23,  -3}, { 7, 14}}, anchorBottomLeft,  "Note",         128,  64, NULL,                                  NULL          }, // 156 Note
    // 157 LevConv
    {moduleTypeLevConv,    paramType1StandardToggle, paramType2Menu,   {{-15,  -1}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          }, // 157 OutType
    {moduleTypeLevConv,    paramType1StandardToggle, paramType2Menu,   {{-57,  -3}, { 7,  7}}, anchorBottomRight, "Input",          3,   0, levConvStrMap,                         NULL          }, // 157 In Signal Type
    // 158 Glide
    {moduleTypeGlide,      paramType1StrMap,         paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, glide_timeStrMap,                      NULL          }, // 158 Time
    {moduleTypeGlide,      paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, offOnStrMap,                           NULL          }, // 158 Glide on/off
    {moduleTypeGlide,      paramType1StandardToggle, paramType2Menu,   {{ 70,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, logStrMap,                             NULL          }, // 158 Shape
    // 159 CompSig -- no params
    // 160 ZeroCnt -- no params
    // 161 MixFader
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev1
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 29,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev2
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 38,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev3
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 47,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev4
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 56,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev5
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 65,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev6
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 74,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev7
    {moduleTypeMixFader,   paramType1MixLevel,       paramType2Dial,   {{ 83,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 161 Lev8
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 20, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On1
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 29, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On2
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 38, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On3
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 47, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On4
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 56, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On5
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 65, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On6
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 74, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On7
    {moduleTypeMixFader,   paramType1Enable,         paramType2Toggle, {{ 83, -20}, { 8,  8}}, anchorBottomLeft,  NULL,             2,   1, NULL,                                  offOnColourMap}, // 161 On8
    {moduleTypeMixFader,   paramType1StandardToggle, paramType2Menu,   {{  3, -28}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 161 ExpLin
    {moduleTypeMixFader,   paramType1StandardToggle, paramType2Menu,   {{  3, -35}, { 7,  7}}, anchorBottomLeft,  "Pad",            3,   0, db12BPadStrMap,                        NULL          }, // 161 Pad
    // 162 FltComb
    {moduleTypeFltComb,    paramType1Freq,           paramType2Dial,   {{ 22, -10}, { 7, 14}}, anchorBottomLeft,  "Freq",         128,   0, NULL,                                  NULL          }, // 102 Freq
    {moduleTypeFltComb,    paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 102 Pitch M
    {moduleTypeFltComb,    paramType1StandardToggle, paramType2Menu,   {{  3, -22}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   4, offTo100KbStrMap,                      NULL          }, // 102 Kbt
    {moduleTypeFltComb,    paramType1CommonDial,     paramType2Dial,   {{ 57, -17}, { 7, 14}}, anchorBottomLeft,  "FB",           128,  64, NULL,                                  NULL          }, // 102 FB
    {moduleTypeFltComb,    paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 102 FB Mod
    {moduleTypeFltComb,    paramType1StandardToggle, paramType2Menu,   {{ 68, -12}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, fltPhaseTypeStrMap,                    NULL          }, // 102 Type
    {moduleTypeFltComb,    paramType1CommonDial,     paramType2Dial,   {{ 83,  -3}, { 7, 14}}, anchorBottomLeft,  "Level",        128,   0, NULL,                                  NULL          }, // 102 Level
    {moduleTypeFltComb,    paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 102 Bypass
    // 163 OscShpA
    {moduleTypeOscShpA,    paramType1Freq,           paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,  64, NULL,                                  NULL          }, // 163 Coarse
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 163 Fine
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 163 Kbt
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 163 Pitch M
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Menu,   {{ 15,  15}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 163 Tune M
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "FM",           128,   0, NULL,                                  NULL          }, // FM
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Menu,   {{ 60,  15}, { 7,  7}}, anchorTopLeft,     NULL,             2,   0, fmTypeStrMap,                          NULL          }, // FM Pitch Track
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{-15,   5}, { 7, 14}}, anchorTopRight,    "Shape",        128,   0, NULL,                                  NULL          }, // 50% to 99%, not decimals
    {moduleTypeOscShpA,    paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "ShpCtrl",      128,   0, NULL,                                  NULL          }, // 163 Shape M
    {moduleTypeOscShpA,    paramType1StandardToggle, paramType2Menu,   {{ 30,  15}, { 7,  7}}, anchorTopLeft,     NULL,             6,   0, oscShpAStrMap,                         NULL          }, // 163 Wave
    {moduleTypeOscShpA,    paramType1Bypass,         paramType2Toggle, {{ -3,   0}, { 5,  5}}, anchorMiddleRight, "Bypass",         2,   1, NULL,                                  NULL          },
    // 164 OscDual
    {moduleTypeOscDual,    paramType1Freq,           paramType2Dial,   {{ 25,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,  64, NULL,                                  NULL          }, // 164 Coarse
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 164 Fine
    {moduleTypeOscDual,    paramType1StandardToggle, paramType2Toggle, {{ 12, -20}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 164 Kbt
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 164 Pitch M
    {moduleTypeOscDual,    paramType1StandardToggle, paramType2Menu,   {{ 25, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 164 Tune M
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 75, -17}, { 7, 14}}, anchorBottomLeft,  "SqrLvl",       128, 100, NULL,                                  NULL          }, // 164 SqrLvl
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 65, -17}, { 7, 14}}, anchorBottomLeft,  "PW",           128,   0, NULL,                                  NULL          }, // 164 PW
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  "SawLvl",       128, 100, NULL,                                  NULL          }, // 164 SawLvl
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 65,  -3}, { 7, 14}}, anchorBottomLeft,  "Phase",        128,   0, NULL,                                  NULL          }, // 164 Phase
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 90, -17}, { 7, 14}}, anchorBottomLeft,  "ubLvl",        128, 100, NULL,                                  NULL          }, // 164 Suboct Lvl
    {moduleTypeOscDual,    paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          },
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 55, -17}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 164 Sqr M
    {moduleTypeOscDual,    paramType1CommonDial,     paramType2Dial,   {{ 55,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 100, NULL,                                  NULL          }, // 164 Saw M
    {moduleTypeOscDual,    paramType1StandardToggle, paramType2Menu,   {{ -3, -10}, { 7,  7}}, anchorBottomRight, "Soft",           2,   0, offOnStrMap,                           NULL          }, // 164 Tune M
    // 165 DXRouter
    {moduleTypeDXRouter,   paramType1StandardToggle, paramType2Menu,   {{ 10, -20}, { 7,  7}}, anchorBottomLeft,  "Algorithm",     32,   0, dxAlgStrMap,                           NULL          }, // 165 Algorithm
    {moduleTypeDXRouter,   paramType1StandardToggle, paramType2Menu,   {{ 10, -10}, { 7,  7}}, anchorBottomLeft,  "Feedback",       8,   0, dxFeedbackStrMap,                      NULL          }, // 165 Feedback
    // 166 Unknown
    // 167 PShift
    {moduleTypePShift,     paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Semi",         128,  64, NULL,                                  NULL          }, // 167 Semi
    {moduleTypePShift,     paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Fine",         128,  64, NULL,                                  NULL          }, // 167 Fine
    {moduleTypePShift,     paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 167 Shift M
    {moduleTypePShift,     paramType1StandardToggle, paramType2Menu,   {{ 75, -10}, { 7,  7}}, anchorBottomLeft,  "Delay",          5,   0, pShiftDelayStrMod,                     NULL          }, // 167 Delay *** CHeck param vals
    {moduleTypePShift,     paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 167 On/Off
    // 168 Unknown
    // 169 ModAHD
    {moduleTypeModAHD,     paramType1ADRTime,        paramType2Dial,   {{ 20, -17}, { 7, 14}}, anchorBottomLeft,  "Attack",       128,   0, NULL,                                  NULL          }, // 46 Attack
    {moduleTypeModAHD,     paramType1ADRTime,        paramType2Dial,   {{ 40, -17}, { 7, 14}}, anchorBottomLeft,  "Hold",         128,   0, NULL,                                  NULL          }, // 46 Hold
    {moduleTypeModAHD,     paramType1ADRTime,        paramType2Dial,   {{ 60, -17}, { 7, 14}}, anchorBottomLeft,  "Decay",        128,   0, NULL,                                  NULL          }, // 46 Decay
    {moduleTypeModAHD,     paramType1CommonDial,     paramType2Dial,   {{ 27,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 167 Attack M
    {moduleTypeModAHD,     paramType1CommonDial,     paramType2Dial,   {{ 47,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 167 Hold M
    {moduleTypeModAHD,     paramType1CommonDial,     paramType2Dial,   {{ 67,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 167 Attack M
    {moduleTypeModAHD,     paramType1StandardToggle, paramType2Menu,   {{-20, -10}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          },
    {moduleTypeModAHD,     paramType1StandardToggle, paramType2Toggle, {{  3, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, kbStrMap,                              offOnColourMap},
    // 170 2-In
    {moduleType2toIn,      paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, twoToInSourceStrMap,                   NULL          },
    {moduleType2toIn,      paramType1Bypass,         paramType2Toggle, {{-24,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 167 On/Off
    {moduleType2toIn,      paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   1, db12PadStrMap,                         NULL          },
    // 171 4-In
    {moduleType4toIn,      paramType1StandardToggle, paramType2Menu,   {{ 30,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, fourToInSourceStrMap,                  NULL          }, // 171 source
    {moduleType4toIn,      paramType1Bypass,         paramType2Toggle, {{-45,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 171 On/Off
    {moduleType4toIn,      paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   1, db12PadStrMap,                         NULL          }, // 171 pad
    // 172 DlySingleA
    {moduleTypeDlySingleA, paramType1Time,           paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, NULL,                                  NULL          }, // 158 Time
    // 173 DlySingleB
    {moduleTypeDlySingleB, paramType1Time,           paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, NULL,                                  NULL          }, // 158 Time
    {moduleTypeDlySingleB, paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 173 Time M
    // 174 DelayDual
    {moduleTypeDelayDual,  paramType1Time,           paramType2Dial,   {{ 53, -10}, { 7, 14}}, anchorBottomLeft,  "Time1",        128,   0, NULL,                                  NULL          }, // 174 Time
    {moduleTypeDelayDual,  paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 174 Time M
    {moduleTypeDelayDual,  paramType1Time,           paramType2Dial,   {{ 88, -10}, { 7, 14}}, anchorBottomLeft,  "Time2",        128,   0, NULL,                                  NULL          }, // 174 Time2
    {moduleTypeDelayDual,  paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 174 Time M
    // 175 DelayQuad
    {moduleTypeDelayQuad,  paramType1TimeClk,        paramType2Dial,   {{  3, -10}, { 7, 14}}, anchorBottomLeft,  "Time1",        128,   0, NULL,                                  NULL          }, // 175 Time1
    {moduleTypeDelayQuad,  paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 175 Time1 M
    {moduleTypeDelayQuad,  paramType1TimeClk,        paramType2Dial,   {{ 23, -10}, { 7, 14}}, anchorBottomLeft,  "Time2",        128,   0, NULL,                                  NULL          }, // 175 Time2
    {moduleTypeDelayQuad,  paramType1CommonDial,     paramType2Dial,   {{ 30,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 175 Time2 M
    {moduleTypeDelayQuad,  paramType1TimeClk,        paramType2Dial,   {{ 43, -10}, { 7, 14}}, anchorBottomLeft,  "Time3",        128,   0, NULL,                                  NULL          }, // 175 Time3
    {moduleTypeDelayQuad,  paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 175 Time3 M
    {moduleTypeDelayQuad,  paramType1TimeClk,        paramType2Dial,   {{ 63, -10}, { 7, 14}}, anchorBottomLeft,  "Time4",        128,   0, NULL,                                  NULL          }, // 175 Time4
    {moduleTypeDelayQuad,  paramType1CommonDial,     paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 175 Time4 M
    {moduleTypeDelayQuad,  paramType1StandardToggle, paramType2Menu,   {{ 80, -10}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, timeClkStrMap,                         NULL          }, // 175 Time/Clk
    // 176 DelayA
    {moduleTypeDelayA,     paramType1TimeClk,        paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, NULL,                                  NULL          }, // 176 Time
    {moduleTypeDelayA,     paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "FB",           128,   0, NULL,                                  NULL          }, // 176 FB
    {moduleTypeDelayA,     paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Filter",       128,  64, NULL,                                  NULL          }, // 176 Filter
    {moduleTypeDelayA,     paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  "Dry/Wet",      128,  64, NULL,                                  NULL          }, // 176 Filter
    {moduleTypeDelayA,     paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 176 On/Off
    {moduleTypeDelayA,     paramType1StandardToggle, paramType2Menu,   {{ 30,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, timeClkStrMap,                         NULL          }, // 176 Time/Clk
    // 177 DelayB
    {moduleTypeDelayB,     paramType1TimeClk,        paramType2Dial,   {{ 40, -10}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, NULL,                                  NULL          }, // 176 Time
    {moduleTypeDelayB,     paramType1CommonDial,     paramType2Dial,   {{ 50, -17}, { 7, 14}}, anchorBottomLeft,  "FB",           128,   0, NULL,                                  NULL          }, // 176 FB
    {moduleTypeDelayB,     paramType1Freq,           paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "LP",           128,  64, NULL,                                  NULL          }, // 176 Filter
    {moduleTypeDelayB,     paramType1CommonDial,     paramType2Dial,   {{ 75, -17}, { 7, 14}}, anchorBottomLeft,  "Dry/Wet",      128,  64, NULL,                                  NULL          }, // 176 Filter
    {moduleTypeDelayB,     paramType1StandardToggle, paramType2Menu,   {{ 30,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, timeClkStrMap,                         NULL          }, // 176 Time/Clk
    {moduleTypeDelayB,     paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 176 FBmod
    {moduleTypeDelayB,     paramType1CommonDial,     paramType2Dial,   {{ 75,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 176 DryWet mod
    {moduleTypeDelayB,     paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 176 On/Off
    {moduleTypeDelayB,     paramType1Freq,           paramType2Dial,   {{ 60, -17}, { 7, 14}}, anchorBottomLeft,  "HP",           128,  64, NULL,                                  NULL          }, // 176 Filter
    // 178 DlyClock
    {moduleTypeDlyClock,   paramType1Int,            paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Sample delay", 128,   0, NULL,                                  NULL          }, // 176 Time
    // 179 DlyShiftReg
    // 180 Operator
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Toggle, {{ 20, -60}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 180 Kbt
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 30, -60}, { 7,  7}}, anchorBottomLeft,  "Sync",           2,   0, offOnStrMap,                           NULL          }, // 180 Sync *** Check values
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 40, -60}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, ratioFixedStrMap,                      NULL          }, // 180 RatioFixes
    {moduleTypeOperator,   paramType1Freq,           paramType2Dial,   {{ 60, -60}, { 7, 14}}, anchorBottomLeft,  "Coarse",       128,   0, NULL,                                  NULL          }, // 180 Freq
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 50, -60}, { 7, 14}}, anchorBottomLeft,  "Fine",         128,   0, NULL,                                  NULL          }, // 180 Freq
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 70, -60}, { 7,  7}}, anchorBottomLeft,  "Detune",        15,   7, detuneStrMap,                          NULL          }, // 180 FreqDetune
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 70, -50}, { 7,  7}}, anchorBottomLeft,  "Vel",            8,   0, operator07StrMap,                      NULL          }, // 180 Vel
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 83, -50}, { 7,  7}}, anchorBottomLeft,  "RateScale",      8,   0, operator07StrMap,                      NULL          }, // 180 RateScale
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 20, -30}, { 7, 14}}, anchorBottomLeft,  "R1",           128, 127, NULL,                                  NULL          }, // 180 R1
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 30, -30}, { 7, 14}}, anchorBottomLeft,  "L1",           128, 127, NULL,                                  NULL          }, // 180 L1
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 40, -30}, { 7, 14}}, anchorBottomLeft,  "R2",           128, 127, NULL,                                  NULL          }, // 180 R2
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 50, -30}, { 7, 14}}, anchorBottomLeft,  "L2",           128, 127, NULL,                                  NULL          }, // 180 L2
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 60, -30}, { 7, 14}}, anchorBottomLeft,  "R3",           128, 127, NULL,                                  NULL          }, // 180 R3
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 70, -30}, { 7, 14}}, anchorBottomLeft,  "L3",           128, 127, NULL,                                  NULL          }, // 180 L3
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 80, -30}, { 7, 14}}, anchorBottomLeft,  "R4",           128, 127, NULL,                                  NULL          }, // 180 R4
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 90, -30}, { 7, 14}}, anchorBottomLeft,  "L4",           128, 127, NULL,                                  NULL          }, // 180 L4
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 17, -20}, { 7,  7}}, anchorBottomLeft,  "AMod",           8,   0, operator07StrMap,                      NULL          }, // 180 AMod
    {moduleTypeOperator,   paramType1NoteDial,       paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "BrPt",         128,  64, NULL,                                  NULL          }, // 180 AMod
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 35,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, operatorDepthStrMap,                   NULL          }, // 180 LDepth Mode
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 45,  -3}, { 7,  7}}, anchorBottomLeft,  "Ldepth",         8,   0, operator07StrMap,                      NULL          }, // 180 LDepth
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, operatorDepthStrMap,                   NULL          }, // 180 RDepth Mode
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Menu,   {{ 70,  -3}, { 7,  7}}, anchorBottomLeft,  "Rdepth",         8,   0, operator07StrMap,                      NULL          }, // 180 RDepth
    {moduleTypeOperator,   paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "Level",        128, 127, NULL,                                  NULL          }, // 180 Level
    {moduleTypeOperator,   paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 180 On/Off
    {moduleTypeOperator,   paramType1StandardToggle, paramType2Toggle, {{ 20, -50}, { 7,  7}}, anchorBottomLeft,  "KBEnv",          2,   1, offOnStrMap,                           offOnColourMap}, // 180 EnvLB
    // 181 DlyEight
    {moduleTypeDlyEight,   paramType1Time,           paramType2Dial,   {{ 40, -10}, { 7, 14}}, anchorBottomLeft,  "Time",         128,   0, NULL,                                  NULL          }, // 181 Time
    // 182 DlyStereo
    {moduleTypeDlyStereo,  paramType1TimeClk,        paramType2Dial,   {{ 30, -17}, { 7, 14}}, anchorBottomLeft,  "Left",         128,   0, NULL,                                  NULL          }, // 182 Time Left
    {moduleTypeDlyStereo,  paramType1TimeClk,        paramType2Dial,   {{ 30,  -3}, { 7, 14}}, anchorBottomLeft,  "Right",        128,   0, NULL,                                  NULL          }, // 182 Time Right
    {moduleTypeDlyStereo,  paramType1CommonDial,     paramType2Dial,   {{ 40, -17}, { 7, 14}}, anchorBottomLeft,  "FB",           128, 127, NULL,                                  NULL          }, // 182 FB L
    {moduleTypeDlyStereo,  paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128, 127, NULL,                                  NULL          }, // 182 FB R
    {moduleTypeDlyStereo,  paramType1CommonDial,     paramType2Dial,   {{ 50, -17}, { 7, 14}}, anchorBottomLeft,  "X-FB",         128, 127, NULL,                                  NULL          }, // 182 X-FB L
    {moduleTypeDlyStereo,  paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "X-FB",         128, 127, NULL,                                  NULL          }, // 180 X-FB R
    {moduleTypeDlyStereo,  paramType1StandardToggle, paramType2Menu,   {{ 20, -24}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, timeClkStrMap,                         NULL          }, // 182 Time/Clk
    {moduleTypeDlyStereo,  paramType1Freq,           paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "LP",           128, 127, NULL,                                  NULL          }, // 182 LP
    {moduleTypeDlyStereo,  paramType1CommonDial,     paramType2Dial,   {{ 80, -17}, { 7, 14}}, anchorBottomLeft,  "Dry/Wet",      128,  64, NULL,                                  NULL          }, // 182 Dry/Wet
    {moduleTypeDlyStereo,  paramType1Bypass,         paramType2Toggle, {{ -3, -17}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 182 On/Off
    {moduleTypeDlyStereo,  paramType1Freq,           paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  "HP",           128, 127, NULL,                                  NULL          }, // 180 HP
    // 183 OscPM
    {moduleTypeOscPM,      paramType1CommonDial,     paramType2Dial,   {{ 32,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 9 Coarse
    {moduleTypeOscPM,      paramType1CommonDial,     paramType2Dial,   {{ 45,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 9 Fine
    {moduleTypeOscPM,      paramType1StandardToggle, paramType2Toggle, {{ 22, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 9 Kbt
    {moduleTypeOscPM,      paramType1StandardToggle, paramType2Menu,   {{ 30,  10}, { 7,  7}}, anchorTopLeft,     NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 9 Tune Mode
    {moduleTypeOscPM,      paramType1CommonDial,     paramType2Dial,   {{ 62,  -3}, { 7, 14}}, anchorBottomLeft,  "PhM",          128,   0, NULL,                                  NULL          }, // FM
    {moduleTypeOscPM,      paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 9 Bypass
    {moduleTypeOscPM,      paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // Pitch Mode
    // 184 Mix1-1A
    {moduleTypeMix1to1A,   paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 184 Level
    {moduleTypeMix1to1A,   paramType1Enable,         paramType2Toggle, {{ 40,  -3}, {14,  4}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 184 Enable1
    {moduleTypeMix1to1A,   paramType1StandardToggle, paramType2Menu,   {{ 30, -25}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 184 Exp
    // 185 Mix1-1S
    {moduleTypeMix1to1S,   paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 184 Level
    {moduleTypeMix1to1S,   paramType1Enable,         paramType2Toggle, {{ 40,  -3}, {14,  4}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 184 Enable1
    {moduleTypeMix1to1S,   paramType1StandardToggle, paramType2Menu,   {{ 30, -25}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 184 Exp
    // 186 Sw1-2M
    {moduleTypeSw1to2M,    paramType1StandardToggle, paramType2Menu,   {{ 37,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, offOnStrMap,                           NULL          }, // 186 Selector *** Needs new UI element
    // 187 Sw2-1M
    {moduleTypeSw2to1M,    paramType1StandardToggle, paramType2Menu,   {{ 37,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, offOnStrMap,                           NULL          }, // 187 Selector *** Needs new UI element
    // 188 ConstSwM
    {moduleTypeConstSwM,   paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 188 Level
    {moduleTypeConstSwM,   paramType1StandardToggle, paramType2Menu,   {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, offOnStrMap,                           NULL          }, // 188 Selector *** needs new UI element
    {moduleTypeConstSwM,   paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, bipUniStrMap,                          NULL          }, // 188 Bip/Uni
    // 189 NoiseGate
    {moduleTypeNoiseGate,  paramType1StrMap,         paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Threshold",    128,  64, noisegate_pitchtrack_threshholdStrMap, NULL          }, // 189 Threshold
    {moduleTypeNoiseGate,  paramType1StrMap,         paramType2Dial,   {{ 20,  -3}, { 7, 14}}, anchorBottomLeft,  "Attack",       128,   0, noise_gate_attackStrMap,               NULL          }, // 189 Attack
    {moduleTypeNoiseGate,  paramType1StrMap,         paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Release",      128,   0, noise_gate_releaseStrMap,              NULL          }, // 189 Release
    {moduleTypeNoiseGate,  paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 9 Bypass
    // 190 LfoB
    {moduleTypeLfoB,       paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 26 Rate  *** Could have Freq but tied to Range
    {moduleTypeLfoB,       paramType1CommonDial,     paramType2Dial,   {{ 30,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 26 Rate M
    {moduleTypeLfoB,       paramType1StandardToggle, paramType2Menu,   {{ 40, -20}, { 7,  7}}, anchorBottomLeft,  NULL,             5,   0, rangeStrMap,                           NULL          }, // 26 Range
    {moduleTypeLfoB,       paramType1StandardToggle, paramType2Menu,   {{ 50, -13}, { 7,  7}}, anchorBottomLeft,  "Kbt",            5,   0, offTo100KbStrMap,                      NULL          }, // 26 Kbt
    {moduleTypeLfoB,       paramType1StandardToggle, paramType2Menu,   {{ 60, -20}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, lfoWaveStrMap,                         NULL          }, // 26 Wave
    {moduleTypeLfoB,       paramType1StandardToggle, paramType2Menu,   {{ 30, -20}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, polyMonoStrMap,                        NULL          }, // 26 Mode
    {moduleTypeLfoB,       paramType1CommonDial,     paramType2Dial,   {{ 77,  -3}, { 7, 14}}, anchorBottomLeft,  "Phase",        128,  64, NULL,                                  NULL          }, // 26 Phase
    {moduleTypeLfoB,       paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 26 Bypass
    {moduleTypeLfoB,       paramType1StandardToggle, paramType2Menu,   {{-17, -17}, { 7,  7}}, anchorBottomRight, NULL,             6,   0, posStrMap,                             NULL          }, // 26 OutType
    {moduleTypeLfoB,       paramType1CommonDial,     paramType2Dial,   {{ 67,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 26 Phase M
    // 191 Unknown
    // 192 Phaser
    {moduleTypePhaser,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, phaserTypeStrMap,                      NULL          }, // 192 Type
    {moduleTypePhaser,     paramType1StrMap,         paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Rate",         128,  64, phaser_freqStrMap,                     NULL          }, // 192 Rate
    {moduleTypePhaser,     paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "FB",           128,   0, NULL,                                  NULL          }, // 192 FB
    {moduleTypePhaser,     paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 192 Bypass
    // 193 Mix4-1A -- no params
    // 194 Mix2-1A
    {moduleTypeMix2to1A,   paramType1CommonDial,     paramType2Dial,   {{ 57,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 123 L1
    {moduleTypeMix2to1A,   paramType1Enable,         paramType2Toggle, {{ 37,  -3}, { 5,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 123 Enable1
    {moduleTypeMix2to1A,   paramType1CommonDial,     paramType2Dial,   {{ 82,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,   0, NULL,                                  NULL          }, // 123 L2
    {moduleTypeMix2to1A,   paramType1Enable,         paramType2Toggle, {{ 67,  -3}, { 5,  3}}, anchorBottomLeft,  NULL,             2,   0, NULL,                                  offOnColourMap}, // 123 Enable2
    {moduleTypeMix2to1A,   paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, expStrMap,                             NULL          }, // 123 Exp
    // 195 ModAmt
    {moduleTypeModAmt,     paramType1CommonDial,     paramType2Dial,   {{ 40,  -3}, { 7, 14}}, anchorBottomLeft,  "Depth",        128,   0, NULL,                                  NULL          }, // 195 Depth
    {moduleTypeModAmt,     paramType1StandardToggle, paramType2Toggle, {{ 60,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   1, offOnStrMap,                           offOnColourMap}, // 195 Enable
    {moduleTypeModAmt,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, expStrMap,                             NULL          }, // 195 Exp/Lin
    {moduleTypeModAmt,     paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, invertStrMap,                          NULL          }, // 195 m/1-m
    // 196 OscPerc
    {moduleTypeOscPerc,    paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Tune",         128,  64, NULL,                                  NULL          }, // 196 Coarse
    {moduleTypeOscPerc,    paramType1CommonDial,     paramType2Dial,   {{ 60,  -3}, { 7, 14}}, anchorBottomLeft,  "Cent",         128,  64, NULL,                                  NULL          }, // 196 Fine
    {moduleTypeOscPerc,    paramType1StandardToggle, paramType2Menu,   {{ 50, -17}, { 7,  7}}, anchorBottomLeft,  NULL,             4,   0, pitchTypeStrMap,                       NULL          }, // 196 Tune Mode
    {moduleTypeOscPerc,    paramType1StandardToggle, paramType2Toggle, {{ 40, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 196  Kbt
    {moduleTypeOscPerc,    paramType1CommonDial,     paramType2Dial,   {{ 12,  -3}, { 7, 14}}, anchorBottomLeft,  "Pitch",        128,   0, NULL,                                  NULL          }, // 196 Pitch M
    {moduleTypeOscPerc,    paramType1ADRTime,        paramType2Dial,   {{ 70,  -3}, { 7, 14}}, anchorBottomLeft,  "Decay",        128,  64, NULL,                                  NULL          }, // 196 Decay
    {moduleTypeOscPerc,    paramType1CommonDial,     paramType2Dial,   {{ 80,  -3}, { 7, 14}}, anchorBottomLeft,  "Click",        128,  64, NULL,                                  NULL          }, // 196 Click
    {moduleTypeOscPerc,    paramType1StandardToggle, paramType2Toggle, {{ 90, -10}, { 7,  7}}, anchorBottomLeft,  "Punch",          2,   1, offOnStrMap,                           offOnColourMap}, // 196 Punch
    {moduleTypeOscPerc,    paramType1Bypass,         paramType2Toggle, {{-10,  -3}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 196 Bypass
    // 197 Status -- No params
    // 198 PitchTrack
    {moduleTypePitchTrack, paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Threshold",    128,  64, NULL,                                  NULL          }, // 198 Threshold
    // 199 MonoKey
    {moduleTypeMonoKey,    paramType1StandardToggle, paramType2Menu,   {{ 30,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, monoKeyStrMap,                         NULL          }, // 199 Priority
    // 200 RandomA
    {moduleTypeRandomA,    paramType1Freq,           paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 200 Rate
    {moduleTypeRandomA,    paramType1StandardToggle, paramType2Menu,   {{ 30,   0}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, polyMonoStrMap,                        NULL          }, // 200 Poly/Mono mode
    {moduleTypeRandomA,    paramType1StandardToggle, paramType2Menu,   {{ 80,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, bipPosNegStrMap,                       NULL          }, // 200 OutType
    {moduleTypeRandomA,    paramType1StandardToggle, paramType2Menu,   {{ 30,  -9}, { 7,  7}}, anchorBottomLeft,  NULL,             5,   0, rangeStrMap,                           NULL          }, // 200 Range
    {moduleTypeRandomA,    paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 26 Bypass
    {moduleTypeRandomA,    paramType1StandardToggle, paramType2Menu,   {{ 65,  -2}, { 7,  7}}, anchorBottomLeft,  "Edge",           5,   0, edgeStepStrMap,                        NULL          }, // 200 edge
    {moduleTypeRandomA,    paramType1StandardToggle, paramType2Menu,   {{ 65, -10}, { 7,  7}}, anchorBottomLeft,  "Step",           4,   0, edgeStepStrMap,                        NULL          }, // 200 step
    // 201 Red2Blue
    // 202 RandomB
    {moduleTypeRandomB,    paramType1Freq,           paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  NULL,           128,  64, NULL,                                  NULL          }, // 202 Rate
    {moduleTypeRandomB,    paramType1StandardToggle, paramType2Menu,   {{ 30,   0}, { 7,  7}}, anchorBottomLeft,  NULL,             2,   0, polyMonoStrMap,                        NULL          }, // 202 Poly/Mono mode
    {moduleTypeRandomB,    paramType1StandardToggle, paramType2Toggle, {{ 40, -10}, { 7,  7}}, anchorBottomLeft,  "Kbt",            2,   1, offOnStrMap,                           offOnColourMap}, // 202  Kbt
    {moduleTypeRandomB,    paramType1CommonDial,     paramType2Dial,   {{ 10,  -3}, { 7, 14}}, anchorBottomLeft,  "Rate",         128,  64, NULL,                                  NULL          }, // 202 Rate M
    {moduleTypeRandomB,    paramType1CommonDial,     paramType2Dial,   {{ 65, -10}, { 7, 14}}, anchorBottomLeft,  "Step",         128,  64, NULL,                                  NULL          }, // 202 Rate M
    {moduleTypeRandomB,    paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, "Bypass",         2,   1, NULL,                                  NULL          }, // 202 Bypass
    {moduleTypeRandomB,    paramType1StandardToggle, paramType2Menu,   {{ 80,  -3}, { 7,  7}}, anchorBottomLeft,  NULL,             3,   0, bipPosNegStrMap,                       NULL          }, // 202 OutType
    {moduleTypeRandomB,    paramType1StandardToggle, paramType2Menu,   {{ 20, -14}, { 7,  7}}, anchorBottomLeft,  NULL,             5,   0, rangeStrMap,                           NULL          }, // 202 Range
    {moduleTypeRandomB,    paramType1StandardToggle, paramType2Menu,   {{ 65,  -2}, { 7,  7}}, anchorBottomLeft,  "Edge",           5,   0, edgeStepStrMap,                        NULL          }, // 202 edge
    // 203 Blue2Red
    // 204 RndClkA
    {moduleTypeRndClkA,    paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Step",         128, 127, NULL,                                  NULL          }, // 204 Step
    {moduleTypeRndClkA,    paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  "Mode",           4,   0, NULL,                                  NULL          }, // 204 Mode
    {moduleTypeRndClkA,    paramType1StandardToggle, paramType2Menu,   {{ 30,  -3}, { 7,  7}}, anchorBottomLeft,  "Dice",           4,   0, NULL,                                  NULL          }, // 204 Dice
    {moduleTypeRndClkA,    paramType1StandardToggle, paramType2Menu,   {{ 65,  -3}, { 7,  7}}, anchorBottomLeft,  "Out",            4,   0, NULL,                                  NULL          }, // 204 OutType
    {moduleTypeRndClkA,    paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 204 On/Off
    // 205 RndTrig
    {moduleTypeRndTrig,    paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Step",         128,  64, NULL,                                  NULL          }, // 205 Step
    {moduleTypeRndTrig,    paramType1CommonDial,     paramType2Dial,   {{ 65,  -3}, { 7, 14}}, anchorBottomLeft,  "StepM",        128,   0, NULL,                                  NULL          }, // 205 Step M
    {moduleTypeRndTrig,    paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 205 On/Off
    {moduleTypeRndTrig,    paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  "Mode",           4,   0, NULL,                                  NULL          }, // 205 Mode

    // 206 RndClkB
    {moduleTypeRndClkB,    paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Step",         128, 127, NULL,                                  NULL          }, // 206 Step
    {moduleTypeRndClkB,    paramType1StandardToggle, paramType2Menu,   {{ 65,  -3}, { 7,  7}}, anchorBottomLeft,  "Out",            4,   0, NULL,                                  NULL          }, // 206 OutType
    {moduleTypeRndClkB,    paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 206 On/Off
    {moduleTypeRndClkB,    paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  "Mode",           4,   0, NULL,                                  NULL          }, // 206 Mode
    {moduleTypeRndClkB,    paramType1CommonDial,     paramType2Dial,   {{ 35,  -3}, { 7, 14}}, anchorBottomLeft,  "StepM",        128,   0, NULL,                                  NULL          }, // 206 Step M
    // 207 Unknown
    // 208 RndPattern
    {moduleTypeRndPattern, paramType1CommonDial,     paramType2Dial,   {{ 50, -14}, { 7, 14}}, anchorBottomLeft,  "PatA",         128,  64, NULL,                                  NULL          }, // 208 PatternA
    {moduleTypeRndPattern, paramType1CommonDial,     paramType2Dial,   {{ 65, -14}, { 7, 14}}, anchorBottomLeft,  "PatB",         128,  64, NULL,                                  NULL          }, // 208 PatternB
    {moduleTypeRndPattern, paramType1CommonDial,     paramType2Dial,   {{ 50,  -3}, { 7, 14}}, anchorBottomLeft,  "Step",         128, 127, NULL,                                  NULL          }, // 208 Step
    {moduleTypeRndPattern, paramType1StandardToggle, paramType2Menu,   {{ 35,  -3}, { 7,  7}}, anchorBottomLeft,  "Loop",          16,  15, NULL,                                  NULL          }, // 208 LoopCount
    {moduleTypeRndPattern, paramType1CommonDial,     paramType2Dial,   {{ 65,  -3}, { 7, 14}}, anchorBottomLeft,  "StepM",        128,   0, NULL,                                  NULL          }, // 208 Step M
    {moduleTypeRndPattern, paramType1StandardToggle, paramType2Menu,   {{ 20,  -3}, { 7,  7}}, anchorBottomLeft,  "Out",            4,   0, NULL,                                  NULL          }, // 208 OutType
    {moduleTypeRndPattern, paramType1Bypass,         paramType2Toggle, {{ -3, -10}, { 5,  5}}, anchorBottomRight, NULL,             2,   1, NULL,                                  NULL          }, // 208 On/Off
};

// Note these need to be in same order of connectors which are referenced by the hardware
const tConnectorLocation connectorLocationList[] = {
    //  0 Unknown
    //  1 Keyboard
    {moduleTypeKeyboard,    connectorDirOut, connectorTypeControl, {{ 10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Pitch",        labelLocLeft },          // 1
    {moduleTypeKeyboard,    connectorDirOut, connectorTypeLogic,   {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Gate",         labelLocLeft },
    {moduleTypeKeyboard,    connectorDirOut, connectorTypeControl, {{ 53,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Lin",          labelLocLeft },
    {moduleTypeKeyboard,    connectorDirOut, connectorTypeControl, {{ 93,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rel",          labelLocLeft },
    {moduleTypeKeyboard,    connectorDirOut, connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Note",         labelLocLeft },
    {moduleTypeKeyboard,    connectorDirOut, connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Exp",          labelLocLeft },
    // 2 Unknown
    // 3 4-Out
    {moduleType4toOut,      connectorDirIn,  connectorTypeAudio,   {{-24,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "1",            labelLocUp   },          // 3 4-out
    {moduleType4toOut,      connectorDirIn,  connectorTypeAudio,   {{-17,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "2",            labelLocUp   },
    {moduleType4toOut,      connectorDirIn,  connectorTypeAudio,   {{-10,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "3",            labelLocUp   },
    {moduleType4toOut,      connectorDirIn,  connectorTypeAudio,   {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "4",            labelLocUp   },
    //  4 2-Out
    {moduleType2toOut,      connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 4
    {moduleType2toOut,      connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },
    //  5 Invert
    {moduleTypeInvert,      connectorDirIn,  connectorTypeLogic,   {{-48,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocLeft },          // 5 In 1
    {moduleTypeInvert,      connectorDirOut, connectorTypeLogic,   {{-33,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocLeft },          // 5 Out 1
    {moduleTypeInvert,      connectorDirIn,  connectorTypeLogic,   {{-18,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocLeft },          // 5 In 2
    {moduleTypeInvert,      connectorDirOut, connectorTypeLogic,   {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocLeft },          // 5 Out 2
    //  6 Unknown
    //  7 OscB
    {moduleTypeOscB,        connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight}, // 7 OscB Pitch
    {moduleTypeOscB,        connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight}, // PitchVar
    {moduleTypeOscB,        connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",         labelLocRight}, // Sync
    {moduleTypeOscB,        connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight}, // FmMod
    {moduleTypeOscB,        connectorDirIn,  connectorTypeAudio,   {{ 73,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight}, // Shape Mod
    {moduleTypeOscB,        connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // Out
    //  8 OscShpB
    {moduleTypeOscShpB,     connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 8 OscShpB
    {moduleTypeOscShpB,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },
    {moduleTypeOscShpB,     connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },
    {moduleTypeOscShpB,     connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },
    {moduleTypeOscShpB,     connectorDirIn,  connectorTypeAudio,   {{ 73,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },
    {moduleTypeOscShpB,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 8 Out
    //  9 OscC
    {moduleTypeOscC,        connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 9 OscC PitchVar
    {moduleTypeOscC,        connectorDirIn,  connectorTypeAudio,   {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",         labelLocRight},          // Sync
    {moduleTypeOscC,        connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // FmMod
    {moduleTypeOscC,        connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // Pitch
    {moduleTypeOscC,        connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // Out
    // 10 Unknown
    // 11 Unknown
    // 12 Reverb
    {moduleTypeReverb,      connectorDirIn,  connectorTypeAudio,   {{-12,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "L",            labelLocLeft },          // 12 Reberb InL
    {moduleTypeReverb,      connectorDirIn,  connectorTypeAudio,   {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "R",            labelLocLeft },          // 12 Reberb InR
    {moduleTypeReverb,      connectorDirOut, connectorTypeAudio,   {{-12,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 12 Reberb OutL
    {moduleTypeReverb,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 12 Reberb InL
    // 13 OscString
    {moduleTypeOscString,   connectorDirIn,  connectorTypeAudio,   {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocLeft },          // 13 OscString Audio In
    {moduleTypeOscString,   connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 13 OscString Pitch
    {moduleTypeOscString,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 13 OscString Pitch Var
    {moduleTypeOscString,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 13 OscString Audio Out
    // 14 Unknown
    // 15 Sw8-1
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 30, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft }, // 15 In 1
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 35, -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft }, // 15 In 2
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 45, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocLeft }, // 15 In 3
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 50, -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocLeft }, // 15 In 4
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 60, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocLeft }, // 15 In 5
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 65, -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocLeft }, // 15 In 6
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 75, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocLeft }, // 15 In 7
    {moduleTypeSw8to1,      connectorDirIn,  connectorTypeControl, {{ 80, -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocLeft }, // 15 In 8
    {moduleTypeSw8to1,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft }, // 13 Out
    {moduleTypeSw8to1,      connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Control",      labelLocUp   }, // 13 Control
    // 16 Unknown
    // 17 ValSw1-2
    {moduleTypeValSw1to2,   connectorDirIn,  connectorTypeControl, {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 17 In
    {moduleTypeValSw1to2,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 17 Ctrl
    {moduleTypeValSw1to2,   connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 17 Out1
    {moduleTypeValSw1to2,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 17 Out2
    // 18 X-Fade
    {moduleTypeXtoFade,     connectorDirIn,  connectorTypeControl, {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft },          // 18 In1
    {moduleTypeXtoFade,     connectorDirIn,  connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft },          // 18 In2
    {moduleTypeXtoFade,     connectorDirIn,  connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 18 Mod
    {moduleTypeXtoFade,     connectorDirOut, connectorTypeControl, {{ 92,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 18 Out
    // 19 Mix4-1B
    {moduleTypeMix4to1B,    connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 19 In 1
    {moduleTypeMix4to1B,    connectorDirIn,  connectorTypeControl, {{ 45,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 19 In 2
    {moduleTypeMix4to1B,    connectorDirIn,  connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },          // 19 In 3
    {moduleTypeMix4to1B,    connectorDirIn,  connectorTypeControl, {{ 75,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },          // 19 In 4
    {moduleTypeMix4to1B,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Chain",        labelLocRight},          // 19 Chain
    {moduleTypeMix4to1B,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 19 Out
    // 20 EnvADSR
    {moduleTypeEnvADSR,     connectorDirIn,  connectorTypeControl, {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 20
    {moduleTypeEnvADSR,     connectorDirIn,  connectorTypeLogic,   {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Gate",         labelLocRight},
    {moduleTypeEnvADSR,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",           labelLocRight},
    {moduleTypeEnvADSR,     connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocUp   },
    {moduleTypeEnvADSR,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    // 21 Mux1-8
    {moduleTypeMux1to8,     connectorDirIn,  connectorTypeControl, {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 21 In
    {moduleTypeMux1to8,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 21 Ctrl
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 21 Out1
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 38,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 21 Out2
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 46,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },          // 21 Out3
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 54,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },          // 21 Out4
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 62,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocUp   },          // 21 Out5
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocUp   },          // 21 Out6
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 78,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocUp   },          // 21 Out7
    {moduleTypeMux1to8,     connectorDirOut, connectorTypeControl, {{ 86,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocUp   },          // 21 Out8
    // 22 PartQuant
    {moduleTypePartQuant,   connectorDirIn,  connectorTypeControl, {{ 85,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 22 In
    {moduleTypePartQuant,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 22 Out
    // 23 ModADSR
    {moduleTypeModADSR,     connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Gate",         labelLocRight},          // 169 Gate
    {moduleTypeModADSR,     connectorDirIn,  connectorTypeControl, {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 Attack M
    {moduleTypeModADSR,     connectorDirIn,  connectorTypeControl, {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 Decay M
    {moduleTypeModADSR,     connectorDirIn,  connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 Sustain M
    {moduleTypeModADSR,     connectorDirIn,  connectorTypeControl, {{ 65,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 Release M
    {moduleTypeModADSR,     connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 169 In
    {moduleTypeModADSR,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",           labelLocRight},          // 169 AM
    {moduleTypeModADSR,     connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocUp   },          // 169 Env
    {moduleTypeModADSR,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 169 Out
    // 24 LfoC
    {moduleTypeLfoC,        connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rate",         labelLocRight},
    {moduleTypeLfoC,        connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    // 25 LfoShpA
    {moduleTypeLfoShpA,     connectorDirIn,  connectorTypeControl, {{ 10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Rate
    {moduleTypeLfoShpA,     connectorDirIn,  connectorTypeControl, {{ 17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // RateVar
    {moduleTypeLfoShpA,     connectorDirIn,  connectorTypeControl, {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocUp   },          // Rst
    {moduleTypeLfoShpA,     connectorDirIn,  connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Shape M
    {moduleTypeLfoShpA,     connectorDirIn,  connectorTypeControl, {{ 67,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Phase M
    {moduleTypeLfoShpA,     connectorDirIn,  connectorTypeControl, {{ 10, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Dir",          labelLocUp   },          // Dir
    {moduleTypeLfoShpA,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // Out
    {moduleTypeLfoShpA,     connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Snc",          labelLocUp   },          // Snc
    // 26 LfoA
    {moduleTypeLfoA,        connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Rate
    {moduleTypeLfoA,        connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Rate
    {moduleTypeLfoA,        connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // Out
    // 27 OscMaster
    {moduleTypeOscMaster,   connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 9 Pitch
    {moduleTypeOscMaster,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 9 PitchVar
    {moduleTypeOscMaster,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // Out
    // 28 Saturate
    {moduleTypeSaturate,    connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 28 In
    {moduleTypeSaturate,    connectorDirIn,  connectorTypeControl, {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 28 Mod
    {moduleTypeSaturate,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 28 Out
    // 29 MetNoise
    {moduleTypeMetNoise,    connectorDirIn,  connectorTypeControl, {{ 32,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 29 FreqMod
    {moduleTypeMetNoise,    connectorDirIn,  connectorTypeControl, {{ 62,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 29 ColorMod
    {moduleTypeMetNoise,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 29 Out
    // 30 Device
    {moduleTypeDevice,      connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Wheel",        labelLocUp   },          // 30 Wheel
    {moduleTypeDevice,      connectorDirOut, connectorTypeControl, {{ 18,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AftTouch",     labelLocUp   },          // 30 AfterTouch
    {moduleTypeDevice,      connectorDirOut, connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "CtrlPedal",    labelLocUp   },          // 30 ControlPedal
    {moduleTypeDevice,      connectorDirOut, connectorTypeLogic,   {{ 48,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sustain",      labelLocUp   },          // 30 SustainPedal
    {moduleTypeDevice,      connectorDirOut, connectorTypeControl, {{ 63,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Stick",        labelLocUp   },          // 30 PitchStick
    {moduleTypeDevice,      connectorDirOut, connectorTypeControl, {{ 78,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "GWh1",         labelLocUp   },          // 30 Global Wheel 1
    {moduleTypeDevice,      connectorDirOut, connectorTypeControl, {{ 88,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "GWh2",         labelLocUp   },          // 30 Global Wheel 2
    // 31 Noise
    {moduleTypeNoise,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 31 Out
    // 32 Eq2Band
    {moduleTypeEq2Band,     connectorDirIn,  connectorTypeAudio,   {{ -3, -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 32 In
    {moduleTypeEq2Band,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 32 Out
    // 33 Eq3Band
    {moduleTypeEq3band,     connectorDirIn,  connectorTypeAudio,   {{ -3, -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 33 In
    {moduleTypeEq3band,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 33 Out
    // 34 ShpExp
    {moduleTypeShpExp,      connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 34 In
    {moduleTypeShpExp,      connectorDirIn,  connectorTypeControl, {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 34 Mod
    {moduleTypeShpExp,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 34 Out
    // 35 Driver
    {moduleTypeDriver,      connectorDirIn,  connectorTypeAudio,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 35 In1
    {moduleTypeDriver,      connectorDirIn,  connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 35 In2
    {moduleTypeDriver,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 35 Out
    // 36 SwOnOffM
    {moduleTypeSwOnOffM,    connectorDirIn,  connectorTypeControl, {{-20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 36 In
    {moduleTypeSwOnOffM,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 36 Out
    {moduleTypeSwOnOffM,    connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 36 Ctrl
    // 37 Unknown
    // 38 Pulse
    {moduleTypePulse,       connectorDirIn,  connectorTypeLogic,   {{-21,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 38 In
    {moduleTypePulse,       connectorDirIn,  connectorTypeControl, {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 38 TimeM
    {moduleTypePulse,       connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 38 Out
    // 39 Unknown
    // 40 Mix8-1B
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 21, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 40 In1
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 30, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 40 In2
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 39, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },          // 40 In3
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 48, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },          // 40 In4
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 57, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocUp   },          // 40 In5
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 66, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocUp   },          // 40 In6
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 75, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocUp   },          // 40 In7
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ 84, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocUp   },          // 40 In8
    {moduleTypeMix8to1B,    connectorDirIn,  connectorTypeControl, {{ -3, -28}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Chain",        labelLocLeft },          // 40 Chain
    {moduleTypeMix8to1B,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 40 Out
    // 41 EnvH
    {moduleTypeEnvH,        connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Trig",         labelLocRight},          // 41 Trig
    {moduleTypeEnvH,        connectorDirIn,  connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",           labelLocRight},
    {moduleTypeEnvH,        connectorDirIn,  connectorTypeControl, {{-12,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 41 In
    {moduleTypeEnvH,        connectorDirOut, connectorTypeControl, {{-20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocUp   },          // 41 Env
    {moduleTypeEnvH,        connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 41 Out
    // 42 Delay
    {moduleTypeDelay,       connectorDirIn,  connectorTypeLogic,   {{-21,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 42 In
    {moduleTypeDelay,       connectorDirIn,  connectorTypeControl, {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 42 TimeM
    {moduleTypeDelay,       connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 42 Out
    // 43 Constant
    {moduleTypeConstant,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 43 Out
    // 44 LevMult
    {moduleTypeLevMult,     connectorDirIn,  connectorTypeControl, {{-30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 44 In
    {moduleTypeLevMult,     connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 44 Mod
    {moduleTypeLevMult,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 43 Out
    // 45 FltVoice
    {moduleTypeFltVoice,    connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 45 In
    {moduleTypeFltVoice,    connectorDirIn,  connectorTypeControl, {{ 38,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 45 Vowel
    {moduleTypeFltVoice,    connectorDirIn,  connectorTypeAudio,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 45 FreqMod
    {moduleTypeFltVoice,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 45 Out
    // 46 EnvAHD
    {moduleTypeEnvAHD,      connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Trig",         labelLocRight},          // 46 Trig
    {moduleTypeEnvAHD,      connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",           labelLocRight},          // 46 AM
    {moduleTypeEnvAHD,      connectorDirIn,  connectorTypeControl, {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 46 In
    {moduleTypeEnvAHD,      connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocUp   },          // 46 Env
    {moduleTypeEnvAHD,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 46 Out
    // 47 Pan
    {moduleTypePan,         connectorDirIn,  connectorTypeControl, {{-25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 47 In
    {moduleTypePan,         connectorDirIn,  connectorTypeControl, {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 47 Mod
    {moduleTypePan,         connectorDirOut, connectorTypeControl, {{-15,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "L",            labelLocLeft },          // 47 OutL
    {moduleTypePan,         connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "R",            labelLocLeft },          // 46 OutR
    // 48 MixStereo
    {moduleTypeMixStereo,   connectorDirIn,  connectorTypeControl, {{ 10, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 48 In1
    {moduleTypeMixStereo,   connectorDirIn,  connectorTypeControl, {{ 22, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 48 In2
    {moduleTypeMixStereo,   connectorDirIn,  connectorTypeControl, {{ 34, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 48 In3
    {moduleTypeMixStereo,   connectorDirIn,  connectorTypeControl, {{ 46, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 48 In4
    {moduleTypeMixStereo,   connectorDirIn,  connectorTypeControl, {{ 58, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 48 In5
    {moduleTypeMixStereo,   connectorDirIn,  connectorTypeControl, {{ 70, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 48 In6
    {moduleTypeMixStereo,   connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "L",            labelLocUp   },          // 48 OutL
    {moduleTypeMixStereo,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "R",            labelLocUp   },          // 48 OutR
    // 49 FltMulti
    {moduleTypeFltMulti,    connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 49 In
    {moduleTypeFltMulti,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "LP",           labelLocLeft },
    {moduleTypeFltMulti,    connectorDirOut, connectorTypeAudio,   {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "BP",           labelLocLeft },
    {moduleTypeFltMulti,    connectorDirOut, connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "HP",           labelLocLeft },
    {moduleTypeFltMulti,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // PitchVar
    {moduleTypeFltMulti,    connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Pitch
    // 50 ConstSwT
    {moduleTypeConstSwT,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 50 Out
    // 51 FltNord
    {moduleTypeFltNord,     connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 51 In
    {moduleTypeFltNord,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // Out
    {moduleTypeFltNord,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // PitchVar
    {moduleTypeFltNord,     connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Pitch
    {moduleTypeFltNord,     connectorDirIn,  connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // FMLin
    {moduleTypeFltNord,     connectorDirIn,  connectorTypeControl, {{ 47,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // Res
    // 52 EnvMulti
    {moduleTypeEnvMulti,    connectorDirIn,  connectorTypeLogic,   {{  3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Gate",         labelLocRight},          // 52 Gate
    {moduleTypeEnvMulti,    connectorDirIn,  connectorTypeControl, {{ 17,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "AM",           labelLocRight},          // 52 AM
    {moduleTypeEnvMulti,    connectorDirIn,  connectorTypeControl, {{-10,   7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocRight},          // 52 In
    {moduleTypeEnvMulti,    connectorDirOut, connectorTypeControl, {{ -3,  14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "Env",          labelLocLeft },          // 52 Env
    {moduleTypeEnvMulti,    connectorDirOut, connectorTypeControl, {{ -3,   7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 52 Out
    // 53 SandH
    {moduleTypeSandH,       connectorDirIn,  connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 53 In
    {moduleTypeSandH,       connectorDirIn,  connectorTypeLogic,   {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocLeft },          // 53 Ctrl
    {moduleTypeSandH,       connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 53 Out
    // 54 FltStatic
    {moduleTypeFltStatic,   connectorDirIn,  connectorTypeAudio,   {{ -3,   7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 54 In
    {moduleTypeFltStatic,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 54 Out
    // 55 EnvD
    {moduleTypeEnvD,        connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Trig",         labelLocRight},          // 55 Trig
    {moduleTypeEnvD,        connectorDirIn,  connectorTypeControl, {{ 17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",           labelLocRight},          // 55 AM
    {moduleTypeEnvD,        connectorDirIn,  connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 55 In
    {moduleTypeEnvD,        connectorDirOut, connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocLeft },          // 55 Env
    {moduleTypeEnvD,        connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 55 Out
    // 56 Resonator
    {moduleTypeResonator,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 56 In1
    {moduleTypeResonator,   connectorDirIn,  connectorTypeControl, {{ 10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 56 In2
    {moduleTypeResonator,   connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 56 In3
    {moduleTypeResonator,   connectorDirOut, connectorTypeControl, {{-10, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 56 Out1
    {moduleTypeResonator,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 56 Out2
    // 57 Automate
    // 58 Drumsynth
    {moduleTypeDrumSynth,   connectorDirIn,  connectorTypeLogic,   {{  3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Trig",         labelLocUp   },        // 55 Trig
    {moduleTypeDrumSynth,   connectorDirIn,  connectorTypeControl, {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Pitch",        labelLocUp   },        // 55 Pitch M
    {moduleTypeDrumSynth,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",          labelLocUp   },        // 55 Velocity
    {moduleTypeDrumSynth,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },        // 55 Out
    // 59 CompLev
    {moduleTypeCompLev,     connectorDirIn,  connectorTypeControl, {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "A",            labelLocLeft },        // 55 Pitch M
    {moduleTypeCompLev,     connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "A>=C",         labelLocLeft },        // 55 Out
    // 57
    {moduleTypeAutomate,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "In",           labelLocUp   },        // 57 In
    {moduleTypeAutomate,    connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Out",          labelLocUp   },        // 57 Out
    // 60 Mux8-1X
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },        // 60 In1
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 18,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },        // 60 In2
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 26,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },        // 60 In3
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 34,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },        // 60 In4
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 42,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocUp   },        // 60 In5
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocUp   },        // 60 In6
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocUp   },        // 60 In7
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{ 66,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocUp   },        // 60 In8
    {moduleTypeMux8to1X,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocUp   },        // 60 Ctrl
    {moduleTypeMux8to1X,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },        // 60 Out
    // 61 Clip
    {moduleTypeClip,        connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },        // 61 In
    {moduleTypeClip,        connectorDirIn,  connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },        // 61 Mod
    {moduleTypeClip,        connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },        // 61 Out
    // 62 OverDrive
    {moduleTypeOverdrive,   connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },        // 62 In
    {moduleTypeOverdrive,   connectorDirIn,  connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },        // 62 Mod
    {moduleTypeOverdrive,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },        // 62 Out
    // 63 Scratch
    {moduleTypeScratch,     connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },        // 63 In
    {moduleTypeScratch,     connectorDirIn,  connectorTypeControl, {{  6,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },        // 63 Mod
    {moduleTypeScratch,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },        // 63 Out
    // 64 Gate
    {moduleTypeGate,        connectorDirIn,  connectorTypeLogic,   {{ 35, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},        // 64 In1_1
    {moduleTypeGate,        connectorDirIn,  connectorTypeLogic,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},        // 64 In1_2
    {moduleTypeGate,        connectorDirIn,  connectorTypeLogic,   {{ 65, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},        // 64 In2_1
    {moduleTypeGate,        connectorDirIn,  connectorTypeLogic,   {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},        // 64 In2_1
    {moduleTypeGate,        connectorDirOut, connectorTypeLogic,   {{ 52,  -6}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },        // 64 Out1
    {moduleTypeGate,        connectorDirOut, connectorTypeLogic,   {{ 82,  -6}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },        // 64 Out1
    // 65 Unknown
    // 66 Mix2-1B
    {moduleTypeMix2to1B,    connectorDirIn,  connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 66 In1
    {moduleTypeMix2to1B,    connectorDirIn,  connectorTypeControl, {{ 58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 66 In2
    {moduleTypeMix2to1B,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Chain",        labelLocRight},          // 66 Chain
    {moduleTypeMix2to1B,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 66 Out
    // 67 Unknown
    // 68 ClkGen
    {moduleTypeClkGen,      connectorDirIn,  connectorTypeLogic,   {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Reset",        labelLocRight},          // 68 Reset
    {moduleTypeClkGen,      connectorDirOut, connectorTypeLogic,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "1/96",         labelLocLeft },          // 68 1/96
    {moduleTypeClkGen,      connectorDirOut, connectorTypeLogic,   {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "1/16",         labelLocLeft },          // 68 1/16
    {moduleTypeClkGen,      connectorDirOut, connectorTypeLogic,   {{ -3, -24}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "ClkActive",    labelLocLeft },          // 68 1/96
    {moduleTypeClkGen,      connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Sync",         labelLocLeft },          // 68 1/96
    // 69 ClkDiv
    {moduleTypeClkDiv,      connectorDirIn,  connectorTypeLogic,   {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clock",        labelLocRight},          // 69 Clock
    {moduleTypeClkDiv,      connectorDirIn,  connectorTypeLogic,   {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Reset",        labelLocRight},          // 69 Reset
    {moduleTypeClkDiv,      connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 69 Out
    // 70 Unknown
    // 71 EnvFollow
    {moduleTypeEnvFollow,   connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 71 In
    {moduleTypeEnvFollow,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 71 Out
    // 72 NoteScaler
    {moduleTypeNoteScaler,  connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 72 In
    {moduleTypeNoteScaler,  connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 72 Out
    // 73 Unknown
    // 74 WaveWrap
    {moduleTypeWaveWrap,    connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 74 Mod
    {moduleTypeWaveWrap,    connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 74 In
    {moduleTypeWaveWrap,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 74 Out
    // 75 NoteQuant
    {moduleTypeNoteQuant,   connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 75 In
    {moduleTypeNoteQuant,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 75 Out
    // 76 SwOnOffT
    {moduleTypeSwOnOffT,    connectorDirIn,  connectorTypeControl, {{-20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 76 In
    {moduleTypeSwOnOffT,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 76 Out
    {moduleTypeSwOnOffT,    connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 76 Ctrl
    // 77 Unknown
    // 78 Sw1-8
    {moduleTypeSw1to8,      connectorDirIn,  connectorTypeControl, {{ 15,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 78 In
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 28,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft },          // 78 Out1
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 36,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft },          // 78 Out2
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 44,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocLeft },          // 78 Out3
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 52,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocLeft },          // 78 Out4
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocLeft },          // 78 Out5
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 68,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocLeft },          // 78 Out6
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 76,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocLeft },          // 78 Out7
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{ 84,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocLeft },          // 78 Out8
    {moduleTypeSw1to8,      connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 78 Ctrl
    // 79 Sw4-1
    {moduleTypeSw4to1,      connectorDirIn,  connectorTypeControl, {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft },          // 79 In1
    {moduleTypeSw4to1,      connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft },          // 79 In2
    {moduleTypeSw4to1,      connectorDirIn,  connectorTypeControl, {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocLeft },          // 79 In3
    {moduleTypeSw4to1,      connectorDirIn,  connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocLeft },          // 79 In4
    {moduleTypeSw4to1,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 79 Out
    {moduleTypeSw4to1,      connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 79 Ctrl
    // 80 Unknown
    // 81 LevAmp
    {moduleTypeLevAmp,      connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 81 In
    {moduleTypeLevAmp,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 81 Out
    // 82 Rect
    {moduleTypeRect,        connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 82 In
    {moduleTypeRect,        connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 82 Out
    // 83 ShpStatic
    {moduleTypeShpStatic,   connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 82 In
    {moduleTypeShpStatic,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 82 Out
    // 84 EnvADR
    {moduleTypeEnvADR,      connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 84 Trig/Gate
    {moduleTypeEnvADR,      connectorDirIn,  connectorTypeControl, {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 84 In
    {moduleTypeEnvADR,      connectorDirIn,  connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",           labelLocUp   },          // 84 AM
    {moduleTypeEnvADR,      connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocUp   },          // 84 Env
    {moduleTypeEnvADR,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 84 Out
    {moduleTypeEnvADR,      connectorDirOut, connectorTypeLogic,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 46 End
    // 85 WindSw
    {moduleTypeWindSw,      connectorDirIn,  connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 85 In
    {moduleTypeWindSw,      connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 85 In
    {moduleTypeWindSw,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 85 Out
    {moduleTypeWindSw,      connectorDirOut, connectorTypeLogic,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 85 Gate
    // 86 8Counter
    {moduleType8Counter,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocRight},          // 86 Clock
    {moduleType8Counter,    connectorDirIn,  connectorTypeLogic,   {{ 10, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocRight},          // 86 Reset
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 86 Out1
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 86 Out2
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },          // 86 Out3
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },          // 86 Out4
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocUp   },          // 86 Out5
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocUp   },          // 86 Out6
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocUp   },          // 86 Out7
    {moduleType8Counter,    connectorDirOut, connectorTypeLogic,   {{ 90,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocUp   },          // 86 Out8
    // 87 FltLP
    {moduleTypeFltLP,       connectorDirIn,  connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 87 In
    {moduleTypeFltLP,       connectorDirIn,  connectorTypeControl, {{ 28,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 87 Mod
    {moduleTypeFltLP,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 87 Out
    // 88 Sw1-4
    {moduleTypeSw1to4,      connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 88 In
    {moduleTypeSw1to4,      connectorDirOut, connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft },          // 88 Out1
    {moduleTypeSw1to4,      connectorDirOut, connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft },          // 88 Out2
    {moduleTypeSw1to4,      connectorDirOut, connectorTypeControl, {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocLeft },          // 88 Out3
    {moduleTypeSw1to4,      connectorDirOut, connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocLeft },          // 88 Out4
    {moduleTypeSw1to4,      connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 88 Ctrl
    // 89 Flanger
    {moduleTypeFlanger,     connectorDirIn,  connectorTypeAudio,   {{ -3, -13}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 89 In
    {moduleTypeFlanger,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 89 Out
    // 90 Sw1-2
    {moduleTypeSw1to2,      connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 88 In
    {moduleTypeSw1to2,      connectorDirOut, connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft },          // 88 Out1
    {moduleTypeSw1to2,      connectorDirOut, connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft },          // 88 Out2
    {moduleTypeSw1to2,      connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 88 Ctrl
    // 91 FlipFlop
    {moduleTypeFlipFlop,    connectorDirIn,  connectorTypeLogic,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocLeft },          // 91 Clock
    {moduleTypeFlipFlop,    connectorDirIn,  connectorTypeLogic,   {{ 15,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocLeft },          // 91 Reset
    {moduleTypeFlipFlop,    connectorDirIn,  connectorTypeLogic,   {{ 35, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "In",           labelLocLeft },          // 91 In
    {moduleTypeFlipFlop,    connectorDirOut, connectorTypeLogic,   {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "NotQ",         labelLocRight},          // 91 NotQ
    {moduleTypeFlipFlop,    connectorDirOut, connectorTypeLogic,   {{ 80, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Q",            labelLocRight},          // 91 Q
    // 92 FltClassic
    {moduleTypeFltClassic,  connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 92
    {moduleTypeFltClassic,  connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    {moduleTypeFltClassic,  connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },
    {moduleTypeFltClassic,  connectorDirIn,  connectorTypeControl, {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,           labelLocUp   },
    // 93 Unknown
    // 94 StChorus
    {moduleTypeStChorus,    connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 94
    {moduleTypeStChorus,    connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    {moduleTypeStChorus,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    // 95 Unknown
    // 96 OscD
    {moduleTypeOscD,        connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 96 Pitch
    {moduleTypeOscD,        connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 96 Out
    // 97 OscA
    {moduleTypeOscA,        connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 97 Pitch
    {moduleTypeOscA,        connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 97  PitchVar
    {moduleTypeOscA,        connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 97 Out
    // 98 FreqShift
    {moduleTypeFreqShift,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 98 Mod
    {moduleTypeFreqShift,   connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 98 In
    {moduleTypeFreqShift,   connectorDirOut, connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Down",         labelLocLeft },          //98 Down
    {moduleTypeFreqShift,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Up",           labelLocLeft },          // 98 Up
    // 99 Unknown
    // 100 Sw2-1
    {moduleTypeSw2to1,      connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 100 In1
    {moduleTypeSw2to1,      connectorDirIn,  connectorTypeControl, {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft },          // 100 In2
    {moduleTypeSw2to1,      connectorDirOut, connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 100 Out
    {moduleTypeSw2to1,      connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 100 Ctrl
    // 101 Unknown
    // 102 FltPhase
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 102 In
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 102 PitchVar
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 102 Spr
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 102 FM
    {moduleTypeFltPhase,    connectorDirIn,  connectorTypeControl, {{  3, -12}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 102 Pitch
    {moduleTypeFltPhase,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 102 Out
    // 103 EqPeak
    {moduleTypeEqPeak,      connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 103 In
    {moduleTypeEqPeak,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 103 Out
    // 104 Unknown
    // 105 ValSw2-1
    {moduleTypeValSw2to1,   connectorDirIn,  connectorTypeControl, {{-24,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 105 In1
    {moduleTypeValSw2to1,   connectorDirIn,  connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "On",           labelLocLeft },          // 105 In2
    {moduleTypeValSw2to1,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 105 Ctrl
    {moduleTypeValSw2to1,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 105 Out
    // 106 OscNoise
    {moduleTypeOscNoise,    connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 1106 Pitch
    {moduleTypeOscNoise,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 106  PitchVar
    {moduleTypeOscNoise,    connectorDirIn,  connectorTypeControl, {{ 58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 106  Width
    {moduleTypeOscNoise,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 106 Out
    // 107 Unknown
    // 108 Vocoder
    {moduleTypeVocoder,     connectorDirIn,  connectorTypeAudio,   {{  3, -35}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 108 Ctrl
    {moduleTypeVocoder,     connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 108 In
    {moduleTypeVocoder,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 108 Out
    // 109 Unknown
    // 110 Unknown
    // 111 Unknown
    // 112 LevAdd
    {moduleTypeLevAdd,      connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 112 In
    {moduleTypeLevAdd,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 112 Out
    // 113 Fade1-2
    {moduleTypeFade1to2,    connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 113 In
    {moduleTypeFade1to2,    connectorDirIn,  connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 113 Ctrl
    {moduleTypeFade1to2,    connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 113 Out1
    {moduleTypeFade1to2,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 113 Out2
    // 114 Fade2-1
    {moduleTypeFade2to1,    connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 114 In1
    {moduleTypeFade2to1,    connectorDirIn,  connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 114 In2
    {moduleTypeFade2to1,    connectorDirIn,  connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 114 Ctrl
    {moduleTypeFade2to1,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 114 Out
    // 115 LevScaler
    {moduleTypeLevScaler,   connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Note",         labelLocRight},          // 115 Note
    {moduleTypeLevScaler,   connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 115 In
    {moduleTypeLevScaler,   connectorDirOut, connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Level",        labelLocUp   },          // 115 Level
    {moduleTypeLevScaler,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 115 Out
    // 116 Mix8-1A
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 21,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 116 In1
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 116 In2
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 39,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },          // 116 In3
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 48,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },          // 116 In4
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 57,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocUp   },          // 116 In5
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 66,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocUp   },          // 116 In6
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 75,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocUp   },          // 116 In7
    {moduleTypeMix8to1A,    connectorDirIn,  connectorTypeControl, {{ 84,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocUp   },          // 116 In8
    {moduleTypeMix8to1A,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 116 Out
    // 117 LevMod
    {moduleTypeLevMod,      connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 117 In
    {moduleTypeLevMod,      connectorDirIn,  connectorTypeControl, {{-10, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Mod",          labelLocLeft },          // 117 Mod
    {moduleTypeLevMod,      connectorDirIn,  connectorTypeControl, {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 117 ModDepth
    {moduleTypeLevMod,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 115 Out
    // 118 Digitizer
    {moduleTypeDigitizer,   connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 118 In
    {moduleTypeDigitizer,   connectorDirIn,  connectorTypeControl, {{ 13,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 118 Rate M
    {moduleTypeDigitizer,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 118 Out
    // 119 EnvADDSR
    {moduleTypeEnvADDSR,    connectorDirIn,  connectorTypeLogic,   {{  3,  15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "Gate",         labelLocRight},          // 119 Gate
    {moduleTypeEnvADDSR,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AM",           labelLocRight},          // 119 AM
    {moduleTypeEnvADDSR,    connectorDirIn,  connectorTypeControl, {{-17,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 119 In
    {moduleTypeEnvADDSR,    connectorDirOut, connectorTypeControl, {{ -3,  24}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "Env",          labelLocLeft },          // 119 Env
    {moduleTypeEnvADDSR,    connectorDirOut, connectorTypeControl, {{ -3,  10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 119 Out
    // 120 Unknown
    // 121 SeqNote
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeLogic,   {{  3, -28}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocUp   },          // 121 Clk
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeLogic,   {{  3, -21}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocUp   },          // 121 Rst
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeLogic,   {{  3, -14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Loop",         labelLocUp   },          // 121 Loop
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeLogic,   {{  3,  -7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Prk",          labelLocUp   },          // 121 Park
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeControl, {{ -3, -28}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Note",         labelLocLeft },          // 121 Note
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeLogic,   {{ -3, -21}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Trig",         labelLocLeft },          // 121 Trig
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeControl, {{ -3, -14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "RecV",         labelLocLeft },          // 121 RecVal
    {moduleTypeSeqNote,     connectorDirIn,  connectorTypeLogic,   {{ -3,  -7}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "RecE",         labelLocLeft },          // 121 RecEnable
    {moduleTypeSeqNote,     connectorDirOut, connectorTypeLogic,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Link",         labelLocUp   },          // 121 Link
    {moduleTypeSeqNote,     connectorDirOut, connectorTypeControl, {{ 45,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Note",         labelLocUp   },          // 121 Note
    {moduleTypeSeqNote,     connectorDirOut, connectorTypeLogic,   {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Trig",         labelLocUp   },          // 121 Trig

    // 122 Unknown
    // 123 Mix4-1C
    {moduleTypeMix4to1C,    connectorDirIn,  connectorTypeControl, {{ 12,  20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "1",            labelLocUp   },          // 123 In1
    {moduleTypeMix4to1C,    connectorDirIn,  connectorTypeControl, {{ 32,  20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "2",            labelLocUp   },          // 123 In2
    {moduleTypeMix4to1C,    connectorDirIn,  connectorTypeControl, {{ 52,  20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "3",            labelLocUp   },          // 123 In3
    {moduleTypeMix4to1C,    connectorDirIn,  connectorTypeControl, {{ 72,  20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "4",            labelLocUp   },          // 123 In4
    {moduleTypeMix4to1C,    connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Chain",        labelLocLeft },          // 123 Chain
    {moduleTypeMix4to1C,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 123 Out
    // 124 Mux8-1
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 60 In1
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 18,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 60 In2
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 26,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },          // 60 In3
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 34,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },          // 60 In4
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 42,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocUp   },          // 60 In5
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocUp   },          // 60 In6
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 58,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocUp   },          // 60 In7
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{ 66,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocUp   },          // 60 In8
    {moduleTypeMux8to1,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocUp   },          // 60 Ctrl
    {moduleTypeMux8to1,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 60 Out
    // 125 WahWah
    {moduleTypeWahWah,      connectorDirIn,  connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 112 In
    {moduleTypeWahWah,      connectorDirIn,  connectorTypeControl, {{ 28,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 112 Sweep
    {moduleTypeWahWah,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 112 Out
    // 126 Name
    // 127 Fx-In
    {moduleTypeFxtoIn,      connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 127
    {moduleTypeFxtoIn,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    // 128 MinMax
    {moduleTypeMinMax,      connectorDirIn,  connectorTypeControl, {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "A",            labelLocLeft },          // 128 In A
    {moduleTypeMinMax,      connectorDirIn,  connectorTypeControl, {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "B",            labelLocLeft },          // 128 In B
    {moduleTypeMinMax,      connectorDirOut, connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Min",          labelLocLeft },          // 128 Min Out
    {moduleTypeMinMax,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Max",          labelLocLeft },          // 128 Max Out
    // 128 Unknown
    // 130 BinCounter
    {moduleTypeBinCounter,  connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocRight},          // 130 Clk
    {moduleTypeBinCounter,  connectorDirIn,  connectorTypeLogic,   {{ 17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocRight},          // 130 Rst
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "001",          labelLocUp   },          // 130 Out 001
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 38,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "002",          labelLocUp   },          // 130 Out 002
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 46,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "004",          labelLocUp   },          // 130 Out 004
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 54,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "008",          labelLocUp   },          // 130 Out 008
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 62,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "016",          labelLocUp   },          // 130 Out 016
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "032",          labelLocUp   },          // 130 Out 032
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 78,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "064",          labelLocUp   },          // 130 Out 064
    {moduleTypeBinCounter,  connectorDirOut, connectorTypeLogic,   {{ 86,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "128",          labelLocUp   },          // 130 Out 128
    // 131 ADConv
    {moduleTypeADConv,      connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Input",        labelLocRight},          // 131 Input
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D0",           labelLocUp   },          // 131 Out D0
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 38,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D1",           labelLocUp   },          // 131 Out D1
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 46,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D2",           labelLocUp   },          // 131 Out D2
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 54,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D3",           labelLocUp   },          // 131 Out D3
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 62,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D4",           labelLocUp   },          // 131 Out D4
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D5",           labelLocUp   },          // 131 Out D5
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 78,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D6",           labelLocUp   },          // 131 Out D6
    {moduleTypeADConv,      connectorDirOut, connectorTypeLogic,   {{ 86,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D7",           labelLocUp   },          // 131 Out D7
    // 132 DAConv
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D0",           labelLocUp   },          // 132 In D0
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 38,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D1",           labelLocUp   },          // 132 In D1
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 46,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D2",           labelLocUp   },          // 132 In D2
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 54,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D3",           labelLocUp   },          // 132 In D3
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 62,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D4",           labelLocUp   },          // 132 In D4
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D5",           labelLocUp   },          // 132 In D5
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 78,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D6",           labelLocUp   },          // 132 In D6
    {moduleTypeDAConv,      connectorDirIn,  connectorTypeLogic,   {{ 86,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "D7",           labelLocUp   },          // 132 In D7
    {moduleTypeDAConv,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Out",          labelLocUp   },          // 132 Output
    // 133 Unknown
    // 134 FltHP
    {moduleTypeFltHP,       connectorDirIn,  connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 134 In
    {moduleTypeFltHP,       connectorDirIn,  connectorTypeControl, {{ 28,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 134 Mod
    {moduleTypeFltHP,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 134 Out
    // 135 Unknown
    // 136 Unknown
    // 137 Unknown
    // 138 Unknown
    // 139 T&H
    {moduleTypeTandH,       connectorDirIn,  connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 139 In
    {moduleTypeTandH,       connectorDirIn,  connectorTypeLogic,   {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocLeft },          // 139 Ctrl
    {moduleTypeTandH,       connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 139 Out

    // 140 Mix4-1S
    {moduleTypeMix4to1S,    connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 140 OutL
    {moduleTypeMix4to1S,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 140 OutR
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In1L
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ 10, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In1R
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ 23, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In2L
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ 30, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In2R
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ 43, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In3L
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ 50, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In3R
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ 63, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In4L
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ 70, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 140 In4R
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 140 ChainL
    {moduleTypeMix4to1S,    connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 140 ChainR
    // 141 CtrlSend
    {moduleTypeCtrlSend,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Send",         labelLocRight},          // 141 Send In
    {moduleTypeCtrlSend,    connectorDirOut, connectorTypeLogic,   {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 141 Send Out
    {moduleTypeCtrlSend,    connectorDirIn,  connectorTypeControl, {{ 65,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 141 Value
    // 142 PCSend
    {moduleTypePCSend,      connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Send",         labelLocRight},          // 142 Send In
    {moduleTypePCSend,      connectorDirOut, connectorTypeLogic,   {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 142 Send Out
    {moduleTypePCSend,      connectorDirIn,  connectorTypeControl, {{ 65,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 142 Program
    // 143 NoteSend
    {moduleTypeNoteSend,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Gate",         labelLocRight},          // 143 Gate
    {moduleTypeNoteSend,    connectorDirIn,  connectorTypeControl, {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",          labelLocUp   },          // 143 Velocity
    {moduleTypeNoteSend,    connectorDirIn,  connectorTypeControl, {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Note",         labelLocUp   },          // 143 Note
    // 144 SeqEvent
    {moduleTypeSeqEvent,    connectorDirIn,  connectorTypeLogic,   {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocRight},          // 144 Clk
    {moduleTypeSeqEvent,    connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocRight},          // 144 Rst
    {moduleTypeSeqEvent,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Lp",           labelLocRight},          // 144 Lp
    {moduleTypeSeqEvent,    connectorDirIn,  connectorTypeLogic,   {{ 60, -25}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Park",         labelLocRight},          // 144 Park
    {moduleTypeSeqEvent,    connectorDirIn,  connectorTypeLogic,   {{ 13, -11}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 144 Trig1
    {moduleTypeSeqEvent,    connectorDirIn,  connectorTypeLogic,   {{ 13,  -1}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 144 Trig1
    {moduleTypeSeqEvent,    connectorDirOut, connectorTypeLogic,   {{ -3, -25}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Link",         labelLocLeft },          // 144 Link
    {moduleTypeSeqEvent,    connectorDirOut, connectorTypeLogic,   {{ -3, -11}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 144 Out1
    {moduleTypeSeqEvent,    connectorDirOut, connectorTypeLogic,   {{ -3,  -1}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 144 Out2
    // 145 SeqVal
    {moduleTypeSeqVal,      connectorDirIn,  connectorTypeLogic,   {{  3, -40}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocRight},          // 145 Clk
    {moduleTypeSeqVal,      connectorDirIn,  connectorTypeLogic,   {{  3, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocRight},          // 145 Rst
    {moduleTypeSeqVal,      connectorDirIn,  connectorTypeLogic,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Lp",           labelLocRight},          // 145 Lp
    {moduleTypeSeqVal,      connectorDirIn,  connectorTypeLogic,   {{ 20, -60}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Park",         labelLocRight},          // 145 Park
    {moduleTypeSeqVal,      connectorDirIn,  connectorTypeControl, {{ 13, -40}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 145 Val
    {moduleTypeSeqVal,      connectorDirIn,  connectorTypeLogic,   {{ 13, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 145 Trig1
    {moduleTypeSeqVal,      connectorDirOut, connectorTypeLogic,   {{ -3, -60}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Link",         labelLocLeft },          // 145 Link
    {moduleTypeSeqVal,      connectorDirOut, connectorTypeControl, {{ -3, -35}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 145 Val
    {moduleTypeSeqVal,      connectorDirOut, connectorTypeLogic,   {{ -3,  -5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 145 Trig

    // 146 SeqLev
    {moduleTypeSeqLev,      connectorDirIn,  connectorTypeLogic,   {{  3, -40}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocRight},          // 145 Clk
    {moduleTypeSeqLev,      connectorDirIn,  connectorTypeLogic,   {{  3, -30}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocRight},          // 145 Rst
    {moduleTypeSeqLev,      connectorDirIn,  connectorTypeLogic,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Lp",           labelLocRight},          // 145 Lp
    {moduleTypeSeqLev,      connectorDirIn,  connectorTypeLogic,   {{ 20, -60}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Park",         labelLocRight},          // 145 Park
    {moduleTypeSeqLev,      connectorDirIn,  connectorTypeControl, {{ 13, -40}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 145 Val
    {moduleTypeSeqLev,      connectorDirIn,  connectorTypeLogic,   {{ 13, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 145 Trig1
    {moduleTypeSeqLev,      connectorDirOut, connectorTypeLogic,   {{ -3, -60}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Link",         labelLocLeft },          // 145 Link
    {moduleTypeSeqLev,      connectorDirOut, connectorTypeControl, {{ -3, -35}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 145 Val
    {moduleTypeSeqLev,      connectorDirOut, connectorTypeLogic,   {{ -3,  -5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft },          // 145 Trig

    // 147 CtrRcv
    {moduleTypeCtrlRcv,     connectorDirOut, connectorTypeLogic,   {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rcv",          labelLocUp   },          // 147 Rcv
    {moduleTypeCtrlRcv,     connectorDirOut, connectorTypeControl, {{ 92,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Val",          labelLocUp   },          // 147 Value
    // 148 NoteRcv
    {moduleTypeNoteRcv,     connectorDirOut, connectorTypeLogic,   {{ 74,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rcv",          labelLocUp   },          // 148 Rcv
    {moduleTypeNoteRcv,     connectorDirOut, connectorTypeControl, {{ 81,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",          labelLocUp   },          // 148 Vel
    {moduleTypeNoteRcv,     connectorDirOut, connectorTypeControl, {{ 88,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "RVel",         labelLocUp   },          // 148 Release vel
    // 149 NoteZone
    // 150 Compress
    {moduleTypeCompress,    connectorDirIn,  connectorTypeAudio,   {{-10,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },          // 150
    {moduleTypeCompress,    connectorDirIn,  connectorTypeAudio,   {{ -3,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    NULL,           labelLocUp   },
    {moduleTypeCompress,    connectorDirIn,  connectorTypeAudio,   {{ 20,   5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     NULL,           labelLocUp   },
    {moduleTypeCompress,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    {moduleTypeCompress,    connectorDirOut, connectorTypeAudio,   {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },

    // 151 Unknown
    // 152 KeyQuant
    {moduleTypeKeyQuant,    connectorDirIn,  connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 152 In
    {moduleTypeKeyQuant,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight},          // 152 Out
    // 153 Unknown
    // 154 SeqCtr
    {moduleTypeSeqCtr,      connectorDirIn,  connectorTypeControl, {{  3, -40}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight},          // 154 Ctrl
    {moduleTypeSeqCtr,      connectorDirIn,  connectorTypeControl, {{  3, -12}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Val",          labelLocRight},          // 154 Val
    {moduleTypeSeqCtr,      connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Trig",         labelLocRight},          // 154 Trig
    {moduleTypeSeqCtr,      connectorDirOut, connectorTypeControl, {{ -3, -12}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Val",          labelLocLeft },          // 154 Val
    {moduleTypeSeqCtr,      connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Trig",         labelLocLeft },          // 154 Trig
    // 155 Unknown
    // 156 NoteDet
    {moduleTypeNoteDet,     connectorDirOut, connectorTypeLogic,   {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Gate",         labelLocUp   },          // 156 Gate
    {moduleTypeNoteDet,     connectorDirOut, connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",          labelLocUp   },          // 156 Vel
    {moduleTypeNoteDet,     connectorDirOut, connectorTypeControl, {{ 90,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "RVel",         labelLocUp   },          // 156 Release vel
    // 157 LevConv
    {moduleTypeLevConv,     connectorDirIn,  connectorTypeControl, {{-23,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 157 In
    {moduleTypeLevConv,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 157 Out
    // 158 Glide
    {moduleTypeGlide,       connectorDirIn,  connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 139 In
    {moduleTypeGlide,       connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Glide on",     labelLocRight},          // 158 Glide On
    {moduleTypeGlide,       connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 139 Out
    // 159 CompSig
    {moduleTypeCompSig,     connectorDirIn,  connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "A",            labelLocLeft },          // 159 A
    {moduleTypeCompSig,     connectorDirIn,  connectorTypeControl, {{ 75,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "B",            labelLocLeft },          // 159 B
    {moduleTypeCompSig,     connectorDirOut, connectorTypeLogic,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "A>=B",         labelLocLeft },          // 159 Cmp
    // 160 ZeroCnt
    {moduleTypeZeroCnt,     connectorDirIn,  connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 160 In
    {moduleTypeZeroCnt,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 160 Out
    // 161 MixFader
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{  3,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "1",            labelLocUp   },          // 161 In1
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ 13,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "2",            labelLocUp   },          // 161 In2
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ 23,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "3",            labelLocUp   },          // 161 In3
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ 33,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "4",            labelLocUp   },          // 161 In4
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ 43,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "5",            labelLocUp   },          // 161 In5
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ 53,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "6",            labelLocUp   },          // 161 In6
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ 63,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "7",            labelLocUp   },          // 161 In7
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ 73,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopLeft,     "8",            labelLocUp   },          // 161 In8
    {moduleTypeMixFader,    connectorDirIn,  connectorTypeAudio,   {{ -3,   3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorTopRight,    "Chain",        labelLocLeft },          // 161 Chain
    {moduleTypeMixFader,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 161 Out
    // 162 FltComb
    {moduleTypeFltComb,     connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 162 In
    {moduleTypeFltComb,     connectorDirIn,  connectorTypeControl, {{  3, -12}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 162 Pitch
    {moduleTypeFltComb,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 162 PitchVar
    {moduleTypeFltComb,     connectorDirIn,  connectorTypeControl, {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 162 FM
    {moduleTypeFltComb,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 162 Out
    // 163 OscShpA
    {moduleTypeOscShpA,     connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 163 Pitch
    {moduleTypeOscShpA,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 163 PitchVar
    {moduleTypeOscShpA,     connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",         labelLocUp   },          // 163 Sync
    {moduleTypeOscShpA,     connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // FM
    {moduleTypeOscShpA,     connectorDirIn,  connectorTypeAudio,   {{ 73,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 163 Shape
    {moduleTypeOscShpA,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // Out
    // 164 OscDual
    {moduleTypeOscDual,     connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 164 Pitch
    {moduleTypeOscDual,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 164 PitchVar
    {moduleTypeOscDual,     connectorDirIn,  connectorTypeAudio,   {{  3, -20}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",         labelLocUp   },          // 164 Sync
    {moduleTypeOscDual,     connectorDirIn,  connectorTypeAudio,   {{ 48, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 164 PW
    {moduleTypeOscDual,     connectorDirIn,  connectorTypeAudio,   {{ 48,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 164 Phase
    {moduleTypeOscDual,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 164 Out
    // 165 DXRouter
    {moduleTypeDXRouter,    connectorDirIn,  connectorTypeAudio,   {{  5,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft },          // 164 In1
    {moduleTypeDXRouter,    connectorDirOut, connectorTypeAudio,   {{ 11,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 164 Out1
    {moduleTypeDXRouter,    connectorDirIn,  connectorTypeAudio,   {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft },          // 164 In2
    {moduleTypeDXRouter,    connectorDirOut, connectorTypeAudio,   {{ 26,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 164 Out2
    {moduleTypeDXRouter,    connectorDirIn,  connectorTypeAudio,   {{ 35,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocLeft },          // 164 In3
    {moduleTypeDXRouter,    connectorDirOut, connectorTypeAudio,   {{ 41,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 164 Out3
    {moduleTypeDXRouter,    connectorDirIn,  connectorTypeAudio,   {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocLeft },          // 164 In4
    {moduleTypeDXRouter,    connectorDirOut, connectorTypeAudio,   {{ 56,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 164 Out4
    {moduleTypeDXRouter,    connectorDirIn,  connectorTypeAudio,   {{ 65,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocLeft },          // 164 In5
    {moduleTypeDXRouter,    connectorDirOut, connectorTypeAudio,   {{ 71,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 164 Out5
    {moduleTypeDXRouter,    connectorDirIn,  connectorTypeAudio,   {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocLeft },          // 164 In6
    {moduleTypeDXRouter,    connectorDirOut, connectorTypeAudio,   {{ 86,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 164 Out6
    {moduleTypeDXRouter,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Main",         labelLocUp   },          // 164 Out
    // 166 Unknown
    // 167 PShift
    {moduleTypePShift,      connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 167 PitchVar
    {moduleTypePShift,      connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 167 In
    {moduleTypePShift,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 167 Out
    // 168 Unknown
    // 169 ModAHD
    {moduleTypeModAHD,      connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Trig",         labelLocUp   },          // 169 Trig
    {moduleTypeModAHD,      connectorDirIn,  connectorTypeControl, {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 Attack M
    {moduleTypeModAHD,      connectorDirIn,  connectorTypeControl, {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 Hold M
    {moduleTypeModAHD,      connectorDirIn,  connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 Delay M
    {moduleTypeModAHD,      connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 169 In
    {moduleTypeModAHD,      connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 169 AM
    {moduleTypeModAHD,      connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocLeft },          // 169 Env
    {moduleTypeModAHD,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 169 Out
    // 170 2-In
    {moduleType2toIn,       connectorDirOut, connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "L",            labelLocUp   },          // 170 OutL
    {moduleType2toIn,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "R",            labelLocUp   },          // 170 OutR
    // 171 4-In
    {moduleType4toIn,       connectorDirOut, connectorTypeAudio,   {{-45,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "1",            labelLocUp   },          // 170 Out1
    {moduleType4toIn,       connectorDirOut, connectorTypeAudio,   {{-31,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "2",            labelLocUp   },          // 170 Out2
    {moduleType4toIn,       connectorDirOut, connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "3",            labelLocUp   },          // 170 Out3
    {moduleType4toIn,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "4",            labelLocUp   },          // 170 Out4
    // 172 DlySingleA
    {moduleTypeDlySingleA,  connectorDirIn,  connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 172 In
    {moduleTypeDlySingleA,  connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 172 Out
    // 173 DlySingleB
    {moduleTypeDlySingleB,  connectorDirIn,  connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 173 In
    {moduleTypeDlySingleB,  connectorDirIn,  connectorTypeAudio,   {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 173 Mod
    {moduleTypeDlySingleB,  connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 173 Out
    // 174 DelayDual
    {moduleTypeDelayDual,   connectorDirIn,  connectorTypeAudio,   {{-25, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "In",           labelLocRight},          // 174 In
    {moduleTypeDelayDual,   connectorDirIn,  connectorTypeAudio,   {{ 33,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 174 Time Mod 1
    {moduleTypeDelayDual,   connectorDirIn,  connectorTypeAudio,   {{ 68,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 174 Time Mod 2
    {moduleTypeDelayDual,   connectorDirOut, connectorTypeAudio,   {{ 53,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 174 Out1
    {moduleTypeDelayDual,   connectorDirOut, connectorTypeAudio,   {{ 88,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 174 Out2
    // 175 DelayQuad
    {moduleTypeDelayQuad,   connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "In",           labelLocLeft },          // 174 In
    {moduleTypeDelayQuad,   connectorDirIn,  connectorTypeAudio,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 175 Time1
    {moduleTypeDelayQuad,   connectorDirIn,  connectorTypeAudio,   {{ 23,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 175 Time2
    {moduleTypeDelayQuad,   connectorDirIn,  connectorTypeAudio,   {{ 43,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 175 Time3
    {moduleTypeDelayQuad,   connectorDirIn,  connectorTypeAudio,   {{ 63,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 175 Time4
    {moduleTypeDelayQuad,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Main",         labelLocUp   },          // 175 Out Main
    {moduleTypeDelayQuad,   connectorDirOut, connectorTypeAudio,   {{ 13, -14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 175 Out1
    {moduleTypeDelayQuad,   connectorDirOut, connectorTypeAudio,   {{ 33, -14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 175 Out2
    {moduleTypeDelayQuad,   connectorDirOut, connectorTypeAudio,   {{ 53, -14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 175 Out3
    {moduleTypeDelayQuad,   connectorDirOut, connectorTypeAudio,   {{ 73, -14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 175 Out4
    // 176 DelayA
    {moduleTypeDelayA,      connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 177
    {moduleTypeDelayA,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    // 177 DelayB
    {moduleTypeDelayB,      connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 177
    {moduleTypeDelayB,      connectorDirIn,  connectorTypeControl, {{ 43,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 169 E
    {moduleTypeDelayB,      connectorDirIn,  connectorTypeControl, {{ 68,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft },          // 169 E
    {moduleTypeDelayB,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },
    // 178 DlyClock
    {moduleTypeDlyClock,    connectorDirIn,  connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft }, // 178 In
    {moduleTypeDlyClock,    connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocRight}, // 178 Clk
    {moduleTypeDlyClock,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // 178 Out
    // 179 DlyShiftReg
    {moduleTypeDlyShiftReg, connectorDirIn,  connectorTypeControl, {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft }, // 179 In
    {moduleTypeDlyShiftReg, connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocRight}, // 179 Clk
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 28,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   }, // 179 Out1
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 36,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   }, // 179 Out2
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 44,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   }, // 179 Out3
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 52,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   }, // 179 Out4
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "5",            labelLocUp   }, // 179 Out5
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 68,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "6",            labelLocUp   }, // 179 Out6
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 76,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "7",            labelLocUp   }, // 179 Out7
    {moduleTypeDlyShiftReg, connectorDirOut, connectorTypeControl, {{ 84,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "8",            labelLocUp   }, // 179 Out8
    // 180 Operator
    {moduleTypeOperator,    connectorDirIn,  connectorTypeControl, {{  3, -45}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Freq",         labelLocUp   }, // 180 Freq
    {moduleTypeOperator,    connectorDirIn,  connectorTypeAudio,   {{ 90, -60}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "FM",           labelLocUp   }, // 180 FM
    {moduleTypeOperator,    connectorDirIn,  connectorTypeLogic,   {{  3, -35}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Gate",         labelLocUp   }, // 180 Freq
    {moduleTypeOperator,    connectorDirIn,  connectorTypeControl, {{  3, -25}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Note",         labelLocUp   }, // 180 Note
    {moduleTypeOperator,    connectorDirIn,  connectorTypeControl, {{  3, -15}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "AMod",         labelLocUp   }, // 180 Amod
    {moduleTypeOperator,    connectorDirIn,  connectorTypeControl, {{  3,  -5}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Vel",          labelLocUp   }, // 180 Vel
    {moduleTypeOperator,    connectorDirIn,  connectorTypeControl, {{  3, -60}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Pitch",        labelLocUp   }, // 180 Pitch
    {moduleTypeOperator,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // 180 Out
    // 181 DlyEight
    {moduleTypeDlyEight,    connectorDirIn,  connectorTypeAudio,   {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // 181 In
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out1
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out2
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out3
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out4
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out5
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out6
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out7
    {moduleTypeDlyEight,    connectorDirOut, connectorTypeAudio,   {{ 90,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 181 Out8
    // 182 DlyStereo
    {moduleTypeDlyStereo,   connectorDirIn,  connectorTypeAudio,   {{ -3, -24}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // 181 In
    {moduleTypeDlyStereo,   connectorDirOut, connectorTypeAudio,   {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "L",            labelLocLeft }, // 181 Out1
    {moduleTypeDlyStereo,   connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "R",            labelLocLeft }, // 181 Out2
    // 183 OscPM
    {moduleTypeOscPM,       connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight}, // 9 OscC PitchVar
    {moduleTypeOscPM,       connectorDirIn,  connectorTypeAudio,   {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Sync",         labelLocRight}, // Sync
    {moduleTypeOscPM,       connectorDirIn,  connectorTypeAudio,   {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight}, // Phase M
    {moduleTypeOscPM,       connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight}, // Pitch
    {moduleTypeOscPM,       connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // Out
    // 184 Mix1-1A
    {moduleTypeMix1to1A,    connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight}, // 184 In
    {moduleTypeMix1to1A,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Chain",        labelLocRight}, // 184 Chain
    {moduleTypeMix1to1A,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocRight}, // 184 Out
    // 185 Mix1-1S
    {moduleTypeMix1to1S,    connectorDirIn,  connectorTypeControl, {{-24,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "L",            labelLocLeft }, // 185 InL
    {moduleTypeMix1to1S,    connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "R",            labelLocLeft }, // 185 InR
    {moduleTypeMix1to1S,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "L",            labelLocRight}, // 185 Chain L
    {moduleTypeMix1to1S,    connectorDirIn,  connectorTypeControl, {{ 10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "R Chain",      labelLocRight}, // 185 Chain R
    {moduleTypeMix1to1S,    connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "L",            labelLocLeft }, // 185 Out L
    {moduleTypeMix1to1S,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "R",            labelLocLeft }, // 185 Out R
    // 186 Sw1-2M
    {moduleTypeSw1to2M,     connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 186 In
    {moduleTypeSw1to2M,     connectorDirOut, connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocLeft }, // 186 Out1
    {moduleTypeSw1to2M,     connectorDirOut, connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft }, // 186 Out2
    {moduleTypeSw1to2M,     connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight}, // 186 Ctrl
    // 187 Sw2-1M
    {moduleTypeSw2to1M,     connectorDirIn,  connectorTypeControl, {{ 30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   }, // 187 InOff
    {moduleTypeSw2to1M,     connectorDirIn,  connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocLeft }, // 187 InOn
    {moduleTypeSw2to1M,     connectorDirOut, connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocLeft }, // 187 Out
    {moduleTypeSw2to1M,     connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Ctrl",         labelLocRight}, // 187 Ctrl
    // 188 ConstSwM
    {moduleTypeConstSwM,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocLeft }, // 188 Out
    // 189 NoiseGate
    {moduleTypeNoiseGate,   connectorDirIn,  connectorTypeControl, {{ -3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // 189 In
    {moduleTypeNoiseGate,   connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // 189 In
    {moduleTypeNoiseGate,   connectorDirOut, connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Env",          labelLocLeft }, // 189 Env
    // 190 LfoB
    {moduleTypeLfoB,        connectorDirIn,  connectorTypeControl, {{ 15,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rate",         labelLocUp   }, // 190 Rate
    {moduleTypeLfoB,        connectorDirIn,  connectorTypeControl, {{ 22,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 190 Ratevar
    {moduleTypeLfoB,        connectorDirIn,  connectorTypeControl, {{  3, -17}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocUp   }, // 190 Reset
    {moduleTypeLfoB,        connectorDirIn,  connectorTypeControl, {{ 60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rate",         labelLocUp   }, // 190 PhaseM
    {moduleTypeLfoB,        connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   }, // 190 Out
    {moduleTypeLfoB,        connectorDirOut, connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   }, // 190 Sync
    // 191 Unknown
    // 192 Phaser
    {moduleTypePhaser,      connectorDirIn,  connectorTypeAudio,   {{-18,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 192 In
    {moduleTypePhaser,      connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 192 Out
    // 193 Mix4-1A
    {moduleTypeMix4to1A,    connectorDirIn,  connectorTypeControl, {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 193 In1
    {moduleTypeMix4to1A,    connectorDirIn,  connectorTypeControl, {{ 40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 193 In2
    {moduleTypeMix4to1A,    connectorDirIn,  connectorTypeControl, {{ 55,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "3",            labelLocUp   },          // 193 In3
    {moduleTypeMix4to1A,    connectorDirIn,  connectorTypeControl, {{ 70,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "4",            labelLocUp   },          // 193 In4
    {moduleTypeMix4to1A,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 193 Out
    // 194 Mix2-1A
    {moduleTypeMix2to1A,    connectorDirIn,  connectorTypeControl, {{ 50,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "1",            labelLocUp   },          // 194 In1
    {moduleTypeMix2to1A,    connectorDirIn,  connectorTypeControl, {{ 75,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "2",            labelLocUp   },          // 194 In2
    {moduleTypeMix2to1A,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Chain",        labelLocRight},          // 185 Chain L
    {moduleTypeMix2to1A,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 194 Out
    // 195 ModAmt
    {moduleTypeModAmt,      connectorDirIn,  connectorTypeControl, {{-10,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 194 In1
    {moduleTypeModAmt,      connectorDirIn,  connectorTypeControl, {{-17,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Mod",          labelLocLeft },          // 194 In2
    {moduleTypeModAmt,      connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 194 Out

    // 196 OscPerc
    {moduleTypeOscPerc,     connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 196 Pitch
    {moduleTypeOscPerc,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 196  PitchVar
    {moduleTypeOscPerc,     connectorDirIn,  connectorTypeAudio,   {{ 25,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Trig",         labelLocUp   },          // 196 Trig
    {moduleTypeOscPerc,     connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 196 Out

    // 197 Status
    {moduleTypeStatus,      connectorDirOut, connectorTypeLogic,   {{-60,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Patch Active", labelLocUp   },          // 197 Patch Active
    {moduleTypeStatus,      connectorDirOut, connectorTypeLogic,   {{-40,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Var. Active",  labelLocUp   },          // 197 Var Active
    {moduleTypeStatus,      connectorDirOut, connectorTypeControl, {{-20,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Voice No.",    labelLocUp   },          // 197 Voice No.
    // 198 PitchTrack
    {moduleTypePitchTrack,  connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Input",        labelLocRight},          // 198 Input
    {moduleTypePitchTrack,  connectorDirOut, connectorTypeLogic,   {{-30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Period",       labelLocUp   },          // 198 Period
    {moduleTypePitchTrack,  connectorDirOut, connectorTypeControl, {{-19,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Gate",         labelLocUp   },          // 198 Gate
    {moduleTypePitchTrack,  connectorDirOut, connectorTypeLogic,   {{ -8,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Pitch",        labelLocUp   },          // 198 Pitch
    // 199 MonoKey
    {moduleTypeMonoKey,     connectorDirOut, connectorTypeControl, {{-30,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Pitch",        labelLocUp   },          // 199 Pitch
    {moduleTypeMonoKey,     connectorDirOut, connectorTypeLogic,   {{-19,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Gate",         labelLocUp   },          // 198 Gate
    {moduleTypeMonoKey,     connectorDirOut, connectorTypeControl, {{ -8,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Vel",          labelLocUp   },          // 199 Vel
    // 200 RandomA
    {moduleTypeRandomA,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 200 Pitch
    {moduleTypeRandomA,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 200 Vel
    // 201 Red2Blue -- ???
    {moduleTypeRed2Blue,    connectorDirIn,  connectorTypeAudio,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 201 In
    {moduleTypeRed2Blue,    connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 201 Out
    // 202 RandomB
    {moduleTypeRandomB,     connectorDirIn,  connectorTypeControl, {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 200 Rate
    {moduleTypeRandomB,     connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocRight},          // 202 RateVar
    {moduleTypeRandomB,     connectorDirOut, connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 202 Out
    // 203 Blue2Red -- ???
    {moduleTypeBlue2Red,    connectorDirIn,  connectorTypeControl, {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 203 In
    {moduleTypeBlue2Red,    connectorDirOut, connectorTypeAudio,   {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, NULL,           labelLocUp   },          // 203 Out
    // 204 RndClkA
    {moduleTypeRndClkA,     connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocUp   },          // 204 Clk
    {moduleTypeRndClkA,     connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocUp   },          // 204 Rst
    {moduleTypeRndClkA,     connectorDirIn,  connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Seed",         labelLocLeft },          // 204 Seed
    {moduleTypeRndClkA,     connectorDirOut, connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 204 Out
    // 205 RndTrig
    {moduleTypeRndTrig,     connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocUp   },          // 205 Clk
    {moduleTypeRndTrig,     connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocUp   },          // 205 Rst
    {moduleTypeRndTrig,     connectorDirIn,  connectorTypeControl, {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Seed",         labelLocLeft },          // 205 Seed
    {moduleTypeRndTrig,     connectorDirIn,  connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Prob",         labelLocLeft },          // 205 Prob
    {moduleTypeRndTrig,     connectorDirOut, connectorTypeLogic,   {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 205 Out
    // 206 RndClkB
    {moduleTypeRndClkB,     connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocUp   },          // 206 Clk
    {moduleTypeRndClkB,     connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocUp   },          // 206 Rst
    {moduleTypeRndClkB,     connectorDirIn,  connectorTypeControl, {{ -3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Seed",         labelLocLeft },          // 206 Seed
    {moduleTypeRndClkB,     connectorDirIn,  connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Step",         labelLocLeft },          // 206 Step input
    {moduleTypeRndClkB,     connectorDirOut, connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 206 Out
    // 207 Unknown
    // 208 RndPattern
    {moduleTypeRndPattern,  connectorDirIn,  connectorTypeLogic,   {{  3, -10}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Clk",          labelLocUp   },          // 208 Clk
    {moduleTypeRndPattern,  connectorDirIn,  connectorTypeLogic,   {{  3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  "Rst",          labelLocUp   },          // 208 Rst
    {moduleTypeRndPattern,  connectorDirIn,  connectorTypeControl, {{ -3, -21}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "A",            labelLocLeft },          // 208 A
    {moduleTypeRndPattern,  connectorDirIn,  connectorTypeControl, {{ -3, -14}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "B",            labelLocLeft },          // 208 B
    {moduleTypeRndPattern,  connectorDirIn,  connectorTypeControl, {{ -3,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomRight, "Step",         labelLocLeft },          // 208 Step input
    {moduleTypeRndPattern,  connectorDirOut, connectorTypeControl, {{ 80,  -3}, {CONNECTOR_SIZE, CONNECTOR_SIZE}}, anchorBottomLeft,  NULL,           labelLocUp   },          // 208 Out
};

// moduleType, paramType, offsetX, offsetY, label, range, defaultValue, string map
const tModeLocation      modeLocationList[] = {
    {moduleTypeOscShpB,    paramType1StandardToggle, paramType2Menu,   {{ 20,   5}, { 7, 7}}, anchorTopLeft,     "Wave",  8, 0, oscShpBStrMap       }, // 8 OscShpB
    {moduleTypeOscC,       paramType1StandardToggle, paramType2Menu,   {{-30,   5}, { 7, 7}}, anchorTopRight,    "Wave",  6, 0, shapeOscATypeStrMap }, // 9 Wave
    {moduleTypeReverb,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, {14, 7}}, anchorBottomLeft,  "Type",  4, 0, reverbTypeStrMap    }, // 12 Reverb
    {moduleTypeLfoC,       paramType1StandardToggle, paramType2Menu,   {{ 64,  -3}, { 7, 7}}, anchorBottomLeft,  "Wave",  6, 0, lfoWaveStrMap       }, // 24 LfoC
    {moduleTypeDriver,     paramType1StandardToggle, paramType2Menu,   {{ 35,  -3}, { 7, 7}}, anchorBottomLeft,  "Type",  4, 0, driverTypeStrMap    }, // 35 Type
    {moduleTypePulse,      paramType1StandardToggle, paramType2Menu,   {{-13,  -3}, { 7, 7}}, anchorBottomRight, "Mode",  2, 0, pulseModeStrMap     }, // 38 Mode
    {moduleTypeDelay,      paramType1StandardToggle, paramType2Menu,   {{-13,  -3}, { 7, 7}}, anchorBottomRight, "Mode",  3, 0, logicDelayModeStrMap}, // 42 Mode
    {moduleTypeGate,       paramType1StandardToggle, paramType2Menu,   {{ 40,  -3}, { 7, 7}}, anchorBottomLeft,  "G1",    6, 0, gateTypeStrMap      }, // 64 Gate 1 Type
    {moduleTypeGate,       paramType1StandardToggle, paramType2Menu,   {{ 70,  -3}, { 7, 7}}, anchorBottomLeft,  "G2",    6, 0, gateTypeStrMap      }, // 64 Gate 2 Type
    {moduleTypeClkDiv,     paramType1StandardToggle, paramType2Menu,   {{ 80,  -3}, { 7, 7}}, anchorBottomLeft,  "Mode",  2, 0, divModeStrMap       }, // 69 DivMode
    {moduleTypeFltLP,      paramType1StandardToggle, paramType2Menu,   {{ 67,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 6, 2, fltLPSlopeStrMap    }, // 87 FltLP
    {moduleTypeFlipFlop,   paramType1StandardToggle, paramType2Menu,   {{ 45,  -3}, { 7, 7}}, anchorBottomLeft,  "Type",  2, 0, flipFlopStrMap      }, // 91 Selector
    {moduleTypeOscD,       paramType1StandardToggle, paramType2Menu,   {{-30,   5}, { 7, 7}}, anchorTopRight,    "Wave",  6, 0, shapeOscATypeStrMap }, // 96 Wave
    {moduleTypeFltHP,      paramType1StandardToggle, paramType2Menu,   {{ 67,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 6, 2, fltLPSlopeStrMap    }, // 134 FltLP
    {moduleTypeDlySingleA, paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 7, 0, delayRangeStrMap    }, // 172 Delay Range
    {moduleTypeDlySingleB, paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 7, 0, delayRangeStrMap    }, // 173 Delay Range
    {moduleTypeDelayDual,  paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 7, 0, delayRangeStrMap    }, // 174 Delay Range
    {moduleTypeDelayQuad,  paramType1StandardToggle, paramType2Menu,   {{ 80,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 7, 0, delayRangeStrMap    }, // 175 Delay Range
    {moduleTypeDelayA,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 4, 0, delayABRangeStrMap  }, // 176 Delay Range
    {moduleTypeDelayB,     paramType1StandardToggle, paramType2Menu,   {{  3,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 4, 0, delayABRangeStrMap  }, // 177 Delay Range
    {moduleTypeDlyStereo,  paramType1StandardToggle, paramType2Menu,   {{ 10,  -3}, { 7, 7}}, anchorBottomLeft,  "Slope", 3, 0, dlyStereoRangeStrMap}, // 182 Delay Range
    {moduleTypeDlyEight,   paramType1StandardToggle, paramType2Menu,   {{ 10, -10}, { 7, 7}}, anchorBottomLeft,  "Slope", 7, 0, delayRangeStrMap    }, // 181 Delay Range
    {moduleTypeOscPM,      paramType1StandardToggle, paramType2Menu,   {{-30,   5}, { 7, 7}}, anchorTopRight,    "Wave",  2, 0, oscPmWaveStrMap     }, // 183 Wave
    {moduleTypeRndClkB,    paramType1StandardToggle, paramType2Menu,   {{  3,   5}, { 7, 7}}, anchorTopLeft,     "Char",  2, 0, rndClkBCharStrMap   }, // 206 Character
    {moduleTypeRndPattern, paramType1StandardToggle, paramType2Toggle, {{  3,   5}, { 7, 7}}, anchorTopLeft,     "Wave",  2, 0, NULL                }, // 208 Wave
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


const tVolumeLocation    volumeLocationList[] = {
    {moduleTypeMix4to1C,  volumeTypeMono,     {{ -4,  6}, {3, 15}}, anchorTopRight},
    {moduleTypeCompress,  volumeTypeCompress, {{-18,  6}, {4, 38}}, anchorTopRight},
    {moduleType2toOut,    volumeTypeStereo,   {{-25,  6}, {3, 10}}, anchorTopRight},
    {moduleType4toOut,    volumeTypeQuad,     {{-40,  6}, {3, 10}}, anchorTopRight},
    {moduleTypeFxtoIn,    volumeTypeStereo,   {{-25,  6}, {3, 10}}, anchorTopRight},
    {moduleTypeMix4to1S,  volumeTypeStereo,   {{-10, 13}, {3, 15}}, anchorTopRight},
    {moduleTypeMix4to1B,  volumeTypeMono,     {{-10,  6}, {3, 10}}, anchorTopRight}, // XPos:231 YPos:10 H:2
    {moduleTypeMix8to1A,  volumeTypeMono,     {{-10,  6}, {3, 10}}, anchorTopRight}, // XPos:231 YPos:10 H:2
    {moduleTypeMix8to1B,  volumeTypeMono,     {{-15,  6}, {3, 25}}, anchorTopRight}, // XPos:242 YPos:27 H:4
    {moduleTypeMixStereo, volumeTypeStereo,   {{-17, 22}, {3, 15}}, anchorTopRight}, // XPos:207+231 YPos:55 H:5
    {moduleTypeMixFader,  volumeTypeMono,     {{ -6, 23}, {3, 55}}, anchorTopRight}, // XPos:242 YPos:102 H:9
    {moduleTypeEq2Band,   volumeTypeMono,     {{-11, 17}, {3, 18}}, anchorTopRight}, // XPos:227 YPos:25 H:3
    {moduleTypeEq3band,   volumeTypeMono,     {{-11, 20}, {3, 22}}, anchorTopRight}, // XPos:227 YPos:40 H:4
    {moduleTypeEqPeak,    volumeTypeMono,     {{-11, 20}, {3, 22}}, anchorTopRight}, // XPos:227 YPos:40 H:4
    {moduleTypeFltVoice,  volumeTypeMono,     {{-10, 20}, {3, 22}}, anchorTopRight}, // XPos:230 YPos:40 H:4
    {moduleTypeFltPhase,  volumeTypeMono,     {{-11, 22}, {3, 18}}, anchorTopRight}, // XPos:227 YPos:55 H:5
    {moduleTypeFltComb,   volumeTypeMono,     {{-11, 20}, {3, 22}}, anchorTopRight}, // XPos:227 YPos:40 H:4
    {moduleType2toIn,     volumeTypeStereo,   {{-25,  6}, {3, 10}}, anchorTopRight}, // XPos:205+231 YPos:10 H:2
    {moduleType4toIn,     volumeTypeQuad,     {{-40,  6}, {3, 10}}, anchorTopRight}, // XPos:153..231 YPos:10 H:2
    {moduleTypeSeqEvent,     volumeTypeSequencer,     {{20,  6}, {60, 5}}, anchorTopLeft}, // XPos:153..231 YPos:10 H:2
    {moduleTypeSeqNote,     volumeTypeSequencer,     {{20,  6}, {60, 5}}, anchorTopLeft}, // XPos:153..231 YPos:10 H:2
    {moduleTypeSeqVal,     volumeTypeSequencer,     {{20,  6}, {60, 5}}, anchorTopLeft}, // XPos:153..231 YPos:10 H:2
    {moduleTypeSeqLev,     volumeTypeSequencer,     {{20,  6}, {60, 5}}, anchorTopLeft}, // XPos:153..231 YPos:10 H:2
    {moduleTypeSeqCtr,     volumeTypeSequencer,     {{20,  6}, {60, 5}}, anchorTopLeft}, // XPos:153..231 YPos:10 H:2
};

const tLedLocation       ledLocationList[] = {
    {moduleTypeInvert,     ledTypeYes, {{-40, 10}, {3, 3}}, anchorTopRight},
    {moduleTypeInvert,     ledTypeYes, {{-10, 10}, {3, 3}}, anchorTopRight},
    {moduleTypeEnvADSR,    ledTypeYes, {{  3,  8}, {3, 3}}, anchorTopLeft },
    {moduleTypeGate,       ledTypeYes, {{-40, 11}, {3, 3}}, anchorTopRight},  // XPos:137 YPos:11 H:2
    {moduleTypeGate,       ledTypeYes, {{-10, 11}, {3, 3}}, anchorTopRight},  // XPos:230 YPos:11 H:2
    {moduleTypeFlipFlop,   ledTypeYes, {{-18, 17}, {3, 3}}, anchorTopRight},  // XPos:204 YPos:17 H:2
    {moduleTypeFlipFlop,   ledTypeYes, {{-10,  7}, {3, 3}}, anchorTopRight},  // XPos:230 YPos:7  H:2
    {moduleTypeValSw1to2,  ledTypeYes, {{-37, 17}, {3, 3}}, anchorTopRight},  // XPos:146 YPos:17 H:2
    {moduleTypeValSw2to1,  ledTypeYes, {{-37, 17}, {3, 3}}, anchorTopRight},  // XPos:146 YPos:17 H:2
    {moduleTypeLfoC,       ledTypeYes, {{-11,  4}, {3, 3}}, anchorTopRight},  // XPos:226 YPos:4  H:2
    {moduleTypeLfoA,       ledTypeYes, {{ -6, 14}, {3, 3}}, anchorTopRight},  // XPos:242 YPos:21 H:3
    {moduleTypeLfoShpA,    ledTypeYes, {{ -6, 20}, {3, 3}}, anchorTopRight},  // XPos:242 YPos:51 H:5
    {moduleTypeLfoB,       ledTypeYes, {{ -6, 18}, {3, 3}}, anchorTopRight},  // XPos:242 YPos:36 H:4
    {moduleTypeModADSR,    ledTypeYes, {{  3, 13}, {3, 3}}, anchorTopLeft },  // XPos:6   YPos:33 H:5
    {moduleTypeModAHD,     ledTypeYes, {{  3,  8}, {3, 3}}, anchorTopLeft },  // XPos:6   YPos:19 H:5
    {moduleTypeEnvAHD,     ledTypeYes, {{  3,  9}, {3, 3}}, anchorTopLeft },  // XPos:6   YPos:18 H:4
    {moduleTypeEnvH,       ledTypeYes, {{ 20, 18}, {3, 3}}, anchorTopLeft },  // XPos:40  YPos:18 H:2
    {moduleTypeEnvD,       ledTypeYes, {{ 20, 18}, {3, 3}}, anchorTopLeft },  // XPos:40  YPos:18 H:2
    {moduleTypeEnvADR,     ledTypeYes, {{  3, 12}, {3, 3}}, anchorTopLeft },  // XPos:6   YPos:18 H:3
    {moduleTypeEnvMulti,   ledTypeYes, {{  3,  6}, {3, 3}}, anchorTopLeft },  // XPos:6   YPos:18 H:6
    {moduleTypeEnvADDSR,   ledTypeYes, {{  3,  8}, {3, 3}}, anchorTopLeft },  // XPos:6   YPos:19 H:5
    {moduleTypePulse,      ledTypeYes, {{ -6,  4}, {3, 3}}, anchorTopRight},  // XPos:242 YPos:4  H:2
    {moduleTypeDelay,      ledTypeYes, {{ -6,  4}, {3, 3}}, anchorTopRight},  // XPos:242 YPos:4  H:2
    {moduleTypeDrumSynth,  ledTypeYes, {{  3, 10}, {3, 3}}, anchorTopLeft },  // XPos:6   YPos:39 H:8
    {moduleTypeWindSw,     ledTypeYes, {{-20, 15}, {3, 3}}, anchorTopRight},  // XPos:198 YPos:15 H:2
    {moduleTypeNoteDet,    ledTypeYes, {{-24, 15}, {3, 3}}, anchorTopRight},  // XPos:186 YPos:15 H:2
    {moduleTypeNoiseGate,  ledTypeYes, {{-15, 21}, {3, 3}}, anchorTopRight},  // XPos:213 YPos:32 H:3
    {moduleTypePitchTrack, ledTypeYes, {{-16, 15}, {3, 3}}, anchorTopRight},  // XPos:212 YPos:15 H:2
    {moduleTypeRandomA,    ledTypeYes, {{-11,  4}, {3, 3}}, anchorTopRight},  // XPos:226 YPos:4  H:2
    {moduleTypeRandomB,    ledTypeYes, {{ -6, 14}, {3, 3}}, anchorTopRight},  // XPos:242 YPos:21 H:3
    {moduleTypeOperator,   ledTypeYes, {{  8, 13}, {3, 3}}, anchorTopLeft },  // XPos:17  YPos:77 H:12
    // Mux1to8 (ID 21) — 8 LEDs across the upper-right strip
    {moduleTypeMux1to8,    ledTypeYes, {{-51,  3}, {3, 3}}, anchorTopRight},  // XPos:102
    {moduleTypeMux1to8,    ledTypeYes, {{-45,  3}, {3, 3}}, anchorTopRight},  // XPos:122
    {moduleTypeMux1to8,    ledTypeYes, {{-38,  3}, {3, 3}}, anchorTopRight},  // XPos:142
    {moduleTypeMux1to8,    ledTypeYes, {{-32,  3}, {3, 3}}, anchorTopRight},  // XPos:162
    {moduleTypeMux1to8,    ledTypeYes, {{-25,  3}, {3, 3}}, anchorTopRight},  // XPos:182
    {moduleTypeMux1to8,    ledTypeYes, {{-19,  3}, {3, 3}}, anchorTopRight},  // XPos:202
    {moduleTypeMux1to8,    ledTypeYes, {{-13,  3}, {3, 3}}, anchorTopRight},  // XPos:222
    {moduleTypeMux1to8,    ledTypeYes, {{ -6,  3}, {3, 3}}, anchorTopRight},  // XPos:242
    // Mux8to1X (ID 60) — 8 LEDs spread across the middle
    {moduleTypeMux8to1X,   ledTypeYes, {{ 13, 14}, {3, 3}}, anchorTopLeft },  // XPos:34
    {moduleTypeMux8to1X,   ledTypeYes, {{ 21, 14}, {3, 3}}, anchorTopLeft },  // XPos:54
    {moduleTypeMux8to1X,   ledTypeYes, {{ 28, 14}, {3, 3}}, anchorTopLeft },  // XPos:74
    {moduleTypeMux8to1X,   ledTypeYes, {{ 36, 14}, {3, 3}}, anchorTopLeft },  // XPos:94
    {moduleTypeMux8to1X,   ledTypeYes, {{ 44, 14}, {3, 3}}, anchorTopLeft },  // XPos:114
    {moduleTypeMux8to1X,   ledTypeYes, {{ 52, 14}, {3, 3}}, anchorTopLeft },  // XPos:134
    {moduleTypeMux8to1X,   ledTypeYes, {{ 59, 14}, {3, 3}}, anchorTopLeft },  // XPos:154
    {moduleTypeMux8to1X,   ledTypeYes, {{ 67, 14}, {3, 3}}, anchorTopLeft },  // XPos:174
    // Mux8to1 (ID 124) — 8 LEDs across the upper-right strip
    {moduleTypeMux8to1,    ledTypeYes, {{-58,  3}, {3, 3}}, anchorTopRight},  // XPos:82
    {moduleTypeMux8to1,    ledTypeYes, {{-51,  3}, {3, 3}}, anchorTopRight},  // XPos:102
    {moduleTypeMux8to1,    ledTypeYes, {{-45,  3}, {3, 3}}, anchorTopRight},  // XPos:122
    {moduleTypeMux8to1,    ledTypeYes, {{-38,  3}, {3, 3}}, anchorTopRight},  // XPos:142
    {moduleTypeMux8to1,    ledTypeYes, {{-32,  3}, {3, 3}}, anchorTopRight},  // XPos:162
    {moduleTypeMux8to1,    ledTypeYes, {{-25,  3}, {3, 3}}, anchorTopRight},  // XPos:202
    {moduleTypeMux8to1,    ledTypeYes, {{-19,  3}, {3, 3}}, anchorTopRight},  // XPos:202
    {moduleTypeMux8to1,    ledTypeYes, {{-13,  3}, {3, 3}}, anchorTopRight},  // XPos:222
    // BinCounter (ID 130) — 8 LEDs across the upper-right strip
    {moduleTypeBinCounter, ledTypeYes, {{-58,  3}, {3, 3}}, anchorTopRight},  // XPos:81
    {moduleTypeBinCounter, ledTypeYes, {{-51,  3}, {3, 3}}, anchorTopRight},  // XPos:104
    {moduleTypeBinCounter, ledTypeYes, {{-43,  3}, {3, 3}}, anchorTopRight},  // XPos:127
    {moduleTypeBinCounter, ledTypeYes, {{-36,  3}, {3, 3}}, anchorTopRight},  // XPos:150
    {moduleTypeBinCounter, ledTypeYes, {{-28,  3}, {3, 3}}, anchorTopRight},  // XPos:173
    {moduleTypeBinCounter, ledTypeYes, {{-21,  3}, {3, 3}}, anchorTopRight},  // XPos:196
    {moduleTypeBinCounter, ledTypeYes, {{-14,  3}, {3, 3}}, anchorTopRight},  // XPos:219
    {moduleTypeBinCounter, ledTypeYes, {{ -6,  3}, {3, 3}}, anchorTopRight},  // XPos:242
    // ADConv (ID 131) — 8 LEDs across the upper-right strip
    {moduleTypeADConv,     ledTypeYes, {{-58,  3}, {3, 3}}, anchorTopRight},  // XPos:81
    {moduleTypeADConv,     ledTypeYes, {{-50,  3}, {3, 3}}, anchorTopRight},  // XPos:105
    {moduleTypeADConv,     ledTypeYes, {{-43,  3}, {3, 3}}, anchorTopRight},  // XPos:127
    {moduleTypeADConv,     ledTypeYes, {{-36,  3}, {3, 3}}, anchorTopRight},  // XPos:150
    {moduleTypeADConv,     ledTypeYes, {{-28,  3}, {3, 3}}, anchorTopRight},  // XPos:173
    {moduleTypeADConv,     ledTypeYes, {{-21,  3}, {3, 3}}, anchorTopRight},  // XPos:196
    {moduleTypeADConv,     ledTypeYes, {{-14,  3}, {3, 3}}, anchorTopRight},  // XPos:219
    {moduleTypeADConv,     ledTypeYes, {{ -6,  3}, {3, 3}}, anchorTopRight},  // XPos:242
    // 8Counter (ID 86) — 8 LEDs across the upper-right strip
    {moduleType8Counter,   ledTypeYes, {{-58,  3}, {3, 3}}, anchorTopRight},  // XPos:81
    {moduleType8Counter,   ledTypeYes, {{-51,  3}, {3, 3}}, anchorTopRight},  // XPos:104
    {moduleType8Counter,   ledTypeYes, {{-43,  3}, {3, 3}}, anchorTopRight},  // XPos:127
    {moduleType8Counter,   ledTypeYes, {{-36,  3}, {3, 3}}, anchorTopRight},  // XPos:150
    {moduleType8Counter,   ledTypeYes, {{-28,  3}, {3, 3}}, anchorTopRight},  // XPos:173
    {moduleType8Counter,   ledTypeYes, {{-21,  3}, {3, 3}}, anchorTopRight},  // XPos:196
    {moduleType8Counter,   ledTypeYes, {{-14,  3}, {3, 3}}, anchorTopRight},  // XPos:219
    {moduleType8Counter,   ledTypeYes, {{ -6,  3}, {3, 3}}, anchorTopRight},  // XPos:242
};

#endif // __MODULE_RESOURCES_H__
