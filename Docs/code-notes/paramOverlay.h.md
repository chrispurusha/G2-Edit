# paramOverlay.h notes

The longer comments from `paramOverlay.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tParamOverlayMode`

The original editor's F5-F8 / Ctrl+F8 "yellow popup box" views: a small label pinned under
EVERY parameter on the canvas at once, each mode showing a different thing about it. G2-Edit
reaches them from the View menu rather than the function keys.

Deliberately NOT in SynthLib. Every mode below is about a G2 module parameter - morph groups,
the 120 Parameter Page knobs, the patch's controller table - none of which exist in the other
two apps.

The modes are mutually exclusive, as they are in the original: choosing one replaces whatever
was showing. overlayModeNone restores the pre-existing behaviour, where a knob/CC label appears
only for the single parameter under the mouse.

## 2. `param_overlay_note_param()`

Called from render_param_common() for every parameter it draws, with the rectangle the widget
occupies and the display string the widget just rendered ("554.4Hz", "0.0", ...) - the caller
already has that in hand, which saves this module re-deriving per-type formatting it has no
business knowing. May be NULL or empty for types that show a strMap entry instead of a number.
Decides for itself whether this parameter gets a row in the current mode, so callers need no
knowledge of the modes.

## 3. `param_overlay_render_pane()`

Paints everything queued this frame. Must run after the whole canvas is drawn, or later modules
paint over the labels.
Draws the chips queued from one module pane, and must be called from inside that pane's transform
and its scissor — the rectangles are in the pane's own coordinate space.
