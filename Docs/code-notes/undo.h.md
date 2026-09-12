# undo.h notes

The longer comments from `undo.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `undo_begin_cable_edit()`

Cable-chain edits are recorded as a before/after snapshot of every cable in one location
rather than as a list of individual changes: a single command (Disconnect especially) can
delete, re-create AND recolour cables at once, and the set it touches is only known by
walking the chain. Snapshotting the location sidesteps all of that, and a patch's cable
count is small enough that it costs nothing worth counting.

Bracket the edit with begin/commit. Commit pushes nothing if the cables came out unchanged,
so a command that turns out to be a no-op leaves no undo entry behind. Nesting is not
supported — a begin while one is already open is ignored, which keeps the outermost
bracket authoritative when one cable operation is built from others.

## 2. `undo_push_create_module()`

Record an Add Module, AFTER the module exists — the snapshot is what redo puts back. displaced/
displacedCount carry the modules the new one pushed down its column, as for undo_push_paste; here
redo needs them too, since it puts the module back at its recorded position rather than re-running
the shift that produced it.
