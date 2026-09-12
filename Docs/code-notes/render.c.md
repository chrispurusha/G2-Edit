# render.c notes

The longer comments from `render.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

WHAT THIS IS FOR.

The reverb's room sizes and decay law were measured by putting a click through the real instrument
(tools/measure.py) and reading the result (tools/analyse_ir.py). What is left — the delay lengths and
the topology they sit in — cannot be settled that way alone, because a plausible-sounding wrong
arrangement is the hardest kind of error to find by ear.

So this puts the SAME click through the engine and writes a file with the SAME shape as a hardware
capture: four channels, dry on 1-2 and wet on 3-4, plus the .json sidecar measure.py writes. The
consequence is that ONE analyser command line works on both, and the two answers are directly
comparable rather than merely similar in spirit:

```
    ./render --out engine.wav --settings 0,1,2,3 --sweep type
    python3 analyse_ir.py engine.wav      --dry-channel 1 --wet-channel 3 --raw --skip 0
    python3 analyse_ir.py g_rooms_t127.wav --dry-channel 5 --wet-channel 7 --raw --skip 2

```
The dry channels carry the click itself, which is what the analyser locates impulses from — the same
job the second output pair does on the hardware. Here it is exact rather than a reference, but the
analysis must not know the difference or it would not be the same analysis.

Rendered at the ENGINE's own rate (96 kHz for a 48 kHz device), so a lag is the same integer as in
the hardware tables and no rescaling stands between the two sets of numbers.

Build: see tools/do-render, which links the engine's headless dependency set.

## 2. `read_wav32_channel()`

A MINIMAL READER for the files ./tools/capture writes: 32-bit PCM, any channel count. Only one
channel is wanted (the dry reference), so it is de-interleaved on the way in and converted to the
float the engine expects. Deliberately not a general wav reader - it accepts exactly what the
capture tool produces and refuses anything else rather than guessing.

## 3. in `main()`

THE CHORUS TAKES A DIFFERENT PATH ENTIRELY and returns early: it is driven by a real recording
rather than an impulse, so none of the sweep machinery below applies to it. Feed it the DRY
channel of a hardware capture and the wet it returns can be put straight beside that capture's
own wet - same source, same fundamental, same band-limiting, so any difference is the module.
