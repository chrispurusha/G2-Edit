# Replace-with-a-similar-module: design note

Living design note for the "replace this module with another of the same kind" feature. The FILTER
GROUP is built and working; the other eighteen groups have their membership recorded but no role
table, so they are not offered yet.

## What the feature is

The G2 manual describes it on p.82 under REPLACE A MODULE, and again on p.79 as "changing a module
into another type from the same module group": an arrow beside the module name opens a popup listing
the other modules in the same group, and picking one swaps the module **with its cable connections
preserved where they can be**. The manual's own example is wanting a mixer with more inputs, or a
different oscillator, without rebuilding the patch around it.

Two things the manual is explicit about and that shape the design:

- It calls the function **intelligent**, and the preservation is qualified — "with all cable
  connections preserved (if possible)". A replacement is therefore not a rename: it needs a mapping
  from the old module's connectors to the new one's, and a rule for what to do when there is no
  counterpart.
- **The popup lists are not the toolbar's module groups.** The manual says so directly: "the
  replacement module pop-ups doesn't always feature exactly the same modules as the module groups in
  the Toolbar". So neither the toolbar's categories nor our own guesses can stand in for the real
  membership.

## The groups

Nineteen groups covering 159 module types. The membership below follows the manual's module-reference
chapters (chapter 13, "Module reference"), which are organised group by group — SHAPER GROUP, FILTER
GROUP, LEVEL GROUP and so on — and which between them name every module the instrument has.

| group | n | members |
|---|---|---|
| Shaper Group | 7 | `Clip`, `Overdrive`, `Saturate`, `ShpExp`, `WaveWrap`, `ShpStatic`, `Rect` |
| Level Group | 14 | `Constant`, `ConstSwM`, `ConstSwT`, `LevAdd`, `LevConv`, `LevAmp`, `LevMult`, `LevMod`, `EnvFollow`, `NoiseGate`, `CompLev`, `CompSig`, `MinMax`, `ModAmt` |
| MIDI Group Send | 4 | `CtrlSend`, `PCSend`, `NoteSend`, `Automate` |
| MIDI Group Recv | 2 | `CtrlRcv`, `NoteRcv` |
| Note Group | 7 | `NoteQuant`, `KeyQuant`, `PartQuant`, `NoteScaler`, `Glide`, `ZeroCnt`, `PitchTrack` |
| Osc Group | 16 | `OscA`, `OscB`, `OscC`, `OscD`, `OscPM`, `OscShpA`, `OscShpB`, `OscDual`, `OscNoise`, `Noise`, `MetNoise`, `OscPerc`, `DrumSynth`, `OscString`, `Operator`, `OscMaster` |
| Keyboard Group | 2 | `Keyboard`, `MonoKey` |
| Input Group | 3 | `2toIn`, `4toIn`, `FxtoIn` |
| Output Group | 2 | `2toOut`, `4toOut` |
| Env Group | 9 | `EnvADSR`, `EnvH`, `EnvD`, `EnvADR`, `EnvAHD`, `EnvADDSR`, `EnvMulti`, `ModAHD`, `ModADSR` |
| Delay Group | 10 | `DlySingleA`, `DlySingleB`, `DelayDual`, `DelayQuad`, `DlyEight`, `DlyShiftReg`, `DlyClock`, `DelayA`, `DelayB`, `DlyStereo` |
| Switch Group | 18 | `SwOnOffM`, `SwOnOffT`, `Sw2to1M`, `Sw2to1`, `Sw4to1`, `Sw8to1`, `Sw1to2M`, `Sw1to2`, `Sw1to4`, `Sw1to8`, `ValSw2to1`, `ValSw1to2`, `WindSw`, `Mux8to1`, `Mux1to8`, `Mux8to1X`, `SandH`, `TandH` |
| Mix Group | 16 | `Mix1to1A`, `Mix1to1S`, `Mix2to1A`, `Mix2to1B`, `Mix4to1A`, `Mix4to1B`, `Mix4to1C`, `Mix4to1S`, `Mix8to1A`, `Mix8to1B`, `MixFader`, `MixStereo`, `Pan`, `XtoFade`, `Fade1to2`, `Fade2to1` |
| Filter Group | 14 | `FltLP`, `FltHP`, `FltNord`, `FltClassic`, `FltMulti`, `FltStatic`, `FltPhase`, `FltComb`, `WahWah`, `FltVoice`, `Vocoder`, `EqPeak`, `Eq2Band`, `Eq3band` |
| Effect Group | 9 | `StChorus`, `Digitizer`, `Flanger`, `FreqShift`, `Reverb`, `Compress`, `Phaser`, `PShift`, `Scratch` |
| LFO Group | 5 | `LfoA`, `LfoB`, `LfoC`, `LfoShpA`, `ClkGen` |
| RND Group | 6 | `RandomA`, `RandomB`, `RndClkA`, `RndClkB`, `RndTrig`, `RndPattern` |
| Sequencer Group | 5 | `SeqEvent`, `SeqVal`, `SeqLev`, `SeqNote`, `SeqCtr` |
| Logic Group | 10 | `Gate`, `Invert`, `FlipFlop`, `ClkDiv`, `Pulse`, `Delay`, `8Counter`, `BinCounter`, `ADConv`, `DAConv` |
The order within each group is the order the popup offers them in, which is neither alphabetical nor
by module type number: it is the order the manual's chapter introduces them.

Notes on the ones that are not where a guess would put them:

- **`Compress` and `Phaser` are effects, not level or filter modules** — both sit in the FX chapter.
- **`ClkGen` is an LFO**, which is right: it is a clock source shaped like one.
- **`Glide`, `ZeroCnt` and `PitchTrack` are Note modules**, not level ones.
- **`Noise`, `MetNoise`, `DrumSynth` and `Operator` are oscillators.** Anything that generates is.
- **`SandH` and `TandH` are switches**, filed with the multiplexers rather than with logic.
- **The MIDI group splits in two** — a send half and a receive half — because a CC sender and a CC
  receiver are not interchangeable even though the manual documents them in one chapter.
- **`Keyboard`/`MonoKey`, the two input modules and the two output modules** are their own small
  groups rather than part of a larger one.

## The eleven modules with no group

`Blue2Red`, `Red2Blue`, `DXRouter`, `Resonator`, `NoteDet`, `NoteZone`, `LevScaler`, and the four
that never appear on the canvas at all — `Device`, `Driver`, `Name`, `Status`.

These have no replace arrow. Every one of the first seven is genuinely one of a kind: there is no
second signal-colour converter, no second DX router. `LevScaler` is the interesting case, because the
manual documents it inside the Note chapter — so chapter membership alone is not sufficient, and a
module can be documented with a group without being offered as a replacement inside it.

## What is built (2026-09-07, extended 2026-09-08)

**All nineteen groups work.** `gModuleRoleList` holds 1202 role rows covering every group; the Filter
group was written first because its geometry is the least forgiving, and the rest followed once that
held.

EVERY INDEX WAS CHECKED AGAINST OUR OWN TABLES - each parameter index against that module's entry
count in `paramLocationList`, each connector index against its count in the matching direction in
`connectorLocationList`. All 1202 are in range. Two independently derived descriptions of the same
170 modules agreeing on every index is the check that would have caught a mis-parse.

There is a `REPLACE [VA|FX] <index> LIST` backdoor command that reports what the right-click menu
would offer, which is how a greyed-out "Replace with" gets diagnosed without a mouse.

The Filter group works. `moduleReplace.c` does the swap, `gModuleRoleList` in `moduleResources.h`
holds the Filter group's 99 role rows, and every module type now carries its group in
`gModuleProperties`. The module right-click menu has a **Replace with** submenu listing the rest of
the group; it is greyed out only for the eleven modules that are in no group at all. There is a `REPLACE [VA|FX] <index> <name>` backdoor command for scripted testing.

Three decisions worth knowing:

- **The module keeps its index.** This is a mutation, not a delete followed by a create. Every cable,
  knob assignment, MIDI CC and morph in the patch refers to a module by index, so handing the
  replacement a new one would break all of them for the sake of a swap the user thinks of as editing
  one module.
- **It goes to the instrument as one whole-patch write.** A module or cable write is an ADD on the
  G2, so rewriting one leaves the device holding it twice, and back-to-back commands race the patch
  version the device bumps asynchronously. One `eMsgCmdWritePatch` has neither problem.
- **Modes do not carry across.** The drop-down selectors — a filter's Slope, an oscillator's
  waveform — have no role rows in this group, so they start at the new module's own defaults. That
  is what the instrument's own table does, not an omission.

**A taller replacement rearranges its column**, and this needed nothing new: `module_replace()` sets
the module's type BEFORE calling `shift_modules_down()`, so the height that function works from is
the new one and the existing insert-and-push logic does the rest. Verified on a packed column -
FltClassic (4 rows) to Vocoder (8) at row 8 of a column filled 0/4/8/12/16 pushed the two modules
below it down by exactly four rows, with no overlap anywhere. If the column cannot take the taller
module the whole swap is refused and nothing changes, cables included.

**Confirmed against the instrument, 2026-09-08.** With a G2 connected the swap is written as one
whole-patch command and the device keeps it: after replacing FltClassic with FltNord, switching to
slot B and back - which pulls the instrument's own copy - the module is still FltNord. The cable
handling survives the same round trip: replacing with an FltStatic left the two audio cables and
removed the pitch-modulation one from the G2's copy, not merely from ours.

## What still has to be worked out before this can be built

The group table is the easy half. The hard half is the mapping, and it is per group rather than per
pair of modules: the original organises it as **named roles** shared across a group's members, with
each member saying which of its own connectors and parameters fill each role. For the Shaper group
those roles are Inputs, Inputs Mod, Outputs and Params; the Level group adds roles like "Inputs B"
and "Params Range". A cable on a Clip's modulation input then moves to an Overdrive's modulation
input because both are the group's "Inputs Mod" role — NOT because both happen to be connector 1.

So the remaining work, for the eighteen groups that are not the Filter group, is:

1. A role table per group: role name, then per member the connector or parameter indices that fill
   it. Our `moduleResources.h` already numbers both, so the table is expressible in the existing
   vocabulary.
2. The swap itself: replace the module in the database, remap every cable whose end lands on it,
   DELETE the cables whose role has no counterpart in the new module, and carry parameter values
   across by role rather than by index.
3. Undo. The cable work already has before/after snapshotting for a location's whole cable set; a
   replace is a bigger version of the same thing and should reuse it.
4. The UI: the manual puts the arrow beside the module name. A right-click menu entry is the
   cheaper first version and is what `todo.txt` asks for.

Steps 2 to 4 are done and are group-agnostic, so each further group costs only its role table. The
Filter group was first because it is the one with the most awkward geometry — Vocoder's audio input
is connector 1 where every other filter's is 0, FltPhase's fixed pitch input is connector 4 where
FltClassic's is 2, and FltStatic has no pitch modulation at all — so a mechanism that gets filters
right is not quietly relying on the indices happening to line up. Oscillators next.

## What this shares with the module palette

`module-palette-design.md` needs the same nineteen groups to organise a drag-on palette, and the
manual's chapters are the same source for both. Whichever is built first should put the table
somewhere the other can use it rather than in its own file.
