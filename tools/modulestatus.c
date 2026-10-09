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
    {"Eq 2-band",             eConfirmed,   "§11.2",                     "43 noise settings across the EQs, 0.53 dB mean (09-12)", "bypass not checked; deep cuts use a stable SVF (§11.5)"},
    {"Eq 3-band",             ePartial,     "§11.3",                     "0.66 dB mean, 1.42 worst (09-12)", "mid-band width is an assumed 1-octave formula"},
    {"Eq Peak",               eConfirmed,   "§11.3",                     "0.57 dB mean (09-12)", "deep cuts use a stable SVF (§11.5)"},
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
    {"Fade 1-2",              eConfirmed,   "§4.2",                      "19 settings, every point within 0.001 (09-12)", "mod-input depth not captured"},
    {"Fade 2-1",              eConfirmed,   "§4.2",                      "as Fade 1-2", "mod-input depth not captured"},
    {"X-Fade",                eConfirmed,   "§4.2",                      "as Fade 1-2", "mod-input depth not captured"},
    {"Pan",                   eConfirmed,   "§4.2",                      "as Fade 1-2", "mod-input depth not captured"},
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
    {"LevMult",               eApproximate, "-",                         "-", "never compared"},
    {"MinMax",                eModelled,    "§43",                       "-", "-"},
    {"ModAmt",                eModelled,    "§29",                       "parameter display only (param-validation)", "-"},
    {"NoiseGate",             eModelled,    "§70.6",                     "-", "-"},
    {"EnvFollow",             eModelled,    "§69.4",                     "-", "times at 24 kHz when not up-rated not modelled (notes §203)"},
    {"Red2Blue",              eModelled,    "§68.8, notes §203",         "-", "-"},
    {"Blue2Red",              eModelled,    "§68.8",                     "-", "-"},
    // Shapers
    {"Saturate",              eApproximate, "-",                         "-", "one ramp per mode would capture it (capture-inventory)"},
    {"Clip",                  eApproximate, "-",                         "-", "as Saturate"},
    {"OverDrive",             eConfirmed,   "§71",                       "energy above 6 kHz in 14 CS80project72: -41.5 against -41.6 dB (10-04)", "-"},
    {"ShpExp",                eApproximate, "-",                         "-", "as Saturate"},
    {"WaveWrap",              eApproximate, "-",                         "-", "as Saturate"},
    {"ShpStatic",             eApproximate, "-",                         "-", "as Saturate"},
    {"Rect",                  eApproximate, "-",                         "-", "-"},
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
    {"2 Outputs",             eConfirmed,   "§63, notes §198",           "Pad +6 dB (09-07); the path every G2 check goes through", "-"},
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

int main(void) {
    uint32_t total[STATE_COUNT + 1]       = {0};    // the last is Not implemented
    bool     evidenceSeen[EVIDENCE_COUNT] = {false};
    int      result                       = 0;

    init_module_resource_cache();

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
