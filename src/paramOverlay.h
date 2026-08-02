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

#ifndef PARAM_OVERLAY_H
#define PARAM_OVERLAY_H

#include "types.h"

// The original editor's F5-F8 / Ctrl+F8 "yellow popup box" views: a small label pinned under
// EVERY parameter on the canvas at once, each mode showing a different thing about it. G2-Edit
// reaches them from the View menu rather than the function keys.
//
// Deliberately NOT in SynthLib. Every mode below is about a G2 module parameter - morph groups,
// the 120 Parameter Page knobs, the patch's controller table - none of which exist in the other
// two apps.
//
// The modes are mutually exclusive, as they are in the original: choosing one replaces whatever
// was showing. overlayModeNone restores the pre-existing behaviour, where a knob/CC label appears
// only for the single parameter under the mouse.
typedef enum {
    overlayModeNone = 0,
    overlayModeValues,        // each parameter's current value
    overlayModeMorphGroups,   // which of the 8 morph groups the parameter belongs to
    overlayModeKnobs,         // the Parameter Page knob it's assigned to, patch or global
    overlayModeMidiCc,        // the MIDI CC# assigned to it
    overlayModeMidiValues,    // the value as it goes out over MIDI (0-127)
    overlayModeMax
} tParamOverlayMode;

tParamOverlayMode param_overlay_mode(void);
void param_overlay_set_mode(tParamOverlayMode mode);
const char * param_overlay_mode_name(tParamOverlayMode mode);

// Called once per frame before any module is drawn - clears the previous frame's rows.
void param_overlay_begin_frame(void);

// Called from render_param_common() for every parameter it draws, with the rectangle the widget
// occupies and the display string the widget just rendered ("554.4Hz", "0.0", ...) - the caller
// already has that in hand, which saves this module re-deriving per-type formatting it has no
// business knowing. May be NULL or empty for types that show a strMap entry instead of a number.
// Decides for itself whether this parameter gets a row in the current mode, so callers need no
// knowledge of the modes.
void param_overlay_note_param(tModule * module, uint32_t paramIndex, tRectangle rectangle, const char * displayValue);

// Paints everything queued this frame. Must run after the whole canvas is drawn, or later modules
// paint over the labels.
void param_overlay_render(void);

#endif /* PARAM_OVERLAY_H */
