/*
 * modulestatus — which module types the sound engine plays, by palette group.
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
// test the canvas uses to grey a module out — and prints the status tables ready to paste. Which
// playing modules are only PARTIAL is editorial, so it is kept here in kPartial.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "moduleResourcesAccess.h"
#include "soundEngine.h"

typedef struct {
    tPaletteGroup group;
    const char *  name;
    const char *  refs;
} tGroupRow;

typedef struct {
    const char * name;
    const char * open;
} tPartialRow;

static const tGroupRow kGroups[] = {
    {palGroupOsc,    "Oscillators", "§5-§8, §12, §21.3, §27, §51, §53, §66, §70"},
    {palGroupFilter, "Filters",     "§10, §13, §21-§23, §56, §67, §69, §70"},
    {palGroupEnv,    "Envelopes",   "§17"                   },
    {palGroupLfo,    "LFOs",        "§28, §42, §50, §54"    },
    {palGroupMixer,  "Mixers",      "§3"                    },
    {palGroupLevel,  "Level",       "§16, §29, §43, §44, §48, §68, §69, §70"},
    {palGroupShaper, "Shapers",     "§3.4"                  },
    {palGroupDelay,  "Delays",      "§24, §52, §65, §69, §70"           },
    {palGroupFx,     "Effects",     "§11, §19, §20, §25, §55, §57, §69, §70"},
    {palGroupIo,     "In/Out",      "§16, §69, §70"                   },
    {palGroupSwitch, "Switches",    "§30, §45, §68, §70"              },
    {palGroupLogic,  "Logic",       "§38, §46, §68"              },
    {palGroupSeq,    "Sequencers",  "§58, §69, §70"                   },
    {palGroupRnd,    "Random",      "§47, §64, §69, §70"                },
    {palGroupNote,   "Note",        "§26, §41, §49, §69, §70"         },
    {palGroupMidi,   "MIDI",        "§70"                      },
};

// Audible, but something about the law is still a guess; to-test.md carries each check.
static const tPartialRow kPartial[] = {
    {"4 Inputs",          "silent (the jacks); a Bus source is not bridged (§69.12)"},
    {"Flanger",           "basic: a swept delay from the manual (§70.2)"},
    {"PShift",            "basic: two crossfaded taps (§70.3)"},
    {"Scratch",           "basic: two crossfaded taps, ratio law guessed (§70.3)"},
    {"Osc String",        "basic: a tuned loop; decay and damp laws guessed (§70.4)"},
    {"Resonator",         "basic: OscString's loop; Alg and inputs guessed (§70.4)"},
    {"Driver",            "a guess: not in the manual in hand (§70.5)"},
    {"NoiseGate",         "basic: follower and gate from the manual (§70.6)"},
    {"Pitch Tracker",     "counter and E2 reference are the instrument's; its detector (followers, filters, flip-flop) is not (§70.7)"},
    {"Vocoder",           "basic: 16 band-passes, band centres guessed (§70.8)"},
    {"Rnd Clock B",       "basic: RndClkA's node; StepM and Character not read (§70.9)"},
    {"Rnd Pattern",       "basic: reseeded LCG pattern; not the instrument's (§70.9)"},
    {"Sequencer Controlled", "basic: step and crossfade from the manual; T/G not read (§70.10)"},
    {"Mux8-1X",           "basic: linear crossfade; the part's program not run (§70.11)"},
    {"Level Scaler",      "basic: dB per octave from the manual (§70.12)"},
    {"Device",            "global wheel 2 reads 0 (§70.13)"},
    {"Status",            "Voice No. law guessed (§70.13)"},
    {"CtrlRcv",           "no MIDI CC reaches the engine: outputs 0 (§70.13)"},
    {"NoteRcv",           "NoteDet whatever the channel (§70.13)"},
    {"Comb Filter",       "not yet checked against the instrument's own part"                          },
    {"Envelope Multi",    "rise to an intermediate level is a guess (§17.9)"                           },
    {"Chorus",            "a third chorus in one patch passes dry (pool of 2 lines)"                   },
    {"Sequencer Note",    "the record inputs are not modelled; steps at 96 kHz whatever the clock's rate (§58)" },
    {"Sequencer Event",   "steps at 96 kHz whatever the clock's rate (§58)"                             },
    {"Sequencer Values",  "steps at 96 kHz whatever the clock's rate (§58)"                             },
    {"NoteSend",          "plays this slot only; notes to other slots and MIDI are dropped (§62)"       },
};

#define PARTIAL_COUNT (sizeof(kPartial) / sizeof(kPartial[0]))
#define GROUP_COUNT   (sizeof(kGroups) / sizeof(kGroups[0]))

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

static int partial_index(const char * name) {
    int index = -1;

    for (uint32_t p = 0; p < PARTIAL_COUNT; p++) {
        if (strcmp(kPartial[p].name, name) == 0) {
            index = (int)p;
            break;
        }
    }
    return index;
}

static void append_name(char * line, uint32_t * n, const char * name) {
    snprintf(line + strlen(line), 2048 - strlen(line), "%s%s", (*n > 0) ? ", " : "", name);
    (*n)++;
}

static const char * cell(const char * line) {
    return (line[0] != '\0') ? line : "-";
}

int main(void) {
    uint32_t totalWorking = 0;
    uint32_t totalPartial = 0;
    uint32_t totalMissing = 0;
    bool     partialSeen[PARTIAL_COUNT] = {false};
    bool     partialSilent[PARTIAL_COUNT] = {false};
    int      result = 0;

    init_module_resource_cache();

    printf("| Group | Working | Partial | Not implemented |\n");
    printf("|---|---|---|---|\n");

    for (uint32_t g = 0; g < GROUP_COUNT; g++) {
        tModuleType types[256];
        uint32_t    count = palette_group_modules(kGroups[g].group, types, 256);
        char        working[2048] = {0};
        char        partial[2048] = {0};
        char        missing[2048] = {0};
        uint32_t    nWorking = 0;
        uint32_t    nPartial = 0;
        uint32_t    nMissing = 0;

        for (uint32_t i = 0; i < count; i++) {
            tModule      module = {0};
            const char * name   = module_name(types[i]);
            int          p      = partial_index(name);

            module.type = types[i];

            if (p >= 0) {
                partialSeen[p] = true;
            }

            if (sound_engine_models_module(&module) == false) {
                append_name(missing, &nMissing, name);
                if (p >= 0) {
                    partialSilent[p] = true;
                }
            } else if (p >= 0) {
                append_name(partial, &nPartial, name);
            } else {
                append_name(working, &nWorking, name);
            }
        }
        printf("| **%s**%s%s%s | %s | %s | %s |\n", kGroups[g].name,
               (kGroups[g].refs[0] != '\0') ? " (" : "", kGroups[g].refs,
               (kGroups[g].refs[0] != '\0') ? ")" : "",
               cell(working), cell(partial), cell(missing));
        totalWorking += nWorking;
        totalPartial += nPartial;
        totalMissing += nMissing;
    }
    printf("| **Total %u** | **%u** | **%u** | **%u** |\n\n",
           (unsigned)(totalWorking + totalPartial + totalMissing),
           (unsigned)totalWorking, (unsigned)totalPartial, (unsigned)totalMissing);

    printf("| Partial module | What is still open |\n");
    printf("|---|---|\n");

    for (uint32_t p = 0; p < PARTIAL_COUNT; p++) {
        if (partialSeen[p] == true && partialSilent[p] == false) {
            printf("| %s | %s |\n", kPartial[p].name, kPartial[p].open);
        }
    }

    for (uint32_t p = 0; p < PARTIAL_COUNT; p++) {
        if (partialSeen[p] == false) {
            fprintf(stderr, "kPartial names '%s', which the palette does not offer\n", kPartial[p].name);
            result = 1;
        } else if (partialSilent[p] == true) {
            fprintf(stderr, "kPartial names '%s', which the engine does not model\n", kPartial[p].name);
            result = 1;
        }
    }
    return result;
}
