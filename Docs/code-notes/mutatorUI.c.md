# mutatorUI.c notes

The longer comments from `mutatorUI.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `MUTATOR_RGB_SINGLE_BACK`

Per-role palette matching the original editor's Mutator dialog: Mother and Children in one colour
pair, Father in another, the Patch Variations row and Temporary Storage ("Gene Bank") in their own.

## 2. `gPendingCommitGenome`

─── Commit to a real variation ──────────────────────────────────────────────
Cmd-click on a Patch Variation box permanently writes the focused box's genome into that real
variation (undoable), unlike a plain click which only loads/auditions. This is the only way
anything from the Mutator becomes permanent - a stand-in for the Temporary Storage row's "v"
(commit row) button until that's built.

## 3. `draw_chromosome()`

Fits mutator_chromosome_path()'s turtle-walk path into rect (uniform scale, aspect preserved,
centered, as the original editor's Mutator window draws it) and draws it as a
connected polyline. Not a single bezier: the path can have hundreds of points, and
render_bezier_curve() only draws one curve through three control points.

## 4. in `render_mutator_panel()`

Temporary Storage: 24 slots (3 rows of 8). Click an empty slot to save the focused genome
there; click a saved slot to load it as Mother; right-click to clear. Drag onto Father to
load it there instead (quietly, no audition) - Shift/Cmd-drag = Interpolate/Cross. Split
across two lines - the full sentence is wider than the panel at its default size.

## 5. in `render_mutator_panel()`

Patch Variations row: mirrors the 8 real hardware variations. Click loads that variation as
Mother; Cmd-click, or a plain drop here, commits (with confirmation, since it's a real write
to the edit buffer). Also a full drag source/destination like every other box - drag = copy
(drag onto Father loads it quietly instead), Shift-drag = Interpolate, Cmd-drag = Cross.

## 6. `tMutatorPendingKind`

─── Click-and-drag model ────────────────────────────────────────────────────
All discrete controls arm on mouse-down and only fire on mouse-up if the release is still over
the same control (standard button behaviour - drag off before releasing cancels). Mother/
Children/Father, Temporary Storage, and Patch Variation boxes all support drag-and-drop between
any two of them, per the manual: plain drag = copy (a plain drop onto a Variation commits there,
with confirmation, since that's a real hardware write), Shift-drag = Interpolate, Cmd-drag =
Cross. Same-box press-then-release (i.e. a plain click, no drag) keeps each box's own click
meaning instead - notably Cmd-click on a Variation still means "commit the focused sound here",
distinct from Cmd-drag *onto* that same Variation meaning Cross.

## 7. `tBoxFamily`

boxFamVariation participates fully in the drag family now: a plain click still does the old
load-Mother/Shift-Father/Cmd-commit thing (click_drag_box), but it can now also be dragged onto
any other box (as a source) or dropped onto (as a destination) just like Mother/Children/Father
and Temporary Storage.

## 8. `box_ref_genome()`

For boxFamVariation this reads the variation's live values on demand into a shared scratch
buffer - safe because callers that need to keep a source's values across a later mutation of
gMutator state (see drop_drag_box) copy out of this buffer immediately, before it can be
overwritten by a second call for the destination side.

## 9. `draw_drag_ghost()`

While a box-drag is armed (mouse down on a box, not yet released), draw a small floating
chromosome preview next to the cursor - mirrors a native drag's "ghost", but drawn as the
dragged genome's own sparkline so it also hints at what's being carried, not just that
something is.

## 10. `click_drag_box()`

The plain-click behaviour for a Mother/Child/Father, Temporary Storage, or Patch Variation box
(i.e. pressed and released on the very same box, or a Storage box being used as a drag source
with nothing in it). Shift-click-to-Father and Cmd-click-to-clear were dropped once drag/right-
click covered the same ground: dragging onto Father now loads it just as quietly (see
drop_drag_box), and right-click clears any box (see clear_box).

## 11. `drop_drag_box()`

A genuine drag from one box to a *different* box. Modifier held at release decides the
operation, matching the manual's mouse shortcuts table. The source is snapshotted into a local
buffer up front, since box_ref_genome's boxFamVariation case reads through a single shared
scratch buffer that a second call (for the destination side) would otherwise overwrite.

## 12. in `handle_mutator_mouse()`

Continuous drag controls (panel title bar, dials) still act immediately on press -
they aren't discrete "click" actions.
Title bar, or anywhere on the panel with Ctrl held — the shared rule, which also keeps the
close button out of the drag handle it sits inside.
