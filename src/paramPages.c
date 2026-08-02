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

#include "paramPages.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "dataBase.h"
#include "globalVars.h"
#include "graphics.h"
#include "menus.h"
#include "moduleGraphics.h"
#include "moduleResourcesAccess.h"
#include "mouseHandle.h"
#include "protocol.h"
#include "renderParams.h"
#include "undo.h"
#include "utilsGraphics.h"
#include "contextMenu.h"

tParamPagesEdit          gParamPages                    = {0};

static const char *const kSlotLabel[MAX_SLOTS]          = {"A", "B", "C", "D"};
static const char *const kPageRowLabel[NUM_PARAM_PAGES] = {"A", "B", "C", "D", "E"};

#define PP_CELL_GAP        6.0
#define PP_DIAL_SIZE       26.0
#define PP_PAGE_BTN_W      30.0
#define PP_PAGE_BTN_GAP    4.0

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void open_param_pages_panel(uint32_t slot) {
    // Deliberately not a memset: reopening the panel keeps whichever page was last being worked
    // on, which is what makes it usable as a scratch surface alongside the canvas.
    gParamPages.active       = true;
    gParamPages.slot         = slot;
    gParamPages.closePressed = false;
}

void close_param_pages_panel(void) {
    gParamPages.active = false;
}

// ─── Knob -> parameter resolution ────────────────────────────────────────────

typedef struct {
    bool       assigned;
    tModuleKey key;
    uint32_t   paramIndex;
    uint32_t   paramRef;
    tModule *  module;
} tKnobTarget;

static uint32_t knob_index(uint32_t page, uint32_t bank, uint32_t pos) {
    return (((page * NUM_BANKS_PER_PAGE) + bank) * NUM_KNOBS_PER_BANK) + pos;
}

// A knob assignment names a param by INDEX within its module; the widget to draw is described by
// the matching entry in paramLocationList, which is keyed by module TYPE. Walk the list counting
// entries for this module's type - the paramIndex'th match is the one, the same correspondence
// render_module_common() relies on when it renders a module's params in list order.
// module->param[][].paramRef caches this, but only once the module has been drawn at least once,
// and a knob can point at a module in the Slot or Location that isn't currently on screen.
static bool param_ref_for_index(tModule * module, uint32_t paramIndex, uint32_t * outRef) {
    uint32_t listSize = array_size_param_location_list();
    uint32_t count    = 0;

    for (uint32_t i = module->paramIndexCache; i < listSize; i++) {
        if (paramLocationList[i].moduleType == module->type) {
            if (count == paramIndex) {
                *outRef = i;
                return true;
            }
            count++;
        }
    }

    return false;
}

// Resolves one of the eight knob positions on the page currently being shown. Returns a target
// with assigned == false for an empty position, and equally for an assignment that no longer
// resolves - the module deleted since, or a param index the module type doesn't have.
static tKnobTarget knob_target(uint32_t pos) {
    tKnobTarget target = {0};
    uint32_t    index  = knob_index(gParamPages.page, gParamPages.bank, pos);

    if (gParamPages.showGlobal) {
        const tGlobalKnob * knob = &gGlobalKnobArray[index];

        if (!knob->assigned) {
            return target;
        }
        target.key        = (tModuleKey){
            knob->slotIndex, knob->location, knob->moduleIndex
        };
        target.paramIndex = knob->paramIndex;
    } else {
        const tKnob * knob = &gKnobArray[gParamPages.slot].knob[index];

        if (!knob->assigned) {
            return target;
        }
        target.key        = (tModuleKey){
            gParamPages.slot, knob->location, knob->moduleIndex
        };
        target.paramIndex = knob->paramIndex;
    }
    target.module   = get_module(target.key);

    if ((target.module == NULL) || !target.module->active) {
        return target;
    }

    if (target.paramIndex >= MAX_NUM_PARAMETERS) {
        return target;
    }

    if (!param_ref_for_index(target.module, target.paramIndex, &target.paramRef)) {
        return target;
    }
    target.assigned = true;
    return target;
}

static bool page_has_any_assignment(uint32_t page, uint32_t bank) {
    for (uint32_t pos = 0; pos < NUM_KNOBS_PER_BANK; pos++) {
        uint32_t index = knob_index(page, bank, pos);

        if (gParamPages.showGlobal) {
            if (gGlobalKnobArray[index].assigned) {
                return true;
            }
        } else if (gKnobArray[gParamPages.slot].knob[index].assigned) {
            return true;
        }
    }

    return false;
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

static const char * module_display_name(const tModule * module) {
    if (module->name[0] != '\0') {
        return module->name;
    }
    return gModuleProperties[module->type].name;
}

void render_param_pages_panel(void) {
    if (!gParamPages.active) {
        return;
    }
    double renderW = get_render_width() / gGlobalGuiScale;
    double renderH = get_render_height() / gGlobalGuiScale;
    double margin  = 10.0;
    double titleH  = 24.0;
    double rowH    = 20.0;
    double btnH    = STANDARD_BUTTON_TEXT_HEIGHT;
    double textH   = STANDARD_TEXT_HEIGHT;
    // Cell height: module name, then the param widget's own label + value + dial (the three
    // rows render_dial_with_text() lays out from the rectangle it's handed).
    double cellH   = textH + (textH * 2.0) + PP_DIAL_SIZE + 8.0;
    double gridW   = (PP_PAGE_BTN_W * NUM_BANKS_PER_PAGE) + (PP_PAGE_BTN_GAP * (NUM_BANKS_PER_PAGE - 1));
    double boxW    = 780.0;

    if (boxW > (renderW - (margin * 2.0))) {
        boxW = renderW - (margin * 2.0);
    }
    double cellsW  = boxW - (margin * 3.0) - gridW;
    double cellW   = (cellsW - (PP_CELL_GAP * (NUM_KNOBS_PER_BANK - 1))) / NUM_KNOBS_PER_BANK;
    double boxH    = titleH + margin + rowH + margin + cellH + margin;
    double gridH   = (rowH * NUM_PARAM_PAGES) + (PP_PAGE_BTN_GAP * (NUM_PARAM_PAGES - 1));

    if (boxH < (titleH + margin + rowH + margin + gridH + margin)) {
        boxH = titleH + margin + rowH + margin + gridH + margin;
    }
    double boxX    = (renderW - boxW) / 2.0;
    double boxY    = (renderH - boxH) / 2.0;
    double y       = boxY + titleH + margin;

    draw_dialog_background_overlay();
    draw_panel_chrome((tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH, "Parameter Pages");
    gParamPages.close = draw_panel_close_button((tRectangle){{boxX, boxY}, {boxW, boxH}}, gParamPages.closePressed);

    // ── Slot buttons in the title bar ──────────────────────────────────
    // Only for the patch pages: a global assignment carries its own Slot per knob, so there's
    // nothing for a panel-wide Slot selector to mean there.
    {
        double slotBtnW = get_text_width((char *)"A", btnH, eCache);
        double slotX    = boxX + boxW - 70.0 - ((slotBtnW + 8.0) * MAX_SLOTS);

        for (uint32_t s = 0; s < MAX_SLOTS; s++) {
            if (gParamPages.showGlobal) {
                gParamPages.slotButton[s] = (tRectangle){
                    0
                };
                continue;
            }
            tRgb colour = (s == gParamPages.slot) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;

            gParamPages.slotButton[s] = draw_button(mainArea,
                                                    (tRectangle){{slotX + (s * (slotBtnW + 8.0)), boxY + 4.0}, {slotBtnW, btnH}},
                                                    kSlotLabel[s], colour);
        }
    }

    // ── Patch / Global toggle ──────────────────────────────────────────
    {
        double patchW  = get_text_width((char *)"Patch", btnH, eCache) + 14.0;
        double globalW = get_text_width((char *)"Global", btnH, eCache) + 14.0;

        gParamPages.patchButton  = draw_button(mainArea, (tRectangle){{boxX + margin, y}, {patchW, btnH}},
                                               "Patch", gParamPages.showGlobal ? (tRgb)RGB_BACKGROUND_GREY : (tRgb)RGB_GREEN_ON);
        gParamPages.globalButton = draw_button(mainArea, (tRectangle){{boxX + margin + patchW + 6.0, y}, {globalW, btnH}},
                                               "Global", gParamPages.showGlobal ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }
    y += rowH + margin;

    // ── Page matrix: 3 banks across, 5 rows down, named A1..E3 ─────────
    {
        double gridX = boxX + margin;

        for (uint32_t page = 0; page < NUM_PARAM_PAGES; page++) {
            for (uint32_t bank = 0; bank < NUM_BANKS_PER_PAGE; bank++) {
                char   label[8] = {0};
                double bx       = gridX + (bank * (PP_PAGE_BTN_W + PP_PAGE_BTN_GAP));
                double by       = y + (page * (rowH + PP_PAGE_BTN_GAP));
                tRgb   colour   = (tRgb)RGB_GREY_5;

                if ((page == gParamPages.page) && (bank == gParamPages.bank)) {
                    colour = (tRgb)RGB_GREEN_ON;
                } else if (page_has_any_assignment(page, bank)) {
                    colour = (tRgb)RGB_BACKGROUND_GREY;
                }
                snprintf(label, sizeof(label), "%s%u", kPageRowLabel[page], bank + 1);
                gParamPages.pageButton[page][bank] = draw_button(mainArea, (tRectangle){{bx, by}, {PP_PAGE_BTN_W, btnH}}, label, colour);
            }
        }
    }

    // ── The eight knobs of the selected page ───────────────────────────
    {
        double cellX = boxX + margin + gridW + margin;

        // The param widgets are the very same ones the patch canvas draws, so they have to be
        // told to render into mainArea for the length of this loop - their default is the
        // scrolled, zoomed moduleArea.
        set_param_render_area(mainArea);

        for (uint32_t pos = 0; pos < NUM_KNOBS_PER_BANK; pos++) {
            tKnobTarget target                      = knob_target(pos);
            double      x                           = cellX + (pos * (cellW + PP_CELL_GAP));
            tRectangle  cell                        = {{x, y}, {cellW, cellH}};
            char        label[CLAVIA_NAME_SIZE + 8] = {0};

            gParamPages.knobCell[pos]   = cell;
            gParamPages.knobWidget[pos] = (tRectangle){
                0
            };

            set_rgb_colour((tRgb)RGB_GREY_9);
            render_rectangle(mainArea, cell);

            if (!target.assigned) {
                set_rgb_colour((tRgb)RGB_GREY_5);
                snprintf(label, sizeof(label), "%u  --", pos + 1);
                render_text(mainArea, (tRectangle){{x + 3.0, y + 2.0}, {BLANK_SIZE, textH}}, label);
                continue;
            }
            // Header line: the knob's position on the page, then the module it drives. A global
            // page's knobs can be spread across all four Slots, so those are named "1 A:OscB".
            {
                char name[CLAVIA_NAME_SIZE + 8] = {0};

                if (gParamPages.showGlobal) {
                    snprintf(name, sizeof(name), "%u %s:%s", pos + 1, kSlotLabel[target.key.slot], module_display_name(target.module));
                } else {
                    snprintf(name, sizeof(name), "%u %s", pos + 1, module_display_name(target.module));
                }
                fit_text(label, sizeof(label), name, cellW - 6.0, textH);
                set_rgb_colour((tRgb)RGB_BLACK);
                render_text(mainArea, (tRectangle){{x + 3.0, y + 2.0}, {BLANK_SIZE, textH}}, label);
            }

            // The param widget itself, drawn by exactly the code the canvas uses - so a dial
            // looks like a dial, a toggle looks like a toggle, and the value text is formatted
            // by that param type's own rule. render_param_common() records the widget's
            // clickable rect in gParamRectangle; read it straight back for hit-testing, so
            // there's only ever one description of where the control is.
            render_param_common((tRectangle){{x + ((cellW - PP_DIAL_SIZE) / 2.0), y + textH + 4.0}, {PP_DIAL_SIZE, PP_DIAL_SIZE}},
                                target.module, target.paramRef, target.paramIndex);
            gParamPages.knobWidget[pos] = gParamRectangle[target.key.slot][target.key.location][target.key.index][target.paramIndex];
        }

        set_param_render_area(moduleArea);
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

// Same modal precedence the other panels use (mousePanels.c): while one of this panel's own
// dropdowns is open, the click belongs to the menu, not to whatever is behind it.
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

static bool param_type_is_dragged(tParamType paramType) {
    return paramType != paramTypeToggle && paramType != paramTypeMenu
           && paramType != paramTypeBypass && paramType != paramTypeEnable
           && paramType != paramTypePush && paramType != paramTypeCustomData;
}

// Mouse-down on a knob's widget. Continuous params start a drag through the very same
// gParamDragging machinery the canvas uses, so cursor_pos() does the tracking and the value
// send; the discrete ones are handled on release instead, as they are on the canvas.
static bool press_knob_widget(tCoord coord) {
    for (uint32_t pos = 0; pos < NUM_KNOBS_PER_BANK; pos++) {
        tKnobTarget target    = {0};

        if (!within_rectangle(coord, gParamPages.knobWidget[pos])) {
            continue;
        }
        target                         = knob_target(pos);

        if (!target.assigned) {
            return false;
        }
        tParamType  paramType = paramLocationList[target.paramRef].type;

        if (!param_type_is_dragged(paramType)) {
            return true;   // consumed; acted on at mouse-up
        }
        uint32_t    variation = gPatchDescr[target.key.slot].activeVariation;

        gParamDragging.moduleKey       = target.key;
        gParamDragging.type3           = paramType3Param;
        gParamDragging.param           = target.paramIndex;
        gParamDragging.startValue      = target.module->param[variation][target.paramIndex].value;
        gParamDragging.startMorphRange = target.module->param[variation][target.paramIndex].morphRange[gMorphGroupFocus];
        gParamDragging.active          = true;

        if ((synthlib_dial_mode() != eDialModeRotary) || (paramType == paramTypeSlider)) {
            start_cursor_drag();
        }
        return true;
    }

    return false;
}

// Mouse-up on a knob's widget, for the param types that aren't dragged: toggles cycle to their
// next value, menus open their picker. Mirrors handle_module_release_for_module().
static bool release_knob_widget(tCoord coord) {
    for (uint32_t pos = 0; pos < NUM_KNOBS_PER_BANK; pos++) {
        tKnobTarget target    = {0};

        if (!within_rectangle(coord, gParamPages.knobWidget[pos])) {
            continue;
        }
        target = knob_target(pos);

        if (!target.assigned) {
            return false;
        }
        tParamType  paramType = paramLocationList[target.paramRef].type;
        uint32_t    variation = gPatchDescr[target.key.slot].activeVariation;
        tParam *    param     = &target.module->param[variation][target.paramIndex];

        if ((paramType == paramTypeMenu) || (paramType == paramTypeCustomData)) {
            open_toggle_menu(coord, target.key, target.paramIndex, target.paramRef);
            return true;
        }

        if ((paramType == paramTypeToggle) || (paramType == paramTypeBypass) || (paramType == paramTypeEnable)) {
            uint32_t range    = paramLocationList[target.paramRef].range;
            uint32_t oldValue = param->value;

            param->value = (param->value + 1) % range;
            send_param_value(target.key.slot, target.key, target.paramIndex, variation, param->value);
            undo_push_param_change(target.key, target.paramIndex, variation, oldValue, param->value);
            return true;
        }
        return true;
    }

    return false;
}

bool handle_param_pages_mouse(tCoord coord, tMouseButton mouseButton) {
    if (!gParamPages.active) {
        return false;
    }

    if (mouseButton == mouseButtonLeftDown) {
        if (gContextMenu.active) {
            return true;   // the open dropdown gets the whole click, down and up
        }

        if (within_rectangle(coord, gParamPages.close)) {
            gParamPages.closePressed = true;
        } else {
            press_knob_widget(coord);
        }
    }

    if (mouseButton == mouseButtonLeftUp) {
        bool wasClosePressed = gParamPages.closePressed;
        bool wasDragging     = gParamDragging.active;

        gParamPages.closePressed = false;

        if (panel_context_menu_click(coord)) {
            return true;
        }
        // The canvas's own mouse-up handler never runs while this panel is up, so the drag has
        // to be closed out here or it would leave the cursor hidden and the edit un-undoable.
        finish_param_drag();

        if (wasDragging) {
            synthlib_request_redraw();
            return true;
        }

        if (wasClosePressed && within_rectangle(coord, gParamPages.close)) {
            close_param_pages_panel();
        } else if (!release_knob_widget(coord)) {
            bool handled = false;

            if (within_rectangle(coord, gParamPages.patchButton)) {
                gParamPages.showGlobal = false;
                handled                = true;
            } else if (within_rectangle(coord, gParamPages.globalButton)) {
                gParamPages.showGlobal = true;
                handled                = true;
            }

            for (uint32_t s = 0; (s < MAX_SLOTS) && !handled; s++) {
                if (within_rectangle(coord, gParamPages.slotButton[s])) {
                    gParamPages.slot = s;
                    handled          = true;
                }
            }

            for (uint32_t page = 0; (page < NUM_PARAM_PAGES) && !handled; page++) {
                for (uint32_t bank = 0; (bank < NUM_BANKS_PER_PAGE) && !handled; bank++) {
                    if (within_rectangle(coord, gParamPages.pageButton[page][bank])) {
                        gParamPages.page = page;
                        gParamPages.bank = bank;
                        handled          = true;
                    }
                }
            }
        }
    }

    // Right-click a knob for the param's own context menu - the same one the canvas offers, so
    // the knob can be reassigned or cleared from the page it's shown on.
    if (mouseButton == mouseButtonRightUp) {
        if (panel_context_menu_click(coord)) {
            return true;
        }

        for (uint32_t pos = 0; pos < NUM_KNOBS_PER_BANK; pos++) {
            if (within_rectangle(coord, gParamPages.knobCell[pos])) {
                tKnobTarget target = knob_target(pos);

                if (target.assigned) {
                    open_param_context_menu(coord, target.key, target.paramIndex);
                }
                break;
            }
        }
    }
    synthlib_request_redraw();
    return true;
}

// ─── Keyboard ────────────────────────────────────────────────────────────────

bool handle_param_pages_key(int key, int mods, int action) {
    (void)mods;

    if (!gParamPages.active || (action != GLFW_PRESS)) {
        return false;
    }

    switch (key) {
        case GLFW_KEY_ESCAPE:
            close_param_pages_panel();
            break;

        case GLFW_KEY_LEFT:

            if (gParamPages.bank > 0) {
                gParamPages.bank--;
            }
            break;

        case GLFW_KEY_RIGHT:

            if (gParamPages.bank < (NUM_BANKS_PER_PAGE - 1)) {
                gParamPages.bank++;
            }
            break;

        case GLFW_KEY_UP:

            if (gParamPages.page > 0) {
                gParamPages.page--;
            }
            break;

        case GLFW_KEY_DOWN:

            if (gParamPages.page < (NUM_PARAM_PAGES - 1)) {
                gParamPages.page++;
            }
            break;

        default:
            return false;
    }
    synthlib_request_redraw();
    return true;
}

#ifdef __cplusplus
}
#endif
