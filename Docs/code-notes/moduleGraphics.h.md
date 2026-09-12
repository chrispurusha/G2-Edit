# moduleGraphics.h notes

The longer comments from `moduleGraphics.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `param_is_under_cursor()`

Draws one module parameter - dial, slider, toggle or menu button, whichever the param's type
calls for - and registers its clickable rect as a click region. Used by render_module() for the
patch canvas and by the Parameter Pages panel, which draws the same widget somewhere else; see
set_param_render_area() (renderParams.h) for switching which area it renders into.
Is this parameter the widget under the given coordinate? Asks the click-region registry, so the
answer matches where a click would land — see the definition.

## 2. `eCanvasWidgetKind`

── What is under the cursor on the canvas ───────────────────────────────────

Every canvas widget registers a click region carrying one of the context structs below, and every
one of those begins with this same pair. That is what lets a caller ask the registry "what is
here?" and then act on the answer, instead of walking the app's own rectangle arrays — which is a
second description of where the widgets are, free to disagree with the registry about z-order.

The shared prefix is the sockaddr idiom: a pointer to any of the contexts may be read as a
tCanvasWidget * to get its kind and its module, because C guarantees the layout of a common
initial sequence. Add a new canvas widget kind and you MUST give its context the same two leading
members, in this order.

## 3. `canvas_widget_at()`

The canvas widget under this coordinate, or NULL. Front-to-back through the click-region registry,
so the answer is the same widget a click at that point would reach.

RESTRICTED TO eClickLayerCanvas on purpose. The morph dials register at eClickLayerPanel and are
not part of the scrolling canvas, so a caller that means "which module widget is the pointer over"
must not get one; and nothing outside this file registers a canvas region, which is what makes
reading the tag off the returned pointer safe.
