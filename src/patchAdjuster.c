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

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include <math.h>
#include <string.h>

#include "patchAdjuster.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "dataBase.h"
#include "globalVars.h"
#include "graphics.h"
#include "moduleResourcesAccess.h"
#include "mouseHandle.h"
#include "protocol.h"
#include "utilsGraphics.h"

tPatchAdjuster           gPatchAdjuster              = {0};

static const char *const kKnobLabel[adjusterKnobMax] = {
    "Attack", "Decay", "Sustain", "Release", "Mod. Rate", "Timbre", "Resonance", "Effects"
};

#define PA_KNOB_SIZE      (30.0)
#define PA_KNOB_GAP       (14.0)
#define PA_DRAG_PIXELS    (140.0)   // vertical travel for the full -50..+50 sweep

// ─── Classification ──────────────────────────────────────────────────────────
//
// Which of the eight knobs owns a parameter. The original does this by matching the G2's own
// parameter-TYPE id (CPatch::ApplyDistribution takes one, and each SetGlobal*Modifier calls it
// once per id it claims — Attack 0x0b; Decay 0x0c,0x10; Sustain 0x0d; Release 0x0e; Mod Rate
// 0x16,0x41,0x88; Resonance 0x12,0x87; Timbre 0x05,0x06,0x11,0x22,0x8b; Effects 0x29,0x83).
// G2-Edit's paramLocationList does NOT carry those ids — it has its own tParamType plus a label —
// so the ids can't be used directly and this classifies on (module type, param type, label)
// instead, the same way mutator.c's classify_param() already does.
//
// THAT MAKES THIS AN APPROXIMATION OF THE ORIGINAL'S COVERAGE, NOT A REPRODUCTION OF IT. The
// manual's own descriptions are loose in the same places ("various waveshape parameters", "some
// Time parameters in the multi-stage envelopes"), so exact parity was never reachable from the
// documentation either. Where a parameter's role is unambiguous from its type — a filter cutoff,
// a resonance, an LFO rate, an envelope stage — it is claimed. Where it would take a guess, it is
// left alone: a knob that misses a parameter is a smaller sin than one that mangles an unrelated
// one, since every move here is invisible until you hear it.
//
// Known gaps, all of the same kind — a parameter whose module gives it meaning but whose type is
// the catch-all paramTypeCommonDial:
//   * LfoB's rate (two undistinguished CommonDials; which is the rate isn't derivable here)
//   * FM amounts and shaper amounts, which Timbre should claim per the manual
//   * dry/wet on FX modules that don't label it "Dry/Wet"
// Extending this is a matter of adding module-name cases below, exactly as the Mutator does.

static bool name_is(const char * name, const char * want) {
    return strcmp(name, want) == 0;
}

static bool name_starts(const char * name, const char * prefix) {
    return strncmp(name, prefix, strlen(prefix)) == 0;
}

static bool label_is(const char * label, const char * want) {
    return (label != NULL) && (strcmp(label, want) == 0);
}

tAdjusterKnob adjuster_classify_param(tModuleType moduleType, tLocation location,
                                      tParamType paramType, const char * label) {
    const char * name = gModuleProperties[moduleType].name;

    (void)location;   // an envelope in the FX area is still an envelope

    // ── Envelope stages ───────────────────────────────────────────────────
    // The stage is carried by the LABEL, not the type: paramTypeADRTime is every envelope time
    // there is. The multi-stage envelopes use T1..T4 / D1 / D2, which the manual folds into Decay
    // ("some Time parameters in the multi-stage envelopes").
    if (paramType == paramTypeADRTime) {
        if (label_is(label, "Attack") || label_is(label, "A") || label_is(label, "T1")) {
            return adjusterAttack;
        }

        if (  label_is(label, "Decay") || label_is(label, "Dcy") || label_is(label, "D")
           || label_is(label, "D1") || label_is(label, "D2")
           || label_is(label, "T2") || label_is(label, "T3")) {
            return adjusterDecay;
        }

        if (label_is(label, "Release") || label_is(label, "Rel") || label_is(label, "R") || label_is(label, "T4")) {
            return adjusterRelease;
        }
        // "Hold" deliberately unclaimed — the manual gives it to no knob.
        return adjusterNone;
    }

    // Sustain is a level, not a time, so it comes through as the 0-64 unit type.
    if ((paramType == paramTypeUniPol) && label_is(label, "Sus")) {
        return adjusterSustain;
    }

    // Compressor, noise gate and envelope follower Attack/Release — named the same, typed
    // differently from an envelope's stages.
    if (name_is(name, "Compress") || name_is(name, "NoiseGate") || name_is(name, "EnvFollow")) {
        if (label_is(label, "Attack") || label_is(label, "Atk")) {
            return adjusterAttack;
        }

        if (label_is(label, "Release") || label_is(label, "Rel")) {
            return adjusterRelease;
        }
    }

    // ── Mod. Rate ─────────────────────────────────────────────────────────
    // "All LFO and Random rates, and the rates of all effect modules incorporating an internal
    // LFO, such as Chorus, Phaser and Flanger."
    if (paramType == paramTypeLFORate) {
        return adjusterModRate;
    }

    if (  (name_starts(name, "Lfo") || name_starts(name, "Random") || name_starts(name, "Rnd"))
       && (label_is(label, "Rate") || label_is(label, "Freq"))) {
        return adjusterModRate;
    }

    if (  (name_starts(name, "Phaser") || name_starts(name, "Flanger") || name_starts(name, "Chorus"))
       && (label_is(label, "Rate") || label_is(label, "Speed"))) {
        return adjusterModRate;
    }

    // ── Resonance ─────────────────────────────────────────────────────────
    // "All filter resonances, and Phaser and Flanger feedback."
    if (paramType == paramTypeResonance) {
        return adjusterResonance;
    }

    if (name_starts(name, "Flt") && (label_is(label, "Res") || label_is(label, "Resonance"))) {
        return adjusterResonance;
    }

    if ((name_starts(name, "Phaser") || name_starts(name, "Flanger")) && label_is(label, "Feedback")) {
        return adjusterResonance;
    }

    // ── Timbre ────────────────────────────────────────────────────────────
    // "Filter cutoff frequencies, FM modulation amounts, various waveshape parameters in
    // oscillators and shapers, as well as various FX parameters affecting the timbre of the
    // sound." Cutoff is the unambiguous part and the one that carries the effect; the FM and
    // shaper amounts are the gap noted at the top of this section.
    if (paramType == paramTypeFreq) {
        return adjusterTimbre;
    }

    if ((paramType == paramTypeGeneralFreq) || (paramType == paramTypeShape)) {
        return adjusterTimbre;
    }

    if (name_starts(name, "Shp") && (label_is(label, "Amount") || label_is(label, "Shape"))) {
        return adjusterTimbre;
    }

    // ── Effects ───────────────────────────────────────────────────────────
    // "Dry/wet effect parameters."
    if (label_is(label, "Dry/Wet") || label_is(label, "DryWet") || label_is(label, "Mix")) {
        return adjusterEffects;
    }
    return adjusterNone;
}

// ─── Baseline ────────────────────────────────────────────────────────────────

// Snapshots the active variation of every module the knobs could touch. Everything the panel does
// is computed from this rather than from live values, which is what lets a knob be returned to
// centre and restore its category exactly.
static void take_baseline(void) {
    uint32_t slot      = gPatchAdjuster.slot;
    uint32_t variation = gPatchDescr[slot].activeVariation;
    uint32_t count     = 0;

    memset(gPatchAdjuster.baseline, 0, sizeof(gPatchAdjuster.baseline));

    for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
        for (uint32_t idx = 0; idx < MAX_NUM_MODULES; idx++) {
            tModule * module    = get_module_slot(slot, loc, idx);

            if ((module == NULL) || !module->active) {
                continue;
            }
            count++;
            uint32_t  numParams = module_param_count(module->type);

            if (numParams > MAX_NUM_PARAMETERS) {
                numParams = MAX_NUM_PARAMETERS;
            }

            for (uint32_t p = 0; p < numParams; p++) {
                gPatchAdjuster.baseline[loc][idx][p] = (uint8_t)module->param[variation][p].value;
            }
        }
    }

    gPatchAdjuster.variation    = variation;
    gPatchAdjuster.moduleCount  = count;
    gPatchAdjuster.haveBaseline = true;
}

// ─── Applying ────────────────────────────────────────────────────────────────

// The original's curve, from CPatch::ApplyDistribution(): interpolate the baseline toward the
// parameter's maximum going right, and toward zero going left, so centre is always a no-op and the
// two halves meet cleanly at the original value.
static uint32_t adjusted_value(uint32_t orig, uint32_t max, int32_t amount) {
    if (amount == 0) {
        return orig;
    }

    if (amount > 0) {
        return (uint32_t)lround((double)orig + (((double)max - (double)orig) * ((double)amount / (double)ADJUSTER_RANGE)));
    }
    return (uint32_t)lround((double)orig * ((double)(ADJUSTER_RANGE + amount) / (double)ADJUSTER_RANGE));
}

// Recomputes every classified parameter from the baseline and the current knob positions, and
// sends only those whose value actually changes. The "only what changed" test is the original's
// too, and it matters: a Timbre sweep over a dense patch touches a lot of parameters, and without
// it every mouse-move would re-send all of them.
//
// Sent one parameter at a time rather than as a whole-patch write, deliberately — the opposite of
// the bulk MIDI CC tools. send_param_value() is a COMMAND_WRITE_NO_RESP write that expects no
// acknowledgement, which is what the canvas's own dial drags have always used, so there is no ack
// to be left in the pipe and no patch-version race to lose (see the note in menus.c).
static void adjuster_apply(void) {
    uint32_t slot      = gPatchAdjuster.slot;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    if (!gPatchAdjuster.haveBaseline) {
        return;
    }

    for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
        for (uint32_t idx = 0; idx < MAX_NUM_MODULES; idx++) {
            tModule * module    = get_module_slot(slot, loc, idx);

            if ((module == NULL) || !module->active) {
                continue;
            }
            uint32_t  numParams = module_param_count(module->type);

            if (numParams > MAX_NUM_PARAMETERS) {
                numParams = MAX_NUM_PARAMETERS;
            }
            uint32_t  seen      = 0;

            for (uint32_t i = 0; i < array_size_param_location_list(); i++) {
                const tParamLocation * loc2       = &paramLocationList[i];

                if (loc2->moduleType != module->type) {
                    continue;
                }

                if (seen >= numParams) {
                    break;
                }
                uint32_t               paramIndex = seen++;
                tAdjusterKnob          knob       = adjuster_classify_param(module->type, (tLocation)loc,
                                                                            loc2->type, loc2->label);

                if ((knob == adjusterNone) || (gPatchAdjuster.amount[knob] == 0)) {
                    continue;
                }
                uint32_t               max        = (loc2->range > 0) ? (loc2->range - 1) : 0;
                uint32_t               orig       = gPatchAdjuster.baseline[loc][idx][paramIndex];

                if (orig > max) {
                    orig = max;   // a baseline from a differently-ranged variation can't overshoot
                }
                uint32_t               value      = adjusted_value(orig, max, gPatchAdjuster.amount[knob]);

                if (value > max) {
                    value = max;
                }

                if (value == module->param[variation][paramIndex].value) {
                    continue;
                }
                module->param[variation][paramIndex].value = (uint8_t)value;

                tModuleKey             key        = {slot, loc, idx};
                send_param_value(slot, key, paramIndex, variation, value);
            }
        }
    }

    synthlib_request_redraw();
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void open_patch_adjuster_panel(uint32_t slot) {
    memset(gPatchAdjuster.amount, 0, sizeof(gPatchAdjuster.amount));
    gPatchAdjuster.active       = true;
    gPatchAdjuster.slot         = slot;
    gPatchAdjuster.closePressed = false;
    gPatchAdjuster.dragKnob     = -1;
    take_baseline();
}

void close_patch_adjuster_panel(void) {
    // Closing COMMITS whatever the knobs are showing, rather than reverting it. That matches the
    // original's model, where the only way back is to return a knob to centre while the panel is
    // still up — the adjustment is a real edit to the parameters, not a preview layered over them.
    gPatchAdjuster.active       = false;
    gPatchAdjuster.dragKnob     = -1;
    memset(gPatchAdjuster.amount, 0, sizeof(gPatchAdjuster.amount));
    gPatchAdjuster.haveBaseline = false;
}

void adjuster_note_patch_changed(void) {
    if (!gPatchAdjuster.active) {
        return;
    }
    memset(gPatchAdjuster.amount, 0, sizeof(gPatchAdjuster.amount));
    take_baseline();
    synthlib_request_redraw();
}

// Notices the two things the manual says commit the adjustment — a variation change and a module
// added or removed — without needing either of those code paths to know this panel exists. Cheap
// enough to run once a frame: a module count and a variation byte.
static void check_for_commit(void) {
    uint32_t slot  = gPatchAdjuster.slot;
    uint32_t count = 0;

    if (gPatchDescr[slot].activeVariation != gPatchAdjuster.variation) {
        adjuster_note_patch_changed();
        return;
    }

    for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
        for (uint32_t idx = 0; idx < MAX_NUM_MODULES; idx++) {
            tModule * module = get_module_slot(slot, loc, idx);

            if ((module != NULL) && module->active) {
                count++;
            }
        }
    }

    if (count != gPatchAdjuster.moduleCount) {
        adjuster_note_patch_changed();
    }
}

// ─── Rendering ───────────────────────────────────────────────────────────────

void render_patch_adjuster_panel(void) {
    if (!gPatchAdjuster.active) {
        return;
    }
    check_for_commit();

    double renderW = get_render_width() / gGlobalGuiScale;
    double renderH = get_render_height() / gGlobalGuiScale;
    double margin  = 12.0;
    double titleH  = 24.0;
    double btnH    = STANDARD_BUTTON_TEXT_HEIGHT;
    double textH   = STANDARD_TEXT_HEIGHT;

    // One column per knob, each as wide as its label needs.
    double colW    = PA_KNOB_SIZE;

    for (uint32_t k = 0; k < adjusterKnobMax; k++) {
        colW = fmax(colW, get_text_width((char *)kKnobLabel[k], textH, eCache));
    }

    colW                += PA_KNOB_GAP;

    double boxW    = (margin * 2.0) + (colW * (double)adjusterKnobMax);
    double boxH    = titleH + margin + textH + 4.0 + PA_KNOB_SIZE + 4.0 + textH + margin + btnH + margin;
    double boxX    = (renderW - boxW) / 2.0;
    double boxY    = (renderH - boxH) / 2.0;
    double y       = boxY + titleH + margin;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH, "Patch Adjuster");
    gPatchAdjuster.close = draw_panel_close_button(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, gPatchAdjuster.closePressed);

    for (uint32_t k = 0; k < adjusterKnobMax; k++) {
        double     cx     = boxX + margin + (k * colW) + (colW / 2.0);
        double     labelW = get_text_width((char *)kKnobLabel[k], textH, eCache);

        set_rgb_colour((tRgb)RGB_GREY_3);
        render_text(mainArea, (tRectangle){{cx - (labelW / 2.0), y}, {BLANK_SIZE, textH}}, kKnobLabel[k]);

        // The knob. Drawn by hand rather than through render_param_common(), because this is not a
        // patch parameter and has no paramLocationList entry to render from — it is a bare -50..+50
        // control that happens to look like a dial.
        double     knobY  = y + textH + 4.0;
        tRectangle knob   = {{cx - (PA_KNOB_SIZE / 2.0), knobY}, {PA_KNOB_SIZE, PA_KNOB_SIZE}};

        gPatchAdjuster.knobRect[k] = knob;

        set_rgb_colour((gPatchAdjuster.amount[k] != 0) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_GREY_9);
        render_circle_part(mainArea, (tCoord){cx, knobY + (PA_KNOB_SIZE / 2.0)}, PA_KNOB_SIZE / 2.0, 16, 0, 16);
        set_rgb_colour((tRgb)RGB_GREY_3);
        render_circle_line(mainArea, (tCoord){cx, knobY + (PA_KNOB_SIZE / 2.0)}, PA_KNOB_SIZE / 2.0, 16, 1.0);

        // Pointer. Centre is straight up; the travel is +/-135 degrees, the usual dial sweep.
        {
            double angle = (-135.0 + (((double)gPatchAdjuster.amount[k] + ADJUSTER_RANGE) * 270.0 / (ADJUSTER_RANGE * 2.0))) * M_PI / 180.0;
            double r     = (PA_KNOB_SIZE / 2.0) - 3.0;
            tCoord mid   = {cx, knobY + (PA_KNOB_SIZE / 2.0)};

            set_rgb_colour((tRgb)RGB_BLACK);
            render_line(mainArea, mid, (tCoord){mid.x + (sin(angle) * r), mid.y - (cos(angle) * r)}, 2.0);
        }

        // The centre marker, which the manual says can be clicked to return the knob to the middle.
        {
            char   value[16] = {0};
            double vw        = 0.0;

            snprintf(value, sizeof(value), "%+d", gPatchAdjuster.amount[k]);
            vw                           = get_text_width(value, textH, eCache);
            gPatchAdjuster.centreRect[k] = (tRectangle){{
                                                            cx - (vw / 2.0) - 3.0, knobY + PA_KNOB_SIZE + 4.0
                                                        }, {
                                                            vw + 6.0, textH
                                                        }
            };

            // Always legible, never greyed. This doubles as the manual's clickable centre marker
            // ("just turn the knob back to its middle position, or click the centre marker"), so
            // drawing it faintly at centre — which is exactly when you want to aim at it — would
            // hide the control at the moment it matters. Off-centre it goes black to stand out.
            set_rgb_colour((gPatchAdjuster.amount[k] != 0) ? (tRgb)RGB_BLACK : (tRgb)RGB_GREY_3);
            render_text(mainArea, (tRectangle){{cx - (vw / 2.0), knobY + PA_KNOB_SIZE + 4.0}, {BLANK_SIZE, textH}}, value);
        }
    }

    // Reset All — not in the original, which only offers the per-knob centre marker, but eight
    // knobs is enough that getting back to neutral in one click earns its place.
    {
        double bw = get_text_width((char *)"Reset All", btnH, eCache) + 14.0;

        gPatchAdjuster.resetAll = draw_button(mainArea,
                                              (tRectangle){{boxX + boxW - margin - bw, boxY + boxH - margin - btnH}, {bw, btnH}},
                                              "Reset All", (tRgb)RGB_BACKGROUND_GREY);
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

static void set_knob(uint32_t k, int32_t amount) {
    if (amount < -ADJUSTER_RANGE) {
        amount = -ADJUSTER_RANGE;
    }

    if (amount > ADJUSTER_RANGE) {
        amount = ADJUSTER_RANGE;
    }

    if (gPatchAdjuster.amount[k] == amount) {
        return;
    }
    gPatchAdjuster.amount[k] = amount;
    adjuster_apply();
}

bool handle_patch_adjuster_mouse(tCoord coord, tMouseButton mouseButton) {
    if (!gPatchAdjuster.active) {
        return false;
    }

    if (mouseButton == mouseButtonLeftDown) {
        if (within_rectangle(coord, gPatchAdjuster.close)) {
            gPatchAdjuster.closePressed = true;
        } else {
            for (uint32_t k = 0; k < adjusterKnobMax; k++) {
                if (within_rectangle(coord, gPatchAdjuster.knobRect[k])) {
                    gPatchAdjuster.dragKnob        = (int32_t)k;
                    gPatchAdjuster.dragStartY      = coord.y;
                    gPatchAdjuster.dragStartAmount = gPatchAdjuster.amount[k];
                    break;
                }
            }
        }
    }

    if (mouseButton == mouseButtonLeftUp) {
        bool wasClosePressed = gPatchAdjuster.closePressed;
        bool wasDragging     = gPatchAdjuster.dragKnob >= 0;

        gPatchAdjuster.closePressed = false;
        gPatchAdjuster.dragKnob     = -1;

        if (wasClosePressed && within_rectangle(coord, gPatchAdjuster.close)) {
            close_patch_adjuster_panel();
        } else if (!wasDragging) {
            if (within_rectangle(coord, gPatchAdjuster.resetAll)) {
                for (uint32_t k = 0; k < adjusterKnobMax; k++) {
                    gPatchAdjuster.amount[k] = 0;
                }

                adjuster_apply();
            } else {
                for (uint32_t k = 0; k < adjusterKnobMax; k++) {
                    if (within_rectangle(coord, gPatchAdjuster.centreRect[k])) {
                        set_knob(k, 0);   // "just turn the knob back to its middle position, or click the centre marker"
                        break;
                    }
                }
            }
        }
    }
    synthlib_request_redraw();
    return true;
}

// Vertical drag, the same gesture the canvas's non-rotary dial modes use. Called from cursor_pos()
// only while a knob is captured, so it can't be confused with an ordinary mouse move.
void handle_patch_adjuster_cursor_pos(tCoord coord) {
    if (!gPatchAdjuster.active || (gPatchAdjuster.dragKnob < 0)) {
        return;
    }
    double  delta  = gPatchAdjuster.dragStartY - coord.y;   // up is positive, as on a real dial
    int32_t amount = gPatchAdjuster.dragStartAmount + (int32_t)lround((delta / PA_DRAG_PIXELS) * (ADJUSTER_RANGE * 2.0));

    set_knob((uint32_t)gPatchAdjuster.dragKnob, amount);
}

// ─── Keyboard ────────────────────────────────────────────────────────────────

bool handle_patch_adjuster_key(int key, int mods, int action) {
    (void)mods;

    if (!gPatchAdjuster.active || (action != GLFW_PRESS)) {
        return false;
    }

    if (key != GLFW_KEY_ESCAPE) {
        return false;
    }
    close_patch_adjuster_panel();
    synthlib_request_redraw();
    return true;
}

#ifdef __cplusplus
}
#endif
