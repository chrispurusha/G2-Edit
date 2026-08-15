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

#ifndef __HELP_PANEL_H__
#define __HELP_PANEL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "floatingPanel.h"

// The keyboard and mouse reference, as a floating panel (floatingPanel.h) so it can be left open
// beside the canvas while you try the things it lists — which is the whole point of a shortcut list
// and the reason it is not a modal dialogue.
//
// Its content is a static table in helpPanel.c. That table is DOCUMENTATION: every row has to match
// what the code actually does, so a binding changed in mouseHandle.c or virtualKeyboard.c means that
// row changes too. A shortcut list that lies is worse than none, because it is believed.
typedef struct {
    bool           active;
    tFloatingPanel panel;
    tRectangle     close;
    bool           closePressed;
} tHelpPanel;

extern tHelpPanel gHelpPanel;

void open_help_panel(void);
void close_help_panel(void);
void render_help_panel(void);
bool handle_help_panel_mouse(tCoord coord, tMouseButton mouseButton);
bool handle_help_panel_key(int key, int mods, int action);

#ifdef __cplusplus
}
#endif

#endif // __HELP_PANEL_H__
