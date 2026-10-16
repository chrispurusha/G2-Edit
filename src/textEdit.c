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
// Notes: Docs/code-notes/textEdit.c.md - "// notes §k" refers there.

// notes §3

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>    // the key CONSTANTS only
#include <string.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
#include "floatingPanels.h"
#include "settingsPanels.h"
#include "menus.h"
#include "undo.h"
#include "textEdit.h"

bool any_text_edit_active(void) {
    return any_name_edit_active() || gPatchNotesEdit.active;
}

// notes §1
static bool notes_own_keyboard(void) {
    return gPatchNotesEdit.active
           && !any_name_edit_active()
           && floating_panel_is_frontmost(&gPatchNotesEdit.panel);
}

bool text_edit_char(unsigned int value) {
    bool taken = any_name_edit_active() || notes_own_keyboard();

    if (gPatchNameEdit.active) {
        size_t   len       = strlen(gPatchNameEdit.buffer);
        uint32_t cursorPos = gPatchNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gPatchNameEdit.buffer[cursorPos + 1],
                    &gPatchNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gPatchNameEdit.buffer[cursorPos] = (char)value;
            gPatchNameEdit.cursorPos++;
        }
    }

    if (notes_own_keyboard()) {
        size_t   len       = strlen(gPatchNotesEdit.buffer);
        uint32_t cursorPos = gPatchNotesEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < PATCH_NOTES_SIZE)) {
            memmove(&gPatchNotesEdit.buffer[cursorPos + 1],
                    &gPatchNotesEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gPatchNotesEdit.buffer[cursorPos] = (char)value;
            gPatchNotesEdit.cursorPos++;
        }
    }

    if (gModuleNameEdit.active) {
        size_t   len       = strlen(gModuleNameEdit.buffer);
        uint32_t cursorPos = gModuleNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gModuleNameEdit.buffer[cursorPos + 1],
                    &gModuleNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gModuleNameEdit.buffer[cursorPos] = (char)value;
            gModuleNameEdit.cursorPos++;
        }
    }

    if (gParamNameEdit.active) {
        size_t   len       = strlen(gParamNameEdit.buffer);
        uint32_t cursorPos = gParamNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < PROTOCOL_PARAM_NAME_SIZE)) {
            memmove(&gParamNameEdit.buffer[cursorPos + 1],
                    &gParamNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gParamNameEdit.buffer[cursorPos] = (char)value;
            gParamNameEdit.cursorPos++;
        }
    }

    if (gSynthNameEdit.active) {
        size_t   len       = strlen(gSynthNameEdit.buffer);
        uint32_t cursorPos = gSynthNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gSynthNameEdit.buffer[cursorPos + 1],
                    &gSynthNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gSynthNameEdit.buffer[cursorPos] = (char)value;
            gSynthNameEdit.cursorPos++;
        }
    }

    if (gPerfNameEdit.active) {
        size_t   len       = strlen(gPerfNameEdit.buffer);
        uint32_t cursorPos = gPerfNameEdit.cursorPos;

        if ((value >= 0x20) && (value <= 0x7e) && (len < CLAVIA_NAME_SIZE)) {
            memmove(&gPerfNameEdit.buffer[cursorPos + 1],
                    &gPerfNameEdit.buffer[cursorPos],
                    len - cursorPos + 1);
            gPerfNameEdit.buffer[cursorPos] = (char)value;
            gPerfNameEdit.cursorPos++;
        }
    }
    return taken;
}

bool text_edit_key(int key, int mods, int action) {
    if (notes_own_keyboard()) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gPatchNotesEdit.buffer);
            uint32_t cursorPos = gPatchNotesEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gPatchNotesEdit.buffer[cursorPos - 1],
                            &gPatchNotesEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPatchNotesEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gPatchNotesEdit.buffer[cursorPos],
                            &gPatchNotesEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (  (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
                      && (mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER))) {
                uint32_t        newSize = (uint32_t)len;

                if (newSize > PATCH_NOTES_SIZE) {
                    newSize = PATCH_NOTES_SIZE;
                }
                memcpy(gPatchNotes[gPatchNotesEdit.slot], gPatchNotesEdit.buffer, newSize);
                gPatchNotes[gPatchNotesEdit.slot][newSize] = '\0';
                gPatchNotesSize[gPatchNotesEdit.slot]      = newSize;
                gPatchNotesEdit.active                     = false;
                tMessageContent msg     = {0};
                msg.cmd                                    = eMsgCmdWritePatch;
                msg.slot                                   = gPatchNotesEdit.slot;
                msg_send(&gToUsbThread, &msg);
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                if (len < PATCH_NOTES_SIZE) {
                    memmove(&gPatchNotesEdit.buffer[cursorPos + 1],
                            &gPatchNotesEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPatchNotesEdit.buffer[cursorPos] = '\r';
                    gPatchNotesEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gPatchNotesEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gPatchNotesEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_UP) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_move_line((int)cursorPos, -1);
            } else if (key == GLFW_KEY_DOWN) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_move_line((int)cursorPos, 1);
            } else if (key == GLFW_KEY_HOME) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_line_home((int)cursorPos);
            } else if (key == GLFW_KEY_END) {
                gPatchNotesEdit.cursorPos = (uint32_t)note_editor_cursor_line_end((int)cursorPos);
            } else if (key == GLFW_KEY_ESCAPE) {
                gPatchNotesEdit.active = false;
            }
        }
        return true;
    } else if (gPatchNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gPatchNameEdit.buffer);
            uint32_t cursorPos = gPatchNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gPatchNameEdit.buffer[cursorPos - 1],
                            &gPatchNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPatchNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gPatchNameEdit.buffer[cursorPos],
                            &gPatchNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gPatchNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gPatchNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gPatchNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gPatchNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                // Commit
                gPatchNameEdit.active = false;
                uint32_t        pnSlot         = gPatchNameEdit.slot;
                char            oldPatchName[CLAVIA_NAME_SIZE + 1];
                COPY_STRING(oldPatchName, gGlobalSettings.slot[pnSlot].patchName);
                COPY_STRING(gGlobalSettings.slot[pnSlot].patchName, gPatchNameEdit.buffer);
                tMessageContent messageContent = {0};
                messageContent.cmd    = eMsgCmdSetPatchName;
                messageContent.slot   = pnSlot;
                COPY_STRING(messageContent.patchName.name, gGlobalSettings.slot[pnSlot].patchName);
                msg_send(&gToUsbThread, &messageContent);
                undo_push_patch_name(pnSlot, oldPatchName, gPatchNameEdit.buffer);
            } else if (key == GLFW_KEY_ESCAPE) {
                // Cancel — discard edits
                gPatchNameEdit.active = false;
            }
        }
    } else if (gModuleNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gModuleNameEdit.buffer);
            uint32_t cursorPos = gModuleNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gModuleNameEdit.buffer[cursorPos - 1],
                            &gModuleNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gModuleNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gModuleNameEdit.buffer[cursorPos],
                            &gModuleNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gModuleNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gModuleNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gModuleNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gModuleNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gModuleNameEdit.active = false;

                tModule * module = get_module(gModuleNameEdit.moduleKey);

                if (module != NULL) {
                    tMessageContent msg = {0};
                    char            oldName[CLAVIA_NAME_SIZE + 1];
                    COPY_STRING(oldName, module->name);
                    COPY_STRING(module->name, gModuleNameEdit.buffer);
                    msg.cmd                       = eMsgCmdSetModuleLabel;
                    msg.slot                      = gModuleNameEdit.moduleKey.slot;
                    msg.moduleLabelData.moduleKey = gModuleNameEdit.moduleKey;
                    COPY_STRING(msg.moduleLabelData.name, gModuleNameEdit.buffer);
                    msg_send(&gToUsbThread, &msg);
                    undo_push_module_name(gModuleNameEdit.moduleKey, oldName, gModuleNameEdit.buffer);
                }
            } else if (key == GLFW_KEY_ESCAPE) {
                gModuleNameEdit.active = false;  // discard
            }
        }
    } else if (gParamNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gParamNameEdit.buffer);
            uint32_t cursorPos = gParamNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gParamNameEdit.buffer[cursorPos - 1],
                            &gParamNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gParamNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gParamNameEdit.buffer[cursorPos],
                            &gParamNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gParamNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gParamNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gParamNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gParamNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gParamNameEdit.active = false;

                tModule * module = get_module(gParamNameEdit.moduleKey);

                if (module != NULL) {
                    tMessageContent msg       = {0};
                    uint32_t        pi        = gParamNameEdit.paramIndex;
                    uint32_t        li        = gParamNameEdit.labelIndex;
                    uint32_t        variation = gPatchDescr[module->key.slot].activeVariation;
                    uint32_t        paramRef  = module->param[variation][pi].paramRef;
                    bool            oldSet    = module->paramNameSet[pi][li];
                    char            oldName[PROTOCOL_PARAM_NAME_SIZE + 1];
                    COPY_STRING(oldName, module->paramName[pi][li]);

                    module->paramNameSet[pi][li]  = true;
                    COPY_STRING(module->paramName[pi][li], gParamNameEdit.buffer);

                    if (paramLocationList[paramRef].type == paramTypeRadioEdit) {
                        // notes §2
                        uint32_t buttons = paramLocationList[paramRef].range;

                        for (uint32_t b = 0; (b < buttons) && (b < MAX_NUM_LABELS); b++) {
                            if (!module->paramNameSet[pi][b]) {
                                module->paramNameSet[pi][b] = true;
                                COPY_STRING(module->paramName[pi][b],
                                            radio_caption(module, pi, b, paramLocationList[paramRef].strMap));
                            }
                        }

                        module->paramNumLabels[pi] = (buttons < MAX_NUM_LABELS) ? buttons : MAX_NUM_LABELS;
                    } else {
                        module->paramNumLabels[pi] = 1;
                    }
                    msg.cmd                       = eMsgCmdSetParamLabel;
                    msg.slot                      = gParamNameEdit.moduleKey.slot;
                    msg.paramLabelData.moduleKey  = gParamNameEdit.moduleKey;
                    msg.paramLabelData.paramIndex = pi;
                    COPY_STRING(msg.paramLabelData.name, gParamNameEdit.buffer);
                    msg_send(&gToUsbThread, &msg);
                    undo_push_param_name(gParamNameEdit.moduleKey, pi, li,
                                         oldName, oldSet,
                                         gParamNameEdit.buffer, true);
                }
            } else if (key == GLFW_KEY_ESCAPE) {
                gParamNameEdit.active = false;  // discard
            }
        }
    } else if (gSynthNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gSynthNameEdit.buffer);
            uint32_t cursorPos = gSynthNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gSynthNameEdit.buffer[cursorPos - 1],
                            &gSynthNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gSynthNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gSynthNameEdit.buffer[cursorPos],
                            &gSynthNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gSynthNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gSynthNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gSynthNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gSynthNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gSynthNameEdit.active = false;
                COPY_STRING(gSynthSettings.name, gSynthNameEdit.buffer);
                send_synth_settings_msg();
            } else if (key == GLFW_KEY_ESCAPE) {
                gSynthNameEdit.active = false;
            }
        }
    } else if (gPerfNameEdit.active) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            size_t   len       = strlen(gPerfNameEdit.buffer);
            uint32_t cursorPos = gPerfNameEdit.cursorPos;

            if (key == GLFW_KEY_BACKSPACE) {
                if (cursorPos > 0) {
                    memmove(&gPerfNameEdit.buffer[cursorPos - 1],
                            &gPerfNameEdit.buffer[cursorPos],
                            len - cursorPos + 1);
                    gPerfNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_DELETE) {
                if (cursorPos < len) {
                    memmove(&gPerfNameEdit.buffer[cursorPos],
                            &gPerfNameEdit.buffer[cursorPos + 1],
                            len - cursorPos);
                }
            } else if (key == GLFW_KEY_LEFT) {
                if (cursorPos > 0) {
                    gPerfNameEdit.cursorPos--;
                }
            } else if (key == GLFW_KEY_RIGHT) {
                if (cursorPos < len) {
                    gPerfNameEdit.cursorPos++;
                }
            } else if (key == GLFW_KEY_HOME) {
                gPerfNameEdit.cursorPos = 0;
            } else if (key == GLFW_KEY_END) {
                gPerfNameEdit.cursorPos = (uint32_t)len;
            } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
                gPerfNameEdit.active = false;
                char            oldPerfName[CLAVIA_NAME_SIZE + 1];
                COPY_STRING(oldPerfName, gGlobalSettings.perfName);
                COPY_STRING(gGlobalSettings.perfName, gPerfNameEdit.buffer);
                tMessageContent messageContent = {0};
                messageContent.cmd   = eMsgCmdWritePerfName;
                msg_send(&gToUsbThread, &messageContent);
                undo_push_perf_name(oldPerfName, gPerfNameEdit.buffer);
            } else if (key == GLFW_KEY_ESCAPE) {
                gPerfNameEdit.active = false;
            }
        }
    } else {
        return false;
    }
    return true;
}
