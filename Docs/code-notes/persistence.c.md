# persistence.c notes

The longer comments from `persistence.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

── RECENT FILES ──────────────────────────────────────────────────────────────────────────────

The File > Open Recent list. Most-recent first, capped at RECENT_FILES_MAX, persisted one prefs
key per slot so the list survives a restart the way every other editor's does.

PATHS, NOT NAMES. The menu shows each file's basename because that is what a menu of files should
read like, but what is stored and what is opened is the full path — two patches called "Lead" in
different folders are different files, and a list keyed on the name would conflate them.
