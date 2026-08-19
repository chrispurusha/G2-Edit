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

#ifndef __MSG_QUEUE_H__
#define __MSG_QUEUE_H__

#include "sysIncludes.h"
#include "synthlibQueue.h" // generic queue mechanism: tMessageQueue / eRcv / msg_init / msg_send / ...

typedef enum {
    eCommandInit,
    eCommandStart,
    eCommandStop,
    eCommandGetPatch
} eCommand;

// gToGuiThread is the render loop's work queue: tMessageContent.cmd holds an eResponseType, NOT an
// eMsgCmd (the gToUsbThread and gToGuiThread queues never cross, so their value spaces are independent).
// Most entries are USB-thread results, but the UI thread also posts to it for its own deferred work
// (e.g. "open the file browser" from a menu click, handled in the render loop). See reverse-queue-design.md.
typedef enum {
    eRspFileLoad,   // fileResultData: result of a file load (eMsgCmdLoadFile)
    eRspFileSave,   // fileResultData: result of a file save (eMsgCmdSavePatchFile / eMsgCmdSavePerfFile)
    eRspNewPatch,   // fileResultData: result of New Patch (eMsgCmdNewPatch); path unused
    eRspAlert,      // alertData: a plain "op finished" message to show_alert (bank backup/restore, store, etc.)
    // Peek results — the peek data itself stays in the gStore/Delete/Load/SynthRestorePeek* globals
    // (set by the USB thread, read by the drain to build the confirm dialog); these just signal "ready".
    eRspStorePeek,
    eRspDeletePeek,
    eRspLoadPeek,
    eRspSynthRestorePeek,
    // UI-thread-originated deferred work (posted from menu actions, run in the render loop).
    eRspShowOpenRead,      // open the "load file" browser
    eRspOpenPath,          // open a file whose path is already known (File > Open Recent) — the
                           // browser is skipped, but the deferral is not: a menu action must not
                           // load a patch from inside its own callback. Path in patchFileData.filePath
    eRspShowOpenWrite,     // open the "save file" browser (drain builds the default name)
    eRspSaveToCurrentPath, // save straight back to the remembered path, no browser
    eRspOfflineConflict    // offlineEditData: edits were made while the G2 was away — ask the user
} eResponseType;

typedef enum {
    eMsgCmdSetValue,
    eMsgCmdSetMode,
    eMsgCmdSetParamMorph,
    eMsgCmdWriteModule,
    eMsgCmdDeleteModule,
    eMsgCmdMoveModule,
    eMsgCmdSetModuleUpRate,
    eMsgCmdWriteCable,
    eMsgCmdSetCableColour,
    eMsgCmdDeleteCable,
    eMsgCmdSelectVariation,
    eMsgCmdSelectSlot,
    eMsgCmdWritePatch,
    eMsgCmdSetModuleLabel,
    eMsgCmdSetPatchName,
    eMsgCmdSetModuleColour,
    eMsgCmdWritePatchDescr,
    eMsgCmdAssignKnob,
    eMsgCmdDeassignKnob,
    eMsgCmdAssignGlobalKnob,
    eMsgCmdDeassignGlobalKnob,
    eMsgCmdAssignMidiCC,
    eMsgCmdDeassignMidiCC,
    eMsgCmdCopyVariation,
    eMsgCmdSetMasterClockBPM,
    eMsgCmdSetMasterClockRun,
    eMsgCmdSetParamLabel,
    eMsgCmdWriteSynthSettings,
    eMsgCmdWriteModePerf,
    eMsgCmdWriteModePatch,
    eMsgCmdWritePerf,
    eMsgCmdLoadFile,
    eMsgCmdSavePatchFile,
    eMsgCmdSavePerfFile,
    eMsgCmdNewPatch,
    eMsgCmdResolveOfflineEdits,  // offlineEditData: the user's answer to eRspOfflineConflict
    eMsgCmdWritePerfName,
    eMsgCmdWritePerfSettings,
    eMsgCmdSetCustomData,
    eMsgCmdBackupBank,
    eMsgCmdBackupSynthSettings,
    eMsgCmdBackupEverything,
    eMsgCmdRestoreBank,
    eMsgCmdPeekBankLocation,
    eMsgCmdStorePatch,
    eMsgCmdPeekDeleteTarget,
    eMsgCmdDeleteBankLocation,
    eMsgCmdPeekLoadTarget,
    eMsgCmdLoadPatch,
    eMsgCmdPeekSynthSettingsRestore,
    eMsgCmdApplySynthSettingsRestore,
    eMsgCmdRestoreEverything,
    eMsgCmdSetMutationLock,
    eMsgCmdPlayNote,
    eMsgCmdSendCtrlSnapshot
    //eMsgCmdReloadAllPatchData
} eMsgCmd;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   variation;
    uint32_t   param;
    uint32_t   value;
} tParamData;

// Virtual Keyboard note on/off. No slot: SUB_COMMAND_PLAY_NOTE carries none, the synth routing the
// note by its own keyboard assignment exactly as it would a note from the real keys.
typedef struct {
    uint32_t note;      // MIDI note number, 0-127
    uint32_t velocity;  // 0-127
    bool     on;
} tPlayNoteData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   variation;
    uint32_t   param;
    uint32_t   paramMorph;
    uint32_t   value;
    uint32_t   negative;
} tParamMorphData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   mode;
    uint32_t   value;
} tModeData;

typedef struct {
    uint32_t variation;
} tVariationData;

typedef struct {
    uint32_t slot;
} tSlotData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   type;
    uint32_t   row;
    uint32_t   column;
    uint32_t   colour;
    uint32_t   upRate;
    uint32_t   excludeFromMutation;
    uint32_t   unknown1;
    uint32_t   modeCount;
    uint32_t   mode[MAX_NUM_MODES];
    char       name[CLAVIA_NAME_SIZE + 1];
} tModuleData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   row;
    uint32_t   column;
} tModuleMoveData;

typedef struct {
    uint32_t location;
    uint32_t colour;
    uint32_t moduleFromIndex;
    uint32_t connectorFromIoIndex;
    uint32_t linkType;
    uint32_t moduleToIndex;
    uint32_t connectorToIoIndex;
} tCableData;

typedef struct {
    tModuleKey moduleKey;
    char       name[CLAVIA_NAME_SIZE + 1];
} tModuleLabelData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   paramIndex;
    char       name[PROTOCOL_PARAM_NAME_SIZE + 1];
} tParamLabelData;

typedef struct {
    char name[CLAVIA_NAME_SIZE + 1];
} tPatchName;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   colour;
} tModuleColourData;

// SUB_COMMAND_SET_MUTATION_LOCK (0x90) - unverified/experimental, see defs.h.
typedef struct {
    tModuleKey moduleKey;
    uint32_t   locked;
} tModuleMutationLockData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   paramIndex;
    uint32_t   knobIndex;
} tKnobAssignData;

typedef struct {
    uint32_t knobIndex;
} tKnobDeassignData;

typedef struct {
    uint32_t slotIndex;
    uint32_t location;
    uint32_t moduleIndex;
    uint32_t paramIndex;
    uint32_t knobIndex;
} tGlobalKnobAssignData;

typedef struct {
    uint32_t knobIndex;
} tGlobalKnobDeassignData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   paramIndex;
    uint32_t   midiCC;
} tMidiCCAssignData;

typedef struct {
    uint32_t midiCC;
} tMidiCCDeassignData;

typedef struct {
    uint32_t fromVariation;
    uint32_t toVariation;
} tCopyVariationData;

typedef struct {
    uint32_t bpm;
} tMasterClockBPMData;

typedef struct {
    uint32_t running;
} tMasterClockRunData;

typedef struct {
    tModuleKey moduleKey;
    uint32_t   customData[2]; // [0]=magnifier, [1]=octave; TLV order matches 0x42 packet
} tCustomDataMsg;

typedef struct {
    uint32_t bank;   // 0-indexed
    bool     isPerf; // true = Performance Bank, false = Patch Bank
    char     destFolder[1024];
} tBankBackupData;

typedef struct {
    char destFolder[1024];
} tSettingsBackupData;

typedef struct {
    char srcFolder[1024];
} tSynthSettingsRestoreData;

typedef struct {
    uint32_t sourceBank; // 0-indexed — which bank's manifest to read
    uint32_t destBank;   // 0-indexed — where the write actually goes (usually == sourceBank)
    bool     isPerf;     // true = Performance Bank, false = Patch Bank
    char     srcFolder[1024];
} tBankRestoreData;

typedef struct {
    uint32_t bank;     // 0-indexed
    uint32_t location; // 0-indexed
    bool     isPerf;   // true = Performance Bank, false = Patch Bank
} tBankLocationPerfData;

typedef struct {
    char filePath[1024];     // .pch2/.prf2 path; the USB thread opens/reads (load) or writes (save) it
                             // directly so the whole CRC+sniff+clear+parse+push (load) / DB-read+
                             // serialise (save) runs on the one thread — no cross-thread DB race. See
                             // eMsgCmdLoadFile / eMsgCmdSavePatchFile / eMsgCmdSavePerfFile handlers.
                             // filePath is unused for eMsgCmdNewPatch; slot is unused for the perf
                             // save and for perf loads (whole-DB, all 4 slots).
    uint32_t slot;
} tPatchFileData;

// Reverse-queue payload: outcome of a USB-thread file op, for the UI to surface (e.g. show_alert).
typedef struct {
    int32_t result;      // EXIT_SUCCESS / EXIT_FAILURE
    char    path[1024];  // the file involved (for the user-facing message)
} tFileResultData;

// Reverse-queue payload: a plain completion alert (title + message) built on the USB thread.
typedef struct {
    char title[64];
    char message[256];
} tAlertData;

// Offline-edit conflict, both directions. slotMask has bit N set for each slot the editor edited
// while the G2 was away (USB -> GUI, eRspOfflineConflict), then comes back alongside the user's
// answer (GUI -> USB, eMsgCmdResolveOfflineEdits) so the push targets exactly those slots.
typedef struct {
    uint32_t slotMask;
    bool     pushToDevice;  // Resolve only: true = push the editor's slots, false = take the G2's
} tOfflineEditData;

typedef struct {
    uint32_t cmd;
    uint32_t slot;
    union {
        tParamData                paramData;
        tParamMorphData           paramMorphData;
        tModeData                 modeData;
        tModuleData               moduleData;
        tCableData                cableData;
        tVariationData            variationData;
        tSlotData                 slotData;
        tModuleLabelData          moduleLabelData;
        tPatchName                patchName;
        tModuleColourData         moduleColourData;
        tModuleMutationLockData   moduleMutationLockData;
        tKnobAssignData           knobAssignData;
        tKnobDeassignData         knobDeassignData;
        tGlobalKnobAssignData     globalKnobAssignData;
        tGlobalKnobDeassignData   globalKnobDeassignData;
        tMidiCCAssignData         midiCCAssignData;
        tMidiCCDeassignData       midiCCDeassignData;
        tPlayNoteData             playNoteData;
        tCopyVariationData        copyVariationData;
        tMasterClockBPMData       masterClockBPMData;
        tMasterClockRunData       masterClockRunData;
        tParamLabelData           paramLabelData;
        tCustomDataMsg            customDataMsg;
        tBankBackupData           bankBackupData;
        tSettingsBackupData       settingsBackupData;
        tSynthSettingsRestoreData synthSettingsRestoreData;
        tBankRestoreData          bankRestoreData;
        tBankLocationPerfData     bankLocationPerfData;
        tPatchFileData            patchFileData;
        tFileResultData           fileResultData;   // reverse queue (gToGuiThread) only
        tAlertData                alertData;        // reverse queue (gToGuiThread) only
        tOfflineEditData          offlineEditData;  // both directions — see tOfflineEditData
    };
} tMessageContent;

// The generic queue mechanism (tMessageQueue / tMessage / eRcv / msg_init / msg_send / msg_receive /
// msg_count) now lives in SynthLib (synthlibQueue.h) — it's payload-agnostic, so it carries this
// app's tMessageContent for both queues (gToUsbThread / gToGuiThread) without depending on it. This
// header defines only the app-specific message content above.

#endif // __MSG_QUEUE_H__
