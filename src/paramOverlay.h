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
// Notes: Docs/code-notes/paramOverlay.h.md - "// notes §k" refers there.

#ifndef PARAM_OVERLAY_H
#define PARAM_OVERLAY_H

#include "types.h"

// notes §1
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

// notes §2
void param_overlay_note_param(tModule * module, uint32_t paramIndex, tRectangle rectangle, const char * displayValue);

// notes §3
void param_overlay_render_pane(uint32_t pane);

#endif /* PARAM_OVERLAY_H */
