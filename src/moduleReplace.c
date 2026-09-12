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
// Notes: Docs/code-notes/moduleReplace.c.md - "// notes §k" refers there.

// notes §1

#include <string.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "msgQueue.h"
#include "moduleResourcesAccess.h"
#include "protocol.h"
#include "moduleReplace.h"
#include "menus.h"
#include "selection.h"
#include "undo.h"

bool module_can_replace(tModuleType moduleType) {
    return module_group_has_roles(module_group(moduleType));
}

uint32_t module_replace_candidates(tModuleType moduleType, tModuleType * out, uint32_t max) {
    tModuleGroup group = module_group(moduleType);
    uint32_t     count = 0;
    tModuleType  t     = (tModuleType)0;

    if ((out == NULL) || (module_can_replace(moduleType) == false)) {
        return 0;
    }

    // notes §2
    for (t = (tModuleType)0; (t < moduleTypeMax) && (count < max); t++) {
        if (  (t != moduleType) && (module_group(t) == group)
           && (gModuleProperties[t].name[0] != '\0')
           && (strcmp(gModuleProperties[t].name, "Unknown") != 0)) {
            out[count] = t;
            count++;
        }
    }

    return count;
}

// A complete image of one module, for undo and for putting things back if the replace is refused
// part-way through.
static void capture_module_image(const tModule * module, tClipboardModule * image) {
    uint32_t v = 0;
    uint32_t p = 0;
    uint32_t m = 0;
    uint32_t l = 0;

    memset(image, 0, sizeof(*image));
    image->type                = module->type;
    image->origIndex           = module->key.index;
    image->origColumn          = module->column;
    image->origRow             = module->row;
    image->colour              = module->colour;
    image->upRate              = module->upRate;
    image->excludeFromMutation = module->excludeFromMutation;
    COPY_STRING(image->name, module->name);

    for (v = 0; v < NUM_VARIATIONS_USB; v++) {
        for (p = 0; p < MAX_NUM_PARAMETERS; p++) {
            image->param[v][p] = module->param[v][p];
        }
    }

    for (m = 0; m < MAX_NUM_MODES; m++) {
        image->mode[m] = module->mode[m].value;
    }

    for (p = 0; p < MAX_NUM_PARAMETERS; p++) {
        image->paramNumLabels[p] = module->paramNumLabels[p];

        for (l = 0; l < MAX_NUM_LABELS; l++) {
            image->paramNameSet[p][l] = module->paramNameSet[p][l];
            COPY_STRING(image->paramName[p][l], module->paramName[p][l]);
        }
    }
}

static void restore_module_image(tModule * module, const tClipboardModule * image) {
    uint32_t v = 0;
    uint32_t p = 0;
    uint32_t m = 0;
    uint32_t l = 0;

    module->type                = image->type;
    module->colour              = image->colour;
    module->upRate              = image->upRate;
    module->excludeFromMutation = image->excludeFromMutation;
    module->actualParamCount    = module_param_count(image->type);
    module->modeCount           = module_mode_count(image->type);
    COPY_STRING(module->name, image->name);

    for (v = 0; v < NUM_VARIATIONS_USB; v++) {
        for (p = 0; p < MAX_NUM_PARAMETERS; p++) {
            module->param[v][p] = image->param[v][p];
        }
    }

    for (m = 0; m < MAX_NUM_MODES; m++) {
        module->mode[m].value = image->mode[m];
    }

    for (p = 0; p < MAX_NUM_PARAMETERS; p++) {
        module->paramNumLabels[p] = image->paramNumLabels[p];

        for (l = 0; l < MAX_NUM_LABELS; l++) {
            module->paramNameSet[p][l] = image->paramNameSet[p][l];
            COPY_STRING(module->paramName[p][l], image->paramName[p][l]);
        }
    }
}

// notes §3
static void carry_params_by_role(tModule * module, tModuleType oldType,
                                 const tClipboardModule * before) {
    uint32_t oldCount = module_param_count(oldType);
    uint32_t newCount = module_param_count(module->type);
    uint32_t p        = 0;
    uint32_t v        = 0;

    for (p = 0; (p < oldCount) && (p < MAX_NUM_PARAMETERS); p++) {
        const char * role     = module_role_for(oldType, roleKindParam, p);
        uint32_t     newIndex = module_index_for_role(module->type, roleKindParam, role);

        if ((role == NULL) || (newIndex == MODULE_ROLE_NONE) || (newIndex >= newCount)) {
            continue;
        }

        for (v = 0; v < NUM_VARIATIONS_USB; v++) {
            module->param[v][newIndex] = before->param[v][p];
        }
    }
}

// Moves one end of a cable from the old module's numbering to the new one's. Returns false when the
// role has no counterpart, which is the "(if possible)" in the manual's promise and the one case
// where a cable has to go.
static bool remap_end(tModuleType oldType, tModuleType newType, tConnectorDir dir,
                      uint32_t * ioCount) {
    tRoleKind    kind     = (dir == connectorDirIn) ? roleKindInput : roleKindOutput;
    const char * role     = module_role_for(oldType, kind, *ioCount);
    uint32_t     newIndex = module_index_for_role(newType, kind, role);

    if ((role == NULL) || (newIndex == MODULE_ROLE_NONE)) {
        return false;
    }
    *ioCount = newIndex;
    return true;
}

// Rewrites every cable touching this module, dropping the ones with nowhere to land. Collected
// first and applied afterwards because the rewrite changes the very keys the walk is iterating.
static void remap_cables(tModuleKey key, tModuleType oldType, tModuleType newType) {
    tCable   doomed[MAX_NUM_CABLES];
    tCable   rebuilt[MAX_NUM_CABLES];
    uint32_t doomedCount  = 0;
    uint32_t rebuiltCount = 0;
    uint32_t i            = 0;

    for (i = 0; i < MAX_NUM_CABLES; i++) {
        tCable *  cable  = get_cable_slot(key.slot, key.location, i);
        tCableKey moved  = {0};
        bool      onFrom = false;
        bool      onTo   = false;
        bool      ok     = true;

        if ((cable == NULL) || (cable->active == false)) {
            continue;
        }
        onFrom              = (cable->key.moduleFromIndex == key.index);
        onTo                = (cable->key.moduleToIndex == key.index);

        if ((onFrom == false) && (onTo == false)) {
            continue;
        }
        moved               = cable->key;

        // BOTH ENDS OF A cableLinkTypeFromInput CABLE ARE INPUTS — that link type is how the G2
        // records one input daisy-chained off another. The `to` end is always an input.
        if (onFrom) {
            tConnectorDir dir = (cable->key.linkType == (uint32_t)cableLinkTypeFromInput)
                                ? connectorDirIn : connectorDirOut;

            ok = remap_end(oldType, newType, dir, &moved.connectorFromIoCount);
        }

        if (ok && onTo) {
            ok = remap_end(oldType, newType, connectorDirIn, &moved.connectorToIoCount);
        }
        doomed[doomedCount] = *cable;
        doomedCount++;

        if (ok) {
            rebuilt[rebuiltCount]     = *cable;
            rebuilt[rebuiltCount].key = moved;
            rebuiltCount++;
        }
    }

    for (i = 0; i < doomedCount; i++) {
        delete_cable(doomed[i].key);
    }

    for (i = 0; i < rebuiltCount; i++) {
        write_cable(rebuilt[i].key, &rebuilt[i]);
    }
}

bool module_replace(tModuleKey key, tModuleType newType) {
    tModule *        module  = get_module(key);
    tModuleType      oldType = moduleTypeUnknown0;
    tClipboardModule before;
    tClipboardModule after;
    tMessageContent  msg     = {0};
    uint32_t         m       = 0;
    uint32_t         seen    = 0;
    uint32_t         i       = 0;

    if ((module == NULL) || (newType >= moduleTypeMax)) {
        return false;
    }
    oldType                  = module->type;

    if (  (oldType == newType) || (module_can_replace(oldType) == false)
       || (module_group(newType) != module_group(oldType))) {
        return false;
    }
    capture_module_image(module, &before);

    // The cable bracket has to be open before anything moves: it snapshots the location's whole
    // cable set, and the undo entry this pushes carries only the module.
    undo_begin_cable_edit(key.slot, key.location);

    module->type             = newType;
    module->actualParamCount = module_param_count(newType);
    module->modeCount        = module_mode_count(newType);

    // A module still carrying its type's default name is renamed to the new type's; one the user
    // has named keeps that name, which is the point of having named it.
    if (strcmp(module->name, gModuleProperties[oldType].name) == 0) {
        COPY_STRING(module->name, gModuleProperties[newType].name);
    }

    // Modes are the drop-down selectors, and they do NOT carry across: the instrument's own role
    // table has no entry for them in this group, so an FltLP's Slope does not become an FltHP's.
    // They start at their own defaults, which is not the same as starting at zero.
    for (m = 0; m < MAX_NUM_MODES; m++) {
        module->mode[m].value = 0;
    }

    for (i = 0, seen = 0; (i < array_size_mode_location_list()) && (seen < MAX_NUM_MODES); i++) {
        if (modeLocationList[i].moduleType == newType) {
            module->mode[seen].value = modeLocationList[i].defaultValue;
            seen++;
        }
    }

    init_params_on_module_all_variations(module, key.location);
    carry_params_by_role(module, oldType, &before);

    // A taller replacement pushes the rest of its column down. If the column cannot take it, put
    // everything back rather than leaving a half-done swap - including the cables, which is what
    // the bracket is for.
    if (shift_modules_down(key) == false) {
        restore_module_image(module, &before);
        undo_commit_cable_edit();
        synthlib_request_redraw();
        return false;
    }
    remap_cables(key, oldType, newType);

    capture_module_image(module, &after);
    undo_push_module_replace(key, &before, &after);
    undo_commit_cable_edit();

    msg.cmd  = eMsgCmdWritePatch;
    msg.slot = key.slot;
    msg_send(&gToUsbThread, &msg);

    update_module_up_rates();
    synthlib_request_redraw();
    return true;
}
