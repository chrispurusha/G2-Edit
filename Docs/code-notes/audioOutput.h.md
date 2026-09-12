# audioOutput.h notes

The longer comments from `audioOutput.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `audio_output_start()`

The platform half of the sound engine: opens an output device and pulls audio from soundEngine.c
on the real-time thread CoreAudio provides. This is the only file in the engine that knows what
operating system it is on — soundEngine.c is plain C and stays portable.

The device and the pair of channels within it are both selectable, so the engine can be sent to
one output of a multi-channel interface and compared against the G2 coming back on another. Both
are remembered between runs: the device by its UID rather than its index, since indices shuffle
as interfaces come and go.

sound_engine_start()/stop() own the lifetime; nothing calls start/stop here directly.

## 2. `audio_output_select_device_by_uid()`

Selecting any of these restarts the audio device if it is running, so a change takes effect
immediately, and records the choice for next time.

Left and right are chosen SEPARATELY rather than as a pair. On a desk the two legs of a monitor
path are not necessarily neighbours, and forcing 29/30 when the wiring wants 29/31 would mean
repatching the desk to suit the software.
BY UID, NOT BY INDEX, and that is the whole point: the list is re-enumerated on every count()
call, so an index taken when a menu was built can name a different device by the time the item is
clicked — a Bluetooth device appearing, an aggregate device being created, a display waking up. The
remembered preference has always been a UID for exactly this reason (see the note at the top of
this file); the act of selecting used to be an index anyway, which left the robust half undermined
by the fragile one.

Returns false if the UID names no current device, or if the device could not be started — a device
can be present and still refuse to open, being in exclusive use by something else or unable to
offer the rate asked of it. The old index-based call discarded that outcome, so a device that
failed to start looked selected and simply made no sound.

## 3. `audio_output_level_db()`

Buffer size, in frames. Fewer frames means a note takes effect sooner — the buffer length is the
floor on how late a keypress can land — at the cost of waking the audio thread more often. 0 means
leave whatever the device is already set to.
The engine's output attenuation in dB, 0 or negative. Remembered between runs like the device and
channel choices; the engine itself holds no preference of its own.
