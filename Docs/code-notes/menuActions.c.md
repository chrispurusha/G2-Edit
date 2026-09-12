# menuActions.c notes

The longer comments from `menuActions.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

Bank number (0-indexed) chosen from the "Backup Patch Bank"/"Backup Performance Bank" dropdown
dialog, stashed here between that dialog's confirm callback and the folder-choose panel's
completion callback (tFileBrowserCallback is a plain C function pointer with no room for
captured context). Defaults the dropdown to whatever was picked last time (see
backup_menu_patch_bank()/backup_menu_perf_bank() below), rather than always resetting to Bank 1.

## 2. file scope

Same stash-between-callbacks pattern as the backup statics above, but for Restore: a first
dropdown dialog picks the source bank/domain (on_restore_source_bank_picked), a second supplies
the (possibly different) target bank (on_bank_restore_confirmed), then the folder-choose panel
supplies the source folder once the user has confirmed.

## 3. `on_backup_bank_picked()`

Confirm callback for the "which bank to back up" dropdown dialog opened by
backup_menu_patch_bank()/backup_menu_perf_bank() below — sPendingBackupIsPerf was already set by
whichever of those two called us, so this only needs to record the bank and move on to the
folder picker.

## 4. `on_restore_source_bank_picked()`

Confirm callback for the "which bank's backup to restore" dropdown dialog opened by
restore_menu_patch_bank()/restore_menu_perf_bank() below — sPendingRestoreIsPerf was already set
by whichever of those two called us. Chains straight into the existing target-bank dropdown
dialog, defaulting it to the same bank just picked (the common "restore into itself" case),
exactly mirroring what used to default from the clicked submenu item's tag.

## 5. in `on_restore_source_bank_picked()`

Domain for the pending Store flow, set by file_menu_store_to_bank() right before opening the
bank/location dialog (mirrors gGlobalSettings.perfMode — Store always acts on whatever's in the
edit buffer) — same stash pattern as sPendingRestoreIsPerf above, needed because
tBankBrowserCallback's signature has no room for it.

## 6. `on_store_bank_location_chosen()`

Kicks off the peek — the actual overwrite-warning confirm and eMsgCmdStorePatch send happen
later in graphics.cpp's check_action_flags(), once the async peek result lands in gStorePeek*
(there's no captured-context callback chain needed here, unlike Restore: the target bank/
location the peek was for is recorded in gStorePeekBank/gStorePeekLocation, so nothing has to be
stashed on the menuActions.c side past this point).

## 7. in `on_store_bank_location_chosen()`

Domain for the pending Delete flow, set by file_menu_delete_patch_location()/
file_menu_delete_perf_location() right before opening the bank/location dialog — same stash
pattern as sPendingRestoreIsPerf above, needed because tBankBrowserCallback's signature has no
room for it.

## 8. `build_bank_browser_items()`

Builds the tBankBrowserItem array feeding open_bank_browser(), from the cached name tables (see
project memory: List Names sweep) — no device round-trip needed just to show the list.
populatedOnly restricts to locations that already contain something (Load/Delete: you can only
act on what exists); when false, every location in the domain is listed, with unpopulated ones
named "(empty)" (Store: the target may be a blank slot). Caller must free() both *outItems and
outNames once done — open_bank_browser() copies everything into its own storage synchronously
before returning, so they only need to survive the call itself.

## 9. in `build_bank_browser_items()`

COPY_STRING, NOT strncpy. `names` is a malloc'd array of contiguous
CLAVIA_NAME_SIZE + 1 byte rows, and strncpy(dst, src, 16) writes a terminator only
when the source is SHORTER than 16 — at exactly 16 characters it fills the row and
leaves the 17th byte as whatever malloc handed back. The rows are adjacent, so the
name then runs straight on into the NEXT patch's row: "Bank 2 Loc 106" displayed as
```
"DreamPad    DLX?DreamPluck   DLXEvolution String" — three consecutive entries, with
```
the uninitialised byte showing up as the '?'.

The same defect was fixed in the name tables themselves (usbComms.c) the same day;
this copy hid from that sweep because it says sizeof(names[i]) - 1 rather than
CLAVIA_NAME_SIZE. COPY_STRING always terminates, whatever the source length.

## 10. in `file_menu_new_patch()`

Works offline. A new patch is a local edit — the device push below is what needs the G2, not
the patch itself, and refusing outright made the editor unusable as an offline sketchpad
(which is also the only way to try the sound engine with no hardware around). The USB thread
skips the push when there is nothing to push to.

Do the init (local DB reset) AND the device push together on the USB thread as one ordered
command, so the reset can't be clobbered by the USB thread's async patch-change re-reads
between the two — same reasoning as the file-load path. (Without a push the G2 would keep
playing its old patch, diverging from what the editor shows.)

## 11. in `file_menu_new_patch()`

OFFLINE IT IS DONE HERE, NOT QUEUED. state_handler() (usbComms.c) returns early for the whole
of eCommsNeverConnected and eCommsReconnecting - it tries to open the device, sleeps 500 ms
and returns - so it never reaches its own msg_receive(). A command queued while there is no
G2 is therefore never dequeued at all: the local reset did not happen AND device_op_begin()'s
busy overlay had nothing to end it, so the editor sat on "New Patch..." until it was force
quit (CT, 2026-09-07). init_patch() is dataBase.c's and touches only the slot's own state, so
the UI thread can do it directly, the same way the backdoor's NEWPATCH already does.
