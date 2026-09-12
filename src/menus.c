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
// Notes: Docs/code-notes/menus.c.md - "// notes §k" refers there.

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <GLFW/glfw3.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "utils.h"
#include "utilsGraphics.h"
#include "msgQueue.h"
#include "moduleGraphics.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
#include "protocol.h"
#include "mouseHandle.h"
#include "midiInput.h"
#include "graphics.h"
#include "menus.h"
#include "selection.h"
#include "undo.h"
#include "moduleReplace.h"
#include "palette.h"
#include "cableChain.h"

// ── Synth settings action targets ──────────────────────────────────────────

static _Atomic uint8_t * gSettingU8Target     = NULL;
static _Atomic int8_t *  gSettingI8Target     = NULL;

// ── Perf settings action targets ───────────────────────────────────────────

static _Atomic uint8_t * gPerfSettingU8Target = NULL;

// ── Patch settings action targets ──────────────────────────────────────────

static uint32_t          gPatchSettingModule  = 0;
static uint32_t          gPatchSettingParam   = 0;

void send_synth_settings_msg(void) {
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdWriteSynthSettings;
    msg_send(&gToUsbThread, &msg);
}

static void action_setting_u8(int index) {
    *gSettingU8Target = (uint8_t)gContextMenu.items[index].param;
    send_synth_settings_msg();
}

static void action_setting_i8(int index) {
    *gSettingI8Target = (int8_t)(int32_t)gContextMenu.items[index].param;
    send_synth_settings_msg();
}

void send_perf_settings_msg(void) {
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdWritePerfSettings;
    msg_send(&gToUsbThread, &msg);
}

void send_master_clock_run(uint32_t running) {
    tMessageContent msg = {0};

    msg.cmd                        = eMsgCmdSetMasterClockRun;
    msg.masterClockRunData.running = running;
    msg_send(&gToUsbThread, &msg);
}

static void action_perf_setting_u8(int index) {
    *gPerfSettingU8Target = (uint8_t)gContextMenu.items[index].param;
    send_perf_settings_msg();
}

static void send_patch_setting_param(uint32_t slot, uint32_t moduleIndex, uint32_t paramIndex, uint32_t value) {
    tMessageContent msg = {0};

    msg.cmd                 = eMsgCmdSetValue;
    msg.slot                = slot;
    msg.paramData.moduleKey = (tModuleKey){
        slot, (uint32_t)locationMorph, moduleIndex
    };
    msg.paramData.param     = paramIndex;
    msg.paramData.value     = value;
    msg.paramData.variation = 0;
    msg_send(&gToUsbThread, &msg);
}

static void action_patch_setting_u8(int index) {
    uint32_t   newValue = (uint32_t)gContextMenu.items[index].param;
    uint32_t   slot     = (uint32_t)gPatchParamsEdit.slot;
    tModuleKey key      = {slot, (uint32_t)locationMorph, gPatchSettingModule};
    tModule *  module   = get_module(key);
    uint32_t   oldValue = module ? module->param[0][gPatchSettingParam].value : newValue;

    if (module != NULL) {
        module->param[0][gPatchSettingParam].value = (uint8_t)newValue;
    }
    send_patch_setting_param(slot, gPatchSettingModule, gPatchSettingParam, newValue);
    undo_push_param_change(key, gPatchSettingParam, 0, oldValue, newValue);
}

static void action_patch_setting_i8(int index) {
    uint32_t   newValue = (uint32_t)(uint8_t)(int8_t)(int32_t)gContextMenu.items[index].param;
    uint32_t   slot     = (uint32_t)gPatchParamsEdit.slot;
    tModuleKey key      = {slot, (uint32_t)locationMorph, gPatchSettingModule};
    tModule *  module   = get_module(key);
    uint32_t   oldValue = module ? module->param[0][gPatchSettingParam].value : newValue;

    if (module != NULL) {
        module->param[0][gPatchSettingParam].value = (uint8_t)newValue;
    }
    send_patch_setting_param(slot, gPatchSettingModule, gPatchSettingParam, newValue);
    undo_push_param_change(key, gPatchSettingParam, 0, oldValue, newValue);
}

// ── Patch descriptor action targets ────────────────────────────────────────

static void send_patch_descr_update(uint32_t slot) {
    tMessageContent messageContent = {0};

    messageContent.cmd  = eMsgCmdWritePatchDescr;
    messageContent.slot = slot;
    msg_send(&gToUsbThread, &messageContent);
}

static void action_set_patch_type(int index) {
    uint32_t slot     = gSlot;
    uint8_t  oldValue = (uint8_t)gPatchDescr[slot].category;
    uint8_t  newValue = (uint8_t)gContextMenu.items[index].param;

    gPatchDescr[slot].category = newValue;
    send_patch_descr_update(slot);
    undo_push_patch_descr(slot, UNDO_PATCH_DESCR_CATEGORY, oldValue, newValue);
    gContextMenu.active        = false;
}

static void action_set_mono_poly(int index) {
    uint32_t slot     = gSlot;
    uint8_t  oldValue = gPatchDescr[slot].monoPoly;
    uint8_t  newValue = (uint8_t)gContextMenu.items[index].param;

    gPatchDescr[slot].monoPoly = newValue;
    send_patch_descr_update(slot);
    undo_push_patch_descr(slot, UNDO_PATCH_DESCR_MONO_POLY, oldValue, newValue);
    gContextMenu.active        = false;
}

static void action_set_voice_count(int index) {
    uint32_t slot     = gSlot;
    uint8_t  oldValue = gPatchDescr[slot].voiceCount;
    uint8_t  newValue = (uint8_t)gContextMenu.items[index].param;

    gPatchDescr[slot].voiceCount = newValue;
    send_patch_descr_update(slot);
    undo_push_patch_descr(slot, UNDO_PATCH_DESCR_VOICE_COUNT, oldValue, newValue);
    gContextMenu.active          = false;
}

static void action_copy_variation(int index) {
    uint32_t        sourceVariation = (uint32_t)(gContextMenu.items[index].param >> 4) & 0xF;
    uint32_t        targetVariation = (uint32_t)(gContextMenu.items[index].param) & 0xF;
    uint32_t        slot            = gSlot;
    uint32_t        numParams       = 0;
    uint32_t        paramIndex      = 0;
    uint32_t        morphIndex      = 0;
    tMessageContent msg             = {0};

    LOG_DEBUG("Copy variation %u to %u\n", sourceVariation, targetVariation);

    for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
        for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
            tModule * module = get_module_slot(slot, loc, i);

            if (!module->active) {
                continue;
            }
            numParams = module_param_count(module->type);

            for (paramIndex = 0; paramIndex < numParams; paramIndex++) {
                module->param[targetVariation][paramIndex].value = module->param[sourceVariation][paramIndex].value;

                for (morphIndex = 0; morphIndex < NUM_MORPHS; morphIndex++) {
                    module->param[targetVariation][paramIndex].morphRange[morphIndex] = module->param[sourceVariation][paramIndex].morphRange[morphIndex];
                }
            }
        }
    }

    msg.cmd                             = eMsgCmdCopyVariation;
    msg.slot                            = slot;
    msg.copyVariationData.fromVariation = sourceVariation;
    msg.copyVariationData.toVariation   = targetVariation;
    msg_send(&gToUsbThread, &msg);

    gContextMenu.active                 = false;
    synthlib_request_redraw();
}

// ── Module / cable / morph actions ─────────────────────────────────────────

// notes §1

// Resolves the right-clicked connector to a chain node, confirming it belongs to the slot and
// location currently on screen. Every cable command starts here.
static bool cable_menu_node(tCableNode * node) {
    if ((gMenuContext.moduleKey.slot != gSlot) || (gMenuContext.moduleKey.location != gLocation)) {
        return false;
    }
    return cable_chain_node_from_connector(get_module(gMenuContext.moduleKey),
                                           gMenuContext.connectorIndex, node);
}

// notes §2
static void menu_action_set_cable_colour(int index) {
    uint32_t   newColour = gContextMenu.items[index].param;
    tCableNode node;
    tCableKey  keys[MAX_NUM_CABLES];

    gContextMenu.active = false;

    if (!cable_menu_node(&node)) {
        return;
    }

    if (cable_chain_colour(gSlot, gLocation, node) == cableColourWhite) {
        return;
    }
    uint32_t   count     = cable_chain_collect_branch(gSlot, gLocation, node, keys, MAX_NUM_CABLES);

    undo_begin_cable_edit(gSlot, gLocation);
    cable_chain_apply_colour(gSlot, gLocation, keys, count, (tCableColour)newColour);
    undo_commit_cable_edit();
    synthlib_request_redraw();
}

// notes §3
static void menu_action_disconnect_cable(int index) {
    tCableNode node;

    (void)index;

    if (!cable_menu_node(&node)) {
        return;
    }
    undo_begin_cable_edit(gSlot, gLocation);
    bool       edited = cable_chain_disconnect(gSlot, gLocation, node);

    undo_commit_cable_edit();  // Pushes nothing when the command was a no-op

    if (!edited) {
        return;
    }
    update_module_up_rates();
    synthlib_request_redraw();
}

// notes §4
static void menu_action_break_cable(int index) {
    tCableNode node;

    (void)index;

    if (!cable_menu_node(&node)) {
        return;
    }
    undo_begin_cable_edit(gSlot, gLocation);
    bool       edited = cable_chain_break(gSlot, gLocation, node);

    undo_commit_cable_edit();

    if (!edited) {
        return;
    }
    update_module_up_rates();
    synthlib_request_redraw();
}

// notes §5
static void menu_action_delete_chain(int index) {
    tCableNode node;
    tCableKey  keys[MAX_NUM_CABLES];

    (void)index;

    if (!cable_menu_node(&node)) {
        return;
    }
    uint32_t   count = cable_chain_collect_branch(gSlot, gLocation, node, keys, MAX_NUM_CABLES);

    if (count == 0) {
        return;
    }
    undo_begin_cable_edit(gSlot, gLocation);
    cable_chain_delete_keys(gSlot, gLocation, keys, count);
    undo_commit_cable_edit();
    update_module_up_rates();
    synthlib_request_redraw();
}

// notes §6
static void menu_action_select_all(int index) {
    (void)index;
    selection_select_all();
    gContextMenu.active = false;
}

// notes §7
static void menu_action_delete_unused_modules(int index) {
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    (void)index;

    selection_clear();

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);
        bool      used   = false;

        if ((module == NULL) || !module->active) {
            continue;
        }

        for (uint32_t c = 0; (c < MAX_NUM_CABLES) && !used; c++) {
            tCable * cable = get_cable_slot(slot, location, c);

            if ((cable == NULL) || !cable->active) {
                continue;
            }

            if ((cable->key.moduleFromIndex == i) || (cable->key.moduleToIndex == i)) {
                used = true;
            }
        }

        if (!used) {
            // toggle, not add: selection_add() is private to selection.c, and the selection was
            // just cleared so toggling can only ever add.
            selection_toggle((tModuleKey){slot, location, i});
        }
    }

    if (gSelection.count > 0) {
        undo_push_delete_selection();
        delete_selection();
    }
    gContextMenu.active = false;
    synthlib_request_redraw();
}

// notes §8
static void menu_action_paste_params(int index) {
    uint32_t                 slot      = gSlot;
    tModuleKey               key       = gMenuContext.moduleKey;
    tModule *                module    = get_module(key);

    (void)index;

    if ((module == NULL) || !gClipboard.active || (gClipboard.moduleCount == 0)) {
        gContextMenu.active = false;
        return;
    }
    const tClipboardModule * src       = &gClipboard.modules[0];

    if (src->type != module->type) {
        gContextMenu.active = false;
        return;
    }
    uint32_t                 numParams = module_param_count(module->type);

    if (numParams > MAX_NUM_PARAMETERS) {
        numParams = MAX_NUM_PARAMETERS;
    }

    for (uint32_t v = 0; v < NUM_VARIATIONS_USB; v++) {
        for (uint32_t p = 0; p < numParams; p++) {
            uint32_t oldValue = module->param[v][p].value;
            uint32_t newValue = src->param[v][p].value;

            if (oldValue == newValue) {
                continue;
            }
            module->param[v][p].value = (uint8_t)newValue;
            send_param_value(slot, key, p, v, newValue);
            undo_push_param_change(key, p, v, oldValue, newValue);
        }
    }

    gContextMenu.active = false;
    synthlib_request_redraw();
}

static void menu_action_delete_unused_cables(int index) {
    uint32_t  slot     = gSlot;
    uint32_t  location = gLocation;
    tCableKey keys[MAX_NUM_CABLES];
    uint32_t  count    = 0;

    (void)index;

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if ((cable == NULL) || !cable->active) {
            continue;
        }

        // Walking back from the to-end covers this cable itself, since it is the one feeding it
        if (!cable_chain_find_root(slot, location, cable_chain_to_node(cable), NULL)) {
            keys[count++] = cable->key;
        }
    }

    if (count == 0) {
        return;
    }
    undo_begin_cable_edit(slot, location);
    cable_chain_delete_keys(slot, location, keys, count);
    undo_commit_cable_edit();
    update_module_up_rates();
    synthlib_request_redraw();
}

static void ensure_module_selected(void) {
    if (!is_selected(gMenuContext.moduleKey) || gSelection.count == 0) {
        selection_set_single(gMenuContext.moduleKey);
    }
}

static void menu_action_copy_module(int index) {
    ensure_module_selected();
    copy_selection();
}

// notes §9
static void menu_action_copy_params_to_marked(int index) {
    uint32_t slot      = gSlot;
    uint32_t variation = gPatchDescr[slot].activeVariation;

    (void)index;
    ensure_module_selected();

    for (uint32_t si = 0; si < gSelection.count; si++) {
        tModule * module = get_module(gSelection.keys[si]);

        if (module == NULL) {
            continue;
        }

        for (uint32_t p = 0; p < module_param_count(module->type); p++) {
            send_param_value_to_links(slot, module->key, p, variation, module->param[variation][p].value);
        }
    }

    gContextMenu.active = false;
    synthlib_request_redraw();
}

static void menu_action_cut_module(int index) {
    ensure_module_selected();
    cut_selection();
    synthlib_request_redraw();
}

static void menu_action_paste(int index) {
    paste_clipboard();
}

// The right-clicked module's group, rebuilt on every open. MAX_REPLACE_CANDIDATES is one more than
// the largest group (Switch, at 18) so the biggest one still fits with its own member removed.
#define MAX_REPLACE_CANDIDATES    (24)
static tModuleType gReplaceCandidates[MAX_REPLACE_CANDIDATES];
static uint32_t    gReplaceCandidateCount;
static tMenuItem   gReplaceMenuItems[MAX_REPLACE_CANDIDATES + 1];

// notes §10
static void menu_action_replace_module(int index) {
    uint32_t which = (uint32_t)gContextMenu.items[index].param;

    if (which < gReplaceCandidateCount) {
        module_replace(gMenuContext.moduleKey, gReplaceCandidates[which]);
    }
    gContextMenu.active = false;
}

static void menu_action_delete_module(int index) {
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    if (gMenuContext.moduleKey.slot == slot && gMenuContext.moduleKey.location == location) {
        ensure_module_selected();
        undo_push_delete_selection();
        delete_selection();
        update_module_up_rates();
    }
}

static void action_rename_module(int index) {
    tModule * module = get_module(gMenuContext.moduleKey);

    if (module != NULL) {
        gModuleNameEdit.active    = true;
        gModuleNameEdit.moduleKey = gMenuContext.moduleKey;
        COPY_STRING(gModuleNameEdit.buffer, module->name);
        gModuleNameEdit.cursorPos = (uint32_t)strlen(gModuleNameEdit.buffer);
    }
    gContextMenu.active = false;
    synthlib_request_redraw();
}

// notes §11
static void action_toggle_exclude_from_mutation(int index) {
    (void)index;
    ensure_module_selected();
    tModule * clicked  = get_module(gMenuContext.moduleKey);

    if (clicked == NULL) {
        gContextMenu.active = false;
        return;
    }
    uint8_t   newValue = clicked->excludeFromMutation ? 0 : 1;

    for (uint32_t i = 0; i < gSelection.count; i++) {
        tModule * module   = get_module(gSelection.keys[i]);

        if (module == NULL) {
            continue;
        }
        uint8_t   oldValue = (uint8_t)module->excludeFromMutation;

        if (oldValue == newValue) {
            continue;
        }
        module->excludeFromMutation = newValue;
        undo_push_module_exclude(gSelection.keys[i], oldValue, newValue);
        send_mutation_lock_value(gSelection.keys[i].slot, gSelection.keys[i], newValue);
    }

    gContextMenu.active = false;
    synthlib_request_redraw();
}

// notes §12
void modules_set_colour(uint32_t colour) {
    for (uint32_t i = 0; i < gSelection.count; i++) {
        tMessageContent messageContent = {0};
        tModule *       module         = get_module(gSelection.keys[i]);

        if ((module == NULL) || (module->colour == colour)) {
            continue;
        }
        undo_push_module_colour(module->key, module->colour, colour);
        module->colour                            = colour;

        messageContent.cmd                        = eMsgCmdSetModuleColour;
        messageContent.slot                       = module->key.slot;
        messageContent.moduleColourData.moduleKey = module->key;
        messageContent.moduleColourData.colour    = module->colour;

        msg_send(&gToUsbThread, &messageContent);
    }

    synthlib_request_redraw();
}

static void action_set_module_colour(int index) {
    ensure_module_selected();
    modules_set_colour((uint32_t)gContextMenu.items[index].param);
}

static void action_rename_morph_label(int index) {
    uint32_t  morphIndex = (uint32_t)gContextMenu.items[index].param;
    uint32_t  slot       = gSlot;

    gMenuContext.moduleKey = (tModuleKey){
        slot, locationMorph, 1
    };

    tModule * module     = get_module(gMenuContext.moduleKey);

    if (module != NULL) {
        uint32_t pi = morphIndex;

        gParamNameEdit.active     = true;
        gParamNameEdit.moduleKey  = gMenuContext.moduleKey;
        gParamNameEdit.paramIndex = pi;
        COPY_STRING(gParamNameEdit.buffer, module->paramName[pi][0]);
        gParamNameEdit.cursorPos  = (uint32_t)strlen(gParamNameEdit.buffer);
    }
    gContextMenu.active    = false;
    synthlib_request_redraw();
}

// ── Module creation helpers ─────────────────────────────────────────────────

static void init_params_on_module(tModule * module, uint32_t location, uint32_t variation) {
    uint32_t paramListIndex = 0;
    uint32_t paramIndex     = 0;
    uint32_t numParams      = module_param_count(module->type);
    uint32_t slot           = gSlot;

    if (location != gLocation) {
        return;
    }

    for (paramListIndex = 0; paramListIndex < array_size_param_location_list(); paramListIndex++) {
        if (paramLocationList[paramListIndex].moduleType == module->type) {
            module->param[variation][paramIndex].value = paramLocationList[paramListIndex].defaultValue;
            send_param_value(slot, module->key, paramIndex, variation, module->param[variation][paramIndex].value);
            paramIndex++;

            if (paramIndex >= numParams) {
                break;
            }
        }
    }
}

void init_params_on_module_all_variations(tModule * module, uint32_t location) {
    if (location != gLocation) {
        return;
    }

    for (uint32_t variation = 0; variation < NUM_VARIATIONS; variation++) {
        init_params_on_module(module, location, variation);
    }
}

int32_t find_unique_module_id(uint32_t location) {
    uint32_t slot = gSlot;

    for (uint32_t i = 1; i < MAX_NUM_MODULES; i++) {
        tModule * candidate = get_module_slot(slot, location, i);

        if ((candidate == NULL) || !candidate->active) {
            return (int32_t)i;
        }
    }

    return -1;
}

// notes §13
void module_prototype(tModuleType type, tModule * module) {
    if (module == NULL) {
        return;
    }
    memset(module, 0, sizeof(*module));
    module->type                = type;
    module->active              = true;
    module->excludeFromMutation = default_mutation_lock(type) ? 1 : 0;
    module->actualParamCount    = module_param_count(type);
    module->modeCount           = module_mode_count(type);

    for (uint32_t i = 0, seen = 0; (i < (uint32_t)array_size_mode_location_list()) && (seen < MAX_NUM_MODES); i++) {
        if (modeLocationList[i].moduleType == type) {
            module->mode[seen].value = modeLocationList[i].defaultValue;
            seen++;
        }
    }

    COPY_STRING(module->name, gModuleProperties[type].name);
    init_params_on_module_all_variations(module, gLocation);
}

int32_t create_module_at(tModuleType type, uint32_t column, uint32_t row, bool syncToDevice) {
    uint32_t        slot           = gSlot;
    uint32_t        location       = gLocation;
    tModule         module         = {0};
    tMessageContent messageContent = {0};
    int32_t         uniqueIndex    = find_unique_module_id(location);

    if (uniqueIndex < 0) {
        return -1;
    }
    module.key.slot                               = slot;
    module.key.location                           = location;
    module.key.index                              = (uint32_t)uniqueIndex;
    module.type                                   = type;
    module.column                                 = column;
    module.row                                    = row;
    module.excludeFromMutation                    = default_mutation_lock(module.type) ? 1 : 0;

    // notes §14
    module.colour                                 = palette_new_module_colour();

    // notes §15
    module.actualParamCount                       = module_param_count(module.type);

    // notes §16
    module.modeCount                              = module_mode_count(module.type);

    // Bounded by MAX_NUM_MODES as well as by the table: module.mode[] holds that many, and the table
    // is free to grow a third row for a type without anyone thinking about this loop.
    for (uint32_t i = 0, seen = 0; (i < (uint32_t)array_size_mode_location_list()) && (seen < MAX_NUM_MODES); i++) {
        if (modeLocationList[i].moduleType == module.type) {
            module.mode[seen].value = modeLocationList[i].defaultValue;
            seen++;
        }
    }

    COPY_STRING(module.name, gModuleProperties[module.type].name);

    messageContent.cmd                            = eMsgCmdWriteModule;
    messageContent.slot                           = slot;
    messageContent.moduleData.moduleKey           = module.key;
    messageContent.moduleData.type                = module.type;
    messageContent.moduleData.row                 = module.row;
    messageContent.moduleData.column              = module.column;
    messageContent.moduleData.colour              = module.colour;
    messageContent.moduleData.upRate              = module.upRate;
    messageContent.moduleData.excludeFromMutation = module.excludeFromMutation;
    messageContent.moduleData.unknown1            = module.unknown1;
    messageContent.moduleData.modeCount           = module_mode_count(module.type);

    for (int i = 0; i < module_mode_count(module.type); i++) {
        messageContent.moduleData.mode[i] = module.mode[i].value;
    }

    COPY_STRING(messageContent.moduleData.name, module.name);

    if (syncToDevice) {
        msg_send(&gToUsbThread, &messageContent); // push to the G2; backdoor/test callers pass false to stay local-only
    }
    write_module(module.key, &module);

    init_params_on_module_all_variations(get_module(module.key), location);

    // notes §17
    if (shift_modules_down(module.key) == false) {
        delete_module_and_cables(module.key);
        return -1;
    }
    return uniqueIndex;
}

static void menu_action_create(int index) {
    if (gContextMenu.items[index].param != 0) {
        uint32_t       column         = 0;
        uint32_t       row            = 0;

        convert_mouse_coord_to_module_column_row(&column, &row, gContextMenu.originCoord);

        // create_module_at() ends in shift_modules_down(), so a module created on top of another
        // pushes it down the column. Those positions have to be captured BEFORE the call for undo to
        // put them back — without it, undoing an Add left the column it disturbed still disturbed.
        tUndoMoveEntry displaced[MAX_NUM_MODULES];
        uint32_t       displacedCount = module_positions_snapshot((uint32_t)gSlot, (uint32_t)gLocation, displaced);

        int32_t        created        = create_module_at((tModuleType)gContextMenu.items[index].param, column, row, true);

        if (created >= 0) {
            undo_push_create_module((tModuleKey){gSlot, gLocation, (uint32_t)created},
                                    displaced, module_positions_changed(displaced, displacedCount));
        }
    }
}

// ── Parameter / knob actions ────────────────────────────────────────────────

int32_t find_knob_for_param(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex) {
    for (int i = 0; i < MAX_NUM_KNOBS; i++) {
        if (  gKnobArray[slot].knob[i].assigned
           && gKnobArray[slot].knob[i].location == location
           && gKnobArray[slot].knob[i].moduleIndex == moduleIndex
           && gKnobArray[slot].knob[i].paramIndex == paramIndex) {
            return i;
        }
    }

    return -1;
}

int32_t find_global_knob_for_param(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex) {
    for (int i = 0; i < MAX_NUM_KNOBS; i++) {
        if (  gGlobalKnobArray[i].assigned
           && gGlobalKnobArray[i].slotIndex == slot
           && gGlobalKnobArray[i].location == location
           && gGlobalKnobArray[i].moduleIndex == moduleIndex
           && gGlobalKnobArray[i].paramIndex == paramIndex) {
            return i;
        }
    }

    return -1;
}

static void action_assign_knob(int index) {
    uint32_t        slot           = gSlot;
    uint32_t        targetKnob     = (uint32_t)gContextMenu.items[index].param;
    uint32_t        location       = gMenuContext.moduleKey.location;
    uint32_t        moduleIndex    = gMenuContext.moduleKey.index;
    uint32_t        paramIndex     = gMenuContext.paramIndex;
    int32_t         existingKnob;
    tMessageContent msg            = {0};

    // Snapshot before state for undo
    tKnob           targetBefore   = gKnobArray[slot].knob[targetKnob];

    existingKnob = find_knob_for_param(slot, location, moduleIndex, paramIndex);
    bool            hasSecond      = existingKnob >= 0 && (uint32_t)existingKnob != targetKnob;
    tKnob           existingBefore = hasSecond ? gKnobArray[slot].knob[existingKnob] : (tKnob){
        0
    };

    if (gKnobArray[slot].knob[targetKnob].assigned) {
        gKnobArray[slot].knob[targetKnob].assigned = false;
        msg.cmd                                    = eMsgCmdDeassignKnob;
        msg.slot                                   = slot;
        msg.knobDeassignData.knobIndex             = targetKnob;
        msg_send(&gToUsbThread, &msg);
        memset(&msg, 0, sizeof(msg));
    }

    if (hasSecond) {
        gKnobArray[slot].knob[existingKnob].assigned = false;
        msg.cmd                                      = eMsgCmdDeassignKnob;
        msg.slot                                     = slot;
        msg.knobDeassignData.knobIndex               = (uint32_t)existingKnob;
        msg_send(&gToUsbThread, &msg);
        memset(&msg, 0, sizeof(msg));
    }
    gKnobArray[slot].knob[targetKnob].assigned    = true;
    gKnobArray[slot].knob[targetKnob].location    = location;
    gKnobArray[slot].knob[targetKnob].moduleIndex = moduleIndex;
    gKnobArray[slot].knob[targetKnob].isLed       = 0;
    gKnobArray[slot].knob[targetKnob].paramIndex  = paramIndex;

    msg.cmd                                       = eMsgCmdAssignKnob;
    msg.slot                                      = slot;
    msg.knobAssignData.moduleKey                  = gMenuContext.moduleKey;
    msg.knobAssignData.paramIndex                 = paramIndex;
    msg.knobAssignData.knobIndex                  = targetKnob;
    msg_send(&gToUsbThread, &msg);

    tKnob targetAfter   = gKnobArray[slot].knob[targetKnob];
    tKnob existingAfter = hasSecond ? gKnobArray[slot].knob[existingKnob] : (tKnob){
        0
    };
    undo_push_knob(slot,
                   targetKnob, &targetBefore, &targetAfter,
                   hasSecond ? existingKnob : -1,
                   hasSecond ? &existingBefore : NULL,
                   hasSecond ? &existingAfter : NULL);

    gContextMenu.active                           = false;
    synthlib_request_redraw();
}

// notes §18
static void action_reset_param_morph(int index) {
    uint32_t  slot       = gSlot;
    uint32_t  variation  = gPatchDescr[slot].activeVariation;
    tModule * module     = get_module(gMenuContext.moduleKey);
    uint32_t  paramIndex = gMenuContext.paramIndex;

    if ((module == NULL) || (paramIndex >= MAX_NUM_PARAMETERS)) {
        return;
    }
    // notes §19
    uint32_t  group      = gContextMenu.items[index].param;
    bool      all        = (group >= (uint32_t)NUM_MORPHS);

    for (uint32_t m = 0; m < (uint32_t)NUM_MORPHS; m++) {
        if (!all && (m != group)) {
            continue;
        }

        if (module->param[variation][paramIndex].morphRange[m] == 0) {
            continue;   // nothing to clear, so nothing to send
        }
        module->param[variation][paramIndex].morphRange[m] = 0;
        send_param_morph(slot, module->key, paramIndex, m, variation, 0);
    }

    synthlib_request_redraw();
}

static void action_deassign_knob(int index) {
    uint32_t        slot        = gSlot;
    uint32_t        location    = gMenuContext.moduleKey.location;
    uint32_t        moduleIndex = gMenuContext.moduleKey.index;
    uint32_t        paramIndex  = gMenuContext.paramIndex;
    int32_t         knobIndex   = find_knob_for_param(slot, location, moduleIndex, paramIndex);
    tMessageContent msg         = {0};

    if (knobIndex >= 0) {
        tKnob before = gKnobArray[slot].knob[knobIndex];
        gKnobArray[slot].knob[knobIndex].assigned = false;
        msg.cmd                                   = eMsgCmdDeassignKnob;
        msg.slot                                  = slot;
        msg.knobDeassignData.knobIndex            = (uint32_t)knobIndex;
        msg_send(&gToUsbThread, &msg);
        tKnob after  = gKnobArray[slot].knob[knobIndex];
        undo_push_knob(slot, (uint32_t)knobIndex, &before, &after, -1, NULL, NULL);
    }
    gContextMenu.active = false;
    synthlib_request_redraw();
}

// Global knob assignments live in a single flat array shared across all Slots
// (gGlobalKnobArray), each entry additionally recording which Slot's module it
// targets. Unlike patch knobs there's no per-slot undo history for these yet.
static void action_assign_global_knob(int index) {
    uint32_t        slot        = gSlot;
    uint32_t        targetKnob  = (uint32_t)gContextMenu.items[index].param;
    uint32_t        location    = gMenuContext.moduleKey.location;
    uint32_t        moduleIndex = gMenuContext.moduleKey.index;
    uint32_t        paramIndex  = gMenuContext.paramIndex;
    int32_t         existingKnob;
    tMessageContent msg         = {0};

    existingKnob                             = find_global_knob_for_param(slot, location, moduleIndex, paramIndex);

    undo_begin_global_knob_edit();

    if (gGlobalKnobArray[targetKnob].assigned) {
        gGlobalKnobArray[targetKnob].assigned = false;
        msg.cmd                               = eMsgCmdDeassignGlobalKnob;
        msg.globalKnobDeassignData.knobIndex  = targetKnob;
        msg_send(&gToUsbThread, &msg);
        memset(&msg, 0, sizeof(msg));
    }

    if (existingKnob >= 0 && (uint32_t)existingKnob != targetKnob) {
        gGlobalKnobArray[existingKnob].assigned = false;
        msg.cmd                                 = eMsgCmdDeassignGlobalKnob;
        msg.globalKnobDeassignData.knobIndex    = (uint32_t)existingKnob;
        msg_send(&gToUsbThread, &msg);
        memset(&msg, 0, sizeof(msg));
    }
    gGlobalKnobArray[targetKnob].assigned    = true;
    gGlobalKnobArray[targetKnob].location    = location;
    gGlobalKnobArray[targetKnob].moduleIndex = moduleIndex;
    gGlobalKnobArray[targetKnob].isLed       = 0;
    gGlobalKnobArray[targetKnob].paramIndex  = paramIndex;
    gGlobalKnobArray[targetKnob].slotIndex   = slot;

    msg.cmd                                  = eMsgCmdAssignGlobalKnob;
    msg.globalKnobAssignData.slotIndex       = slot;
    msg.globalKnobAssignData.location        = location;
    msg.globalKnobAssignData.moduleIndex     = moduleIndex;
    msg.globalKnobAssignData.paramIndex      = paramIndex;
    msg.globalKnobAssignData.knobIndex       = targetKnob;
    msg_send(&gToUsbThread, &msg);

    undo_commit_global_knob_edit();
    gContextMenu.active                      = false;
    synthlib_request_redraw();
}

static void action_deassign_global_knob(int index) {
    uint32_t        slot        = gSlot;
    uint32_t        location    = gMenuContext.moduleKey.location;
    uint32_t        moduleIndex = gMenuContext.moduleKey.index;
    uint32_t        paramIndex  = gMenuContext.paramIndex;
    int32_t         knobIndex   = find_global_knob_for_param(slot, location, moduleIndex, paramIndex);
    tMessageContent msg         = {0};

    undo_begin_global_knob_edit();

    if (knobIndex >= 0) {
        gGlobalKnobArray[knobIndex].assigned = false;
        msg.cmd                              = eMsgCmdDeassignGlobalKnob;
        msg.globalKnobDeassignData.knobIndex = (uint32_t)knobIndex;
        msg_send(&gToUsbThread, &msg);
    }
    undo_commit_global_knob_edit();
    gContextMenu.active = false;
    synthlib_request_redraw();
}

// notes §20

int32_t find_controller_for_param(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex) {
    for (uint32_t i = 0; i < gControllerCount[slot]; i++) {
        if (  gControllerArray[slot].controller[i].location == location
           && gControllerArray[slot].controller[i].moduleIndex == moduleIndex
           && gControllerArray[slot].controller[i].paramIndex == paramIndex) {
            return (int32_t)i;
        }
    }

    return -1;
}

static int32_t find_controller_for_cc(uint32_t slot, uint32_t midiCC) {
    for (uint32_t i = 0; i < gControllerCount[slot]; i++) {
        if (gControllerArray[slot].controller[i].midiCC == midiCC) {
            return (int32_t)i;
        }
    }

    return -1;
}

// Removes entry idx from the compact list via swap-with-last, and clears the
// shadow hasMidiCC flag on the module param it used to point to.
static void remove_controller_entry(uint32_t slot, uint32_t idx) {
    tModuleKey key  = {slot, gControllerArray[slot].controller[idx].location, gControllerArray[slot].controller[idx].moduleIndex};
    uint32_t   pi   = gControllerArray[slot].controller[idx].paramIndex;
    tModule *  mod  = get_module(key);

    if ((mod != NULL) && (pi < MAX_NUM_PARAMETERS)) {
        mod->param[0][pi].hasMidiCC = false;
    }
    uint32_t   last = gControllerCount[slot] - 1;
    gControllerArray[slot].controller[idx] = gControllerArray[slot].controller[last];
    gControllerCount[slot]                 = last;
}

// notes §21
void assign_midi_cc_to_param(uint32_t slot, tModuleKey moduleKey, uint32_t paramIndex, uint32_t targetCC) {
    uint32_t        location    = moduleKey.location;

    LOG_INFO("Assign MIDI CC %u -> slot %u location %u module %u param %u\n",
             targetCC, slot, moduleKey.location, moduleKey.index, paramIndex);

    uint32_t        moduleIndex = moduleKey.index;
    int32_t         ccOwner     = find_controller_for_cc(slot, targetCC);
    int32_t         paramEntry;
    tMessageContent msg         = {0};
    tModule *       mod         = NULL;

    // One click can both steal the CC from whoever had it and assign it here — see
    // undo_begin_midi_cc_edit(), which snapshots the whole table rather than the entries.
    undo_begin_midi_cc_edit(slot);

    if (  ccOwner >= 0
       && !(  gControllerArray[slot].controller[ccOwner].location == location
           && gControllerArray[slot].controller[ccOwner].moduleIndex == moduleIndex
           && gControllerArray[slot].controller[ccOwner].paramIndex == paramIndex)) {
        remove_controller_entry(slot, (uint32_t)ccOwner);
        msg.cmd                       = eMsgCmdDeassignMidiCC;
        msg.slot                      = slot;
        msg.midiCCDeassignData.midiCC = targetCC;
        msg_send(&gToUsbThread, &msg);
        memset(&msg, 0, sizeof(msg));
    }
    paramEntry                      = find_controller_for_param(slot, location, moduleIndex, paramIndex);

    if (paramEntry >= 0) {
        gControllerArray[slot].controller[paramEntry].midiCC = (uint8_t)targetCC;
    } else if (gControllerCount[slot] < MAX_NUM_CONTROLLERS) {
        uint32_t idx = gControllerCount[slot]++;
        gControllerArray[slot].controller[idx] = (tController){
            (uint8_t)targetCC, location, moduleIndex, paramIndex
        };
    }
    mod                             = get_module(moduleKey);

    if ((mod != NULL) && (paramIndex < MAX_NUM_PARAMETERS)) {
        mod->param[0][paramIndex].midiCC    = (uint8_t)targetCC;
        mod->param[0][paramIndex].hasMidiCC = true;
    }
    msg.cmd                         = eMsgCmdAssignMidiCC;
    msg.slot                        = slot;
    msg.midiCCAssignData.moduleKey  = moduleKey;
    msg.midiCCAssignData.paramIndex = paramIndex;
    msg.midiCCAssignData.midiCC     = targetCC;
    msg_send(&gToUsbThread, &msg);

    undo_commit_midi_cc_edit();
    synthlib_request_redraw();
}

// notes §22
void clear_assignments_for_module(tModuleKey key) {
    uint32_t        slot = key.slot;
    tMessageContent msg  = {0};

    for (uint32_t i = 0; i < MAX_NUM_KNOBS; i++) {
        if (  !gKnobArray[slot].knob[i].assigned
           || (gKnobArray[slot].knob[i].location != key.location)
           || (gKnobArray[slot].knob[i].moduleIndex != key.index)) {
            continue;
        }
        gKnobArray[slot].knob[i]       = (tKnob){
            0
        };

        msg                            = (tMessageContent){
            0
        };
        msg.cmd                        = eMsgCmdDeassignKnob;
        msg.slot                       = slot;
        msg.knobDeassignData.knobIndex = i;
        msg_send(&gToUsbThread, &msg);
    }

    // Performance-wide, so the entry's own slotIndex decides whether it belongs to this module.
    for (uint32_t i = 0; i < MAX_NUM_KNOBS; i++) {
        if (  !gGlobalKnobArray[i].assigned
           || (gGlobalKnobArray[i].slotIndex != slot)
           || (gGlobalKnobArray[i].location != key.location)
           || (gGlobalKnobArray[i].moduleIndex != key.index)) {
            continue;
        }
        gGlobalKnobArray[i]                  = (tGlobalKnob){
            0
        };

        msg                                  = (tMessageContent){
            0
        };
        msg.cmd                              = eMsgCmdDeassignGlobalKnob;
        msg.globalKnobDeassignData.knobIndex = i;
        msg_send(&gToUsbThread, &msg);
    }

    // Walked BACKWARDS because remove_controller_entry() closes the gap by moving the last entry
    // into the freed one — going forwards would step straight over whatever landed there.
    for (int32_t i = (int32_t)gControllerCount[slot] - 1; i >= 0; i--) {
        if (  (gControllerArray[slot].controller[i].location != key.location)
           || (gControllerArray[slot].controller[i].moduleIndex != key.index)) {
            continue;
        }
        uint32_t midiCC = gControllerArray[slot].controller[i].midiCC;

        remove_controller_entry(slot, (uint32_t)i);

        msg                           = (tMessageContent){
            0
        };
        msg.cmd                       = eMsgCmdDeassignMidiCC;
        msg.slot                      = slot;
        msg.midiCCDeassignData.midiCC = midiCC;
        msg_send(&gToUsbThread, &msg);
    }
}

// "Assign to CC# nn (last received)". The CC travels in the item's payload like the picker entries
// do, so this shares their assign path; it exists separately only to clear the focus, which the L
// key also does — either route ends with nothing selected, so neither can be repeated by accident.
static void action_assign_midi_cc_learn(int index) {
    assign_midi_cc_to_param(gSlot, gMenuContext.moduleKey, gMenuContext.paramIndex,
                            (uint32_t)gContextMenu.items[index].param);
    gParamFocus.valid   = false;
    gContextMenu.active = false;
}

static void action_assign_midi_cc(int index) {
    assign_midi_cc_to_param(gSlot, gMenuContext.moduleKey, gMenuContext.paramIndex,
                            (uint32_t)gContextMenu.items[index].param);
    gContextMenu.active = false;
}

// notes §23
int32_t midi_learn_last_cc(const char ** source) {
    int32_t fromSynth = atomic_load(&gLastDeviceMidiCC[gSlot]);
    int32_t fromLocal = midi_input_last_cc();

    if (fromSynth >= 0) {
        if (source != NULL) {
            *source = "synth";
        }
        return fromSynth;
    }

    if (source != NULL) {
        *source = "MIDI in";
    }
    return fromLocal;
}

// notes §24
bool midi_learn_focused_param(void) {
    const char * source     = NULL;
    int32_t      cc         = midi_learn_last_cc(&source);
    tModuleKey   moduleKey  = gParamFocus.moduleKey;
    uint32_t     paramIndex = gParamFocus.paramIndex;

    LOG_INFO("MIDI Learn: focus valid=%d slot=%u loc=%u module=%u param=%u | gSlot=%u | "
             "cc=%d (from %s) synth=%d local=%d\n",
             (int)gParamFocus.valid, moduleKey.slot, moduleKey.location, moduleKey.index,
             paramIndex, (unsigned)gSlot, cc, (source != NULL) ? source : "?",
             (int)atomic_load(&gLastDeviceMidiCC[gSlot]), (int)midi_input_last_cc());

    if (gParamFocus.valid == false) {
        LOG_INFO("MIDI Learn: select a parameter first\n");
        return false;
    }
    // L CONSUMES THE FOCUS, whether or not the assignment goes through. Nothing is left armed, and a
    // second L cannot quietly reassign the parameter still sitting in focus from the first — the
    // sequence is deliberately select, press, done.
    gParamFocus.valid = false;

    if (cc < 0) {
        LOG_INFO("MIDI Learn: no MIDI CC received yet\n");
        return false;
    }

    if (moduleKey.slot != gSlot) {
        LOG_INFO("MIDI Learn: that parameter is in another slot\n");
        return false;
    }
    assign_midi_cc_to_param(gSlot, moduleKey, paramIndex, (uint32_t)cc);
    return true;
}

// notes §25

// Queues the whole slot as one patch write. See the note above for why bulk edits go this way.
static void send_whole_patch(uint32_t slot) {
    tMessageContent msg = {0};

    msg.cmd  = eMsgCmdWritePatch;
    msg.slot = slot;
    msg_send(&gToUsbThread, &msg);
}

// Assigns the lowest CC number not already in use. -1 when all 128 are taken.
static int32_t next_free_midi_cc(uint32_t slot) {
    for (uint32_t cc = 0; cc < MAX_NUM_CONTROLLERS; cc++) {
        if (find_controller_for_cc(slot, cc) < 0) {
            return (int32_t)cc;
        }
    }

    return -1;
}

// notes §26
void midi_cc_assign_all_knobs(uint32_t slot) {
    bool changed = false;

    if (slot >= MAX_SLOTS) {
        return;
    }
    undo_begin_midi_cc_edit(slot);

    for (uint32_t knobIdx = 0; knobIdx < MAX_NUM_KNOBS; knobIdx++) {
        const tKnob * knob = &gKnobArray[slot].knob[knobIdx];

        if (!knob->assigned || (knob->paramIndex >= MAX_NUM_PARAMETERS)) {
            continue;
        }
        tModuleKey    key  = {slot, knob->location, knob->moduleIndex};
        tModule *     mod  = get_module(key);

        if ((mod == NULL) || !mod->active) {
            continue;
        }

        if (find_controller_for_param(slot, knob->location, knob->moduleIndex, knob->paramIndex) >= 0) {
            continue;   // already has one
        }

        if (gControllerCount[slot] >= MAX_NUM_CONTROLLERS) {
            break;
        }
        int32_t       cc   = next_free_midi_cc(slot);

        if (cc < 0) {
            break;
        }
        uint32_t      idx  = gControllerCount[slot]++;
        gControllerArray[slot].controller[idx]    = (tController){
            (uint8_t)cc, knob->location, knob->moduleIndex, knob->paramIndex
        };
        mod->param[0][knob->paramIndex].midiCC    = (uint8_t)cc;
        mod->param[0][knob->paramIndex].hasMidiCC = true;
        changed                                   = true;
    }

    if (changed) {
        send_whole_patch(slot);
    }
    undo_commit_midi_cc_edit();
    synthlib_request_redraw();
}

// Clears every MIDI CC in the Slot. remove_controller_entry() compacts by swapping the last entry
// into the hole, so repeatedly removing index 0 walks the whole table.
void midi_cc_clear_all(uint32_t slot) {
    bool changed = false;

    if (slot >= MAX_SLOTS) {
        return;
    }
    changed = (gControllerCount[slot] > 0);

    undo_begin_midi_cc_edit(slot);

    while (gControllerCount[slot] > 0) {
        remove_controller_entry(slot, 0);
    }

    if (changed) {
        send_whole_patch(slot);
    }
    undo_commit_midi_cc_edit();
    synthlib_request_redraw();
}

// notes §27

// Walks the selection in the order the modules were selected, and each module's params in index
// order, which is the order the original numbers them in.
void midi_cc_assign_selection(void) {
    uint32_t slot    = gSlot;
    bool     changed = false;

    if (gSelection.count == 0) {
        return;
    }
    undo_begin_midi_cc_edit(slot);

    for (uint32_t si = 0; si < gSelection.count; si++) {
        tModuleKey key    = gSelection.keys[si];
        tModule *  module = get_module(key);

        if ((module == NULL) || !module->active || (key.slot != slot)) {
            continue;
        }
        uint32_t   count  = module_param_count(module->type);

        if (count > MAX_NUM_PARAMETERS) {
            count = MAX_NUM_PARAMETERS;
        }

        for (uint32_t paramIndex = 0; paramIndex < count; paramIndex++) {
            if (find_controller_for_param(slot, key.location, key.index, paramIndex) >= 0) {
                continue;   // already has one — fill the gaps, don't renumber
            }

            if (gControllerCount[slot] >= MAX_NUM_CONTROLLERS) {
                break;
            }
            int32_t  cc  = next_free_midi_cc(slot);

            if (cc < 0) {
                break;
            }
            uint32_t idx = gControllerCount[slot]++;
            gControllerArray[slot].controller[idx] = (tController){
                (uint8_t)cc, key.location, key.index, paramIndex
            };
            module->param[0][paramIndex].midiCC    = (uint8_t)cc;
            module->param[0][paramIndex].hasMidiCC = true;
            changed                                = true;
        }
    }

    if (changed) {
        send_whole_patch(slot);
    }
    undo_commit_midi_cc_edit();
    synthlib_request_redraw();
}

// Clears every CC belonging to a selected module. Walks the controller table backwards because
// remove_controller_entry() compacts by swapping the last entry into the hole — going forwards
// would step straight over whatever got moved down into the index just vacated.
void midi_cc_deassign_selection(void) {
    uint32_t slot    = gSlot;
    bool     changed = false;

    if (gSelection.count == 0) {
        return;
    }
    undo_begin_midi_cc_edit(slot);

    for (int32_t i = (int32_t)gControllerCount[slot] - 1; i >= 0; i--) {
        tModuleKey key = {
            slot,
            gControllerArray[slot].controller[i].location,
            gControllerArray[slot].controller[i].moduleIndex
        };

        if (is_selected(key)) {
            remove_controller_entry(slot, (uint32_t)i);
            changed = true;
        }
    }

    if (changed) {
        send_whole_patch(slot);
    }
    undo_commit_midi_cc_edit();
    synthlib_request_redraw();
}

static void action_deassign_midi_cc(int index) {
    uint32_t        slot        = gSlot;
    uint32_t        location    = gMenuContext.moduleKey.location;
    uint32_t        moduleIndex = gMenuContext.moduleKey.index;
    uint32_t        paramIndex  = gMenuContext.paramIndex;
    int32_t         entry       = find_controller_for_param(slot, location, moduleIndex, paramIndex);
    tMessageContent msg         = {0};

    undo_begin_midi_cc_edit(slot);

    if (entry >= 0) {
        uint32_t cc = gControllerArray[slot].controller[entry].midiCC;
        remove_controller_entry(slot, (uint32_t)entry);
        msg.cmd                       = eMsgCmdDeassignMidiCC;
        msg.slot                      = slot;
        msg.midiCCDeassignData.midiCC = cc;
        msg_send(&gToUsbThread, &msg);
    }
    undo_commit_midi_cc_edit();  // Pushes nothing if the click landed on an unassigned param
    gContextMenu.active = false;
    synthlib_request_redraw();
}

static void action_set_toggle_value(int index) {
    uint32_t  slot      = gSlot;
    uint32_t  variation = gPatchDescr[slot].activeVariation;
    tModule * module    = get_module(gMenuContext.moduleKey);

    if (module != NULL) {
        uint32_t paramIdx = gMenuContext.paramIndex;
        uint32_t oldValue = module->param[variation][paramIdx].value;
        uint32_t newValue = (uint32_t)gContextMenu.items[index].param;
        module->param[variation][paramIdx].value = (uint8_t)newValue;

        uint32_t paramRef = module->param[variation][paramIdx].paramRef;

        if (paramLocationList[paramRef].type == paramTypeCustomData) {
            send_custom_data_value(slot, gMenuContext.moduleKey);
        } else {
            send_param_value(slot, gMenuContext.moduleKey, paramIdx, variation, newValue);
            undo_push_param_change(gMenuContext.moduleKey, paramIdx, variation, oldValue, newValue);
            send_param_value_to_links(slot, gMenuContext.moduleKey, paramIdx, variation, newValue);
        }
    }
    gContextMenu.active = false;
    synthlib_request_redraw();
}

// Set just before a waveform picker's menu is opened, and read back by the draw callback below.
static uint32_t sWaveMenuModuleType = 0;

static void draw_wave_menu_item(tRectangle cell, uint32_t param) {
    render_wave_icon(cell, sWaveMenuModuleType, param, module_wave_icon_shape(sWaveMenuModuleType));
}

void open_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex, uint32_t paramRef) {
    static tMenuItem menuItems[33];
    static char      labels[32][32];

    const char **    strMap  = paramLocationList[paramRef].strMap;
    uint32_t         range   = paramLocationList[paramRef].range;
    tModule *        module  = get_module(moduleKey);

    // notes §28
    bool             isWaves = (module != NULL) && module_wave_picker_param(module->type, paramIndex);

    if (isWaves == true) {
        sWaveMenuModuleType = module->type;
    }

    for (uint32_t v = 0; v < range && v < 32; v++) {
        if (strMap && strMap[v]) {
            snprintf(labels[v], sizeof(labels[v]), "%s", strMap[v]);
        } else {
            snprintf(labels[v], sizeof(labels[v]), "%u", v);
        }
        menuItems[v]          = (tMenuItem){
            labels[v], RGB_GREY_3, action_set_toggle_value, v, NULL
        };
        menuItems[v].drawItem = (isWaves == true) ? draw_wave_menu_item : NULL;
    }

    menuItems[range < 32 ? range : 32] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    gMenuContext.moduleKey             = moduleKey;
    gMenuContext.paramIndex            = paramIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
}

static void action_set_mode_value(int index) {
    uint32_t  slot   = gSlot;
    tModule * module = get_module(gMenuContext.moduleKey);

    if (module != NULL) {
        uint32_t modeIdx  = gMenuContext.paramIndex;
        uint32_t oldValue = module->mode[modeIdx].value;
        uint32_t newValue = (uint32_t)gContextMenu.items[index].param;
        module->mode[modeIdx].value = newValue;
        send_mode_value(slot, gMenuContext.moduleKey, modeIdx, newValue);
        undo_push_mode_change(gMenuContext.moduleKey, modeIdx, oldValue, newValue);
    }
    gContextMenu.active = false;
    synthlib_request_redraw();
}

void open_mode_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t modeIndex, uint32_t modeRef) {
    static tMenuItem menuItems[33];
    static char      labels[32][32];

    const char **    strMap  = modeLocationList[modeRef].strMap;
    uint32_t         range   = modeLocationList[modeRef].range;
    tModule *        module  = get_module(moduleKey);

    // Same treatment as the parameter-backed pickers: the entries are painted as waves, while the
    // labels stay filled in so the engine still measures the rows from them.
    bool             isWaves = (module != NULL) && module_wave_picker_mode(module->type, modeIndex);

    if (isWaves == true) {
        sWaveMenuModuleType = module->type;
    }

    for (uint32_t v = 0; v < range && v < 32; v++) {
        if (strMap && strMap[v]) {
            snprintf(labels[v], sizeof(labels[v]), "%s", strMap[v]);
        } else {
            snprintf(labels[v], sizeof(labels[v]), "%u", v);
        }
        menuItems[v]          = (tMenuItem){
            labels[v], RGB_GREY_3, action_set_mode_value, v, NULL
        };
        menuItems[v].drawItem = (isWaves == true) ? draw_wave_menu_item : NULL;
    }

    menuItems[range < 32 ? range : 32] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    gMenuContext.moduleKey             = moduleKey;
    gMenuContext.paramIndex            = modeIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
}

static void action_rename_param_label(int index) {
    uint32_t  pi     = gMenuContext.paramIndex;
    // The menu item's own param carries WHICH button, for a Channel Select group; it is 0 for the
    // single-name case, which is every other parameter.
    uint32_t  li     = (uint32_t)gContextMenu.items[index].param;
    tModule * module = get_module(gMenuContext.moduleKey);

    if ((module != NULL) && (li < MAX_NUM_LABELS)) {
        gParamNameEdit.active     = true;
        gParamNameEdit.moduleKey  = gMenuContext.moduleKey;
        gParamNameEdit.paramIndex = pi;
        gParamNameEdit.labelIndex = li;
        memset(gParamNameEdit.buffer, 0, sizeof(gParamNameEdit.buffer));

        // Seeded with what the box currently READS, not only with a name already stored: renaming a
        // Channel Select button that still shows its default should start from "Out 3", not empty.
        if (module->paramNameSet[pi][li]) {
            COPY_STRING(gParamNameEdit.buffer, module->paramName[pi][li]);
        } else if (paramLocationList[module->param[gPatchDescr[module->key.slot].activeVariation][pi].paramRef].type == paramTypeRadioEdit) {
            COPY_STRING(gParamNameEdit.buffer,
                        radio_caption(module, pi, li, paramLocationList[module->param[gPatchDescr[module->key.slot].activeVariation][pi].paramRef].strMap));
        }
        gParamNameEdit.cursorPos  = (uint32_t)strlen(gParamNameEdit.buffer);
    }
    gContextMenu.active = false;
    synthlib_request_redraw();
}

void open_param_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex) {
    static tMenuItem pageMenuItems[NUM_PARAM_PAGES + 1];
    static char      pageLabels[NUM_PARAM_PAGES][10];
    static tMenuItem bankMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE + 1];
    static char      bankLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][24];
    static tMenuItem slotMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK + 1];
    static char      slotLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK][64];

    // Global Parameter Pages — same 5 page x 3 bank x 8 knob layout (labelled
    // "Global A1".."Global E3" per the manual), but backed by the single
    // flat gGlobalKnobArray shared across all Slots rather than gKnobArray[slot].
    static tMenuItem globalPageMenuItems[NUM_PARAM_PAGES + 1];
    static char      globalPageLabels[NUM_PARAM_PAGES][14];
    static tMenuItem globalBankMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE + 1];
    static char      globalBankLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][24];
    static tMenuItem globalSlotMenuItems[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK + 1];
    static char      globalSlotLabels[NUM_PARAM_PAGES][NUM_BANKS_PER_PAGE][NUM_KNOBS_PER_BANK][64];

    // MIDI CC — grouped in the same 8-per-list shape as the knob banks
    // (submenus opened via tMenuItem::subMenu always render single-column,
    // so 128 flat entries would run off the bottom of the screen).
    static tMenuItem ccGroupMenuItems[NUM_MIDI_CC_GROUPS + 1];
    static char      ccGroupLabels[NUM_MIDI_CC_GROUPS][16];
    static tMenuItem ccMenuItems[NUM_MIDI_CC_GROUPS][MIDI_CC_GROUP_SIZE + 1];
    static char      ccLabels[NUM_MIDI_CC_GROUPS][MIDI_CC_GROUP_SIZE][20];

    // notes §29
    static tMenuItem menuItems[12];

    uint32_t         slot           = gSlot;
    int32_t          assigned       = find_knob_for_param(slot, moduleKey.location, moduleKey.index, paramIndex);
    int32_t          globalAssigned = find_global_knob_for_param(slot, moduleKey.location, moduleKey.index, paramIndex);
    int32_t          ccAssigned     = find_controller_for_param(slot, moduleKey.location, moduleKey.index, paramIndex);
    uint32_t         currentCC      = (ccAssigned >= 0) ? gControllerArray[slot].controller[ccAssigned].midiCC : (uint32_t)MAX_NUM_CONTROLLERS;
    int              count          = 0;

    for (int pg = 0; pg < NUM_PARAM_PAGES; pg++) {
        snprintf(pageLabels[pg], sizeof(pageLabels[pg]), "Page %c", 'A' + pg);

        for (int bk = 0; bk < NUM_BANKS_PER_PAGE; bk++) {
            snprintf(bankLabels[pg][bk], sizeof(bankLabels[pg][bk]), "%c - Bank %d", 'A' + pg, bk + 1);

            for (int k = 0; k < NUM_KNOBS_PER_BANK; k++) {
                uint32_t knobIdx = (uint32_t)((pg * NUM_BANKS_PER_PAGE + bk) * NUM_KNOBS_PER_BANK + k);
                bool     inUse   = gKnobArray[slot].knob[knobIdx].assigned;

                if (inUse) {
                    tModuleKey   modKey  = {slot, gKnobArray[slot].knob[knobIdx].location, gKnobArray[slot].knob[knobIdx].moduleIndex};
                    uint32_t     pi      = gKnobArray[slot].knob[knobIdx].paramIndex;
                    const char * modName = "";
                    const char * parName = "";
                    tModule *    mod     = get_module(modKey);

                    if (mod != NULL) {
                        uint32_t variation = gPatchDescr[slot].activeVariation;

                        modName = (mod->name[0] != '\0') ? mod->name : gModuleProperties[mod->type].name;

                        if ((pi < MAX_NUM_PARAMETERS) && mod->paramNameSet[pi][0]) {
                            parName = mod->paramName[pi][0];
                        } else if (pi < MAX_NUM_PARAMETERS) {
                            const char * label = paramLocationList[mod->param[variation][pi].paramRef].label;

                            if (label != NULL && label[0] != '\0') {
                                parName = label;
                            }
                        }
                    }
                    snprintf(slotLabels[pg][bk][k], sizeof(slotLabels[pg][bk][k]),
                             "%c %d - %d Used - %s %s", 'A' + pg, bk + 1, k + 1, modName, parName);
                } else {
                    snprintf(slotLabels[pg][bk][k], sizeof(slotLabels[pg][bk][k]),
                             "%c %d - %d ---", 'A' + pg, bk + 1, k + 1);
                }
                slotMenuItems[pg][bk][k] = (tMenuItem){
                    slotLabels[pg][bk][k], RGB_GREY_3, action_assign_knob, knobIdx, NULL
                };
            }

            slotMenuItems[pg][bk][NUM_KNOBS_PER_BANK] = (tMenuItem){
                NULL, RGB_BLACK, NULL, 0, NULL
            };

            bankMenuItems[pg][bk]                     = (tMenuItem){
                bankLabels[pg][bk], RGB_GREY_3, NULL, 0, slotMenuItems[pg][bk]
            };
        }

        bankMenuItems[pg][NUM_BANKS_PER_PAGE] = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };

        pageMenuItems[pg]                     = (tMenuItem){
            pageLabels[pg], RGB_GREY_3, NULL, 0, bankMenuItems[pg]
        };
    }

    pageMenuItems[NUM_PARAM_PAGES] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    for (int pg = 0; pg < NUM_PARAM_PAGES; pg++) {
        snprintf(globalPageLabels[pg], sizeof(globalPageLabels[pg]), "Global %c", 'A' + pg);

        for (int bk = 0; bk < NUM_BANKS_PER_PAGE; bk++) {
            snprintf(globalBankLabels[pg][bk], sizeof(globalBankLabels[pg][bk]), "Global %c%d", 'A' + pg, bk + 1);

            for (int k = 0; k < NUM_KNOBS_PER_BANK; k++) {
                uint32_t knobIdx = (uint32_t)((pg * NUM_BANKS_PER_PAGE + bk) * NUM_KNOBS_PER_BANK + k);
                bool     inUse   = gGlobalKnobArray[knobIdx].assigned;

                if (inUse) {
                    uint32_t     gSlotIdx = gGlobalKnobArray[knobIdx].slotIndex;
                    tModuleKey   modKey   = {gSlotIdx, gGlobalKnobArray[knobIdx].location, gGlobalKnobArray[knobIdx].moduleIndex};
                    uint32_t     pi       = gGlobalKnobArray[knobIdx].paramIndex;
                    const char * modName  = "";
                    const char * parName  = "";
                    tModule *    mod      = get_module(modKey);

                    if (mod != NULL) {
                        uint32_t variation = gPatchDescr[gSlotIdx].activeVariation;

                        modName = (mod->name[0] != '\0') ? mod->name : gModuleProperties[mod->type].name;

                        if ((pi < MAX_NUM_PARAMETERS) && mod->paramNameSet[pi][0]) {
                            parName = mod->paramName[pi][0];
                        } else if (pi < MAX_NUM_PARAMETERS) {
                            const char * label = paramLocationList[mod->param[variation][pi].paramRef].label;

                            if (label != NULL && label[0] != '\0') {
                                parName = label;
                            }
                        }
                    }
                    snprintf(globalSlotLabels[pg][bk][k], sizeof(globalSlotLabels[pg][bk][k]),
                             "%c %d - %d Used - Slot %u %s %s", 'A' + pg, bk + 1, k + 1, gSlotIdx + 1, modName, parName);
                } else {
                    snprintf(globalSlotLabels[pg][bk][k], sizeof(globalSlotLabels[pg][bk][k]),
                             "%c %d - %d ---", 'A' + pg, bk + 1, k + 1);
                }
                globalSlotMenuItems[pg][bk][k] = (tMenuItem){
                    globalSlotLabels[pg][bk][k], RGB_GREY_3, action_assign_global_knob, knobIdx, NULL
                };
            }

            globalSlotMenuItems[pg][bk][NUM_KNOBS_PER_BANK] = (tMenuItem){
                NULL, RGB_BLACK, NULL, 0, NULL
            };

            globalBankMenuItems[pg][bk]                     = (tMenuItem){
                globalBankLabels[pg][bk], RGB_GREY_3, NULL, 0, globalSlotMenuItems[pg][bk]
            };
        }

        globalBankMenuItems[pg][NUM_BANKS_PER_PAGE] = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };

        globalPageMenuItems[pg]                     = (tMenuItem){
            globalPageLabels[pg], RGB_GREY_3, NULL, 0, globalBankMenuItems[pg]
        };
    }

    globalPageMenuItems[NUM_PARAM_PAGES] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    for (int g = 0; g < NUM_MIDI_CC_GROUPS; g++) {
        snprintf(ccGroupLabels[g], sizeof(ccGroupLabels[g]), "CC %d - %d",
                 g * MIDI_CC_GROUP_SIZE, (g * MIDI_CC_GROUP_SIZE) + MIDI_CC_GROUP_SIZE - 1);

        for (int k = 0; k < MIDI_CC_GROUP_SIZE; k++) {
            uint32_t cc     = (uint32_t)((g * MIDI_CC_GROUP_SIZE) + k);
            bool     isCurr = (cc == currentCC);

            snprintf(ccLabels[g][k], sizeof(ccLabels[g][k]), isCurr ? "CC %u (current)" : "CC %u", cc);
            ccMenuItems[g][k] = (tMenuItem){
                ccLabels[g][k], isCurr ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_GREY_3, action_assign_midi_cc, cc, NULL
            };
        }

        ccMenuItems[g][MIDI_CC_GROUP_SIZE] = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };

        ccGroupMenuItems[g]                = (tMenuItem){
            ccGroupLabels[g], RGB_GREY_3, NULL, 0, ccMenuItems[g]
        };
    }

    ccGroupMenuItems[NUM_MIDI_CC_GROUPS] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    menuItems[count++]                   = (tMenuItem){
        "Assign knob...", RGB_GREY_3, NULL, 0, pageMenuItems
    };

    if (assigned >= 0) {
        menuItems[count++] = (tMenuItem){
            "Deassign knob", RGB_GREY_3, action_deassign_knob, 0, NULL
        };
    }
    menuItems[count++]                   = (tMenuItem){
        "Global assign knob...", RGB_GREY_3, NULL, 0, globalPageMenuItems
    };

    if (globalAssigned >= 0) {
        menuItems[count++] = (tMenuItem){
            "Deassign global knob", RGB_GREY_3, action_deassign_global_knob, 0, NULL
        };
    }
    // notes §30
    {
        static char  learnLabel[64];
        const char * source = NULL;
        int32_t      lastCC = midi_learn_last_cc(&source);

        if (lastCC >= 0) {
            snprintf(learnLabel, sizeof(learnLabel), "Assign to CC# %d (last received)", lastCC);
            menuItems[count++] = (tMenuItem){
                learnLabel, RGB_GREY_3, action_assign_midi_cc_learn, (uint32_t)lastCC, NULL
            };
        } else {
            menuItems[count++] = (tMenuItem){
                "No MIDI CC received yet", RGB_GREY_5, NULL, 0, NULL
            };
        }
    }

    menuItems[count++] = (tMenuItem){
        "MIDI CC...", RGB_GREY_3, NULL, 0, ccGroupMenuItems
    };

    if (ccAssigned >= 0) {
        menuItems[count++] = (tMenuItem){
            "Remove MIDI CC", RGB_GREY_3, action_deassign_midi_cc, 0, NULL
        };
    }
    {
        uint32_t  variation = gPatchDescr[slot].activeVariation;
        tModule * mod       = get_module(moduleKey);

        if ((mod != NULL) && (paramIndex < MAX_NUM_PARAMETERS)) {
            uint32_t    paramRef = mod->param[variation][paramIndex].paramRef;

            if (paramLocationList[paramRef].type == paramTypeEnable) {
                menuItems[count++] = (tMenuItem){
                    "Rename", RGB_GREY_3, action_rename_param_label, 0, NULL
                };
            } else if (paramLocationList[paramRef].type == paramTypeRadioEdit) {
                // notes §31
                static char   renameLabels[MAX_NUM_LABELS][24];
                uint32_t      buttons = paramLocationList[paramRef].range;
                const char ** strMap  = paramLocationList[paramRef].strMap;

                for (uint32_t b = 0; (b < buttons) && (b < MAX_NUM_LABELS) && (count < (int)(sizeof(menuItems) / sizeof(menuItems[0])) - 1); b++) {
                    snprintf(renameLabels[b], sizeof(renameLabels[b]), "Rename \"%s\"", radio_caption(mod, paramIndex, b, strMap));
                    menuItems[count++] = (tMenuItem){
                        renameLabels[b], RGB_GREY_3, action_rename_param_label, (int)b, NULL
                    };
                }
            }
            // notes §32
            static char morphLabel[40];
            uint32_t    morphsSet = 0;

            for (uint32_t m = 0; m < (uint32_t)NUM_MORPHS; m++) {
                if (mod->param[variation][paramIndex].morphRange[m] != 0) {
                    morphsSet++;
                }
            }

            if (mod->param[variation][paramIndex].morphRange[gMorphGroupFocus] != 0) {
                snprintf(morphLabel, sizeof(morphLabel), "Reset %s morph", morphStrMap[gMorphGroupFocus]);
                menuItems[count++] = (tMenuItem){
                    morphLabel, RGB_GREY_3, action_reset_param_morph, gMorphGroupFocus, NULL
                };
            }

            if (morphsSet > 1) {
                menuItems[count++] = (tMenuItem){
                    "Reset all morphs", RGB_GREY_3, action_reset_param_morph, (uint32_t)NUM_MORPHS, NULL
                };
            }
        }
    }

    menuItems[count]        = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    gMenuContext.moduleKey  = moduleKey;
    gMenuContext.paramIndex = paramIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
}

// ── Static menu item arrays (menuResources.h uses the actions above) ───────

#include "menuResources.h"

// ── Synth settings dropdowns ────────────────────────────────────────────────

void open_midi_chan_dropdown(tCoord coord, _Atomic uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gMidiChanItems, 0, 0.0);
}

void open_sysex_id_dropdown(tCoord coord, _Atomic uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gSysexIdItems, 0, 0.0);
}

void open_tune_semi_dropdown(tCoord coord, _Atomic int8_t * target) {
    static tMenuItem items[26];
    static char      labels[25][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 25; i++) {
            int val = i - 12;
            snprintf(labels[i], sizeof(labels[i]), "%+d", val);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_i8;
            items[i].param   = (uint32_t)(int32_t)val;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingI8Target = target;
    open_context_menu(coord, items, 5, 0.0);
}

void open_tune_cent_dropdown(tCoord coord, _Atomic int8_t * target) {
    static tMenuItem items[102];
    static char      labels[101][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 101; i++) {
            int val = i - 50;
            snprintf(labels[i], sizeof(labels[i]), "%+d", val);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_i8;
            items[i].param   = (uint32_t)(int32_t)val;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingI8Target = target;
    open_context_menu(coord, items, 10, 0.0);
}

void open_octave_shift_dropdown(tCoord coord, _Atomic int8_t * target) {
    gSettingI8Target = target;
    open_context_menu(coord, gOctaveShiftItems, 0, 0.0);
}

void open_pedal_gain_dropdown(tCoord coord, _Atomic uint8_t * target) {
    static tMenuItem items[34];
    static char      labels[33][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 33; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%.2f", 1.0 + i / 64.0);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_setting_u8;
            items[i].param   = (uint32_t)i;
            items[i].subMenu = NULL;
        }

        initialized = true;
    }
    gSettingU8Target = target;
    open_context_menu(coord, items, 4, 0.0);
}

void open_patch_sort_dropdown(tCoord coord, _Atomic uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPatchSortItems, 0, 0.0);
}

void open_perf_sort_dropdown(tCoord coord, _Atomic uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPerfSortItems, 0, 0.0);
}

void open_on_off_dropdown(tCoord coord, _Atomic uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gOnOffItems, 0, 0.0);
}

void open_active_off_dropdown(tCoord coord, _Atomic uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gActiveOffItems, 0, 0.0);
}

void open_pedal_polarity_dropdown(tCoord coord, _Atomic uint8_t * target) {
    gSettingU8Target = target;
    open_context_menu(coord, gPedalPolarityItems, 0, 0.0);
}

// ── Perf settings dropdowns ─────────────────────────────────────────────────

void open_perf_on_off_dropdown(tCoord coord, _Atomic uint8_t * target) {
    static tMenuItem items[] = {
        {"Off", RGB_GREY_3, action_perf_setting_u8, 0, NULL},
        {"On",  RGB_GREY_3, action_perf_setting_u8, 1, NULL},
        {NULL,  RGB_BLACK,  NULL,                   0, NULL},
    };

    gPerfSettingU8Target = target;
    open_context_menu(coord, items, 0, 0.0);
}

void open_stop_run_dropdown(tCoord coord, _Atomic uint8_t * target) {
    static tMenuItem items[] = {
        {"Stop", RGB_GREY_3, action_perf_setting_u8, 0, NULL},
        {"Run",  RGB_GREY_3, action_perf_setting_u8, 1, NULL},
        {NULL,   RGB_BLACK,  NULL,                   0, NULL},
    };

    gPerfSettingU8Target = target;
    open_context_menu(coord, items, 0, 0.0);
}

void open_master_clock_dropdown(tCoord coord, _Atomic uint8_t * target) {
    static tMenuItem items[213];
    static char      labels[212][5];
    static bool      initialized = false;
    int              i           = 0;

    if (!initialized) {
        for (i = 0; i < 211; i++) {
            int bpm = 30 + i;
            snprintf(labels[i], sizeof(labels[i]), "%d", bpm);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_perf_setting_u8;
            items[i].param   = (uint32_t)bpm;
            items[i].subMenu = NULL;
        }

        items[211]  = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        initialized = true;
    }
    gPerfSettingU8Target = target;
    open_context_menu(coord, items, 6, 0.0);
}

void open_midi_note_dropdown(tCoord coord, _Atomic uint8_t * target) {
    static const char * noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static tMenuItem    items[130];
    static char         labels[128][6];
    static bool         initialized = false;
    int                 i           = 0;

    if (!initialized) {
        for (i = 0; i < 128; i++) {
            int octave = (i / 12) - 1;
            snprintf(labels[i], sizeof(labels[i]), "%s%d", noteNames[i % 12], octave);
            items[i].label   = labels[i];
            items[i].colour  = (tRgb)RGB_GREY_3;
            items[i].action  = action_perf_setting_u8;
            items[i].param   = (uint32_t)i;
            items[i].subMenu = NULL;
        }

        items[128]  = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        initialized = true;
    }
    gPerfSettingU8Target = target;
    open_context_menu(coord, items, 8, 0.0);
}

// ── Patch settings dropdowns ────────────────────────────────────────────────

void toggle_patch_on_off(uint32_t moduleIndex, uint32_t paramIndex) {
    uint32_t   slot     = (uint32_t)gPatchParamsEdit.slot;
    tModuleKey key      = {slot, (uint32_t)locationMorph, moduleIndex};
    tModule *  module   = get_module(key);
    uint32_t   oldValue = module ? module->param[0][paramIndex].value : 0;
    uint32_t   newValue = oldValue ? 0 : 1;

    if (module != NULL) {
        module->param[0][paramIndex].value = (uint8_t)newValue;
    }
    send_patch_setting_param(slot, moduleIndex, paramIndex, newValue);
    undo_push_param_change(key, paramIndex, 0, oldValue, newValue);
}

void open_patch_on_off_dropdown(tCoord coord, uint32_t moduleIndex, uint32_t paramIndex) {
    static tMenuItem items[] = {
        {"Off", RGB_GREY_3, action_patch_setting_u8, 0, NULL},
        {"On",  RGB_GREY_3, action_patch_setting_u8, 1, NULL},
        {NULL,  RGB_BLACK,  NULL,                    0, NULL},
    };

    gPatchSettingModule = moduleIndex;
    gPatchSettingParam  = paramIndex;
    open_context_menu(coord, items, 0, 0.0);
}

void open_arp_rate_dropdown(tCoord coord) {
    static const char * rateLabels[] = {
        "1/96", "1/48", "1/32", "1/24", "1/16T", "1/16",
        "1/8T", "1/8",  "1/4T", "1/4",  "1/2T",  "1/2",
        "3/4",  "1/1",
    };
    static tMenuItem    items[15];
    static bool         initialized  = false;

    if (!initialized) {
        for (int i = 0; i < 14; i++) {
            items[i] = (tMenuItem){
                rateLabels[i], RGB_GREY_3, action_patch_setting_u8, (uint32_t)i, NULL
            };
        }

        items[14]   = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        initialized = true;
    }
    gPatchSettingModule = patchModuleArpeggiator;
    gPatchSettingParam  = ARP_SPEED;
    open_context_menu(coord, items, 0, 0.0);
}

void open_arp_direction_dropdown(tCoord coord) {
    static tMenuItem items[] = {
        {"Up",     RGB_GREY_3, action_patch_setting_u8, 0, NULL},
        {"Down",   RGB_GREY_3, action_patch_setting_u8, 1, NULL},
        {"Up+Dn",  RGB_GREY_3, action_patch_setting_u8, 2, NULL},
        {"Random", RGB_GREY_3, action_patch_setting_u8, 3, NULL},
        {NULL,     RGB_BLACK,  NULL,                    0, NULL},
    };

    gPatchSettingModule = patchModuleArpeggiator;
    gPatchSettingParam  = ARP_DIRECTION;
    open_context_menu(coord, items, 0, 0.0);
}

void open_arp_octave_dropdown(tCoord coord) {
    static tMenuItem items[] = {
        {"1 oct", RGB_GREY_3, action_patch_setting_u8, 0, NULL},
        {"2 oct", RGB_GREY_3, action_patch_setting_u8, 1, NULL},
        {"3 oct", RGB_GREY_3, action_patch_setting_u8, 2, NULL},
        {"4 oct", RGB_GREY_3, action_patch_setting_u8, 3, NULL},
        {NULL,    RGB_BLACK,  NULL,                    0, NULL},
    };

    gPatchSettingModule = patchModuleArpeggiator;
    gPatchSettingParam  = ARP_OCTAVES;
    open_context_menu(coord, items, 0, 0.0);
}

void open_vibrato_source_dropdown(tCoord coord) {
    static tMenuItem items[] = {
        {"Off",     RGB_GREY_3, action_patch_setting_u8, 0, NULL},
        {"AfTouch", RGB_GREY_3, action_patch_setting_u8, 1, NULL},
        {"Wheel",   RGB_GREY_3, action_patch_setting_u8, 2, NULL},
        {NULL,      RGB_BLACK,  NULL,                    0, NULL},
    };

    gPatchSettingModule = patchModuleVibrato;
    gPatchSettingParam  = VIBRATO_MOD;
    open_context_menu(coord, items, 0, 0.0);
}

void open_vibrato_amount_dropdown(tCoord coord) {
    static tMenuItem items[129];
    static char      labels[128][8];
    static bool      initialized = false;

    if (!initialized) {
        for (int i = 0; i < 128; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%d cnt", i);
            items[i] = (tMenuItem){
                labels[i], RGB_GREY_3, action_patch_setting_u8, (uint32_t)i, NULL
            };
        }

        items[128]  = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        initialized = true;
    }
    gPatchSettingModule = patchModuleVibrato;
    gPatchSettingParam  = VIBRATO_DEPTH;
    open_context_menu(coord, items, 8, 0.0);
}

void open_glide_mode_dropdown(tCoord coord) {
    static tMenuItem items[] = {
        {"Off",    RGB_GREY_3, action_patch_setting_u8, 0, NULL},
        {"Normal", RGB_GREY_3, action_patch_setting_u8, 1, NULL},
        {"Auto",   RGB_GREY_3, action_patch_setting_u8, 2, NULL},
        {NULL,     RGB_BLACK,  NULL,                    0, NULL},
    };

    gPatchSettingModule = patchModuleGlide;
    gPatchSettingParam  = GLIDE_TYPE;
    open_context_menu(coord, items, 0, 0.0);
}

void open_glide_time_dropdown(tCoord coord) {
    static tMenuItem items[121];
    static bool      initialized = false;

    if (!initialized) {
        for (int i = 0; i < 120; i++) {
            items[i] = (tMenuItem){
                patch_settings_glideStrMap[i], RGB_GREY_3, action_patch_setting_u8, (uint32_t)i, NULL
            };
        }

        items[120]  = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        initialized = true;
    }
    gPatchSettingModule = patchModuleGlide;
    gPatchSettingParam  = GLIDE_SPEED;
    open_context_menu(coord, items, 6, 0.0);
}

void open_bend_range_dropdown(tCoord coord) {
    static tMenuItem items[26];
    static char      labels[25][8];
    static bool      initialized = false;

    if (!initialized) {
        for (int i = 0; i < 25; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%d semi", i);
            items[i] = (tMenuItem){
                labels[i], RGB_GREY_3, action_patch_setting_u8, (uint32_t)i, NULL
            };
        }

        items[25]   = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        initialized = true;
    }
    gPatchSettingModule = patchModuleBend;
    gPatchSettingParam  = BEND_RANGE;
    open_context_menu(coord, items, 0, 0.0);
}

void open_patch_octave_shift_dropdown(tCoord coord) {
    static tMenuItem items[] = {
        {"-2", RGB_GREY_3, action_patch_setting_i8, (uint32_t)(int32_t)-2, NULL},
        {"-1", RGB_GREY_3, action_patch_setting_i8, (uint32_t)(int32_t)-1, NULL},
        {"0",  RGB_GREY_3, action_patch_setting_i8,                     0, NULL},
        {"+1", RGB_GREY_3, action_patch_setting_i8,                     1, NULL},
        {"+2", RGB_GREY_3, action_patch_setting_i8,                     2, NULL},
        {NULL, RGB_BLACK,  NULL,                                        0, NULL},
    };

    gPatchSettingModule = patchModuleSustain;
    gPatchSettingParam  = OCTAVE_SHIFT;
    open_context_menu(coord, items, 0, 0.0);
}

// ── Module / cable / morph menus ────────────────────────────────────────────

void open_connector_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t connectorIndex) {
    static tMenuItem cableColourItems[] = {
        {"",   {0.7, 0.1, 0.1}, menu_action_set_cable_colour, cableColourRed,    NULL},
        {"",   {0.3, 0.3, 0.7}, menu_action_set_cable_colour, cableColourBlue,   NULL},
        {"",   {0.7, 0.7, 0.1}, menu_action_set_cable_colour, cableColourYellow, NULL},
        {"",   {0.8, 0.3, 0.2}, menu_action_set_cable_colour, cableColourOrange, NULL},
        {"",   {0.1, 0.7, 0.1}, menu_action_set_cable_colour, cableColourGreen,  NULL},
        {"",   {0.7, 0.1, 0.7}, menu_action_set_cable_colour, cableColourPurple, NULL},
        {NULL, RGB_BLACK, NULL, 0, NULL}
    };

    // Order follows the original editor's cable popup (g2manual.txt, "CABLE POPUP").
    static tMenuItem menuItems[]        = {
        {"Disconnect",           RGB_GREY_3, menu_action_disconnect_cable,     0, NULL            },
        {"Break",                RGB_GREY_3, menu_action_break_cable,          0, NULL            },
        {"Cable colour",         RGB_GREY_3, NULL,                             0, cableColourItems},
        {"Delete",               RGB_GREY_3, menu_action_delete_chain,         0, NULL            },
        {"Delete unused cables", RGB_GREY_3, menu_action_delete_unused_cables, 0, NULL            },
        {NULL,                   RGB_BLACK,  NULL,                             0, NULL            }
    };

    gMenuContext.moduleKey      = moduleKey;
    gMenuContext.connectorIndex = connectorIndex;
    open_context_menu(coord, menuItems, 0, 0.0);
}

static char gExcludeMutationMenuLabel[40];

// notes §33
static char gModuleInfoMenuLabel[64];

void open_module_context_menu(tCoord coord, tModuleKey moduleKey) {
    static tMenuItem colourMenuItems[] = {
        {"",   MODULE_RED_1,         action_set_module_colour,  6, NULL},
        {"",   MODULE_GREEN_1,       action_set_module_colour, 10, NULL},
        {"",   MODULE_BLUE_1,        action_set_module_colour,  5, NULL},
        {"",   MODULE_YELLOW_1,      action_set_module_colour,  9, NULL},
        {"",   MODULE_PURPLE_1,      action_set_module_colour, 21, NULL},
        {"",   MODULE_CYAN_1,        action_set_module_colour, 17, NULL},
        {"",   MODULE_RED_2,         action_set_module_colour, 13, NULL},
        {"",   MODULE_GREEN_2,       action_set_module_colour,  8, NULL},
        {"",   MODULE_BLUE_2,        action_set_module_colour, 20, NULL},
        {"",   MODULE_YELLOW_2,      action_set_module_colour, 11, NULL},
        {"",   MODULE_PURPLE_2,      action_set_module_colour, 22, NULL},
        {"",   MODULE_CYAN_2,        action_set_module_colour,  7, NULL},
        {"",   MODULE_RED_3,         action_set_module_colour, 14, NULL},
        {"",   MODULE_GREEN_3,       action_set_module_colour, 16, NULL},
        {"",   MODULE_BLUE_3,        action_set_module_colour, 12, NULL},
        {"",   MODULE_YELLOW_3,      action_set_module_colour, 15, NULL},
        {"",   MODULE_PURPLE_3,      action_set_module_colour, 23, NULL},
        {"",   MODULE_CYAN_3,        action_set_module_colour, 18, NULL},
        {"",   MODULE_RED_4,         action_set_module_colour,  1, NULL},
        {"",   MODULE_GREEN_4,       action_set_module_colour,  2, NULL},
        {"",   MODULE_BLUE_4,        action_set_module_colour,  3, NULL},
        {"",   MODULE_YELLOW_4,      action_set_module_colour,  4, NULL},
        {"",   MODULE_PURPLE_4,      action_set_module_colour, 24, NULL},
        {"",   MODULE_CYAN_4,        action_set_module_colour, 19, NULL},
        {"",   MODULE_STANDARD_GREY, action_set_module_colour,  0, NULL},
        {NULL, RGB_BLACK,            NULL,                      0, NULL}
    };

    // notes §34
    enum {
        kItemInfo,
        kItemRename,
        kItemReplace,
        kItemColour,
        kItemCopy,
        kItemCut,
        kItemPaste,
        kItemPasteParams,
        kItemParamsToMarked,
        kItemDelete,
        kItemExclude,
        kItemTerminator
    };

    static tMenuItem menuItems[] = {
        // A HEADING, NOT A COMMAND: no action and the disabled colour, the same way Paste Params
        // greys itself out below. It names the module you right-clicked and gives its index.
        {NULL,           RGB_GREY_5, NULL,                                0, NULL,            0,                      0.0},
        {"Rename",       RGB_GREY_3, action_rename_module,                0, NULL,            0,                      0.0},
        // Filled in below: the submenu and whether it is offered at all depend on the module.
        {"Replace with", RGB_GREY_3, NULL,                                0, NULL,            0,                      0.0},
        {"Set colour",   RGB_GREY_3, NULL,                                0, colourMenuItems, 6, STANDARD_TEXT_HEIGHT * 2},
        {"Copy",         RGB_GREY_3, menu_action_copy_module,             0, NULL,            0,                      0.0},
        {"Cut",          RGB_GREY_3, menu_action_cut_module,              0, NULL},
        {"Paste",        RGB_GREY_3, menu_action_paste,                   0, NULL},
        {"Paste Params", RGB_GREY_3, menu_action_paste_params,            0, NULL},
        {
            "Copy Params to Marked Variations",
            RGB_GREY_3, menu_action_copy_params_to_marked, 0, NULL
        },
        {"Delete",       RGB_GREY_3, menu_action_delete_module,           0, NULL},
        {NULL,           RGB_GREY_3, action_toggle_exclude_from_mutation, 0, NULL},
        {NULL,           RGB_BLACK,  NULL,                                0, NULL}
    };

    tModule *        module   = get_module(moduleKey);
    bool             excluded = (module != NULL) && module->excludeFromMutation;

    // notes §35
    gReplaceCandidateCount                    = (module != NULL)
                             ? module_replace_candidates(module->type, gReplaceCandidates,
                                                         MAX_REPLACE_CANDIDATES) : 0;

    for (uint32_t i = 0; i < gReplaceCandidateCount; i++) {
        gReplaceMenuItems[i] = (tMenuItem){
            gModuleProperties[gReplaceCandidates[i]].name, RGB_GREY_3,
            menu_action_replace_module, i, NULL, 0, 0.0
        };
    }

    gReplaceMenuItems[gReplaceCandidateCount] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    menuItems[kItemReplace].colour            = (gReplaceCandidateCount > 0) ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5;
    menuItems[kItemReplace].subMenu           = (gReplaceCandidateCount > 0) ? gReplaceMenuItems : NULL;

    // Paste Params only means anything with a module of the SAME TYPE on the clipboard — a Decay
    // from an EnvADSR has no counterpart on an OscB. Greyed rather than hidden, so the entry stays
    // in the same place and its absence is explained by looking at it.
    bool     canPasteParams = gClipboard.active && (gClipboard.moduleCount > 0)
                              && (module != NULL) && (gClipboard.modules[0].type == module->type);

    menuItems[kItemPasteParams].colour        = canPasteParams ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5;
    menuItems[kItemPasteParams].action        = canPasteParams ? menu_action_paste_params : NULL;

    // Nothing to copy INTO unless a variation other than the one on screen is marked. Greyed rather
    // than hidden for the same reason as Paste Params above: the entry keeps its place, and its
    // being unavailable is explained by the empty variation strip in the topbar.
    bool     haveMarked     = false;

    for (uint32_t v = 0; v < VARIATION_INIT; v++) {
        if ((v != gPatchDescr[gSlot].activeVariation) && variation_is_linked((uint32_t)gSlot, v)) {
            haveMarked = true;
            break;
        }
    }

    menuItems[kItemParamsToMarked].colour     = haveMarked ? (tRgb)RGB_GREY_3 : (tRgb)RGB_GREY_5;
    menuItems[kItemParamsToMarked].action     = haveMarked ? menu_action_copy_params_to_marked : NULL;

    snprintf(gExcludeMutationMenuLabel, sizeof(gExcludeMutationMenuLabel), "[%s] Exclude From Mutation",
             excluded ? "x" : " ");
    menuItems[kItemExclude].label             = gExcludeMutationMenuLabel;

    unsigned rows           = (module != NULL) ? (unsigned)gModuleProperties[module->type].height : 0u;

    snprintf(gModuleInfoMenuLabel, sizeof(gModuleInfoMenuLabel), "%s  -  index %u, %u row%s",
             (module != NULL) ? gModuleProperties[module->type].name : "?",
             (unsigned)moduleKey.index, rows, (rows == 1) ? "" : "s");
    menuItems[kItemInfo].label                = gModuleInfoMenuLabel;

    gMenuContext.moduleKey                    = moduleKey;
    open_context_menu(coord, menuItems, 0, 0.0);
}

void open_module_area_context_menu(tCoord coord) {
    // notes §36
    static tMenuItem groupItems[palGroupCount + 1]                     = {0};
    static tMenuItem moduleItems[palGroupCount][PALETTE_GROUP_MAX + 1] = {0};

    for (uint32_t g = 0; g < (uint32_t)palGroupCount; g++) {
        uint32_t count = 0;

        for (uint32_t e = 0; (e < array_size_palette_list()) && (count < PALETTE_GROUP_MAX); e++) {
            if (gPaletteList[e].group != (tPaletteGroup)g) {
                continue;
            }
            moduleItems[g][count] = (tMenuItem){
                gPaletteList[e].menuLabel, (tRgb)RGB_GREY_3, menu_action_create,
                (uint32_t)gPaletteList[e].moduleType, NULL, 0, 0.0
            };
            count++;
        }

        moduleItems[g][count] = (tMenuItem){
            NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
        };

        groupItems[g]         = (tMenuItem){
            palette_group_name((tPaletteGroup)g), (tRgb)RGB_GREY_3, menu_action_create,
            0, moduleItems[g], 0, 0.0
        };
    }

    groupItems[palGroupCount] = (tMenuItem){
        NULL, (tRgb)RGB_BLACK, NULL, 0, NULL, 0, 0.0
    };

    static tMenuItem menuItems[] = {
        {"Create module",         RGB_GREY_3, menu_action_create,                0, groupItems},
        {"Paste",                 RGB_GREY_3, menu_action_paste,                 0, NULL      },
        {"Select All",            RGB_GREY_3, menu_action_select_all,            0, NULL      },
        {"Delete unused modules", RGB_GREY_3, menu_action_delete_unused_modules, 0, NULL      },
        {NULL,                    RGB_BLACK,  NULL,                              0, NULL      },
    };

    gContextMenu.originCoord  = coord;
    open_context_menu(coord, menuItems, 0, 0.0);
}

void open_morph_label_context_menu(tCoord coord, uint32_t morphIndex) {
    static tMenuItem menuItems[2];

    menuItems[0] = (tMenuItem){
        "Rename", RGB_GREY_3, action_rename_morph_label, morphIndex + NUM_MORPHS, NULL
    };
    menuItems[1] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };
    open_context_menu(coord, menuItems, 0, 0.0);
}

// ── Patch settings menus ────────────────────────────────────────────────────

void open_patch_type_context_menu(tCoord coord) {
    open_context_menu(coord, gPatchTypeItems, 0, 0.0);
}

void open_mono_poly_context_menu(tCoord coord) {
    open_context_menu(coord, gMonoPolyItems, 0, 0.0);
}

void open_voice_count_context_menu(tCoord coord) {
    static tMenuItem menuItems[33];
    static char      labels[32][4];
    static bool      labelsInitialised = false;
    uint32_t         slot              = gSlot;
    uint32_t         assignedVoices    = gAssignedVoices[slot];

    if (!labelsInitialised) {
        for (int i = 0; i < 32; i++) {
            snprintf(labels[i], sizeof(labels[i]), "%d", i + 1);
            menuItems[i].label   = labels[i];
            menuItems[i].action  = action_set_voice_count;
            menuItems[i].param   = (uint32_t)i;
            menuItems[i].subMenu = NULL;
        }

        menuItems[32]     = (tMenuItem){
            NULL, RGB_BLACK, NULL, 0, NULL
        };
        labelsInitialised = true;
    }

    for (int i = 0; i < 32; i++) {
        bool invalid = (assignedVoices > 0) && ((uint32_t)(i + 1) > assignedVoices);
        menuItems[i].colour = invalid ? (tRgb)RGB_RED_5 : (tRgb)RGB_GREY_3;
    }

    open_context_menu(coord, menuItems, 4, 0.0);
}

void open_variation_copy_menu(tCoord coord, uint32_t sourceVariation) {
    static tMenuItem menuItems[NUM_VARIATIONS + 1];
    static char      labels[NUM_VARIATIONS][32];
    int              count           = 0;
    uint32_t         targetVariation = 0;

    memset(&labels, 0, sizeof(labels));

    for (targetVariation = 0; targetVariation < NUM_VARIATIONS; targetVariation++) {
        if (targetVariation != sourceVariation) {
            if (targetVariation == VARIATION_INIT) {
                snprintf(labels[targetVariation], sizeof(labels[targetVariation]), "Copy to Init");
            } else {
                snprintf(labels[targetVariation], sizeof(labels[targetVariation]), "Copy to variation %u", targetVariation + 1);
            }
            menuItems[count].label   = labels[targetVariation];
            menuItems[count].colour  = (tRgb)RGB_GREY_3;
            menuItems[count].action  = action_copy_variation;
            menuItems[count].param   = (int)((sourceVariation << 4) | targetVariation);
            menuItems[count].subMenu = NULL;
            count++;
        }
    }

    menuItems[count] = (tMenuItem){
        NULL, RGB_BLACK, NULL, 0, NULL
    };

    open_context_menu(coord, menuItems, 0, 0.0);
}

#ifdef __cplusplus
}
#endif
