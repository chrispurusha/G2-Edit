# g2Prefs.c notes

The longer comments from `g2Prefs.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `synthlib_load_window_and_dial_mode()`

synthlibPersistence.c is NOT linked: its only other job is restoring the WINDOW, and it does that
with glfwSetWindowSize()/glfwSetWindowPos(). A plug-in owns neither — the host places and sizes
the editor, and VST3 offers no way to ask otherwise (the width it reopens at is handled in
g2Editor.mm through getSize()). So the dial-mode half is done here and the window half dropped.
