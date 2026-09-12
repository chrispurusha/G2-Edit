# canvasCoords.h notes

The longer comments from `canvasCoords.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `convert_mouse_coord_to_module_area_coord()`

Canvas coordinate arithmetic, with no window system in it.

This lived in mouseHandle.c, which is the most GLFW-bound file in the project — but the maths
itself only ever needed module_area(), the scroll offsets and the zoom factor, none of which know
what a window is. Splitting it out lets the VST3 plug-in convert a mouse position the same way the
application does, instead of keeping a second copy that could drift.

Takes a coordinate already in the canvas's logical units (what get_global_gui_scaled_mouse_coord()
produces) and returns the position within the scrolled, zoomed module area.
