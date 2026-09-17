# g2Prefs.h notes

The longer comments from `g2Prefs.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `G2_PREFS_APP_NAME`

Settings the plug-in remembers between sessions, through the same SynthLib prefs store the
application uses (prefs.h) — but under its OWN name, so it gets its own file.

NOT SHARED WITH THE APPLICATION'S, deliberately. prefs.cpp rewrites the whole file on a change, so
two processes writing the same one — and the standalone editor and a hosted plug-in are very
likely to be open together — would let a last-writer-wins clobber quietly lose settings. A shared
dial-mode preference would be a nice touch; it is not worth that.

THE ONE SHARED SETTING (2026-09-17, CT): the file browser's last folder is read from and written to
G2-Edit's own prefs.txt, one key at a time (`prefs_set_string_in()` / `prefs_get_string_from()`), and
asked for afresh whenever the browser opens. SynthLib's store now writes only the keys a process has
set, over the file as it stands, so the application saving its zoom no longer undoes the plug-in's
folder, or the reverse.
