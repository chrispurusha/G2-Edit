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

void open_file_read_dialogue_async(tFileDialogueCallback callback);
void open_file_write_dialogue_async(tFileDialogueCallback callback, const char * defaultName);
void open_folder_dialogue_async(tFileDialogueCallback callback, const char * title);
void show_alert_async(const char * title, const char * message);
void show_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle, tConfirmCallback callback);
// Same as show_confirm_dialogue_async, plus a dropdown (labeled fieldLabel) listing "Bank 1"
// through "Bank maxBank1Indexed" for picking a bank number without typing one — pre-selected to
// defaultTargetBank1Indexed. Generic across every "pick a bank number" case in Backup/Restore: the
// bank to back up, the bank whose backup to restore, and the (possibly different) target bank to
// restore into. callback's targetBank1Indexed is only meaningful when confirmed is true.
void show_bank_target_confirm_dialogue_async(const char * title, const char * message, const char * confirmButtonTitle, const char * fieldLabel, uint32_t defaultTargetBank1Indexed, uint32_t maxBank1Indexed, tBankTargetConfirmCallback callback);

// Every Load/Store/Delete bank/location picker (previously an NSAlert+NSTableView here) now uses
// SynthLib's cross-platform open_bank_browser() — see bankBrowser.h.

#ifdef __cplusplus
}
#endif

#endif // FILE_DIALOG_H
