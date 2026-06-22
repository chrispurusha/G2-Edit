/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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

double                 gGlobalGuiScale                                                              = 2;
_Atomic bool           gQuitAll                                                                     = false;
GLFWwindow *           gWindow                                                                      = NULL;
_Atomic uint32_t       gLocation                                                                    = locationVa;
_Atomic bool           gReDraw                                                                      = true;

bool                   gCommandKeyPressed                                                           = false;

tTopbarControl         gTopbarControls[topbarControlMax]                                            = {0};

const char *           patchTypeStrMap[patchTypeUserMax]                                            = {"No Cat", "Acoustic", "Sequencer", "Bass", "Classic", "Drum", "Fantasy", "Fx", "Lead", "Organ", "Pad", "Piano", "Synth", "Audio In", "User 1", "User 2"};
const char *           monoPolyStrMap[monoPolyMax]                                                  = {"Poly", "Mono", "Legato"};

bool                   gShowOpenFileReadDialogue                                                    = false;
bool                   gShowOpenFileWriteDialogue                                                   = false;

tScrollState           gScrollState                                                                 = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, 0.0, NULL_RECTANGLE, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, 0.0, NULL_RECTANGLE};
tContextMenu           gContextMenu                                                                 = {0};
tCableDragging         gCableDrag                                                                   = {0};
tHoverConnector        gHoverConnector                                                              = {0};
tParamDragging         gParamDragging                                                               = {0};
tModuleDragging        gModuleDrag                                                                  = {0};
tMessageQueue          gCommandQueue                                                                = {0};
uint32_t               gMorphGroupFocus                                                             = 0;
_Atomic uint32_t       gSlot                                                                        = 0;
tPatchDescr            gPatchDescr[MAX_SLOTS]                                                       = {0};
tKnobArray             gKnobArray[MAX_SLOTS]                                                        = {0};
tGlobalKnob            gGlobalKnobArray[MAX_NUM_KNOBS]                                              = {0};
tSelectedParam         gSelectedParam[MAX_SLOTS]                                                    = {0};
uint32_t               gMorphCount[MAX_SLOTS]                                                       = {0};
uint32_t               gNote2Size[MAX_SLOTS]                                                        = {0};
uint8_t                gNote2[MAX_SLOTS][1024]                                                      = {0};
uint32_t               gAssignedVoices[MAX_SLOTS]                                                   = {0};
tControllerArray       gControllerArray[MAX_SLOTS]                                                  = {0};
uint32_t               gControllerCount[MAX_SLOTS]                                                  = {0};
uint32_t               gPatchNotesSize[MAX_SLOTS]                                                   = {0};
uint8_t                gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1]                                 = {0};
_Atomic uint8_t        gPatchVersion[MAX_SLOTS]                                                     = {0};
_Atomic uint8_t        gPerfVersion                                                                 = 0;
_Atomic uint8_t        gMasterClock                                                                 = 0;
_Atomic uint8_t        gPerfMode                                                                    = 0;
_Atomic uint8_t        gMasterClockRunning                                                          = 0;
_Atomic tCommsState    gCommsState                                                                  = eCommsNeverConnected;
_Atomic uint32_t       gChangedSlot                                                                 = 0;
_Atomic uint8_t        gGlobalPage                                                                  = 0;
tNameEdit              gPatchNameEdit                                                               = {0};
tModuleNameEdit        gModuleNameEdit                                                              = {0};
tParamNameEdit         gParamNameEdit                                                               = {0};
tNameEdit              gSynthNameEdit                                                               = {0};
tPatchNotesEdit        gPatchNotesEdit                                                              = {0};
tSynthSettings         gSynthSettings                                                               = {0};
tPatchSettingsEdit     gPatchSettingsEdit                                                           = {0};
tSettingsPanelRects    gSettingsPanelRects                                                          = {0};
tRectangle             gMorphLabelRect[NUM_MORPHS]                                                  = {0};
//_Atomic uint32_t       gHiddenCableMask                             = 0; // TODO - Send to G2 when changes
uint32_t               gCableColour                                                                 = 0;
_Atomic bool           gCablesTransparent                                                           = false;
_Atomic bool           gCablesHideAll                                                               = false;
tResourceAlloc         gResourceAlloc[MAX_SLOTS]                                                    = {0};

tRectangle             gPatchNotesCloseRect                                                         = {0};
tRectangle             gPatchNotesDiscardRect                                                       = {0};
bool                   gTempoDragging                                                               = false;
_Atomic uint64_t       gUsbTxTime                                                                   = 0;
_Atomic uint64_t       gUsbRxTime                                                                   = 0;
tRectangle             gParamRectangle[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS] = {0}; // Rectangle references for each module. It's big - around 6mb

/* Stored here, but don't access directly, use functions to access instead */
static char            gPatchName[MAX_SLOTS][CLAVIA_NAME_SIZE + 1]                                  = {0};
static pthread_mutex_t gPatchNameMutex                                                              = PTHREAD_MUTEX_INITIALIZER;

void patch_name_set(uint32_t slot, const char * name) {
    pthread_mutex_lock(&gPatchNameMutex);

    strncpy(gPatchName[slot], name, sizeof(gPatchName[0]) - 1);
    gPatchName[slot][sizeof(gPatchName[0]) - 1] = '\0';

    pthread_mutex_unlock(&gPatchNameMutex);
}

void patch_name_get(uint32_t slot, char * name, size_t size) {
    pthread_mutex_lock(&gPatchNameMutex);

    strncpy(name,
            gPatchName[slot],
            size - 1);

    name[size - 1] = '\0';

    pthread_mutex_unlock(&gPatchNameMutex);
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
