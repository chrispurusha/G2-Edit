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

#import "misc.h"
#import <Cocoa/Cocoa.h>
#include <stdatomic.h>
#include <string.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "globalVars.h"
#include "graphics.h"
#include "mouseHandle.h"
#include "usbComms.h"
#include "fileDialogue.h"
#include "fileBrowser.h"
#include "bankBrowser.h"

extern "C" {
void set_zoom_factor(double zoomFactor, tCoord mouseCoord);
double get_zoom_factor(void);
}

// Bank number (0-indexed) chosen from the "Backup Patch Bank"/"Backup Performance Bank" dropdown
// dialog, stashed here between that dialog's confirm callback and the folder-choose panel's
// completion callback (tFileDialogueCallback is a plain C function pointer with no room for
// captured context). Defaults the dropdown to whatever was picked last time (see backupBank:/
// backupPerfBank: below), rather than always resetting to Bank 1.
static uint32_t sPendingBackupBank        = 0;
static bool     sPendingBackupIsPerf      = false;

// Same stash-between-callbacks pattern as the backup statics above, but for Restore: a first
// dropdown dialog picks the source bank/domain (on_restore_source_bank_picked), a second supplies
// the (possibly different) target bank (on_bank_restore_confirmed), then the folder-choose panel
// supplies the source folder once the user has confirmed.
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
    msg_send(&gCommandQueue, &msg);
}

// Confirm callback for the "which bank to back up" dropdown dialog opened by backupBank:/
// backupPerfBank: below — sPendingBackupIsPerf was already set by whichever of those two called us,
// so this only needs to record the bank and move on to the folder picker.
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
    msg_send(&gCommandQueue, &msg);
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
    msg_send(&gCommandQueue, &msg);
}

static void on_everything_backup_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdBackupEverything;
    strncpy(msg.settingsBackupData.destFolder, path, sizeof(msg.settingsBackupData.destFolder) - 1);
    msg_send(&gCommandQueue, &msg);
}

static void on_everything_restore_folder_chosen(const char * path) {
    if (path == NULL) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd = eMsgCmdRestoreEverything;
    strncpy(msg.synthSettingsRestoreData.srcFolder, path, sizeof(msg.synthSettingsRestoreData.srcFolder) - 1);
    msg_send(&gCommandQueue, &msg);
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
    msg_send(&gCommandQueue, &msg);
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

// Confirm callback for the "which bank's backup to restore" dropdown dialog opened by
// restoreBank:/restorePerfBank: below — sPendingRestoreIsPerf was already set by whichever of those
// two called us. Chains straight into the existing target-bank dropdown dialog, defaulting it to
// the same bank just picked (the common "restore into itself" case), exactly mirroring what used to
// default from the clicked submenu item's tag.
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
    show_bank_target_confirm_dialogue_async(sPendingRestoreIsPerf ? "Restore Performance Bank" : "Restore Patch Bank",
                                            message, "Restore...", "Restore to Bank:", bank1Indexed,
                                            sPendingRestoreIsPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS,
                                            on_bank_restore_confirmed);
}

// Domain for the pending Store flow, set by storeToBank: right before opening the bank/location
// dialog (mirrors gGlobalSettings.perfMode — Store always acts on whatever's in the edit buffer) —
// same stash pattern as sPendingRestoreIsPerf above, needed because tBankBrowserCallback's
// signature has no room for it.
static bool sPendingStoreIsPerf = false;

// Kicks off the peek — the actual overwrite-warning confirm and eMsgCmdStorePatch send happen
// later in graphics.cpp's check_action_flags(), once the async peek result lands in gStorePeek*
// (there's no captured-context callback chain needed here, unlike Restore: the target bank/
// location the peek was for is recorded in gStorePeekBank/gStorePeekLocation, so nothing has to be
// stashed on the misc.mm side past this point).
static void on_store_bank_location_chosen(bool confirmed, uint32_t bank1Indexed, uint32_t location1Indexed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdPeekBankLocation;
    msg.bankLocationPerfData.bank     = bank1Indexed - 1;
    msg.bankLocationPerfData.location = location1Indexed - 1;
    msg.bankLocationPerfData.isPerf   = sPendingStoreIsPerf;
    msg_send(&gCommandQueue, &msg);
}

// Domain for the pending Delete flow, set by deletePatchLocation:/deletePerfLocation: right before
// opening the bank/location dialog — same stash pattern as sPendingRestoreIsPerf above, needed
// because tBankBrowserCallback's signature has no room for it.
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
    msg_send(&gCommandQueue, &msg);
}

// Domain for the pending Load flow, set by loadPatchLocation:/loadPerfLocation: right before
// opening the bank/location dialog — same stash pattern as sPendingDeleteIsPerf above.
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
    msg_send(&gCommandQueue, &msg);
}

// Builds the tBankBrowserItem array feeding open_bank_browser(), from the cached name tables (see
// project memory: List Names sweep) — no device round-trip needed just to show the list.
// populatedOnly restricts to locations that already contain something (Load/Delete: you can only
// act on what exists); when false, every location in the domain is listed, with unpopulated ones
// named "(empty)" (Store: the target may be a blank slot). Caller must free() both *outItems and
// *outNames once done — open_bank_browser() copies everything into its own storage synchronously
// before returning, so they only need to survive the call itself.
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
                if (populated) {
                    strncpy(names[i], isPerf ? gPerfNameTable[bank][location].name : gPatchNameTable[bank][location].name,
                            sizeof(names[i]) - 1);
                } else {
                    strncpy(names[i], "(empty)", sizeof(names[i]) - 1);
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

void file_menu_open_patch(void) {
    gShowOpenFileReadDialogue = true;
    wake_glfw();
}

void file_menu_save_patch(void) {
    gShowOpenFileWriteDialogue = true;
    wake_glfw();
}

void file_menu_new_patch(void) {
    init_patch(gSlot);
    wake_glfw();
}

void file_menu_store_to_bank(void) {
    bool               isPerf       = gGlobalSettings.perfMode == 1;
    const char *       typeName     = isPerf ? "performance" : "patch";
    char               message[320] = {0};
    tBankBrowserItem * items        = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t           count        = 0;

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before storing to a bank.");
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

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before deleting a patch.");
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

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before deleting a performance.");
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

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before loading a patch.");
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

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before loading a performance.");
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
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up a bank.");
        return;
    }
    sPendingBackupIsPerf = false;
    show_bank_target_confirm_dialogue_async("Backup Patch Bank", "Choose which patch bank to back up.", "Backup...",
                                            "Bank to Back Up:", sPendingBackupBank + 1, NUM_PATCH_BANKS,
                                            on_backup_bank_picked);
}

void backup_menu_perf_bank(void) {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up a performance bank.");
        return;
    }
    sPendingBackupIsPerf = true;
    show_bank_target_confirm_dialogue_async("Backup Performance Bank", "Choose which performance bank to back up.", "Backup...",
                                            "Bank to Back Up:", sPendingBackupBank + 1, NUM_PERF_BANKS,
                                            on_backup_bank_picked);
}

void backup_menu_synth_settings(void) {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up synth settings.");
        return;
    }
    open_file_browser_folder(on_synth_settings_backup_folder_chosen, "Choose a Folder for Synth Settings Backup");
}

void backup_menu_everything(void) {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up everything.");
        return;
    }
    open_file_browser_folder(on_everything_backup_folder_chosen, "Choose a Folder for Backup Everything");
}

void restore_menu_patch_bank(void) {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring a bank.");
        return;
    }
    sPendingRestoreIsPerf = false;
    show_bank_target_confirm_dialogue_async("Restore Patch Bank", "Choose which patch bank's backup to restore.", "Next...",
                                            "Restore from Bank:", sPendingRestoreBank + 1, NUM_PATCH_BANKS,
                                            on_restore_source_bank_picked);
}

void restore_menu_perf_bank(void) {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring a performance bank.");
        return;
    }
    sPendingRestoreIsPerf = true;
    show_bank_target_confirm_dialogue_async("Restore Performance Bank", "Choose which performance bank's backup to restore.", "Next...",
                                            "Restore from Bank:", sPendingRestoreBank + 1, NUM_PERF_BANKS,
                                            on_restore_source_bank_picked);
}

void restore_menu_synth_settings(void) {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring synth settings.");
        return;
    }
    open_file_browser_folder(on_synth_settings_restore_folder_chosen, "Choose the Backup Folder to Restore Synth Settings From");
}

void restore_menu_everything(void) {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring everything.");
        return;
    }
    show_confirm_dialogue_async("Restore Everything",
                                "This restores every Patch Bank, Performance Bank, and Synth Settings backup found in a folder you choose next, "
                                "overwriting the G2's current contents to match. Any bank with no manifest file in that folder is left untouched "
                                "rather than erased. This cannot be undone.",
                                "Next...", on_restore_everything_confirmed);
}

void save_dial_mode(int mode) {
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];

    [defaults setInteger:mode forKey:@"dialMode"];
    [defaults synchronize];
}

// Sets up the minimal native Cocoa app menu (Quit/About/Hide/Services — GLFW's Cocoa backend
// already populates these at index 0) and restores window/zoom/dial-mode state saved from a
// previous run. File/Settings/Backup/Restore/Controls/View menus used to be constructed here
// too; they're now the in-window bar built in src/appMenuBar.c on top of SynthLib's menuBar
// engine, sharing the same action functions defined below.
void setup_main_menu(void) {
    NSMenu *         menuBar  = [[NSApplication sharedApplication] mainMenu];

    if (menuBar == nil) {
        menuBar = [[NSMenu alloc] init];
        [[NSApplication sharedApplication] setMainMenu:menuBar];
    }
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];

    if ([defaults objectForKey:@"dialMode"] != nil) {
        gDialMode = (tDialMode)[defaults integerForKey:@"dialMode"];
    }

    if ([defaults objectForKey:@"zoomFactor"] != nil) {
        double savedZoom = [defaults doubleForKey:@"zoomFactor"];

        if (savedZoom >= 0.24) {
            set_zoom_factor(savedZoom, (tCoord){0.0, 0.0});
        }
    }

    if ([defaults objectForKey:@"windowWidth"] != nil) {
        int savedW = (int)[defaults integerForKey:@"windowWidth"];
        int savedH = savedW * TARGET_FRAME_BUFF_HEIGHT / TARGET_FRAME_BUFF_WIDTH;

        if (savedW > 0) {
            resize_window(savedW, savedH);
        }
    }

    if ([defaults objectForKey:@"windowX"] != nil && [defaults objectForKey:@"windowY"] != nil) {
        int savedX = (int)[defaults integerForKey:@"windowX"];
        int savedY = (int)[defaults integerForKey:@"windowY"];

        reposition_window(savedX, savedY);
    }
    NSString * savedBrowserDir = [defaults stringForKey:@"fileBrowserLastDirectory"];

    if (savedBrowserDir != nil) {
        set_file_browser_start_directory([savedBrowserDir UTF8String]);
    }
    set_file_browser_directory_changed_callback(save_file_browser_directory);
}

void save_file_browser_directory(const char * path) {
    if (path == NULL) {
        return;
    }
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];

    [defaults setObject:[NSString stringWithUTF8String:path] forKey:@"fileBrowserLastDirectory"];
    [defaults synchronize];
}

void save_zoom_factor(double zoom) {
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];

    [defaults setDouble:zoom forKey:@"zoomFactor"];
    [defaults synchronize];
}

void save_window_size(int w) {
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];

    [defaults setInteger:w forKey:@"windowWidth"];
    [defaults synchronize];
}

void save_window_pos(int x, int y) {
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];

    [defaults setInteger:x forKey:@"windowX"];
    [defaults setInteger:y forKey:@"windowY"];
    [defaults synchronize];
}

void register_sleep_wake_notifications(void) {
    [[[NSWorkspace sharedWorkspace] notificationCenter]
     addObserverForName:NSWorkspaceDidWakeNotification
     object:nil
     queue:nil
     usingBlock:^(NSNotification * note) {
         usb_signal_reconnect();
     }];
}
