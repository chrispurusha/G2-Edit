# Sound engine: module status

What the local sound engine does with each module type the palette offers, by palette group, under
the names the palette itself shows. Section numbers are `sound-engine-reference.md`'s.

| state | meaning |
|---|---|
| **Working** | modelled, and its law checked against the instrument or the reference model |
| **Partial** | audible, but something about its law is still a guess - the second table says what, and to-test.md carries the check |
| **Not implemented** | not modelled: it contributes nothing and is pruned from the chain |

The tables are GENERATED - `tools/do-modulestatus` builds and runs `tools/modulestatus`, which asks
`sound_engine_models_module()` about every type the palette offers. **Regenerate rather than edit.**
Which modelled modules count as Partial is editorial and lives in the generator's `kPartial` list,
which fails the run if it names a module that is not offered or not modelled.

| Group | Working | Partial | Not implemented |
|---|---|---|---|
| **Oscillators** (§5-§8, §12, §21.3, §27, §51, §53, §66) | Osc A, Osc B, Osc C, Osc D, Osc Shape A, Osc Shape B, Osc Dual, Noise, Metallic Noise, Osc Percussion, Drum Synth, FM Operator, DX Router, Osc Master | Osc Phase Mod, Noise Osc | Osc String, Driver, Resonator |
| **Filters** (§10, §13, §21-§23, §56, §67, §69) | LP Filter, HP Filter, Nord Filter, Classic Filter, Phase Filter, Static Filter, WahWah, Eq 2-band, Eq 3-band, Eq Peak | Multi Filter, Comb Filter, FltVoice | Vocoder |
| **Envelopes** (§17) | Envelope ADSR | Envelope AHD, Envelope ADR, Envelop ADDSR, Envelope H, Envelope D, Envelope Multi, Envelope Mod AHD, Envelope Mod ADSR | - |
| **LFOs** (§28, §42, §50, §54) | LFO A, LFO B, LFO C, LFO Shp A | Clock Generator | - |
| **Mixers** (§3) | Mixer 1-1 A, Mixer 1-1 S, Mixer 2-1 A, Mixer 4-1 A, Mixer 4-1 B, Mixer 4-1 C, Mixer 4-1 S, Mixer 2-1 B, Mixer 8-1 A, Mixer 8-1 B, MixFader, MixStereo, Fade 1-2, Fade 2-1, X-Fade, Pan | - | - |
| **Level** (§16, §29, §43, §44, §48, §68, §69) | Constant, ConstSwM, ConstSwT, CompLev, CompSig, LevAdd, LevAmp, LevConv, LevMod, LevMult, MinMax, EnvFollow, Red2Blue, Blue2Red | ModAmt | NoiseGate |
| **Shapers** (§3.4) | Saturate, Clip, OverDrive, ShpExp, WaveWrap, ShpStatic, Rect | - | - |
| **Delays** (§24, §52, §65, §69) | Delay A, Delay B, Delay Stereo, Delay Clock, DlyShiftReg | Delay Single A, Delay Single B | Delay Dual, Delay Quad, Delay Eight |
| **Effects** (§11, §19, §20, §25, §55, §57, §69) | Compressor, Digitizer, Phaser, Reverb | FreqShift, Chorus | Flanger, PShift, Scratch |
| **In/Out** (§16, §69) | 2 Outputs, 4 Outputs, 2 Inputs, FX Input, Keyboard, Monophonic Keyboard, Name Bar | 4 Inputs, Note Detector | Device, Status |
| **Switches** (§30, §45, §68) | SwOnOffM, Sw2-1, Sw2-1M, Sw4-1, Sw8-1, Sw1-2, Sw1-2M, Sw1-4, Sw1-8, Mux8-1, Mux1-8, S&H, T&H, WindSw | SwOnOffT, ValSw2-1, ValSw1-2 | Mux8-1X |
| **Logic** (§38, §46, §68) | Invert, Pulse, Gate, FlipFlop, ClkDiv, 8Counter, BinCounter, ADConv, DAConv | Delay | - |
| **Sequencers** (§58, §69) | Sequencer Level | Sequencer Event, Sequencer Values, Sequencer Note | Sequencer Controlled |
| **Random** (§47, §64, §69) | Random A, Random B, Rnd Clock A, Rnd Trig | - | Rnd Clock B, Rnd Pattern |
| **Note** (§26, §41, §49, §69) | Note Quantiser, Key Quantiser, Partial Quantiser, Note Scaler | Glide | Pitch Tracker, Zero Crossing Counter, Level Scaler |
| **MIDI** | - | NoteSend | CtrlSend, PCSend, CtrlRcv, NoteRcv, NoteZone, Automate |
| **Total 170** | **114** | **30** | **26** |

| Partial module | What is still open |
|---|---|
| ValSw2-1 | switches at the threshold (manual); its part tests equality within 1/2 unit (§68.2) |
| ValSw1-2 | switches at the threshold (manual); its part tests equality within 1/2 unit (§68.2) |
| 4 Inputs | silent (the jacks); a Bus source is not bridged (§69.12) |
| Note Detector | release velocity is not kept, so RVel reads 0 (§69.11) |
| Noise Osc | Q and level measured, not yet read from the reference model (§8) |
| Comb Filter | not yet checked against the reference model |
| Multi Filter | GComp not yet checked against the reference model |
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
| Delay | the time Mod input is not read (§46) |
| Osc Phase Mod | Tri's corner correction and the Sync input not modelled (§53) |
| FreqShift | Sub range: the module's word and the readout disagree 12x (§57) |
| FltVoice | the fine-pitch table offset is read as none, not decoded (§56) |
| Sequencer Note | the record inputs are not modelled; steps at 96 kHz whatever the clock's rate (§58) |
| Sequencer Event | steps at 96 kHz whatever the clock's rate (§58) |
| Sequencer Values | steps at 96 kHz whatever the clock's rate (§58) |
| Clock Generator | Master follows a fixed 120 BPM, not the global clock (§59) |
| NoteSend | plays this slot only; notes to other slots and MIDI are dropped (§62) |
| Delay Single A | the tap's interpolator is ours, not the instrument's table (§52) |
| Delay Single B | the tap's interpolator is ours, not the instrument's table (§52) |

## What is left (2026-09-27, 26 modules)

The Not implemented column by what each module would take. The groups are editorial and are not
regenerated; update them by hand as modules land.

| Kind | Modules |
|---|---|
| **Makes no sound, or lives on the MIDI side** (8) | Device, Status, CtrlSend, PCSend, Automate (nothing to render); CtrlRcv, NoteRcv, NoteZone (incoming MIDI, which the engine could supply from its own note stream, as NoteDet does, §69.11) |
| **the reference model to port, several per module** (12) | Rnd Clock B, Rnd Pattern (parts that share registers, §69 - need the whole module in the harness), NoiseGate (a follower, the simple envelope's parts and a VCA), Delay Dual / Quad / Eight (the delay-base and tap stages), PShift, Scratch (the pitch-shift tap and crossfade parts), Flanger, Osc String (Karplus), Pitch Tracker, Vocoder |

| **Needs the reference model run, not read** (1) | Mux8-1X (§68.3) |
| **No part in the reference** (4) | Driver, Resonator, Zero Crossing Counter, Level Scaler - later additions; they would need captures |

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
