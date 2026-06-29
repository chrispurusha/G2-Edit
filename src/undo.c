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

#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "msgQueue.h"
#include "moduleResourcesAccess.h"
#include "protocol.h"
#include "selection.h"
#include "undo.h"

#define UNDO_STACK_SIZE    32

// ─── Internal payload types ────────────────────────────────────────────────

typedef struct {
    uint32_t fromIndex;
    uint32_t fromIoCount;
    uint32_t toIndex;
    uint32_t toIoCount;
    uint32_t linkType;
    uint32_t colour;
} tUndoCableEntry;

typedef struct {
    uint32_t           slot;
    uint32_t           location;
    uint32_t           moduleCount;
    tClipboardModule * modules;   // malloc'd
    uint32_t           cableCount;
    tUndoCableEntry *  cables;    // malloc'd
} tUndoDeletePayload;

typedef struct {
    uint32_t       slot;
    uint32_t       location;
    uint32_t       count;
    tUndoMoveEntry entries[MAX_NUM_MODULES];
} tUndoMovePayload;

typedef struct {
    uint32_t           slot;
    uint32_t           location;
    uint32_t           anchorCol;
    uint32_t           anchorRow;
    uint32_t           pastedCount;
    tModuleKey         pastedKeys[MAX_NUM_MODULES];
    uint32_t           clipModuleCount;
    tClipboardModule * clipModules;  // malloc'd copy
    uint32_t           clipCableCount;
    tClipboardCable *  clipCables;   // malloc'd copy
} tUndoPastePayload;

typedef struct {
    tModuleKey key;
    uint32_t   index;    // param index or mode index
    uint32_t   variation;
    uint32_t   oldValue;
    uint32_t   newValue;
    bool       isMode;
} tUndoParamPayload;

typedef enum {
    eUndoCmdMove,
    eUndoCmdDelete,
    eUndoCmdPaste,
    eUndoCmdParam,
} tUndoCmdType;

typedef struct {
    tUndoCmdType type;
    void *       payload;
} tUndoCommand;

// ─── Stack state ───────────────────────────────────────────────────────────

static tUndoCommand gUndoHistory[UNDO_STACK_SIZE];
static int32_t      gUndoCount  = 0;  // valid commands in history[0..count-1]
static int32_t      gUndoCursor = 0;  // # commands currently "applied"

// ─── Memory management ─────────────────────────────────────────────────────

static void free_command(tUndoCommand * cmd) {
    if (cmd->payload == NULL) {
        return;
    }

    switch (cmd->type) {
        case eUndoCmdMove:
            free(cmd->payload);
            break;

        case eUndoCmdDelete:
        {
            tUndoDeletePayload * p = cmd->payload;
            free(p->modules);
            free(p->cables);
            free(p);
            break;
        }

        case eUndoCmdPaste:
        {
            tUndoPastePayload * p = cmd->payload;
            free(p->clipModules);
            free(p->clipCables);
            free(p);
            break;
        }

        case eUndoCmdParam:
            free(cmd->payload);
            break;
    }
    cmd->payload = NULL;
}

// ─── Stack push ────────────────────────────────────────────────────────────

static void stack_push(tUndoCmdType type, void * payload) {
    // Discard any redo history above the cursor
    for (int32_t i = gUndoCursor; i < gUndoCount; i++) {
        free_command(&gUndoHistory[i]);
    }

    gUndoCount                       = gUndoCursor;

    // Evict the oldest entry if full
    if (gUndoCount == UNDO_STACK_SIZE) {
        free_command(&gUndoHistory[0]);
        memmove(&gUndoHistory[0], &gUndoHistory[1],
                (UNDO_STACK_SIZE - 1) * sizeof(tUndoCommand));
        gUndoCount--;
        gUndoCursor--;
    }
    gUndoHistory[gUndoCount].type    = type;
    gUndoHistory[gUndoCount].payload = payload;
    gUndoCount++;
    gUndoCursor++;
}

// ─── Push helpers (public) ─────────────────────────────────────────────────

void undo_push_delete_selection(void) {
    if (gSelection.count == 0) {
        return;
    }
    uint32_t             slot     = (uint32_t)gSlot;
    uint32_t             location = (uint32_t)gLocation;

    tUndoDeletePayload * p        = malloc(sizeof(tUndoDeletePayload));

    if (!p) {
        return;
    }
    p->slot        = slot;
    p->location    = location;
    p->modules     = malloc(gSelection.count * sizeof(tClipboardModule));
    p->cables      = malloc(MAX_NUM_CABLES * sizeof(tUndoCableEntry));

    if (!p->modules || !p->cables) {
        free(p->modules);
        free(p->cables);
        free(p);
        return;
    }
    p->moduleCount = 0;
    p->cableCount  = 0;

    // Snapshot each selected module
    for (uint32_t si = 0; si < gSelection.count; si++) {
        tModule *          mod = get_module(gSelection.keys[si]);

        if (!mod) {
            continue;
        }
        tClipboardModule * cm  = &p->modules[p->moduleCount++];
        memset(cm, 0, sizeof(*cm));
        cm->type       = mod->type;
        cm->origIndex  = mod->key.index;
        cm->origColumn = mod->column;
        cm->origRow    = mod->row;
        cm->dColumn    = 0;
        cm->dRow       = 0;
        cm->colour     = mod->colour;
        cm->upRate     = mod->upRate;
        cm->isLed      = mod->isLed;
        COPY_STRING(cm->name, mod->name);
        memcpy(cm->param, mod->param, sizeof(cm->param));

        for (uint32_t m = 0; m < MAX_NUM_MODES; m++) {
            cm->mode[m] = mod->mode[m].value;
        }

        for (uint32_t pi = 0; pi < MAX_NUM_PARAMETERS; pi++) {
            cm->paramNumLabels[pi] = mod->paramNumLabels[pi];

            for (uint32_t l = 0; l < MAX_NUM_LABELS; l++) {
                cm->paramNameSet[pi][l] = mod->paramNameSet[pi][l];
                COPY_STRING(cm->paramName[pi][l], mod->paramName[pi][l]);
            }
        }
    }

    // Snapshot all cables that touch any selected module
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable *          cable   = get_cable_slot(slot, location, i);

        if (!cable || !cable->active) {
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

        if (!fromSel && !toSel) {
            continue;
        }
        tUndoCableEntry * ce      = &p->cables[p->cableCount++];
        ce->fromIndex   = cable->key.moduleFromIndex;
        ce->fromIoCount = cable->key.connectorFromIoCount;
        ce->toIndex     = cable->key.moduleToIndex;
        ce->toIoCount   = cable->key.connectorToIoCount;
        ce->linkType    = cable->key.linkType;
        ce->colour      = cable->colour;
    }

    // Shrink cable allocation to actual usage
    if (p->cableCount > 0) {
        tUndoCableEntry * shrunk = realloc(p->cables, p->cableCount * sizeof(tUndoCableEntry));

        if (shrunk) {
            p->cables = shrunk;
        }
    } else {
        free(p->cables);
        p->cables = NULL;
    }
    stack_push(eUndoCmdDelete, p);
}

void undo_push_move(uint32_t slot, uint32_t location, tUndoMoveEntry * entries, uint32_t count) {
    if (count == 0) {
        return;
    }
    tUndoMovePayload * p = malloc(sizeof(tUndoMovePayload));

    if (!p) {
        return;
    }
    p->slot     = slot;
    p->location = location;
    p->count    = count;
    memcpy(p->entries, entries, count * sizeof(tUndoMoveEntry));
    stack_push(eUndoCmdMove, p);
}

void undo_push_paste(uint32_t slot, uint32_t location,
                     uint32_t anchorCol, uint32_t anchorRow,
                     tModuleKey * pastedKeys, uint32_t pastedCount,
                     tClipboardModule * clipModules, uint32_t clipModuleCount,
                     tClipboardCable * clipCables, uint32_t clipCableCount) {
    tUndoPastePayload * p = malloc(sizeof(tUndoPastePayload));

    if (!p) {
        return;
    }
    p->slot            = slot;
    p->location        = location;
    p->anchorCol       = anchorCol;
    p->anchorRow       = anchorRow;
    p->pastedCount     = pastedCount < MAX_NUM_MODULES ? pastedCount : MAX_NUM_MODULES;
    memcpy(p->pastedKeys, pastedKeys, p->pastedCount * sizeof(tModuleKey));

    p->clipModuleCount = clipModuleCount;
    p->clipModules     = NULL;

    if (clipModuleCount > 0) {
        p->clipModules = malloc(clipModuleCount * sizeof(tClipboardModule));

        if (p->clipModules) {
            memcpy(p->clipModules, clipModules, clipModuleCount * sizeof(tClipboardModule));
        }
    }
    p->clipCableCount  = clipCableCount;
    p->clipCables      = NULL;

    if (clipCableCount > 0) {
        p->clipCables = malloc(clipCableCount * sizeof(tClipboardCable));

        if (p->clipCables) {
            memcpy(p->clipCables, clipCables, clipCableCount * sizeof(tClipboardCable));
        }
    }
    stack_push(eUndoCmdPaste, p);
}

// ─── Apply helpers ─────────────────────────────────────────────────────────

static void apply_move(tUndoMovePayload * p, bool isUndo) {
    for (uint32_t i = 0; i < p->count; i++) {
        tModule * mod = get_module(p->entries[i].key);

        if (!mod) {
            continue;
        }

        if (isUndo) {
            mod->column = p->entries[i].oldColumn;
            mod->row    = p->entries[i].oldRow;
        } else {
            mod->column = p->entries[i].newColumn;
            mod->row    = p->entries[i].newRow;
        }
        send_module_move_msg(mod);
    }

    gReDraw = true;
}

static void recreate_module(tUndoDeletePayload * p, tClipboardModule * cm) {
    tModuleKey      key      = {p->slot, p->location, cm->origIndex};

    // Abort if the original slot is occupied by something else
    tModule *       existing = get_module_slot(p->slot, p->location, cm->origIndex);

    if (existing && existing->active) {
        LOG_ERROR("undo_delete: index %u occupied\n", cm->origIndex);
        return;
    }
    tModule         module   = {0};
    module.key               = key;
    module.type              = cm->type;
    module.column            = cm->origColumn;
    module.row               = cm->origRow;
    module.colour            = cm->colour;
    module.upRate            = cm->upRate;
    module.isLed             = cm->isLed;
    COPY_STRING(module.name, cm->name);

    tMessageContent msg      = {0};
    msg.cmd                  = eMsgCmdWriteModule;
    msg.slot                 = p->slot;
    msg.moduleData.moduleKey = key;
    msg.moduleData.type      = cm->type;
    msg.moduleData.row       = cm->origRow;
    msg.moduleData.column    = cm->origColumn;
    msg.moduleData.colour    = cm->colour;
    msg.moduleData.upRate    = cm->upRate;
    msg.moduleData.isLed     = cm->isLed;
    msg.moduleData.modeCount = module_mode_count(cm->type);

    for (uint32_t m = 0; m < MAX_NUM_MODES; m++) {
        msg.moduleData.mode[m] = (uint8_t)cm->mode[m];
    }

    COPY_STRING(msg.moduleData.name, cm->name);
    msg_send(&gCommandQueue, &msg);

    write_module(key, &module);

    tModule * dbMod      = get_module(key);

    if (!dbMod) {
        return;
    }
    uint32_t  paramCount = module_param_count(cm->type);
    uint32_t  modeCount  = module_mode_count(cm->type);

    for (uint32_t v = 0; v < NUM_VARIATIONS_USB; v++) {
        for (uint32_t pi = 0; pi < paramCount; pi++) {
            dbMod->param[v][pi] = cm->param[v][pi];
            send_param_value(p->slot, key, pi, v, cm->param[v][pi].value);
        }
    }

    for (uint32_t m = 0; m < modeCount; m++) {
        dbMod->mode[m].value = cm->mode[m];
        send_mode_value(p->slot, key, m, cm->mode[m]);
    }

    for (uint32_t pi = 0; pi < paramCount; pi++) {
        dbMod->paramNumLabels[pi] = cm->paramNumLabels[pi];

        for (uint32_t l = 0; l < MAX_NUM_LABELS; l++) {
            dbMod->paramNameSet[pi][l] = cm->paramNameSet[pi][l];
            COPY_STRING(dbMod->paramName[pi][l], cm->paramName[pi][l]);

            if (cm->paramNameSet[pi][l]) {
                tMessageContent nmsg = {0};
                nmsg.cmd                       = eMsgCmdSetParamLabel;
                nmsg.slot                      = p->slot;
                nmsg.paramLabelData.moduleKey  = key;
                nmsg.paramLabelData.paramIndex = pi;
                COPY_STRING(nmsg.paramLabelData.name, cm->paramName[pi][l]);
                msg_send(&gCommandQueue, &nmsg);
            }
        }
    }
}

static void apply_delete_undo(tUndoDeletePayload * p) {
    // Re-create all modules at their original indices
    for (uint32_t i = 0; i < p->moduleCount; i++) {
        recreate_module(p, &p->modules[i]);
    }

    // Re-create all cables
    for (uint32_t i = 0; i < p->cableCount; i++) {
        tUndoCableEntry * ce       = &p->cables[i];

        tCableKey         cableKey = {0};
        cableKey.slot                      = p->slot;
        cableKey.location                  = p->location;
        cableKey.moduleFromIndex           = ce->fromIndex;
        cableKey.connectorFromIoCount      = ce->fromIoCount;
        cableKey.linkType                  = ce->linkType;
        cableKey.moduleToIndex             = ce->toIndex;
        cableKey.connectorToIoCount        = ce->toIoCount;

        tCable            cable    = {0};
        cable.key                          = cableKey;
        cable.colour                       = ce->colour;
        cable.active                       = true;
        write_cable(cableKey, &cable);

        tMessageContent   msg      = {0};
        msg.cmd                            = eMsgCmdWriteCable;
        msg.slot                           = p->slot;
        msg.cableData.location             = p->location;
        msg.cableData.colour               = ce->colour;
        msg.cableData.moduleFromIndex      = ce->fromIndex;
        msg.cableData.connectorFromIoIndex = ce->fromIoCount;
        msg.cableData.linkType             = ce->linkType;
        msg.cableData.moduleToIndex        = ce->toIndex;
        msg.cableData.connectorToIoIndex   = ce->toIoCount;
        msg_send(&gCommandQueue, &msg);
    }

    update_module_up_rates();
    gReDraw = true;
}

static void apply_delete_redo(tUndoDeletePayload * p) {
    for (uint32_t i = 0; i < p->moduleCount; i++) {
        tModuleKey key = {p->slot, p->location, p->modules[i].origIndex};
        tModule *  mod = get_module(key);

        if (mod && mod->active) {
            delete_module_and_cables(key);
        }
    }

    selection_clear();
    update_module_up_rates();
    gReDraw = true;
}

static void apply_paste_undo(tUndoPastePayload * p) {
    for (uint32_t i = 0; i < p->pastedCount; i++) {
        tModule * mod = get_module(p->pastedKeys[i]);

        if (mod && mod->active) {
            delete_module_and_cables(p->pastedKeys[i]);
        }
    }

    selection_clear();
    update_module_up_rates();
    gReDraw = true;
}

static void apply_paste_redo(tUndoPastePayload * p) {
    if (!p->clipModules || p->clipModuleCount == 0) {
        return;
    }
    paste_snapshot(p->slot, p->location,
                   p->anchorCol, p->anchorRow,
                   p->clipModules, p->clipModuleCount,
                   p->clipCables, p->clipCableCount);
}

void undo_push_param_change(tModuleKey key, uint32_t paramIndex, uint32_t variation,
                            uint32_t oldValue, uint32_t newValue) {
    if (oldValue == newValue) {
        return;
    }
    tUndoParamPayload * p = malloc(sizeof(tUndoParamPayload));

    if (!p) {
        return;
    }
    p->key       = key;
    p->index     = paramIndex;
    p->variation = variation;
    p->oldValue  = oldValue;
    p->newValue  = newValue;
    p->isMode    = false;
    stack_push(eUndoCmdParam, p);
}

void undo_push_mode_change(tModuleKey key, uint32_t modeIndex,
                           uint32_t oldValue, uint32_t newValue) {
    if (oldValue == newValue) {
        return;
    }
    tUndoParamPayload * p = malloc(sizeof(tUndoParamPayload));

    if (!p) {
        return;
    }
    p->key       = key;
    p->index     = modeIndex;
    p->variation = 0;
    p->oldValue  = oldValue;
    p->newValue  = newValue;
    p->isMode    = true;
    stack_push(eUndoCmdParam, p);
}

static void apply_param(tUndoParamPayload * p, bool isUndo) {
    tModule * mod   = get_module(p->key);

    if (!mod) {
        return;
    }
    uint32_t  value = isUndo ? p->oldValue : p->newValue;

    if (p->isMode) {
        mod->mode[p->index].value = value;
        send_mode_value(p->key.slot, p->key, p->index, value);
    } else {
        mod->param[p->variation][p->index].value = (uint8_t)value;
        send_param_value(p->key.slot, p->key, p->index, p->variation, value);
    }
    gReDraw = true;
}

// ─── Public undo / redo ────────────────────────────────────────────────────

bool undo_can_undo(void) {
    return gUndoCursor > 0;
}

bool undo_can_redo(void) {
    return gUndoCursor < gUndoCount;
}

void undo_undo(void) {
    if (!undo_can_undo()) {
        return;
    }
    gUndoCursor--;
    tUndoCommand * cmd = &gUndoHistory[gUndoCursor];

    switch (cmd->type) {
        case eUndoCmdMove:
            apply_move(cmd->payload, true);
            break;

        case eUndoCmdDelete:
            apply_delete_undo(cmd->payload);
            break;

        case eUndoCmdPaste:
            apply_paste_undo(cmd->payload);
            break;

        case eUndoCmdParam:
            apply_param(cmd->payload, true);
            break;
    }
}

void undo_redo(void) {
    if (!undo_can_redo()) {
        return;
    }
    tUndoCommand * cmd = &gUndoHistory[gUndoCursor];
    gUndoCursor++;

    switch (cmd->type) {
        case eUndoCmdMove:
            apply_move(cmd->payload, false);
            break;

        case eUndoCmdDelete:
            apply_delete_redo(cmd->payload);
            break;

        case eUndoCmdPaste:
            apply_paste_redo(cmd->payload);
            break;

        case eUndoCmdParam:
            apply_param(cmd->payload, false);
            break;
    }
}

void undo_clear(void) {
    for (int32_t i = 0; i < gUndoCount; i++) {
        free_command(&gUndoHistory[i]);
    }

    gUndoCount  = 0;
    gUndoCursor = 0;
}
