# waveModels.h notes

The longer comments from `waveModels.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `wave_sine1()`

── The shaped oscillator waves, measured from the instrument ───────────────────────────────────

ONE HOME FOR EVERY MEASURED CONSTANT, so a wave cannot be corrected in one place and left wrong in
another. This is exactly what paramCurves.c exists for and was pulled out of renderParams.c to
achieve; these wave laws had the same problem and now get the same treatment.

THE PROBLEM THIS SOLVES WAS REAL, not theoretical. The editor DREW these waves from laws measured
off the hardware in August 2026 (Docs/todo.md has the capture method, the sweeps and the
correlation figures), while soundEngine.c's osc_shp_wave() carried a separate set derived from the
manual's prose. The two disagreed on things the measurement had settled: the engine remapped Shape
as (shape - 0.5)/0.49 CLAMPED AT ZERO, so HALF THE DIAL WAS DEAD where the capture shows harmonics
growing from raw 16 upward; its Sine3 was a rectified sine, an even-harmonic reading the spectrum
disproves; its Sine4 was a tanh soft clip; its Sine1 had both the wrong phase and the wrong
endpoint. Those were corrected in place on 2026-08-23 — leaving two copies that agreed on the day
and nothing to keep them agreeing.

PLATFORM-FREE, like paramCurves.c. Nothing here draws or makes a sound; it needs no graphics API
and no audio device, which is what lets both the editor and the VST3 plug-in link it.

── Why this is two kinds of function ───────────────────────────────────────

SINE1..SINE4 ARE COMPLETE. They are closed-form and have no discontinuity, so every consumer wants
the identical value at a given phase and there is nothing to decide. Call them and use the answer.

THE OTHER FOUR ARE PARAMETERS ONLY, and that is deliberate rather than half-finished. TriSaw,
DblSaw, Pulse and SymPulse all contain a step, and how you render a step is a property of what you
are rendering INTO, not of the wave:
```
  - the sound engine band-limits, because an unlimited step aliases across the whole spectrum;
  - the editor draws a 200-point curve, and a hard step between two sample points is simply
    missed, so it adds explicit narrow ramps at the crossings to make the drawn line pass through
    zero where the real wave does.
```
Handing both a single "sample" function would force one of them to undo the other's work. What
they must share is the measured law — the duty, the skew, the detune — and that is what is here.
The shape of the wave is settled; the sampling of it is the caller's business.
