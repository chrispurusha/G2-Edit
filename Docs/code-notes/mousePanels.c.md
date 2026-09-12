# mousePanels.c notes

The longer comments from `mousePanels.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. in `handle_patch_notes_mouse()`

FLOATING NOW, exactly as the settings panels are: the title bar moves it, the close button
closes it, and a click anywhere else on the panel is content. gNoteEditDismissed went with the
dismissal it existed to debounce — it swallowed the mouse-UP that followed a click-away, so
that the release did not land on whatever the canvas had underneath.

## 2. in `handle_patch_notes_mouse()`

A CLICK ELSEWHERE NO LONGER CLOSES THE EDITOR. It used to: any press that missed the
text area set active = false, which meant clicking the title bar — the part of a panel
you are most likely to press, since on every other panel it is the drag handle — shut
the notes editor outright. Reported 2026-08-20.

Click-away-to-dismiss is not the behaviour any other panel here has, and it is not
what a floating panel should do: the canvas underneath is live, so a click on it means
what it says and has no business closing a window somewhere else. Close and Discard
are the ways out, plus Escape.

## 3. in `handle_patch_settings_mouse()`

FLOATING NOW, so this claims only the clicks that land on it. It used to end in an
unconditional `return true`, i.e. it swallowed every click anywhere on screen while open —
which is what "modal" meant here, and why the canvas and every other panel went dead. The
three-way answer comes from SynthLib (floatingPanel.h) rather than being written out per
panel, because the two ways of getting it wrong are subtle and both have bitten before.

## 4. `handle_patch_settings_key()`

── Escape, through the same front-to-back registry as clicks ────────────────

These replace a settings_panel_escape() helper written the same day, which walked the three panels
in a hand-fixed order. That was right only for as long as the order matched what was drawn — the
exact drift this registry exists to prevent — and it could not know which panel the user was
actually looking at. Registering the panels means the key goes to the front one, by the same sort
that decides drawing and hit-testing.
