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
// Notes: Docs/code-notes/settingsPanels.c.md - "// notes §k" refers there.

// THE FOUR SETTINGS-FAMILY PANELS, moved out of graphics.c on 2026-09-16. graphics.c is the
// application's GLFW render loop and is not in the plug-in's build, so these opened there and had
// nothing to draw them - see floatingPanels.c. Nothing here touches a window, which is what made
// the move possible, and is the same reasoning that split patchWrite.c out.

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "globalVars.h"
#include "synthlibGlobals.h"
#include "utilsGraphics.h"
#include "dataBase.h"
#include "floatingPanel.h"
#include "paramCurves.h"
#include "synthSettingsResources.h"
#include "patchParamsResources.h"
#include "perfSettingsResources.h"
#include "settingsPanels.h"

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
    strncpy(tmp, text, (size_t)textLen);
    tmp[textLen] = '\0';

    if (get_text_width(tmp, textH, eNoCache) <= textW) {
        return textLen;
    }
    int  lo = 1, hi = textLen;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;

        strncpy(tmp, text, (size_t)mid);
        tmp[mid] = '\0';

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
                    gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                        segStart, segStart, true
                    };
                }
                break;
            }
            int  wrapAt    = find_wrap_point(buf + segStart, remaining, textW, textH);
            bool softWrap  = (wrapAt < remaining);
            gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                segStart, segStart + wrapAt, !softWrap
            };
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
            gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                len, len, true
            };
            break;
        }
    }

    if (gNoteLineCount == 0) {
        gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
            0, 0, true
        };
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

// Helper: draw a fixed-width dropdown trigger button, return updated x.
static double render_dropdown(double x, double y, double btnH,
                              const char * valStr, const char * widestVal,
                              tRectangle * rect) {
    *rect = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width((char *)widestVal, btnH, eCache) + 8.0, btnH}},
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
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, *y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x             += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        *items[i].rect = draw_button(mainArea,
                                     (tRectangle){{x, *y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
                                     items[i].get_str(),
                                     items[i].get_colour());
    }

    *y += rowH;
}

static double render_pp_row(double x, double y, double btnH,
                            const tPatchParamItem * items, int count) {
    int i = 0;

    for (i = 0; i < count; i++) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x                                += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        gPatchParamRects[items[i].rectId] = draw_button(mainArea,
                                                        (tRectangle){{x, y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
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
// notes §1

// Dims the module canvas behind a modal dialog. Not used by Mutator, which floats
// alongside the canvas rather than blocking it.

// notes §2

// notes §3

void render_patch_settings_panel(void) {
    static const char * slotLabel[4] = {"A", "B", "C", "D"};
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
    // notes §4
    tRectangle          box          = floating_panel_place(&gPatchSettingsEdit.panel, boxW, boxH);

    boxX                                  = box.coord.x;
    boxY                                  = box.coord.y;
    y                                     = boxY + titleH + margin;

    gPatchSettingsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Synth Settings");
    gSettingsPanelRects.close             = draw_panel_close_button(mainArea, box, gSettingsPanelRects.closePressed);
    gPatchSettingsEdit.panel.closeRect    = gSettingsPanelRects.close;   // carve it out of the title-bar drag

    // ── Synth Name ─────────────────────────────────────────────────
    {
        char displayBuf[CLAVIA_NAME_SIZE + 2] = {0};

        x  = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;

        if (gSynthNameEdit.active) {
            uint32_t cp = gSynthNameEdit.cursorPos;
            memcpy(displayBuf, gSynthNameEdit.buffer, cp);
            displayBuf[cp]                = '|';
            memcpy(&displayBuf[cp + 1], &gSynthNameEdit.buffer[cp], strlen(gSynthNameEdit.buffer) - cp + 1);
            gSettingsPanelRects.synthName = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_WHITE);
        } else {
            snprintf(displayBuf, sizeof(displayBuf), "%s", gSynthSettings.name);
            gSettingsPanelRects.synthName = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_BACKGROUND_GREY);
        }
    }
    y   += rowH;

    // ── MIDI Channels ──────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "MIDI Channels");
    y   += secH;
    colW = (boxW - margin * 2.0) / 4.0;

    for (i = 0; i < 4; i++) {
        x  = boxX + margin + i * colW;
        snprintf(buf, sizeof(buf), "%c:", slotLabel[i][0]);
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, buf);
        x += get_text_width((char *)"A:", btnH, eCache) + 4.0;
        midi_chan_str(gSynthSettings.midiChanSlot[i], buf, sizeof(buf));
        render_dropdown(x, y, btnH, buf, "Off", &gSettingsPanelRects.midiChan[i]);
    }

    y   += rowH;

    // ── Global + SysEx ─────────────────────────────────────────────
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSGlobal, kSSGlobalCount);

    // ── Options ────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Options");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSOptions, kSSOptionsCount);

    // ── Tuning ─────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Tuning");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSTuning, kSSTuningCount);

    // ── Pedal ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Pedal");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSPedal, kSSPedalCount);

    // ── Sort Mode ──────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Sort Mode");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSSort, kSSSortCount);
}

void render_patch_params_panel(void) {
    if (!gPatchParamsEdit.active) {
        return;
    }
    uint32_t   slot          = gPatchParamsEdit.slot;
    double     boxW          = 680.0;
    double     boxH          = 320.0;
    tRectangle box           = floating_panel_place(&gPatchParamsEdit.panel, boxW, boxH);
    double     boxX          = box.coord.x;
    double     boxY          = box.coord.y;
    double     margin        = 10.0;
    double     titleH        = 24.0;
    double     rowH          = 26.0;
    double     secH          = 18.0;
    double     btnH          = STANDARD_BUTTON_TEXT_HEIGHT;
    double     y             = boxY + titleH + margin;
    double     x             = 0.0;
    double     dialH         = 0.0;
    tModule *  sustMod       = get_module_slot(slot, (uint32_t)locationMorph, patchModuleSustain);
    tModule *  vibMod        = get_module_slot(slot, (uint32_t)locationMorph, patchModuleVibrato);
    tModule *  glideMod      = get_module_slot(slot, (uint32_t)locationMorph, patchModuleGlide);
    uint8_t    sustainPedal  = sustMod ? sustMod->param[0][SUSTAIN_PEDAL].value : 0;
    int8_t     octaveShift   = sustMod ? (int8_t)sustMod->param[0][OCTAVE_SHIFT].value : 0;
    uint8_t    vibratoRate   = vibMod ? vibMod->param[0][VIBRATO_RATE].value : 0;
    uint8_t    vibratoAmount = vibMod ? vibMod->param[0][VIBRATO_DEPTH].value : 0;
    uint8_t    glideTime     = glideMod ? glideMod->param[0][GLIDE_SPEED].value : 0;
    char       buf[16]       = {0};

    gPatchParamsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Patch Settings");
    gPatchParamClose                    = draw_panel_close_button(mainArea, box, gPatchParamClosePressed);
    gPatchParamsEdit.panel.closeRect    = gPatchParamClose;   // carve it out of the title-bar drag

    // ── Slot buttons in title bar ──────────────────────────────────
    {
        static const char * slotLabels[MAX_SLOTS] = {"A", "B", "C", "D"};
        double              slotBtnW              = get_text_width((char *)"A", btnH, eCache) /* + 6.0*/;
        // Right-aligned against the panel edge, now that the close control has moved top left.
        double              slotX                 = boxX + boxW - 8.0 - BORDER_LINE_WIDTH - ((slotBtnW + 8.0) * MAX_SLOTS);

        for (uint32_t s = 0; s < MAX_SLOTS; s++) {
            tRgb col = (s == slot) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
            gPatchParamSlots[s] = draw_button(mainArea,
                                              (tRectangle){{slotX + s * (slotBtnW + 8.0), boxY + 4.0}, {slotBtnW, btnH}},
                                              slotLabels[s], col);
        }
    }

    // ── Sustain Pedal + Octave Shift ───────────────────────────────
    {
        x                                = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Sustain Pedal:");
        x                               += get_text_width((char *)"Sustain Pedal:", btnH, eCache) + 4.0;
        gPatchParamRects[pPSustainPedal] = draw_button(mainArea,
                                                       (tRectangle){{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                       sustainPedal ? "On" : "Off",
                                                       sustainPedal ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

        x                                = boxX + boxW / 2.0;
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Octave Shift:");
        x                               += get_text_width((char *)"Octave Shift:", btnH, eCache) + 4.0;
        snprintf(buf, sizeof(buf), "%+d", (int)octaveShift);
        render_dropdown(x, y, btnH, buf, "+2", &gPatchParamRects[pPOctaveShift]);
    }
    y                                += rowH;

    // ── Arpeggiator ────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Arpeggiator");
    y                                += secH;
    render_pp_row(boxX + margin, y, btnH, kPPArp, kPPArpCount);
    y                                += rowH;

    // ── Vibrato ────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Vibrato");
    y                                += secH;
    x                                 = render_pp_row(boxX + margin, y, btnH, kPPVibrato, kPPVibratoCount);
    // Dial-anchored: the rect is the circle, and its label and value are drawn in the two text
    // rows above it - so the dial goes two rows below where the block used to start.
    dialH                             = 20.0;
    snprintf(buf, sizeof(buf), "%u cnt", (unsigned)vibratoAmount);
    gPatchParamRects[pPVibratoAmount] = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Amount", buf, btnH, vibratoAmount, 100, 0, (tRgb)RGB_BACKGROUND_GREY);
    x                                += get_text_width((char *)"100 cnt", btnH, eCache) + 8.0;
    snprintf(buf, sizeof(buf), "%.2f Hz", vibrato_rate_hz((double)vibratoRate));
    gPatchParamRects[pPVibratoRate]   = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Rate", buf, btnH, vibratoRate, 127, 0, (tRgb)RGB_BACKGROUND_GREY);
    y                                += rowH;

    // ── Glide ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Glide");
    y                                += secH;
    x                                 = render_pp_row(boxX + margin, y, btnH, kPPGlide, kPPGlideCount);
    gPatchParamRects[pPGlideTime]     = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Time", get_glide_time_str(glideTime), btnH, glideTime, 127, 0, (tRgb)RGB_BACKGROUND_GREY);
    y                                += rowH;

    // ── Bend ───────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Bend");
    y                                += secH;
    render_pp_row(boxX + margin, y, btnH, kPPBend, kPPBendCount);

    (void)y;
}

void render_perf_settings_panel(void) {
    if (!gPerfSettingsEdit.active) {
        return;
    }
    double     boxW                     = 700.0;
    double     boxH                     = 390.0;
    tRectangle box                      = floating_panel_place(&gPerfSettingsEdit.panel, boxW, boxH);
    double     boxX                     = box.coord.x;
    double     boxY                     = box.coord.y;
    double     margin                   = 10.0;
    double     titleH                   = 24.0;
    double     rowH                     = 26.0;
    double     secH                     = 18.0;
    double     btnH                     = STANDARD_BUTTON_TEXT_HEIGHT;
    double     y                        = boxY + titleH + margin;
    double     colX[kPSSlotToggleCount] = {0.0, 0.0, 0.0};
    int        i                        = 0;
    int        col                      = 0;
    char       buf[32]                  = {0};
    char       note[8]                  = {0};
    char       loNote[8]                = {0};
    char       hiNote[8]                = {0};
    char       rangeBuf[18]             = {0};

    gPerfSettingsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Performance Settings");
    gPerfSettingsPanelRects.close        = draw_panel_close_button(mainArea, box, gPerfSettingsPanelRects.closePressed);
    gPerfSettingsEdit.panel.closeRect    = gPerfSettingsPanelRects.close;   // carve it out of the title-bar drag

    // ── Perf Name ──────────────────────────────────────────────────
    {
        char   nameBuf[CLAVIA_NAME_SIZE + 2] = {0};
        double x                             = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;
        snprintf(nameBuf, sizeof(nameBuf), "%s", gGlobalSettings.perfName);
        draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}},
                    nameBuf, (tRgb)RGB_BACKGROUND_GREY);
    }
    y                                   += rowH;

    // ── Master Clock ───────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Master Clock");
    y                                   += secH;

    {
        // blockH is the whole BPM readout + dial, which the Running button centres against and
        // the row advance uses. The dial itself is one text row down from the top of that, since
        // render_dial_with_text() is dial-anchored and draws the BPM string above it.
        double blockH = 48.0;
        double x      = boxX + margin;
        snprintf(buf, sizeof(buf), "%u BPM", (unsigned)gGlobalSettings.masterClock);
        gPerfSettingsPanelRects.masterClock        = render_dial_with_text(mainArea, (tRectangle){{x, y + STANDARD_BUTTON_TEXT_HEIGHT}, {20.0, 20.0}}, NULL, buf, STANDARD_BUTTON_TEXT_HEIGHT, gGlobalSettings.masterClock >= 30 ? gGlobalSettings.masterClock - 30 : 0, 211, 0, (tRgb)RGB_BACKGROUND_GREY);
        x                                         += 20.0 + 12.0;
        gPerfSettingsPanelRects.masterClockRunning = draw_button(mainArea,
                                                                 (tRectangle){{x, y + (blockH - btnH) / 2.0}, {get_text_width((char *)"Stopped", btnH, eCache) + 8.0, btnH}},
                                                                 gGlobalSettings.masterClockRunning ? "Running" : "Stopped",
                                                                 gGlobalSettings.masterClockRunning ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        y                                         += blockH + 4.0;
    }

    // ── Slots ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Slots");
    y      += secH;

    double labelColW = get_text_width((char *)"Slot X:", btnH, eCache) + 8.0;
    double dropW     = get_text_width((char *)"On", btnH, eCache) + 16.0;
    double noteDropW = get_text_width((char *)"C#-1", btnH, eCache) + 10.0;
    double colEn     = boxX + margin + labelColW;
    double colKbd    = colEn + dropW + 8.0;
    double colHld    = colKbd + dropW + 30.0;
    double colLo     = colHld + dropW + 16.0;
    double colHi     = colLo + noteDropW + 8.0;
    double colRng    = colHi + noteDropW + 12.0;

    colX[0] = colEn;
    colX[1] = colKbd;
    colX[2] = colHld;

    // Column headers
    set_rgb_colour((tRgb)RGB_BLACK);
    render_text(mainArea, (tRectangle){{colEn, y}, {BLANK_SIZE, btnH}}, "Enable");
    render_text(mainArea, (tRectangle){{colKbd, y}, {BLANK_SIZE, btnH}}, "Keyboard");
    render_text(mainArea, (tRectangle){{colHld, y}, {BLANK_SIZE, btnH}}, "Hold");
    render_text(mainArea, (tRectangle){{colLo, y}, {BLANK_SIZE, btnH}}, "Lower");
    render_text(mainArea, (tRectangle){{colHi, y}, {BLANK_SIZE, btnH}}, "Upper");

    // Keyboard Range global toggle — right side of header
    {
        double x = colRng;
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Kbd Range:");
        x                                    += get_text_width((char *)"Kbd Range:", btnH, eCache) + 4.0;
        gPerfSettingsPanelRects.keyboardRange = draw_button(mainArea,
                                                            (tRectangle){{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                            gPerfSettings.keyboardRange ? "On" : "Off",
                                                            gPerfSettings.keyboardRange ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }
    y      += rowH;

    // Slot rows A–D
    static const char * slotLabel[] = {"Slot A:", "Slot B:", "Slot C:", "Slot D:"};

    for (i = 0; i < MAX_SLOTS; i++) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{boxX + margin, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)slotLabel[i]);

        for (col = 0; col < kPSSlotToggleCount; col++) {
            kPSSlotToggles[col].rects[i] = draw_button(mainArea,
                                                       (tRectangle){{colX[col], y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
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
        render_text(mainArea, (tRectangle){{colRng, y + 2.0}, {BLANK_SIZE, btnH}}, rangeBuf);

        y += rowH;
    }

    (void)buf;
}

void render_patch_notes_edit(void) {
    if (!gPatchNotesEdit.active) {
        return;
    }
    double     boxW         = 700.0;
    double     boxH         = 500.0;

    // FLOATING, so the position comes from the panel rather than from the window — chosen once on
    // first show, and thereafter wherever the user has dragged it.
    tRectangle panelBox     = floating_panel_place(&gPatchNotesEdit.panel, boxW, boxH);
    double     boxX         = panelBox.coord.x;
    double     boxY         = panelBox.coord.y;
    double     margin       = 10.0;
    double     titleH       = 24.0;
    double     lineH        = STANDARD_TEXT_HEIGHT + 3.0;
    double     hintH        = STANDARD_TEXT_HEIGHT + 12.0; // extra headroom below the button/text baseline so descenders (g, y, p) aren't clipped by the border
    double     textY0       = boxY + titleH + margin;
    double     textX        = boxX + margin;
    double     textW        = boxW - margin * 2.0;
    double     maxTextH     = boxH - titleH - hintH - margin * 3.0;
    char       countBuf[32] = {0};

    double     btnH         = STANDARD_BUTTON_TEXT_HEIGHT;

    // No draw_dialog_background_overlay(): dimming the canvas is what a MODAL dialog does, and the
    // notes editor is not one — it floats over a live canvas like the settings panels.
    gPatchNotesPanelRect               = panelBox;
    gPatchNotesEdit.panel.titleBarRect = draw_panel_chrome(mainArea, panelBox, titleH, "Patch Notes");
    gPatchNotesCloseRect               = draw_panel_close_button(mainArea, panelBox, gPatchNotesClosePressed);
    gPatchNotesEdit.panel.closeRect    = gPatchNotesCloseRect;

    // Character count
    snprintf(countBuf, sizeof(countBuf), "%zu / %d", strlen(gPatchNotesEdit.buffer), PATCH_NOTES_SIZE);
    set_rgb_colour((tRgb)RGB_GREY_9);
    render_text(mainArea, (tRectangle){{boxX + boxW / 2.0 - 30.0, boxY + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, countBuf);

    // Cache geometry for click-to-cursor and keyboard navigation
    gNoteTextX                         = textX;
    gNoteTextY0                        = textY0;
    gNoteLineH                         = lineH;
    gNoteTextW                         = textW;
    gNoteTextHParam                    = STANDARD_TEXT_HEIGHT;

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
    set_rgb_colour((tRgb)RGB_WHITE);
    render_rectangle(mainArea, (tRectangle){{boxX + 1, boxY + titleH}, {boxW - 2, boxH - titleH - hintH - 1}});

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
            set_rgb_colour((tRgb)RGB_BLACK);
            render_text(mainArea, (tRectangle){{textX, y}, {textW, STANDARD_TEXT_HEIGHT}}, displayLine);
            y += lineH;
        }
    }

    // Bottom bar: Discard Edits button + hint text. Inset so it doesn't paint over the panel's
    // bottom/left/right border line, same as the title bar above.
    set_rgb_colour((tRgb)RGB_GREY_3);
    render_rectangle(mainArea, (tRectangle){{boxX + BORDER_LINE_WIDTH, boxY + boxH - hintH}, {boxW - 2.0 * BORDER_LINE_WIDTH, hintH - BORDER_LINE_WIDTH}});

    double btnY       = boxY + boxH - hintH + (hintH - btnH) / 2.0 - 2.0; // draw_button's internal padding sits its text a couple px lower than render_text at the same y - nudge up so "Discard Edits" lines up with the hint text baseline
    double btnX       = boxX + margin;
    tRgb   discardCol = gPatchNotesDiscardPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
    gPatchNotesDiscardRect = draw_button(mainArea,
                                         (tRectangle){{btnX, btnY}, {get_text_width((char *)"Discard Edits", btnH, eCache) + 4.0, btnH}},
                                         (char *)"Discard Edits", discardCol);
    btnX                  += gPatchNotesDiscardRect.size.w + 12.0;
    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{btnX, boxY + boxH - hintH + (hintH - STANDARD_TEXT_HEIGHT) / 2.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
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

#ifdef __cplusplus
}
#endif
