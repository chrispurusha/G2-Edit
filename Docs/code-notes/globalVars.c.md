# globalVars.c notes

The longer comments from `globalVars.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `gDefaultDocument`

── The documents ───────────────────────────────────────────────────────────────────────────────

THE APPLICATION'S ONE DOCUMENT, zero-filled like the globals it replaced, and every thread's
current document from the moment it starts. The few defaults that are not zero go in at load time
(below) rather than in an initialiser: a designated initialiser on a ~90 MB struct would put the
whole of it in the binary as initialised data instead of leaving it as zero-fill.

## 2. `variation_is_linked()`

── Linked variations ───────────────────────────────────────────────────────────────────────────

One bit per variation, one mask per slot — see the note on variation_is_linked() in globalVars.h.
A mask rather than an array of bools because "is the group empty" and "clear it" are then a single
comparison and a single store, which is what most of the callers actually ask.

## 3. `stop_patch_name_editing()`

The drag reference points (gDragStartX/Y, gDragPrevX/Y) are document fields now - see globalVars.h.
Shared between canvasDrag.c's parameter dragging and mouseHandle.c's tempo, vibrato and glide drags,
which difference against the same two points: the start fixed at the press, the previous advancing
with each event.
