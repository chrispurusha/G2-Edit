# tools — measuring the real G2

Characterising a module by measurement rather than by guesswork. The engine's `KNOWN GAPS`
(`src/soundEngine.h`) are mostly questions these can answer: reverb delay lengths and room sizes,
filter responses, envelope times, static transfer curves.

Nothing here is part of the application build. `capture` is a single C file; the two Python scripts are
stdlib only, deliberately, so they run wherever the editor builds.

```
cc -O2 -Wall -o capture capture.c -framework CoreAudio -framework AudioToolbox -framework CoreFoundation
```

## The three parts

| | |
|---|---|
| `capture.c` | Multichannel recorder, through CoreAudio's HAL. `./capture --list`, then `--device Fireface --out f.wav --seconds N`. |
| `measure.py` | Steps a parameter or a mode on the hardware while `capture` records, and writes a `.json` sidecar describing the plan. |
| `analyse_ir.py` | Turns a capture into numbers: pre-delay, arrivals, recirculating delays, decay time, spectra. `--selftest` checks it against a synthetic response with known answers. |
| `render.c` + `do-render` | Renders **our own engine's** reverb response into a file shaped like a hardware capture, so one analyser command line measures both and the difference is a diff. |

## Measuring the engine against the instrument

```
./do-render && ./render --out engine.wav --sweep type --settings 0,1,2,3 --time 127
python3 analyse_ir.py engine.wav        --dry-channel 1 --wet-channel 3 --raw --skip 0 --decay-span 15
python3 analyse_ir.py hardware.wav      --dry-channel 5 --wet-channel 7 --raw --skip 2 --decay-span 15
```

`--decay-span` is not optional for a comparison: a recording stops at its noise floor while a render
decays to denormals, so without a cap one side measures early decay and the other measures all of it.

**Point any new analysis at the render first.** It is the only case where the answer is known, and it has
already overturned two plausible readings of the hardware data: allpass lengths do not appear in a tail
autocorrelation at all, and "drop the lags that are sums of other lags" is not a valid way to recover
delay lengths, because a set closed under sums and differences explains itself in both directions. See
`generating_sets()`, which searches instead of filtering, and was validated by recovering the engine's
own comb lengths before being pointed at hardware.

`do-render`'s nine-file source list doubles as a check that the engine is platform-free. If it ever needs
`graphics.c` or `audioOutput.c` to link, something has been added to the engine that does not belong
there — and the VST3 plug-in will break for the same reason.

## The patch it expects

```
EnvADSR "Env" out --> Pulse (Sub range, Time 0) --+--> [module] --> out 3-4   (wet)
                                                  \-------------> out 1-2   (dry reference)
```

One note per impulse; the envelope's rising edge fires the Pulse. **The Pulse must be up-rated** — its
cables turn orange rather than yellow. At control rate it cannot produce a pulse shorter than a control
period, so the shortest Time setting does not exist and the excitation comes out long and ragged.

**The dry pair earns its cable three times over.** It locates every impulse to the sample instead of
inferring position from the processed signal; it is the system reference — converters, cabling,
interface — captured in the *same* take, so deconvolution cannot drift; and comparing dry repeats to one
another says whether the excitation repeats at all, independently of the module. Measured excitation
repeatability with this patch is +0.99.

Put any gain the levels need on the module's **output**, never its input: a bigger click into the
algorithm risks saturating its fixed-point maths, which makes the response nonlinear and the
measurement meaningless.

The editor must be running with `G2_EDIT_BACKDOOR=1` — that is the channel `measure.py` drives, and
`CABLE`/`DELCABLE`/`PUSH` let a measurement patch be built by script rather than by hand.

## Example: the reverb's room sizes

```
./capture --list
python3 measure.py --out rooms.wav --device Fireface --loc VA --index 2 \
    --sweep-mode 0 --values 0,1,2,3 \
    --pre "VA 2 0 127" --pre "VA 2 2 127" --pre "VA 2 1 64" \
    --period 8.0 --repeats 5 --gate 60
python3 analyse_ir.py rooms.wav --dry-channel 5 --wet-channel 7 --raw --skip 2
```

`--sweep-mode` matters: a **drop-down selector is a MODE, not a parameter**. Driving one as a
high-numbered parameter is accepted locally, dropped by the G2, and reported OK — that produced three
captures of the same room before `DEVSET` learned to check. Results in the `REVERB` entry of
`todo.txt`.

## Traps worth knowing before trusting a result

- **ffmpeg cannot do this job.** Its avfoundation input reports the interface's real rate and then
  delivers 48 kHz anyway, because an AVCaptureSession resamples: a 192 kHz capture becomes a file
  *labelled* 192000 with a quarter of the samples in it, and nothing in the file says so. Every length
  measured from one is out by four. The tell is that a 5 s capture claims to be 1.14 s long.
- **Group impulses by time, not by count.** `analyse_ir.py` reads the sidecar's plan for this. Counting
  assumes none were missed; 18 detected out of 20 put one room's impulse inside another room's average
  and reported the two rooms as one.
- **A reverb's wet consistency is low and that is not a fault.** Repeats correlated at +0.75 with an
  excitation repeatable to +0.99, because a tail longer than the spacing leaves residue under the next
  impulse. That residue is uncorrelated with the impulse, so averaging removes it — which is the point.
  Averaging is gated on the *excitation's* repeatability, never the module's.
- **Peak-picking does not give delay lengths.** A tank is sparse for a few arrivals and dense after
  that; the peak list changes with the threshold, which is how you know it is fiction. Use the tail
  autocorrelation, and treat a lag as a candidate rather than a length — a recirculating network shows
  its lengths *and* their sums, so the sums are the corroboration.
- **Schroeder integration measures the window** if the segment ends on a noise floor rather than in
  silence. An 8 s window reported ~15 s for every room; 20 s reported ~39 s for every setting. Both are
  twice the window. `decay_time()` fits the log envelope instead and reports its span, r² and
  extrapolation factor so a bad fit is visible rather than merely plausible.
- **Deconvolution outside the excitation's band is undefined, not ill-conditioned.** Recording a 96 kHz
  device at 192 kHz leaves half the spectrum as converter noise, and a floored denominator divides it by
  epsilon. Zero those bins (`--band-db`) — but note zeroing band-limits the answer, turning each delta
  into a sinc whose side lobes read as extra reflections.
- Ask for only the channels you need (`read_wav(path, wanted)`): eight channels of a two-minute 192 kHz
  take is 189 million samples, about 6 GB as Python floats.
