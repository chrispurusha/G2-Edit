# Sound engine implementation notes

The longer comments from `soundEngine.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tFilterParams`

Where each filter module keeps its parameters. They are NOT all laid out like FltClassic: FltLP
has no resonance at all, and its Slope sits one index earlier because of it. Reading a missing
parameter would take whatever the next one happens to be — for FltLP that would be Slope read as
resonance, i.e. a filter that self-oscillates because a menu index landed in a gain.

-1 for a parameter the module does not have.

## 2. `engine_no_free_run()`

G2_FILTER_LEGACY=1 restores the filter behaviour as it stood before 2026-08-30 — FltLP reading
its Slope from the Bypass parameter, and a maximum of four poles. It exists so the corrected
filters can be A/B'd by ear against the old ones without a rebuild, and so a regression can be
backed out in one environment variable rather than a revert.
THE VOICE AREA RUNS CONTINUOUSLY ON THE INSTRUMENT, and this makes the engine do the same.
Set G2_ENGINE_NO_FREERUN=1 to get the old note-gated behaviour back without a rebuild, the same
way G2_FILTER_LEGACY works.

## 3. in `filter_param_map()`

DESIGNATED INITIALISERS DELIBERATELY. These were positional, and adding slopeMode in the
middle of the struct silently shifted every one of them - slopeMode took what active meant
and active became 0, which switched the filter off rather than failing to build. Naming the
fields makes the next insertion harmless.

## 4. in `filter_param_map()`

Freq, FreqMod, Kbt, Bypass - no Res, and SLOPE IS A MODE, not a parameter.

This read .slope = 3 and .active = 4 until 2026-08-30. FltLP has FOUR parameters, so
that took Slope from param 3 - which is BYPASS - and the active flag from a param that
does not exist. The slope therefore followed the bypass switch, and only two of the
six slopes were ever reachable. modeLocationList had already moved Slope to a mode on
2026-08-15; this map was not moved with it.

## 5. `OUT_PARAM_DESTINATION`

"Out to" — where the module sends. NOT every Out module reaches the speakers: the other settings
are internal routing, and a patch commonly uses one to feed its FX area.
```
  2toOut, outToStrMap:     Out 1/2, Out 3/4, FX 1/2, FX 3/4, Bus 1/2, Bus 3/4  -> 0,1 audible
  4toOut, outTo4OutStrMap: Out, Fx, Bus                                        -> 0   audible
```

## 6. `SHPB_MODE_WAVEFORM`

The waveform selector is a MODE, not a parameter — the G2 keeps drop-down selectors out of the
parameter list entirely because, unlike a knob, they cannot be assigned to a morph group or a
controller and hold one setting across every variation (manual p.20). It lives in
modeLocationList, so it is read from module->mode[] and reading param[10] found nothing.

## 7. `SHPA_PARAM_TUNE`

OscShpA is OscShpB's sibling and shares its DSP, but two things differ and both were read off the
instrument rather than assumed. Its Waveform is a PLAIN PARAMETER, not a mode - a PARAMDUMP of a
freshly added one reports "modes count=0" where OscShpB has one - and its parameters run in a
different order, which the same dump pins exactly: 64 64 1 0 0 0 0 0 0 0 1 puts Kbt at 2 and the
power button at 10.

Its Wave menu is OscShpB's with DblSaw and Pulse removed: {Sine1, Sine2, Sine3, Sine4, TriSaw,
SymPulse} against {Sine1, Sine2, Sine3, Sine4, TriSaw, DblSaw, Pulse, SymPulse}. So 0..4 are the
same waveform in both and A's fifth is B's seventh - see kShpAWave.

## 8. `CLIP_PARAM_LEVEL_MOD`

SHAPER GROUP - Clip, Overdrive, Saturate, ShpExp, WaveWrap, ShpStatic and Rect (manual p.204-207).

Every one of these is MEMORYLESS: the output depends only on the present input sample, through
what the manual calls a transfer function and draws as a graph. That is why they arrive as one
node kind carrying a mode rather than as seven, and why they cost nothing to run at audio rate -
which is exactly what the G2 means by a control module promoted to audio.

THE ORDERS ARE NOT UNIFORM AND ARE NOT GUESSES WORTH REPEATING FROM MEMORY. WaveWrap lists its
modulation depth BEFORE its amount and its Mod jack BEFORE its In jack; Overdrive and Clip list
the mod dial first but the In jack first; Saturate and ShpExp list the amount first. Every one of
these came from the layout tables in moduleResources.h, and none is confirmed against the
instrument yet.

## 9. `DELAY_PARAM_TIME`

DelayB. Its range is a MODE, like the shape oscillators' waveform.
The two delays share their first four parameters but NOT their Bypass: DelayA has six parameters
with Bypass at 4, DelayB has nine with Bypass at 7 (and an extra HP at 8). Reading DelayB's index
on a DelayA lands past the end of its parameter list, reads zero, and silently bypasses it.

## 10. `DELAY_LP_MIN_HZ`

The LP dial's cutoff, swept exponentially across its travel — see where node->damping is set.

FITTED TO A BURST MEASUREMENT, which is how to measure anything inside a feedback loop: a short
burst of saw leaves the repeats separated in time, so each can be transformed on its own and
repeat[n+1]/repeat[n] IS the per-pass response, feedback and filter together. Normalising that to
its own flattest point leaves the filter alone.

```
    LP 127   flat within 0.2 dB from 0.5 to 15 kHz  -> wide open, cutoff at or above 20 kHz
    LP  64   -1.9 dB at 2.8 kHz, -3.2 at 3.7, -4.2 at 5.6  -> one-pole knee near 3.5 kHz
    LP   0   only two repeats survive at all        -> cutoff well down, most energy removed

```
660 Hz at the bottom puts fc(64) at 3.7 kHz, which is that knee. A CONTINUOUS tone cannot measure
this: the repeats overlap, and at high feedback the loop regenerates and the ratios stop meaning
anything — measured per-pass "gains" came out above unity at FB 100.

## 11. `DELAY_HP_LOG_A`

The HP dial's cutoff, as a QUADRATIC IN THE DIAL VALUE — log fc = a + b*hp + c*hp^2, not the
exponential the LP uses. That is not a preference, it is what the instrument does: an exponential
fitted to the two ends misses the middle of this dial by a factor of 2.4.

MEASURED with the burst method — short saw burst, repeats separated in time, repeat[n+1]/repeat[n]
per harmonic, each row referenced to the 6-12 kHz band which sits above every cutoff here:

```
    HP    32     64     96    127
    -3dB  92    841   2342   4561 Hz        fit error  +0.4  -1.1  +1.1  -0.4 dB

```
VALIDATED at three settings that were NOT used to fit it:

```
    HP    48     80    112
    meas 313   1208   3696 Hz               error      -0.6  +2.1  +0.1 dB

```
HP 0 is the filter switched out rather than its lowest cutoff — it measures flat within 0.9 dB.

The reference band matters more than it looks: taking it at 1.5-4.5 kHz, as a first attempt did,
puts the reference INSIDE the transition band at high settings, which flattens the measured curve
and hides the filter completely. HP 127 looked unmeasurable until the reference moved above it.

## 12. `tLfoParams`

The four LFO variants share a design but not a parameter order, so each one carries its own index
set. A -1 means the variant does not have that control at all: LfoC has no waveform selector and
no Kbt, and only LfoShpA has a Shape dial.

None of them has an output LEVEL. G2 LFOs emit at full scale and the DEPTH is set at the
destination — the oscillator's own Pitch modulation knob — which is exactly how a vibrato patch is
wired, and why no level parameter is read here.

## 13. `FLT_CONNECTOR_ENV_IN`

FltClassic's control input, the one beside its Env knob.

A RAW CONNECTOR INDEX, like every other entry in the connector maps below — cable_chain_node_from_
connector() indexes module->connector[] directly and derives the ioCount itself. FltClassic's
connectors run In(audio), Out(audio), In(control), In(control), so 2 is the first control input.
Briefly changed to 1 on the mistaken belief that these were input-direction indices; 1 is the
module's OUTPUT connector, which broke the filter outright.

## 14. `PITCH_MOD_SEMITONES`

A full-scale bipolar signal on an oscillator's Pitch input sweeps one octave either way: the
manual's own worked example (p.78) is an A4 modulated "up and down by one octave", and it stays an
octave whatever note is played, because a Pitch input modulates on the note scale rather than
linearly in frequency. That is a much smaller range than the filter's Env input above.

## 15. `type_ii_attenuator()`

The oscillators' Pitch mod-amount knob is an ATTENUATOR TYPE II — exponential, not linear. The
manual names the family ("the pitch mod-input on the various oscillators ... are examples of Type
II attenuation", p.79) and says what it means: "a setting of 50 attenuates the incoming signal by
a factor considerably less than 0.5". Reading the knob linearly, as this did, leaves roughly twice
the modulation at a half-open knob, which on a vibrato patch is the difference between a detune
and a siren.

SQUARED is the same approximation the mixer's Exp/dB curve already uses (see eNodeMix, which the
manual confirms is the same Type II scale). Exact at both ends — 0 "shuts off the modulation
completely", 127 "leaves the incoming signal unaffected" — and convex between them, which is the
shape described. The exact law is not stated numerically anywhere in the manual, and the knob has
no value display to read it off, so this is closer rather than right.

## 16. `OSCB_TUNE_UNITY`

The G2's Tune value is the MIDI note number of the oscillator's base pitch: value 0 is 8.1758 Hz
(note 0) and value 127 is 12.55 kHz (note 127), which is why the "Semi" display shows the value
minus 64 and the "Freq" display shows the same dial in Hz. So a single pitch calculation covers
both display modes, and 64 is the value at which the oscillator plays the note as struck.

## 17. `VOICE_GAIN`

Output trim. A busy patch — several oscillators into a mixer, a resonant filter, then a second
mixer summing dry against delays and reverb — genuinely reaches five or six times a single
oscillator's level, and that is the G2's own arrangement rather than anything wrong. So the trim
has to leave room for it: 0.15 puts a hot patch just under full scale instead of 3 dB into the
clipper, at the cost of a single-oscillator sketch being quieter.

## 18. `MAX_ENGINE_NODES`

The chain the engine renders. Small and fixed: these are hand-built sketches, not whole patches,
and a bound is what keeps the walk safe against a patch that feeds back into itself.
Raised from 12 once whole patches came into scope: a real one runs to a couple of dozen modules
across the Voice and FX areas.

## 19. `MAX_VOICES`

How many voices the engine can hold at once. The patch's own figure is what actually governs it —
see voice_count_for_patch() — and this is only the ceiling that sizes the state arrays. 32 is the
most the patch descriptor can ask for: voiceCount is a 5-bit field holding the count MINUS ONE.

COST IS PER SOUNDING VOICE, NOT PER ALLOCATED VOICE. Only voices actually producing sound are
rendered (see voice_is_finished()), so a 32-voice patch played one note at a time costs what the
monophonic engine cost. Holding 32 notes really does cost 32 times as much, which no amount of
arranging avoids — it is 32 copies of the Voice Area.

## 20. `VOICE_MAX_TAIL_SECONDS`

How long a voice may go on sounding after its key is up and its envelope has finished, before it
is faded out and taken back. A patch whose EnvADSR modulates only the filter never stops on its
own — correct, and what the hardware does, but on a soft synth it means every voice the patch owns
stays in the render for ever, and the cost of that is permanent rather than while you are playing.
The fade is what makes taking it back inaudible; without one this would be a click.

## 21. in `type_ii_attenuator()`

What feeds each input: the node index, and WHICH of that node's outputs the cable came from.
The output matters because a module can offer more than one — an EnvADSR's first output is the
envelope itself and its second is the audio it has shaped, and a cable to one means something
quite different from a cable to the other.

## 22. `MAX_ENGINE_TAPS`

A patch can hold more than one Out module — SimpleLead has two, a Voice Area output carrying the
dry voice and an FX Area output carrying the delays and reverb — and on the hardware they SUM at
the sockets. Tapping only the first one silently drops the other, which on that patch means
hearing the effects with no dry signal underneath them.

## 23. in `type_ii_attenuator()`

Published by the UI thread, consumed by the audio thread, via a seqlock: the writer makes the
sequence odd before touching the snapshot and even again after, so a reader that sees an odd
sequence — or a different one either side of its copy — knows it read during a write. Neither
side ever blocks, and the audio thread never waits on the UI thread. A plain pair of buffers
would not do: the UI can publish twice while one audio buffer is being filled, which is long
enough to land back on the buffer the audio thread is mid-copy of.
── ONE BANK OF STATE PER ENGINE ────────────────────────────────────────────────────────────────

Every piece of engine state below is an array over engines, and SE says which one the calling
thread is working with. The names are macros onto the current engine's element, so the DSP reads
exactly as it did when these were plain globals - which they were until 2026-09-11, and which is
what made G2 Alike one instance per process.

SE IS THE CURRENT DOCUMENT'S ENGINE (globalVars.h). A plug-in instance claims one with
sound_engine_attach() and selects its document at every entry, on whatever thread the host calls
from; the application has exactly one engine and one document, so there SE is the constant 0 and
this costs nothing at all.

PERFORMANCE MODE IS WHY IT IS AN INDEX. A performance plays four slots at once, so an instance
will one day hold four engines, each bound to one slot (sound_engine_bind_slot()) and mixed. That
is not built yet: today an engine plays whichever slot is selected.
READ ONCE PER CALL, NOT PER ACCESS. SE names a LOCAL (tEngineIdx) that every function touching engine
state declares on entry with SE_LOCAL. It used to be gDoc->engineIndex itself, which put a
thread-local lookup and a reload on every banked access - dozens per sample - and cost the plug-in
build 13% against the application (2.41 s vs 2.13 s CPU for 30 s of a 4-voice chord). A local the
compiler can keep in a register costs nothing after the first read.

THERE IS NO FILE-SCOPE tEngineIdx, deliberately: a function that touches a bank without SE_LOCAL
does not compile, so none can quietly read the wrong engine. In the application both vanish -
there is one engine and SE is the constant 0.

## 24. `gParamsWriteMutexBank`

SERIALISES WRITERS ONLY. The audio thread never takes this — it is the seqlock's reader and stays
lock-free, so there is no priority inversion to worry about.

A seqlock tolerates exactly one writer, and for a long time there was one: the render thread,
rebuilding the snapshot every frame. That is what forced a morph to go the long way round —
sound_engine_set_morph() records the position, but only a rebuild folds it into what the audio
thread reads, so the MIDI thread had to ask for a REDRAW and wait for it. Mod wheel response was
therefore capped at the frame rate, with a full canvas repaint sitting between the wheel and the
sound.

With writers serialised here, any thread may rebuild. The MIDI thread now does so immediately on a
morph change (midiInput.c) instead of waiting to be drawn.

## 25. `NOTE_QUEUE_SIZE`

Note events queue up here rather than being a single "current note" the audio thread samples once
per buffer. Two things were wrong with that: the note only took effect at a buffer boundary, which
is audible jitter at any sensible buffer size, and if two events landed inside one buffer only the
last survived — so fast playing dropped notes.

Written by the MIDI thread and the UI thread, drained by the audio thread. Multiple producers, one
consumer: the write index is claimed with a fetch_add so no two producers take the same slot, and
each slot publishes its own sequence number afterwards so the consumer can tell a slot that has
been claimed from one that has actually been filled in.

## 26. `METER_VALUE_MASK`

Pitch bend as it arrives, -1..+1. Scaled to semitones by the patch's own Bend range at render
time, so changing the range takes effect without the wheel having to move.
Output attenuation, as a gain x1000 so the audio thread reads one atomic rather than calling pow.
Applied BEFORE the output knee, which is the point of it: pulling a hot patch down so the limiter
stops being the thing that controls the level.
METERS THE ENGINE PRODUCES, for the module faces to show while it is playing. Indexed by location
and module index rather than by node, so a reader needs neither the snapshot nor a lock: the audio
thread stores, the UI thread loads, and the worst a race can do is a meter one frame old.

THE SAME 8-BIT VALUE THE USB STREAM CARRIES, deliberately - usbComms.c reads volumes as an 8-bit
field and the renderer already knows how to draw one, so the engine's meter needs no new path and
no new drawing code. For the compressor that value is a BAR, (1 << lit) - 1, which is exactly what
the instrument sends: 1, 7, 31, 63, 127, 255 were read off it.

A WRITTEN FLAG PACKED WITH THE VALUE, in one word, and both parts matter.

The FLAG rather than a sentinel value, because the value has to stay byte-identical to what the USB
stream carries: the whole point of metering from the engine is to be able to put the two side by
side, and a value shifted by one to make room for a sentinel could not be compared without
remembering to undo it. METER_VALUE_MASK is the 8 bits usbComms.c reads; METER_WRITTEN sits above
them.

ONE WORD rather than two, because the flag and the value must be read from the SAME store. Split
across two atomics a reader could take the flag from one update and the value from another, and
find a meter that says "valid" carrying a number from a different moment.

ONLY THE COMPRESSOR SO FAR: its meter is the one whose meaning has been measured.

## 27. `gMetersDirtyBank`

SET WHEN A PUBLISHED METER OR LED VALUE ACTUALLY CHANGES, and read by the render loop, which
only draws when something asks it to (see synthlib_request_redraw). Without this the meters moved
only while the mouse did: the audio thread was updating the arrays perfectly well and nothing was
telling the GUI to look at them, so a meter tracked the pointer rather than the sound.

A FLAG RATHER THAN A REDRAW REQUEST FROM HERE. synthlib_request_redraw() is safe from any thread,
but it calls glfwPostEmptyEvent(), and doing that once per audio block is a syscall on the audio
thread several hundred times a second. Setting a relaxed atomic costs nothing, and the loop is
already awake on a timeout whenever the engine is running.

COMPARED, NOT SET BLINDLY. The publish happens every block whatever the value, so setting this
unconditionally would hold the GUI at the tick rate for as long as the engine was on, silence
included. atomic_exchange gives the old value back for free, so the comparison is one operation.

## 28. `gModuleLedBank`

AND THE LEDS, same packing and same reasoning. A module's LED value is a two-bit field - bit 0
green, bit 1 red - so the mask below covers it with room to spare and the value stays exactly what
parse_led_data() would have put there.

ONLY THE LFOs SO FAR. Their LED was MEASURED rather than assumed (2026-09-07): polled against
LEDDUMP's own timestamps at Rate Lo 60, it ran at 0.5226 Hz against a predicted LFO rate of
0.5110 Hz with a 53% duty cycle - so it simply follows the SIGN of the LFO output, on for half the
cycle, and it is green rather than red.

## 29. `ENGINE_OVERSAMPLE`

Audio-thread-only state. Nothing else may touch these.
THE WHOLE GRAPH RUNS OVERSAMPLED, which is what the G2 does: its audio rate is 96 kHz against a
typical 48 kHz output (manual p.71). Two things need it and cannot get it any other way — the
ladder filter, whose model stops holding as its poles approach Nyquist, and any nonlinearity,
whose harmonics fold back down if they are made too close to the output rate.

Doing it for the WHOLE graph rather than per node is both simpler and better: there is no input
to interpolate for each nonlinear node and no per-node decimator, just one filter at the very
end. The linear parts (mixers, amplifiers, delay, reverb) gain nothing from it but cost little,
and having one rate throughout means nothing has to know it is happening.

## 30. `tVoice`

── VOICES ──────────────────────────────────────────────────────────────────────────────────────

One of these per simultaneously sounding note. Everything here used to be a single global, which
is what made the engine monophonic — not any shortage of oscillators, just one copy of "which note
is playing and is its key still down".

`note` OUTLIVES THE GATE. A released voice is still sounding its release, and that release has to
stay at the pitch it was played at, so the note is only cleared when the voice is taken for
something else.

## 31. `gEngineVoicesBank`

Published for the note stack, which has to know whether to release the note it was given or to
fall back to the newest one still held. An atomic rather than a look into the parameter snapshot:
it is read from the MIDI thread, and copying the whole snapshot to answer one question would be
absurd. See sound_engine_is_polyphonic().

## 32. `gLoadPercentBank`

RENDER LOAD, as a percentage of real time, peak-held. The time spent inside sound_engine_render()
against the time the buffer it filled will take to play: at 100 % the engine is using the whole of
its deadline and the next buffer is late, which is heard as crackling rather than as anything
musical. Peak-held because the interesting figure is the worst buffer, not the average — one late
buffer in a hundred is plainly audible and would vanish into a mean.

## 33. `OSC_OVERSAMPLE`

Per-node state, indexed by node position. Carried across snapshots while the topology signature
holds, so turning a knob does not restart the oscillator or reopen the envelope.
Oversampling for the oscillator section. The G2 runs its audio at 96 kHz against the 48 kHz
typical here, so 2x alone would match the hardware's rate; 4x is used because polyBLEP's residual
error falls with the square of the phase step, and the shape waveforms have no band-limiting of
their own at all and depend entirely on this.

The two numbers are not independent, and the filter is the one that matters. Measured on a
sawtooth at C7 (the worst case in the audible range), residual aliasing went:

```
    32 taps -34 dB | 64 taps -43 dB | 128 taps -71 dB | 256 taps -73 dB

```
and 8x oversampling at 256 taps measured the same as 4x at 128 — what counts is the transition
width, which is taps DIVIDED BY the oversampling factor, so doubling the rate without doubling the
filter buys nothing and costs twice the arithmetic. 4x/128 sits at the knee: the hardware capture
this was matched against measures about -32 dB, so the engine is now well clear of it.

Decimation only computes the samples it keeps, so the cost is 128 multiply-accumulates plus four
waveform evaluations per oscillator per output sample.
Relative to the ENGINE rate, which is itself oversampled — so the oscillators still run at four
times the device rate overall, as they did when the graph ran at the device rate and this was 4.

## 34. `OSC_DECIMATE_TAPS`

48, not the 128 this began with, and the difference is CPU rather than taste. This filter is the
engine's single largest cost — it runs once per oscillator per voice per oversampled sample, so
its length is multiplied by the polyphony, and at eight voices 128 taps was enough on its own to
miss the audio deadline (CoreAudio reports that as "skipping cycle due to overload", heard as
crackling).

MEASURED, worst image folding back into 0..20 kHz when decimating 192 kHz to 96 kHz, against the
deviation the filter causes inside that band:

```
    128 taps  -116 dB   0.00 dB      48 taps   -90 dB   0.00 dB
     64 taps   -98 dB   0.00 dB      32 taps   -82 dB   0.00 dB

```
The passband is untouched at every length because the cutoff sits at 43 kHz, far above anything
audible — the taps buy stopband depth alone. 90 dB is below the noise floor of any playback path
this will meet, so the remaining 26 dB was being paid for in CPU and heard by nobody.

## 35. `gOscHistoryBank`

── PER-VOICE NODE STATE ────────────────────────────────────────────────────────────────────────

The Voice Area is instantiated once PER VOICE on the hardware and the FX Area once for the whole
patch (manual p.85: "you don't need a separate Reverb in each voice, all voices can share one
Reverb module in the FX Area"). So everything a Voice Area module remembers between samples —
oscillator phase, filter poles, envelope stage — has to exist once per voice, or two notes held
together share one oscillator phase and one envelope and behave as one.

Indexed [voice][node]. The voice index is 0 for everything in the FX Area, which is evaluated once
after the voices have been summed.

NOT per voice, deliberately: the delay lines, the chorus lines and the reverb. They are the large
buffers, they are FX modules, and one shared instance is what the hardware has. A patch that puts
one of them in the VOICE area gets a single shared instance rather than one per voice — an
approximation, and the only one in this split.
FLOAT, not double, and for the same reason the tap count came down: at eight voices this array is
walked a few million times a second and the loop is bound by how fast it can be read rather than
by the arithmetic. Halving the bytes halves that. The accumulation is still done in double.

## 36. `DELAY_LINE_SAMPLES`

Long enough for the longest range the Time dial offers (2.7 s), at the INTERNAL rate. It used to
be a flat 48000, i.e. one second at 48 kHz — so the top of the dial was silently truncated to
well under half the delay it promised.
2.8 s at 48 kHz, times the oversampling — integer arithmetic so it stays a constant expression an
array can be sized with.

## 37. `CHORUS_PHASE0`

The chorus's own short sweep, plus its LFO phase. TWO LINES PER NODE: the instrument runs left and
right through the same algorithm with their LFOs in ANTIPHASE, so one phase accumulator serves
both — the right channel simply reads it half a cycle along. See chorus_step().
WHERE THE LFO RESTS, which only shows at Detune 0 - where the instrument does not sweep at all but
holds the two taps 2.4425 ms apart with their centre at 2.856 ms (measured). That separation puts
the triangle at |T| = 0.5343, and the centre picks the sign: +0.5343 predicts a centre of 2.860
against the measured 2.856, where -0.5343 would give 2.494. Starting at phase 0 instead - i.e.
T = -1 - left the static comb 4.571 ms wide rather than 2.44, notches every 219 Hz instead of 410.
For any other Detune the LFO free-runs and the starting phase does not matter; both channels take
it together, so the quarter-cycle L/R relationship is untouched.

## 38. `REVERB_COMBS`

A Schroeder reverb: eight combs into three allpasses. One reverb is modelled; any further ones pass
their input through, which is what a patch with two of them would mostly sound like anyway.

SIXTEEN COMBS SINCE 2026-08-18, AND THE SHORTAGE IS WHY IT FLUTTERED. This bank started as
Freeverb's, which specifies eight; only the FIRST FOUR were ever here, so the tank ran at a quarter
of the density it now has. A comb is a periodic echo generator, and too few of them means the tail
is not a decay at all but a train of discrete echoes at the comb recirculation rates.

MEASURED AGAINST THE INSTRUMENT, same patch and settings (Hall, Time 122, Bright 64), using the
MODULATION SPECTRUM of the tail's envelope — which is what "flutter" actually names, and the only
metric tried that separated the two. Envelope ripple and echo density both said the engine was
FINE, and both were wrong:

```
    hardware    flat: nothing above 2.9% anywhere from 5 to 58 Hz
    4 -> 8      5.4% at 18.2 Hz, standing clear of everything around it. 18.2 Hz is a 55 ms
                period against Hall's longest comb of 56.6 ms — one comb ringing on its own
    + damping   4.4%, no longer clear of its neighbours (see comb_damping)
    16 combs    3.6% and FLAT, with the 18 Hz peak gone entirely

```
The other eight lengths continue Freeverb's progression and are prime, so no two lines reinforce.
THE INSTRUMENT HAS ABOUT 31 DELAY LINES (see Docs/todo.md), so sixteen is still short of it —
but it is now dense enough that the flutter does not survive the measurement.

## 39. `REVERB_MODE_TYPE`

The Reverb's TYPE selector — Small, Medium, Large, Hall (reverbTypeStrMap) — is what sets the size
of the room, and it was not read at all: all four types sounded identical, which is most of why
this reverb does not sound like the instrument's. It is a MODE, not a parameter, so it comes from
module->mode[] like OscShpB's waveform does.

## 40. `REVERB_DAMP_MAX`

Decay time against the Time dial, MEASURED per room type 2026-08-09: seconds = base + slope * value,
with value the raw 0..127. See the long note at the point of use for the measurements, for why the
slope is not simply proportional to room size, and for why these are early-decay figures rather than
true RT60.

This replaced "Range: 1.1 ms to 17.58 s" (manual p.251) driven through a cubic whose exponent was
fitted by ear. The endpoints were the only documented part and the measurement does not reach them:
Large tops out at 8.11 s. The manual's figure is left recorded here because it is still unexplained,
not because it is unused — nothing reads it now.
How hard the Brightness dial damps the comb loop. The one-pole coefficient is _MAX times brightness
raised to _CURVE, and both are FITTED against nine measured points of the instrument's own dial.

WHAT TO MEASURE THIS AGAINST, because two other metrics sent me the wrong way first. The right target
is the ratio of HIGH-BAND to LOW-BAND DECAY RATE, which is what an in-loop lowpass actually controls.
Hall at Time 127, decay of 3-10 kHz over decay of 150-800 Hz:

```
    Brightness       16     64    127
    hardware       0.54   0.75   0.95
    engine         0.53   0.70   1.00     (with the constants below)

  - Broadband decay is NOT the target: it follows whichever band holds the energy, so it agreed with
    several quite different filters. It also made the engine look 40-60% short of its decay target
    when the low band was in fact within a second of the hardware; the shortfall was the measurement.
  - Absolute tail COLOUR at a fixed moment is not the target either: scored that way, an exponent of
    1.0 beat 0.15, which is the opposite of what the decay rates say. Colour at an instant mixes the
    loop's damping with everything outside the loop, so it cannot isolate this coefficient.

```
BRIGHTNESS, FITTED ACROSS THREE ROOMS (2026-09-07). A per-pass one-pole coefficient, and the dial
maps to it exponentially: on the instrument the amount by which the top decays faster than the
bottom falls by a roughly constant factor every sixteen dial steps.

THE ROOM DEPENDENCE IS REAL AND MUST NOT BE NORMALISED AWAY. The same dial position damps far
harder in a small room, because the filter runs once per pass and a Small room's lines are 1.68x
shorter, so they are traversed that much more often per second. MEASURED on the instrument, HF
excess in dB/s at Brightness 64:

```
    Small -17.1     Medium -12.5     Hall -8.9        i.e. 1.92 / 1.40 / 1.00

```
A per-pass coefficient reproduces that for free — it gives 1.80 / 1.35 / 1.00. THIS WAS TRIED THE
OTHER WAY AND IT WAS WRONG: reasoning that "one dial should mean one thing", the coefficient was
once solved from a target dB PER SECOND so the loss per second came out equal in every room. That
is defensible physics and does not match the instrument — it flattened the spread to 1.39 / 1.26 /
1.00 and left a Small room audibly under-damped. The instrument's dial sets a coefficient, not a
rate. Do not re-derive this; the sweeps that settle it are g_small_bright7, g_medium_bright7 and
g_hall_bright9.

```
    dial            48     64     80     96    112
    instrument   -29.0  -17.1   -9.2   -6.8   -4.3     Small,  dB/s, 8 kHz minus 125 Hz
    engine       -28.4  -12.8   -8.9   -5.8   -3.3
    instrument   -15.2   -8.9   -5.0   -2.9   -1.5     Hall
    engine       -14.2   -7.1   -4.3   -2.5   -1.7

```
RMS error 1.5 dB/s over three rooms and five dial positions. The weakest point is Brightness 64,
under-damped in every room, which says the dial's shape is not quite a pure exponential — but five
points per room will not settle what it is instead, and fitting harder here would be fitting noise.

BELOW BRIGHTNESS 48 NOTHING CAN BE FITTED, on either side. The instrument's high band is in the
noise there and the engine's own decay fit returns nothing usable, so both stop measuring in the
same place. The ceiling governs that region and is a guess.

REFIT WHENEVER THE LOOP'S HIGH-FREQUENCY BEHAVIOUR CHANGES: these constants absorb whatever else
costs high frequency per pass, which is how replacing the linear interpolator with a Hermite one
invalidated the previous pair. Render a Brightness sweep per room and read 8 kHz minus 125 Hz.
REFITTED 2026-09-07 against FOUR rooms and six dial positions, replacing a fit made on three rooms
and five. The engine was UNDER-DAMPED everywhere - at Brightness 64 it lost roughly half the high
end the instrument does (Medium 6.6 dB/s of excess against 10.3), and at 16 about a third
(18.7 against 56.3). The old CEILING of 0.9 was also biting from dial 19 downwards, which is why
the engine's excess went NON-MONOTONIC at the bottom of the dial where the instrument's does not.

FITTED THROUGH THE FILTER, not by scaling the dial constant. The measured quantity is excess decay
in dB/s, which is passes-per-second times the one-pole's per-pass loss; passes-per-second is fixed
by the room, so the RATIO of measured to rendered excess gives the ratio of per-pass losses
directly, and |H(w)| = (1-a)/sqrt(1 - 2a cos w + a^2) inverts that to the coefficient the
instrument implies. Doing it that way is what let four rooms agree: the implied coefficients at
dial 32 are 0.780 / 0.783 / 0.718 / 0.702 across Small / Medium / Large / Hall, where the raw
dB/s figures differ by a factor of two between those rooms.

```
    dial                      24      32      40      48      64
    implied                 0.5475  0.4866  0.4137  0.3718  0.2742
    this law                0.5526  0.4811  0.4189  0.3648  0.2766

```
THE FOUR ROOMS AGREE, which is what says the model is right rather than merely fitted: at dial 32
they imply 0.4566 / 0.5263 / 0.4921 / 0.4715 for Small / Medium / Large / Hall, within 8% of each
other, from raw dB/s figures that differ by 60% between those rooms.

MEASURE THE BANDS WITH A SHARP FILTER OR THE ANSWER IS THE FILTER'S. A single Q=4 bandpass at 8 kHz
leaks enough of the much louder, slowly-decaying low band that the measured HF decay FLOORS OUT:
scored that way the engine's excess ran 11.4 dB/s at dial 16 and 7.1 at 64, a range of 1.6x, where
two cascaded Q=8 sections on the same renders give 59.9 and 17.4, a range of 3.4x. Both sides of
the comparison have to use the same filter, and it has to be sharp enough that the number belongs
to the band it names. A per-pass coefficient is what
makes that collapse.

THE CEILING NO LONGER BITES. The old curve was far too steep - 0.90 at dial 16 against an implied
0.63, and 0.2165 at 64 against 0.2898 - so it over-damped the bottom of the dial and under-damped
the top, and the 0.9 clamp cut in below dial 19, which is why the engine's excess went
NON-MONOTONIC at the bottom where the instrument's does not. This curve peaks at 0.8024 at dial 0
and never reaches the ceiling, so that artefact is gone.

A WRONG TURN WORTH RECORDING. Fitting from the RATIO of measured to rendered excess gave 0.99 at
dial 16 and blew the whole tail up - the rendered excess it was divided by was itself saturated by
the old ceiling. It also assumed the 500 Hz band is untouched by the filter, which fails once the
coefficient is large: at 0.99 the one-pole corner is 154 Hz and the BASELINE decay went from 26 to
134 dB/s. Calibrate passes-per-second from a dial position where the coefficient is small, then
invert each measurement against the filter absolutely.

## 41. `kReverbDecayBase`

RE-FITTED 2026-08-18 FOR THE NEW STRUCTURE. The old 0.15 was fitted against a comb bank, where
the damping sat inside every comb's own loop and bit hard. In a feedback network the signal passes
the damping once per circuit instead, so the same exponent barely moved the tail at all: the
high-to-low decay ratio measured 0.89 / 0.94 / 0.96 at Brightness 16 / 64 / 127 against the
instrument's 0.54 / 0.75 / 0.95, i.e. the dial did almost nothing. At 0.70 it reads
0.51 / 0.83 / 0.96. The ends are close; the middle is still about 0.08 too bright, which says the
dial's shape is not a pure power law on this structure.

## 42. `REVERB_DIFFUSE_SLOPE`

The allpass diffusion coefficient rises with the reverb time and is held between two limits. The
slope and the limits are the instrument's; what drives them is normalised Time here, which is the
part that is inferred rather than known — but the limits are close enough together that the whole
range is only 0.45..0.62, so being wrong about the position within it is a small error and being
outside it would not be.

## 43. `kReverbTypeScale`

How much bigger each type's room is than the base set below. THE SHAPE OF THIS IS RIGHT: the
instrument really does scale every one of its delay lines by a single factor per type, so one
number per room is the correct form rather than a convenience.

MEASURED ON THE HARDWARE, 2026-08-09, and the four numbers are no longer guesses. A click was fed
through the Reverb at 192 kHz with the dry impulse captured on a second output pair, and the tail's
autocorrelation gives the lengths the tank recirculates at (tools/measure.py, tools/analyse_ir.py).
Fitting ONE scale per room against every lag Small shows lands within 0.1% on the strong ones:

```
    Small  2408 -> Large  3669  (predicted 3673.4, -0.12%)   -> Hall  4042  (4044.2, -0.06%)
    Small  2422 -> Large  3695  (predicted 3694.8, +0.01%)   -> Hall  4064  (4067.7, -0.09%)
    Small  4215 -> Large  6430  (predicted 6430.0, +0.00%)   -> Hall  7078  (7079.1, -0.02%)
    Small  4599 -> Large  7016  (predicted 7015.8, +0.00%)   -> Hall  7723  (7724.0, -0.01%)

```
Four independent lengths agreeing with a ONE-parameter fit to a hundredth of a percent is not a
coincidence, and it settles the assumption as well as the numbers: the instrument does scale the
whole tank by a single factor. The short diffusion lags scale by the same factor (Small 216 ->
Large 329, exact; Small 576 -> Hall 965, -0.25%), so it is the room and not just the tail.

SMALL IS THE REFERENCE (1.0), not Large, because Small is the room whose lengths were measured most
completely — the base table below is Small's. See [[project_g2_reverb_measurements]].

## 44. `REVERB_COMB_BASE`

Mutually prime lengths, so the combs do not reinforce each other into a ringing tone. The buffers
are sized from the longest of each set rather than a hand-written number — getting those out of
step is a buffer overrun, and it is the kind that only shows up as a crash much later. The base
set is scaled with the rate (these are sample counts, so leaving them fixed would halve the room)
AND by the type, hence the extra headroom for the largest type in the two MAX figures.

THE BASE SET IS STILL NOT THE INSTRUMENT'S, and now that the scale above is measured it is the only
part of the room that is not. Measured Small recirculates at 2376, 2408, 2422, 4215 and 4599 samples
at 96 kHz on Out 3 alone — a tight cluster of three plus two lines at roughly 1.8x — where this base
set is four lengths spread over 1.21:1 and nothing long. It is deliberately NOT swapped for the
measured numbers yet: three lengths within 2% of each other in a PARALLEL COMB BANK beat against
each other, so the measured set only makes sense in a network that feeds each line from the others,
and the measurement does not say which topology the instrument uses. Loading them into this
structure could easily sound worse while matching the numbers better.

The scale fix stands on its own, though: it is a ratio, so it is right whatever the base set is, and
it moves Small from half the room to the whole of it.

## 45. `REVERB_SPREAD`

INTEGER ARITHMETIC, NOT A CAST OF A FLOAT PRODUCT. These size static arrays, and an array bound has
to be an integer constant expression — `(uint32_t)(base * 1.6)` is not one, so clang accepted it only
as a GNU extension, "variable length array folded to constant array". A static VLA is not something
to leave resting on an extension.

x18/10 COVERS REVERB_SCALE_MAX (1.6795) WITH ROOM TO SPARE, and it must: this pair and the scale
table are one decision in two places, so a scale raised without raising this writes past the end of
every delay line. It was 16/10 when the largest type was exactly 1.6, which the measured 1.6795 then
silently outgrew by 66 samples per comb. Rounding up rather than tracking the scale exactly costs a
few kilobytes and removes the trap.
THE STEREO SPREAD: the right channel runs the SAME topology with every line lengthened by this.

Deliberately a spread rather than the instrument's own measured lengths. Its two output channels
are near-disjoint tap sets (Small L 2338/2407/2420/4214/5022 against R 2378/2904/3903/3972/4046)
but its tank has about 31 lines against this one's 4 combs and 3 allpasses, and the ROUTING is not
recoverable — a real set of lengths in the wrong arrangement sounds plausible and is wrong, which
is the hardest kind of error to find. See the REVERB entry in Docs/todo.md.

So this claims no new structure. It is Freeverb's own answer to the same question, and what makes
it honest is that the thing it is aimed at IS measured: the instrument's two outputs correlate at
+0.012..+0.045, so 0.03 is the target, and tools/render + analyse_ir.py read the same number off
this code. Tuned against that — see the note in sound_engine_render_reverb_ir().
THE RIGHT CHANNEL READS ITS TAPS EARLIER, NOT LATER. The instrument's two outputs were measured
1.14 ms apart with the RIGHT one arriving first -- 12.89 ms against 11.75 in the Small room, and
the same gap in each of the other three -- so this is SUBTRACTED from the right channel's tap
offsets. 110 samples is that gap at 96 kHz. It used to be added, which put the right channel on
the wrong side of the left; no amount of correcting the magnitude would have found that.

## 46. `REVERB_PREDELAY_MAXSAMP`

PER-CHANNEL PRE-DELAY, MEASURED PER ROOM ON THE HARDWARE 2026-08-18. The instrument's two outputs
do not start together, and this engine had no pre-delay at all, so both tails began at the input.

THIS IS THE ONE LINE IN THIS REVERB THAT DOES NOT FOLLOW THE ONE-FACTOR RULE. Every other length
here is a room-size factor times a constant (see kReverbTypeScale), and an earlier version of this
table assumed the pre-delay was too. It is not: across the four rooms the left pre-delay moves only
12.90 -> 13.33 ms, a 3.3% spread, where kReverbTypeScale spans 68%. Scaling it would have put Hall
at 21.67 ms against a measured 13.33. So these are eight independent numbers, not two and a factor.

AND THE OLD RIGHT-CHANNEL FIGURE WAS SIMPLY WRONG — recorded as 7.54 ms, actually 11.75 ms. At
7.5 ms both channels are still in the noise (3.8-6.7% of peak); the right channel leaves it at
11.50 ms and the left at 12.75 ms, read sample by sample off the raw capture rather than through a
threshold. So the two channels are about 1.15 ms apart, not 5.34.

MEASURED TWICE, INDEPENDENTLY, AND THEY AGREE TO 0.06 ms: once from the stored 192 kHz Fireface
captures (tools/ rig, Time sweeps read at Time 0 where the tail clears between impulses) and once
live into a QU-24 at 48 kHz. Pre-delay is a fixed line length — Type sets the delay lengths, Time
only the feedback gain — which the measurement confirms: the same figures come back at Time 0, 32
and 64.

Expressed as sample counts at the 48 kHz base rate, times ENGINE_OVERSAMPLE, for the same reason
the comb lengths are: an array bound has to be an INTEGER constant expression. Sizing one with a
float cast is what produced the -Wgnu-folding-constant pair recorded in Docs/todo.md.

```
                       Small   Medium   Large    Hall
    left    (ms)       12.89   13.05    13.30    13.36
    right   (ms)       11.75   11.90    12.14    12.20
```

## 47. `RV_OUTTAPS`

─── THE INSTRUMENT'S OWN REVERB STRUCTURE ──────────────────────────────────────────────────────

Recovered 2026-08-18 and rebuilt here. It is NOT a bank of parallel combs, which is what this used
to be and why no amount of tuning ever made it sound right: a comb generates a periodic echo at
its own rate, and with every line between 23 and 34 ms that periodicity is audible as flutter,
worst at the end of a tail where the density thins.

The instrument is a serial allpass diffuser feeding a feedback delay network, read by a set of
fixed output taps. Its lines span 1.1 ms to 235 ms; ours spanned 23 to 34 ms and nothing else,
which is the whole difference.

THE LENGTHS ARE EXACT, in samples at the base rate for the Small room, scaled by kReverbTypeScale
for the others. What is inferred rather than measured is how the mixing stages are wired to each
other — the tap set, the stage pairings and the coefficients are all recovered.

## 48. `tRvSpan`

ONE TRIP IS BOTH BRANCHES, since each feeds the other: the two branch lengths added. In samples
at 96 kHz, and NOT scaled by the room — every span scales together, so the trip scales with it,
which is why a Hall rings longer than a Small room at the same Time setting.
Nothing shared here any more: each line gets its own decay gain from its own length, worked out
in gRvGain below. A single figure for the whole tank is what a series loop needs, and this is not
one.

## 49. `tRvSpan`

THE LAYOUT. Spans laid end to end, each one a line; a section writes at its own base and reads at
the next, so these lengths ARE the delays. Every length is the instrument's, recovered from the
spacing of its tap addresses: the allpasses at 672, 738, 666 and 812, the lines at 2300, 2456,
3999 and 5326.

## 50. `RV_MOD_DEPTH`

MODULATION DEPTH, in samples at 96 kHz, and the rate each line sweeps it at.

A TANK WITH FIXED DELAYS HAS FIXED MODES, and fixed modes ring -- that is what a metallic reverb
is. Measured as how much a tail's magnitude spectrum resembles itself a moment later, over
400 Hz to 4 kHz and at matched resolution:

```
                     0.05 s  0.15 s  0.35 s  0.75 s  1.50 s
    the instrument    +0.31   +0.23   +0.26   +0.19   +0.34
    fixed delays      +0.74   +0.74   +0.73   +0.76   +0.73

```
The instrument's fine structure is somewhere else a twentieth of a second later and stays that
way; a fixed tank is still three-quarters itself a second and a half on. Sliding each line a few
samples breaks the modes up without moving anything the ear hears as pitch: 16 samples at about
1 Hz is a peak shift near 0.9 cents, and on the shortest line it is 2.4% of its length.

THE RATES SHARE NO SIMPLE RATIO, for the same reason the line lengths do not -- eight sweeps that
realign every cycle would put their own period into the tail, which is the fault being fixed.

## 51. `RV_MOD_LOSS`

WHAT THE SWEEP COSTS THE DECAY -- WHICH TURNS OUT TO BE NOTHING MEASURABLE.

Reading a delay line at a fractional position interpolates between two samples, and linear
interpolation is a mild lowpass, so every pass round the tank might lose a little that a
whole-sample read would not. This constant existed to give that back, at 0.9955.

IT WAS COMPENSATING FOR A DIFFERENT BUG. The measurement that produced 0.9955 was made while the
Brightness detent was applying 0.021 of low-frequency damping inside the loop at every normal
setting -- see the tilt mapping in reverb_step(). The tail really was short; the interpolator was
not why. With the detent landing on zero as it should, the engine delivers the RT60 its dial asks
for with NO compensation at all:

```
    dial Time            40     64     90    127
    rendered / asked   0.975  1.010  0.990  0.978    at 1.0000, this value
                       1.037  1.125  1.150  1.221    at 0.9955, the old one
                       0.943  0.954  0.916  0.869    at 1.0027, overshooting the other way

```
Within 2.5% across the whole range, against 4% to 22% before, and the instrument matches its own
law to 0.1-0.8% by the same measurement. A residual that GREW with the requested time was the tell:
that is a fixed per-pass gain error, not anything the interpolator does.

KEPT AT 1.0 RATHER THAN DELETED so the question stays asked. If a future change to the modulation
depth or the interpolator makes the loss real, this is where it goes and this is how to measure it
-- render a Time sweep and look at rendered/asked, which should be flat at 1.0.

## 52. `kRvLen`

EIGHT LINES IN PARALLEL, EACH WITH AN ALLPASS IN FRONT OF IT, MIXED INTO ONE ANOTHER.

THE INPUT DIFFUSER IS WHAT MAKES IT DENSE, and density is a separate question from anything the
frequency response can show. Measured as normalised echo density -- the fraction of samples in a
sliding window exceeding that window's own standard deviation, over the 0.3173 a Gaussian gives,
so 1.0 means fully dense, each side measured from its OWN wet onset:

```
                       5 ms   10 ms   20 ms   40 ms
    the instrument      0.97    0.99    1.01    1.00
    four diffusers      0.78    0.82    0.99    1.00
    six diffusers       0.85    0.99    1.08    1.01

```
Sparse early reflections are heard as separate echoes, which is a metallic ring, and the two short
sections at the head of the chain -- 43 and 73 -- are what fixed it. TEN sections made it worse,
not better, dropping the 10 ms figure to 0.66: a run of very short allpasses lays its own
repeating fine structure over the response. Six is where it matches.

ALIGN BOTH SIDES TO THEIR OWN ONSET before comparing this. Measured from t=0 the engine's early
windows sit in the pre-delay's silence and read 0.45 at 5 ms, which is the measurement and not
the tank.

THE LINE LENGTHS MUST SHARE NO COMMON FACTOR. Lines whose lengths share a factor share a period,
and a shared period is a ring. The recovered figures -- 666, 672, 738, 812, 2300, 2456, 3999,
5326 -- are every one of them even and three share a 3, so these are the nearest prime to each.

## 53. `kRvTapLine`

WHICH LINE EACH OUTPUT TAP READS, AND HOW FAR ALONG IT — ONE SET PER CHANNEL.

THIS IS WHERE THE STEREO COMES FROM, and it is the whole of it. There is ONE tank; the two
channels are two different sets of taps into it, which is what the instrument does — its own
reverb holds a single 32768-word memory and reads it twice. Two tanks fed the same mono input
hold the same state by construction, so anything derived from one is derivable from the other,
and no amount of offsetting the read positions changes that.

WHAT THE PREVIOUS ARRANGEMENT ACTUALLY DID, measured rather than argued: it ran two identical
tanks and read the right one's taps REVERB_SPREAD samples earlier. With the line modulation
switched off the right channel was then a BIT-EXACT COPY of the left delayed by 110 samples —
cross-correlation +1.0000 at lag 110, in all four rooms. Every bit of the decorrelation came
from the modulation LFOs running a quarter cycle apart, none from the structure, so turning the
modulation down would have collapsed the image without touching anything named "stereo".

AND THE METRIC THAT PASSED IT WAS BLIND TO EXACTLY THAT. Correlation read at lag zero scores a
signal against a delayed copy of itself as uncorrelated: the old arrangement read +0.03 at lag 0
against the instrument's +0.012..+0.045 and looked like a match. Score the PEAK over lag
instead, which is what tells a decorrelated pair from a delayed one:

```
                         peak r      at lag (96 kHz samples)
    the instrument      +0.124..+0.159    676..1185, and it SCALES with the room
    two tanks + spread  +0.126..+0.137    74..111, pinned to REVERB_SPREAD
    two tanks, no mod   +1.0000           110, exactly

```
The instrument's peak lag scaling with the room (1.00, 1.26, 1.59, 1.75 against kReverbTypeScale's
1.00, 1.27, 1.53, 1.68) is the tell that its two channels are tap sets on one tank: the residual
similarity sits at the DISTANCE BETWEEN AN L TAP AND AN R TAP on the same line, and every length
in the tank scales with the room. A fixed offset cannot do that, and the old one did not.

TWO TAPS PER LINE, SIXTEEN PER CHANNEL, AND NO FRACTION SHARED BETWEEN THE SETS. A tap the two
channels read at the same point on the same line is common-mode and contributes nothing but
correlation.

EVERY TAP MUST SIT AT LEAST AS FAR ALONG AS ITS CHANNEL'S EARLIEST ONE. The first arrival at a
tap is frac * length after the tank's input, so the SMALLEST frac * length in a set is that
channel's onset, and kRvTankLead is that number for the left set. A tap placed nearer the head of
a short line silently becomes the new onset and moves the whole room forward — which is why the
short lines carry the large fractions here and only the long ones carry small ones.

THE RIGHT CHANNEL ARRIVES FIRST, by the 110 samples measured on the hardware and previously spent
on REVERB_SPREAD. Here it is a tap position rather than a subtraction: left's earliest is
0.13 * 2297 = 299 samples, right's is 0.0823 * 2297 = 189, and the difference is the 1.14 ms gap.
Being a position, it scales with the room the way the instrument's does, and it cannot clamp —
the old subtraction hit zero on the short lines of the Small room and handed those taps to both
channels identically.

## 54. `gRvAddrBank`

─── THE OUTPUT TAP SETS ARE THE INSTRUMENT'S OWN ────────────────────────────────────────────────

Its output stage sums SEVEN taps into each wet slot, and the two sets are DISJOINT: seven PAIRS,
with the left channel reading the later member. Separations in samples at roomSize 1.0:

```
    the instrument   1909   904  1645  1919  1067  1672  1871      mean 1569
    measured L/R correlation peak, hardware, 2026-09-06:           1504 (Small)

```
THOSE TWO NUMBERS ARE THE SAME MEASUREMENT FROM DIFFERENT DIRECTIONS, which is the reason to trust
both: the recovered pair separations average 1569 and the hardware's cross-correlation peaks at
1504. Six of the seven separations are used here; the seventh slot carries the measured 110-sample
ONSET gap instead, which the instrument gets from propagation through its network rather than from
any pair, and which this tank has to place explicitly because its pre-delay is a span.

BOTH CHANNELS COMBINE THEIR SEVEN AS + - + + - - +, the SAME pattern, so the stereo is carried
entirely by tap POSITION and never by sign. That is the instrument's pattern, read off its two
output accumulators, not an alternation chosen for convenience.

WATCH THE CROSS PAIRS, NOT JUST THE INTENDED ONES. Every L tap correlates with every R tap on the
SAME line, so a set of seven pairs really carries thirteen distances. Two taps that land near each
other by accident dominate the result: L at 2403 against R at 2342 on the long line put the
correlation peak at lag 61 and hid everything else. Shifting BOTH taps of that pair together fixes
it while preserving the separation the instrument specifies.

WHAT THIS MATCHES, AND WHAT IT CANNOT. Peak L/R cross-correlation, against hardware measured the
same way:

```
    room             Small   Medium   Large    Hall
    the instrument   0.161   0.168    0.171    0.173
    this engine      0.186   0.166    0.141    0.157

```
Close, and flat across the rooms the way the instrument's is — which the sixteen-tap fit that
preceded this was not, declining 0.159 -> 0.124 as the room grew.

THE LAG IS NOT MATCHED AND CANNOT BE, in this tank. The instrument reads ONE shared 32768-word
memory, so all forty-nine tap-pair distances contribute and they cluster; here the eight lines are
separate, only same-line pairs correlate at all, and thirteen distances are too few for any one to
dominate — so the peak wanders between rooms and windows rather than sitting at 1504 * roomScale.
That is a property of the architecture, not of these numbers, and no tap placement fixes it. It is
what a single shared buffer would fix. Do not tune this further: see the reverb entry in
Docs/findings.md for the full specification of the instrument's tank, which is what closes it.

## 55. `RV_RATE`

THE RECOVERED LENGTHS ARE ALREADY IN 96 kHz SAMPLES — that is the rate the instrument's tank runs
at and the rate every recovered figure is quoted in. They must NOT be multiplied by
ENGINE_OVERSAMPLE the way the old Freeverb constants were: those were 44.1 kHz numbers that needed
scaling up, these are not. Doing it anyway made every delay twice as long as it should be, put the
feedback loop at 1.7 s instead of 0.85, and had the tail arriving in audible waves about a second
apart. Converting by the engine's ACTUAL rate keeps the times right at any device rate.

## 56. `RV_MEM_SHIFT`

The sixteen recovered tap ADDRESSES are gone from here. They were positions in the instrument's
own memory map, and this tank lays its spans out differently, so an address off that map means
nothing against this one; kRvTapLine/kRvTapFrac say which line and how far along instead. What
carried over is the count and the spread — sixteen taps scattered across every long line.

## 57. `RV_MEM_SHIFT`

ONE SHARED MEMORY FOR THE WHOLE TANK, big enough for the largest room's highest address
(21432 * 1.6795 + 1200, about 37200) with room to spare. The instrument uses 32768 words and
wraps; the next power of two above what the addresses need costs 256 kB a channel and removes
any question of a site aliasing onto another.

## 58. `REVERB_INPUT_LP_HZ`

THE TANK IS FED THROUGH A LOWPASS, because the instrument's tail STARTS darker than ours did.

This is NOT the same thing as the in-loop damping, and the two were confused for a whole session.
The damping sets how fast the high end DECAYS relative to the low, and it is already right —
measured against the instrument at Hall / Time 122 / Bright 64, the decay rates agree closely
(hardware -5.6 dB/s low and -7.7 high, engine -6.0 and -7.6, i.e. a high-to-low decay-time ratio
of 0.72 against 0.79, on a fitted target of 0.75). What was wrong is where the tail STARTS: the
instrument's is band-limited going in, ours was white.

MEASURED, same capture, tail spectrum normalised so the two agree below 500 Hz:

```
    1 kHz  -1.4 dB      2 kHz  -3.7 dB      4 kHz  -9.0 dB      8 kHz  -17.9 dB

```
which is a one-pole to within about 3 dB at the very top. Hence the cutoff below, fitted to those
four points. This is what the owner heard as "the G2 has more low end" — it does, relatively,
because ours had far too much top.
TWO POLES, not one: a single pole matched the instrument up to 2 kHz but left 4 and 8 kHz 2.0 and
6.7 dB too bright, because the instrument's roll-off is steeper than 6 dB/octave at the very top.
The second pole sits an octave up so it barely touches the region the first one already fitted.

## 59. `REVERB_INPUT_LP_TIME`

A THIRD POLE, AND THIS ONE IS THE INSTRUMENT'S OWN, not a fit. Its coefficient comes straight off
the Time dial as 0.7 * time, so the filter closes as the room gets longer -- and the hardware does
exactly that. Measured at Brightness 64, Hall, relative to 1 kHz:

```
                  4 kHz    8 kHz   12 kHz   16 kHz
    Time  32      -4.26   -12.86   -18.81   -22.07
    Time 122      -5.93   -15.42   -21.84   -26.05

```
A longer tail is a darker one, by 1.7 dB at 4 kHz rising to 4.0 dB at 16 kHz, and this pole is
where that comes from. The two fixed poles above it carry the rest: at Time 32 this one is nearly
wide open, yet the instrument is still 12.9 dB down at 8 kHz, so most of the darkness does not
move with the dial and cannot be this.

## 60. `REVERB_INPUT_LP4_HZ`

A FOURTH POLE, well above the other three, and this one IS a fit. With the three above it the
engine still ran 2 to 3 dB bright from 8 kHz up at both ends of the Time dial -- a fixed shortfall
that gets steeper with frequency, which wants another pole rather than a lower corner on the ones
already there. Dropping those instead would have cost a decibel at 4 kHz, where the match is
already good.

## 61. `PARAM_SMOOTH_SECONDS`

Linear 0..1 through the current segment, and the level it started from. Shaping this rather than
the step keeps a segment's DURATION exactly what its dial says, whatever curve it draws.
PARAMETER SMOOTHING. The G2 runs its modulation at 24 kHz (manual p.71 — "modules can process and
output signals at two sample rates: 96kHz and 24kHz", the lower one being for modulation), so a
dial being turned arrives at the DSP finely stepped and needs no smoothing of its own. This engine
rebuilds its parameter snapshot on a REDRAW, i.e. at frame rate, which is a few hundred times
coarser — and a stepped parameter is audible as zipper noise, most obviously on a Shape sweep
where each step moves the waveform itself.

Interpolating per sample toward the snapshot value restores what the hardware gets for free.
The time constant is short enough not to lag a deliberate move and long enough to bridge the gap
between frames.

## 62. `glide_time_seconds()`

The Glide dial's 128 settings are a table of times running from 19 ms to 6.27 s, written as text
for the patch-settings display. Reading the milliseconds back out of it means the engine glides
for exactly as long as the editor says it will.
Glide time. INTERPOLATED between table entries rather than computed, because unlike the A/D/R
curve this table has no decent closed form — the best power-law fit is 17% out at the median and
38% at worst, which would be a far bigger error than reading it. Interpolating gives what
computing was wanted for, a value that moves continuously with a morphed or smoothed dial, while
staying exact at every position the dial can actually stop on.

## 63. in `reset_node_state()`

Spread rather than zeroed, for the same reason the note-on path leaves them alone:
from the very first note the oscillators should be at unrelated points in their
cycles. The step is irrational-ish so no two land together — and the VOICE is folded
into it as well, so two voices playing the same note are not phase-locked copies of
each other. Held notes on the hardware do not cancel and reinforce like that.

## 64. `build_decimator()`

The lowpass that turns OSC_OVERSAMPLE samples back into one. A windowed sinc: cut just under the
output rate's Nyquist so nothing is lost from the audible band, with a Blackman window to hold the
stopband down where the images sit — an image that survives here is exactly the aliasing the
oversampling was meant to remove.

## 65. in `engine_prime()`

Start from silence rather than inheriting whatever the last run left behind. That includes
the note queue: anything posted while the engine was off — the Virtual Keyboard, or MIDI from
a previous run — is stale, and starting the read index behind the write index would have the
audio thread chewing through history instead of playing what is being pressed now.

## 66. in `sound_engine_start()`

Start from silence rather than inheriting whatever the last run left behind. That includes
the note queue: anything posted while the engine was off — the Virtual Keyboard, or MIDI from
a previous run — is stale, and starting the read index behind the write index would have the
audio thread chewing through history instead of playing what is being pressed now.

## 67. `sound_engine_modulation_text()`

Answers the question "why is there no vibrato" without a debugger: whether the keyboard is
sending pressure at all, where that has left the morph, and whether the patch actually put an LFO
into the graph. Those three failures look identical from the outside — silence — but need
completely different fixes.

## 68. `voice_count_for_patch()`

How many voices this patch may use at once. Mono and Legato are one voice whatever the count says,
and in Poly the descriptor's field holds the count MINUS ONE — the topbar's readout does exactly
this arithmetic (topbarRender.c), and taking it from the same place is what stops the engine
playing a different number of notes from the one on screen.

## 69. `voice_to_allocate()`

Which voice a new note should take, out of the `count` the patch allows. In preference order: one
that is doing nothing, then the longest-released, then the oldest still held. Only the last of
those is a steal — cutting a note off — and it is what a polyphonic instrument does when it runs
out, so it is worth being sure the two cheaper cases are exhausted first.

## 70. in `voice_note_on()`

Auto glide only slides between overlapping notes, which is the point of it: a phrase played
legato slides, a detached note starts where it means to. Whether THIS VOICE'S gate is already
open is that test — and it is why the check has to happen before the gate is opened below.

Per voice rather than patch-wide: in Poly each note lands on a voice of its own, which was not
playing anything, so nothing slides. That is correct. A glide in Poly only happens when a
voice is reused, which is also what the hardware does.

## 71. in `voice_note_on()`

MONO RESTARTS THE ENVELOPES, LEGATO DOES NOT, and that is the whole difference between them.
The G2 manual's Voice Mode description: in Legato "the Envelope modules do not retrigger when
you play a new key before releasing the previous key" - which says Mono does.

THE GATE CANNOT SAY IT. A key played over a held one lands on a voice whose gate is already
open, so the envelope sees no edge; this engine used to treat that as legato in every mode, and
with no sustain the second key sounded nothing at all once the decay had run out. Hence a count
the envelope compares against rather than a flag it could miss.

Poly gains from it too: a note that STEALS a held voice is a new note, and now starts like one.

## 72. in `take_next_note_event()`

If the writer has lapped us the oldest events have already been overwritten, and the slot the
read index points at now holds something far newer. Waiting for a sequence number that can
never arrive would wedge the queue for good — every later note silently dropped — so skip
forward to the oldest event still intact. Losing the tail of a burst is recoverable; wedging
is not, and wedging is what made rapid playing fall apart.

## 73. `pulse_time_seconds()`

The exact scale the dial prints, rather than the power-law fit this used to be — see
adr_time_seconds() in renderParams.c. Shared so the envelope that is heard cannot take a
different time from the one shown.
THE PULSE'S WIDTH IN SECONDS, as a closed form rather than a copy of the dial's 128 readings.
Written this way deliberately: a 128-entry table truncates the FRACTIONAL dial values a morph or a
smoothed knob produces, and would disagree with the dial's own text between steps.

MEASURED ON HARDWARE 2026-09-07 - 17 dial values in the Sub range, captured at 192 kHz so the
shortest gate is resolved (at 48 kHz it is four samples and cannot be). EVERY width came back an
integer count of 96 kHz samples: 8, 16, 28, 52, 92, 160, 288, 512, 912, 1628, 2916, 5244, 9460,
17116, 31076, 56660, 96083 at dials 0, 8, 16 ... 120, 127. That is also independent confirmation
of the 96 kHz engine rate, arrived at from a different module and a different rig than the reverb.

IT IS NOT A CONSTANT-RATIO PROGRESSION, which is what this used to assume. The per-step ratio
drifts smoothly from about 1.0748 low on the dial to 1.0768 at the top - small, but compounded over
127 steps it is the curvature the polynomial below carries, and without it a straight line in log
runs about 11% LONG
across the whole middle of the dial (+24.8% at dial 0, +11.3% at 64, converging only at 127 because
that endpoint was pinned). The old two-endpoint form fitted the ends and missed everything between.

A CUBIC IN LOG, over all 17 points, because nothing simpler covers the whole dial. A quadratic
fitted only where the measurement is sharpest (dial >= 48, where the gate is hundreds of samples
and edge placement is worth a fraction of a percent) lands inside 0.12% from there to the top - but
extrapolates to 9.9 samples at dial 0 where BOTH measurement methods, a 50% crossing and an
edge-slope, independently returned 8. Something in the bottom two dial steps is not on the curve
the top follows. Rather than be exact over most of the range and 24% out at one end, this fits
everything: worst case 4.1%, and 1.8% rms.

THAT REMAINS THE OPEN QUESTION on this module. Either the very bottom of the dial genuinely departs
from the curve, or an 8-sample gate defeats both measures - at 192 kHz it is 16 samples with the
reconstruction filter's ringing across its edges, so a two-sample bias is not impossible. Settling
it needs either a higher capture rate or the instrument's own readout via DEVKNOB.

Range shifts it by a decade either way (pulseRangeStrMap order: Sub, Lo, Hi). Sub is the base here
because Sub is what was measured; the old code based it on Lo.

## 74. `connector_index_for_input()`

The RAW connector index of a module type's Nth input, optionally restricted to a connector type.

The engine used to carry hand-written index constants per module — CONNECTOR_IN_A, an "env in" at
2, a mixer's legs at {2..9}. Every one of those encodes a fact the resources already state, and
getting one wrong is invisible: the signal simply never arrives, or arrives from the wrong socket.
Two such constants were "corrected" in opposite directions in one session before it became clear
they were describing the same thing badly. Ask the table instead.

`anyConnectorType` means "any". Returns -1 when there is no such input, which callers treat as
unconnected.

## 75. `voice_area_output_for_fx()`

The Voice area's Out module, which is what feeds the FX area. There is no cable for this link —
the 2-Out's "Out to" setting routes it — so the walk has to make the jump itself when it reaches
an Fx-In, or the whole FX chain would look like it had nothing patched into it.
The Voice area Out that feeds a given Fx-In — the one whose "Out to" names the same FX bus the
Fx-In is listening on.

This used to return the FIRST Out module in the Voice area whatever it was set to, which routed
signal into the FX area even when the Out was aimed at the speakers and nothing was being sent to
FX at all. The two selectors have to agree for anything to cross:

```
  2toOut "Out to":  0 Out 1/2, 1 Out 3/4, 2 FX 1/2, 3 FX 3/4, 4 Bus 1/2, 5 Bus 3/4
  4toOut "Out to":  0 Out, 1 Fx, 2 Bus            — one setting for all four channels
  Fx-In  "In from": 0 FX 1/2, 1 FX 3/4

```
Returns NULL when nothing is feeding that bus, which is correct: an Fx-In listening to a bus
nobody sends to receives silence.

## 76. `add_node()`

Adds `module` and everything upstream of it, depth first so a node's inputs always occupy lower
indices than the node itself — which is what lets the audio thread evaluate the list as a single
forward pass. Returns the node's index, or -1 if it could not be added.

`depth` bounds the recursion. G2 patches are allowed to contain feedback loops, so without it a
cycle would recurse until the stack ran out.

## 77. in `add_node()`

Every leg starts UNCONNECTED. This used to be written {-1, -1, -1, -1}, which supplies only
four of the eight and lets C zero-fill the rest — and 0 is not "unconnected", it is node 0,
the first node in the chain. A stereo mixer reads all eight legs, so its unpatched channels
were quietly summing in whatever node 0 happened to be, usually an oscillator, raw.

## 78. in `add_node()`

COPIED before the loop, because input_connectors() may hand back a pointer to a static
buffer and add_node() below recurses into itself for every input — a deeper node's own
call would otherwise overwrite this node's list while it is still being walked, leaving
every input after the first reading whatever the deepest module happened to want. The
chain then differs from one build to the next, and since the engine resets its node state
whenever the topology signature changes, the result is envelopes restarting continuously.

## 79. in `add_node()`

An Fx-In takes no cable: it carries whatever a Voice area Out sends across the FX bus it is
listening on. Follow that link explicitly, or a patch whose real output lives in the FX
area looks like it has nothing patched into it and plays silence. The bus has to MATCH,
though — see voice_area_output_for_fx().

## 80. in `add_node()`

BOTH LEGS, because the FX bus is a STEREO pair and this used to take only the left.
The module has two audio outputs and a stereo meter; the Voice-area Out it listens to
fills leg 0 and leg 1 with a genuine left and right and keeps them apart. Resolving
only leg 0 threw the right channel away entirely — not summed into the left, discarded
— so anything panned right vanished and a stereo source arrived as its own left
channel doubled. A Reverb fed from it then summed two copies of the same signal.

## 81. in `add_node()`

The waveform index is kept RAW: the shape oscillators have their own eight waveforms
with their own meanings, and Shape morphs each of them rather than acting as a pulse
width. osc_shp_wave() does the work — mapping these onto the plain oscillator's
waveforms lost the entire point of the module, since at 50% Shape all four Sine
variants ARE a plain sine and everything interesting happens as Shape opens.

## 82. in `add_node()`

RAW, normalised to 0..1 - not the displayed percentage. waveModels.h states the
contract ("Shape is the raw 0-127 parameter normalised to 0..1. It is NOT a
percentage"), and module_shape_value() in moduleGraphics.c passes param/127 to draw
the same wave. Feeding osc_shape_percent()/100 here handed the models 0.5..0.99, so
the dial acted over the model's upper half only and raw 0 - the capture's pure sine -
came out already half-shaped. Drawn wave and heard wave disagreed, which is the drift
waveModels.c exists to make impossible.

## 83. in `add_node()`

ALL FOUR OF THESE WERE WRONG, and none of it needed the hardware: the instrument's own
dial readings settle every one. The note that used to sit here called the curve "an
approximation, not a reading of it", which was honest and is now unnecessary.

```
  THRESHOLD  the dial reads raw - 30 dB, and raw 42 reads "Off". This had raw - 42,
             putting every setting 12 dB too low, and had no Off at all — so the
             compressor was still working where the instrument stops.
  RATIO      three straight runs, reaching about 95:1. This was 1 + raw/8, which tops
             out at 9.6:1 — a tenth of the range, so the hardest settings barely
             compressed.
  ATTACK     0.53 ms to 767 ms, and raw 0 is "Fast", i.e. instant. This was
             0.1 ms to 300 ms.
  RELEASE    125 ms to 10.2 s. This was 10 ms to 3 s.

```
Attack and release are pure exponentials across the dial — fitted to the printed
scales, worst error 0.07 dB and 0.04 dB respectively, so the shape is not in doubt.

## 84. in `add_node()`

The range selector is a mode. Its four settings are progressively longer maximum
times; the dial then scales within the chosen one.
The four range settings, straight off delayABRangeStrMap: 500ms, 1.0s, 2.0s, 2.7s.
Three different Range tables exist and the delay modules do not share one — this
held only DelayA/DelayB's, so every other delay's Range was read against the wrong
list. delay_range_max_seconds() is the single definition, shared with the dial.

## 85. in `add_node()`

Clock mode: the dial picks a musical division, not a time. The engine has no
running master clock of its own, so it works to a FIXED 120 BPM reference —
half a second to the beat. That keeps a clocked delay musically proportioned
and the dial honest about which division it selects; it will not agree with a
patch running at some other tempo on the hardware.

## 86. in `add_node()`

The Range still caps it. Manual, Time/Clk scroll button: "if the delay time
(based on the current Master Clock rate and the Sync factor) should exceed the
selected 'Range' time, the actual delay time will automatically be divided by
two." Halving repeatedly is what makes the long divisions land somewhere
musical instead of simply being clamped to the Range — 2/1 is four seconds at
120 BPM, past every Range there is, so without this the top of the dial was
wrong on every setting.

The DISPLAY deliberately does not do this: the dial shows the sync factor you
chose, which is what the hardware shows too.

## 87. in `add_node()`

FEEDBACK IS LINEAR TO EXACTLY UNITY, MEASURED ON THE INSTRUMENT 2026-08-15. This was
scaled by 0.95, which is why the engine's repeats died away where the hardware's hold.

DelayB, Range 500 ms, LP wide open so the in-loop filter is transparent, decay read off
the repeat train of a note-gated click captured from the G2's main outputs:

```
    FB dial      64        96       127
    measured   0.4973    0.7413    1.0004
    value/127  0.5039    0.7559    1.0000
    was (x.95) 0.4787    0.7181    0.9500

```
At 127 the hardware does not decay AT ALL — nine repeats within 0.06 dB of each other,
then flat. The old 0.95 turned that infinite sustain into -0.45 dB a repeat, audibly
gone inside thirty. The measured values sit ~0.7% under value/127 at the two lower
settings, which is the residual loss of the LP even at its widest, not a different law.

## 88. in `add_node()`

LP IS A CUTOFF, AND 127 IS WIDE OPEN. This read the dial as an amount of damping and
had it the wrong way round, with a fatal end point: delay_step() uses (1 - damping) as
its one-pole coefficient, so LP 127 — the brightest, most ordinary setting there is —
gave a coefficient of exactly ZERO. The filter state then never updated, nothing was
ever fed back, and the delay produced NO REPEATS AT ALL. Anywhere near the top of the
dial it was near enough silent.

MEASURED ON THE INSTRUMENT (DelayB, fully wet, FB 96, repeats measured after cutting
the oscillator). The dial runs dark-to-bright and the tail lengthens with it:

```
    LP    0     32     64     96    127
    tilt  -37.1  -41.4  -23.2  -13.2  -10.7 dB   (2-10 kHz against 80-400 Hz)
    tail  0.5    1.6    1.8    2.0    2.0  s

```
So LP 0 is dark and short, LP 127 open and long — the exact opposite of what this did.

An exponential sweep of the cutoff fits that: 200 Hz at the bottom of the dial, 20 kHz
at the top. Against the measurements above, taking LP 127 as the open reference, it
predicts about 28 dB of extra rolloff at LP 0 where 26 was measured, and 8 dB at LP 64
where 12.5 was. Close, and the right shape — but the filter sits INSIDE the feedback
loop, so what is measured is several passes through it rather than one, and these
constants deserve a proper fit before they are called settled.

## 89. in `add_node()`

MEASURED ON THE HARDWARE 2026-08-09. The Time dial is LINEAR in decay time, with a
slope per room type — not the cubic that used to be here, and not reaching anything like
the 17.58 s the manual quotes:

```
    room     Time 42   Time 85   Time 127    s per dial unit
    Small          -    1.95 s    2.89 s     0.0224
    Medium    2.01 s    3.72 s    5.49 s     0.0409
    Large     2.94 s    5.49 s    8.11 s     0.0608
    Hall      3.77 s    7.30 s   10.75 s     0.0821

```
Straight lines (r2 0.996..0.998) whose slope agrees across both halves of the range to
three digits, so the shape is not in doubt. The old cubic gave 17.58 s at Time 127 where
Large measures 8.11 s — more than twice too long — and its exponent was openly a guess
fitted to make the midpoint musical.

THE SLOPE IS NOT PROPORTIONAL TO ROOM SIZE (Hall/Small is 3.67 against a size ratio of
1.68), so Type sets the feedback GAIN as well as the delay lengths. That is why this is a
table rather than kReverbTypeScale doing the work.

READ AS EARLY DECAY, EXTRAPOLATED. The G2's tail only clears the measurement noise floor
by about 21 dB, so each figure is a straight-line fit over ~15 dB stretched to 60. If the
instrument has a double-slope tail — a bright early decay over a longer low-frequency one,
which reverbs often do — the late part is invisible here and the true RT60 is LONGER than
these numbers. That would also explain the manual's 17.58 s, which is not reachable even
at Brightness 127 (Hall measures 11.83 s there). Resolving it needs a quieter floor, not
a different formula. See the REVERB entry in todo.md.

## 90. in `add_node()`

Read raw, not through param_value(): Curve is a drop-down, and drop-downs cannot be
assigned to a morph group (manual p.20), so there is never a morph range on one.

expStrMap is {"Exp", "Lin", "dB"} — Lin is the MIDDLE entry, so the test is against 1
and not against 0. The manual (p.216) is explicit that Exp and dB are the same curve:
"there is no functional difference between the Exp and the dB curves, it is just a
matter of whether you want the knobs to display an exact dB value or the basically
meaningless Exp value". So both non-Lin settings take the same branch.

## 91. in `add_node()`

THE PAD HAS THREE POSITIONS, not two - measured 2026-09-07 as 0.00, -6.01 and
-12.04 dB, the parameter clamping at 2. This treated anything non-zero as -6 dB, so the
third position was 6 dB out. It attenuates every input together. The 8-channel mixers'
Pad (db12BPadStrMap) names the same three positions.

## 92. in `add_node()`

OSCA SHARES THIS ENTIRELY and differs only in where its dials sit and what its Wave
menu offers. It has no Shape and no FM, so its parameters are packed four indices
tighter, and its waveform list is {Sine, Tri, Saw, Sqr50, Sqr25, Sqr10} against OscB's
{Sine, Tri, Saw, Sqr, DualSaw} - three FIXED pulse widths in place of one square whose
width a dial varies.

## 93. in `add_node()`

GUARD THESE TWO. They were read unguarded because every filter mapped until now had
both; FltStatic has NEITHER an Env input nor a Kbt selector, and -1 cast to uint32_t
indexes far off the end of the parameter array. It CRASHED the application rather
than misbehaving, which is at least a loud failure - but the -1 convention is only
safe where every reader checks it, and two of them did not.
FLTNORD IS NOT FLTCLASSIC'S LADDER, and this is where that shows. MEASURED
2026-08-30, both modules through the same rig at an input verified linear:

```
    passband level    Res 0    Res 110
    FltClassic        -1.3 dB   -12.7 dB    droops - real ladder feedback
    FltNord, GC off   +0.5 dB    +1.6 dB    FLAT; only the peak grows, to +29.7
    FltNord, GC on    -1.3 dB   -15.5 dB    GC pulls it down

```
We borrow FltClassic's ladder for FltNord, so our passband droops where the
instrument's does not. A four-pole ladder's DC gain is 1/(1 + k), so multiplying by
(1 + k) cancels exactly that droop and leaves the flat passband the hardware has;
GC's measured attenuation then goes on top. Applying GC WITHOUT the (1 + k) would
have counted the droop twice and left FltNord about 29 dB quiet at high resonance.

This corrects the LEVEL behaviour. Whether FltNord's peak has the same shape as
FltClassic's is a separate question and still open - see Docs/todo.md.

## 94. in `add_node()`

THE PAD'S SENSE IS THE OTHER WAY ROUND, measured on the instrument 2026-09-07. Setting
it makes the output 6.02 dB LOUDER, not quieter - checked on two independent 2-Out
modules (the VA one feeding the FX bus and an FX one feeding Out 1/2), with the write
read back from the instrument each time and the OTHER output pair confirmed unchanged.
The ratio is 2.0016, i.e. exactly a factor of two. This code had it as 0.5, so it was
12 dB out whenever the setting was engaged.

WHICH OF THE PAIR IS UNITY IS NOT SETTLED. All these measurements give RATIOS: every
path runs through several pads and none of them is a known 0 dB reference, so 1.0/2.0
and 0.5/1.0 fit the data equally. Unity is put on the DEFAULT setting here, so existing
patches are unaffected and only the engaged state moves - but see the note in
capture-inventory.md, because the label says "-6dB" for the setting that measures +6,
and whether the strings, the wire values or both are inverted needs the instrument's
own display read back through DEVKNOB.

## 95. in `add_node()`

WHICH PHYSICAL PAIR IT FEEDS. A 2-Out's "Out to" selects Out 1/2 or Out 3/4, and a
measurement patch depends on the difference: the rig puts its dry reference on one
pair and the processed signal on the other, so summing them would destroy the very
comparison it exists to make. A 4-Out has one "Out" setting covering all four
channels and is only half-modelled here anyway (eNodeOut carries two legs, not four),
so it stays on the first pair.

## 96. `chain_has_source()`

A chain has to start somewhere. An oscillator is the only thing here that generates a signal from
nothing, so without one the whole thing renders silence and the menu should say why rather than
leaving it a mystery — the usual cause is a filter with an empty input.
A PULSE COUNTS AS A SOURCE, not just an oscillator - and so does Noise (§9.4). It generates a click of its own and needs
nothing upstream but an edge to fire on, so a patch whose only generator is a Pulse is not silent
and should not be reported as having nothing patched into it.

THIS IS WHAT THE MEASUREMENT RIG IS BUILT FROM — see PatchTestFiles/FxMeasure.pch2, where a
free-running LfoShpA fires a Pulse into the module under test. Without this the engine resolves
that patch correctly, all nine nodes and the right topology, and then refuses to play it, so the
one patch designed for comparing engine against instrument could be rendered by neither.

## 97. `out_module_is_audible()`

With nothing selected, play the patch: find the Out module that is actually the end of it.

The FX area is searched FIRST and that ordering matters. A patch like this one has an Out in each
area: the Voice area's is labelled "Fx Out" and routes into the FX area rather than to the
speakers, and the FX area's is the real end of the chain. Taking the Voice one — which is what
scanning in area order does — plays the patch dry, with the delays and reverb silently skipped.
Does this Out module actually reach the speakers, or is it internal routing? Getting this wrong is
audible in both directions: treat a send as an output and the FX area's input is heard raw
alongside the finished signal; ignore a real output and the patch is silent.

## 98. `mark_post_mix_nodes()`

Which nodes are evaluated after the voices are mixed rather than once per voice.

THE DELAY, CHORUS AND REVERB MODULES OWN ONE BUFFER EACH, and a buffer has one write pointer. Run
one of them once per voice and that pointer advances as many times per sample as there are notes
held: the delay time divides by the number of voices and the read position sweeps at that multiple
too, which is heard as the sound stretching and tearing — intermittently, because it only happens
while more than one note is down. They are therefore evaluated exactly once, on the summed voices,
which is what the FX Area already does and what the hardware does with the FX Area.

The flag has to spread DOWNSTREAM as well. A module fed by one of these has an input that only
exists after the mix, so it cannot be evaluated per voice either. add_node() lists every node
after its own inputs, so one forward pass settles the whole graph.

## 99. in `sound_engine_update_from_patch()`

SOUND COMES FROM THE PATCH'S AUDIBLE OUTPUTS, never from whatever happens to be selected.

Auditioning the selected module was useful while the engine could only render a fragment of a
patch; now that it resolves the whole thing, a selection quietly changing what you hear is a
surprise rather than a feature. It also gave the application and the plug-in two different
signal paths from one patch — the plug-in has no selection and always took the outputs — which
hid engine faults in whichever path was not being listened to.

## 100. in `read_params()`

FENCE BETWEEN THE COPY AND THE RE-READ. Without it the second load only guarantees that
what follows it is not hoisted above it - it says nothing about the copy above being
allowed to sink below it. That is the classic seqlock hole: the validation can be
performed against a sequence read before the data was actually fetched, and the reader
then accepts a torn snapshot as whole.

In practice both loads are seq_cst and compile to ldar on arm64, which makes this very
unlikely to bite - but "unlikely on today's compiler and architecture" is not the same as
correct, and an audio thread reading a half-written node table is not a failure anyone
would enjoy diagnosing.

## 101. `osc_shp_wave()`

OscShpB's eight waveforms. Shape runs 50%..99% and morphs each one — at 50% every waveform in the
first four is a pure sine, and the character only appears as Shape is opened. Descriptions are
from the G2 manual (p.176-177); the implementations are ordinary floating point approximations of
what it describes, not models of the hardware.

`t` below is Shape mapped to 0..1 across that 50%..99% range.

## 102. in `osc_shp_wave()`

THE LAWS LIVE IN waveModels.c, shared with the wave the editor DRAWS so the two cannot drift
apart. They had: everything here once came from the manual's prose, which is wrong in several
places, and from a dial mapping that was wrong everywhere — (shape - 0.5)/0.49 clamped at
zero, so nothing happened below raw 64 and HALF THE DIAL WAS DEAD. That is fixed and, more to
the point, can no longer come back on one side only.

What stays here is BAND-LIMITING, which is this file's business and not the drawing's: a step
rendered as a step aliases across the whole spectrum, so the four waves that contain one are
built from osc_saw/osc_square/osc_triangle, which take dt and limit accordingly.

## 103. in `delay_step()`

Then the high-pass, also in the loop, so each repeat loses more low end than the last — the
counterpart to the LP above. Built as a one-pole lowpass subtracted from the signal, which is
the cheapest honest one-pole high-pass there is. A coefficient of zero is the dial at 0,
where the filter measures flat and is simply switched out.

## 104. in `delay_step()`

DRY/WET IS THE SAME NON-CROSSFADE THE REVERB USES, and this was a plain linear blend. The two
gains are independent, each a ramp cubed, and they overlap: dry holds full scale until the
knob passes the middle and only then falls, while wet reaches full AT the middle and stays.

MEASURED ON THE INSTRUMENT — a saw through a real DelayB with the oscillator cut, so the
repeats could be read on their own. The repeat level came out IDENTICAL at DryWet 64 and 127,
both -12.2 dB against the dry reference, where a linear crossfade would put 64 a full 6 dB
below 127. Total output stayed flat within 0.8 dB across the whole dial.

Unlike the reverb, the delay's wet needs NO overall attenuation: fully wet measures -0.3 dB
against fully dry, where the reverb needed -11.3. REVERB_WET_GAIN does not belong here.

## 105. `CHORUS_RATE_MAX_HZ`

A short delay whose length is swept by a slow LFO — detune sets the sweep depth, amount how much
of it is mixed in. Stereo on the hardware; mono here, since the engine sums to mono anyway.

THE STEREO OFFSET IS HALF A CYCLE, measured 2026-08-15 and the one number the stereo chorus was
waiting on (Docs/todo.md). The two channels run the SAME algorithm with their LFOs in ANTIPHASE:
comparing the phase of each channel's amplitude modulation gave R - L = 179.9, 179.6 and 178.9
degrees across three captures at two tone frequencies and two Detune settings. Not a quarter cycle,
which was the other candidate.
Measured on the instrument — see the notes inside chorus_step().
REMEASURED 2026-08-15 AND RAISED BY A FACTOR OF 3.9. The old 0.853 made the sweep four times
slower than the instrument's, which is why turning Detune up did so much less here than there.

Method, deliberately different from the null counting that produced the old figure: a steady tone
through a real StChorus comes out AMPLITUDE modulated, because the comb notch walks across it as
the delay sweeps, so the modulation rate IS the LFO rate — no disentangling of rate from depth.
Captured from the G2's main outputs and read three independent ways, all agreeing:

```
    Detune 32   0.840 Hz        Detune 64   1.680 Hz      exactly 2x for 2x the dial

  - at a 98 Hz tone and again at 16 Hz. The second matters: there the sweep spans only 0.14 of a
    wavelength, so the modulation CANNOT be a harmonic of the LFO, which is the one way this
    method could have been read four times too fast.
  - and by eye off the envelope: minima 1.19 s apart at Detune 32, which is 0.84 Hz.

```
Proportional to the dial as before, so 127 gives 0.02625 * 127. THE OLD FIGURE IS EXACTLY 1/3.907
OF THIS AT BOTH SETTINGS, which is close enough to 4 to suggest the null-counting method dropped a
factor rather than being noisy — its own note says the gaps swell "once per half LFO cycle", and
reading that as a whole cycle is worth two of the four. That is not explained, only bounded.

RE-ANALYSED FROM THE RETAINED CAPTURES, 2026-08-15, offline and with no G2 present. The three
files in ~/Documents/G2 Captures/ were demodulated at the tone frequency: for a single input tone
the module's output is dry + m*delayed, so |z|^2 of the demodulate is a direct read-out of the
comb argument, and its modulation IS the LFO. What that settled, in order of importance:

```
  - THE RATE ABOVE IS CONFIRMED. AM fundamentals of 0.8382 Hz (Detune 32) and 1.6795 Hz (Detune
    64), exactly 2:1, so 0.8382 * 127/32 = 3.327 Hz at Detune 127. An earlier pass in the same
    session called this 4x too fast; that was a frequency scan capped at 1.2 Hz clipping the real
    peak in the Detune 64 files, not a fault in the figure.
  - THE HALF-CYCLE STEREO OFFSET IS CONFIRMED to a fraction of a degree: L/R phase at the AM
    fundamental of 180.0, 179.9 and 180.1 degrees across the three files.
  - THE LFO IS A SYMMETRIC TRIANGLE, NOT A SINE. This is the one that was wrong. Where the phase
    swing is small the folded profile IS the LFO waveform, and the 32.7 Hz captures fold to a
    triangle — straight flanks, sharp turn — with a fitted rise fraction of exactly 0.50. Fitting
    P + Q*cos(th0 + X*triangle) to the folded profiles lands at R^2 = 0.9992..1.0000, where every
    sinusoid-based estimator returned impossible sweeps of 4 to 14 ms against a 3 ms centre.
  - THE SWEEP IS 2.38 ms, from the 98 Hz capture (both channels agreeing to 0.1%), which is the
    well-conditioned one: at 32.7 Hz the swing and the mix trade off against each other and those
    fits are not to be believed. The old 2.1 ms was close, so the frozen-delay cross-check below
    stands. Both tone frequencies independently put the centre at 2.4..3.9 ms, bracketing
    CHORUS_CENTRE_S — a consistency check that only passes if the model is right.

```
ALL THREE OF THOSE NUMBERS WERE REPLACED ON 2026-09-07, and so was the topology they belonged to.
The measurement above was made from a SUSTAINED TONE, which can only ever show the envelope of the
delay; an impulse response with a dry reference on a second output pair shows the delay line
itself, and it shows TWO taps where this assumed one. See chorus_tap(). The old sweep of 2.38 ms
about a 3.00 ms centre is very close to the two real taps' combined envelope of 0.425..4.995 ms,
which is how one tap came to stand in for two.
THE TWO TAPS ARE NOT SYMMETRIC, found 2026-09-07 from CT hearing "a wah at around 1 second
intervals" on the instrument where ours sounded "a little bit metallic". Both are driven by ONE
triangle but with DIFFERENT depths, so the pair's CENTRE moves as well as its separation - and a
moving centre is a comb whose whole structure slides, which is the wah. A symmetric pair holds its
centre still and pins the comb in place, which is the metallic part.

THE RATE HERE IS HALF WHAT IT WAS, and the old figure was an artefact of the analysis rather than a
reading of the instrument: the tap extractor SORTED the two taps, so once they cross it reports
```
|separation| and doubles the apparent frequency. The centre does not fold, so it gave the rate
```
directly - and came out at exactly half the separation's at every Detune (0.2620 against 0.5236 Hz
at dial 24, 1.3905 against 2.7809 at 127), which is what a fold looks like.

A + B is the widest separation (4.571 ms) and A - B is the centre's swing (0.685 ms); both were
measured, and the pair reproduces the centre range 2.334..3.020 against a measured 2.341..3.026.

## 106. `shaper_odd_power()`

A one-shot gate: a rising edge at the input starts it, and it stays high for the width above.

A RISING EDGE WHILE THE GATE IS STILL HIGH RESTARTS IT rather than being ignored. That is what the
instrument does, and it matters only for an input faster than the width — the measurement patch
fires one edge per note, so nothing there depends on it.

------------------------------------------------------------------------------------------------
SHAPER GROUP

Seven memoryless transfer functions, sharing one entry point. Full scale is +-1.0 here, which is
the +-64 units the manual quotes for the instrument's headroom.

HOW MUCH OF THIS IS KNOWN. Rect is EXACT: the manual states all four operations in words, and
there is no dial to get wrong. ShpStatic's four labels - Inv x3, Inv x2, x2, x3 - name their own
curves, so its SHAPE is known and only whether the instrument normalises them is not. Everything
else here is structurally right and numerically a guess: the manual describes the family (a
logarithmic curve for Saturate, an exponential one for ShpExp, four named overdrive characters,
a fold rather than a clip for WaveWrap) but names no constant anywhere.

THESE ARE THE CHEAPEST MEASUREMENTS LEFT. A memoryless module gives up its ENTIRE transfer
function to one capture: send a slow full-scale ramp - or simply a low sine, which sweeps every
input level twice per cycle - through it and plot output against input. One capture per mode,
no impulse, no windowing, no decay fitting. See to-test.md.

## 107. in `shaper_step()`

shpStaticStrMap is {"Inv x3", "Inv x2", "x2", "x3"}: the inverses are the roots, so
the four exponents are 1/3, 1/2, 2 and 3. Every one of them leaves full scale at
full scale and moves only what is between, which is what "amplification/attenuation
characteristic" means on the module's own buttons.

## 108. in `shaper_step()`

shpExpCurveStrMap is {"x2", "x3", "x4", "x5"}, and Amount morphs the EXPONENT from
linear towards the named curve rather than crossfading between two signals. That
keeps full scale at full scale at every setting, which is the property the manual
describes when it warns the module wants a fixed-amplitude input: the output falls
exponentially only as the INPUT falls.

## 109. in `shaper_step()`

"Shapes an input signal in a logarithmic fashion", Curve 1 smooth and Curve 4 hard.
A log curve normalised to unity at full scale: y = log(1 + k|x|) / log(1 + k), with
k rising with both the Curve selector and the Amount dial, and k -> 0 giving back a
straight line. Structure from the manual, k range UNMEASURED.

## 110. in `shaper_step()`

Amplify, then fold. Up to 19 dB of drive, which is four folds on a full-scale input -
the "deep distortion and FM-like characteristics" of the manual.

THE MAXIMUM DRIVE IS ODD ON PURPOSE. shaper_fold() returns exactly zero at every EVEN
integer, so an even maximum - 16 was the first thing written here - sends full scale
to silence at the top of the dial, and a full-scale input then vanishes exactly where
the module should be at its most extreme. Nine folds full scale back to full scale.

## 111. in `shaper_step()`

Drive into a soft limiter whose KNEE is what the four type names select:
y = x / (1 + |x|^n)^(1/n) reaches +-1 asymptotically, gently for a small n and
almost squarely for a large one. odTypeStrMap is {Soft, Hard, Fat, Heavy}, so Fat
takes the most drive and Hard the sharpest knee.

AMOUNT BOTH DRIVES AND MIXES, and the mix is what makes zero mean zero. The limiter
bends the curve at every drive setting, unity included - x/(1+x^2)^(1/2) is already
3 dB down at full scale with no drive at all - so a dial that only fed the drive
would leave the module audibly distorting with its depth control shut. Crossfading
the shaped signal against the dry one by the same dial is the only construction here
that reaches genuine transparency at 0 and full character at 127. Which of the two
the instrument actually does is UNMEASURED; that it is transparent at 0 is not in
doubt, since the module has no separate bypass reading of its own dial.

## 112. in `shaper_step()`

"Decreasing the clip level limit below the normal headroom": the dial LOWERS the
threshold rather than raising a gain, which is why the manual warns the level drops
as it opens and suggests a feedback loop to get it back. 36 dB of travel is a guess;
only the direction is from the manual.

## 113. `chorus_triangle()`

The LFO shape: a symmetric triangle in [-1, 1], phase in [0, 1). Measured, not assumed — see above.

IT IS THE SHAPE, NOT THE DEPTH, THAT MAKES THIS SOUND LIKE A CHORUS. Pitch shift through a swept
delay is the sweep VELOCITY, so a triangle gives a CONSTANT detune that flips sign twice a cycle
— two steady pitches alternating, which is what doubling is — where a sine glides smoothly
through zero to a peak and back, which is the textbook definition of vibrato. With a sine here,
Amount 127 came out sounding like a slow vibrato rather than a chorus.

## 114. `chorus_read()`

ONE CHANNEL of the sweep, read at the LFO phase it is given. The two channels differ ONLY in that
phase, which is why this is one function called twice rather than two structures — measured, see
the antiphase note above chorus_step().
A FRACTIONAL READ, and for a chorus this is not a refinement - it is the effect.

The pitch shift a chorus produces IS the rate of change of its delay. Read at whole samples only,
the delay is a staircase: within each step the delay is CONSTANT and there is no shift at all, and
the whole of it collects into a discontinuity at the step edge. So an integer-delay chorus does not
produce a weak detune, it produces NO detune plus a click - and the faster the sweep the more
clicks, which is why the fault showed up first at maximum Detune (CT, by ear, 2026-09-07) where the
instrument is at its most obvious.

Catmull-Rom rather than linear, the same choice and for the same measured reason as the reverb's
RVDLYM: linear interpolation is |1 - fr + fr*e^-jw|, a null at Nyquist at the half-sample offset,
i.e. a lowpass whose corner moves with the sweep. Four multiplies more buys a response flat far
higher.

Clamped so all four taps stay inside the line. The delays this is called with are 0.4..5.0 ms, so
41..480 samples at 96 kHz against a 4096-sample line - the clamp never bites in practice and is
here so it cannot read outside the buffer if a constant is ever changed.

## 115. in `chorus_tap()`

TWO TAPS PER CHANNEL, MOVING IN OPPOSITE DIRECTIONS about a common centre. This is the shape of
the module and it is what a single sweeping tap cannot reproduce: two taps crossing put a pair
of comb notches through each other, which is the sound, where one tap gives a single moving
notch. Measured 2026-09-07 by impulse response against a dry reference - 46 Hz impulses, both
channels, the whole Detune dial - and cross-checked unclipped after the instrument's own meter
showed red.

THE TAPS MEET. The measured minimum separation tracked whatever threshold the analysis used to
call two peaks distinct (0.354 ms at a 0.35 ms threshold, 0.094 at 0.06), so the separation
really does reach zero rather than resting on a floor - hence a spread that starts at 0.

The shape is a TRIANGLE, confirmed rather than assumed for the first time: folded over 888
impulses at Detune 24 it fits a triangle with a mean error of 0.025 against a sine's 0.045,
and the flanks are straight to a few parts in a hundred. See chorus_triangle().

## 116. in `chorus_tap()`

1/sqrt(2) EACH, NOT A HALF. The blend below was fitted from notch depth on a static delay, which
measures the SUM of the two taps without being able to see that there are two, so the pair has
to carry that same total - but the taps are at DIFFERENT delays and are therefore largely
decorrelated, and decorrelated signals add in POWER. Splitting by amplitude threw away 3 dB:
measured against the hardware through the same dry saw, the engine's wet sat 2.1 to 2.9 dB low
UNIFORMLY from 200 Hz to 14 kHz - flat, so a level error and not the filtering it was mistaken
for. The residual after this correction is the taps not being perfectly decorrelated.

## 117. in `chorus_tap()`

A CONSTANT-POWER BLEND whose wet/dry ratio IS the dial, measured on the instrument.

Setting Detune to zero makes the delay static, which turns the module into a plain comb
filter — and the depth of a comb's notches is a direct read-out of the dry/wet balance, since
equal parts cancel completely. Sweeping Amount on a real StChorus:

```
    Amount        0     32     64     96    127
    wet/dry    0.02   0.28   0.64   0.94   0.91      (from notch depth)
    total      -0.0   -0.2   -0.0   +0.4   +1.1 dB

```
So the ratio tracks the dial roughly one for one and the total stays flat. Both matter: this
used to be dry (1 - amount/2) against wet (amount/2), which gives a ratio of only 0.33 at the
middle of the dial where 0.64 was measured — half the chorus it should have been — and loses
3 dB of level at the top where the instrument holds steady.

Dividing by sqrt(1 + m^2) is what keeps the sum constant: at full Amount both legs sit at
0.707 rather than both at 0.5.

THE RATIO REACHES 1.19 AT THE TOP OF THE DIAL, NOT 1.0 (measured 2026-09-07). The table above
came from NOTCH DEPTH, and a notch cannot tell a ratio r from 1/r - it is deepest at exactly
equal parts and shallows symmetrically either side. That is why the table's own top is
non-monotonic, 0.94 at Amount 96 then 0.91 at 127: the reading had folded back through 1.0.

Measured instead by STEREO WIDTH, which has no such ambiguity. The L/R correlation of the wet
output falls as the wet leg grows, because what the two channels share is the direct. Against a
hardware capture of the same dry saw the instrument sits at 0.1495; the engine reaches 0.1479 at
a ratio of 1.189 and 0.2723 at 1.0. Scaled back down the dial that lands on 0.30 and 0.60 at
Amount 32 and 64, against the notch table's 0.28 and 0.64 - which agree, because below 1.0 the
notch is unambiguous. So the law is linear in the dial and only its endpoint was wrong.

## 118. in `chorus_tap()`

THE WET/DRY RATIO IS NOT LINEAR IN THE DIAL. Measured 2026-09-07 across the whole Amount
dial by STEREO WIDTH - the L/R correlation of the wet output falls as the wet leg grows,
because what the two channels share is the direct, and unlike a notch depth it cannot
confuse a ratio r with 1/r. Inverting the engine's own correlation-versus-ratio curve
against the instrument at eight settings gives

```
    Amount   32     48     64     80     96    112    127
    ratio   0.194  0.316  0.465  0.643  0.856  1.110  1.429

```
which the form below reproduces to about 1%: 0.197, 0.320, 0.464, 0.639, 0.851, 1.115,
1.429. It reaches 1.429 at the top, not the 1.0 a linear law would give.

THE FORM IS A DIVIDING DRY LEG, not an added wet one: m = x / (A - B*x) is what a ratio
looks like when the DENOMINATOR falls with the dial, here from 1.474 down to 0.700. That is
a crossfade attenuating the dry, which is a thing an instrument would plausibly do, rather
than a curve fitted for its own sake.
NOT A CONSTANT-POWER BLEND. That was the wrong SHAPE, not the wrong constant: it holds the
total flat by construction, and the instrument's total is not flat - it FALLS from +2.46 dB
on the dry at Amount 16 to +0.91 dB around 80..96 and then rises again to +1.35 dB at 127.
A normalised blend can never produce a dip.

Two fixed gains do, and with the SAME two constants the ratio law already needed: the dry
leg falls as the dial rises while the wet leg follows it, so their ratio is
x / (A - B*x) - the law measured across the whole Amount dial - and the total is whatever
those two gains happen to sum to. One overall trim then puts it on the instrument: the
model reproduces all eight measured levels to +/-0.05 dB, dip included.

## 119. `chorus_step()`

STEREO, from one LFO: the right channel reads it HALF A CYCLE along. Measured 2026-08-15 and
re-confirmed from the retained captures the same day — L/R phase at the AM fundamental of 180.0,
179.9 and 180.1 degrees across three files, so antiphase and not the quarter cycle that was the
other candidate.

## 120. in `chorus_step()`

DETUNE SETS THE RATE, NOT THE DEPTH — this had it the other way round, with the rate fixed at
0.7 Hz and the sweep scaled by the dial.

MEASURED with a pure 1976 Hz tone through a real StChorus. Dry and wet beat against each other
as the delay moves, and one null is exactly one wavelength of delay change, so counting nulls
measures the sweep VELOCITY outright. The gaps between nulls swell and shrink once per half
LFO cycle, which separates rate from depth:

```
    Detune 32   rate 0.215 Hz   depth 2.11 ms      Detune 0   no nulls at all: static
    Detune 64   rate 0.430 Hz   depth 2.04 ms

```
Exactly twice the rate for twice the dial, at constant depth. Above about 96 the nulls come
too close to separate the two, so the top of the range is extrapolated from that proportion.
(The RATES in that table are the ones later found to be 3.907x low; the DEPTHS survived the
2026-08-15 re-analysis nearly unchanged, at 2.38 ms.)

CROSS-CHECKED against a quite different measurement: at Detune 0 the LFO stops wherever it
happens to be, and rebuilding the patch repeatedly froze the delay at 1.33, 1.52, 1.94, 2.13
and 5.33 ms. A 3 ms centre swept +/-2.38 ms spans 0.6 to 5.4 ms, and every one of those frozen
values falls inside it.
BOTH TAPS READ THE PHASE BEFORE IT ADVANCES, so the two channels are sampled at the same
instant rather than one being a sample ahead of the other.

## 121. `compress_step()`

A LEVELLER, NOT A DOWNWARD COMPRESSOR - and that is a difference in kind, not in tuning. This used
to divide the excess over the threshold by the ratio, the textbook arrangement, and it ignored Ref
Level completely. The manual says what the module actually does: "With the Ref Level knob you set
the level to compress the stereo signals TOWARDS."

MEASURED 2026-09-07. With the signal at -1 dB, threshold -15 dB and ratio 80:1, the output tracks
Ref Level one for one:

```
    RefLvl    +12    +6      0     -6    -12    -18    -24    -30 dB
    output  -30.24 -36.16 -42.08 -48.10 -54.02 -57.03 -57.03 -57.03 dBFS

```
Six dB in, six dB out - and note it BOOSTS when Ref Level is above the signal, which a downward
compressor can never do. Below -18 it floors at -57.03, which is the threshold (-15 dB internal is
-56.4 dBFS on that rig), so the threshold bounds how far down it will drive the signal.

RATIO SETS HOW FAR TOWARDS REF LEVEL IT GETS. With Ref Level 11 dB under the signal, the fraction of
that gap actually closed came out 0.00, 0.36, 0.54, 0.75, 0.86, 0.96 at ratios 1.0, 1.5, 2.0, 3.4,
5.0 and 9.5 to 1 - against (1 - 1/ratio) of 0.00, 0.33, 0.50, 0.71, 0.80, 0.90. That also CONFIRMS
compressor_ratio(), which had been transcribed from the instrument's formatter and never checked.

So the whole law is one line in decibels,

```
    out = env - (env - target) * (1 - 1/ratio),   target = max(RefLvl, threshold)

```
which is the pow() below once it is written as a gain. It behaves correctly at both ends without
special-casing: at ratio 1 the exponent is 0 and the gain is exactly 1, and as the ratio grows the
gain tends to target/env, putting the output exactly on Ref Level.

THE THRESHOLD IS A MAXIMUM-GAIN LIMIT, NOT A GATE - corrected 2026-09-08, and this was audible
rather than theoretical (CT: "when the compressor lights a LED the effect is quite brutal, and it
has audio glitches around it").

The law above has a gain of 1 only where env == target. Gating it - returning unity below the
threshold and the law above it - therefore puts a STEP at the threshold of exactly the makeup the
law asks for there, and the threshold crossing is the very moment the LED lights. At the stock
settings (Thr -12 dB, RefLvl 0 dB, Ratio 4:1) that step is

```
    (target/threshold)^(1 - 1/ratio) = (1.0 / 0.2512)^0.75 = 2.82  ->  +9.0 dB IN ONE SAMPLE

```
up on the way in and -9.0 dB on the way out, on every note onset and again on every decay - and
with the detector sitting near the threshold it chatters between the two at audio rate. A step is
a click; a chattering step is a buzz. Both are what was reported.

The fix is to CLAMP THE DETECTOR AT THE THRESHOLD FROM BELOW rather than to branch on it. Below
the threshold the compressor then holds the gain it had AT the threshold, so the function is
continuous through the crossing and the law above the threshold is untouched:

```
    gain = (target / max(env, threshold))^(1 - 1/ratio)

```
which is one expression with no branch and no step. Every measurement above still holds: they were
all taken with the signal 14 dB OVER the threshold, where max(env, threshold) is env and nothing
has changed. What HAS changed is silence: the module now applies its makeup all the time instead of
only while working, so a patch with a compressor in it is up to (target/threshold)^(1-1/ratio)
louder in the gaps - +9 dB at the stock settings. That is what a compressor with makeup gain does,
and it is the only reading of "the level to compress towards" that does not step.

STILL UNMEASURED, and the one test that would settle it: feed a steady tone BELOW the threshold and
read the output. This law says it comes back with the makeup on it; a true gate says it comes back
untouched. Nothing captured so far distinguishes the two, because nothing was ever played quietly
enough. What is NOT in doubt is that the instrument does not step 9 dB at the threshold - a module
that did would be notorious.

## 122. in `compress_step()`

THE PANEL METER SHOWS SOMETHING DIFFERENT FROM THE GAIN ABOVE - measured 2026-09-07. Holding Ref
Level over the signal so the gain is constant, the instrument's meter still climbs as the
threshold falls, so it displays HOW FAR OVER THRESHOLD the signal is, not what was done about
it. Lit LEDs against excess were 1, 3, 5, 6, 7, 8 at 0, 3, 6, 9, 12 and 15 dB over.

Interpolated between those points rather than fitted: the spacing is uneven - about 1.5 dB per
LED at the bottom and 3 dB at the top - and six points will not settle what curve that is.
Below the threshold it reads zero, which is what makes first movement a clean threshold
crossing and is the basis of the level probe in findings.md.

## 123. `reverb_step()`

Schroeder reverb — parallel combs for density, allpasses to smear the result.

brightness is the dial as it reads: HIGH IS BRIGHT. It used to be handed straight to the damping
filter's coefficient, which inverted it — a knob labelled Brightness made the tail darker as it
opened, and the manual's advice that "the most natural range is between 25 and 50" (p.251) landed
on the dullest part of the travel instead of the liveliest.

## 124. in `reverb_step()`

A one-pole lowpass inside each comb, so every pass round the loop loses more high end — which
is what makes a tail decay into a thump rather than ringing on with the same tone.

THE DIAL DRIVES THE COEFFICIENT THROUGH A CURVE, and it has to. Taken linearly — which is what
this was — the filter is savage over most of the travel: measured on an offline render of this
very code (tools/render), the decay reached 25% of its requested length at Brightness 32, 41% at
64 and 53% at 96, only arriving at 100% when Brightness 127 switches the filter off altogether.
The instrument does not behave remotely like that: at Brightness 64 it decays for 10.76 s against
11.83 s at 127, i.e. 91%, so most of the dial is nearly transparent to the DECAY while still
moving the tail's colour (its 6-20 kHz band gains about 5 dB from 64 to 127).

Note this was NOT a feedback-gain error, which is where I first looked: the one-pole has unity DC
gain, so `fb` sets the low-frequency decay exactly right, and the render proves it by hitting the
requested time to within 0.01 s once the filter is out of the loop. What was wrong is how much
filter a given dial position asks for.

The dial drives the coefficient through a curve — see REVERB_DAMP_MAX
for the numbers, and for why the obvious ways of scoring this are misleading. Taken linearly, as
this was, the loop damped high frequency about twice as fast as the instrument does at any given
dial position.

TWO THINGS THIS IS NOT, both of which I diagnosed wrongly before measuring properly. It is not a
feedback-gain error: the one-pole has unity DC gain, so `fb` sets the low-frequency decay exactly
right, and an offline render confirms the requested time to within 0.01 s once the filter is out
of the loop. And the instrument's damping is not a fixed loss dressed up as a dial — its tail
demonstrably darkens as it decays, by 14-16 dB over three seconds at mid dial, which only
something inside the loop can do.
BRIGHTNESS 64 IS THE NEUTRAL DETENT, and the tail there is neither darkened nor lifted. That
is measured: at Type 3, Time 122, Brightness 64 the instrument's own tail decays at -5.47,
-5.35, -6.15 and -5.83 dB/s in the 125 Hz, 500 Hz, 2 kHz and 6 kHz bands — flat to within
0.8 dB/s across six octaves, so nothing is being taken out of one end.

This used to read 1.0 - bright^0.7, which puts a coefficient of 0.384 at the detent. A
one-pole that deep loses 3.6 dB of broadband energy EVERY TIME the signal passes it, twice
per trip round the tank, and that loss was most of why a Hall decayed in four seconds where
the instrument takes eleven. A damping control has to pass its neutral position through
untouched or it is a loss dressed up as a tone control.

THE DIAL IS READ AS A WHOLE, 0 to 127, NOT AS TWO HALVES ABOUT A DETENT. There is no detent:
see the mapping below for the sweep that settled it. An earlier version treated 64 as neutral
and mapped (brightness - 0.5) * 2, which had a second fault of its own worth remembering —
`brightness` is the dial over 127, so 64 arrived as 0.50394 and the tilt as +0.0079 rather
than zero, and NO dial position gave zero because 0.5 falls between 63 and 64. An exponent
under one amplifies that: pow(0.0079, 0.70) is 0.034, so a supposedly neutral detent asked for
0.021 of damping on every pass. Both faults are gone with the dial read whole.

## 125. in `reverb_step()`

BRIGHTNESS IS HIGH-FREQUENCY DAMPING ACROSS THE WHOLE DIAL, AND NOTHING ELSE. It never damps
the low end at any setting. MEASURED on a nine-point sweep of the dial (Hall, Time 127): the
125-500 Hz bands sit flat at about -5.0 dB/s from Brightness 16 to 112 while 8 kHz sweeps
-31.5 to -6.9. The dial moves the top and leaves the bottom alone.

WHAT THIS REPLACES WAS A SYMMETRIC GUESS, and the comment here used to say so: the lower half
damped the top, the upper half damped the BOTTOM by the same law about a neutral detent. The
instrument has no such detent and no low-end damping. Above 64 the engine was subtracting a
low-passed copy inside the loop and destroying the bass -- 125 Hz decayed at -53.8, -71.1 and
-79.8 dB/s at Brightness 80, 96 and 112 against the instrument's flat -5.0. Sixteen times too
fast, at settings anyone reaching for a bright reverb would use.

The two endpoints of the sweep are NOT usable and were not fitted: at Brightness 0 everything
above 500 Hz is far enough down that the fit is on noise, and 127 goes the same way at the
bottom. The dial was fitted over 16..112, where every band is above the floor.

## 126. in `reverb_step()`

Changing type resizes every delay line, so the positions into them are meaningless and the
contents are a room that no longer exists. Cleared rather than carried over — which is also
what the instrument does: "changing reverb type will force the Sound Engine to recalculate and
thus cause a brief moment of silence" (p.251).

## 127. in `reverb_step()`

THE PRE-DELAY IS MEASURED, NOT SCALED. Every other span is a length recovered from
the instrument's tap spacing and grows with the room; this one was read off the
hardware per room, and barely moves between them, for the reason above. The left
channel's figure sets the layout and REVERB_SPREAD carries the right.
THE TANK HAS ITS OWN LEAD-IN and the pre-delay has to give it back. The earliest
wet sample cannot leave before the first output tap, which sits 13% along the first
long line; an allpass passes its input straight through, so nothing in front of that
tap delays anything. Measured onset is input-to-first-tap, so the span in front of
the tank is the measurement MINUS that lead-in, or every room lands 7.2 ms late.

It also fixes the scaling. The lead-in grows with the room and the measurement does
not, so subtracting one from the other leaves a span that shrinks as the room grows
— which is what keeps the onset near-constant across rooms, the way the hardware's is.
THE TANK'S OWN LEAD-IN, per room, in samples at 96 kHz. The measured pre-delay is
input-to-first-wet-sample, so whatever the tank puts in front of its taps has to come
off the span ahead of them.

THIS IS A TABLE BECAUSE IT IS NOT DERIVABLE, and pretending otherwise put the Small
room 0.49 ms late. Computing it as the median tap offset assumes the lead-in scales
with the room exactly as every span does; solving for what it would have to be to hit
the measured onsets gives 485.6 * scale + 110.4, i.e. a term that does NOT scale. The
input diffuser's allpasses each pass a fraction of their input straight through, so
the first arrival is a mixture of paths that scale and paths that partly do not, and
no single length stands in for it. Measured against the onsets in kReverbPreDelay,
which is the same kind of table for the same kind of reason.

## 128. in `reverb_step()`

Diffusion first: three short allpasses smear the input within a few milliseconds, so there is
something there before the combs respond and no single tap stands out as an echo.

The coefficient RISES WITH THE REVERB TIME rather than sitting at a fixed 0.5, and both the
slope and the two limits it is held between are the instrument's own: a longer room diffuses
harder. The bounds are what matter most here — a coefficient outside them stops sounding like
this reverb — and they are narrow enough that the exact position within them is a detail.
ONE DECAY GAIN PER LINE. A line of L samples is traversed fs/L times a second, so losing 60 dB
in `timeSeconds` means losing 3 decades per timeSeconds, i.e. this per trip. The lines are
different lengths, so their gains differ; the Householder mix is orthogonal and takes nothing
out, which is what lets a closed form like this set the decay exactly with no trim fitted to a
render.

## 129. in `reverb_step()`

ONE BANK PER CHANNEL. The two run the same structure and decorrelate through their tap
phases, which is what the instrument does — its own outputs correlate at only +0.0044.
THE SWEEP PHASES, read once and used by both channels. The right channel runs a quarter cycle
behind, so the two never move their modes the same way at the same moment -- one more thing
keeping them uncorrelated, on top of the tap offset.

## 130. in `reverb_step()`

THE PRE-DELAY IS A SPAN OF THE TANK'S OWN MEMORY, the first one, and it does not
scale quite like the rest: the instrument's addresses are roomSize * k + 1200 and that
1200 is shared by every site, so the distance from the input to the first tap barely
moves between rooms. That is the explanation for a measurement taken long before there
was a structure to explain it — 12.89 ms in the Small room against 13.36 in the Hall,
while every line inside the room gets 68% longer.

## 131. in `reverb_step()`

── THE TANK ──────────────────────────────────────────────────────────────────────────

ONE BUFFER, A CURSOR THAT WALKS BACKWARDS, AND A LAYOUT OF NON-OVERLAPPING
SPANS. Nothing here computes a delay: a value written at address W reappears at address
W + L exactly L samples later, so each span IS its line and the two cannot drift apart.
Sections are visited in increasing address order and each reads before it writes, so the
read returns that section's own output from L samples ago rather than its neighbour's.

FOUR ALLPASS SECTIONS, NOT TWELVE. The instrument's recovered mixing gains settle this
exactly: they pair 0.4820 with 0.7676 and 0.3102 with 0.9038, and 1 - g*g for those two
g values is 0.7677 and 0.9038. That identity is the allpass, written out — a section
takes its delayed content d and its input x to (g*d + x, (1 - g*g)*d - g*x) — so the
gain table names four of them and their coefficients, and nothing is being guessed here.

An earlier build read the same site list as ONE serial chain of twelve allpasses. That is
what a chain of allpasses does to an impulse: at four trips a second it put fifty-odd
passes into every second of tail and turned the whole thing to noise. Four is the number
the gains support and the number a tank of this kind wants.

THE LINES RUN IN PARALLEL AND MIX INTO EACH OTHER. The input reaches all four, and the
Householder reflection below sends each line's output into all four on the next pass, so
there is no single path back to the start and so no one period for the tail to ring at.

## 132. `RVDLYM`

An allpass section, the form the recovered gains describe.
A MODULATED LINE. The read position sweeps across the slack at the end of the span,
interpolating between the two samples it falls between -- without that the delay would
step a whole sample at a time and the steps would be heard as clicks.
FOUR-POINT HERMITE, NOT LINEAR, and the reason is measurable rather than tasteful. Linear
interpolation between two samples has the response |1 - fr + fr*e^-jw|, which at a half-sample
offset is a complete null at Nyquist — a lowpass sitting inside the feedback loop, applied on
every pass. Measured, it left the engine with about 4 dB/s of excess high-frequency decay at the
bright end of the Brightness dial that no damping constant could remove, because it is not
damping. A Catmull-Rom cubic is flat to far higher frequency for four multiplies more.

THE WINDOW IS BIASED DOWN BY THREE SAMPLES so all four taps stay inside this line's own span. The
read sweeps [addr[n+1] - modMax - 3, addr[n+1] - 3], so ri+2 cannot reach addr[n+1] and read the
NEXT line's first cell, and ri-1 stays clear of addr[n]. Three samples of delay is nothing beside
a line of thousands, and reading a neighbour's span would mix two lines together.

## 133. in `reverb_step()`

BAND-LIMIT THE FEED. The instrument's reverb is MUCH darker than its input, and this
is where that comes from. Measured as the wet energy per band against the dry impulse
in the same capture -- which divides the excitation out, so a hardware pulse and a
unit-sample render compare directly -- it runs -1.5 dB at 2 kHz, -5.9 at 4 kHz, -15.4
at 8 kHz and -26.1 at 16 kHz, all relative to 1 kHz. Two poles at 3.5 kHz land within
0.5 dB of that across the whole range.

IT IS A FIXED FILTER, NOT IN-LOOP DAMPING, and the tail says which: the instrument's
6 kHz band decays only about 1 dB/s faster than its 125 Hz band, nowhere near enough
to account for a 15 dB deficit at 8 kHz. Something the signal passes ONCE takes that
out, so it belongs here in front of the tank and not inside it.

Leaving it out is what made the tank sound metallic: dead flat to 16 kHz, 26 dB of
treble the instrument does not have.

## 134. in `reverb_step()`

THE FOUR LINES. Each gets the input with its own sign and its own share of the
previous sample's mix. Injecting in phase into every line drives the tank's common
mode -- the one where all four hold the same thing -- and that mode has a period of
its own, so it beats. In phase it put a 12.2 dB lobe at 6.8 Hz into the tail.

## 135. in `reverb_step()`

Brightness, one filter per line and inside the loop, so it accumulates with every
pass rather than colouring the output once on the way out.
One damping path, in the loop, so it accumulates with every pass rather than
colouring the output once on the way out. There is no second path taking the low
end out: the instrument does not do that at any dial setting.

## 136. in `reverb_step()`

THE MIXING MATRIX, a 4-point Hadamard as two butterfly stages. Orthogonal, so it moves
energy between the lines without creating or destroying any -- which is what lets the
decay below be a closed form rather than a figure trimmed against a render.

EVERY LINE REACHES EVERY OTHER LINE ON EVERY PASS. That is what stops each one being a
comb in its own right: an echo entering one line leaves spread across all four, is
spread again a few milliseconds later, and the echo count squares instead of
repeating. Without it, four parallel lines are just four combs.

## 137. in `reverb_step()`

THE OUTPUT TAPS read INSIDE the four lines, never at a section's own write address.
Every cell in this buffer holds delay state, and the state at a write address is a
section's input side -- broadband by construction, and sixteen of those summed is
white noise, which is exactly what an earlier build sounded like. A tap part-way
along a line is the circulating signal at that point of its trip, which is what a
reverb output is made of.

ALTERNATING SIGNS, and EACH CHANNEL READS ITS OWN SET -- different lines at different
fractions, never the same positions offset by a constant. That is the whole of the
stereo; see kRvTapFrac.

## 138. `REVERB_WET_GAIN`

THE WET PATH IS QUIETER THAN THE DRY ONE, by about 11 dB, and this engine had it at almost
unity — which is why its reverb sat so much more prominently in a patch than the instrument's
does at the same settings.

MEASURED BOTH SIDES THE SAME WAY, at the module's own defaults (Type 0, Time 64, Bright 64):

```
  the instrument   full wet is 11.3 dB below full dry. A saw was fed through a real Reverb and
                   the oscillator cut mid-recording, so the tail could be measured on its own;
                   the DryWet dial was then swept and the steady output read at each step.
  this engine      the wet impulse response carries -1.1 dB of energy against the impulse that
                   produced it (tools/render, sqrt(sum h^2)), i.e. 10.2 dB too much.

```
The dial's SHAPE was already right and is unchanged — sweeping DryWet on the instrument gives
0.0 / 0.0 / +0.3 / -10.0 / -11.3 dB at 0/32/64/96/127, which the ramps below reproduce to
within 0.6 dB once this scale is applied. It was only ever the wet level that was wrong.

RE-MEASURE IF THE COMB SET OR THEIR COUNT CHANGES: this is the sum of REVERB_COMBS parallel
combs, so its level moves with how many there are.
RE-DERIVED THREE TIMES ON 2026-08-18. Twice because the comb count went 4 -> 8 -> 16, which is
exactly what the warning above this line is for — this scales the SUM of REVERB_COMBS parallel
combs, so it moves with how many there are, and each doubling added close to the 3 dB an incoherent
sum predicts (2.96 then 3.17). Once more, and much larger, because REVERB_INPUT_LP_HZ then took
11.9 dB of high-frequency energy out of the tank: 0.31 -> 0.2205 -> 0.1531 -> 0.6028.
The last one looks alarming beside the others and is not: the wet path is simply much quieter
before this gain now that it is band-limited, and the figure it has to land on is unchanged.

AND THE TARGET IS NO LONGER SECOND-HAND: -11.5 dB was measured on the instrument on 2026-08-18,
wet-to-dry energy through the impulse rig, agreeing with the -11.3 dB the old figure came from.
Re-derived for the tank. The figure below it is measured the same way it always was -- the wet
impulse response's energy against the impulse that produced it, sqrt(sum h*h) from tools/render
-- and the tank summed its taps 14.05 dB hotter than the comb bank it replaced, so this is the
old 1.0695 scaled to put full wet back on the instrument's -11.3 dB.

RE-DERIVED ONCE MORE 2026-09-06, 0.3016 -> 0.5002, when the tap set went from sixteen fitted taps
to the instrument's own SEVEN. Same method, same target.

RE-DERIVED EARLIER THE SAME DAY, 0.3956 -> 0.3016, and for the reason the warning above predicts: the
tap tables declared RV_OUTTAPS entries and filled only eight, so the other eight zero-initialised
to line eRvPre at fraction 0.0 -- all reading one cell, in +/- pairs that cancelled exactly. Half
the taps contributed nothing, proven by rendering with RV_OUTTAPS at 8 and 16 and differencing:
bit-identical, 0.000e+00. Filling both sets put sixteen live taps in where there had been eight,
which is the 2.36 dB this takes back out. Measured the same way as every figure above it.

## 139. in `reverb_step()`

DRY/WET IS NOT A CROSSFADE, and this was the largest single difference from the instrument.
The two gains are independent, each a ramp CUBED, and the ramps overlap: the dry side holds
full scale until the knob passes the middle and only then falls, while the wet side reaches
full scale AT the middle and stays there. So the centre detent is both signals at full, not
half of each — which is why the hardware's reverb at a middle setting is so much wetter, and
louder, than a linear blend of the same two signals.

The cube makes the taper steep at the quiet end: a quarter-open knob passes an eighth of the
wet signal, where a linear reading would pass a quarter.

## 140. `sound_engine_render_reverb_ir()`

Renders the Reverb's impulse response on its own — no patch, no voice, no audio device.

WHY THE ENGINE HAS A MEASUREMENT ENTRY POINT. The room sizes and the decay law above came from
putting a click through the real instrument and measuring what came back. The same click can go
through this code, and then the two sit in the same units and the same analysis: lag sets, decay
time, and how alike the two output channels are. That turns "does it sound like the G2" into a diff,
which is the only way the remaining work — the delay lengths and the topology — can converge instead
of being tuned by ear against a memory of the hardware.

The click is one sample at full scale, not the ~13-sample band-limited pulse the instrument's
converters produce. It does not need to match: the lengths are recovered from how the TAIL correlates
with itself, which the excitation's shape does not enter.

`out` receives `frames` interleaved stereo pairs at the ENGINE's rate, which is
deviceRate * ENGINE_OVERSAMPLE — pass 48000 to get the 96 kHz the hardware measurements are
expressed in, so a lag is the same integer in both.

THE TWO CHANNELS ARE A REAL PAIR, and this is where the tap sets get scored: render, then take the
PEAK OF THE L/R CROSS-CORRELATION OVER LAG and compare it with the instrument's 0.124..0.159.

TAKE THE PEAK, NEVER THE VALUE AT LAG ZERO. Correlation at lag zero scores a signal against a
delayed copy of itself as uncorrelated, so it cannot tell a decorrelated pair from a delayed one —
and that is not hypothetical: the arrangement this replaced read +0.03 at lag zero, looking like a
match to the instrument, while being a bit-exact copy of the left channel delayed by 110 samples
(peak +1.0000 at lag 110 with the line modulation switched off, in all four rooms).

## 141. in `sound_engine_render_reverb_ir()`

Cleared explicitly rather than relying on reverb_step()'s own type-change reset: a second render
at the SAME type in one process would otherwise start inside the first one's tail, and the
resulting lag set would be a mixture of two rooms — the identical trap the hardware captures hit
when settings were grouped by counting.

## 142. `sound_engine_meters_dirty()`

THE CHORUS, RENDERED THROUGH ITS OWN INPUT, so an engine wet can be put beside a hardware wet that
was made from the same signal. That matters more here than it did for the reverb: the reverb takes
an impulse, which is the same everywhere, but a chorus is judged on a sustained tone and any
difference in the SOURCE - band-limiting, level, the exact fundamental - would land in the
comparison as if it were the module's doing. Feeding it the hardware's own dry capture removes that
entirely, and what is left is only what the module did.

Rendered at the engine's rate like the reverb IR, so the caller supplies the DEVICE rate and gets
back ENGINE_OVERSAMPLE times as many samples per second.
WHAT THE ENGINE WOULD PUT ON A MODULE'S METER, for the renderer to show in place of the value the
instrument last sent over USB. False when the engine is idle or has nothing for that module, and the
caller then falls back to the database - so a patch shown with the engine off, or a module the
engine does not meter, looks exactly as it always did.
Whether any published meter or LED has changed since this was last asked. Consuming, so the render
loop can ask once a tick and redraw only when there is something new to draw.

## 143. `smooth_to()`

A cascade of one-pole lowpasses with the last stage fed back to the input — the usual ladder
arrangement, which is what gives a resonant peak at the cutoff and the gentle saturation the
classic filters are liked for. Two stages is 12 dB/octave, three 18, four 24, matching the dB
scroll button.
Smooth saturation for a ladder stage: y = x - x^3/3, the first two terms of tanh's series, held
flat outside +/-1 where the cubic would turn back on itself. Unity slope at the origin, so a quiet
signal passes through untouched and only a driven one is shaped.
One-pole move toward a target. Snapping when unprimed is what keeps a patch load instant.

## 144. `LADDER_KNEE`

LINEAR BELOW THE KNEE, saturating above it. The knee matters as much as the curve: a nonlinearity
that acts on every sample generates harmonics on every sample, and this filter runs at the output
rate with no oversampling, so anything it makes above Nyquist folds back down. With the cutoff up
near Nyquist and the resonant feedback amplifying those products before they fold, a plain
x - x^3/3 — only 0.2% away from linear at these levels — was enough to put audible rasp roughly
30 dB below the note. The hardware does not have this problem because it runs at 96 kHz.

So below LADDER_KNEE the response is exactly linear and generates nothing at all; above it the
curve approaches 1 exponentially, with unity slope at the knee so there is no corner to radiate
harmonics of its own. A driven filter still compresses; an ordinary one is untouched.

## 145. `cascade_hp_filter()`

FltHP: N ONE-POLE HIGH-PASSES IN SERIES, measured 2026-08-30 - the slope mode is literally the
pole count, 1 to 6, and every pole sits at the dial's own corner. Each stage is the complement of
the one-pole low-pass the ladder uses, so the same state array serves both and a node is only ever
one topology.

## 146. `svf_filter()`

FltStatic: A PLAIN RESONANT BIQUAD, and the only filter of the seven that is - its passband does
not move with resonance, where FltClassic's and FltNord's drop away. A Chamberlin state-variable
section gives low, band and high from one pair of states, which is what the FilterType selector
needs; band-reject is low + high.

state[0] is the low output, state[1] the band. TWO STATES ONLY, so it shares gLadder harmlessly.

## 147. in `ladder_filter()`

NO PASSBAND COMPENSATION. Feeding the output back subtracts from the input, so a ladder loses
passband level as resonance rises — and that is not an artefact to be corrected, it is the
specified behaviour: the manual (p.198, FltClassic) says "just like on analog filters the
amplitude of the passband will drop about 12 dB when the resonance is set to a high value".
Earlier versions put a quarter of the loss back, which made the filter louder than the
hardware exactly where a patch is most likely to be driven hard.

## 148. in `ladder_filter()`

The stage input saturates rather than clipping flat. A real ladder's transistor stages
compress smoothly, which is what rounds off the resonance peak instead of tearing it, and it
is what bounds self-oscillation. The cubic below is the standard cheap stand-in for that
curve: unity slope through zero, flattening to +/-2/3 at the limits, and constant beyond.

## 149. `envelope_step()`

One ADSR step. Times are in seconds.

EnvADSR's Shape scroll button selects the curve, in envShapeStrMap order: LogExp, LinExp, ExpExp,
LinLin - the first word naming the attack and the second the decay and release. The curves live in
paramCurves.c, shared with the envelope the editor DRAWS on the module face; the two carried the
same law with different sharpness constants until 2026-08-24, so the drawn envelope and the played
one were never quite the same curve.

APPLIED BY SHAPING A LINEAR 0..1 PROGRESS rather than by changing the step size, so a segment still
takes exactly the time its dial states whatever curve it is drawn with. (This used to say the
stages move linearly towards their targets; that stopped being true when the shapes were read.)

## 150. in `envelope_step()`

Retrigger from Release as well as from Idle. Only accepting Idle meant a note played
before the previous release had finished was ignored until it had: the envelope carried on
FALLING, holding the filter part open, and the attack began late from wherever it landed.
Attacking from the current level is what an ADSR does — the level is deliberately not
zeroed, so a fast retrigger rises from where it was rather than clicking to nothing first.

And from ANY stage when the voice's trigger count has moved: a Mono key played over a held
one, which keeps the gate open throughout - see voice_note_on().

## 151. `osc_waveform()`

One sample of the raw waveform, at whatever rate the caller is stepping the phase.
`voice` IS NEEDED HERE, and its absence was a bug rather than an omission. gSuperPhase is
[MAX_VOICES][MAX_ENGINE_NODES][2]; the Super branch below indexed it as gSuperPhase[node][0], which
puts the NODE number in the VOICE position and 0/1 in the node position. The compiler had been saying
so all along — passing `double (*)[2]` where a `double *` is expected is what a two-deep index into a
three-deep array produces.

It was not out of bounds, by luck: 28 nodes fits inside 32 voices. What it did do was ignore the
voice entirely, so every voice sounding the same node shared one pair of phase accumulators, and two
different Super oscillators trod on each other's storage. A single voice with one Super oscillator
is unaffected — it read [node][0][0] and now reads [0][node][0], the same value in a different slot —
so what changes audibly is polyphonic Super and multi-Super patches, which is the point.

## 152. in `osc_waveform()`

SHAPE DOES NOT REACH THE TRIANGLE. Measured on the instrument 2026-08-30: OscB set to
Tri returns exactly -19.2 / -28.1 / -34.0 dB with no even harmonics at raw 0, 64 AND
127 - the same symmetric triangle at every point on the dial. Shape is the PULSE WIDTH
and only the square uses it; we were skewing the triangle with it, which turned Tri
into a sawtooth at the top of the dial. The sine and saw already ignore it.

## 153. in `osc_frequency_hz()`

The two pitch modulation inputs are NOT equivalent. The upper one ("Pitch") is direct — what
arrives is what it does — while the lower one ("PitchVar") is attenuated by the module's Pitch
knob, which is the knob drawn alongside it. A vibrato patch rides on the variable one, since
that is the knob an aftertouch morph can open.

## 154. `oscillator_step()`

Runs the oscillator OSC_OVERSAMPLE times per output sample and filters the result back down.

The oscillators are the only part of the graph that creates harmonics which were not already
there — the filter, mixers and amplifiers below them are linear — so oversampling here alone
removes the aliasing without disturbing the delay, chorus and reverb, whose buffers are sized in
samples and would all have to be resized for a change of engine rate.

## 155. in `oscillator_step()`

Above Nyquist there is no waveform left to produce, only aliasing. Return silence rather than
just stopping the phase: a halted sawtooth is not silence, it is a DC offset held at whatever
level the waveform sat at, which thumps. The limit stays the OUTPUT rate's Nyquist even though
the oscillator now runs faster, because the decimator would remove anything above it anyway.

## 156. in `oscillator_step()`

One output for every OSC_OVERSAMPLE inputs, so the filter only has to be evaluated at the
output rate however high the oversampling factor is.
THE INDEX IS WALKED, NOT RECOMPUTED. This loop is the engine's hottest: it runs once per
oscillator per voice per oversampled sample, so at eight voices it is executed a few million
times a second, and it used to do an integer division (the %) on every one of its 128 taps.
Walking the read position and wrapping with a comparison is the identical sequence of taps in
the identical order — bit-for-bit the same output — for a fraction of the cost.

## 157. `lfo_step()`

One LFO sample. The waveform is generated bipolar and then mapped into whichever range the Pos
scroll button selects — posStrMap is {Pos, PosInv, Neg, NegInv, Bip, BipInv}, so half the settings
are simply the inverse of another, which is what makes an LFO able to close something as it opens
something else.

Not band-limited, and deliberately so: an LFO runs at control rate on the hardware, well below
anything that could alias into the audio band.

## 158. in `lfo_step()`

The synth names this shape Sqr2Tri, i.e. square AT one end of Shape and triangle at
the other. This runs the other way round - Shape at 0 gives very nearly a triangle
and winding it up drives the tanh into a square - so either the name reads
right-to-left or the morph is inverted. Nobody has listened to it against the
hardware, and a label is not enough to justify flipping a waveform, so it stands.

## 159. `FLT_CONTROL_MIN`

MODULATION IS SUMMED INTO THE DIAL VALUE AND CLAMPED THERE, then converted to a frequency exactly
once. This is not a rearrangement for tidiness — the clamp is the whole point, and it can only be
applied in this domain.

The Freq dial is a pitch: flt_cutoff_hz() is 13.75 * 2^(value/12), i.e. the value counts semitones
up from A-1, reaching the 21.1 kHz the manual quotes at 127. Because the dial is already
logarithmic in frequency, "multiply the cutoff by 2^(semitones/12)" and "add semitones to the dial
value" are algebraically THE SAME OPERATION. The previous code did the former, so its shape was
never actually wrong — what it lacked was any limit, because there is no natural place to put one
in the frequency domain, and a modulated cutoff could run far past the top of the dial's range.

It did not run away audibly only because two later clamps caught it: the Nyquist guard below and
LADDER_MAX_G. Both are safety limits on the filter model, not statements about the instrument's
range, so the cutoff was being bounded by an implementation detail at whatever frequency those
happened to bite. Clamping the control to the dial's own 0..127 puts the limit where the hardware
has it, and leaves the other two doing only the job they were written for.

## 160. in `filter_step()`

Whatever is patched into the Env input sweeps the cutoff, scaled by the Env knob. An envelope
there is what turns a static filter into one that opens and closes with the note.

FULL_MOD_SEMITONES is 64 against a modAmount that reaches 2.0 (the dial's 0..200%), so a
full-scale modulator at the knob's maximum sweeps 128 semitones — the dial's whole range.
That the two constants multiply out to the range exactly is the reason to believe 64 rather
than some value fitted to make the old unclamped arithmetic sound reasonable.

## 161. in `filter_step()`

Kbt moves the cutoff with the note, relative to middle C, at the percentage the scroll button
selects (manual p.196). One semitone of note is one unit of dial, which is what makes 100% Kbt
track the keyboard exactly.
Tracks the SOUNDING pitch, so a glide carries the cutoff with it rather than snapping.

## 162. in `filter_step()`

THE LADDER MODEL ONLY HOLDS WHILE g IS WELL BELOW 1. Each stage is a plain one-pole using the
previous sample's output, and four of those inside a feedback loop stop behaving as a filter
once the poles get close to Nyquist: measured with full resonance, everything up to g = 0.806
is clean at 60 dB of margin, g = 0.842 loses 7 dB of it, and by g = 0.874 the margin has
collapsed to 32 dB AND the passband has dropped 6 dB — the filter is misbehaving, not merely
adding a little distortion. What is heard is the saturation's harmonics folding back down,
amplified on the way by the resonant feedback.

This USED to be load-bearing, and the note here used to say so: when the graph ran at the
output rate, the top of FltClassic's 21.1 kHz range (manual p.198) landed outside the model
and this clamp was what kept it from rasping. Both halves of that have since gone away. The
whole graph now runs oversampled (ENGINE_OVERSAMPLE), so 21.1 kHz against 96 kHz gives
g = 0.75, inside the model; and the control clamp in this function now stops the modulated
cutoff exceeding the top of the dial in the first place.

So this is now a backstop that should never fire at a normal device rate, rather than
something shaping the sound. Left in place deliberately: it costs one comparison, and it is
the only thing standing between an unusual rate — or a future module whose range exceeds
FltClassic's — and a filter that misbehaves rather than merely distorts.

## 163. `LADDER_K_MAX`

MAXIMUM FEEDBACK, and it is not the textbook 4. That figure is for a ladder with no delay in
its loop; this one has a sample of it, and how much phase that sample contributes depends on
the sample rate — so the rate at which the loop actually reaches oscillation moved when the
engine started running oversampled, and the resonance went quiet with it.

4.3 IS MEASURED AGAINST THE INSTRUMENT, not chosen by ear. A saw was put through a real
FltClassic and through this ladder, and both responses taken the same way — output over input
at each harmonic of a 98 Hz saw, so the source spectrum cancels and only the filter is left.
Peak height above the passband at Res 127, a displayed cutoff of 1397 Hz:

```
                      12 dB      18 dB      24 dB
    the instrument    +31.5      +28.7      +25.9
    k = 4.3           +31.6      +28.5      +25.4      <- 0.3 dB mean error
    k = 5.0           +37.0      +33.9      +30.7      <- what this used to be

```
The previous note here recorded k 5.0 as "+79/+81/+73 dB". That was a different measurement,
not this one, and the two are not comparable — which is precisely why it read as though the
engine were 50 dB out when it was really about 5.

THE ANSWER DEPENDS ON INPUT LEVEL, because ladder_saturate() does. At a quarter of full scale
the same k peaks some 6 dB higher, the loop being driven less deeply into the knee. 4.3 is
right for a full-scale oscillator straight into the filter, which is how the instrument was
measured; a much quieter source will resonate more sharply here than it does there.

RE-MEASURE IF ENGINE_OVERSAMPLE CHANGES — the loop's phase, and so the k at which it reaches
oscillation, is a property of the rate rather than of the filter.

CONFIRMED INDEPENDENTLY 2026-08-24, by a measurement that shares nothing with the one above
except the instrument: noise through FltClassic, every setting divided by the same patch
bypassed, fitted against G^tap / (1 + k.G^4). It agrees with what this code already does, which
is worth recording precisely because it could have disagreed —
```
  - THE LOOP IS FOUR POLES LONG WHATEVER THE SLOPE, and the dB switch only moves the tap. That
    is what ladder_filter() has always done (feedback from state[LADDER_POLES - 1], return
    state[tapStage]) and it is now measured rather than assumed. Fitting each slope on its own
    gives the same k to within 0.011; a loop that matched the tap fits three to six times worse.
  - THE PEAK SPACING BETWEEN TAPS falls out of that topology at 3.01 dB and needs no fitting.
    The saw measurement above got 2.8 and 2.8; the noise one 3.2 and 3.0.
  - THE Res DIAL IS LINEAR IN FEEDBACK, which is how resonance is scaled here.
  - THE PASSBAND DROP AT FULL RESONANCE MEASURES -13.8 to -15.0 dB, against the "about 12 dB"
    the manual quotes and ladder_filter()'s note cites. Deliberately NOT compensating for it
    therefore remains right, and the real figure is a shade deeper than the manual's.
```
The continuous-time model used for the DRAWN response (flt_ladder_feedback(), paramCurves.c)
puts the top of the dial at k = 4.0 — measured 2026-08-30, when the hardware was found to sustain
an oscillation at Res 127; it read 3.914 before that, from a peak that was really a limited
oscillation. That is
NOT this number and must not replace it: that model has no delay in its loop and no saturation
in its stages, both of which move where oscillation actually starts. Same topology, different
constant, each measured for what it describes.

## 164. in `filter_step()`

g is the one-pole coefficient; the SVF wants 2.sin(pi.fc/sr), and for the corners a
patch actually uses the two agree closely enough that deriving one from the other
keeps a single cutoff path. Damping is 1/Q from the MEASURED resonance law, not from
the Q the dial prints - see flt_static_q() in paramCurves.c.

## 165. `eval_node()`

One node's output for one voice, written into value[n]. Extracted so the Voice Area pass and the
FX Area pass are the same code rather than two copies that could drift — they differ only in which
nodes they visit and in the voice index they carry.

`voice` selects the per-voice state; FX Area nodes are evaluated once with voice 0, which is also
the only voice the shared delay/chorus/reverb buffers ever see.

## 166. in `eval_node()`

A stereo mixer reads eight legs but has only four level knobs, so both legs
of a channel share one — and each CHANNEL contributes the average of its
two legs, not their sum.

That halving matters because the engine is mono. Where a stereo pair is
fed from one mono-collapsed module — an Fx-In's L and R, or a reverb's two
outputs — both legs carry the SAME value, so summing them counted that
channel twice. A patch mixing dry (one stereo source) against two separate
mono delays (a pair of different modules) therefore heard the dry and the
reverb 6 dB hot against the delays. Averaging is also the right mono
downmix for a genuinely stereo pair, so it is correct in both cases.

## 167. in `eval_node()`

THE TWO LEGS ARE THE LEFT AND RIGHT CHANNELS AND THEY STAY SEPARATE. This used to sum
them into one value, which is what made the whole engine mono however stereo the
modules feeding it were.

AN UNPATCHED SOCKET MIRRORS THE OTHER, and that rule is load-bearing rather than
tidiness. Cabling only the left socket is very common, and letting signal_in() return
its usual 0.0 for the absent leg would play such a patch out of one speaker — which
the instrument never does, both of its sockets being real. Mirroring leaves every
one-socket patch exactly as it sounded before this change.

WHAT DOES CHANGE IS THE DUAL-MONO PATCH: the same signal cabled to both sockets used
to be summed to 2a and that sum sent to both channels, i.e. 6 dB hot. It now plays at
a, which is what the hardware does with two sockets carrying the same thing.

## 168. in `eval_node()`

EVERY NODE MUST LEAVE BOTH LEGS VALID, and most kinds are mono and write only leg 0 —
the oscillators, the filter, LevAmp, LevMult and the mixers all do.

Leaving leg 1 at the zero this function starts it from was INVISIBLE while eNodeOut summed its
two legs: a spurious 0 on the right just made the sum equal the left, which is what got played.
It stopped being invisible the moment the Out module began keeping them apart.
PatchTestFiles/SimpleLead.pch2 cables one module's output 0 to Out L and its output 1 to Out R
— an entirely ordinary thing for a patch to do — and the right channel fell silent.

The three exceptions fill both legs themselves and must NOT be flattened here: an envelope
keeps its SHAPED AUDIO in leg 1, and the chorus and the Out module are genuinely stereo.

## 169. `tap_pair()`

One tapped module's stereo pair.

DELIBERATELY CONSERVATIVE: only eNodeOut is known to fill BOTH legs with a genuine left and right.
Most node kinds mirror leg 0 into leg 1, but some — the oscillators among them — write leg 0 and
leave leg 1 at the zero eval_node() starts it from. Reading leg 1 blindly would give those a
silent right channel, so anything that is not an Out module has its leg 0 mirrored, which is
exactly what the mono path did before stereo. An envelope used as an amp is the standing
exception: its SHAPED AUDIO is in leg 1 and is mono, so both channels take that.

## 170. `voice_is_finished()`

A voice is done when its key is up AND it has stopped making sound — only then can it be handed
to another note without cutting anything off. Which test that is depends on what is shaping the
note: an EnvADSR's own release when the patch has one, the anti-click ramp when it does not.

Asking the envelopes rather than watching the output level is deliberate: an envelope says when it
has finished, where a level has to be watched for long enough to be sure it is not just passing
through zero.

## 171. in `sound_engine_render()`

WORTH LOGGING, because reset_node_state() below empties every delay line and reverb buffer
in the engine. A topology change that is real — a module added, a cable moved — has to do
that. One that is NOT real takes the delay repeats and the reverb tail with it, and what
is heard is the effect stopping dead and then filling up again from nothing.

So if a delay or reverb is ever reported cutting out at random, this line is the first
thing to look for: if it fires when nothing about the patch changed, the signature is
unstable and the wipe is the symptom rather than the cause. Debug builds only.

Not the explanation for every such report: 45 s of idle playing, 120 parameter edits and
repeated select/deselect cycles all produced ZERO changes here, so whatever else may cut a
delay short, it is not this under those conditions.

## 172. in `sound_engine_render()`

Oscillator phases are deliberately NOT reset when a note starts. They free-run, as the G2's do
unless something is patched to their Sync input, and that matters more than it sounds: several
oscillators detuned by a few cents are what makes a patch thick, and starting them all at
phase zero has them summing as one voice for the seconds a 7 cent difference takes to drift
apart. Note events themselves are taken inside the sample loop below.

## 173. in `sound_engine_render()`

A PER-VOICE EnvADSR is the note's shape; the fixed ramp is only there to stop a click when
there is none to do that job. Per-voice only, and it has to be: an envelope after the mix
shapes the effect rather than the note, and counting it here would leave every voice unramped
AND have voice_is_finished() retire voices the moment a key came up.

## 174. in `sound_engine_render()`

FREE-RUNNING PHASE - THE CHEAP HALF, AND THE HALF THAT MATTERS FOR EVERY PATCH.

The instrument's oscillators and LFOs never stop, so a note started later finds them
somewhere else. Ours did not: gPhase is seeded once (a golden-ratio scatter, so voices start
decorrelated) and then FROZEN whenever a voice is idle, which meant two notes seconds apart
could begin on identical phase. Not a reset to zero - a freeze - but just as wrong.

Advancing the STATE and computing the AUDIO are separable: the phase an oscillator would
have reached is arithmetic, so it does not need the graph. MEASURED at 12.8% CPU against
12.3% with free-running off, i.e. half a point, where rendering the graph instead costs
over a point on a THREE-node patch and scales with the patch.
So do that once per block for the idle voices and skip the DSP entirely. Voices that ARE
sounding advance through the render as before, and the full free-run render still happens
for the patches that can actually be heard while idle.

MEASURE INSTANTANEOUS CPU, NOT `ps -o %cpu`, which reports an average over the process's
whole lifetime and made an early version of this look fifteen times worse than it was.
Take the delta of `ps -o time=` over a fixed window instead.

A "reset phase on note-on" option, for predictable bass, would zero gPhase for the allocated
voice instead. It is deliberately not the default and not written yet - the hardware
free-runs. See Docs/todo.md.

## 175. in `sound_engine_render()`

ONLY the voices the render will NOT touch. A free-running voice is rendered even
though it is not sounding, and oscillator_step() advances its phase as it goes - so
advancing it here as well moved it twice, jumping a whole block's worth at every
block boundary. The beat between the block rate and the oscillator's frequency put a
wah of a few Hz on every note. Get this condition wrong in the other direction and
the phase simply stops; it has to mirror the render's own test exactly.

## 176. in `sound_engine_render()`

The patch's own Vibrato, which is nothing to do with the cabling: it lives on a hidden
module beside Glide and Bend, and is how a patch gets aftertouch vibrato without an LFO
anywhere in it. The chosen controller sets the depth, so at rest there is none.

ONE PHASE FOR THE WHOLE PATCH, not one per voice: it is a property of the patch rather
than of a note, so a chord's notes wobble together instead of drifting apart.

## 177. in `sound_engine_render()`

Smoothed in DIAL units, not hertz. Smoothing a logarithmic control linearly in
frequency makes a knob move slowly at the bottom of its travel and leap at the
top; smoothing the dial value sweeps evenly in pitch, which is what the dial
means and what turning it sounds like.

## 178. in `sound_engine_render()`

── VOICE AREA: the whole area, once per sounding voice ──────────────────────────

Each voice is a complete instance of the Voice Area with its own oscillator phases,
filter state and envelopes, exactly as the hardware instantiates it. The FX Area is
NOT in here: it is one shared instance fed by the sum of the voices, which is what
lets a chord share one reverb instead of running 8 of them.

## 179. in `sound_engine_render()`

FREE-RUNNING. The instrument's Voice Area runs whether or not a key is down: an
oscillator patched to an output sounds on its own, an LFO keeps its phase, and a
note gates the ENVELOPE rather than the area. This engine used to skip the whole
voice when nothing was sounding, so any patch that depends on that produced
silence. Voice 0 is therefore always rendered, and while it holds no note it is
rendered UNGATED - no anti-click ramp to zero, no release tail, no retirement.

ONLY voice 0. The hardware runs every allocated voice continuously, so a poly
patch really does stack that many free-running oscillators, but matching that
would cost 32 voices of idle CPU for a difference of level. One instance is the
deliberate approximation; the post-mix section below already runs unconditionally
for the same reason, so a reverb tail outlives the last note.
FREE-RUN ONLY WHERE IT CAN BE HEARD. A patch whose amp is a per-voice envelope
puts out nothing until a key is pressed, so rendering its whole graph while idle
computes silence. MEASURED on a drone patch, which takes this path: 3.3% CPU
against 2.0% with free-running off - so the render is worth about 1.3 points on
three nodes and more on a large patch. Not ruinous, but it buys nothing at all
for the enveloped case, and the phase advance before the frame loop already
covers the part that IS audible there (where the oscillators are when the next
note starts).

The cost is that an LFO in an ENVELOPED patch still does not advance between
notes, which is what the instrument does. Fixing that properly means asking
whether a node reaches an Out without passing through a gated envelope, rather
than whether an envelope exists at all - see Docs/todo.md.

## 180. in `sound_engine_render()`

A KEY COMING UP MUST NOT INTERRUPT THE OSCILLATOR. The anti-click ramp exists for
patches with no envelope, and in exactly those the instrument carries on sounding
when the note is released - a key gates the envelope, and there isn't one. So the
free-running voice hands straight back to free running instead of ramping to
silence and then waiting out the release tail before resuming, which put an
audible dip in a drone that the hardware does not have. Phase and every other
per-voice state carry across untouched, so the level is continuous and there is
nothing to click. With an envelope in the chain the normal path still runs,
because there the envelope owns the release and cutting it would truncate it.

## 181. in `sound_engine_render()`

Portamento. The sounding pitch chases the played note; how fast, and whether at
all, comes from the patch's Glide setting. Exponential rather than linear — it is
what a glide sounds like, and the coefficient is set so the remaining distance is
down to a percent by the time the dial says.

## 182. in `sound_engine_render()`

RETIRED ONLY WHEN IT HAS GONE QUIET AS WELL as finishing its envelope. The
envelope alone is not enough: a patch whose EnvADSR modulates the filter rather
than acting as the amp goes on sounding after that envelope is idle, and dropping
it from the render at that moment cuts it off mid-note with a click. A patch that
genuinely drones simply never frees the voice, so new notes take the others and
eventually steal — which is what the instrument does with a droning patch too.

## 183. in `sound_engine_render()`

── AFTER THE MIX: one shared instance, whatever the polyphony ───────────────────────

The FX Area, plus any delay, chorus or reverb sitting in the Voice Area and anything
downstream of one — see mark_post_mix_nodes(). Runs even with every voice silent, so a
reverb tail or a delay repeat carries on after the last note is released rather than
being cut off with it.

## 184. in `sound_engine_render()`

The patch's other Out modules, summed rather than mixed at some fraction: that is
what the hardware's sockets do when two areas both drive them. Summed per channel
AND PER PAIR, so a patch whose Out modules feed different physical pairs — which
is what every measurement patch does — keeps them apart instead of folding them
into one stereo image.

## 185. in `sound_engine_render()`

With an envelope module shaping the note, the fixed ramp would only double up on it; it is
still applied when the chain has none.
THE METERS READ THE LOUDER CHANNEL. A per-channel peak would need a per-channel meter
to show it, and what these drive is one number.

## 186. in `sound_engine_render()`

The anti-click ramp is applied PER VOICE as each voice's output leaves the Voice Area
(see the voice loop), not here. Applying it to the mixed output would fade the whole
instrument — including the FX tail — every time any one note was released.
The user's own attenuation, ahead of the knee.

## 187. in `sound_engine_render()`

Soft knee rather than a hard edge. Below the knee nothing is touched at all, so ordinary
playing is untouched; above it the curve bends over instead of shearing the tops off, which
is both kinder to listen to and closer to what an overloaded analogue output does. The hard
clamp afterwards is only a guard against a bug producing something enormous.

## 188. in `sound_engine_render()`

FOUR CHANNELS IF THE CALLER ASKED FOR THEM, otherwise the two pairs are SUMMED.

The summing is what keeps the application unchanged: its device is stereo, every Out
module used to be added together whatever pair it fed, and a patch sending anything to
Out 3/4 would fall silent if this suddenly routed by destination. A caller that wants
them apart — the measurement harness, which needs the rig's dry reference on one pair
and its processed signal on the other — asks for four and gets them.

Choosing WHICH pair a stereo device should monitor, rather than always summing, wants
a menu item; see the todo. Summing is the answer that changes nothing until then.
