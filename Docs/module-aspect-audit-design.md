# Module aspect audit - design

Living design note, started 2026-10-09. Step 1 built 2026-10-10: `./tools/do-modulestatus --aspects`.

## Why

`engine-module-status.md` gives each module ONE state. A module marked Confirmed on the G2 can mean that its
main law matched one capture while its modulation jacks, its Off state, its level or its envelope slopes were
never compared. Those gaps appear only where someone happened to write them in the Open column (Pan's
mod-input depth, the EQs' bypass, ModADSR's Attack and Sustain mod) and are silent everywhere else. The audit
replaces the single verdict with one state per ASPECT of each module.

## The aspects

| Aspect | Covers | Applies to |
|---|---|---|
| Core law | the transfer curve, waveform, filter or effect response | every module that makes or shapes a signal |
| Timing | envelope stage times and slopes, LFO and clock rates, delay times, glide | modules with a time or rate dial |
| Dials | each dial's law: taper, range, the value at 127, morph resolution | every module with a dial |
| Mod inputs | each modulation jack's depth and law, and what an unpatched jack reads | every module with a control input besides its signal input |
| Level / gain | absolute output level against a known reference (an OscA sine, a Constant) | every module that makes or scales a signal |
| Modes / On-Off | drop-down modes, and what Off does - pass the input, go silent or hold | every module with a mode, a bypass or an On/Off |

Further aspects (CT, 2026-10-09):

| Aspect | Covers |
|---|---|
| Rate | audio rate or the 24 kHz control tick, and correct behaviour when up-rated or not (notes §203) |
| Voicing | per-voice or shared state, Mono/Poly, what a new note resets, how a voice's tail ends |
| Start state | what the module holds at patch load (random phases, seeds, Logic Delay's Neg high, ClkGen's first tick) |
| Control response | knob smoothing and zipper, morph and Vel/Keyb per voice, MIDI CC |
| Headroom / precision | saturation points and fixed-point truncation (tails reaching exact zero, 0x7FFFFF not 4.0) |
| Rate dependence | behaviour at other graph rates: 48 kHz economy, 44.1 kHz hosts |
| Area | Voice area against FX area (no key in the FX area, evaluation after the mix) |
| Limits | pool sizes (`MAX_*_LINES` and the like) - instances the engine drops where the G2 runs them |
| Meters / LEDs | the module's lamp and meter against the instrument's law |
| Cross-slot / area | only for NoteSend, CtrlSend, CtrlRcv, NoteRcv, ClkGen, Status, FX Input, 2-In/4-In from a bus: points at the engine-behaviour table below |

## Engine behaviours (not per module)

A second table in the status doc, same states, for what belongs to the engine rather than to a module:

| Behaviour | Covers |
|---|---|
| Slot to slot | NoteSend/CtrlSend delivery and timing, controller 70 variation switching at the next snapshot rebuild |
| Master clock | one position shared by every slot: ClkGen on Master, LFO Clk/BPM, the arpeggiator |
| Voice to FX | the 2-Out / FX In / bus bridge and its timing |
| Threads | voice workers: determinism (18 Unreal Dreams differs threaded and serial), resets on the worker, one-block silences |
| Variations | a change landing between blocks; per-variation patch settings |
| Latency | each path's delay, application and plug-in |

Each aspect takes the states the status doc already uses: Confirmed on the G2, Modelled, Approximate, and
Unknown (nothing documented either way). An aspect that does not apply is left out, not marked.

## How the generator finds them

`tools/modulestatus.c` already walks every offered module. The aspects a module HAS come from the module
tables, so none can be forgotten:

- Dials: `paramLocationList` rows for the type (the On/Off button is split out into Modes / On-Off).
- Mod inputs: the type's input connectors other than its signal input (`connectorLocationList`; the role
  table in moduleResources.h already names "VCA Inputs", "Trig & Gate Inputs", "Amp Inputs" and so on).
- Modes / On-Off: `modeLocationList` rows, and an On/Off or Bypass parameter.
- Timing: dials whose parameter type is a time or rate (`paramTypeLFORate`, the envelope time types, delay
  times, glide).
- Core law and Level / gain: every module that is not purely a control or logic module.

The EVIDENCE stays editorial: `kEvidence` grows from one row per module to one row per (module, aspect), with
the reference section, what was compared on the G2, and what is open. The run fails if a module has an aspect
with no evidence row, so a new jack or dial cannot slip through unrecorded.

## Output

- The summary table counts aspects as well as modules, by state: that shows where the gaps are.
- Each module's row becomes a small grid, one column per aspect, its state and a section reference.
- The G2 check queue is rebuilt from the Unknown and Modelled aspects, grouped by the rig that would settle
  them (a ramp for transfer curves, a gate train for envelopes, a Constant into each mod jack, a level
  reference, an On/Off toggle).

## Order of work

1. Generator: enumerate each module's aspects from the tables; print the aspect list with an empty evidence
   column, and the count. No editorial content yet.
2. Fill the evidence from `sound-engine-reference.md`, group by group, starting with the modules most stage
   patches use: oscillators, filters, envelopes, LFOs, mixers.
3. `findings.md` and `capture-inventory.md` for what was compared on the G2, aspect by aspect.
4. Regenerate the status doc; rebuild the check queue from it.

## Progress

- **Step 1 (2026-10-10).** 773 aspects over the 170 modules: Core law 168, Timing 27, Dials 121, Inputs 154,
  Level / gain 111, Modes / On-Off 134, Meters 19, LEDs 39 (code-notes/modulestatus.c.md §1). Two changes from
  the plan: **Mod inputs became Inputs**, every input jack - the table cannot tell signal from modulation
  (182 inputs have no label), and what an unpatched SIGNAL socket reads turned out to matter (an unpatched
  2-Out side is silent on the G2, not a copy of the other); **Level / gain goes by palette group**, since
  mixers, Pan and the Outs have blue outputs until up-rated. The further aspects (Rate, Voicing, Start
  state, ...) cannot be read off the tables and come in with the evidence.
- **Meters and LEDs are two aspects** (CT, 2026-10-10): a meter's law and voice against each lamp's own
  logic. Each row names what the module shows (mono/stereo/quad meter, gain-reduction LEDs, lamps, a
  multi-LED group, the sequencers' step position). The engine drives the mixer, Out, In and FX In meters,
  the Compressor's lamps, the LFO family's and DrumSynth's lamps - and nothing else: six filter and EQ
  meters, 29 modules' lamps and the step position stay dark without a G2. Those read Partial.
- 72 aspects have evidence so far, seeded from 2026-10-10's findings: Pan's mod depth, the unpatched Out
  socket, the meters' one voice, LFO start phase, and the Pads and mixer taper already measured.

Expect some modules to lose standing: a Confirmed module whose mod inputs and Off state were never compared
becomes Confirmed for its core law and Modelled or Unknown for the rest.
