# Sound engine: module status

What the local sound engine does with each module type the palette offers, by palette group, under
the names the palette itself shows. Section numbers are `sound-engine-reference.md`'s.

| state | meaning |
|---|---|
| **Working** | modelled, and its law checked against the instrument or its own parts |
| **Partial** | audible, but something about its law is still a guess - the second table says what, and to-test.md carries the check |
| **Not implemented** | not modelled: it contributes nothing and is pruned from the chain |

The tables are GENERATED - `tools/do-modulestatus` builds and runs `tools/modulestatus`, which asks
`sound_engine_models_module()` about every type the palette offers. **Regenerate rather than edit.**
Which modelled modules count as Partial is editorial and lives in the generator's `kPartial` list,
which fails the run if it names a module that is not offered or not modelled.

| Group | Working | Partial | Not implemented |
|---|---|---|---|
| **Oscillators** (§5-§8, §12, §21.3, §27, §51, §53, §66, §70) | Osc A, Osc B, Osc C, Osc D, Osc Phase Mod, Osc Shape A, Osc Shape B, Osc Dual, Noise Osc, Noise, Metallic Noise, Osc Percussion, Drum Synth, FM Operator, DX Router, Osc Master | Osc String, Driver, Resonator | - |
| **Filters** (§10, §13, §21-§23, §56, §67, §69, §70) | LP Filter, HP Filter, Nord Filter, Classic Filter, Multi Filter, Phase Filter, Comb Filter, Static Filter, FltVoice, WahWah, Eq 2-band, Eq 3-band, Eq Peak | Vocoder | - |
| **Envelopes** (§17) | Envelope ADSR, Envelope AHD, Envelope ADR, Envelop ADDSR, Envelope H, Envelope D, Envelope Multi, Envelope Mod AHD, Envelope Mod ADSR | - | - |
| **LFOs** (§28, §42, §50, §54) | LFO A, LFO B, LFO C, LFO Shp A, Clock Generator | - | - |
| **Mixers** (§3) | Mixer 1-1 A, Mixer 1-1 S, Mixer 2-1 A, Mixer 4-1 A, Mixer 4-1 B, Mixer 4-1 C, Mixer 4-1 S, Mixer 2-1 B, Mixer 8-1 A, Mixer 8-1 B, MixFader, MixStereo, Fade 1-2, Fade 2-1, X-Fade, Pan | - | - |
| **Level** (§16, §29, §43, §44, §48, §68, §69, §70) | Constant, ConstSwM, ConstSwT, CompLev, CompSig, LevAdd, LevAmp, LevConv, LevMod, LevMult, MinMax, ModAmt, NoiseGate, EnvFollow, Red2Blue, Blue2Red | - | - |
| **Shapers** (§3.4) | Saturate, Clip, OverDrive, ShpExp, WaveWrap, ShpStatic, Rect | - | - |
| **Delays** (§24, §52, §65, §69, §70) | Delay Single A, Delay Single B, Delay Dual, Delay Quad, Delay A, Delay B, Delay Stereo, Delay Clock, Delay Eight, DlyShiftReg | - | - |
| **Effects** (§11, §19, §20, §25, §55, §57, §69, §70) | Compressor, Digitizer, FreqShift, Phaser, Reverb | Flanger, Chorus, PShift, Scratch | - |
| **In/Out** (§16, §69, §70) | 2 Outputs, 4 Outputs, 2 Inputs, FX Input, Keyboard, Monophonic Keyboard, Status, Note Detector, Name Bar | 4 Inputs, Device | - |
| **Switches** (§30, §45, §68, §70) | SwOnOffM, SwOnOffT, Sw2-1, Sw2-1M, Sw4-1, Sw8-1, Sw1-2, Sw1-2M, Sw1-4, Sw1-8, ValSw2-1, ValSw1-2, Mux8-1, Mux1-8, Mux8-1X, S&H, T&H, WindSw | - | - |
| **Logic** (§38, §46, §68) | Invert, Pulse, Delay, Gate, FlipFlop, ClkDiv, 8Counter, BinCounter, ADConv, DAConv | - | - |
| **Sequencers** (§58, §69, §70) | Sequencer Event, Sequencer Values, Sequencer Level, Sequencer Note, Sequencer Controlled | - | - |
| **Random** (§47, §64, §69, §70) | Random A, Random B, Rnd Clock A, Rnd Clock B, Rnd Trig, Rnd Pattern | - | - |
| **Note** (§26, §41, §49, §69, §70) | Note Quantiser, Key Quantiser, Partial Quantiser, Note Scaler, Glide, Zero Crossing Counter | Pitch Tracker, Level Scaler | - |
| **MIDI** (§70) | CtrlSend, PCSend, NoteZone, Automate | NoteSend, CtrlRcv, NoteRcv | - |
| **Total 170** | **155** | **15** | **0** |

| Partial module | What is still open |
|---|---|
| 4 Inputs | silent (the jacks); a Bus source is not bridged (§69.12) |
| Flanger | basic: a swept delay from the manual (§70.2) |
| PShift | basic: two crossfaded taps (§70.3) |
| Scratch | basic: two crossfaded taps, ratio law guessed (§70.3) |
| Osc String | basic: a tuned loop; decay and damp laws guessed (§70.4) |
| Resonator | basic: OscString's loop; Alg and inputs guessed (§70.4) |
| Driver | a guess: not in the manual in hand (§70.5) |
| Pitch Tracker | counter and E2 reference are the instrument's; its detector (followers, filters, flip-flop) is not (§70.7) |
| Vocoder | basic: 16 band-passes, band centres guessed (§70.8) |
| Level Scaler | basic: dB per octave from the manual (§70.12) |
| Device | global wheel 2 reads 0 (§70.13) |
| CtrlRcv | no MIDI CC reaches the engine: outputs 0 (§70.13) |
| NoteRcv | NoteDet whatever the channel (§70.13) |
| Chorus | a third chorus in one patch passes dry (pool of 2 lines) |
| NoteSend | plays this slot only; notes to other slots and MIDI are dropped (§62) |

## What is left (2026-09-27): refinement, not coverage

Every module type the palette offers is now modelled at least at a basic level (§70, CT: "as many modules
at a basic level, then iteratively improve"). The Partial table above is the refinement queue. The groups
below say what refining each will take; update them by hand as modules move to Working.

| Kind | Modules |
|---|---|
| **Basic versions to replace with the instrument's parts** | Delay Dual / Quad / Eight (delay-base and tap parts), PShift, Scratch (pitch-shift parts), Flanger, Osc String (Karplus parts), Pitch Tracker, Vocoder |
| **Basic versions with no part in the reference** | Driver, Resonator, Zero Crossing Counter, Level Scaler - need captures from the G2 |
| **Limited by what reaches the engine** | CtrlRcv (no MIDI CC stream), NoteRcv (no channels), Device (global wheel 2), 4 Inputs (Bus not bridged) |
| **Nothing to render** | CtrlSend, PCSend, Automate, NoteZone (counted Working) |

**Moved to Working on 2026-09-27 (end of session):** the seven envelopes other than Multi. Each now reads
its own KB and Reset, and EnvD and EnvH have no KB at all (§17.4). Also improved without a change of
status: Nord Filter's FM lin, Res and Pitch inputs (§23.5), the LFOs' RndSt and Rnd (§28.3), and the
outputs' AC coupling (notes §198). Still open from that session: 14 CS80project72 lacks the G2's 527 Hz
partial (todo.md).

## Notes

A patch of nothing but **Not implemented** modules reports "Nothing is patched into it". One that
mixes the two plays its modelled part, which is why an unfinished patch sounds thin rather than
broken - and why 02 Big Pad played for months while quietly missing its velocity-to-filter path.

`2 Inputs` is listed as Working but plays SILENCE - the engine has no audio input (§37). It is
modelled and not pruned, which is what stops a patch containing one reading as unsupported.

The palette offers 170 of the 210 types in `types.h`; the rest the G2 does not offer from its own
menus. `Operator` and `Name` sit outside `module_kind()` - an Operator plays only as part of the
DXRouter that owns it (§14), and a Name module is text.
