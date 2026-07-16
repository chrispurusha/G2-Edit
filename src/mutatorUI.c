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
#include "alertDialog.h"
#include "mouseHandle.h"
#include "graphics.h"

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
        80.0, 80.0 + MENU_BAR_HEIGHT
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

    // Seed Father too if nothing's there yet, so Interpolate/Cross are usable right after a first
    // Randomize without an extra manual step. Never overwrites a Father the user already set up.
    if (!gMutator.genomeValid[mutatorFocusFather]) {
        mutator_randomize(gMutator.genome[base], gMutator.schema, gMutator.schemaCount, &gMutator.locks, gMutator.genome[mutatorFocusFather]);
        gMutator.genomeValid[mutatorFocusFather] = true;
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

static uint8_t  gPendingCommitGenome[MUTATOR_MAX_SCHEMA];
static bool     gPendingCommitValid     = false;
static uint32_t gPendingCommitVariation = 0;

static void on_variation_commit_confirmed(bool confirmed) {
    if (!confirmed || !gPendingCommitValid) {
        gPendingCommitValid = false;
        return;
    }
    mutator_apply_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot,
                         gPendingCommitVariation, gPendingCommitGenome, true);
    gPendingCommitValid = false;
}

// genome is snapshotted immediately (the caller's buffer - e.g. a box being dragged - isn't
// guaranteed to still be valid/unchanged by the time the async dialog resolves).
static void request_commit_to_variation(const uint8_t * genome, uint32_t variationIndex) {
    memcpy(gPendingCommitGenome, genome, sizeof(gPendingCommitGenome));
    gPendingCommitValid     = true;
    gPendingCommitVariation = variationIndex;

    char title[48];
    char message[192];
    snprintf(title, sizeof(title), "Commit to Variation %u", variationIndex + 1);
    snprintf(message, sizeof(message),
             "Overwrite edit buffer variation %u with the focused sound? (Cmd-Z).",
             variationIndex + 1);
    show_confirm(title, message, "Commit...", on_variation_commit_confirmed);
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

// Defined further down, once gPendingKind/gPendingBoxFam/gPendingBoxIdx exist - forward declared
// here so render_mutator_panel can draw it last (on top of everything else in the panel).
static void draw_drag_ghost(void);

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

    // titleBarRect is also the drag handle, so it stays the full-width rect returned by
    // draw_panel_chrome() (whose visual fill is inset from the border internally).
    gMutator.titleBarRect    = draw_panel_chrome(panel, titleH, "Patch Mutator");
    gMutator.closeButtonRect = draw_panel_close_button(panel, gMutator.closeButtonPressed);

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
                                           (char *)"Link",
                                           gMutator.linkProbRange ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

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
    // there; click a saved slot to load it as Mother; right-click to clear. Drag onto Father to
    // load it there instead (quietly, no audition) - Shift/Cmd-drag = Interpolate/Cross. Split
    // across two lines - the full sentence is wider than the panel at its default size.
    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
                "Temporary Storage (click empty = save, click saved = load Mother, right-click = clear)");
    rowY += STANDARD_TEXT_HEIGHT + 2.0;
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
                "drag = copy (drop on Father = quiet load), Shift-drag = Interpolate, Cmd-drag = Cross");
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
    double catW = (w - margin * 2.0) / (double)mutatorCatNone;

    for (int cat = 0; cat < mutatorCatNone; cat++) {
        double catX = x + margin + catW * cat;

        gMutator.categoryLockRect[cat] = draw_category_button(catX, rowY, catW - 4.0, STANDARD_BUTTON_TEXT_HEIGHT,
                                                              kCategoryLabel[cat], gMutator.locks.locked[cat]);
        gMutator.categorySoloRect[cat] = draw_category_button(catX, rowY + STANDARD_BUTTON_TEXT_HEIGHT + 4.0, catW - 4.0, STANDARD_BUTTON_TEXT_HEIGHT,
                                                              "Solo", gMutator.locks.solo[cat]);
    }

    rowY += (STANDARD_BUTTON_TEXT_HEIGHT + 4.0) * 2.0 + 14.0;

    // Patch Variations row: mirrors the 8 real hardware variations. Click loads that variation as
    // Mother; Cmd-click, or a plain drop here, commits (with confirmation, since it's a real write
    // to the edit buffer). Also a full drag source/destination like every other box - drag = copy
    // (drag onto Father loads it quietly instead), Shift-drag = Interpolate, Cmd-drag = Cross.
    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Patch Variations (click = Mother, Cmd/drop = commit, drag = copy/Shift = Interp/Cmd = Cross)");
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
    draw_drag_ghost();
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

// ─── Click-and-drag model ────────────────────────────────────────────────────
// All discrete controls arm on mouse-down and only fire on mouse-up if the release is still over
// the same control (standard button behaviour - drag off before releasing cancels). Mother/
// Children/Father, Temporary Storage, and Patch Variation boxes all support drag-and-drop between
// any two of them, per the manual: plain drag = copy (a plain drop onto a Variation commits there,
// with confirmation, since that's a real hardware write), Shift-drag = Interpolate, Cmd-drag =
// Cross. Same-box press-then-release (i.e. a plain click, no drag) keeps each box's own click
// meaning instead - notably Cmd-click on a Variation still means "commit the focused sound here",
// distinct from Cmd-drag *onto* that same Variation meaning Cross.

typedef enum {
    pendingNone,
    pendingClose,
    pendingOperator,
    pendingLink,
    pendingCategoryLock,
    pendingCategorySolo,
    pendingBoxDrag,   // Mother/Children/Father, Temporary Storage, and Patch Variations - the full drag-and-drop family
} tMutatorPendingKind;

// boxFamVariation participates fully in the drag family now: a plain click still does the old
// load-Mother/Shift-Father/Cmd-commit thing (click_drag_box), but it can now also be dragged onto
// any other box (as a source) or dropped onto (as a destination) just like Mother/Children/Father
// and Temporary Storage.
typedef enum {
    boxFamMCF,
    boxFamStorage,
    boxFamVariation,
} tBoxFamily;

static tMutatorPendingKind gPendingKind   = pendingNone;
static int32_t             gPendingIndex  = -1;
static tBoxFamily          gPendingBoxFam = boxFamMCF;
static int32_t             gPendingBoxIdx = -1;

static bool box_ref_valid(tBoxFamily fam, int32_t idx) {
    switch (fam) {
        case boxFamMCF:       return gMutator.genomeValid[idx];

        case boxFamStorage:   return gMutator.storageValid[idx];

        case boxFamVariation: return gMutator.schemaCount > 0;
    }
    return false;
}

// For boxFamVariation this reads the variation's live values on demand into a shared scratch
// buffer - safe because callers that need to keep a source's values across a later mutation of
// gMutator state (see drop_drag_box) copy out of this buffer immediately, before it can be
// overwritten by a second call for the destination side.
static const uint8_t * box_ref_genome(tBoxFamily fam, int32_t idx) {
    static uint8_t variationScratch[MUTATOR_MAX_SCHEMA];

    switch (fam) {
        case boxFamMCF:     return gMutator.genome[idx];

        case boxFamStorage: return gMutator.storageGenome[idx];

        case boxFamVariation:
            mutator_read_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot, (uint32_t)idx, variationScratch);
            return variationScratch;
    }
    return NULL;
}

static void box_ref_set_genome(tBoxFamily fam, int32_t idx, const uint8_t * values) {
    if (fam == boxFamMCF) {
        memcpy(gMutator.genome[idx], values, sizeof(gMutator.genome[0]));
        gMutator.genomeValid[idx] = true;
    } else {
        memcpy(gMutator.storageGenome[idx], values, sizeof(gMutator.genome[0]));
        gMutator.storageValid[idx] = true;
    }
}

static bool hit_test_drag_box(tCoord coord, tBoxFamily * outFam, int32_t * outIdx) {
    for (int i = 0; i < MUTATOR_NUM_BOXES; i++) {
        if (within_rectangle(coord, gMutator.boxRect[i])) {
            *outFam = boxFamMCF;
            *outIdx = i;
            return true;
        }
    }

    for (int s = 0; s < MUTATOR_NUM_STORAGE; s++) {
        if (within_rectangle(coord, gMutator.storageRect[s])) {
            *outFam = boxFamStorage;
            *outIdx = s;
            return true;
        }
    }

    for (int v = 0; v < MUTATOR_NUM_REAL_VARIATIONS; v++) {
        if (within_rectangle(coord, gMutator.variationRect[v])) {
            *outFam = boxFamVariation;
            *outIdx = v;
            return true;
        }
    }

    return false;
}

// While a box-drag is armed (mouse down on a box, not yet released), draw a small floating
// chromosome preview next to the cursor - mirrors a native drag's "ghost", but drawn as the
// dragged genome's own sparkline so it also hints at what's being carried, not just that
// something is.
static void draw_drag_ghost(void) {
    if (gPendingKind != pendingBoxDrag) {
        return;
    }
    tCoord          mouse;

    get_global_gui_scaled_mouse_coord(&mouse);

    double          size      = 40.0;
    tRectangle      ghostRect = (tRectangle){{
                                                 mouse.x + 12.0, mouse.y + 12.0
                                             }, {
                                                 size, size
                                             }
    };

    set_rgb_colour((tRgb)RGB_GREY_3);
    render_rectangle_with_border(mainArea, ghostRect);

    const uint8_t * genome    = box_ref_genome(gPendingBoxFam, gPendingBoxIdx);

    draw_chromosome(ghostRect, genome, (tRgb)RGB_WHITE);
}

// The plain-click behaviour for a Mother/Child/Father, Temporary Storage, or Patch Variation box
// (i.e. pressed and released on the very same box, or a Storage box being used as a drag source
// with nothing in it). Shift-click-to-Father and Cmd-click-to-clear were dropped once drag/right-
// click covered the same ground: dragging onto Father now loads it just as quietly (see
// drop_drag_box), and right-click clears any box (see clear_box).
static void click_drag_box(tBoxFamily fam, int32_t idx) {
    if (fam == boxFamMCF) {
        audition(idx);
        return;
    }

    if (fam == boxFamVariation) {
        if (cmd_held()) {
            if (gMutator.focus != mutatorFocusNone) {
                request_commit_to_variation(gMutator.genome[gMutator.focus], (uint32_t)idx);
            }
            return;
        }
        mutator_read_genome(gMutator.schema, gMutator.schemaCount, gMutator.slot, (uint32_t)idx, gMutator.genome[mutatorFocusMother]);
        gMutator.genomeValid[mutatorFocusMother] = true;
        audition(mutatorFocusMother);
        return;
    }

    if (gMutator.storageValid[idx]) {
        memcpy(gMutator.genome[mutatorFocusMother], gMutator.storageGenome[idx], sizeof(gMutator.genome[0]));
        gMutator.genomeValid[mutatorFocusMother] = true;
        audition(mutatorFocusMother);
    } else if (gMutator.focus != mutatorFocusNone) {
        memcpy(gMutator.storageGenome[idx], gMutator.genome[gMutator.focus], sizeof(gMutator.genome[0]));
        gMutator.storageValid[idx] = true;
    }
}

// A genuine drag from one box to a *different* box. Modifier held at release decides the
// operation, matching the manual's mouse shortcuts table. The source is snapshotted into a local
// buffer up front, since box_ref_genome's boxFamVariation case reads through a single shared
// scratch buffer that a second call (for the destination side) would otherwise overwrite.
static void drop_drag_box(tBoxFamily srcFam, int32_t srcIdx, tBoxFamily dstFam, int32_t dstIdx) {
    uint8_t srcGenome[MUTATOR_MAX_SCHEMA];

    memcpy(srcGenome, box_ref_genome(srcFam, srcIdx), sizeof(srcGenome));

    if (shift_held() || cmd_held()) {
        uint8_t dstGenome[MUTATOR_MAX_SCHEMA];

        memcpy(dstGenome, box_ref_genome(dstFam, dstIdx), sizeof(dstGenome));

        memcpy(gMutator.genome[mutatorFocusMother], srcGenome, sizeof(gMutator.genome[0]));
        memcpy(gMutator.genome[mutatorFocusFather], dstGenome, sizeof(gMutator.genome[0]));
        gMutator.genomeValid[mutatorFocusMother] = true;
        gMutator.genomeValid[mutatorFocusFather] = true;

        if (shift_held()) {
            run_interpolate();
        } else {
            run_cross();
        }
        return;
    }

    // A plain drop onto a real Variation is a real hardware write - always confirm, exactly like
    // Cmd-click on a Variation box already does.
    if (dstFam == boxFamVariation) {
        request_commit_to_variation(srcGenome, (uint32_t)dstIdx);
        return;
    }
    box_ref_set_genome(dstFam, dstIdx, srcGenome);

    if (dstFam == boxFamMCF) {
        // Father is a breeding partner, not something to preview on drop - loading it should stay
        // quiet (no write to real hardware), same as every other route into Father used to be.
        if (dstIdx == mutatorFocusFather) {
            gMutator.focus = dstIdx;
        } else {
            audition(dstIdx);
        }
    }
}

// Right-click shortcut to empty a box, instead of Cmd-click (Storage) or a drag (everywhere).
// Real hardware Variations have nothing to "clear" - a no-op there.
static void clear_box(tBoxFamily fam, int32_t idx) {
    switch (fam) {
        case boxFamMCF:
            gMutator.genomeValid[idx]  = false;

            if (gMutator.focus == idx) {
                gMutator.focus = mutatorFocusNone;
            }
            break;

        case boxFamStorage:
            gMutator.storageValid[idx] = false;
            break;

        case boxFamVariation:
            break;
    }
}

bool handle_mutator_mouse(tCoord coord, tMouseButton mouseButton) {
    if (!gMutator.active) {
        return false;
    }

    if (mouseButton == mouseButtonRightUp) {
        tBoxFamily fam;
        int32_t    idx;

        if (hit_test_drag_box(coord, &fam, &idx)) {
            clear_box(fam, idx);
        }
        return within_rectangle(coord, gMutator.panelRect);
    }

    // Any other button/action this handler doesn't specifically act on must still be swallowed
    // while the cursor is over the panel, so it can't reach the module canvas underneath and open
    // e.g. a module's context menu.
    if ((mouseButton != mouseButtonLeftDown) && (mouseButton != mouseButtonLeftUp)) {
        return within_rectangle(coord, gMutator.panelRect);
    }

    if (mouseButton == mouseButtonLeftDown) {
        // Close sits visually inside the title bar rect, so it must be checked first - otherwise
        // a press on Close arms a panel-drag instead, and the drag release swallows the click
        // before the close action ever gets a chance to fire.
        if (within_rectangle(coord, gMutator.closeButtonRect)) {
            gPendingKind                = pendingClose;
            gMutator.closeButtonPressed = true;
            return true;
        }

        // Continuous drag controls (panel title bar, dials) still act immediately on press -
        // they aren't discrete "click" actions.
        if (within_rectangle(coord, gMutator.titleBarRect)) {
            gMutator.draggingPanel  = true;
            gMutator.dragMouseStart = coord;
            gMutator.dragPanelStart = gMutator.panelRect.coord;
            return true;
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

        // Everything else just arms here; the action fires on release, only if the mouse is
        // still (or, for the drag family, now) over a valid target.
        for (int i = 0; i < 4; i++) {
            if (within_rectangle(coord, gMutator.operatorRect[i])) {
                gPendingKind                = pendingOperator;
                gPendingIndex               = i;
                gMutator.operatorPressed[i] = true;
                return true;
            }
        }

        if (within_rectangle(coord, gMutator.linkButtonRect)) {
            gPendingKind = pendingLink;
            return true;
        }

        for (int cat = 0; cat < mutatorCatNone; cat++) {
            if (within_rectangle(coord, gMutator.categoryLockRect[cat])) {
                gPendingKind  = pendingCategoryLock;
                gPendingIndex = cat;
                return true;
            }

            if (within_rectangle(coord, gMutator.categorySoloRect[cat])) {
                gPendingKind  = pendingCategorySolo;
                gPendingIndex = cat;
                return true;
            }
        }

        tBoxFamily fam;
        int32_t    idx;

        if (hit_test_drag_box(coord, &fam, &idx)) {
            gPendingKind   = pendingBoxDrag;
            gPendingBoxFam = fam;
            gPendingBoxIdx = idx;
            return true;
        }
        // Swallow clicks inside the panel that didn't hit a control, so they don't fall through
        // to the module canvas underneath.
        return within_rectangle(coord, gMutator.panelRect);
    }

    // mouseButtonLeftUp
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
    tMutatorPendingKind kind = gPendingKind;

    gPendingKind = pendingNone;

    switch (kind) {
        case pendingClose:
            gMutator.closeButtonPressed = false;

            if (within_rectangle(coord, gMutator.closeButtonRect)) {
                close_mutator_panel();
            }
            return true;

        case pendingOperator:

            if (within_rectangle(coord, gMutator.operatorRect[gPendingIndex])) {
                switch (gPendingIndex) {
                    case 0: run_mutate();
                        break;

                    case 1: run_randomize();
                        break;

                    case 2: run_interpolate();
                        break;

                    case 3: run_cross();
                        break;
                }
            }
            return true;

        case pendingLink:

            if (within_rectangle(coord, gMutator.linkButtonRect)) {
                gMutator.linkProbRange = !gMutator.linkProbRange;
            }
            return true;

        case pendingCategoryLock:

            if (within_rectangle(coord, gMutator.categoryLockRect[gPendingIndex])) {
                gMutator.locks.locked[gPendingIndex] = !gMutator.locks.locked[gPendingIndex];
            }
            return true;

        case pendingCategorySolo:

            if (within_rectangle(coord, gMutator.categorySoloRect[gPendingIndex])) {
                gMutator.locks.solo[gPendingIndex] = !gMutator.locks.solo[gPendingIndex];
            }
            return true;

        case pendingBoxDrag:
        {
            tBoxFamily dstFam;
            int32_t    dstIdx;
            bool       foundTarget = hit_test_drag_box(coord, &dstFam, &dstIdx);

            if (foundTarget && (dstFam == gPendingBoxFam) && (dstIdx == gPendingBoxIdx)) {
                click_drag_box(gPendingBoxFam, gPendingBoxIdx);
            } else if (foundTarget && box_ref_valid(gPendingBoxFam, gPendingBoxIdx)) {
                drop_drag_box(gPendingBoxFam, gPendingBoxIdx, dstFam, dstIdx);
            }
            // Released somewhere else entirely, or dragged an empty box: no-op.
            return true;
        }

        default:
            break;
    }
    return within_rectangle(coord, gMutator.panelRect);
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
