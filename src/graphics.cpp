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

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include "math.h"
#include "defs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "protocol.h"
#include "usbComms.h"
#include "graphics.h"
#include "utilsGraphics.h"
#include "mouseHandle.h"
#include "dataBase.h"
#include "moduleGraphics.h"
#include "fileDialogue.h"
#include "moduleResourcesAccess.h"
#include "topbarResourcesAccess.h"
#include "globalVars.h"
#include "synthSettingsResources.h"
#include "patchParamsResources.h"
#include "perfSettingsResources.h"

static FT_Library      gLibrary        = {0};
static FT_Face         gFace           = {0};
static _Atomic bool    gNeedFocus      = false;

#define MAX_NOTE_VISUAL_LINES    1000

typedef struct {
    int  bufStart;
    int  bufEnd;
    bool hardBreak;
} tNoteVisualLine;

static tNoteVisualLine gNoteLines[MAX_NOTE_VISUAL_LINES];
static int             gNoteLineCount  = 0;
static int             gNoteScrollLine = 0;
static double          gNoteTextX      = 0.0;
static double          gNoteTextY0     = 0.0;
static double          gNoteLineH      = 0.0;
static double          gNoteTextW      = 0.0;
static double          gNoteTextHParam = 0.0;

static int find_wrap_point(const char * text, int textLen, double textW, double textH) {
    if (textLen <= 0) {
        return 0;
    }
    char tmp[PATCH_NOTES_SIZE + 1];
    COPY_STRING(tmp, text);

    if (get_text_width(tmp, textH, eNoCache) <= textW) {
        return textLen;
    }
    int  lo = 1, hi = textLen;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;

        COPY_STRING(tmp, text);

        if (get_text_width(tmp, textH, eNoCache) <= textW) {
            lo = mid;
        } else{
            hi = mid - 1;
        }
    }
    int  charBreak = lo;

    int  wordBreak = charBreak;

    while (wordBreak > 0 && text[wordBreak - 1] != ' ') {
        wordBreak--;
    }
    return (wordBreak > 0) ? wordBreak : charBreak;
}

static void build_note_visual_lines(const char * buf, double textW, double textH) {
    gNoteLineCount = 0;
    int len = (int)strlen(buf);
    int pos = 0;

    while (gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
        int logicalEnd = pos;

        while (logicalEnd < len && buf[logicalEnd] != '\r') {
            logicalEnd++;
        }
        int segStart   = pos;

        while (gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
            int  remaining = logicalEnd - segStart;

            if (remaining <= 0) {
                if (segStart == pos) {
                    gNoteLines[gNoteLineCount++] = {segStart, segStart, true};
                }
                break;
            }
            int  wrapAt    = find_wrap_point(buf + segStart, remaining, textW, textH);
            bool softWrap  = (wrapAt < remaining);
            gNoteLines[gNoteLineCount++] = {segStart, segStart + wrapAt, !softWrap};
            segStart                    += wrapAt;

            if (!softWrap) {
                break;
            }
        }

        if (logicalEnd >= len) {
            break;
        }
        pos = logicalEnd + 1;

        if (pos >= len && gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
            gNoteLines[gNoteLineCount++] = {len, len, true};
            break;
        }
    }

    if (gNoteLineCount == 0) {
        gNoteLines[gNoteLineCount++] = {0, 0, true};
    }
}

static int find_note_cursor_line(int cursorPos) {
    int result = 0;

    for (int i = 0; i < gNoteLineCount; i++) {
        if (gNoteLines[i].bufStart <= cursorPos) {
            result = i;
        }
    }

    return result;
}

void framebuffer_size_callback(GLFWwindow * window, int width, int height) {
    glViewport(0, 0, width, height);
    gReDraw = true;
}

void window_close_callback(GLFWwindow * window) {
    gReDraw = false;

    glfwSetFramebufferSizeCallback(gWindow, NULL);
    glfwSetWindowCloseCallback(gWindow, NULL);
    glfwSetKeyCallback(gWindow, NULL);
    glfwSetCharCallback(gWindow, NULL);
    glfwSetCursorPosCallback(gWindow, NULL);
    glfwSetMouseButtonCallback(gWindow, NULL);
    glfwSetScrollCallback(gWindow, NULL);

    glfwSetWindowShouldClose(gWindow, GLFW_TRUE);
    glfwPostEmptyEvent();
}

void set_window_title(const char * filePath) {
    char         newTitle[100] = {0};
    const char * filename      = strrchr(filePath, '/');

    if (filename) {
        filename += 1;  // Skip the slash
    } else {
        filename = filePath;
    }
    snprintf(newTitle, sizeof(newTitle), "%s - %s", WINDOW_TITLE, filename);
    glfwSetWindowTitle(gWindow, newTitle);
}

void error_callback(int error, const char * description) {
    LOG_ERROR("GLFW error [%d]: %s\n", error, description);
}

void render_context_menu(void) {
    double     size        = 0.0;
    double     largestSize = 0.0;
    tCoord     mouseCoord  = {0};
    tRectangle menuItem    = {0};
    double     itemHeight  = STANDARD_TEXT_HEIGHT;
    uint32_t   columns     = (gContextMenu.columns > 1) ? gContextMenu.columns : 1;

    if (!gContextMenu.active) {
        return;
    }
    get_global_gui_scaled_mouse_coord(&mouseCoord);

    if (gContextMenu.items != NULL) {
        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            size = get_text_width(gContextMenu.items[i].label, itemHeight, eNoCache);

            if (size > largestSize) {
                largestSize = size;
            }
        }

        double computed = (largestSize + (5 * 2) > itemHeight) ? largestSize + (5 * 2) : itemHeight;
        double cellW    = (gContextMenu.cellWidth > 0.0) ? gContextMenu.cellWidth : computed;
        double cellH    = itemHeight + (5 * 2);

        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            int    col = (int)(i % columns);
            int    row = (int)(i / columns);
            double x   = gContextMenu.coord.x + col * cellW;
            double y   = gContextMenu.coord.y + row * cellH;
            menuItem = {{x, y}, {cellW, cellH}};

            set_rgb_colour(gContextMenu.items[i].colour);
            render_rectangle(mainArea, menuItem);

            set_rgb_colour(RGB_GREY_9);
            render_text(mainArea, {{x + 5, y + 5}, {BLANK_SIZE, itemHeight}}, gContextMenu.items[i].label);

            if (columns > 1) {
                set_rgb_colour(RGB_BLACK);
                render_line(mainArea, {x, y}, {x + cellW, y}, 1);
                render_line(mainArea, {x + cellW, y}, {x + cellW, y + cellH}, 1);
                render_line(mainArea, {x, y + cellH}, {x + cellW, y + cellH}, 1);
                render_line(mainArea, {x, y}, {x, y + cellH}, 1);
            }
        }

        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            int    col = (int)(i % columns);
            int    row = (int)(i / columns);
            double x   = gContextMenu.coord.x + col * cellW;
            double y   = gContextMenu.coord.y + row * cellH;
            menuItem = {{x, y}, {cellW, cellH}};

            if (within_rectangle(mouseCoord, menuItem)) {
                set_rgb_colour(RGB_BLACK);
                render_line(mainArea, {menuItem.coord.x, menuItem.coord.y}, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y}, 1);
                render_line(mainArea, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y}, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y + menuItem.size.h}, 1);
                render_line(mainArea, {menuItem.coord.x, menuItem.coord.y}, {menuItem.coord.x, menuItem.coord.y + menuItem.size.h}, 1);
                render_line(mainArea, {menuItem.coord.x, menuItem.coord.y + menuItem.size.h}, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y + menuItem.size.h}, 1);
                set_rgb_colour(RGB_WHITE);
                render_line(mainArea, {menuItem.coord.x + 1, menuItem.coord.y + 1}, {(menuItem.coord.x + menuItem.size.w) - 1, menuItem.coord.y + 1}, 1);
                render_line(mainArea, {(menuItem.coord.x + menuItem.size.w - 1), menuItem.coord.y + 1}, {(menuItem.coord.x + menuItem.size.w) - 1, (menuItem.coord.y + menuItem.size.h) - 1}, 1);
                render_line(mainArea, {menuItem.coord.x + 1, menuItem.coord.y + 1}, {menuItem.coord.x + 1, (menuItem.coord.y + menuItem.size.h) - 1}, 1);
                render_line(mainArea, {menuItem.coord.x + 1, (menuItem.coord.y + menuItem.size.h) - 1}, {(menuItem.coord.x + menuItem.size.w) - 1, (menuItem.coord.y + menuItem.size.h) - 1}, 1);
            }
        }
    }
}

void render_scrollbars(GLFWwindow * window) {
    double renderWidth  = get_render_width() / gGlobalGuiScale;
    double renderHeight = get_render_height() / gGlobalGuiScale;

    // Scrollbar background
    set_rgb_colour(RGB_GREY_7);
    render_rectangle(mainArea, {{0.0, renderHeight - SCROLLBAR_WIDTH}, {renderWidth - SCROLLBAR_MARGIN, SCROLLBAR_WIDTH}});
    render_rectangle(mainArea, {{renderWidth - SCROLLBAR_WIDTH, 0.0}, {SCROLLBAR_WIDTH, renderHeight - SCROLLBAR_MARGIN}});

    // Bottom right box
    set_rgb_colour(RGB_BACKGROUND_GREY);
    render_rectangle(mainArea, {{renderWidth - SCROLLBAR_WIDTH, renderHeight - SCROLLBAR_WIDTH}, {SCROLLBAR_WIDTH, SCROLLBAR_WIDTH}});

    // Scroll indicator blocks
    set_rgb_colour(RGB_GREY_9);
    gScrollState.yThumb = render_rectangle(mainArea, {{renderWidth - SCROLLBAR_WIDTH, gScrollState.yBar - (SCROLLBAR_LENGTH / 2.0)}, {SCROLLBAR_WIDTH, SCROLLBAR_LENGTH}});
    gScrollState.xThumb = render_rectangle(mainArea, {{gScrollState.xBar - (SCROLLBAR_LENGTH / 2.0), renderHeight - SCROLLBAR_WIDTH}, {SCROLLBAR_LENGTH, SCROLLBAR_WIDTH}});
}

void render_top_bar(void) {
    tRectangle  rectangle                           = {0};
    char        patchNameCopy[CLAVIA_NAME_SIZE + 1] = {0};
    char        buff[32]                            = {0};
    tCommsState commsState                          = gCommsState;
    char *      commsStateText                      = "Unknown";
    tRgb        commsStateColour                    = RGB_RED_7;
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
    double      indicatorX                          = 0.0;
    double      indicatorW                          = 0.0;

    set_rgb_colour(RGB_GREY_5);
    render_rectangle_with_border(mainArea, {{0.0, 0.0}, {(get_render_width() / gGlobalGuiScale) - SCROLLBAR_MARGIN, TOP_BAR_HEIGHT}});

    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{400, 43}, {NULL, STANDARD_TEXT_HEIGHT}}, "Variation");

    COPY_STRING(patchNameCopy, gGlobalSettings.slot[slot].patchName);

    //patch_name_get(slot, patchNameCopy, sizeof(patchNameCopy));

    if (patchNameCopy[0] == '\0') {
        COPY_STRING(patchNameCopy, "---");
    }
    //set_rgb_colour(RGB_BLACK);
    //render_text(mainArea, {{80, 43}, {NULL, STANDARD_TEXT_HEIGHT}}, "Patch Name");

    if (gPatchNameEdit.active && gPatchNameEdit.slot == slot) {
        // Show edit buffer with cursor
        char displayBuf[CLAVIA_NAME_SIZE + 2] = {0};
        snprintf(displayBuf, sizeof(displayBuf), "%s|", gPatchNameEdit.buffer);

        gTopbarControls[topbarPatchNameId].rectangle = draw_button(mainArea, {topbar_control_def(topbarPatchNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, displayBuf, (tRgb)RGB_WHITE);
    } else {
        gTopbarControls[topbarPatchNameId].rectangle = draw_button(mainArea, {topbar_control_def(topbarPatchNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, patchNameCopy, (tRgb)RGB_BACKGROUND_GREY);
    }
    gTopbarControls[topbarPatchTypeId].rectangle = draw_button(mainArea, {topbar_control_def(topbarPatchTypeId)->coord, {get_text_width("Sequencer", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)patchTypeStrMap[gPatchDescr[slot].category], (tRgb)RGB_BACKGROUND_GREY);
    gTopbarControls[topbarMonoPolyId].rectangle  = draw_button(mainArea, {topbar_control_def(topbarMonoPolyId)->coord, {get_text_width("Legato", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)monoPolyStrMap[gPatchDescr[slot].monoPoly], (tRgb)RGB_BACKGROUND_GREY);
    {
        const tTopbarControlDef * def         = topbar_control_def(topbarPatchNotesId);
        tRgb                      notesColour = (strlen((char *)gPatchNotes[slot]) > 0) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
        gTopbarControls[topbarPatchNotesId].rectangle = draw_button(mainArea, {def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, def->text, notesColour);
    }
    {
        const tTopbarControlDef * def = topbar_control_def(topbarSettingsId);
        gTopbarControls[topbarSettingsId].rectangle = draw_button(mainArea, {def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, def->text, (tRgb)RGB_BACKGROUND_GREY);
    }
    {
        const tTopbarControlDef * def = topbar_control_def(topbarPerfSettingsId);
        gTopbarControls[topbarPerfSettingsId].rectangle = draw_button(mainArea, {def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, def->text, (tRgb)RGB_BACKGROUND_GREY);
    }
    {
        const tTopbarControlDef * def = topbar_control_def(topbarPatchSettingsId);
        gTopbarControls[topbarPatchSettingsId].rectangle = draw_button(mainArea, {def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, def->text, (tRgb)RGB_BACKGROUND_GREY);
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
    gTopbarControls[topbarVoiceCountId].rectangle = draw_button(mainArea, {topbar_control_def(topbarVoiceCountId)->coord, {get_text_width("XX", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, buff, buttonBackgroundColour);

    {
        tModuleKey volKey = {slot, (uint32_t)locationMorph, PATCH_VOLUME};
        tModule *  module = get_module(volKey);

        if (module != NULL) {
            snprintf(buff, sizeof(buff), "%s", patchVolumeStrMap[module->param[variation][VOLUME_LEVEL].value]);
            render_text(mainArea, {{gTopbarControls[topbarPatchVolumeId].rectangle.coord.x, gTopbarControls[topbarPatchVolumeId].rectangle.coord.y - 12}, {NULL, STANDARD_TEXT_HEIGHT}}, buff);
            gParamRectangle[module->key.slot][module->key.location][module->key.index][VOLUME_LEVEL] = render_dial(mainArea, gTopbarControls[topbarPatchVolumeId].rectangle, module->param[variation][VOLUME_LEVEL].value, 127, 0, RGB_GREY_7);
        }
    }

    for (int i = 0; i < TOPBAR_STANDARD_BUTTON_COUNT; i++) {
        const tTopbarControlDef * def = topbar_control_def((tTopbarControlId)i);

        rectangle                    = {def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}};

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

    commsStateColour = RGB_BACKGROUND_GREY;

    switch (commsState) {
        case eCommsOnLine:
            commsStateText   = "Online";
            commsStateColour = RGB_GREEN_7;
            break;
        default:
            commsStateText   = "Offline";
            break;
    }
    set_rgb_colour(commsStateColour);
    rectangle        = {{790, 8}, {get_text_width("Offline", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}};
    commsStateRect   = draw_button(mainArea, rectangle, commsStateText, commsStateColour);
    draw_button(mainArea, {{790, 25}, {get_text_width("Tx", (double)STANDARD_BUTTON_TEXT_HEIGHT * 0.6, eCache), (double)STANDARD_BUTTON_TEXT_HEIGHT * 0.6}}, "Tx", txActive ? (tRgb)RGB_GREEN_7 : (tRgb)RGB_BACKGROUND_GREY);
    draw_button(mainArea, {{803, 25}, {get_text_width("Tx", (double)STANDARD_BUTTON_TEXT_HEIGHT * 0.6, eCache), (double)STANDARD_BUTTON_TEXT_HEIGHT * 0.6}}, "Rx", rxActive ? (tRgb)RGB_GREEN_7 : (tRgb)RGB_BACKGROUND_GREY);

    if (txActive || rxActive) {
        wake_glfw();
    }
    // Cable colour visibility toggles — 6 small squares
    //uint32_t hiddenMask = gHiddenCableMask;

    for (int i = 0; i < cableColourMax; i++) {
        bool                      visible  = gPatchDescr[slot].visible[i];
        tRgb                      colour   = gCableColourMap[i];
        double                    x        = 700.0 + (i * (get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT, eCache) + 5));
        tTopbarControlId          toggleId = (tTopbarControlId)((int)topbarCableColourToggle0Id + i);
        const tTopbarControlDef * def      = topbar_control_def(toggleId);

        if (visible) {
            gTopbarControls[toggleId].rectangle = draw_button(mainArea, {{x, def->coord.y}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, "X", colour);
        } else {
            gTopbarControls[toggleId].rectangle = draw_button(mainArea, {{x, def->coord.y}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, " ", colour);
        }
    }

    bool hideAll = gCablesHideAll;
    bool transp  = gCablesTransparent;

    {
        const tTopbarControlDef * def = topbar_control_def(topbarHideAllCablesId);
        gTopbarControls[topbarHideAllCablesId].rectangle = draw_button(mainArea,
                                                                       {def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                       def->text, hideAll ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }
    {
        const tTopbarControlDef * def = topbar_control_def(topbarTransparentCablesId);
        gTopbarControls[topbarTransparentCablesId].rectangle = draw_button(mainArea,
                                                                           {def->coord, {get_text_width(def->text, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                           def->text, transp ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }

    snprintf(buff, sizeof(buff), "%u BPM", gGlobalSettings.masterClock);
    gTopbarControls[topbarTempoDialId].rectangle    = render_dial_with_text(mainArea, {topbar_control_def(topbarTempoDialId)->coord, {20, 48}}, NULL, buff, STANDARD_BUTTON_TEXT_HEIGHT, gGlobalSettings.masterClock, 241, 0, (tRgb)RGB_BACKGROUND_GREY);
    gTopbarControls[topbarClockRunStopId].rectangle = draw_button(mainArea, {topbar_control_def(topbarClockRunStopId)->coord, {get_text_width("Stopped", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)(clockRunning ? "Running" : "Stopped"), clockRunning ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

    if (gGlobalSettings.perfMode == 1) {
        snprintf(buff, sizeof(buff), "Perf Mode");
    } else {
        snprintf(buff, sizeof(buff), "Patch Mode");
    }
    gTopbarControls[topbarPerfModeId].rectangle     = draw_button(mainArea,
                                                                  {{20, 42}, {get_text_width("Patch Mode", STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                  buff, (tRgb)RGB_BACKGROUND_GREY);

    if (gGlobalSettings.perfMode == 1) {
        char perfNameDisplay[CLAVIA_NAME_SIZE + 2] = {0};

        if (gPerfNameEdit.active) {
            snprintf(perfNameDisplay, sizeof(perfNameDisplay), "%s|", gPerfNameEdit.buffer);
            gTopbarControls[topbarPerfNameId].rectangle = draw_button(mainArea,
                                                                      {topbar_control_def(topbarPerfNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                      perfNameDisplay, (tRgb)RGB_WHITE);
        } else {
            snprintf(perfNameDisplay, sizeof(perfNameDisplay), "%s", gGlobalSettings.perfName[0] ? gGlobalSettings.perfName : "---");
            gTopbarControls[topbarPerfNameId].rectangle = draw_button(mainArea,
                                                                      {topbar_control_def(topbarPerfNameId)->coord, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache), STANDARD_BUTTON_TEXT_HEIGHT}},
                                                                      perfNameDisplay, (tRgb)RGB_BACKGROUND_GREY);
        }
    }
    {
        double resLabelH  = STANDARD_TEXT_HEIGHT * 0.7;
        double col1X      = 600.0;
        double col2X      = 644.0;
        double row1Y      = 44.0;
        double row2Y      = 60.0;
        double valW       = get_text_width("XX.X%", STANDARD_BUTTON_TEXT_HEIGHT, eCache);
        double labelX     = 581.0;
        double headerY    = row1Y - resLabelH - 2.0;
        double rowLabelOY = (STANDARD_TEXT_HEIGHT - resLabelH) / 2.0;

        render_text(mainArea, {{col1X, headerY}, {BLANK_SIZE, resLabelH}}, "Cycles");
        render_text(mainArea, {{col2X, headerY}, {BLANK_SIZE, resLabelH}}, "Memory");
        render_text(mainArea, {{labelX, row1Y + rowLabelOY}, {BLANK_SIZE, resLabelH}}, "VA");
        render_text(mainArea, {{labelX, row2Y + rowLabelOY}, {BLANK_SIZE, resLabelH}}, "FX");

        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].cycles[locationVa]);
        draw_button(mainArea, {{col1X, row1Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].cycles[locationFx]);
        draw_button(mainArea, {{col1X, row2Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].mem[locationVa]);
        draw_button(mainArea, {{col2X, row1Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
        snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].mem[locationFx]);
        draw_button(mainArea, {{col2X, row2Y}, {valW, STANDARD_BUTTON_TEXT_HEIGHT}}, buff, (tRgb)RGB_BACKGROUND_GREY);
    }
}

void wake_glfw(void) {
    gReDraw = true;

    // Safe GLFW call from any thread
    glfwPostEmptyEvent();
}

void notify_full_patch_change(void) {
    gLocation                          = locationVa;
    gTopbarControls[topbarVaId].colour = (tRgb)RGB_GREEN_ON;
    gTopbarControls[topbarFxId].colour = (tRgb)RGB_BACKGROUND_GREY;
    // Set scrollbars back to top/left
    gScrollState.xBar                  = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_x_scroll_bar(gScrollState.xBar);
    gScrollState.yBar                  = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_y_scroll_bar(gScrollState.yBar);
}

void setup_render_context(void) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, get_render_width(), get_render_height(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

void init_graphics(void) {
    int           fbWidth           = 0;
    int           fbHeight          = 0;
    int           windowWidth       = 0;
    int           windowHeight      = 0;
    GLFWmonitor * monitor           = NULL;
    float         xScale            = 1;
    float         yScale            = 1;
    char          title[128]        = {0};

    snprintf(title, sizeof(title), "%s - Build %s %s", WINDOW_TITLE, __DATE__, __TIME__);

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    register_glfw_wake_cb(wake_glfw);
    register_full_patch_change_notify_cb(notify_full_patch_change);
    topbar_init_controls();

    monitor      = glfwGetPrimaryMonitor();

    int           x, y, width, height;
    double        widthScaleFactor  = 1;
    double        heightScaleFactor = 1;
    double        scaleFactor       = 1;
    glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
    glfwGetMonitorContentScale(monitor, &xScale, &yScale);
    windowWidth  = TARGET_FRAME_BUFF_WIDTH / xScale;
    windowHeight = TARGET_FRAME_BUFF_HEIGHT / yScale;

    if ((windowWidth > width) || (windowHeight > height)) {
        heightScaleFactor = (double)windowHeight / (double)height;
        widthScaleFactor  = (double)windowWidth / (double)width;

        if (heightScaleFactor >= widthScaleFactor) {
            scaleFactor = heightScaleFactor;
        } else {
            scaleFactor = widthScaleFactor;
        }
        windowWidth       = (int)((double)windowWidth / scaleFactor);
        windowHeight      = (int)((double)windowHeight / scaleFactor);
        gGlobalGuiScale   = gGlobalGuiScale / scaleFactor;
    }
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);
    gWindow      = glfwCreateWindow(windowWidth, windowHeight, title, NULL, NULL);

    if (!gWindow) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetWindowSizeLimits(gWindow, windowWidth / gGlobalGuiScale, windowHeight / gGlobalGuiScale, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowAspectRatio(gWindow, windowWidth, windowHeight);

    glfwMakeContextCurrent(gWindow);

    glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);
    glfwSwapInterval(1);
    glfwSetWindowCloseCallback(gWindow, window_close_callback);
    glfwSetKeyCallback(gWindow, key_callback);
    glfwSetCharCallback(gWindow, char_event);
    glfwSetCursorPosCallback(gWindow, cursor_pos);
    glfwSetMouseButtonCallback(gWindow, mouse_button);
    glfwSetScrollCallback(gWindow, scroll_event);

    FT_Init_FreeType(&gLibrary);
    FT_New_Face(gLibrary, "/System/Library/Fonts/Supplemental/Arial.ttf", 0, &gFace);
    FT_Set_Char_Size(gFace, 0, 48 * 64, 300, 300);

    // Preload glyph textures
    if (!preload_glyph_textures("/System/Library/Fonts/Supplemental/Arial.ttf", 72.0f)) {
        LOG_ERROR("Failed to preload glyph textures\n");
    }
    int renderWidth  = 0;
    int renderHeight = 0;
    //glfwGetWindowSize(gWindow, &renderWidth, &renderHeight);
    glfwGetFramebufferSize(gWindow, &renderWidth, &renderHeight);
    set_render_width(renderWidth);
    set_render_height(renderHeight);

    setup_render_context();
}

void set_patch_name_from_filename(uint32_t slot, const char * filepath) {
    const char * base                            = filepath;
    const char * p                               = filepath;
    char         patchName[CLAVIA_NAME_SIZE + 1] = {0};
    int          i                               = 0;

    // Find last path separator
    while (*p != '\0') {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
        p++;
    }
    // Copy up to PATCH_NAME_SIZE chars, stop at '.' (extension)
    memset(patchName, 0, sizeof(patchName));

    while (i < CLAVIA_NAME_SIZE && base[i] != '\0' && base[i] != '.') {
        patchName[i] = base[i];
        i++;
    }
    COPY_STRING(gGlobalSettings.slot[slot].patchName, patchName);

    LOG_DEBUG("Patch name from file: '%s'\n", patchName);
}

void clear_slot_data(uint32_t slot) {
    if (slot < MAX_SLOTS) {
        database_delete_cables_by_slot(slot);
        database_delete_modules_by_slot(slot);
        gMorphCount[slot]      = 0;
        gNote2Size[slot]       = 0;
        gControllerCount[slot] = 0;
        gPatchNotesSize[slot]  = 0;
        memset(&(gPatchDescr[slot]), 0, sizeof(gPatchDescr[0]));
        memset(&(gKnobArray[slot]), 0, sizeof(gKnobArray[0]));
        memset(gNote2[slot], 0, sizeof(gNote2[0]));
        memset(&(gControllerArray[slot]), 0, sizeof(gControllerArray[0]));
        memset(gPatchNotes[slot], 0, sizeof(gPatchNotes[0]));
    }
}

void read_file_into_memory_and_process(const char * filepath) {
    int64_t   byteOffset = 0;
    int64_t   fileSize   = 0;
    FILE *    file       = NULL;
    uint8_t * buff       = NULL;
    size_t    readSize   = 0;
    uint8_t   version    = 23;
    uint8_t   type       = 0;
    uint32_t  readCrc    = 0;
    uint32_t  calcCrc    = 0;
    uint32_t  slot       = gSlot;

    file     = fopen(filepath, "rb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    clearerr(file);

    buff     = (uint8_t *)malloc(fileSize);

    if (buff == NULL) {
        LOG_ERROR("Memory allocation failed\n");
        fclose(file);
        return;
    }
    readSize = fread(buff, 1, fileSize, file);

    if (readSize != fileSize) {
        LOG_ERROR("Failed to read entire file\n");
        free(buff);
        fclose(file);
        return;
    }

    for (int64_t i = 0; i < fileSize; i++) {
        if (buff[i] == 0x00) {
            byteOffset = i + 1;
            break;
        }
    }

    readCrc  = buff[fileSize - 2] << 8 | buff[fileSize - 1];
    calcCrc  = calc_crc16(buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));

    if (readCrc == calcCrc) {
        version = buff[byteOffset++];
        type    = buff[byteOffset++];
        LOG_DEBUG("Version %u\n", version);
        LOG_DEBUG("Type %u\n", type);

        if (type == 0) {
            clear_slot_data(slot);
            parse_patch(slot, buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));  // TODO: parse_patch should really be in a commonly accessible source file, for file or USB access
            set_patch_name_from_filename(slot, filepath);

            if (gCommsState == eCommsOnLine) {
                tMessageContent msg = {0};
                msg.cmd  = eMsgCmdWritePatch;
                msg.slot = slot;
                msg_send(&gCommandQueue, &msg);
            }
        } else if (type == 1) {
            if (gGlobalSettings.perfMode == 0) {
                tMessageContent msg = {0};
                msg.cmd                  = eMsgCmdWriteModePerf; // Really need to be in perf mode before loading a performance
                msg_send(&gCommandQueue, &msg);
                gGlobalSettings.perfMode = 1;                    // TODO - Ideally, we'd get an indication back or request state until perf mode = 1, so we could remove the big sleep below
                usleep(2000000);                                 // TODO - currently, when we write new patches as part of a perf, it triggers a read of the patches. We need to be stable in perf mode in that case.
            }
            int i = 0;

            for (i = 0; i < MAX_SLOTS; i++) {
                clear_slot_data(i);
            }

            // Performance file — parse_perf clears all 4 slots and populates them;
            // slot names come from the file itself so set_patch_name_from_filename is not called.
            // Derive performance name from the filename (strip directory and .prf2 extension).
            {
                const char * slash    = strrchr(filepath, '/');
                const char * baseName = slash ? slash + 1 : filepath;
                COPY_STRING(gGlobalSettings.perfName, baseName);
                char *       dot      = strrchr(gGlobalSettings.perfName, '.');

                if (dot) {
                    *dot = '\0';
                }
            }

            gGlobalSettings.perfMode = 1;
            parse_perf(buff + byteOffset, (int)((fileSize - byteOffset) - 2));

            if (gCommsState == eCommsOnLine) {
                tMessageContent msg = {0};
                msg.cmd = eMsgCmdWritePerf;
                msg_send(&gCommandQueue, &msg);
            }
        }
    } else {
        LOG_WARNING("CRC check failed\n");
    }
    free(buff);
    fclose(file);
}

void write_database_to_file(const char * filepath) {
    FILE *    file           = NULL;
    //uint8_t ch          = 0;
    size_t    writtenSize    = 0;
    char      charBuff[1024] = {0};
    char      eol[]          = {0x0d, 0x0a, 0x00};
    uint8_t * buff           = NULL;
    uint32_t  bitPos         = 0;
    uint32_t  calcCrc        = 0;
    uint32_t  slot           = gSlot;

    file = fopen(filepath, "wb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return;
    }
    // Couldn't really find a nice way to construct the write buffer, so allocating on the heap
    buff = (uint8_t *)malloc(PATCH_FILE_SIZE);

    if (buff == NULL) {
        LOG_ERROR("Failed to allocate buffer\n");
        fclose(file);
        return;
    }
    memset(buff, 0, PATCH_FILE_SIZE);

    // Header text, which seems to be constant across latest patch files
    snprintf(charBuff, sizeof(charBuff) - 1, "Version=Nord Modular G2 File Format 1");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    snprintf(charBuff, sizeof(charBuff) - 1, "Type=Patch");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    snprintf(charBuff, sizeof(charBuff) - 1, "Version=23");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    snprintf(charBuff, sizeof(charBuff) - 1, "Info=BUILD 320");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    charBuff[0] = '\0';
    fwrite(charBuff, 1, 1, file);

    write_bit_stream(buff, &bitPos, 8, 23); // Version
    write_bit_stream(buff, &bitPos, 8, 0);  // Type (0 = patch, 1 = performance when we get round to implementing that)

    write_patch_descr(slot, buff, &bitPos);
    write_module_list(slot, locationVa, buff, &bitPos);
    write_module_list(slot, locationFx, buff, &bitPos);
    write_current_note_2(slot, buff, &bitPos);
    write_cable_list(slot, locationVa, buff, &bitPos);
    write_cable_list(slot, locationFx, buff, &bitPos);
    write_param_list(slot, locationMorph, buff, &bitPos, NUM_VARIATIONS);
    write_param_list(slot, locationVa, buff, &bitPos, NUM_VARIATIONS);
    write_param_list(slot, locationFx, buff, &bitPos, NUM_VARIATIONS);
    write_morph_params(slot, buff, &bitPos, NUM_VARIATIONS);
    write_knobs(slot, buff, &bitPos);
    write_controllers(slot, buff, &bitPos);
    //write_param_names(slot, locationMorph, buff, &bitPos);
    write_param_names(slot, locationVa, buff, &bitPos);
    write_param_names(slot, locationFx, buff, &bitPos);
    write_module_names(slot, locationVa, buff, &bitPos);
    write_module_names(slot, locationFx, buff, &bitPos);
    write_patch_notes(slot, buff, &bitPos);

    bitPos      = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(bitPos)); // Final byte alignment round-up

    calcCrc     = calc_crc16(buff, BIT_TO_BYTE_ROUND_UP(bitPos));

    write_bit_stream(buff, &bitPos, 16, calcCrc);

    writtenSize = fwrite(buff, 1, BIT_TO_BYTE_ROUND_UP(bitPos), file);

    if (writtenSize != BIT_TO_BYTE_ROUND_UP(bitPos)) {
        LOG_ERROR("Written %zu of %u\n", writtenSize, BIT_TO_BYTE_ROUND_UP(bitPos));
    }

    if (BIT_TO_BYTE_ROUND_UP(bitPos) > ((PATCH_FILE_SIZE * 3) / 4)) {
        LOG_ERROR("Write file size > 3/4 of %d, might need to increase PATCH_FILE_SIZE\n", PATCH_FILE_SIZE);
    }
    free(buff);
    fclose(file);
}

void write_perf_to_file(const char * filepath) {
    FILE *    file        = NULL;
    size_t    writtenSize = 0;
    char      eol[]       = {0x0d, 0x0a, 0x00};
    char      nullByte    = '\0';
    uint8_t * buff        = NULL;
    uint32_t  bitPos      = 0;
    uint32_t  calcCrc     = 0;

    file = fopen(filepath, "wb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return;
    }
    buff = (uint8_t *)malloc(PERF_FILE_SIZE);

    if (buff == NULL) {
        LOG_ERROR("Memory allocation failed\n");
        fclose(file);
        return;
    }
    memset(buff, 0, PERF_FILE_SIZE);

    fwrite("Version=Nord Modular G2 File Format 1", 1, 37, file);
    fwrite(eol, 1, 2, file);
    fwrite("Type=Performance", 1, 16, file);
    fwrite(eol, 1, 2, file);
    fwrite("Version=23", 1, 10, file);
    fwrite(eol, 1, 2, file);
    fwrite("Info=BUILD 320", 1, 14, file);
    fwrite(eol, 1, 2, file);
    fwrite(&nullByte, 1, 1, file);

    write_bit_stream(buff, &bitPos, 8, 23); // version
    write_bit_stream(buff, &bitPos, 8, 1);  // type = performance

    write_perf_header(buff, &bitPos);

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        uint32_t savedU1 = gPatchDescr[slot].unknown1;
        uint32_t savedU2 = gPatchDescr[slot].unknown2;
        gPatchDescr[slot].unknown1 = 0;
        gPatchDescr[slot].unknown2 = 0;
        write_patch_descr(slot, buff, &bitPos);
        gPatchDescr[slot].unknown1 = savedU1;
        gPatchDescr[slot].unknown2 = savedU2;
        write_module_list(slot, locationVa, buff, &bitPos);
        write_module_list(slot, locationFx, buff, &bitPos);
        write_current_note_2_perf(slot, buff, &bitPos);
        write_cable_list(slot, locationVa, buff, &bitPos);
        write_cable_list(slot, locationFx, buff, &bitPos);
        write_param_list(slot, locationMorph, buff, &bitPos, NUM_VARIATIONS);
        write_param_list(slot, locationVa, buff, &bitPos, NUM_VARIATIONS);
        write_param_list(slot, locationFx, buff, &bitPos, NUM_VARIATIONS);
        write_morph_params(slot, buff, &bitPos, NUM_VARIATIONS);
        write_knobs(slot, buff, &bitPos);
        write_controllers(slot, buff, &bitPos);
        write_param_names(slot, locationMorph, buff, &bitPos);
        write_param_names(slot, locationVa, buff, &bitPos);
        write_param_names(slot, locationFx, buff, &bitPos);
        write_module_names(slot, locationVa, buff, &bitPos);
        write_module_names(slot, locationFx, buff, &bitPos);
        write_slot_separator(buff, &bitPos); // 0x6f — same as PATCH_NOTES type, not written in perf
    }

    write_global_knobs(buff, &bitPos);

    bitPos      = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(bitPos));
    calcCrc     = calc_crc16(buff, BIT_TO_BYTE_ROUND_UP(bitPos));
    write_bit_stream(buff, &bitPos, 16, calcCrc);

    writtenSize = fwrite(buff, 1, BIT_TO_BYTE_ROUND_UP(bitPos), file);

    if (writtenSize != BIT_TO_BYTE_ROUND_UP(bitPos)) {
        LOG_ERROR("Written %zu of %u\n", writtenSize, BIT_TO_BYTE_ROUND_UP(bitPos));
    }

    if (BIT_TO_BYTE_ROUND_UP(bitPos) > ((PERF_FILE_SIZE * 3) / 4)) {
        LOG_ERROR("Write file size > 3/4 of %d, might need to increase PERF_FILE_SIZE\n", PERF_FILE_SIZE);
    }
    free(buff);
    fclose(file);
}

static void on_file_opened(const char * path) {
    if (path) {
        LOG_INFO("Selected file: %s", path);
        read_file_into_memory_and_process(path);
        //set_window_title(path);
    }
    gNeedFocus = true;
    wake_glfw();
}

static void on_file_saved(const char * path) {
    uint32_t slot = gSlot;

    if (path) {
        LOG_INFO("Saving file: %s", path);

        if (gGlobalSettings.perfMode == 1) {
            write_perf_to_file(path);
        } else {
            write_database_to_file(path);
            set_patch_name_from_filename(slot, path);
        }
    }
    gNeedFocus = true;
    wake_glfw();
}

static void check_action_flags(void) {
    uint32_t slot                              = gSlot;
    //bool     perfMode                          = gPerfMode != 0;
    char     patchName[CLAVIA_NAME_SIZE + 1]   = {0};
    char     defaultName[CLAVIA_NAME_SIZE + 6] = {0}; // name (16) + extension (5) + null

    if (gShowOpenFileReadDialogue) {
        gShowOpenFileReadDialogue = false;
        open_file_read_dialogue_async(on_file_opened);
    }

    if (gShowOpenFileWriteDialogue) {
        gShowOpenFileWriteDialogue = false;

        if (gGlobalSettings.perfMode == 1) {
            if (gGlobalSettings.perfName[0] != '\0') {
                snprintf(defaultName, sizeof(defaultName), "%s.prf2", gGlobalSettings.perfName);
            } else {
                COPY_STRING(defaultName, "performance.prf2");
            }
        } else {
            COPY_STRING(patchName, gGlobalSettings.slot[slot].patchName);

            if (patchName[0] != '\0') {
                snprintf(defaultName, sizeof(defaultName), "%s.pch2", patchName);
            } else {
                snprintf(defaultName, sizeof(defaultName), "patch.pch2");
            }
        }
        open_file_write_dialogue_async(on_file_saved, defaultName);
    }

    if (gNeedFocus == true) {
        gNeedFocus = false;
        glfwFocusWindow(gWindow);
    }
}

// Helper: draw a fixed-width dropdown trigger button, return updated x.
static double render_dropdown(double x, double y, double btnH,
                              const char * valStr, const char * widestVal,
                              tRectangle * rect) {
    *rect = draw_button(mainArea, {{x, y}, {get_text_width((char *)widestVal, btnH, eCache) + 8.0, btnH}},
                        (char *)valStr, (tRgb)RGB_BACKGROUND_GREY);
    return x + rect->size.w;
}

// Helper: MIDI note number → note name string (e.g. 0 → "C-1", 60 → "C4")
static void midi_note_name_str(uint8_t note, char * buf, size_t bufLen) {
    static const char * names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int                 octave  = (int)(note / 12) - 1;

    snprintf(buf, bufLen, "%s%d", names[note % 12], octave);
}

static void render_ss_section(double boxX, double boxW, double margin, double * y,
                              double rowH, double btnH,
                              const tSynthSettingItem * items, int count) {
    double itemW = (boxW - margin * 2.0) / 2.0;
    double x     = 0.0;
    int    i     = 0;

    for (i = 0; i < count; i++) {
        x              = boxX + margin + (i % 2) * itemW;

        if (i > 0 && (i % 2) == 0) {
            *y += rowH;
        }
        set_rgb_colour(RGB_BLACK);
        render_text(mainArea, {{x, *y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x             += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        *items[i].rect = draw_button(mainArea,
                                     {{x, *y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
                                     items[i].get_str(),
                                     items[i].get_colour());
    }

    *y += rowH;
}

static double render_pp_row(double x, double y, double btnH,
                            const tPatchParamItem * items, int count) {
    int i = 0;

    for (i = 0; i < count; i++) {
        set_rgb_colour(RGB_BLACK);
        render_text(mainArea, {{x, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x                                += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        gPatchParamRects[items[i].rectId] = draw_button(mainArea,
                                                        {{x, y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
                                                        items[i].get_str(),
                                                        items[i].get_colour());
        x                                += get_text_width((char *)items[i].widest, btnH, eCache) + 8.0 + 16.0;
    }

    return x;
}

static void midi_chan_str(uint8_t val, char * buf, size_t bufLen) {
    if (val >= 0x10) {
        snprintf(buf, bufLen, "Off");
    } else {
        snprintf(buf, bufLen, "%u", (unsigned)val + 1u);
    }
}

static void render_patch_settings_panel(void) {
    static const char * slotLabel[4] = {"A", "B", "C", "D"};
    double              renderW      = 0.0;
    double              renderH      = 0.0;
    double              boxW         = 600.0;
    double              boxH         = 453.0;
    double              boxX         = 0.0;
    double              boxY         = 0.0;
    double              margin       = 10.0;
    double              titleH       = 24.0;
    double              rowH         = 26.0;
    double              secH         = 18.0;
    double              btnH         = STANDARD_BUTTON_TEXT_HEIGHT;
    double              y            = 0.0;
    double              colW         = 0.0;
    double              x            = 0.0;
    int                 i            = 0;
    char                buf[16]      = {0};

    if (!gPatchSettingsEdit.active) {
        return;
    }
    renderW                   = get_render_width() / gGlobalGuiScale;
    renderH                   = get_render_height() / gGlobalGuiScale;
    boxX                      = (renderW - boxW) / 2.0;
    boxY                      = (renderH - boxH) / 2.0;
    y                         = boxY + titleH + margin;

    set_rgb_colour(RGB_GREY_2);
    render_rectangle(mainArea, {{0.0, 0.0}, {renderW, renderH}});

    set_rgb_colour(RGB_GREY_5);
    render_rectangle_with_border(mainArea, {{boxX, boxY}, {boxW, boxH}});

    set_rgb_colour(RGB_GREY_3);
    render_rectangle(mainArea, {{boxX, boxY}, {boxW, titleH}});
    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{boxX + margin, boxY + 6.0}, {BLANK_SIZE, btnH}}, "Synth Settings");

    gSettingsPanelRects.close = draw_button(mainArea,
                                            {{boxX + boxW - 44.0, boxY + 4.0}, {get_text_width((char *)"Close", btnH, eCache) + 4.0, btnH}},
                                            "Close", (tRgb)RGB_BACKGROUND_GREY);

    // ── Synth Name ─────────────────────────────────────────────────
    {
        char displayBuf[CLAVIA_NAME_SIZE + 2] = {0};

        x  = boxX + margin;
        set_rgb_colour(RGB_BLACK);
        render_text(mainArea, {{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;

        if (gSynthNameEdit.active) {
            snprintf(displayBuf, sizeof(displayBuf), "%s|", gSynthNameEdit.buffer);
            gSettingsPanelRects.synthName = draw_button(mainArea, {{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_WHITE);
        } else {
            snprintf(displayBuf, sizeof(displayBuf), "%s", gSynthSettings.name);
            gSettingsPanelRects.synthName = draw_button(mainArea, {{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_BACKGROUND_GREY);
        }
    }
    y   += rowH;

    // ── MIDI Channels ──────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "MIDI Channels");
    y   += secH;
    colW = (boxW - margin * 2.0) / 4.0;

    for (i = 0; i < 4; i++) {
        x  = boxX + margin + i * colW;
        snprintf(buf, sizeof(buf), "%c:", slotLabel[i][0]);
        set_rgb_colour(RGB_BLACK);
        render_text(mainArea, {{x, y + 2.0}, {BLANK_SIZE, btnH}}, buf);
        x += get_text_width((char *)"A:", btnH, eCache) + 4.0;
        midi_chan_str(gSynthSettings.midiChanSlot[i], buf, sizeof(buf));
        render_dropdown(x, y, btnH, buf, "Off", &gSettingsPanelRects.midiChan[i]);
    }

    y   += rowH;

    // ── Global + SysEx ─────────────────────────────────────────────
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSGlobal, kSSGlobalCount);

    // ── Options ────────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Options");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSOptions, kSSOptionsCount);

    // ── Tuning ─────────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Tuning");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSTuning, kSSTuningCount);

    // ── Pedal ──────────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Pedal");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSPedal, kSSPedalCount);

    // ── Sort Mode ──────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Sort Mode");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSSort, kSSSortCount);
}

static void render_patch_params_panel(void) {
    if (!gPatchParamsEdit.active) {
        return;
    }
    uint32_t  slot         = gPatchParamsEdit.slot;
    double    renderW      = get_render_width() / gGlobalGuiScale;
    double    renderH      = get_render_height() / gGlobalGuiScale;
    double    boxW         = 680.0;
    double    boxH         = 320.0;
    double    boxX         = (renderW - boxW) / 2.0;
    double    boxY         = (renderH - boxH) / 2.0;
    double    margin       = 10.0;
    double    titleH       = 24.0;
    double    rowH         = 26.0;
    double    secH         = 18.0;
    double    btnH         = STANDARD_BUTTON_TEXT_HEIGHT;
    double    y            = boxY + titleH + margin;
    double    x            = 0.0;
    double    dialH        = 0.0;
    tModule * sustMod      = get_module_slot(slot, (uint32_t)locationMorph, PATCH_SUSTAIN);
    tModule * vibMod       = get_module_slot(slot, (uint32_t)locationMorph, PATCH_VIBRATO);
    uint8_t   sustainPedal = sustMod ? sustMod->param[0][SUSTAIN_PEDAL].value : 0;
    int8_t    octaveShift  = sustMod ? (int8_t)sustMod->param[0][OCTAVE_SHIFT].value : 0;
    uint8_t   vibratoRate  = vibMod ? vibMod->param[0][VIBRATO_RATE].value : 0;
    char      buf[16]      = {0};

    set_rgb_colour(RGB_GREY_2);
    render_rectangle(mainArea, {{0.0, 0.0}, {renderW, renderH}});

    set_rgb_colour(RGB_GREY_5);
    render_rectangle_with_border(mainArea, {{boxX, boxY}, {boxW, boxH}});

    set_rgb_colour(RGB_GREY_3);
    render_rectangle(mainArea, {{boxX, boxY}, {boxW, titleH}});
    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{boxX + margin, boxY + 6.0}, {BLANK_SIZE, btnH}}, "Patch Settings");

    gPatchParamClose = draw_button(mainArea,
                                   {{boxX + boxW - 44.0, boxY + 4.0}, {get_text_width((char *)"Close", btnH, eCache) + 4.0, btnH}},
                                   "Close", (tRgb)RGB_BACKGROUND_GREY);

    // ── Slot buttons in title bar ──────────────────────────────────
    {
        static const char * slotLabels[MAX_SLOTS] = {"A", "B", "C", "D"};
        double              slotBtnW              = get_text_width((char *)"A", btnH, eCache) + 6.0;
        double              slotX                 = boxX + boxW - 44.0 - (slotBtnW + 2.0) * MAX_SLOTS - 8.0;

        for (uint32_t s = 0; s < MAX_SLOTS; s++) {
            tRgb col = (s == slot) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
            gPatchParamSlots[s] = draw_button(mainArea,
                                              {{slotX + s * (slotBtnW + 2.0), boxY + 4.0}, {slotBtnW, btnH}},
                                              slotLabels[s], col);
        }
    }

    // ── Sustain Pedal + Octave Shift ───────────────────────────────
    {
        x                                = boxX + margin;
        set_rgb_colour(RGB_BLACK);
        render_text(mainArea, {{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Sustain Pedal:");
        x                               += get_text_width((char *)"Sustain Pedal:", btnH, eCache) + 4.0;
        gPatchParamRects[pPSustainPedal] = draw_button(mainArea,
                                                       {{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                       sustainPedal ? "On" : "Off",
                                                       sustainPedal ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

        x                                = boxX + boxW / 2.0;
        render_text(mainArea, {{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Octave Shift:");
        x                               += get_text_width((char *)"Octave Shift:", btnH, eCache) + 4.0;
        snprintf(buf, sizeof(buf), "%+d", (int)octaveShift);
        render_dropdown(x, y, btnH, buf, "+2", &gPatchParamRects[pPOctaveShift]);
    }
    y                              += rowH;

    // ── Arpeggiator ────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Arpeggiator");
    y                              += secH;
    render_pp_row(boxX + margin, y, btnH, kPPArp, kPPArpCount);
    y                              += rowH;

    // ── Vibrato ────────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Vibrato");
    y                              += secH;
    x                               = render_pp_row(boxX + margin, y, btnH, kPPVibrato, kPPVibratoCount);
    dialH                           = rowH * 2.0;
    gPatchParamRects[pPVibratoRate] = render_dial_with_text(mainArea, {{x, y + 2.0}, {20.0, dialH}}, "Rate", NULL, btnH, vibratoRate, 127, 0, (tRgb)RGB_BACKGROUND_GREY);
    y                              += rowH;

    // ── Glide ──────────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Glide");
    y                              += secH;
    render_pp_row(boxX + margin, y, btnH, kPPGlide, kPPGlideCount);
    y                              += rowH;

    // ── Bend ───────────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Bend");
    y                              += secH;
    render_pp_row(boxX + margin, y, btnH, kPPBend, kPPBendCount);

    (void)y;
}

static void render_perf_settings_panel(void) {
    if (!gPerfSettingsEdit.active) {
        return;
    }
    double renderW                  = get_render_width() / gGlobalGuiScale;
    double renderH                  = get_render_height() / gGlobalGuiScale;
    double boxW                     = 700.0;
    double boxH                     = 390.0;
    double boxX                     = (renderW - boxW) / 2.0;
    double boxY                     = (renderH - boxH) / 2.0;
    double margin                   = 10.0;
    double titleH                   = 24.0;
    double rowH                     = 26.0;
    double secH                     = 18.0;
    double btnH                     = STANDARD_BUTTON_TEXT_HEIGHT;
    double y                        = boxY + titleH + margin;
    double colX[kPSSlotToggleCount] = {0.0, 0.0, 0.0};
    int    i                        = 0;
    int    col                      = 0;
    char   buf[32]                  = {0};
    char   note[8]                  = {0};
    char   loNote[8]                = {0};
    char   hiNote[8]                = {0};
    char   rangeBuf[18]             = {0};

    set_rgb_colour(RGB_GREY_2);
    render_rectangle(mainArea, {{0.0, 0.0}, {renderW, renderH}});

    set_rgb_colour(RGB_GREY_5);
    render_rectangle_with_border(mainArea, {{boxX, boxY}, {boxW, boxH}});

    set_rgb_colour(RGB_GREY_3);
    render_rectangle(mainArea, {{boxX, boxY}, {boxW, titleH}});
    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{boxX + margin, boxY + 6.0}, {BLANK_SIZE, btnH}}, "Performance Settings");

    gPerfSettingsPanelRects.close = draw_button(mainArea,
                                                {{boxX + boxW - 44.0, boxY + 4.0}, {get_text_width((char *)"Close", btnH, eCache) + 4.0, btnH}},
                                                "Close", (tRgb)RGB_BACKGROUND_GREY);

    // ── Perf Name ──────────────────────────────────────────────────
    {
        char   nameBuf[CLAVIA_NAME_SIZE + 2] = {0};
        double x                             = boxX + margin;
        set_rgb_colour(RGB_BLACK);
        render_text(mainArea, {{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;
        snprintf(nameBuf, sizeof(nameBuf), "%s", gGlobalSettings.perfName);
        draw_button(mainArea, {{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}},
                    nameBuf, (tRgb)RGB_BACKGROUND_GREY);
    }
    y                            += rowH;

    // ── Master Clock ───────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Master Clock");
    y                            += secH;

    {
        double dialH = 48.0;
        double x     = boxX + margin;
        snprintf(buf, sizeof(buf), "%u BPM", (unsigned)gGlobalSettings.masterClock);
        gPerfSettingsPanelRects.masterClock        = render_dial_with_text(mainArea, {{x, y}, {20.0, dialH}}, NULL, buf, STANDARD_BUTTON_TEXT_HEIGHT, gGlobalSettings.masterClock, 241, 0, (tRgb)RGB_BACKGROUND_GREY);
        x                                         += 20.0 + 12.0;
        gPerfSettingsPanelRects.masterClockRunning = draw_button(mainArea,
                                                                 {{x, y + (dialH - btnH) / 2.0}, {get_text_width((char *)"Stopped", btnH, eCache) + 8.0, btnH}},
                                                                 gGlobalSettings.masterClockRunning ? "Running" : "Stopped",
                                                                 gGlobalSettings.masterClockRunning ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        y                                         += dialH + 4.0;
    }

    // ── Slots ──────────────────────────────────────────────────────
    set_rgb_colour(RGB_GREY_7);
    render_text(mainArea, {{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Slots");
    y                            += secH;

    double labelColW = get_text_width((char *)"Slot A:", btnH, eCache) + 8.0;
    double dropW     = get_text_width((char *)"On", btnH, eCache) + 16.0;
    double noteDropW = get_text_width((char *)"C#-1", btnH, eCache) + 10.0;
    double colEn     = boxX + margin + labelColW;
    double colKbd    = colEn + dropW + 8.0;
    double colHld    = colKbd + dropW + 8.0;
    double colLo     = colHld + dropW + 16.0;
    double colHi     = colLo + noteDropW + 8.0;
    double colRng    = colHi + noteDropW + 12.0;

    colX[0]                       = colEn;
    colX[1]                       = colKbd;
    colX[2]                       = colHld;

    // Column headers
    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{colEn, y}, {BLANK_SIZE, btnH}}, "Enable");
    render_text(mainArea, {{colKbd, y}, {BLANK_SIZE, btnH}}, "Keyboard");
    render_text(mainArea, {{colHld, y}, {BLANK_SIZE, btnH}}, "Hold");
    render_text(mainArea, {{colLo, y}, {BLANK_SIZE, btnH}}, "Lower");
    render_text(mainArea, {{colHi, y}, {BLANK_SIZE, btnH}}, "Upper");

    // Keyboard Range global toggle — right side of header
    {
        double x = colRng;
        render_text(mainArea, {{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Kbd Range:");
        x                                    += get_text_width((char *)"Kbd Range:", btnH, eCache) + 4.0;
        gPerfSettingsPanelRects.keyboardRange = draw_button(mainArea,
                                                            {{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                            gPerfSettings.keyboardRange ? "On" : "Off",
                                                            gPerfSettings.keyboardRange ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }
    y                            += rowH;

    // Slot rows A–D
    static const char * slotLabel[] = {"Slot A:", "Slot B:", "Slot C:", "Slot D:"};

    for (i = 0; i < MAX_SLOTS; i++) {
        set_rgb_colour(RGB_BLACK);
        render_text(mainArea, {{boxX + margin, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)slotLabel[i]);

        for (col = 0; col < kPSSlotToggleCount; col++) {
            kPSSlotToggles[col].rects[i] = draw_button(mainArea,
                                                       {{colX[col], y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                       kPSSlotToggles[col].get_str(i),
                                                       kPSSlotToggles[col].get_colour(i));
        }

        midi_note_name_str(gPerfSettings.slot[i].rangeLower, note, sizeof(note));
        render_dropdown(colLo, y, btnH, note, "C#-1", &gPerfSettingsPanelRects.rangeLower[i]);

        midi_note_name_str(gPerfSettings.slot[i].rangeUpper, note, sizeof(note));
        render_dropdown(colHi, y, btnH, note, "C#-1", &gPerfSettingsPanelRects.rangeUpper[i]);

        midi_note_name_str(gPerfSettings.slot[i].rangeLower, loNote, sizeof(loNote));
        midi_note_name_str(gPerfSettings.slot[i].rangeUpper, hiNote, sizeof(hiNote));
        snprintf(rangeBuf, sizeof(rangeBuf), "%s - %s", loNote, hiNote);
        render_text(mainArea, {{colRng, y + 2.0}, {BLANK_SIZE, btnH}}, rangeBuf);

        y += rowH;
    }

    (void)buf;
}

static void render_patch_notes_edit(void) {
    if (!gPatchNotesEdit.active) {
        return;
    }
    double renderW      = get_render_width() / gGlobalGuiScale;
    double renderH      = get_render_height() / gGlobalGuiScale;
    double boxW         = 700.0;
    double boxH         = 500.0;
    double boxX         = (renderW - boxW) / 2.0;
    double boxY         = (renderH - boxH) / 2.0;
    double margin       = 10.0;
    double titleH       = 24.0;
    double lineH        = STANDARD_TEXT_HEIGHT + 3.0;
    double hintH        = STANDARD_TEXT_HEIGHT + 4.0;
    double textY0       = boxY + titleH + margin;
    double textX        = boxX + margin;
    double textW        = boxW - margin * 2.0;
    double maxTextH     = boxH - titleH - hintH - margin * 3.0;
    char   countBuf[32] = {0};

    // Background overlay to de-emphasise content beneath the dialog
    set_rgb_colour(RGB_GREY_2);
    render_rectangle(mainArea, {{0.0, 0.0}, {renderW, renderH}});

    // Dialog box
    set_rgb_colour(RGB_GREY_5);
    render_rectangle_with_border(mainArea, {{boxX, boxY}, {boxW, boxH}});

    // Title bar
    double btnH         = STANDARD_BUTTON_TEXT_HEIGHT;
    set_rgb_colour(RGB_GREY_3);
    render_rectangle(mainArea, {{boxX, boxY}, {boxW, titleH}});
    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{boxX + margin, boxY + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Patch Notes");

    // Character count
    snprintf(countBuf, sizeof(countBuf), "%zu / %d", strlen(gPatchNotesEdit.buffer), PATCH_NOTES_SIZE);
    set_rgb_colour(RGB_GREY_9);
    render_text(mainArea, {{boxX + boxW / 2.0 - 30.0, boxY + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, countBuf);

    // Close button in title bar
    gPatchNotesCloseRect = draw_button(mainArea,
                                       {{boxX + boxW - (get_text_width((char *)"Close", btnH, eCache) + 4.0) - 4.0, boxY + (titleH - btnH) / 2.0},
                                           {get_text_width((char *)"Close", btnH, eCache) + 4.0, btnH}},
                                       (char *)"Close", RGB_BACKGROUND_GREY);

    // Cache geometry for click-to-cursor and keyboard navigation
    gNoteTextX           = textX;
    gNoteTextY0          = textY0;
    gNoteLineH           = lineH;
    gNoteTextW           = textW;
    gNoteTextHParam      = STANDARD_TEXT_HEIGHT;

    build_note_visual_lines(gPatchNotesEdit.buffer, textW, STANDARD_TEXT_HEIGHT);

    int cursorPos  = (int)gPatchNotesEdit.cursorPos;
    int cursorLine = find_note_cursor_line(cursorPos);
    int visLines   = (int)(maxTextH / lineH);

    // Keep scroll so the cursor line is always visible
    if (cursorLine < gNoteScrollLine) {
        gNoteScrollLine = cursorLine;
    }

    if (cursorLine >= gNoteScrollLine + visLines) {
        gNoteScrollLine = cursorLine - visLines + 1;
    }

    if (gNoteScrollLine < 0) {
        gNoteScrollLine = 0;
    }
    // Text content area background
    set_rgb_colour(RGB_WHITE);
    render_rectangle(mainArea, {{boxX + 1, boxY + titleH}, {boxW - 2, boxH - titleH - hintH - 1}});

    {
        const char * buf = gPatchNotesEdit.buffer;
        double       y   = textY0;

        for (int i = gNoteScrollLine; i < gNoteLineCount && i < gNoteScrollLine + visLines; i++) {
            int  start                             = gNoteLines[i].bufStart;
            int  end                               = gNoteLines[i].bufEnd;
            int  len                               = end - start;

            char displayLine[PATCH_NOTES_SIZE + 4] = {0};

            if (i == cursorLine) {
                int col = cursorPos - start;

                if (col < 0) {
                    col = 0;
                }

                if (col > len) {
                    col = len;
                }
                strncpy(displayLine, buf + start, col);
                displayLine[col]     = '|';
                strncpy(displayLine + col + 1, buf + start + col, len - col);
                displayLine[len + 1] = '\0';
            } else {
                strncpy(displayLine, buf + start, len);
                displayLine[len] = '\0';
            }
            set_rgb_colour(RGB_BLACK);
            render_text(mainArea, {{textX, y}, {textW, STANDARD_TEXT_HEIGHT}}, displayLine);
            y += lineH;
        }
    }

    // Bottom bar: Discard Edits button + hint text
    set_rgb_colour(RGB_GREY_3);
    render_rectangle(mainArea, {{boxX, boxY + boxH - hintH}, {boxW, hintH}});

    double btnY = boxY + boxH - hintH + (hintH - btnH) / 2.0;
    double btnX = boxX + margin;
    gPatchNotesDiscardRect = draw_button(mainArea,
                                         {{btnX, btnY}, {get_text_width((char *)"Discard Edits", btnH, eCache) + 4.0, btnH}},
                                         (char *)"Discard Edits", RGB_BACKGROUND_GREY);
    btnX                  += gPatchNotesDiscardRect.size.w + 12.0;
    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{btnX, boxY + boxH - hintH + 3.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
                "Arrows/Click=move   Enter=newline   Esc=close without saving");
}

int note_editor_cursor_move_line(int cursorPos, int delta) {
    if (gNoteLineCount == 0) {
        return cursorPos;
    }
    int curLine = find_note_cursor_line(cursorPos);
    int col     = cursorPos - gNoteLines[curLine].bufStart;
    int newLine = curLine + delta;

    if (newLine < 0) {
        newLine = 0;
    }

    if (newLine >= gNoteLineCount) {
        newLine = gNoteLineCount - 1;
    }
    int newLen  = gNoteLines[newLine].bufEnd - gNoteLines[newLine].bufStart;
    return gNoteLines[newLine].bufStart + (col < newLen ? col : newLen);
}

int note_editor_cursor_line_home(int cursorPos) {
    if (gNoteLineCount == 0) {
        return 0;
    }
    return gNoteLines[find_note_cursor_line(cursorPos)].bufStart;
}

int note_editor_cursor_line_end(int cursorPos) {
    if (gNoteLineCount == 0) {
        return 0;
    }
    return gNoteLines[find_note_cursor_line(cursorPos)].bufEnd;
}

int note_editor_cursor_from_click(double logicalX, double logicalY) {
    if (gNoteLineCount == 0) {
        return -1;
    }
    double       relY    = logicalY - gNoteTextY0;

    if (relY < 0) {
        return -1;
    }
    int          lineIdx = gNoteScrollLine + (int)(relY / gNoteLineH);

    if (lineIdx >= gNoteLineCount) {
        lineIdx = gNoteLineCount - 1;
    }

    if (lineIdx < 0) {
        return -1;
    }
    int          start   = gNoteLines[lineIdx].bufStart;
    int          end     = gNoteLines[lineIdx].bufEnd;
    const char * buf     = gPatchNotesEdit.buffer;
    double       relX    = logicalX - gNoteTextX;
    char         tmp[PATCH_NOTES_SIZE + 1];

    for (int col = 0; col <= end - start; col++) {
        strncpy(tmp, buf + start, col);
        tmp[col] = '\0';

        if (get_text_width(tmp, gNoteTextHParam, eNoCache) > relX) {
            if (col > 0) {
                strncpy(tmp, buf + start, col - 1);
                tmp[col - 1] = '\0';
                double wPrev = get_text_width(tmp, gNoteTextHParam, eNoCache);
                strncpy(tmp, buf + start, col);
                tmp[col]     = '\0';
                double wCur  = get_text_width(tmp, gNoteTextHParam, eNoCache);
                return start + ((relX - wPrev < wCur - relX) ? col - 1 : col);
            }
            return start;
        }
    }

    return start + (end - start);
}

void do_graphics_loop(void) {
    bool reDraw = false;

    while ((gQuitAll == false) && (!glfwWindowShouldClose(gWindow))) {
        check_action_flags();

        reDraw  = gReDraw;
        gReDraw = false;

        if (reDraw == true) {
            glClearColor(0.8, 0.8, 0.8, 1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            render_modules();
            render_cables();

            if (gCableDrag.active == true) {
                tModule * module = get_module(gCableDrag.fromModuleKey);

                if (module != NULL) {
                    tCableColour dragColour = cable_colour_for_connector_type(module->connector[gCableDrag.fromConnectorIndex].type);
                    set_rgb_colour(gCableColourMap[dragColour]);
                    render_cable_from_to(module->connector[gCableDrag.fromConnectorIndex], gCableDrag.toConnector, 4.0);
                }
            }
            render_top_bar();
            render_morph_groups();
            render_scrollbars(gWindow);
            render_patch_settings_panel();
            render_perf_settings_panel();
            render_patch_params_panel();
            render_context_menu();
            render_patch_notes_edit();
            //Debug only
            //{
            //    double x        = 0.0;
            //    double y        = 0.0;
            //    int    fbWidth  = 0;
            //    int    fbHeight = 0;
            //    glfwGetCursorPos(gWindow, &x, &y);
            //    glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);
            //
            //    x /= fbWidth;
            //    y /= fbHeight;
            //    x *= TARGET_FRAME_BUFF_WIDTH;
            //    y *= TARGET_FRAME_BUFF_HEIGHT;
            //
            //    set_rgb_colour(RGB_BLACK);
            //    render_line(mainArea, {x, y - 10}, {x, y + 10}, 3);
            //    render_line(mainArea, {x - 10, y}, {x + 10, y}, 3);
            //}

            // Swap buffers and look for events
            glfwSwapBuffers(gWindow);
        }

        if ((gModuleDrag.active == true) || (gCableDrag.active == true)) {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(gWindow, &x, &y);
            cursor_pos(gWindow, x, y);  // Artificially do cursor_pos call for drag scrolling when cursor not moving
            glfwWaitEventsTimeout(0.016);
        } else {
            glfwWaitEvents();
        }
    }
}

void clean_up_graphics(void) {
    // Clean up
    FT_Done_Face(gFace);
    FT_Done_FreeType(gLibrary);
    free_textures();

    // glfwMakeContextCurrent(NULL); // TODO: Probably not needed, leaving whilst more tests are done
    glfwDestroyWindow(gWindow);
    gWindow = NULL;
    glfwTerminate();
}

#ifdef __cplusplus
}
#endif
