# patchWrite.c notes

The longer comments from `patchWrite.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

Writing the patch database back out as a .pch2 / .prf2 file.

SPLIT OUT OF graphics.c on 2026-09-09 so the VST3 plug-in can save. Both functions are pure
serialisation — database in, file out — with no GLFW, no window and no device in them; they only
ever lived in graphics.c because that is where the file-browser callback that calls them lives.
The plug-in does not compile graphics.c (it has its own render loop in vst3/g2Draw.c), so File >
Save there consumed its own message and did nothing.

The application's on_file_saved() still owns the POLICY around a save — online vs offline, the
recent-files list, the remembered path. Only the bytes-to-disk half is here.
