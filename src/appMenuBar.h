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

#ifndef __APP_MENU_BAR_H__
#define __APP_MENU_BAR_H__

#include "sysIncludes.h"
#include "menuBar.h" // tMenuBarItem/render_menu_bar/handle_menu_bar_click/update_menu_bar_hover — see SynthLib

#ifdef __cplusplus
extern "C" {
#endif

// G2-Edit's own File/Settings/Backup/Restore/Controls/View row, replacing the
// native Cocoa menu bar (misc.mm) with SynthLib's cross-platform menuBar
// engine. gAppMenuBar is a NULL-label-terminated tMenuBarItem[] suitable for
// passing straight into render_menu_bar()/handle_menu_bar_click()/
// update_menu_bar_hover(); app_menu_bar_rect() is the bar's screen rectangle
// for this frame.
extern tMenuBarItem gAppMenuBar[];

tRectangle app_menu_bar_rect(void);

// The individual menus, so a host other than the application can compose its own bar from a subset
// of them — the VST3 plug-in drops Backup, Restore and Experimental entirely.
void open_file_menu(tCoord anchor);
void open_settings_menu(tCoord anchor);
void open_backup_menu(tCoord anchor);
void open_restore_menu(tCoord anchor);
void open_controls_menu(tCoord anchor);
void open_tools_menu(tCoord anchor);
void open_view_menu(tCoord anchor);
void open_help_menu(tCoord anchor);
void open_experimental_menu(tCoord anchor);

// False omits the bank and device entries rather than greying them. See the note in appMenuBar.c:
// greyed means "not right now", and the plug-in needs "not ever".
void app_menu_set_device_capable(bool capable);

#ifdef __cplusplus
}
#endif

#endif // __APP_MENU_BAR_H__
