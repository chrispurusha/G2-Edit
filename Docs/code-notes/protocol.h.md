# protocol.h notes

The longer comments from `protocol.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `send_param_value_to_links()`

Call after send_param_value() at a USER edit, with the same arguments: repeats the write into
every linked variation. Discrete edits (toggles, dropdowns, a scroll step) call it as they happen;
a drag calls it once on release. See the definition in protocol.c for why both, and for why bulk
tools, undo/redo and the variation-copy commands deliberately do NOT call it.
