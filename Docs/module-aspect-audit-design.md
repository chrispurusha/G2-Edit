# Module aspect audit - design

Living design note, started 2026-10-09. Nothing built yet.

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

Expect some modules to lose standing: a Confirmed module whose mod inputs and Off state were never compared
becomes Confirmed for its core law and Modelled or Unknown for the rest.
