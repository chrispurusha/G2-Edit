# frontPanel.c notes

The longer comments for `frontPanel.c`, numbered; the code points at each as `// notes §k`. The design and
the decisions behind it are in `Docs/front-panel-mode-design.md`.

## 1. file scope

THE G2 KEYBOARD'S FRONT PANEL IN PLACE OF THE PATCH CANVAS, switched from Tools > Show Front Panel / Show
Editor (CT 2026-10-08: a menu item, not a topbar button). The menu bar and topbar stay; only the module
panes, split bar, scrollbars and dragged cable give way (graphics.c and plugin/g2Draw.c). The mode is a
document field, so every G2 Alike instance has its own; the application remembers it in the preference
`frontPanelMode`, G2 Alike in its state record as `panel=`.

First version (CT): the four displays, eight knobs, eight buttons, the PARAMETER PAGES buttons and the
PATCH SETTINGS / GLOBAL PANEL button.

## 2. `render_page_position()` and `position_is_button()`

ONE ASSIGNMENT PER POSITION. Each of the eight positions holds one parameter; `isLed` in the knob
record says whether the G2 puts it on the knob (0) or on the button under it (1). So a position draws a
live knob or a live button, never both, and the other is drawn blank.

A knob whose parameter the canvas drags as a dial is the canvas's own widget, `render_param_common()`
drawn into mainArea. That function registers the canvas's click handler for it, so press, drag (the
rotary rect captured at press), Alt-drag morphs, undo and right-click all come with it, in the
application and the plug-in, with no input code here. It draws its name and value ABOVE the dial
(value at dialY - textH, name at dialY - 2 textH). SynthLib's `set_dial_text_gap()` lifts those two
rows while the panel draws, so they land on the display's lines 2 and 3 and the dial sits clear below
the box; the panel's own lines 2 and 3 use the same rows. The gap goes back to 0 for the canvas.

## 3. `settings_button_click()`

PATCH SETTINGS / GLOBAL PANEL, as on the G2 (manual pp.33-34): a press switches the displays between
the Parameter Pages and the eight patch settings; Shift + press, or a second press within 400 ms,
switches to the performance's Global Parameter Pages, and from there a press goes back. A PARAMETER
PAGES button pressed during Patch Settings returns to the pages.

## 4. `tSettingKind`

WHERE EACH PATCH SETTING LIVES. Most are parameters of the hidden modules in locationMorph, and they are
PER VARIATION (sound-engine-reference §63a; CT 2026-10-08): the panel reads, writes, sends and undoes
them in the slot's active variation, through `send_param_value()`, as the topbar's Volume dial does. The
Patch Settings panel does the same since the same day; both go through `patch_settings_variation()`. The master clock is the instrument's
(gGlobalSettings), and voices and mono/poly are the patch descriptor's.

## 5. `kSettings`

THE PANEL'S OWN ORDER, knob over button, left to right (manual p.34): Master Clock rate / Run-Stop;
Voices / Poly-Mono-Legato; Arpeggiator period / On; Arp direction / range; Vibrato depth / source;
Glide rate / mode; Bend range / On; Patch Level / Mute. Voices are stored 0-31 and shown + 1, and a
Mono or Legato patch shows 1, as the topbar does. The arpeggiator's four periods are the Patch
Settings panel's; its dropdown offers fourteen, and which the G2 has is open (todo).

## 6. `front_panel_set_active()`

THE SELECTION IS CLEARED ON THE WAY IN. Delete, Cut and the rest are guarded in key_callback() while
the panel is up, but clearing the selection as well means nothing invisible can be acted on by a
path that guard does not know about.

## 7. in `render_page_position()`

DASHES FOR THE SAME MODULE, as the G2 displays them (manual p.38): when the position before is on the
same module, line 1 shows dashes instead of repeating the name. A global page names the slot too
("A:OscB"), because a global page's knobs can be on any slot's patch.

## 8. `front_panel_render()`

A FIXED LAYOUT IN mainArea UNITS, centred, the displays between 170 and 300 wide: the canvas widgets
drawn into it measure their text in those units, so the panel cannot simply be scaled. The area runs
from below the topbar and palette band to the bottom of the window. `set_click_region_clip(NULL)`
first: the last pane drawn would otherwise leave its clip on every region registered here.

INCLUDE defs.h FIRST. It defines G2_EDIT, and synthlibDefs.h picks the G2's 80-unit topbar only when
that is already defined; included after frontPanel.h, it gave TOP_BAR_HEIGHT 0 and drew the whole
panel 80 units high, under the topbar.

## 9. `tPanelDrag`

ONE DRAG FOR THE KNOBS THE CANVAS DOES NOT DRAG: a patch setting (the canvas would give any
locationMorph parameter a range of 128), and an assigned parameter that is a switch or a list, which
the G2's knob steps through. Started from the knob's own click region, moved from canvasDrag.c's
gesture table (`canvasGesturePanel`) so the application and the plug-in share it, and finished by
the same region's release or release-outside, which pushes one undo for the gesture.
`stop_dragging()` finishes it too. A parameter knob pressed and released without moving acts as a
click on its canvas widget would - a switch steps, a list opens.

## 10. the PARAMETER PAGES buttons

LAID OUT AS ON THE G2 (CT): the row buttons A-E down the right, the column buttons 1-3 across to the
left of E, level with it. A row and a column together pick one of the fifteen pages; each set lights
its selection. Patch and Global pages keep separate selections.

## 11. `display_line()` and `position_button()`

ONE CENTRE LINE PER POSITION (CT): the half of the display above a knob, the knob and the button under
it are all centred on it, as on the G2. The canvas widget cannot centre its own text - it draws name
and value left-aligned from the dial's edge - so the panel hides that text (SynthLib's
`set_dial_text_hidden()`) and draws both lines itself, the value being the very string the widget
formatted (`render_param_last_value_text()`).

## 12. `panel_button()`

THE LED SAYS IT, NOT THE BUTTON (CT): every panel button is grey and carries a small LED above it, lit
for on - a position's switch, the selected page row and column, Patch Settings. Red, as the G2's are.

## 13. the colours

FROM THE MANUAL'S PICTURE OF THE PANEL (p.25), sampled at 300 dpi (CT asked for the G2's own): the
control area is a pale slate grey in a navy surround - the red is the instrument's casing, not the panel
- the displays are lime LCDs in near-black bezels, and a lit LED is red. Text printed on the panel is
the surround's navy. The knobs on the G2 also wear a ring of LEDs showing their position; not drawn yet.
