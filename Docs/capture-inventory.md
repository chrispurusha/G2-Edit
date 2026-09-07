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

---

## Covered — captured, fitted, and the constants carry their measurements

| module | captures | state |
|---|---|---|
| **Reverb** | 19 files: four rooms × Time, decay, stereo; Brightness in Hall, Small and Medium | The most complete. Room scale, decay law, pre-delay, wet level, input filtering, stereo tap sets and the Brightness law are all measured. |
| **OscShpB** | `G2Captures/oscshpb/`, 8 files | Harmonic spectra per waveform at two Shape settings. |
| **OscA** | `G2Captures/osca/`, 6 files | |
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
| **LfoShpA, LfoC** | `lfo_rate_hz()` cites the manual for the Sub range and one hardware divider reading; the other three ranges are unverified. NOW ALSO THE RIG'S OWN CLOCK, so an error here shifts every capture's timebase. |
| **LevMult** | |
| **Mix4to1C, Mix4to1S** | Whether the mixer sums or averages, and what its level law is. |
| **FxtoIn** | The Pad menu is read as +6 / 0 / −6 / −12 dB from its label. Never checked. |
| **2-Out, 4-Out** | Same: the Pad is read from its label. |
| **Pulse** | The Time dial's law. It is the rig's excitation, so its width enters every impulse response taken. |
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
