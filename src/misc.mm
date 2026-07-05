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

extern "C" {
void set_zoom_factor(double zoomFactor, tCoord mouseCoord);
double get_zoom_factor(void);
}

@interface G2MenuTarget : NSObject
- (void)openPatch:(id)sender;
- (void)savePatch:(id)sender;
- (void)newPatch:(id)sender;
- (void)openNotes:(id)sender;
- (void)openSettings:(id)sender;
- (void)openPatchParams:(id)sender;
- (void)openPerfSettings:(id)sender;
- (void)setDialModeRotary:(id)sender;
- (void)setDialModeVertical:(id)sender;
- (void)setDialModeHorizontal:(id)sender;
- (void)backupBank:(id)sender;
- (void)backupPerfBank:(id)sender;
- (void)backupSynthSettings:(id)sender;
- (void)restoreSynthSettings:(id)sender;
- (void)backupEverything:(id)sender;
- (void)restoreBank:(id)sender;
- (void)restorePerfBank:(id)sender;
- (void)restoreEverything:(id)sender;
- (void)storeToBank:(id)sender;
- (void)deletePatchLocation:(id)sender;
- (void)deletePerfLocation:(id)sender;
- (void)loadPatchLocation:(id)sender;
- (void)loadPerfLocation:(id)sender;
- (void)zoomIn:(id)sender;
- (void)zoomOut:(id)sender;
- (void)zoomReset:(id)sender;
- (BOOL)validateMenuItem:(NSMenuItem *)item;
@end

// Bank number (0-indexed) selected from the "Backup Bank"/"Backup Performance Bank" submenu,
// stashed here between the menu click and the folder-choose panel's completion callback
// (tFileDialogueCallback is a plain C function pointer with no room for captured context).
static uint32_t sPendingBackupBank        = 0;
static bool     sPendingBackupIsPerf      = false;

// Same stash-between-callbacks pattern as the backup statics above, but for Restore: the source
// bank/domain is fixed by which menu item was clicked; the confirmation alert's accessory field
// then supplies the (possibly different) target bank; the folder-choose panel supplies the source
// folder once the user has confirmed.
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
    open_folder_dialogue_async(on_everything_restore_folder_chosen, "Choose the Backup Folder to Restore Everything From");
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
    open_folder_dialogue_async(on_bank_restore_folder_chosen, title);
}

// Domain for the pending Store flow, set by storeToBank: right before opening the bank/location
// dialog (mirrors gGlobalSettings.perfMode — Store always acts on whatever's in the edit buffer) —
// same stash pattern as sPendingRestoreIsPerf above, needed because tBankLocationConfirmCallback's
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
// because tBankLocationConfirmCallback's signature has no room for it.
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

// Builds the tBankLocationListItem array feeding show_bank_location_list_dialogue_async(), from
// the cached name tables (see project memory: List Names sweep) — no device round-trip needed
// just to show the list. populatedOnly restricts to locations that already contain something
// (Load/Delete: you can only act on what exists); when false, every location in the domain is
// listed, with unpopulated ones named "(empty)" (Store: the target may be a blank slot).
// Caller must free() both *outItems and *outNames once done — show_bank_location_list_dialogue_
// async() copies everything into Cocoa objects synchronously before returning, so they only need
// to survive the call itself.
static void build_bank_location_items(bool isPerf, bool populatedOnly,
                                      tBankLocationListItem ** outItems, char(**outNames)[CLAVIA_NAME_SIZE + 1], uint32_t * outCount) {
    uint32_t                numBanks = isPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS;
    uint32_t                count    = 0;
    uint32_t                i        = 0;
    tBankLocationListItem * items    = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;

    for (uint32_t bank = 0; bank < numBanks; bank++) {
        for (uint32_t location = 0; location < NUM_LOCATIONS_PER_BANK; location++) {
            bool populated = isPerf ? gPerfNameTable[bank][location].populated : gPatchNameTable[bank][location].populated;

            if (!populatedOnly || populated) {
                count++;
            }
        }
    }

    items                              = (tBankLocationListItem *)malloc(count * sizeof(tBankLocationListItem));
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

@implementation G2MenuTarget

- (void)openPatch:(id)sender {
    gShowOpenFileReadDialogue = true;
    wake_glfw();
}

- (void)savePatch:(id)sender {
    gShowOpenFileWriteDialogue = true;
    wake_glfw();
}

- (void)newPatch:(id)sender {
    init_patch(gSlot);
    wake_glfw();
}

- (void)openNotes:(id)sender {
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

- (void)openSettings:(id)sender {
    uint32_t slot = gSlot;

    gPatchSettingsEdit.active = true;
    gPatchSettingsEdit.slot   = slot;
    wake_glfw();
}

- (void)openPatchParams:(id)sender {
    uint32_t slot = gSlot;

    gPatchParamsEdit.active = true;
    gPatchParamsEdit.slot   = slot;
    wake_glfw();
}

- (void)openPerfSettings:(id)sender {
    gPerfSettingsEdit.active = true;
    wake_glfw();
}

- (void)setDialModeRotary:(id)sender {
    gDialMode = eDialModeRotary;
    [[NSUserDefaults standardUserDefaults] setInteger:gDialMode forKey:@"dialMode"];
}

- (void)setDialModeVertical:(id)sender {
    gDialMode = eDialModeVertical;
    [[NSUserDefaults standardUserDefaults] setInteger:gDialMode forKey:@"dialMode"];
}

- (void)setDialModeHorizontal:(id)sender {
    gDialMode = eDialModeHorizontal;
    [[NSUserDefaults standardUserDefaults] setInteger:gDialMode forKey:@"dialMode"];
}

- (void)backupBank:(id)sender {
    NSMenuItem * item      = (NSMenuItem *)sender;
    char         title[64] = {0};

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up a bank.");
        return;
    }
    sPendingBackupBank   = (uint32_t)[item tag];
    sPendingBackupIsPerf = false;
    snprintf(title, sizeof(title), "Choose a Folder for Bank %ld Backup", (long)[item tag] + 1);
    open_folder_dialogue_async(on_bank_backup_folder_chosen, title);
}

- (void)backupPerfBank:(id)sender {
    NSMenuItem * item      = (NSMenuItem *)sender;
    char         title[64] = {0};

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up a performance bank.");
        return;
    }
    sPendingBackupBank   = (uint32_t)[item tag];
    sPendingBackupIsPerf = true;
    snprintf(title, sizeof(title), "Choose a Folder for Performance Bank %ld Backup", (long)[item tag] + 1);
    open_folder_dialogue_async(on_bank_backup_folder_chosen, title);
}

- (void)backupSynthSettings:(id)sender {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up synth settings.");
        return;
    }
    open_folder_dialogue_async(on_synth_settings_backup_folder_chosen, "Choose a Folder for Synth Settings Backup");
}

- (void)restoreSynthSettings:(id)sender {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring synth settings.");
        return;
    }
    open_folder_dialogue_async(on_synth_settings_restore_folder_chosen, "Choose the Backup Folder to Restore Synth Settings From");
}

- (void)backupEverything:(id)sender {
    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before backing up everything.");
        return;
    }
    open_folder_dialogue_async(on_everything_backup_folder_chosen, "Choose a Folder for Backup Everything");
}

- (void)restoreBank:(id)sender {
    NSMenuItem * item         = (NSMenuItem *)sender;
    char         message[320] = {0};

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring a bank.");
        return;
    }
    sPendingRestoreBank   = (uint32_t)[item tag];
    sPendingRestoreIsPerf = false;
    snprintf(message, sizeof(message),
             "This reads Patch Bank %ld's backup and writes it to the target bank chosen below on the G2. "
             "Any location not present in that backup will be erased there. This cannot be undone.",
             (long)[item tag] + 1);
    show_bank_target_confirm_dialogue_async("Restore Patch Bank", message, "Restore...",
                                            (uint32_t)[item tag] + 1, NUM_PATCH_BANKS, on_bank_restore_confirmed);
}

- (void)restorePerfBank:(id)sender {
    NSMenuItem * item         = (NSMenuItem *)sender;
    char         message[320] = {0};

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before restoring a performance bank.");
        return;
    }
    sPendingRestoreBank   = (uint32_t)[item tag];
    sPendingRestoreIsPerf = true;
    snprintf(message, sizeof(message),
             "This reads Performance Bank %ld's backup and writes it to the target bank chosen below on the G2. "
             "Any location not present in that backup will be erased there. This cannot be undone.",
             (long)[item tag] + 1);
    show_bank_target_confirm_dialogue_async("Restore Performance Bank", message, "Restore...",
                                            (uint32_t)[item tag] + 1, NUM_PERF_BANKS, on_bank_restore_confirmed);
}

- (void)restoreEverything:(id)sender {
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

- (void)storeToBank:(id)sender {
    bool                    isPerf       = gGlobalSettings.perfMode == 1;
    const char *            typeName     = isPerf ? "performance" : "patch";
    char                    message[320] = {0};
    tBankLocationListItem * items        = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t                count        = 0;

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before storing to a bank.");
        return;
    }
    sPendingStoreIsPerf                = isPerf;
    snprintf(message, sizeof(message),
             "Choose the bank and location to store the current edit buffer %s to. "
             "You'll be shown what's currently there before anything is written.", typeName);
    build_bank_location_items(isPerf, false, &items, &names, &count);
    show_bank_location_list_dialogue_async(isPerf ? "Store Performance to Bank" : "Store Patch to Bank", message, "Next...",
                                           items, count, on_store_bank_location_chosen);
    free(items);
    free(names);
}

- (void)deletePatchLocation:(id)sender {
    tBankLocationListItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t                count = 0;

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before deleting a patch.");
        return;
    }
    sPendingDeleteIsPerf               = false;
    build_bank_location_items(false, true, &items, &names, &count);
    show_bank_location_list_dialogue_async("Delete Patch",
                                           "Choose the patch to delete. "
                                           "You'll be shown its name again before anything is erased.",
                                           "Next...", items, count, on_delete_bank_location_chosen);
    free(items);
    free(names);
}

- (void)deletePerfLocation:(id)sender {
    tBankLocationListItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t                count = 0;

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before deleting a performance.");
        return;
    }
    sPendingDeleteIsPerf               = true;
    build_bank_location_items(true, true, &items, &names, &count);
    show_bank_location_list_dialogue_async("Delete Performance",
                                           "Choose the performance to delete. "
                                           "You'll be shown its name again before anything is erased.",
                                           "Next...", items, count, on_delete_bank_location_chosen);
    free(items);
    free(names);
}

- (void)loadPatchLocation:(id)sender {
    tBankLocationListItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t                count = 0;

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before loading a patch.");
        return;
    }
    sPendingLoadIsPerf                 = false;
    build_bank_location_items(false, true, &items, &names, &count);
    show_bank_location_list_dialogue_async("Load Patch",
                                           "Choose the patch to load into the current edit buffer. "
                                           "You'll be shown its name again before anything is replaced.",
                                           "Next...", items, count, on_load_bank_location_chosen);
    free(items);
    free(names);
}

- (void)loadPerfLocation:(id)sender {
    tBankLocationListItem * items = NULL;

    char(*names)[CLAVIA_NAME_SIZE + 1] = NULL;
    uint32_t                count = 0;

    if (gCommsState != eCommsOnLine) {
        show_alert_async("G2 Not Connected", "Connect the G2 and wait for it to come online before loading a performance.");
        return;
    }
    sPendingLoadIsPerf                 = true;
    build_bank_location_items(true, true, &items, &names, &count);
    show_bank_location_list_dialogue_async("Load Performance",
                                           "Choose the performance to load into the current edit buffer. "
                                           "You'll be shown its name again before anything is replaced.",
                                           "Next...", items, count, on_load_bank_location_chosen);
    free(items);
    free(names);
}

- (void)zoomIn:(id)sender {
    double zoomFactor = get_zoom_factor() + ZOOM_DELTA;

    set_zoom_factor(zoomFactor, (tCoord){0.0, 0.0});
    save_zoom_factor(get_zoom_factor());
    wake_glfw();
}

- (void)zoomOut:(id)sender {
    double zoomFactor = get_zoom_factor() - ZOOM_DELTA;

    set_zoom_factor(zoomFactor, (tCoord){0.0, 0.0});
    save_zoom_factor(get_zoom_factor());
    wake_glfw();
}

- (void)zoomReset:(id)sender {
    set_zoom_factor(NO_ZOOM, (tCoord){0.0, 0.0});
    save_zoom_factor(get_zoom_factor());
    wake_glfw();
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL action = [item action];

    if (action == @selector(setDialModeRotary:)) {
        [item setState:(gDialMode == eDialModeRotary) ? NSControlStateValueOn : NSControlStateValueOff];
    } else if (action == @selector(setDialModeVertical:)) {
        [item setState:(gDialMode == eDialModeVertical) ? NSControlStateValueOn : NSControlStateValueOff];
    } else if (action == @selector(setDialModeHorizontal:)) {
        [item setState:(gDialMode == eDialModeHorizontal) ? NSControlStateValueOn : NSControlStateValueOff];
    } else if (  action == @selector(backupBank:) || action == @selector(backupPerfBank:)
              || action == @selector(backupSynthSettings:) || action == @selector(restoreSynthSettings:)
              || action == @selector(backupEverything:)
              || action == @selector(restoreBank:) || action == @selector(restorePerfBank:)
              || action == @selector(restoreEverything:)
              || action == @selector(deletePatchLocation:) || action == @selector(deletePerfLocation:)
              || action == @selector(loadPatchLocation:) || action == @selector(loadPerfLocation:)) {
        return gCommsState == eCommsOnLine;
    } else if (action == @selector(storeToBank:)) {
        [item setTitle:(gGlobalSettings.perfMode == 1) ? @"Store Perf to Bank..." : @"Store Patch to Bank..."];
        return gCommsState == eCommsOnLine;
    } else if (action == @selector(savePatch:)) {
        [item setTitle:(gGlobalSettings.perfMode == 1) ? @"Save Perf to File..." : @"Save Patch to File..."];
    }
    return YES;
}

@end

static NSMenuItem * make_item(NSString * title, SEL action, NSString * key, G2MenuTarget * target) {
    NSMenuItem * item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];

    [item setTarget:target];
    return item;
}

void setup_main_menu(void) {
    NSMenu *              menuBar  = [[NSApplication sharedApplication] mainMenu];
    static G2MenuTarget * target   = nil;

    if (target == nil) {
        target = [[G2MenuTarget alloc] init];
    }

    if (menuBar == nil) {
        menuBar = [[NSMenu alloc] init];
        [[NSApplication sharedApplication] setMainMenu:menuBar];
    }
    NSUserDefaults *      defaults = [NSUserDefaults standardUserDefaults];

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
    // File menu
    NSMenuItem * fileMI            = [[NSMenuItem alloc] init];
    NSMenu *     fileMenu          = [[NSMenu alloc] initWithTitle:@"File"];

    [fileMenu addItem:make_item(@"Open Patch/Perf File...", @selector(openPatch:), @"o", target)];
    [fileMenu addItem:make_item(@"Load Patch from Bank...", @selector(loadPatchLocation:), @"", target)];
    [fileMenu addItem:make_item(@"Load Performance from Bank...", @selector(loadPerfLocation:), @"", target)];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItem:make_item(@"Save Patch to File...", @selector(savePatch:), @"s", target)];
    [fileMenu addItem:make_item(@"Store Patch to Bank...", @selector(storeToBank:), @"", target)];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItem:make_item(@"Delete Patch...", @selector(deletePatchLocation:), @"", target)];
    [fileMenu addItem:make_item(@"Delete Performance...", @selector(deletePerfLocation:), @"", target)];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItem:make_item(@"New Patch", @selector(newPatch:), @"n", target)];
    [fileMI setSubmenu:fileMenu];
    [menuBar insertItem:fileMI atIndex:1];

    // Settings menu
    NSMenuItem * patchMI           = [[NSMenuItem alloc] init];
    NSMenu *     patchMenu         = [[NSMenu alloc] initWithTitle:@"Settings"];

    [patchMenu addItem:make_item(@"Synth", @selector(openSettings:), @",", target)];
    [patchMenu addItem:make_item(@"Patch", @selector(openPatchParams:), @"", target)];
    [patchMenu addItem:make_item(@"Perf", @selector(openPerfSettings:), @"", target)];
    [patchMenu addItem:[NSMenuItem separatorItem]];
    [patchMenu addItem:make_item(@"Notes", @selector(openNotes:), @"", target)];
    [patchMI setSubmenu:patchMenu];
    [menuBar insertItem:patchMI atIndex:2];

    // Backup menu
    NSMenuItem * bankMI            = [[NSMenuItem alloc] init];
    NSMenu *     bankMenu          = [[NSMenu alloc] initWithTitle:@"Backup"];
    NSMenuItem * backupSubMI       = [[NSMenuItem alloc] initWithTitle:@"Patch Bank" action:NULL keyEquivalent:@""];
    NSMenu *     backupSubMenu     = [[NSMenu alloc] initWithTitle:@"Patch Bank"];

    for (NSInteger i = 0; i < NUM_PATCH_BANKS; i++) {
        NSMenuItem * bankItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Bank %ld", (long)i + 1]
                                 action:@selector(backupBank:)
                                 keyEquivalent:@""];
        [bankItem setTarget:target];
        [bankItem setTag:i];
        [backupSubMenu addItem:bankItem];
    }

    NSMenuItem * backupPerfSubMI   = [[NSMenuItem alloc] initWithTitle:@"Performance Bank" action:NULL keyEquivalent:@""];
    NSMenu *     backupPerfSubMenu = [[NSMenu alloc] initWithTitle:@"Performance Bank"];

    for (NSInteger i = 0; i < NUM_PERF_BANKS; i++) {
        NSMenuItem * perfBankItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Bank %ld", (long)i + 1]
                                     action:@selector(backupPerfBank:)
                                     keyEquivalent:@""];
        [perfBankItem setTarget:target];
        [perfBankItem setTag:i];
        [backupPerfSubMenu addItem:perfBankItem];
    }

    [backupSubMI setSubmenu:backupSubMenu];
    [bankMenu addItem:backupSubMI];
    [backupPerfSubMI setSubmenu:backupPerfSubMenu];
    [bankMenu addItem:backupPerfSubMI];
    [bankMenu addItem:[NSMenuItem separatorItem]];
    [bankMenu addItem:make_item(@"Backup Synth Settings...", @selector(backupSynthSettings:), @"", target)];
    [bankMenu addItem:[NSMenuItem separatorItem]];
    [bankMenu addItem:make_item(@"Everything...", @selector(backupEverything:), @"", target)];
    [bankMI setSubmenu:bankMenu];
    [menuBar insertItem:bankMI atIndex:3];

    // Restore menu
    NSMenuItem * restoreMI          = [[NSMenuItem alloc] init];
    NSMenu *     restoreMenu        = [[NSMenu alloc] initWithTitle:@"Restore"];
    NSMenuItem * restoreSubMI       = [[NSMenuItem alloc] initWithTitle:@"Patch Bank" action:NULL keyEquivalent:@""];
    NSMenu *     restoreSubMenu     = [[NSMenu alloc] initWithTitle:@"Patch Bank"];

    for (NSInteger i = 0; i < NUM_PATCH_BANKS; i++) {
        NSMenuItem * bankItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Bank %ld", (long)i + 1]
                                 action:@selector(restoreBank:)
                                 keyEquivalent:@""];
        [bankItem setTarget:target];
        [bankItem setTag:i];
        [restoreSubMenu addItem:bankItem];
    }

    NSMenuItem * restorePerfSubMI   = [[NSMenuItem alloc] initWithTitle:@"Performance Bank" action:NULL keyEquivalent:@""];
    NSMenu *     restorePerfSubMenu = [[NSMenu alloc] initWithTitle:@"Performance Bank"];

    for (NSInteger i = 0; i < NUM_PERF_BANKS; i++) {
        NSMenuItem * perfBankItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Bank %ld", (long)i + 1]
                                     action:@selector(restorePerfBank:)
                                     keyEquivalent:@""];
        [perfBankItem setTarget:target];
        [perfBankItem setTag:i];
        [restorePerfSubMenu addItem:perfBankItem];
    }

    [restoreSubMI setSubmenu:restoreSubMenu];
    [restoreMenu addItem:restoreSubMI];
    [restorePerfSubMI setSubmenu:restorePerfSubMenu];
    [restoreMenu addItem:restorePerfSubMI];
    [restoreMenu addItem:[NSMenuItem separatorItem]];
    [restoreMenu addItem:make_item(@"Synth Settings...", @selector(restoreSynthSettings:), @"", target)];
    [restoreMenu addItem:[NSMenuItem separatorItem]];
    [restoreMenu addItem:make_item(@"Everything...", @selector(restoreEverything:), @"", target)];
    [restoreMI setSubmenu:restoreMenu];
    [menuBar insertItem:restoreMI atIndex:4];

    // Controls menu
    NSMenuItem * ctrlMI   = [[NSMenuItem alloc] init];
    NSMenu *     ctrlMenu = [[NSMenu alloc] initWithTitle:@"Controls"];

    [ctrlMenu addItem:make_item(@"Rotary", @selector(setDialModeRotary:), @"", target)];
    [ctrlMenu addItem:make_item(@"Vertical", @selector(setDialModeVertical:), @"", target)];
    [ctrlMenu addItem:make_item(@"Horizontal", @selector(setDialModeHorizontal:), @"", target)];
    [ctrlMI setSubmenu:ctrlMenu];
    [menuBar insertItem:ctrlMI atIndex:5];

    // View menu
    NSMenuItem * viewMI   = [[NSMenuItem alloc] init];
    NSMenu *     viewMenu = [[NSMenu alloc] initWithTitle:@"View"];

    [viewMenu addItem:make_item(@"Zoom In [⌘=]", @selector(zoomIn:), @"", target)];
    [viewMenu addItem:make_item(@"Zoom Out [⌘-]", @selector(zoomOut:), @"", target)];
    [viewMenu addItem:make_item(@"Zoom Reset", @selector(zoomReset:), @"", target)];
    [viewMI setSubmenu:viewMenu];
    [menuBar insertItem:viewMI atIndex:6];
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
