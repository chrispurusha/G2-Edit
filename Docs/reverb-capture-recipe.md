# Reverb capture recipe

What to capture on the hardware for the Reverb, what is already captured, and the traps that have
each produced a confident wrong answer at least once. Written 2026-09-06.

Captures live in `~/Documents/G2-Measurements/reverb/`. Every one has a `.json` sidecar recording
period, repeats and the setting list — **read the sidecar, never infer the layout from the audio.**

## What is already on disk — do not re-capture these

| file | what it holds | the catch |
|---|---|---|
| `g_rooms_t127` | all four rooms, Time 127, Bright 64 | period 8 s, 5 impulses. Hall's tail is ~11 s, so it overlaps the next impulse |
| `g_hall_bright9` | **Brightness 0–127 in nine steps**, Hall, Time 127 | period 20 s, 2 repeats. Good data, never fitted |
| `g_hall_bright` | Brightness 0 / 64 / 127, Hall | a subset of the above |
| `g_{small,medium,large,hall}_time` | Time 0–127 in five, per room | period 3 s — tails overlap badly above Time 64 |
| `g_{small,medium,large,hall}_decay` | Time 0 / 42 / 85 / 127, per room | period 20 s, 3 repeats. This is the clean one for decay |

**The Brightness sweep already exists.** `REVERB_DAMP_MAX` and `REVERB_BRIGHT_CURVE` are still a
guess, but that is an analysis job against `g_hall_bright9`, not a capture job.

## What is actually missing

### 1. A stereo capture — the one blocking current work

The L/R tap geometry is fitted against a cross-correlation peak whose **lag is ambiguous**: across
the five impulses in `g_rooms_t127`, Small returns a peak at ~677 twice and at ~1250 twice. Two
tap-distance clusters, five impulses, no way to tell which dominates. The magnitude is stable
(0.152–0.167); only the lag is not.

Needs a long period so tails never overlap, and enough impulses to average.

```
for r in 0 1 2 3; do
  ./tools/measure.py \
    --out ~/Documents/G2-Measurements/reverb/g_${names[$r]}_stereo.wav \
    --device "QU-24" \
    --loc VA --index 3 --sweep-mode 0 --values $r \
    --pre "VA 3 0 127" --pre "VA 3 1 64" --pre "VA 3 2 127" \
    --gate 60 --period 20 --repeats 10
done
```

About 14 minutes. `--sweep-mode 0` steps the room; `--pre "VA 3 0 127"` sets Time (parameter 0) and
`--pre "VA 3 1 64"` Brightness. Note both a mode and a parameter can be "0" — the flag says which.

**ONE FILE PER ROOM, NOT ALL FOUR IN ONE.** capture.c records every channel the device has and has no
channel selection, so on a 32-channel desk at 48 kHz that is 6.1 MB/s: four rooms in one file is
4.9 GB and a WAV stores its size in 32 bits. Per-room files are ~1.2 GB and safe. Nothing is lost for
this measurement — a correlation is scale-invariant, so the cross-file normalisation that made
`g_rooms_t127` worth having in one piece does not apply.

### 2. Brightness in a second room

`g_hall_bright9` is Hall only, so whether the damping law is room-independent is untested. Small is
the cheap check: its tail is 2.9 s, so the period can be short.

```
./tools/measure.py \
  --out ~/Documents/G2-Measurements/reverb/g_small_bright9.wav \
  --device "QU-24" \
  --loc VA --index 3 --param 1 --values 0,16,32,48,64,80,96,112,127 \
  --pre-mode "VA 3 0 0" --pre "VA 3 0 127" --pre "VA 3 2 127" \
  --gate 60 --period 8 --repeats 4
```

### 3. Signal-to-noise, for the double-slope question

Every RT60 on record is a straight fit over about 15 dB, stretched to 60, because the tail clears
the noise floor by only ~21 dB. A tail that decays fast early over a slower low-frequency one would
be invisible, and that would explain the manual's 17.58 s — unreachable by any current measurement.

This is not a different formula, it is a louder EXCITATION — see the level section below, which took
the tail from 42.7 dB above the noise to 71.5 dB without touching the desk. Folded into capture 1
rather than done separately.

## Getting enough level — the thing that actually mattered

The first attempt at this rig captured the wet tail at **-59.7 dBFS**, 42.7 dB above the noise. Four
changes took it to **-29.9 dBFS and 71.5 dB**, and only one of them was obvious:

| step | wet peak | peak-to-noise |
|---|---|---|
| Reverb straight into 2-Out | -59.7 dBFS | 42.7 dB |
| + 2.5x LevAmps on the outputs | -51.5 | |
| + **Pulse Time 64 instead of 0** | -39.7 | 61.8 |
| + wet on the louder input pair | **-29.9** | **71.5** |

**THE STIMULUS, NOT THE GAIN STAGING.** A Pulse at Sub range and Time 0 is a 0.135 ms click and
carries almost no ENERGY, so the tail it excites is tiny however the desk is set. Lengthening it is
worth **+13 dB** and costs nothing. Time 64 is the peak of the curve — past it the pulse spreads its
energy over time and the peak falls again (96 gives -40.0, 127 gives -42.2).

**WHEN THAT IS SAFE, AND WHEN IT IS NOT.** An L/R correlation compares the instrument's two outputs
to EACH OTHER, so it is self-referential and the excitation shape divides out. A hardware-versus-
ENGINE comparison is a different matter — see the note about matching excitation before comparing,
which cost a session once by faking a 4 dB flutter defect. For those, keep Time 0 and accept the
level.

**THE LEVAMPS ARE NOT OPTIONAL** and are worth exactly the 2.5x they claim (+8.2 dB, measured). They
top out at 4x, and pushing them there returned +3.3 dB where +4.1 was predicted, which is close
enough to the instrument's output ceiling to leave alone. Gain on the module's OUTPUT only.

**CHECK THE TWO INPUT PAIRS AGAINST ONE ANOTHER** before assuming a channel is faulty: route the
same pulse to both output pairs and compare. On this desk inputs 5/6 read 2.6 dB below 19/20 — small,
but free to exploit by putting the quieter WET pair on the louder inputs.

## THE RIG LIVES IN THE FX AREA NOW — PatchTestFiles/FxMeasure.pch2

```
VOICE AREA   EnvADSR --> Pulse --> 2-Out ("Out to" = FX 1/2)      the excitation, and nothing else
FX AREA      Fx-In --+--> [MODULE UNDER TEST] --> LevAmp x2 --> 2-Out (Out 3/4)   wet
                     \------------------------------------> 2-Out (Out 1/2)   dry reference
```

**The module under test belongs in the FX area, not the Voice Area.** A Voice-Area effect is
per-voice, so the voice cannot be reallocated until that voice's tail has decayed and a gate arriving
before then makes no sound. With the reverb in FX the gate is independent of it — 8 of 8 gates fire
at 3 s spacing with the Reverb at Time 0 AND at Time 127, where the same test in the Voice Area
depended on the tail.

It also generalises: the chorus, the delays and the compressor all live in the FX area, so the same
rig measures them by swapping one module.

Module indices as the file has them: VA EnvADSR 1, Pulse 2, 2-Out 3; FX Reverb 1, LevAmp 2 and 3,
2-Out (wet) 4, 2-Out (dry) 5, Fx-In 6. `measure.py --loc FX --index 1` for the module under test.

**COUNT GATES BY SCANNING FOR BURSTS, NEVER AT EXPECTED TIMES.** Every backdoor command costs
latency, so a loop asking for 3.2 s spacing actually delivers 3.7 s and the error accumulates — by
the eighth gate a fixed window is 4 s out and reports silence that is not there. That artefact
produced a stable-looking "fires, three silent, fires" pattern that cost a long detour.

## The patch

```
EnvADSR "Env" out --> Pulse (Sub range, Time 0) --+--> Reverb --> out 3-4   (wet)
                                                  \------------> out 1-2   (dry reference)
```

- **Module indices as built by the script above:** EnvADSR 1, Pulse 2, **Reverb 3**, 2-Out (wet) 4,
  LevAmp 5 and 6, 2-Out (dry) 7. `measure.py --index 3` for the Reverb; an older note saying index 2
  predates the envelope being added first.
- **Wet goes out 3/4, dry out 1/2** in this build, which lands wet on capture channels 18/19 and dry
  on 4/5 (0-based). Watch channels 26/27: they carry the desk's main mix and were once nearly as hot
  as the signal.
- **The Pulse must be up-rated** — its cables turn orange, not yellow. At control rate it cannot make
  a pulse shorter than a control period and the excitation comes out long and ragged.
- **Reverb DryWet at 127**, so the wet pair carries no dry.
- **Gain goes on the module's OUTPUT, never its input.** Input gain risks saturating the algorithm's
  fixed-point maths and makes the response nonlinear. 2.5x LevAmps on all four outputs is the
  arrangement that has been used.
- The dry pair earns its cable: it locates every impulse to the sample, it is the system reference
  captured in the same take, and comparing dry repeats to each other says whether the excitation
  repeated at all — independently of the module.

## SPACE THE IMPULSES BEYOND THE TAIL, NOT MERELY BEYOND THE NOTE

The Reverb sits in the VOICE AREA, so every voice carries its own reverb and cannot be reallocated
until that voice's tail has decayed. A gate arriving before then gets no voice and makes no sound —
correct behaviour, and it looks exactly like a dropped note. At one fixed gate timing:

| Reverb Time 127 (tail of seconds) | 3 gates in 8 |
| Reverb Time 0 (very short tail)   | 8 gates in 8 |

Two sessions spent chasing this as a `DEVNOTE` comms fault. It is not one. `--period` must exceed
the TAIL, which at Time 127 means about 3 s for Small and 12 s or more for Hall.

**Better still, put the module under test in the FX AREA**, where it is not per-voice and gating is
independent of its tail. That is also what a real patch does. `DEVNOTES` reports which notes the
instrument believes are held, if you need to see the state rather than infer it.

## Before believing anything

- **Check every backdoor command returns OK.** A whole DryWet sweep was once reported as "flat within
  0.8 dB" when the app had exited and all five `DEVSET`s silently failed, giving five identical
  captures. `measure.py` now raises if a command is not consumed, but check the console anyway.
- **Confirm the captures actually differ from one another** before analysing.
- **Type is a MODE, not a parameter.** Three full captures measured the same room before this
  surfaced. `--sweep-mode 0` for the room; `--param 0` is Time.
- **The Pulse needs `--gate 60`** or the tail never fires.
- **Group by TIME from the sidecar, never by counting impulses.** 18 of 20 detected once put a Large
  impulse inside the Medium average and reported two rooms as one.
- **Scan every channel for the signal.** The 8-channel Fireface captures put dry on ch 4/5 and wet on
  ch 6/7 (0-indexed). On the QU-24, G2 outs 1–2 land on inputs 5–6 and outs 3–4 on 19–20, and the
  desk's main mix on 29–30 is far louder than either. Bleed at 95–128 dB looks plausible and is not
  the signal.
- **Never record through ffmpeg.** Its avfoundation input resamples to 48 kHz while reporting the
  interface's real rate: a 192 kHz capture came back as a file *labelled* 192000 with a quarter of
  the samples in it, and nothing in the file said so. Use `./tools/capture`.
- **Check the instrument's output level.** A capture taken with the volume down buries everything
  under the second harmonic in noise and still fits confidently.
- **Match the analysis window to the one the reference was measured in.** An apparent L/R correlation
  regression turned out to be a whole-segment average including the near-silent end, where the
  statistic is meaningless.

## Turning a capture into numbers

`tools/analyse_ir.py` measures pre-delay, arrivals, recirculating lags, decay and spectra, and has a
`--selftest` against a synthetic response with known answers. `tools/render` puts the same click
through the engine into the same file shape, so one analyser command line measures both and the
comparison is a diff rather than a judgement.

**For L/R, take the PEAK of the cross-correlation over lag, never the value at lag zero.** Lag zero
scores a signal against a delayed copy of itself as uncorrelated, which is how a right channel that
was a bit-exact 110-sample delay of the left passed as a decorrelated pair.
