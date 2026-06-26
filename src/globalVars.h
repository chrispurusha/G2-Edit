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

extern const char *        patchTypeStrMap[patchTypeUserMax];
extern const char *        monoPolyStrMap[monoPolyMax];

extern double              gGlobalGuiScale;
extern _Atomic bool        gQuitAll;
extern GLFWwindow *        gWindow;
extern _Atomic uint32_t    gLocation;
extern _Atomic bool        gReDraw;
extern bool                gCommandKeyPressed;
extern tTopbarControl      gTopbarControls[topbarControlMax];
extern bool                gShowOpenFileReadDialogue;
extern bool                gShowOpenFileWriteDialogue;
extern tScrollState        gScrollState;
extern tContextMenu        gContextMenu;
extern tCableDragging      gCableDrag;
extern tHoverConnector     gHoverConnector;
extern tParamDragging      gParamDragging;
extern tModuleDragging     gModuleDrag;
extern tMessageQueue       gCommandQueue;
extern uint32_t            gMorphGroupFocus;
extern _Atomic uint32_t    gSlot;
extern tPatchDescr         gPatchDescr[MAX_SLOTS];
extern tKnobArray          gKnobArray[MAX_SLOTS]; // TODO - Don't forget to nullify on new load
extern tGlobalKnob         gGlobalKnobArray[MAX_NUM_KNOBS];
extern tSelectedParam      gSelectedParam[MAX_SLOTS];
extern uint32_t            gMorphCount[MAX_SLOTS];
extern uint32_t            gNote2Size[MAX_SLOTS];
extern uint8_t             gNote2[MAX_SLOTS][1024];
extern uint32_t            gAssignedVoices[MAX_SLOTS];
extern tControllerArray    gControllerArray[MAX_SLOTS]; // TODO - Don't forget to nullify on new load
extern uint32_t            gControllerCount[MAX_SLOTS]; // TODO - Don't forget to nullify on new load
extern uint32_t            gPatchNotesSize[MAX_SLOTS];
extern uint8_t             gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1];
extern _Atomic uint8_t     gPatchVersion[MAX_SLOTS];
extern _Atomic uint8_t     gSlotEnabled[MAX_SLOTS];
extern _Atomic uint8_t     gPerfVersion;
extern _Atomic uint8_t     gMasterClock;
extern _Atomic uint8_t     gMasterClockRunning;
//extern _Atomic uint8_t     gPerfMode;
//extern char                gPatchName[MAX_SLOTS][PATCH_NAME_SIZE + 1];
extern _Atomic tCommsState gCommsState;
//extern _Atomic uint32_t    gChangedSlot;
extern _Atomic uint8_t     gGlobalPage;
extern tNameEdit           gPatchNameEdit;
extern tModuleNameEdit     gModuleNameEdit;
extern tParamNameEdit      gParamNameEdit;
extern tNameEdit           gSynthNameEdit;
extern tNameEdit           gPerfNameEdit;
extern char                gPerfName[CLAVIA_NAME_SIZE + 1];
extern tPerfSettings       gPerfSettings;
extern tPatchNotesEdit     gPatchNotesEdit;
extern tSynthSettings      gSynthSettings;
extern tPatchSettingsEdit  gPatchSettingsEdit;
extern tSettingsPanelRects gSettingsPanelRects;
extern tRectangle          gMorphLabelRect[NUM_MORPHS];
//extern _Atomic uint32_t    gHiddenCableMask;
extern _Atomic bool        gCablesTransparent; // true = draw all cables semi-transparent
extern _Atomic bool        gCablesHideAll;
extern tResourceAlloc      gResourceAlloc[MAX_SLOTS];

extern tRectangle          gPatchNotesCloseRect;
extern tRectangle          gPatchNotesDiscardRect;
extern bool                gTempoDragging;
extern _Atomic uint64_t    gUsbTxTime;
extern _Atomic uint64_t    gUsbRxTime;
extern tRectangle          gParamRectangle[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS];
extern tDialMode           gDialMode;
extern pthread_mutex_t     gStringCopyMutex;
extern _Atomic uint8_t     gSelectedSlot;

extern char                gPatchName[MAX_SLOTS][CLAVIA_NAME_SIZE + 1];

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
