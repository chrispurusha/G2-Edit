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

tMutatorState            gMutator                       = {0};

static const char *const kCategoryLabel[mutatorCatNone] = {
    "OscFreq", "OscFine", "Envelope", "SeqValue", "SeqEvent", "Delays", "Effects",
};

static const char *const kOperatorLabel[4]              = {"Mutate", "Randomise", "Interpolate", "Cross"};

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
        620.0, 320.0
    };

    // Seed Mother from whatever's currently playing, so there's something to work with even
    // before the (not-yet-built) Temporary Storage / Patch Variations rows exist.
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

// ─── Rendering ───────────────────────────────────────────────────────────────

static tRectangle draw_category_button(double x, double y, double w, double h, const char * label, bool on) {
    tRgb bg = on ? (tRgb)RGB_GREY_9 : (tRgb)RGB_GREY_5;

    return draw_button(mainArea, (tRectangle){{x, y}, {w, h}}, label, bg);
}

// draw_slider() fills bottom-up (built for the G2's own vertical faders) - not usable for a
// left-to-right probability bar, so this fills horizontally instead: a dark track with a lighter
// fill proportional to value01 (0..1).
static void draw_horizontal_bar(tRectangle rect, double value01) {
    if (value01 < 0.0) {
        value01 = 0.0;
    } else if (value01 > 1.0) {
        value01 = 1.0;
    }
    set_rgb_colour((tRgb)RGB_GREY_3);
    render_rectangle_with_border(mainArea, rect);

    double fillW = rect.size.w * value01;

    if (fillW > 0.0) {
        set_rgb_colour((tRgb)RGB_GREY_9);
        render_rectangle(mainArea, (tRectangle){{rect.coord.x, rect.coord.y}, {fillW, rect.size.h}});
    }
}

// "Chromosome" sparkline (manual: "a curvy line derived from the actual parameter values" - shows
// at a glance how different two genomes are). Plots every schema entry left-to-right, normalized
// height, connected by straight segments - not a single bezier, since a genome can have hundreds
// of entries and render_bezier_curve() only draws one curve through three points.
static void draw_chromosome(tRectangle rect, const uint8_t * genome) {
    static double normalized[MUTATOR_MAX_SCHEMA];
    uint32_t      count = gMutator.schemaCount;

    if (count < 2) {
        return;
    }
    mutator_chromosome(genome, gMutator.schema, count, normalized);

    set_rgb_colour((tRgb)RGB_GREY_2);
    tCoord        prev  = {rect.coord.x, rect.coord.y + rect.size.h * (1.0 - normalized[0])};

    for (uint32_t i = 1; i < count; i++) {
        tCoord next = {
            rect.coord.x + rect.size.w * ((double)i / (double)(count - 1)),
            rect.coord.y + rect.size.h * (1.0 - normalized[i])
        };
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

    // Region 1: operator buttons + sliders
    double                   btnW                        = 110.0;
    double                   btnH                        = STANDARD_BUTTON_TEXT_HEIGHT;

    for (int i = 0; i < 4; i++) {
        gMutator.operatorRect[i] = draw_button(mainArea, (tRectangle){{x + margin + (btnW + 6.0) * i, rowY}, {btnW, btnH}},
                                               (char *)kOperatorLabel[i], (tRgb)RGB_BACKGROUND_GREY);
    }

    rowY                    += btnH + 18.0;

    double                   sliderW                     = 150.0;

    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Prob");
    gMutator.probSliderRect  = (tRectangle){{
                                                x + margin + 34.0, rowY - 2.0
                                            }, {
                                                sliderW, STANDARD_TEXT_HEIGHT + 4.0
                                            }
    };
    draw_horizontal_bar(gMutator.probSliderRect, gMutator.mutateProb);

    double                   rangeX                      = x + margin + 34.0 + sliderW + 20.0;

    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{rangeX, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Range");
    gMutator.rangeSliderRect = (tRectangle){{
                                                rangeX + 40.0, rowY - 2.0
                                            }, {
                                                sliderW, STANDARD_TEXT_HEIGHT + 4.0
                                            }
    };
    draw_horizontal_bar(gMutator.rangeSliderRect, gMutator.mutateRange);

    double                   linkX                       = rangeX + 40.0 + sliderW + 12.0;

    gMutator.linkButtonRect  = draw_button(mainArea, (tRectangle){{linkX, rowY}, {50.0, STANDARD_BUTTON_TEXT_HEIGHT}},
                                           gMutator.linkProbRange ? (char *)"Link" : (char *)"Free",
                                           gMutator.linkProbRange ? (tRgb)RGB_GREY_9 : (tRgb)RGB_GREY_5);
    rowY                    += STANDARD_TEXT_HEIGHT + 10.0;

    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Cross Prob");
    gMutator.crossSliderRect = (tRectangle){{
                                                x + margin + 70.0, rowY - 2.0
                                            }, {
                                                sliderW, STANDARD_TEXT_HEIGHT + 4.0
                                            }
    };
    draw_horizontal_bar(gMutator.crossSliderRect, gMutator.crossProb);
    rowY                    += STANDARD_TEXT_HEIGHT + 16.0;

    // Region 2: Mother / Children x6 / Father
    double                   boxW                        = (w - margin * 2.0 - 7.0 * 6.0) / 8.0;
    double                   boxH                        = 46.0;

    static const char *const boxLabel[MUTATOR_NUM_BOXES] = {"Mother", "1", "2", "3", "4", "5", "6", "Father"};

    for (int i = 0; i < MUTATOR_NUM_BOXES; i++) {
        double     bx      = x + margin + (boxW + 7.0) * i;
        tRectangle boxRect = {{bx, rowY}, {boxW, boxH}};

        gMutator.boxRect[i] = boxRect;

        bool       focused = (gMutator.focus == i);
        bool       valid   = gMutator.genomeValid[i];
        tRgb       bg      = focused ? (tRgb)RGB_GREY_9 : (valid ? (tRgb)RGB_GREY_5 : (tRgb)RGB_GREY_3);

        // Manual box drawing (not draw_button()) - draw_button() scales its text to fill the
        // whole rectangle height, which is fine for slim controls but produces oversized/clipped
        // text on a box this tall; a small top-aligned label reads better.
        set_rgb_colour(bg);
        render_rectangle_with_border(mainArea, boxRect);
        set_rgb_colour(contrasting_text_colour(bg));
        render_text(mainArea, (tRectangle){{bx + 4.0, rowY + 4.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, boxLabel[i]);

        if (valid) {
            draw_chromosome((tRectangle){{bx + 3.0, rowY + 18.0}, {boxW - 6.0, boxH - 22.0}}, gMutator.genome[i]);
        }
    }

    rowY += boxH + 14.0;

    // Region 5 (deferred 3/4 skipped): Quick Lock row - Lock + Solo per category
    double catW    = (w - margin * 2.0) / (double)mutatorCatNone;

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
    render_text(mainArea, (tRectangle){{x + margin, rowY}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Patch Variations (click = Mother, Shift-click = Father)");
    rowY += STANDARD_TEXT_HEIGHT + 6.0;

    double varBoxW = (w - margin * 2.0 - 7.0 * (MUTATOR_NUM_REAL_VARIATIONS - 1)) / (double)MUTATOR_NUM_REAL_VARIATIONS;
    double varBoxH = 24.0;

    for (int v = 0; v < MUTATOR_NUM_REAL_VARIATIONS; v++) {
        double     vx      = x + margin + (varBoxW + 7.0) * v;
        tRectangle varRect = {{vx, rowY}, {varBoxW, varBoxH}};

        gMutator.variationRect[v] = varRect;

        bool       active  = ((uint32_t)v == gPatchDescr[gMutator.slot].activeVariation);
        char       label[4];
        snprintf(label, sizeof(label), "%d", v + 1);
        draw_button(mainArea, varRect, label, active ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }

    rowY += varBoxH;

    if ((rowY + margin) > (y + panel.size.h)) {
        gMutator.panelRect.size.h = (rowY + margin) - y;
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

// index: 0=Prob, 1=Range, 2=Cross Prob. Called on initial click and on every subsequent
// cursor_pos() while that slider is being dragged.
static void set_slider_from_mouse(int32_t index, tCoord coord) {
    const tRectangle * rect  = (index == 0) ? &gMutator.probSliderRect
                             : (index == 1) ? &gMutator.rangeSliderRect
                                            : &gMutator.crossSliderRect;
    double             value = (coord.x - rect->coord.x) / rect->size.w;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 1.0) {
        value = 1.0;
    }

    switch (index) {
        case 0:
            gMutator.mutateProb  = value;

            if (gMutator.linkProbRange) {
                gMutator.mutateRange = 1.0 - value;
            }
            break;

        case 1:
            gMutator.mutateRange = value;

            if (gMutator.linkProbRange) {
                gMutator.mutateProb = 1.0 - value;
            }
            break;

        default:
            gMutator.crossProb   = value;
            break;
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
                bool    shiftHeld = (glfwGetKey((GLFWwindow *)gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                                    || (glfwGetKey((GLFWwindow *)gWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
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

        if (within_rectangle(coord, gMutator.probSliderRect)) {
            gMutator.draggingSlider = 0;
            set_slider_from_mouse(0, coord);
            return true;
        }

        if (within_rectangle(coord, gMutator.rangeSliderRect)) {
            gMutator.draggingSlider = 1;
            set_slider_from_mouse(1, coord);
            return true;
        }

        if (within_rectangle(coord, gMutator.crossSliderRect)) {
            gMutator.draggingSlider = 2;
            set_slider_from_mouse(2, coord);
            return true;
        }
        // Swallow clicks inside the panel that didn't hit a control, so they don't fall through
        // to the module canvas underneath.
        return within_rectangle(coord, gMutator.panelRect);
    }

    if (mouseButton == mouseButtonLeftUp) {
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
        set_slider_from_mouse(gMutator.draggingSlider, coord);
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

        default: break;
    }
    return false;
}
