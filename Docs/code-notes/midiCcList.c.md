# midiCcList.c notes

The longer comments from `midiCcList.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `row_text()`

One row's text. The module and parameter names come from paramPages.h so this panel, the
Parameter Pages and the Parameter Overview can never disagree about what a parameter is called —
a patch-given name wins over the paramLocationList one, which is the precedence the canvas itself
applies.

## 2. in `render_midi_cc_list_panel()`

24.0, as every other panel in the app uses. Derived from the text height it USED to be
(STANDARD_BUTTON_TEXT_HEIGHT + 8.0), which came to 20.0 — and the close button that
draw_panel_close_button() puts in the banner is inset 6.0 from the panel top and is 14.0
square, so it ended exactly ON the bar's bottom edge and hung out of it. The button's geometry
is measured from the PANEL's corner and never sees the title height, so a bar shorter than
20.0 has nowhere to put it; this was the only panel not using the common value.
