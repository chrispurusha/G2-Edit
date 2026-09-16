floatingPanels.c - notes

The coordinator over the eleven floating panels: which one is drawn on top, which one gets the
click, which one gets the key. Split out of graphics.c on 2026-09-16, keeping its text verbatim -
these were graphics.c §23-§33, and older notes naming them there are stale.

WHY IT MOVED. It sat in the application's render loop, which the plug-in does not compile, so G2
Alike drew none of these panels and routed nothing to them: every Settings and Tools menu entry
opened a panel that could not appear (CT, 2026-09-16). Both hosts now register the four callbacks
in floatingPanels.h in their own popup table - graphics.c's gAppPopups and the plug-in's own - so
each still chooses the LAYER the group sits at, which is the one part that is genuinely per host.

The panels' own renderers live in their own files; the four settings-family ones are in
settingsPanels.c, and their mouse and key handlers in mousePanels.c.

## 1. `gFloatingPanels`

THE FLOATING PANELS, ONCE. This list existed THREE TIMES — here for drawing, and twice in
mouseHandle.c, once for clicks and once for keys — each copy filling a different column of the
same struct and each carrying a comment warning that it had to agree with the others. It is the
duplication the struct was introduced to remove, reintroduced one channel at a time.

Sorted in place on every walk. That is not wasteful and it is not a cache: floating_panel_sort()
orders by last-raised, which a click can change between one walk and the next, so asking again is
the only way to be right.

## 2. in `gFloatingPanels`

Joined the floating panels on 2026-08-20, having been fixed, window-centred panels drawn over
a dimmed canvas. They are the same KIND of thing as the settings panels above and now behave
like them: draggable by the title bar, raised by a click, closed by their button or Escape.
Patch Notes in particular used to close on any click that missed its text area, so pressing
its title bar — the drag handle everywhere else — shut it.

Patch Notes takes no key entry: its Escape lives in key_event() beside the text editing it
belongs with, and is reached after this dispatch.

## 3. `panel_press_takes_the_keyboard()`

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

## 4. `raise_newly_shown_panels()`

SHOWING A PANEL BRINGS IT TO THE FRONT. Without this a panel opened from a menu keeps whatever
order it last had — zero, if it has never been clicked — so opening the notes editor over an
already-open Virtual Keyboard left the two tied, and which one ended up in front was decided by
their position in the table rather than by which was just asked for.

Keyed by POINTER, not by index: floating_panel_sort() reorders the table in place, so entry i is
a different panel from one frame to the next and a parallel array indexed by i would compare the
wrong panels. Raising on the transition rather than on first placement also covers REOPENING,
which keeps its old position and so never looked new to floating_panel_place().

## 5. in `floating_panels_render()`

Panels stay off the canvas scrollbars, which run along the bottom and the right. Overlapping
the TOP bar is deliberately still allowed — a panel has to start somewhere, and the bar is not
something you scroll — but a panel lying over a scrollbar reads as a mistake rather than as a
panel in front. Set per frame so a window resize cannot leave it stale.

## 6. in `floating_panels_key()`

NULL-CHECKED, WHICH IT WAS NOT: every entry had a key handler until Patch Notes joined the
table with none — its Escape belongs in key_event() beside the text editing — and the very
first keystroke typed into the notes editor called through a null pointer. The columns of
this table are independently optional, so every walk over it has to say so.

## 7. `floating_panel_is_frontmost()`

DOES THIS PANEL OWN THE KEYBOARD? It does if it is the frontmost panel that is actually shown.

There was no answer to this question before 2026-08-20, and the Patch Notes editor was the one
that needed it: its typing is handled in key_event()/char_event() rather than through the panel
key walk, gated on nothing but "is the notes editor open". So an open notes editor swallowed the
keyboard from wherever you were actually looking — with Synth Settings in front of it, the synth
name could not be typed into at all. Reported 2026-08-20.

Frontmost is not a new concept: floating_panel_raise() has maintained it since panels could
overlap, and a click on a panel already raises it. This just asks it out loud.

## 8. in `floating_panel_is_frontmost()`

"NOT BEHIND" RATHER THAN "IN FRONT OF", so that equal orders resolve the way the DRAWING
resolves them. floating_panel_sort() is stable, so panels sharing an order keep table
order and the LAST of them is drawn on top; testing strictly-in-front here would have
picked the FIRST, and the panel you were looking at would not have been the one taking
the keys. Ties are rare now that showing a panel raises it (see floating_panels_render),
but "rare" is how the last few of these bugs got in.

## 9. `floating_panels_drag()`

THE POINTER IS OVER A PANEL — so the canvas underneath must not react to the motion.

This is what the hover path needed and could not ask. cursor_pos() named the Mutator in an if and
suppressed hover only for that one, so moving the pointer across Synth Settings (or any of the
other five) ran the canvas hover detection underneath it: connectors the panel was covering lit
up and the cable-hiding that goes with a connector hover triggered, over a panel. Reported
2026-08-20 against Synth Settings.

Visibility is checked, not just the rectangle: a closed panel keeps its rect so it can reopen
where it was left, and testing that alone would suppress hover over a strip of empty canvas.

## 10. `floating_panels_drag()`

A panel being MOVED owns the pointer until it is released. This was a fourth hand-written copy of
the list — the draw, click and key copies are gone; this one had already lost the Mutator (which
is fine, see below) and had a comment recording that the Help panel was once missed off it
entirely, so it could be raised and closed but never moved.

The Mutator is harmless to include even though cursor_pos() handles its move separately: that
branch returns before this is reached whenever the Mutator is actually dragging, so the entry can
only ever be a no-op here.

## 11. `floating_panels_scroll()`

The wheel over a panel must not scroll the canvas underneath it — the hover bug again, on the one
channel that cannot be asked where the pointer is: the coordinator's scroll callback carries only
a delta, so the position is fetched here exactly as scroll_event() fetches it.

Swallowing rather than forwarding is deliberate. No panel scrolls its own content today; if one
ever does, it gains a scroll handler and this stays as the backstop for the rest.
