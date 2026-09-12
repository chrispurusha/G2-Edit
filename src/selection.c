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
// Notes: Docs/code-notes/selection.c.md - "// notes §k" refers there.

#include <string.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "msgQueue.h"
#include "moduleResourcesAccess.h"
#include "protocol.h"
#include "mouseHandle.h"
#include "menus.h"
#include "selection.h"
#include "splitView.h"
#include "utilsGraphics.h"
#include "undo.h"
#include "cableChain.h"

bool is_selected(tModuleKey key) {
    for (uint32_t i = 0; i < gSelection.count; i++) {
        tModuleKey k = gSelection.keys[i];

        if (k.slot == key.slot && k.location == key.location && k.index == key.index) {
            return true;
        }
    }

    return false;
}

static void selection_remove(tModuleKey key);

// Is this Location currently displayed by one of the panes? In a split view both are, and in a
// single-pane view only the focused one is.
static bool location_on_screen(uint32_t location) {
    for (uint32_t pane = 0; pane < module_pane_count(); pane++) {
        if ((uint32_t)split_view_location_for_pane(pane) == location) {
            return true;
        }
    }

    return false;
}

void selection_clear(void) {
    memset(&gSelection, 0, sizeof(gSelection));
}

// notes §1
void selection_validate(void) {
    static bool     init         = false;
    static uint32_t lastSlot     = 0;
    static uint32_t lastLocation = 0;
    static uint32_t lastGen[MAX_SLOTS];

    uint32_t        slot         = (uint32_t)gSlot;
    uint32_t        location     = (uint32_t)gLocation;
    bool            stale        = false;

    if (init == false) {
        init         = true;
        lastSlot     = slot;
        lastLocation = location;

        for (uint32_t i = 0; i < MAX_SLOTS; i++) {
            lastGen[i] = gPatchGeneration[i];
        }

        return;
    }

    // A SLOT CHANGE STILL INVALIDATES EVERYTHING — the canvas only ever shows one slot, so every key
    // held refers to modules that are no longer on screen.
    if (slot != lastSlot) {
        stale    = true;
        lastSlot = slot;
    }
    lastLocation = location;

    // notes §2
    for (uint32_t i = gSelection.count; i > 0; i--) {
        if (!location_on_screen(gSelection.keys[i - 1].location)) {
            selection_remove(gSelection.keys[i - 1]);
            synthlib_request_redraw();
        }
    }

    // ANY slot's generation, not just the visible one: a selection can only hold keys for one slot at
    // a time, but a load into a background slot still invalidates a selection held there.
    for (uint32_t i = 0; i < MAX_SLOTS; i++) {
        uint32_t gen = gPatchGeneration[i];

        if (gen != lastGen[i]) {
            lastGen[i] = gen;
            stale      = true;
        }
    }

    if ((stale == true) && (gSelection.count > 0)) {
        selection_clear();
        synthlib_request_redraw();
    }
}

static void selection_add(tModuleKey key) {
    if (gSelection.count >= MAX_NUM_MODULES) {
        return;
    }

    if (is_selected(key)) {
        return;
    }
    gSelection.keys[gSelection.count++] = key;
}

static void selection_remove(tModuleKey key) {
    for (uint32_t i = 0; i < gSelection.count; i++) {
        tModuleKey k = gSelection.keys[i];

        if (k.slot == key.slot && k.location == key.location && k.index == key.index) {
            gSelection.keys[i] = gSelection.keys[--gSelection.count];
            return;
        }
    }
}

void selection_set_single(tModuleKey key) {
    selection_clear();
    selection_add(key);
}

// notes §3
void selection_select_all(void) {
    uint32_t slot     = (uint32_t)gSlot;
    uint32_t location = (uint32_t)gLocation;

    selection_clear();

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if ((module != NULL) && module->active) {
            selection_add((tModuleKey){slot, location, i});
        }
    }

    synthlib_request_redraw();
}

void selection_toggle(tModuleKey key) {
    if (is_selected(key)) {
        selection_remove(key);
    } else {
        selection_add(key);
    }
}

// Add all active modules in the given slot/location whose rectangle overlaps rect.
// rect and module positions are both in module-space (pre-zoom, pre-scroll).
// mod->rectangle is screen-space and is NOT used here.
void selection_add_rect(tRectangle rect, uint32_t slot, uint32_t location) {
    double r1x2 = rect.coord.x + rect.size.w;
    double r1y2 = rect.coord.y + rect.size.h;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * mod      = get_module_slot(slot, location, i);

        if (!mod->active) {
            continue;
        }
        double    h        = (double)gModuleProperties[mod->type].height;
        double    mx       = mod->column * MODULE_X_SPAN;
        double    my       = mod->row * MODULE_Y_SPAN;
        double    mw       = MODULE_WIDTH;
        double    mh       = (h * MODULE_Y_SPAN) - MODULE_Y_GAP;
        bool      overlaps = rect.coord.x < mx + mw && r1x2 > mx
                             && rect.coord.y < my + mh && r1y2 > my;

        if (overlaps) {
            selection_add(mod->key);
        }
    }
}

// Send cables-then-module delete to the G2 and remove from local DB.
void delete_module_and_cables(tModuleKey key) {
    uint32_t        slot          = key.slot;
    uint32_t        location      = key.location;
    tMessageContent msg           = {0};
    tCableNode      orphaned[MAX_NUM_CABLES];
    uint32_t        orphanedCount = 0;

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if (cable == NULL || !cable->active) {
            continue;
        }

        if (cable->key.moduleFromIndex != key.index && cable->key.moduleToIndex != key.index) {
            continue;
        }
        msg                                = (tMessageContent){
            0
        };
        msg.cmd                            = eMsgCmdDeleteCable;
        msg.slot                           = slot;
        msg.cableData.location             = location;
        msg.cableData.moduleFromIndex      = cable->key.moduleFromIndex;
        msg.cableData.connectorFromIoIndex = cable->key.connectorFromIoCount;
        msg.cableData.moduleToIndex        = cable->key.moduleToIndex;
        msg.cableData.connectorToIoIndex   = cable->key.connectorToIoCount;
        msg.cableData.linkType             = cable->key.linkType;

        // Anything this module was FEEDING may be left without a source. Its own inputs go with
        // it, so only the far ends matter. Recoloured below, once the cables are actually gone.
        if ((cable->key.moduleToIndex != key.index) && (orphanedCount < MAX_NUM_CABLES)) {
            orphaned[orphanedCount++] = cable_chain_to_node(cable);
        }
        msg_send(&gToUsbThread, &msg);
        delete_cable(cable->key);
    }

    // Deleting a module is a topology change, so the chains it fed have to be re-derived or we
    // leave them coloured but sourceless — a state the original editor cannot represent (see
    // cable_chain_recolour()). Cheap and idempotent, so over-calling it is harmless.
    for (uint32_t i = 0; i < orphanedCount; i++) {
        cable_chain_recolour(slot, location, orphaned[i]);
    }

    // Knob, global knob and MIDI CC assignments naming this module go with it. Before the delete, so
    // the entries can still be read back and the module they point at still exists — remove_controller
    // _entry() clears a shadow flag on the module's own parameter.
    clear_assignments_for_module(key);

    // notes §4
    if (  gParamFocus.valid
       && (gParamFocus.moduleKey.slot == key.slot)
       && (gParamFocus.moduleKey.location == key.location)
       && (gParamFocus.moduleKey.index == key.index)) {
        gParamFocus.valid = false;
    }
    msg                      = (tMessageContent){
        0
    };
    msg.cmd                  = eMsgCmdDeleteModule;
    msg.slot                 = slot;
    msg.moduleData.moduleKey = key;
    msg_send(&gToUsbThread, &msg);
    delete_module(key);
}

void delete_selection(void) {
    for (uint32_t i = 0; i < gSelection.count; i++) {
        delete_module_and_cables(gSelection.keys[i]);
    }

    selection_clear();
}

// notes §5
static uint32_t shift_fit_row(uint32_t slot, uint32_t location, uint32_t index,
                              uint32_t column, uint32_t row, uint32_t height,
                              bool selectionTransparent, bool * fits) {
    *fits = true;

    // notes §6
    if (height > MAX_ROWS) {
        *fits = false;             // taller than the whole grid; there is nowhere it could go
        return 0;
    }

    if ((row + height) > MAX_ROWS) {
        row = MAX_ROWS - height;
    }

    // Bounded rather than while(true): each pass strictly lowers `row`, so MAX_ROWS + 1 passes is
    // more than it can ever need, and a table that somehow reports a zero height cannot spin here.
    for (uint32_t pass = 0; pass <= MAX_ROWS; pass++) {
        uint32_t topOfBlock = 0;
        uint32_t lowestRow  = 0;
        bool     hit        = false;

        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * walk = get_module_slot(slot, location, i);

            if (  !walk->active || (walk->key.index == index)
               || (selectionTransparent && is_selected(walk->key))
               || (walk->column != column)) {
                continue;
            }

            if ((walk->row >= row) && (walk->row < row + height)) {
                if (!hit || (walk->row < topOfBlock)) {
                    topOfBlock = walk->row;
                }
                hit = true;
            }
        }

        if (!hit) {
            return row;    // Nothing in the way at all.
        }

        // Everything at or below the block's top travels together - the drop loops move exactly that
        // set - so the deepest of them is what decides whether the push fits.
        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * walk = get_module_slot(slot, location, i);

            if (  !walk->active || (walk->key.index == index)
               || (selectionTransparent && is_selected(walk->key))
               || (walk->column != column)) {
                continue;
            }

            if ((walk->row >= topOfBlock) && (walk->row > lowestRow)) {
                lowestRow = walk->row;
            }
        }

        uint32_t dropAmount = (row + height) - topOfBlock;

        if ((lowestRow + dropAmount) <= MAX_ROWS) {
            return row;
        }
        uint32_t excess     = (lowestRow + dropAmount) - MAX_ROWS;

        if (excess >= row) {
            *fits = false;
            return 0;
        }
        row -= excess;
    }

    return row;
}

// Every module in the location back to a recorded set of positions, for a trial placement that has
// to be taken back. Nothing is sent while a trial is running, so a restore is purely local.
static void restore_positions(const tUndoMoveEntry * entries, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        tModule * mod = get_module(entries[i].key);

        if ((mod == NULL) || !mod->active) {
            continue;
        }
        mod->column = entries[i].oldColumn;
        mod->row    = entries[i].oldRow;
    }
}

// notes §7
static uint32_t count_overlapping_pairs(uint32_t slot, uint32_t location) {
    uint32_t pairs = 0;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * a = get_module_slot(slot, location, i);

        if ((a == NULL) || !a->active) {
            continue;
        }

        for (uint32_t j = i + 1; j < MAX_NUM_MODULES; j++) {
            tModule * b = get_module_slot(slot, location, j);

            if ((b == NULL) || !b->active || (b->column != a->column)) {
                continue;
            }

            if (  (a->row < (b->row + gModuleProperties[b->type].height))
               && (b->row < (a->row + gModuleProperties[a->type].height))) {
                pairs++;
            }
        }
    }

    return pairs;
}

// notes §8
static void shift_member_place(uint32_t slot, uint32_t location, tModuleKey key,
                               bool selectionTransparent, uint32_t * shortfall, bool * fits) {
    tModule * module            = get_module(key);

    *shortfall = 0;
    *fits      = true;

    if (module == NULL) {
        return;
    }
    bool      doDrop            = false;
    uint32_t  rowAndBelowToDrop = 0;
    uint32_t  dropAmount        = 0;
    uint32_t  height            = gModuleProperties[module->type].height;
    uint32_t  wantRow           = module->row;
    uint32_t  landingRow        = wantRow;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * walk = get_module_slot(slot, location, i);

        if (  !walk->active || (walk->key.index == key.index)
           || (selectionTransparent && is_selected(walk->key))) {
            continue;
        }

        if ((walk->column == module->column) && (landingRow > walk->row) && (landingRow < walk->row + gModuleProperties[walk->type].height)) {
            uint32_t below = walk->row + gModuleProperties[walk->type].height;

            // notes §9
            if (below > MAX_ROWS) {
                if (walk->row < height) {
                    *fits = false;    // Neither above nor below it fits on the grid.
                    return;
                }
                landingRow = walk->row - height;
            } else {
                landingRow = below;
            }
            break;
        }
    }

    uint32_t fittedRow = shift_fit_row(slot, location, key.index, module->column, landingRow, height, selectionTransparent, fits);

    if (*fits == false) {
        return;
    }
    // notes §10
    *shortfall  = (fittedRow < wantRow) ? (wantRow - fittedRow) : 0;
    module->row = fittedRow;

    // notes §11
    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * walk = get_module_slot(slot, location, i);

        if (  !walk->active || (walk->key.index == key.index)
           || (selectionTransparent && is_selected(walk->key))) {
            continue;
        }

        if ((walk->column == module->column) && (walk->row >= module->row) && (walk->row < module->row + height)) {
            if ((doDrop == false) || (walk->row < rowAndBelowToDrop)) {
                rowAndBelowToDrop = walk->row;
            }
            doDrop = true;
        }
    }

    if (doDrop == true) {
        dropAmount = (module->row + height) - rowAndBelowToDrop;

        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * walk = get_module_slot(slot, location, i);

            if (  !walk->active || (walk->key.index == key.index)
               || (selectionTransparent && is_selected(walk->key))) {
                continue;
            }

            // NO CLAMP TO MAX_ROWS HERE, and there must not be one again: it was what piled modules
            // onto the last row instead of shifting them. shift_fit_row() has already guaranteed this
            // arithmetic stays on the grid.
            if ((walk->column == module->column) && (walk->row >= rowAndBelowToDrop)) {
                walk->row += dropAmount;
            }
        }
    }
}

// notes §12
bool shift_modules_down(tModuleKey key) {
    tModule *      module     = get_module(key);

    if (module == NULL) {
        return false;
    }
    tUndoMoveEntry start[MAX_NUM_MODULES];
    uint32_t       startCount = module_positions_snapshot(key.slot, key.location, start);
    uint32_t       shortfall  = 0;
    bool           fits       = true;

    shift_member_place(key.slot, key.location, key, false, &shortfall, &fits);
    (void)shortfall;    // One module has no group shape to keep, so the raised row is simply taken.

    if (fits == false) {
        restore_positions(start, startCount);
        return false;    // Column full to the bottom - the caller puts the gesture back.
    }
    // Sent only now, and never from inside a trial. The module itself goes unconditionally: whatever
    // moved it here - a drag, a paste, a fresh create - has not told the G2 yet.
    send_module_move_msg(module);

    for (uint32_t i = 0; i < startCount; i++) {
        tModule * mod = get_module(start[i].key);

        if ((mod == NULL) || !mod->active || (mod->key.index == key.index)) {
            continue;
        }

        if ((mod->column != start[i].oldColumn) || (mod->row != start[i].oldRow)) {
            send_module_move_msg(mod);
        }
    }

    return true;
}

// Like shift_modules_down but applied to every selected module. Selected modules are transparent to
// each other — only conflicts with non-selected modules are resolved.
bool shift_selection_down(void) {
    return shift_selection_down_in((uint32_t)gSlot, (uint32_t)gLocation);
}

// notes §13
bool shift_selection_down_in(uint32_t slot, uint32_t location) {
    tUndoMoveEntry start[MAX_NUM_MODULES];
    uint32_t       startCount  = module_positions_snapshot(slot, location, start);
    uint32_t       wasOverlaps = count_overlapping_pairs(slot, location);
    uint32_t       rigidRaise  = 0;

    // Bounded rather than while(true): rigidRaise only ever grows, and a raise past MAX_ROWS is
    // refused below, so this cannot spin.
    for (uint32_t attempt = 0; attempt <= MAX_ROWS; attempt++) {
        restore_positions(start, startCount);

        for (uint32_t si = 0; si < gSelection.count; si++) {
            tModule * member = get_module(gSelection.keys[si]);

            if (member == NULL) {
                continue;
            }

            if (member->row < rigidRaise) {
                restore_positions(start, startCount);
                return false;    // The group would leave the top of the grid - nowhere to put it.
            }
            member->row -= rigidRaise;
        }

        bool     retry = false;
        uint32_t worst = 0;

        for (uint32_t si = 0; si < gSelection.count; si++) {
            uint32_t shortfall = 0;
            bool     fits      = true;

            shift_member_place(slot, location, gSelection.keys[si], true, &shortfall, &fits);

            if (fits == false) {
                restore_positions(start, startCount);
                return false;
            }

            // FAIL FAST on the first member that will not fit. Carrying on would measure the
            // remaining members against a column this one has not pushed yet, and the raise would
            // come out too large - the group would jump further up than it needed to.
            if (shortfall > 0) {
                worst = shortfall;
                retry = true;
                break;
            }
        }

        if (retry) {
            rigidRaise += worst;
            continue;
        }

        // notes §14
        if (count_overlapping_pairs(slot, location) > wasOverlaps) {
            restore_positions(start, startCount);
            return false;
        }

        // Kept, so the G2 can be told. Every selected module is sent whether or not this function
        // moved it — the drag did, and that has not been sent yet — and every other module that
        // ended up somewhere new.
        for (uint32_t si = 0; si < gSelection.count; si++) {
            tModule * member = get_module(gSelection.keys[si]);

            if (member != NULL) {
                send_module_move_msg(member);
            }
        }

        for (uint32_t i = 0; i < startCount; i++) {
            tModule * mod = get_module(start[i].key);

            if ((mod == NULL) || !mod->active || is_selected(mod->key)) {
                continue;
            }

            if ((mod->column != start[i].oldColumn) || (mod->row != start[i].oldRow)) {
                send_module_move_msg(mod);
            }
        }

        return true;
    }

    restore_positions(start, startCount);
    return false;
}

// notes §15
uint32_t module_positions_snapshot(uint32_t slot, uint32_t location, tUndoMoveEntry * out) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * mod = get_module_slot(slot, location, i);

        if ((mod == NULL) || !mod->active) {
            continue;
        }
        out[count].key       = mod->key;
        out[count].oldColumn = mod->column;
        out[count].oldRow    = mod->row;
        out[count].newColumn = mod->column;
        out[count].newRow    = mod->row;
        count++;
    }

    return count;
}

// Compacted in place — the write index never overtakes the read index — leaving only the entries
// whose module actually moved, each carrying both its old and its new position.
uint32_t module_positions_changed(tUndoMoveEntry * entries, uint32_t count) {
    uint32_t moved = 0;

    for (uint32_t i = 0; i < count; i++) {
        tModule * mod = get_module(entries[i].key);

        if ((mod == NULL) || !mod->active) {
            continue;
        }

        if ((mod->column == entries[i].oldColumn) && (mod->row == entries[i].oldRow)) {
            continue;
        }
        entries[moved].key       = entries[i].key;
        entries[moved].oldColumn = entries[i].oldColumn;
        entries[moved].oldRow    = entries[i].oldRow;
        entries[moved].newColumn = mod->column;
        entries[moved].newRow    = mod->row;
        moved++;
    }

    return moved;
}

void copy_selection(void) {
    if (gSelection.count == 0) {
        return;
    }
    uint32_t slot     = (uint32_t)gSlot;
    uint32_t location = (uint32_t)gLocation;

    // Anchor at top-left of bounding box
    uint32_t minCol   = UINT32_MAX;
    uint32_t minRow   = UINT32_MAX;

    for (uint32_t si = 0; si < gSelection.count; si++) {
        tModule * mod = get_module(gSelection.keys[si]);

        if (mod == NULL) {
            continue;
        }

        if (mod->column < minCol) {
            minCol = mod->column;
        }

        if (mod->row < minRow) {
            minRow = mod->row;
        }
    }

    memset(&gClipboard, 0, sizeof(gClipboard));
    gClipboard.location = location;

    for (uint32_t si = 0; si < gSelection.count; si++) {
        tModule *          mod = get_module(gSelection.keys[si]);

        if (mod == NULL) {
            continue;
        }
        tClipboardModule * cm  = &gClipboard.modules[gClipboard.moduleCount++];
        cm->type                = mod->type;
        cm->dColumn             = (int32_t)mod->column - (int32_t)minCol;
        cm->dRow                = (int32_t)mod->row - (int32_t)minRow;
        cm->origIndex           = mod->key.index;
        cm->origColumn          = mod->column;
        cm->origRow             = mod->row;
        cm->colour              = mod->colour;
        cm->upRate              = mod->upRate;
        cm->excludeFromMutation = mod->excludeFromMutation;
        COPY_STRING(cm->name, mod->name);

        for (uint32_t v = 0; v < NUM_VARIATIONS_USB; v++) {
            for (uint32_t p = 0; p < MAX_NUM_PARAMETERS; p++) {
                cm->param[v][p] = mod->param[v][p];
            }
        }

        for (uint32_t m = 0; m < MAX_NUM_MODES; m++) {
            cm->mode[m] = mod->mode[m].value;
        }

        for (uint32_t p = 0; p < MAX_NUM_PARAMETERS; p++) {
            cm->paramNumLabels[p] = mod->paramNumLabels[p];

            for (uint32_t l = 0; l < MAX_NUM_LABELS; l++) {
                cm->paramNameSet[p][l] = mod->paramNameSet[p][l];
                COPY_STRING(cm->paramName[p][l], mod->paramName[p][l]);
            }
        }
    }

    // Only store cables where both endpoints are selected
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable *          cable   = get_cable_slot(slot, location, i);

        if (cable == NULL || !cable->active) {
            continue;
        }
        bool              fromSel = false;
        bool              toSel   = false;

        for (uint32_t si = 0; si < gSelection.count; si++) {
            if (gSelection.keys[si].index == cable->key.moduleFromIndex) {
                fromSel = true;
            }

            if (gSelection.keys[si].index == cable->key.moduleToIndex) {
                toSel = true;
            }
        }

        if (!fromSel || !toSel) {
            continue;
        }
        tClipboardCable * cc      = &gClipboard.cables[gClipboard.cableCount++];
        cc->fromOrigIndex = cable->key.moduleFromIndex;
        cc->fromIoCount   = cable->key.connectorFromIoCount;
        cc->toOrigIndex   = cable->key.moduleToIndex;
        cc->toIoCount     = cable->key.connectorToIoCount;
        cc->linkType      = cable->key.linkType;
        cc->colour        = cable->colour;
    }

    gClipboard.active = true;
}

void cut_selection(void) {
    copy_selection();
    undo_push_delete_selection();
    delete_selection();
    update_module_up_rates();
    synthlib_request_redraw();
}

void paste_snapshot(uint32_t slot, uint32_t location,
                    uint32_t anchorCol, uint32_t anchorRow,
                    tClipboardModule * modules, uint32_t moduleCount,
                    tClipboardCable * cables, uint32_t cableCount) {
    uint32_t indexMap[MAX_NUM_MODULES] = {0};

    selection_clear();

    for (uint32_t ci = 0; ci < moduleCount; ci++) {
        tClipboardModule * cm       = &modules[ci];
        int32_t            newIndex = find_unique_module_id(location);

        if (newIndex < 0) {
            continue;
        }
        tModule            module   = {0};
        module.key.slot                    = slot;
        module.key.location                = location;
        module.key.index                   = (uint32_t)newIndex;
        module.type                        = cm->type;

        int32_t            nc       = (int32_t)anchorCol + cm->dColumn;
        int32_t            nr       = (int32_t)anchorRow + cm->dRow;
        module.column                      = (uint32_t)(nc < 0 ? 0 : (nc > (int32_t)MAX_COLUMNS ? MAX_COLUMNS : nc));
        module.row                         = (uint32_t)(nr < 0 ? 0 : (nr > (int32_t)MAX_ROWS ? MAX_ROWS : nr));
        module.colour                      = cm->colour;
        module.upRate                      = cm->upRate;
        module.excludeFromMutation         = cm->excludeFromMutation;
        COPY_STRING(module.name, cm->name);

        tMessageContent    msg      = {0};
        msg.cmd                            = eMsgCmdWriteModule;
        msg.slot                           = slot;
        msg.moduleData.moduleKey           = module.key;
        msg.moduleData.type                = module.type;
        msg.moduleData.row                 = module.row;
        msg.moduleData.column              = module.column;
        msg.moduleData.colour              = module.colour;
        msg.moduleData.upRate              = module.upRate;
        msg.moduleData.excludeFromMutation = module.excludeFromMutation;
        msg.moduleData.modeCount           = module_mode_count(module.type);

        for (uint32_t m = 0; m < MAX_NUM_MODES; m++) {
            msg.moduleData.mode[m] = (uint8_t)cm->mode[m];
        }

        COPY_STRING(msg.moduleData.name, cm->name);
        msg_send(&gToUsbThread, &msg);

        write_module(module.key, &module);

        tModule * dbMod = get_module(module.key);

        if (dbMod != NULL) {
            for (uint32_t v = 0; v < NUM_VARIATIONS_USB; v++) {
                for (uint32_t p = 0; p < module_param_count(module.type); p++) {
                    dbMod->param[v][p] = cm->param[v][p];
                    send_param_value(slot, module.key, p, v, cm->param[v][p].value);
                }
            }

            for (uint32_t m = 0; m < module_mode_count(module.type); m++) {
                dbMod->mode[m].value = cm->mode[m];
                send_mode_value(slot, module.key, m, cm->mode[m]);
            }

            for (uint32_t p = 0; p < module_param_count(module.type); p++) {
                dbMod->paramNumLabels[p] = cm->paramNumLabels[p];

                for (uint32_t l = 0; l < MAX_NUM_LABELS; l++) {
                    dbMod->paramNameSet[p][l] = cm->paramNameSet[p][l];
                    COPY_STRING(dbMod->paramName[p][l], cm->paramName[p][l]);

                    if (cm->paramNameSet[p][l]) {
                        tMessageContent nmsg = {0};
                        nmsg.cmd                       = eMsgCmdSetParamLabel;
                        nmsg.slot                      = slot;
                        nmsg.paramLabelData.moduleKey  = module.key;
                        nmsg.paramLabelData.paramIndex = p;
                        COPY_STRING(nmsg.paramLabelData.name, cm->paramName[p][l]);
                        msg_send(&gToUsbThread, &nmsg);
                    }
                }
            }
        }

        if (cm->origIndex < MAX_NUM_MODULES) {
            indexMap[cm->origIndex] = (uint32_t)newIndex;
        }
        selection_add(module.key);
    }

    // notes §16
    (void)shift_selection_down_in(slot, location);

    // Reconnect internal cables using remapped indices
    for (uint32_t ci = 0; ci < cableCount; ci++) {
        tClipboardCable * cc       = &cables[ci];
        uint32_t          newFrom  = indexMap[cc->fromOrigIndex];
        uint32_t          newTo    = indexMap[cc->toOrigIndex];

        tCableKey         cableKey = {0};
        cableKey.slot                      = slot;
        cableKey.location                  = location;
        cableKey.moduleFromIndex           = newFrom;
        cableKey.connectorFromIoCount      = cc->fromIoCount;
        cableKey.linkType                  = cc->linkType;
        cableKey.moduleToIndex             = newTo;
        cableKey.connectorToIoCount        = cc->toIoCount;

        tCable            cable    = {0};
        cable.key                          = cableKey;
        cable.colour                       = cc->colour;
        cable.active                       = true;
        write_cable(cableKey, &cable);

        tMessageContent   msg      = {0};
        msg.cmd                            = eMsgCmdWriteCable;
        msg.slot                           = slot;
        msg.cableData.location             = location;
        msg.cableData.colour               = cc->colour;
        msg.cableData.moduleFromIndex      = newFrom;
        msg.cableData.connectorFromIoIndex = cc->fromIoCount;
        msg.cableData.linkType             = cc->linkType;
        msg.cableData.moduleToIndex        = newTo;
        msg.cableData.connectorToIoIndex   = cc->toIoCount;
        msg_send(&gToUsbThread, &msg);
    }

    update_module_up_rates();
    synthlib_request_redraw();
}

void paste_clipboard(void) {
    if (!gClipboard.active || gClipboard.moduleCount == 0) {
        return;
    }
    uint32_t       slot           = (uint32_t)gSlot;
    uint32_t       location       = (uint32_t)gLocation;
    uint32_t       anchorCol      = 0;
    uint32_t       anchorRow      = 0;
    tCoord         mouseCoord     = {0};

    get_global_gui_scaled_mouse_coord(&mouseCoord);
    convert_mouse_coord_to_module_column_row(&anchorCol, &anchorRow, mouseCoord);

    // Modules already here may be pushed down to make room. Their positions are recorded before and
    // after so ONE Undo puts them back as well as removing what was pasted — a paste that shoved half
    // a column downwards and then only half-undid it would be worse than not shifting at all.
    tUndoMoveEntry displaced[MAX_NUM_MODULES];
    uint32_t       displacedCount = module_positions_snapshot(slot, location, displaced);

    paste_snapshot(slot, location, anchorCol, anchorRow,
                   gClipboard.modules, gClipboard.moduleCount,
                   gClipboard.cables, gClipboard.cableCount);

    uint32_t       movedCount     = module_positions_changed(displaced, displacedCount);

    undo_push_paste(slot, location, anchorCol, anchorRow,
                    gSelection.keys, gSelection.count,
                    gClipboard.modules, gClipboard.moduleCount,
                    gClipboard.cables, gClipboard.cableCount,
                    displaced, movedCount);
}
