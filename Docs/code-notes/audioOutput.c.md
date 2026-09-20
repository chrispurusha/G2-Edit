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

## 4. the sample-rate listener

THE DEVICE'S RATE CAN CHANGE UNDER A RUNNING UNIT - Audio MIDI Setup, or another application
opening the device first and setting it. The rate was read once from the unit's stream format at
`audio_output_start()` and handed to the engine there, and nothing watched it afterwards, so the
engine went on rendering at the old rate: everything plays at the wrong speed, the device asks for
frames at a rate the engine is not producing them for, and since notes §29a the rate also decides
the oversampling factor, so it is doing the wrong AMOUNT of work as well.

`kAudioDevicePropertyNominalSampleRate` is now listened for on the selected device. The listener
**only raises a flag and wakes the loop**. It runs on a HAL thread, and neither of the two obvious
things can be done there: re-opening the unit would tear down the callback that may be running, and
rebuilding the decimators would race the audio thread reading them. `audio_output_poll_rate_change()`
does the work from the render loop, and does it by re-opening the output rather than patching the
rate in behind it - the open is what re-reads the format, tells the engine and rebuilds what
depends on it.

One owning thread, others post - the same rule the USB thread's wake follows.

## 5. the overload counter

**`kAudioDeviceProcessorOverload` is CoreAudio's own verdict on whether we missed the deadline**,
and nothing in this project was listening for it. That left no way to tell the engine being late
apart from the device glitching for its own reasons - which matters, because every measurement of
the render time says it is NOT the engine: at 256 frames and 48 kHz the worst block is under a
third of the budget with the whole voice area sounding.

The idea is JUCE's - its CoreAudio device counts the same notification as `xruns` - and so is the
rest of the watched set. Read it for the mechanism; the code here is ours.

The listener watches four selectors on the device. One counts, three restart:

| selector | what it means |
|---|---|
| `kAudioDeviceProcessorOverload` | an IO cycle overran - counted, nothing else |
| `kAudioDevicePropertyNominalSampleRate` | §4 |
| `kAudioDevicePropertyBufferFrameSize` | the buffer changed under us, same problem as the rate |
| `kAudioDevicePropertyStreamFormat` | likewise - the format we opened with is not the one running |

The count is zeroed when the output opens, so it is always "overruns since this device was opened".
`SNDSTATUS` reports it with the rate and the buffer size, because the sound engine cannot: it is
deliberately platform-free and knows nothing about CoreAudio.

