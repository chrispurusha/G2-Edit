# modulestatus.c - notes

Numbered notes for `tools/modulestatus.c`; the code points here as `// notes §k`.

## 1. The aspect audit (`--aspects`, `module_aspects()`, `kAspectEvidence`)

`./tools/do-modulestatus --aspects` prints every modelled module's aspects, one row each, with its state and
evidence (Docs/module-aspect-audit-design.md). The aspects a module HAS are read off the module tables, so
a new dial or jack cannot go unlisted:

- **Dials**: `paramLocationList` rows that are not switches, menus or buttons. **Timing**: those of a time
  or rate type. **Modes / On-Off**: the switch, menu and Bypass rows, and `modeLocationList`.
- **Inputs**: EVERY input jack, signal ones included - what an unpatched socket reads belongs here (an
  unpatched 2-Out side is silent, notes §167 of the engine). The table cannot tell a signal jack from a
  modulation one: 182 inputs carry no label, and blue jacks carry audio once up-rated.
- **Core law**: any module with an output, and the In/Out modules. **Level / gain**: those in the groups
  that make or scale a signal. **Meters**: `volumeLocationList` level meters and the Compressor's
  gain-reduction LEDs. **LEDs**: `ledLocationList` lamps and multi-LED groups, and the sequencers' step
  position, which arrives as a volume but is a position.

Dial names come from `Docs/param-validation.md` (its rows are in parameter-index order, as the table's
are); a jack with no label is named by its number and colour.

The evidence is editorial, one `kAspectEvidence` row per (module, aspect); an aspect without one reads
Unknown. The run fails if a row names a module that is not modelled or an aspect the module does not have.

## 2. The Fixed point column (`kPrecision`, 2026-10-16)

What arithmetic each module's model runs in, against the instrument's 24-bit words - the question of
whether a module's integer behaviour (truncation, rounding, internal saturation) is modelled. Asked after a
proposal for a global float/fixed-point switch was set aside (CT, 2026-10-16): the G2's fixed-point
behaviour matters module by module, so it is recorded module by module.

The classification was made by reading each model's step function: which values it holds in `int32_t` /
`int64_t` words, which it carries in doubles on the instrument's scaling and saturates (`dly_sat()`,
`seq_sat()`, `phaser_sat()` and the like), and which are plain doubles. It is editorial for the same reason
`kEvidence` is - the engine cannot say how faithful its own arithmetic is - and the run fails on a module
with no row, or a row for a module the tables do not list. Revise a row when a model moves to words.
