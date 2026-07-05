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

#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tFileDialogueCallback)(const char * path);
typedef void (*tConfirmCallback)(bool confirmed);
typedef void (*tBankTargetConfirmCallback)(bool confirmed, uint32_t targetBank1Indexed);
typedef void (*tBankLocationConfirmCallback)(bool confirmed, uint32_t bank1Indexed, uint32_t location1Indexed);

void open_file_read_dialogue_async(tFileDialogueCallback callback);
void open_file_write_dialogue_async(tFileDialogueCallback callback, const char * defaultName);
void open_folder_dialogue_async(tFileDialogueCallback callback, const char * title);
void show_alert_async(const char * title, const char * message);
void show_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle, tConfirmCallback callback);
// Same as show_confirm_dialogue_async, plus a numeric accessory field (clamped to
// [1, maxBank1Indexed], pre-filled with defaultTargetBank1Indexed) for picking a target bank that
// may differ from the one the dialog's message/title refer to — used by Bank Restore's "restore
// bank 5's backup into bank 7" case. callback's targetBank1Indexed is only meaningful when
// confirmed is true.
void show_bank_target_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle, uint32_t defaultTargetBank1Indexed, uint32_t maxBank1Indexed, tBankTargetConfirmCallback callback);

// One row of show_bank_location_list_dialogue_async()'s picker — name is a caller-owned string
// (copied synchronously before the function returns, so it only needs to survive the call itself,
// not the async dialog's lifetime). category is a patchTypeStrMap index (0-15); the dialog itself
// builds the visible "Bank X, Loc Y: name" label and the category grouping, so callers don't need
// to format anything.
typedef struct {
    const char * name;
    uint8_t      category;
    uint32_t     bank1Indexed;
    uint32_t     location1Indexed;
} tBankLocationListItem;

// Presents a scrollable, named list of pre-built bank/location choices (e.g. from
// gPatchNameTable/gPerfNameTable, populated by the List Names sweep) instead of blind numeric
// fields — used by every Load/Store/Delete bank/location picker. A segmented control lets the user
// switch between Bank/Location order, Category (alphabetical within each category), and fully
// alphabetical. If itemCount is 0, the list is empty and confirming is a no-op (callback still
// fires with confirmed=false in that case).
void show_bank_location_list_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle, const tBankLocationListItem * items, uint32_t itemCount, tBankLocationConfirmCallback callback);

#ifdef __cplusplus
}
#endif

#endif // FILE_DIALOG_H
