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

#ifndef SPLIT_VIEW_H
#define SPLIT_VIEW_H

#include "types.h"
#include "synthlibTypes.h"

// The Patch Window Split Bar — the original editor shows the Voice Area and the FX Area at the same
// time in one window, divided by a drag-resizable horizontal bar (see todo.txt). This owns which
// areas are on screen, how the canvas band is divided between them, and which one has focus.
//
// It sits on top of SynthLib's module PANES (utilsGraphics.h): a pane owns a scroll position and a
// slice of the canvas band, and this decides how many panes there are, how big each slice is, and
// which Location each pane displays. SynthLib knows nothing about Voice/FX.
//
// THE DIVIDER IS ALWAYS ON SCREEN. There is no separate "one area full height" mode: a full-height
// Voice Area is just the divider pushed hard to the bottom, and a full-height FX Area is the
// divider at the top. That is how the original behaves, and collapsing the two states into one
// position removes a mode — the VA and FX topbar buttons simply slam the divider to an end, which
// is also what the bar's own up/down arrows do.
//
// THE POSITION IS PATCH DATA, NOT APP STATE. It lives in gPatchDescr[slot].barPosition, the 14-bit
// field protocol.c has always parsed and written but nothing ever read — confirmed against the
// reference's NSFile_V11::CPatchHeaderData_11::Get/SetSplitterPos, a short clamped to 0..0x3fff.
// So it round-trips to file and to the G2 for free, and each Slot remembers its own split.
//
// IT IS MEASURED IN PIXELS — specifically the VOICE AREA pane's height. The reference's layout code
// adds it to the toolbar and scrollbar heights to get the top pane's bottom edge, then clamps. That
// is why a window resize keeps the top pane's height and lets the BOTTOM absorb the change, rather
// than redistributing proportionally: it is what the original does, and storing pixels means an
// exact round-trip instead of one that depends on the window size at the time.
//
// FOCUS is what gives the rest of the app a single answer to "which Location am I editing?". Only
// one pane is focused at a time; clicking in a pane focuses it and points gLocation at its
// Location, so every existing gLocation reader keeps working without knowing panes exist. A pane
// collapsed to nothing cannot be focused, and focus moves off it automatically.

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
    uint16_t   restorePosition;   // where the double-arrow puts the divider back to

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

// Sends the divider to the G2 if it has moved. barPosition is patch data, so a move is a patch
// edit; this is called once at the END of a gesture, never per frame of a drag.
void split_view_flush_position(void);


// One vertical scrollbar per visible pane, replacing the window's single shared one — two panes
// scroll independently, so one thumb could only ever be right about one of them.
// Scrolls one pane by a number of content pixels, relative to its own current position. Used by the
// wheel and by the drag-past-the-edge auto-scroll; both used to share a single accumulator, which
// made scrolling one pane yank the other to the same place.
void pane_scroll_by(uint32_t pane, double dxPixels, double dyPixels);

void render_pane_scrollbars(void);
bool handle_pane_scrollbar_click(tCoord coord);
void handle_pane_scrollbar_drag(tCoord coord);
bool pane_scrollbar_dragging(void);
void pane_scrollbar_release(void);

void render_split_bar(void);
bool handle_split_bar_mouse(tCoord coord, tMouseButton mouseButton);
void handle_split_bar_cursor_pos(tCoord coord);

#endif /* SPLIT_VIEW_H */
