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

#include "midiCcList.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "dataBase.h"
#include "globalVars.h"
#include "graphics.h"
#include "menus.h"
#include "moduleResourcesAccess.h"
#include "paramPages.h"
#include "utilsGraphics.h"

// See midiCcList.h. Read-only: this lists what exists, it does not edit. Removing an assignment
// stays on the parameter's own right-click menu, where the parameter being acted on is unambiguous.

#define CC_LIST_COLUMNS    (3)
tMidiCcList gMidiCcList = {0};   // declared in midiCcList.h — see the note there on why it is public

void open_midi_cc_list_panel(uint32_t slot) {
    gMidiCcList.active       = true;
    gMidiCcList.slot         = (slot < MAX_SLOTS) ? slot : 0;
    gMidiCcList.closePressed = false;
    synthlib_request_redraw();
}

void close_midi_cc_list_panel(void) {
    gMidiCcList.active = false;
    synthlib_request_redraw();
}

bool midi_cc_list_active(void) {
    return gMidiCcList.active;
}

// One row's text. The module and parameter names come from paramPages.h so this panel, the
// Parameter Pages and the Parameter Overview can never disagree about what a parameter is called —
// a patch-given name wins over the paramLocationList one, which is the precedence the canvas itself
// applies.
static void row_text(const tController * controller, char * buff, size_t buffSize) {
    tModuleKey  key    = {gMidiCcList.slot, controller->location, controller->moduleIndex};
    tModule *   module = get_module(key);
    tKnobTarget target = {0};

    if (module == NULL) {
        // The module was deleted without its controller entry going with it.
        snprintf(buff, buffSize, "CC %3u  (module %u gone)", controller->midiCC, controller->moduleIndex);
        return;
    }
    target.assigned   = true;
    target.key        = key;
    target.paramIndex = controller->paramIndex;
    target.module     = module;

    if (controller->paramIndex < MAX_NUM_PARAMETERS) {
        target.paramRef = module->param[0][controller->paramIndex].paramRef;
    }
    snprintf(buff, buffSize, "CC %3u  %s  %s  %s", controller->midiCC,
             (controller->location == (uint32_t)locationVa) ? "VA" : "FX",
             param_pages_module_display_name(module),
             param_pages_knob_param_label(&target));
}

// Ascending by CC number, which is the order the question "what is CC 20 doing?" is asked in. The
// controller table itself is in assignment order and gets reshuffled by removals, so it is never a
// sensible thing to show directly.
static int compare_by_cc(const void * a, const void * b) {
    const tController * ca = (const tController *)a;
    const tController * cb = (const tController *)b;

    return (int)ca->midiCC - (int)cb->midiCC;
}

void render_midi_cc_list_panel(void) {
    static const char * kSlotLabel[MAX_SLOTS] = {"A", "B", "C", "D"};
    tController         sorted[MAX_NUM_CONTROLLERS];
    uint32_t            count                 = 0;
    double              renderW               = 0.0;
    double              renderH               = 0.0;
    // 24.0, as every other panel in the app uses. Derived from the text height it USED to be
    // (STANDARD_BUTTON_TEXT_HEIGHT + 8.0), which came to 20.0 — and the close button that
    // draw_panel_close_button() puts in the banner is inset 6.0 from the panel top and is 14.0
    // square, so it ended exactly ON the bar's bottom edge and hung out of it. The button's geometry
    // is measured from the PANEL's corner and never sees the title height, so a bar shorter than
    // 20.0 has nowhere to put it; this was the only panel not using the common value.
    double              titleH                = 24.0;
    double              btnH                  = (double)STANDARD_BUTTON_TEXT_HEIGHT;
    double              margin                = 10.0;
    double              rowH                  = btnH + 4.0;
    double              colW                  = 0.0;
    double              boxW                  = 0.0;
    double              boxH                  = 0.0;
    double              boxX                  = 0.0;
    double              boxY                  = 0.0;
    uint32_t            i                     = 0;
    uint32_t            rows                  = 0;
    char                buff[96];

    if (gMidiCcList.active == false) {
        return;
    }
    count                          = gControllerCount[gMidiCcList.slot];

    if (count > MAX_NUM_CONTROLLERS) {
        count = MAX_NUM_CONTROLLERS;
    }

    for (i = 0; i < count; i++) {
        sorted[i] = gControllerArray[gMidiCcList.slot].controller[i];
    }

    if (count > 1) {
        qsort(sorted, count, sizeof(sorted[0]), compare_by_cc);
    }
    // Width follows the widest row: nothing clips anywhere in SynthLib, so a column narrower than
    // its content would be painted over by the next one.
    colW                           = get_text_width((char *)"CC 000  VA  ModuleName  ParameterName", btnH, eCache);

    for (i = 0; i < count; i++) {
        double w = 0.0;

        row_text(&sorted[i], buff, sizeof(buff));
        w = get_text_width(buff, btnH, eCache);

        if (w > colW) {
            colW = w;
        }
    }

    colW                          += margin;
    rows                           = (count + CC_LIST_COLUMNS - 1) / CC_LIST_COLUMNS;

    if (rows < 1) {
        rows = 1;
    }

    if (rows > CC_LIST_ROWS) {
        rows = CC_LIST_ROWS;
    }
    boxW                           = (colW * CC_LIST_COLUMNS) + (margin * 2.0);
    boxH                           = titleH + (rowH * (double)(rows + 1)) + (margin * 2.0);
    renderW                        = get_render_width() / gGlobalGuiScale;
    renderH                        = get_render_height() / gGlobalGuiScale;

    if (boxW > (renderW - 20.0)) {
        boxW = renderW - 20.0;
    }
    // FLOATING, so the position comes from the panel rather than from the window: chosen once on
    // first show and thereafter wherever the user has dragged it. Centring every frame is what made
    // a panel impossible to move — it snapped back before the next redraw.
    tRectangle panelBox = floating_panel_place(&gMidiCcList.panel, boxW, boxH);

    boxX                           = panelBox.coord.x;
    boxY                           = panelBox.coord.y;

    // No draw_dialog_background_overlay(): dimming the canvas behind is what a MODAL dialog does,
    // and this is not one.
    gMidiCcList.panel.titleBarRect = draw_panel_chrome(mainArea, panelBox, titleH, "MIDI Controller List");
    gMidiCcList.close              = draw_panel_close_button(mainArea, panelBox, gMidiCcList.closePressed);
    gMidiCcList.panel.closeRect    = gMidiCcList.close;

    {
        double   slotBtnW = get_text_width((char *)"A", btnH, eCache);
        double   slotX    = (boxX + boxW) - 8.0 - BORDER_LINE_WIDTH - ((slotBtnW + 8.0) * MAX_SLOTS);
        uint32_t s        = 0;

        for (s = 0; s < MAX_SLOTS; s++) {
            tRgb colour = (s == gMidiCcList.slot) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;

            gMidiCcList.slotButton[s] = draw_button(mainArea,
                                                    (tRectangle){{slotX + (s * (slotBtnW + 8.0)), boxY + 4.0},
                                                                 {slotBtnW, btnH}
                                                    },
                                                    (char *)kSlotLabel[s], colour);
        }
    }

    set_rgb_colour((tRgb)RGB_WHITE);

    if (count == 0) {
        render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin}, {boxW, btnH}},
                    "No MIDI CC assignments in this slot");
        return;
    }
    snprintf(buff, sizeof(buff), "%u assignment%s", count, (count == 1) ? "" : "s");
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin}, {boxW, btnH}}, buff);

    for (i = 0; i < count; i++) {
        uint32_t column = i / rows;
        uint32_t row    = i % rows;
        double   x      = boxX + margin + ((double)column * colW);
        double   y      = boxY + titleH + margin + rowH + ((double)row * rowH);

        if (column >= CC_LIST_COLUMNS) {
            break;   // more than the panel can show; see CC_LIST_ROWS
        }
        row_text(&sorted[i], buff, sizeof(buff));
        render_text(mainArea, (tRectangle){{x, y}, {colW, btnH}}, buff);
    }
}

bool handle_midi_cc_list_mouse(tCoord coord, tMouseButton mouseButton) {
    uint32_t s = 0;

    if (gMidiCcList.active == false) {
        return false;
    }

    // FLOATING NOW: the move/raise/close-button routing comes from SynthLib rather than being
    // written out per panel. eFloatingPanelContent means "it landed on me, but not on my chrome" —
    // which is the only case the panel's own hit-testing below should see.
    switch (floating_panel_mouse(&gMidiCcList.panel, coord, mouseButton, gMidiCcList.closePressed)) {
        case eFloatingPanelPassThrough:
            return false;

        case eFloatingPanelConsumed:
            return true;

        case eFloatingPanelContent:
        default:
            break;
    }

    if (mouseButton == mouseButtonLeftUp) {
        if (within_rectangle(coord, gMidiCcList.close)) {
            close_midi_cc_list_panel();
            return true;
        }

        for (s = 0; s < MAX_SLOTS; s++) {
            if (within_rectangle(coord, gMidiCcList.slotButton[s])) {
                gMidiCcList.slot = s;
                synthlib_request_redraw();
                return true;
            }
        }
    }
    // Modal: swallow everything else so a click cannot reach the canvas underneath.
    return true;
}

bool handle_midi_cc_list_key(int key, int mods, int action) {
    (void)mods;

    if ((gMidiCcList.active == false) || (action != GLFW_PRESS)) {
        return false;
    }

    if (key == GLFW_KEY_ESCAPE) {
        close_midi_cc_list_panel();
    }
    return true;
}

#ifdef __cplusplus
}
#endif
