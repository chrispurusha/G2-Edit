# paramOverview.h notes

The longer comments from `paramOverview.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `PARAM_OVERVIEW_ROWS`

The Parameter Overview panel — the original editor's Tools > Parameter Overview (manual p.126).
Where the Parameter Pages panel (paramPages.c) shows ONE page's eight knobs as live widgets,
this shows ALL FIFTEEN pages' assignments at once, as a 15-row x 8-column grid of display
boxes, so a patch's whole panel layout can be read and reorganised in one view.

It is a second view onto the same gKnobArray[slot] / gGlobalKnobArray that Parameter Pages
reads, and resolves assignments through that panel's param_pages_knob_target() so the two can
never disagree about what a knob points at. Nothing here owns any data.

WHAT A BOX SHOWS, and why not a live widget: the original's overview is a grid of grey display
boxes carrying names, not controls, and 120 live param widgets would be both unreadable at this
density and a great deal of per-frame work. So a box carries the module name and the parameter
name — or the parameter's MIDI CC# when View MIDI is on, which is that button's whole purpose
in the original.

DRAG TO REORGANISE is the feature the manual leads with ("you can very quickly reorganize all
your knob assignments"): drag a box onto another box to MOVE that assignment to the new panel
position, swapping nothing and overwriting whatever was there. The original also supports
dragging a box out onto a module parameter in the patch window to CREATE an assignment; that
direction is not possible here while the panel is a full-canvas modal overlay, so creating an
assignment stays where it already was — the canvas's right-click Assign menu. See todo.md.

Opened from Settings > Parameter Overview. NO keyboard shortcut: the original uses Ctrl-L, but
the owner's standing call for this family of panels is menu-only (as with Parameter Pages,
whose Ctrl-F was implemented and then removed).
