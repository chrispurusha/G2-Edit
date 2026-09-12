# misc.h notes

The longer comments from `misc.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `platform_any_mouse_button_down()`

register_sleep_wake_notifications() and setup_main_menu() are implemented in misc.mm — the only
two things left in this codebase that genuinely need Objective-C/Cocoa. Everything else
declared below is plain C: menu actions live in menuActions.c, settings persistence (backed by
SynthLib's cross-platform prefs.h rather than NSUserDefaults) lives in persistence.c.
True while any mouse button is physically down, read from the window server rather than from the
event stream. Used to recover a drag whose mouse-up never arrived — see the definition.

## 2. `file_menu_open_patch()`

File/Settings/Backup/Restore menu actions — plain-C-callable bodies used by the in-window menu
bar (src/appMenuBar.c). File open/save and folder picking all go through the custom in-window
browser (SynthLib/src/fileBrowser.cpp); alerts/confirms/bank-target pickers go through
SynthLib/src/alertDialog.cpp — none of it uses native Cocoa panels any more. Only the dispatch
logic (which browser mode to open, with what pre-filled state) lives here.
