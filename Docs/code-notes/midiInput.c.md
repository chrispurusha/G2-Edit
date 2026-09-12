# midiInput.c notes

The longer comments from `midiInput.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `gPressureCount`

How many pressure messages have arrived, of either kind. This exists because the obvious way to
answer "is the keyboard sending aftertouch at all" — logging each message — is exactly what must
not happen here: LOG_DEBUG writes to stdout AND to the USB log file, and doing that per message on
CoreMIDI's callback thread stalls MIDI input outright. An atomic counter costs nothing and the UI
thread can read it whenever it likes.

## 2. in `send_note_to_synth()`

WITH THE LOCAL ENGINE SOUNDING, THE G2 DOES NOT ALSO GET THE NOTE. Otherwise one key press
plays twice — once here and once on the instrument — which is exactly the comparison the
engine exists to make, ruined by doing both at once.

NOTE-OFFS ALWAYS GO THROUGH. Enabling the engine while a key is held would otherwise swallow
the release and leave the G2 droning with no way to stop it short of a panic. A release sent
for a note the instrument is not playing is harmless, so this is the safe asymmetry rather
than a tidy one.

## 3. `morph_moved()`

A morph position is not read by the audio thread the way pitch bend is — it is folded into the
parameter snapshot, and that snapshot is only rebuilt during a redraw. An idle window sits in
glfwWaitEvents(), and turning a wheel produces no window event, so without this a morph would not
be heard until something unrelated happened to wake the render loop. Safe from the MIDI thread.

The redraw is wanted in its own right too: morphed dials move on screen as the wheel turns.

## 4. in `morph_moved()`

FOLD IT IN HERE, not on the next redraw. sound_engine_set_morph() only records the
position; the audio thread reads a parameter SNAPSHOT, and until that is rebuilt the wheel
has moved nothing it can hear. This used to be left to the redraw below, which capped mod
wheel and aftertouch response at the frame rate and put a full canvas repaint between the
control and the sound — the wheel felt laggy, and a fast sweep arrived as steps.

Safe from this thread since the snapshot's writers were given a mutex of their own
(gParamsWriteMutex in soundEngine.c); the audio thread is a lock-free reader and is not
held up by it.
READ-LOCKED HERE, not inside sound_engine_update_from_patch(). That function is also
called from inside render_frame(), which already holds the read lock - and taking a
non-recursive rwlock twice on one thread can deadlock outright. So each of its callers
that is NOT already holding the lock takes it, and the function itself never does.

This is the call that made the database a three-thread structure: it runs on the CoreMIDI
thread so a morph reaches the engine immediately rather than waiting to be drawn.

## 5. in `handle_message()`

Omni takes everything; otherwise only the chosen channel. Filtering here rather than per
message type means a controller chattering on another channel cannot move a morph either.

An MPE controller needs Omni. MPE gives every note its own member channel and sends that
note's pressure and bend on the same channel, so picking a single channel throws away most of
the keyboard — and, because the notes on the surviving channel still play, it fails by
dropping expression rather than by going silent, which looks like the pressure not working.

## 6. in `handle_message()`

Polyphonic key pressure, which carries the note in the FIRST data byte and the
pressure in the second — the opposite way round from channel pressure below.

Plenty of keyboards send this instead of channel pressure, and an MPE controller may
send either. The engine has a single voice, so only the note actually sounding is
allowed to move the morph; without that test a key still held underneath would fight
the one being played.
