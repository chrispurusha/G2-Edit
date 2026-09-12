# splitView.c notes

The longer comments from `splitView.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `note_restore_position()`

REMEMBER ANY BOTH-PANES-VISIBLE POSITION WE SEE, wherever it came from.

set_bar_position() is not the only way the divider moves, and recording only there was the bug:
a patch carries its own barPosition and is parsed straight into gPatchDescr (protocol.c), a new
patch is created with a default (dataBase.c), and the device can move it behind us. So after a
fresh load, collapsing a pane and pressing the double-arrow went to the MIDDLE instead of back —
and only started behaving once the divider had been dragged by hand at least once. That history
dependence is what read as "inconsistent".

Observing here, where the layout is recomputed, catches every one of those sources without having
to find and hook each of them. A collapsed position is deliberately NOT remembered: "previous
split position" means the last one where both areas were actually visible, which is the only
answer that makes the button useful.

## 2. in `split_view_apply()`

The TOP pane gives up a strip at its foot for its own horizontal scrollbar, PLUS the same gap
the divider gets, so modules never butt straight up against the bar. The bottom pane doesn't
need to reserve anything: the canvas band already stops MODULE_SCROLLBAR_WIDTH + MODULE_MARGIN above
the window bottom, and that reserved strip is exactly where its bar lands once the same gap is
applied below its own foot.

## 3. `split_view_set_position()`

Put the divider at an explicit Voice Area height, in pixels. Clamped and remembered exactly as a
drag is, because it goes through the same set_bar_position(). Added for the backdoor's SPLIT
command: a synthetic drag does not reach the app, so without this there is no scripted way to
frame the canvas for a render check.

## 4. `split_bar_grab_rect()`

The bar is only SV_BAR_H tall, which is a small target and gives no cursor feedback — miss it and
the click lands in a pane and starts a rubber-band select instead, which reads as "the drag does
not work". So the GRAB area is taller than the drawn bar, the usual treatment for a thin
splitter. The buttons are hit-tested first and keep their own exact rects.

## 5. in `render_split_bar()`

Anchored to the BOTTOM pane and given a fixed height, rather than filling whatever space lies
between the two panes. That gap is no longer just the bar: the top pane also gives up a strip
at its foot for its own horizontal scrollbar, so measuring the gap made the bar 26px tall and
drew it straight over that scrollbar. Working back from the bottom pane's top edge keeps the
order — top pane, its scrollbar, gap, bar, gap, bottom pane — and stays correct when the top
pane is collapsed and has no scrollbar at all.

## 6. `PANE_SCROLL_MIN_THUMB`

─── Pane scrollbars ─────────────────────────────────────────────────────────

One vertical bar per visible pane, plus a horizontal bar for the focused pane. The window's old
single shared pair could not do the vertical: two panes scroll independently, so one thumb could
only ever tell the truth about one of them.

THE THUMB IS PROPORTIONAL, which is the thing that makes a scrollbar read as a scrollbar on any
desktop: its length is the fraction of the content you can currently see, so it grows as you zoom
out and shrinks as you zoom in, and its position is that fraction slid along the remaining track.
The old bars drew a fixed-length block that said nothing about how much content there was.

The thumb is also DERIVED from the pane's scroll percent every frame rather than tracked
alongside it, so it cannot drift out of step with a pane scrolled by the wheel or by a zoom.

## 7. `draw_thumb()`

Draws the thumb with ROUNDED ENDS — a body rectangle short by one radius at each end, capped
with a filled circle. Square ends made the thumb look truncated, as though it had been clipped by
the track rather than sitting in it, which is the one detail that stops a scrollbar reading as a
scrollbar. The radius is half the thumb's short side, so the caps are exact semicircles whatever
the bar's width; PANE_SCROLL_MIN_THUMB is comfortably more than one diameter, so the body never
inverts.

## 8. in `render_pane_scrollbars()`

One horizontal bar per pane too. A single shared bar could only ever be right about one of
them — the wheel already scrolls each pane's X independently, so a shared bar would either
misreport a pane or drag them into lockstep and undo what the wheel just did. The reference
does the same: its layout adds a scrollbar's height into the TOP pane's extent.

## 9. `pane_scroll_by()`

Scrolls one pane by a number of CONTENT pixels, relative to where that pane already is.

The wheel and the drag-scroll used to accumulate into gScrollState.xBar/yBar and convert that
into the current pane's percent. That shared accumulator was fine with one canvas and wrong with
two: scrolling pane 0 to 50% and then wheeling over pane 1 snapped pane 1 to ~50% rather than
nudging it from its own position. Reading the pane's own percent back and adjusting it keeps each
pane independent, and expressing the step in content pixels keeps the feel constant as the zoom
and the pane's height change.

## 10. `tContentBounds`

The bounding box of a Location's modules, in module-space units — the same space module positions
are expressed in, before zoom and before scroll. `any` is false for an empty area, in which case
the bounds mean nothing. mod->rectangle is screen-space and deliberately not used here, exactly as
in selection_add_rect().

## 11. `split_view_zoom_to_fit()`

The zoom at which every module in every VISIBLE pane is on screen at once. One zoom serves both
panes — it is a property of the canvas, not of a pane — so the answer is the SMALLEST any pane
asks for: whichever area has to shrink furthest sets the figure, and the other then has room to
spare. A collapsed pane asks for nothing, since no zoom would make it visible.

Fitted to the SPAN of the modules, not to their distance from the canvas origin, and on both axes
rather than just the wider one. A patch parked out at column 4 with nothing to its left is no
bigger than the same patch at column 0, and a tall patch fitted only for width still runs off the
bottom — which is the case this exists for. What makes the span the right measure is that
split_view_scroll_to_content() then puts that span's corner in the corner of the pane.

## 12. `split_view_scroll_to_content()`

Scrolls each pane so its own leftmost module sits against the left edge and its topmost module
against the top, by whatever amount that takes. NOT the canvas origin: a patch built out at column
4 would otherwise be fitted correctly and then left showing four columns of empty grid beside it.
Each pane goes to ITS OWN content, the two areas being laid out independently of each other.

MUST BE CALLED AFTER THE ZOOM IS SET. Scroll positions are held as a percentage of the travel, and
the travel is the canvas extent AT THE CURRENT ZOOM less the pane — so the same percentage means a
different place before and after a zoom change.
