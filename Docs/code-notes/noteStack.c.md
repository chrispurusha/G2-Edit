# noteStack.c notes

The longer comments from `noteStack.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Each is titled by what it documents.

## 1. `note_stack_note_off()`

EVERY KEY GOES TO THE ENGINE AS PLAYED (2026-09-13). This used to be where Mono and Legato happened:
the stack fell back to the newest note still held and re-sent it as a note-on. The engine now keeps
its own record of the keys held and decides what sounds after a release (sound engine reference §15),
and the G2 goes back to the HIGHEST key held, not the newest - so the stack was answering the wrong
question as well as answering it in the wrong place.

What the stack still holds is for its caller: midiInput.c walks it on a panic, to release on the G2
every key it was sent.
