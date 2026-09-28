# Will this patch fit on the G2? - design note

Living design note for estimating a patch's DSP budget on the instrument and WARNING when it is over -
never limiting the sound engine to match (todo.md, CT 2026-09-27). Step 0 built (2026-09-27); the offline estimate is not.

## What the instrument does (decoded 2026-09-27)

**Resources.** The original editor keeps a per-patch resource record per area (Voice area, FX area, and
a third context) and shows each as a percentage of one DSP's capacity. The record holds, per module:

| resource | fields | capacity (one DSP) |
|---|---|---|
| Cycles | audio-rate cycles + 0.25 x control-rate cycles | 1 371 per 96 kHz sample (~131.6 MHz) |
| X memory | audio-rate words + control-rate words | 4 336 |
| Y memory | audio + control | 2 992 |
| P (program) memory | audio + control | 6 498 |
| Zero page | one count | 128 |
| RAM, Q, R | integer counts (delay memory etc.) | not yet decoded |

Control-rate parts run at 24 kHz, which is why their cycles weigh a quarter; up-rating a module moves its
control-rate figures into the audio-rate ones. Every cable adds 3 cycles to its area. The "critical
resource" is whichever percentage is highest. The dialog calls these "Resources Used (PVA/CVA in %)" -
per-voice and common Voice-area load.

**Per-part costs.** Each DSP part has a size record: cycles, a flags word, X words, Y words, P words, and
two more. Checked against parts whose programs and frames are known: ValSw's part is 9 cycles, X 2, Y 2,
P 10 (its program is 10 words, its frames 2 and 2); Glide's main part 14 cycles, X 2, Y 6, P 15.

**Voice placement.** The synth's code places voices across its DSPs with these records (a voice
placer that pre-allocates voices slot by slot and compares load sizes). This is what decides the voice
count the G2 reports, e.g. "15 (16)".

**The instrument reports its own load.** The protocol carries the DSP's current load (the editor
requests it and the synth streams it back); the original editor stores it as the "reported" figure
beside its own estimate.

## Open

1. **The per-module record.** A module's record hangs off its type descriptor. `_k<Module>ModuleSize` is
   exactly the record's size (32 bytes), but read in the record's layout it does not obviously match the
   module's parts (ValSw: 12 cycles and 1 word of each memory against its part's 9 cycles, 2/2/10) -
   either a different quantity or a layout not yet understood. Decode which, and whether a module's
   cost depends on its parameters (parts linked in or out by mode, e.g. a delay's range).
2. **RAM / Q / R capacities** and the third context.
3. **The voice placer**: how many DSPs a slot may use, how the common (FX) load is shared, and the
   placement order - the synth's own code is in the instrument's own code.
4. ~~The reported load message~~ - ALREADY RECEIVED: `parse_resources_used()` (usbComms.c) reads the
   G2's per-location record (red/blue cycles, zero page, X/Y/P for both rates, RAM, Q, R) into
   `gResourceAlloc`, with the same formula and capacities as above. With a G2 connected the warning
   needs no estimate at all.

## Plan

0. DONE 2026-09-27 - with a G2 connected, the top bar's four resource figures (the G2's own, already
   shown) turn red at 100%, as the voice count already does when fewer voices are assigned than asked.
1. Offline and in the plug-in, an estimate: settle (1) against the G2's reports - build patches over
   the backdoor, read `gResourceAlloc`, compare with the per-part sums.
2. Port the resource record and percentages as a pure function over the patch database, with a test
   over the stage patches.
3. Port the voice placer, then show assigned voices and the critical resource in the top bar, in
   warning colour when over 100% or when fewer voices fit than the patch asks for.
