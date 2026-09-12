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
// Notes: Docs/code-notes/deviceSync.h.md - "// notes §k" refers there.

#ifndef DEVICE_SYNC_H
#define DEVICE_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// notes §1

// USB thread. Drains every queued command, discarding it, and returns a mask with bit N set for
// each slot that had at least one PATCH-CHANGING command queued (queries and view-only commands
// do not count). 0 means the editor never diverged and the caller should just pull as usual.
uint32_t device_sync_drain_offline_edits(void);

// notes §2
uint32_t device_sync_write_recovery_files(uint32_t slotMask, char * outLocation, size_t outLocationSize);

// Removes the files the last device_sync_write_recovery_files() wrote. Call ONLY after the user
// has saved the same work somewhere of their own choosing — never on the branch that takes the
// G2's copy, where the recovery file is the only surviving copy of what they did.
void device_sync_discard_recovery_files(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_SYNC_H
