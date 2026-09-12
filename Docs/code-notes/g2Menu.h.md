# g2Menu.h notes

The longer comments from `g2Menu.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `G2_PLUGIN_TOPBAR_HEIGHT`

Height reserved above the canvas for the topbar that is not built yet — variation buttons, patch
name, voice count, patch volume and the cable view toggles. Reserved NOW so the canvas is laid out
around it from the start; adding it later would otherwise shift the whole patch down at that
point. The application's own TOP_BAR_HEIGHT is 80; this is smaller because the slot, performance
and clock controls that fill much of the app's bar have no meaning in a plug-in.
The application's own bar height, because it IS the application's bar — render_top_bar() lays
itself out against TOP_BAR_HEIGHT, so anything else here would clip it.
