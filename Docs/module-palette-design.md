# Module palette — design & progress

Living design note. Nothing is built yet; this records the decisions and the numbers
behind them so the work can start without re-deriving any of it. See also `todo.txt`
("Module palette").

## What is being asked for

Users want to add modules by dragging them from a palette onto the patch, as the
original editor does. `todo.txt` has carried "Module palette drag selection" as a
one-liner under USER REQUESTS; this note is that item worked out.

## What the original does

From the G2 manual (p60 and p81):

> "The MODULE GROUP selectors are located in the left section of the Toolbar. Click on
> a selector to select a module group. The currently focused Module Group is shown as a
> blue Module Group selector button."

> "When a Module group is selected all the modules in that group will show up as MODULE
> ICONS under the Module Group selectors. As you move the cursor over each MODULE ICON, a
> preview of the module with its module name is shown. Modules are added to a Patch by
> simply dragging a MODULE ICON to either the Voice Area or the FX Area."

> "…you could also double-click a module icon to automatically add it to the Patch window
> below the currently focused module. The other modules will move, if necessary, when you
> drop a new one."

So: two persistent toolbar rows, hover preview, drag to place, double-click as a no-drag
alternative, and existing modules pushed out of the way on drop.

## Decisions

**1. The topbar switches MODE rather than growing.** The original's toolbar is taller than
ours and can carry the palette permanently. Ours is 104 px of chrome already
(`MENU_BAR_HEIGHT` 24 + `TOP_BAR_HEIGHT` 80) with two dense rows, and a third row would cost
about 44 px of canvas permanently — 6% at the default window, 12% at the 640x360 minimum.
A mode instead costs nothing.

A vertical palette down the side was rejected on geometry: modules are 350 wide and only
38 px per row, so horizontal is the scarce axis. A side palette costs a whole module column
(350 px, a quarter of the default window) to show the same thing.

**2. A RADIO SELECTOR, not a toggle**, sitting in the same place in every mode so it is
always findable. Two modes to start — Patch and Modules — with room for more later; the
Parameter Pages and Parameter Overview panels are menu-only today and are candidates.

**3. Online and Undo/Redo stay visible in every mode.** Undo especially: dropping a module
is among the most undo-prone actions in the editor, and `create_module_at()` already pushes
an undo entry that captures the modules its drop displaced.

**4. The icons are our own module faces, rendered small.** Not pictograms — we have none —
and not text. Since the faces now follow the original's control arrangement, a module's
silhouette carries real information: where its dials sit, where its connectors are, how tall
it is. Legibility at icon size does not matter because the hover preview shows the full-size
face, which is better than the original's small stored icon.

## Layout

Fixed region, identical in every mode (row 1):

    x  20 ..  85   Online + Tx/Rx
    x  95 .. 320   mode radio (~235 px, two or three buttons at ~70)
    x 330 .. 400   Undo / Redo

Everything from x ~410 rightward is mode-variable. In Modules mode:

    row 1, right of Undo/Redo   17 group selectors across ~980 px, ~57 px each
    row 2, FULL width           module icons across ~1370 px

Row 2 can use the full width because its normal content (Patch Mode, patch name, category,
voices, mono) is itself mode-variable.

**Icon scale 0.2x** — 70 px wide, height varying exactly as it does on the canvas, which is
itself information. 19 icons fit row 2, which is exactly the largest group (Osc).

**Heights work because the population is bottom-heavy.** Of 171 modules, 94 are 2 rows and
144 are 4 rows or fewer — 17 px and 34 px at 0.2x, comfortable in a ~40 px row. Only EIGHT
exceed 5 rows: Operator (12), SeqNote and MixFader (9), DrumSynth, Vocoder and SeqVal (8),
and two at 6. Crop those to the row and show the name; the top rows plus the label still
identify them, and the hover preview gives the whole face.

**Group sizes**, from the existing menus: io 11, note 8, osc 19, random 6, lfo 5, env 9,
filter 14, delay 10, level 16, switch 18, seq 5, shaper 7, mixer 16, logic 10, fx 9, midi 8.
Only Osc at 19 is tight.

## What this builds on, rather than adds

- **The 17 groups already exist** in `menus.c` (`ioMenuItems`, `oscMenuItems`, …), driving
  the right-click add-module menu. The palette must use the same list, not a second one.
- **Insert-and-push already works.** `create_module_at()` ends in `shift_modules_down()`, so
  the manual's "the other modules will move when you drop a new one" is implemented and the
  palette drop reuses it.
- **The canvas origin is one value.** `graphics.c` sets `theme.topBarHeight` and
  `utilsGraphics.c` derives the whole module band from it (`bandTop`, `bandHeight`). Nothing
  else needs to know the topbar changed - and in this design it does not change height anyway.
- **In-window panels are precedent**: `helpPanel.c`, `paramPages.c`, the Parameter Overview,
  all on the shared click-region registry.

## The one piece of new plumbing

The renderer takes a `tModule *` and does not care where it came from, but the palette needs
a RENDER-ONLY prototype - a populated module that is never inserted in the database.
`create_module_at()` currently builds the defaults inline (`module_param_count()`, each
mode's own default rather than zero). Factor that into something returning a populated
`tModule` by type, and both callers use it.

## Open questions

- **Patch Load meters.** The manual ties adding modules to watching them ("as you add
  modules… the Patch Load indicator(s) will expand. Maximum Patch Load is 100% per Patch
  Area"), so they arguably should stay pinned in Modules mode - but they cost about four
  group selectors, which would push the 17 groups onto two lines or narrower buttons.
- Does the mode reset to Patch on its own, or stay where the user left it? Prefs either way.
- Drop onto the FX area as well as the Voice area - the manual says both.

## Suggested first slice

Prototype builder, group selectors, icon row, and DOUBLE-CLICK to add. That is useful on its
own, exercises every new piece, and is verifiable from the backdoor - a synthetic drag does
not reach the canvas at all (see `backdoor.c`, and why SCROLL and ZOOM exist), so the drag
path needs a human. Drag-and-drop second.
