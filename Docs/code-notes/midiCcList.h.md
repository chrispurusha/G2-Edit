# midiCcList.h notes

The longer comments from `midiCcList.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `open_midi_cc_list_panel()`

Every MIDI CC assignment in the patch, in one list — the original editor's "MIDI Controller"
function (manual p.143), which it reaches from a right-click menu and the M key. Here it is a
menu entry only, matching the standing decision for this family of panels.

NAMED FOR THE ORIGINAL'S FUNCTION, not for what it contains, because "MIDI CC Assignments" is
almost exactly what the View menu's overlay is called — and that overlay annotates parameters on
the canvas rather than listing them, so two near-identical names for two different things sent
the owner to the wrong one.

WHY A LIST AND NOT JUST THE OVERLAY: the View MIDI CC Assignments overlay annotates parameters on
the canvas, so it can only ever show the location being viewed — an FX assignment is invisible
while the Voice Area is on screen. The manual makes the same distinction, pointing at this
function for "the complete list of all MIDI CC# assignments in a Patch".

It also answers a question MIDI Learn creates: pressing L STEALS the CC from whatever held it,
silently and by design, and this is the only place that shows what it took.

## 2. `CC_LIST_ROWS`

THE STATE IS PUBLISHED, as gParamPages and gParamOverview already are, because the panel is a
FLOATING one now: the shared registry in graphics.c takes the address of its tFloatingPanel and of
its `active` flag, and both have to be compile-time constants to sit in a static table. It was
file-static in midiCcList.c until 2026-08-20.
