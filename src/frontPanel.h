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
// Notes: Docs/code-notes/frontPanel.c.md - "// notes §k" refers there.

#ifndef FRONT_PANEL_H
#define FRONT_PANEL_H

#include <stdbool.h>
#include "synthlibTypes.h"

// What the four displays show - the PATCH SETTINGS / GLOBAL PANEL button cycles them (notes §3)
typedef enum {
    eFrontPanelPages = 0,       // the focused slot's Parameter Pages
    eFrontPanelSettings,        // the eight patch settings printed above the displays
    eFrontPanelGlobal,          // the performance's Global Parameter Pages
} tFrontPanelView;

#define PREF_KEY_FRONT_PANEL    "frontPanelMode"

// The mode, per document - each G2 Alike instance has its own
bool front_panel_active(void);
void front_panel_set_active(bool on);
void front_panel_toggle(void);
void front_panel_load_preference(void);         // the application's, at start
const char * front_panel_button_label(void);    // what the topbar switch says now

// Drawn in place of the module panes; registers its own click regions
void front_panel_render(void);

// The patch-settings knob drag, run from canvasDrag.c's gesture table so both builds share it
bool front_panel_drag_motion(tCoord coord, double rawX, double rawY);
void front_panel_drag_cancel(void);

// A right-click while the panel is up: a knob's or a button's parameter menu. Consumes every click.
bool front_panel_right_click(tCoord coord);

#endif /* FRONT_PANEL_H */
