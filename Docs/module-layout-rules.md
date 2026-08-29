# Module layout rules

Guidelines for positioning controls, connectors, labels and meters within a
module's face (the per-module `paramLocationList` / `connectorLocationList` /
`volumeLocationList` rows in `src/moduleResources.h`). Derived from the mixer
layout cleanup (2026-07), but they apply to any module.

## Coordinate reminders
- Rect coords are **percentages**, scaled to `MODULE_WIDTH` (350px) on **both
  axes** (`rectangle_scale_from_percent` in SynthLib `utilsGraphics.cpp`) — so
  `y` values are also a fraction of 350, not of the module height.
- **`y = 0` is the top of the BODY, just below the title bar** — not the
  module's outer edge (changed 2026-08-29). The face renderers are handed a body
  rectangle inset by `MODULE_BODY_TOP_PERCENT` (5%, i.e. 17.5px); the bar itself
  is 17px, so the body starts half a pixel clear of it. Before this, every
  top-anchored row had to leave room for the bar by hand, and they all settled on
  `y = 5` — which is exactly why the inset is 5% and not the bar's own 4.857%:
  the tables were already written against that figure, so the change cost no
  coordinate its whole number and moved no pixel.
- Which anchor you choose decides whether the inset touches you at all:
  **bottom-anchored rows are unaffected** (the bottom edge does not move), top
  ones start below the bar, and middle ones are centred on the body, which sits
  8.75px lower than the module's own centre.
- Module width is a fixed `MODULE_WIDTH` for **every** module; only height
  varies (`tModuleProperties.height` = number of rows; drawn height =
  `rows * MODULE_Y_SPAN - MODULE_Y_GAP`). You cannot widen a single module — fit
  the content into the shared width.
- A 2-row module is only ~81px tall; a dial is ~49px tall — so **vertical
  jack-above-dial stacking only fits on taller (3–4 row) modules**. On 2-row
  modules, pair a jack *beside* its dial instead.
- **A dial row's rect is the DIAL, not the label+value block** (SynthLib
  `render_dial_with_text`, dial-anchored since 2026-08-02). The value and label
  are drawn in the two rows *above* it, growing upwards, so a dial's position no
  longer depends on how many of those two strings exist: **`NULL` and `""` are
  equivalent** and neither can move the dial. Dial rect heights are 7, not 14 —
  do not "restore" the taller figure, and do not write `""` in place of `NULL`
  to reserve a blank row.

## The rules
1. **Nothing overlaps.** No component may sit on top of another — and that
   includes a control's **label / value text**, not just its body. The rightmost
   dial's value label overhangs its dial; leave room for it.

2. **Give the volume meter clear space — its own column *when there's room*.**
   On a wide-enough module, reserve a dedicated column for the meter (typically
   the far-right edge). When width is tight, fit it into available space instead
   — but never overlapping any control or label. Keep its **top and bottom gaps
   symmetric**.

3. **Standard dial width (7%)** across modules, for a consistent look — don't
   shrink dials just to make room; respace instead.

4. **Proximity encodes relatedness.** When a connector is associated with a
   specific control (an input jack and the dial that scales it), group them: the
   gap *within* a related pair must be **smaller** than the gap *between*
   adjacent groups (≈1:3 reads cleanly). Pair horizontally (jack beside dial) on
   2-row modules; vertically (jack directly above dial) on taller ones.

5. **Equal pitch** between like groups, and **centre the group block** so the
   left and right margins to the fixed clusters are balanced.

6. **Far-right I/O column pattern** (from Mix4-1C; applied to Mix8-1B): stack
   **Chain (top) → meter → Out (bottom)** on the right edge, and pull the channel
   block left to clear it. Put the Chain jack's label where it won't run back
   into the meter (label above, or high enough to clear).

## Worked examples
- **Mix4-1B** (2-row): jack-beside-dial pairs, Exp moved to the top-left
  standard position above the Chain jack, meter in a slim right column with
  symmetric gaps.
- **Mix8-1B** (4-row): vertical jack-above-dial strips, far-right Chain/meter/Out
  column, channel block centred between the Exp/Pad cluster and the meter.
- **Mix4-1C / Mix4-1S**: the reference "good" modules the pattern was inferred
  from.

_Status: modules changed under these rules are flagged "NEEDS (CT) VERIFY" in
`Module dev debug notes.txt` until confirmed on hardware by the owner._
