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

//double                  gGlobalGuiScale                                                              = 2;
_Atomic uint32_t        gLocation                                                = locationVa;

bool                    gCommandKeyPressed                                       = false;

tTopbarControl          gTopbarControls[topbarControlMax]                        = {0};

const char *            patchTypeStrMap[patchTypeUserMax]                        = {"No Cat", "Acoustic", "Sequencer", "Bass", "Classic", "Drum", "Fantasy", "Fx", "Lead", "Organ", "Pad", "Piano", "Synth", "Audio In", "User 1", "User 2"};
const char *            monoPolyStrMap[monoPolyMax]                              = {"Poly", "Mono", "Legato"};


//tScrollState            gScrollState                                                                 = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, 0.0, NULL_RECTANGLE, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, 0.0, NULL_RECTANGLE};
tCableDragging          gCableDrag                                               = {0};
tHoverConnector         gHoverConnector                                          = {0};
tParamDragging          gParamDragging                                           = {0};
tParamFocus             gParamFocus                                              = {0};
_Atomic int32_t         gLastDeviceMidiCC[MAX_SLOTS]                             = {-1, -1, -1, -1};
int32_t                 gLastDeviceMidiChan[MAX_SLOTS]                           = {-1, -1, -1, -1};
uint32_t                gDeviceMidiCCCount                                       = 0;
tModuleDragging         gModuleDrag                                              = {0};
tSelection              gSelection                                               = {0};
tRubberBand             gRubberBand                                              = {0};
_Atomic uint32_t        gPatchGeneration[MAX_SLOTS]                              = {0};
tClipboard              gClipboard                                               = {0};
tMessageQueue           gToUsbThread                                             = {0};
tMessageQueue           gToGuiThread                                             = {0};
int                     gDeviceOpInProgress                                      = 0;
char                    gDeviceOpLabel[32]                                       = {0};
uint32_t                gMorphGroupFocus                                         = 0;
_Atomic uint32_t        gSlot                                                    = 0;
tPatchDescr             gPatchDescr[MAX_SLOTS]                                   = {0};
tKnobArray              gKnobArray[MAX_SLOTS]                                    = {0};
tGlobalKnob             gGlobalKnobArray[MAX_NUM_KNOBS]                          = {0};
tSelectedParam          gSelectedParam[MAX_SLOTS]                                = {0};
uint32_t                gMorphCount[MAX_SLOTS]                                   = {0};
uint32_t                gNote2Size[MAX_SLOTS]                                    = {0};
uint8_t                 gNote2[MAX_SLOTS][1024]                                  = {0};
uint32_t                gAssignedVoices[MAX_SLOTS]                               = {0};
tControllerArray        gControllerArray[MAX_SLOTS]                              = {0};
uint32_t                gControllerCount[MAX_SLOTS]                              = {0};
uint32_t                gPatchNotesSize[MAX_SLOTS]                               = {0};
uint8_t                 gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1]             = {0};
// Where each slot's patch (and the performance) was last opened from or saved to, so "Save" can
// write straight back without a dialogue. Empty until a file has been opened or saved this session.
char                    gSavedPatchPath[MAX_SLOTS][FILE_PATH_SIZE]               = {0};
char                    gSavedPerfPath[FILE_PATH_SIZE]                           = {0};
//_Atomic uint8_t     gPatchVersion[MAX_SLOTS]                                                     = {0};
tGlobalSettings         gGlobalSettings                                          = {0};                      // Note - should reflect settings in the G2
_Atomic tCommsState     gCommsState                                              = eCommsNeverConnected;
_Atomic uint8_t         gGlobalPage                                              = 0;
tNameEdit               gPatchNameEdit                                           = {0};
tModuleNameEdit         gModuleNameEdit                                          = {0};
tParamNameEdit          gParamNameEdit                                           = {0};
tMenuContext            gMenuContext                                             = {0};
tNameEdit               gSynthNameEdit                                           = {0};
tNameEdit               gPerfNameEdit                                            = {0};
tPerfSettings           gPerfSettings                                            = {0};                      // Note - should reflect settings in the G2
tPatchNotesEdit         gPatchNotesEdit                                          = {0};
tSynthSettings          gSynthSettings                                           = {0};                      // Note - should reflect settings in the G2
tPatchSettingsEdit      gPatchSettingsEdit                                       = {0};
tSettingsPanelRects     gSettingsPanelRects                                      = {0};
tPerfSettingsEdit       gPerfSettingsEdit                                        = {0};
tPerfSettingsPanelRects gPerfSettingsPanelRects                                  = {0};
tPatchSettingsEdit      gPatchParamsEdit                                         = {0};
tRectangle              gPatchParamClose                                         = {0};
bool                    gPatchParamClosePressed                                  = false;
tRectangle              gPatchParamSlots[MAX_SLOTS]                              = {0};
tRectangle              gPatchParamRects[pPCount]                                = {0};
tRectangle              gMorphLabelRect[NUM_MORPHS]                              = {0};
//_Atomic uint32_t       gHiddenCableMask                             = 0; // TODO - Send to G2 when changes
bool                    gCablesTransparent                                       = false;
bool                    gCablesHideAll                                           = false;
tResourceAlloc          gResourceAlloc[MAX_SLOTS]                                = {0};

tRectangle              gPatchNotesPanelRect                                     = {0};
tRectangle              gPatchNotesCloseRect                                     = {0};
bool                    gPatchNotesClosePressed                                  = false;
tRectangle              gPatchNotesDiscardRect                                   = {0};
bool                    gPatchNotesDiscardPressed                                = false;
bool                    gTempoDragging                                           = false;
bool                    gPerfTempoDragging                                       = false;
bool                    gVibRateDragging                                         = false;
bool                    gVibAmountDragging                                       = false;
bool                    gGlideTimeDragging                                       = false;
_Atomic uint64_t        gUsbTxTime                                               = 0;
_Atomic uint64_t        gUsbRxTime                                               = 0;
// gParamRectangle used to live here: a ~6MB [slot][location][module][param] table of every
// parameter widget's clickable rectangle, written by the renderer and read back by every hit test in
// the app. It is gone (2026-08-20). Hit-testing comes from the click-region registry, which the
// renderer already fills and which clear_click_regions() empties every frame, so a widget that is not
// drawn cannot be clicked without anyone having to blank a table to say so. See Docs/todo.txt.
pthread_mutex_t         gStringCopyMutex                                         = PTHREAD_MUTEX_INITIALIZER;
_Atomic bool            gBankBackupActive                                        = false;
_Atomic bool            gBankBackupIsPerf                                        = false;
_Atomic bool            gBankBackupIsEverything                                  = false;
_Atomic uint32_t        gBankBackupBank                                          = 0;
_Atomic uint32_t        gBankBackupLocation                                      = 0;
_Atomic uint32_t        gBankBackupWritten                                       = 0;
_Atomic bool            gBankRestoreActive                                       = false;
_Atomic bool            gBankRestoreIsEverything                                 = false;
_Atomic bool            gBankRestoreIsPerf                                       = false;
_Atomic uint32_t        gBankRestoreBank                                         = 0;
_Atomic uint32_t        gBankRestoreLocation                                     = 0;
_Atomic uint32_t        gBankRestoreWritten                                      = 0;
_Atomic bool            gStorePeekFailed                                         = false;
_Atomic bool            gStorePeekPopulated                                      = false;
_Atomic bool            gStorePeekIsPerf                                         = false;
_Atomic uint32_t        gStorePeekBank                                           = 0;
_Atomic uint32_t        gStorePeekLocation                                       = 0;
char                    gStorePeekName[CLAVIA_NAME_SIZE + 1]                     = {0};
_Atomic bool            gDeletePeekFailed                                        = false;
_Atomic bool            gDeletePeekPopulated                                     = false;
_Atomic bool            gDeletePeekIsPerf                                        = false;
_Atomic uint32_t        gDeletePeekBank                                          = 0;
_Atomic uint32_t        gDeletePeekLocation                                      = 0;
char                    gDeletePeekName[CLAVIA_NAME_SIZE + 1]                    = {0};
_Atomic bool            gLoadPeekFailed                                          = false;
_Atomic bool            gLoadPeekPopulated                                       = false;
_Atomic bool            gLoadPeekIsPerf                                          = false;
_Atomic uint32_t        gLoadPeekBank                                            = 0;
_Atomic uint32_t        gLoadPeekLocation                                        = 0;
char                    gLoadPeekName[CLAVIA_NAME_SIZE + 1]                      = {0};
_Atomic bool            gSynthRestorePeekFailed                                  = false;
char                    gSynthRestorePeekErrorMessage[256]                       = {0};
char                    gSynthRestorePeekFileName[64]                            = {0};
char                    gSynthRestorePeekName[CLAVIA_NAME_SIZE + 1]              = {0};
tNameTableEntry         gPatchNameTable[NUM_PATCH_BANKS][NUM_LOCATIONS_PER_BANK] = {0};
tNameTableEntry         gPerfNameTable[NUM_PERF_BANKS][NUM_LOCATIONS_PER_BANK]   = {0};

// One bit per variation, one mask per slot — see the note on variation_is_linked() in globalVars.h.
// A mask rather than an array of bools because "is the group empty" and "clear it" are then a single
// comparison and a single store, which is what most of the callers actually ask.
static uint32_t         gVariationLinks[MAX_SLOTS]                               = {0};

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

// Drag reference points, in RAW cursor coordinates. Shared rather than private to either file: the
// parameter-drag arm lives in canvasDrag.c (so the plug-in can use it) while the tempo, vibrato and
// glide drags stayed in mouseHandle.c, and both difference against the same two points.
//
// gDragStart* is fixed at the press; gDragPrev* advances with each event. Alt-held morph dragging
// measures from the START, because the value it is adjusting deliberately does not move — measuring
// from the previous event would collapse to nothing the moment the mouse paused.
double gDragStartX = 0.0;
double gDragStartY = 0.0;
double gDragPrevX  = 0.0;
double gDragPrevY  = 0.0;

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

