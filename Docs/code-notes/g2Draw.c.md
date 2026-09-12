# g2Draw.c notes

The longer comments from `g2Draw.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `GL_SILENCE_DEPRECATION`

What gets drawn into the plug-in's OpenGL surface — plain C, no Cocoa.

THIS NOW USES THE APPLICATION'S OWN RENDERER. Everything below draws through SynthLib's
utilsGraphics.c — the same render_rectangle(), render_text() and set_rgb_colour() the editor
canvas is built from — rather than through raw GL calls of its own. That is the point of the
exercise: the drawing code was never the part tied to GLFW, and this file is the evidence.

The split from g2View.m matters more than the contents. That file owns the NSOpenGLView, the
context and the host's resize notifications; this one owns pixels and knows nothing about who is
hosting it. It is C rather than Objective-C because nothing here needs a runtime.

## 2. file scope

defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
behind the top bar and with the margin between them swallowed.

## 3. `g2_remember_file_path()`

Remembers where the patch on screen came from, so File > Save can write straight back to it - and
so the host stores it with the project (g2_get_state()). A performance's path is remembered by the
save itself (g2_on_file_saved()) and by the loader. Self-copy is guarded for the same reason it is
in the application's remember_file_path() (graphics.c): File > Save hands this the very buffer it
is about to write into, and COPY_STRING expands its destination three times.

THE SELECTED SLOT, not slot 0: an instance holds all four slots now (globalVars.h's document), and
the editor's A-D buttons choose between them as the application's do.

## 4. `g2_on_file_saved()`

The other half. The application's on_file_saved() (graphics.c) chooses between serialising on the
USB thread and writing here, according to whether a G2 is connected; there is never one here, so
only the offline branch survives. The writer itself is shared — patchWrite.c was split out of
graphics.c so this file could reach it.

## 5. `apply_top_bar_height()`

The palette opens by making the topbar taller, and the canvas origin is derived from exactly one
value - so this is the whole of it. The application's version lives in graphics.c and is not in
this build; the arithmetic is the same but the base is NOT, because the plug-in reserves a
smaller band than the application's slot-and-clock bar.

## 6. in `g2_draw_init()`

The canvas starts BELOW the menu bar and the reserved topbar band. topBarHeight is how the
renderer is told that, and it is what keeps modules from being drawn underneath them.
NOT the application's value: its bar carries slot, performance and clock controls that a
plug-in has no use for, so the reserved band here is smaller. Reserving it now means adding
the topbar's controls later does not shift the whole patch down.

## 7. in `g2_draw_init()`

A brand-new empty patch FIRST. gPatchDescr is otherwise all zeroes, and the pane divider's
position is patch data (gPatchDescr[].barPosition) — zero meaning "Voice Area takes no
height", which pinned the divider to the top of an empty window. init_patch() sets the same
300 the application uses for a new patch, deliberately showing both areas.

A patch loaded afterwards carries its own barPosition and overwrites this, exactly as in the
application.
Tells appMenuBar.c there can never be a G2 attached, so its bank entries are omitted.

## 8. in `g2_draw_init()`

The top bar's controls take their colours from here. WITHOUT IT every control is drawn with a
zeroed tRgb — which is BLACK — so Undo/Redo and the A-D slot buttons came out as black
rectangles. init_graphics() calls it for the same reason; nothing about the bar works until
it has.

## 9. in `g2_draw_frame()`

The viewport-and-projection half of synthlibScale.c's synthlib_scale_update(). That file
cannot be linked here — its two OTHER functions call glfwGetWindowContentScale, which would
drag GLFW into the plug-in — but the graphics half now lives in utilsGraphics.c, which the
plug-in already compiles, so it is shared rather than repeated. What stays below is the
scaling arithmetic, which genuinely does differ from the application's.

## 10. in `g2_draw_frame()`

THE SLOT AND VARIATION BUTTONS SHOW THIS DOCUMENT'S CHOICE. Their colours live in
gTopbarControls, which is outside the document (a static table points into it - globalVars.h),
and the application only sets them on a click or a message from the G2. So a slot chosen any
other way - a restored project, or another editor's click - left A lit over slot C's patch.
Setting them from gSlot each frame costs a dozen stores and cannot disagree with it.

## 11. in `g2_draw_frame()`

THE APPLICATION'S OWN SCALING FORMULA, and adopting it is what makes the plug-in show the same
field of view as the editor rather than a cropped corner of it.

The whole UI is laid out in a fixed logical canvas TARGET_FRAME_BUFF_WIDTH/2 units wide (1280),
and gGlobalGuiScale maps that onto however many physical pixels there are. This used to be set
to the backing scale, which quietly redefined the logical canvas as 900 units — so roughly 70%
of the app's field of view, with larger patches running off the edge and no scrolling to
recover them.

A consequence worth stating: coordinates below are now LOGICAL UNITS, not points. At a 900pt
window they are about 1.42 to the point. Everything the renderer draws — including the menu
bar's own MENU_BAR_HEIGHT — is in those units, which is exactly how the application treats
them, so the chrome scales with the canvas instead of staying a fixed pixel size.

## 12. in `g2_draw_frame()`

PUSH THE PATCH TO THE ENGINE, exactly as the application's render_frame() does (graphics.c).

Turning a dial writes to the module database; the audio thread reads a parameter SNAPSHOT, and
nothing is heard until that snapshot is rebuilt. In the application a redraw is the event that
rebuilds it, and every edit causes a redraw — so doing it here gives the plug-in the same
behaviour, and covers every kind of edit rather than dials alone.

Safe against process() rebuilding on the audio thread at the same moment: the snapshot's
writers were given a mutex (gParamsWriteMutex in soundEngine.c) when the mod wheel latency was
fixed, and the critical section is one struct copy.
Deferred menu actions, drained here for the same reason the application drains them in its
render loop: a browser must not be opened from inside a menu callback. See msg_send() in
g2AppStubs.c for how the message gets this far.

## 13. in `g2_draw_frame()`

BOTH PANES, driven exactly as render_frame() drives them. render_modules()/render_cables()
read gLocation at their top, so the location is set around each pass rather than passed in —
the "mode rather than argument" style the pane machinery uses throughout. gLocation is put
back to the focused pane's afterwards, since that is what every other reader means by it.

## 14. in `g2_draw_frame()`

THE APPLICATION'S OWN TOP BAR, not a second implementation of it. An earlier attempt here
drew a hand-picked subset — patch name, variations, cable toggles — and looked nothing like
the editor, which defeats the point: a plug-in that resembles the application is the whole
reason for reusing its renderer. The controls that describe hardware draw too and should:
"Offline" is the truthful state here, and the TX/RX lamps simply stay dark.

## 15. in `g2_draw_frame()`

THE PALETTE BAND, and it belongs here rather than with the popups: apply_top_bar_height()
above has already reserved its height in the theme, so the canvas starts below it whether or
not anything is drawn there. Omitting this call did not hide the palette — it left the band
it had already pushed the patch down for EMPTY, which is what "the plug-in doesn't show the
extended top bar" looked like. Straight after render_top_bar(), exactly as render_frame()
orders the two in graphics.c.

## 16. in `g2_draw_frame()`

LAST, so an open menu is drawn over everything it overlaps: above the canvas and the chrome,
below nothing.

ONE CALL, AND THE ORDER IS DATA - the application's own words for the same line in
render_frame(). This used to be render_file_browser() and render_context_menu(), named
individually, and the consequence was that the THREE SynthLib popups nobody had thought to
name were never drawn. The alert dialog is one of them, so show_alert() had never once been
visible in the plug-in: Help > About did nothing at all, and so would any error it ever tried
to report. The coordinator draws SynthLib's own five in layer order, so a popup added there
arrives here without this file changing. The menu bar is the exception above: its render slot
is NULL because the BAR is the application's, drawn from gPluginMenuBar.
