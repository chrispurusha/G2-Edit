# misc.mm notes

The longer comments from `misc.mm`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

Everything that doesn't strictly need Objective-C/Cocoa has moved out of this file — File/
Settings/Backup/Restore menu actions live in menuActions.c, and settings persistence lives in
persistence.c (backed by SynthLib's cross-platform prefs.h rather than NSUserDefaults). What's
left here is genuinely Mac-only: the minimal native app menu Cocoa itself requires, and
sleep/wake notifications (NSWorkspace has no cross-platform equivalent in this codebase).

## 2. `setup_main_menu()`

Sets up the minimal native Cocoa app menu (Quit/About/Hide/Services — GLFW's Cocoa backend
already populates these at index 0), then restores window/zoom/dial-mode/last-folder state
from the prefs file (see load_saved_settings() in persistence.c; settings used to live in
NSUserDefaults, now a plain text file via SynthLib's prefs.h so the same mechanism can work on
Windows/Linux too). File/Settings/Backup/Restore/Controls/View menus used to be constructed
here too; they're now the in-window bar built in src/appMenuBar.c on top of SynthLib's menuBar
engine, sharing menuActions.c's action functions.

## 3. `platform_any_mouse_button_down()`

WHETHER ANY MOUSE BUTTON IS PHYSICALLY DOWN, asked of the window server rather than of our own
event history.

This exists for one failure: the mouse-up that never arrives. A captured dial drag has the pointer
hidden and decoupled from the hardware, so losing the release does not merely leave a dial held —
it leaves the user with no pointer at all. Every other way of answering "is a drag still going"
is derived from the event that went missing: our drag flags were set by the press and cleared by
the release, and glfwGetMouseButton() reports the last event GLFW was handed, which is the same
stream. [NSEvent pressedMouseButtons] reports the hardware, so it is true whether or not we were
told — and it is what the VST3 shell already uses for the same job (vst3/g2View.m).
