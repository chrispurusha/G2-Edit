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

#ifndef PALETTE_H
#define PALETTE_H

#include <stdbool.h>
#include <stdint.h>
#include "types.h"

// The drag-on module palette: a band under the topbar carrying the sixteen module groups and the
// modules in the selected one. See Docs/module-palette-design.md.

#define PALETTE_BAND_HEIGHT    (74.0)

bool palette_is_open(void);
void palette_set_open(bool open);
void palette_toggle(void);

// How much taller the topbar is right now: the band height when open, zero when closed. The canvas
// origin comes from this, through the theme.
double palette_band_height(void);

tPaletteGroup palette_selected_group(void);
void palette_select_group(tPaletteGroup group);

void palette_render(void);

// Mouse. Each returns true when it consumed the event.
bool palette_left_down(tCoord coord);
bool palette_left_up(tCoord coord);
void palette_cursor_moved(tCoord coord);
bool palette_scroll(double delta, tCoord coord);

// Whether a tile is being dragged right now — the canvas asks, so it can show where a drop lands.
bool palette_drag_active(void);

// Add the selected group's nth module to the patch under the focused module, the way the manual's
// double-click does. Exposed for the backdoor, which cannot synthesise a drag.
// The colour new modules are created in, chosen from the band's swatches. The instrument works the
// same way (manual p.61): the selection persists, so a run of modules can be added in one colour.
uint32_t palette_new_module_colour(void);

bool palette_add_module(tModuleType type);

// Put the palette into a drag, as though a tile had been picked up and moved to `coord`. Exists for
// the backdoor: a synthetic mouse drag never reaches the canvas, so this is the only way to render
// the drag ghost without a person holding the mouse. palette_left_up() finishes it.
void palette_begin_drag(tModuleType type, tCoord coord);

#endif // PALETTE_H
