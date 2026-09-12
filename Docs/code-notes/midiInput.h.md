# midiInput.h notes

The longer comments from `midiInput.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `midi_input_start()`

MIDI note input, so a real keyboard can play the local sound engine, the G2 itself, or both at
once — which is the point of "both": hearing the engine against the hardware on the same notes is
how the two get compared. Like audioOutput.c this is the platform half; CoreMIDI lives here and
nowhere else.

Deliberately NOT owned by the sound engine. It runs from application startup, because sending MIDI
on to the G2 is useful whether or not the engine is switched on.

The source and channel are chosen rather than assumed. Listening to everything on every channel is
a fine default for one keyboard on a desk, but it is wrong the moment a controller sends on a
channel the patch is not using, or a DAW's echo port is also present. The source is remembered by
its CoreMIDI unique ID rather than its name, since names repeat across identical interfaces.

## 2. `MIDI_INPUT_NONE`

Pass MIDI_INPUT_NONE to take no input at all, or MIDI_INPUT_ALL to take every source at once —
everything currently attached AND anything plugged in later, since the CoreMIDI setup-changed
notification reconnects. That state already existed as the startup default (no specific source
chosen); what was missing was any way back to it once a single source had been picked.
