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
// Notes: Docs/code-notes/splitView.h.md - "// notes §k" refers there.

#ifndef SPLIT_VIEW_H
#define SPLIT_VIEW_H

#include "types.h"
#include "synthlibTypes.h"

// notes §1

#define SPLIT_BAR_HEIGHT    (11.0)      // the reference uses 0xb
// Breathing room either side of the bar, so modules never butt straight up against it — the same
// courtesy MODULE_MARGIN already gives between the top bar and the canvas, and the reason the bar's
// total vertical footprint is larger than the bar itself.
#define SPLIT_BAR_GAP          (MODULE_MARGIN)
#define SPLIT_BAR_FOOTPRINT    (SPLIT_BAR_HEIGHT + (SPLIT_BAR_GAP * 2.0))
#define SPLIT_MIN_PANE         (40.0)   // a pane is either collapsed to nothing or at least this tall
#define SPLIT_POS_MAX          (0x3fff) // the field is 14 bits, as the reference's setter clamps it

typedef struct {
    uint32_t   focusedPane;       // 0 or 1 — the pane clicks and the scrollbars act on

    // notes §2
    uint16_t   restorePosition[MAX_SLOTS];

    tRectangle barRect;           // the draggable bar, window coordinates
    tRectangle upButton;          // give the TOP pane everything
    tRectangle downButton;        // give the BOTTOM pane everything
    tRectangle restoreButton;     // back to the remembered position
    bool       dragging;
    bool       dirty;             // divider moved — push to the device on mouse-up, not per frame
} tSplitView;

extern tSplitView gSplitView;

void split_view_init(void);

// Pushes the current split into SynthLib's pane extents and count. Called every frame before
// anything is drawn, so a window resize or a divider drag lands before the first render call.
void split_view_apply(void);

uint32_t split_view_location_for_pane(uint32_t pane);
uint32_t split_view_focused_pane(void);

// The pane whose rectangle contains coord, or -1 if the point is outside every pane (the bar
// itself, the scrollbars, the top bar).
int32_t split_view_pane_at(tCoord coord);

// Focuses the pane under coord, if any, and points gLocation at its Location. Returns true when
// the focus actually moved, so a caller can redraw.
bool split_view_focus_at(tCoord coord);

// Give one Location the whole window — the VA / FX topbar buttons and the bar's own arrows. This is
// the divider slammed to an end, not a separate mode.
void split_view_show_full(uint32_t location);

// True when that Location currently has the whole window (the other pane collapsed).
bool split_view_is_full(uint32_t location);

// Bring both areas back into view, at the last position that had them both — or an even split if
// the patch arrived with one collapsed.
void split_view_restore_balance(void);

// Put the divider at an explicit Voice Area height in pixels, clamped as a drag would be.
void split_view_set_position(double pixels);

// Sends the divider to the G2 if it has moved. barPosition is patch data, so a move is a patch
// edit; this is called once at the END of a gesture, never per frame of a drag.
void split_view_flush_position(void);


// notes §3
void pane_scroll_by(uint32_t pane, double dxPixels, double dyPixels);

// The zoom at which every module in every visible pane is on screen at once — the smallest zoom any
// pane asks for, since one zoom serves both. Returns the current zoom for a patch with nothing in it.
double split_view_zoom_to_fit(void);

// Each pane scrolled so its own leftmost module is against its left edge and its topmost module
// against its top. Pairs with the above, and must be called AFTER the new zoom is set.
void split_view_scroll_to_content(void);

void render_pane_scrollbars(void);
bool handle_pane_scrollbar_click(tCoord coord);
void handle_pane_scrollbar_drag(tCoord coord);
bool pane_scrollbar_dragging(void);
void pane_scrollbar_release(void);

void render_split_bar(void);
bool handle_split_bar_mouse(tCoord coord, tMouseButton mouseButton);
void handle_split_bar_cursor_pos(tCoord coord);

#endif /* SPLIT_VIEW_H */
