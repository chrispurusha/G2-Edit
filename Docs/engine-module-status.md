# Sound engine: module status

Written 2026-09-19. What the local sound engine does with each module type, by palette group, under
the names the palette itself shows.

The lists come from `module_kind()` in `soundEngine.c` against the palette table in
`moduleResources.h`; the notes against each unsettled one are by hand. **Regenerate the lists from
the code rather than editing them**, or this drifts the moment a module is added.

| state | meaning |
|---|---|
| **Plays** | modelled, and its law checked against the instrument or its own parts |
| **Plays, unsettled** | audible, but something about its law is still a guess - each says what, and to-test.md carries the check |
| **Silent** | not modelled: it contributes nothing and is pruned from the chain |

A patch of nothing but **Silent** modules reports "Nothing is patched into it". One that mixes the
two plays its modelled part, which is why an unfinished patch sounds thin rather than broken - and
why 02 Big Pad played for months while quietly missing its velocity-to-filter path.

## Oscillators  (§5-§8, §12, §21.3, §27)

**Plays** (8): `DX Router`, `Osc A`, `Osc B`, `Osc C`, `Osc D`, `Osc Dual`, `Osc Shape A`, `Osc Shape B`

**Plays, unsettled** (2):

- `Noise` - a closer model is known but not adopted, pending a listening check
- `Noise Osc` - Sine3/Sine4 level still open

**Silent** (9): `Driver`, `Drum Synth`, `FM Operator`, `Metallic Noise`, `Osc Master`, `Osc Percussion`, `Osc Phase Mod`, `Osc String`, `Resonator`

## Filters  (§10, §13, §21-§23)

**Plays** (8): `Classic Filter`, `Eq 2-band`, `Eq 3-band`, `Eq Peak`, `HP Filter`, `LP Filter`, `Nord Filter`, `Static Filter`

**Plays, unsettled** (2):

- `Comb Filter` - not yet checked against the instrument's own part
- `Multi Filter` - GComp not yet checked against the instrument's own part

**Silent** (4): `FltVoice`, `Phase Filter`, `Vocoder`, `WahWah`

## Envelopes  (§17)

**Plays** (1): `Envelope ADSR`

**Plays, unsettled** (8):

- `Envelop ADDSR` - KB gate and Reset not read (§17.9)
- `Envelope ADR` - KB gate and Reset not read (§17.9)
- `Envelope AHD` - KB gate and Reset not read (§17.9)
- `Envelope D` - KB gate and Reset not read (§17.9)
- `Envelope H` - KB gate and Reset not read (§17.9)
- `Envelope Mod ADSR` - KB gate and Reset not read (§17.9)
- `Envelope Mod AHD` - KB gate and Reset not read (§17.9)
- `Envelope Multi` - rise to an intermediate level is a guess (§17.9)

## LFOs  (§28)

**Plays** (4): `LFO A`, `LFO B`, `LFO C`, `LFO Shp A`

**Silent** (1): `Clock Generator`

## Mixers  (§3)

**Plays** (16): `Fade 1-2`, `Fade 2-1`, `MixFader`, `MixStereo`, `Mixer 1-1 A`, `Mixer 1-1 S`, `Mixer 2-1 A`, `Mixer 2-1 B`, `Mixer 4-1 A`, `Mixer 4-1 B`, `Mixer 4-1 C`, `Mixer 4-1 S`, `Mixer 8-1 A`, `Mixer 8-1 B`, `Pan`, `X-Fade`

## Level  (§16, §29)

**Plays** (5): `Constant`, `LevAdd`, `LevAmp`, `LevConv`, `LevMult`

**Plays, unsettled** (1):

- `ModAmt` - Enable button unconfirmed (§29.4)

**Silent** (10): `Blue2Red`, `CompLev`, `CompSig`, `ConstSwM`, `ConstSwT`, `EnvFollow`, `LevMod`, `MinMax`, `NoiseGate`, `Red2Blue`

## Shapers  (§3.4)

**Plays** (7): `Clip`, `OverDrive`, `Rect`, `Saturate`, `ShpExp`, `ShpStatic`, `WaveWrap`

## Delays  (§24)

**Plays** (2): `Delay A`, `Delay B`

**Silent** (8): `Delay Clock`, `Delay Dual`, `Delay Eight`, `Delay Quad`, `Delay Single A`, `Delay Single B`, `Delay Stereo`, `DlyShiftReg`

## Effects  (§11, §19, §20, §25)

**Plays** (2): `Compressor`, `Reverb`

**Plays, unsettled** (1):

- `Chorus` - a third chorus in one patch passes dry (pool of 2 lines)

**Silent** (6): `Digitizer`, `Flanger`, `FreqShift`, `PShift`, `Phaser`, `Scratch`

## In/Out  (§16)

**Plays** (6): `2 Inputs`, `2 Outputs`, `4 Outputs`, `FX Input`, `Keyboard`, `Monophonic Keyboard`

**Silent** (5): `4 Inputs`, `Device`, `Name Bar`, `Note Detector`, `Status`

(`2 Inputs` plays as SILENCE - the engine has no audio input, §37. It is listed as playing because
it is modelled and not pruned, which is what stops a patch containing one reading as unsupported.)

## Switches  (§30)

**Plays, unsettled** (1):

- `SwOnOffT` - untested on hardware (§30)

**Plays** (3): `Sw2-1`, `Sw8-1`, `ValSw2-1`

**Silent** (14): `Mux1-8`, `Mux8-1`, `Mux8-1X`, `S&H`, `Sw1-2`, `Sw1-2M`, `Sw1-4`, `Sw1-8`, `Sw2-1M`, `Sw4-1`, `SwOnOffM`, `T&H`, `ValSw1-2`, `WindSw`

## Logic

**Plays** (5): `ClkDiv`, `FlipFlop`, `Gate`, `Invert`, `Pulse`

**Silent** (5): `8Counter`, `ADConv`, `BinCounter`, `DAConv`, `Delay`

## Sequencers

**Silent** (5): `Sequencer Controlled`, `Sequencer Event`, `Sequencer Level`, `Sequencer Note`, `Sequencer Values`

## Random

**Silent** (6): `Random A`, `Random B`, `Rnd Clock A`, `Rnd Clock B`, `Rnd Pattern`, `Rnd Trig`

## Note  (§26)

**Plays, unsettled** (1):

- `Glide` - Lin and the Time table are the instrument's; the Log SHAPE is ours (§36.1)

**Silent** (7): `Key Quantiser`, `Level Scaler`, `Note Quantiser`, `Note Scaler`, `Partial Quantiser`, `Pitch Tracker`, `Zero Crossing Counter`

## MIDI

**Silent** (7): `Automate`, `CtrlRcv`, `CtrlSend`, `NoteRcv`, `NoteSend`, `NoteZone`, `PCSend`

## Totals

| state | count |
|---|---|
| Plays | 56 |
| Plays, unsettled | 15 |
| Silent | 99 |
| **palette total** | **170** |

The palette offers 170 of the 210 types in `types.h`; the rest the G2 does not offer from its own
menus. `Operator` and `Name` sit outside `module_kind()` - an Operator plays only as part of the
DXRouter that owns it (§14), and a Name module is text.

## The two target patches

Neither is a factory patch - both are CT's own, and the copies in `PatchTestFiles/` are the only
reference to them.

- **02 Big Pad** (`PatchTestFiles/BigPad.pch2`) - complete since 2026-09-19; its last
  two, `ModAmt` and `SwOnOffT`, are both in the unsettled list.
- **01 Mini Emulator** (`PatchTestFiles/MiniEmulator.pch2`) - needs eight more:
  `MonoKey`, `Glide`, `LevConv`, `LevAdd`, `Sw2-1`, `Sw8-1`, `ValSw2-1`, `2-In`. Their laws are
  worked out in `mini-emulator-engine-plan.md`.
