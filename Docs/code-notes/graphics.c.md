# graphics.c notes

The longer comments from `graphics.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `render_scrollbars()`

NO GLFWwindow * ARGUMENT ANY MORE, AND THERE NEVER SHOULD HAVE BEEN ONE. It took a window and read
nothing from it: the sizes come from get_render_width()/get_render_height() and the bars from the
split view. Every OTHER function here that takes a GLFWwindow * is a GLFW callback, whose signature
GLFW dictates — this was the only one advertising a dependency it did not have, which is
vst3/plugin-gui-notes.md's third observation. Worth removing rather than shrugging at: a signature
like that is what made the earlier extractions look daunting when they turned out to be mechanical,
and the plug-in already draws its own bars through render_pane_scrollbars().

## 2. in `render_scrollbars()`

The tracks and thumbs belong to the split view now — one vertical bar per pane and a
horizontal one for the focused pane, all with proportional thumbs. The filler square that used
to sit where the two bars met has gone with them: both bars now stop the same distance clear
of the corner, so there is no junction left to cover.

## 3. `gAppTheme`

The canvas origin is derived from ONE value - the theme's topBarHeight, which utilsGraphics.c
turns into the module band's top and height. So growing the topbar for the module palette is a
single re-application of the theme rather than a change anywhere the canvas is drawn.

The theme is kept here rather than read back out of SynthLib because configure_synthlib_theme()
takes a whole struct by value and there is nothing to read it back with; keeping our own copy is
simpler than adding an accessor to the submodule for one field.

## 4. in `init_graphics()`

Things that must be in place before the first frame but need no window. They stay here rather
than moving into SynthLib because every one of them is this application's own business: what
its patch categories are, that it opens with a single pane, what a wake-up from the USB thread
should do.

The bank browser's Category mode sorts its groups by name, which buries the two categories the
player actually assigns themselves at the bottom under U. Pin them to the top instead. Read
out of patchTypeStrMap rather than spelt again here, so renaming a category cannot silently
unpin it. Set once: it holds for every browser this app opens, all of which use this same map.

## 5. in `init_graphics()`

THE WINDOW, ITS SCALE AND ITS INPUT WIRING ARE SYNTHLIB'S NOW — see synthlibWindow.h. What
used to be ~60 lines here, and the same ~60 lines in SynthEdit and EmuUtility, is a config and
a callback table. The six callbacks that only ever called back into SynthLib (error,
framebuffer size, content scale, window size, window position, window close) went with it;
the ones below are the ones that reach into this app's own domain.

The dial mode is set through the config rather than left to default, because this app is the
odd one out: SynthLib defaults to eDialModeVertical to match EmuUtility/SynthEdit, and this
one wants rotary. It is applied before setup_main_menu()'s load_saved_settings() (misc.mm)
runs, so a real saved value still wins.
The popup coordinator needs to know this app's own panels and its menu bar before the first
frame — see synthlibPopups.h. Through a function because the table names render functions
defined further down this file.

## 6. in `init_graphics()`

While a dial drag hides the pointer, its reported position is a relative-delta
accumulator - it drifts, and anything that highlights "what is under the mouse" lights
the wrong thing. CT: "If I drag a dial, and hidden mouse coincides with top menu, top
menu item lights!" paramOverlay.c already suppressed itself this way; the shared menu bar
could not, having no way to ask. Now it can.

## 7. `remember_file_path()`

Remembers where the patch (or performance) currently on screen came from, so File > Save can
write straight back to it. Called after an open and after a save — the same rule every editor
uses: the last file you opened or saved to is the one Save overwrites. Recorded per slot,
because each of the four can have come from a different file; perf files own all four at once
and so get a single path of their own.

## 8. in `remember_file_path()`

Read the slot ONCE. COPY_STRING expands its destination three times — the strncpy, the
sizeof, and the terminator write — and gSlot is atomic, so writing gSavedPatchPath[gSlot]
directly is three separate loads. A slot change part way through would terminate a different
buffer than the one just copied into.

## 9. in `remember_file_path()`

File > Save hands back the very buffer it is about to write into: the path being saved to IS
the remembered path (see eRspSaveToCurrentPath). strncpy's arguments are restrict-qualified,
so copying a buffer onto itself is undefined behaviour rather than a harmless no-op, and it
crashed here intermittently. Save As never hit it because the panel supplies a fresh buffer.

## 10. in `on_file_opened()`

── Offline-edit conflict on reconnect ──────────────────────────────────────

The USB thread has found edits made while the G2 was away and parked itself until the user
decides whose copy wins. Recovery files are already on disk by the time the dialog appears, so
every answer here — including Escape — is safe.

Non-zero only between choosing "Save As..." and that save finishing, so on_file_saved() knows to
finish resolving the conflict afterwards. The dialog is modal and the USB thread is parked, so
no second conflict can start meanwhile.

## 11. in `on_file_saved()`

A SAVE PUTS THE FILE IN File > Open Recent, exactly as an open does. It only used to go in
on the way IN, so a patch written with Save As — the one you are most likely to want back
— was the one file the menu never listed. Both save routes arrive here (the browser's
callback, and File > Save going straight to the remembered path), so this covers them
both; recent_files_add() moves an existing entry to the front and returns early if it is
already there, so saving repeatedly costs nothing.

## 12. `on_store_confirmed()`

Fires after the user has seen the overwrite warning built from a gStorePeekComplete result
(below) and clicked "Store...". The target is whatever peek_store_target() just recorded in
gStorePeekBank/gStorePeekLocation — no separate captured-context state needed here since nothing
can change those globals between the peek landing and this callback firing (both happen on the
main thread, and the user can't trigger a second Store attempt while this alert is up).

## 13. `on_synth_restore_confirmed()`

Fires once the user has confirmed past the file-found warning built from a
gSynthRestorePeekComplete result (below). Sends eMsgCmdApplySynthSettingsRestore with no
payload — the parsed settings are already staged on the USB thread
(sSynthSettingsRestoreStaged), set by peek_synth_settings_restore().

## 14. `DEVICE_OP_TIMEOUT_MS`

Busy state for in-flight whole-slot device ops (load/save/new patch). Set when the op is enqueued,
cleared when its completion response is drained off gToGuiThread. See reverse-queue-design.md.
get_time_ms() of the oldest in-flight op, for the safety timeout below. IN MILLISECONDS AND ON
get_time_ms()'s CLOCK, both of which matter: this was set from get_time_ms() / 1000.0 and compared
against glfwGetTime(), which are two different origins - CLOCK_MONOTONIC counts from boot and
glfwGetTime() from library init. On a machine up for a day the difference is about 86400, so the
subtraction was hugely negative and the timeout could never fire. The busy overlay then had no way
out at all if its completion response never came, which is what left "New Patch..." on screen
until the editor was force quit (CT, 2026-09-07).

## 15. in `check_action_flags()`

Drain the reverse (USB->UI) response queue — see reverse-queue-design.md. One response per frame:
if a modal alert is already up, leave the queue untouched so it isn't clobbered (we'll drain the
next once it's dismissed); if more remain after handling one, self-wake so the next frame
continues rather than blocking in glfwWaitEvents.

## 16. in `check_action_flags()`

Safety net: if a device op's completion response never arrives - the G2 disconnected between
the enqueue and the processing, or the USB thread never dequeued it at all because it was busy
trying to reconnect - don't leave the GUI locked forever. Whole-slot ops complete in well under
a second in practice. Both sides of this comparison must be on get_time_ms()'s clock; see the
note on sDeviceOpStartTime for what happened when they were not.

## 17. in `check_action_flags()`

Completion alerts (bank backup/restore/store/delete/load/synth) and peek→confirm prompts now all
arrive on the reverse queue (see the drain switch above) — their poll flags were retired. The
gBank*IsPerf / gBank*IsEverything flags stay (the progress overlays read them); the gStore/Delete/
Load/SynthRestorePeek* data globals stay too (the drain reads them to build the confirm dialog).

## 18. in `midi_chan_str()`

---------------------------------------------------------------------------
Shared popup-panel chrome — every dialog-style panel (Synth/Perf/Patch
Settings, Patch Notes, Mutator) draws the same bordered box + inset title
bar + right-aligned Close button. Pulled out here so the border-inset and
close-button-position fixes only need to exist in one place.
---------------------------------------------------------------------------

## 19. `render_patch_settings_panel()`

Draws the bordered box and the title bar (inset from the border so it never paints
over the white/black border line) with white title text. Returns the full-width,
non-inset title bar rectangle — callers that need it as a drag handle (Mutator) can
hit-test against that; everyone else can ignore the return value.

## 20. `render_patch_settings_panel()`

Draws the standard "Close" button, right-aligned in the title bar at the app's
standard inset, darkened while closePressed is true. Returns its rectangle for the
caller's own hit-testing (this function does not track press state itself, since
each panel already has its own closePressed bool wired into its mouse handler).

## 21. in `render_patch_settings_panel()`

FLOATING, NOT MODAL (2026-08-20). The position comes from the panel state instead of being
recomputed as (renderW - boxW) / 2 every frame — which is what made these three impossible to
move — and there is no background overlay, so the patch stays visible and clickable underneath
and a second panel can share the screen. Same treatment the Virtual Keyboard already had, and
the reason renderW/renderH are gone from here: centring was all they were for. See SynthLib
floatingPanel.h.

## 22. in `note_editor_cursor_from_click()`

── TEMPORARY DEBUG AID — mouse crosshair ───────────────────────────────────
Draws full-width/full-height lines through the cursor plus a numeric readout,
for validating button hit points against their registered rectangles.

Deliberately uses get_global_gui_scaled_mouse_coord() — the SAME call the
click handlers use — so the number shown is literally the coordinate that
gets compared against each rectangle, not an independently-derived one that
could agree by luck while the real dispatch path disagrees.

NOTE: this is mainArea (unscrolled) space. Top bar, menu bar and panel
buttons live here, so their hit rects can be read off directly. Module-area
elements are scroll/zoom-adjusted afterwards, so for those the crosshair
shows the pre-adjustment cursor position, not the module-local one.

Debug builds only (ENABLE_MOUSE_CROSSHAIR lives in defs.h), and OFF until F9
is pressed — the lines sit above even the modal alert, so leaving it on by
default would be intrusive.

## 23. `gFloatingPanels`

THE FLOATING PANELS, ONCE. This list existed THREE TIMES — here for drawing, and twice in
mouseHandle.c, once for clicks and once for keys — each copy filling a different column of the
same struct and each carrying a comment warning that it had to agree with the others. It is the
duplication the struct was introduced to remove, reintroduced one channel at a time.

Sorted in place on every walk. That is not wasteful and it is not a cache: floating_panel_sort()
orders by last-raised, which a click can change between one walk and the next, so asking again is
the only way to be right.

## 24. in `render_mouse_crosshair()`

Joined the floating panels on 2026-08-20, having been fixed, window-centred panels drawn over
a dimmed canvas. They are the same KIND of thing as the settings panels above and now behave
like them: draggable by the title bar, raised by a click, closed by their button or Escape.
Patch Notes in particular used to close on any click that missed its text area, so pressing
its title bar — the drag handle everywhere else — shut it.

Patch Notes takes no key entry: its Escape lives in key_event() beside the text editing it
belongs with, and is reached after this dispatch.

## 25. `panel_press_takes_the_keyboard()`

PRESSING A PANEL ABANDONS A NAME EDIT SOMEWHERE ELSE. Clicking back onto the notes editor while
the topbar patch name was being edited used to leave that edit running, so the keyboard stayed
with the name field and the panel just clicked took not one character.

Abandon, not commit: stop_*_name_editing() memsets the edit state, discarding the half-typed
buffer and leaving the real name untouched. That is already the meaning everywhere else — a click
on the canvas does exactly this — and it is what makes clicking away safe, rather than a way to
half-rename something by accident.

The SYNTH name is exempt when the press lands on the Synth Settings panel, because that is the
panel the edit belongs to: a click elsewhere within its own panel is that panel's business, and
its handler already ends the edit on the release.

## 26. `raise_newly_shown_panels()`

SHOWING A PANEL BRINGS IT TO THE FRONT. Without this a panel opened from a menu keeps whatever
order it last had — zero, if it has never been clicked — so opening the notes editor over an
already-open Virtual Keyboard left the two tied, and which one ended up in front was decided by
their position in the table rather than by which was just asked for.

Keyed by POINTER, not by index: floating_panel_sort() reorders the table in place, so entry i is
a different panel from one frame to the next and a parallel array indexed by i would compare the
wrong panels. Raising on the transition rather than on first placement also covers REOPENING,
which keeps its old position and so never looked new to floating_panel_place().

## 27. in `floating_panels_render()`

Panels stay off the canvas scrollbars, which run along the bottom and the right. Overlapping
the TOP bar is deliberately still allowed — a panel has to start somewhere, and the bar is not
something you scroll — but a panel lying over a scrollbar reads as a mistake rather than as a
panel in front. Set per frame so a window resize cannot leave it stale.

## 28. in `floating_panels_key()`

NULL-CHECKED, WHICH IT WAS NOT: every entry had a key handler until Patch Notes joined the
table with none — its Escape belongs in key_event() beside the text editing — and the very
first keystroke typed into the notes editor called through a null pointer. The columns of
this table are independently optional, so every walk over it has to say so.

## 29. `floating_panel_is_frontmost()`

DOES THIS PANEL OWN THE KEYBOARD? It does if it is the frontmost panel that is actually shown.

There was no answer to this question before 2026-08-20, and the Patch Notes editor was the one
that needed it: its typing is handled in key_event()/char_event() rather than through the panel
key walk, gated on nothing but "is the notes editor open". So an open notes editor swallowed the
keyboard from wherever you were actually looking — with Synth Settings in front of it, the synth
name could not be typed into at all. Reported 2026-08-20.

Frontmost is not a new concept: floating_panel_raise() has maintained it since panels could
overlap, and a click on a panel already raises it. This just asks it out loud.

## 30. in `floating_panel_is_frontmost()`

"NOT BEHIND" RATHER THAN "IN FRONT OF", so that equal orders resolve the way the DRAWING
resolves them. floating_panel_sort() is stable, so panels sharing an order keep table
order and the LAST of them is drawn on top; testing strictly-in-front here would have
picked the FIRST, and the panel you were looking at would not have been the one taking
the keys. Ties are rare now that showing a panel raises it (see floating_panels_render),
but "rare" is how the last few of these bugs got in.

## 31. `floating_panels_drag()`

THE POINTER IS OVER A PANEL — so the canvas underneath must not react to the motion.

This is what the hover path needed and could not ask. cursor_pos() named the Mutator in an if and
suppressed hover only for that one, so moving the pointer across Synth Settings (or any of the
other five) ran the canvas hover detection underneath it: connectors the panel was covering lit
up and the cable-hiding that goes with a connector hover triggered, over a panel. Reported
2026-08-20 against Synth Settings.

Visibility is checked, not just the rectangle: a closed panel keeps its rect so it can reopen
where it was left, and testing that alone would suppress hover over a strip of empty canvas.

## 32. `floating_panels_drag()`

A panel being MOVED owns the pointer until it is released. This was a fourth hand-written copy of
the list — the draw, click and key copies are gone; this one had already lost the Mutator (which
is fine, see below) and had a comment recording that the Help panel was once missed off it
entirely, so it could be raised and closed but never moved.

The Mutator is harmless to include even though cursor_pos() handles its move separately: that
branch returns before this is reached whenever the Mutator is actually dragging, so the entry can
only ever be a no-op here.

## 33. `floating_panels_scroll()`

The wheel over a panel must not scroll the canvas underneath it — the hover bug again, on the one
channel that cannot be asked where the pointer is: the coordinator's scroll callback carries only
a delta, so the position is fetched here exactly as scroll_event() fetches it.

Swallowing rather than forwarding is deliberate. No panel scrolls its own content today; if one
ever does, it gains a scroll handler and this stays as the backstop for the rest.

## 34. `gAppPopups`

The application's own popups, registered into SynthLib's ordering (synthlibPopups.h) so that this
app's panels and the library's cannot disagree about who is in front.

THE LAYERS ARE NOW THE WHOLE PIPELINE, not just the render order. Every entry below carries its
mouse and key handlers, so this table is the answer to "who gets the click, and after whom" — a
question that used to be answered by the ORDER OF THIRTEEN ifs in mouseHandle.c, restated a second
time by the order of eight calls in render_frame(), with nothing anywhere able to check that the
two agreed. They did not: the three panels below sat BELOW the floating panels when drawn and
ABOVE them when clicked, so a floating panel lying over the Parameter Pages panel was drawn in
front and took no clicks. That class of defect — paint order and hit order disagreeing — is the
one this app has hit repeatedly (the context menu over the scrollbars, the VA module under the FX
pane), and here it is now impossible to write down: ONE layer decides both.

The numbers still reproduce exactly what the render calls did, which is what makes this a
re-expression rather than a redesign, with one deliberate exception noted at patchNotes. Written
relative to SynthLib's constants so the intent survives someone renumbering the library's layers.

## 35. in `floating_panels_scroll()`

ABOVE the context menu, because that is where it is DRAWN. Its clicks used to be offered after
the menu's, i.e. below — the one place where making input follow paint changes behaviour. A
menu raised over the notes editor is drawn underneath it, so it could previously be clicked
while invisible.

## 36. in `render_frame()`

READ-LOCKED FOR THE WHOLE FRAME. A frame walks the module and cable tables from end to end,
and until now the USB thread could rewrite them halfway through - a patch arriving mid-render
is exactly the case, and it rewrites structure rather than a value. See dataBase.c.

The whole frame rather than each walk: two walks either side of an unlocked gap would each be
internally consistent and disagree with each other, which is a subtler version of the same
bug. It also means sound_engine_update_from_patch() below must NOT take the lock itself.

## 37. in `render_frame()`

The sound engine reads the selected module's parameters from here. A redraw is exactly the
event it needs — every parameter change and every selection change causes one, whether it came
from the mouse or from the USB thread — so it needs no polling of its own and this costs
nothing when the engine is switched off.

## 38. in `render_frame()`

Draw each module pane in turn. render_modules()/render_cables() both read gLocation at their
top, so the Location is set around each pass in the same "mode rather than argument" style
the panes themselves use — which is why neither function needed a new parameter. gLocation is
put back to the focused pane's Location afterwards, since that is what every other reader in
the app means by it.

## 39. in `render_frame()`

The BAR itself stays here, ahead of the floating panels, because it is chrome they float above
— a panel is allowed to overlap it, and drawing the bar afterwards would put it over the panel
while the panel still took the click. Only its hover tick and its dropdown (which is the
context menu) are the coordinator's. See synthlibPopups.h.

## 40. in `render_frame()`

ONE CALL, AND THE ORDER IS DATA. This used to be twelve calls whose sequence WAS the z-order —
the three fixed panels, the seven floating ones and SynthLib's own — correct, unstated, and
one careless insertion away from being wrong. Every one of them is now a row in gAppPopups
above, ranked by the same layer that decides which of them gets the click. See
synthlibPopups.h.

## 41. in `do_graphics_loop()`

Every registered popup's hover/dwell update, in one call. Polled every tick rather than only
on cursor movement, so a hover-dwell timer elapses while the mouse sits still — and the
host can no longer forget one, which is a bug that has shipped twice in this family of
apps. See synthlibPopups.h.

## 42. in `do_graphics_loop()`

THE ACTIVITY LAMPS ARE THE ONE THING ON SCREEN THAT CHANGES WITH NO EVENT BEHIND IT. A
lamp goes out because COMMS_LAMP_MS elapsed, not because anything happened, and the loop
below only renders when a redraw has been REQUESTED — waking on a timeout is not the same
as asking for a frame. So the Rx lamp used to change state only when some unrelated event
(a mouse move, an LED message) happened to request a redraw for its own reasons. Ask for
exactly the frames the lamps need: one when either changes, none at all while they sit
still. Lighting one is handled at the other end, in usbComms.c's note_usb_activity().

## 43. in `do_graphics_loop()`

A METER THAT MOVED IS A REASON TO DRAW. The sound engine publishes its meters and LEDs
from the audio thread into atomic arrays and cannot ask for a frame itself without a
syscall per block, so it raises a flag and this consumes it - the same shape as the comms
lamps just above. Without it the arrays were updating perfectly and nothing was looking:
the meters moved only while the mouse did (CT, 2026-09-08).

## 44. in `do_graphics_loop()`

A Tx/Rx lamp is lit and has to be drawn going out when traffic stops. Coming back on
a timeout costs one redraw every 100ms; the topbar used to ask for one every FRAME
while a lamp was lit, which with the G2's continuous interrupt stream meant 60 fps
for the whole session. Lighting a lamp needs no help from here — the USB thread wakes
the loop when data arrives.
