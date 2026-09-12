# splitView.h notes

The longer comments from `splitView.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `SPLIT_BAR_HEIGHT`

The Patch Window Split Bar — the original editor shows the Voice Area and the FX Area at the same
time in one window, divided by a drag-resizable horizontal bar (see todo.md). This owns which
areas are on screen, how the canvas band is divided between them, and which one has focus.

It sits on top of SynthLib's module PANES (utilsGraphics.h): a pane owns a scroll position and a
slice of the canvas band, and this decides how many panes there are, how big each slice is, and
which Location each pane displays. SynthLib knows nothing about Voice/FX.

THE DIVIDER IS ALWAYS ON SCREEN. There is no separate "one area full height" mode: a full-height
Voice Area is just the divider pushed hard to the bottom, and a full-height FX Area is the
divider at the top. That is how the original behaves, and collapsing the two states into one
position removes a mode — the VA and FX topbar buttons simply slam the divider to an end, which
is also what the bar's own up/down arrows do.

THE POSITION IS PATCH DATA, NOT APP STATE. It lives in gPatchDescr[slot].barPosition, the 14-bit
field protocol.c has always parsed and written but nothing ever read, a value clamped to
0..0x3fff.
So it round-trips to file and to the G2 for free, and each Slot remembers its own split.

IT IS MEASURED IN PIXELS — specifically the VOICE AREA pane's height. The reference's layout code
adds it to the toolbar and scrollbar heights to get the top pane's bottom edge, then clamps. That
is why a window resize keeps the top pane's height and lets the BOTTOM absorb the change, rather
than redistributing proportionally: it is what the original does, and storing pixels means an
exact round-trip instead of one that depends on the window size at the time.

FOCUS is what gives the rest of the app a single answer to "which Location am I editing?". Only
one pane is focused at a time; clicking in a pane focuses it and points gLocation at its
Location, so every existing gLocation reader keeps working without knowing panes exist. A pane
collapsed to nothing cannot be focused, and focus moves off it automatically.

## 2. file scope

WHERE THE DOUBLE-ARROW PUTS THE DIVIDER BACK TO — the manual's "previous split position" —
and PER SLOT, because the thing it remembers is per slot. barPosition lives in
gPatchDescr[slot], so it is patch data: each of the four slots has its own divider, and a
single app-wide value handed slot B the position last seen in slot A.

## 3. `pane_scroll_by()`

One vertical scrollbar per visible pane, replacing the window's single shared one — two panes
scroll independently, so one thumb could only ever be right about one of them.
Scrolls one pane by a number of content pixels, relative to its own current position. Used by the
wheel and by the drag-past-the-edge auto-scroll; both used to share a single accumulator, which
made scrolling one pane yank the other to the same place.
