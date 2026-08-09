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

// The top bar, moved out of graphics.c so the VST3 plug-in draws the SAME ONE.
//
// It was never platform-bound — every call in it is drawing or state — it simply lived in the file
// that owns the window. The plug-in first got a hand-written bar carrying a subset of the controls;
// that was a mistake. The point is for the plug-in to look like the editor, and a second
// implementation of the same bar can only ever drift away from it.
//
// The controls that describe hardware still draw here, and should: "Offline" is the truthful state
// for a plug-in, the TX/RX indicators stay dark because gUsbTxTime/gUsbRxTime are never set, and the
// slot buttons show which slot the patch occupies. Nothing has to be hidden to be honest.

#include <math.h>

// defs.h FIRST — it defines G2_EDIT, and synthlibDefs.h gates the colour palette and the bar's own
// TOP_BAR_HEIGHT on it. Including them the other way round leaves every RGB_* undefined.
#include "defs.h"
#include "synthlibDefs.h"
#include "sysIncludes.h"
#include "synthlibTypes.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "topbarResourcesAccess.h"
#include "utils.h"
#include "graphics.h"
#include "topbarRender.h"

void render_top_bar(void) {
    tRectangle  rectangle                           = {0};
    char        patchNameCopy[CLAVIA_NAME_SIZE + 1] = {0};
    char        buff[32]                            = {0};
    tCommsState commsState                          = gCommsState;
    char *      commsStateText                      = "Unknown";
    tRgb        commsStateColour                    = (tRgb)RGB_RED_7;
    tRgb        buttonBackgroundColour              = (tRgb)RGB_BACKGROUND_GREY;
    uint32_t    slot                                = gSlot;
    uint32_t    variation                           = gPatchDescr[slot].activeVariation;
    int         voiceCount                          = 0;
    bool        clockRunning                        = gGlobalSettings.masterClockRunning;
    uint64_t    txTime                              = gUsbTxTime;
    uint64_t    rxTime                              = gUsbRxTime;
    uint64_t    nowMs                               = (uint64_t)get_time_ms();
    bool        txActive                            = (txTime != 0) && ((nowMs - txTime) < 100);
    bool        rxActive                            = (rxTime != 0) && ((nowMs - rxTime) < 100);
    tRectangle  commsStateRect                      = {0};

    set_rgb_colour((tRgb)RGB_GREY_5);
    // Full width now. The strip on the right used to be reserved for the vertical scrollbar, which
    // ran the whole window height; the per-pane bars start at their pane's top edge, which is below
    // this, so nothing lives up here any more.
    render_rectangle_with_border(mainArea, (tRectangle){{0.0, MENU_BAR_HEIGHT}, {get_render_width() / gGlobalGuiScale, TOP_BAR_HEIGHT}});

    set_rgb_colour((tRgb)RGB_BLACK);
    render_text(mainArea, (tRectangle){{400, 43 + MENU_BAR_HEIGHT}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Variation");

    COPY_STRING(patchNameCopy, gGlobalSettings.slot[slot].patchName);

    //patch_name_get(slot, patchNameCopy, sizeof(patchNameCopy));

    if (patchNameCopy[0] == '\0') {
        COPY_STRING(patchNameCopy, "---");
    }
    //set_rgb_colour((tRgb)RGB_BLACK);
    //render_text(mainArea, (tRectangle){{80, 43}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Patch Name");

    if (gPatchNameEdit.active && gPatchNameEdit.slot == slot) {
        // Show edit buffer with cursor at cursorPos
        char     displayBuf[CLAVIA_NAME_SIZE + 2] = {0};
        uint32_t cp                               = gPatchNameEdit.cursorPos;
        memcpy(displayBuf, gPatchNameEdit.buffer, cp);
        displayBuf[cp]                               = '|';
        memcpy(&displayBuf[cp + 1], &gPatchNameEdit.buffer[cp], strlen(gPatchNameEdit.buffer) - cp + 1);

        gTopbarControls[topbarPatchNameId].rectangle = draw_button(mainArea, (tRectangle){topbar_control_def(topbarPatchNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, displayBuf, (tRgb)RGB_WHITE);
    } else {
        tRgb col = gTopbarControls[topbarPatchNameId].isPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
        gTopbarControls[topbarPatchNameId].rectangle = draw_button(mainArea, (tRectangle){topbar_control_def(topbarPatchNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, patchNameCopy, col);
    }
    {
        const tTopbarControlDef * def = topbar_control_def(topbarPatchTypeId);
        tRgb                      col = gTopbarControls[topbarPatchTypeId].isPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
        gTopbarControls[topbarPatchTypeId].rectangle = draw_button(mainArea, (tRectangle){def->coord, {get_text_width("Sequencer", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)patchTypeStrMap[gPatchDescr[slot].category], col);
    }
    {
        const tTopbarControlDef * def = topbar_control_def(topbarMonoPolyId);
        tRgb                      col = gTopbarControls[topbarMonoPolyId].isPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
        gTopbarControls[topbarMonoPolyId].rectangle = draw_button(mainArea, (tRectangle){def->coord, {get_text_width("Legato", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)monoPolyStrMap[gPatchDescr[slot].monoPoly], col);
    }

    if (gPatchDescr[slot].monoPoly == monoPolyPoly) {
        voiceCount = gPatchDescr[slot].voiceCount + 1;
    } else {
        voiceCount = 1;
    }

    if ((gAssignedVoices[slot] == 0) || (gAssignedVoices[slot] == voiceCount)) {
        buttonBackgroundColour = (tRgb)RGB_BACKGROUND_GREY;
    } else {
        buttonBackgroundColour = (tRgb)RGB_RED_5;
    }
    snprintf(buff, sizeof(buff), "%u", voiceCount);
    gTopbarControls[topbarVoiceCountId].rectangle = draw_button(mainArea, (tRectangle){topbar_control_def(topbarVoiceCountId)->coord, {get_text_width("XX", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, buff, buttonBackgroundColour);

    {
        tModuleKey volKey = {slot, (uint32_t)locationMorph, patchModuleVolume};
        tModule *  module = get_module(volKey);

        if (module != NULL) {
            snprintf(buff, sizeof(buff), "%s", patchVolumeStrMap[module->param[variation][VOLUME_LEVEL].value]);
            render_text(mainArea, (tRectangle){{gTopbarControls[topbarPatchVolumeId].rectangle.coord.x, gTopbarControls[topbarPatchVolumeId].rectangle.coord.y - 12}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
            gParamRectangle[module->key.slot][module->key.location][module->key.index][VOLUME_LEVEL] = render_dial(mainArea, gTopbarControls[topbarPatchVolumeId].rectangle, module->param[variation][VOLUME_LEVEL].value, 127, 0, (tRgb)RGB_GREY_7);
        }
    }

    for (int i = 0; i < TOPBAR_STANDARD_BUTTON_COUNT; i++) {
        const tTopbarControlDef * def = topbar_control_def((tTopbarControlId)i);

        rectangle = (tRectangle){
            def->coord, {
                get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT
            }
        };

        switch (def->anchor) {
            case anchorTopRight:
            {
                rectangle.coord.x = (get_render_width() / gGlobalGuiScale) + def->coord.x;
                break;
            }
            default:
            {
                break;
            }
        }

        if (gTopbarControls[i].isPressed) {
            buttonBackgroundColour = (tRgb)RGB_GREY_7;
        } else {
            buttonBackgroundColour = gTopbarControls[i].colour;
        }
        gTopbarControls[i].rectangle = draw_button(mainArea, rectangle, def->text, buttonBackgroundColour);
    }

    commsStateColour = (tRgb)RGB_BACKGROUND_GREY;

    switch (commsState) {
        case eCommsOnLine:
            commsStateText   = "Online";
            commsStateColour = (tRgb)RGB_GREEN_7;
            break;
        default:
            commsStateText   = "Offline";
            break;
    }
    set_rgb_colour(commsStateColour);

    // Online/Offline indicator: top-left now there's room — same x as "Patch Mode" (20), same row as
    // "Undo" (y = 8). Tx/Rx sit just to its right, stacked vertically.
    double onlineX = 20.0;
    double onlineY = 8.0 + MENU_BAR_HEIGHT;
    double onlineW = get_text_width("Offline", STANDARD_BUTTON_TEXT_HEIGHT, eCache);

    // Tx (top) / Rx (bottom) activity as two blank boxes stacked to the right of Online, each roughly
    // half its height with a gap between, so Rx's bottom lines up with Online's bottom.
    double txrxGap = 3.0;
    double boxH    = (STANDARD_BUTTON_TEXT_HEIGHT - txrxGap) / 2.0;
    double boxW    = boxH; // square
    double txrxX   = onlineX + onlineW + 6.0;

    rectangle      = (tRectangle){{
                                      onlineX, onlineY
                                  }, {
                                      onlineW, STANDARD_BUTTON_TEXT_HEIGHT
                                  }
    };
    commsStateRect = draw_button(mainArea, rectangle, commsStateText, commsStateColour);
    draw_button(mainArea, (tRectangle){{txrxX, onlineY}, {boxW, boxH}}, "", txActive ? (tRgb)RGB_GREEN_7 : (tRgb)RGB_BACKGROUND_GREY);
    draw_button(mainArea, (tRectangle){{txrxX, onlineY + boxH + txrxGap}, {boxW, boxH}}, "", rxActive ? (tRgb)RGB_GREEN_7 : (tRgb)RGB_BACKGROUND_GREY);

    if (txActive || rxActive) {
        wake_glfw();
    }
    // Cable colour visibility toggles — 6 small squares
    //uint32_t hiddenMask = gHiddenCableMask;

    for (int i = 0; i < cableColourMax; i++) {
        bool                      visible  = gPatchDescr[slot].visible[i];
        tTopbarControlId          toggleId = (tTopbarControlId)((int)topbarCableColourToggle0Id + i);
        tRgb                      colour   = gTopbarControls[toggleId].isPressed ? (tRgb)RGB_GREY_7 : gCableColourMap[i];
        double                    x        = 700.0 + (i * (get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT, eCache) + 5));
        const tTopbarControlDef * def      = topbar_control_def(toggleId);

        if (visible) {
            gTopbarControls[toggleId].rectangle = draw_button(mainArea, (tRectangle){{x, def->coord.y}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, "X", colour);
        } else {
            gTopbarControls[toggleId].rectangle = draw_button(mainArea, (tRectangle){{x, def->coord.y}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, " ", colour);
        }
    }

    bool hideAll = gCablesHideAll;
    bool transp  = gCablesTransparent;

    {
        const tTopbarControlDef * def = topbar_control_def(topbarHideAllCablesId);
        tRgb                      col = gTopbarControls[topbarHideAllCablesId].isPressed ? (tRgb)RGB_GREY_7 : (hideAll ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        gTopbarControls[topbarHideAllCablesId].rectangle = draw_button(mainArea,
                                                                       (tRectangle){def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                       def->text, col);
    }
    {
        const tTopbarControlDef * def = topbar_control_def(topbarTransparentCablesId);
        tRgb                      col = gTopbarControls[topbarTransparentCablesId].isPressed ? (tRgb)RGB_GREY_7 : (transp ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        gTopbarControls[topbarTransparentCablesId].rectangle = draw_button(mainArea,
                                                                           (tRectangle){def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                           def->text, col);
    }

    snprintf(buff, sizeof(buff), "%u BPM", gGlobalSettings.masterClock);
    {
        // render_dial_with_text() is dial-anchored: the rect is the circle's bounding square and
        // text grows upwards from it. The topbar entry's coord is still where the BPM string goes,
        // so the dial itself sits one text row below that.
        tCoord tempoCoord = topbar_control_def(topbarTempoDialId)->coord;

        tempoCoord.y                                += STANDARD_BUTTON_TEXT_HEIGHT;
        gTopbarControls[topbarTempoDialId].rectangle = render_dial_with_text(mainArea, (tRectangle){tempoCoord, {20, 20}}, NULL, buff, STANDARD_BUTTON_TEXT_HEIGHT, gGlobalSettings.masterClock >= 30 ? gGlobalSettings.masterClock - 30 : 0, 211, 0, (tRgb)RGB_BACKGROUND_GREY);
    }
    {
        tRgb clockCol = gTopbarControls[topbarClockRunStopId].isPressed ? (tRgb)RGB_GREY_7 : (clockRunning ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        gTopbarControls[topbarClockRunStopId].rectangle = draw_button(mainArea, (tRectangle){topbar_control_def(topbarClockRunStopId)->coord, {get_text_width("Stopped", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)(clockRunning ? "Running" : "Stopped"), clockCol);
    }

    if (gGlobalSettings.perfMode == 1) {
        snprintf(buff, sizeof(buff), "Perf Mode");
    } else {
        snprintf(buff, sizeof(buff), "Patch Mode");
    }
    {
        tRgb perfModeCol = gTopbarControls[topbarPerfModeId].isPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
        gTopbarControls[topbarPerfModeId].rectangle = draw_button(mainArea,
                                                                  (tRectangle){{20, 42 + MENU_BAR_HEIGHT}, {get_text_width("Patch Mode", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                  buff, perfModeCol);
    }

    if (gGlobalSettings.perfMode == 1) {
        char perfNameDisplay[CLAVIA_NAME_SIZE + 2] = {0};

        if (gPerfNameEdit.active) {
            uint32_t cp = gPerfNameEdit.cursorPos;
            memcpy(perfNameDisplay, gPerfNameEdit.buffer, cp);
            perfNameDisplay[cp]                         = '|';
            memcpy(&perfNameDisplay[cp + 1], &gPerfNameEdit.buffer[cp], strlen(gPerfNameEdit.buffer) - cp + 1);
            gTopbarControls[topbarPerfNameId].rectangle = draw_button(mainArea,
                                                                      (tRectangle){topbar_control_def(topbarPerfNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                      perfNameDisplay, (tRgb)RGB_WHITE);
        } else {
            tRgb perfNameCol = gTopbarControls[topbarPerfNameId].isPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
            snprintf(perfNameDisplay, sizeof(perfNameDisplay), "%s", gGlobalSettings.perfName[0] ? gGlobalSettings.perfName : "---");
            gTopbarControls[topbarPerfNameId].rectangle = draw_button(mainArea,
                                                                      (tRectangle){topbar_control_def(topbarPerfNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                      perfNameDisplay, perfNameCol);
        }
    }
    {
        double resLabelH  = STANDARD_TEXT_HEIGHT * 0.7;
        double col1X      = 600.0;
        double col2X      = 644.0;
        double row1Y      = 44.0 + MENU_BAR_HEIGHT;
        double row2Y      = 60.0 + MENU_BAR_HEIGHT;
        double valW       = get_text_width("XX.X%", STANDARD_BUTTON_TEXT_HEIGHT, eCache);
        double labelX     = 581.0;
        double headerY    = row1Y - resLabelH - 2.0;
        double rowLabelOY = (STANDARD_TEXT_HEIGHT - resLabelH) / 2.0;

        render_text(mainArea, (tRectangle){{col1X, headerY}, {BLANK_SIZE, resLabelH}}, "Cycles");
        render_text(mainArea, (tRectangle){{col2X, headerY}, {BLANK_SIZE, resLabelH}}, "Memory");
        render_text(mainArea, (tRectangle){{labelX, row1Y + rowLabelOY}, {BLANK_SIZE, resLabelH}}, "VA");
        render_text(mainArea, (tRectangle){{labelX, row2Y + rowLabelOY}, {BLANK_SIZE, resLabelH}}, "FX");

        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].cycles[locationVa]);
        draw_button(mainArea, (tRectangle){{col1X, row1Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].cycles[locationFx]);
        draw_button(mainArea, (tRectangle){{col1X, row2Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].mem[locationVa]);
        draw_button(mainArea, (tRectangle){{col2X, row1Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].mem[locationFx]);
        draw_button(mainArea, (tRectangle){{col2X, row2Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
    }
}
