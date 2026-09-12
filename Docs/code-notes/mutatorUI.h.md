# mutatorUI.h notes

The longer comments from `mutatorUI.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `MUTATOR_NUM_BOXES`

Patch Mutator floater: panel chrome + drag, Mutate/Randomize/Interpolate/Cross operators with
Probability/Range/Cross-probability sliders, Mother/Children x6/Father row (click to
focus+audition), 7 Quick Lock category buttons, a Temporary Storage grid (click an empty slot
to save the focused genome there, click a saved slot to load it as Mother), and a Patch
Variations mirror row (click = load as Mother, Cmd-click = commit the focused genome into that
variation's edit buffer). Right-click empties a Storage or Mother/Children/Father box.
Drag-and-drop works between any two boxes across all three rows - Mother/Children/Father,
Temporary Storage, and Patch Variations (plain drag = copy - dragging onto Father loads it
quietly, without auditioning, since it's just the other breeding parent; Shift-drag =
Interpolate; Cmd-drag = Cross); a plain drop onto a Variation commits there with confirmation,
since that's a real hardware write. Deliberately deferred: multi-select for the Exclude From
Mutation toggle.
