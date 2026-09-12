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
// Notes: Docs/code-notes/globalVars.c.md - "// notes §k" refers there.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "globalVars.h"

const char *                patchTypeStrMap[patchTypeUserMax]                        = {"No Cat", "Acoustic", "Sequencer", "Bass", "Classic", "Drum", "Fantasy", "Fx", "Lead", "Organ", "Pad", "Piano", "Synth", "Audio In", "User 1", "User 2"};
const char *                monoPolyStrMap[monoPolyMax]                              = {"Poly", "Mono", "Legato"};

// Process-wide, deliberately: it serialises string copies between THREADS, and has nothing to do
// with which G2 a thread is working on.
pthread_mutex_t             gStringCopyMutex                                         = PTHREAD_MUTEX_INITIALIZER;

// Outside the document - see globalVars.h for why.
bool                        gTempoDragging                                           = false;
bool                        gPerfTempoDragging                                       = false;
tTopbarControl              gTopbarControls[topbarControlMax]                        = {0};
tPerfSettingsPanelRects     gPerfSettingsPanelRects                                  = {0};
bool                        gVibAmountDragging                                       = false;
bool                        gVibRateDragging                                         = false;
bool                        gGlideTimeDragging                                       = false;
tRectangle                  gPatchParamRects[pPCount]                                = {0};
tPatchSettingsEdit          gPatchSettingsEdit                                       = {0};
tPerfSettingsEdit           gPerfSettingsEdit                                        = {0};
tPatchSettingsEdit          gPatchParamsEdit                                         = {0};
tPatchNotesEdit             gPatchNotesEdit                                          = {0};
tSettingsPanelRects         gSettingsPanelRects                                      = {0};
tNameTableEntry             gPatchNameTable[NUM_PATCH_BANKS][NUM_LOCATIONS_PER_BANK] = {0};
tNameTableEntry             gPerfNameTable[NUM_PERF_BANKS][NUM_LOCATIONS_PER_BANK]   = {0};

// notes §1
static tG2Document          gDefaultDocument;

_Thread_local tG2Document * gDoc                                                     = &gDefaultDocument;

// The non-zero defaults, applied to whichever document is CURRENT - the names below are macros onto
// it, which is also why this cannot take the document as a pointer and write through it.
static void document_defaults(void) {
    gLocation   = locationVa;
    gCommsState = eCommsNeverConnected;
    pthread_rwlock_init(&gDatabaseLock, NULL);

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        gLastDeviceMidiCC[slot]   = -1;
        gLastDeviceMidiChan[slot] = -1;
    }
}

// Before main(), and before the plug-in's first callback: the loader runs it when the image loads.
__attribute__((constructor)) static void default_document_init(void) {
    tG2Document * saved = gDoc;

    gDoc = &gDefaultDocument;
    document_defaults();
    gDoc = saved;
}

tG2Document * g2_document_create(void) {
    // calloc and not malloc: the zero-fill IS the default state, and at this size it comes straight
    // from fresh pages, so the four slots of module storage cost nothing until a patch uses them.
    tG2Document * doc   = (tG2Document *)calloc(1, sizeof(tG2Document));
    tG2Document * saved = gDoc;

    if (doc == NULL) {
        return NULL;
    }
    gDoc = doc;
    document_defaults();
    gDoc = saved;
    return doc;
}

void g2_document_destroy(tG2Document * doc) {
    tG2Document * saved = gDoc;

    if ((doc == NULL) || (doc == &gDefaultDocument)) {
        return;
    }
    gDoc = doc;
    pthread_rwlock_destroy(&gDatabaseLock);
    gDoc = (saved == doc) ? &gDefaultDocument : saved;
    free(doc);
}

void g2_document_select(tG2Document * doc) {
    gDoc = (doc != NULL) ? doc : &gDefaultDocument;
}

tG2Document * g2_document_current(void) {
    return gDoc;
}

// notes §2

bool variation_is_linked(uint32_t slot, uint32_t variation) {
    if ((slot >= MAX_SLOTS) || (variation >= VARIATION_INIT)) {
        return false;
    }
    return (gVariationLinks[slot] & (1u << variation)) != 0;
}

void variation_toggle_link(uint32_t slot, uint32_t variation) {
    if ((slot >= MAX_SLOTS) || (variation >= VARIATION_INIT)) {
        return; // Init is not a real variation — see globalVars.h
    }
    gVariationLinks[slot] ^= (1u << variation);
}

void variation_clear_links(uint32_t slot) {
    if (slot < MAX_SLOTS) {
        gVariationLinks[slot] = 0;
    }
}

void set_exclusive_button_highlight(tTopbarControlId first, tTopbarControlId last, tTopbarControlId active) {
    tTopbarControlId i = first;

    for (i = first; i <= last; i++) {
        gTopbarControls[i].colour = (tRgb)RGB_BACKGROUND_GREY;
    }

    gTopbarControls[active].colour = (tRgb)RGB_GREEN_ON;
}

#ifdef __cplusplus
}
#endif

// notes §3

// Cancelling an in-progress name edit. One memset each, on state defined in this file — they were
// in mouseHandle.c, which meant a GUI-less build could not dismiss an edit it could start.
void stop_patch_name_editing(void) {
    memset(&gPatchNameEdit, 0, sizeof(gPatchNameEdit));
}

void stop_module_name_editing(void) {
    memset(&gModuleNameEdit, 0, sizeof(gModuleNameEdit));
}

void stop_param_name_editing(void) {
    memset(&gParamNameEdit, 0, sizeof(gParamNameEdit));
}

void stop_perf_name_editing(void) {
    memset(&gPerfNameEdit, 0, sizeof(gPerfNameEdit));
}

void stop_synth_name_editing(void) {
    memset(&gSynthNameEdit, 0, sizeof(gSynthNameEdit));
}

void stop_patch_notes_editing(void) {
    memset(&gPatchNotesEdit, 0, sizeof(gPatchNotesEdit));
}
