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

#include <stdbool.h>

// register_sleep_wake_notifications() and setup_main_menu() are implemented in misc.mm — the only
// two things left in this codebase that genuinely need Objective-C/Cocoa. Everything else
// declared below is plain C: menu actions live in menuActions.c, settings persistence (backed by
// SynthLib's cross-platform prefs.h rather than NSUserDefaults) lives in persistence.c.
void register_sleep_wake_notifications(void);
void setup_main_menu(void);

// Applies saved window size/position, zoom, dial mode, and last-browsed folder — called once by
// setup_main_menu() right after prefs_init(). Implemented in persistence.c.
void load_saved_settings(void);

void save_zoom_factor(double zoom);

// File/Settings/Backup/Restore menu actions — plain-C-callable bodies used by the in-window menu
// bar (src/appMenuBar.c). File open/save and folder picking all go through the custom in-window
// browser (SynthLib/src/fileBrowser.cpp); alerts/confirms/bank-target pickers go through
// SynthLib/src/alertDialog.cpp — none of it uses native Cocoa panels any more. Only the dispatch
// logic (which browser mode to open, with what pre-filled state) lives here.
void file_menu_open_patch(void);
void file_menu_save_patch(void);
void file_menu_save_patch_to_current_path(void);
bool file_menu_have_saved_path(void);
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
void settings_menu_open_param_pages(void);
void settings_menu_open_param_overview(void);
void settings_menu_open_virtual_keyboard(void);
void settings_menu_open_patch_adjuster(void);

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

// Registered with the file browser (SynthLib/src/fileBrowser.cpp) via
// set_file_browser_directory_changed_callback() at startup — see setup_main_menu() — so it can
// persist the last folder browsed across app launches the same way zoom/window/dial-mode are.
void save_file_browser_directory(const char * path);

#ifdef __cplusplus
}
#endif

#endif // __MISC_H__
