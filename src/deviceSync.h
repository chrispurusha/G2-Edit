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

#ifndef DEVICE_SYNC_H
#define DEVICE_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Keeping the editor's copy of a patch and the G2's in step.
//
// While the G2 is connected every edit is sent to it as it happens, so the two copies can only
// drift apart while it is NOT connected — "dirty" here means precisely "edited while the G2 was
// away", which is a far smaller thing to track than general dirty state.
//
// Those edits are already recorded, in the one place nobody thinks to look: the USB command
// queue. state_handler() only drains it while online, so anything sitting there when the device
// comes back was enqueued while it was gone. That makes the queue both the divergence detector
// and the record of WHICH slots diverged — no per-edit flag needed anywhere.
//
// The queued commands themselves are always discarded rather than replayed: they are increments
// addressed to a patch state that the reconnect is about to replace, and replaying them against
// a freshly-pulled database is how you get modules edited by index into whatever now occupies
// that index. Pushing whole patches is the only honest way to make the G2 match the editor.

// USB thread. Drains every queued command, discarding it, and returns a mask with bit N set for
// each slot that had at least one PATCH-CHANGING command queued (queries and view-only commands
// do not count). 0 means the editor never diverged and the caller should just pull as usual.
uint32_t device_sync_drain_offline_edits(void);

// UI thread. Writes one recovery .pch2 per dirty slot before the user is asked anything, so no
// answer they can give — including a mis-click — can lose the work. Returns the number written,
// and copies a human-readable location (the folder, or the single file when there is only one)
// into outLocation for the dialog to quote.
uint32_t device_sync_write_recovery_files(uint32_t slotMask, char * outLocation, size_t outLocationSize);

// Removes the files the last device_sync_write_recovery_files() wrote. Call ONLY after the user
// has saved the same work somewhere of their own choosing — never on the branch that takes the
// G2's copy, where the recovery file is the only surviving copy of what they did.
void device_sync_discard_recovery_files(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_SYNC_H
