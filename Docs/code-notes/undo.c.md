# undo.c notes

The longer comments from `undo.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tUndoModuleReplacePayload`

A module swapped for another of the same group. Both images are complete tClipboardModules, so
undo and redo are the same operation with a different one — there is no need to work out what
changed. The CABLES are not in here: module_replace() brackets itself with undo_begin_cable_edit,
so the cable half arrives as its own adjacent entry.

## 2. `tUndoMidiCcPayload`

MIDI CC assignments are recorded as a before/after image of the whole slot's controller table,
for the same reason cable edits are: assigning a CC that another parameter already owns both
steals it from that parameter AND assigns it here, so a single click is two changes, and the
table is the only thing that describes the result honestly.

## 3. `snapshot_module()`

Everything about a module that has to survive being deleted and put back: type, position,
colour, up-rate, name, every variation's params, modes, and any custom param labels. The same
snapshot serves delete-undo, create-redo and the clipboard, which is why it is a tClipboardModule
rather than a private struct.

## 4. `apply_midi_cc()`

Drives the slot's controller table to the chosen image, then tells the G2 with ONE whole-patch
write. This used to replay a deassign per dropped CC followed by an assign per surviving entry,
which put it in exactly the position the bulk Tools sweep was in: each of those commands carries
the slot's patch version, that version only advances when the device's async 0x38 notification is
parsed, and back-to-back commands race it — so undoing a sweep of 120 assignments landed only
partly, silently. write_controllers() (protocol.c) sends the whole table inside a single
versioned command, which is both atomic and exact, so there is no over-apply to reason about.
See the bulk MIDI CC note in menus.c.

## 5. `gGlobalKnobEditOpen`

─── Global knob assignments ───────────────────────────────────────────────

Same shape as MIDI CC: assigning a knob that is already taken frees it first, so one click is
two changes and the table is what has to be recorded. Global knobs are performance-wide, hence
no slot on the payload.

## 6. `cable_sets_differ()`

Cables are compared as a SET, not as two ordered lists: deleting and re-creating a cable
moves it to a different index in the database array, so an identical patch can snapshot in a
different order. Keys are unique within a location, so with equal counts a one-way lookup is
a full set comparison.

## 7. `apply_module_replace()`

Puts the module back to one of its two images. The whole record is written rather than a diff:
a replace changes the type, the parameter count, the mode count, every parameter value and
possibly the name, and picking those apart afterwards would be a second chance to get the role
mapping wrong. update_module_up_rates() is not called here - the cable entry that always
accompanies this one calls it, and the two are applied together.
