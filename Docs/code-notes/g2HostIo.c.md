# g2HostIo.c notes

The longer comments from `g2HostIo.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

The plug-in's stand-in for the two platform I/O layers.

In the application the sound engine opens its own CoreAudio device (audioOutput.c) and its own
CoreMIDI ports (midiInput.c). In a plug-in the host owns both: it hands us a buffer to fill and
an event list to read, so neither layer exists. These are the few entry points the engine still
references, given null implementations so the link resolves.

This IS the "a wrapper replaces audioOutput.c and nothing else" plan, arrived at literally.

sound_engine_start()/stop() are the only callers of the audio ones, and the plug-in calls
sound_engine_start_hosted()/stop_hosted() instead - so these are compiled in but never reached.
They return failure rather than success on purpose: if a future change ever routes the plug-in
through sound_engine_start(), it will fail loudly and visibly rather than appear to open a device
that is not there.
