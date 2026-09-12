# menus.h notes

The longer comments from `menus.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `midi_cc_assign_selection()`

The original's "Assign MIDI to Selection" / "Deassign MIDI from Selection". NOT the same thing as
the pair above: these cover every parameter of the selected modules, assigned to a panel knob or
not, where those two only ever touch the 120 panel knob assignments. Always the selected Slot,
since that is the only Slot a selection can be in.
