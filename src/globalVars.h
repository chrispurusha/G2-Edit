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

//extern double                  gGlobalGuiScale;
extern _Atomic uint32_t        gLocation;
extern bool                    gCommandKeyPressed;
extern tTopbarControl          gTopbarControls[topbarControlMax];
//extern tScrollState            gScrollState;
extern tCableDragging          gCableDrag;
extern tHoverConnector         gHoverConnector;
extern tParamDragging          gParamDragging;
extern tParamFocus             gParamFocus;

// The last MIDI CC the SYNTH reported receiving (SUB_RESPONSE_MIDI_CC). Written on the USB thread,
// read by the UI thread for MIDI Learn. -1 until one arrives. See parse_midi_cc() for the open
// question of whether the byte is the CC number or its value.
extern _Atomic int32_t         gLastDeviceMidiCC[MAX_SLOTS];
extern int32_t                 gLastDeviceMidiChan[MAX_SLOTS];
extern uint32_t                gDeviceMidiCCCount;
extern tModuleDragging         gModuleDrag;
extern tSelection              gSelection;
extern tRubberBand             gRubberBand;

// BUMPED WHENEVER A SLOT'S CONTENTS ARE REPLACED — a patch parsed into it, or the slot cleared. The
// UI thread watches it to know that anything it was holding a module key for is gone; see
// selection_validate(). Atomic because patches arrive on the USB thread and this is read on the
// render thread, which is also why the selection is not simply cleared at the point of the load.
extern _Atomic uint32_t        gPatchGeneration[MAX_SLOTS];
extern tClipboard              gClipboard;
extern tMessageQueue           gToUsbThread; // GUI thread -> USB thread (commands); USB thread blocks on it
extern tMessageQueue           gToGuiThread; // USB thread -> GUI thread (results); poll-drained in the render loop

// Busy state for in-flight whole-slot device ops (load/save/new patch). UI-thread only: set when the
// op is enqueued (device_op_begin), cleared when its completion response is drained (device_op_end).
// While > 0 the render loop dims the canvas + shows gDeviceOpLabel and mouse_button swallows clicks.
extern int                     gDeviceOpInProgress;
extern char                    gDeviceOpLabel[32];
extern uint32_t                gMorphGroupFocus;
extern _Atomic uint32_t        gSlot;
extern tPatchDescr             gPatchDescr[MAX_SLOTS];
extern tKnobArray              gKnobArray[MAX_SLOTS];
extern tGlobalKnob             gGlobalKnobArray[MAX_NUM_KNOBS];
extern tSelectedParam          gSelectedParam[MAX_SLOTS];
extern uint32_t                gMorphCount[MAX_SLOTS];
extern uint32_t                gNote2Size[MAX_SLOTS];
extern uint8_t                 gNote2[MAX_SLOTS][1024];
extern uint32_t                gAssignedVoices[MAX_SLOTS];
extern tControllerArray        gControllerArray[MAX_SLOTS];
extern uint32_t                gControllerCount[MAX_SLOTS]; // nullified alongside gKnobArray/gControllerArray by clear_slot_data() (dataBase.c) on every new patch load
extern uint32_t                gPatchNotesSize[MAX_SLOTS];
extern uint8_t                 gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1];
extern char                    gSavedPatchPath[MAX_SLOTS][FILE_PATH_SIZE];
extern char                    gSavedPerfPath[FILE_PATH_SIZE];
//extern _Atomic uint8_t     gPatchVersion[MAX_SLOTS];
//extern _Atomic uint8_t     gSlotEnabled[MAX_SLOTS];
//extern _Atomic uint8_t     gPerfVersion;
//extern _Atomic uint8_t     gMasterClock;
//extern _Atomic uint8_t     gMasterClockRunning;
extern tGlobalSettings         gGlobalSettings;
//extern _Atomic uint8_t     gPerfMode;
//extern char                gPatchName[MAX_SLOTS][PATCH_NAME_SIZE + 1];
// IS THE G2 ANSWERING? Set the moment it replies to the patch-version request, which is the point
// at which traffic is demonstrably working both ways. Deliberately SEPARATE from gCommsState:
// that is a load SEQUENCE (waiting-ready, awaiting-sync-decision, online-and-loaded) and it moves
// on through several values while the device is sitting there answering perfectly well. The lamp
// wants the simple question, and asking the sequence gave the wrong answer twice over — "Offline"
// for the 8 seconds of the initial pull, and "Offline" again while the editor asks the user to
// choose between their offline edits and the G2's patches.
extern _Atomic bool            gDeviceConnected;

extern _Atomic tCommsState     gCommsState;

// IS IT SAFE TO ISSUE A DEVICE OPERATION? One predicate, because this was open-coded as
// "gCommsState == eCommsOnLine" in more than thirty places — twelve bulk operations in usbComms.c,
// thirteen menu actions, the menu enable/disable gates, MIDI-to-synth forwarding and the backdoor —
// and a rule spread over thirty copies is a rule that gets changed in twenty-nine.
//
// Note what it is NOT: it is not "is a G2 attached". eCommsOnLine is only reached once the initial
// pull has finished, so this asks "has the editor finished loading and is it safe to start
// something big" — which is exactly right for a Backup or a bank Store, and exactly wrong for the
// Online lamp, which should light as soon as the device answers. Splitting those two meanings is
// what this predicate exists to make possible.
static inline bool device_ready(void) {
    return gCommsState == eCommsOnLine;
}

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
extern bool                    gPatchParamClosePressed;
extern tRectangle              gPatchParamSlots[MAX_SLOTS];
extern tRectangle              gPatchParamRects[pPCount];
extern tRectangle              gMorphLabelRect[NUM_MORPHS];
//extern _Atomic uint32_t    gHiddenCableMask;
extern bool                    gCablesTransparent;  // true = draw all cables semi-transparent
extern bool                    gCablesHideAll;
extern tResourceAlloc          gResourceAlloc[MAX_SLOTS];

// The whole panel, not just its buttons. The mouse handler needs it to tell "clicked away from the
// editor, dismiss it" from "clicked on the editor's own chrome", which is not a distinction it could
// make while the only rectangles it had were the two buttons.
extern tRectangle              gPatchNotesPanelRect;
extern tRectangle              gPatchNotesCloseRect;
extern bool                    gPatchNotesClosePressed;
extern tRectangle              gPatchNotesDiscardRect;
extern bool                    gPatchNotesDiscardPressed;
extern bool                    gTempoDragging;
extern bool                    gPerfTempoDragging;
extern bool                    gVibRateDragging;
extern bool                    gVibAmountDragging;
extern bool                    gGlideTimeDragging;
extern _Atomic uint64_t        gUsbTxTime;
extern _Atomic uint64_t        gUsbRxTime;
extern _Atomic bool            gBankBackupActive;
extern _Atomic bool            gBankBackupIsPerf;                                        // true = backing up a Performance Bank, false = Patch Bank
extern _Atomic bool            gBankBackupIsEverything;                                  // true = part of a "Backup Everything" sweep
extern _Atomic uint32_t        gBankBackupBank;                                          // 0-indexed bank currently being backed up
extern _Atomic uint32_t        gBankBackupLocation;                                      // 0-indexed location currently being requested
extern _Atomic uint32_t        gBankBackupWritten;                                       // count of patches actually written so far
extern _Atomic bool            gBankRestoreActive;
extern _Atomic bool            gBankRestoreIsEverything;                                 // true = part of a "Restore Everything" sweep
extern _Atomic bool            gBankRestoreIsPerf;                                       // true = restoring a Performance Bank, false = Patch Bank
extern _Atomic uint32_t        gBankRestoreBank;                                         // 0-indexed bank currently being restored (destination)
extern _Atomic uint32_t        gBankRestoreLocation;                                     // 0-indexed location currently being written/cleared
extern _Atomic uint32_t        gBankRestoreWritten;                                      // count of patches actually written so far
extern _Atomic bool            gStorePeekFailed;                                         // true if the lookup round-trip itself failed (e.g. offline)
extern _Atomic bool            gStorePeekPopulated;                                      // true if the peeked location currently has a patch
extern _Atomic bool            gStorePeekIsPerf;                                         // true = storing/peeking a Performance, false = Patch (mirrors edit buffer's mode)
extern _Atomic uint32_t        gStorePeekBank;                                           // 0-indexed bank that was peeked (== the Store target)
extern _Atomic uint32_t        gStorePeekLocation;                                       // 0-indexed location that was peeked (== the Store target)
extern char                    gStorePeekName[CLAVIA_NAME_SIZE + 1];                     // name of what's currently there, if populated
extern _Atomic bool            gDeletePeekFailed;                                        // true if the lookup round-trip itself failed (e.g. offline)
extern _Atomic bool            gDeletePeekPopulated;                                     // true if the peeked location currently has a patch/performance
extern _Atomic bool            gDeletePeekIsPerf;                                        // true = Performance Bank, false = Patch Bank
extern _Atomic uint32_t        gDeletePeekBank;                                          // 0-indexed bank that was peeked (== the Delete target)
extern _Atomic uint32_t        gDeletePeekLocation;                                      // 0-indexed location that was peeked (== the Delete target)
extern char                    gDeletePeekName[CLAVIA_NAME_SIZE + 1];                    // name of what's currently there, if populated
extern _Atomic bool            gLoadPeekFailed;                                          // true if the lookup round-trip itself failed (e.g. offline)
extern _Atomic bool            gLoadPeekPopulated;                                       // true if the peeked location currently has a patch/performance to load
extern _Atomic bool            gLoadPeekIsPerf;                                          // true = Performance Bank, false = Patch Bank
extern _Atomic uint32_t        gLoadPeekBank;                                            // 0-indexed bank that was peeked (== the Load source)
extern _Atomic uint32_t        gLoadPeekLocation;                                        // 0-indexed location that was peeked (== the Load source)
extern char                    gLoadPeekName[CLAVIA_NAME_SIZE + 1];                      // name of what's currently there, if populated
extern _Atomic bool            gSynthRestorePeekFailed;                                  // true if no backup file was found, or it couldn't be parsed
extern char                    gSynthRestorePeekErrorMessage[256];                       // reason for the failure above, if any
extern char                    gSynthRestorePeekFileName[64];                            // basename of the backup file that was found
extern char                    gSynthRestorePeekName[CLAVIA_NAME_SIZE + 1];              // the backup's own "Name" field, for display
extern tNameTableEntry         gPatchNameTable[NUM_PATCH_BANKS][NUM_LOCATIONS_PER_BANK]; // filled by send_list_names_sweep() during init
extern tNameTableEntry         gPerfNameTable[NUM_PERF_BANKS][NUM_LOCATIONS_PER_BANK];
extern pthread_mutex_t         gStringCopyMutex;

#ifdef __cplusplus
extern "C" {
#endif

//void patch_name_set(uint32_t slot, const char * name);
//void patch_name_get(uint32_t slot, char * name, size_t size);
void set_exclusive_button_highlight(tTopbarControlId first, tTopbarControlId last, tTopbarControlId active);

// ── Linked variations ───────────────────────────────────────────────────────
//
// The set of variations that a parameter edit fans out to as well as the selected one — built by
// shift-clicking variation buttons in the topbar, one set per slot.
//
// Membership is EXPLICIT and independent of which variation is selected, which is what lets you
// audition another variation without dismantling the group. The selected variation always receives
// its own edits whether or not it is a member; being a member is what makes it keep receiving them
// once you have moved on. If the selected variation were only ever an implicit member, selecting a
// different one would silently drop it from the group with nothing on screen having changed.
//
// Editor-only state: deliberately NOT part of tPatchDescr, which is a wire structure the G2 reads
// back. The device knows nothing about this and each fanned-out write reaches it as an ordinary
// per-variation parameter change.
//
// Variations are 0-based here; VARIATION_INIT is not a real variation and is never a member.
bool variation_is_linked(uint32_t slot, uint32_t variation);
void variation_toggle_link(uint32_t slot, uint32_t variation);
void variation_clear_links(uint32_t slot);

#ifdef __cplusplus
}
#endif

// Drag reference points in raw cursor coordinates — see globalVars.c. Shared between canvasDrag.c's
// parameter dragging and mouseHandle.c's tempo/vibrato/glide dragging.
extern double gDragStartX;
extern double gDragStartY;
extern double gDragPrevX;
extern double gDragPrevY;

// Cancel an in-progress name edit — see globalVars.c.
void stop_patch_name_editing(void);
void stop_module_name_editing(void);
void stop_param_name_editing(void);
void stop_perf_name_editing(void);
void stop_synth_name_editing(void);
void stop_patch_notes_editing(void);

#endif // __GLOBAL_VARS_H__
