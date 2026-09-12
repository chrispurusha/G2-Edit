# deviceSync.h notes

The longer comments from `deviceSync.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `device_sync_drain_offline_edits()`

Keeping the editor's copy of a patch and the G2's in step.

While the G2 is connected every edit is sent to it as it happens, so the two copies can only
drift apart while it is NOT connected — "dirty" here means precisely "edited while the G2 was
away", which is a far smaller thing to track than general dirty state.

Those edits are already recorded, in the one place nobody thinks to look: the USB command
queue. state_handler() only drains it while online, so anything sitting there when the device
comes back was enqueued while it was gone. That makes the queue both the divergence detector
and the record of WHICH slots diverged — no per-edit flag needed anywhere.

The queued commands themselves are always discarded rather than replayed: they are increments
addressed to a patch state that the reconnect is about to replace, and replaying them against
a freshly-pulled database is how you get modules edited by index into whatever now occupies
that index. Pushing whole patches is the only honest way to make the G2 match the editor.

## 2. `device_sync_write_recovery_files()`

UI thread. Writes one recovery .pch2 per dirty slot before the user is asked anything, so no
answer they can give — including a mis-click — can lose the work. Returns the number written,
and copies a human-readable location (the folder, or the single file when there is only one)
into outLocation for the dialog to quote.
