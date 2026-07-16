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

#ifndef __MISC_H__
#define __MISC_H__

#ifdef __cplusplus
extern "C" {
#endif

void register_sleep_wake_notifications(void);
void setup_main_menu(void);
void save_zoom_factor(double zoom);
void save_window_size(int w);
void save_window_pos(int x, int y);

// File/Settings/Backup/Restore menu actions — plain-C-callable bodies used by the in-window menu
// bar (src/appMenuBar.c). File open/save and folder picking all go through the custom in-window
// browser (SynthLib/src/fileBrowser.cpp); alerts/confirms/bank-target pickers go through
// SynthLib/src/alertDialog.cpp — none of it uses native Cocoa panels any more. Only the dispatch
// logic (which browser mode to open, with what pre-filled state) lives here.
void file_menu_open_patch(void);
void file_menu_save_patch(void);
void file_menu_new_patch(void);
void file_menu_load_patch_location(void);
void file_menu_load_perf_location(void);
void file_menu_delete_patch_location(void);
void file_menu_delete_perf_location(void);
void file_menu_store_to_bank(void);

// Settings menu actions
void settings_menu_open_synth(void);
void settings_menu_open_patch(void);
void settings_menu_open_perf(void);
void settings_menu_open_notes(void);

// Backup menu actions
void backup_menu_patch_bank(void);
void backup_menu_perf_bank(void);
void backup_menu_synth_settings(void);
void backup_menu_everything(void);

// Restore menu actions
void restore_menu_patch_bank(void);
void restore_menu_perf_bank(void);
void restore_menu_synth_settings(void);
void restore_menu_everything(void);

// Controls menu — save_dial_mode persists the choice the same way save_zoom_factor/
// save_window_size/save_window_pos do; setting gDialMode itself is plain global state the
// caller sets directly (see src/appMenuBar.c).
void save_dial_mode(int mode);

// Registered with the file browser (SynthLib/src/fileBrowser.cpp) via
// set_file_browser_directory_changed_callback() at startup — see setup_main_menu() — so it can
// persist the last folder browsed across app launches the same way zoom/window/dial-mode are.
void save_file_browser_directory(const char * path);

#ifdef __cplusplus
}
#endif

#endif // __MISC_H__
