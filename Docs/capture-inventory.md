# Capture inventory

What the sound engine implements, what has been captured off the hardware, and what has not. Written
2026-09-07 because the answer was not written down anywhere and had to be reconstructed from three
capture directories and `findings.txt`.

**Where captures live.** None are in the repository — they are far too large.

| location | what is in it |
|---|---|
| `~/Documents/G2-Measurements/reverb/` | the reverb programme, 19 captures, about 21 GB |
| `~/Documents/G2 Captures/` | the early chorus and delay captures |
| `~/Documents/GitHub/G2Captures/` | oscillator and filter sets, plus the A/B listening files |

A capture is only worth keeping if its `.json` sidecar is beside it. Several early ones have no
sidecar and their settings are only recoverable from `findings.txt`.

**TWO RIGS, AND THE SIDECAR DOES NOT SAY WHICH.** Verified 2026-09-07 by reading the WAV headers, not
from any note - nothing recorded it at the time. The rate and channel count identify the rig on
sight:

| rig | header | where it appears |
|---|---|---|
| Fireface UC, G2 outs 3/4 into inputs 5/6 (zero-based 4/5) | 8 ch, 192000 Hz | every reverb `_decay`, `_time` and `_rooms` capture |
| QU-24, inputs 5/6 (zero-based 4/5) | 32 ch, 48000 Hz | every reverb `_stereo` capture, `g_medium_bright7`, `g_small_bright7`, and all of the older chorus/delay/oscillator/filter sets |

This split is not cosmetic. The engine runs at 96 kHz, so a 192 kHz capture carries exactly two
samples per engine sample and a 48 kHz one carries a single sample per TWO engine samples - an odd
engine-sample lag cannot be resolved at all at 48 kHz. Both of the reverb's open questions sit on the
wrong side of that line: the L/R peak lag was measured only on 48 kHz captures, and the Brightness
fit spans three rooms of which Hall is 192 kHz and Medium and Small are 48.

`analyse_ir.py` reads the rate from each file and converts through `engine_rate`, so nothing is
silently mis-scaled - but no arithmetic can recover a resolution the capture never had.

Measured levels on the Fireface rig, for reproducing it: true peak -14.4 dBFS in the loudest burst,
idle floor -74.0 dBFS on the connected pair against -85.0 dBFS on the interface's own unused inputs.
That last pair of numbers says 11 dB of the noise arrives down the cable from the G2 and is not the
converter's to fix, so a quieter interface would not buy much - about 60 dB of usable range on an
impulse is what this rig gives.

---

## Covered — captured, fitted, and the constants carry their measurements

| module | captures | state |
|---|---|---|
| **Reverb** | 19 files: four rooms × Time, decay, stereo; Brightness in Hall, Small and Medium | The most complete. Room scale, decay law, pre-delay, wet level, input filtering, stereo tap sets and the Brightness law are all measured. |
| **OscShpB** | `G2Captures/oscshpb/`, 8 files | Harmonic spectra per waveform at two Shape settings. |
| **OscA** | `G2Captures/osca/`, 6 files | |
| **Pulse** | `pulse192b.wav`, 17 dial values at 192 kHz | Time dial measured across the whole range in Sub. Every width is an integer count of 96 kHz samples. Our closed form is ~11% long and needs refitting - see `findings.txt`. Amplitude still uncalibrated. |
| **FltClassic** | `G2Captures/fltclassic/`, 18 files | Bypass, resonance and spectra. Ladder topology and K range settled from it. |

## Partly covered — measured, but the captures are thin or gone

| module | what exists | what is missing |
|---|---|---|
| **StChorus** | 3 files in `G2 Captures/`, two Detune settings at two tone frequencies | Rate, centre delay and the triangle LFO shape are settled. There is no systematic sweep of either dial, and the STEREO behaviour has never been measured — it is on `to-test.txt` as needing an ear. |
| **DelayA / DelayB** | 3 files, feedback at 64/96/127 | Feedback and the LP dial measured; the Time dial and its Clk mapping were measured separately and are hardware-confirmed. No captures retained for either. |
| **Compress** | none retained | Threshold, ratio, attack and release were all measured and corrected — the numbers are in `findings.txt`, the audio is not. Re-deriving anything means re-capturing. |
| **EnvADSR** | none retained | Curve sharpness measured 2026-08-24; the attack FORM re-measured 2026-09-07 and our law confirmed against a one-pole. Both from captures that were not kept. |
| **LevAmp** | none retained | Gain law measured at 33 dial positions 2026-08-30, four segments. |

## Not captured at all — implemented on the manual, a datasheet reading, or an assumption

These play in the engine and have never been measured against the instrument.

| module | what is assumed |
|---|---|
| **FltLP, FltHP, FltStatic, FltNord** | All four are on `to-test.txt` as needing an ear and none is tuned. FltClassic is the only filter with captures. |
| **LfoShpA** | Rate Sub, Rate Lo and Rate Hi all MEASURED 2026-09-07 and now agree within 0.013% - Rate Lo's base was 0.43% low and is now derived as Rate Hi over 16. BPM and Clk still unverified (they need a master clock). LfoC untested. |
| **LevMult** | |
| **Mix4to1C, Mix4to1S** | SUMS - measured 2026-09-07, +6.02 dB per doubling of identical inputs, and the engine already matches. Its level-dial law and its -6/-12 dB Pad are still unmeasured. |
| **FxtoIn** | Pad MEASURED 2026-09-07: +6.02 / 0 / −6.02 / −12.06 dB, exactly its label, confirmed three ways (direct capture, the manual, and the compressor probe). Its absolute reference is still not separated from the rest of the chain. |
| **2-Out, 4-Out** | Pad MEASURED 2026-09-07 and it BOOSTS: two positions, 0 dB and +6.02 dB, confirmed by the manual ("on the Output modules between 0dB and +6dB"). The engine had it halving and was 12 dB out when engaged; fixed. Note the parameter is unclamped on the wire, so a backdoor sweep shows further +6 dB steps the dial cannot select. |
| **Mix4to1C** | Pad MEASURED 2026-09-07: THREE positions, 0 / −6.01 / −12.04 dB (the DSP clamps at 2, so this one is real). Level taper measured as a CUBE, not the square the engine carried — 6 dB out by mid-dial. Both fixed. Lin mode was already right. |
| **Constant** | |

## Where to start, if the list is being worked through

The rig is now general — `PatchTestFiles/FxMeasure.pch2` measures any FX-area module by swapping one
module, and the engine can render the same patch for comparison. So the FX modules are cheap:

1. **StChorus** — already partly done, the stereo question is open, and it is the last unmeasured
   effect anyone will hear.
2. **DelayA / DelayB** — a tail measurement the rig is already shaped for.
3. **Compress** — needs a different stimulus (a level ramp, not an impulse), so it wants thought.

Two that are worth doing for a different reason, being upstream of everything else:

4. **Pulse** — its width sets the excitation of every impulse response we take.
5. **LfoShpA** — its rate now sets the timebase of every capture.

The filters need a stimulus the rig does not have yet: a magnitude/phase response wants a sweep or
noise, not a click. See the MEASUREMENT PROGRAMME entry in `todo.txt`.
