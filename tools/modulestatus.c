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
// test the canvas uses to grey a module out — and prints the Plays/Silent lists ready to paste.
// It cannot know which of the playing ones are UNSETTLED: that is editorial and lives in the doc.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "moduleResourcesAccess.h"
#include "soundEngine.h"

static const char * kGroupName[palGroupCount] = {
    [palGroupIo] = "In/Out",     [palGroupOsc] = "Oscillators", [palGroupRnd] = "Random",
    [palGroupFilter] = "Filters", [palGroupDelay] = "Delays",   [palGroupLevel] = "Level",
    [palGroupSwitch] = "Switches", [palGroupSeq] = "Sequencers", [palGroupNote] = "Note",
    [palGroupLfo] = "LFOs",      [palGroupEnv] = "Envelopes",   [palGroupFx] = "Effects",
    [palGroupShaper] = "Shapers", [palGroupMixer] = "Mixers",   [palGroupLogic] = "Logic",
    [palGroupMidi] = "MIDI",
};

int main(void) {
    uint32_t plays = 0;
    uint32_t total = 0;

    init_module_resource_cache();

    for (uint32_t g = 0; g < (uint32_t)palGroupCount; g++) {
        tModuleType  types[256];
        uint32_t     count = palette_group_modules((tPaletteGroup)g, types, 256);
        char         playLine[2048] = {0};
        char         quietLine[2048] = {0};
        uint32_t     nPlay = 0;
        uint32_t     nQuiet = 0;

        if (count == 0) {
            continue;
        }

        for (uint32_t i = 0; i < count; i++) {
            tModule      module = {0};
            const char * name   = "?";
            char *       line;
            uint32_t *   n;

            for (uint32_t e = 0; e < array_size_palette_list(); e++) {
                if (gPaletteList[e].moduleType == types[i]) {
                    name = gPaletteList[e].menuLabel;
                    break;
                }
            }

            module.type = types[i];
            total++;

            if (sound_engine_models_module(&module) == true) {
                line = playLine; n = &nPlay; plays++;
            } else {
                line = quietLine; n = &nQuiet;
            }
            snprintf(line + strlen(line), 1024, "%s`%s`", (*n > 0) ? ", " : "", name);
            (*n)++;
        }
        printf("## %s\n\n", (kGroupName[g] != NULL) ? kGroupName[g] : "?");

        if (nPlay > 0) {
            printf("**Plays** (%u): %s\n\n", (unsigned)nPlay, playLine);
        }

        if (nQuiet > 0) {
            printf("**Silent** (%u): %s\n\n", (unsigned)nQuiet, quietLine);
        }
    }
    printf("TOTAL: %u of %u module types play\n", (unsigned)plays, (unsigned)total);
    return 0;
}
