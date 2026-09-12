# appMenuBar.c notes

The longer comments from `appMenuBar.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `action_open_patch()`

Real actions land in these six open_*_menu() functions as misc.mm's Cocoa
menu items get ported over (File first, then
Settings/Backup/Restore/Controls/View). The bar itself, its layout, and
click/hover routing are already real and final.

## 2. in `action_store_to_bank()`

WHETHER A G2 CAN EVER BE ATTACHED, as distinct from whether one is attached right now.

The application greys its bank and device entries while offline, because going online is a thing
that can happen and a greyed entry says "this exists". The VST3 plug-in has no USB layer at all,
so for it those entries are not disabled — they are meaningless, and a permanently greyed row is
worse than no row. Default true, so the application is unchanged.

## 3. in `open_file_menu()`

OPEN RECENT, as a flyout. Both arrays are static because the menu engine keeps the pointers it
is given and reads them while the menu is open — a stack array would be gone by then. So are
the labels: tMenuItem.label is a borrowed const char *, and a basename points into the stored
path, which outlives the menu.

A MISSING FILE IS SHOWN GREYED, not hidden. Removing it silently would make a patch on an
unplugged drive vanish from the list for good; greying says "this is still yours, it is just
not reachable right now", which is what the platform menus do.

## 4. in `open_controls_menu()`

Labels are fixed strings with a checkmark prefix baked in (tMenuItem has no separate
"checked" flag) — point each entry's label at the checked or unchecked variant depending
on the current dial mode, rather than mutating the string in place. Plain "*" rather than a
Unicode checkmark glyph: the app's glyph atlas only preloads ASCII (MAX_GLYPH_CHAR == 127 in
synthlibDefs.h), so anything above that silently fails to render.

## 5. `action_overlay_mode()`

The overlay views. Selecting the mode already showing turns it off again, so the entries behave
as a radio group with a toggle on the active one.

NOTE the argument is the item's POSITION in the menu, not the payload - contextMenu.c calls
action(index) and leaves the action to fetch its own value out of
gContextMenu.items[index].param, the same way every action in menus.c does.

## 6. in `open_view_menu()`

3 zoom entries + Zoom to Fit + one per overlay view + the NULL terminator. It was exactly full
at 9 before Zoom to Fit was added; overflowing one of these arrays does not fail visibly, it
quietly writes over whatever static follows it (see the Experimental menu's note in todo.md).
3 zoom + Zoom to Fit + the palette toggle + one per overlay view + the NULL terminator.
GROWN FROM 10 when the palette toggle was added: it was exactly full, and overflowing one of
these arrays does not fail visibly - it quietly writes over whatever static follows it.

## 7. in `action_toggle_mutator()`

THE UID SNAPSHOT TAKEN WHEN THE MENU WAS BUILT, one per row. The row a user clicks means "the
device whose name I just read", and only the UID can still say which device that was: the list is
re-enumerated on every audio_output_device_count(), so by the time this action runs, row 3 may be a
different device — or the list may be shorter than the menu still on screen.

## 8. in `action_select_audio_device()`

Said out loud rather than swallowed. A device can be listed and still refuse to open — in
exclusive use by another application, or unable to offer the rate asked of it — and the
previous version discarded that, leaving a ticked device making no sound and no way to tell
why. It can also have been unplugged while this very menu was open.

## 9. in `open_tools_menu()`

Label reflects current state the same way Controls' dial-mode items do (checkmark-style
"* " prefix — see open_controls_menu — isn't used here since the item's own name already
says what it does; a "Close Mutator" vs "Open Mutator" label reads clearer for a single
toggle than a checkmark would).

## 10. in `open_experimental_menu()`

Sized with room to spare, and deliberately generous: the entries here are conditional — the
status line only appears with the engine running, the output lists only on a multi-channel
device — so the real count varies, and overrunning this array corrupts whatever static
follows it. It did exactly that, blanking the device flyout only while the engine was on.
One slot per entry with room to spare. This overflowed once already, at 8, and the symptom
was another menu's array being quietly overwritten rather than anything obviously wrong here.

## 11. in `open_experimental_menu()`

ALL SOURCES AT ONCE, which is what a desk with a keyboard, a control surface and a DAW port
on it actually wants, and it takes anything plugged in LATER too — the setup-changed
notification reconnects. It was already the startup state, with no way back to it once a
single source had been chosen.
