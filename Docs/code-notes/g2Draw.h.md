# g2Draw.h notes

The longer comments from `g2Draw.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `g2_draw_enter()`

One-off GL state and the font atlas. Must be called with the context CURRENT — building the glyph
textures is a GL operation, and doing it without a context silently produces a font that draws
nothing.
Makes `doc` (a tG2Document *, from g2Plugin.c) the calling thread's current document. The editor
view calls it before every frame and every event, since AppKit calls it and not the plug-in.

## 2. `g2_draw_frame()`

Draw one frame into the current context.

Dimensions are PHYSICAL pixels and backingScale is how many of them make a point; the caller has
already resolved both, because asking for them is a platform question and this file is
deliberately not part of the platform. The renderer works in logical points, so the scale is what
connects the two.
