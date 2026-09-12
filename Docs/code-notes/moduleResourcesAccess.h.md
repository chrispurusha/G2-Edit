# moduleResourcesAccess.h notes

The longer comments from `moduleResourcesAccess.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `effective_connector_type()`

A module running at the higher (audio) bandwidth promotes its Control connectors to Audio and
its Logic connectors to TurboLogic (orange) — see render_connector_common()'s own comment
(moduleGraphics.cpp) for the manual references this is confirmed against.
Audio connectors are never affected; upRate has no effect when false. Shared by both the
connector-hole rendering itself and cable-creation's "inherit the source connector's current
colour" logic (mouseHandle.c), so the two can never disagree.
