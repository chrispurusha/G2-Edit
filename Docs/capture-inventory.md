# Capture inventory

What the sound engine implements, what has been captured off the hardware, and what has not. Written
2026-09-07 because the answer was not written down anywhere and had to be reconstructed from three
capture directories and `findings.md`.

**Where captures live.** None are in the repository — they are far too large.

| location | what is in it |
|---|---|
| `~/Documents/G2-Measurements/reverb/` | the reverb programme, 19 captures, about 21 GB |
| `~/Documents/G2 Captures/` | the early chorus and delay captures |
| `~/Documents/GitHub/G2Captures/` | oscillator and filter sets, plus the A/B listening files |

A capture is only worth keeping if its `.json` sidecar is beside it. Several early ones have no
sidecar and their settings are only recoverable from `findings.md`.

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
| **Pulse** | `pulse192b.wav`, 17 dial values at 192 kHz | Time dial measured across the whole range in Sub. Every width is an integer count of 96 kHz samples. Our closed form is ~11% long and needs refitting - see `findings.md`. Amplitude still uncalibrated. |
| **FltClassic** | `G2Captures/fltclassic/`, 18 files | Bypass, resonance and spectra. Ladder topology and K range settled from it. |

## Partly covered — measured, but the captures are thin or gone

| module | what exists | what is missing |
|---|---|---|
| **StChorus** | 3 files in `G2 Captures/`, two Detune settings at two tone frequencies | Rate, centre delay and the triangle LFO shape are settled. There is no systematic sweep of either dial, and the STEREO behaviour has never been measured — it is on `to-test.md` as needing an ear. |
| **DelayA / DelayB** | 3 files, feedback at 64/96/127 | **More complete than this row used to claim.** Feedback is linear to exactly unity (measured at 64/96/127; the hardware does not decay at all at 127). LP measured at five settings and HP at four, both by the BURST method - a short saw burst separates the repeats so repeat[n+1]/repeat[n] is the per-pass response - with the HP fit then validated at three settings that were NOT used to fit it. Dry/wet measured and found to be the same non-crossfade the reverb uses, needing no wet attenuation. Time and its Clk mapping hardware-confirmed separately. The audio is not retained; the numbers and the method are in `findings.md`. |
| **Compress** | none retained | Threshold, ratio, attack and release were all measured and corrected — the numbers are in `findings.md`, the audio is not. Re-deriving anything means re-capturing. |
| **EnvADSR** | none retained | Curve sharpness measured 2026-08-24; the attack FORM re-measured 2026-09-07 and our law confirmed against a one-pole. Both from captures that were not kept. |
| **LevAmp** | none retained | Gain law measured at 33 dial positions 2026-08-30, four segments. |

## Not captured at all — implemented on the manual, a datasheet reading, or an assumption

These play in the engine and have never been measured against the instrument.

| module | what is assumed |
|---|---|
| **FltLP, FltHP, FltStatic, FltNord** | All four are on `to-test.md` as needing an ear and none is tuned. FltClassic is the only filter with captures. |
| **LfoShpA** | Rate Sub, Rate Lo and Rate Hi all MEASURED 2026-09-07 and now agree within 0.013% - Rate Lo's base was 0.43% low and is now derived as Rate Hi over 16. BPM and Clk still unverified (they need a master clock). LfoC untested. |
| **LevMult** | |
| **Mix4to1C, Mix4to1S** | SUMS - measured 2026-09-07, +6.02 dB per doubling of identical inputs, and the engine already matches. Its level-dial law and its -6/-12 dB Pad are still unmeasured. |
| **FxtoIn** | Pad MEASURED 2026-09-07: +6.02 / 0 / −6.02 / −12.06 dB, exactly its label, confirmed three ways (direct capture, the manual, and the compressor probe). Its absolute reference is still not separated from the rest of the chain. |
| **2-Out, 4-Out** | Pad MEASURED 2026-09-07 and it BOOSTS: two positions, 0 dB and +6.02 dB, confirmed by the manual ("on the Output modules between 0dB and +6dB"). The engine had it halving and was 12 dB out when engaged; fixed. Note the parameter is unclamped on the wire, so a backdoor sweep shows further +6 dB steps the dial cannot select. |
| **OscNoise** | MEASURED 2026-09-12: Width is param 6 on the instrument (tables swap 5/6); two cascaded band-passes (−10 dB span 2.2-2.3× the −3 dB width), Q = 3.34·e^(0.032·(127−W)) from 96-127, below 80 narrower than resolved; level ≈ −4.5 dB RMS re FS. In the engine (§8). |
| **EqPeak, Eq2Band, Eq3band** | MEASURED 2026-09-12: noise through all three, 43 settings. Gain = displayed dB; peak centre = Freq curve, damping 2(2^N−1)/√2^N; cuts mirror boosts (q/G, shelf corner fc/G or fc×G); Level = mixer Exp taper; Hi Freq settings 0/1 sound at 8k/6k (names say 6k/8k). Fits 0.5-0.7 dB mean. In the engine (§11). Capture (`eq.wav`, 66 MB) in the session scratchpad only, not kept. |
| **FltMulti** | MEASURED 2026-09-12: noise through LP, BP and HP at Freq 40/64/88, Res 0/64/110, both slopes, GComp on (54 responses). The DSP code's Chamberlin filter fits all of them to 0.5-0.6 dB mean with nothing fitted - cutoff `flt_cutoff_hz()`, Q = 0.5/d², GComp × d, 6 dB outputs as sums (BP = LP − HP). In the engine (§10). GComp off and the Freq/Pitch inputs not measured. Capture (`fltmulti.wav`, 75 MB, two channels) in the session scratchpad only, not kept. |
| **Noise** | MEASURED 2026-09-12: one-pole low-pass per Color (17 points, fit 0.5-0.7 dB), corner 18.3 kHz -> 129 Hz, level compensated (-7.7 .. -14.8 dB RMS re FS, after a 1.2 dB reference correction). In the engine as a table (kNoiseColour). Brightest settings peak one meter value higher in the engine. |
| **OscC, OscD** | CHECKED 2026-09-12 against OscA, all six waveforms: level within 0.02 dB, harmonics within 0.25 dB. In the engine (shared oscillator, waveform read as a mode). FM not modelled. |
| **Pan, X-Fade, Fade1-2, Fade2-1, MixStereo** | MEASURED 2026-09-12, 19 dial settings each, both Log and Lin: u = dial/128 (127 = 1); Lin 1−u / u; Log 1−u² / 1−(1−u)²; the faders STEER (x = 2u−1, centre silent); MixStereo levels = mix Exp curve, pan = Log law with u = dial/127 and coefficients scaled to (127/128)². All in the engine. Mod-input depth (×4 of the range at full attenuator) is from the DSP code, not yet measured. |
| **Mixer family** | ALL ELEVEN summing mixers checked 2026-09-12 (106 configurations, every one within 0.07 dB): Exp = cube + 1%, Lin = dial/128 (127 = 1), Pad 0/−6/−12, Chain at unity and unpadded, Mix2-1B Inv cancels. The engine implements every one. Captures in the session scratchpad only, not kept. |
| **Mix4to1C** | Pad MEASURED 2026-09-07: THREE positions, 0 / −6.01 / −12.04 dB (the DSP clamps at 2, so this one is real). Level taper measured as a CUBE, not the square the engine carried — 6 dB out by mid-dial. Both fixed. Lin mode was already right. **2026-09-12: a cube with 1% of the line mixed in** (218 steps, three waveforms, 0.01 dB RMS) — the dial readout's own curve; the engine now plays through it. Its METER (and 2-Out's) measured too: one value per octave of PEAK — see findings.md. |
| **Constant** | |
| **Shaper group** | Implemented 2026-09-07 — Clip, Overdrive, Saturate, ShpExp, WaveWrap, ShpStatic, Rect. **Rect is exact** (the manual states all four operations) and **ShpStatic's four buttons name their own curves**; the other five have the manual's SHAPE and a guessed DEPTH law. Every one is memoryless, so one slow ramp per mode captures the whole transfer function — the cheapest measurements left on this list. |

## Where to start, if the list is being worked through

The rig is now general — `PatchTestFiles/FxMeasure.pch2` measures any FX-area module by swapping one
module, and the engine can render the same patch for comparison. So the FX modules are cheap:

1. **StChorus** — already partly done, the stereo question is open, and it is the last unmeasured
   effect anyone will hear.
2. **Compress** — needs a different stimulus (a level ramp, not an impulse), so it wants thought.
3. **The shaper group** — and the ramp Compress wants is the SAME stimulus these need, so capture it
   once and run it through all eight. A memoryless module plotted output-against-input from a single
   full-scale ramp gives its entire transfer function with no windowing, no decay fit and no
   spectrum: five curves (Clip, Overdrive, Saturate, ShpExp, WaveWrap) times their modes, in one
   session.

Two that are worth doing for a different reason, being upstream of everything else:

4. **Pulse** — its width sets the excitation of every impulse response we take.
5. **LfoShpA** — its rate now sets the timebase of every capture.

The filters need a stimulus the rig does not have yet: a magnitude/phase response wants a sweep or
noise, not a click. See the MEASUREMENT PROGRAMME entry in `todo.md`.

## Trimmed captures (2026-09-12)

The 32-channel QU-24 takes in `G2Captures` were trimmed to the channels that carry the G2, verified
sample for sample; each file's INFO comment and `.json` sidecar (`"channels"`) record the ORIGINAL
desk channel numbers. Channel indices in older notes refer to the untrimmed files:

| Files | Kept (desk, 0-indexed) | Now channels |
|---|---|---|
| `hw_hall_t122_ir`, `hw_rooms_time0`, `hw_small_time0` | 4, 5 (dry, G2 outs 1/2), 18, 19 (wet, outs 3/4) | 0-1 dry, 2-3 wet - `analyse_ir.py`'s own four-channel order |
| `oscshpb/*` | 4, 5 (G2 outs 1/2) | 0-1 |

Dropped: the desk's main mix (inputs 29-30, ch 28/29) and bleed. New takes: `tools/capture --channels`.

