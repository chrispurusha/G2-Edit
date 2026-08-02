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

#include "paramOverlay.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "globalVars.h"
#include "menus.h"
#include "moduleResourcesAccess.h"
#include "mouseHandle.h"
#include "mutatorUI.h"
#include "paramPages.h"
#include "utilsGraphics.h"

// One row per parameter, and a patch can hold MAX_NUM_MODULES modules of MAX_NUM_PARAMETERS each -
// but only what is on screen is ever queued, and a parameter contributes at most three rows (it
// can carry a patch knob, a global knob and a MIDI CC at once). This is sized for a full screen of
// dense modules rather than for the theoretical patch; overflow simply stops queueing, which loses
// labels off the bottom of a very crowded canvas rather than misdrawing anything.
#define MAX_PARAM_OVERLAYS    1024
#define OVERLAY_TEXT_SCALE    0.8

// The label's backing box is translucent so the control stays readable underneath - the point of
// these views is to annotate the patch, not to hide it, and a dial with an opaque chip over its
// centre is just a blank square. Opaque enough that the text stays legible against whatever the
// module's colour happens to be.
#define OVERLAY_BOX_ALPHA    0.75

// Nudge for a chip sitting on a button, on top of the button's own text inset. The chip's text is
// smaller than the button's, so landing them on exactly the same origin still leaves the chip
// looking high and tight against the button's edge; a pixel down and right settles it.
#define OVERLAY_CHIP_NUDGE    1.0

static tParamOverlayMode gMode                                 = overlayModeNone;
static int               gOverlayCount                         = 0;
static tRectangle        gOverlayRect[MAX_PARAM_OVERLAYS]      = {0};
static char              gOverlayLabel[MAX_PARAM_OVERLAYS][32] = {0};

// How many rows this parameter has already contributed this frame, so a second or third row for
// the same parameter stacks below the first rather than on top of it. Reset per parameter.
static int               gRowsThisParam                        = 0;

tParamOverlayMode param_overlay_mode(void) {
    return gMode;
}

void param_overlay_set_mode(tParamOverlayMode mode) {
    if (mode < overlayModeMax) {
        gMode = mode;
        synthlib_request_redraw();
    }
}

const char * param_overlay_mode_name(tParamOverlayMode mode) {
    switch (mode) {
        case overlayModeNone:        return "Off";

        case overlayModeValues:      return "Parameter Values";

        case overlayModeMorphGroups: return "Morph Groups";

        case overlayModeKnobs:       return "Knob Assignments";

        case overlayModeMidiCc:      return "MIDI CC Assignments";

        case overlayModeMidiValues:  return "Parameter MIDI Values";

        default:                     return "";
    }
}

void param_overlay_begin_frame(void) {
    gOverlayCount = 0;
}

void param_overlay_render(void) {
    tRgb backing = (tRgb)RGB_GREY_9;

    for (int i = 0; i < gOverlayCount; i++) {
        // gOverlayRect is the TEXT rect. draw_button() would pad it and put the text back at
        // +margin inside; the backing is drawn by hand here to get an alpha on it, so recover the
        // same padding from draw_button_bounds() and inset the box around the text rather than
        // letting it hang off to one side.
        double     pad = (draw_button_bounds(gOverlayRect[i]).size.w - gOverlayRect[i].size.w) / 2.0;
        tRectangle box = {{gOverlayRect[i].coord.x - pad,        gOverlayRect[i].coord.y - pad       },
                          {gOverlayRect[i].size.w + (pad * 2.0), gOverlayRect[i].size.h + (pad * 2.0)}};

        // Translucent, and nothing is ever blanked underneath: these views annotate the patch, so
        // the dial, button or menu box being annotated has to stay visible. That is affordable
        // only because the label never repeats what the widget already shows - see
        // param_overlay_note_param() - so the chip stays small enough to sit in a corner of a
        // button rather than across its face.
        // TODO - blend enables belong inside the drawing primitives, the way render_text() already
        // manages its own; see the GL_BLEND note in todo.txt.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        set_rgba_colour((tRgba){backing.red, backing.green, backing.blue, OVERLAY_BOX_ALPHA});
        render_rectangle(moduleArea, box);
        glDisable(GL_BLEND);

        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(moduleArea, gOverlayRect[i], gOverlayLabel[i]);
    }
}

// Queues one row over the given rectangle, the way the original editor's popup boxes sit on the
// parameter rather than under it - there is no room underneath, where in a dense module the next
// row is already the next widget's label.
//
// textAnchor lines the chip up with the widget's OWN text instead of centring it. That is for the
// widgets that carry text - a button or a menu box - where centring would land the chip across the
// middle of the word and leave neither readable ("Semi" under a "0" reads as "S0mi"). Starting it
// exactly where the widget's first character starts means the chip reads as a prefix to the word
// rather than something dropped on top of it. A dial has nothing inside its circle to collide
// with, so it gets the centre.
// Further rows for the same parameter stack downwards from the first.
static void queue_row(tRectangle rectangle, const char * label, bool textAnchor) {
    if ((gOverlayCount >= MAX_PARAM_OVERLAYS) || (label[0] == '\0')) {
        return;
    }
    // One chip size everywhere - a button's chip matching its own 12px text made it noticeably
    // bigger than the one on the dial beside it, and the views read better when every chip looks
    // the same wherever it lands.
    double     textHeight = (double)STANDARD_BUTTON_TEXT_HEIGHT * OVERLAY_TEXT_SCALE;
    double     labelWidth = get_text_width(label, textHeight, eCache);
    double     rowHeight  = textHeight + 2.0;

    // draw_button() insets its text from the rect it is handed by the padding it adds around it.
    // Recover that from draw_button_bounds() rather than reaching for the constant, which isn't
    // visible to G2-Edit's own sources - see the G2_EDIT branch in synthlibDefs.h.
    double     buttonPad  = draw_button_bounds((tRectangle){
        {0.0, 0.0}, {0.0, 0.0}
    }).size.w / 2.0;
    double     x          = textAnchor ? rectangle.coord.x + buttonPad + OVERLAY_CHIP_NUDGE
                            : rectangle.coord.x + ((rectangle.size.w - labelWidth) / 2.0);
    double     y          = textAnchor ? rectangle.coord.y + buttonPad + OVERLAY_CHIP_NUDGE
                            : rectangle.coord.y + ((rectangle.size.h - rowHeight) / 2.0);
    tRectangle labelRect  = {{x,          y + (rowHeight * (double)gRowsThisParam)},
                             {labelWidth, textHeight                              }};

    COPY_STRING(gOverlayLabel[gOverlayCount], label);
    gOverlayRect[gOverlayCount] = labelRect;
    gOverlayCount++;
    gRowsThisParam++;
}

// "A 1 3" for a patch knob, "G A 1 3" for a global one, from a 0-119 knob index. The 15 pages are
// a 5 x 3 matrix, so 24 knobs to a lettered row - the same decode menus.c uses when it builds the
// assign menu.
static void queue_knob_row(tRectangle rectangle, int32_t knobIdx, bool isGlobal, bool maskWidget) {
    int  page = knobIdx / 24;
    int  bank = (knobIdx % 24) / 8;
    int  pos  = knobIdx % 8;
    char label[16];

    if (isGlobal) {
        snprintf(label, sizeof(label), "G %c %d %d", 'A' + page, bank + 1, pos + 1);
    } else {
        snprintf(label, sizeof(label), "%c %d %d", 'A' + page, bank + 1, pos + 1);
    }
    queue_row(rectangle, label, maskWidget);
}

// The knob and CC rows, shared by the hover behaviour and the two assignment overlays. `wanted`
// selects which of them apply.
static void queue_assignment_rows(tModule * module, uint32_t paramIndex, tRectangle rectangle,
                                  bool wantKnobs, bool wantCc, bool maskWidget) {
    if (wantKnobs) {
        int32_t localKnobIdx  = find_knob_for_param(module->key.slot, module->key.location, module->key.index, paramIndex);
        int32_t globalKnobIdx = find_global_knob_for_param(module->key.slot, module->key.location, module->key.index, paramIndex);

        if (localKnobIdx >= 0) {
            queue_knob_row(rectangle, localKnobIdx, false, maskWidget);
        }

        if (globalKnobIdx >= 0) {
            queue_knob_row(rectangle, globalKnobIdx, true, maskWidget);
        }
    }

    if (wantCc) {
        int32_t ccIdx = find_controller_for_param(module->key.slot, module->key.location, module->key.index, paramIndex);

        if (ccIdx >= 0) {
            char ccLabel[16];

            snprintf(ccLabel, sizeof(ccLabel), "CC %u", gControllerArray[module->key.slot].controller[ccIdx].midiCC);
            queue_row(rectangle, ccLabel, maskWidget);
        }
    }
}

// True when a panel is covering the canvas, in which case no overlay should be drawn at all - the
// labels would land on top of the panel, and in the Parameter Pages case the rectangle handed in
// is the panel's own widget rather than anything on the canvas.
static bool canvas_obscured(void) {
    return gParamPages.active || gPatchSettingsEdit.active || gPerfSettingsEdit.active
           || gPatchParamsEdit.active || gPatchNotesEdit.active;
}

void param_overlay_note_param(tModule * module, uint32_t paramIndex, tRectangle rectangle, const char * displayValue) {
    if ((module == NULL) || (paramIndex >= MAX_NUM_PARAMETERS)) {
        return;
    }
    gRowsThisParam = 0;

    if (canvas_obscured()) {
        return;
    }
    uint32_t variation    = gPatchDescr[module->key.slot].activeVariation;
    tParam * param        = &module->param[variation][paramIndex];
    char     label[32]    = {0};
    // A widget that draws TEXT of its own inside the rect - a toggle, menu, enable or bypass
    // button. Those get the chip lined up with that text so the word stays readable. Dials and
    // sliders put their text in the rows ABOVE the rect, which the chip never reaches, so they
    // take it centred; the caller handing us a non-empty display string is what distinguishes them.
    //
    // No allowance is needed for a toggle's own label row: render_paramType1StandardToggle() is
    // button-anchored now, so the rect IS the button whether or not the param carries a label.
    bool     isTextWidget = (displayValue == NULL) || (displayValue[0] == '\0');

    switch (gMode) {
        case overlayModeNone:
        {
            // The long-standing hover behaviour: assignment labels for the one parameter under the
            // mouse. Skipped outright during a cursor-hiding drag, when the reported pointer
            // position is a relative-delta accumulator rather than a real point and can drift over
            // an unrelated parameter.
            tCoord mouseCoord = {0};

            if (is_cursor_hidden_dragging()) {
                return;
            }
            get_global_gui_scaled_mouse_coord(&mouseCoord);

            if (gMutator.active && within_rectangle(mouseCoord, gMutator.panelRect)) {
                return;
            }

            if (!within_rectangle(mouseCoord, gParamRectangle[module->key.slot][module->key.location][module->key.index][paramIndex])) {
                return;
            }
            queue_assignment_rows(module, paramIndex, rectangle, true, true, isTextWidget);
            break;
        }

        case overlayModeValues:

            // The raw wire value ONLY. Nothing is blanked any more, so whatever the widget renders
            // for itself is still on screen - a dial's "554.4Hz" in the row above it, a button's
            // own "Semi" alongside the chip - and printing it again here would be the same string
            // twice. Raw and formatted still end up next to each other, which is what the original
            // shows; the difference is that the formatted half comes from the control itself.
            snprintf(label, sizeof(label), "%u", param->value);
            queue_row(rectangle, label, isTextWidget);
            break;

        case overlayModeMorphGroups:

            // A parameter belongs to a morph group by having a non-zero range recorded against it;
            // it can be in more than one, so every group that applies gets a row.
            for (uint32_t group = 0; group < NUM_MORPHS; group++) {
                if (param->morphRange[group] != 0) {
                    snprintf(label, sizeof(label), "%s", morphStrMap[group]);
                    queue_row(rectangle, label, isTextWidget);
                }
            }

            break;

        case overlayModeKnobs:
            queue_assignment_rows(module, paramIndex, rectangle, true, false, isTextWidget);
            break;

        case overlayModeMidiCc:
            queue_assignment_rows(module, paramIndex, rectangle, false, true, isTextWidget);
            break;

        case overlayModeMidiValues:
        {
            // What the parameter's current value becomes on the wire. A CC carries 0-127, so a
            // parameter whose range is not 128 is scaled into that.
            // NEEDS A HARDWARE CHECK: this assumes a plain proportional scale. The G2 may well
            // round differently, and the manual only says the view shows "how each knob is
            // actually sent and received over MIDI" without stating the mapping.
            uint32_t range   = paramLocationList[param->paramRef].range;
            uint32_t midiVal = param->value;

            if ((range > 1) && (range != 128)) {
                midiVal = (param->value * 127) / (range - 1);
            }
            snprintf(label, sizeof(label), "%u", midiVal);
            queue_row(rectangle, label, isTextWidget);
            break;
        }

        default:
            break;
    }
}

#ifdef __cplusplus
}
#endif
