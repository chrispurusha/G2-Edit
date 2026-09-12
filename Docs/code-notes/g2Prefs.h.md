# g2Prefs.h notes

The longer comments from `g2Prefs.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `G2_PREFS_APP_NAME`

Settings the plug-in remembers between sessions, through the same SynthLib prefs store the
application uses (prefs.h) — but under its OWN name, so it gets its own file.

NOT SHARED WITH THE APPLICATION'S, deliberately. prefs.cpp rewrites the whole file on a change, so
two processes writing the same one — and the standalone editor and a hosted plug-in are very
likely to be open together — would let a last-writer-wins clobber quietly lose settings. A shared
dial-mode preference would be a nice touch; it is not worth that.
