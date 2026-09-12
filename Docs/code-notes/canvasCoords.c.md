# canvasCoords.c notes

The longer comments from `canvasCoords.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `canvas_zoom_step_at()`

── Zoom, stepped ───────────────────────────────────────────────────────────────────────────────

One Cmd +/- worth of canvas zoom, anchored at the module area's top-left and remembered in prefs.
Shared because both shells offer the same shortcut and neither should own the arithmetic: the
application had these four lines written out twice in its key handler (once per direction), and the
plug-in would have made a third and fourth copy.
The ANCHOR is what the two callers disagree about and nothing else: Cmd +/- has no meaningful
position so it uses the module area's corner, while Cmd + wheel zooms around the pointer, which is
what makes zooming feel like it is aimed at something.
