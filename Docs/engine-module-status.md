# Sound engine: module status

What the local sound engine does with each module type the palette offers, by palette group, under
the names the palette itself shows - and, for each, what it has been checked against. Section numbers
are `sound-engine-reference.md`'s, where each module's model is described.

| state | meaning |
|---|---|
| **Confirmed on the G2** | compared with a G2 (a capture, its meters, or a stage patch played against it) and it agrees |
| **Modelled** | a full model in the reference, not yet compared with a G2 |
| **Approximate** | follows the manual's description; a full model is still to come |
| **Partial** | audible, but something about it is known to be missing - the Open column says what |
| **No sound** | nothing to render (a name, or a module whose output leaves by MIDI) |
| **Not implemented** | not modelled: it contributes nothing and is pruned from the chain |

Modelled is not the same as checked. A model that has never been put beside a G2 has not been
checked, however complete it looks; the comparison is what found the DrumSynth click, the OscDual
modulation depth and the FltMulti FreqM.

The tables are GENERATED - `tools/do-modulestatus` builds and runs `tools/modulestatus`, which asks
`sound_engine_models_module()` about every type the palette offers and takes each modelled module's
evidence from its `kEvidence` table. **Regenerate rather than edit**: change a module's row in
`kEvidence` when its evidence changes. The run fails if a modelled module has no row, or a row names a
module that is not offered or not modelled.

| Group | Confirmed on the G2 | Modelled | Approximate | Partial | No sound | Not implemented |
|---|---|---|---|---|---|---|
| **Oscillators** | Osc A, Osc B, Osc C, Osc D, Osc Shape B, Osc Dual, Noise Osc, Noise, Osc Percussion, Drum Synth, FM Operator, DX Router | Osc Phase Mod, Osc Shape A, Metallic Noise, Osc String, Driver, Resonator, Osc Master | - | - | - | - |
| **Filters** | Multi Filter, Comb Filter, Eq 2-band, Eq Peak | LP Filter, HP Filter, Nord Filter, Classic Filter, Phase Filter, Static Filter, FltVoice, WahWah, Vocoder | - | Eq 3-band | - | - |
| **Envelopes** | Envelope ADSR, Envelope AHD, Envelope H, Envelope Mod AHD, Envelope Mod ADSR | Envelope ADR, Envelop ADDSR, Envelope D, Envelope Multi | - | - | - | - |
| **LFOs** | LFO A, LFO Shp A, Clock Generator | LFO B, LFO C | - | - | - | - |
| **Mixers** | Mixer 1-1 A, Mixer 1-1 S, Mixer 2-1 A, Mixer 4-1 A, Mixer 4-1 B, Mixer 4-1 C, Mixer 4-1 S, Mixer 2-1 B, Mixer 8-1 A, Mixer 8-1 B, MixFader, MixStereo, Fade 1-2, Fade 2-1, X-Fade, Pan | - | - | - | - | - |
| **Level** | Constant, LevAmp | ConstSwM, ConstSwT, CompLev, CompSig, LevAdd, LevConv, LevMod, MinMax, ModAmt, NoiseGate, EnvFollow, Red2Blue, Blue2Red | LevMult | - | - | - |
| **Shapers** | OverDrive | - | Saturate, Clip, ShpExp, WaveWrap, ShpStatic, Rect | - | - | - |
| **Delays** | Delay A, Delay B | Delay Single A, Delay Single B, Delay Stereo, Delay Clock, DlyShiftReg | Delay Dual, Delay Quad, Delay Eight | - | - | - |
| **Effects** | Chorus, Reverb | Compressor, Digitizer, FreqShift, Flanger, Phaser, PShift, Scratch | - | - | - | - |
| **In/Out** | 2 Outputs, 4 Outputs, 2 Inputs, FX Input, Keyboard, Monophonic Keyboard, Status | 4 Inputs, Note Detector | Device | - | Name Bar | - |
| **Switches** | - | SwOnOffM, SwOnOffT, Sw2-1, Sw2-1M, Sw4-1, Sw8-1, Sw1-2, Sw1-2M, Sw1-4, Sw1-8, ValSw2-1, ValSw1-2, Mux8-1, Mux1-8, Mux8-1X, S&H, T&H, WindSw | - | - | - | - |
| **Logic** | Pulse, ClkDiv | Invert, Delay, Gate, 8Counter, BinCounter, ADConv, DAConv | FlipFlop | - | - | - |
| **Sequencers** | Sequencer Event, Sequencer Values, Sequencer Level, Sequencer Note | Sequencer Controlled | - | - | - | - |
| **Random** | - | Random A, Random B, Rnd Clock A, Rnd Clock B, Rnd Trig, Rnd Pattern | - | - | - | - |
| **Note** | - | Note Quantiser, Key Quantiser, Partial Quantiser, Note Scaler, Glide, Pitch Tracker, Zero Crossing Counter, Level Scaler | - | - | - | - |
| **MIDI** | CtrlSend | CtrlRcv, NoteRcv | - | NoteSend | PCSend, NoteZone, Automate | - |
| **Total 170** | **61** | **91** | **12** | **2** | **4** | **0** |

### Oscillators

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Osc A | Confirmed on the G2 | §6.3 | captures (G2Captures/osca); OscC/OscD level against it 09-12 | - |
| Osc B | Confirmed on the G2 | §6.3, §6.6-§6.8 | 5 waves x 8 pitches, harmonics within 0.1 dB (09-17); DualSaw below zero 0.1 dB (10-08) | FM and Sync not compared on the G2; Shape 127's one-sample click not modelled |
| Osc C | Confirmed on the G2 | §6.3 | Saw, Sqr50/25/10, Tri at 440-3520 Hz (09-17) | FM not compared on the G2 |
| Osc D | Confirmed on the G2 | §6.3, §6.1a | level and harmonics against OscA (09-12) | Pitch Type read since 09-28, not heard on the G2 |
| Osc Phase Mod | Modelled | §53 | - | - |
| Osc Shape A | Modelled | §27, §6.7 | through OscShpB's captures only | no OscShpA capture of its own |
| Osc Shape B | Confirmed on the G2 | §27, §6.7 | every wave, Shape Mod -1..+1, harmonics within 0.16 dB (10-02); Sine3/4 within 0.001 (09-17) | Pulse at exactly +-1: click -33 dB against the G2's -41 |
| Osc Dual | Confirmed on the G2 | §12.4, §12.5 | levels, duty, phase (09-12); PW and Phase mod (10-03) | - |
| Noise Osc | Confirmed on the G2 | §8.2-§8.5 | 09-12 capture agrees 110 Hz-1 kHz (the old model's fit) | level above 1 kHz not re-captured since the §8 model |
| Noise | Confirmed on the G2 | §7.2a | 09-12 capture reproduced through the desk's shelf to a constant | - |
| Metallic Noise | Modelled | §66 | - | no capture |
| Osc Percussion | Confirmed on the G2 | §40 | 8 takes: levels 0.1 dB, decay 2 ms, pitch exact (09-25) | - |
| Drum Synth | Confirmed on the G2 | §39 | Master Freq 0.2% (09-21); noise, click, oscillator energies within 0.8 dB (09-25) | level curve set by §39.3, not fitted to the 09-20 sweeps |
| Osc String | Modelled | §70.4 | - | - |
| FM Operator | Confirmed on the G2 | §14.2-§14.6 | envelope, level, FM depth, feedback, level and rate scaling, velocity (10-03) | AMod and Pitch inputs (§14.6) not yet compared (to-test); Gate/Note/Vel jacks read from the voice |
| DX Router | Confirmed on the G2 | §14.1, §14.5 | Main level 8.8 dB against 8.45 (10-03) | Out1-Out6 not used outside the node |
| Driver | Modelled | §70.5 | - | - |
| Resonator | Modelled | §70.4a | - | - |
| Osc Master | Modelled | §51 | - | - |

### Filters

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| LP Filter | Modelled | §22 | FM input at 2 semitones a unit only (notes §160) | no response captured |
| HP Filter | Modelled | §22 | - | no response captured |
| Nord Filter | Modelled | §23 | - | no response captured |
| Classic Filter | Modelled | §21 | 18 captures settled the loop, taken before the current law (09) | not re-compared since the 09-14 law |
| Multi Filter | Confirmed on the G2 | §10 | 54 noise responses within 0.5-0.6 dB (09-12); FreqM within 1% (10-08) | GComp off not captured |
| Phase Filter | Modelled | §67 | - | - |
| Comb Filter | Confirmed on the G2 | §13.4 | noise through all three Types (09-12); Pitch attenuator (10-08) | - |
| Static Filter | Modelled | §10.4 | peak heights only (2026-08) | HP tap term not modelled; a noise capture per type |
| FltVoice | Modelled | §56 | - | - |
| WahWah | Modelled | §69.9 | - | - |
| Vocoder | Modelled | §70.8 | - | - |
| Eq 2-band | Confirmed on the G2 | §11.2 | 43 noise settings across the EQs, 0.53 dB mean (09-12) | bypass not checked; deep cuts use a stable SVF (§11.5) |
| Eq 3-band | Partial | §11.3 | 0.66 dB mean, 1.42 worst (09-12) | mid-band width is an assumed 1-octave formula |
| Eq Peak | Confirmed on the G2 | §11.3 | 0.57 dB mean (09-12) | deep cuts use a stable SVF (§11.5) |

### Envelopes

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Envelope ADSR | Confirmed on the G2 | §17.1-§17.8 | time law and curves agree with the 08-24 and 09-07 captures (not kept) | - |
| Envelope AHD | Confirmed on the G2 | §17.11a | Hold 16-80, within 1% (10-08) | - |
| Envelope ADR | Modelled | §17.9 | - | stages never heard against the G2 |
| Envelop ADDSR | Modelled | §17.9 | - | stages never heard against the G2; release fixed 10-09 (§17.9a) |
| Envelope H | Confirmed on the G2 | §17.9a | Hold 12-64 within 0.1 ms (10-08) | - |
| Envelope D | Modelled | §17.9 | no KB: an untriggered EnvD silent, as in 14 CS80project72 (09-27) | decay never compared |
| Envelope Multi | Modelled | §17.11 | - | - |
| Envelope Mod AHD | Confirmed on the G2 | §17.9a, §17.10 | Hold within 0.3% (10-08) | time-mod jacks not checked |
| Envelope Mod ADSR | Confirmed on the G2 | §17.10 | Decay mod at -32..+32 units (09-20) | Attack and Sustain mod not checked; +32 units 4.1 s against 3.6 |

### LFOs

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| LFO A | Confirmed on the G2 | §28 | Hi rate measured | BPM and Clk not checked on the G2 |
| LFO B | Modelled | §28, §28.4, §54 | - | - |
| LFO C | Modelled | §28 | - | never captured |
| LFO Shp A | Confirmed on the G2 | §28.6 | Rate Sub, Lo, Hi within 0.013% (09-07) | BPM and Clk not checked; waves not captured |
| Clock Generator | Confirmed on the G2 | §59 | Master clock within 7-17 ppm; engine within 1-4 ms over 66 s (10-09) | - |

### Mixers

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Mixer 1-1 A | Confirmed on the G2 | §3 | 106 configurations of the eleven mixers within 0.07 dB (09-12) | - |
| Mixer 1-1 S | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| Mixer 2-1 A | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| Mixer 4-1 A | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| Mixer 4-1 B | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| Mixer 4-1 C | Confirmed on the G2 | §3 | 218 steps 0.01 dB; Pad 0/-6/-12 (09-07, 09-12) | - |
| Mixer 4-1 S | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| Mixer 2-1 B | Confirmed on the G2 | §3 | as Mixer 1-1 A; Inv cancels | - |
| Mixer 8-1 A | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| Mixer 8-1 B | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| MixFader | Confirmed on the G2 | §3 | as Mixer 1-1 A | - |
| MixStereo | Confirmed on the G2 | §5 | 19 settings each, Log and Lin (09-12) | - |
| Fade 1-2 | Confirmed on the G2 | §4.2 | 19 settings, every point within 0.001 (09-12) | mod-input depth not captured |
| Fade 2-1 | Confirmed on the G2 | §4.2 | as Fade 1-2 | mod-input depth not captured |
| X-Fade | Confirmed on the G2 | §4.2 | as Fade 1-2 | mod-input depth not captured |
| Pan | Confirmed on the G2 | §4.2 | as Fade 1-2 | mod-input depth not captured |

### Level

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Constant | Confirmed on the G2 | §16.1 | the known input of most G2 checks since 09-13 (OscShpB, OscDual, DrumSynth, FltMulti) | - |
| ConstSwM | Modelled | §68.1 | - | - |
| ConstSwT | Modelled | §44 | - | - |
| CompLev | Modelled | §48 | - | - |
| CompSig | Modelled | §69.2 | - | - |
| LevAdd | Modelled | §32 | - | - |
| LevAmp | Confirmed on the G2 | paramCurves notes §29 | 33 dial positions (08-30); 127 as 0x7FFFFF settled BCHydro_DZLW (10-09) | - |
| LevConv | Modelled | §31 | - | - |
| LevMod | Modelled | §69.3 | - | - |
| LevMult | Approximate | - | - | never compared |
| MinMax | Modelled | §43 | - | - |
| ModAmt | Modelled | §29 | parameter display only (param-validation) | - |
| NoiseGate | Modelled | §70.6 | - | - |
| EnvFollow | Modelled | §69.4 | - | times at 24 kHz when not up-rated not modelled (notes §203) |
| Red2Blue | Modelled | §68.8, notes §203 | - | - |
| Blue2Red | Modelled | §68.8 | - | - |

### Shapers

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Saturate | Approximate | - | - | one ramp per mode would capture it (capture-inventory) |
| Clip | Approximate | - | - | as Saturate |
| OverDrive | Confirmed on the G2 | §71 | energy above 6 kHz in 14 CS80project72: -41.5 against -41.6 dB (10-04) | - |
| ShpExp | Approximate | - | - | as Saturate |
| WaveWrap | Approximate | - | - | as Saturate |
| ShpStatic | Approximate | - | - | as Saturate |
| Rect | Approximate | - | - | - |

### Delays

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Delay Single A | Modelled | §52 | through DelayA only | - |
| Delay Single B | Modelled | §52.1 | - | - |
| Delay Dual | Approximate | §70.1, §52.1 | - | only the taps are fully modelled (§52.1) |
| Delay Quad | Approximate | §70.1, §52.1 | - | as Delay Dual |
| Delay A | Confirmed on the G2 | §24 | FB, LP, dry/wet, Time and Clk (09; audio not kept) | - |
| Delay B | Confirmed on the G2 | §24 | as Delay A; HP at four settings | - |
| Delay Stereo | Modelled | §65 | - | X-FB never compared |
| Delay Clock | Modelled | §69.7 | - | - |
| Delay Eight | Approximate | §70.1, §52.1 | - | as Delay Dual |
| DlyShiftReg | Modelled | §69.6 | - | - |

### Effects

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Compressor | Modelled | §25 | earlier fits (not kept) predate the current law | not re-captured since 09-14 |
| Digitizer | Modelled | §69.8 | - | - |
| FreqShift | Modelled | §57 | - | - |
| Flanger | Modelled | §70.2 | - | - |
| Chorus | Confirmed on the G2 | §19 | tap spans, rate, mix (09-07); unity at Amount 0 (09-14) | - |
| Phaser | Modelled | §55 | - | - |
| PShift | Modelled | §70.3 | - | - |
| Reverb | Confirmed on the G2 | §20 | 19 captures: onsets, stereo, Time law (09) | captured decays 6-12% long (their fits) |
| Scratch | Modelled | §70.3 | - | - |

### In/Out

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| 2 Outputs | Confirmed on the G2 | §63, notes §198 | Pad +6 dB (09-07); the path every G2 check goes through | - |
| 4 Outputs | Confirmed on the G2 | - | Pad +6 dB (09-07) | - |
| 2 Inputs | Confirmed on the G2 | §37 | full scale and Pad off the G2's own meter (10-08) | the application has no input device |
| 4 Inputs | Modelled | §37, §69.12 | through 2-In only | - |
| FX Input | Confirmed on the G2 | capture-inventory | Pad +6/0/-6/-12 dB (09-07) | - |
| Keyboard | Confirmed on the G2 | §26 | velocity through the Operators within 0.03 dB (10-03) | - |
| Monophonic Keyboard | Confirmed on the G2 | §35 | held-note return on 01 Mini Emulator (CT, 09-19) | Poly use is a guess |
| Device | Approximate | §70.13 | - | - |
| Status | Confirmed on the G2 | §70.13 | Patch Active is low, off the G2 (10-04) | - |
| Note Detector | Modelled | §69.11 | - | - |
| Name Bar | No sound | - | - | - |

### Switches

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| SwOnOffM | Modelled | §68.1 | - | - |
| SwOnOffT | Modelled | §30 | - | - |
| Sw2-1 | Modelled | §33 | - | - |
| Sw2-1M | Modelled | §68.1 | - | - |
| Sw4-1 | Modelled | §68.1 | - | - |
| Sw8-1 | Modelled | §33 | - | - |
| Sw1-2 | Modelled | §68.1 | - | - |
| Sw1-2M | Modelled | §68.1 | - | - |
| Sw1-4 | Modelled | §68.1 | - | - |
| Sw1-8 | Modelled | §45 | - | - |
| ValSw2-1 | Modelled | §34 | - | - |
| ValSw1-2 | Modelled | §68.2 | - | - |
| Mux8-1 | Modelled | §68.3 | - | - |
| Mux1-8 | Modelled | §68.3 | - | - |
| Mux8-1X | Modelled | §70.11 | - | - |
| S&H | Modelled | §38.5 | - | - |
| T&H | Modelled | §68.4 | - | - |
| WindSw | Modelled | §68.5 | - | - |

### Logic

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Invert | Modelled | §38.1 | - | - |
| Pulse | Confirmed on the G2 | §18 | 17 widths within two samples (09) | Plus/Minus edge (10-09) in to-test |
| Delay | Modelled | §46 | - | - |
| Gate | Modelled | §38.2 | - | - |
| FlipFlop | Approximate | §38.3 | - | - |
| ClkDiv | Confirmed on the G2 | §38.4 | 14 pattern seq steps once a bar, as on the G2 (10-07) | - |
| 8Counter | Modelled | §68.6 | - | - |
| BinCounter | Modelled | §68.6 | - | - |
| ADConv | Modelled | §68.7 | - | - |
| DAConv | Modelled | §68.7 | - | - |

### Sequencers

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Sequencer Event | Confirmed on the G2 | §58 | step timing at 192 kHz (09-26); 1:2 BCHydro_DZLW within 0.2 dB (10-09) | - |
| Sequencer Values | Confirmed on the G2 | §58 | as Sequencer Event | - |
| Sequencer Level | Confirmed on the G2 | §58, §69.1 | Length/Cycle restart in BCHydro_DZLW (10-09) | - |
| Sequencer Note | Confirmed on the G2 | §58, §58.1 | 18 Unreal Dreams' pitches (09-28) | the record stage never compared |
| Sequencer Controlled | Modelled | §70.10 | - | - |

### Random

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Random A | Modelled | §47 | - | Pitch input not read |
| Random B | Modelled | §69.10 | - | - |
| Rnd Clock A | Modelled | §64 | - | - |
| Rnd Clock B | Modelled | §70.9 | - | - |
| Rnd Trig | Modelled | §64 | - | - |
| Rnd Pattern | Modelled | §70.9 | - | - |

### Note

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| Note Quantiser | Modelled | §49 | - | - |
| Key Quantiser | Modelled | §41 | - | - |
| Partial Quantiser | Modelled | §69.5 | - | - |
| Note Scaler | Modelled | §60 | - | - |
| Glide | Modelled | §36 | - | - |
| Pitch Tracker | Modelled | §70.7 | - | - |
| Zero Crossing Counter | Modelled | §70.7a | - | - |
| Level Scaler | Modelled | §70.12 | - | - |

### MIDI

| Module | State | Reference | On the G2 | Open |
|---|---|---|---|---|
| CtrlSend | Confirmed on the G2 | §62.3 | 1:2 BCHydro_DZLW's variation switching, within 1-4 ms (10-09) | MIDI channels 1-16 dropped |
| PCSend | No sound | - | - | nothing leaves by MIDI |
| NoteSend | Partial | §62 | 18 Unreal Dreams' pitches and band balance (09-28) | MIDI channels 1-16 dropped |
| CtrlRcv | Modelled | §70.13 | - | in the plug-in only notes arrive |
| NoteRcv | Modelled | §70.13 | - | - |
| NoteZone | No sound | - | - | nothing leaves by MIDI |
| Automate | No sound | - | - | nothing leaves by MIDI |

## The G2 check queue

What would move the most modules to Confirmed on the G2, cheapest first. `to-test.md` carries the
individual checks; `capture-inventory.md` the rigs and what is already on disk.

| Check | Modules | Why first |
|---|---|---|
| One slow full-scale ramp through each mode | Saturate, Clip, ShpExp, WaveWrap, ShpStatic, Rect | memoryless, so one ramp is the whole transfer function; all six are Approximate |
| Noise through each filter, as FltMulti was (§10.3) | LP, HP, Nord, Classic, Static Filter | the filters most stage patches use; no response ever captured |
| A gate train through each envelope | Envelope ADR, ADDSR, D, Multi; Mod ADSR's Attack and Sustain mod | their stage structure has never been heard against a G2 |
| A level ramp through it | Compressor | not captured since its current model (§25) |
| Rate at a few dials, and BPM/Clk against the master clock | LFO B, LFO C, and BPM/Clk on LFO A and LFO Shp A | LFO C is in many patches and never captured |
| A Constant into each jack | FM Operator's AMod and Pitch (§14.6) | added 2026-10-09; nothing on file exercises them |
| Model fully, then capture | LevMult, Delay Dual/Quad/Eight, FlipFlop, Device, Eq 3-band's mid width | Approximate or Partial |


## Notes

A patch of nothing but **Not implemented** modules reports "Nothing is patched into it". One that
mixes the two plays its modelled part, which is why an unfinished patch sounds thin rather than
broken.

`2 Inputs` and `4 Inputs` play whatever the caller feeds the engine (§37): G2 Alike's side-chain as
In 1/2. The application has no input device yet, so there they are silent.

The palette offers 170 of the 210 types in `types.h`; the rest the G2 does not offer from its own
menus. `Operator` and `Name` sit outside `module_kind()` - an Operator plays only as part of the
DXRouter that owns it (§14), and a Name module is text.
