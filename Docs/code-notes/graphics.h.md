# graphics.h notes

The longer comments from `graphics.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `notify_full_patch_change()`

Re-derives the canvas origin from the topbar plus whatever the module palette is currently
taking. Called when the palette opens or closes; nothing else needs to know the bar changed.
Re-reads the whole patch into the UI's own derived state after the database has been replaced
wholesale. Normally driven from the USB thread's callback; menuActions.c calls it directly for
the offline New Patch, which the USB thread never sees.

## 2. `floating_panel_is_frontmost()`

The floating panels, asked as a group. Their one table lives in graphics.c beside the popup
registration; these are the two questions the cursor-position handler has to ask it. Clicks and
keys do NOT need an entry point here — those go through the popup coordinator, which walks the
same table. See synthlibPopups.h and floatingPanel.h.
Is this panel the frontmost one currently shown? What "has keyboard focus" means for a panel
whose text handling lives outside the panel key walk — see the note in graphics.c.
