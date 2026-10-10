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
    {"Osc A",                 eConfirmed,   "§6.3",                      "captures (G2Captures/osca); OscC/OscD level against it 09-12", "-"},
    {"Osc B",                 eConfirmed,   "§6.3, §6.6-§6.8",           "5 waves x 8 pitches, harmonics within 0.1 dB (09-17); DualSaw below zero 0.1 dB (10-08)", "FM and Sync not compared on the G2; Shape 127's one-sample click not modelled"},
    {"Osc C",                 eConfirmed,   "§6.3",                      "Saw, Sqr50/25/10, Tri at 440-3520 Hz (09-17)", "FM not compared on the G2"},
    {"Osc D",                 eConfirmed,   "§6.3, §6.1a",               "level and harmonics against OscA (09-12)", "Pitch Type read since 09-28, not heard on the G2"},
    {"Osc Phase Mod",         eModelled,    "§53",                       "-", "-"},
    {"Osc Shape A",           eModelled,    "§27, §6.7",                 "through OscShpB's captures only", "no OscShpA capture of its own"},
    {"Osc Shape B",           eConfirmed,   "§27, §6.7",                 "every wave, Shape Mod -1..+1, harmonics within 0.16 dB (10-02); Sine3/4 within 0.001 (09-17)", "Pulse at exactly +-1: click -33 dB against the G2's -41"},
    {"Osc Dual",              eConfirmed,   "§12.4, §12.5",              "levels, duty, phase (09-12); PW and Phase mod (10-03)", "-"},
    {"Noise Osc",             eConfirmed,   "§8.2-§8.5",                 "09-12 capture agrees 110 Hz-1 kHz (the old model's fit)", "level above 1 kHz not re-captured since the §8 model"},
    {"Noise",                 eConfirmed,   "§7.2a",                     "09-12 capture reproduced through the desk's shelf to a constant", "-"},
    {"Sampler",               ePartial,     "notes §213",                "engine only - the G2 has no such module", "zones by nearest root key until the key map is read; mono files only"},
    {"Metallic Noise",        eModelled,    "§66",                       "-", "no capture"},
    {"Osc Percussion",        eConfirmed,   "§40",                       "8 takes: levels 0.1 dB, decay 2 ms, pitch exact (09-25)", "-"},
    {"Drum Synth",            eConfirmed,   "§39",                       "Master Freq 0.2% (09-21); noise, click, oscillator energies within 0.8 dB (09-25)", "level curve set by §39.3, not fitted to the 09-20 sweeps"},
    {"Osc String",            eModelled,    "§70.4",                     "-", "-"},
    {"FM Operator",           eConfirmed,   "§14.2-§14.6",               "envelope, level, FM depth, feedback, level and rate scaling, velocity (10-03)", "AMod and Pitch inputs (§14.6) not yet compared (to-test); Gate/Note/Vel jacks read from the voice"},
    {"DX Router",             eConfirmed,   "§14.1, §14.5",              "Main level 8.8 dB against 8.45 (10-03)", "Out1-Out6 not used outside the node"},
    {"Driver",                eModelled,    "§70.5",                     "-", "-"},
    {"Resonator",             eModelled,    "§70.4a",                    "-", "-"},
    {"Osc Master",            eModelled,    "§51",                       "-", "-"},
    // Filters
    {"LP Filter",             eModelled,    "§22",                       "FM input at 2 semitones a unit only (notes §160)", "no response captured"},
    {"HP Filter",             eModelled,    "§22",                       "-", "no response captured"},
    {"Nord Filter",           eModelled,    "§23",                       "-", "no response captured"},
    {"Classic Filter",        eModelled,    "§21",                       "18 captures settled the loop, taken before the current law (09)", "not re-compared since the 09-14 law"},
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
    {"Envelope ADSR",         eConfirmed,   "§17.1-§17.8",               "time law and curves agree with the 08-24 and 09-07 captures (not kept)", "-"},
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
    {"Mixer 1-1 S",           eConfirmed,   "§3",                        "as Mixer 1-1 A", "-"},
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
    {"Compressor",            eModelled,    "§25",                       "earlier fits (not kept) predate the current law", "not re-captured since 09-14"},
    {"Digitizer",             eModelled,    "§69.8",                     "-", "-"},
    {"FreqShift",             eModelled,    "§57",                       "-", "-"},
    {"Flanger",               eModelled,    "§70.2",                     "-", "-"},
    {"Chorus",                eConfirmed,   "§19",                       "tap spans, rate, mix (09-07); unity at Amount 0 (09-14)", "-"},
    {"Phaser",                eModelled,    "§55",                       "-", "-"},
    {"PShift",                eModelled,    "§70.3",                     "-", "-"},
    {"Reverb",                eConfirmed,   "§20",                       "19 captures: onsets, stereo, Time law (09)", "captured decays 6-12% long (their fits)"},
    {"Scratch",               eModelled,    "§70.3",                     "-", "-"},
    // In/Out
    {"2 Outputs",             eConfirmed,   "§63, notes §167, §198",     "Pad +6 dB (09-07); an unpatched socket is silent (10-10); the path every G2 check goes through", "-"},
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
                } else {
                    printf("| %s | %s | %s | Unknown | - |\n", name, kAspectName[a], cell(items[a]));
                }
                aspectTotal[a]++;
                all++;
            }
        }
    }
    printf("\n| Aspect | Modules |\n|---|---|\n");

    for (uint32_t a = 0; a < ASPECT_COUNT; a++) {
        printf("| %s | %u |\n", kAspectName[a], (unsigned)aspectTotal[a]);
    }
    printf("| **%u aspects over %u modules, %u with evidence** | |\n", (unsigned)all, (unsigned)modules, (unsigned)evidenced);

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
        printf("| Module | State | Reference | On the G2 | Open |\n");
        printf("|---|---|---|---|---|\n");

        for (uint32_t i = 0; i < count; i++) {
            int e = evidence_index(module_name(types[i]));

            if ((e >= 0) && (evidenceSeen[e] == true)) {
                const tEvidence * row = &kEvidence[e];

                printf("| %s | %s | %s | %s | %s |\n", row->name, kStateName[row->state], row->refs, row->g2, row->open);
            }
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
