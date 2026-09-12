# noteStack.h notes

The longer comments from `noteStack.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `NOTE_STACK_MAX`

LAST-NOTE PRIORITY for a monophonic engine — what makes legato work.

Hold D, play F, release F, and the note should fall back to the D still under your finger rather
than stopping. That needs a record of what is held, which is what this is. Without it, releasing
any note simply silences the engine and legato playing falls apart.

Split out of midiInput.c so the VST3 plug-in gets the same behaviour from the same code. It could
not be reused where it was: midiInput.c interleaves the stack with send_note_to_synth(), which
talks to the G2 over USB — something a plug-in must never do. Only the STACK is shared; who else
hears about a note stays with the caller.

The G2 itself is told about every note as played, not the stack's view: the hardware does its own
voice allocation and wants them all. The stack exists purely for the local monophonic engine.

NOT THREAD SAFE, and does not need to be: each host drives it from one thread — the MIDI thread in
the application, the audio thread in the plug-in.
