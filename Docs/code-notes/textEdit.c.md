# textEdit.c notes

The longer comments from `textEdit.c`. The code points at each as `// notes §k`.

## 1. `notes_own_keyboard()`

Moved from `mouseHandle.c` (its notes §4) on 2026-10-16.

WHO OWNS THE KEYBOARD, asked once, instead of six independent editors each helping themselves.

char_event() is a row of sequential ifs with no else between them, so every ACTIVE editor used to
receive every character. Two of them being active at once is easy: opening the notes editor ends
no name edit, and starting a name edit does not close the notes editor. The results were a
character landing in the patch name AND the notes buffer at the same time, and — reported — the
synth name being untypeable whenever the notes editor sat open behind the Synth Settings panel.

Two rules settle it, in this order:

```
  * A NAME EDIT WINS. You entered it by clicking that exact field, which is the most specific
    intent available, and it is dismissed by clicking away.
  * OTHERWISE THE FRONTMOST PANEL WINS, which is what you are looking at. The notes editor is the
    only panel whose typing is handled here rather than through the panel key walk — the walk is
    already ordered front to back, so the other panels have always had this for free.
```

## 2. in `text_edit_key()`

Moved from `mouseHandle.c` (its notes §32) on 2026-10-16.

A Channel Select group is named as a SET: the parameter carries one name
per button and the wire format sends them together, so renaming one button
has to fill in the others from what they already read. Leave them out and
the instrument is told the group has fewer buttons than it does.

## 3. Why this file exists

The typing for the five name fields (patch, module, parameter label, synth, performance) and the
patch notes lived in `mouseHandle.c`'s `char_event()` and `key_callback()`. The plug-in does not
compile `mouseHandle.c` - it is the application's GLFW input layer - so in G2 Alike Rename opened the
field and no keystroke ever reached it: each one went on to the host, and under Ableton, with its
computer MIDI keyboard on, the letters played notes (CT, 2026-10-16).

Moved out unchanged so both editors run the same code: the application calls it where the code used
to be, and the plug-in from `g2_input_popup_key()` / `_glfw()` after SynthLib's popups have had the key.
