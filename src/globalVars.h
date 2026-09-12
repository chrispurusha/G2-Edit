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
// Notes: Docs/code-notes/globalVars.h.md - "// notes §k" refers there.

#ifndef __GLOBAL_VARS_H__
#define __GLOBAL_VARS_H__

// No GLFW here: nothing this header declares uses it, and pulling it in made globalVars.h — which
// the sound engine needs — impossible to include from a build with no window system, such as the
// VST3 plug-in. The files that genuinely draw include it themselves.

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"
#include "msgQueue.h"
#include "synthlibGlobals.h" // synthlib_quit_requested()/synthlib_request_redraw()/synthlib_window()/synthlib_dial_mode() etc.
#include <pthread.h>

extern const char *            patchTypeStrMap[patchTypeUserMax];
extern const char *            monoPolyStrMap[monoPolyMax];

extern pthread_mutex_t         gStringCopyMutex;

// notes §1
extern bool                    gTempoDragging;
extern bool                    gPerfTempoDragging;
extern tTopbarControl          gTopbarControls[topbarControlMax];
extern tPerfSettingsPanelRects gPerfSettingsPanelRects;
extern bool                    gVibAmountDragging;
extern bool                    gVibRateDragging;
extern bool                    gGlideTimeDragging;
extern tRectangle              gPatchParamRects[pPCount];
extern tPatchSettingsEdit      gPatchSettingsEdit;
extern tPerfSettingsEdit       gPerfSettingsEdit;
extern tPatchSettingsEdit      gPatchParamsEdit;
extern tPatchNotesEdit         gPatchNotesEdit;
extern tSettingsPanelRects     gSettingsPanelRects;
extern tNameTableEntry         gPatchNameTable[NUM_PATCH_BANKS][NUM_LOCATIONS_PER_BANK];  // filled by send_list_names_sweep() during init
extern tNameTableEntry         gPerfNameTable[NUM_PERF_BANKS][NUM_LOCATIONS_PER_BANK];

// notes §2
typedef struct tG2Document {
    //extern double                  gGlobalGuiScale;
    _Atomic uint32_t gLocation;
    bool             gCommandKeyPressed;
    //extern tScrollState            gScrollState;
    tCableDragging   gCableDrag;
    tHoverConnector  gHoverConnector;
    tParamDragging   gParamDragging;
    tParamFocus      gParamFocus;

    // The last MIDI CC the SYNTH reported receiving (SUB_RESPONSE_MIDI_CC). Written on the USB thread,
    // read by the UI thread for MIDI Learn. -1 until one arrives. See parse_midi_cc() for the open
    // question of whether the byte is the CC number or its value.
    _Atomic int32_t  gLastDeviceMidiCC[MAX_SLOTS];
    int32_t          gLastDeviceMidiChan[MAX_SLOTS];
    uint32_t         gDeviceMidiCCCount;
    tModuleDragging  gModuleDrag;
    tSelection       gSelection;
    tRubberBand      gRubberBand;

    // notes §3
    _Atomic uint32_t gPatchGeneration[MAX_SLOTS];
    tClipboard       gClipboard;
    tMessageQueue    gToUsbThread;         // GUI thread -> USB thread (commands); USB thread blocks on it
    tMessageQueue    gToGuiThread;         // USB thread -> GUI thread (results); poll-drained in the render loop

    // Busy state for in-flight whole-slot device ops (load/save/new patch). UI-thread only: set when the
    // op is enqueued (device_op_begin), cleared when its completion response is drained (device_op_end).
    // While > 0 the render loop dims the canvas + shows gDeviceOpLabel and mouse_button swallows clicks.
    int              gDeviceOpInProgress;
    char             gDeviceOpLabel[32];
    uint32_t         gMorphGroupFocus;
    _Atomic uint32_t gSlot;
    tPatchDescr      gPatchDescr[MAX_SLOTS];
    tKnobArray       gKnobArray[MAX_SLOTS];
    tGlobalKnob      gGlobalKnobArray[MAX_NUM_KNOBS];
    tSelectedParam   gSelectedParam[MAX_SLOTS];
    uint32_t         gMorphCount[MAX_SLOTS];
    uint32_t         gNote2Size[MAX_SLOTS];
    // Bumped every time a current-note reply lands, so a caller can wait for a FRESH one rather than
    // reading whatever happens to be in the buffer — see the DEVNOTES backdoor command.
    _Atomic uint32_t gNote2Updates;
    uint8_t          gNote2[MAX_SLOTS][1024];
    uint32_t         gAssignedVoices[MAX_SLOTS];
    tControllerArray gControllerArray[MAX_SLOTS];
    uint32_t         gControllerCount[MAX_SLOTS];         // nullified alongside gKnobArray/gControllerArray by clear_slot_data() (dataBase.c) on every new patch load
    uint32_t         gPatchNotesSize[MAX_SLOTS];
    uint8_t          gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1];
    char             gSavedPatchPath[MAX_SLOTS][FILE_PATH_SIZE];
    char             gSavedPerfPath[FILE_PATH_SIZE];
    //extern _Atomic uint8_t     gPatchVersion[MAX_SLOTS];
    //extern _Atomic uint8_t     gSlotEnabled[MAX_SLOTS];
    //extern _Atomic uint8_t     gPerfVersion;
    //extern _Atomic uint8_t     gMasterClock;
    //extern _Atomic uint8_t     gMasterClockRunning;
    tGlobalSettings     gGlobalSettings;
    // notes §4
    _Atomic bool        gDeviceConnected;

    _Atomic tCommsState gCommsState;


    //extern _Atomic uint32_t    gChangedSlot;
    _Atomic uint8_t gGlobalPage;
    tNameEdit       gPatchNameEdit;
    tModuleNameEdit gModuleNameEdit;
    tParamNameEdit  gParamNameEdit;
    tMenuContext    gMenuContext;
    tNameEdit       gSynthNameEdit;
    tNameEdit       gPerfNameEdit;
    tPerfSettings   gPerfSettings;
    tSynthSettings  gSynthSettings;
    tRectangle      gPatchParamClose;
    bool            gPatchParamClosePressed;
    tRectangle      gPatchParamSlots[MAX_SLOTS];
    tRectangle      gMorphLabelRect[NUM_MORPHS];
    //extern _Atomic uint32_t    gHiddenCableMask;
    bool            gCablesTransparent;           // true = draw all cables semi-transparent
    bool            gCablesHideAll;
    tResourceAlloc  gResourceAlloc[MAX_SLOTS];

    // The whole panel, not just its buttons. The mouse handler needs it to tell "clicked away from the
    // editor, dismiss it" from "clicked on the editor's own chrome", which is not a distinction it could
    // make while the only rectangles it had were the two buttons.
    tRectangle       gPatchNotesPanelRect;
    tRectangle       gPatchNotesCloseRect;
    bool             gPatchNotesClosePressed;
    tRectangle       gPatchNotesDiscardRect;
    bool             gPatchNotesDiscardPressed;
    _Atomic uint64_t gUsbTxTime;
    _Atomic uint64_t gUsbRxTime;
    _Atomic bool     gBankBackupActive;
    _Atomic bool     gBankBackupIsPerf;                                                // true = backing up a Performance Bank, false = Patch Bank
    _Atomic bool     gBankBackupIsEverything;                                          // true = part of a "Backup Everything" sweep
    _Atomic uint32_t gBankBackupBank;                                                  // 0-indexed bank currently being backed up
    _Atomic uint32_t gBankBackupLocation;                                              // 0-indexed location currently being requested
    _Atomic uint32_t gBankBackupWritten;                                               // count of patches actually written so far
    _Atomic bool     gBankRestoreActive;
    _Atomic bool     gBankRestoreIsEverything;                                         // true = part of a "Restore Everything" sweep
    _Atomic bool     gBankRestoreIsPerf;                                               // true = restoring a Performance Bank, false = Patch Bank
    _Atomic uint32_t gBankRestoreBank;                                                 // 0-indexed bank currently being restored (destination)
    _Atomic uint32_t gBankRestoreLocation;                                             // 0-indexed location currently being written/cleared
    _Atomic uint32_t gBankRestoreWritten;                                              // count of patches actually written so far
    _Atomic bool     gStorePeekFailed;                                                 // true if the lookup round-trip itself failed (e.g. offline)
    _Atomic bool     gStorePeekPopulated;                                              // true if the peeked location currently has a patch
    _Atomic bool     gStorePeekIsPerf;                                                 // true = storing/peeking a Performance, false = Patch (mirrors edit buffer's mode)
    _Atomic uint32_t gStorePeekBank;                                                   // 0-indexed bank that was peeked (== the Store target)
    _Atomic uint32_t gStorePeekLocation;                                               // 0-indexed location that was peeked (== the Store target)
    char             gStorePeekName[CLAVIA_NAME_SIZE + 1];                             // name of what's currently there, if populated
    _Atomic bool     gDeletePeekFailed;                                                // true if the lookup round-trip itself failed (e.g. offline)
    _Atomic bool     gDeletePeekPopulated;                                             // true if the peeked location currently has a patch/performance
    _Atomic bool     gDeletePeekIsPerf;                                                // true = Performance Bank, false = Patch Bank
    _Atomic uint32_t gDeletePeekBank;                                                  // 0-indexed bank that was peeked (== the Delete target)
    _Atomic uint32_t gDeletePeekLocation;                                              // 0-indexed location that was peeked (== the Delete target)
    char             gDeletePeekName[CLAVIA_NAME_SIZE + 1];                            // name of what's currently there, if populated
    _Atomic bool     gLoadPeekFailed;                                                  // true if the lookup round-trip itself failed (e.g. offline)
    _Atomic bool     gLoadPeekPopulated;                                               // true if the peeked location currently has a patch/performance to load
    _Atomic bool     gLoadPeekIsPerf;                                                  // true = Performance Bank, false = Patch Bank
    _Atomic uint32_t gLoadPeekBank;                                                    // 0-indexed bank that was peeked (== the Load source)
    _Atomic uint32_t gLoadPeekLocation;                                                // 0-indexed location that was peeked (== the Load source)
    char             gLoadPeekName[CLAVIA_NAME_SIZE + 1];                              // name of what's currently there, if populated
    _Atomic bool     gSynthRestorePeekFailed;                                          // true if no backup file was found, or it couldn't be parsed
    char             gSynthRestorePeekErrorMessage[256];                               // reason for the failure above, if any
    char             gSynthRestorePeekFileName[64];                                    // basename of the backup file that was found
    char             gSynthRestorePeekName[CLAVIA_NAME_SIZE + 1];                      // the backup's own "Name" field, for display

    // Drag reference points in raw cursor coordinates — see globalVars.c. Shared between canvasDrag.c's
    // parameter dragging and mouseHandle.c's tempo/vibrato/glide dragging.
    double gDragStartX;
    double gDragStartY;
    double gDragPrevX;
    double gDragPrevY;

    // ── The patch database, from dataBase.c ────────────────────────────────────────────────────────
    // Four slots of patch, which is what performance mode is: an instance of the plug-in is a whole
    // performance, not one slot of somebody else's. See dataBase.c for the lock's discipline.
    tModule          gModule[MAX_SLOTS][locationMax][MAX_NUM_MODULES];
    tCable           gCable[MAX_SLOTS][locationMax][MAX_NUM_CABLES];
    pthread_rwlock_t gDatabaseLock;

    // Formerly file-static in globalVars.c - see variation_is_linked() below.
    uint32_t         gVariationLinks[MAX_SLOTS];

    // ── Not reached through a macro: named here only by the code that owns them ──────────────────
    // Which bank of the sound engine's state this document plays through - soundEngine.c. Always 0
    // in the application, which has one engine; each plug-in instance claims its own.
    uint32_t engineIndex;

    // The plug-in's stand-in for gToGuiThread, which it never initialises: one deferred menu action
    // at a time, held per document so an editor drains only its own - see g2AppStubs.c.
    tMessageContent hostedGuiMsg;
    bool            hostedGuiMsgValid;
} tG2Document;

#ifdef __cplusplus
extern "C" {
#endif

extern _Thread_local tG2Document * gDoc;

// A fresh document with the application's defaults, or NULL. About 90 MB, nearly all of it the four
// slots of module storage - calloc'd, so only the pages a patch actually touches are ever committed.
tG2Document * g2_document_create(void);
void g2_document_destroy(tG2Document * doc);

// Makes `doc` the calling thread's current document; NULL selects the built-in one.
void g2_document_select(tG2Document * doc);
tG2Document * g2_document_current(void);

#ifdef __cplusplus
}
#endif

#define gLocation                        (gDoc->gLocation)
#define gCommandKeyPressed               (gDoc->gCommandKeyPressed)
#define gCableDrag                       (gDoc->gCableDrag)
#define gHoverConnector                  (gDoc->gHoverConnector)
#define gParamDragging                   (gDoc->gParamDragging)
#define gParamFocus                      (gDoc->gParamFocus)
#define gLastDeviceMidiCC                (gDoc->gLastDeviceMidiCC)
#define gLastDeviceMidiChan              (gDoc->gLastDeviceMidiChan)
#define gDeviceMidiCCCount               (gDoc->gDeviceMidiCCCount)
#define gModuleDrag                      (gDoc->gModuleDrag)
#define gSelection                       (gDoc->gSelection)
#define gRubberBand                      (gDoc->gRubberBand)
#define gPatchGeneration                 (gDoc->gPatchGeneration)
#define gClipboard                       (gDoc->gClipboard)
#define gToUsbThread                     (gDoc->gToUsbThread)
#define gToGuiThread                     (gDoc->gToGuiThread)
#define gDeviceOpInProgress              (gDoc->gDeviceOpInProgress)
#define gDeviceOpLabel                   (gDoc->gDeviceOpLabel)
#define gMorphGroupFocus                 (gDoc->gMorphGroupFocus)
#define gSlot                            (gDoc->gSlot)
#define gPatchDescr                      (gDoc->gPatchDescr)
#define gKnobArray                       (gDoc->gKnobArray)
#define gGlobalKnobArray                 (gDoc->gGlobalKnobArray)
#define gSelectedParam                   (gDoc->gSelectedParam)
#define gMorphCount                      (gDoc->gMorphCount)
#define gNote2Size                       (gDoc->gNote2Size)
#define gNote2Updates                    (gDoc->gNote2Updates)
#define gNote2                           (gDoc->gNote2)
#define gAssignedVoices                  (gDoc->gAssignedVoices)
#define gControllerArray                 (gDoc->gControllerArray)
#define gControllerCount                 (gDoc->gControllerCount)
#define gPatchNotesSize                  (gDoc->gPatchNotesSize)
#define gPatchNotes                      (gDoc->gPatchNotes)
#define gSavedPatchPath                  (gDoc->gSavedPatchPath)
#define gSavedPerfPath                   (gDoc->gSavedPerfPath)
#define gGlobalSettings                  (gDoc->gGlobalSettings)
#define gDeviceConnected                 (gDoc->gDeviceConnected)
#define gCommsState                      (gDoc->gCommsState)
#define gGlobalPage                      (gDoc->gGlobalPage)
#define gPatchNameEdit                   (gDoc->gPatchNameEdit)
#define gModuleNameEdit                  (gDoc->gModuleNameEdit)
#define gParamNameEdit                   (gDoc->gParamNameEdit)
#define gMenuContext                     (gDoc->gMenuContext)
#define gSynthNameEdit                   (gDoc->gSynthNameEdit)
#define gPerfNameEdit                    (gDoc->gPerfNameEdit)
#define gPerfSettings                    (gDoc->gPerfSettings)
#define gSynthSettings                   (gDoc->gSynthSettings)
#define gPatchParamClose                 (gDoc->gPatchParamClose)
#define gPatchParamClosePressed          (gDoc->gPatchParamClosePressed)
#define gPatchParamSlots                 (gDoc->gPatchParamSlots)
#define gMorphLabelRect                  (gDoc->gMorphLabelRect)
#define gCablesTransparent               (gDoc->gCablesTransparent)
#define gCablesHideAll                   (gDoc->gCablesHideAll)
#define gResourceAlloc                   (gDoc->gResourceAlloc)
#define gPatchNotesPanelRect             (gDoc->gPatchNotesPanelRect)
#define gPatchNotesCloseRect             (gDoc->gPatchNotesCloseRect)
#define gPatchNotesClosePressed          (gDoc->gPatchNotesClosePressed)
#define gPatchNotesDiscardRect           (gDoc->gPatchNotesDiscardRect)
#define gPatchNotesDiscardPressed        (gDoc->gPatchNotesDiscardPressed)
#define gUsbTxTime                       (gDoc->gUsbTxTime)
#define gUsbRxTime                       (gDoc->gUsbRxTime)
#define gBankBackupActive                (gDoc->gBankBackupActive)
#define gBankBackupIsPerf                (gDoc->gBankBackupIsPerf)
#define gBankBackupIsEverything          (gDoc->gBankBackupIsEverything)
#define gBankBackupBank                  (gDoc->gBankBackupBank)
#define gBankBackupLocation              (gDoc->gBankBackupLocation)
#define gBankBackupWritten               (gDoc->gBankBackupWritten)
#define gBankRestoreActive               (gDoc->gBankRestoreActive)
#define gBankRestoreIsEverything         (gDoc->gBankRestoreIsEverything)
#define gBankRestoreIsPerf               (gDoc->gBankRestoreIsPerf)
#define gBankRestoreBank                 (gDoc->gBankRestoreBank)
#define gBankRestoreLocation             (gDoc->gBankRestoreLocation)
#define gBankRestoreWritten              (gDoc->gBankRestoreWritten)
#define gStorePeekFailed                 (gDoc->gStorePeekFailed)
#define gStorePeekPopulated              (gDoc->gStorePeekPopulated)
#define gStorePeekIsPerf                 (gDoc->gStorePeekIsPerf)
#define gStorePeekBank                   (gDoc->gStorePeekBank)
#define gStorePeekLocation               (gDoc->gStorePeekLocation)
#define gStorePeekName                   (gDoc->gStorePeekName)
#define gDeletePeekFailed                (gDoc->gDeletePeekFailed)
#define gDeletePeekPopulated             (gDoc->gDeletePeekPopulated)
#define gDeletePeekIsPerf                (gDoc->gDeletePeekIsPerf)
#define gDeletePeekBank                  (gDoc->gDeletePeekBank)
#define gDeletePeekLocation              (gDoc->gDeletePeekLocation)
#define gDeletePeekName                  (gDoc->gDeletePeekName)
#define gLoadPeekFailed                  (gDoc->gLoadPeekFailed)
#define gLoadPeekPopulated               (gDoc->gLoadPeekPopulated)
#define gLoadPeekIsPerf                  (gDoc->gLoadPeekIsPerf)
#define gLoadPeekBank                    (gDoc->gLoadPeekBank)
#define gLoadPeekLocation                (gDoc->gLoadPeekLocation)
#define gLoadPeekName                    (gDoc->gLoadPeekName)
#define gSynthRestorePeekFailed          (gDoc->gSynthRestorePeekFailed)
#define gSynthRestorePeekErrorMessage    (gDoc->gSynthRestorePeekErrorMessage)
#define gSynthRestorePeekFileName        (gDoc->gSynthRestorePeekFileName)
#define gSynthRestorePeekName            (gDoc->gSynthRestorePeekName)
#define gDragStartX                      (gDoc->gDragStartX)
#define gDragStartY                      (gDoc->gDragStartY)
#define gDragPrevX                       (gDoc->gDragPrevX)
#define gDragPrevY                       (gDoc->gDragPrevY)
#define gModule                          (gDoc->gModule)
#define gCable                           (gDoc->gCable)
#define gDatabaseLock                    (gDoc->gDatabaseLock)
#define gVariationLinks                  (gDoc->gVariationLinks)

// notes §5
static inline bool device_ready(void) {
    return gCommsState == eCommsOnLine;
}


#ifdef __cplusplus
extern "C" {
#endif

//void patch_name_set(uint32_t slot, const char * name);
//void patch_name_get(uint32_t slot, char * name, size_t size);
void set_exclusive_button_highlight(tTopbarControlId first, tTopbarControlId last, tTopbarControlId active);

// notes §6
bool variation_is_linked(uint32_t slot, uint32_t variation);
void variation_toggle_link(uint32_t slot, uint32_t variation);
void variation_clear_links(uint32_t slot);

#ifdef __cplusplus
}
#endif


// Cancel an in-progress name edit — see globalVars.c.
void stop_patch_name_editing(void);
void stop_module_name_editing(void);
void stop_param_name_editing(void);
void stop_perf_name_editing(void);
void stop_synth_name_editing(void);
void stop_patch_notes_editing(void);

#endif // __GLOBAL_VARS_H__
