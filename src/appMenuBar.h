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

#ifdef __cplusplus
}
#endif

#endif // __APP_MENU_BAR_H__
