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
// Notes: Docs/code-notes/menuActions.c.md - "// notes §k" refers there.

// File/Settings/Backup/Restore menu action bodies, split out of misc.mm so the only code left in
// that Objective-C++ file is what genuinely needs Cocoa (the native menu-bar bootstrap and
// sleep/wake notifications) — none of this needs Objective-C, so it lives in plain C instead.

#include <string.h>

#include "misc.h"
#include "defs.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "graphics.h"
#include "mouseHandle.h"
#include "alertDialog.h"
#include "fileBrowser.h"
#include "bankBrowser.h"
#include "msgQueue.h"
#include "paramPages.h"
#include "paramOverview.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"

// notes §1
static uint32_t sPendingBackupBank        = 0;
static bool     sPendingBackupIsPerf      = false;

// notes §2
static uint32_t sPendingRestoreBank       = 0;
static bool     sPendingRestoreIsPerf     = false;
static uint32_t sPendingRestoreTargetBank = 0;

static void on_bank_backup_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd                   = eMsgCmdBackupBank;
    msg.bankBackupData.bank   = sPendingBackupBank;
    msg.bankBackupData.isPerf = sPendingBackupIsPerf;
    strncpy(msg.bankBackupData.destFolder, path, sizeof(msg.bankBackupData.destFolder) - 1);
    msg_send(&gToUsbThread, &msg);
}

// notes §3
static void on_backup_bank_picked(bool confirmed, uint32_t bank1Indexed) {
    char title[64] = {0};

    if (!confirmed) {
        return;
    }
    sPendingBackupBank = bank1Indexed - 1;
    snprintf(title, sizeof(title), "Choose a Folder for %s Bank %u Backup",
             sPendingBackupIsPerf ? "Performance" : "Patch", bank1Indexed);
    open_file_browser_folder(on_bank_backup_folder_chosen, title);
}

static void on_synth_settings_backup_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdBackupSynthSettings;
    strncpy(msg.settingsBackupData.destFolder, path, sizeof(msg.settingsBackupData.destFolder) - 1);
    msg_send(&gToUsbThread, &msg);
}

// Kicks off the find+parse of the latest backup file in the chosen folder — the actual confirm
// dialog and eMsgCmdApplySynthSettingsRestore send happen later in graphics.cpp's
// check_action_flags(), once the (fast, local-disk-only) peek result lands in gSynthRestorePeek*.
static void on_synth_settings_restore_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdPeekSynthSettingsRestore;
    strncpy(msg.synthSettingsRestoreData.srcFolder, path, sizeof(msg.synthSettingsRestoreData.srcFolder) - 1);
    msg_send(&gToUsbThread, &msg);
}

static void on_everything_backup_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdBackupEverything;
    strncpy(msg.settingsBackupData.destFolder, path, sizeof(msg.settingsBackupData.destFolder) - 1);
    msg_send(&gToUsbThread, &msg);
}

static void on_everything_restore_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdRestoreEverything;
    strncpy(msg.synthSettingsRestoreData.srcFolder, path, sizeof(msg.synthSettingsRestoreData.srcFolder) - 1);
    msg_send(&gToUsbThread, &msg);
}

static void on_restore_everything_confirmed(bool confirmed) {
    if (!confirmed) {
        return;
    }
    open_file_browser_folder(on_everything_restore_folder_chosen, "Choose the Backup Folder to Restore Everything From");
}

static void on_bank_restore_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd                        = eMsgCmdRestoreBank;
    msg.bankRestoreData.sourceBank = sPendingRestoreBank;
    msg.bankRestoreData.destBank   = sPendingRestoreTargetBank;
    msg.bankRestoreData.isPerf     = sPendingRestoreIsPerf;
    strncpy(msg.bankRestoreData.srcFolder, path, sizeof(msg.bankRestoreData.srcFolder) - 1);
    msg_send(&gToUsbThread, &msg);
}

static void on_bank_restore_confirmed(bool confirmed, uint32_t targetBank1Indexed) {
    char title[80] = {0};

    if (!confirmed) {
        return;
    }
    sPendingRestoreTargetBank = targetBank1Indexed - 1;
    snprintf(title, sizeof(title), "Choose the Backup Folder to Restore %s Bank %u From",
             sPendingRestoreIsPerf ? "Performance" : "Patch", sPendingRestoreBank + 1);
    open_file_browser_folder(on_bank_restore_folder_chosen, title);
}

// notes §4
static void on_restore_source_bank_picked(bool confirmed, uint32_t bank1Indexed) {
    char message[320] = {0};

    if (!confirmed) {
        return;
    }
    sPendingRestoreBank = bank1Indexed - 1;
    snprintf(message, sizeof(message),
             "This reads %s Bank %u's backup and writes it to the target bank chosen below on the G2. "
             "Any location not present in that backup will be erased there. This cannot be undone.",
             sPendingRestoreIsPerf ? "Performance" : "Patch", bank1Indexed);
    show_bank_confirm(sPendingRestoreIsPerf ? "Restore Performance Bank" : "Restore Patch Bank",
                      message, "Restore...", "Restore to Bank:", bank1Indexed,
                      sPendingRestoreIsPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS,
                      on_bank_restore_confirmed);
}

// notes §5
static bool sPendingStoreIsPerf  = false;

// notes §6
static void on_store_bank_location_chosen(bool confirmed, uint32_t bank1Indexed, uint32_t location1Indexed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdPeekBankLocation;
    msg.bankLocationPerfData.bank     = bank1Indexed - 1;
    msg.bankLocationPerfData.location = location1Indexed - 1;
    msg.bankLocationPerfData.isPerf   = sPendingStoreIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// notes §7
static bool sPendingDeleteIsPerf = false;

// Same "kick off the peek, let graphics.cpp take it from there" shape as
// on_store_bank_location_chosen above.
static void on_delete_bank_location_chosen(bool confirmed, uint32_t bank1Indexed, uint32_t location1Indexed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdPeekDeleteTarget;
    msg.bankLocationPerfData.bank     = bank1Indexed - 1;
    msg.bankLocationPerfData.location = location1Indexed - 1;
    msg.bankLocationPerfData.isPerf   = sPendingDeleteIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// Domain for the pending Load flow, set by file_menu_load_patch_location()/
// file_menu_load_perf_location() right before opening the bank/location dialog — same stash
// pattern as sPendingDeleteIsPerf above.
static bool sPendingLoadIsPerf   = false;

// Same "kick off the peek, let graphics.cpp take it from there" shape as
// on_store_bank_location_chosen/on_delete_bank_location_chosen above.
static void on_load_bank_location_chosen(bool confirmed, uint32_t bank1Indexed, uint32_t location1Indexed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdPeekLoadTarget;
    msg.bankLocationPerfData.bank     = bank1Indexed - 1;
    msg.bankLocationPerfData.location = location1Indexed - 1;
    msg.bankLocationPerfData.isPerf   = sPendingLoadIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// notes §8
static void build_bank_browser_items(bool isPerf, bool populatedOnly,
                                     tBankBrowserItem ** outItems, char(**outNames)[CLAVIA_NAME_SIZE + 1], uint32_t * outCount) {
    uint32_t           numBanks = isPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS;
    uint32_t           count    = 0;
    uint32_t           i        = 0;
    tBankBrowserItem * items    = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;

    for (uint32_t bank = 0; bank < numBanks; bank++) {
        for (uint32_t location = 0; location < NUM_LOCATIONS_PER_BANK; location++) {
            bool populated = isPerf ? gPerfNameTable[bank][location].populated : gPatchNameTable[bank][location].populated;

            if (!populatedOnly || populated) {
                count++;
            }
        }
    }

    items                              = (tBankBrowserItem *)malloc(count * sizeof(tBankBrowserItem));
    names                              = (char(*)[CLAVIA_NAME_SIZE + 1])malloc(count * sizeof(*names));

    for (uint32_t bank = 0; (bank < numBanks) && (i < count); bank++) {
        for (uint32_t location = 0; (location < NUM_LOCATIONS_PER_BANK) && (i < count); location++) {
            bool populated = isPerf ? gPerfNameTable[bank][location].populated : gPatchNameTable[bank][location].populated;

            if (!populatedOnly || populated) {
                // notes §9
                if (populated) {
                    COPY_STRING(names[i], isPerf ? gPerfNameTable[bank][location].name : gPatchNameTable[bank][location].name);
                } else {
                    COPY_STRING(names[i], "(empty)");
                }
                items[i].name             = names[i];
                items[i].category         = populated ? (isPerf ? gPerfNameTable[bank][location].category : gPatchNameTable[bank][location].category) : 0;
                items[i].bank1Indexed     = bank + 1;
                items[i].location1Indexed = location + 1;
                i++;
            }
        }
    }

    *outItems = items;
    *outNames = names;
    *outCount = count;
}

// Defer the actual browser open to the render loop (it must not open mid-menu-callback) by posting
// onto the GUI-thread work queue — the drain in check_action_flags() opens it. wake_glfw() nudges the
// loop to drain promptly.
void file_menu_open_patch(void) {
    tMessageContent msg = {0};

    msg.cmd = eRspShowOpenRead;
    msg_send(&gToGuiThread, &msg);
    wake_glfw();
}

// File > Open Recent — the path is already known, so no browser, but the same deferral: opening a
// patch from inside a menu callback would run the load while the menu that raised it is still on
// screen.
void file_menu_open_path(const char * path) {
    tMessageContent msg = {0};

    if ((path == NULL) || (path[0] == '\0')) {
        return;
    }
    msg.cmd = eRspOpenPath;
    COPY_STRING(msg.patchFileData.filePath, path);
    msg_send(&gToGuiThread, &msg);
    wake_glfw();
}

void file_menu_save_patch(void) {
    tMessageContent msg = {0};

    msg.cmd = eRspShowOpenWrite;
    msg_send(&gToGuiThread, &msg);
    wake_glfw();
}

// File > Save — overwrite the file this patch/perf was last opened from or saved to. Offered by
// the menu only when file_menu_have_saved_path() says there is one.
void file_menu_save_patch_to_current_path(void) {
    tMessageContent msg = {0};

    msg.cmd = eRspSaveToCurrentPath;
    msg_send(&gToGuiThread, &msg);
    wake_glfw();
}

bool file_menu_have_saved_path(void) {
    return (gGlobalSettings.perfMode == 1) ? (gSavedPerfPath[0] != '\0') : (gSavedPatchPath[gSlot][0] != '\0');
}

void file_menu_new_patch(void) {
    // notes §10
    tMessageContent messageContent = {0};

    // notes §11
    if (!device_ready()) {
        init_patch(gSlot);
        notify_full_patch_change();
        synthlib_request_redraw();
        wake_glfw();
        return;
    }
    messageContent.cmd                = eMsgCmdNewPatch;
    messageContent.patchFileData.slot = gSlot;
    msg_send(&gToUsbThread, &messageContent);
    device_op_begin("New Patch...");

    wake_glfw();
}

void file_menu_store_to_bank(void) {
    bool               isPerf       = gGlobalSettings.perfMode == 1;
    const char *       typeName     = isPerf ? "performance" : "patch";
    char               message[320] = {0};
    tBankBrowserItem * items        = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t           count        = 0;

    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before storing to a bank.");
        return;
    }
    sPendingStoreIsPerf                = isPerf;
    snprintf(message, sizeof(message),
             "Choose the bank and location to store the current edit buffer %s to. "
             "You'll be shown what's currently there before anything is written.", typeName);
    build_bank_browser_items(isPerf, false, &items, &names, &count);
    open_bank_browser(isPerf ? "Store Performance to Bank" : "Store Patch to Bank", message, "Next...",
                      items, count, patchTypeStrMap, patchTypeUserMax, on_store_bank_location_chosen);
    free(items);
    free(names);
}

void file_menu_delete_patch_location(void) {
    tBankBrowserItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t           count = 0;

    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before deleting a patch.");
        return;
    }
    sPendingDeleteIsPerf               = false;
    build_bank_browser_items(false, true, &items, &names, &count);
    open_bank_browser("Delete Patch",
                      "Choose the patch to delete. "
                      "You'll be shown its name again before anything is erased.",
                      "Next...", items, count, patchTypeStrMap, patchTypeUserMax, on_delete_bank_location_chosen);
    free(items);
    free(names);
}

void file_menu_delete_perf_location(void) {
    tBankBrowserItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t           count = 0;

    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before deleting a performance.");
        return;
    }
    sPendingDeleteIsPerf               = true;
    build_bank_browser_items(true, true, &items, &names, &count);
    open_bank_browser("Delete Performance",
                      "Choose the performance to delete. "
                      "You'll be shown its name again before anything is erased.",
                      "Next...", items, count, patchTypeStrMap, patchTypeUserMax, on_delete_bank_location_chosen);
    free(items);
    free(names);
}

void file_menu_load_patch_location(void) {
    tBankBrowserItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t           count = 0;

    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before loading a patch.");
        return;
    }
    sPendingLoadIsPerf                 = false;
    build_bank_browser_items(false, true, &items, &names, &count);
    open_bank_browser("Load Patch",
                      "Choose the patch to load into the current edit buffer. "
                      "You'll be shown its name again before anything is replaced.",
                      "Next...", items, count, patchTypeStrMap, patchTypeUserMax, on_load_bank_location_chosen);
    free(items);
    free(names);
}

void file_menu_load_perf_location(void) {
    tBankBrowserItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t           count = 0;

    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before loading a performance.");
        return;
    }
    sPendingLoadIsPerf                 = true;
    build_bank_browser_items(true, true, &items, &names, &count);
    open_bank_browser("Load Performance",
                      "Choose the performance to load into the current edit buffer. "
                      "You'll be shown its name again before anything is replaced.",
                      "Next...", items, count, patchTypeStrMap, patchTypeUserMax, on_load_bank_location_chosen);
    free(items);
    free(names);
}

void settings_menu_open_synth(void) {
    uint32_t slot = gSlot;

    gPatchSettingsEdit.active = true;
    gPatchSettingsEdit.slot   = slot;
    wake_glfw();
}

void settings_menu_open_patch(void) {
    uint32_t slot = gSlot;

    gPatchParamsEdit.active = true;
    gPatchParamsEdit.slot   = slot;
    wake_glfw();
}

void settings_menu_open_param_pages(void) {
    open_param_pages_panel(gSlot);
    wake_glfw();
}

void settings_menu_open_param_overview(void) {
    open_param_overview_panel(gSlot);
    wake_glfw();
}

void settings_menu_open_virtual_keyboard(void) {
    open_virtual_keyboard_panel();
    wake_glfw();
}

void settings_menu_open_patch_adjuster(void) {
    open_patch_adjuster_panel(gSlot);
    wake_glfw();
}

void settings_menu_open_perf(void) {
    gPerfSettingsEdit.active = true;
    wake_glfw();
}

void settings_menu_open_notes(void) {
    uint32_t slot = gSlot;

    gPatchNotesEdit.active    = true;
    gPatchNotesEdit.slot      = slot;
    gPatchNotesEdit.cursorPos = gPatchNotesSize[slot];
    memset(gPatchNotesEdit.buffer, 0, sizeof(gPatchNotesEdit.buffer));
    memcpy(gPatchNotesEdit.buffer, gPatchNotes[slot], gPatchNotesSize[slot]);
    memset(gPatchNotesEdit.original, 0, sizeof(gPatchNotesEdit.original));
    memcpy(gPatchNotesEdit.original, gPatchNotes[slot], gPatchNotesSize[slot]);
    wake_glfw();
}

void backup_menu_patch_bank(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up a bank.");
        return;
    }
    sPendingBackupIsPerf = false;
    show_bank_confirm("Backup Patch Bank", "Choose which patch bank to back up.", "Backup...",
                      "Bank to Back Up:", sPendingBackupBank + 1, NUM_PATCH_BANKS,
                      on_backup_bank_picked);
}

void backup_menu_perf_bank(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up a performance bank.");
        return;
    }
    sPendingBackupIsPerf = true;
    show_bank_confirm("Backup Performance Bank", "Choose which performance bank to back up.", "Backup...",
                      "Bank to Back Up:", sPendingBackupBank + 1, NUM_PERF_BANKS,
                      on_backup_bank_picked);
}

void backup_menu_synth_settings(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up synth settings.");
        return;
    }
    open_file_browser_folder(on_synth_settings_backup_folder_chosen, "Choose a Folder for Synth Settings Backup");
}

void backup_menu_everything(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up everything.");
        return;
    }
    open_file_browser_folder(on_everything_backup_folder_chosen, "Choose a Folder for Backup Everything");
}

void restore_menu_patch_bank(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring a bank.");
        return;
    }
    sPendingRestoreIsPerf = false;
    show_bank_confirm("Restore Patch Bank", "Choose which patch bank's backup to restore.", "Next...",
                      "Restore from Bank:", sPendingRestoreBank + 1, NUM_PATCH_BANKS,
                      on_restore_source_bank_picked);
}

void restore_menu_perf_bank(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring a performance bank.");
        return;
    }
    sPendingRestoreIsPerf = true;
    show_bank_confirm("Restore Performance Bank", "Choose which performance bank's backup to restore.", "Next...",
                      "Restore from Bank:", sPendingRestoreBank + 1, NUM_PERF_BANKS,
                      on_restore_source_bank_picked);
}

void restore_menu_synth_settings(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring synth settings.");
        return;
    }
    open_file_browser_folder(on_synth_settings_restore_folder_chosen, "Choose the Backup Folder to Restore Synth Settings From");
}

void restore_menu_everything(void) {
    if (!device_ready()) {
        show_alert("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring everything.");
        return;
    }
    show_confirm("Restore Everything",
                 "This restores every Patch Bank, Performance Bank, and Synth Settings backup found in a folder you choose next, "
                 "overwriting the G2's current contents to match. Any bank with no manifest file in that folder is left untouched "
                 "rather than erased. This cannot be undone.",
                 "Next...", on_restore_everything_confirmed);
}
