# helpPanel.h notes

The longer comments from `helpPanel.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tHelpPanel`

The keyboard and mouse reference, as a floating panel (floatingPanel.h) so it can be left open
beside the canvas while you try the things it lists — which is the whole point of a shortcut list
and the reason it is not a modal dialogue.

Its content is a static table in helpPanel.c. That table is DOCUMENTATION: every row has to match
what the code actually does, so a binding changed in mouseHandle.c or virtualKeyboard.c means that
row changes too. A shortcut list that lies is worse than none, because it is believed.
