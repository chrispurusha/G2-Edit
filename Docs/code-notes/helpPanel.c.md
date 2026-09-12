# helpPanel.c notes

The longer comments from `helpPanel.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `kLeftColumn`

EVERY ROW HERE IS A CLAIM ABOUT THE CODE. Where each comes from, so the next person changing a
binding knows which file to check: the canvas and shortcut rows are mouseHandle.c (key_event,
mouse_button, scroll_event) and canvasDrag.c (the gesture table); the note-entry rows are
virtualKeyboard.c (note_offset_for_key and handle_note_entry_key); the panel rows are
floatingPanel.c; the variation rows are mouseTopbar.c and protocol.c's fan-out.
