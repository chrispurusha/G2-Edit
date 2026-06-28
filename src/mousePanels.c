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
#include "mousePanels.h"

// Suppresses a spurious leftUp event after dismissing the notes editor by
// clicking outside it — prevents immediately re-triggering topbar controls.
static bool gNoteEditDismissed = false;

static bool handle_panel_context_menu(tCoord coord) {
    if (!gContextMenu.active) {
        return false;
    }

    if (!handle_context_menu_click(coord)) {
        gContextMenu.active = false;
    }
    gReDraw = true;
    return true;
}

bool handle_patch_notes_mouse(tCoord coord, tMouseButton mouseButton) {
    size_t          len     = 0;
    uint32_t        newSize = 0;
    size_t          origLen = 0;
    int             newPos  = 0;
    tMessageContent msg     = {0};

    if (gNoteEditDismissed) {
        if (mouseButton == mouseButtonLeftUp) {
            gNoteEditDismissed = false;
        }
        gReDraw = true;
        return true;
    }

    if (!gPatchNotesEdit.active) {
        return false;
    }

    if (mouseButton == mouseButtonLeftDown) {
        if (within_rectangle(coord, gPatchNotesCloseRect)) {
            len                                        = strlen(gPatchNotesEdit.buffer);
            newSize                                    = (uint32_t)len;

            if (newSize > PATCH_NOTES_SIZE) {
                newSize = PATCH_NOTES_SIZE;
            }
            memcpy(gPatchNotes[gPatchNotesEdit.slot], gPatchNotesEdit.buffer, newSize);
            gPatchNotes[gPatchNotesEdit.slot][newSize] = '\0';
            gPatchNotesSize[gPatchNotesEdit.slot]      = newSize;
            gPatchNotesEdit.active                     = false;

            msg.cmd                                    = eMsgCmdWritePatch;
            msg.slot                                   = gPatchNotesEdit.slot;
            msg_send(&gCommandQueue, &msg);
        } else if (within_rectangle(coord, gPatchNotesDiscardRect)) {
            origLen                   = strlen(gPatchNotesEdit.original);
            memset(gPatchNotesEdit.buffer, 0, sizeof(gPatchNotesEdit.buffer));
            memcpy(gPatchNotesEdit.buffer, gPatchNotesEdit.original, origLen);
            gPatchNotesEdit.cursorPos = (uint32_t)origLen;
        } else {
            newPos = note_editor_cursor_from_click(coord.x, coord.y);

            if (newPos >= 0) {
                gPatchNotesEdit.cursorPos = (uint32_t)newPos;
            } else {
                gPatchNotesEdit.active = false;
                gNoteEditDismissed     = true;
            }
        }
    }
    gReDraw = true;
    return true;
}

bool handle_perf_settings_mouse(tCoord coord, tMouseButton mouseButton) {
    int s = 0;

    if (!gPerfSettingsEdit.active) {
        return false;
    }

    if (mouseButton == mouseButtonLeftDown) {
        if (within_rectangle(coord, gPerfSettingsPanelRects.masterClock)) {
            gPerfTempoDragging = true;

            if (gDialMode != eDialModeRotary) {
                start_cursor_drag();
            }
        }
    }

    if (mouseButton == mouseButtonLeftUp) {
        stop_dragging();

        if (handle_panel_context_menu(coord)) {
            return true;
        }

        if (within_rectangle(coord, gPerfSettingsPanelRects.close)) {
            gPerfSettingsEdit.active = false;
        } else {
            if (within_rectangle(coord, gPerfSettingsPanelRects.masterClockRunning)) {
                gGlobalSettings.masterClockRunning = !gGlobalSettings.masterClockRunning;
                send_master_clock_run((uint32_t)gGlobalSettings.masterClockRunning);
            } else if (within_rectangle(coord, gPerfSettingsPanelRects.keyboardRange)) {
                gPerfSettings.keyboardRange = !gPerfSettings.keyboardRange;
                send_perf_settings_msg();
            } else {
                for (s = 0; s < MAX_SLOTS; s++) {
                    if (within_rectangle(coord, gPerfSettingsPanelRects.slotEnabled[s])) {
                        gGlobalSettings.slot[s].enabled = !gGlobalSettings.slot[s].enabled;
                        send_perf_settings_msg();
                    } else if (within_rectangle(coord, gPerfSettingsPanelRects.slotKeyboard[s])) {
                        gPerfSettings.slot[s].keyboardEnabled = !gPerfSettings.slot[s].keyboardEnabled;
                        send_perf_settings_msg();
                    } else if (within_rectangle(coord, gPerfSettingsPanelRects.slotHold[s])) {
                        gPerfSettings.slot[s].holdEnabled = !gPerfSettings.slot[s].holdEnabled;
                        send_perf_settings_msg();
                    } else if (within_rectangle(coord, gPerfSettingsPanelRects.rangeLower[s])) {
                        open_midi_note_dropdown(below_rect(gPerfSettingsPanelRects.rangeLower[s]), &gPerfSettings.slot[s].rangeLower);
                    } else if (within_rectangle(coord, gPerfSettingsPanelRects.rangeUpper[s])) {
                        open_midi_note_dropdown(below_rect(gPerfSettingsPanelRects.rangeUpper[s]), &gPerfSettings.slot[s].rangeUpper);
                    }
                }
            }
        }
    }
    gReDraw = true;
    return true;
}

bool handle_patch_params_mouse(tCoord coord, tMouseButton mouseButton) {
    bool              switched           = false;
    uint32_t          s                  = 0;
    int               i                  = 0;
    int               j                  = 0;
    tPatchParamRectId rid                = (tPatchParamRectId)0;

    static const struct {
        tPatchParamRectId id;
        void              (*open_fn)(tCoord);
    }                 kPatchParamTable[] = {
        {pPSustainPedal,  NULL                            },
        {pPOctaveShift,   open_patch_octave_shift_dropdown},
        {pPArpEnabled,    NULL                            },
        {pPArpRate,       open_arp_rate_dropdown          },
        {pPArpDirection,  open_arp_direction_dropdown     },
        {pPArpOctaves,    open_arp_octave_dropdown        },
        {pPVibratoAmount, open_vibrato_amount_dropdown    },
        {pPVibratoSource, open_vibrato_source_dropdown    },
        {pPVibratoRate,   NULL                            },
        {pPGlideTime,     open_glide_time_dropdown        },
        {pPGlideMode,     open_glide_mode_dropdown        },
        {pPBendRange,     open_bend_range_dropdown        },
        {pPBendEnabled,   NULL                            },
    };
    static const struct {
        tPatchParamRectId id;
        uint32_t          moduleIndex;
        uint32_t          paramIndex;
    }                 kPatchOnOffTable[] = {
        {pPSustainPedal, PATCH_SUSTAIN,     SUSTAIN_PEDAL},
        {pPArpEnabled,   PATCH_ARPEGGIATOR, ARP_ON_OFF   },
        {pPBendEnabled,  PATCH_BEND,        BEND_ON_OFF  },
    };

    if (!gPatchParamsEdit.active) {
        return false;
    }

    if (mouseButton == mouseButtonLeftUp) {
        if (handle_panel_context_menu(coord)) {
            return true;
        }

        if (within_rectangle(coord, gPatchParamClose)) {
            gPatchParamsEdit.active = false;
        } else {
            switched = false;

            for (s = 0; s < MAX_SLOTS; s++) {
                if (within_rectangle(coord, gPatchParamSlots[s])) {
                    gPatchParamsEdit.slot = s;
                    switched              = true;
                    break;
                }
            }

            if (!switched) {
                for (i = 0; i < (int)(sizeof(kPatchParamTable) / sizeof(*kPatchParamTable)); i++) {
                    rid = kPatchParamTable[i].id;

                    if (within_rectangle(coord, gPatchParamRects[rid])) {
                        if (kPatchParamTable[i].open_fn != NULL) {
                            kPatchParamTable[i].open_fn(below_rect(gPatchParamRects[rid]));
                        } else {
                            for (j = 0; j < (int)(sizeof(kPatchOnOffTable) / sizeof(*kPatchOnOffTable)); j++) {
                                if (kPatchOnOffTable[j].id == rid) {
                                    toggle_patch_on_off(kPatchOnOffTable[j].moduleIndex,
                                                        kPatchOnOffTable[j].paramIndex);
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    gReDraw = true;
    return true;
}

bool handle_patch_settings_mouse(tCoord coord, tMouseButton mouseButton) {
    int s = 0;

    if (!gPatchSettingsEdit.active) {
        return false;
    }

    if (mouseButton == mouseButtonLeftUp) {
        if (handle_panel_context_menu(coord)) {
            return true;
        }
        stop_synth_name_editing();

        if (within_rectangle(coord, gSettingsPanelRects.close)) {
            gPatchSettingsEdit.active = false;
        } else {
            for (s = 0; s < 4; s++) {
                if (within_rectangle(coord, gSettingsPanelRects.midiChan[s])) {
                    open_midi_chan_dropdown(below_rect(gSettingsPanelRects.midiChan[s]), &gSynthSettings.midiChanSlot[s]);
                }
            }

            if (within_rectangle(coord, gSettingsPanelRects.synthName)) {
                gSynthNameEdit.active = true;
                COPY_STRING(gSynthNameEdit.buffer, gSynthSettings.name);
            } else if (within_rectangle(coord, gSettingsPanelRects.globalChan)) {
                open_midi_chan_dropdown(below_rect(gSettingsPanelRects.globalChan), &gSynthSettings.globalChan);
            } else if (within_rectangle(coord, gSettingsPanelRects.sysexId)) {
                open_sysex_id_dropdown(below_rect(gSettingsPanelRects.sysexId), &gSynthSettings.sysexId);
            } else if (within_rectangle(coord, gSettingsPanelRects.localOn)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.localOn), &gSynthSettings.localOn);
            } else if (within_rectangle(coord, gSettingsPanelRects.memoryProtect)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.memoryProtect), &gSynthSettings.memoryProtect);
            } else if (within_rectangle(coord, gSettingsPanelRects.progChangeRcv)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.progChangeRcv), &gSynthSettings.progChangeRcv);
            } else if (within_rectangle(coord, gSettingsPanelRects.progChangeSnd)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.progChangeSnd), &gSynthSettings.progChangeSnd);
            } else if (within_rectangle(coord, gSettingsPanelRects.controllersRcv)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.controllersRcv), &gSynthSettings.controllersRcv);
            } else if (within_rectangle(coord, gSettingsPanelRects.controllersSnd)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.controllersSnd), &gSynthSettings.controllersSnd);
            } else if (within_rectangle(coord, gSettingsPanelRects.sendClock)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.sendClock), &gSynthSettings.sendClock);
            } else if (within_rectangle(coord, gSettingsPanelRects.receiveClock)) {
                open_on_off_dropdown(below_rect(gSettingsPanelRects.receiveClock), &gSynthSettings.receiveClock);
            } else if (within_rectangle(coord, gSettingsPanelRects.tuneSemi)) {
                open_tune_semi_dropdown(below_rect(gSettingsPanelRects.tuneSemi), &gSynthSettings.tuneSemi);
            } else if (within_rectangle(coord, gSettingsPanelRects.tuneCent)) {
                open_tune_cent_dropdown(below_rect(gSettingsPanelRects.tuneCent), &gSynthSettings.tuneCent);
            } else if (within_rectangle(coord, gSettingsPanelRects.globalShiftActive)) {
                open_active_off_dropdown(below_rect(gSettingsPanelRects.globalShiftActive), &gSynthSettings.globalShiftActive);
            } else if (within_rectangle(coord, gSettingsPanelRects.globalOctaveShift)) {
                open_octave_shift_dropdown(below_rect(gSettingsPanelRects.globalOctaveShift), &gSynthSettings.globalOctaveShift);
            } else if (within_rectangle(coord, gSettingsPanelRects.pedalPolarity)) {
                open_pedal_polarity_dropdown(below_rect(gSettingsPanelRects.pedalPolarity), &gSynthSettings.pedalPolarity);
            } else if (within_rectangle(coord, gSettingsPanelRects.pedalGain)) {
                open_pedal_gain_dropdown(below_rect(gSettingsPanelRects.pedalGain), &gSynthSettings.pedalGain);
            } else if (within_rectangle(coord, gSettingsPanelRects.patchSortMode)) {
                open_patch_sort_dropdown(below_rect(gSettingsPanelRects.patchSortMode), &gSynthSettings.patchSortMode);
            } else if (within_rectangle(coord, gSettingsPanelRects.perfSortMode)) {
                open_perf_sort_dropdown(below_rect(gSettingsPanelRects.perfSortMode), &gSynthSettings.perfSortMode);
            }
        }
    }
    gReDraw = true;
    return true;
}

#ifdef __cplusplus
}
#endif
