# noteStack.c notes

The longer comments from `noteStack.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. in `note_stack_note_off()`

POLYPHONIC: release exactly the note that was let go and leave the rest alone. The fallback
below would be actively wrong here — the note it falls back to already has a voice of its own
sounding it, so retriggering it would restart a note the player is still holding, and the note
actually released would never stop.

## 2. in `note_stack_note_off()`

THE LEGATO CASE, and it is monophonic by definition. Retrigger the newest note still held
rather than releasing — releasing here is what makes a monophonic synth stop dead when a
passing note is let go.

Whether that note's envelopes START AGAIN is the voice mode's business, not the stack's:
the engine restarts them in Mono and glides on in Legato (voice_note_on() in soundEngine.c).
So in Mono the note returned to attacks afresh, which is unconfirmed on the hardware.
