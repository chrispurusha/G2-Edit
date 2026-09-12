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
#include "helpPanel.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"
#include "soundEngine.h"
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

static FT_Library      gLibrary        = {0};
static FT_Face         gFace           = {0};
static _Atomic bool    gNeedFocus      = false;

#define MAX_NOTE_VISUAL_LINES    1000

typedef struct {
    int  bufStart;
    int  bufEnd;
    bool hardBreak;
} tNoteVisualLine;

static tNoteVisualLine gNoteLines[MAX_NOTE_VISUAL_LINES];
static int             gNoteLineCount  = 0;
static int             gNoteScrollLine = 0;
static double          gNoteTextX      = 0.0;
static double          gNoteTextY0     = 0.0;
static double          gNoteLineH      = 0.0;
static double          gNoteTextW      = 0.0;
static double          gNoteTextHParam = 0.0;

static int find_wrap_point(const char * text, int textLen, double textW, double textH) {
    if (textLen <= 0) {
        return 0;
    }
    char tmp[PATCH_NOTES_SIZE + 1];
    strncpy(tmp, text, (size_t)textLen);
    tmp[textLen] = '\0';

    if (get_text_width(tmp, textH, eNoCache) <= textW) {
        return textLen;
    }
    int  lo = 1, hi = textLen;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;

        strncpy(tmp, text, (size_t)mid);
        tmp[mid] = '\0';

        if (get_text_width(tmp, textH, eNoCache) <= textW) {
            lo = mid;
        } else{
            hi = mid - 1;
        }
    }
    int  charBreak = lo;

    int  wordBreak = charBreak;

    while (wordBreak > 0 && text[wordBreak - 1] != ' ') {
        wordBreak--;
    }
    return (wordBreak > 0) ? wordBreak : charBreak;
}

static void build_note_visual_lines(const char * buf, double textW, double textH) {
    gNoteLineCount = 0;
    int len = (int)strlen(buf);
    int pos = 0;

    while (gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
        int logicalEnd = pos;

        while (logicalEnd < len && buf[logicalEnd] != '\r') {
            logicalEnd++;
        }
        int segStart   = pos;

        while (gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
            int  remaining = logicalEnd - segStart;

            if (remaining <= 0) {
                if (segStart == pos) {
                    gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                        segStart, segStart, true
                    };
                }
                break;
            }
            int  wrapAt    = find_wrap_point(buf + segStart, remaining, textW, textH);
            bool softWrap  = (wrapAt < remaining);
            gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                segStart, segStart + wrapAt, !softWrap
            };
            segStart                    += wrapAt;

            if (!softWrap) {
                break;
            }
        }

        if (logicalEnd >= len) {
            break;
        }
        pos = logicalEnd + 1;

        if (pos >= len && gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
            gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                len, len, true
            };
            break;
        }
    }

    if (gNoteLineCount == 0) {
        gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
            0, 0, true
        };
    }
}

static int find_note_cursor_line(int cursorPos) {
    int result = 0;

    for (int i = 0; i < gNoteLineCount; i++) {
        if (gNoteLines[i].bufStart <= cursorPos) {
            result = i;
        }
    }

    return result;
}

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
static void remember_file_path(const char * path) {
    // notes §8
    uint32_t slot = gSlot;

    if ((path == NULL) || (path[0] == '\0')) {
        return;
    }

    if (slot >= MAX_SLOTS) {
        LOG_ERROR("remember_file_path: slot %u out of range\n", slot);
        return;
    }

    // notes §9
    if (gGlobalSettings.perfMode == 1) {
        if (gSavedPerfPath != path) {
            COPY_STRING(gSavedPerfPath, path);
        }
    } else if (gSavedPatchPath[slot] != path) {
        COPY_STRING(gSavedPatchPath[slot], path);
    }
}

static void on_file_opened(const char * path) {
    if (path) {
        LOG_INFO("Selected file: %s", path);
        read_file_into_memory_and_process(path);
        recent_files_add(path);    // File > Open Recent — same event that settles File > Save's target
        remember_file_path(path);  // Read AFTER the load: it is the load that settles perf vs patch
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
            file_menu_save_patch();
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

        if (gGlobalSettings.perfMode == 1) {
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
        remember_file_path(path);

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

                    if (gGlobalSettings.perfMode == 1) {
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
                    const char * path = (gGlobalSettings.perfMode == 1) ? gSavedPerfPath : gSavedPatchPath[gSlot];

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

// Helper: draw a fixed-width dropdown trigger button, return updated x.
static double render_dropdown(double x, double y, double btnH,
                              const char * valStr, const char * widestVal,
                              tRectangle * rect) {
    *rect = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width((char *)widestVal, btnH, eCache) + 8.0, btnH}},
                        (char *)valStr, (tRgb)RGB_BACKGROUND_GREY);
    return x + rect->size.w;
}

// Helper: MIDI note number → note name string (e.g. 0 → "C-1", 60 → "C4")
static void midi_note_name_str(uint8_t note, char * buf, size_t bufLen) {
    static const char * names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int                 octave  = (int)(note / 12) - 1;

    snprintf(buf, bufLen, "%s%d", names[note % 12], octave);
}

static void render_ss_section(double boxX, double boxW, double margin, double * y,
                              double rowH, double btnH,
                              const tSynthSettingItem * items, int count) {
    double itemW = (boxW - margin * 2.0) / 2.0;
    double x     = 0.0;
    int    i     = 0;

    for (i = 0; i < count; i++) {
        x              = boxX + margin + (i % 2) * itemW;

        if (i > 0 && (i % 2) == 0) {
            *y += rowH;
        }
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, *y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x             += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        *items[i].rect = draw_button(mainArea,
                                     (tRectangle){{x, *y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
                                     items[i].get_str(),
                                     items[i].get_colour());
    }

    *y += rowH;
}

static double render_pp_row(double x, double y, double btnH,
                            const tPatchParamItem * items, int count) {
    int i = 0;

    for (i = 0; i < count; i++) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x                                += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        gPatchParamRects[items[i].rectId] = draw_button(mainArea,
                                                        (tRectangle){{x, y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
                                                        items[i].get_str(),
                                                        items[i].get_colour());
        x                                += get_text_width((char *)items[i].widest, btnH, eCache) + 8.0 + 16.0;
    }

    return x;
}

static void midi_chan_str(uint8_t val, char * buf, size_t bufLen) {
    if (val >= 0x10) {
        snprintf(buf, bufLen, "Off");
    } else {
        snprintf(buf, bufLen, "%u", (unsigned)val + 1u);
    }
}

// notes §18

// Dims the module canvas behind a modal dialog. Not used by Mutator, which floats
// alongside the canvas rather than blocking it.

// notes §19

// notes §20

static void render_patch_settings_panel(void) {
    static const char * slotLabel[4] = {"A", "B", "C", "D"};
    double              boxW         = 600.0;
    double              boxH         = 453.0;
    double              boxX         = 0.0;
    double              boxY         = 0.0;
    double              margin       = 10.0;
    double              titleH       = 24.0;
    double              rowH         = 26.0;
    double              secH         = 18.0;
    double              btnH         = STANDARD_BUTTON_TEXT_HEIGHT;
    double              y            = 0.0;
    double              colW         = 0.0;
    double              x            = 0.0;
    int                 i            = 0;
    char                buf[16]      = {0};

    if (!gPatchSettingsEdit.active) {
        return;
    }
    // notes §21
    tRectangle          box          = floating_panel_place(&gPatchSettingsEdit.panel, boxW, boxH);

    boxX                                  = box.coord.x;
    boxY                                  = box.coord.y;
    y                                     = boxY + titleH + margin;

    gPatchSettingsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Synth Settings");
    gSettingsPanelRects.close             = draw_panel_close_button(mainArea, box, gSettingsPanelRects.closePressed);
    gPatchSettingsEdit.panel.closeRect    = gSettingsPanelRects.close;   // carve it out of the title-bar drag

    // ── Synth Name ─────────────────────────────────────────────────
    {
        char displayBuf[CLAVIA_NAME_SIZE + 2] = {0};

        x  = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;

        if (gSynthNameEdit.active) {
            uint32_t cp = gSynthNameEdit.cursorPos;
            memcpy(displayBuf, gSynthNameEdit.buffer, cp);
            displayBuf[cp]                = '|';
            memcpy(&displayBuf[cp + 1], &gSynthNameEdit.buffer[cp], strlen(gSynthNameEdit.buffer) - cp + 1);
            gSettingsPanelRects.synthName = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_WHITE);
        } else {
            snprintf(displayBuf, sizeof(displayBuf), "%s", gSynthSettings.name);
            gSettingsPanelRects.synthName = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_BACKGROUND_GREY);
        }
    }
    y   += rowH;

    // ── MIDI Channels ──────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "MIDI Channels");
    y   += secH;
    colW = (boxW - margin * 2.0) / 4.0;

    for (i = 0; i < 4; i++) {
        x  = boxX + margin + i * colW;
        snprintf(buf, sizeof(buf), "%c:", slotLabel[i][0]);
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, buf);
        x += get_text_width((char *)"A:", btnH, eCache) + 4.0;
        midi_chan_str(gSynthSettings.midiChanSlot[i], buf, sizeof(buf));
        render_dropdown(x, y, btnH, buf, "Off", &gSettingsPanelRects.midiChan[i]);
    }

    y   += rowH;

    // ── Global + SysEx ─────────────────────────────────────────────
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSGlobal, kSSGlobalCount);

    // ── Options ────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Options");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSOptions, kSSOptionsCount);

    // ── Tuning ─────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Tuning");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSTuning, kSSTuningCount);

    // ── Pedal ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Pedal");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSPedal, kSSPedalCount);

    // ── Sort Mode ──────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Sort Mode");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSSort, kSSSortCount);
}

static void render_patch_params_panel(void) {
    if (!gPatchParamsEdit.active) {
        return;
    }
    uint32_t   slot          = gPatchParamsEdit.slot;
    double     boxW          = 680.0;
    double     boxH          = 320.0;
    tRectangle box           = floating_panel_place(&gPatchParamsEdit.panel, boxW, boxH);
    double     boxX          = box.coord.x;
    double     boxY          = box.coord.y;
    double     margin        = 10.0;
    double     titleH        = 24.0;
    double     rowH          = 26.0;
    double     secH          = 18.0;
    double     btnH          = STANDARD_BUTTON_TEXT_HEIGHT;
    double     y             = boxY + titleH + margin;
    double     x             = 0.0;
    double     dialH         = 0.0;
    tModule *  sustMod       = get_module_slot(slot, (uint32_t)locationMorph, patchModuleSustain);
    tModule *  vibMod        = get_module_slot(slot, (uint32_t)locationMorph, patchModuleVibrato);
    tModule *  glideMod      = get_module_slot(slot, (uint32_t)locationMorph, patchModuleGlide);
    uint8_t    sustainPedal  = sustMod ? sustMod->param[0][SUSTAIN_PEDAL].value : 0;
    int8_t     octaveShift   = sustMod ? (int8_t)sustMod->param[0][OCTAVE_SHIFT].value : 0;
    uint8_t    vibratoRate   = vibMod ? vibMod->param[0][VIBRATO_RATE].value : 0;
    uint8_t    vibratoAmount = vibMod ? vibMod->param[0][VIBRATO_DEPTH].value : 0;
    uint8_t    glideTime     = glideMod ? glideMod->param[0][GLIDE_SPEED].value : 0;
    char       buf[16]       = {0};

    gPatchParamsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Patch Settings");
    gPatchParamClose                    = draw_panel_close_button(mainArea, box, gPatchParamClosePressed);
    gPatchParamsEdit.panel.closeRect    = gPatchParamClose;   // carve it out of the title-bar drag

    // ── Slot buttons in title bar ──────────────────────────────────
    {
        static const char * slotLabels[MAX_SLOTS] = {"A", "B", "C", "D"};
        double              slotBtnW              = get_text_width((char *)"A", btnH, eCache) /* + 6.0*/;
        // Right-aligned against the panel edge, now that the close control has moved top left.
        double              slotX                 = boxX + boxW - 8.0 - BORDER_LINE_WIDTH - ((slotBtnW + 8.0) * MAX_SLOTS);

        for (uint32_t s = 0; s < MAX_SLOTS; s++) {
            tRgb col = (s == slot) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
            gPatchParamSlots[s] = draw_button(mainArea,
                                              (tRectangle){{slotX + s * (slotBtnW + 8.0), boxY + 4.0}, {slotBtnW, btnH}},
                                              slotLabels[s], col);
        }
    }

    // ── Sustain Pedal + Octave Shift ───────────────────────────────
    {
        x                                = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Sustain Pedal:");
        x                               += get_text_width((char *)"Sustain Pedal:", btnH, eCache) + 4.0;
        gPatchParamRects[pPSustainPedal] = draw_button(mainArea,
                                                       (tRectangle){{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                       sustainPedal ? "On" : "Off",
                                                       sustainPedal ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

        x                                = boxX + boxW / 2.0;
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Octave Shift:");
        x                               += get_text_width((char *)"Octave Shift:", btnH, eCache) + 4.0;
        snprintf(buf, sizeof(buf), "%+d", (int)octaveShift);
        render_dropdown(x, y, btnH, buf, "+2", &gPatchParamRects[pPOctaveShift]);
    }
    y                                += rowH;

    // ── Arpeggiator ────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Arpeggiator");
    y                                += secH;
    render_pp_row(boxX + margin, y, btnH, kPPArp, kPPArpCount);
    y                                += rowH;

    // ── Vibrato ────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Vibrato");
    y                                += secH;
    x                                 = render_pp_row(boxX + margin, y, btnH, kPPVibrato, kPPVibratoCount);
    // Dial-anchored: the rect is the circle, and its label and value are drawn in the two text
    // rows above it - so the dial goes two rows below where the block used to start.
    dialH                             = 20.0;
    snprintf(buf, sizeof(buf), "%u cnt", (unsigned)vibratoAmount);
    gPatchParamRects[pPVibratoAmount] = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Amount", buf, btnH, vibratoAmount, 100, 0, (tRgb)RGB_BACKGROUND_GREY);
    x                                += get_text_width((char *)"100 cnt", btnH, eCache) + 8.0;
    snprintf(buf, sizeof(buf), "%.2f Hz", 4.0 + (vibratoRate / 127.0) * 4.0);
    gPatchParamRects[pPVibratoRate]   = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Rate", buf, btnH, vibratoRate, 127, 0, (tRgb)RGB_BACKGROUND_GREY);
    y                                += rowH;

    // ── Glide ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Glide");
    y                                += secH;
    x                                 = render_pp_row(boxX + margin, y, btnH, kPPGlide, kPPGlideCount);
    gPatchParamRects[pPGlideTime]     = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Time", get_glide_time_str(glideTime), btnH, glideTime, 127, 0, (tRgb)RGB_BACKGROUND_GREY);
    y                                += rowH;

    // ── Bend ───────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Bend");
    y                                += secH;
    render_pp_row(boxX + margin, y, btnH, kPPBend, kPPBendCount);

    (void)y;
}

static void render_perf_settings_panel(void) {
    if (!gPerfSettingsEdit.active) {
        return;
    }
    double     boxW                     = 700.0;
    double     boxH                     = 390.0;
    tRectangle box                      = floating_panel_place(&gPerfSettingsEdit.panel, boxW, boxH);
    double     boxX                     = box.coord.x;
    double     boxY                     = box.coord.y;
    double     margin                   = 10.0;
    double     titleH                   = 24.0;
    double     rowH                     = 26.0;
    double     secH                     = 18.0;
    double     btnH                     = STANDARD_BUTTON_TEXT_HEIGHT;
    double     y                        = boxY + titleH + margin;
    double     colX[kPSSlotToggleCount] = {0.0, 0.0, 0.0};
    int        i                        = 0;
    int        col                      = 0;
    char       buf[32]                  = {0};
    char       note[8]                  = {0};
    char       loNote[8]                = {0};
    char       hiNote[8]                = {0};
    char       rangeBuf[18]             = {0};

    gPerfSettingsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Performance Settings");
    gPerfSettingsPanelRects.close        = draw_panel_close_button(mainArea, box, gPerfSettingsPanelRects.closePressed);
    gPerfSettingsEdit.panel.closeRect    = gPerfSettingsPanelRects.close;   // carve it out of the title-bar drag

    // ── Perf Name ──────────────────────────────────────────────────
    {
        char   nameBuf[CLAVIA_NAME_SIZE + 2] = {0};
        double x                             = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;
        snprintf(nameBuf, sizeof(nameBuf), "%s", gGlobalSettings.perfName);
        draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}},
                    nameBuf, (tRgb)RGB_BACKGROUND_GREY);
    }
    y                                   += rowH;

    // ── Master Clock ───────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Master Clock");
    y                                   += secH;

    {
        // blockH is the whole BPM readout + dial, which the Running button centres against and
        // the row advance uses. The dial itself is one text row down from the top of that, since
        // render_dial_with_text() is dial-anchored and draws the BPM string above it.
        double blockH = 48.0;
        double x      = boxX + margin;
        snprintf(buf, sizeof(buf), "%u BPM", (unsigned)gGlobalSettings.masterClock);
        gPerfSettingsPanelRects.masterClock        = render_dial_with_text(mainArea, (tRectangle){{x, y + STANDARD_BUTTON_TEXT_HEIGHT}, {20.0, 20.0}}, NULL, buf, STANDARD_BUTTON_TEXT_HEIGHT, gGlobalSettings.masterClock >= 30 ? gGlobalSettings.masterClock - 30 : 0, 211, 0, (tRgb)RGB_BACKGROUND_GREY);
        x                                         += 20.0 + 12.0;
        gPerfSettingsPanelRects.masterClockRunning = draw_button(mainArea,
                                                                 (tRectangle){{x, y + (blockH - btnH) / 2.0}, {get_text_width((char *)"Stopped", btnH, eCache) + 8.0, btnH}},
                                                                 gGlobalSettings.masterClockRunning ? "Running" : "Stopped",
                                                                 gGlobalSettings.masterClockRunning ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        y                                         += blockH + 4.0;
    }

    // ── Slots ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Slots");
    y      += secH;

    double labelColW = get_text_width((char *)"Slot X:", btnH, eCache) + 8.0;
    double dropW     = get_text_width((char *)"On", btnH, eCache) + 16.0;
    double noteDropW = get_text_width((char *)"C#-1", btnH, eCache) + 10.0;
    double colEn     = boxX + margin + labelColW;
    double colKbd    = colEn + dropW + 8.0;
    double colHld    = colKbd + dropW + 30.0;
    double colLo     = colHld + dropW + 16.0;
    double colHi     = colLo + noteDropW + 8.0;
    double colRng    = colHi + noteDropW + 12.0;

    colX[0] = colEn;
    colX[1] = colKbd;
    colX[2] = colHld;

    // Column headers
    set_rgb_colour((tRgb)RGB_BLACK);
    render_text(mainArea, (tRectangle){{colEn, y}, {BLANK_SIZE, btnH}}, "Enable");
    render_text(mainArea, (tRectangle){{colKbd, y}, {BLANK_SIZE, btnH}}, "Keyboard");
    render_text(mainArea, (tRectangle){{colHld, y}, {BLANK_SIZE, btnH}}, "Hold");
    render_text(mainArea, (tRectangle){{colLo, y}, {BLANK_SIZE, btnH}}, "Lower");
    render_text(mainArea, (tRectangle){{colHi, y}, {BLANK_SIZE, btnH}}, "Upper");

    // Keyboard Range global toggle — right side of header
    {
        double x = colRng;
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Kbd Range:");
        x                                    += get_text_width((char *)"Kbd Range:", btnH, eCache) + 4.0;
        gPerfSettingsPanelRects.keyboardRange = draw_button(mainArea,
                                                            (tRectangle){{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                            gPerfSettings.keyboardRange ? "On" : "Off",
                                                            gPerfSettings.keyboardRange ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }
    y      += rowH;

    // Slot rows A–D
    static const char * slotLabel[] = {"Slot A:", "Slot B:", "Slot C:", "Slot D:"};

    for (i = 0; i < MAX_SLOTS; i++) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{boxX + margin, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)slotLabel[i]);

        for (col = 0; col < kPSSlotToggleCount; col++) {
            kPSSlotToggles[col].rects[i] = draw_button(mainArea,
                                                       (tRectangle){{colX[col], y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                       kPSSlotToggles[col].get_str(i),
                                                       kPSSlotToggles[col].get_colour(i));
        }

        midi_note_name_str(gPerfSettings.slot[i].rangeLower, note, sizeof(note));
        render_dropdown(colLo, y, btnH, note, "C#-1", &gPerfSettingsPanelRects.rangeLower[i]);

        midi_note_name_str(gPerfSettings.slot[i].rangeUpper, note, sizeof(note));
        render_dropdown(colHi, y, btnH, note, "C#-1", &gPerfSettingsPanelRects.rangeUpper[i]);

        midi_note_name_str(gPerfSettings.slot[i].rangeLower, loNote, sizeof(loNote));
        midi_note_name_str(gPerfSettings.slot[i].rangeUpper, hiNote, sizeof(hiNote));
        snprintf(rangeBuf, sizeof(rangeBuf), "%s - %s", loNote, hiNote);
        render_text(mainArea, (tRectangle){{colRng, y + 2.0}, {BLANK_SIZE, btnH}}, rangeBuf);

        y += rowH;
    }

    (void)buf;
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

static void render_patch_notes_edit(void) {
    if (!gPatchNotesEdit.active) {
        return;
    }
    double     boxW         = 700.0;
    double     boxH         = 500.0;

    // FLOATING, so the position comes from the panel rather than from the window — chosen once on
    // first show, and thereafter wherever the user has dragged it.
    tRectangle panelBox     = floating_panel_place(&gPatchNotesEdit.panel, boxW, boxH);
    double     boxX         = panelBox.coord.x;
    double     boxY         = panelBox.coord.y;
    double     margin       = 10.0;
    double     titleH       = 24.0;
    double     lineH        = STANDARD_TEXT_HEIGHT + 3.0;
    double     hintH        = STANDARD_TEXT_HEIGHT + 12.0; // extra headroom below the button/text baseline so descenders (g, y, p) aren't clipped by the border
    double     textY0       = boxY + titleH + margin;
    double     textX        = boxX + margin;
    double     textW        = boxW - margin * 2.0;
    double     maxTextH     = boxH - titleH - hintH - margin * 3.0;
    char       countBuf[32] = {0};

    double     btnH         = STANDARD_BUTTON_TEXT_HEIGHT;

    // No draw_dialog_background_overlay(): dimming the canvas is what a MODAL dialog does, and the
    // notes editor is not one — it floats over a live canvas like the settings panels.
    gPatchNotesPanelRect               = panelBox;
    gPatchNotesEdit.panel.titleBarRect = draw_panel_chrome(mainArea, panelBox, titleH, "Patch Notes");
    gPatchNotesCloseRect               = draw_panel_close_button(mainArea, panelBox, gPatchNotesClosePressed);
    gPatchNotesEdit.panel.closeRect    = gPatchNotesCloseRect;

    // Character count
    snprintf(countBuf, sizeof(countBuf), "%zu / %d", strlen(gPatchNotesEdit.buffer), PATCH_NOTES_SIZE);
    set_rgb_colour((tRgb)RGB_GREY_9);
    render_text(mainArea, (tRectangle){{boxX + boxW / 2.0 - 30.0, boxY + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, countBuf);

    // Cache geometry for click-to-cursor and keyboard navigation
    gNoteTextX                         = textX;
    gNoteTextY0                        = textY0;
    gNoteLineH                         = lineH;
    gNoteTextW                         = textW;
    gNoteTextHParam                    = STANDARD_TEXT_HEIGHT;

    build_note_visual_lines(gPatchNotesEdit.buffer, textW, STANDARD_TEXT_HEIGHT);

    int cursorPos  = (int)gPatchNotesEdit.cursorPos;
    int cursorLine = find_note_cursor_line(cursorPos);
    int visLines   = (int)(maxTextH / lineH);

    // Keep scroll so the cursor line is always visible
    if (cursorLine < gNoteScrollLine) {
        gNoteScrollLine = cursorLine;
    }

    if (cursorLine >= gNoteScrollLine + visLines) {
        gNoteScrollLine = cursorLine - visLines + 1;
    }

    if (gNoteScrollLine < 0) {
        gNoteScrollLine = 0;
    }
    // Text content area background
    set_rgb_colour((tRgb)RGB_WHITE);
    render_rectangle(mainArea, (tRectangle){{boxX + 1, boxY + titleH}, {boxW - 2, boxH - titleH - hintH - 1}});

    {
        const char * buf = gPatchNotesEdit.buffer;
        double       y   = textY0;

        for (int i = gNoteScrollLine; i < gNoteLineCount && i < gNoteScrollLine + visLines; i++) {
            int  start                             = gNoteLines[i].bufStart;
            int  end                               = gNoteLines[i].bufEnd;
            int  len                               = end - start;

            char displayLine[PATCH_NOTES_SIZE + 4] = {0};

            if (i == cursorLine) {
                int col = cursorPos - start;

                if (col < 0) {
                    col = 0;
                }

                if (col > len) {
                    col = len;
                }
                strncpy(displayLine, buf + start, col);
                displayLine[col]     = '|';
                strncpy(displayLine + col + 1, buf + start + col, len - col);
                displayLine[len + 1] = '\0';
            } else {
                strncpy(displayLine, buf + start, len);
                displayLine[len] = '\0';
            }
            set_rgb_colour((tRgb)RGB_BLACK);
            render_text(mainArea, (tRectangle){{textX, y}, {textW, STANDARD_TEXT_HEIGHT}}, displayLine);
            y += lineH;
        }
    }

    // Bottom bar: Discard Edits button + hint text. Inset so it doesn't paint over the panel's
    // bottom/left/right border line, same as the title bar above.
    set_rgb_colour((tRgb)RGB_GREY_3);
    render_rectangle(mainArea, (tRectangle){{boxX + BORDER_LINE_WIDTH, boxY + boxH - hintH}, {boxW - 2.0 * BORDER_LINE_WIDTH, hintH - BORDER_LINE_WIDTH}});

    double btnY       = boxY + boxH - hintH + (hintH - btnH) / 2.0 - 2.0; // draw_button's internal padding sits its text a couple px lower than render_text at the same y - nudge up so "Discard Edits" lines up with the hint text baseline
    double btnX       = boxX + margin;
    tRgb   discardCol = gPatchNotesDiscardPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
    gPatchNotesDiscardRect = draw_button(mainArea,
                                         (tRectangle){{btnX, btnY}, {get_text_width((char *)"Discard Edits", btnH, eCache) + 4.0, btnH}},
                                         (char *)"Discard Edits", discardCol);
    btnX                  += gPatchNotesDiscardRect.size.w + 12.0;
    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{btnX, boxY + boxH - hintH + (hintH - STANDARD_TEXT_HEIGHT) / 2.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
                "Arrows/Click=move   Enter=newline   Esc=close without saving");
}

int note_editor_cursor_move_line(int cursorPos, int delta) {
    if (gNoteLineCount == 0) {
        return cursorPos;
    }
    int curLine = find_note_cursor_line(cursorPos);
    int col     = cursorPos - gNoteLines[curLine].bufStart;
    int newLine = curLine + delta;

    if (newLine < 0) {
        newLine = 0;
    }

    if (newLine >= gNoteLineCount) {
        newLine = gNoteLineCount - 1;
    }
    int newLen  = gNoteLines[newLine].bufEnd - gNoteLines[newLine].bufStart;
    return gNoteLines[newLine].bufStart + (col < newLen ? col : newLen);
}

int note_editor_cursor_line_home(int cursorPos) {
    if (gNoteLineCount == 0) {
        return 0;
    }
    return gNoteLines[find_note_cursor_line(cursorPos)].bufStart;
}

int note_editor_cursor_line_end(int cursorPos) {
    if (gNoteLineCount == 0) {
        return 0;
    }
    return gNoteLines[find_note_cursor_line(cursorPos)].bufEnd;
}

int note_editor_cursor_from_click(double logicalX, double logicalY) {
    if (gNoteLineCount == 0) {
        return -1;
    }
    double       relY    = logicalY - gNoteTextY0;

    if (relY < 0) {
        return -1;
    }
    int          lineIdx = gNoteScrollLine + (int)(relY / gNoteLineH);

    if (lineIdx >= gNoteLineCount) {
        lineIdx = gNoteLineCount - 1;
    }

    if (lineIdx < 0) {
        return -1;
    }
    int          start   = gNoteLines[lineIdx].bufStart;
    int          end     = gNoteLines[lineIdx].bufEnd;
    const char * buf     = gPatchNotesEdit.buffer;
    double       relX    = logicalX - gNoteTextX;
    char         tmp[PATCH_NOTES_SIZE + 1];

    for (int col = 0; col <= end - start; col++) {
        strncpy(tmp, buf + start, col);
        tmp[col] = '\0';

        if (get_text_width(tmp, gNoteTextHParam, eNoCache) > relX) {
            if (col > 0) {
                strncpy(tmp, buf + start, col - 1);
                tmp[col - 1] = '\0';
                double wPrev = get_text_width(tmp, gNoteTextHParam, eNoCache);
                strncpy(tmp, buf + start, col);
                tmp[col]     = '\0';
                double wCur  = get_text_width(tmp, gNoteTextHParam, eNoCache);
                return start + ((relX - wPrev < wCur - relX) ? col - 1 : col);
            }
            return start;
        }
    }

    return start + (end - start);
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

// notes §23
static tFloatingPanelEntry gFloatingPanels[] = {
    {&gVirtualKeyboard.panel,   render_virtual_keyboard_panel, handle_virtual_keyboard_mouse, handle_virtual_keyboard_key, &gVirtualKeyboard.active  },
    {&gPatchAdjuster.panel,     render_patch_adjuster_panel,   handle_patch_adjuster_mouse,   handle_patch_adjuster_key,   &gPatchAdjuster.active    },
    {&gHelpPanel.panel,         render_help_panel,             handle_help_panel_mouse,       handle_help_panel_key,       &gHelpPanel.active        },
    {&gMutator.panel,           render_mutator_panel,          handle_mutator_mouse,          handle_mutator_key,          &gMutator.active          },
    {&gPatchSettingsEdit.panel, render_patch_settings_panel,   handle_patch_settings_mouse,   handle_patch_settings_key,   &gPatchSettingsEdit.active},
    {&gPerfSettingsEdit.panel,  render_perf_settings_panel,    handle_perf_settings_mouse,    handle_perf_settings_key,    &gPerfSettingsEdit.active },
    {&gPatchParamsEdit.panel,   render_patch_params_panel,     handle_patch_params_mouse,     handle_patch_params_key,     &gPatchParamsEdit.active  },

    // notes §24
    {&gPatchNotesEdit.panel,    render_patch_notes_edit,       handle_patch_notes_mouse,      NULL,                        &gPatchNotesEdit.active   },
    {&gParamPages.panel,        render_param_pages_panel,      handle_param_pages_mouse,      handle_param_pages_key,      &gParamPages.active       },
    {&gParamOverview.panel,     render_param_overview_panel,   handle_param_overview_mouse,   handle_param_overview_key,   &gParamOverview.active    },
    {&gMidiCcList.panel,        render_midi_cc_list_panel,     handle_midi_cc_list_mouse,     handle_midi_cc_list_key,     &gMidiCcList.active       }
};

#define FLOATING_PANEL_COUNT    ((uint32_t)(sizeof(gFloatingPanels) / sizeof(gFloatingPanels[0])))

// WHICH panel, front to back — the same walk the clicks take, so the panel this names is the panel
// that would receive a press.
static const tFloatingPanel * floating_panel_at(tCoord coord) {
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        if (  floating_panel_entry_visible(&gFloatingPanels[i - 1])
           && floating_panel_contains(gFloatingPanels[i - 1].panel, coord)) {
            return gFloatingPanels[i - 1].panel;
        }
    }

    return NULL;
}

bool floating_panels_under(tCoord coord) {
    return floating_panel_at(coord) != NULL;
}

// notes §25
static void panel_press_takes_the_keyboard(tCoord coord) {
    const tFloatingPanel * hit = floating_panel_at(coord);

    if (hit == NULL) {
        return;
    }
    stop_patch_name_editing();
    stop_module_name_editing();
    stop_param_name_editing();
    stop_perf_name_editing();

    if (hit != &gPatchSettingsEdit.panel) {
        stop_synth_name_editing();
    }
}

// notes §26
static void raise_newly_shown_panels(void) {
    static const tFloatingPanel * wasVisible[FLOATING_PANEL_COUNT] = {NULL};
    static uint32_t               wasVisibleCount                  = 0;
    const tFloatingPanel *        nowVisible[FLOATING_PANEL_COUNT] = {NULL};
    uint32_t                      nowVisibleCount                  = 0;

    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (!floating_panel_entry_visible(&gFloatingPanels[i])) {
            continue;
        }
        const tFloatingPanel * panel = gFloatingPanels[i].panel;
        bool                   seen  = false;

        for (uint32_t j = 0; j < wasVisibleCount; j++) {
            if (wasVisible[j] == panel) {
                seen = true;
                break;
            }
        }

        if (!seen) {
            floating_panel_raise(gFloatingPanels[i].panel);
        }
        nowVisible[nowVisibleCount++] = panel;
    }

    for (uint32_t i = 0; i < nowVisibleCount; i++) {
        wasVisible[i] = nowVisible[i];
    }

    wasVisibleCount = nowVisibleCount;
}

static void floating_panels_render(void) {
    raise_newly_shown_panels();

    // notes §27
    floating_panel_set_bounds((tRectangle){{
                                               0.0, 0.0
                                           }, {
                                               (get_render_width() / gGlobalGuiScale) - SCROLLBAR_WIDTH,
                                               (get_render_height() / gGlobalGuiScale) - SCROLLBAR_WIDTH
                                           }
                              });

    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (gFloatingPanels[i].render != NULL) {
            gFloatingPanels[i].render();     // back to front, so the most recently clicked ends up on top
        }
    }
}

// Reversed against the draw walk: sorted back-to-front for drawing, so front-to-back is the
// hit-test order. Fixed call order was wrong the moment two of them could overlap — whichever was
// tested first swallowed the press, even when it was the one underneath.
static bool floating_panels_mouse(tCoord coord, tMouseButton mouseButton) {
    if (mouseButton == mouseButtonLeftDown) {
        panel_press_takes_the_keyboard(coord);
    }
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        if (  (gFloatingPanels[i - 1].mouse != NULL)
           && gFloatingPanels[i - 1].mouse(coord, mouseButton)) {
            return true;
        }
    }

    return false;
}

// Keys are ordered for the same reason clicks are: Escape has to close the panel you are LOOKING at.
// Fixed call order closed whichever handler came first — with the Help panel in front and the
// Virtual Keyboard behind it, Escape shut the keyboard.
static bool floating_panels_key(int key, int mods, int action) {
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        // notes §28
        if (  (gFloatingPanels[i - 1].key != NULL)
           && gFloatingPanels[i - 1].key(key, mods, action)) {
            return true;
        }
    }

    return false;
}

// notes §29
bool floating_panel_is_frontmost(const tFloatingPanel * panel) {
    const tFloatingPanel * front = NULL;

    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (!floating_panel_entry_visible(&gFloatingPanels[i])) {
            continue;   // a closed panel keeps its order, so it must not win this
        }

        // notes §30
        if ((front == NULL) || !floating_panel_in_front_of(front, gFloatingPanels[i].panel)) {
            front = gFloatingPanels[i].panel;
        }
    }

    return (front != NULL) && (front == panel);
}

// notes §31

// notes §32
bool floating_panels_drag(tCoord coord) {
    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (floating_panel_drag(gFloatingPanels[i].panel, coord)) {
            return true;
        }
    }

    return false;
}

// notes §33
static bool floating_panels_scroll(double yDelta) {
    tCoord coord = {0};

    (void)yDelta;
    get_global_gui_scaled_mouse_coord(&coord);
    return floating_panels_under(coord);
}

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
