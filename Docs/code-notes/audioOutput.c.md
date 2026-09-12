# audioOutput.c notes

The longer comments from `audioOutput.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `OUTPUT_CHANNELS`

A HAL output AudioUnit rather than the default-output one. The difference is the whole point: the
default-output unit always follows the system's chosen device and cannot be pointed anywhere else,
while the HAL unit takes a device and a channel map — which is what allows the engine to be sent
to, say, outputs 29/30 of an interface while the system carries on using the built-in speakers.
See audioOutput.h.

## 2. `gLevelDb`

The engine's output attenuation, in dB and never positive. Kept here with the other output
settings rather than in the engine, because this is where the preferences plumbing already lives
and where the rest of the audio path's remembered state is read at startup. The engine holds the
working value; this owns the persistence.

## 3. in `audio_output_load_settings()`

Left and right used to be one "first channel of a pair" setting. Carry an old one over rather
than dropping someone back to outputs 1/2 without explanation.
Read the level before anything can make a noise, and push it into the engine — the engine has
no preference of its own, so without this a remembered attenuation would be forgotten.

## 4. in `audio_output_start()`

Route our stereo pair to the chosen output channels. The map has one entry per DEVICE channel
saying which of our two it takes, or -1 for silence — so sending to outputs 29 and 30 means a
map of -1s with 0 and 1 at positions 28 and 29. Without this the audio always lands on the
device's first pair, whatever the interface.
