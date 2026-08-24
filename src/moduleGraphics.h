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

#ifndef __MODULE_GRAPHICS_H__
#define __MODULE_GRAPHICS_H__

#include "sysIncludes.h"
#include "types.h"

void render_module(tModule * module);
void render_modules(void);

// Draws one module parameter - dial, slider, toggle or menu button, whichever the param's type
// calls for - and registers its clickable rect as a click region. Used by render_module() for the
// patch canvas and by the Parameter Pages panel, which draws the same widget somewhere else; see
// set_param_render_area() (renderParams.h) for switching which area it renders into.
// Is this parameter the widget under the given coordinate? Asks the click-region registry, so the
// answer matches where a click would land — see the definition.
bool param_is_under_cursor(const tModule * module, uint32_t paramIndex, tCoord coord);

// ── What is under the cursor on the canvas ───────────────────────────────────
//
// Every canvas widget registers a click region carrying one of the context structs below, and every
// one of those begins with this same pair. That is what lets a caller ask the registry "what is
// here?" and then act on the answer, instead of walking the app's own rectangle arrays — which is a
// second description of where the widgets are, free to disagree with the registry about z-order.
//
// The shared prefix is the sockaddr idiom: a pointer to any of the contexts may be read as a
// tCanvasWidget * to get its kind and its module, because C guarantees the layout of a common
// initial sequence. Add a new canvas widget kind and you MUST give its context the same two leading
// members, in this order.
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

// The canvas widget under this coordinate, or NULL. Front-to-back through the click-region registry,
// so the answer is the same widget a click at that point would reach.
//
// RESTRICTED TO eClickLayerCanvas on purpose. The morph dials register at eClickLayerPanel and are
// not part of the scrolling canvas, so a caller that means "which module widget is the pointer over"
// must not get one; and nothing outside this file registers a canvas region, which is what makes
// reading the tag off the returned pointer safe.
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

