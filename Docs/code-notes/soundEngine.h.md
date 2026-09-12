# soundEngine.h notes

The longer comments from `soundEngine.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `sound_engine_active()`

A local sound engine, so the editor can make a noise on its own — most usefully offline, where
there is no G2 attached to hear a parameter change on.

FIRST MILESTONE, and the shape of it matters for what you should expect:

```
  - One module sounds: whichever single OscB is selected on the canvas. Cables are ignored
    entirely. This is not "the patch playing", it is "that oscillator playing".
  - Monophonic. One voice, last note wins.
  - The active variation only, and morphs are not applied.

```
The oscillator is an ordinary floating-point implementation using the usual published
techniques — polyBLEP correction at waveform discontinuities, and a plain phase accumulator.
It is not a model of the G2's own DSP and will not match it sample for sample. It reads the
same parameter curves the dials display (see osc_* in renderParams.h), so pitch and shape
agree with what the module shows.

KNOWN GAPS, deliberate at this stage:
```
  - Waveform "sup" is approximated by three detuned sawtooths, not the G2's own algorithm.
  - Shape applies to "squ" (pulse width) and "tri" (symmetry). It does nothing to "sin",
    "saw" or "sup" yet.
  - PitchType "Factor" and "Partial" are relative to a master oscillator, which has no meaning
    with cables ignored; both fall back to being read as "Semi".
  - "sin" and "tri" are not band-limited. Their partials fall away steeply enough (1/n^2 or
    better) that aliasing stays well down; "saw" and "squ", which need it, do get polyBLEP.
  - The pitch, FM, shape and sync inputs are all unconnected by definition, so their
    modulation-amount knobs (Pitch M, FM, ShpM) have nothing to act on and are ignored.
  - THE REVERB'S STEREO IS TWO TAP SETS ON ONE TANK, which is the instrument's own arrangement,
    but the tap POSITIONS are chosen rather than recovered. Scored by the peak of the L/R
    cross-correlation over lag — the measure that tells a decorrelated pair from a delayed copy,
    which reading correlation at lag zero does not — the engine sits at 0.124..0.159 against the
    instrument's 0.124..0.159, and its peak lag scales with the room as the instrument's does.
    What is not matched is WHERE that peak sits room by room, and the instrument's own lag is not
    a single number: repeated windows of one capture put Small at both ~677 and ~1250 samples, so
    there are at least two tap-distance clusters and only one capture to separate them.
  - THE REVERB'S BRIGHTNESS IS FITTED FROM 48 UPWARD AND EXTRAPOLATED BELOW IT. Sweeps in Small,
    Medium and Hall settle the dial over 48..112 to about 1.5 dB/s; below 48 the instrument's high
    band is in the noise and the engine's own decay fit returns nothing usable, so both sides stop
    measuring in the same place and the dark end over-damps. It wants a quieter capture of the
    dial's lower third, not another constant.
```

## 2. `sound_engine_render_reverb_ir()`

MEASUREMENT ENTRY POINT: renders the Reverb's impulse response alone, with no patch, no voice and no
audio device. Fills `frames` interleaved STEREO pairs at deviceRate * ENGINE_OVERSAMPLE — pass 48000
for the 96 kHz the hardware measurements are expressed in, so a delay length is the same integer in
both. Fully wet, and the delay lines are cleared first, so two renders in one process cannot bleed
into one another.

This exists so tools/render.c can put the SAME click through this code that tools/measure.py puts
through the instrument, and tools/analyse_ir.py can then produce the same numbers for both. The
reverb is mono, so the two channels are identical and their correlation reads 1.000 against the
instrument's +0.03 — the harness reporting the gap, not a fault in it.

## 3. `sound_engine_meters_dirty()`

The chorus, driven by a caller-supplied input so an engine render can be compared against a
hardware capture made from the same signal. Output is stereo interleaved at deviceRate *
ENGINE_OVERSAMPLE, as for the reverb IR above.
The meter the engine would show on a module's face, so the renderer can display what the engine is
actually doing rather than the last value the instrument sent. False when the engine is idle or has
nothing for that module; the caller falls back to the database.
Whether any engine-driven meter or LED has CHANGED since this was last called; consuming. The
render loop draws only when asked, so without this the meters moved only when the mouse did.

## 4. `sound_engine_set_output_level_db()`

A morph group's position, 0..1. The G2 has eight, each hard-wired to a source — group 0 is the
modulation wheel, and morphStrMap in moduleResources.h names the rest. Setting one sweeps every
parameter that has a morph range recorded for that group between its dialled value and its morph
target. Called from the MIDI thread.

Returns true if the position actually moved. Unlike pitch bend, a morph is not read by the audio
thread directly — it is folded into the parameter snapshot, which is only rebuilt on a redraw. So
a caller that moves a morph MUST ask for a redraw when this returns true, or the change will not
be heard until something else happens to wake the render loop.
Output attenuation in dB, 0 or negative. Applied before the output limiter, so it pulls a hot
patch down rather than leaving the limiter to do it. Positive values are treated as 0 — this is a
trim, and boosting into the limiter is what it exists to avoid.

## 5. `sound_engine_note()`

Note input. Called from the UI thread — see set_sounding_note() in virtualKeyboard.c, which is
the single point every note change goes through.

A note-off NAMES ITS NOTE: the engine releases the voice holding that note and leaves the others
alone. Passing note < 0 with on == false is all-notes-off. This matters now the engine is
polyphonic — a note-off that did not say which note could only ever mean "stop everything".

## 6. `sound_engine_debug_text()`

The resolved chain as the engine currently sees it — one line per node with the parameters it
actually read. For diagnosing "it looks right on screen but makes no sound": the usual causes are
a parameter read from the wrong variation, or a chain that resolved differently than it looks.
UI thread only.

## 7. `sound_engine_attach()`

── More than one engine (the plug-in) ──────────────────────────────────────────────────────────

The engine's state is banked, one bank per engine, and the calling thread's CURRENT DOCUMENT says
which bank it is using (globalVars.h). The application has one of each and never calls these.

Claims a free engine for the current document and resets it to silence; false when every engine is
in use. Pair with sound_engine_detach() when the document goes away.
