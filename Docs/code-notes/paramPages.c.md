# paramPages.c notes

The longer comments from `paramPages.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `param_ref_for_index()`

A knob assignment names a param by INDEX within its module; the widget to draw is described by
the matching entry in paramLocationList, which is keyed by module TYPE. Walk the list counting
entries for this module's type - the paramIndex'th match is the one, the same correspondence
render_module_common() relies on when it renders a module's params in list order.
module->param[][].paramRef caches this, but only once the module has been drawn at least once,
and a knob can point at a module in the Slot or Location that isn't currently on screen.

## 2. `tKnobMetrics`

What one knob's widget needs horizontally.

Both numbers have to be worked out per param TYPE, because the renderers don't agree on what
rectangle.size.w means. A dial takes it as its diameter and then draws label and value text
left-anchored at the same x, ignoring the width entirely. A toggle ignores it too and sizes its
own button to its strMap. But paramTypeEnable draws a button exactly rectangle.size.w wide with
the param's LABEL inside it - so a param the patch has renamed to something long needs to be
given a rect that wide, or the text simply runs out of the button. On the canvas that never
shows, because adjust_rectangle() hands each param the width its paramLocationList entry
specifies; here the panel is choosing the rectangle, so it has to choose a big enough one.

`content` is the total extent the widget will paint, which matters because nothing clips (there
is no scissor anywhere in SynthLib) - a widget wider than its cell isn't trimmed, it runs into
the next cell and that cell's background then paints over the top of it.

## 3. in `knob_metrics()`

Label on its own line, then a button sized to largest_text_width() over the param's
declared range. Measure the string actually on screen as well: where a module's
declared range is out of step with its strMap (there are known cases - see the
module-verification items in todo.md) the current entry can be longer than
anything largest_text_width() looked at.

## 4. in `render_param_pages_panel()`

Cells are sized to the widest widget on the page rather than to a fixed panel width, and
the panel width follows from them. All eight share one width so the row stays a row, and
the page keeps its size as the mouse moves over it. The panel is as wide as this page needs
and no wider - switching pages can resize it, which is the honest trade for never having a
widget run into its neighbour.

## 5. in `render_param_pages_panel()`

A dial is centred by putting the DIAL on the cell's centre line, but its label and
value are drawn from the dial's left edge rightwards - so everything past the dial has
to fit in the right half of the cell. Asking for that width here is what lets the
centring below actually happen; if the row can't have it, the fallback there keeps the
content inside the cell at the cost of sitting off-centre.

## 6. in `render_param_pages_panel()`

FLOATING, so the position comes from the panel rather than from the window: chosen once on
first show and thereafter wherever the user has dragged it. Centring every frame is what made
a panel impossible to move — it snapped back before the next redraw.

No draw_dialog_background_overlay() either. Dimming the canvas behind is what a MODAL dialog
does, and this is not one: the canvas stays live underneath and stays legible to match.

## 7. in `render_param_pages_panel()`

Where in the cell the widget starts. A dial goes on the cell's centre line; anything
else has its whole painted block centred, which is the closest equivalent for a
widget that is all button. Both fall back to a left-anchored position if centring
would push the content past the cell's right edge - which only happens once the
window is too narrow for the width the sizing pass above asked for.

## 8. in `render_param_pages_panel()`

The param widget itself, drawn by exactly the code the canvas uses - so a dial
looks like a dial, a toggle looks like a toggle, and the value text is formatted
by that param type's own rule. It RETURNS the clickable rect it registered, which is
what this keeps for its own hit-testing — so there is still only ever one description
of where the control is, without a round trip through gParamRectangle to fetch back
something the call just computed.
Dials and sliders take the rect as the control itself and draw their text upwards,
so they start two text rows down. Toggles, menus and Enable buttons still draw
downwards from the rect, so they start at the top of the widget block and their
button lands on the same line as a dial's value.
