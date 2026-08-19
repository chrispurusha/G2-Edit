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

#include "defs.h"
#include "synthlibDefs.h"
#include "globalVars.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"

tModule gModule[MAX_SLOTS][locationMax][MAX_NUM_MODULES] = {0};

tCable  gCable[MAX_SLOTS][locationMax][MAX_NUM_CABLES]   = {0};

// Called from both threads — no internal locking.
tModule * get_module_slot(uint32_t slot, uint32_t location, uint32_t index) {
    if ((slot < MAX_SLOTS) && (location < (uint32_t)locationMax) && (index < MAX_NUM_MODULES)) {
        return &gModule[slot][location][index];
    }
    return NULL;
}

// Called from both threads — no internal locking.
tModule * get_module(tModuleKey key) {
    tModule * module = get_module_slot(key.slot, key.location, key.index);

    if ((module != NULL) && module->active) {
        return module;
    }
    return NULL;
}

void write_module(tModuleKey key, tModule * module) {
    module->key    = key;
    module->active = true;

    // Same reason as the patch-parse path: .dir/.type drive every cable lookup and must not
    // depend on the module having been drawn. Cheap, and idempotent if already populated.
    populate_module_connectors(module);

    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
        gModule[key.slot][key.location][key.index] = *module;
    } else {
        LOG_ERROR("Module key out of bounds slot=%u location=%u index=%u\n", key.slot, key.location, key.index);
    }
}

void delete_module(tModuleKey key) {
    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax) && (key.index < MAX_NUM_MODULES)) {
        memset(&gModule[key.slot][key.location][key.index], 0, sizeof(tModule));
    }
}

void database_delete_modules_by_slot(uint32_t slot) {
    if (slot < MAX_SLOTS) {
        for (uint32_t location = 0; location < (uint32_t)locationMax; location++) {
            for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
                memset(&gModule[slot][location][index], 0, sizeof(tModule));
            }
        }
    }
}

uint32_t count_active_modules(uint32_t slot) {
    uint32_t count = 0;

    if (slot >= MAX_SLOTS) {
        return 0;
    }

    // Only count VA and FX — morph-location modules exist on all slots (sent by G2 via USB)
    // but are not written to patch/perf files and don't indicate real patch content.
    for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
        if (gModule[slot][locationVa][index].active) {
            count++;
        }

        if (gModule[slot][locationFx][index].active) {
            count++;
        }
    }

    return count;
}

bool slot_has_modules(uint32_t slot) {
    return count_active_modules(slot) > 0;
}

void database_clear_modules(void) {
    memset(gModule, 0, sizeof(gModule));
}

void dump_modules(void) {
    uint32_t count = 0;

    LOG_DEBUG("\n\nDump modules\n");

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        for (uint32_t location = 0; location < (uint32_t)locationMax; location++) {
            for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
                if (gModule[slot][location][index].active) {
                    tModule * module = &gModule[slot][location][index];
                    LOG_DEBUG("Slot %u Location %u Index %u Type %u Name %s Row %u Column %u\n",
                              module->key.slot, module->key.location, module->key.index,
                              module->type, module->name, module->row, module->column);
                    count++;
                }
            }
        }
    }

    LOG_DEBUG("\nModule Count=%u\n\n", count);
}

// Called from both threads — no internal locking.
tCable * get_cable_slot(uint32_t slot, uint32_t location, uint32_t index) {
    if ((slot < MAX_SLOTS) && (location < (uint32_t)locationMax) && (index < MAX_NUM_CABLES)) {
        return &gCable[slot][location][index];
    }
    return NULL;
}

tCable * get_cable(tCableKey key) {
    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax)) {
        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable * cable = &gCable[key.slot][key.location][i];

            if (cable->active && (memcmp(&cable->key, &key, sizeof(tCableKey)) == 0)) {
                return cable;
            }
        }
    }
    return NULL;
}

// Called from both threads — no internal locking.
void write_cable(tCableKey key, tCable * cable) {
    tCable * existing = get_cable(key);

    if (existing != NULL) {
        existing->colour = cable->colour;
        return;
    }

    if ((key.slot < MAX_SLOTS) && (key.location < (uint32_t)locationMax)) {
        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            if (!gCable[key.slot][key.location][i].active) {
                gCable[key.slot][key.location][i]        = *cable;
                gCable[key.slot][key.location][i].key    = key;
                gCable[key.slot][key.location][i].active = true;
                return;
            }
        }

        LOG_ERROR("write_cable: no free slot slot=%u location=%u\n", key.slot, key.location);
    }
}

void delete_cable(tCableKey key) {
    tCable * cable = get_cable(key);

    if (cable != NULL) {
        memset(cable, 0, sizeof(tCable));
    }
}

void dump_cables(void) {
    LOG_DEBUG("\n\nDump cables\n");

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
            for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
                tCable * cable = &gCable[slot][loc][i];

                if (cable->active) {
                    LOG_DEBUG("Slot %u Location %u Colour %u From %u/%u LinkType %u To %u/%u\n",
                              cable->key.slot, cable->key.location, cable->colour,
                              cable->key.moduleFromIndex, cable->key.connectorFromIoCount,
                              cable->key.linkType, cable->key.moduleToIndex,
                              cable->key.connectorToIoCount);
                }
            }
        }
    }

    LOG_DEBUG("\n\n");
}

void database_delete_cables_by_slot(uint32_t slot) {
    if (slot < MAX_SLOTS) {
        for (uint32_t loc = 0; loc < (uint32_t)locationMax; loc++) {
            memset(gCable[slot][loc], 0, sizeof(gCable[slot][loc]));
        }
    }
}

void database_clear_cables(void) {
    memset(gCable, 0, sizeof(gCable));
}

int find_io_count_from_index(tModule * module, tConnectorDir dir, int index) {
    int ioCount = -1;

    for (int i = 0; i <= index; i++) {
        //LOG_DEBUG("%d is type %d\n", i, module->connector[i].dir);
        if (module->connector[i].dir == dir) {
            ioCount++;
        }
    }

    return ioCount;  // Index does not match the direction
}

int find_index_from_io_count(tModule * module, tConnectorDir dir, int targetCount) {
    int count = 0;

    //LOG_DEBUG("%s find index num connectors %u\n", gModuleProperties[module->type].name, gModuleProperties[module->type].numConnectors);
    for (uint32_t index = 0; index < module_connector_count(module->type); index++) {
        if (module->connector[index].dir == dir) {
            if (count == targetCount) {
                return index;
            }
            count++;
        }
    }

    return -1;  // Not found
}

void init_database(void) {
}

#ifdef __cplusplus
}
#endif

// Clearing everything a slot holds - modules, cables and the per-slot arrays that live in
// globalVars rather than in the database proper. Was in graphics.c, which made it unreachable
// from anything without a GUI; nothing in it ever touched the screen.

// The patch name a file implies: its basename with the extension dropped. Moved from graphics.c —
// it is string handling, and the plug-in sets the name the same way when it opens a file.
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

// A brand-new, empty patch. Moved here from mouseHandle.c, where its own comment asked where it
// really belonged: nothing in it touches a window, and clear_slot_data() below came here for the
// same reason — a GUI-less build could not reach it otherwise.
//
// THE VST3 PLUG-IN NEEDS THIS BEFORE IT HAS LOADED ANYTHING. Without it gPatchDescr is all zeroes,
// and a zero barPosition means "Voice Area takes no height" — which put the pane divider hard
// against the top of an empty plug-in window instead of partway down it.
void init_patch(uint32_t slot) {
    memset(&gPatchDescr[slot], 0, sizeof(gPatchDescr[0]));
    gPatchDescr[slot].voiceCount      = 1;
    // Voice Area pane height in pixels — see splitView.h. The reference's own default is 4000, which
    // is larger than any window and so clamps to "Voice Area takes everything"; G2-Edit shows BOTH
    // areas on a new patch instead (owner's call), because the divider is the point of the window
    // and a new patch is exactly when you want to see it. Patches loaded from file or from the G2
    // carry their own value and are untouched by this.
    gPatchDescr[slot].barPosition     = 300;
    gPatchDescr[slot].unknown3        = 2;    // unknown9 in Delphi
    gPatchDescr[slot].visible[0]      = 1;
    gPatchDescr[slot].visible[1]      = 1;
    gPatchDescr[slot].visible[2]      = 1;
    gPatchDescr[slot].visible[3]      = 1;
    gPatchDescr[slot].visible[4]      = 1;
    gPatchDescr[slot].visible[5]      = 1;
    gPatchDescr[slot].visible[6]      = 1;
    gPatchDescr[slot].monoPoly        = 1;
    gPatchDescr[slot].activeVariation = 0;
    gPatchDescr[slot].category        = 0;

    database_delete_cables_by_slot(slot);
    database_delete_modules_by_slot(slot);
    gMorphCount[slot]                 = 8; // Check default!?

    // database_delete_modules_by_slot() above zeroes every module for this slot, including the
    // morph-groups pseudo-module (locationMorph/patchModuleMorph) that every patch structurally has.
    // render_morph_groups() reads it via get_module(), which returns NULL unless active is set, so
    // without this the morph knobs would silently render nothing for a freshly-initialised patch —
    // reactivate it here with its key set, same as parse_module_list() does when a real patch
    // arrives from the device.
    {
        tModule * morphModule = get_module_slot(slot, (uint32_t)locationMorph, patchModuleMorph);

        morphModule->active = true;
        morphModule->key    = (tModuleKey){
            slot, (uint32_t)locationMorph, patchModuleMorph
        };

        // Each morph group's "mode" param (index i+NUM_MORPHS) is 0 for plain manual-knob mode,
        // nonzero for assigned-to-a-fixed-source mode (see render_morph_groups()'s isKnob check) —
        // default every group to its fixed source (Wheel, Vel, Keyb, ... per morphStrMap[i]) rather
        // than leaving all 8 as unnamed knobs, across every variation so it holds regardless of
        // which one is active.
        for (uint32_t variation = 0; variation < NUM_VARIATIONS_USB; variation++) {
            for (uint32_t morph = 0; morph < NUM_MORPHS; morph++) {
                morphModule->param[variation][morph + NUM_MORPHS].value = 1;
            }
        }
    }
    gNote2Size[slot]       = 0;
    gControllerCount[slot] = 0;            // Seems to default to 2, so might need to set up defaults
    gPatchNotesSize[slot]  = 0;
    memset(&(gKnobArray[slot]), 0, sizeof(gKnobArray[0]));
    memset(gNote2[slot], 0, sizeof(gNote2[0]));
    memset(&(gControllerArray[slot]), 0, sizeof(gControllerArray[0]));
    memset(gPatchNotes[slot], 0, sizeof(gPatchNotes[0]));

    COPY_STRING(gGlobalSettings.slot[slot].patchName, "Init");
}

void clear_slot_data(uint32_t slot) {
    if (slot < MAX_SLOTS) {
        gPatchGeneration[slot]++;   // everything keyed to this slot's modules is now stale
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
