settingsPanels.c - notes

The four settings-family panels: Synth Settings, Patch Settings, Performance Settings and Patch
Notes. Split out of graphics.c on 2026-09-16, keeping their text verbatim - these were graphics.c
§18-§21 and older notes and commit messages that name them there are stale.

WHY THEY MOVED. graphics.c is the application's GLFW render loop and is not in the plug-in's build
(do-plugin's source list is manual), so in G2 Alike every one of these panels opened - the menu
action ran, the `active` flag went true - and nothing ever drew it. CT: "Most of the plugin menu
options aren't working. Setings etc." Nothing in this file touches a window, which is what let it
move; the same reasoning split patchWrite.c out on 2026-09-09.

The panels' MOUSE and KEY handlers are in mousePanels.c, and the coordinator that decides which
panel is in front and who gets the click is floatingPanels.c.

## 1. in `midi_chan_str()`

---------------------------------------------------------------------------
Shared popup-panel chrome — every dialog-style panel (Synth/Perf/Patch
Settings, Patch Notes, Mutator) draws the same bordered box + inset title
bar + right-aligned Close button. Pulled out here so the border-inset and
close-button-position fixes only need to exist in one place.
---------------------------------------------------------------------------

## 2. `render_patch_settings_panel()`

Draws the bordered box and the title bar (inset from the border so it never paints
over the white/black border line) with white title text. Returns the full-width,
non-inset title bar rectangle — callers that need it as a drag handle (Mutator) can
hit-test against that; everyone else can ignore the return value.

## 3. `render_patch_settings_panel()`

Draws the standard "Close" button, right-aligned in the title bar at the app's
standard inset, darkened while closePressed is true. Returns its rectangle for the
caller's own hit-testing (this function does not track press state itself, since
each panel already has its own closePressed bool wired into its mouse handler).

## 4. in `render_patch_settings_panel()`

FLOATING, NOT MODAL (2026-08-20). The position comes from the panel state instead of being
recomputed as (renderW - boxW) / 2 every frame — which is what made these three impossible to
move — and there is no background overlay, so the patch stays visible and clickable underneath
and a second panel can share the screen. Same treatment the Virtual Keyboard already had, and
the reason renderW/renderH are gone from here: centring was all they were for. See SynthLib
floatingPanel.h.
