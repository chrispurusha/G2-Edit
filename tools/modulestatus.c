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
    {palGroupOsc,    "Oscillators", "§5-§8, §12, §21.3, §27, §51, §53, §66"},
    {palGroupFilter, "Filters",     "§10, §13, §21-§23, §56, §67, §69"},
    {palGroupEnv,    "Envelopes",   "§17"                   },
    {palGroupLfo,    "LFOs",        "§28, §42, §50, §54"    },
    {palGroupMixer,  "Mixers",      "§3"                    },
    {palGroupLevel,  "Level",       "§16, §29, §43, §44, §48, §68, §69"},
    {palGroupShaper, "Shapers",     "§3.4"                  },
    {palGroupDelay,  "Delays",      "§24, §52, §65, §69"           },
    {palGroupFx,     "Effects",     "§11, §19, §20, §25, §55, §57, §69"},
    {palGroupIo,     "In/Out",      "§16, §69"                   },
    {palGroupSwitch, "Switches",    "§30, §45, §68"              },
    {palGroupLogic,  "Logic",       "§38, §46, §68"              },
    {palGroupSeq,    "Sequencers",  "§58, §69"                   },
    {palGroupRnd,    "Random",      "§47, §64, §69"                },
    {palGroupNote,   "Note",        "§26, §41, §49, §69"         },
    {palGroupMidi,   "MIDI",        ""                      },
};

// Audible, but something about the law is still a guess; to-test.md carries each check.
static const tPartialRow kPartial[] = {
    {"ValSw2-1",          "switches at the threshold (manual); its part tests equality within 1/2 unit (§68.2)"},
    {"ValSw1-2",          "switches at the threshold (manual); its part tests equality within 1/2 unit (§68.2)"},
    {"4 Inputs",          "silent (the jacks); a Bus source is not bridged (§69.12)"},
    {"Note Detector",     "release velocity is not kept, so RVel reads 0 (§69.11)"},
    {"Noise Osc",         "Q and level measured, not yet read from the reference model (§8)"                  },
    {"Comb Filter",       "not yet checked against the reference model"                          },
    {"Multi Filter",      "GComp not yet checked against the reference model"                    },
    {"Envelop ADDSR",     "KB gate and Reset not read (§17.9)"                                         },
    {"Envelope ADR",      "KB gate and Reset not read (§17.9)"                                         },
    {"Envelope AHD",      "KB gate and Reset not read (§17.9)"                                         },
    {"Envelope D",        "KB gate and Reset not read (§17.9)"                                         },
    {"Envelope H",        "KB gate and Reset not read (§17.9)"                                         },
    {"Envelope Mod ADSR", "KB gate and Reset not read (§17.9)"                                         },
    {"Envelope Mod AHD",  "KB gate and Reset not read (§17.9)"                                         },
    {"Envelope Multi",    "rise to an intermediate level is a guess (§17.9)"                           },
    {"ModAmt",            "Enable button unconfirmed (§29.4)"                                          },
    {"Chorus",            "a third chorus in one patch passes dry (pool of 2 lines)"                   },
    {"SwOnOffT",          "untested on hardware (§30)"                                                 },
    {"Glide",             "Lin and the Time table are the instrument's; the Log shape is ours (§36.1)" },
    {"Delay",             "the time Mod input is not read (§46)"                                       },
    {"Osc Phase Mod",     "Tri's corner correction and the Sync input not modelled (§53)"          },
    {"FreqShift",         "Sub range: the module's word and the readout disagree 12x (§57)"          },
    {"FltVoice",          "the fine-pitch table offset is read as none, not decoded (§56)"         },
    {"Sequencer Note",    "the record inputs are not modelled; steps at 96 kHz whatever the clock's rate (§58)" },
    {"Sequencer Event",   "steps at 96 kHz whatever the clock's rate (§58)"                             },
    {"Sequencer Values",  "steps at 96 kHz whatever the clock's rate (§58)"                             },
    {"Clock Generator",   "Master follows a fixed 120 BPM, not the global clock (§59)"                  },
    {"NoteSend",          "plays this slot only; notes to other slots and MIDI are dropped (§62)"       },
    {"Delay Single A",    "the tap's interpolator is ours, not the instrument's table (§52)"          },
    {"Delay Single B",    "the tap's interpolator is ours, not the instrument's table (§52)"          },
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
