# g2Patch.h notes

The longer comments from `g2Patch.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tG2FileKind`

A .pch2 OR a .prf2 - what File > Open and a restored project both go through. A patch goes into
`slot`; a performance fills all four slots, selects the slot it was saved with and turns
performance mode on. Either way the file's path is recorded as the one Save writes back to
(gSavedPatchPath[slot] or gSavedPerfPath) and the slot names come from it, exactly as the
application's offline loader does (graphics.c).
