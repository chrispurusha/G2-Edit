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

#ifndef __GLOBAL_VARS_H__
#define __GLOBAL_VARS_H__

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"
#include "msgQueue.h"
#include <pthread.h>

extern const char *            patchTypeStrMap[patchTypeUserMax];
extern const char *            monoPolyStrMap[monoPolyMax];

//extern double                  gGlobalGuiScale;
extern _Atomic bool            gQuitAll;
extern void *                  gWindow;
extern _Atomic uint32_t        gLocation;
extern _Atomic bool            gReDraw;
extern bool                    gCommandKeyPressed;
extern tTopbarControl          gTopbarControls[topbarControlMax];
extern bool                    gShowOpenFileReadDialogue;
extern bool                    gShowOpenFileWriteDialogue;
//extern tScrollState            gScrollState;
extern tCableDragging          gCableDrag;
extern tHoverConnector         gHoverConnector;
extern tParamDragging          gParamDragging;
extern tModuleDragging         gModuleDrag;
extern tSelection              gSelection;
extern tRubberBand             gRubberBand;
extern tClipboard              gClipboard;
extern tMessageQueue           gCommandQueue;
extern uint32_t                gMorphGroupFocus;
extern _Atomic uint32_t        gSlot;
extern tPatchDescr             gPatchDescr[MAX_SLOTS];
extern tKnobArray              gKnobArray[MAX_SLOTS];  // TODO - Don't forget to nullify on new load
extern tGlobalKnob             gGlobalKnobArray[MAX_NUM_KNOBS];
extern tSelectedParam          gSelectedParam[MAX_SLOTS];
extern uint32_t                gMorphCount[MAX_SLOTS];
extern uint32_t                gNote2Size[MAX_SLOTS];
extern uint8_t                 gNote2[MAX_SLOTS][1024];
extern uint32_t                gAssignedVoices[MAX_SLOTS];
extern tControllerArray        gControllerArray[MAX_SLOTS];  // TODO - Don't forget to nullify on new load
extern uint32_t                gControllerCount[MAX_SLOTS];  // TODO - Don't forget to nullify on new load
extern uint32_t                gPatchNotesSize[MAX_SLOTS];
extern uint8_t                 gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1];
//extern _Atomic uint8_t     gPatchVersion[MAX_SLOTS];
//extern _Atomic uint8_t     gSlotEnabled[MAX_SLOTS];
//extern _Atomic uint8_t     gPerfVersion;
//extern _Atomic uint8_t     gMasterClock;
//extern _Atomic uint8_t     gMasterClockRunning;
extern tGlobalSettings         gGlobalSettings;
//extern _Atomic uint8_t     gPerfMode;
//extern char                gPatchName[MAX_SLOTS][PATCH_NAME_SIZE + 1];
extern _Atomic tCommsState     gCommsState;
//extern _Atomic uint32_t    gChangedSlot;
extern _Atomic uint8_t         gGlobalPage;
extern tNameEdit               gPatchNameEdit;
extern tModuleNameEdit         gModuleNameEdit;
extern tParamNameEdit          gParamNameEdit;
extern tMenuContext            gMenuContext;
extern tNameEdit               gSynthNameEdit;
extern tNameEdit               gPerfNameEdit;
extern tPerfSettings           gPerfSettings;
extern tPatchNotesEdit         gPatchNotesEdit;
extern tSynthSettings          gSynthSettings;
extern tPatchSettingsEdit      gPatchSettingsEdit;
extern tSettingsPanelRects     gSettingsPanelRects;
extern tPerfSettingsEdit       gPerfSettingsEdit;
extern tPerfSettingsPanelRects gPerfSettingsPanelRects;
extern tPatchSettingsEdit      gPatchParamsEdit;
extern tRectangle              gPatchParamClose;
extern tRectangle              gPatchParamSlots[MAX_SLOTS];
extern tRectangle              gPatchParamRects[pPCount];
extern tRectangle              gMorphLabelRect[NUM_MORPHS];
//extern _Atomic uint32_t    gHiddenCableMask;
extern bool                    gCablesTransparent;  // true = draw all cables semi-transparent
extern bool                    gCablesHideAll;
extern tResourceAlloc          gResourceAlloc[MAX_SLOTS];

extern tRectangle              gPatchNotesCloseRect;
extern tRectangle              gPatchNotesDiscardRect;
extern bool                    gTempoDragging;
extern bool                    gPerfTempoDragging;
extern bool                    gVibRateDragging;
extern bool                    gVibAmountDragging;
extern bool                    gGlideTimeDragging;
extern _Atomic uint64_t        gUsbTxTime;
extern _Atomic uint64_t        gUsbRxTime;
extern _Atomic bool            gBankBackupActive;
extern _Atomic bool            gBankBackupIsPerf;            // true = backing up a Performance Bank, false = Patch Bank
extern _Atomic bool            gBankBackupIsEverything;      // true = part of a "Backup Everything" sweep
extern _Atomic uint32_t        gBankBackupBank;              // 0-indexed bank currently being backed up
extern _Atomic uint32_t        gBankBackupLocation;          // 0-indexed location currently being requested
extern _Atomic uint32_t        gBankBackupWritten;           // count of patches actually written so far
extern _Atomic bool            gBankBackupComplete;          // set once finished, so the UI can show a completion alert
extern char                    gBankBackupResultMessage[256];
extern _Atomic bool            gSynthSettingsBackupComplete; // set once finished, so the UI can show a completion alert
extern char                    gSynthSettingsBackupResultMessage[256];
extern _Atomic bool            gBankRestoreActive;
extern _Atomic bool            gBankRestoreIsEverything;                                 // true = part of a "Restore Everything" sweep
extern _Atomic bool            gBankRestoreIsPerf;                                       // true = restoring a Performance Bank, false = Patch Bank
extern _Atomic uint32_t        gBankRestoreBank;                                         // 0-indexed bank currently being restored (destination)
extern _Atomic uint32_t        gBankRestoreLocation;                                     // 0-indexed location currently being written/cleared
extern _Atomic uint32_t        gBankRestoreWritten;                                      // count of patches actually written so far
extern _Atomic bool            gBankRestoreComplete;                                     // set once finished, so the UI can show a completion alert
extern char                    gBankRestoreResultMessage[256];
extern _Atomic bool            gStorePeekComplete;                                       // set once a pre-Store location lookup returns, polled by check_action_flags
extern _Atomic bool            gStorePeekFailed;                                         // true if the lookup round-trip itself failed (e.g. offline)
extern _Atomic bool            gStorePeekPopulated;                                      // true if the peeked location currently has a patch
extern _Atomic bool            gStorePeekIsPerf;                                         // true = storing/peeking a Performance, false = Patch (mirrors edit buffer's mode)
extern _Atomic uint32_t        gStorePeekBank;                                           // 0-indexed bank that was peeked (== the Store target)
extern _Atomic uint32_t        gStorePeekLocation;                                       // 0-indexed location that was peeked (== the Store target)
extern char                    gStorePeekName[CLAVIA_NAME_SIZE + 1];                     // name of what's currently there, if populated
extern _Atomic bool            gStorePatchComplete;                                      // set once Store itself finishes, so the UI can show a completion alert
extern char                    gStorePatchResultMessage[256];
extern _Atomic bool            gDeletePeekComplete;                                      // set once a pre-Delete location lookup returns, polled by check_action_flags
extern _Atomic bool            gDeletePeekFailed;                                        // true if the lookup round-trip itself failed (e.g. offline)
extern _Atomic bool            gDeletePeekPopulated;                                     // true if the peeked location currently has a patch/performance
extern _Atomic bool            gDeletePeekIsPerf;                                        // true = Performance Bank, false = Patch Bank
extern _Atomic uint32_t        gDeletePeekBank;                                          // 0-indexed bank that was peeked (== the Delete target)
extern _Atomic uint32_t        gDeletePeekLocation;                                      // 0-indexed location that was peeked (== the Delete target)
extern char                    gDeletePeekName[CLAVIA_NAME_SIZE + 1];                    // name of what's currently there, if populated
extern _Atomic bool            gDeleteComplete;                                          // set once Delete itself finishes, so the UI can show a completion alert
extern char                    gDeleteResultMessage[256];
extern _Atomic bool            gLoadPeekComplete;                                        // set once a pre-Load location lookup returns, polled by check_action_flags
extern _Atomic bool            gLoadPeekFailed;                                          // true if the lookup round-trip itself failed (e.g. offline)
extern _Atomic bool            gLoadPeekPopulated;                                       // true if the peeked location currently has a patch/performance to load
extern _Atomic bool            gLoadPeekIsPerf;                                          // true = Performance Bank, false = Patch Bank
extern _Atomic uint32_t        gLoadPeekBank;                                            // 0-indexed bank that was peeked (== the Load source)
extern _Atomic uint32_t        gLoadPeekLocation;                                        // 0-indexed location that was peeked (== the Load source)
extern char                    gLoadPeekName[CLAVIA_NAME_SIZE + 1];                      // name of what's currently there, if populated
extern _Atomic bool            gLoadComplete;                                            // set once Load itself finishes, so the UI can show a completion alert
extern _Atomic bool            gLoadFailed;                                              // true if the Load itself failed — success is shown by the redrawn canvas, not an alert
extern char                    gLoadResultMessage[256];
extern _Atomic bool            gSynthRestorePeekComplete;                                // set once find+parse of the latest backup file finishes, polled by check_action_flags
extern _Atomic bool            gSynthRestorePeekFailed;                                  // true if no backup file was found, or it couldn't be parsed
extern char                    gSynthRestorePeekErrorMessage[256];                       // reason for the failure above, if any
extern char                    gSynthRestorePeekFileName[64];                            // basename of the backup file that was found
extern char                    gSynthRestorePeekName[CLAVIA_NAME_SIZE + 1];              // the backup's own "Name" field, for display
extern _Atomic bool            gSynthRestoreComplete;                                    // set once the restore itself finishes, so the UI can show a completion alert
extern char                    gSynthRestoreResultMessage[256];
extern tNameTableEntry         gPatchNameTable[NUM_PATCH_BANKS][NUM_LOCATIONS_PER_BANK]; // filled by send_list_names_sweep() during init
extern tNameTableEntry         gPerfNameTable[NUM_PERF_BANKS][NUM_LOCATIONS_PER_BANK];
extern tRectangle              gParamRectangle[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS];
extern tDialMode               gDialMode;
extern pthread_mutex_t         gStringCopyMutex;

#ifdef __cplusplus
extern "C" {
#endif

//void patch_name_set(uint32_t slot, const char * name);
//void patch_name_get(uint32_t slot, char * name, size_t size);
void set_exclusive_button_highlight(tTopbarControlId first, tTopbarControlId last, tTopbarControlId active);

#ifdef __cplusplus
}
#endif

#endif // __GLOBAL_VARS_H__
