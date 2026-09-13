# noteStack.h notes

The longer comments from `noteStack.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Each is titled by what it documents.

## 1. `NOTE_STACK_MAX`

The keys a caller has sent on and not yet off. Every note goes to the engine as played; what sounds
after a release in Mono or Legato is the ENGINE'S decision (sound engine reference §15), made the way
the G2 makes it. Until 2026-09-13 the stack made it instead, falling back to the newest key held.

Split out of midiInput.c so the plug-in shares the same code. It could not be reused where it was:
midiInput.c interleaves the stack with send_note_to_synth(), which talks to the G2 over USB -
something a plug-in must never do. Only the STACK is shared; who else hears about a note stays with
the caller. midiInput.c walks it on a panic, to release on the G2 every key it was sent.

NOT THREAD SAFE, and does not need to be: each host drives it from one thread - the MIDI thread in
the application, the audio thread in the plug-in.
