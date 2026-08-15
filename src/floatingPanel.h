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

#ifndef __FLOATING_PANEL_H__
#define __FLOATING_PANEL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

// A panel that sits ON the canvas rather than over it: movable, non-modal, and able to share the
// screen with other panels and with the patch underneath.
//
// The Virtual Keyboard and the Patch Adjuster were both written as MODAL dialogues — each computed
// (renderW - boxW) / 2 every frame, so it re-centred itself continuously and could not be moved, and
// each drew draw_dialog_background_overlay() over the whole window and returned true from its mouse
// handler for every click anywhere. Two of those cannot be open at once in any useful sense. This
// holds the small amount of state that turns such a panel into a floating one, so the behaviour is
// written once instead of once per panel.
//
// The Patch Mutator already floats, by hand, with its own copy of these four fields. It is left
// alone deliberately — it works, and porting it buys nothing but risk — but it is the model this
// follows, and it could be migrated later.
typedef struct {
    tRectangle rect;           // where the panel IS: position kept across frames, size set by content
    tRectangle titleBarRect;   // the drag handle; the render pass fills this in from draw_panel_chrome()
    bool       placed;         // false until first shown — a position is chosen ONCE, never per frame
    bool       dragging;
    tCoord     dragMouseStart;
    tCoord     dragPanelStart;

    // Stacking order: higher is nearer the front. Panels overlap, so the one drawn on top must also
    // be the one that gets the click — without this the hit-test order is whatever order the
    // handlers happen to be called in, and a panel underneath silently swallows presses aimed at the
    // panel above it.
    uint32_t order;
} tFloatingPanel;

// Call at the top of the panel's render pass with the size its content wants. Chooses a position the
// first time the panel is shown and leaves it wherever the user has since dragged it. Returns the
// rect to draw into.
tRectangle floating_panel_place(tFloatingPanel * panel, double width, double height);

// Is this coordinate inside the panel? A floating panel's mouse handler must claim ONLY its own
// clicks — the modal versions returned true for everything, which is exactly what stopped a second
// panel, or the canvas, from ever seeing a click.
bool floating_panel_contains(const tFloatingPanel * panel, tCoord coord);

// Press routing. Starts a move when the press lands on the title bar, or anywhere in the panel with
// CTRL held — the whole face is a drag handle then, which is the only practical way to move a panel
// whose title bar is behind another one.
bool floating_panel_press(tFloatingPanel * panel, tCoord coord);

// Call from the cursor-position handler. Returns true while it is actually moving something.
bool floating_panel_drag(tFloatingPanel * panel, tCoord coord);

void floating_panel_release(tFloatingPanel * panel);

// Forget the chosen position, so the next open places the panel afresh. For a panel being closed
// that should not remember where it was — none currently, but a "reset window positions" action
// would want it.
void floating_panel_unplace(tFloatingPanel * panel);

// Bring to the front. Called automatically by floating_panel_press() for any press the panel claims,
// so clicking a panel raises it exactly as a window manager would.
void floating_panel_raise(tFloatingPanel * panel);

// Is a in front of b? Hit-test panels in front-to-back order and draw them back-to-front. A panel
// that has never been raised sorts behind one that has.
bool floating_panel_in_front_of(const tFloatingPanel * a, const tFloatingPanel * b);

#ifdef __cplusplus
}
#endif

#endif // __FLOATING_PANEL_H__
