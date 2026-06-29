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

// System header files
#include <math.h>

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include "defs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "topbarResourcesAccess.h"
#include "utilsGraphics.h"
#include "mouseHandle.h"
#include "graphics.h"
#include "globalVars.h"
#include "protocol.h"
#include "menus.h"
#include "mouseTopbar.h"
#include "undo.h"

static void handle_button(tTopbarControlId controlId) {
    uint32_t slot = gSlot;

    switch (controlId) {
        case topbarVaId:
        {
            gLocation = locationVa;
            set_exclusive_button_highlight(topbarVaId, topbarFxId, controlId);
            set_x_scroll_bar(0);
            set_y_scroll_bar(0);
            break;
        }
        case topbarFxId:
        {
            gLocation = locationFx;
            set_exclusive_button_highlight(topbarVaId, topbarFxId, controlId);
            set_x_scroll_bar(0);
            set_y_scroll_bar(0);
            break;
        }
        case topbarVariation1Id:
        case topbarVariation2Id:
        case topbarVariation3Id:
        case topbarVariation4Id:
        case topbarVariation5Id:
        case topbarVariation6Id:
        case topbarVariation7Id:
        case topbarVariation8Id:
        case topbarVariationInitId:
        {
            uint32_t        variation      = (uint32_t)controlId - (uint32_t)topbarVariation1Id;

            gPatchDescr[slot].activeVariation      = variation;

            set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId, controlId);

            tMessageContent messageContent = {0};
            messageContent.cmd                     = eMsgCmdSelectVariation;
            messageContent.slot                    = slot;
            messageContent.variationData.variation = variation;
            msg_send(&gCommandQueue, &messageContent);

            break;
        }
        case topbarSlotAId:
        case topbarSlotBId:
        case topbarSlotCId:
        case topbarSlotDId:
        {
            uint32_t        slot           = (uint32_t)controlId - (uint32_t)topbarSlotAId;

            gSlot                        = slot;

            tMessageContent messageContent = {0};
            messageContent.cmd           = eMsgCmdSelectSlot;
            messageContent.slot          = slot;
            messageContent.slotData.slot = slot;
            msg_send(&gCommandQueue, &messageContent);

            set_exclusive_button_highlight(topbarSlotAId, topbarSlotDId, controlId);
            set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId,
                                           (tTopbarControlId)((uint32_t)topbarVariation1Id + gPatchDescr[slot].activeVariation));
            break;
        }
        case topbarNewPatchId:
        {
            init_patch(slot);

            tMessageContent messageContent = {0};
            messageContent.cmd  = eMsgCmdWritePatch;
            messageContent.slot = slot;
            msg_send(&gCommandQueue, &messageContent);
            break;
        }
        case topbarUndoId:
        {
            undo_undo();
            gReDraw = true;
            break;
        }
        case topbarRedoId:
        {
            undo_redo();
            gReDraw = true;
            break;
        }
        default:
            break;
    }
}

bool handle_topbar_left_down(tCoord coord, uint32_t slot) {
    bool       found   = false;
    int        i       = 0;
    bool       running = false;
    tModuleKey volKey  = {0, 0, 0};

    for (i = 0; i < TOPBAR_STANDARD_BUTTON_COUNT; i++) {
        if (within_rectangle(coord, gTopbarControls[i].rectangle)) {
            found                        = true;
            gTopbarControls[i].isPressed = true;
            break;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarPatchVolumeId].rectangle)) {
            volKey.slot              = slot;
            volKey.location          = (uint32_t)locationMorph;
            volKey.index             = PATCH_VOLUME;

            gParamDragging.moduleKey = volKey;
            gParamDragging.type3     = paramType3Param;
            gParamDragging.param     = VOLUME_LEVEL;
            gParamDragging.active    = true;

            if (gDialMode != eDialModeRotary) {
                start_cursor_drag();
            }
            found                    = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarTempoDialId].rectangle)) {
            gTempoDragging = true;

            if (gDialMode != eDialModeRotary) {
                start_cursor_drag();
            }
            found          = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarClockRunStopId].rectangle)) {
            running                            = !gGlobalSettings.masterClockRunning;
            gGlobalSettings.masterClockRunning = (uint8_t)running;
            send_master_clock_run((uint32_t)running);
            gReDraw                            = true;
            found                              = true;
        }
    }
    return found;
}

bool handle_topbar_left_up(tCoord coord, uint32_t slot) {
    bool            found          = false;
    int             i              = 0;
    int             colour         = 0;
    tMessageContent messageContent = {0};

    for (i = 0; i < TOPBAR_STANDARD_BUTTON_COUNT; i++) {
        if (within_rectangle(coord, gTopbarControls[i].rectangle)) {
            handle_button((tTopbarControlId)i);
            found = true;
            break;
        }
    }

    if (found == false) {
        for (i = 0; i < cableColourMax; i++) {
            if (within_rectangle(coord, gTopbarControls[(int)topbarCableColourToggle0Id + i].rectangle)) {
                for (colour = 0; colour < cableColourMax; colour++) {
                    gPatchDescr[slot].visible[i] = !gPatchDescr[slot].visible[i];
                }

                gReDraw             = true;
                messageContent.cmd  = eMsgCmdWritePatchDescr;
                messageContent.slot = slot;
                msg_send(&gCommandQueue, &messageContent);
                found               = true;
                break;
            }
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarHideAllCablesId].rectangle)) {
            gCablesHideAll = !gCablesHideAll;
            gReDraw        = true;
            found          = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarTransparentCablesId].rectangle)) {
            gCablesTransparent = !gCablesTransparent;
            gReDraw            = true;
            found              = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarPatchNotesId].rectangle)) {
            gPatchNotesEdit.active    = true;
            gPatchNotesEdit.slot      = slot;
            gPatchNotesEdit.cursorPos = gPatchNotesSize[slot];
            memset(gPatchNotesEdit.buffer, 0, sizeof(gPatchNotesEdit.buffer));
            memcpy(gPatchNotesEdit.buffer, gPatchNotes[slot], gPatchNotesSize[slot]);
            memset(gPatchNotesEdit.original, 0, sizeof(gPatchNotesEdit.original));
            memcpy(gPatchNotesEdit.original, gPatchNotes[slot], gPatchNotesSize[slot]);
            found                     = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarPerfModeId].rectangle)) {
            memset(&messageContent, 0, sizeof(messageContent));

            if (gGlobalSettings.perfMode == 0) {
                messageContent.cmd       = eMsgCmdWriteModePerf;
                gGlobalSettings.perfMode = 1;
            } else {
                messageContent.cmd       = eMsgCmdWriteModePatch;
                gGlobalSettings.perfMode = 0;
            }
            msg_send(&gCommandQueue, &messageContent);
            found = true;
        }
    }

    if (found == false) {
        if (gGlobalSettings.perfMode == 1 && within_rectangle(coord, gTopbarControls[topbarPerfNameId].rectangle)) {
            gPerfNameEdit.active    = true;
            COPY_STRING(gPerfNameEdit.buffer, gGlobalSettings.perfName);
            gPerfNameEdit.cursorPos = (uint32_t)strlen(gPerfNameEdit.buffer);
            found                   = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarSettingsId].rectangle)) {
            gPatchSettingsEdit.active = true;
            gPatchSettingsEdit.slot   = slot;
            found                     = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarPerfSettingsId].rectangle)) {
            gPerfSettingsEdit.active = true;
            found                    = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarPatchSettingsId].rectangle)) {
            gPatchParamsEdit.active = true;
            gPatchParamsEdit.slot   = slot;
            found                   = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarPatchNameId].rectangle)) {
            gPatchNameEdit.active    = true;
            gPatchNameEdit.slot      = slot;
            COPY_STRING(gPatchNameEdit.buffer, gGlobalSettings.slot[slot].patchName);
            gPatchNameEdit.cursorPos = (uint32_t)strlen(gPatchNameEdit.buffer);
            found                    = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarPatchTypeId].rectangle)) {
            open_patch_type_context_menu(gTopbarControls[topbarPatchTypeId].rectangle.coord);
            found = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarMonoPolyId].rectangle)) {
            open_mono_poly_context_menu(coord);
            found = true;
        }
    }

    if (found == false) {
        if (within_rectangle(coord, gTopbarControls[topbarVoiceCountId].rectangle)) {
            open_voice_count_context_menu(coord);
            found = true;
        }
    }
    return found;
}

bool handle_topbar_right_up(tCoord coord) {
    bool found = false;
    int  i     = 0;

    for (i = (int)topbarVariation1Id; i <= (int)topbarVariationInitId; i++) {
        if (within_rectangle(coord, gTopbarControls[i].rectangle)) {
            uint32_t sourceVariation = (uint32_t)i - (uint32_t)topbarVariation1Id;
            open_variation_copy_menu(coord, sourceVariation);
            found = true;
            break;
        }
    }

    return found;
}

#ifdef __cplusplus
}
#endif
