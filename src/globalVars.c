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

#include "globalVars.h"

tDialMode               gDialMode                                                                    = eDialModeRotary;

double                  gGlobalGuiScale                                                              = 2;
_Atomic bool            gQuitAll                                                                     = false;
GLFWwindow *            gWindow                                                                      = NULL;
_Atomic uint32_t        gLocation                                                                    = locationVa;
_Atomic bool            gReDraw                                                                      = true;

bool                    gCommandKeyPressed                                                           = false;

tTopbarControl          gTopbarControls[topbarControlMax]                                            = {0};

const char *            patchTypeStrMap[patchTypeUserMax]                                            = {"No Cat", "Acoustic", "Sequencer", "Bass", "Classic", "Drum", "Fantasy", "Fx", "Lead", "Organ", "Pad", "Piano", "Synth", "Audio In", "User 1", "User 2"};
const char *            monoPolyStrMap[monoPolyMax]                                                  = {"Poly", "Mono", "Legato"};

bool                    gShowOpenFileReadDialogue                                                    = false;
bool                    gShowOpenFileWriteDialogue                                                   = false;

tScrollState            gScrollState                                                                 = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, 0.0, NULL_RECTANGLE, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, 0.0, NULL_RECTANGLE};
tContextMenu            gContextMenu                                                                 = {0};
tCableDragging          gCableDrag                                                                   = {0};
tHoverConnector         gHoverConnector                                                              = {0};
tParamDragging          gParamDragging                                                               = {0};
tModuleDragging         gModuleDrag                                                                  = {0};
tMessageQueue           gCommandQueue                                                                = {0};
uint32_t                gMorphGroupFocus                                                             = 0;
_Atomic uint32_t        gSlot                                                                        = 0;
tPatchDescr             gPatchDescr[MAX_SLOTS]                                                       = {0};
tKnobArray              gKnobArray[MAX_SLOTS]                                                        = {0};
tGlobalKnob             gGlobalKnobArray[MAX_NUM_KNOBS]                                              = {0};
tSelectedParam          gSelectedParam[MAX_SLOTS]                                                    = {0};
uint32_t                gMorphCount[MAX_SLOTS]                                                       = {0};
uint32_t                gNote2Size[MAX_SLOTS]                                                        = {0};
uint8_t                 gNote2[MAX_SLOTS][1024]                                                      = {0};
uint32_t                gAssignedVoices[MAX_SLOTS]                                                   = {0};
tControllerArray        gControllerArray[MAX_SLOTS]                                                  = {0};
uint32_t                gControllerCount[MAX_SLOTS]                                                  = {0};
uint32_t                gPatchNotesSize[MAX_SLOTS]                                                   = {0};
uint8_t                 gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1]                                 = {0};
//_Atomic uint8_t     gPatchVersion[MAX_SLOTS]                                                     = {0};
tGlobalSettings         gGlobalSettings                                                              = {0};  // Note - should reflect settings in the G2
_Atomic tCommsState     gCommsState                                                                  = eCommsNeverConnected;
_Atomic uint8_t         gGlobalPage                                                                  = 0;
tNameEdit               gPatchNameEdit                                                               = {0};
tModuleNameEdit         gModuleNameEdit                                                              = {0};
tParamNameEdit          gParamNameEdit                                                               = {0};
tNameEdit               gSynthNameEdit                                                               = {0};
tNameEdit               gPerfNameEdit                                                                = {0};
tPerfSettings           gPerfSettings                                                                = {0};  // Note - should reflect settings in the G2
tPatchNotesEdit         gPatchNotesEdit                                                              = {0};
tSynthSettings          gSynthSettings                                                               = {0};  // Note - should reflect settings in the G2
tPatchSettingsEdit      gPatchSettingsEdit                                                           = {0};
tSettingsPanelRects     gSettingsPanelRects                                                          = {0};
tPerfSettingsEdit       gPerfSettingsEdit                                                            = {0};
tPerfSettingsPanelRects gPerfSettingsPanelRects                                                      = {0};
tPatchSettingsEdit      gPatchParamsEdit                                                             = {0};
tRectangle              gPatchParamClose                                                             = {0};
tRectangle              gPatchParamSlots[MAX_SLOTS]                                                  = {0};
tRectangle              gPatchParamRects[pPCount]                                                    = {0};
tRectangle              gMorphLabelRect[NUM_MORPHS]                                                  = {0};
//_Atomic uint32_t       gHiddenCableMask                             = 0; // TODO - Send to G2 when changes
bool                    gCablesTransparent                                                           = false;
bool                    gCablesHideAll                                                               = false;
tResourceAlloc          gResourceAlloc[MAX_SLOTS]                                                    = {0};

tRectangle              gPatchNotesCloseRect                                                         = {0};
tRectangle              gPatchNotesDiscardRect                                                       = {0};
bool                    gTempoDragging                                                               = false;
bool                    gPerfTempoDragging                                                           = false;
bool                    gVibRateDragging                                                             = false;
bool                    gVibAmountDragging                                                           = false;
bool                    gGlideTimeDragging                                                           = false;
_Atomic uint64_t        gUsbTxTime                                                                   = 0;
_Atomic uint64_t        gUsbRxTime                                                                   = 0;
tRectangle              gParamRectangle[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS] = {0};  // Rectangle references for each module. It's big - around 6mb
pthread_mutex_t         gStringCopyMutex                                                             = PTHREAD_MUTEX_INITIALIZER;

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
