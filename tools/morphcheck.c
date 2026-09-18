/*
 * morphcheck — does a Vel or Keyb morph on a parameter actually reach the sound, per voice?
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
// Notes: Docs/code-notes/morphcheck.c.md - "// notes §k" refers there.

// notes §1

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "types.h"
#include "dataBase.h"
#include "globalVars.h"
#include "moduleResourcesAccess.h"
#include "soundEngine.h"
#include "g2Patch.h"

#define RATE               (48000.0)
#define BLOCK              (512)
#define SETTLE_BLOCKS      (20)     // past the attack before anything is measured
#define MEASURE_BLOCKS     (40)
#define PRIME_BLOCKS       (4)      // the engine's own first blocks, before the note
#define START_PHASE_SEED   (0x5EEDBEEFu)

// §26.2 - the two axes quantise, and the reference has to quantise with them or every reading is
// out by up to one step. These mirror soundEngine.c's velocity_row()/key_row() and axis_amount():
// if those change, these must.
#define VEL_MORPH_LEVELS    (32)
#define KEY_MORPH_LEVELS    (64)
#define KEY_ZERO_NOTE       (36.0)
#define KEY_SPAN            (60.0)

#define VEL_GROUP           (1)
#define KEYB_GROUP          (2)

// notes §2 - the harness has no undo stack, and undo.c reaches into the menus and the selection.
void undo_push_param_change(tModuleKey key, uint32_t paramIndex, uint32_t variation, uint32_t oldValue, uint32_t newValue) {
    (void)key;
    (void)paramIndex;
    (void)variation;
    (void)oldValue;
    (void)newValue;
}

typedef enum {
    eAxisVel = 0,
    eAxisKeyb
} tAxis;

static tModule * sModule;
static uint32_t  sVariation;
static uint32_t  sParamIndex;

static double velocity_amount(uint8_t velocity) {
    double row = round(((double)velocity * (double)(VEL_MORPH_LEVELS - 1)) / 127.0);

    return row / (double)(VEL_MORPH_LEVELS - 1);
}

static double key_amount(int32_t note) {
    int32_t row = (note < 0) ? 0 : ((note + 1) / 2);

    if (row >= KEY_MORPH_LEVELS) {
        row = KEY_MORPH_LEVELS - 1;
    }
    return (((double)row * 2.0) - KEY_ZERO_NOTE) / KEY_SPAN;
}

static double render_rms(uint32_t blocks) {
    static float buff[BLOCK * 2];
    double       sum = 0.0;

    for (uint32_t b = 0; b < blocks; b++) {
        memset(buff, 0, sizeof(buff));
        sound_engine_render(buff, BLOCK, 2);

        for (uint32_t i = 0; i < (BLOCK * 2); i++) {
            sum += (double)buff[i] * (double)buff[i];
        }
    }
    return sqrt(sum / (double)(blocks * BLOCK * 2));
}

// notes §3 - one reading: the dial and its morph ranges, then one note into a FRESH engine.
static double reading(uint8_t dial, int32_t velRange, int32_t keybRange, int32_t note, uint8_t velocity) {
    tParam * param = &sModule->param[sVariation][sParamIndex];

    param->value                  = dial;
    param->morphRange[VEL_GROUP]  = (uint8_t)((velRange < 0) ? (256 + velRange) : velRange);
    param->morphRange[KEYB_GROUP] = (uint8_t)((keybRange < 0) ? (256 + keybRange) : keybRange);

    // notes §3 - the same phases every time, or three readings of the SAME dial disagree by tens of
    // percent and the verdict is noise.
    sound_engine_stop_hosted();
    sound_engine_set_start_phase_seed(START_PHASE_SEED);
    sound_engine_start_hosted(RATE);
    sound_engine_update_from_patch();
    render_rms(PRIME_BLOCKS);

    sound_engine_note(note, velocity, true);
    render_rms(SETTLE_BLOCKS);

    double   rms = render_rms(MEASURE_BLOCKS);

    sound_engine_note(note, 64, false);
    return rms;
}

static uint8_t hand_dial(uint8_t dial, int32_t range, double amount) {
    double want = (double)dial + ((double)range * amount);

    if (want < 0.0) {
        want = 0.0;
    } else if (want > 127.0) {
        want = 127.0;
    }
    return (uint8_t)lround(want);
}

static void list_modules(void) {
    for (uint32_t location = 0; location < locationMax; location++) {
        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * module = get_module_slot(0, location, i);

            if ((module == NULL) || (module->type == 0)) {
                continue;
            }
            printf("  %s module %3u  type %3u  %s\n", (location == locationVa) ? "VA" : "FX",
                   i, module->type, gModuleProperties[module->type].name);
        }
    }
}

static void usage(void) {
    fprintf(stderr,
            "usage: morphcheck <patch.pch2> --module N --param N [--axis vel|keyb|both]\n"
            "                  [--area va|fx] [--range N] [--note N] [--velocity N]\n"
            "                  [--points N] [--tolerance PCT] [--separation PCT]\n"
            "       morphcheck <patch.pch2> --list\n"
            "\n"
            "Checks that a Vel or Keyb morph on one parameter reaches the sound PER VOICE, by\n"
            "playing the morph against the same dial turned down by hand. See\n"
            "Docs/code-notes/morphcheck.c.md.\n");
}

// notes §4 - one axis, swept. Returns false if any point failed or the whole sweep was vacuous.
static bool sweep(tAxis axis, uint8_t dial, int32_t range, int32_t fixedNote, uint8_t fixedVelocity,
                  uint32_t points, double tolerance, double separation) {
    const char * name      = (axis == eAxisVel) ? "VELOCITY" : "KEYBOARD";
    uint32_t     failed    = 0;
    uint32_t     vacuous   = 0;

    printf("\n%s axis, morph range %+d on a dial of %u\n", name, range, dial);

    if (axis == eAxisVel) {
        printf("  note %d held; velocity swept\n", fixedNote);
    } else {
        printf("  velocity %u held; note swept\n", fixedVelocity);
    }
    printf("  %-26s %10s %10s %10s  %8s %9s\n", "point (hand dial)", "morphed", "by hand", "no morph",
           "error", "separation");

    for (uint32_t p = 0; p < points; p++) {
        double  span     = (points > 1) ? ((double)p / (double)(points - 1)) : 1.0;
        int32_t note     = fixedNote;
        uint8_t velocity = fixedVelocity;
        double  amount   = 0.0;

        if (axis == eAxisVel) {
            velocity = (uint8_t)lround(1.0 + (span * 126.0));
            amount   = velocity_amount(velocity);
        } else {
            note   = (int32_t)lround(36.0 + (span * 60.0));   // C1 to C6, where the axis runs 0 to 1
            amount = key_amount(note);
        }
        uint8_t hand     = hand_dial(dial, range, amount);

        // The three readings the verdict needs: the morph, the dial moved by hand to where the morph
        // should have put it, and the dial left alone. notes §5.
        double  got      = reading(dial, (axis == eAxisVel) ? range : 0, (axis == eAxisKeyb) ? range : 0,
                                   note, velocity);
        double  want     = reading(hand, 0, 0, note, velocity);
        double  null     = reading(dial, 0, 0, note, velocity);

        double  error    = (want > 0.0) ? (fabs(got - want) / want) : ((got > 0.0) ? 1.0 : 0.0);
        double  apart    = (want > 0.0) ? (fabs(null - want) / want) : 0.0;
        char    point[32];
        const char * verdict;

        if (axis == eAxisVel) {
            snprintf(point, sizeof(point), "velocity %3u (dial %3u)", velocity, hand);
        } else {
            snprintf(point, sizeof(point), "note %3d (dial %3u)", note, hand);
        }

        if (apart < separation) {
            verdict = "INCONCLUSIVE";      // the dial does not move the sound here - nothing is being tested
            vacuous++;
        } else if (error <= tolerance) {
            verdict = "MATCH";
        } else {
            verdict = "DIFFER";
            failed++;
        }
        printf("  %-26s %10.6f %10.6f %10.6f  %7.2f%% %8.2f%%  %s\n",
               point, got, want, null, error * 100.0, apart * 100.0, verdict);
    }

    if (vacuous == points) {
        printf("  VACUOUS: the dial does not move this patch's sound at any point on the axis.\n");
        printf("           Nothing was tested. Pick another parameter, note or range.\n");
        return false;
    }

    if (failed > 0) {
        printf("  FAIL: %u of %u points do not follow the dial.\n", failed, points);
        return false;
    }
    printf("  PASS: every conclusive point follows the dial (%u of %u conclusive).\n",
           points - vacuous, points);
    return true;
}

int main(int argc, char ** argv) {
    const char * patch      = NULL;
    int32_t      moduleIdx  = -1;
    int32_t      paramIdx   = -1;
    uint32_t     location   = locationVa;
    int32_t      range      = 0;
    int32_t      note       = 60;
    uint8_t      velocity   = 100;
    uint32_t     points     = 7;
    double       tolerance  = 0.03;
    double       separation = 0.03;
    bool         wantList   = false;
    bool         doVel      = true;
    bool         doKeyb     = true;

    if (argc < 2) {
        usage();
        return 2;
    }
    patch = argv[1];

    for (int i = 2; i < argc; i++) {
        if ((strcmp(argv[i], "--module") == 0) && ((i + 1) < argc)) {
            moduleIdx = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--param") == 0) && ((i + 1) < argc)) {
            paramIdx = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--area") == 0) && ((i + 1) < argc)) {
            location = (strcmp(argv[++i], "fx") == 0) ? locationFx : locationVa;
        } else if ((strcmp(argv[i], "--range") == 0) && ((i + 1) < argc)) {
            range = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--note") == 0) && ((i + 1) < argc)) {
            note = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--velocity") == 0) && ((i + 1) < argc)) {
            velocity = (uint8_t)atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--points") == 0) && ((i + 1) < argc)) {
            points = (uint32_t)atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--tolerance") == 0) && ((i + 1) < argc)) {
            tolerance = atof(argv[++i]) / 100.0;
        } else if ((strcmp(argv[i], "--separation") == 0) && ((i + 1) < argc)) {
            separation = atof(argv[++i]) / 100.0;
        } else if (strcmp(argv[i], "--axis") == 0 && ((i + 1) < argc)) {
            const char * which = argv[++i];

            doVel  = (strcmp(which, "keyb") != 0);
            doKeyb = (strcmp(which, "vel") != 0);
        } else if (strcmp(argv[i], "--list") == 0) {
            wantList = true;
        } else {
            usage();
            return 2;
        }
    }

    if (g2_plugin_load_patch(patch, 0) == false) {
        fprintf(stderr, "morphcheck: cannot load %s\n", patch);
        return 1;
    }
    gSlot      = 0;
    sVariation = gPatchDescr[0].activeVariation;

    printf("%s\n", patch);

    if (wantList == true) {
        list_modules();
        return 0;
    }

    if ((moduleIdx < 0) || (paramIdx < 0)) {
        fprintf(stderr, "morphcheck: --module and --param are required (or --list)\n");
        return 2;
    }
    sModule     = get_module_slot(0, location, (uint32_t)moduleIdx);
    sParamIndex = (uint32_t)paramIdx;

    if ((sModule == NULL) || (sModule->type == 0)) {
        fprintf(stderr, "morphcheck: no module at index %d\n", moduleIdx);
        return 1;
    }
    uint8_t dial = sModule->param[sVariation][sParamIndex].value;

    // notes §6 - a range that lands the dial on the far end without clamping past it
    if (range == 0) {
        range = (dial >= 64) ? -(int32_t)dial : (int32_t)(127 - dial);
    }
    printf("module %d (%s), parameter %d, dial %u\n",
           moduleIdx, gModuleProperties[sModule->type].name, paramIdx, dial);

    sound_engine_set_sample_rate(RATE);
    sound_engine_start_hosted(RATE);

    // notes §7 - one throwaway reading first. The first note of a run measures a little low whatever
    // the settings, and a sweep's first point is otherwise the one that carries it.
    reading(dial, 0, 0, note, velocity);

    bool ok = true;

    if (doVel == true) {
        ok = sweep(eAxisVel, dial, range, note, velocity, points, tolerance, separation) && ok;
    }

    if (doKeyb == true) {
        ok = sweep(eAxisKeyb, dial, range, note, velocity, points, tolerance, separation) && ok;
    }
    sound_engine_stop_hosted();
    printf("\n%s\n", (ok == true) ? "morphcheck: PASS" : "morphcheck: FAIL");
    return (ok == true) ? 0 : 1;
}
