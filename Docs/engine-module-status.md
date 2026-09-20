# Sound engine: module status

Written 2026-09-19. What the local sound engine does with each module type, by palette group, under
the names the palette itself shows.

The Plays and Silent lists are GENERATED - `tools/do-modulestatus` builds `tools/modulestatus`,
which asks `sound_engine_models_module()` about every type the palette offers and prints them by
group. **Regenerate rather than edit**: this drifted the moment eight modules were added by hand
(2026-09-19). Which of the playing ones are UNSETTLED is editorial and stays by hand, and those
names are subtracted from the Plays line above them.

| state | meaning |
|---|---|
| **Plays** | modelled, and its law checked against the instrument or its own parts |
| **Plays, unsettled** | audible, but something about its law is still a guess - each says what, and to-test.md carries the check |
| **Silent** | not modelled: it contributes nothing and is pruned from the chain |

A patch of nothing but **Silent** modules reports "Nothing is patched into it". One that mixes the
two plays its modelled part, which is why an unfinished patch sounds thin rather than broken - and
why 02 Big Pad played for months while quietly missing its velocity-to-filter path.

## Oscillators  (§5-§8, §12, §21.3, §27)

**Plays** (9): `Osc A`, `Osc B`, `Osc C`, `Osc D`, `Osc Shape A`, `Osc Shape B`, `Osc Dual`, `FM Operator`, `DX Router`

**Plays, unsettled** (3):

- `Noise` - a closer model is known but not adopted, pending a listening check
- `Noise Osc` - Sine3/Sine4 level still open
- `Drum Synth` - the level balance between master, slave, noise and click (§39.3)

**Silent** (7): `Osc Phase Mod`, `Metallic Noise`, `Osc Percussion`, `Osc String`, `Driver`, `Resonator`, `Osc Master`

## Filters  (§10, §13, §21-§23)

**Plays** (8): `LP Filter`, `HP Filter`, `Nord Filter`, `Classic Filter`, `Static Filter`, `Eq 2-band`, `Eq 3-band`, `Eq Peak`

**Plays, unsettled** (2):

- `Comb Filter` - not yet checked against the instrument's own part
- `Multi Filter` - GComp not yet checked against the instrument's own part

**Silent** (4): `Phase Filter`, `FltVoice`, `WahWah`, `Vocoder`

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

**Plays** (16): `Mixer 1-1 A`, `Mixer 1-1 S`, `Mixer 2-1 A`, `Mixer 4-1 A`, `Mixer 4-1 B`, `Mixer 4-1 C`, `Mixer 4-1 S`, `Mixer 2-1 B`, `Mixer 8-1 A`, `Mixer 8-1 B`, `MixFader`, `MixStereo`, `Fade 1-2`, `Fade 2-1`, `X-Fade`, `Pan`

## Level  (§16, §29)

**Plays** (5): `Constant`, `LevAdd`, `LevAmp`, `LevConv`, `LevMult`

**Plays, unsettled** (1):

- `ModAmt` - Enable button unconfirmed (§29.4)

**Silent** (10): `ConstSwM`, `ConstSwT`, `CompLev`, `CompSig`, `LevMod`, `MinMax`, `NoiseGate`, `EnvFollow`, `Red2Blue`, `Blue2Red`

## Shapers  (§3.4)

**Plays** (7): `Saturate`, `Clip`, `OverDrive`, `ShpExp`, `WaveWrap`, `ShpStatic`, `Rect`

## Delays  (§24)

**Plays** (2): `Delay A`, `Delay B`

**Silent** (8): `Delay Single A`, `Delay Single B`, `Delay Dual`, `Delay Quad`, `Delay Stereo`, `Delay Clock`, `Delay Eight`, `DlyShiftReg`

## Effects  (§11, §19, §20, §25)

**Plays** (2): `Compressor`, `Reverb`

**Plays, unsettled** (1):

- `Chorus` - a third chorus in one patch passes dry (pool of 2 lines)

**Silent** (6): `Digitizer`, `FreqShift`, `Flanger`, `Phaser`, `PShift`, `Scratch`

## In/Out  (§16)

**Plays** (7): `2 Outputs`, `4 Outputs`, `2 Inputs`, `FX Input`, `Keyboard`, `Monophonic Keyboard`, `Name Bar`

(`2 Inputs` plays as SILENCE - the engine has no audio input, §37. It is listed as playing because
it is modelled and not pruned, which is what stops a patch containing one reading as unsupported.)

**Silent** (4): `4 Inputs`, `Device`, `Status`, `Note Detector`

## Switches  (§30)

**Plays** (3): `Sw2-1`, `Sw8-1`, `ValSw2-1`

**Plays, unsettled** (1):

- `SwOnOffT` - untested on hardware (§30)

**Silent** (14): `SwOnOffM`, `Sw2-1M`, `Sw4-1`, `Sw1-2`, `Sw1-2M`, `Sw1-4`, `Sw1-8`, `ValSw1-2`, `Mux8-1`, `Mux1-8`, `Mux8-1X`, `S&H`, `T&H`, `WindSw`

## Logic

**Plays** (5): `Invert`, `Pulse`, `Gate`, `FlipFlop`, `ClkDiv`

**Silent** (5): `Delay`, `8Counter`, `BinCounter`, `ADConv`, `DAConv`

## Sequencers

**Silent** (5): `Sequencer Event`, `Sequencer Values`, `Sequencer Level`, `Sequencer Note`, `Sequencer Controlled`

## Random

**Silent** (6): `Random A`, `Random B`, `Rnd Clock A`, `Rnd Clock B`, `Rnd Trig`, `Rnd Pattern`

## Note  (§26)

**Plays, unsettled** (1):

- `Glide` - Lin and the Time table are the instrument's; the Log SHAPE is ours (§36.1)

**Silent** (7): `Note Quantiser`, `Key Quantiser`, `Partial Quantiser`, `Note Scaler`, `Pitch Tracker`, `Zero Crossing Counter`, `Level Scaler`

## MIDI

**Silent** (7): `CtrlSend`, `PCSend`, `NoteSend`, `CtrlRcv`, `NoteRcv`, `NoteZone`, `Automate`

## Totals

| state | count |
|---|---|
| Plays | 69 |
| Plays, unsettled | 17 |
| Silent | 84 |
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
