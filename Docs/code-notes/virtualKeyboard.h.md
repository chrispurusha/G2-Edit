# virtualKeyboard.h notes

The longer comments from `virtualKeyboard.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `VKB_KEYS_VISIBLE`

The Virtual Keyboard panel — the original editor's Tools > Virtual Keyboard (manual p.128).
"Click on the keys of the Virtual Keyboard to play single notes. The selected note will be
indicated by a black dot on the corresponding key. The note will sustain if you keep the mouse
button depressed, just like pressing a key on a real keyboard."

Opened from Settings > Virtual Keyboard. NO keyboard shortcut — the original's Ctrl-K, dropped
on the owner's instruction, consistent with Parameter Pages and Parameter Overview.

THE NOTE ITSELF GOES OUT OVER SUB_COMMAND_PLAY_NOTE (0x56), whose wire format is
REVERSE-ENGINEERED AND UNCONFIRMED ON HARDWARE — see
send_play_note() in usbComms.c for the derivation. If nothing sounds, that function is the thing
to doubt, not this file: everything here is ordinary panel drawing and hit-testing.

One note at a time, which is what the manual describes (a mouse has one button and the original
speaks of "single notes"). noteOn is the sounding note, or -1. Any transition sends the note-off
for the previous note before the note-on for the new one, so dragging across the keyboard can
never leave one hanging.

DRONE and REPEAT are the original's other two buttons, both plain toggles (manual p.128):
```
  Drone  — "make the next played note start sounding 'infinitely'. Click the Drone button again
           to disengage." So a key release stops sending the note-off; the note runs until
           another key is played, Drone is switched off, or the panel closes.
  Repeat — "make the last played note play repeatedly. Click the Repeat button again to
           disengage." The note is re-struck on a timer, which is why this panel needs
           virtual_keyboard_tick() driven from the render loop.
```
"Hold" is the obvious name for Drone but is NOT what the original calls it, so the button says
Drone to match.

WHAT IS NOT DONE, and deliberately: the original expands its range by drag-resizing the window
frame, and hides its button bar the same way. This panel has a fixed span and the four scroll
buttons instead. Nothing else here depends on that, so it can be added later without rework.

## 2. `virtual_keyboard_tick()`

Repeat's clock. tick() re-strikes the note when one is due and is a no-op otherwise, so the
render loop can call it unconditionally; wants_ticks() tells that loop to wait with a timeout
rather than sleeping on glfwWaitEvents(), which would otherwise stall the repeat until the next
input event.
