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

double                 gGlobalGuiScale    = 2;
_Atomic bool           gQuitAll           = false;
GLFWwindow *           gWindow            = NULL;
_Atomic uint32_t       gLocation          = locationVa;
_Atomic bool           gReDraw            = true;

bool                   gCommandKeyPressed = false;

tButton                gMainButtonArray[] = { // Must align with tButtonId enumaration (in types.h)
    {{400,  8}, NULL_RECTANGLE, anchorTopLeft, "VA",         RGB_GREEN_ON,        false},
    {{425,  8}, NULL_RECTANGLE, anchorTopLeft, "FX",         RGB_BACKGROUND_GREY, false},
    {{ 20,  8}, NULL_RECTANGLE, anchorTopLeft, "Read File",  RGB_BACKGROUND_GREY, false},
    {{ 80,  8}, NULL_RECTANGLE, anchorTopLeft, "Write File", RGB_BACKGROUND_GREY, false},
    {{400, 60}, NULL_RECTANGLE, anchorTopLeft, "1",          RGB_BACKGROUND_GREY, false},
    {{412, 60}, NULL_RECTANGLE, anchorTopLeft, "2",          RGB_BACKGROUND_GREY, false},
    {{424, 60}, NULL_RECTANGLE, anchorTopLeft, "3",          RGB_BACKGROUND_GREY, false},
    {{436, 60}, NULL_RECTANGLE, anchorTopLeft, "4",          RGB_BACKGROUND_GREY, false},
    {{448, 60}, NULL_RECTANGLE, anchorTopLeft, "5",          RGB_BACKGROUND_GREY, false},
    {{460, 60}, NULL_RECTANGLE, anchorTopLeft, "6",          RGB_BACKGROUND_GREY, false},
    {{472, 60}, NULL_RECTANGLE, anchorTopLeft, "7",          RGB_BACKGROUND_GREY, false},
    {{484, 60}, NULL_RECTANGLE, anchorTopLeft, "8",          RGB_BACKGROUND_GREY, false},
    {{500, 60}, NULL_RECTANGLE, anchorTopLeft, "Init",       RGB_BACKGROUND_GREY, false},
    {{600,  8}, NULL_RECTANGLE, anchorTopLeft, "A",          RGB_GREEN_ON,        false},
    {{615,  8}, NULL_RECTANGLE, anchorTopLeft, "B",          RGB_BACKGROUND_GREY, false},
    {{630,  8}, NULL_RECTANGLE, anchorTopLeft, "C",          RGB_BACKGROUND_GREY, false},
    {{645,  8}, NULL_RECTANGLE, anchorTopLeft, "D",          RGB_BACKGROUND_GREY, false},
    {{140,  8}, NULL_RECTANGLE, anchorTopLeft, "New Patch",  RGB_BACKGROUND_GREY, false},
};

const char *           patchTypeStrMap[patchTypeUserMax]            = {"No Cat", "Acoustic", "Sequencer", "Bass", "Classic", "Drum", "Fantasy", "Fx", "Lead", "Organ", "Pad", "Piano", "Synth", "Audio In", "User 1", "User 2"};
const char *           monoPolyStrMap[monoPolyMax]                  = {"Poly", "Mono", "Legato"};

bool                   gShowOpenFileReadDialogue                    = false;
bool                   gShowOpenFileWriteDialogue                   = false;

tScrollState           gScrollState                                 = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, NULL_RECTANGLE, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, NULL_RECTANGLE};
tContextMenu           gContextMenu                                 = {0};
tCableDragging         gCableDrag                                   = {0};
tHoverConnector        gHoverConnector                              = {0};
tParamDragging         gParamDragging                               = {0};
tModuleDragging        gModuleDrag                                  = {0};
tMessageQueue          gCommandQueue                                = {0};
uint32_t               gMorphGroupFocus                             = 0;
_Atomic uint32_t       gSlot                                        = 0;
tPatchDescr            gPatchDescr[MAX_SLOTS]                       = {0};
tKnobArray             gKnobArray[MAX_SLOTS]                        = {0};
tGlobalKnob            gGlobalKnobArray[MAX_NUM_KNOBS]              = {0};
tSelectedParam         gSelectedParam[MAX_SLOTS]                    = {0};
uint32_t               gMorphCount[MAX_SLOTS]                       = {0};
uint32_t               gNote2Size[MAX_SLOTS]                        = {0};
uint8_t                gNote2[MAX_SLOTS][1024]                      = {0};
uint32_t               gAssignedVoices[MAX_SLOTS]                   = {0};
tControllerArray       gControllerArray[MAX_SLOTS]                  = {0};
uint32_t               gControllerCount[MAX_SLOTS]                  = {0};
uint32_t               gPatchNotesSize[MAX_SLOTS]                   = {0};
uint8_t                gPatchNotes[MAX_SLOTS][PATCH_NOTES_SIZE + 1] = {0};
_Atomic uint8_t        gPatchVersion[MAX_SLOTS]                     = {0};
_Atomic uint8_t        gPerfVersion                                 = 0;
_Atomic uint8_t        gMasterClock                                 = 0;
_Atomic uint8_t        gPerfMode                                    = 0;
_Atomic uint8_t        gMasterClockRunning                          = 0;
_Atomic tCommsState    gCommsState                                  = eCommsNeverConnected;
_Atomic uint32_t       gChangedSlot                                 = 0;
tNameEdit              gPatchNameEdit                               = {0};
tModuleNameEdit        gModuleNameEdit                              = {0};
//_Atomic uint32_t       gHiddenCableMask                             = 0; // TODO - Send to G2 when changes
tRectangle             gCableColourToggleRect[cableColourMax]       = {0};
tRectangle             gCableColourSelectRect[cableColourMax]       = {0};
uint32_t               gCableColour                                 = 0;
_Atomic bool           gCablesTransparent                           = false;
//_Atomic bool           gCablesHideAll                               = false;
tResourceAlloc         gResourceAlloc[MAX_SLOTS]                    = {0};

tRectangle             gHideAllCablesRect                           = {0};
tRectangle             gTransparentCablesRect                       = {0};
tRectangle             gPatchNameRectangle                          = {0};
tRectangle             gPatchTypeRectangle                          = {0};  // TODO - potentially roll these non-standard buttons into the main button mechanism with special types
tRectangle             gVoiceCountRectangle                         = {0};
//tRectangle             gVoiceCountIncRectangle                      = {0};
//tRectangle             gVoiceCountDecRectangle                      = {0};
tRectangle             gMonoPolyRectangle                           = {0};
tRectangle             gPatchVolumeRectangle                        = {{320, 56}, {20, 20}};

/* Stored here, but don't access directly, use functions to access instead */
static char            gPatchName[MAX_SLOTS][CLAVIA_NAME_SIZE + 1]  = {0};
static pthread_mutex_t gPatchNameMutex                              = PTHREAD_MUTEX_INITIALIZER;

uint32_t array_size_main_button_array(void) {
    return ARRAY_SIZE(gMainButtonArray);
}

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

void set_exclusive_button_highlight(tButtonId first, tButtonId last, tButtonId active) {
    for (tButtonId i = first; i <= last; i++) {
        gMainButtonArray[i].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
    }

    gMainButtonArray[active].backgroundColour = (tRgb)RGB_GREEN_ON;
}

#ifdef __cplusplus
}
#endif
