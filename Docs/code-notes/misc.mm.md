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

## 2a. the audio activity

**APP NAP AND THE EFFICIENCY CORES.** This application draws only when something asks it to
(`synthlib_request_redraw()`, and see the note on that), so between gestures it genuinely looks
idle to macOS. An idle process gets napped: timers coalesced, threads deprioritised, work moved to
the efficiency cores. A DAW never looks idle and never gets any of it - which is the shape of what
CT saw in Activity Monitor, Ableton busy on the performance cores and the standalone not.

`NSActivityLatencyCritical` is the documented way for a process to say it is doing audio: no
napping, no timer coalescing. It is held only while the audio output is open, so an editor with the
engine switched off still naps as it should, and `audio_output_start()` / `audio_output_stop()` are
the two ends.

Under ARC the returned token is kept alive by the strong static alone - no retain, and `endActivity`
plus clearing the static is the whole teardown.

**This is a hypothesis with a mechanism, not a diagnosis.** CoreAudio reported zero overruns while
the break-up was audible, and a thread running late on an efficiency core should have produced some
- so if this fixes it, the overrun count was lying and the reason for that is the next thing to
understand.

