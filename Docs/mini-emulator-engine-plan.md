# 01 Mini Emulator in the sound engine

Living design note, started 2026-09-13. **BUILT 2026-09-19 - the patch plays, and needs an ear
against the G2 (to-test.md).** What was missing is below as it was written, since the laws and the
routing survey are still the reference for checking it; the module table's eight types are all in
(reference §§31-37), and the thing that actually kept the voice area silent was not on the list at
all - an envelope's In/Gate/AM jacks were found by position, so the chain stopped at every ModADSR
(§17.4a). The engine builds 80 of its modules.

**01 Mini Emulator** is not a factory patch - it is one of CT's own, and
`PatchTestFiles/MiniEmulator.pch2` is the only copy of it here. What follows is what the engine
lacked and the order it was added in; the survey of how the patch is built is what to check it
against.

That copy was saved from Slot A. Its name modules read
"MiniMoogy G2", "Patched by Varice J. Mire", "2005". Voice mode Mono.

## What the engine did with it before this (kept for the diagnosis)

It built the FX area only (Fx-In, two DelayB, Reverb, Mix4-1S, 2-Out, and a Constant for the delay
feedback), all of which it already models. Nothing from the Voice area arrives: every path from the
Voice area's Output back to a source runs through a module type the engine does not have, so the
status reads "Nothing is patched into it".

85 Voice-area modules are reachable from its Output, by type:

| Type | Count | | Type | Count |
|---|---|---|---|---|
| Constant | 19 | | ModADSR | 2 |
| Mix1-1A | 9 | | Glide | 2 |
| LevConv | 5 | | LevAmp | 2 |
| Noise | 4 (Sw2-1s named so) | | ShpExp | 2 |
| Sw2-1 | 4 | | SwOnOffT | 2 |
| Sw8-1 | 3 | | Mix2-1A | 2 |
| X-Fade | 3 | | OscC, OscD | 2 each |
| OscA | 3 | | LevAdd | 2 |
| ValSw2-1 | 3 | | LevMult | 2 |
| Mix2-1B | 3 | | FltClassic, MonoKey, 2-Out, Saturate, FltHP, 2-In, Pulse, MixFader, LfoShpA | 1 each |

(the "Noise" row is Noise plus modules named Noise; the Name modules are text and play no part.)

## How the patch is built

- **Pitch is patched, not hardwired.** All three OscA have KBT off. MonoKey's Pitch goes through a Glide
  (Log, Time 28) and a Mix1-1A ("Osc Mod", which also takes the modulation mix) into a Mix2-1B per
  oscillator that adds a "-7/+7" and a "Fine" Constant, and on into each oscillator's Pitch input.
- **Range switches are octave Constants.** Each oscillator's Range is a Sw8-1 choosing between
  Constants at 0, 28, 40, 52, 64, 76, 88, 100 - steps of 12 units, i.e. octaves, around 64 - so the
  Constants are BIPOLAR. Osc 1 takes its Range on PitchVar at a Pitch Mod of 127.
- **Envelopes are single-trigger.** Both ModADSR have KB off and take their Gate from MonoKey's Gate,
  which stays high until the last key comes up. Decay and Release are set through the Decay/Release mod
  inputs, from "Decay" Constants and ValSw2-1s: a SwOnOffT ("Decay") switches Release between its own
  Constant and the Decay one, the Minimoog's decay switch.
- **Audio.** Osc 1-3 (saw) -> LevConv (Bip to Bip: unity) -> TriSaw Sw2-1s (on In 1, the plain
  oscillator; In 2 is an OscC/OscD alternative, idle) -> MixFader (Osc 1 and 2 at 127, Osc 3 and Noise
  off, External 2-In off) -> FltHP -> LevAmp -> Saturate ("Filter Overdrive") -> FltClassic -> LevAmp
  ("VCA Level") -> Amp Env's VCA -> Output, then the FX area.
- **Filter.** FltClassic Freq 64, Res 40, 24 dB. Its Env input (the attenuated one) takes the Filter Env
  through a Mix1-1A ("Env Amt"); its second control input - the direct Pitch input - takes a Mix2-1A
  ("Filter KBT") of MonoKey's Pitch and a Glide-smoothed "Cutoff Freq" Constant.
- **Modulation.** An LfoShpA through a Sw2-1 ("LFO/Osc 3") and an X-Fade ("Mod.Mix"), with noise, into
  the "Osc Mod" mixer - the mod wheel's vibrato. One Sw2-1's Ctrl output drives a ValSw2-1.

## Module types to add

Laws from the manual unless marked; each needs confirming on the hardware once built.

| Module | Inputs -> outputs | Behaviour |
|---|---|---|
| MonoKey | - -> Pitch, Gate, Vel | Last/Lo/Hi key, shared by every voice. Pitch is the full keyboard pitch (note + bend + glide), E4 = 0 units, 1 unit a semitone. Gate is high from the first key down until the LAST key comes up ("single-trigger"). Vel of that key. In a Mono patch Last is the voice's note; Lo and Hi need the keys held, which the engine now keeps (reference §15.1) |
| Glide | In, Glide On -> Out | A slew for control signals. Log: the same time whatever the jump, 0.2 ms to 22.4 s; Lin: constant rate, 0.2 ms/oct to 23.5 s/oct. Glides while its button is on or Glide On is high; otherwise passes the input. The Time map fits ln(t) as a quartic in the dial to a median 2 % |
| ModADSR | Gate, A/D/S/R mod, In, AM -> Env, Out | Linear attack, exponential decay and release, fixed. KB gates it from the keyboard as well as the Gate input. Out is In through the envelope's VCA; AM scales the envelope. Output Type: the six Pos..BipInv forms. MOD LAW, WORKING ASSUMPTION: each amount moves its dial linearly, a full amount and a 64-unit input moving A, D or R the whole 127 steps and S its whole range |
| LevConv | In -> Out | Reads the input as Bip, Pos or Neg and writes it as Pos, PosInv, Neg, NegInv, Bip or BipInv - a straight-line map between the ranges |
| LevAdd | In -> Out | Adds its dial: Bipolar (dial - 64) units, Unipolar dial / 2 units, 127 reading 64 in both |
| Sw2-1, Sw8-1 | In1..n -> Out, Ctrl | Out is the selected input. Ctrl is 0 units for In 1, 4 for In 2, ... 28 for In 8 (manual, Common Switch parameters) |
| SwOnOffT | In -> Out, Ctrl | Closed: Out is In, or 64 units with nothing patched; open: 0 |
| ValSw2-1 | In1, In2 (On), Ctrl -> Out | In2 when Ctrl reaches the limit (0-64 units), else In1 |
| 2-In | - -> L, R | Audio In or a bus. The engine has no audio input: silence |

## Engine limits the patch runs into

- **Node budget - DONE 2026-09-19.** `MAX_ENGINE_NODES` is now 128 (was 28); the patch needs about
  92 (85 Voice, 7 FX). The chorus moved onto a pool of 2 lines as the delays, reverbs and combs
  already had, which paid for nearly all of it: the plug-in grew 26 MB, not 142, and the application
  shrank. `sound_engine_render()`'s snapshot moved off the audio thread's stack; the other two
  snapshot copies were already `_Thread_local`. See findings 2026-09-19.
- **FltClassic's second control input** - the direct Pitch input, 1 unit a semitone on the Freq
  dial's scale - is not modelled; the engine reads only the audio and the Env input.
- **Pitch-input scale and the Constant.** Both were wrong in the engine and matter to every pitch path
  in this patch; see findings.md, 2026-09-13.
- **Sustain** and **velocity** are not modelled (reference §15.5); the patch uses neither directly.

## Order of work

1. The limits above: the node budget and its three prerequisites, then FltClassic's Pitch input.
2. The routing and level modules - Sw2-1, Sw8-1, SwOnOffT, ValSw2-1, LevConv, LevAdd, 2-In - which can be
   checked offline against their laws.
3. MonoKey and Glide, so the pitch path is complete; check the oscillators land on the played note.
4. ModADSR, measured against the G2 in Slot A: the stage times with the mod inputs, and the mod law.
5. The whole patch against a capture of the G2 playing it.
