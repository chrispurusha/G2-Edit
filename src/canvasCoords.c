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

// See canvasCoords.h for why this is not in mouseHandle.c any more.

#include <math.h>

#include "sysIncludes.h"
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "misc.h"          // save_zoom_factor() — the prefs side of the zoom
#include "canvasCoords.h"

void convert_mouse_coord_to_module_area_coord(tCoord * targetCoord, tCoord coord) {
    if (targetCoord == NULL) {
        return;
    }
    double     val  = 0.0;
    tRectangle area = module_area();

    val            = coord.x - area.coord.x;
    val           += calc_scroll_x();
    val           /= get_zoom_factor();
    targetCoord->x = val;

    val            = coord.y - area.coord.y;
    val           += calc_scroll_y();
    val           /= get_zoom_factor();
    targetCoord->y = val;
}

// Which module GRID SQUARE a coordinate falls in. Moved here from menus.c alongside its sibling
// above: same arithmetic, same absence of any window, and the module drag needs it.
void convert_mouse_coord_to_module_column_row(uint32_t * column, uint32_t * row, tCoord coord) {
    double     val  = 0.0;
    tRectangle area = module_area();

    if (column != NULL) {
        val     = coord.x - area.coord.x;
        val    += calc_scroll_x();
        val    /= MODULE_X_SPAN;
        val    /= get_zoom_factor();

        if (val < 0.0) {
            val = 0.0;
        }
        *column = floor(val);
    }

    if (row != NULL) {
        val  = coord.y - area.coord.y;
        val += calc_scroll_y();
        val /= MODULE_Y_SPAN;
        val /= get_zoom_factor();

        if (val < 0.0) {
            val = 0.0;
        }
        *row = floor(val);
    }
}

// ── Zoom, stepped ───────────────────────────────────────────────────────────────────────────────
//
// One Cmd +/- worth of canvas zoom, anchored at the module area's top-left and remembered in prefs.
// Shared because both shells offer the same shortcut and neither should own the arithmetic: the
// application had these four lines written out twice in its key handler (once per direction), and the
// plug-in would have made a third and fourth copy.
// The ANCHOR is what the two callers disagree about and nothing else: Cmd +/- has no meaningful
// position so it uses the module area's corner, while Cmd + wheel zooms around the pointer, which is
// what makes zooming feel like it is aimed at something.
void canvas_zoom_step_at(double delta, tCoord anchor) {
    set_zoom_factor(get_zoom_factor() + delta, anchor);
    save_zoom_factor(get_zoom_factor());
}

void canvas_zoom_step(double delta) {
    tRectangle area = module_area();

    canvas_zoom_step_at(delta, (tCoord){area.coord.x, area.coord.y});
}
