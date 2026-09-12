# appMenuBar.h notes

The longer comments from `appMenuBar.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `gAppMenuBar`

G2-Edit's own File/Settings/Backup/Restore/Controls/View row, replacing the
native Cocoa menu bar (misc.mm) with SynthLib's cross-platform menuBar
engine. gAppMenuBar is a NULL-label-terminated tMenuBarItem[] suitable for
passing straight into render_menu_bar()/handle_menu_bar_click()/
update_menu_bar_hover(); app_menu_bar_rect() is the bar's screen rectangle
for this frame.
