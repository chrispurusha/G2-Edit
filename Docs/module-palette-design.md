# Module palette — design & progress

Living design note. Nothing is built yet; this records the decisions and the numbers
behind them so the work can start without re-deriving any of it. See also `todo.txt`
("Module palette").

## What is being asked for

Users want to add modules by dragging them from a palette onto the patch, as the
original editor does. `todo.txt` has carried "Module palette drag selection" as a
one-liner under USER REQUESTS; this note is that item worked out.

## What the original ACTUALLY does — corrected 2026-09-07 from the manual's own figures

The figures on manual p.60 settle two things this note previously got wrong from the text alone.

**The group selectors are a 2 x 8 GRID of sixteen, not a row of seventeen.** Top row In/Out, Osc,
Rnd, Filter, Delay, Level, Switch, Seq; bottom row Note, LFO, Env, FX, Shaper, Mixer, Logic, MIDI.
Small text buttons, the focused one blue. The whole block is about the width of four module icons -
far more compact than a single row of seventeen, and it leaves the rest of the bar free.

**The module icons are UNIFORM LITTLE TILES, NOT SCALED-DOWN MODULE FACES.** Roughly 30 x 26 px,
every one the same size whatever the module's height, each carrying an abbreviated NAME across the
top and a small pictogram beneath it: 2-OUT, 4-OUT, 2-IN, 4-IN, FX IN, KBD, MONO, DEVICE, STAT,
N-DET, N-BAR. The name is what identifies the module; the pictogram is decoration.

That reverses decision 4 below, and the module heights say why it has to. Of 170 modules, 94 are two
rows and 144 are four or fewer - but TEN are six rows or more, and Operator is TWELVE. At the 0.2x
scale this note proposed, Operator is about 100 px tall in a 40 px band. There is no scale at which
a 1-row Constant and a 12-row Operator are both legible in one row of icons, which is presumably
exactly why the original did not try.

The original's own pictograms are not the model - they are 2009 artwork for a different-looking
editor. What carries over is the LAYOUT LESSON: uniform tiles, name-led, in a compact block.

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

**4. SUPERSEDED — see "What the original ACTUALLY does" above.** This said the icons should be our
own module faces rendered at 0.2x. The manual's figure shows the original uses uniform tiles, and
the height distribution shows why a scaled face cannot work: a 12-row Operator and a 1-row Constant
have no common scale. Replaced by decision 6.

**5. A SWITCHABLE EXTENSION BAND, not a topbar mode.** This too is a change of mind, and it follows
from decision 6: with uniform tiles the palette needs about 60 px, not the 44 px per variable-height
row this note first assumed, and it no longer has to displace anything to fit. A mode that swapped
the topbar's contents would hide the patch name, the category, the voice count and the Patch Load
meters exactly while modules are being added - and the manual ties adding modules to watching those
meters ("as you add modules... the Patch Load indicator(s) will expand"). Growing the bar keeps
everything visible and costs canvas only while the palette is open, which is the user's own choice.

**6. Uniform tiles, drawn in OUR module aesthetic.** Not the original's pictograms, which are
another editor's artwork, and not a scaled face, which the heights rule out. A tile is a small
rectangle in the module's own body colour with its name in our font, and the marks along its edges
are its CONNECTORS, coloured by type the way the canvas colours them - red audio, blue control,
yellow logic. That is real information at tile size (what a module takes and gives) where a 0.2x
face is a grey smudge, and it reads as ours because it is drawn from the same colour tables.

**7. A popup browser is a COMPLEMENT, not an alternative.** A floating dialogue is the wrong shape
for drag-to-place - it sits over the canvas you are dragging to - but it is the right shape for the
thing the band cannot do: find one module among 170 by typing part of its name. Worth having later,
sharing the same tile drawing and the same group table. Not in the first slice.

## Confirmed with CT, 2026-09-07

- **Switchable extension band.** The topbar grows when the palette is on and shrinks back when it is
  off; nothing is hidden while modules are being added.
- **Coloured tile, name, connector dots.** Uniform size, the module's own body colour, its name in
  our font, connector marks down the edges coloured by type from the canvas colour tables.
- **Horizontal, not vertical** - not put to a vote, because width is the scarce axis: modules are
  350 px wide against 43 px per row, so a side palette costs a whole module column to show what a
  60 px band shows.
- **Drag-and-drop FIRST**, against the suggestion to start with double-click. Noted with its cost: a
  synthetic drag never reaches the canvas (which is why the backdoor has SCROLL and ZOOM at all), so
  none of the drag path can be verified without a person at the machine.

## Built (2026-09-07)

`palette.c` is the band: the sixteen groups as a 2 x 8 grid, the selected group's modules as uniform
tiles, drag to the canvas, and a scroll row for a group wider than the window. `gPaletteList` in
`moduleResources.h` is the sixteen groups and their 171 entries as (group, module) pairs - pairs
rather than a field on the module because `NoteDet` is in two groups and a field could not say that.
Toggled from a **Modules** button in the topbar between Online and Undo, from View > Module Palette,
or from the backdoor's `PALETTE` command.

**Tiles shrink to fit before they scroll.** Osc is the largest group at 19 and nineteen full-width
tiles run off the right of the default window, so the width is divided by the count and only
FLOORED. Below the floor the names stop being readable and the row scrolls instead, with arrows at
the ends - the wheel alone would leave the overflow invisible.

**The ghost is up from the moment the button goes down**, with no movement threshold. A slop
distance was tried and taken out: it exists to stop a click being mistaken for a drag, and there is
nothing else a press on a tile can mean. A press that never reaches a module area is abandoned on
release - as is one released on the band, the topbar or the split bar - so nothing is created and
nothing is disturbed.

**The drag ghost is a real module face**, built by `module_prototype()` - the same defaults
`create_module_at()` lays down - and rendered by `render_module()` at the column and row the drop
would use. Click registration is suppressed while it draws by setting an EMPTY click clip:
`register_click_region()` drops anything outside the clip, and a ghost has no key of its own to
register under. Over the band itself, where every drag starts, there is no pane under the cursor at
all, so the ghost goes into the FOCUSED pane at the top of the cursor's own column and is a full
face from the outset; once the cursor enters a pane it tracks the drop position exactly. It was an
outline there at first, which left the opening moments of every drag showing a module with none of
its controls in it.

**Double-click a tile to add** the module below the focused one, without a drag - the manual offers
it as a first-class alternative (p.81) and it is the only route that works when the target is off
screen. The second click of a pair cancels the drag the first one started, so it never also drops a
ghost somewhere.

**A colour selector sits to the right of the group grid** (CT's suggestion, 2026-09-08), and the
instrument does the same thing - manual p.61: the selector "stays in its new selection, causing any
new modules you add to the Patch window to get the selected color". It applies to everything
`create_module_at()` makes, from the palette or from the right-click menu, and defaults to the
standard grey so nothing changes for anyone who never touches it. The drag ghost is drawn in it too,
so the choice is previewed rather than discovered after the drop.

A swatch does the OTHER half of what the instrument's selector does as well: it recolours whatever
is selected right now, a group selection included (manual p.61). With nothing selected only the
new-module colour changes, so setting up the next few modules never repaints anything by surprise.
The module right-click menu's own colour entry now goes through the same function - it used to
recolour only the module that had been clicked even with a group selected, which the manual says it
should not.

### Three bugs this shook out

- **Every drop into the already-focused pane was discarded.** `split_view_focus_at()` returns
  whether the focus MOVED, not whether the coordinate is in a pane; used as the validity test it
  said "outside a pane" for the commonest case of all. `split_view_pane_at()` is the question to
  ask; `focus_at()` is only for its side effect.
- **File > New Patch hung for ever with no G2 connected.** `state_handler()` returns early for the
  whole of `eCommsNeverConnected`/`eCommsReconnecting` - it tries to open the device, sleeps and
  returns - so it never reaches its own `msg_receive()`, and a command queued while offline is never
  dequeued at all. The reset did not happen AND the busy overlay had nothing to end it.
- **The busy overlay's five-second safety timeout had never once fired.** It was armed from
  `get_time_ms() / 1000.0` and tested against `glfwGetTime()` - CLOCK_MONOTONIC counts from boot and
  glfwGetTime() from library init, so on a machine up for a day the subtraction was about -86400 and
  could never exceed 5. Both sides are on `get_time_ms()` now. This is what turned any lost
  completion response into a permanent lock-up, not just this one.

### Checked without a person at the machine

The band changes the topbar height and so the canvas origin, which is the part most likely to break
something else. `PALETTE TOGGLE` was hammered 100+ times, and opened and closed across every
combination of three zooms, three scroll positions and three split positions with no failure and no
crash. **Clipping is right**: a module scrolled up under the band is cut cleanly at its lower edge,
and since `module_pane_clip_begin()` sets the pixel scissor and the click-region clip from the SAME
rectangle in the same call, the click regions move with it by construction.

What still needs a person: the drag itself with a real mouse, the hover preview, and the wheel/arrow
scrolling. A synthetic drag never reaches the canvas, which is why `PALETTE DRAG`/`DROP` exist at
all - they drive the state machine, not the pointer.

## What is left

- The sixteen static arrays in `menus.c` are now a second copy of `gPaletteList` and should be
  deleted, with the create-module menu built from the table instead.
- Double-click a tile to add below the focused module, which the manual documents alongside the
  drag. `palette_add_module()` is already there for the backdoor; only the double-click is missing.
- Whether the band's open state should persist across sessions.

## Superseded: the data layer note

`gPaletteList` in `moduleResources.h` is the sixteen groups and their 171 entries as (group, module)
pairs, with `gPaletteGroupName` for the labels and `palette_group_modules()` / `palette_group_name()`
to read them. Pairs rather than a field on the module because `NoteDet` is in two groups (In/Out and
MIDI), and a field could not say that.

It is a list of PAIRS for a second reason too: it makes the create-module menu's sixteen static
arrays in `menus.c` redundant, and those must be deleted and rebuilt from this table as part of the
palette work. Until they are, there are two copies of the same fact and they can drift.

Nothing visual exists yet. The band, the tiles, the hit testing and the drag are the next slice.

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

## The group table, and which one

**The palette uses the SIXTEEN TOOLBAR GROUPS, not the nineteen replace groups.** They are different
partitions of the same modules and the manual says so outright - "the replacement module pop-ups
doesn't always feature exactly the same modules as the module groups in the Toolbar". The nineteen
in `tModuleGroup` leave eleven modules in no group at all, which is right for replacement (there is
nothing to swap a Blue2Red with) and wrong for a palette (a Blue2Red still has to be addable). The
sixteen in `menus.c` cover all 170.

So the palette's source is the existing create-module menu list, and the two must become ONE table
rather than a second copy - a `paletteGroup` field beside `group` in `gModuleProperties` would do
it, and would retire the hand-maintained arrays in `menus.c` at the same time.

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
