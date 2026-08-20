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

#include "paramOverview.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "dataBase.h"
#include "globalVars.h"
#include "graphics.h"
#include "menus.h"
#include "moduleResourcesAccess.h"
#include "mouseHandle.h"
#include "msgQueue.h"
#include "paramPages.h"
#include "protocol.h"
#include "undo.h"
#include "utilsGraphics.h"
#include "contextMenu.h"

tParamOverviewEdit       gParamOverview                 = {0};

static const char *const kSlotLabel[MAX_SLOTS]          = {"A", "B", "C", "D"};
static const char *const kPageRowLabel[NUM_PARAM_PAGES] = {"A", "B", "C", "D", "E"};

#define PO_CELL_GAP      3.0
#define PO_CELL_PAD      3.0
#define PO_MIN_CELL_W    56.0
#define PO_MAX_CELL_W    150.0     // a very long module name shouldn't stretch the grid off-screen

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void open_param_overview_panel(uint32_t slot) {
    // Not a memset, for the same reason the Parameter Pages panel isn't: reopening keeps the
    // Patch/Global and View MIDI choices, so it stays usable as a working surface.
    gParamOverview.active       = true;
    gParamOverview.slot         = slot;
    gParamOverview.closePressed = false;
    gParamOverview.dragFrom     = -1;
}

void close_param_overview_panel(void) {
    gParamOverview.active   = false;
    gParamOverview.dragFrom = -1;
}

// ─── Assignments ─────────────────────────────────────────────────────────────

// Row r of the grid is page A1..E3, so it maps to the flat knob index the same way
// paramPages.c's knob_index() does: page = r / NUM_BANKS_PER_PAGE, bank = r % NUM_BANKS_PER_PAGE.
static uint32_t knob_index_for_cell(uint32_t row, uint32_t col) {
    return (row * NUM_KNOBS_PER_BANK) + col;
}

static tKnobTarget cell_target(uint32_t row, uint32_t col) {
    return param_pages_knob_target(gParamOverview.showGlobal, gParamOverview.slot,
                                   knob_index_for_cell(row, col));
}

static bool knob_is_assigned(uint32_t index) {
    if (index >= MAX_NUM_KNOBS) {
        return false;
    }

    if (gParamOverview.showGlobal) {
        return gGlobalKnobArray[index].assigned;
    }
    return gKnobArray[gParamOverview.slot].knob[index].assigned;
}

// Moves the assignment at `from` to `to`, overwriting whatever was there. This is the manual's
// headline use of this window ("drag a grey display area to another grey display area and you
// move the knob assignment to the new position"), so it OVERWRITES rather than swaps — that is
// what the original does, and a swap would silently resurrect an assignment the user was
// deliberately replacing.
//
// The patch case tells the G2 with ONE whole-patch write rather than the deassign/deassign/assign
// triple the right-click Assign menu sends. write_knobs() (protocol.c) is already part of
// push_slot_to_device(), and a burst of small slot commands is exactly the pattern that loses
// assignments to the patch-version race — see the bulk MIDI CC note in menus.c. Global knobs have
// no whole-perf push to ride on (write_global_knobs() is only used when SAVING a performance
// file), so those still go as individual commands, matching action_assign_global_knob().
static void move_assignment(uint32_t from, uint32_t to) {
    tMessageContent msg = {0};

    if ((from == to) || (from >= MAX_NUM_KNOBS) || (to >= MAX_NUM_KNOBS) || !knob_is_assigned(from)) {
        return;
    }

    if (gParamOverview.showGlobal) {
        undo_begin_global_knob_edit();

        tGlobalKnob moving = gGlobalKnobArray[from];

        if (gGlobalKnobArray[to].assigned) {
            msg.cmd                              = eMsgCmdDeassignGlobalKnob;
            msg.globalKnobDeassignData.knobIndex = to;
            msg_send(&gToUsbThread, &msg);
            memset(&msg, 0, sizeof(msg));
        }
        msg.cmd                              = eMsgCmdDeassignGlobalKnob;
        msg.globalKnobDeassignData.knobIndex = from;
        msg_send(&gToUsbThread, &msg);
        memset(&msg, 0, sizeof(msg));

        gGlobalKnobArray[to]                 = moving;
        gGlobalKnobArray[from]               = (tGlobalKnob){
            0
        };

        msg.cmd                              = eMsgCmdAssignGlobalKnob;
        msg.globalKnobAssignData.slotIndex   = moving.slotIndex;
        msg.globalKnobAssignData.location    = moving.location;
        msg.globalKnobAssignData.moduleIndex = moving.moduleIndex;
        msg.globalKnobAssignData.paramIndex  = moving.paramIndex;
        msg.globalKnobAssignData.knobIndex   = to;
        msg_send(&gToUsbThread, &msg);

        undo_commit_global_knob_edit();
    } else {
        uint32_t slot       = gParamOverview.slot;
        tKnob    toBefore   = gKnobArray[slot].knob[to];
        tKnob    fromBefore = gKnobArray[slot].knob[from];

        gKnobArray[slot].knob[to]   = fromBefore;
        gKnobArray[slot].knob[from] = (tKnob){
            0
        };

        undo_push_knob(slot,
                       to, &toBefore, &gKnobArray[slot].knob[to],
                       (int32_t)from, &fromBefore, &gKnobArray[slot].knob[from]);

        msg.cmd                     = eMsgCmdWritePatch;
        msg.slot                    = slot;
        msg_send(&gToUsbThread, &msg);
    }
    synthlib_request_redraw();
}

// ─── Rendering ───────────────────────────────────────────────────────────────

// Copies src into dst, dropping characters off the end until it fits maxWidth at textHeight.
static void fit_text(char * dst, size_t dstSize, const char * src, double maxWidth, double textHeight) {
    size_t length = strlen(src);

    if (length >= dstSize) {
        length = dstSize - 1;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';

    while ((length > 0) && (get_text_width(dst, textHeight, eCache) > maxWidth)) {
        length--;
        dst[length] = '\0';
    }
}

// The second line of a box: the param's name, or its MIDI CC# when View MIDI is on. The original's
// View MIDI shows the CC# "for those knobs and buttons that are also assigned to panel controls",
// which is precisely the set this grid draws, so it is a straight substitution on the same line.
static void cell_detail_text(const tKnobTarget * target, char * dst, size_t dstSize) {
    if (!gParamOverview.showMidi) {
        snprintf(dst, dstSize, "%s", param_pages_knob_param_label(target));
        return;
    }
    int32_t entry = find_controller_for_param(target->key.slot, target->key.location,
                                              target->key.index, target->paramIndex);

    if (entry < 0) {
        snprintf(dst, dstSize, "--");
    } else {
        snprintf(dst, dstSize, "CC %u", gControllerArray[target->key.slot].controller[entry].midiCC);
    }
}

void render_param_overview_panel(void) {
    if (!gParamOverview.active) {
        return;
    }
    double renderW   = get_render_width() / gGlobalGuiScale;
    double renderH   = get_render_height() / gGlobalGuiScale;
    double margin    = 10.0;
    double titleH    = 24.0;
    double rowH      = 20.0;
    double btnH      = STANDARD_BUTTON_TEXT_HEIGHT;
    double textH     = STANDARD_TEXT_HEIGHT;
    double cellH     = (textH * 2.0) + (PO_CELL_PAD * 2.0);
    double rowLabelW = get_text_width((char *)"E3", textH, eCache) + 10.0;

    // Cells are all one width, sized to the widest thing any of the 120 boxes has to say, so the
    // grid stays a grid. Clamped at both ends: PO_MIN_CELL_W keeps an empty patch from collapsing
    // to a sliver, PO_MAX_CELL_W keeps one long module name from pushing the grid off the window.
    double cellW     = PO_MIN_CELL_W;

    for (uint32_t row = 0; row < PARAM_OVERVIEW_ROWS; row++) {
        for (uint32_t col = 0; col < NUM_KNOBS_PER_BANK; col++) {
            tKnobTarget target                     = cell_target(row, col);

            if (!target.assigned) {
                continue;
            }
            char        detail[64]                 = {0};
            char        name[CLAVIA_NAME_SIZE + 8] = {0};

            cell_detail_text(&target, detail, sizeof(detail));

            if (gParamOverview.showGlobal) {
                snprintf(name, sizeof(name), "%s:%s", kSlotLabel[target.key.slot],
                         param_pages_module_display_name(target.module));
            } else {
                snprintf(name, sizeof(name), "%s", param_pages_module_display_name(target.module));
            }
            cellW = fmax(cellW, get_text_width(name, textH, eCache) + (PO_CELL_PAD * 2.0));
            cellW = fmax(cellW, get_text_width(detail, textH, eCache) + (PO_CELL_PAD * 2.0));
        }
    }

    cellW = fmin(cellW, PO_MAX_CELL_W);

    double gridW = (cellW * NUM_KNOBS_PER_BANK) + (PO_CELL_GAP * (NUM_KNOBS_PER_BANK - 1));
    double boxW  = (margin * 2.0) + rowLabelW + gridW;
    double gridH = (cellH * PARAM_OVERVIEW_ROWS) + (PO_CELL_GAP * (PARAM_OVERVIEW_ROWS - 1));
    double boxH  = titleH + margin + rowH + margin + textH + 2.0 + gridH + margin;

    // Too wide or too tall for the window: give the grid whatever is left. Nothing clips (there is
    // no scissor anywhere in SynthLib), so a box that has to shrink runs its text into its
    // neighbour - still better than a panel running off both edges of the window.
    if (boxW > (renderW - (margin * 2.0))) {
        boxW  = renderW - (margin * 2.0);
        cellW = ((boxW - (margin * 2.0) - rowLabelW) - (PO_CELL_GAP * (NUM_KNOBS_PER_BANK - 1))) / NUM_KNOBS_PER_BANK;
        gridW = (cellW * NUM_KNOBS_PER_BANK) + (PO_CELL_GAP * (NUM_KNOBS_PER_BANK - 1));
    }

    if (boxH > (renderH - (margin * 2.0))) {
        boxH  = renderH - (margin * 2.0);
        cellH = ((boxH - titleH - (margin * 3.0) - rowH - textH - 2.0) - (PO_CELL_GAP * (PARAM_OVERVIEW_ROWS - 1))) / PARAM_OVERVIEW_ROWS;
    }
    // FLOATING, so the position comes from the panel rather than from the window: chosen once on
    // first show and thereafter wherever the user has dragged it. Centring every frame is what made
    // a panel impossible to move — it snapped back before the next redraw.
    //
    // No draw_dialog_background_overlay() either. Dimming the canvas behind is what a MODAL dialog
    // does, and this is not one: the canvas stays live underneath and stays legible to match.
    tRectangle panelBox = floating_panel_place(&gParamOverview.panel, boxW, boxH);
    double     boxX     = panelBox.coord.x;
    double     boxY     = panelBox.coord.y;
    double     y        = boxY + titleH + margin;

    gParamOverview.panel.titleBarRect = draw_panel_chrome(mainArea, panelBox, titleH, "Parameter Overview");
    gParamOverview.close              = draw_panel_close_button(mainArea, panelBox, gParamOverview.closePressed);
    gParamOverview.panel.closeRect    = gParamOverview.close;

    // ── Slot buttons in the title bar ──────────────────────────────────
    // Patch pages only: a global assignment carries its own Slot per knob, so a panel-wide Slot
    // selector would have nothing to mean there.
    {
        double slotBtnW = get_text_width((char *)"A", btnH, eCache);
        double slotX    = boxX + boxW - 8.0 - BORDER_LINE_WIDTH - ((slotBtnW + 8.0) * MAX_SLOTS);

        for (uint32_t s = 0; s < MAX_SLOTS; s++) {
            if (gParamOverview.showGlobal) {
                gParamOverview.slotButton[s] = (tRectangle){
                    0
                };
                continue;
            }
            tRgb colour = (s == gParamOverview.slot) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;

            gParamOverview.slotButton[s] = draw_button(mainArea,
                                                       (tRectangle){{slotX + (s * (slotBtnW + 8.0)), boxY + 4.0}, {slotBtnW, btnH}},
                                                       kSlotLabel[s], colour);
        }
    }

    // ── Button row: Patch/Global, View MIDI, and the two bulk MIDI tools ───
    // Assign MIDI and Clear MIDI live here because this is where the original puts them (manual
    // p.126); they are the same operations the Tools menu offers, run on the Slot THIS panel is
    // showing rather than the selected one.
    {
        double x       = boxX + margin;
        double patchW  = get_text_width((char *)"Patch", btnH, eCache) + 14.0;
        double globalW = get_text_width((char *)"Global", btnH, eCache) + 14.0;
        double midiW   = get_text_width((char *)"View MIDI", btnH, eCache) + 14.0;
        double assignW = get_text_width((char *)"Assign MIDI", btnH, eCache) + 14.0;
        double clearW  = get_text_width((char *)"Clear MIDI", btnH, eCache) + 14.0;

        gParamOverview.patchButton  = draw_button(mainArea, (tRectangle){{x, y}, {patchW, btnH}},
                                                  "Patch", gParamOverview.showGlobal ? (tRgb)RGB_BACKGROUND_GREY : (tRgb)RGB_GREEN_ON);
        x                          += patchW + 6.0;
        gParamOverview.globalButton = draw_button(mainArea, (tRectangle){{x, y}, {globalW, btnH}},
                                                  "Global", gParamOverview.showGlobal ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        x                          += globalW + 16.0;
        gParamOverview.midiButton   = draw_button(mainArea, (tRectangle){{x, y}, {midiW, btnH}},
                                                  "View MIDI", gParamOverview.showMidi ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

        // The bulk tools act on a Slot's controller table, which the global pages are not - their
        // knobs each name their own Slot, so "assign every CC in this view" has no single table to
        // write. Hidden rather than greyed, as there is nothing to explain.
        if (gParamOverview.showGlobal) {
            gParamOverview.assignMidiButton = (tRectangle){
                0
            };
            gParamOverview.clearMidiButton  = (tRectangle){
                0
            };
        } else {
            double bx = boxX + boxW - margin - clearW;

            gParamOverview.clearMidiButton  = draw_button(mainArea, (tRectangle){{bx, y}, {clearW, btnH}},
                                                          "Clear MIDI", (tRgb)RGB_BACKGROUND_GREY);
            bx                             -= assignW + 6.0;
            gParamOverview.assignMidiButton = draw_button(mainArea, (tRectangle){{bx, y}, {assignW, btnH}},
                                                          "Assign MIDI", (tRgb)RGB_BACKGROUND_GREY);
        }
    }
    y += rowH + margin;

    // ── Column headers: the eight knob positions ───────────────────────
    {
        double gridX = boxX + margin + rowLabelW;

        set_rgb_colour((tRgb)RGB_GREY_3);

        for (uint32_t col = 0; col < NUM_KNOBS_PER_BANK; col++) {
            char   label[4] = {0};
            double cx       = gridX + (col * (cellW + PO_CELL_GAP));

            snprintf(label, sizeof(label), "%u", col + 1);
            render_text(mainArea, (tRectangle){{cx + ((cellW - get_text_width(label, textH, eCache)) / 2.0), y}, {BLANK_SIZE, textH}}, label);
        }
    }
    y += textH + 2.0;

    // ── The grid: 15 pages down, 8 knob positions across ───────────────
    {
        double gridX = boxX + margin + rowLabelW;

        for (uint32_t row = 0; row < PARAM_OVERVIEW_ROWS; row++) {
            double cy          = y + (row * (cellH + PO_CELL_GAP));
            char   rowLabel[8] = {0};

            snprintf(rowLabel, sizeof(rowLabel), "%s%u",
                     kPageRowLabel[row / NUM_BANKS_PER_PAGE], (row % NUM_BANKS_PER_PAGE) + 1);
            set_rgb_colour((tRgb)RGB_GREY_3);
            render_text(mainArea, (tRectangle){{boxX + margin, cy + ((cellH - textH) / 2.0)}, {BLANK_SIZE, textH}}, rowLabel);

            for (uint32_t col = 0; col < NUM_KNOBS_PER_BANK; col++) {
                tKnobTarget target                     = cell_target(row, col);
                double      cx                         = gridX + (col * (cellW + PO_CELL_GAP));
                tRectangle  cell                       = {{cx, cy}, {cellW, cellH}};
                char        line[CLAVIA_NAME_SIZE + 8] = {0};
                char        text[64]                   = {0};

                gParamOverview.cell[row][col] = cell;

                // The box being dragged stays visible where it was, marked rather than emptied, so
                // the source is still readable while choosing a destination.
                bool        isSource                   = (gParamOverview.dragFrom >= 0)
                                                         && ((uint32_t)gParamOverview.dragFrom == knob_index_for_cell(row, col));

                set_rgb_colour(isSource ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_GREY_9);
                render_rectangle(mainArea, cell);

                if (!target.assigned) {
                    continue;
                }

                if (gParamOverview.showGlobal) {
                    snprintf(text, sizeof(text), "%s:%s", kSlotLabel[target.key.slot],
                             param_pages_module_display_name(target.module));
                } else {
                    snprintf(text, sizeof(text), "%s", param_pages_module_display_name(target.module));
                }
                fit_text(line, sizeof(line), text, cellW - (PO_CELL_PAD * 2.0), textH);
                set_rgb_colour((tRgb)RGB_BLACK);
                render_text(mainArea, (tRectangle){{cx + PO_CELL_PAD, cy + PO_CELL_PAD}, {BLANK_SIZE, textH}}, line);

                cell_detail_text(&target, text, sizeof(text));
                fit_text(line, sizeof(line), text, cellW - (PO_CELL_PAD * 2.0), textH);
                set_rgb_colour((tRgb)RGB_GREY_3);
                render_text(mainArea, (tRectangle){{cx + PO_CELL_PAD, cy + PO_CELL_PAD + textH}, {BLANK_SIZE, textH}}, line);
            }
        }
    }

    // ── The dragged assignment, following the cursor ───────────────────
    if (gParamOverview.dragFrom >= 0) {
        tKnobTarget target = param_pages_knob_target(gParamOverview.showGlobal, gParamOverview.slot,
                                                     (uint32_t)gParamOverview.dragFrom);

        if (target.assigned) {
            tCoord     coord                      = {0};
            char       line[CLAVIA_NAME_SIZE + 8] = {0};

            get_global_gui_scaled_mouse_coord(&coord);
            fit_text(line, sizeof(line), param_pages_module_display_name(target.module), cellW - (PO_CELL_PAD * 2.0), textH);

            tRectangle ghost                      = {{coord.x + 8.0, coord.y + 8.0}, {cellW, textH + (PO_CELL_PAD * 2.0)}};

            set_rgb_colour((tRgb)RGB_GREEN_ON);
            render_rectangle(mainArea, ghost);
            set_rgb_colour((tRgb)RGB_BLACK);
            render_text(mainArea, (tRectangle){{ghost.coord.x + PO_CELL_PAD, ghost.coord.y + PO_CELL_PAD}, {BLANK_SIZE, textH}}, line);
        }
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

// Same modal precedence the other panels use: while one of this panel's own dropdowns is open,
// the click belongs to the menu, not to whatever is behind it.
static bool panel_context_menu_click(tCoord coord) {
    if (!gContextMenu.active) {
        return false;
    }

    if (!handle_context_menu_click(coord)) {
        gContextMenu.active = false;
    }
    synthlib_request_redraw();
    return true;
}

// The flat knob index of the box under the cursor, or -1.
static int32_t cell_at(tCoord coord) {
    for (uint32_t row = 0; row < PARAM_OVERVIEW_ROWS; row++) {
        for (uint32_t col = 0; col < NUM_KNOBS_PER_BANK; col++) {
            if (within_rectangle(coord, gParamOverview.cell[row][col])) {
                return (int32_t)knob_index_for_cell(row, col);
            }
        }
    }

    return -1;
}

bool handle_param_overview_mouse(tCoord coord, tMouseButton mouseButton) {
    if (!gParamOverview.active) {
        return false;
    }

    // FLOATING NOW: the move/raise/close-button routing comes from SynthLib rather than being
    // written out per panel. eFloatingPanelContent means "it landed on me, but not on my chrome" —
    // which is the only case the panel's own hit-testing below should see.
    switch (floating_panel_mouse(&gParamOverview.panel, coord, mouseButton, gParamOverview.closePressed)) {
        case eFloatingPanelPassThrough:
            return false;

        case eFloatingPanelConsumed:
            return true;

        case eFloatingPanelContent:
        default:
            break;
    }

    if (mouseButton == mouseButtonLeftDown) {
        if (gContextMenu.active) {
            return true;   // the open dropdown gets the whole click, down and up
        }

        if (within_rectangle(coord, gParamOverview.close)) {
            gParamOverview.closePressed = true;
        } else {
            int32_t index = cell_at(coord);

            // Only an assigned box can be picked up — there is nothing to move out of an empty one.
            if ((index >= 0) && knob_is_assigned((uint32_t)index)) {
                gParamOverview.dragFrom = index;
            }
        }
    }

    if (mouseButton == mouseButtonLeftUp) {
        bool    wasClosePressed = gParamOverview.closePressed;
        int32_t dragFrom        = gParamOverview.dragFrom;

        gParamOverview.closePressed = false;
        gParamOverview.dragFrom     = -1;

        if (panel_context_menu_click(coord)) {
            return true;
        }

        if (wasClosePressed && within_rectangle(coord, gParamOverview.close)) {
            close_param_overview_panel();
        } else if (dragFrom >= 0) {
            // A drop on a different box moves the assignment; anywhere else, including the box it
            // started on, is a no-op. Dropping outside the grid deliberately does NOT clear the
            // assignment - the original has no such gesture, and losing an assignment to a stray
            // release would be a nasty way to find that out.
            int32_t dropOn = cell_at(coord);

            if ((dropOn >= 0) && (dropOn != dragFrom)) {
                move_assignment((uint32_t)dragFrom, (uint32_t)dropOn);
            }
        } else {
            bool handled = false;

            if (within_rectangle(coord, gParamOverview.patchButton)) {
                gParamOverview.showGlobal = false;
                handled                   = true;
            } else if (within_rectangle(coord, gParamOverview.globalButton)) {
                gParamOverview.showGlobal = true;
                handled                   = true;
            } else if (within_rectangle(coord, gParamOverview.midiButton)) {
                gParamOverview.showMidi = !gParamOverview.showMidi;
                handled                 = true;
            } else if (within_rectangle(coord, gParamOverview.assignMidiButton)) {
                midi_cc_assign_all_knobs(gParamOverview.slot);
                handled = true;
            } else if (within_rectangle(coord, gParamOverview.clearMidiButton)) {
                midi_cc_clear_all(gParamOverview.slot);
                handled = true;
            }

            for (uint32_t s = 0; (s < MAX_SLOTS) && !handled; s++) {
                if (within_rectangle(coord, gParamOverview.slotButton[s])) {
                    gParamOverview.slot = s;
                    handled             = true;
                }
            }
        }
    }

    // Right-click a box for the parameter's own context menu — the same one the canvas and the
    // Parameter Pages panel offer, so a knob can be reassigned or cleared from the overview.
    if (mouseButton == mouseButtonRightUp) {
        if (panel_context_menu_click(coord)) {
            return true;
        }
        int32_t index = cell_at(coord);

        if (index >= 0) {
            tKnobTarget target = param_pages_knob_target(gParamOverview.showGlobal, gParamOverview.slot, (uint32_t)index);

            if (target.assigned) {
                open_param_context_menu(coord, target.key, target.paramIndex);
            }
        }
    }
    synthlib_request_redraw();
    return true;
}

// ─── Keyboard ────────────────────────────────────────────────────────────────

bool handle_param_overview_key(int key, int mods, int action) {
    (void)mods;

    if (!gParamOverview.active || (action != GLFW_PRESS)) {
        return false;
    }

    if (key != GLFW_KEY_ESCAPE) {
        return false;
    }
    close_param_overview_panel();
    synthlib_request_redraw();
    return true;
}

#ifdef __cplusplus
}
#endif
