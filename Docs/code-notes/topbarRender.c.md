# topbarRender.c notes

The longer comments from `topbarRender.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

The top bar, moved out of graphics.c so the VST3 plug-in draws the SAME ONE.

It was never platform-bound — every call in it is drawing or state — it simply lived in the file
that owns the window. The plug-in first got a hand-written bar carrying a subset of the controls;
that was a mistake. The point is for the plug-in to look like the editor, and a second
implementation of the same bar can only ever drift away from it.

The controls that describe hardware still draw here, and should: "Offline" is the truthful state
for a plug-in, the TX/RX indicators stay dark because gUsbTxTime/gUsbRxTime are never set, and the
slot buttons show which slot the patch occupies. Nothing has to be hidden to be honest.

## 2. in `render_top_bar()`

A variation button carries two states that vary independently — SELECTED (exactly one, and
already in gTopbarControls[].colour via set_exclusive_button_highlight) and LINKED (any
number of them, see variation_is_linked() in globalVars.h). One fill colour cannot say
both, so a button that is both is split across the middle: green above for where you are,
orange below for what the edit will also reach. Linked-but-not-selected is plain orange,
and the press highlight still wins over either, being momentary feedback for this click.
The palette toggle shows its own state rather than only reacting to the click, the way
the slot and variation buttons do - it is the only way to tell from the topbar whether
the band below is open, and the band itself disappears when it is not.

## 3. in `render_top_bar()`

NO WAKE HERE. This used to call wake_glfw() on every frame either lamp was lit — and since
the G2's interrupt stream never pauses for as long as the lamp's 100ms window, "lit" means
"for the rest of the session". The application therefore ran at 60 fps for as long as a G2
was attached, measured against 1 fps with no device, and it was doing so during the initial
pull, competing with the USB thread for the very seconds being waited on.

It is not needed to light a lamp: the USB thread already wakes the render loop when data
arrives, in two dozen places. It was only ever needed to put one OUT once traffic stops, and
do_graphics_loop() now waits with a timeout for that instead — see comms_lamps_lit().
Cable colour visibility toggles — 6 small squares
uint32_t hiddenMask = gHiddenCableMask;

## 4. `comms_lamp_state()`

Which activity lamps are lit right now, as bit 0 = Tx, bit 1 = Rx. The Tx/Rx lamps are the only
thing on screen that changes with no event behind it — a lamp goes out because time passed, not
because anything happened — so do_graphics_loop() compares this against what it last drew and
asks for a frame when it differs. Per lamp rather than "either", so Tx going out while Rx stays
lit is still a change.
