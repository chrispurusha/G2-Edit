# main.c notes

The longer comments from `main.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. in `main()`

Give every slot a default patch (including an activated, source-assigned morph module — see
init_patch()) before the first frame renders, rather than leaving the database's zeroed/
inactive startup state on screen until a G2 connects and sends real patch data. If a
connection succeeds shortly after, send_init_sequence_pull()'s real data simply overwrites
this placeholder per slot, same as loading over a manually-created New Patch would.

## 2. in `main()`

BEFORE init_graphics(), and the order is load-bearing. The window is built differently for
each render backend — OpenGL needs a GL context created alongside it, Metal needs none — so
synthlib_window_create() reads the saved choice before it makes the window. prefs_init() also
runs from setup_main_menu() below, where it always did; it clears and re-reads, so calling it
twice is harmless and nothing has written a preference in between.
