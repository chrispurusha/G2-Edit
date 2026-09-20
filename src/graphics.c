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
// Notes: Docs/code-notes/graphics.c.md - "// notes §k" refers there.

#ifdef __cplusplus
extern "C" {
#endif

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include <math.h>


#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "protocol.h"
#include "usbComms.h"
#include "graphics.h"
#include "palette.h"
#include "topbarRender.h"
#include "splitView.h"
#include "utilsGraphics.h"
#include "synthlibWindow.h"
#include "synthlibPopups.h"
#include "mouseHandle.h"
#include "dataBase.h"
#include "patchWrite.h"
#include "moduleGraphics.h"
#include "canvasDrag.h"
#include "alertDialog.h"
#include "moduleResourcesAccess.h"
#include "topbarResourcesAccess.h"
#include "globalVars.h"
#include "synthSettingsResources.h"
#include "misc.h"
#include "clickRegion.h"
#include "patchParamsResources.h"
#include "perfSettingsResources.h"
#include "menus.h"
#include "undo.h"
#include "deviceSync.h"
#include "mutatorUI.h"
#include "mousePanels.h"
#include "paramPages.h"
#include "paramOverview.h"
#include "midiCcList.h"
#include "floatingPanel.h"
#include "floatingPanels.h"
#include "settingsPanels.h"
#include "helpPanel.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"
#include "soundEngine.h"
#include "audioOutput.h"
#include "paramCurves.h"
#include "paramOverlay.h"
#include <strings.h>

#include "appMenuBar.h"
#include "selection.h"
#include "contextMenu.h"
#include "fileBrowser.h"
#include "bankBrowser.h"
#include "alertDialog.h"
#include "synthlibHost.h"
#include "synthlibScale.h"
#include "synthlibPersistence.h"
#include "backdoor.h"

static void register_app_popups(void);

static FT_Library   gLibrary   = {0};
static FT_Face      gFace      = {0};
static _Atomic bool gNeedFocus = false;

void resize_window(int w, int h) {
    glfwSetWindowSize((GLFWwindow *)synthlib_window(), w, h);
}

void reposition_window(int x, int y) {
    glfwSetWindowPos((GLFWwindow *)synthlib_window(), x, y);
}

void set_window_title(const char * filePath) {
    char         newTitle[100] = {0};
    const char * filename      = strrchr(filePath, '/');

    if (filename) {
        filename += 1;  // Skip the slash
    } else {
        filename = filePath;
    }
    snprintf(newTitle, sizeof(newTitle), "%s - %s", WINDOW_TITLE, filename);
    glfwSetWindowTitle((GLFWwindow *)synthlib_window(), newTitle);
}

// notes §1
void render_scrollbars(void) {
    double renderWidth  = get_render_width() / gGlobalGuiScale;
    double renderHeight = get_render_height() / gGlobalGuiScale;

    // notes §2
    (void)renderWidth;
    (void)renderHeight;

    gScrollState.xThumb = (tRectangle){
        0
    };
    gScrollState.yThumb = (tRectangle){
        0
    };
    render_pane_scrollbars();
}

void wake_glfw(void) {
    // synthlib_request_redraw() already does both of these (safe to call from any thread).
    synthlib_request_redraw();
}

void notify_full_patch_change(void) {
    gLocation         = locationVa;
    // Set scrollbars back to top/left
    gScrollState.xBar = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_x_scroll_bar(gScrollState.xBar);
    gScrollState.yBar = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_y_scroll_bar(gScrollState.yBar);
}

// notes §3
static tSynthLibTheme gAppTheme = {0};

void apply_top_bar_height(void) {
    gAppTheme.topBarHeight = TOP_BAR_HEIGHT + MENU_BAR_HEIGHT + palette_band_height();
    configure_synthlib_theme(gAppTheme);
    synthlib_request_redraw();
}

void init_graphics(void) {
    char              title[128]           = {0};

    snprintf(title, sizeof(title), "%s - Build %s %s", WINDOW_TITLE, __DATE__, __TIME__);

    // notes §4
    const char *const priorityCategories[] = {
        patchTypeStrMap[patchTypeUser1], patchTypeStrMap[patchTypeUser2]
    };

    bank_browser_set_priority_categories(priorityCategories, ARRAY_SIZE(priorityCategories));

    split_view_init();   // one pane showing the Voice Area — the pre-split behaviour, as the default
    register_glfw_wake_cb(wake_glfw);
    register_full_patch_change_notify_cb(notify_full_patch_change);
    topbar_init_controls();

    // notes §5
    register_app_popups();

    // Built before the window is created so the palette's band - closed at startup - is already
    // accounted for, and so apply_top_bar_height() has something to re-send when it opens.
    gAppTheme = (tSynthLibTheme){
        .topBarHeight   = TOP_BAR_HEIGHT + MENU_BAR_HEIGHT + palette_band_height(),
        .orange1        = (tRgb)RGB_ORANGE_1,
        .orange2        = (tRgb)RGB_ORANGE_2,
        .greenOn        = (tRgb)RGB_GREEN_ON,
        .backgroundGrey = (tRgb)RGB_BACKGROUND_GREY,
    };

    synthlib_window_create(&(tSynthLibWindowConfig){
        .title           = title,
        .targetWidth     = TARGET_FRAME_BUFF_WIDTH,
        .targetHeight    = TARGET_FRAME_BUFF_HEIGHT,
        .dialMode        = eDialModeRotary,
        .theme           = gAppTheme,
        .mouseCoord      = get_global_gui_scaled_mouse_coord,
        // notes §6
        .pointerCaptured = is_cursor_hidden_dragging,
        .handlers        = &(const tSynthLibInputHandlers){
            .mouseButton = mouse_button,
            .cursorPos   = cursor_pos,
            .key         = key_callback,
            .character   = char_event,
            .scroll      = scroll_event,
            .windowFocus = window_focus_callback,   // clears held modifiers — see inputState.h
        },
    }, NULL);

    FT_Init_FreeType(&gLibrary);
    FT_New_Face(gLibrary, "/System/Library/Fonts/Supplemental/Arial.ttf", 0, &gFace);
    FT_Set_Char_Size(gFace, 0, 48 * 64, 300, 300);

    // Preload glyph textures
    if (!preload_glyph_textures("/System/Library/Fonts/Supplemental/Arial.ttf", 72.0f)) {
        LOG_ERROR("Failed to preload glyph textures\n");
    }
}

void read_file_into_memory_and_process(const char * filepath) {
    int64_t   byteOffset = 0;
    int64_t   fileSize   = 0;
    FILE *    file       = NULL;
    uint8_t * buff       = NULL;
    size_t    readSize   = 0;
    uint8_t   version    = 23;
    uint8_t   type       = 0;
    uint32_t  readCrc    = 0;
    uint32_t  calcCrc    = 0;
    uint32_t  slot       = gSlot;

    if (device_ready()) {
        // Online: hand the entire load to the USB thread as one ordered command. It opens the file,
        // validates the CRC, sniffs the type byte (patch vs perf) and does the clear/parse/push
        // itself, so the UI thread touches neither the file nor the shared DB. See load_file_to_device().
        tMessageContent msg = {0};
        msg.cmd                = eMsgCmdLoadFile;
        msg.patchFileData.slot = slot;
        COPY_STRING(msg.patchFileData.filePath, filepath);
        msg_send(&gToUsbThread, &msg);
        device_op_begin("Loading...");
        return;
    }
    file     = fopen(filepath, "rb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    clearerr(file);

    buff     = (uint8_t *)malloc(fileSize);

    if (buff == NULL) {
        LOG_ERROR("Memory allocation failed\n");
        fclose(file);
        return;
    }
    readSize = fread(buff, 1, fileSize, file);

    if (readSize != fileSize) {
        LOG_ERROR("Failed to read entire file\n");
        free(buff);
        fclose(file);
        return;
    }

    for (int64_t i = 0; i < fileSize; i++) {
        if (buff[i] == 0x00) {
            byteOffset = i + 1;
            break;
        }
    }

    readCrc  = buff[fileSize - 2] << 8 | buff[fileSize - 1];
    calcCrc  = calc_crc16(buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));

    if (readCrc == calcCrc) {
        version = buff[byteOffset++];
        type    = buff[byteOffset++];
        LOG_DEBUG("Version %u\n", version);
        LOG_DEBUG("Type %u\n", type);

        // Offline only: the online case returned early above (handled on the USB thread). With no
        // device there's no mode switch and no patch-change cascade to race, so we parse straight
        // into the database on this thread.
        if (type == 0) {
            clear_slot_data(slot);
            parse_patch(slot, buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));
            set_patch_name_from_filename(slot, filepath);
        } else if (type == 1) {
            // Performance file — parse_perf clears all 4 slots and populates them; slot names come
            // from the file itself so set_patch_name_from_filename is not called. Derive the
            // performance name from the filename (strip dir + .prf2).
            for (int i = 0; i < MAX_SLOTS; i++) {
                clear_slot_data(i);
            }

            const char * slash    = strrchr(filepath, '/');
            const char * baseName = slash ? slash + 1 : filepath;
            COPY_STRING(gGlobalSettings.perfName, baseName);
            char *       dot      = strrchr(gGlobalSettings.perfName, '.');

            if (dot) {
                *dot = '\0';
            }
            gGlobalSettings.perfMode = 1;
            parse_perf(buff + byteOffset, (int)((fileSize - byteOffset) - 2));
        }
    } else {
        LOG_WARNING("CRC check failed\n");
    }
    free(buff);
    fclose(file);
}

// notes §7
static void remember_file_path(const char * path, bool isPerf) {
    // notes §8
    uint32_t slot  = gSlot;

    if ((path == NULL) || (path[0] == '\0')) {
        return;
    }

    if (slot >= MAX_SLOTS) {
        LOG_ERROR("remember_file_path: slot %u out of range\n", slot);
        return;
    }

    // notes §9
    if (isPerf) {
        if (gSavedPerfPath != path) {
            COPY_STRING(gSavedPerfPath, path);
        }
    } else if (gSavedPatchPath[slot] != path) {
        COPY_STRING(gSavedPatchPath[slot], path);
    }
    // The file is now where this lives, rather than any bank location it was loaded from
    uint32_t index = isPerf ? BANK_ORIGIN_PERF : slot;

    gSavedPathSerial[index] = gPatchSourceSerial[index];
    gBankOrigin[index]      = BANK_ORIGIN_NONE;
}

static void on_file_opened(const char * path) {
    if (path) {
        LOG_INFO("Selected file: %s", path);
        read_file_into_memory_and_process(path);
        recent_files_add(path);    // File > Open Recent — same event that settles File > Save's target
        // Read AFTER the load: it is the load that settles perf vs patch
        remember_file_path(path, gGlobalSettings.perfMode == 1);
        //set_window_title(path);
    }
    gNeedFocus = true;
    wake_glfw();
}

// notes §10
static uint32_t sConflictMaskPendingSave = 0;

// Tells the USB thread to resume: push the editor's slots to the G2, or take the G2's patches.
// Either way the undo history goes, because after a wholesale resend (or a wholesale replacement)
// its entries describe a device state that no longer exists.
static void resolve_offline_conflict(uint32_t slotMask, bool pushToDevice) {
    tMessageContent msg = {0};

    msg.cmd                          = eMsgCmdResolveOfflineEdits;
    msg.offlineEditData.slotMask     = slotMask;
    msg.offlineEditData.pushToDevice = pushToDevice;
    msg_send(&gToUsbThread, &msg);

    undo_clear();
    wake_glfw();
}

static void on_offline_conflict_choice(int choice) {
    uint32_t slotMask = sConflictMaskPendingSave;

    sConflictMaskPendingSave = 0;

    switch (choice) {
        case 0:  // Send to Synth — the editor's patches win
            resolve_offline_conflict(slotMask, true);
            break;

        case 1:  // Save As... — park the resolve until the save has been through the browser
            sConflictMaskPendingSave = slotMask;
            file_menu_save_patch(gGlobalSettings.perfMode == 1);
            break;

        default:  // Pull from Synth, and what Escape means: the G2's patches win
            resolve_offline_conflict(slotMask, false);
            break;
    }
}

void show_offline_conflict_dialog(uint32_t slotMask) {
    char     location[FILE_PATH_SIZE] = {0};
    char     slots[32]                = {0};
    char     message[512]             = {0};
    uint32_t recovered                = device_sync_write_recovery_files(slotMask, location, sizeof(location));
    size_t   used                     = 0;

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if ((slotMask & (1u << slot)) != 0) {
            used += (size_t)snprintf(slots + used, sizeof(slots) - used, "%s%c",
                                     (used > 0) ? ", " : "", (char)('A' + slot));
        }
    }

    snprintf(message, sizeof(message),
             "You edited slot %s while the G2 was disconnected, so the editor and the synth now "
             "disagree.\n\n"
             "%s\n\n"
             "Send to Synth writes the editor's patches over the synth's. Pull from Synth replaces "
             "the editor's with the synth's. Either way the undo history is cleared.",
             slots,
             (recovered > 0)
             ? "Your edits have already been saved to a recovery file, so nothing here can lose them:"
             : "WARNING: a recovery file could not be written, so choosing Pull WILL discard your edits.");

    if ((recovered > 0) && (location[0] != '\0')) {
        used = strlen(message);
        snprintf(message + used, sizeof(message) - used, "\n%s", location);
    }
    sConflictMaskPendingSave = slotMask;  // Carried into the callback, which has no user-data slot
    show_choice("Offline Edits", message, "Send to Synth", "Save As...", "Pull from Synth",
                on_offline_conflict_choice);
}

static void on_file_saved(const char * path) {
    uint32_t slot = gSlot;

    if (path) {
        LOG_INFO("Saving file: %s", path);

        // §14 - what the MENU ITEM asked for, not what mode the G2 happens to be in. Saving one
        // slot's patch while the G2 is in Performance mode is a perfectly ordinary thing to want.
        if (file_menu_save_is_perf()) {
            if (device_ready()) {
                // Online: serialise the whole DB (all 4 slots) on the USB thread — same reason as the
                // patch save below: the DB read must be atomic against this thread's async reparses.
                tMessageContent msg = {0};
                msg.cmd = eMsgCmdSavePerfFile;
                COPY_STRING(msg.patchFileData.filePath, path);
                msg_send(&gToUsbThread, &msg);
                device_op_begin("Saving...");
            } else {
                write_perf_to_file(path);
            }
        } else if (device_ready()) {
            // Online: serialise the slot on the USB thread so the DB read can't tear against the USB
            // thread's own DB writes (e.g. an async patch-change reparse). Name update (a single
            // field, harmless) stays here so the title bar reflects the saved filename immediately.
            tMessageContent msg = {0};
            msg.cmd                = eMsgCmdSavePatchFile;
            msg.patchFileData.slot = slot;
            COPY_STRING(msg.patchFileData.filePath, path);
            msg_send(&gToUsbThread, &msg);
            device_op_begin("Saving...");
            set_patch_name_from_filename(slot, path);
        } else {
            write_database_to_file(path, slot);
            set_patch_name_from_filename(slot, path);
        }
        remember_file_path(path, file_menu_save_is_perf());

        // notes §11
        recent_files_add(path);
    }

    // The save that a conflict's "Save As..." branch was waiting on. Resolve either way: a
    // cancelled save (path == NULL) still has to release the parked USB thread, and the recovery
    // file written before the dialog means taking the synth's copy is not a loss even then.
    if (sConflictMaskPendingSave != 0) {
        uint32_t slotMask = sConflictMaskPendingSave;

        sConflictMaskPendingSave = 0;

        if (path != NULL) {
            // Saved where they chose, so the automatic copy is redundant. Only here — on the Pull
            // branch that file is the only copy of the work that exists.
            device_sync_discard_recovery_files();
        }
        resolve_offline_conflict(slotMask, false);
    }
    gNeedFocus = true;
    wake_glfw();
}

// notes §12
static void on_store_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdStorePatch;
    msg.bankLocationPerfData.bank     = gStorePeekBank;
    msg.bankLocationPerfData.location = gStorePeekLocation;
    msg.bankLocationPerfData.isPerf   = gStorePeekIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// Same shape as on_store_confirmed above, but for Delete — target comes from
// gDeletePeekBank/gDeletePeekLocation/gDeletePeekIsPerf, set by peek_delete_target().
static void on_delete_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdDeleteBankLocation;
    msg.bankLocationPerfData.bank     = gDeletePeekBank;
    msg.bankLocationPerfData.location = gDeletePeekLocation;
    msg.bankLocationPerfData.isPerf   = gDeletePeekIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// Same shape as on_store_confirmed/on_delete_confirmed above, but for Load — target comes from
// gLoadPeekBank/gLoadPeekLocation/gLoadPeekIsPerf, set by peek_load_target().
static void on_load_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdLoadPatch;
    msg.bankLocationPerfData.bank     = gLoadPeekBank;
    msg.bankLocationPerfData.location = gLoadPeekLocation;
    msg.bankLocationPerfData.isPerf   = gLoadPeekIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// notes §13
static void on_synth_restore_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd = eMsgCmdApplySynthSettingsRestore;
    msg_send(&gToUsbThread, &msg);
}

// notes §14
#define DEVICE_OP_TIMEOUT_MS    (5000.0)

static double sDeviceOpStartTime = 0.0;

void device_op_begin(const char * label) {
    if (gDeviceOpInProgress == 0) {
        sDeviceOpStartTime = get_time_ms();
    }
    gDeviceOpInProgress++;

    if (label != NULL) {
        COPY_STRING(gDeviceOpLabel, label);
    }
    synthlib_request_redraw();
}

void device_op_end(void) {
    if (gDeviceOpInProgress > 0) {
        gDeviceOpInProgress--;
    }
    synthlib_request_redraw();
}

// Dim the canvas + a small centred panel while a whole-slot device op is in flight. Drawn on top of
// the normal canvas (below the alert dialog, which only appears once the op has completed).
static void render_device_busy_overlay(void) {
    if (gDeviceOpInProgress <= 0) {
        return;
    }
    double renderW = get_render_width() / gGlobalGuiScale;
    double renderH = get_render_height() / gGlobalGuiScale;
    double boxW    = 240.0;
    double boxH    = 56.0;
    double boxX    = (renderW - boxW) / 2.0;
    double boxY    = (renderH - boxH) / 2.0;
    double titleH  = 24.0;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH, "Please wait");
    render_text(mainArea, (tRectangle){{boxX + 10.0, boxY + titleH + 10.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, gDeviceOpLabel);
}

static void check_action_flags(void) {
    // notes §15
    if (!alert_dialog_active()) {
        tMessageContent resp = {0};

        if (msg_receive(&gToGuiThread, eRcvPoll, &resp) == EXIT_SUCCESS) {
            const char * slash          = strrchr(resp.fileResultData.path, '/');
            const char * baseName       = (slash != NULL) ? slash + 1 : resp.fileResultData.path;
            char         alertMsg[1100] = {0};

            switch (resp.cmd) {
                case eRspFileLoad:
                    device_op_end();

                    if (resp.fileResultData.result != EXIT_SUCCESS) {
                        snprintf(alertMsg, sizeof(alertMsg),
                                 "Could not load \"%s\".\nThe file may be corrupt or not a valid G2 patch/performance.",
                                 baseName);
                        show_alert("Load Failed", alertMsg);
                    }
                    break;

                case eRspFileSave:
                    device_op_end();

                    if (resp.fileResultData.result != EXIT_SUCCESS) {
                        snprintf(alertMsg, sizeof(alertMsg),
                                 "Could not save \"%s\".\nCheck the location is writable and has free space.",
                                 baseName);
                        show_alert("Save Failed", alertMsg);
                    }
                    break;

                case eRspNewPatch:
                    device_op_end();

                    if (resp.fileResultData.result != EXIT_SUCCESS) {
                        show_alert("New Patch Failed", "The G2 did not accept the new patch. It may have gone offline.");
                    }
                    break;

                case eRspOfflineConflict:
                    show_offline_conflict_dialog(resp.offlineEditData.slotMask);
                    break;

                case eRspAlert:
                    show_alert(resp.alertData.title, resp.alertData.message);
                    break;

                case eRspStorePeek:
                {
                    // Peek data is in the gStorePeek* globals (set by the USB thread before it posted
                    // this); this response is just the "ready" nudge to open the overwrite confirm.
                    char title[64]    = {0};
                    char message[320] = {0};
                    bool isPerf       = gStorePeekIsPerf;

                    snprintf(title, sizeof(title), "Store %s to Bank %u, Location %u",
                             isPerf ? "Performance" : "Patch", gStorePeekBank + 1, gStorePeekLocation + 1);

                    if (gStorePeekFailed) {
                        show_alert(title, "Could not check what's currently at this location — the G2 may have gone offline. Try again.");
                    } else {
                        if (gStorePeekPopulated) {
                            snprintf(message, sizeof(message),
                                     "This location currently contains \"%s\". Storing will overwrite it with the current edit buffer %s. "
                                     "This cannot be undone.", gStorePeekName, isPerf ? "performance" : "patch");
                        } else {
                            snprintf(message, sizeof(message),
                                     "This location is currently empty. Store the current edit buffer %s there?", isPerf ? "performance" : "patch");
                        }
                        show_confirm(title, message, "Store...", on_store_confirmed);
                    }
                    break;
                }

                case eRspDeletePeek:
                {
                    char title[64]    = {0};
                    char message[320] = {0};
                    bool isPerf       = gDeletePeekIsPerf;

                    snprintf(title, sizeof(title), "Delete %s Bank %u, Location %u",
                             isPerf ? "Performance" : "Patch", gDeletePeekBank + 1, gDeletePeekLocation + 1);

                    if (gDeletePeekFailed) {
                        show_alert(title, "Could not check what's currently at this location — the G2 may have gone offline. Try again.");
                    } else {
                        if (gDeletePeekPopulated) {
                            snprintf(message, sizeof(message),
                                     "This location currently contains \"%s\". Deleting will erase it. This cannot be undone.", gDeletePeekName);
                        } else {
                            snprintf(message, sizeof(message), "This location is already empty. Nothing to delete — continue anyway?");
                        }
                        show_confirm(title, message, "Delete...", on_delete_confirmed);
                    }
                    break;
                }

                case eRspLoadPeek:
                {
                    char title[64] = {0};
                    bool isPerf    = gLoadPeekIsPerf;

                    snprintf(title, sizeof(title), "Load %s from Bank %u, Location %u",
                             isPerf ? "Performance" : "Patch", gLoadPeekBank + 1, gLoadPeekLocation + 1);

                    if (gLoadPeekFailed) {
                        show_alert(title, "Could not check what's at this location — the G2 may have gone offline. Try again.");
                    } else if (!gLoadPeekPopulated) {
                        show_alert(title, "This location is empty. There's nothing to load.");
                    } else {
                        // No confirm — loading from a file doesn't ask "replace the current edit buffer?" either.
                        on_load_confirmed(true);
                    }
                    break;
                }

                case eRspSynthRestorePeek:
                {
                    char message[400] = {0};

                    if (gSynthRestorePeekFailed) {
                        show_alert("Restore Synth Settings", gSynthRestorePeekErrorMessage);
                    } else {
                        snprintf(message, sizeof(message),
                                 "This will overwrite the current synth settings on the G2 with the contents of \"%s\" (Name: %s). "
                                 "This cannot be undone.", gSynthRestorePeekFileName, gSynthRestorePeekName);
                        show_confirm("Restore Synth Settings", message, "Restore...", on_synth_restore_confirmed);
                    }
                    break;
                }

                case eRspShowOpenRead:
                    // Deferred from a menu click so the browser opens from the render loop, not mid-callback.
                    open_file_browser_read(on_file_opened);
                    break;

                case eRspOpenPath:
                    // File > Open Recent. Goes through on_file_opened() exactly as the browser does,
                    // so a recent open records its path, updates File > Save's target and re-orders
                    // the recent list itself by the same route — no second copy of any of that.
                    on_file_opened(resp.patchFileData.filePath);
                    break;

                case eRspShowOpenWrite:
                {
                    uint32_t slot                              = gSlot;
                    char     patchName[CLAVIA_NAME_SIZE + 1]   = {0};
                    char     defaultName[CLAVIA_NAME_SIZE + 6] = {0}; // name (16) + extension (5) + null

                    if (file_menu_save_is_perf()) {
                        if (gGlobalSettings.perfName[0] != '\0') {
                            snprintf(defaultName, sizeof(defaultName), "%s.prf2", gGlobalSettings.perfName);
                        } else {
                            COPY_STRING(defaultName, "performance.prf2");
                        }
                    } else {
                        COPY_STRING(patchName, gGlobalSettings.slot[slot].patchName);

                        if (patchName[0] != '\0') {
                            snprintf(defaultName, sizeof(defaultName), "%s.pch2", patchName);
                        } else {
                            snprintf(defaultName, sizeof(defaultName), "patch.pch2");
                        }
                    }
                    open_file_browser_write(on_file_saved, defaultName);
                    break;
                }

                case eRspSaveToCurrentPath:
                {
                    // File > Save: straight back to the remembered path, no browser. The menu only
                    // offers this once there IS one, but re-check here — the drain runs a frame or
                    // more after the click, and a slot change in between would move the goalposts.
                    const char * path = file_menu_save_is_perf() ? gSavedPerfPath : gSavedPatchPath[gSlot];

                    if (path[0] == '\0') {
                        open_file_browser_write(on_file_saved, "patch.pch2");  // Nothing to save back to
                    } else {
                        on_file_saved(path);
                    }
                    break;
                }

                default:
                    break;
            }

            if (msg_count(&gToGuiThread) > 0) {
                wake_glfw(); // more queued — come back next frame rather than blocking in glfwWaitEvents
            }
        }
    }

    // notes §16
    if ((gDeviceOpInProgress > 0) && ((get_time_ms() - sDeviceOpStartTime) > DEVICE_OP_TIMEOUT_MS)) {
        LOG_ERROR("Device op busy-state timed out — force-clearing\n");
        gDeviceOpInProgress = 0;
        synthlib_request_redraw();
    }

    if (gNeedFocus == true) {
        gNeedFocus = false;
        glfwFocusWindow((GLFWwindow *)synthlib_window());
    }
    // notes §17
}

static void render_bank_backup_progress(void) {
    if (!gBankBackupActive) {
        return;
    }
    double renderW      = get_render_width() / gGlobalGuiScale;
    double renderH      = get_render_height() / gGlobalGuiScale;
    double boxW         = 360.0;
    double boxH         = 90.0;
    double boxX         = (renderW - boxW) / 2.0;
    double boxY         = (renderH - boxH) / 2.0;
    double margin       = 10.0;
    double titleH       = 24.0;
    char   lineBuf[128] = {0};
    bool   isPerf       = gBankBackupIsPerf;
    bool   isEverything = gBankBackupIsEverything;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH,
                      isEverything ? "Backup Everything" : (isPerf ? "Backing Up Performance Bank" : "Backing Up Patch Bank"));

    if (isEverything) {
        snprintf(lineBuf, sizeof(lineBuf), "%s Bank %u of %u - location %u / %u",
                 isPerf ? "Performance" : "Patch", gBankBackupBank + 1,
                 isPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS, gBankBackupLocation + 1, NUM_LOCATIONS_PER_BANK);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Bank %u - location %u / %u",
                 gBankBackupBank + 1, gBankBackupLocation + 1, NUM_LOCATIONS_PER_BANK);
    }
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "%u %s%s written so far",
             gBankBackupWritten, isPerf ? "performance" : "patch",
             gBankBackupWritten == 1 ? "" : (isPerf ? "s" : "es"));
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin + STANDARD_TEXT_HEIGHT + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    // Progress bar
    double barY = boxY + boxH - margin - 8.0;
    double barW = boxW - margin * 2.0;
    double frac = (double)(gBankBackupLocation + 1) / (double)NUM_LOCATIONS_PER_BANK;

    set_rgb_colour((tRgb)RGB_GREY_9);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW, 8.0}});
    set_rgb_colour((tRgb)RGB_GREEN_ON);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW * frac, 8.0}});
}

static void render_bank_restore_progress(void) {
    if (!gBankRestoreActive) {
        return;
    }
    double renderW      = get_render_width() / gGlobalGuiScale;
    double renderH      = get_render_height() / gGlobalGuiScale;
    double boxW         = 360.0;
    double boxH         = 90.0;
    double boxX         = (renderW - boxW) / 2.0;
    double boxY         = (renderH - boxH) / 2.0;
    double margin       = 10.0;
    double titleH       = 24.0;
    char   lineBuf[128] = {0};
    bool   isPerf       = gBankRestoreIsPerf;
    bool   isEverything = gBankRestoreIsEverything;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH,
                      isEverything ? "Restore Everything" : (isPerf ? "Restoring Performance Bank" : "Restoring Patch Bank"));

    if (isEverything) {
        snprintf(lineBuf, sizeof(lineBuf), "%s Bank %u of %u - location %u / %u",
                 isPerf ? "Performance" : "Patch", gBankRestoreBank + 1,
                 isPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS, gBankRestoreLocation + 1, NUM_LOCATIONS_PER_BANK);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Bank %u - location %u / %u",
                 gBankRestoreBank + 1, gBankRestoreLocation + 1, NUM_LOCATIONS_PER_BANK);
    }
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "%u %s%s written so far",
             gBankRestoreWritten, isPerf ? "performance" : "patch",
             gBankRestoreWritten == 1 ? "" : (isPerf ? "s" : "es"));
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin + STANDARD_TEXT_HEIGHT + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    // Progress bar
    double barY = boxY + boxH - margin - 8.0;
    double barW = boxW - margin * 2.0;
    double frac = (double)(gBankRestoreLocation + 1) / (double)NUM_LOCATIONS_PER_BANK;

    set_rgb_colour((tRgb)RGB_GREY_9);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW, 8.0}});
    set_rgb_colour((tRgb)RGB_GREEN_ON);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW * frac, 8.0}});
}


// notes §22
#ifdef ENABLE_MOUSE_CROSSHAIR

static bool gShowMouseCrosshair = false;

void toggle_mouse_crosshair(void) {
    gShowMouseCrosshair = !gShowMouseCrosshair;
    LOG_DEBUG("Mouse crosshair %s\n", gShowMouseCrosshair ? "ON" : "OFF");
}

static void render_mouse_crosshair(void) {
    tCoord coord      = {0};
    char   buff[64]   = {0};
    double logicalW   = 0.0;
    double logicalH   = 0.0;
    double thickness  = 0.0;
    double textHeight = STANDARD_TEXT_HEIGHT;
    double textX      = 0.0;
    double textY      = 0.0;

    if (gShowMouseCrosshair == false) {
        return;
    }
    logicalW  = get_render_width() / gGlobalGuiScale;
    logicalH  = get_render_height() / gGlobalGuiScale;
    thickness = 1.0 / gGlobalGuiScale; // exactly one device pixel at any scale

    get_global_gui_scaled_mouse_coord(&coord);

    set_rgb_colour((tRgb)RGB_RED_7);
    render_rectangle(mainArea, (tRectangle){{0.0, coord.y}, {logicalW, thickness}});
    render_rectangle(mainArea, (tRectangle){{coord.x, 0.0}, {thickness, logicalH}});

    // Keep the readout on-screen when the cursor is near the right/top edge,
    // otherwise the one value you actually want to read is the one clipped away.
    snprintf(buff, sizeof(buff), "%.1f, %.1f", coord.x, coord.y);
    textX     = coord.x + 5.0;
    textY     = coord.y - 5.0;

    if (textX > logicalW - 90.0) {
        textX = coord.x - 90.0;
    }

    if (textY < textHeight) {
        textY = coord.y + textHeight + 5.0;
    }
    render_text(mainArea, (tRectangle){{textX, textY}, {BLANK_SIZE, textHeight}}, buff);
}
#endif


// notes §34
static const tSynthLibPopup gAppPopups[] = {
    // notes §35
    {"bankBackup",     SYNTHLIB_POPUP_LAYER_CONTEXT_MENU + 20, false, NULL, render_bank_backup_progress,  NULL, NULL,                  NULL,                NULL,                   NULL},
    {"bankRestore",    SYNTHLIB_POPUP_LAYER_CONTEXT_MENU + 30, false, NULL, render_bank_restore_progress, NULL, NULL,                  NULL,                NULL,                   NULL},
    {"deviceBusy",     SYNTHLIB_POPUP_LAYER_BROWSERS + 10,     false, NULL, render_device_busy_overlay,   NULL, NULL,                  NULL,                NULL,                   NULL},

    // The group, not the panels: their order among themselves is dynamic (they raise on click) and
    // belongs to floatingPanel.c. What is constant, and so belongs here, is that all of them sit
    // above the fixed panels below and below the context menu above.
    {"floatingPanels", SYNTHLIB_POPUP_LAYER_CONTEXT_MENU - 10, false, NULL, floating_panels_render,       NULL, floating_panels_mouse, floating_panels_key, floating_panels_scroll, NULL},
};

static void register_app_popups(void) {
    synthlib_popups_register(gAppPopups, (uint32_t)(sizeof(gAppPopups) / sizeof(gAppPopups[0])));
    synthlib_popups_set_menu_bar(gAppMenuBar, app_menu_bar_rect);
}

// Renders one full frame and swaps buffers. Extracted from do_graphics_loop's inlined render block
// so the backdoor SCREENSHOT command can force a synchronous frame (see backdoor_screenshot() in
// backdoor.c) — which is the only reason it is not static.
void render_frame(void) {
    // notes §36
    database_read_lock();

    render_backend_clear((tRgb){0.8, 0.8, 0.8});

    // notes §4 in audioOutput.c - the device may have changed rate since the last frame. Here
    // rather than on the HAL thread that noticed: re-opening the unit is what tells the engine.
    (void)audio_output_poll_rate_change();

    // notes §37
    sound_engine_update_from_patch();

    clear_click_regions();

    // notes §38
    split_view_apply();

    tLocation focusLocation = gLocation;
    uint32_t  focusPane     = split_view_focused_pane();

    param_overlay_begin_frame();   // hoisted out of render_modules(): one queue per FRAME, not per pane

    for (uint32_t pane = 0; pane < module_pane_count(); pane++) {
        set_module_pane(pane);
        gLocation = (tLocation)split_view_location_for_pane(pane);
        module_pane_clip_begin();
        render_modules();
        render_cables();
        // Inside the pane's own clip and transform. Drawing every pane's chips in one pass after
        // the loop put them all through the FOCUSED pane's transform with no scissor, so the Voice
        // Area's annotations landed over the FX Area.
        param_overlay_render_pane(pane);
        module_pane_clip_end();
    }

    set_module_pane(focusPane);
    gLocation = focusLocation;

    render_split_bar();

    if (gCableDrag.active == true) {
        if (gCableDrag.rerouting) {
            // ONE DRAGGED LINE PER CABLE. A Ctrl-drag picks up the whole hole, so all of its cables
            // are following the cursor — drawing only the one whose far end happens to be recorded
            // in fromModuleKey showed a single line while several cables were actually in flight.
            for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
                tCable *      cable     = get_cable_slot(gSlot, gLocation, i);

                if (!cable_touches_connector(cable, gCableDrag.rerouteModuleIndex,
                                             gCableDrag.rerouteIoCount, gCableDrag.rerouteDir)) {
                    continue;
                }
                uint32_t      farModule = 0;
                uint32_t      farIo     = 0;
                tConnectorDir farDir    = connectorDirIn;

                cable_far_end(cable, gCableDrag.rerouteModuleIndex, gCableDrag.rerouteIoCount,
                              &farModule, &farIo, &farDir);

                tModule *     module    = get_module_slot(gSlot, gLocation, farModule);
                int           index     = (module != NULL) ? find_index_from_io_count(module, farDir, (int)farIo) : -1;

                if (index < 0) {
                    continue;
                }
                set_rgb_colour(gCableColourMap[cable->colour]);   // the cable's own colour, not the connector's
                render_cable_from_to(module->connector[index], gCableDrag.toConnector, 4.0);
            }
        } else {
            tModule * module = get_module(gCableDrag.fromModuleKey);

            if (module != NULL) {
                tCableColour dragColour = cable_colour_for_connector_type(module->connector[gCableDrag.fromConnectorIndex].type);
                set_rgb_colour(gCableColourMap[dragColour]);
                render_cable_from_to(module->connector[gCableDrag.fromConnectorIndex], gCableDrag.toConnector, 4.0);
            }
        }
    }
    render_top_bar();
    palette_render();

    // notes §39
    render_menu_bar(gAppMenuBar, app_menu_bar_rect());
    render_morph_groups();
    render_scrollbars();

    // notes §40
    synthlib_popups_render();

#ifdef ENABLE_MOUSE_CROSSHAIR
    render_mouse_crosshair();     // TEMPORARY debug aid (F9) — above even the modal, so it is never hidden
#endif

    // Released BEFORE the present. Everything that reads the database has been done by now - what is
    // left is handing a finished vertex array to the GPU - and a present can block on the display,
    // which is not a thing to keep the USB thread waiting behind.
    database_read_unlock();

    // Submits the frame's one vertex array and puts it on screen. This was a render_backend_flush()
    // followed by glfwSwapBuffers() — the last GLFW call in this loop, and one that cannot exist
    // under Metal, where the window is created with no context to swap. See utilsGraphics.h.
    render_present();
}

void do_graphics_loop(void) {
    bool reDraw = false;

    while ((!synthlib_quit_requested()) && (!glfwWindowShouldClose((GLFWwindow *)synthlib_window()))) {
        check_action_flags();
        // notes §41
        synthlib_popups_tick();

        // notes §42
        {
            static uint32_t lastLampState = 0;
            uint32_t        lampState     = comms_lamp_state();

            if (lampState != lastLampState) {
                lastLampState = lampState;
                synthlib_request_redraw();
            }
        }

        // notes §43
        if (sound_engine_meters_dirty()) {
            synthlib_request_redraw();
        }
        reDraw = synthlib_consume_redraw();

        if (reDraw == true) {
            render_frame();
        }
        // See backdoor.c's own header comment — a cheap no-op access() check
        // every iteration when idle, and completely skipped (returns
        // immediately) unless the G2_EDIT_BACKDOOR env var is set.
        backdoor_poll();

        // The Virtual Keyboard's Repeat button. No-op unless a repeat is actually running.
        virtual_keyboard_tick();

        // Belt and braces: a hidden pointer with no drag behind it never survives a frame, and nor
        // does a canvas gesture whose release went missing.
        recover_lost_cursor();

        // The selection is only valid for the slot, location and patch currently on screen.
        selection_validate();

        if ((gModuleDrag.active == true) || (gCableDrag.active == true) || (gContextMenu.active == true)) {
            tCoord at = {0};

            get_global_gui_scaled_mouse_coord(&at);
            cursor_pos(at);   // Artificially do cursor_pos call for drag scrolling when cursor not moving
            glfwWaitEventsTimeout(0.016);
        } else if (gDeviceOpInProgress > 0) {
            glfwWaitEventsTimeout(0.05); // tick while busy so the device-op safety timeout can fire even with no events
        } else if (comms_lamps_lit()) {
            // notes §44
            glfwWaitEventsTimeout(0.1);
        } else if (sound_engine_active()) {
            // Awake often enough to notice a meter change. glfwWaitEvents() would block until the
            // next input event, which is exactly the state the meters were stuck in - and the flag
            // above means a tick that finds nothing new costs one comparison, not a frame.
            glfwWaitEventsTimeout(0.05);
        } else if (virtual_keyboard_wants_ticks()) {
            glfwWaitEventsTimeout(0.02); // Repeat is running — glfwWaitEvents() would stall it until the next input event
        } else if (backdoor_enabled()) {
            glfwWaitEventsTimeout(0.1);  // poll cadence for the backdoor command file — only when enabled (owner's normal launch keeps full idle-sleep below)
        } else {
            glfwWaitEvents();
        }
    }
}

void clean_up_graphics(void) {
    // Clean up
    FT_Done_Face(gFace);
    FT_Done_FreeType(gLibrary);
    free_textures();

    glfwDestroyWindow((GLFWwindow *)synthlib_window());
    synthlib_set_window(NULL);
    glfwTerminate();
}

#ifdef __cplusplus
}
#endif
