# paramPages.h notes

The longer comments from `paramPages.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tKnobTarget`

The Parameter Pages panel - the on-screen stand-in for the G2 front panel's PARAMETER PAGES
and their eight ASSIGNABLE KNOBS (manual p.19 and p.89-91).

A patch has 120 knob assignments, laid out as 8 knobs x 15 pages, and the 15 pages are
themselves a matrix 3 wide (banks 1-3) by 5 high (rows A-E) - so a page is named A1, D2 and so
on. There are another 120 GLOBAL assignments, the same shape but shared by all four Slots and
each recording which Slot its module lives in; one toggle switches the panel between the two.

Nothing here owns any data: the assignments are gKnobArray[slot] / gGlobalKnobArray, already
parsed from the patch by protocol.c and already round-tripping to file and device, and the
values are the module params themselves. The panel is a second view onto them, so an edit made
here IS an edit to the module parameter - it goes out to the G2 and lands in the undo history
exactly as if it had been made on the patch canvas. The reverse direction is free for the same
reason: turning the knob on the hardware sends a param change, usbComms.c writes it into the
same module param, and the panel redraws from it.

Knob index -> page mapping is page = index / 24, bank = (index % 24) / 8, position = index % 8,
matching the assign/deassign menus in menus.c and the canvas hover overlay in moduleGraphics.c.
NEEDS A HARDWARE CONFIRM: that A1,A2,A3,B1... really is the order the G2 numbers them in, and
not column-major.

## 2. `tKnobTarget`

One knob assignment resolved to the thing it actually drives. Shared with the Parameter
Overview panel (paramOverview.c), which needs the same resolution over all 120 assignments at
once — hence public rather than private to paramPages.c. `assigned` is false both for an empty
position and for an assignment that no longer resolves: the module deleted since, or a param
index the module's type does not have.
