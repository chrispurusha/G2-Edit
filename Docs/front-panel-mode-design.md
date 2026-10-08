# Front panel mode — design & progress

Living design note. Nothing is built yet; this records what the mode is for, what it reuses and the
order to build it in, so the work can start without re-deriving any of it. See also `todo.md`
("Plan a mode switch ... G2 keyboard's front panel").

## What is being asked for

A second view of the instrument: instead of the patch canvas, the G2 Keyboard's front panel - the
controls a player uses, with the patch's own assignments on them - and a switch back to editor mode.
Useful with a G2 Engine (which has no panel), in G2 Alike (which has no hardware at all), and on a
G2 Keyboard sitting across the room. The manual makes the same point: "virtually all described G2
Keyboard and G2X panel functions are also available as 'soft' functions in the Editor program"
(chapter 3, p.25) - this mode gathers them into the panel's own layout.

## What the front panel has (manual chapter 3, pp.25-35)

Two sections, left and right.

**System Functions (left).** Master Level knob; MIDI LED; Mic Level knob and its three input LEDs;
SYSTEM, PATCH and STORE buttons above the MAIN DISPLAY; four NAVIGATOR buttons and the ROTARY DIAL;
PATCH LOAD; SLOT buttons A-D below the display, with their Active/Focus LEDs; PERF MODE; OCTAVE SHIFT
left/right with five LEDs (Shift+left: global shift); KB HOLD / PANIC (Shift); FOCUS/COPY (Shift:
ASSIGN/PASTE); DISPLAY MODE (names+values or module+names); pitch stick and mod wheel, and on a G2X
the two global wheels (morph groups 5 and 8).

**Sound Functions (right).** Four ASSIGNABLE DISPLAYS, each over two ASSIGNABLE KNOBS and two
ASSIGNABLE BUTTONS: eight knobs, eight buttons. Each display shows a module name and two parameter
names, a parameter's value while it is being moved (DISPLAY MODE changes which lines show). The
PARAMETER PAGES buttons pick one of 15 pages by ROW and COLUMN. Eight VARIATION buttons, which double
as MORPH GROUP buttons in Morph mode (the MORPH button). PATCH SETTINGS / GLOBAL PANEL switches the four
displays between the Parameter Pages, the patch settings printed above them, and (Shift or double
press) the performance's Global Parameter Pages.

**Patch Settings, as the panel lays them out** (p.34-35): knob over button, left to right - Master
Clock rate / Run-Stop; Voices / Poly-Mono-Legato; Arpeggiator period / On; Arp direction / range;
Vibrato depth / source; Glide rate / mode; Bend range / on; Patch Level / Mute.

## What already exists to build it from

| Need | Already in the editor |
|---|---|
| Eight knobs of a page, live, editable, undoable | `paramPages.c` - draws them with the canvas's own `render_param_common()` and drags with `gParamDragging` (memory: project_g2edit_parameter_pages). The three seams: `set_param_render_area()`, the `gParamRectangle` read-back, `finish_param_drag()` |
| Which module/param a knob is on, display names | `param_pages_knob_target()`, `param_pages_module_display_name()`, `param_pages_knob_param_label()` in `paramPages.h`, shared with `paramOverview.c` |
| Patch settings and their values | `settingsPanels.c` (`render_patch_settings_panel`), the topbar's clock, voices, mono/poly, volume |
| Slots, variations, perf mode | the topbar's own controls (`topbarControls.def`) and their handlers in `mouseTopbar.c` |
| Wheel / stick / pedals | the topbar's morph knobs (Wheel, Vel, Keyb, Aft.Tch, Sust.Pd, Ctrl.Pd, P.Stick, G.Wh 2) |
| A keyboard | `virtualKeyboard.c` |
| Bank browsing and load | the List Names cache and Load from Bank (memory: project_list_names_protocol, project_store_to_bank) |
| Store to bank | Store to Bank Location, peek-then-confirm |

Every edit goes through the paths these already use, so the G2 (when connected), the local engine and
undo all see a panel edit exactly as they see a canvas edit. Nothing here talks to the device itself.

## Decisions proposed

1. **The switch is a TOPBAR button, not a menu-bar one** (CT 2026-10-08). The topbar is drawn by the
   same code in the application and G2 Alike and stays on screen in both modes; a button at the far
   right of the menu bar would not survive a host's narrow window, and the menu bar is not the same
   bar in the plug-in (`gPluginMenuBar`). Place it on the top row between Redo (x 335) and the clock
   (x 475), e.g. x 380, as a two-state button: "Panel" in editor mode, "Editor" in panel mode (CT
   2026-10-08). A View menu item alongside it can come for free. The topbar may be rearranged to make
   room or use the space better (CT 2026-10-08) - it is laid out from `topbarControls.def`.
2. **The panel replaces the module panes only.** Menu bar and topbar stay, so the switch back is always
   where it was, and so are the slot, variation and morph controls the panel would otherwise have to
   duplicate first. The split bar, scrollbars, palette band and cable dragging are off in panel mode;
   every canvas mouse handler must see the mode and step aside (the "click on a module that is gone"
   guard of 10-03 is the pattern).
3. **Drawn in the editor's own style, laid out like the hardware** - the two sections in their places,
   SynthLib primitives and the ASCII glyph atlas, no photograph. A fixed logical canvas scaled to the
   area, so it reads the same in a plug-in window and a full-screen app.
4. **Display content follows the hardware**: module name, two parameter names, the value replacing the
   name while a control moves; DISPLAY MODE switches to names+values.
5. **The mode is per window and remembered**: a preference in the application, a key in G2 Alike's
   state record (`G2_STATE_HEADER` text), so a project reopens in the mode it was saved in.

## What is left out, and why

- Master Level and Mic Level: analogue controls with no MIDI or USB counterpart (manual p.26).
- OCTAVE SHIFT, STORE, SYSTEM menus and the NAVIGATOR/ROTARY DIAL text menus: the editor already
  offers these as dialogs; the panel can carry buttons that open them rather than re-creating a
  two-line LCD menu system. Revisit once the rest is in use.
- FOCUS/COPY/ASSIGN: assignment is done on the canvas (right-click a parameter); the panel shows
  assignments, it does not make them in the first version.

## Scope of the first version (CT 2026-10-08)

The eight dials, the eight buttons, the four displays and the PARAMETER PAGES buttons - nothing else.
Everything else under "Order of work" from step 2's Patch Settings onwards is optional, to be decided
once this is in use. The keyboard stays the separate Virtual Keyboard panel.

## Order of work

1. The mode itself: the topbar button, a `gFrontPanelMode` flag (document field, so each G2 Alike
   instance has its own), the module panes replaced by an empty panel area, mouse routing, the
   preference and the state-record key. Both builds.
2. Sound Functions: the four displays and eight knobs + eight buttons of the current page, through
   `paramPages.c`'s seams; the PARAMETER PAGES row/column buttons; PATCH SETTINGS switching the
   displays to the eight patch settings above.
3. Variation and Morph buttons (Morph mode turning them into group selectors), and the Global Panel
   (the performance's Global Parameter Pages).
4. System Functions: the main display (slot, patch and performance name, bank:location), PATCH LOAD
   with the bank list, slot buttons with Active/Focus, KB HOLD/PANIC, DISPLAY MODE, a MIDI LED driven
   from incoming MIDI.
5. Optional: the virtual keyboard docked along the bottom of the panel.

## Settled questions

- First version: dials, buttons, displays and page buttons only (above).
- Keyboard: separate, as now.
- Button label: "Panel" / "Editor".

## Progress (2026-10-08, session ended mid-build)

First-version scope adds the PATCH SETTINGS / GLOBAL PANEL button (CT): one press shows the eight
patch settings, Shift or a quick second press the Global Parameter Pages, a press again the patch's pages.

DONE, uncommitted, builds (app and plug-in):
- `globalVars.h`: per-document `gFrontPanelMode`, `gFrontPanelView`, `gFrontPanelPage[2]`,
  `gFrontPanelBank[2]` ([0] patch pages, [1] global); zero-fill = editor mode, pages, A1.
- `paramPages.c/.h`: `param_pages_release_target()` public (toggle cycles / menu opens on release),
  and the Parameter Pages panel now sets `gParamDragging.rect` at press - rotary drags there turned
  about whatever dial the canvas last captured.
- `menus.c/.h`: `send_master_clock_bpm()` (moved from mouseHandle.c, which the plug-in does not
  compile), `send_patch_setting_param()`, `send_patch_descr_update()` now public.
- `src/frontPanel.h`: the interface. `frontPanel.c` NOT written yet.

DECIDED, from reading the code:
- Knobs on the Parameter Pages are drawn with `render_param_common()`: it registers the canvas's
  own click region per widget, so press, drag (rotary rect captured), toggle and menu all work in both
  builds with no new input code. Its name and value rows sit ABOVE the dial (value at dialY - textH,
  name at dialY - 2 textH, left-aligned at the dial's x), so the display box runs down to the dial:
  line 1 the module name (dashes when the previous position is the same module, as on the G2), lines
  2-3 the widget's own text. Text is black: pale LCD display colour, red panel.
- A position with `isLed` = 1 is a BUTTON assignment: drawn as a lit button below the dial, its own
  click region (eClickLayerPanel) calling `param_pages_release_target()` on release.
- Patch settings need their own drag (the canvas gives every locationMorph param a range of 128):
  a new row in canvasDrag.c's `sGestures` (`canvasGesturePanel`, motion only), started from the knob's
  own region on press, undo pushed on release / release-outside; `stop_dragging()` cancels it.
  Pairs: Clock rate 30-240 / Run (gGlobalSettings, send_master_clock_*); Voices 0-31 shown +1 / Poly-
  Mono-Legato (gPatchDescr, send_patch_descr_update, undo_push_patch_descr); Arp ARP_SPEED (4) /
  ARP_ON_OFF; ARP_DIRECTION / ARP_OCTAVES; VIBRATO_DEPTH (101, "cnt") / VIBRATO_MOD (Off, AfTouch,
  Wheel); GLIDE_SPEED (patch_settings_glideStrMap) / GLIDE_TYPE (Off, Normal, Auto); BEND_RANGE (24,
  +1 semi) / BEND_ON_OFF; VOLUME_LEVEL (active variation, send_param_value, patch_volume_db) /
  VOLUME_MUTE. All but Volume go through variation 0 and send_patch_setting_param, as the Patch
  Settings panel does.
- Routing guards in panel mode: `canvas_empty_press()` consumes (no rubber band);
  `canvas_right_click()` hands to `front_panel_right_click()` (canvas widget first, then the panel's
  buttons); the app's split-bar and scrollbar clicks and the plug-in's are skipped (their rects are
  last canvas frame's); scroll wheel returns after the palette in both; key_callback ignores Delete,
  Backspace, arrows, bare V and Cmd-C/X/V/A; the plug-in's bare V likewise. Entering the mode clears
  the selection.
- Frame: graphics.c and plugin/g2Draw.c draw `front_panel_render()` instead of the pane loop, split
  bar, scrollbars and dragged cable; `set_click_region_clip(NULL)` first. Area: from
  MENU_BAR_HEIGHT + TOP_BAR_HEIGHT + palette_band_height() to the bottom.
- Topbar: `X(topbarFrontPanelId, 380, 8, "Panel", ...)` after Redo; its label comes from
  `front_panel_button_label()` ("Panel"/"Editor"), green while active; mouseTopbar.c toggles.
- Remembered: app pref `frontPanelMode` (written only by the app build); G2 Alike state line `panel=`.

TODO next session: frontPanel.c and Docs/code-notes/frontPanel.c.md; the hooks above; add
src/frontPanel.c to do-plugin's source list; build both; screenshot both; to-test entry.
Also noticed: the Patch Settings panel's arp rate shows 4 values (1/8 .. 1/16T) but its dropdown
offers 14 (1/96 .. 1/1) - settle which the G2 has.
