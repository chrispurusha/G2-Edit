/*
 * The G2 Editor application.
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

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>
#pragma clang diagnostic pop

#include "mutatorUI.h"
#include "defs.h"
#include "types.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "globalVars.h"
#include "fileDialogue.h"

tMutatorState            gMutator                       = {0};

static const char *const kCategoryLabel[mutatorCatNone] = {
    "OscFreq", "OscFine", "Envelope", "SeqValue", "SeqEvent", "Delays", "Effects",
};

static const char *const kOperatorLabel[4]              = {"Mutate", "Randomise", "Interpolate", "Cross"};

// Per-role palette ported from the decompiled original editor's Mutator dialog constructor
// (CDialogMutaBackground::CDialogMutaBackground: Mother/Children built with
// _kSingleLineColor/_kSingleBackColor, Father with _kCoupleLineColor/_kCoupleBackColor, the Patch
// Variations row with _kVariationLineColor/_kVariationBackColor, Temporary Storage - "Gene Bank" -
// with _kGeneBankLineColor/_kGeneBankBackColor). Values are the same colours, normalized from the
// original's 16-bit-per-channel constants.
#define MUTATOR_RGB_SINGLE_BACK       {0.0, 0.0, 0.5}
#define MUTATOR_RGB_SINGLE_LINE       {0.0, 1.0, 1.0}
#define MUTATOR_RGB_COUPLE_BACK       {0.5, 0.5, 0.0}
#define MUTATOR_RGB_COUPLE_LINE       {1.0, 1.0, 0.0}
#define MUTATOR_RGB_VARIATION_BACK    {0.87, 0.87, 0.87}
#define MUTATOR_RGB_VARIATION_LINE    {0.0, 0.0, 0.0}
#define MUTATOR_RGB_GENEBANK_BACK     {0.0, 0.5, 0.5}
#define MUTATOR_RGB_GENEBANK_LINE     {0.0, 1.0, 0.0}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void open_mutator_panel(uint32_t slot) {
    memset(&gMutator, 0, sizeof(gMutator));
    gMutator.active                          = true;
    gMutator.slot                            = slot;
    gMutator.schemaCount                     = mutator_build_schema(slot, gMutator.schema, MUTATOR_MAX_SCHEMA);
    gMutator.focus                           = mutatorFocusNone;
    gMutator.draggingSlider                  = -1;
    gMutator.mutateProb                      = 0.3;
    gMutator.mutateRange                     = 0.3;
    gMutator.linkProbRange                   = true;
    gMutator.crossProb                       = 0.5;
    gMutator.panelRect.coord                 = (tCoord){
        80.0, 80.0
    };
    gMutator.panelRect.size                  = (tSize){
        700.0, 320.0
    };

    // Seed Mother from whatever's currently playing, so there's something to work with right away.
    uint32_t activeVariation = gPatchDescr[slot].activeVariation;

    mutator_read_genome(gMutator.schema, gMutator.schemaCount, slot, activeVariation, gMutator.genome[mutatorFocusMother]);
    gMutator.genomeValid[mutatorFocusMother] = true;
    gMutator.focus                           = mutatorFocusMother;
}

void close_mutator_panel(void) {
    if (gMutator.auditionBackupValid) {
        mutator_apply_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot,
                             gMutator.auditionVariation, gMutator.auditionBackup, false);
    }
    gMutator.active = false;
}

// ─── Audition ────────────────────────────────────────────────────────────────

static void audition(int32_t boxIndex) {
    if ((boxIndex < 0) || (boxIndex >= MUTATOR_NUM_BOXES) || !gMutator.genomeValid[boxIndex]) {
        return;
    }

    if (!gMutator.auditionBackupValid) {
        gMutator.auditionVariation   = gPatchDescr[gMutator.slot].activeVariation;
        mutator_read_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot,
                            gMutator.auditionVariation, gMutator.auditionBackup);
        gMutator.auditionBackupValid = true;
    }
    mutator_apply_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot,
                         gMutator.auditionVariation, gMutator.genome[boxIndex], false);
    gMutator.focus = boxIndex;
}

// ─── Operators ───────────────────────────────────────────────────────────────

static void run_mutate(void) {
    if (!gMutator.genomeValid[mutatorFocusMother]) {
        return;
    }

    for (int32_t c = 0; c < 6; c++) {
        mutator_mutate(gMutator.genome[mutatorFocusMother], gMutator.schema, gMutator.schemaCount,
                       gMutator.mutateProb, gMutator.mutateRange, &gMutator.locks, gMutator.genome[1 + c]);
        gMutator.genomeValid[1 + c] = true;
    }

    audition(1);
}

static void run_randomize(void) {
    int32_t base = (gMutator.focus != mutatorFocusNone) ? gMutator.focus : mutatorFocusMother;

    if (!gMutator.genomeValid[base]) {
        return;
    }

    for (int32_t c = 0; c < 6; c++) {
        mutator_randomize(gMutator.genome[base], gMutator.schema, gMutator.schemaCount, &gMutator.locks, gMutator.genome[1 + c]);
        gMutator.genomeValid[1 + c] = true;
    }

    audition(1);
}

static void run_interpolate(void) {
    if (!gMutator.genomeValid[mutatorFocusMother] || !gMutator.genomeValid[mutatorFocusFather]) {
        return;
    }

    for (int32_t c = 0; c < 6; c++) {
        double t = (double)(c + 1) / 7.0;
        mutator_interpolate(gMutator.genome[mutatorFocusMother], gMutator.genome[mutatorFocusFather],
                            gMutator.schema, gMutator.schemaCount, t, gMutator.genome[1 + c]);
        gMutator.genomeValid[1 + c] = true;
    }

    audition(1);
}

static void run_cross(void) {
    if (!gMutator.genomeValid[mutatorFocusMother] || !gMutator.genomeValid[mutatorFocusFather]) {
        return;
    }

    for (int32_t c = 0; c < 6; c++) {
        mutator_cross(gMutator.genome[mutatorFocusMother], gMutator.genome[mutatorFocusFather],
                      gMutator.schema, gMutator.schemaCount, gMutator.crossProb, gMutator.genome[1 + c]);
        gMutator.genomeValid[1 + c] = true;
    }

    audition(1);
}

static void copy_focused_to(int32_t targetBox) {
    if (gMutator.focus == mutatorFocusNone) {
        return;
    }
    memcpy(gMutator.genome[targetBox], gMutator.genome[gMutator.focus], sizeof(gMutator.genome[0]));
    gMutator.genomeValid[targetBox] = true;
}

// Manual's `S` shortcut: "Save focused variation to first empty box in the Temporary Storage."
static void save_focused_to_storage(void) {
    if (gMutator.focus == mutatorFocusNone) {
        return;
    }

    for (int s = 0; s < MUTATOR_NUM_STORAGE; s++) {
        if (!gMutator.storageValid[s]) {
            memcpy(gMutator.storageGenome[s], gMutator.genome[gMutator.focus], sizeof(gMutator.genome[0]));
            gMutator.storageValid[s] = true;
            return;
        }
    }
}

// ─── Commit to a real variation ──────────────────────────────────────────────
// Cmd-click on a Patch Variation box permanently writes the focused box's genome into that real
// variation (undoable), unlike a plain click which only loads/auditions. This is the only way
// anything from the Mutator becomes permanent - a stand-in for the Temporary Storage row's "v"
// (commit row) button until that's built.

static int32_t  gPendingCommitBox       = -1;
static uint32_t gPendingCommitVariation = 0;

static void on_variation_commit_confirmed(bool confirmed) {
    if (!confirmed || (gPendingCommitBox < 0)) {
        gPendingCommitBox = -1;
        return;
    }
    mutator_apply_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot,
                         gPendingCommitVariation, gMutator.genome[gPendingCommitBox], true);
    gPendingCommitBox = -1;
}

static void request_commit_to_variation(uint32_t variationIndex) {
    if (gMutator.focus == mutatorFocusNone) {
        return;
    }
    gPendingCommitBox       = gMutator.focus;
    gPendingCommitVariation = variationIndex;

    char title[48];
    char message[192];
    snprintf(title, sizeof(title), "Commit to Variation %u", variationIndex + 1);
    snprintf(message, sizeof(message),
             "Overwrite edit buffer variation %u with the focused sound? (Cmd-Z).",
             variationIndex + 1);
    show_confirm_dialogue_async(title, message, "Commit...", on_variation_commit_confirmed);
}

// ─── Rendering ───────────────────────────────────────────────────────────────

static tRectangle draw_category_button(double x, double y, double w, double h, const char * label, bool on) {
    tRgb bg = on ? (tRgb)RGB_GREY_9 : (tRgb)RGB_GREY_5;

    return draw_button(mainArea, (tRectangle){{x, y}, {w, h}}, label, bg);
}

// Fits mutator_chromosome_path()'s turtle-walk path into rect (uniform scale, aspect preserved,
// centered - mirrors the original editor's DrawChromosome bounding-box fit) and draws it as a
// connected polyline. Not a single bezier: the path can have hundreds of points, and
// render_bezier_curve() only draws one curve through three control points.
static void draw_chromosome(tRectangle rect, const uint8_t * genome, tRgb lineColour) {
    static tCoord path[MUTATOR_MAX_SCHEMA];
    uint32_t      count = gMutator.schemaCount;

    if (count < 2) {
        return;
    }
    mutator_chromosome_path(genome, count, path);

    double        minX = path[0].x, maxX = path[0].x;
    double        minY = path[0].y, maxY = path[0].y;

    for (uint32_t i = 1; i < count; i++) {
        if (path[i].x < minX) {
            minX = path[i].x;
        }

        if (path[i].x > maxX) {
            maxX = path[i].x;
        }

        if (path[i].y < minY) {
            minY = path[i].y;
        }

        if (path[i].y > maxY) {
            maxY = path[i].y;
        }
    }

    double        xRange  = (maxX > minX) ? (maxX - minX) : 1.0;
    double        yRange  = (maxY > minY) ? (maxY - minY) : 1.0;
    double        scaleX  = rect.size.w / xRange;
    double        scaleY  = rect.size.h / yRange;
    double        scale   = ((scaleX < scaleY) ? scaleX : scaleY) * 0.9;
    double        midX    = (minX + maxX) * 0.5;
    double        midY    = (minY + maxY) * 0.5;
    double        centreX = rect.coord.x + (rect.size.w * 0.5);
    double        centreY = rect.coord.y + (rect.size.h * 0.5);

    set_rgb_colour(lineColour);
    tCoord        prev    = {centreX + ((path[0].x - midX) * scale), centreY + ((path[0].y - midY) * scale)};

    for (uint32_t i = 1; i < count; i++) {
        tCoord next = {centreX + ((path[i].x - midX) * scale), centreY + ((path[i].y - midY) * scale)};
        render_line(mainArea, prev, next, 1.0);
        prev = next;
    }
}

void render_mutator_panel(void) {
    if (!gMutator.active) {
        return;
    }
    tRectangle               panel                       = gMutator.panelRect;
    double                   x                           = panel.coord.x;
    double                   y                           = panel.coord.y;
    double                   w                           = panel.size.w;
    double                   margin                      = 10.0;
    double                   titleH                      = 24.0;

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_rectangle_with_border(mainArea, panel);

    // Title bar (also the drag handle)
    gMutator.titleBarRect    = (tRectangle){{
                                                x, y
                                            }, {
                                                w, titleH
                                            }
    };
    set_rgb_colour((tRgb)RGB_GREY_3);
    render_rectangle(mainArea, gMutator.titleBarRect);
    set_rgb_colour((tRgb)RGB_BLACK);
    render_text(mainArea, (tRectangle){{x + margin, y + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Patch Mutator");

    double                   closeW                      = get_text_width((char *)"Close", STANDARD_BUTTON_TEXT_HEIGHT, eCache) + 4.0;

    gMutator.closeButtonRect = draw_button(mainArea, (tRectangle){{x + w - closeW - 4.0, y + 4.0}, {closeW, STANDARD_BUTTON_TEXT_HEIGHT}},
                                           (char *)"Close", (tRgb)RGB_BACKGROUND_GREY);

    double                   rowY                        = y + titleH + margin;

    // Region 1: operator buttons + sliders. Sized to their own text (plus a little padding)
    // rather than a fixed width, and centred as a group in the panel.
    double                   btnH                        = STANDARD_BUTTON_TEXT_HEIGHT;
    double                   btnGap                      = 8.0;
    double                   btnW[4];
    double                   btnRowW                     = 0.0;

    for (int i = 0; i < 4; i++) {
        btnW[i]  = get_text_width((char *)kOperatorLabel[i], STANDARD_BUTTON_TEXT_HEIGHT, eCache) + 14.0;
        btnRowW += btnW[i] + ((i > 0) ? btnGap : 0.0);
    }

    double                   btnRowX                     = x + ((w - btnRowW) / 2.0);

    for (int i = 0; i < 4; i++) {
        tRgb bg = gMutator.operatorPressed[i] ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;

        gMutator.operatorRect[i] = draw_button(mainArea, (tRectangle){{btnRowX, rowY}, {btnW[i], btnH}},
                                               (char *)kOperatorLabel[i], bg);
        btnRowX                 += btnW[i] + btnGap;
    }

    rowY                    += btnH + 12.0;

    // Real rotary dials (matching the original editor) instead of flat bars - Prob/Range on the
    // left (linked by default), Cross Prob on the right.
    double                   dialW                       = 26.0;
    double                   dialRowH                    = STANDARD_TEXT_HEIGHT * 2.0 + dialW;
    char                     buf[16];

    snprintf(buf, sizeof(buf), "%d%%", (int)lround(gMutator.mutateProb * 100.0));
    gMutator.probSliderRect  = render_dial_with_text(mainArea, (tRectangle){{x + margin, rowY}, {dialW, dialW}},
                                                     "Prob", buf, STANDARD_TEXT_HEIGHT,
                                                     (uint32_t)lround(gMutator.mutateProb * 100.0), 101, 0, (tRgb)RGB_GREY_9);

    double                   rangeX                      = x + margin + dialW + 20.0;

    snprintf(buf, sizeof(buf), "+/-%d%%", (int)lround(gMutator.mutateRange * 100.0));
    gMutator.rangeSliderRect = render_dial_with_text(mainArea, (tRectangle){{rangeX, rowY}, {dialW, dialW}},
                                                     "Range", buf, STANDARD_TEXT_HEIGHT,
                                                     (uint32_t)lround(gMutator.mutateRange * 100.0), 101, 0, (tRgb)RGB_GREY_9);

    double                   linkX                       = rangeX + dialW + 20.0;

    gMutator.linkButtonRect  = draw_button(mainArea, (tRectangle){{linkX, rowY + STANDARD_TEXT_HEIGHT * 2.0}, {50.0, STANDARD_BUTTON_TEXT_HEIGHT}},
                                           gMutator.linkProbRange ? (char *)"Link" : (char *)"Free",
                                           gMutator.linkProbRange ? (tRgb)RGB_GREY_9 : (tRgb)RGB_GREY_5);

    double                   crossX                      = x + w - margin - dialW;

    snprintf(buf, sizeof(buf), "%d%%", (int)lround(gMutator.crossProb * 100.0));
    gMutator.crossSliderRect = render_dial_with_text(mainArea, (tRectangle){{crossX, rowY}, {dialW, dialW}},
                                                     "Cross", buf, STANDARD_TEXT_HEIGHT,
                                                     (uint32_t)lround(gMutator.crossProb * 100.0), 101, 0, (tRgb)RGB_GREY_9);

    rowY                    += dialRowH + 10.0;

    // Region 2: Mother / Children x6 / Father. rowBoxW is shared with the Temporary Storage and
    // Patch Variations rows below so all three stay vertically aligned (8 boxes, 7 gaps, each row).
    double                   rowBoxW                     = (w - margin * 2.0 - 7.0 * 7.0) / 8.0;
    double                   boxW                        = rowBoxW;
    double                   boxH                        = 46.0;

    static const char *const boxLabel[MUTATOR_NUM_BOXES] = {"Mother", "1", "2", "3", "4", "5", "6", "Father"};

    for (int i = 0; i < MUTATOR_NUM_BOXES; i++) {
        double     bx         = x + margin + (boxW + 7.0) * i;
        tRectangle boxRect    = {{bx, rowY}, {boxW, boxH}};

        gMutator.boxRect[i] = boxRect;

        bool       focused    = (gMutator.focus == i);
        bool       valid      = gMutator.genomeValid[i];
        bool       isMother   = (i == mutatorFocusMother);
        tRgb       roleBack   = isMother ? (tRgb)MUTATOR_RGB_SINGLE_BACK : (tRgb)MUTATOR_RGB_COUPLE_BACK;
        tRgb       lineColour = isMother ? (tRgb)MUTATOR_RGB_SINGLE_LINE : (tRgb)MUTATOR_RGB_COUPLE_LINE;
        tRgb       bg;

        if (!valid) {
            bg = (tRgb)RGB_GREY_3;
        } else if (focused) {
            // Focused: role colour lightened towards white, so it's still identifiable as
            // Mother/Child/Father but clearly the one currently playing.
            bg = (tRgb){
                (roleBack.red + 1.0) / 2.0, (roleBack.green + 1.0) / 2.0, (roleBack.blue + 1.0) / 2.0
            };
        } else {
            bg = roleBack;
        }
        // Manual box drawing (not draw_button()) - draw_button() scales its text to fill the
        // whole rectangle height, which is fine for slim controls but produces oversized/clipped
        // text on a box this tall; a small top-aligned label reads better.
        set_rgb_colour(bg);
        render_rectangle_with_border(mainArea, boxRect);
        set_rgb_colour(contrasting_text_colour(bg));
        render_text(mainArea, (tRectangle){{bx + 4.0, rowY + 4.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, boxLabel[i]);

        if (valid) {
            draw_chromosome((tRectangle){{bx + 3.0, rowY + 18.0}, {boxW - 6.0, boxH - 22.0}}, gMutator.genome[i], lineColour);
        }
    }

    rowY += boxH + 14.0;

    // Temporary Storage: 24 slots (3 rows of 8). Click an empty slot to save the focused genome
    // there; click a saved slot to load it as Mother (Shift-click = Father); Cmd-click to clear.
    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
                "Temporary Storage (click empty = save focused, click saved = load Mother, Shift = Father, Cmd = clear)");
    rowY += STANDARD_TEXT_HEIGHT + 6.0;

    double storeBoxW = rowBoxW;
    double storeBoxH = 30.0;

    for (int s = 0; s < MUTATOR_NUM_STORAGE; s++) {
        int        row       = s / 8;
        int        col       = s % 8;
        double     sx        = x + margin + (storeBoxW + 7.0) * col;
        double     sy        = rowY + (storeBoxH + 4.0) * row;
        tRectangle storeRect = {{sx, sy}, {storeBoxW, storeBoxH}};

        gMutator.storageRect[s] = storeRect;

        bool       valid     = gMutator.storageValid[s];

        // Manual reference: empty slots are plain "Gene Bank" teal too, not a distinct empty
        // colour - only the chromosome line (drawn below) distinguishes filled from empty.
        set_rgb_colour((tRgb)MUTATOR_RGB_GENEBANK_BACK);
        render_rectangle_with_border(mainArea, storeRect);

        if (valid) {
            draw_chromosome((tRectangle){{sx + 2.0, sy + 2.0}, {storeBoxW - 4.0, storeBoxH - 4.0}},
                            gMutator.storageGenome[s], (tRgb)MUTATOR_RGB_GENEBANK_LINE);
        }
    }

    rowY += (storeBoxH + 4.0) * 3.0;

    // Region 5 (deferred 3/4 skipped): Quick Lock row - Lock + Solo per category
    double         catW    = (w - margin * 2.0) / (double)mutatorCatNone;

    for (int cat = 0; cat < mutatorCatNone; cat++) {
        double catX = x + margin + catW * cat;

        gMutator.categoryLockRect[cat] = draw_category_button(catX, rowY, catW - 4.0, STANDARD_BUTTON_TEXT_HEIGHT,
                                                              kCategoryLabel[cat], gMutator.locks.locked[cat]);
        gMutator.categorySoloRect[cat] = draw_category_button(catX, rowY + STANDARD_BUTTON_TEXT_HEIGHT + 4.0, catW - 4.0, STANDARD_BUTTON_TEXT_HEIGHT,
                                                              "Solo", gMutator.locks.solo[cat]);
    }

    rowY += (STANDARD_BUTTON_TEXT_HEIGHT + 4.0) * 2.0 + 14.0;

    // Patch Variations row: read-only mirror of the 8 real hardware variations. Click loads that
    // variation as Mother; Shift-click loads it as Father - a stand-in for the Temporary
    // Storage/drag-and-drop mechanism the manual describes, until that's built.
    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Patch Variations (click = Mother, Shift-click = Father, Cmd-click = commit focused sound here)");
    rowY += STANDARD_TEXT_HEIGHT + 6.0;

    double         varBoxW = rowBoxW;
    double         varBoxH = 32.0;
    static uint8_t varGenome[MUTATOR_MAX_SCHEMA];

    for (int v = 0; v < MUTATOR_NUM_REAL_VARIATIONS; v++) {
        double     vx      = x + margin + (varBoxW + 7.0) * v;
        tRectangle varRect = {{vx, rowY}, {varBoxW, varBoxH}};

        gMutator.variationRect[v] = varRect;

        bool       active  = ((uint32_t)v == gPatchDescr[gMutator.slot].activeVariation);
        tRgb       bg      = active ? (tRgb)RGB_GREEN_ON : (tRgb)MUTATOR_RGB_VARIATION_BACK;
        char       label[4];
        snprintf(label, sizeof(label), "%d", v + 1);

        set_rgb_colour(bg);
        render_rectangle_with_border(mainArea, varRect);
        set_rgb_colour(contrasting_text_colour(bg));
        render_text(mainArea, (tRectangle){{vx + 2.0, rowY + 2.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT * 0.7}}, label);

        mutator_read_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot, (uint32_t)v, varGenome);
        draw_chromosome((tRectangle){{vx + 2.0, rowY + 11.0}, {varBoxW - 4.0, varBoxH - 13.0}}, varGenome, (tRgb)MUTATOR_RGB_VARIATION_LINE);
    }

    rowY += varBoxH;

    if ((rowY + margin) > (y + panel.size.h)) {
        gMutator.panelRect.size.h = (rowY + margin) - y;
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

static bool shift_held(void) {
    return (glfwGetKey((GLFWwindow *)gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
           || (glfwGetKey((GLFWwindow *)gWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
}

static bool cmd_held(void) {
    return (glfwGetKey((GLFWwindow *)gWindow, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS)
           || (glfwGetKey((GLFWwindow *)gWindow, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS);
}

// index: 0=Prob, 1=Range, 2=Cross Prob. A real dial, dragged vertically like every other knob in
// this app (drag up = increase) rather than jumping straight to an absolute position.
static double * dial_value_ptr(int32_t index) {
    switch (index) {
        case 0: return &gMutator.mutateProb;

        case 1: return &gMutator.mutateRange;

        default: return &gMutator.crossProb;
    }
}

static void begin_dial_drag(int32_t index, tCoord coord) {
    gMutator.draggingSlider = index;
    gMutator.dragLastY      = coord.y;
}

// Called on every cursor_pos() while a dial is being dragged.
static void continue_dial_drag(tCoord coord) {
    double   deltaY = gMutator.dragLastY - coord.y;   // up = positive = increase

    gMutator.dragLastY = coord.y;

    double * value  = dial_value_ptr(gMutator.draggingSlider);

    *value            += deltaY * 0.005;

    if (*value < 0.0) {
        *value = 0.0;
    } else if (*value > 1.0) {
        *value = 1.0;
    }

    if (gMutator.linkProbRange) {
        if (gMutator.draggingSlider == 0) {
            gMutator.mutateRange = 1.0 - gMutator.mutateProb;
        } else if (gMutator.draggingSlider == 1) {
            gMutator.mutateProb = 1.0 - gMutator.mutateRange;
        }
    }
}

bool handle_mutator_mouse(tCoord coord, tMouseButton mouseButton) {
    if (!gMutator.active) {
        return false;
    }

    // Right-click (and any other button/action this handler doesn't specifically act on) must
    // still be swallowed while the cursor is over the panel, so it can't reach the module canvas
    // underneath and open e.g. a module's context menu.
    if ((mouseButton != mouseButtonLeftDown) && (mouseButton != mouseButtonLeftUp)) {
        return within_rectangle(coord, gMutator.panelRect);
    }

    if (mouseButton == mouseButtonLeftDown) {
        if (within_rectangle(coord, gMutator.closeButtonRect)) {
            close_mutator_panel();
            return true;
        }

        if (within_rectangle(coord, gMutator.titleBarRect)) {
            gMutator.draggingPanel  = true;
            gMutator.dragMouseStart = coord;
            gMutator.dragPanelStart = gMutator.panelRect.coord;
            return true;
        }

        for (int i = 0; i < 4; i++) {
            if (within_rectangle(coord, gMutator.operatorRect[i])) {
                gMutator.operatorPressed[i] = true;

                switch (i) {
                    case 0: run_mutate();
                        break;

                    case 1: run_randomize();
                        break;

                    case 2: run_interpolate();
                        break;

                    case 3: run_cross();
                        break;
                }
                return true;
            }
        }

        if (within_rectangle(coord, gMutator.linkButtonRect)) {
            gMutator.linkProbRange = !gMutator.linkProbRange;
            return true;
        }

        for (int i = 0; i < MUTATOR_NUM_BOXES; i++) {
            if (within_rectangle(coord, gMutator.boxRect[i])) {
                audition(i);
                return true;
            }
        }

        for (int cat = 0; cat < mutatorCatNone; cat++) {
            if (within_rectangle(coord, gMutator.categoryLockRect[cat])) {
                gMutator.locks.locked[cat] = !gMutator.locks.locked[cat];
                return true;
            }

            if (within_rectangle(coord, gMutator.categorySoloRect[cat])) {
                gMutator.locks.solo[cat] = !gMutator.locks.solo[cat];
                return true;
            }
        }

        for (int v = 0; v < MUTATOR_NUM_REAL_VARIATIONS; v++) {
            if (within_rectangle(coord, gMutator.variationRect[v])) {
                if (cmd_held()) {
                    request_commit_to_variation((uint32_t)v);
                    return true;
                }
                bool    shiftHeld = shift_held();
                int32_t target    = shiftHeld ? mutatorFocusFather : mutatorFocusMother;

                mutator_read_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot, (uint32_t)v, gMutator.genome[target]);
                gMutator.genomeValid[target] = true;

                if (!shiftHeld) {
                    audition(target);
                } else {
                    gMutator.focus = target;
                }
                return true;
            }
        }

        for (int s = 0; s < MUTATOR_NUM_STORAGE; s++) {
            if (within_rectangle(coord, gMutator.storageRect[s])) {
                if (cmd_held()) {
                    gMutator.storageValid[s] = false;
                    return true;
                }

                if (gMutator.storageValid[s]) {
                    bool    shiftHeld = shift_held();
                    int32_t target    = shiftHeld ? mutatorFocusFather : mutatorFocusMother;

                    memcpy(gMutator.genome[target], gMutator.storageGenome[s], sizeof(gMutator.genome[0]));
                    gMutator.genomeValid[target] = true;

                    if (!shiftHeld) {
                        audition(target);
                    } else {
                        gMutator.focus = target;
                    }
                } else if (gMutator.focus != mutatorFocusNone) {
                    memcpy(gMutator.storageGenome[s], gMutator.genome[gMutator.focus], sizeof(gMutator.genome[0]));
                    gMutator.storageValid[s] = true;
                }
                return true;
            }
        }

        if (within_rectangle(coord, gMutator.probSliderRect)) {
            begin_dial_drag(0, coord);
            return true;
        }

        if (within_rectangle(coord, gMutator.rangeSliderRect)) {
            begin_dial_drag(1, coord);
            return true;
        }

        if (within_rectangle(coord, gMutator.crossSliderRect)) {
            begin_dial_drag(2, coord);
            return true;
        }
        // Swallow clicks inside the panel that didn't hit a control, so they don't fall through
        // to the module canvas underneath.
        return within_rectangle(coord, gMutator.panelRect);
    }

    if (mouseButton == mouseButtonLeftUp) {
        for (int i = 0; i < 4; i++) {
            gMutator.operatorPressed[i] = false;
        }

        if (gMutator.draggingPanel) {
            gMutator.draggingPanel = false;
            return true;
        }

        if (gMutator.draggingSlider >= 0) {
            gMutator.draggingSlider = -1;
            return true;
        }
        return within_rectangle(coord, gMutator.panelRect);
    }
    return false;
}

void handle_mutator_cursor_pos(tCoord coord) {
    if (!gMutator.active) {
        return;
    }

    if (gMutator.draggingPanel) {
        gMutator.panelRect.coord.x = gMutator.dragPanelStart.x + (coord.x - gMutator.dragMouseStart.x);
        gMutator.panelRect.coord.y = gMutator.dragPanelStart.y + (coord.y - gMutator.dragMouseStart.y);
        return;
    }

    if (gMutator.draggingSlider >= 0) {
        continue_dial_drag(coord);
    }
}

// ─── Keyboard ────────────────────────────────────────────────────────────────

// Only claims the specific Mutator shortcut keys - everything else (Cmd+Z, Delete, arrow keys,
// normal module editing, etc.) must keep working while the panel is open, per the manual: "you can
// still edit the patch as usual, add or delete modules or turn the knobs."
bool handle_mutator_key(int key, int mods, int action) {
    (void)mods;

    if (!gMutator.active || (action != GLFW_PRESS)) {
        return false;
    }

    if (key == GLFW_KEY_ESCAPE) {
        close_mutator_panel();
        return true;
    }

    if ((key >= GLFW_KEY_1) && (key <= GLFW_KEY_8)) {
        int32_t box = key - GLFW_KEY_1;   // 0=Mother .. 7=Father, matches box index directly

        audition(box);
        return true;
    }

    switch (key) {
        case GLFW_KEY_O: copy_focused_to(mutatorFocusMother);
            return true;

        case GLFW_KEY_T: copy_focused_to(mutatorFocusFather);
            return true;

        case GLFW_KEY_E: copy_focused_to(mutatorFocusMother);
            run_mutate();
            return true;

        case GLFW_KEY_U: run_mutate();
            return true;

        case GLFW_KEY_N: run_randomize();
            return true;

        case GLFW_KEY_I: run_interpolate();
            return true;

        case GLFW_KEY_X: run_cross();
            return true;

        case GLFW_KEY_S: save_focused_to_storage();
            return true;

        default: break;
    }
    return false;
}
