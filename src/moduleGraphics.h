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
// Notes: Docs/code-notes/moduleGraphics.h.md - "// notes §k" refers there.

#ifndef __MODULE_GRAPHICS_H__
#define __MODULE_GRAPHICS_H__

#include "sysIncludes.h"
#include "types.h"

void render_module(tModule * module);
void render_modules(void);

// notes §1
bool param_is_under_cursor(const tModule * module, uint32_t paramIndex, tCoord coord);

// notes §2
typedef enum {
    eCanvasWidgetNone = 0,
    eCanvasWidgetParam,
    eCanvasWidgetMode,
    eCanvasWidgetConnector,
    eCanvasWidgetModule,      // the module body AND its drag strip: one context, one menu
    eCanvasWidgetMorph,       // a morph group dial, which lives on the fixed overlay, not the canvas
} eCanvasWidgetKind;

typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
} tCanvasWidget;

// notes §3
const tCanvasWidget * canvas_widget_at(tCoord coord);

// The same, across EVERY layer, so the fixed morph overlay wins over the canvas scrolling beneath it
// — the precedence a click already gets from the layer order. For callers that mean "whatever the
// pointer is on", rather than "which module widget".
const tCanvasWidget * canvas_widget_at_any_layer(tCoord coord);

// The param / mode / connector index the widget carries, or 0 for a kind that has none.
uint32_t canvas_widget_index(const tCanvasWidget * widget);

// Returns the rectangle the widget was actually drawn and registered at — see the definition.
tRectangle render_param_common(tRectangle rectangle, tModule * module, uint32_t paramRef, uint32_t paramIndex);
void render_cables(void);
void render_morph_groups(void);
void calculate_module_bounds(double * xEndMax, double * yEndMax, tRectangle moduleArea);
void render_cable_from_to(tConnector from, tConnector to, double thickness);
tRectangle module_area(void);

// Waveform pickers draw a picture of the wave instead of its name — on the module's button face and,
// via tMenuItem::drawItem, on the drop-down's entries too.
bool module_wave_picker_param(uint32_t moduleType, uint32_t paramIndex);
bool module_wave_picker_mode(uint32_t moduleType, uint32_t modeIndex);
double module_wave_icon_shape(uint32_t moduleType);
void render_wave_icon(tRectangle buttonRect, uint32_t moduleType, uint32_t waveValue, double shape);

#endif // __MODULE_GRAPHICS_H__

