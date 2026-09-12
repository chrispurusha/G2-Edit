# capture.c notes

The longer comments from `capture.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

WHY THIS EXISTS RATHER THAN A LINE OF ffmpeg.

ffmpeg's avfoundation input REPORTS the interface's real rate and channel count and then delivers
something else: an AVCaptureSession converts audio to 48 kHz, so a 192 kHz interface yields a file
LABELLED 192000 containing 48 kHz frames — a quarter of the samples, stretched over four times the
stated duration. Nothing in the file says so. Measuring a delay line from that is off by 4x, and
the only clue is that a 5 second capture claims to be 1.14 seconds long.

It also has no working channel selection here: `-channels` is rejected outright by this build, and
what arrives is whatever the device presents.

So this talks to the HAL through AUHAL, which hands over the device's own format untouched. It
prints the rate and channel count it actually got, and writes 32-bit PCM — see write_wav32() for
why that width and not 24.

Build:  cc -O2 -Wall -o capture capture.c -framework CoreAudio -framework AudioToolbox \
```
                                          -framework CoreFoundation
```
Usage:  ./capture --list
```
        ./capture --device Fireface --seconds 12 --out cap.wav [--rate 192000]
```

## 2. `write_wav32()`

32-bit PCM, not 24 — not for the dynamic range (the interface has nowhere near it) but because a
4-byte sample is a width Python's `array` module can load in ONE call, so analyse_ir.py can slice
a channel out of a 12 million sample capture instantly instead of calling int.from_bytes() per
sample. At these sizes that is the difference between a minute and a moment, and it is what keeps
the analyser dependency-free.

Clamped rather than wrapped: a wrapped sample looks exactly like a transient, and this rig
measures transients.

ONLY THE CHANNELS ASKED FOR (--channels), in the order asked. The QU-24 presents 32 inputs and a
measurement uses two or four, so writing all of them made a two-minute sweep 1.3 GB, nineteen-
twentieths of it bleed and silence. `keep` lists the device's own channel numbers (0-indexed, as
this tool has always numbered them); NULL keeps every channel.

THE ORIGINAL NUMBERS GO IN THE FILE, as an INFO comment after the audio - "channels 4,5 of 32
(0-indexed) from QU-24 Audio" - because a two-channel file no longer says which desk inputs it was,
and the analysis scripts address channels by number. Python's wave module stops reading at the
data chunk, so it never sees the comment and needs no change.

## 3. in `main()`

A STALLED DEVICE MUST NOT BE AN INFINITE WAIT. Unplugging the interface — or swapping a USB
isolator into the chain — stops the callback dead, and a loop that only watches the frame count
then waits forever: a 123 second recording sat at 5 minutes and counting, holding up the rest of
a sweep, with nothing written and nothing said. So progress is what is waited on, not completion,
and a stall ends the recording with what it has plus a warning that says why.
