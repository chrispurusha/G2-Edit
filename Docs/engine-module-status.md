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
| **Oscillators** (§5-§8, §12, §21.3, §27) | Osc A, Osc B, Osc C, Osc D, Osc Shape A, Osc Shape B, Osc Dual, Osc Percussion, FM Operator, DX Router | Noise Osc, Noise, Drum Synth | Osc Phase Mod, Metallic Noise, Osc String, Driver, Resonator, Osc Master |
| **Filters** (§10, §13, §21-§23) | LP Filter, HP Filter, Nord Filter, Classic Filter, Static Filter, Eq 2-band, Eq 3-band, Eq Peak | Multi Filter, Comb Filter | Phase Filter, FltVoice, WahWah, Vocoder |
| **Envelopes** (§17) | Envelope ADSR | Envelope AHD, Envelope ADR, Envelop ADDSR, Envelope H, Envelope D, Envelope Multi, Envelope Mod AHD, Envelope Mod ADSR | - |
| **LFOs** (§28) | LFO A, LFO B, LFO C, LFO Shp A | - | Clock Generator |
| **Mixers** (§3) | Mixer 1-1 A, Mixer 1-1 S, Mixer 2-1 A, Mixer 4-1 A, Mixer 4-1 B, Mixer 4-1 C, Mixer 4-1 S, Mixer 2-1 B, Mixer 8-1 A, Mixer 8-1 B, MixFader, MixStereo, Fade 1-2, Fade 2-1, X-Fade, Pan | - | - |
| **Level** (§16, §29) | Constant, LevAdd, LevAmp, LevConv, LevMult | ModAmt | ConstSwM, ConstSwT, CompLev, CompSig, LevMod, MinMax, NoiseGate, EnvFollow, Red2Blue, Blue2Red |
| **Shapers** (§3.4) | Saturate, Clip, OverDrive, ShpExp, WaveWrap, ShpStatic, Rect | - | - |
| **Delays** (§24) | Delay A, Delay B | - | Delay Single A, Delay Single B, Delay Dual, Delay Quad, Delay Stereo, Delay Clock, Delay Eight, DlyShiftReg |
| **Effects** (§11, §19, §20, §25) | Compressor, Reverb | Chorus | Digitizer, FreqShift, Flanger, Phaser, PShift, Scratch |
| **In/Out** (§16) | 2 Outputs, 4 Outputs, 2 Inputs, FX Input, Keyboard, Monophonic Keyboard, Name Bar | - | 4 Inputs, Device, Status, Note Detector |
| **Switches** (§30) | Sw2-1, Sw8-1, ValSw2-1 | SwOnOffT | SwOnOffM, Sw2-1M, Sw4-1, Sw1-2, Sw1-2M, Sw1-4, Sw1-8, ValSw1-2, Mux8-1, Mux1-8, Mux8-1X, S&H, T&H, WindSw |
| **Logic** (§38) | Invert, Pulse, Gate, FlipFlop, ClkDiv | - | Delay, 8Counter, BinCounter, ADConv, DAConv |
| **Sequencers** | - | - | Sequencer Event, Sequencer Values, Sequencer Level, Sequencer Note, Sequencer Controlled |
| **Random** | - | - | Random A, Random B, Rnd Clock A, Rnd Clock B, Rnd Trig, Rnd Pattern |
| **Note** (§26) | - | Glide | Note Quantiser, Key Quantiser, Partial Quantiser, Note Scaler, Pitch Tracker, Zero Crossing Counter, Level Scaler |
| **MIDI** | - | - | CtrlSend, PCSend, NoteSend, CtrlRcv, NoteRcv, NoteZone, Automate |
| **Total 170** | **69** | **17** | **84** |

| Partial module | What is still open |
|---|---|
| Noise | a closer model is known but not adopted, pending a listening check |
| Noise Osc | Sine3/Sine4 level still open |
| Drum Synth | level balance between master, slave, noise and click (§39.3) |
| Comb Filter | not yet checked against the instrument's own part |
| Multi Filter | GComp not yet checked against the instrument's own part |
| Envelop ADDSR | KB gate and Reset not read (§17.9) |
| Envelope ADR | KB gate and Reset not read (§17.9) |
| Envelope AHD | KB gate and Reset not read (§17.9) |
| Envelope D | KB gate and Reset not read (§17.9) |
| Envelope H | KB gate and Reset not read (§17.9) |
| Envelope Mod ADSR | KB gate and Reset not read (§17.9) |
| Envelope Mod AHD | KB gate and Reset not read (§17.9) |
| Envelope Multi | rise to an intermediate level is a guess (§17.9) |
| ModAmt | Enable button unconfirmed (§29.4) |
| Chorus | a third chorus in one patch passes dry (pool of 2 lines) |
| SwOnOffT | untested on hardware (§30) |
| Glide | Lin and the Time table are the instrument's; the Log shape is ours (§36.1) |

## Notes

A patch of nothing but **Not implemented** modules reports "Nothing is patched into it". One that
mixes the two plays its modelled part, which is why an unfinished patch sounds thin rather than
broken - and why 02 Big Pad played for months while quietly missing its velocity-to-filter path.

`2 Inputs` is listed as Working but plays SILENCE - the engine has no audio input (§37). It is
modelled and not pruned, which is what stops a patch containing one reading as unsupported.

The palette offers 170 of the 210 types in `types.h`; the rest the G2 does not offer from its own
menus. `Operator` and `Name` sit outside `module_kind()` - an Operator plays only as part of the
DXRouter that owns it (§14), and a Name module is text.

## The two target patches

Neither is a factory patch - both are CT's own, and the copies in `PatchTestFiles/` are the only
reference to them.

- **02 Big Pad** (`PatchTestFiles/BigPad.pch2`) - complete since 2026-09-19; its last
  two, `ModAmt` and `SwOnOffT`, are both Partial.
- **01 Mini Emulator** (`PatchTestFiles/MiniEmulator.pch2`) - needs eight more:
  `MonoKey`, `Glide`, `LevConv`, `LevAdd`, `Sw2-1`, `Sw8-1`, `ValSw2-1`, `2-In`. Their laws are
  worked out in `mini-emulator-engine-plan.md`.
