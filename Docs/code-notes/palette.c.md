# palette.c notes

The longer comments from `palette.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

The drag-on module palette. Docs/module-palette-design.md carries the design and why it looks the
way it does; the short version is that the instrument's own toolbar uses UNIFORM tiles rather than
scaled-down module faces, and the module heights say why it has to — 94 of 170 modules are two
rows, but Operator is twelve, and no single scale suits both.

The band is an EXTENSION of the topbar rather than a mode that swaps its contents. A mode would
hide the patch name, the voice count and the Patch Load meters exactly while modules are being
added, and the manual ties adding modules to watching those meters. Growing the bar hides nothing
and costs canvas only while the palette is open.

## 2. `PALETTE_GROUP_Y`

THE BAND'S PADDING IS SYMMETRIC, and these four have to be changed together to keep it so: the
gap above the group grid, between the grid and the tiles, and below the tiles are all 5 px, which
makes PALETTE_BAND_HEIGHT 5 + (2 rows x 17) + 5 + 26 + 5 = 74. Two rows of PALETTE_GROUP_H plus
one pixel between them is the 34 in the middle.

## 3. `PALETTE_SWATCH_W`

The colour NEW modules are created in, and the swatches that set it. The instrument does the same
thing (manual p.61: "the color selector stays in its new selection, causing any new modules you
add to the Patch window to get the selected color"), and the band has the room for it - the group
grid leaves the whole right-hand half of its own row empty. 0 is the standard grey.
A swatch is exactly as TALL as a group button and sits on the same two rows, so the two blocks
read as one piece of furniture rather than as a grid with something small parked beside it. Its
row position is derived from PALETTE_GROUP_H rather than from its own height for that reason:
the two cannot drift apart when one of them is changed.

## 4. `PALETTE_SWATCH_COLS`

TWELVE, so each hue's four shades occupy the SAME four columns on both rows - red, green and blue
above; yellow, purple and cyan below - and the grades line up the way the right-click menu's grid
does. The standard grey then falls at the END of the second row rather than leading the first,
where it pushed every hue one column along and broke the alignment (CT, 2026-09-08).

## 5. `PALETTE_DOUBLE_CLICK_MS`

Double-click a tile and the module is added below the focused one, without a drag. The manual
offers it as a first-class alternative ("you could also double-click a module icon to
automatically add it to the Patch window below the currently focused module", p.81), and it is
the only route that works when the target is off-screen or the hand is not steady.

## 6. in `draw_tile()`

A TILE IS DRAWN IN THE COLOUR THE MODULE WOULD BE CREATED IN, so picking a swatch previews
itself across the whole row rather than only on the drag ghost. Hover is then a black frame
rather than a paler fill, which would have thrown that colour away exactly when the pointer
is on the tile you are about to take.

## 7. in `palette_render()`

render_text's coord.y is the TOP of the text, not its baseline - draw_button_split() is
the proof, offsetting its text rectangle by the button margin from the button's own top.
Both labels here were first written as though it were a baseline, which drew every group
name below its own button and through the row beneath it.

## 8. in `palette_render()`

TILES SHRINK TO FIT BEFORE THEY SCROLL. Osc is the largest group at 19, and nineteen tiles at
the full width run off the right of the default window - so the width is divided by the count
and only FLOORED, not fixed. Below that floor the names stop being readable, and at that point
the row scrolls instead, with arrows at the ends to say so.

## 9. in `palette_render()`

THE GHOST IS A REAL MODULE FACE - dials, buttons, connectors and all - not an outline and not
the tile. It is built by module_prototype(), the same defaults create_module_at() lays down,
so what follows the cursor is exactly what the drop will produce, drawn at the canvas's own
zoom in the column and row it would occupy.

CLICK REGISTRATION IS SUPPRESSED WHILE IT DRAWS. render_module() and every widget under it
register click regions keyed by the module's slot/location/index, and a ghost has none - it
would be registering regions for a module that does not exist, on top of whichever real one
owns that index. An EMPTY click clip is the seam for that: register_click_region() drops
anything that falls outside the clip, so nothing drawn here can be clicked.

## 10. in `palette_render()`

OVER THE BAND ITSELF - which is where every drag STARTS - there is no pane under the
cursor, and a plain outline was drawn there instead. That left the first moments of every
drag showing a rectangle with none of the module's controls in it (CT, 2026-09-07: "the
components aren't in the module representation"). So the ghost goes straight into the
FOCUSED pane at the top of the cursor's own column and is a full face from the outset;
once the cursor enters a pane it tracks the drop position exactly.

## 11. in `palette_render()`

CLICK REGISTRATION IS SUPPRESSED WHILE IT DRAWS. render_module() and every widget under
it register regions keyed by the module's slot/location/index, and a ghost has none - it
would be registering regions for a module that does not exist, on top of whichever real
one owns that index. An EMPTY click clip is the seam: register_click_region() drops
anything falling outside the clip, so nothing drawn here can be clicked.

## 12. in `palette_left_down()`

A swatch does two things, as the instrument's own colour selector does: it sets
the colour NEW modules get, and it recolours whatever is selected right now
(manual p.61). With nothing selected only the first applies, so clicking a swatch
to set up the next few modules never repaints anything by surprise.

## 13. in `palette_left_down()`

THE GHOST IS UP FROM THE MOMENT THE BUTTON GOES DOWN, with no movement threshold first.
A slop distance was tried and taken out: it exists to stop a click being mistaken for a
drag, but there is nothing else a press on a tile can mean here, and it left the first
few pixels of every drag showing nothing at all. A press that never reaches a module area
is simply abandoned on release, which costs the user one frame of ghost and no more.

## 14. in `palette_left_up()`

THE DROP LANDS IN THE PANE UNDER THE CURSOR, not the focused one. Everything downstream -
module_area() inside convert_mouse_coord_to_module_column_row(), the scroll offsets it
adds, and create_module_at()'s own use of gLocation - reads the FOCUSED pane, so focusing
the pane being dropped on is what makes all three agree. Without it every drop landed in
whichever half had focus: dragging onto the Voice Area created the module in the FX Area.
WHETHER THE COORDINATE IS IN A PANE IS split_view_pane_at()'S ANSWER, NOT
split_view_focus_at()'S. focus_at() returns whether the focus MOVED, so dropping into the
pane that already had focus returned false - and this treated that as "outside a pane"
and threw the drop away. Every drop into the already-focused half silently did nothing,
which is most of them (CT, 2026-09-07: "Dropping a new module isn't working").
