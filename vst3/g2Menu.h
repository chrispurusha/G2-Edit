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

#ifndef __G2_MENU_H__
#define __G2_MENU_H__

#include "sysIncludes.h"
#include "synthlibTypes.h"
#include "menuBar.h"
#include "defs.h"   // MENU_BAR_HEIGHT

#ifdef __cplusplus
extern "C" {
#endif

// Height reserved above the canvas for the topbar that is not built yet — variation buttons, patch
// name, voice count, patch volume and the cable view toggles. Reserved NOW so the canvas is laid out
// around it from the start; adding it later would otherwise shift the whole patch down at that
// point. The application's own TOP_BAR_HEIGHT is 80; this is smaller because the slot, performance
// and clock controls that fill much of the app's bar have no meaning in a plug-in.
#define G2_PLUGIN_TOPBAR_HEIGHT    (44.0)

// Everything above the canvas. The editor sizes its window by this so it need not know how the
// chrome is divided up, and so moving height between the menu bar and the topbar changes one place.
#define G2_PLUGIN_CHROME_HEIGHT    (MENU_BAR_HEIGHT + G2_PLUGIN_TOPBAR_HEIGHT)

extern tMenuBarItem gPluginMenuBar[];

tRectangle g2_menu_bar_rect(double pointWidth);

// Reserved band below the menu bar. Currently drawn as an empty strip.
tRectangle g2_topbar_rect(double pointWidth);

// Name of the patch chosen through the File menu, or NULL while the built-in one is playing.
const char * g2_menu_loaded_patch_name(void);

#ifdef __cplusplus
}
#endif

#endif // __G2_MENU_H__
