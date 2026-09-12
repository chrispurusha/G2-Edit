# mouseTopbar.c notes

The longer comments from `mouseTopbar.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. in `handle_button()`

Shift-click links a variation into the edit group instead of selecting it — see
variation_is_linked() in globalVars.h. It toggles on the SELECTED button too: the
selected variation receives its own edits regardless, but only explicit membership
survives selecting a different one, and that survival is the whole point of the group.
Init is not a real variation, so it is left to select normally.
