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

## Porting a module face from the original editor

**The original editor's own layout tables are readable text inside
`Original Editor/EditorResources/Nord Modular G2 Editor.rsrc`.** `strings` on
that file yields one `<#Module ... #>` block per module — 119 of them — each
listing every `<#Knob>`, `<#Input>`, `<#Output>`, `<#ButtonFlat>`,
`<#ButtonIncDec>`, `<#ButtonText>`, `<#TextField>`, `<#Text>`, `<#Led>`,
`<#Graph>` and `<#Bitmap>` with `XPos`/`YPos`. This is far better ground truth
than a screenshot of the manual, and it settles arrangement questions outright.

**`CodeRef` is the parameter index**, and our `paramLocationList` rows for a
module are in that same index order, so the mapping is exact and needs no
guesswork. `Input`/`Output` `CodeRef`s likewise match `connectorLocationList`
order. A `TextField` carries `MasterRef` naming the control it displays — pair
it with its control rather than treating it as a separate item.

**The coordinate transform.** The original's space is **255 px wide × 15 px per
row**; ours is `MODULE_WIDTH` (350) wide × `MODULE_Y_SPAN` (43) per row, less
`MODULE_Y_GAP`, less the body inset. For a module of `n` rows:

    x_pct = x_orig * 100 / 255
    y_pct = y_orig * bodyPct / (n * 15)      where bodyPct = (n*43 - 5 - 17.5) * 100 / 350

For the 12-row Operator that is `x * 0.392` and `y * 0.783`. **Our faces are
about 2.1x taller relative to their width than the original's**, so this is a
deliberately anisotropic transform — the arrangement is preserved, the
proportions are not, and the extra vertical room is what stops our larger
controls colliding.

**What to place at which element.** A dial goes at the `<#Knob>` position (its
value and label are drawn *above* it by the renderer, so the `<#Text>` and
`<#TextField>` rows for it are not separate items). A menu or toggle goes at the
`<#TextField>` position, not at its `<#ButtonIncDec>` spinner — the value box is
the visual anchor, and the label lands above it.

**Take the arrangement, not the appearance.** What the original is authoritative
about is *what sits where* — which control belongs beside which, what the bands
are, which parameters drive a graph. It is not authoritative about how any of it
looks. Colours, fonts, dial and jack styling stay ours (CT, 2026-08-29: "We don't
need to match the colour scheme exactly. Our own style is good."). So a heading
the original ships as a pre-rendered white-on-grey bitmap becomes ordinary text
in our style, and a graph it draws in its palette gets drawn in ours. This is the
same line the graph survey already drew for module icons.

**Then throw the original's stagger away.** Its few-pixel vertical offsets are
invisible at a 9 px font in a 255-wide face; at our scale the same offsets read
as sloppiness. Snap everything in a band to one `y`. Keep the *arrangement*, not
the pixel offsets.

**VALIDATE AT THE SMALLEST ZOOM, NOT AT 1.0.** Below roughly 0.7 the font stops
shrinking with the module, so every label is proportionally *wider* at low zoom
than at high. A top row that fits perfectly at `ZOOM 1.0` can run off the right
edge at the 0.59 a user actually works at. Capture at both.

**Two things the original draws that we do not (yet):**

- `<#Graph>` — computed curves, with `Graph Func` naming the drawing routine and
  `Dependencies` listing the parameters that drive it. The Operator has two: an
  envelope over params 8-15, and a keyboard level-scaling curve over 17-21.
  43 modules across the file carry one, which independently confirms the graph
  survey's "43 modules" figure.
- `<#Bitmap>` — pixel data in the `Data:` field as colon-separated `RRRGGGBBB`
  triplets, with `255000255` (magenta) as the transparency key. **These are not
  separators.** Decoded, the Operator's two read "Envelope" and "KB Lev Scale":
  section headings, pre-rendered as white-on-grey text 215x12 with only the first
  ~55px carrying the word. 42 modules carry them, 59 in all, and they are a mix
  of headings and small signal-flow icons (PolarPan, PolarFade and OscDual have
  three each; Invert, S&H and T&H two each — far more likely icons). Per the rule
  above, a heading becomes ordinary text in our style, never a copied bitmap.
  Headings go in `labelLocationList` (`tLabelLocation`, added 2026-08-29), which
  is a table of its own precisely because a `paramLocationList` row's *position*
  is the parameter index — a heading in the middle of a face would shift every
  parameter after it.

Their space is left empty by a faithful port, which is correct — do not close the
gaps up, or the graphs will have nowhere to go.

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
