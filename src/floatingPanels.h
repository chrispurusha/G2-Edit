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
// Notes: Docs/code-notes/floatingPanels.c.md - "// notes §k" refers there.

#ifndef __FLOATING_PANELS_H__
#define __FLOATING_PANELS_H__

#include "sysIncludes.h"
#include "geometry.h"
#include "floatingPanel.h"

#ifdef __cplusplus
extern "C" {
#endif

// THE COORDINATOR OVER THE ELEVEN PANELS, and the one list of them. It lived in graphics.c, which
// the plug-in does not compile, so the plug-in drew none of them and routed no clicks to them -
// every Settings and Tools menu entry opened a panel that could not appear. Moved out 2026-09-16;
// both hosts now register the four callbacks below in their own popup table (synthlibPopups.h), at
// whatever layer they choose.
void floating_panels_render(void);
bool floating_panels_mouse(tCoord coord, tMouseButton mouseButton);
bool floating_panels_key(int key, int mods, int action);
bool floating_panels_scroll(double yDelta);

// Is the pointer over a panel? The canvas underneath must not hover, highlight or scroll when it is.
bool floating_panels_under(tCoord coord);

// A panel being moved owns the pointer until it is released.
bool floating_panels_drag(tCoord coord);

// notes §7
bool floating_panel_is_frontmost(const tFloatingPanel * panel);

#ifdef __cplusplus
}
#endif

#endif // __FLOATING_PANELS_H__
