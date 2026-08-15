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

#ifdef __cplusplus
extern "C" {
#endif

#include "defs.h"
#include "types.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "synthlibGlobals.h"
#include "inputState.h"   // ctrl_modifier_held() — ctrl turns the whole panel into a drag handle
#include "floatingPanel.h"

// Each newly placed panel is offset from the last so a second one does not land exactly on top of
// the first. Centring them all — which is what the modal versions did — is the one placement that
// guarantees they hide each other, and the complaint that started this work was precisely that these
// panels take the whole screen.
#define PANEL_CASCADE_STEP    (28.0)
#define PANEL_CASCADE_WRAP    (6)

static uint32_t sCascade  = 0;

// Monotonic, so "most recently raised" is simply the largest. Never reset: at one raise per click it
// would take longer than any session to wrap a uint32_t.
static uint32_t sTopOrder = 0;

void floating_panel_raise(tFloatingPanel * panel) {
    sTopOrder++;
    panel->order = sTopOrder;
}

bool floating_panel_in_front_of(const tFloatingPanel * a, const tFloatingPanel * b) {
    return a->order > b->order;
}

tRectangle floating_panel_place(tFloatingPanel * panel, double width, double height) {
    double renderW = get_render_width() / gGlobalGuiScale;
    double renderH = get_render_height() / gGlobalGuiScale;

    panel->rect.size = (tSize){
        width, height
    };

    if (panel->placed == false) {
        double step = (double)(sCascade % PANEL_CASCADE_WRAP) * PANEL_CASCADE_STEP;

        panel->rect.coord = (tCoord){
            ((renderW - width) / 2.0) + step, ((renderH - height) / 2.0) + step
        };
        panel->placed     = true;
        sCascade++;
        floating_panel_raise(panel);   // a newly opened panel opens in front
    }
    // Re-clamp every frame, not just on placement: the window can be resized under a panel that was
    // dragged to an edge, and a panel parked entirely off-screen cannot be dragged back.
    double maxX    = renderW - PANEL_CASCADE_STEP;
    double maxY    = renderH - PANEL_CASCADE_STEP;

    if (panel->rect.coord.x > maxX) {
        panel->rect.coord.x = maxX;
    }

    if (panel->rect.coord.y > maxY) {
        panel->rect.coord.y = maxY;
    }

    // Left/top are clamped to keep the TITLE BAR reachable rather than to keep the whole panel on
    // screen: a wide panel may legitimately hang off the right, but a title bar above the window top
    // is unreachable and the panel is then stuck for good.
    if (panel->rect.coord.x < (-width + PANEL_CASCADE_STEP)) {
        panel->rect.coord.x = -width + PANEL_CASCADE_STEP;
    }

    if (panel->rect.coord.y < 0.0) {
        panel->rect.coord.y = 0.0;
    }
    return panel->rect;
}

bool floating_panel_contains(const tFloatingPanel * panel, tCoord coord) {
    return within_rectangle(coord, panel->rect);
}

bool floating_panel_press(tFloatingPanel * panel, tCoord coord) {
    bool onTitle = within_rectangle(coord, panel->titleBarRect);
    bool ctrlAny = ctrl_modifier_held() && within_rectangle(coord, panel->rect);

    // Raise on ANY press the panel claims, not just one that starts a move — clicking a key or a
    // knob on a partly covered panel should bring it forward, which is what makes two overlapping
    // panels usable at all.
    if (within_rectangle(coord, panel->rect)) {
        floating_panel_raise(panel);
    }

    if (!onTitle && !ctrlAny) {
        return false;
    }
    panel->dragging       = true;
    panel->dragMouseStart = coord;
    panel->dragPanelStart = panel->rect.coord;
    return true;
}

bool floating_panel_drag(tFloatingPanel * panel, tCoord coord) {
    if (panel->dragging == false) {
        return false;
    }
    panel->rect.coord.x = panel->dragPanelStart.x + (coord.x - panel->dragMouseStart.x);
    panel->rect.coord.y = panel->dragPanelStart.y + (coord.y - panel->dragMouseStart.y);
    synthlib_request_redraw();
    return true;
}

void floating_panel_release(tFloatingPanel * panel) {
    panel->dragging = false;
}

void floating_panel_unplace(tFloatingPanel * panel) {
    panel->placed = false;
}

#ifdef __cplusplus
}
#endif
