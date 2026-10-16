/*
 * modulestatus — which module types the sound engine plays, by palette group, and on what evidence.
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
// Notes: Docs/code-notes/modulestatus.c.md - "// notes §k" refers there.

// Docs/engine-module-status.md used to be kept by hand and was wrong about eight modules within a
// day of their being added. This asks the engine itself — sound_engine_models_module() is the same
// test the canvas uses to grey a module out — and prints the status tables ready to paste. The
// EVIDENCE for each modelled module is editorial, so it is kept here in kEvidence; the run fails if
// a modelled module has no row, or a row names a module that is not offered or not modelled.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "moduleResourcesAccess.h"
#include "soundEngine.h"

typedef enum {
    eConfirmed = 0,    // compared with the G2, and it agrees
    eModelled,         // a full model in the reference, not yet compared with the G2
    eApproximate,      // follows the manual's description; a full model is still to come
    ePartial,          // something about it is known to be missing
    eNoSound,          // nothing to render
    STATE_COUNT
} tState;

typedef struct {
    tPaletteGroup group;
    const char *  name;
} tGroupRow;

typedef struct {
    const char * name;
    tState       state;
    const char * refs;     // where the reference describes it
    const char * g2;       // what was compared on the G2, and when
    const char * open;     // what is still open
} tEvidence;

static const char * kStateName[STATE_COUNT] = {"Confirmed on the G2", "Modelled", "Approximate", "Partial", "No sound"};

static const tGroupRow kGroups[] = {
    {palGroupOsc,    "Oscillators"},
    {palGroupFilter, "Filters"    },
    {palGroupEnv,    "Envelopes"  },
    {palGroupLfo,    "LFOs"       },
    {palGroupMixer,  "Mixers"     },
    {palGroupLevel,  "Level"      },
    {palGroupShaper, "Shapers"    },
    {palGroupDelay,  "Delays"     },
    {palGroupFx,     "Effects"    },
    {palGroupIo,     "In/Out"     },
    {palGroupSwitch, "Switches"   },
    {palGroupLogic,  "Logic"      },
    {palGroupSeq,    "Sequencers" },
    {palGroupRnd,    "Random"     },
    {palGroupNote,   "Note"       },
    {palGroupMidi,   "MIDI"       },
};

static const tEvidence kEvidence[] = {
    // Oscillators
    {"Osc A",                 eConfirmed,   "§6.3",                      "captures (G2Captures/osca); OscC/OscD level against it 09-12", "Freq mode and Partial below 33 now track Kbt (§6.1a) - heard on Osc C only"},
    {"Osc B",                 eConfirmed,   "§6.3, §6.6-§6.8",           "5 waves x 8 pitches, harmonics within 0.1 dB (09-17); DualSaw below zero 0.1 dB (10-08)", "FM and Sync not compared on the G2; Shape 127's one-sample click not modelled; Freq mode and Partial below 33 now track Kbt (§6.1a) - heard on Osc C only"},
    {"Osc C",                 eConfirmed,   "§6.3, §6.1a",               "Saw, Sqr50/25/10, Tri at 440-3520 Hz (09-17); Freq mode tracks the key: AdditiveG2 within 1.4 dB per octave (10-16)", "FM not compared on the G2"},
    {"Osc D",                 eConfirmed,   "§6.3, §6.1a",               "level and harmonics against OscA (09-12)", "Pitch Type: Freq tracking heard on Osc C (10-16), not on Osc D"},
    {"Osc Phase Mod",         eModelled,    "§53",                       "in Bassic and Basslines FM4: held balance within 2 dB per octave (10-16)", "Freq mode and Partial below 33 now track Kbt (§6.1a) - heard on Osc C only"},
    {"Osc Shape A",           eModelled,    "§27, §6.7",                 "through OscShpB's captures only", "no OscShpA capture of its own; Freq mode and Partial below 33 now track Kbt (§6.1a) - heard on Osc C only"},
    {"Osc Shape B",           eConfirmed,   "§27, §6.7",                 "every wave, Shape Mod -1..+1, harmonics within 0.16 dB (10-02); Sine3/4 within 0.001 (09-17)", "Pulse at exactly +-1: click -33 dB against the G2's -41; Freq mode and Partial below 33 now track Kbt (§6.1a) - heard on Osc C only"},
    {"Osc Dual",              eConfirmed,   "§12.4, §12.5",              "levels, duty, phase (09-12); PW and Phase mod (10-03)", "Freq mode and Partial below 33 now track Kbt (§6.1a) - heard on Osc C only"},
    {"Noise Osc",             eConfirmed,   "§8.2-§8.5",                 "09-12 capture agrees 110 Hz-1 kHz (the old model's fit)", "level above 1 kHz not re-captured since the §8 model; Freq mode and Partial below 33 now track Kbt (§6.1a) - heard on Osc C only"},
    {"Noise",                 eConfirmed,   "§7.2a",                     "09-12 capture reproduced through the desk's shelf to a constant", "-"},
    {"Sampler",               ePartial,     "notes §213",                "engine only - the G2 has no such module", "velocity layers untested (no layered file); E-mu banks: first preset only, no layered voices; .npno pianos not read"},
    {"Metallic Noise",        eModelled,    "§66",                       "-", "no capture"},
    {"Osc Percussion",        eConfirmed,   "§40",                       "8 takes: levels 0.1 dB, decay 2 ms, pitch exact (09-25)", "-"},
    {"Drum Synth",            eConfirmed,   "§39",                       "Master Freq 0.2% (09-21); noise, click, oscillator energies within 0.8 dB (09-25)", "level curve set by §39.3, not fitted to the 09-20 sweeps"},
    {"Osc String",            eModelled,    "§70.4",                     "-", "-"},
    {"FM Operator",           eConfirmed,   "§14.2-§14.6",               "envelope, level, FM depth, feedback, level and rate scaling, velocity (10-03); an idle voice starts from L4: Backtalk DX repeated notes as the G2 within 0.7 dB (10-16)", "AMod and Pitch inputs (§14.6) not yet compared (to-test); Gate/Note/Vel jacks read from the voice"},
    {"DX Router",             eConfirmed,   "§14.1, §14.5",              "Main level 8.8 dB against 8.45 (10-03); Backtalk DX held tone within 1-2 dB per octave (10-16)", "Out1-Out6 not used outside the node"},
    {"Driver",                eModelled,    "§70.5",                     "-", "-"},
    {"Resonator",             eModelled,    "§70.4a",                    "-", "-"},
    {"Osc Master",            eModelled,    "§51",                       "-", "-"},
    // Filters
    {"LP Filter",             eModelled,    "§22",                       "FM input at 2 semitones a unit only (notes §160)", "no response captured"},
    {"HP Filter",             eModelled,    "§22",                       "-", "no response captured"},
    {"Nord Filter",           eConfirmed,   "§23",                       "noise through LP/BP/HP/BR, 12 and 24 dB, Freq 50 and 85, Res 0 and 100: within 1.44 dB, mean 0.06-0.35 (10-11)", "Pitch, FM and Res mod not compared"},
    {"Classic Filter",        eConfirmed,   "§21",                       "04 Chris' Pad within 0.5 dB 100 Hz-12 kHz; a high-Res test patch's peak within 0.04 semitone (09-28)", "Env and the dB/Oct tap not compared since the 09-14 law"},
    {"Multi Filter",          eConfirmed,   "§10",                       "54 noise responses within 0.5-0.6 dB (09-12); FreqM within 1% (10-08)", "GComp off not captured"},
    {"Phase Filter",          eModelled,    "§67",                       "-", "-"},
    {"Comb Filter",           eConfirmed,   "§13.4",                     "noise through all three Types (09-12); Pitch attenuator (10-08)", "-"},
    {"Static Filter",         eModelled,    "§10.4",                     "peak heights only (2026-08)", "HP tap term not modelled; a noise capture per type"},
    {"FltVoice",              eModelled,    "§56",                       "-", "-"},
    {"WahWah",                eModelled,    "§69.9",                     "-", "-"},
    {"Vocoder",               eModelled,    "§70.8",                     "-", "-"},
    {"Eq 2-band",             eConfirmed,   "§11.2",                     "43 noise settings across the EQs, 0.53 dB mean (09-12)", "high shelf changed 10-09 (0.3-0.7 dB near 3 kHz), not re-captured"},
    {"Eq 3-band",             eConfirmed,   "§11",                       "0.66 dB mean, 1.42 worst (09-12)", "the 1.42 dB setting (mid -13.5 dB at 8 kHz) not explained by the model"},
    {"Eq Peak",               eConfirmed,   "§11.3, §11.5",              "0.57 dB mean (09-12)", "-"},
    // Envelopes
    {"Envelope ADSR",         eConfirmed,   "§17.1-§17.8",               "time law and curves agree with the 08-24 and 09-07 captures (not kept); release at LogExp R14 and LinExp R20 within one 50 ms window (10-16)", "-"},
    {"Envelope AHD",          eConfirmed,   "§17.11a",                   "Hold 16-80, within 1% (10-08)", "-"},
    {"Envelope ADR",          eModelled,    "§17.9",                     "-", "stages never heard against the G2"},
    {"Envelop ADDSR",         eModelled,    "§17.9",                     "-", "stages never heard against the G2; release fixed 10-09 (§17.9a)"},
    {"Envelope H",            eConfirmed,   "§17.9a",                    "Hold 12-64 within 0.1 ms (10-08)", "-"},
    {"Envelope D",            eModelled,    "§17.9",                     "no KB: an untriggered EnvD silent, as in 14 CS80project72 (09-27)", "decay never compared"},
    {"Envelope Multi",        eModelled,    "§17.11",                    "-", "-"},
    {"Envelope Mod AHD",      eConfirmed,   "§17.9a, §17.10",            "Hold within 0.3% (10-08)", "time-mod jacks not checked"},
    {"Envelope Mod ADSR",     eConfirmed,   "§17.10",                    "Decay mod at -32..+32 units (09-20)", "Attack and Sustain mod not checked; +32 units 4.1 s against 3.6"},
    // LFOs
    {"LFO A",                 eConfirmed,   "§28",                       "Hi rate measured", "BPM and Clk not checked on the G2"},
    {"LFO B",                 eModelled,    "§28, §28.4, §54",           "-", "-"},
    {"LFO C",                 eModelled,    "§28",                       "-", "never captured"},
    {"LFO Shp A",             eConfirmed,   "§28.6",                     "Rate Sub, Lo, Hi within 0.013% (09-07)", "BPM and Clk not checked; waves not captured"},
    {"Clock Generator",       eConfirmed,   "§59",                       "Master clock within 7-17 ppm; engine within 1-4 ms over 66 s (10-09)", "-"},
    // Mixers
    {"Mixer 1-1 A",           eConfirmed,   "§3",                        "106 configurations of the eleven mixers within 0.07 dB (09-12)", "-"},
    {"Mixer 1-1 S",           eConfirmed,   "§3",                        "as Mixer 1-1 A; On off mutes to -93 dBFS at Lev 40/78/127, Lin and Exp (10-16)", "-"},
    {"Mixer 2-1 A",           eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
    {"Mixer 4-1 A",           eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
    {"Mixer 4-1 B",           eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
    {"Mixer 4-1 C",           eConfirmed,   "§3",                        "218 steps 0.01 dB; Pad 0/-6/-12 (09-07, 09-12)", "-"},
    {"Mixer 4-1 S",           eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
    {"Mixer 2-1 B",           eConfirmed,   "§3",                        "as Mixer 1-1 A; Inv cancels", "-"},
    {"Mixer 8-1 A",           eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
    {"Mixer 8-1 B",           eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
    {"MixFader",              eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
    {"MixStereo",             eConfirmed,   "§5",                        "19 settings each, Log and Lin (09-12)", "-"},
    {"Fade 1-2",              eConfirmed,   "§4.2",                      "19 settings, every point within 0.001 (09-12)", "mod-input depth not captured, still 4 x the input"},
    {"Fade 2-1",              eConfirmed,   "§4.2",                      "as Fade 1-2", "mod-input depth not captured, still 4 x the input"},
    {"X-Fade",                eConfirmed,   "§4.2, §4.3",                "as Fade 1-2", "mod-input depth (Pan's law since 10-10) not captured"},
    {"Pan",                   eConfirmed,   "§4.2, §4.3",                "as Fade 1-2; mod depth, an LFO at 31: +-6.3 dB (10-10)", "-"},
    // Level
    {"Constant",              eConfirmed,   "§16.1",                     "the known input of most G2 checks since 09-13 (OscShpB, OscDual, DrumSynth, FltMulti)", "-"},
    {"ConstSwM",              eModelled,    "§68.1",                     "-", "-"},
    {"ConstSwT",              eModelled,    "§44",                       "-", "-"},
    {"CompLev",               eModelled,    "§48",                       "-", "-"},
    {"CompSig",               eModelled,    "§69.2",                     "-", "-"},
    {"LevAdd",                eModelled,    "§32",                       "-", "-"},
    {"LevAmp",                eConfirmed,   "paramCurves notes §29",     "33 dial positions (08-30); 127 as 0x7FFFFF settled BCHydro_DZLW (10-09)", "-"},
    {"LevConv",               eModelled,    "§31",                       "-", "-"},
    {"LevMod",                eModelled,    "§69.3",                     "-", "-"},
    {"LevMult",               eModelled,    "§74",                       "-", "-"},
    {"MinMax",                eModelled,    "§43",                       "-", "-"},
    {"ModAmt",                eModelled,    "§29",                       "parameter display only (param-validation)", "-"},
    {"NoiseGate",             eModelled,    "§70.6",                     "-", "-"},
    {"EnvFollow",             eModelled,    "§69.4",                     "-", "times at 24 kHz when not up-rated not modelled (notes §203)"},
    {"Red2Blue",              eModelled,    "§68.8, notes §203",         "-", "-"},
    {"Blue2Red",              eModelled,    "§68.8",                     "-", "-"},
    // Shapers
    {"Saturate",              eModelled,    "paramCurves notes §33", "-", "one ramp per mode would capture it"},
    {"Clip",                  eModelled,    "paramCurves notes §36", "-", "one ramp per mode would capture it"},
    {"OverDrive",             eConfirmed,   "§71",                       "energy above 6 kHz in 14 CS80project72: -41.5 against -41.6 dB (10-04)", "-"},
    {"ShpExp",                eModelled,    "paramCurves notes §32", "-", "one ramp per mode would capture it"},
    {"WaveWrap",              eModelled,    "§73, paramCurves notes §34", "-", "one ramp per mode would capture it"},
    {"ShpStatic",             eModelled,    "paramCurves notes §31", "-", "one ramp per mode would capture it"},
    {"Rect",                  eModelled,    "paramCurves notes §30", "-", "one ramp per mode would capture it"},
    // Delays
    {"Delay Single A",        eModelled,    "§52",                       "through DelayA only", "-"},
    {"Delay Single B",        eModelled,    "§52.1",                     "-", "-"},
    {"Delay Dual",            eApproximate, "§70.1, §52.1",              "-", "only the taps are fully modelled (§52.1)"},
    {"Delay Quad",            eApproximate, "§70.1, §52.1",              "-", "as Delay Dual"},
    {"Delay A",               eConfirmed,   "§24",                       "FB, LP, dry/wet, Time and Clk (09; audio not kept)", "-"},
    {"Delay B",               eConfirmed,   "§24",                       "as Delay A; HP at four settings", "-"},
    {"Delay Stereo",          eModelled,    "§65",                       "-", "X-FB never compared"},
    {"Delay Clock",           eModelled,    "§69.7",                     "-", "-"},
    {"Delay Eight",           eApproximate, "§70.1, §52.1",              "-", "as Delay Dual"},
    {"DlyShiftReg",           eModelled,    "§69.6",                     "-", "-"},
    // Effects
    {"Compressor",            eConfirmed,   "§25",                       "a steady sine at 8 levels, -47 to -19 dB, 04 Chris' Pad's settings: gain equal to the G2's within 0.01 dB (10-11)", "attack/release timing and SideChain not compared on the G2"},
    {"Digitizer",             eModelled,    "§69.8",                     "-", "-"},
    {"FreqShift",             eModelled,    "§57",                       "-", "-"},
    {"Flanger",               eModelled,    "§70.2",                     "-", "-"},
    {"Chorus",                eConfirmed,   "§19",                       "tap spans, rate, mix (09-07); unity at Amount 0 (09-14); no tail once its input stops, as the G2 (10-16)", "-"},
    {"Phaser",                eModelled,    "§55",                       "-", "-"},
    {"PShift",                eModelled,    "§70.3",                     "-", "-"},
    {"Reverb",                eConfirmed,   "§20",                       "19 captures: onsets, stereo, Time law (09); Dry/Wet 96/64/48/24 within 1 dB of the cubed law (10-16)", "captured decays 6-12% long (their fits)"},
    {"Scratch",               eModelled,    "§70.3",                     "-", "-"},
    // In/Out
    {"2 Outputs",             eConfirmed,   "§63, notes §167, §198",     "Pad +6 dB (09-07); an unpatched socket is silent (10-10); the path every G2 check goes through; Out to Bus 1/2 silent on its own, as the G2 (10-16)", "-"},
    {"4 Outputs",             eConfirmed,   "-",                         "Pad +6 dB (09-07)", "-"},
    {"2 Inputs",              eConfirmed,   "§37",                       "full scale and Pad off the G2's own meter (10-08)", "the application has no input device"},
    {"4 Inputs",              eModelled,    "§37, §69.12",               "through 2-In only", "-"},
    {"FX Input",              eConfirmed,   "capture-inventory",         "Pad +6/0/-6/-12 dB (09-07)", "-"},
    {"Keyboard",              eConfirmed,   "§26",                       "velocity through the Operators within 0.03 dB (10-03)", "-"},
    {"Monophonic Keyboard",   eConfirmed,   "§35",                       "held-note return on 01 Mini Emulator (CT, 09-19)", "Poly use is a guess"},
    {"Device",                eApproximate, "§70.13",                    "-", "-"},
    {"Status",                eConfirmed,   "§70.13",                    "Patch Active is low, off the G2 (10-04)", "-"},
    {"Note Detector",         eModelled,    "§69.11",                    "-", "-"},
    {"Name Bar",              eNoSound,     "-",                         "-", "-"},
    // Switches
    {"SwOnOffM",              eModelled,    "§68.1",                     "-", "-"},
    {"SwOnOffT",              eModelled,    "§30",                       "-", "-"},
    {"Sw2-1",                 eModelled,    "§33",                       "-", "-"},
    {"Sw2-1M",                eModelled,    "§68.1",                     "-", "-"},
    {"Sw4-1",                 eModelled,    "§68.1",                     "-", "-"},
    {"Sw8-1",                 eModelled,    "§33",                       "-", "-"},
    {"Sw1-2",                 eModelled,    "§68.1",                     "-", "-"},
    {"Sw1-2M",                eModelled,    "§68.1",                     "-", "-"},
    {"Sw1-4",                 eModelled,    "§68.1",                     "-", "-"},
    {"Sw1-8",                 eModelled,    "§45",                       "-", "-"},
    {"ValSw2-1",              eModelled,    "§34",                       "-", "-"},
    {"ValSw1-2",              eModelled,    "§68.2",                     "-", "-"},
    {"Mux8-1",                eModelled,    "§68.3",                     "-", "-"},
    {"Mux1-8",                eModelled,    "§68.3",                     "-", "-"},
    {"Mux8-1X",               eModelled,    "§70.11",                    "-", "-"},
    {"S&H",                   eModelled,    "§38.5",                     "-", "-"},
    {"T&H",                   eModelled,    "§68.4",                     "-", "-"},
    {"WindSw",                eModelled,    "§68.5",                     "-", "-"},
    // Logic
    {"Invert",                eModelled,    "§38.1",                     "-", "-"},
    {"Pulse",                 eConfirmed,   "§18",                       "17 widths within two samples (09)", "Plus/Minus edge (10-09) in to-test"},
    {"Delay",                 eModelled,    "§46",                       "-", "-"},
    {"Gate",                  eModelled,    "§38.2",                     "-", "-"},
    {"FlipFlop",              eApproximate, "§38.3",                     "-", "-"},
    {"ClkDiv",                eConfirmed,   "§38.4",                     "14 pattern seq steps once a bar, as on the G2 (10-07)", "-"},
    {"8Counter",              eModelled,    "§68.6",                     "-", "-"},
    {"BinCounter",            eModelled,    "§68.6",                     "-", "-"},
    {"ADConv",                eModelled,    "§68.7",                     "-", "-"},
    {"DAConv",                eModelled,    "§68.7",                     "-", "-"},
    // Sequencers
    {"Sequencer Event",       eConfirmed,   "§58",                       "step timing at 192 kHz (09-26); 1:2 BCHydro_DZLW within 0.2 dB (10-09)", "-"},
    {"Sequencer Values",      eConfirmed,   "§58",                       "as Sequencer Event", "-"},
    {"Sequencer Level",       eConfirmed,   "§58, §69.1",                "Length/Cycle restart in BCHydro_DZLW (10-09)", "-"},
    {"Sequencer Note",        eConfirmed,   "§58, §58.1",                "18 Unreal Dreams' pitches (09-28)", "the record stage never compared"},
    {"Sequencer Controlled",  eModelled,    "§70.10",                    "-", "-"},
    // Random
    {"Random A",              eModelled,    "§47",                       "-", "Pitch input not read"},
    {"Random B",              eModelled,    "§69.10",                    "-", "-"},
    {"Rnd Clock A",           eModelled,    "§64",                       "-", "-"},
    {"Rnd Clock B",           eModelled,    "§70.9",                     "-", "-"},
    {"Rnd Trig",              eModelled,    "§64",                       "-", "-"},
    {"Rnd Pattern",           eModelled,    "§70.9",                     "-", "-"},
    // Note
    {"Note Quantiser",        eModelled,    "§49",                       "-", "-"},
    {"Key Quantiser",         eModelled,    "§41",                       "-", "-"},
    {"Partial Quantiser",     eModelled,    "§69.5",                     "-", "-"},
    {"Note Scaler",           eModelled,    "§60",                       "-", "-"},
    {"Glide",                 eModelled,    "§36",                       "-", "-"},
    {"Pitch Tracker",         eModelled,    "§70.7",                     "-", "-"},
    {"Zero Crossing Counter", eModelled,    "§70.7a",                    "-", "-"},
    {"Level Scaler",          eModelled,    "§70.12",                    "-", "-"},
    // MIDI
    {"CtrlSend",              eConfirmed,   "§62.3",                     "1:2 BCHydro_DZLW's variation switching, within 1-4 ms (10-09)", "MIDI channels 1-16 dropped"},
    {"PCSend",                eNoSound,     "-",                         "-", "nothing leaves by MIDI"},
    {"CtrlRcv",               eModelled,    "§70.13",                    "-", "in the plug-in only notes arrive"},
    {"NoteRcv",               eModelled,    "§70.13",                    "-", "-"},
    {"NoteZone",              eNoSound,     "-",                         "-", "nothing leaves by MIDI"},
    {"Automate",              eNoSound,     "-",                         "-", "nothing leaves by MIDI"},
    {"NoteSend",              ePartial,     "§62",                       "18 Unreal Dreams' pitches and band balance (09-28)", "MIDI channels 1-16 dropped"},
};

// notes §2 - the arithmetic each module's model runs in, against the instrument's 24-bit words
typedef enum {
    ePrecWords = 0,    // the signal path in the instrument's words: its truncation, rounding and saturation
    ePrecControl,      // counters, phases, steps or coefficients in its words; the audio path in doubles
    ePrecSaturating,   // doubles on the instrument's scaling, saturated where it saturates; never rounded to 24 bits
    ePrecDoubles,      // doubles throughout, saturated only where the value leaves the module
    ePrecNone,         // logic, switching, copies, MIDI: thresholds and whole numbers, which no rounding moves
    PREC_COUNT
} tPrecision;

static const char * kPrecisionName[PREC_COUNT] = {"Integer words", "Integer control", "Saturating doubles", "Doubles", "Not affected"};

typedef struct {
    const char * name;
    tPrecision   precision;
    const char * note;    // what is in words, or "-"
} tPrecisionRow;

static const tPrecisionRow kPrecision[] = {
    // Oscillators
    {"Osc A",                 ePrecDoubles,    "phase accumulator in doubles"},
    {"Osc B",                 ePrecDoubles,    "phase accumulator in doubles"},
    {"Osc C",                 ePrecDoubles,    "phase accumulator in doubles"},
    {"Osc D",                 ePrecDoubles,    "phase accumulator in doubles"},
    {"Osc Phase Mod",         ePrecDoubles,    "phase and Phase M in doubles; the sine is the module's polynomial"},
    {"Osc Shape A",           ePrecDoubles,    "phase accumulator in doubles"},
    {"Osc Shape B",           ePrecDoubles,    "phase accumulator in doubles"},
    {"Osc Dual",              ePrecDoubles,    "phase accumulator in doubles"},
    {"Noise Osc",             ePrecSaturating, "24-bit LFSR; the band-passes saturate at the word's +-1"},
    {"Noise",                 ePrecDoubles,    "xorshift draw, not the instrument's generator"},
    {"Metallic Noise",        ePrecWords,      "squares, Freq word and one-poles"},
    {"Osc Percussion",        ePrecSaturating, "DSP words carried in doubles, saturated"},
    {"Drum Synth",            ePrecSaturating, "the decoded words carried in doubles"},
    {"Osc String",            ePrecDoubles,    "-"},
    {"FM Operator",           ePrecControl,    "envelope, level and rate offsets in words; the operator in doubles"},
    {"DX Router",             ePrecControl,    "envelope, level and rate offsets in words; the operators in doubles"},
    {"Driver",                ePrecDoubles,    "-"},
    {"Resonator",             ePrecDoubles,    "-"},
    {"Osc Master",            ePrecDoubles,    "-"},
    // Filters
    {"LP Filter",             ePrecDoubles,    "-"},
    {"HP Filter",             ePrecDoubles,    "-"},
    {"Nord Filter",           ePrecDoubles,    "-"},
    {"Classic Filter",        ePrecDoubles,    "-"},
    {"Multi Filter",          ePrecDoubles,    "-"},
    {"Phase Filter",          ePrecWords,      "the six allpass stages in the accumulator"},
    {"Comb Filter",           ePrecSaturating, "every word saturated"},
    {"Static Filter",         ePrecDoubles,    "-"},
    {"FltVoice",              ePrecControl,    "vowel table and resonance word; the resonators in doubles"},
    {"WahWah",                ePrecWords,      "sweep and filter words"},
    {"Vocoder",               ePrecSaturating, "the converters' and bands' words, saturated"},
    {"Eq 2-band",             ePrecDoubles,    "-"},
    {"Eq 3-band",             ePrecDoubles,    "-"},
    {"Eq Peak",               ePrecDoubles,    "-"},
    // Envelopes
    {"Envelope ADSR",         ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    {"Envelope AHD",          ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    {"Envelope ADR",          ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    {"Envelop ADDSR",         ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    {"Envelope H",            ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    {"Envelope D",            ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    {"Envelope Multi",        ePrecWords,      "its tick in words"},
    {"Envelope Mod AHD",      ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    {"Envelope Mod ADSR",     ePrecWords,      "level and segments in words; the VCA multiply in doubles"},
    // LFOs
    {"LFO A",                 ePrecDoubles,    "phase in doubles"},
    {"LFO B",                 ePrecDoubles,    "phase in doubles"},
    {"LFO C",                 ePrecDoubles,    "phase in doubles"},
    {"LFO Shp A",             ePrecDoubles,    "phase in doubles"},
    {"Clock Generator",       ePrecWords,      "phase and tempo words"},
    // Mixers
    {"Mixer 1-1 A",           ePrecDoubles,    "-"},
    {"Mixer 1-1 S",           ePrecDoubles,    "-"},
    {"Mixer 2-1 A",           ePrecDoubles,    "-"},
    {"Mixer 4-1 A",           ePrecDoubles,    "-"},
    {"Mixer 4-1 B",           ePrecDoubles,    "-"},
    {"Mixer 4-1 C",           ePrecDoubles,    "-"},
    {"Mixer 4-1 S",           ePrecDoubles,    "-"},
    {"Mixer 2-1 B",           ePrecDoubles,    "-"},
    {"Mixer 8-1 A",           ePrecDoubles,    "-"},
    {"Mixer 8-1 B",           ePrecDoubles,    "-"},
    {"MixFader",              ePrecDoubles,    "-"},
    {"MixStereo",             ePrecDoubles,    "-"},
    {"Fade 1-2",              ePrecDoubles,    "-"},
    {"Fade 2-1",              ePrecDoubles,    "-"},
    {"X-Fade",                ePrecDoubles,    "-"},
    {"Pan",                   ePrecDoubles,    "-"},
    // Level
    {"Constant",              ePrecDoubles,    "-"},
    {"ConstSwM",              ePrecDoubles,    "-"},
    {"ConstSwT",              ePrecDoubles,    "-"},
    {"CompLev",               ePrecWords,      "compared as words"},
    {"CompSig",               ePrecWords,      "compared as words"},
    {"LevAdd",                ePrecDoubles,    "-"},
    {"LevAmp",                ePrecDoubles,    "-"},
    {"LevConv",               ePrecDoubles,    "-"},
    {"LevMod",                ePrecWords,      "-"},
    {"LevMult",               ePrecDoubles,    "-"},
    {"MinMax",                ePrecDoubles,    "-"},
    {"ModAmt",                ePrecDoubles,    "-"},
    {"NoiseGate",             ePrecWords,      "follower and gate in words"},
    {"EnvFollow",             ePrecWords,      "peak word and follower"},
    {"Red2Blue",              ePrecNone,       "a copy, held at 24 kHz"},
    {"Blue2Red",              ePrecNone,       "a copy"},
    // Shapers
    {"Saturate",              ePrecDoubles,    "-"},
    {"Clip",                  ePrecDoubles,    "-"},
    {"OverDrive",             ePrecSaturating, "drive, feedback and blend saturated as words"},
    {"ShpExp",                ePrecDoubles,    "-"},
    {"WaveWrap",              ePrecDoubles,    "-"},
    {"ShpStatic",             ePrecDoubles,    "-"},
    {"Rect",                  ePrecDoubles,    "-"},
    // Delays
    {"Delay Single A",        ePrecDoubles,    "a 4-point read"},
    {"Delay Single B",        ePrecDoubles,    "a 4-point read"},
    {"Delay Dual",            ePrecDoubles,    "a 4-point read"},
    {"Delay Quad",            ePrecDoubles,    "a 4-point read"},
    {"Delay A",               ePrecWords,      "the tap, LP, HP and feedback on half-scale words"},
    {"Delay B",               ePrecWords,      "the tap, LP, HP and feedback on half-scale words"},
    {"Delay Stereo",          ePrecWords,      "Delay B's tap on both lines"},
    {"Delay Clock",           ePrecNone,       "a copy on each clock"},
    {"Delay Eight",           ePrecDoubles,    "a 4-point read"},
    {"DlyShiftReg",           ePrecNone,       "copies"},
    // Effects
    {"Compressor",            ePrecWords,      "detector, gain table and make-up"},
    {"Digitizer",             ePrecWords,      "rate word and phase"},
    {"FreqShift",             ePrecSaturating, "allpass chains saturated at the word's +-1"},
    {"Flanger",               ePrecControl,    "sweep phase word; the ring read in doubles"},
    {"Chorus",                ePrecControl,    "LFO phase word; the taps in doubles"},
    {"Phaser",                ePrecSaturating, "the module's fractions, saturated"},
    {"PShift",                ePrecControl,    "phase word; the taps in doubles"},
    {"Reverb",                ePrecWords,      "every stored value rounded down to the 24-bit grid and clamped"},
    {"Scratch",               ePrecControl,    "ratio and phase words; the taps in doubles"},
    // In/Out
    {"2 Outputs",             ePrecDoubles,    "saturated at twice full scale"},
    {"4 Outputs",             ePrecDoubles,    "saturated at twice full scale"},
    {"2 Inputs",              ePrecDoubles,    "-"},
    {"4 Inputs",              ePrecDoubles,    "-"},
    {"FX Input",              ePrecDoubles,    "-"},
    {"Keyboard",              ePrecNone,       "-"},
    {"Monophonic Keyboard",   ePrecNone,       "-"},
    {"Device",                ePrecNone,       "-"},
    {"Status",                ePrecNone,       "-"},
    {"Note Detector",         ePrecNone,       "-"},
    {"Name Bar",              ePrecNone,       "-"},
    // Switches
    {"SwOnOffM",              ePrecNone,       "-"},
    {"SwOnOffT",              ePrecNone,       "-"},
    {"Sw2-1",                 ePrecNone,       "-"},
    {"Sw2-1M",                ePrecNone,       "-"},
    {"Sw4-1",                 ePrecNone,       "-"},
    {"Sw8-1",                 ePrecNone,       "-"},
    {"Sw1-2",                 ePrecNone,       "-"},
    {"Sw1-2M",                ePrecNone,       "-"},
    {"Sw1-4",                 ePrecNone,       "-"},
    {"Sw1-8",                 ePrecNone,       "-"},
    {"ValSw2-1",              ePrecNone,       "-"},
    {"ValSw1-2",              ePrecNone,       "-"},
    {"Mux8-1",                ePrecControl,    "Ctrl's word >> 17 picks the input"},
    {"Mux1-8",                ePrecControl,    "Ctrl's word >> 17 picks the output"},
    {"Mux8-1X",               ePrecDoubles,    "-"},
    {"S&H",                   ePrecNone,       "a copy"},
    {"T&H",                   ePrecNone,       "a copy"},
    {"WindSw",                ePrecNone,       "-"},
    // Logic
    {"Invert",                ePrecNone,       "-"},
    {"Pulse",                 ePrecNone,       "-"},
    {"Delay",                 ePrecNone,       "-"},
    {"Gate",                  ePrecNone,       "-"},
    {"FlipFlop",              ePrecNone,       "-"},
    {"ClkDiv",                ePrecNone,       "-"},
    {"8Counter",              ePrecNone,       "-"},
    {"BinCounter",            ePrecNone,       "-"},
    {"ADConv",                ePrecWords,      "the input's word, bit by bit"},
    {"DAConv",                ePrecWords,      "the bits as a word"},
    // Sequencers
    {"Sequencer Event",       ePrecWords,      "the module's own arithmetic"},
    {"Sequencer Values",      ePrecWords,      "the module's own arithmetic"},
    {"Sequencer Level",       ePrecWords,      "the module's own arithmetic"},
    {"Sequencer Note",        ePrecWords,      "the module's own arithmetic"},
    {"Sequencer Controlled",  ePrecWords,      "Ctrl's word picks the step"},
    // Random
    {"Random A",              ePrecControl,    "step words; the one-pole in doubles"},
    {"Random B",              ePrecControl,    "step words; the one-pole in doubles"},
    {"Rnd Clock A",           ePrecControl,    "seed and step words"},
    {"Rnd Clock B",           ePrecControl,    "seed, step and Step M words"},
    {"Rnd Trig",              ePrecControl,    "the draw against the threshold"},
    {"Rnd Pattern",           ePrecWords,      "generator and counter"},
    // Note
    {"Note Quantiser",        ePrecWords,      "the module's own arithmetic"},
    {"Key Quantiser",         ePrecWords,      "the reference model's integer arithmetic"},
    {"Partial Quantiser",     ePrecWords,      "-"},
    {"Note Scaler",           ePrecSaturating, "saturated as a word"},
    {"Glide",                 ePrecDoubles,    "-"},
    {"Pitch Tracker",         ePrecDoubles,    "-"},
    {"Zero Crossing Counter", ePrecDoubles,    "-"},
    {"Level Scaler",          ePrecSaturating, "gain and output saturated"},
    // MIDI
    {"CtrlSend",              ePrecSaturating, "the signal saturated as a word, then the controller value"},
    {"PCSend",                ePrecNone,       "-"},
    {"CtrlRcv",               ePrecNone,       "-"},
    {"NoteRcv",               ePrecNone,       "-"},
    {"NoteZone",              ePrecNone,       "-"},
    {"Automate",              ePrecNone,       "-"},
    {"NoteSend",              ePrecNone,       "-"},
};

#define PRECISION_COUNT (sizeof(kPrecision) / sizeof(kPrecision[0]))

#define EVIDENCE_COUNT (sizeof(kEvidence) / sizeof(kEvidence[0]))
#define GROUP_COUNT    (sizeof(kGroups) / sizeof(kGroups[0]))
#define LINE_BYTES     (4096)

static const char * module_name(tModuleType type) {
    const char * name = "?";

    for (uint32_t e = 0; e < array_size_palette_list(); e++) {
        if (gPaletteList[e].moduleType == type) {
            name = gPaletteList[e].menuLabel;
            break;
        }
    }
    return name;
}

static int evidence_index(const char * name) {
    int index = -1;

    for (uint32_t e = 0; e < EVIDENCE_COUNT; e++) {
        if (strcmp(kEvidence[e].name, name) == 0) {
            index = (int)e;
            break;
        }
    }
    return index;
}

static int precision_index(const char * name) {
    for (uint32_t r = 0; r < PRECISION_COUNT; r++) {
        if (strcmp(kPrecision[r].name, name) == 0) {
            return (int)r;
        }
    }
    return -1;
}

static void append_name(char * line, uint32_t * n, const char * name) {
    snprintf(line + strlen(line), LINE_BYTES - strlen(line), "%s%s", (*n > 0) ? ", " : "", name);
    (*n)++;
}

static const char * cell(const char * line) {
    return (line[0] != '\0') ? line : "-";
}

// notes §1 - the aspects a module has, read off the module tables (Docs/module-aspect-audit-design.md)
typedef enum {
    eAspectCore = 0,
    eAspectTiming,
    eAspectDials,
    eAspectInputs,
    eAspectLevel,
    eAspectModes,
    eAspectMeters,
    eAspectLeds,
    ASPECT_COUNT
} tAspect;

static const char * kAspectName[ASPECT_COUNT] = {"Core law", "Timing", "Dials", "Inputs", "Level / gain", "Modes / On-Off", "Meters", "LEDs"};

#define MAX_NAMED_TYPES  (256)
#define MAX_NAMED_PARAMS (MAX_NUM_PARAMETERS)
#define PARAM_NAME_BYTES (24)

// Parameter names by module type and index, from Docs/param-validation.md - the table rows carry few labels
static char sParamName[MAX_NAMED_TYPES][MAX_NAMED_PARAMS][PARAM_NAME_BYTES];

static void load_param_names(const char * path) {
    FILE *   file = fopen(path, "r");
    char     text[512];
    int      type = -1;

    if (file == NULL) {
        fprintf(stderr, "cannot read %s - dials will be numbered, not named\n", path);
        return;
    }

    while (fgets(text, sizeof(text), file) != NULL) {
        const char * header = strstr(text, "[module type ");
        unsigned     index  = 0;
        int          used   = 0;

        if (header != NULL) {
            type = ((sscanf(header, "[module type %d]", &type) == 1) && (type < MAX_NAMED_TYPES)) ? type : -1;
        } else if ((type >= 0) && (sscanf(text, " %u. %n", &index, &used) == 1) && (used > 0) && (index < MAX_NAMED_PARAMS)) {
            // The name runs up to the first run of two spaces: "Pitch Type  Menu"
            const char * name = text + used;
            const char * end  = strstr(name, "  ");
            size_t       len  = (end != NULL) ? (size_t)(end - name) : strcspn(name, "\n");

            if ((len > 0) && (strncmp(name, "(unnamed)", 9) != 0)) {
                snprintf(sParamName[type][index], PARAM_NAME_BYTES, "%.*s", (int)len, name);
            }
        }
    }
    fclose(file);
}

static bool is_switch_param(tParamType type) {
    switch (type) {
        case paramTypeBypass:
        case paramTypeEnable:
        case paramTypeToggle:
        case paramTypePush:
        case paramTypeMenu:
        case paramTypeStrMap:
        case paramTypeRadioEdit:
        case paramTypeOscWave:
        case paramTypeCustomData:
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

static bool is_time_param(tParamType type) {
    switch (type) {
        case paramTypeLFORate:
        case paramTypeADRTime:
        case paramTypePulseTime:
        case paramTypeTime:
        case paramTypeTimeClk:
        case paramTypeFlangerRate:
        case paramTypePhaserRate:
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

static void append_item(char * line, uint32_t * n, const char * label, uint32_t index, const char * fallback) {
    char name[64];

    if ((label != NULL) && (label[0] != '\0')) {
        snprintf(name, sizeof(name), "%s", label);
    } else {
        snprintf(name, sizeof(name), "%s %u", fallback, (unsigned)(index + 1u));
    }
    append_name(line, n, name);
}

// One module's aspects: items[a] lists what the aspect covers (its dials, its jacks), count[a] how many.
static bool group_has_level(tPaletteGroup group) {
    switch (group) {
        case palGroupOsc:
        case palGroupFilter:
        case palGroupEnv:
        case palGroupMixer:
        case palGroupLevel:
        case palGroupShaper:
        case palGroupDelay:
        case palGroupFx:
        case palGroupIo:
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

static void module_aspects(tModuleType type, tPaletteGroup group, char items[ASPECT_COUNT][LINE_BYTES], uint32_t count[ASPECT_COUNT],
                           bool has[ASPECT_COUNT]) {
    uint32_t paramIndex  = 0;
    bool     anyOut      = false;
    uint32_t inIndex     = 0;

    memset(items, 0, sizeof(char) * ASPECT_COUNT * LINE_BYTES);
    memset(count, 0, sizeof(uint32_t) * ASPECT_COUNT);
    memset(has, 0, sizeof(bool) * ASPECT_COUNT);

    for (uint32_t r = 0; r < array_size_param_location_list(); r++) {
        const tParamLocation * row = &paramLocationList[r];

        if (row->moduleType != type) {
            continue;
        }

        const char * label = ((type < MAX_NAMED_TYPES) && (paramIndex < MAX_NAMED_PARAMS) && (sParamName[type][paramIndex][0] != '\0'))
                             ? sParamName[type][paramIndex] : row->label;

        if (is_switch_param(row->type)) {
            append_item(items[eAspectModes], &count[eAspectModes], label, paramIndex, "param");
        } else {
            append_item(items[eAspectDials], &count[eAspectDials], label, paramIndex, "dial");

            if (is_time_param(row->type)) {
                append_item(items[eAspectTiming], &count[eAspectTiming], label, paramIndex, "dial");
            }
        }
        paramIndex++;
    }

    for (uint32_t r = 0, modeIndex = 0; r < array_size_mode_location_list(); r++) {
        if (modeLocationList[r].moduleType == type) {
            append_item(items[eAspectModes], &count[eAspectModes], modeLocationList[r].label, modeIndex++, "mode");
        }
    }

    for (uint32_t r = 0; r < array_size_connector_location_list(); r++) {
        const tConnectorLocation * jack = &connectorLocationList[r];

        if (jack->moduleType != type) {
            continue;
        }

        if (jack->direction != connectorDirIn) {
            anyOut = true;
            continue;
        }

        // Every input, signal or modulation: what an unpatched one reads is part of the aspect
        {
            static const char * kColour[] = {"red", "blue", "yellow"};
            char                 fallback[24];
            bool                 named    = (jack->label != NULL) && (jack->label[0] != '\0') && (strcmp(jack->label, "-") != 0)
                                            && (strcmp(jack->label, "--") != 0);

            snprintf(fallback, sizeof(fallback), "in %u (%s)", (unsigned)(inIndex + 1u),
                     ((uint32_t)jack->type < 3u) ? kColour[jack->type] : "?");
            append_name(items[eAspectInputs], &count[eAspectInputs], named ? jack->label : fallback);
        }
        inIndex++;
    }

    has[eAspectCore]      = anyOut || (group == palGroupIo);
    has[eAspectLevel]     = (anyOut || (group == palGroupIo)) && group_has_level(group);
    has[eAspectDials]     = (count[eAspectDials] > 0);
    has[eAspectTiming]    = (count[eAspectTiming] > 0);
    has[eAspectInputs] = (count[eAspectInputs] > 0);
    has[eAspectModes]     = (count[eAspectModes] > 0);
    // Meters: the level indications. LEDs: the lamps, and the sequencers' step position, which arrives as a
    // "volume" but is a position, not a level
    for (uint32_t r = 0; r < array_size_volume_location_list(); r++) {
        const tVolumeLocation * meter = &volumeLocationList[r];

        if (meter->moduleType != type) {
            continue;
        }

        switch (meter->volumeType) {
            case volumeTypeMono:      append_name(items[eAspectMeters], &count[eAspectMeters], "mono meter");
                break;
            case volumeTypeStereo:    append_name(items[eAspectMeters], &count[eAspectMeters], "stereo meter");
                break;
            case volumeTypeQuad:      append_name(items[eAspectMeters], &count[eAspectMeters], "quad meter");
                break;
            case volumeTypeCompress:  append_name(items[eAspectMeters], &count[eAspectMeters], "gain-reduction LEDs");
                break;
            case volumeTypeSequencer: append_name(items[eAspectLeds], &count[eAspectLeds], "step position");
                break;
            case volumeTypeNone:
                break;
        }
    }
    {
        uint32_t lamps = 0;
        uint32_t multi = 0;
        char     text[48];

        for (uint32_t r = 0; r < array_size_led_location_list(); r++) {
            if (ledLocationList[r].moduleType == type) {
                lamps += (ledLocationList[r].ledType == ledTypeYes) ? 1u : 0u;
                multi += (ledLocationList[r].ledType == ledTypeMultiBit) ? 1u : 0u;
            }
        }

        if (lamps > 0) {
            snprintf(text, sizeof(text), (lamps == 1) ? "%u lamp" : "%u lamps", (unsigned)lamps);
            append_name(items[eAspectLeds], &count[eAspectLeds], text);
        }

        if (multi > 0) {
            snprintf(text, sizeof(text), "a %u-LED group on one value", (unsigned)multi);
            append_name(items[eAspectLeds], &count[eAspectLeds], text);
        }
    }
    has[eAspectMeters] = (count[eAspectMeters] > 0);
    has[eAspectLeds]   = (count[eAspectLeds] > 0);
}

typedef struct {
    const char * module;
    tAspect      aspect;
    tState       state;
    const char * refs;
    const char * g2;
    const char * open;
} tAspectEvidence;

// notes §1 - the evidence one aspect at a time; an aspect with no row reads Unknown
static const tAspectEvidence kAspectEvidence[] = {
    {"Mixer 4-1 C", eAspectCore,      eConfirmed, "§3",              "Exp taper 0.99x^3 + 0.01x, 218 steps within 0.01 dB (09-12)", "-"},
    {"Mixer 4-1 C", eAspectDials,     eConfirmed, "§3",              "as Core law; Pad 0/-6/-12 (09-07)", "-"},
    {"Mixer 4-1 C", eAspectMeters,    eConfirmed, "§1.1, notes §191", "law on 82 steps (09-12); a chord meters one voice, 7-9 on both (10-10); clip held 1 s", "-"},
    {"Mixer 4-1 S", eAspectMeters,    eModelled,  "§1.1",            "FX area under a chord: 7-9 on the engine, 6-7 on the G2 (10-10)", "is 04 Chris' Pad's FX side hot? (todo)"},
    {"Pan",         eAspectCore,      eConfirmed, "§4.2",            "19 settings, Log and Lin, every point within 0.001 (09-12)", "-"},
    {"Pan",         eAspectInputs,    eConfirmed, "§4.3",            "LFO at PanMod 31: +-6.3 dB on both (10-10)", "the signal input's level not compared"},
    {"X-Fade",      eAspectCore,      eConfirmed, "§4.2",            "as Pan", "-"},
    {"X-Fade",      eAspectInputs,    eModelled,  "§4.3",            "-", "mod depth: Pan's law, not captured"},
    {"Fade 1-2",    eAspectInputs,    eApproximate, "§4.3",          "-", "mod depth still 4 x the input; its dial scales differently from Pan's"},
    {"Fade 2-1",    eAspectInputs,    eApproximate, "§4.3",          "-", "as Fade 1-2"},
    {"2 Outputs",   eAspectInputs,    eConfirmed, "notes §167",      "an unpatched socket is silent, outputs 1/2 and the FX bus (10-10)", "-"},
    {"2 Outputs",   eAspectLevel,     eConfirmed, "§63",             "Pad +6 dB (09-07)", "-"},
    {"4 Outputs",   eAspectLevel,     eConfirmed, "-",               "Pad +6 dB (09-07)", "-"},
    {"FX Input",    eAspectLevel,     eConfirmed, "capture-inventory", "Pad +6/0/-6/-12 dB (09-07)", "-"},
    {"FX Input",    eAspectCore,      eConfirmed, "notes §167",      "a side left unpatched at the 2-Out arrives silent (10-10)", "-"},
    {"LFO A",       eAspectCore,      eConfirmed, "§28, notes §63",  "never restarted by a note; three notes start at three phases on both (10-10)", "waves not captured"},
    // Meters and LEDs (2026-10-10): what the engine drives; a module it leaves dark shows only a connected G2's
    {"Mixer 4-1 B", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"Mixer 8-1 A", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"Mixer 8-1 B", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"MixFader", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"MixStereo", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"2 Outputs", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"4 Outputs", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"2 Inputs", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"4 Inputs", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"FX Input", eAspectMeters, eModelled, "§1.1, notes §191", "the law as Mixer 4-1 C", "-"},
    {"Compressor", eAspectMeters, eModelled, "§25, notes §122", "-", "gain-reduction lamps not compared"},
    {"Phase Filter", eAspectMeters, eModelled, "§1.1", "-", "the output's peak since 10-10, as Comb Filter and Eq Peak; not compared"},
    {"Comb Filter", eAspectMeters, eConfirmed, "§1.1", "its output's peak: 7, now and then 9, on both (10-10)", "-"},
    {"FltVoice", eAspectMeters, eModelled, "§1.1", "-", "the output's peak since 10-10, as Comb Filter and Eq Peak; not compared"},
    {"Eq 2-band", eAspectMeters, eModelled, "§1.1", "-", "the output's peak since 10-10, as Comb Filter and Eq Peak; not compared"},
    {"Eq 3-band", eAspectMeters, eModelled, "§1.1", "-", "the output's peak since 10-10, as Comb Filter and Eq Peak; not compared"},
    {"Eq Peak", eAspectMeters, eConfirmed, "§1.1", "its output's peak: 7, now and then 9, on both (10-10)", "-"},
    {"LFO A", eAspectLeds, eConfirmed, "notes §194", "half of each cycle, same rate and duty as the G2's (10-10)", "-"},
    {"LFO B", eAspectLeds, eModelled, "notes §194", "-", "lit on the positive half; not compared"},
    {"LFO C", eAspectLeds, eModelled, "notes §194", "-", "lit on the positive half; not compared"},
    {"LFO Shp A", eAspectLeds, eModelled, "notes §194", "-", "lit on the positive half; not compared"},
    {"Drum Synth", eAspectLeds, eModelled, "§39.5, notes §194", "-", "the master envelope; not compared"},
    {"FM Operator", eAspectLeds, ePartial, "notes §194", "-", "the engine lights no lamp here"},
    {"Envelope ADSR", eAspectLeds, eConfirmed, "notes §194", "the gate: lit at note-on, dark at key-up through a long release, on both (10-10)", "-"},
    {"Envelope AHD", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"Envelope ADR", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"Envelop ADDSR", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"Envelope H", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"Envelope D", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"Envelope Multi", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"Envelope Mod AHD", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"Envelope Mod ADSR", eAspectLeds, eModelled, "notes §194", "-", "the gate, as Envelope ADSR; not compared"},
    {"NoiseGate", eAspectLeds, ePartial, "notes §194", "-", "the engine lights no lamp here"},
    {"Note Detector", eAspectLeds, eModelled, "notes §194", "the output; dark or brief on both in the 10-10 test, never compared lit", "-"},
    {"ValSw2-1", eAspectLeds, eModelled, "notes §194", "-", "lit while Ctrl matches; not compared"},
    {"ValSw1-2", eAspectLeds, eModelled, "notes §194", "-", "lit while Ctrl matches; not compared"},
    {"Mux8-1", eAspectLeds, eModelled, "notes §194", "the input through, mostly the same LEDs (10-10)", "a negative Ctrl shows no LED on the G2, the first LED here"},
    {"Mux1-8", eAspectLeds, eModelled, "notes §194", "-", "built 10-10; not compared"},
    {"Mux8-1X", eAspectLeds, ePartial, "notes §194", "-", "the engine lights no lamp here"},
    {"WindSw", eAspectLeds, eModelled, "notes §194", "the output; dark or brief on both in the 10-10 test, never compared lit", "-"},
    {"Invert", eAspectLeds, eConfirmed, "notes §194", "the output, toggling with the G2's at the same duty (10-10)", "-"},
    {"Pulse", eAspectLeds, eModelled, "notes §194", "the output; dark or brief on both in the 10-10 test, never compared lit", "-"},
    {"Delay", eAspectLeds, eConfirmed, "notes §194", "the output, toggling with the G2's at the same duty (10-10)", "-"},
    {"Gate", eAspectLeds, eModelled, "notes §194", "the output; dark or brief on both in the 10-10 test, never compared lit", "-"},
    {"FlipFlop", eAspectLeds, ePartial, "notes §194", "-", "the engine lights no lamp here"},
    {"8Counter", eAspectLeds, eConfirmed, "notes §194", "the count, one LED stepping up once a clock on both (10-10)", "-"},
    {"BinCounter", eAspectLeds, eModelled, "notes §194", "-", "built 10-10; not compared"},
    {"ADConv", eAspectLeds, eModelled, "notes §194", "-", "built 10-10; not compared"},
    {"Random A", eAspectLeds, eConfirmed, "notes §194", "the rate clock, half of each cycle, at the G2's rate (10-10)", "-"},
    {"Random B", eAspectLeds, eModelled, "notes §194", "-", "the rate clock, as Random A; not compared"},
    {"Pitch Tracker", eAspectLeds, ePartial, "notes §194", "-", "the engine lights no lamp here"},
    {"Sequencer Event", eAspectLeds, ePartial, "-", "-", "the engine sends no step position"},
    {"Sequencer Values", eAspectLeds, ePartial, "-", "-", "the engine sends no step position"},
    {"Sequencer Level", eAspectLeds, ePartial, "-", "-", "the engine sends no step position"},
    {"Sequencer Note", eAspectLeds, ePartial, "-", "-", "the engine sends no step position"},
    {"Sequencer Controlled", eAspectLeds, ePartial, "-", "-", "the engine sends no step position"},
    // Oscillators (2026-10-11) - Confirmed only where the whole aspect was compared on the G2; a part
    // compared is named in the evidence and the rest is open
    {"Osc A", eAspectCore, eConfirmed, "§6.2, §6.3", "captured (G2Captures/osca); its waves as OscC's (09-17)", "-"},
    {"Osc A", eAspectDials, eModelled, "§6.1a, §2", "Semi Tune: pitch exact in every capture", "Freq, Factor and Partial Tune, Cent and Pitch not compared"},
    {"Osc A", eAspectInputs, eModelled, "§6.4", "-", "Pitch and PitchVar depths not compared on the G2"},
    {"Osc A", eAspectLevel, eConfirmed, "§6.3", "the level reference OscC and OscD were measured against (09-12)", "-"},
    {"Osc A", eAspectModes, eModelled, "§6.2, §6.1a", "Waveform: every wave (09-17)", "Pitch Type, Kbt and Bypass not compared"},
    {"Osc B", eAspectCore, eConfirmed, "§6.3", "5 waves x 8 pitches, harmonics within 0.1 dB (09-17)", "Shape 127's one-sample click not modelled"},
    {"Osc B", eAspectDials, eModelled, "§6.3, §6.7, §6.8", "Shape 0-127 on Sqr and DualSaw (09-17); Shape M through a Constant (10-08)", "Tune other than Semi, Cent, Pitch and FM not compared"},
    {"Osc B", eAspectInputs, eModelled, "§6.4, §6.6-§6.8", "Shape Mod: DualSaw at +-0.5 and +-0.75 within 0.1 dB (10-08)", "Pitch, PitchVar, Sync and FM jacks not compared on the G2"},
    {"Osc B", eAspectLevel, eConfirmed, "§6.3", "harmonic levels within 0.1 dB at 8 pitches (09-17); a bare OscB is the rig's calibration (10-11)", "-"},
    {"Osc B", eAspectModes, eModelled, "§6.2, §6.1a, §6.8", "Waveform: all five (09-17)", "Pitch Type, Kbt, FM Lin/Trk and Bypass not compared"},
    {"Osc C", eAspectCore, eConfirmed, "§6.3", "Saw, Sqr50/25/10 and Tri at four pitches, harmonics within 0.1 dB (09-17)", "its Sine not captured"},
    {"Osc C", eAspectDials, eModelled, "§6.1a, §6.8", "Tune in Freq mode at 64 (10-16)", "Tune in Factor and Partial, Cent, Pitch and FM not compared"},
    {"Osc C", eAspectInputs, eModelled, "§6.4, §6.6, §6.8", "-", "Pitch, Sync and FM jacks not compared on the G2"},
    {"Osc C", eAspectLevel, eConfirmed, "§6.3", "against OscA (09-12)", "-"},
    {"Osc C", eAspectModes, eModelled, "§6.1a, §6.2, §6.8", "Wave: Saw, Sqr50/25/10, Tri (09-17); Tune Mode Freq with Kbt on tracks the key, AdditiveG2 (10-16)", "Tune Modes Factor and Partial, FM Type and Bypass not compared"},
    {"Osc D", eAspectCore, eConfirmed, "§6.3", "level and harmonics against OscA (09-12)", "-"},
    {"Osc D", eAspectDials, eModelled, "§6.1a", "-", "Tune Mode other than Semi (read since 09-28) not heard on the G2"},
    {"Osc D", eAspectInputs, eModelled, "§6.4", "-", "its Pitch jack not compared"},
    {"Osc D", eAspectLevel, eConfirmed, "§6.3", "against OscA (09-12)", "-"},
    {"Osc D", eAspectModes, eModelled, "§6.2", "Wave: against OscA (09-12)", "Kbt and Bypass not compared"},
    {"Osc Phase Mod", eAspectCore, eModelled, "§53", "-", "no capture"},
    {"Osc Shape A", eAspectCore, eModelled, "§27", "through OscShpB's captures only", "no OscShpA capture of its own"},
    {"Metallic Noise", eAspectCore, eModelled, "§66", "-", "no capture"},
    {"Osc String", eAspectCore, eModelled, "§70.4", "-", "no capture"},
    {"Driver", eAspectCore, eModelled, "§70.5", "-", "no capture"},
    {"Resonator", eAspectCore, eModelled, "§70.4a", "-", "no capture"},
    {"Osc Master", eAspectCore, eModelled, "§51", "-", "no capture"},
    {"Osc Phase Mod", eAspectDials, eModelled, "§53", "-", "-"},
    {"Osc Shape A", eAspectDials, eModelled, "§27, §6.1a, §6.6-§6.8", "-", "-"},
    {"Metallic Noise", eAspectDials, eModelled, "§66", "-", "-"},
    {"Osc String", eAspectDials, eModelled, "§70.4", "-", "-"},
    {"Driver", eAspectDials, eModelled, "§70.5", "-", "-"},
    {"Resonator", eAspectDials, eModelled, "§70.4a", "-", "-"},
    {"Osc Master", eAspectDials, eModelled, "§51", "-", "-"},
    {"Osc Phase Mod", eAspectInputs, eModelled, "§53", "-", "-"},
    {"Osc Shape A", eAspectInputs, eModelled, "§27, §6.1a, §6.6-§6.8", "-", "-"},
    {"Metallic Noise", eAspectInputs, eModelled, "§66", "-", "-"},
    {"Osc String", eAspectInputs, eModelled, "§70.4", "-", "-"},
    {"Driver", eAspectInputs, eModelled, "§70.5", "-", "-"},
    {"Resonator", eAspectInputs, eModelled, "§70.4a", "-", "-"},
    {"Osc Master", eAspectInputs, eModelled, "§51", "-", "-"},
    {"Osc Phase Mod", eAspectLevel, eModelled, "§53", "-", "-"},
    {"Osc Shape A", eAspectLevel, eModelled, "§27", "-", "-"},
    {"Metallic Noise", eAspectLevel, eModelled, "§66", "-", "-"},
    {"Osc String", eAspectLevel, eModelled, "§70.4", "-", "-"},
    {"Driver", eAspectLevel, eModelled, "§70.5", "-", "-"},
    {"Resonator", eAspectLevel, eModelled, "§70.4a", "-", "-"},
    {"Osc Master", eAspectLevel, eModelled, "§51", "-", "-"},
    {"Osc Phase Mod", eAspectModes, eModelled, "§53", "-", "-"},
    {"Osc Shape A", eAspectModes, eModelled, "§27, §6.1a, §6.6-§6.8", "-", "-"},
    {"Metallic Noise", eAspectModes, eModelled, "§66", "-", "-"},
    {"Osc String", eAspectModes, eModelled, "§70.4", "-", "-"},
    {"Driver", eAspectModes, eModelled, "§70.5", "-", "-"},
    {"Resonator", eAspectModes, eModelled, "§70.4a", "-", "-"},
    {"Osc Master", eAspectModes, eModelled, "§51", "-", "-"},
    {"Osc Shape B", eAspectCore, eConfirmed, "§27, §6.7", "every wave at Shape -1..+1, harmonics within 0.16 dB (10-02); Sine3/4 within 0.001 (09-17)", "Pulse at exactly +-1: click -33 dB against the G2's -41"},
    {"Osc Shape B", eAspectDials, eModelled, "§6.7, §6.8, §6.1a", "Shape and ShpCtrl through a Constant at every wave (10-02)", "Tune other than Semi, Cent, Pitch and FM not compared"},
    {"Osc Shape B", eAspectInputs, eModelled, "§6.4, §6.6-§6.8", "Shape Mod -1..+1 at every wave (10-02)", "Pitch, PitchVar, Sync and FM jacks not compared"},
    {"Osc Shape B", eAspectLevel, eConfirmed, "§27", "harmonics within 0.16 dB (10-02)", "-"},
    {"Osc Shape B", eAspectModes, eModelled, "§27, §6.1a", "Wave: all eight (10-02)", "Pitch Type (read since 10-07), Kbt, FM Type and Bypass not compared"},
    {"Osc Dual", eAspectCore, eConfirmed, "§12.2-§12.5", "square, saw and sub against an OscA sine; Soft at three pitches (09-12)", "-"},
    {"Osc Dual", eAspectDials, eModelled, "§12.2, §12.4", "SqrLvl, SawLvl, SubLvl, PW and Phase (09-12); Sqr M and Saw M at full (10-03)", "Tune other than Semi, Cent and Pitch not compared"},
    {"Osc Dual", eAspectInputs, eModelled, "§12.4, §6.6", "PW and Phase jacks: duty within 0.01%, harmonics within 0.15 dB (10-03)", "Pitch, PitchVar and Sync not compared"},
    {"Osc Dual", eAspectLevel, eConfirmed, "§12.5", "against an OscA sine (09-12)", "meters one value above the G2 at 9 of 10 settings (band-limited edges)"},
    {"Osc Dual", eAspectModes, eModelled, "§12.3, §12.5", "Soft on and off (09-12)", "Tune M, Kbt and Bypass not compared"},
    {"Noise Osc", eAspectCore, eModelled, "§8.2", "the 09-12 capture agrees 110 Hz-1 kHz", "above 1 kHz not re-captured since the §8 model"},
    {"Noise Osc", eAspectDials, eModelled, "§8.3, §8.4", "-", "-"},
    {"Noise Osc", eAspectInputs, eModelled, "§8.4", "-", "-"},
    {"Noise Osc", eAspectLevel, eModelled, "§8.5", "-", "-"},
    {"Noise Osc", eAspectModes, eModelled, "§8.1", "-", "-"},
    {"Noise", eAspectCore, eConfirmed, "§7.2a", "the 09-12 capture reproduced through the desk's shelf to a constant", "-"},
    {"Noise", eAspectDials, eConfirmed, "§7.2a, §7.4", "Color across its range in the 09-12 capture; meters agree at 6 of 9 settings", "the brightest settings meter one value higher"},
    {"Noise", eAspectLevel, eConfirmed, "§7.3, §7.4", "meters agree at 6 of 9 settings (09-12)", "the brightest one value higher"},
    {"Noise", eAspectModes, eModelled, "-", "-", "Bypass not compared"},
    {"Osc Percussion", eAspectCore, eConfirmed, "§40.3", "eight takes at E4: levels within 0.1 dB, pitch exact (09-25)", "-"},
    {"Osc Percussion", eAspectTiming, eConfirmed, "§40.3", "Decay 32/64/100 within 2 ms (09-25)", "-"},
    {"Osc Percussion", eAspectDials, eModelled, "§40", "Decay and Click in the takes (09-25)", "Tune other than Semi, Cent and Pitch not compared"},
    {"Osc Percussion", eAspectInputs, eModelled, "§40", "Trig through the takes (09-25)", "the Pitch jacks not compared"},
    {"Osc Percussion", eAspectLevel, eConfirmed, "§40.3", "3.36 dB over a full-scale sine against 3.45 (09-25)", "-"},
    {"Osc Percussion", eAspectModes, eModelled, "§40", "-", "Tune Mode, Kbt, Punch and Bypass not compared"},
    {"Drum Synth", eAspectCore, eConfirmed, "§39", "noise, click and oscillator energies within 0.8 dB of one constant (09-25); Kick 1-5 band balance within 0.5 dB (10-11)", "-"},
    {"Drum Synth", eAspectTiming, eModelled, "§39", "-", "decay times not compared with the 09-20 sweeps (§39.3)"},
    {"Drum Synth", eAspectDials, eModelled, "§39", "Master Freq at five dials within 0.2% (09-21); the five Kick presets, every band within 0.5 dB (10-11)", "Snare, Tom, Cymbal and Perc presets; dials swept one at a time"},
    {"Drum Synth", eAspectInputs, eModelled, "§39", "Vel and Pitch read from the inputs the G2 uses (a Constant, 09-21)", "their depths not compared"},
    {"Drum Synth", eAspectLevel, eConfirmed, "§39", "the master alone against the G2 (09-25); the five Kick presets within 0.3 dB (10-11)", "-"},
    {"Drum Synth", eAspectModes, eModelled, "§39", "-", "Noise filter type and On not compared"},
    {"FM Operator", eAspectCore, eConfirmed, "§14.2-§14.5, §14.2a", "carrier harmonics 1-10 within 0.2 dB, 0.4 with feedback (10-03); a note on an idle voice starts from L4, Backtalk DX within 0.7 dB note to note (10-16)", "-"},
    {"FM Operator", eAspectDials, eModelled, "§14.2-§14.4", "rates and levels within one 50 ms window; BrPt with L/R-Depth within 0.12 dB (10-03)", "Coarse and Fine readouts against the G2 panel (todo)"},
    {"FM Operator", eAspectInputs, eModelled, "§14.6", "-", "AMod and Pitch not yet compared (to-test); Gate, Note and Vel are read from the voice"},
    {"FM Operator", eAspectLevel, eConfirmed, "§14.5", "8.8 dB below an OscDual square against 8.45 (10-03)", "-"},
    {"FM Operator", eAspectModes, eModelled, "§14", "RateScale 7, L/R-Depth Exp and Lin, Vel 7 (10-03)", "Kbt, Sync, RatioFixed, Detune, AMod, KBEnv and Bypass not compared"},
    {"DX Router", eAspectCore, eConfirmed, "§14.1", "algorithms 1 and 2 through two operators (10-03)", "-"},
    {"DX Router", eAspectInputs, eModelled, "§14.5", "-", "the six inputs from outside the node not compared"},
    {"DX Router", eAspectLevel, eConfirmed, "§14.5", "Main 8.8 dB below an OscDual square against 8.45 (10-03)", "-"},
    {"DX Router", eAspectModes, eModelled, "§14.1", "algorithm 1 and 2, Feedback 3, 5 and 7 (10-03)", "the other algorithms not compared"},
    // Filters (2026-10-11)
    {"LP Filter", eAspectCore, eModelled, "§22", "-", "no response captured"},
    {"LP Filter", eAspectDials, eModelled, "§22", "-", "-"},
    {"LP Filter", eAspectInputs, eModelled, "§22", "-", "-"},
    {"LP Filter", eAspectLevel, eModelled, "§22", "-", "-"},
    {"LP Filter", eAspectModes, eModelled, "§22", "-", "-"},
    {"HP Filter", eAspectCore, eModelled, "§22", "-", "no response captured"},
    {"HP Filter", eAspectDials, eModelled, "§22", "-", "-"},
    {"HP Filter", eAspectInputs, eModelled, "§22", "-", "-"},
    {"HP Filter", eAspectLevel, eModelled, "§22", "-", "-"},
    {"HP Filter", eAspectModes, eModelled, "§22", "-", "-"},
    {"Nord Filter", eAspectCore, eConfirmed, "§23", "36 settings of type, slope, Freq, Res and GC within 1.44 dB, mean under 0.4 (10-11)", "-"},
    {"Nord Filter", eAspectDials, eModelled, "§23", "Freq 50 and 85, Res 0 and 100 (10-11)", "Pitch, Fm Lin and Res M not compared"},
    {"Nord Filter", eAspectInputs, eModelled, "§23.5", "the signal (10-11)", "Pitch, PitchVar, FM and Res jacks not compared"},
    {"Nord Filter", eAspectLevel, eConfirmed, "§23", "the passband in the same responses (10-11)", "-"},
    {"Nord Filter", eAspectModes, eModelled, "§23", "FilterType all four, dB/Oct both, GC on and off (10-11)", "Kbt and Bypass not compared"},
    {"Phase Filter", eAspectCore, eModelled, "§67", "-", "no capture"},
    {"Phase Filter", eAspectDials, eModelled, "§67", "-", "-"},
    {"Phase Filter", eAspectInputs, eModelled, "§67", "-", "-"},
    {"Phase Filter", eAspectLevel, eModelled, "§67", "-", "-"},
    {"Phase Filter", eAspectModes, eModelled, "§67", "-", "-"},
    {"FltVoice", eAspectCore, eModelled, "§56", "-", "no capture"},
    {"FltVoice", eAspectDials, eModelled, "§56", "-", "-"},
    {"FltVoice", eAspectInputs, eModelled, "§56", "-", "-"},
    {"FltVoice", eAspectLevel, eModelled, "§56", "-", "-"},
    {"FltVoice", eAspectModes, eModelled, "§56", "-", "-"},
    {"WahWah", eAspectCore, eModelled, "§69.9", "-", "no capture"},
    {"WahWah", eAspectDials, eModelled, "§69.9", "-", "-"},
    {"WahWah", eAspectInputs, eModelled, "§69.9", "-", "-"},
    {"WahWah", eAspectLevel, eModelled, "§69.9", "-", "-"},
    {"WahWah", eAspectModes, eModelled, "§69.9", "-", "-"},
    {"Vocoder", eAspectCore, eModelled, "§70.8", "-", "no capture"},
    {"Vocoder", eAspectInputs, eModelled, "§70.8", "-", "-"},
    {"Vocoder", eAspectLevel, eModelled, "§70.8", "-", "-"},
    {"Vocoder", eAspectModes, eModelled, "§70.8", "-", "-"},
    {"Classic Filter", eAspectCore, eConfirmed, "§21; findings 09-28", "04 Chris' Pad within 0.5 dB 100 Hz-12 kHz; a high-Res test patch's peak within 0.04 semitone (09-28)", "-"},
    {"Classic Filter", eAspectDials, eModelled, "§21, §26.2", "Freq and Res in the 09-28 tests", "Env not compared"},
    {"Classic Filter", eAspectInputs, eModelled, "§21", "the signal through the 09-28 tests", "the Env and Pitch jacks not compared"},
    {"Classic Filter", eAspectLevel, eModelled, "§21", "-", "absolute level not compared on its own"},
    {"Classic Filter", eAspectModes, eModelled, "§21", "-", "the dB/Oct tap, Kbt and Bypass not compared since the 09-14 law"},
    {"Multi Filter", eAspectCore, eConfirmed, "§10", "54 noise responses within 0.5-0.6 dB mean, 1.1 dB worst (09-12)", "-"},
    {"Multi Filter", eAspectDials, eConfirmed, "§10", "Freq and Res in the 54 responses (09-12); Pitch at +8 units within 1% (10-08)", "-"},
    {"Multi Filter", eAspectInputs, eModelled, "§10", "the signal and FreqM at +8 units within 1% (10-08)", "the direct Pitch jack not compared"},
    {"Multi Filter", eAspectLevel, eConfirmed, "§10", "within the 54 responses, the 6 dB BP included (09-12)", "-"},
    {"Multi Filter", eAspectModes, eModelled, "§10", "LP, BP and HP, 12 and 24 dB in the responses (09-12)", "GC off not captured; Kbt and Bypass not compared"},
    {"Comb Filter", eAspectCore, eConfirmed, "§13.4", "noise through all three Types (09-12)", "-"},
    {"Comb Filter", eAspectDials, eModelled, "§13", "Freq at four settings to 0.01 samples; Pitch at +63 units (10-08)", "FB, FB Mod and Level not compared on their own"},
    {"Comb Filter", eAspectInputs, eModelled, "§13", "PitchVar at +63 units (10-08)", "the other jacks not compared"},
    {"Comb Filter", eAspectLevel, eModelled, "§13.4", "-", "absolute level not compared on its own"},
    {"Comb Filter", eAspectModes, eModelled, "§13", "Type: all three (09-12)", "Kbt and Bypass not compared"},
    {"Static Filter", eAspectCore, eModelled, "§10.4", "peak heights only (2026-08)", "HP tap term not modelled; a noise capture per type"},
    {"Static Filter", eAspectDials, eModelled, "§10.4", "-", "-"},
    {"Static Filter", eAspectInputs, eModelled, "§10.4", "-", "-"},
    {"Static Filter", eAspectLevel, eModelled, "§10.4", "-", "-"},
    {"Static Filter", eAspectModes, eModelled, "§10.4", "-", "-"},
    {"Eq 2-band", eAspectCore, eModelled, "§11.2", "43 noise settings across the EQs, 0.53 dB mean (09-12)", "the high shelf changed 10-09 (0.3-0.7 dB near 3 kHz), not re-captured"},
    {"Eq 2-band", eAspectDials, eModelled, "§11.2", "Lo and Hi in the 09-12 settings", "re-capture after the 10-09 shelf change; Level not compared"},
    {"Eq 2-band", eAspectInputs, eModelled, "§11", "the signal through the captures", "unpatched behaviour not compared"},
    {"Eq 2-band", eAspectLevel, eModelled, "§11.2", "-", "not compared on its own"},
    {"Eq 2-band", eAspectModes, eModelled, "§11.2", "Lo Freq and Hi Freq in the 09-12 settings", "Bypass not compared"},
    {"Eq 3-band", eAspectCore, eConfirmed, "§11", "0.66 dB mean, 1.42 worst (09-12)", "the 1.42 dB setting (mid -13.5 dB at 8 kHz) not explained"},
    {"Eq 3-band", eAspectDials, eModelled, "§11", "Lo, MidGn, Freq and Hi in the 09-12 settings", "Level not compared"},
    {"Eq 3-band", eAspectInputs, eModelled, "§11", "the signal through the captures", "unpatched behaviour not compared"},
    {"Eq 3-band", eAspectLevel, eModelled, "§11", "-", "not compared on its own"},
    {"Eq 3-band", eAspectModes, eModelled, "§11", "Lo Freq and Hi Freq in the 09-12 settings", "Bypass not compared"},
    {"Eq Peak", eAspectCore, eConfirmed, "§11.3, §11.5", "0.57 dB mean (09-12)", "-"},
    {"Eq Peak", eAspectDials, eModelled, "§11.3, §11.5", "Freq, Gain and BW in the 09-12 settings", "Level not compared"},
    {"Eq Peak", eAspectInputs, eModelled, "§11", "the signal through the captures", "unpatched behaviour not compared"},
    {"Eq Peak", eAspectLevel, eModelled, "§11", "-", "not compared on its own"},
    {"Eq Peak", eAspectModes, eModelled, "§11", "-", "Bypass not compared"},
    // Envelopes (2026-10-11)
    {"Envelope ADSR", eAspectCore, eConfirmed, "§17.1-§17.3", "time law and curves against the 08-24 and 09-07 captures", "-"},
    {"Envelope ADSR", eAspectTiming, eConfirmed, "§17.1", "Attack, Decay and Rel against the same captures", "-"},
    {"Envelope ADSR", eAspectDials, eModelled, "§17.1, §17.6", "Attack, Decay and Rel in the captures", "Sustain's level not compared on its own"},
    {"Envelope ADSR", eAspectInputs, eModelled, "§17.4, §17.5", "-", "Gate and AM jacks not compared on the G2"},
    {"Envelope ADSR", eAspectLevel, eModelled, "§17.6", "-", "-"},
    {"Envelope ADSR", eAspectModes, eModelled, "§17.2, §17.6, §17.7", "Env Shape LogExp and LinExp: the release within one 50 ms window (10-16)", "ExpExp, LinLin, Output Type, KB and Reset not compared"},
    {"Envelope AHD", eAspectCore, eConfirmed, "§17.11a", "its segments from 10 ms to 3.35 s within 1% (10-08)", "-"},
    {"Envelope AHD", eAspectTiming, eConfirmed, "§17.11a", "as Core law (10-08)", "-"},
    {"Envelope AHD", eAspectDials, eModelled, "§17.11a", "Hold 16-80 within 1% (10-08)", "Attack and Release swept on their own not compared"},
    {"Envelope AHD", eAspectInputs, eModelled, "§17.11a", "-", "-"},
    {"Envelope AHD", eAspectLevel, eModelled, "§17.11a", "-", "-"},
    {"Envelope AHD", eAspectModes, eModelled, "§17.11a", "-", "Shape, Reset and OutType not compared"},
    {"Envelope ADR", eAspectCore, eModelled, "§17.9", "-", "stages never heard against the G2"},
    {"Envelop ADDSR", eAspectCore, eModelled, "§17.9, §17.9a", "-", "stages never heard against the G2; release fixed 10-09"},
    {"Envelope Multi", eAspectCore, eModelled, "§17.11", "-", "no capture"},
    {"Envelope ADR", eAspectTiming, eModelled, "§17.9", "-", "-"},
    {"Envelop ADDSR", eAspectTiming, eModelled, "§17.9, §17.9a", "-", "-"},
    {"Envelope Multi", eAspectTiming, eModelled, "§17.11", "-", "-"},
    {"Envelope ADR", eAspectDials, eModelled, "§17.9", "-", "-"},
    {"Envelop ADDSR", eAspectDials, eModelled, "§17.9, §17.9a", "-", "-"},
    {"Envelope Multi", eAspectDials, eModelled, "§17.11", "-", "-"},
    {"Envelope ADR", eAspectInputs, eModelled, "§17.9", "-", "-"},
    {"Envelop ADDSR", eAspectInputs, eModelled, "§17.9, §17.9a", "-", "-"},
    {"Envelope Multi", eAspectInputs, eModelled, "§17.11", "-", "-"},
    {"Envelope ADR", eAspectLevel, eModelled, "§17.9", "-", "-"},
    {"Envelop ADDSR", eAspectLevel, eModelled, "§17.9, §17.9a", "-", "-"},
    {"Envelope Multi", eAspectLevel, eModelled, "§17.11", "-", "-"},
    {"Envelope ADR", eAspectModes, eModelled, "§17.9", "-", "-"},
    {"Envelop ADDSR", eAspectModes, eModelled, "§17.9, §17.9a", "-", "-"},
    {"Envelope Multi", eAspectModes, eModelled, "§17.11", "-", "-"},
    {"Envelope H", eAspectCore, eConfirmed, "§17.9a", "Hold 12-64 within 0.1 ms (10-08)", "-"},
    {"Envelope H", eAspectTiming, eConfirmed, "§17.9a", "as Core law (10-08)", "-"},
    {"Envelope H", eAspectDials, eConfirmed, "§17.9a", "Hold 12-64 within 0.1 ms (10-08)", "-"},
    {"Envelope H", eAspectInputs, eModelled, "§17.9a", "-", "Trig and AM not compared"},
    {"Envelope H", eAspectLevel, eModelled, "§17.9a", "-", "-"},
    {"Envelope H", eAspectModes, eModelled, "§17.9a", "-", "Pos not compared"},
    {"Envelope D", eAspectCore, eModelled, "§17.9", "an untriggered EnvD is silent, as in 14 CS80project72 (09-27)", "decay never compared"},
    {"Envelope D", eAspectTiming, eModelled, "§17.9", "-", "-"},
    {"Envelope D", eAspectDials, eModelled, "§17.9", "-", "-"},
    {"Envelope D", eAspectInputs, eModelled, "§17.9", "-", "-"},
    {"Envelope D", eAspectLevel, eModelled, "§17.9", "-", "-"},
    {"Envelope D", eAspectModes, eModelled, "§17.9", "-", "-"},
    {"Envelope Mod AHD", eAspectCore, eModelled, "§17.9a, §17.10", "Hold within 0.3% (10-08)", "Attack and Decay segments not compared"},
    {"Envelope Mod AHD", eAspectTiming, eModelled, "§17.9a", "Hold within 0.3% (10-08)", "Attack and Decay not compared"},
    {"Envelope Mod AHD", eAspectDials, eModelled, "§17.10", "Hold (10-08)", "the time-mod amounts not compared"},
    {"Envelope Mod AHD", eAspectInputs, eModelled, "§17.10", "-", "time-mod jacks not checked"},
    {"Envelope Mod AHD", eAspectLevel, eModelled, "§17.10", "-", "-"},
    {"Envelope Mod AHD", eAspectModes, eModelled, "§17.10", "-", "-"},
    {"Envelope Mod ADSR", eAspectCore, eModelled, "§17.9, §17.10", "-", "its stages not compared on their own"},
    {"Envelope Mod ADSR", eAspectTiming, eModelled, "§17.10", "Decay under its mod (09-20)", "Attack and Release not compared"},
    {"Envelope Mod ADSR", eAspectDials, eModelled, "§17.10", "Decay M at -32..+32 units (09-20)", "Attack M, Sustain M and Release M not compared"},
    {"Envelope Mod ADSR", eAspectInputs, eModelled, "§17.10", "the Decay mod jack at -32..+32 units (09-20)", "Attack and Sustain mod not checked; +32 units 4.1 s against 3.6"},
    {"Envelope Mod ADSR", eAspectLevel, eModelled, "§17.10", "-", "-"},
    {"Envelope Mod ADSR", eAspectModes, eModelled, "§17.10", "-", "-"},
    // LFOs (2026-10-11)
    {"LFO A", eAspectTiming, eModelled, "§28", "the Hi range measured", "Sub and Lo ranges, BPM and Clk not checked on the G2"},
    {"LFO A", eAspectDials, eModelled, "§28, §50", "-", "Rate M not compared"},
    {"LFO A", eAspectInputs, eModelled, "§50", "-", "the rate jacks not compared"},
    {"LFO A", eAspectModes, eModelled, "§28", "-", "Wave, OutType, Kbt and Mode not compared; waves not captured"},
    {"LFO B", eAspectCore, eModelled, "§28, §28.4, §54", "-", "never captured"},
    {"LFO C", eAspectCore, eModelled, "§28", "-", "never captured"},
    {"LFO B", eAspectTiming, eModelled, "§28, §28.4, §54", "-", "-"},
    {"LFO C", eAspectTiming, eModelled, "§28", "-", "-"},
    {"LFO B", eAspectDials, eModelled, "§28, §28.4, §54", "-", "-"},
    {"LFO C", eAspectDials, eModelled, "§28", "-", "-"},
    {"LFO B", eAspectInputs, eModelled, "§28, §28.4, §54", "-", "-"},
    {"LFO C", eAspectInputs, eModelled, "§28", "-", "-"},
    {"LFO B", eAspectModes, eModelled, "§28, §28.4, §54", "-", "-"},
    {"LFO C", eAspectModes, eModelled, "§28", "-", "-"},
    {"LFO Shp A", eAspectCore, eModelled, "§28.6", "-", "waves not captured"},
    {"LFO Shp A", eAspectTiming, eModelled, "§28.6", "Rate Sub, Lo and Hi within 0.013% (09-07)", "BPM and Clk not checked"},
    {"LFO Shp A", eAspectDials, eModelled, "§28.4, §28.6", "Rate (09-07)", "Shape, Phase and the mod amounts not compared"},
    {"LFO Shp A", eAspectInputs, eModelled, "§28.4", "-", "Rst, Dir, Phase M and Shape M jacks not compared"},
    {"LFO Shp A", eAspectModes, eModelled, "§28.6", "Range (09-07)", "Wave, OutType, Kbt and Mode not compared"},
    {"Clock Generator", eAspectCore, eConfirmed, "§59", "master clock within 7-17 ppm; the engine within 1-4 ms over 66 s (10-09)", "-"},
    {"Clock Generator", eAspectDials, eModelled, "§59", "Tempo in the 10-09 test", "Swing not compared"},
    {"Clock Generator", eAspectInputs, eModelled, "§59", "-", "Reset not compared"},
    {"Clock Generator", eAspectModes, eModelled, "§59", "Source: Master (10-09)", "Internal source, Sync every and Bypass not compared"},
    // Mixers (2026-10-11)
    {"Mixer 1-1 A", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 1-1 A", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"Mixer 1-1 A", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 1-1 A", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 1-1 A", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"Mixer 1-1 S", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 1-1 S", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"Mixer 1-1 S", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 1-1 S", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 1-1 S", eAspectModes, eConfirmed, "§3.2-§3.4", "curves and Pad in the configurations (09-12); On off mutes to -93 dBFS, Exp at Lev 78/40 -12.6/-29.4 dB and Lin -4.3/-10.1 dB against 127, both laws exact (10-16)", "-"},
    {"Mixer 2-1 A", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 2-1 A", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"Mixer 2-1 A", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 2-1 A", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 2-1 A", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"Mixer 4-1 A", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 4-1 A", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 4-1 A", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 4-1 B", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 4-1 B", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"Mixer 4-1 B", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 4-1 B", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 4-1 B", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"Mixer 4-1 C", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 4-1 C", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 4-1 C", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"Mixer 4-1 S", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 4-1 S", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"Mixer 4-1 S", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 4-1 S", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 4-1 S", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"Mixer 2-1 B", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 2-1 B", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"Mixer 2-1 B", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 2-1 B", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 2-1 B", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12); Inv cancels", "the On buttons not compared"},
    {"Mixer 8-1 A", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 8-1 A", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 8-1 A", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 8-1 A", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"Mixer 8-1 B", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"Mixer 8-1 B", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"Mixer 8-1 B", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"Mixer 8-1 B", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"Mixer 8-1 B", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"MixFader", eAspectCore, eConfirmed, "§3.2, §3.6", "all eleven mixers, 106 configurations, within 0.07 dB (09-12)", "-"},
    {"MixFader", eAspectDials, eConfirmed, "§3.2, §3.3", "the levels in the 106 configurations (09-12)", "-"},
    {"MixFader", eAspectInputs, eModelled, "§3.5", "the channels in the 106 configurations (09-12)", "Chain and an unpatched channel not compared on their own"},
    {"MixFader", eAspectLevel, eConfirmed, "§3.6", "within 0.07 dB (09-12)", "-"},
    {"MixFader", eAspectModes, eModelled, "§3.2-§3.4", "curves and Pad in the configurations (09-12)", "the On buttons not compared"},
    {"Fade 1-2", eAspectCore, eConfirmed, "§4.2", "19 settings, every point within 0.001 (09-12)", "-"},
    {"Fade 1-2", eAspectDials, eModelled, "§4.2, §4.3", "Mix at 19 settings, every point within 0.001 (09-12)", "Mix M not compared"},
    {"Fade 1-2", eAspectLevel, eConfirmed, "§4.2", "19 settings, every point within 0.001 (09-12)", "-"},
    {"Fade 2-1", eAspectCore, eConfirmed, "§4.2", "as Fade 1-2 (09-12)", "-"},
    {"Fade 2-1", eAspectDials, eModelled, "§4.2, §4.3", "as Fade 1-2 (09-12)", "Mix M not compared"},
    {"Fade 2-1", eAspectLevel, eConfirmed, "§4.2", "as Fade 1-2 (09-12)", "-"},
    {"X-Fade", eAspectDials, eModelled, "§4.2, §4.3", "as Fade 1-2 (09-12)", "Mix M not compared"},
    {"X-Fade", eAspectLevel, eConfirmed, "§4.2", "as Fade 1-2 (09-12)", "-"},
    {"X-Fade", eAspectModes, eModelled, "§4.2", "-", "not compared"},
    {"Pan", eAspectDials, eConfirmed, "§4.2, §4.3", "Pan at 19 settings (09-12); PanMod 31 against an LFO, +-6.3 dB on both (10-10)", "-"},
    {"Pan", eAspectLevel, eConfirmed, "§4.2", "19 settings, every point within 0.001 (09-12)", "-"},
    {"Pan", eAspectModes, eConfirmed, "§4.2", "Log and Lin, 19 settings each (09-12)", "-"},
    {"MixStereo", eAspectCore, eConfirmed, "§5", "19 settings each, Log and Lin (09-12)", "-"},
    {"MixStereo", eAspectDials, eConfirmed, "§5.1, §5.2", "Lvl and Pan in the 19 settings (09-12)", "-"},
    {"MixStereo", eAspectInputs, eModelled, "§5.3", "the channels in the 09-12 settings", "an unpatched channel not compared"},
    {"MixStereo", eAspectLevel, eConfirmed, "§5.2", "0.14 dB below unity at the extremes, as measured (09-12)", "-"},
};

#define ASPECT_EVIDENCE_COUNT (sizeof(kAspectEvidence) / sizeof(kAspectEvidence[0]))

static int aspect_evidence_index(const char * module, tAspect aspect) {
    for (uint32_t e = 0; e < ASPECT_EVIDENCE_COUNT; e++) {
        if ((kAspectEvidence[e].aspect == aspect) && (strcmp(kAspectEvidence[e].module, module) == 0)) {
            return (int)e;
        }
    }
    return -1;
}

// --aspects: the audit - every modelled module's aspects, with their evidence where there is some
static int print_aspects(const char * paramNames) {
    uint32_t modules                  = 0;
    uint32_t aspectTotal[ASPECT_COUNT] = {0};
    uint32_t aspectState[ASPECT_COUNT][STATE_COUNT + 1] = {{0}};   // the last column is Unknown
    uint32_t all                      = 0;
    uint32_t evidenced                = 0;
    bool     seen[ASPECT_EVIDENCE_COUNT] = {false};
    int      result                   = 0;

    load_param_names(paramNames);

    for (uint32_t g = 0; g < GROUP_COUNT; g++) {
        tModuleType types[256];
        uint32_t    count = palette_group_modules(kGroups[g].group, types, 256);

        printf("\n### %s\n\n", kGroups[g].name);
        printf("| Module | Aspect | Covers | State | Evidence |\n");
        printf("|---|---|---|---|---|\n");

        for (uint32_t i = 0; i < count; i++) {
            tModule      module = {0};
            const char * name   = module_name(types[i]);
            static char  items[ASPECT_COUNT][LINE_BYTES];
            uint32_t     n[ASPECT_COUNT];
            bool         has[ASPECT_COUNT];

            module.type = types[i];

            if (sound_engine_models_module(&module) == false) {
                continue;
            }
            module_aspects(types[i], kGroups[g].group, items, n, has);
            modules++;

            for (uint32_t a = 0; a < ASPECT_COUNT; a++) {
                int e = aspect_evidence_index(name, (tAspect)a);

                if (has[a] == false) {
                    if (e >= 0) {
                        fprintf(stderr, "kAspectEvidence names %s's %s, which it does not have\n", name, kAspectName[a]);
                        result = 1;
                    }
                    continue;
                }

                if (e >= 0) {
                    const tAspectEvidence * row = &kAspectEvidence[e];

                    seen[e] = true;
                    printf("| %s | %s | %s | %s | %s: %s; open: %s |\n", name, kAspectName[a], cell(items[a]),
                           kStateName[row->state], row->refs, row->g2, row->open);
                    evidenced++;
                    aspectState[a][row->state]++;
                } else {
                    printf("| %s | %s | %s | Unknown | - |\n", name, kAspectName[a], cell(items[a]));
                    aspectState[a][STATE_COUNT]++;
                }
                aspectTotal[a]++;
                all++;
            }
        }
    }
    // notes §1 - by state as well, which is where the gaps show
    uint32_t stateTotal[STATE_COUNT + 1] = {0};

    printf("\n| Aspect | Modules | Confirmed on the G2 | Modelled | Approximate | Partial | No sound | Unknown |\n"
           "|---|---|---|---|---|---|---|---|\n");

    for (uint32_t a = 0; a < ASPECT_COUNT; a++) {
        printf("| %s | %u |", kAspectName[a], (unsigned)aspectTotal[a]);

        for (uint32_t st = 0; st <= STATE_COUNT; st++) {
            printf(" %u |", (unsigned)aspectState[a][st]);
            stateTotal[st] += aspectState[a][st];
        }
        printf("\n");
    }
    printf("| **%u aspects over %u modules, %u with evidence** | |", (unsigned)all, (unsigned)modules, (unsigned)evidenced);

    for (uint32_t st = 0; st <= STATE_COUNT; st++) {
        printf(" **%u** |", (unsigned)stateTotal[st]);
    }
    printf("\n");

    for (uint32_t e = 0; e < ASPECT_EVIDENCE_COUNT; e++) {
        if (seen[e] == false) {
            fprintf(stderr, "kAspectEvidence names '%s', which is not a modelled module\n", kAspectEvidence[e].module);
            result = 1;
        }
    }
    return result;
}

int main(int argc, char ** argv) {
    uint32_t total[STATE_COUNT + 1]       = {0};    // the last is Not implemented
    bool     evidenceSeen[EVIDENCE_COUNT] = {false};
    bool     precisionSeen[PRECISION_COUNT] = {false};
    uint32_t precisionTotal[PREC_COUNT]     = {0};
    int      result                       = 0;

    init_module_resource_cache();

    if ((argc > 1) && (strcmp(argv[1], "--aspects") == 0)) {
        return print_aspects((argc > 2) ? argv[2] : "Docs/param-validation.md");
    }
    printf("| Group | Confirmed on the G2 | Modelled | Approximate | Partial | No sound | Not implemented |\n");
    printf("|---|---|---|---|---|---|---|\n");

    for (uint32_t g = 0; g < GROUP_COUNT; g++) {
        tModuleType types[256];
        uint32_t    count                      = palette_group_modules(kGroups[g].group, types, 256);
        static char line[STATE_COUNT + 1][LINE_BYTES];
        uint32_t    n[STATE_COUNT + 1]         = {0};

        memset(line, 0, sizeof(line));

        for (uint32_t i = 0; i < count; i++) {
            tModule      module = {0};
            const char * name   = module_name(types[i]);
            int          e      = evidence_index(name);

            module.type = types[i];

            if (sound_engine_models_module(&module) == false) {
                append_name(line[STATE_COUNT], &n[STATE_COUNT], name);

                if (e >= 0) {
                    fprintf(stderr, "kEvidence has a row for '%s', which the engine does not model\n", name);
                    result = 1;
                }
            } else if (e < 0) {
                fprintf(stderr, "'%s' is modelled but has no row in kEvidence\n", name);
                result = 1;
            } else {
                evidenceSeen[e] = true;
                append_name(line[kEvidence[e].state], &n[kEvidence[e].state], name);
            }
        }
        printf("| **%s** |", kGroups[g].name);

        for (uint32_t s = 0; s <= STATE_COUNT; s++) {
            printf(" %s |", cell(line[s]));
            total[s] += n[s];
        }
        printf("\n");
    }
    {
        uint32_t all = 0;

        for (uint32_t s = 0; s <= STATE_COUNT; s++) {
            all += total[s];
        }
        printf("| **Total %u** |", (unsigned)all);

        for (uint32_t s = 0; s <= STATE_COUNT; s++) {
            printf(" **%u** |", (unsigned)total[s]);
        }
        printf("\n");
    }

    for (uint32_t g = 0; g < GROUP_COUNT; g++) {
        tModuleType types[256];
        uint32_t    count = palette_group_modules(kGroups[g].group, types, 256);

        printf("\n### %s\n\n", kGroups[g].name);
        printf("| Module | State | Reference | On the G2 | Open | Fixed point |\n");
        printf("|---|---|---|---|---|---|\n");

        for (uint32_t i = 0; i < count; i++) {
            int e = evidence_index(module_name(types[i]));

            if ((e >= 0) && (evidenceSeen[e] == true)) {
                const tEvidence * row = &kEvidence[e];

                int               p   = precision_index(row->name);

                if (p < 0) {
                    fprintf(stderr, "'%s' has no row in kPrecision\n", row->name);
                    result = 1;
                    continue;
                }
                precisionSeen[p] = true;
                precisionTotal[kPrecision[p].precision]++;

                if (strcmp(kPrecision[p].note, "-") == 0) {
                    printf("| %s | %s | %s | %s | %s | %s |\n", row->name, kStateName[row->state], row->refs, row->g2, row->open,
                           kPrecisionName[kPrecision[p].precision]);
                } else {
                    printf("| %s | %s | %s | %s | %s | %s: %s |\n", row->name, kStateName[row->state], row->refs, row->g2, row->open,
                           kPrecisionName[kPrecision[p].precision], kPrecision[p].note);
                }
            }
        }
    }

    printf("\n### Fixed point\n\n| Arithmetic | Modules |\n|---|---|\n");

    for (uint32_t k = 0; k < PREC_COUNT; k++) {
        printf("| %s | %u |\n", kPrecisionName[k], (unsigned)precisionTotal[k]);
    }

    for (uint32_t r = 0; r < PRECISION_COUNT; r++) {
        if (precisionSeen[r] == false) {
            fprintf(stderr, "kPrecision names '%s', which has no status row\n", kPrecision[r].name);
            result = 1;
        }
    }

    for (uint32_t e = 0; e < EVIDENCE_COUNT; e++) {
        if (evidenceSeen[e] == false) {
            fprintf(stderr, "kEvidence names '%s', which the palette does not offer or the engine does not model\n",
                    kEvidence[e].name);
            result = 1;
        }
    }
    return result;
}
