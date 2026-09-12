# paramOverview.c notes

The longer comments from `paramOverview.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `move_assignment()`

Moves the assignment at `from` to `to`, overwriting whatever was there. This is the manual's
headline use of this window ("drag a grey display area to another grey display area and you
move the knob assignment to the new position"), so it OVERWRITES rather than swaps — that is
what the original does, and a swap would silently resurrect an assignment the user was
deliberately replacing.

The patch case tells the G2 with ONE whole-patch write rather than the deassign/deassign/assign
triple the right-click Assign menu sends. write_knobs() (protocol.c) is already part of
push_slot_to_device(), and a burst of small slot commands is exactly the pattern that loses
assignments to the patch-version race — see the bulk MIDI CC note in menus.c. Global knobs have
no whole-perf push to ride on (write_global_knobs() is only used when SAVING a performance
file), so those still go as individual commands, matching action_assign_global_knob().

## 2. in `render_param_overview_panel()`

FLOATING, so the position comes from the panel rather than from the window: chosen once on
first show and thereafter wherever the user has dragged it. Centring every frame is what made
a panel impossible to move — it snapped back before the next redraw.

No draw_dialog_background_overlay() either. Dimming the canvas behind is what a MODAL dialog
does, and this is not one: the canvas stays live underneath and stays legible to match.

## 3. in `render_param_overview_panel()`

── Button row: Patch/Global, View MIDI, and the two bulk MIDI tools ───
Assign MIDI and Clear MIDI live here because this is where the original puts them (manual
p.126); they are the same operations the Tools menu offers, run on the Slot THIS panel is
showing rather than the selected one.

## 4. in `handle_param_overview_mouse()`

A drop on a different box moves the assignment; anywhere else, including the box it
started on, is a no-op. Dropping outside the grid deliberately does NOT clear the
assignment - the original has no such gesture, and losing an assignment to a stray
release would be a nasty way to find that out.
