# menus.c notes

The longer comments from `menus.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `cable_menu_node()`

The original editor's cable popup has five items — DISCONNECT, BREAK, COLOR, DELETE and
DELETE UNUSED CABLES (G2 manual) — and each acts on a
CONNECTOR rather than on a cable, which is why they fit G2-Edit's per-connector popup even
though the original's popup opens on a right-click on a cable.

What differs between them is the SCOPE they walk:

```
  whole tree  - up to the chain root, then all of it. Used by CONNECT.
  branch      - from the clicked connector down only. Used by COLOR and DELETE.
  subtree     - the part matching the chain. Used inside BREAK.
```
The chain walking itself lives in cableChain.c so the connect path can share it.

## 2. `menu_action_set_cable_colour()`

COLOR. The manual is explicit that this is branch-scoped, not per-cable and not whole-tree:
"Cables in a serial cable chain will always have the same color", but "Cables in a branch
connection may have different colors" — which is exactly the branch scope.

White is NOT one of the six user colours; it is the STATE "this chain has no source".
No connector ever maps to it; only Disconnect and Break, leaving a chain with no source, produce it. Painting a dead chain would therefore break the
invariant, so it is refused rather than allowed to produce a colour the original cannot mean.

## 3. `menu_action_disconnect_cable()`

DISCONNECT. Splices the clicked connector out of
the chain and joins the chain back up around it, so what remains keeps working — this is the
"any remaining cable chains will be rerouted" of the manual, and the whole difference from
Break, which cuts without splicing.

The original picks ONE surviving neighbour to become the new parent — normally the connector's
own parent, or the first child when the connector is the chain root (an output, which has no
parent) — then reconnects every other neighbour to it.

It recolours using a flag computed before the edit; we recompute from source-reachability
afterwards instead. The result is the same where it matters and it cannot strand a coloured
sourceless chain, which is a state the original has no way to represent.

## 4. `menu_action_break_cable()`

BREAK, per the cable popup the G2 manual describes ("BREAK"). Splits a serial cable chain WITHOUT rerouting it, which is
what separates it from Disconnect: the part that still reaches an output keeps working, and
everything past the break stays connected but dead, shown white.

At an input, the cable feeding that input goes and the input's own onward chain is the dead
tail. At an output, every cable leaving that output goes, and each input it was feeding heads a
dead tail of its own.

## 5. `menu_action_delete_chain()`

DELETE — "Deletes the entire serial cable chain that the connection is part of. If you want to
delete a complete branch connection, this must be done from the cable origin of the branch."
That second sentence is the branch scope showing through: DELETE covers the same branch COLOR
does, so starting lower down deletes only what hangs off that point.

## 6. `menu_action_select_all()`

DELETE UNUSED CABLES — "Deletes all non-functional input-to-input connections (white cables) in
the Patch."

White means exactly "this chain has no source", so the sweep is computed from live
source-reachability rather than from the stored colour. A patch loaded from a file, or from a
G2 that an older build wrote, can carry a colour that predates the invariant, so the topology
is the only thing worth trusting here.

## 7. `menu_action_delete_unused_modules()`

Deletes every module in the viewed location with nothing patched to it — the original's
Patch > Delete Unused Modules. "Unused" means no cable touches it at either end, which is the
same test the original applies and is deliberately blunter than "contributes no audio": a module
feeding only a muted chain is still wired up, and guessing at intent would delete work.

Goes through the selection rather than deleting directly, so it inherits the existing
undo_push_delete_selection() bracket and one Ctrl-Z takes the whole sweep back.

## 8. `menu_action_paste_params()`

Pastes the copied module's PARAMETER VALUES onto the module that was right-clicked, leaving that
module, its cables and its position alone — the original's Edit > Paste Params. Only meaningful
between two modules of the same type, which is what the menu entry checks before offering it.

All variations are pasted, not just the active one, because that is what was copied and because
pasting one variation's worth would leave the others silently disagreeing with it.

## 9. `menu_action_copy_params_to_marked()`

Copies every parameter of the selected module(s), as they stand in the CURRENT variation, into
every MARKED variation — the one-shot form of the fan-out that shift-marking already applies to
each edit as you make it. For bringing variations into line after the fact, rather than having had
to mark them before touching anything.

Works on the whole selection, so several modules go across together; right-clicking a module that
is not part of the selection selects it first, exactly as Copy/Cut/Delete do.

send_param_value_to_links() does the per-variation work, so this inherits its rules for free: the
current variation is never written to itself, values that already match are skipped, and each
variation actually changed gets its own undo entry carrying its own previous value.

## 10. `menu_action_replace_module()`

"Replace with" — swap this module for another of the same group, keeping the cables that have a
counterpart. gReplaceCandidates is filled by open_module_context_menu() just before the menu is
shown, and the item's param is an index into it rather than a module type, so the submenu can be
rebuilt for whatever module was right-clicked.

## 11. `action_toggle_exclude_from_mutation()`

Patch Mutator "Exclude From Mutation" toggle. Applies to every module in the current
multi-selection if the right-clicked module is part of one (same ensure_module_selected()/
gSelection pattern menu_action_delete_module already uses), taking its new value from the
right-clicked module's own current state (matching what the menu's checkbox label shows).
Each toggled module gets its own undo entry rather than one grouped entry for the whole batch -
deliberately simple, since this flag has no audible/hardware effect and is trivial to re-toggle
by hand, so it doesn't warrant a dedicated bulk-undo payload type the way delete_selection has.
Also sends the change live via send_mutation_lock_value() (SUB_COMMAND_SET_MUTATION_LOCK,
confirmed working on real hardware - see its comment in defs.h).

## 12. `modules_set_colour()`

Only ever called for items with no subMenu — handle_context_menu_click()/
update_context_menu_hover() open a subMenu-bearing item's flyout themselves
and never invoke its action.
Recolours EVERY module in the current selection, not just one. The manual describes it that way
("make a selection of any combination of modules... to apply the color to the module(s)", p.61),
and the right-click entry used to recolour only the module that was clicked even with a group
selected. Shared with the palette band's colour swatches so both routes behave identically.

One undo entry per module rather than a grouped one, matching what the Exclude From Mutation
toggle does: colour has no audible effect and is trivial to set again, so it does not warrant a
bulk-undo payload of its own.

## 13. `module_prototype()`

Creates a module of `type` at column/row in the current slot's current
location (gLocation): assigns the next free index, inits its params, syncs it
to the device, and shifts existing modules down to make room. The shared core
of the right-click "add module" menu (menu_action_create) and the backdoor
ADDMODULE test command. Returns the new module's index, or -1 if the location
is full.
A module of `type` populated the way a freshly created one is - the right parameter count, the
right mode count, each mode at its OWN default rather than zero, and the type's name. No key, no
position, and nothing written to the database.

Factored out of create_module_at() when the palette needed a module to DRAW that is never
inserted: the drag ghost renders a real face, with its dials and connectors, so what follows the
cursor is the thing being placed rather than a rectangle standing in for it. Both callers share
this so the ghost cannot show defaults the created module will not have.

## 14. in `create_module_at()`

The palette's colour swatch applies to everything created from here on, which is what the
instrument does too (manual p.61: the selector "stays in its new selection, causing any new
modules you add to the Patch window to get the selected color"). It defaults to the standard
grey, so nothing changes for anyone who never touches it.

## 15. in `create_module_at()`

How many parameters this module actually has. Only parse_param_list() used to set this, so a
module created here rather than received from the G2 was left at 0 — and write_param_list()
skips any module with a count of 0, which meant the knob values of every editor-created module
were silently dropped on save. Offline that is every module in the patch.

## 16. in `create_module_at()`

The same omission, one field over: modeCount was only ever set by parse_module_list(), so a
module created here had none. Modes are the drop-down selectors — an oscillator's waveform, a
filter's slope, the reverb's room — and every place that reads them is bounded by this count,
so at zero they cannot be drawn, cannot be clicked, and write_module() stores none of them.
Add a module in the editor, set its waveform, save, and the waveform was gone.

The values themselves start at each mode's own default rather than at zero, which is not the
same thing: a filter's slope default is not its first entry.

## 17. in `create_module_at()`

A column with no room left refuses the new module rather than dropping it on top of what is
already there. The write above has to be undone by hand: unlike a drag, there is no earlier
position to restore, and delete_module_and_cables() is what tells the G2 the module it was just
given is going away again.

## 18. `action_reset_param_morph()`

Clears a parameter's morph RANGE and leaves its value alone. `index` is the morph group to clear,
or NUM_MORPHS for "all of them".

WHY THIS EXISTS: an Alt-drag can put a morph offset on any dial, and until now nothing could take
it off again except another Alt-drag back to exactly zero, which is not a thing a mouse can be
relied upon to hit. The dial shows the morph as an arc, so a stray one is visible but not removable.

Sending one message per cleared group is safe here even for all eight — eMsgCmdSetParamMorph is a
no-ack real-time write, in the same class as a dial drag, so there is no acknowledgement in the pipe
and no patch-version race of the sort the bulk MIDI CC tools have to avoid (see send_param_morph()).

NOT UNDOABLE, and deliberately not pretending otherwise: morph edits have never been on the undo
stack — the Alt-drag that creates one is not either — and adding a morph entry type is a change to
undo.c rather than to this menu. Worth doing, but as its own piece of work.

## 19. in `action_reset_param_morph()`

`index` IS THE ITEM'S ROW IN THE MENU, NOT ITS PAYLOAD — contextMenu.c calls action(index) with
the position it hit, and an item's own `param` field has to be read back out of
gContextMenu.items[index], exactly as action_assign_midi_cc() does. Using `index` directly is
the trap here, and a quiet one: this first version cleared "morph group 6" because the reset
item happened to sit at row 6, so the menu appeared to do nothing at all.

## 20. `find_controller_for_param()`

── MIDI CC actions ──────────────────────────────────────────────────────────
gControllerArray[slot] is a compact list (0..gControllerCount[slot]-1), not
a fixed 128-slot array like gKnobArray — each param can hold at most one
CC (matches module->param[0][paramIndex].midiCC/hasMidiCC being singular
fields), and send_deassign_midi_cc() takes only a CC number (no param),
confirming a CC number is exclusive to one param at a time — assigning an
already-used CC steals it, same as assigning an occupied knob position.

## 21. `assign_midi_cc_to_param()`

Assigns one parameter to one MIDI CC#, stealing the CC from whatever held it. Shared by the
right-click "Assign to CC# nn" menu and by MIDI Learn, so the two cannot drift apart.

The steal is the documented behaviour, not a side effect: the manual is explicit that "a MIDI CC#
can only be assigned to one single knob in the patch and if L assigns a MIDI CC# to a knob that
was already assigned to another knob the other knob will loose its assignment".

## 22. `clear_assignments_for_module()`

Everything that points AT a module by index, released when that module goes: its patch knob
assignments, its global knob assignments and its MIDI CC assignments. Called from
delete_module_and_cables(), so it covers Cut, Delete, and the deletes undo and redo perform
themselves — the one place every route to a module's removal passes through.

LEAVING THESE BEHIND IS NOT MERELY UNTIDY. All three tables address a module by its INDEX, and
find_unique_module_id() hands a freed index straight to the next module created — at which point a
knob the user never touched is suddenly wired to a parameter of a module they have just added, and
the G2 still believes the assignment it was never told to drop.

The entries are ZEROED rather than just flagged unassigned: the deassign actions above only clear
.assigned and leave the module index behind them, which reads as a live assignment to anything
that looks at the fields before the flag.

## 23. `midi_learn_last_cc()`

Where "last received" comes from, in preference order. The SYNTH is the thing with a MIDI IN, so
a controller wired into the G2 is only visible through its SUB_RESPONSE_MIDI_CC report; the
editor's own CoreMIDI input only sees what is plugged into the Mac. Either can be the real setup,
so both are accepted and the synth wins when both have something.

NOTHING IS REQUESTED. Confirmed from a USB capture of the original editor doing a Learn: when the
G2 receives a CC that is assigned to nothing it PUSHES SUB_RESPONSE_MIDI_CC unsolicited, carrying
the channel and the controller NUMBER, and the editor simply remembers it. The Learn itself is
then a single assign command with no query anywhere. An earlier attempt here polled the synth at
the moment L was pressed, which is both unnecessary and too late.

## 24. `midi_learn_focused_param()`

MIDI Learn, the original editor's L key (manual p.145): assign the parameter that was last
clicked to whichever CC# arrived most recently. The manual's own warning applies — it is the fast
way rather than the safe way, since nothing shows you which CC you are about to use.

Returns false with a reason logged when there is nothing to do, so the caller can stay silent.

## 25. `send_whole_patch()`

─── Bulk MIDI CC tools (Tools menu) ─────────────────────────────────────────

The original editor's "Assign MIDI" and "Clear MIDI". Both are single undoable operations over
the whole Slot: undo_begin_midi_cc_edit() snapshots the entire controller table, so however many
entries a sweep touches, Ctrl-Z puts all of them back in one step. Both push nothing when the
sweep changes nothing.

WHY ONE WHOLE-PATCH WRITE RATHER THAN A BURST OF PER-ENTRY COMMANDS. Every slot command is
stamped with the slot's patch version by usb_cmd_slot() (usbComms.c), and that version is only
ever refreshed when the G2's asynchronous SUB_RESPONSE_PATCH_VERSION_CHANGE (0x38) notification
is parsed — nothing bumps it locally after a send. int_rec() returns the moment it sees the
expected SUB_RESPONSE_OK and does not drain what else is pending, so back-to-back commands race
the version bump from the one before: whichever loses is rejected by the device, silently,
because the caller discards send_assign_midi_cc()'s return value and the local table has already
recorded the change. That is why a sweep of up to 120 assigns landed only sometimes. A single
eMsgCmdWritePatch carries the entire controller table (write_controllers(), protocol.c) in one
versioned command, so there is no second command to race. The per-parameter right-click assign
is left alone: one command with idle time either side never hits this, and a whole-patch write
for a single CC would be heavy-handed.

## 26. `midi_cc_assign_all_knobs()`

Gives every panel-assigned knob a MIDI CC, walking the knobs in page order (A1 knob 1 first) so
the numbering follows the panel layout rather than the order modules happen to sit in the patch.
Parameters that already carry a CC keep the one they have - the point is to fill in the gaps,
not to renumber a patch someone has already set up by hand.

## 27. `midi_cc_assign_selection()`

─── MIDI CC over the selection (Tools menu) ─────────────────────────────────

The original's "Assign MIDI to Selection" / "Deassign MIDI from Selection" (manual p.130, p.143).
A DIFFERENT OPERATION from the two above despite the similar name, and worth keeping straight:
midi_cc_assign_all_knobs() walks the 120 PANEL KNOB ASSIGNMENTS in page order, so it only ever
touches parameters someone has already put on a Parameter Page. These two walk EVERY PARAMETER
OF THE SELECTED MODULES ("all parameters of the selected modules will be automatically assigned
to MIDI CC# numbers"), whether or not a knob points at them. One fills in a control surface, the
other blankets a few modules.

Both share the everything-else of the bulk tools: one undo bracket over the whole controller
table, existing assignments left alone by the assign (it fills gaps, it does not renumber), and
a single whole-patch write at the end rather than a burst of per-entry commands.

## 28. in `open_toggle_menu()`

A WAVEFORM PICKER LISTS PICTURES, NOT WORDS, the way the original hardware editor's does. The
label is still filled in for every entry: the menu engine measures the cell from it, so the
rows come out the width the names would have needed, and only the PAINTING is replaced.
sWaveMenuModuleType carries the one piece of context the callback needs — SynthLib's menu
knows nothing about modules, so the app holds that itself, as its header describes.

## 29. in `open_param_context_menu()`

SIZED FOR EVERY ITEM AT ONCE PLUS THE NULL TERMINATOR, and it was not before: the worst case
was already 8 entries — assign knob, deassign knob, global assign, deassign global, the CC-learn
line, MIDI CC..., remove MIDI CC, rename — which wrote the terminator to menuItems[8], one past
the end of an array of 8. Reachable by any paramTypeEnable parameter assigned to a knob, a
global knob and a CC at the same time. The two morph-reset entries below take the worst case to
10, so this is 12: count the conditional items above before adding another.

## 30. in `open_param_context_menu()`

The manual's own alternative to the L key (p.145): right-click and "first check if the Assign
to CC# xx menu item indeed shows the MIDI CC# that the other instrument is sending". The
128-entry picker below cannot answer that question, so this carries the number the synth last
reported and assigns it in one click.

## 31. in `open_param_context_menu()`

ONE ENTRY PER BUTTON, captioned with what that button currently reads. The manual
has you right-click the button itself, but the whole group is a single click
region here — the button is geometry inside it, and the right-click path is a
registry QUERY rather than a dispatched press, so it has no captured rectangle to
divide up. Naming the buttons in the menu says the same thing without a second,
parallel record of where each box was drawn, and it also means a 4x2 group can be
renamed without having to hit a small box exactly.

## 32. in `open_param_context_menu()`

Morph reset. Offered ONLY where there is a morph to remove, so the menu does not grow a
permanently greyed entry on the great majority of dials that have none — and named after
the group it clears, because "Reset morph" on a dial carrying two of them would be a
guess. The focused group is the one an Alt-drag writes, so it is the one to offer first.

## 33. `gModuleInfoMenuLabel`

The module's type name and index, which used to be printed on the FACE - the type in brackets
across the middle of the title bar and the index in its top-right corner. Neither is patch data
and neither is something a user reads often; the original editor shows neither, and the face is
the scarcest space we have. They live here now, on the module's own right-click menu, where they
are one gesture away when you actually want them. CT, 2026-08-30.

## 34. in `open_module_context_menu()`

The two entries that are rewritten below are indexed by name rather than by a bare number:
inserting Paste Params into this array silently moved "Delete" under the index the exclude
label was being written to, which deleted the Delete entry from the menu. Named indices make
the next insertion harmless.

## 35. in `open_module_context_menu()`

"Replace with" lists the rest of this module's group. Greyed rather than hidden when there is
nothing to offer — the same treatment Paste Params gets below — so the entry keeps its place
and its being unavailable is visible rather than mysterious. It is unavailable for the eleven
modules that are in no group, and for every group whose role table is not written yet: with
no roles a replace could only move cables by raw connector number, which is the mechanical
behaviour the feature exists to avoid.

## 36. in `open_module_area_context_menu()`

BUILT FROM gPaletteList, not written out. These sixteen submenus used to be sixteen static
arrays here - 171 entries, the same 171 the drag-on palette reads from its own table - and two
copies of one fact drift apart the moment anyone adds a module to only one of them. The
grouping, the order within each group and the label all now come from the single table; the
palette's tiles use the module's short name instead, which is why the label lives in the
entry rather than being taken from gModuleProperties.
