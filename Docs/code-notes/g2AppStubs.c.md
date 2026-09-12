# g2AppStubs.c notes

The longer comments from `g2AppStubs.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

What the canvas renderer refers to but a plug-in has no answer for.

The plug-in draws the patch; it does not edit it, does not own a mouse, and is not connected to a
G2. Everything below exists because moduleGraphics.c and renderParams.c contain the CLICK HANDLERS
as well as the drawing, in the same translation units — the handlers are never called here, but
their references still have to resolve.

THIS FILE IS A MEASUREMENT, NOT JUST A CONVENIENCE. It is the complete list of what stands between
the editor's renderer and a build with no application around it, and it is thirteen functions in
three groups: mouse position, editing, and two pieces of ambient state. That is the real size of
the coupling, and it is small enough to be worth reading as an argument for the shell/renderer
split described in plugin-gui-notes.md.

Each stub is inert rather than merely empty where that distinction matters — a coordinate is
zeroed, a "find a free slot" returns "none" — so that if one ever IS called, the result is
something harmless and obvious rather than uninitialised memory.

## 2. file scope

defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
behind the top bar and with the margin between them swallowed.

## 3. file scope

── Mouse position and modifiers ────────────────────────────────────────────────────────────────

THIS GROUP IS NOW EMPTY, and it is worth saying what used to be in it, because the reason it
emptied is the pattern the rest of this file is still waiting for.

get_global_gui_scaled_mouse_coord() is answered by g2Input.c from the host's events, and both
coordinate conversions moved into src/canvasCoords.c — out of mouseHandle.c and menus.c
respectively — because the arithmetic never needed a window in the first place.

multi_select_modifier_held(), shift_modifier_held() and cmd_modifier_held() were stubs returning
false, so Shift-drag on a mutator slider and Cmd-click behaved as unmodified clicks here. They are
now REAL, from SynthLib's inputState.c, because that seam holds pushed state instead of polling a
window: g2View.m translates each NSEvent's modifierFlags and pushes them exactly as the
application pushes GLFW's. A stub that answers false is a feature quietly missing; a shared piece
of state each shell fills in is the same code working in both.

start_cursor_drag() is NOT here any more — it is real, in g2Input.c, because it needs the mouse.

## 4. file scope

── Editing ─────────────────────────────────────────────────────────────────────────────────────

A plug-in has no G2 to send to and nothing to undo. msg_send() is the one worth noticing: it is
the single point through which the whole UI reaches the hardware, so a plug-in that never calls
it cannot accidentally write to a connected synth — which is the behaviour we want anyway.

## 5. `msg_send()`

MESSAGES TO THE G2 ARE DROPPED; MESSAGES TO THE GUI ARE NOT.

Both go through this one function in the application. Dropping everything was right while the
plug-in only drew — but File > Open Patch File... does not open a browser itself: it POSTS
eRspShowOpenRead to the GUI queue so the browser opens from the render loop rather than from
inside a menu callback (see menuActions.c). Dropping that made the menu item do nothing at all.

So gToUsbThread is still discarded — there is no synth, and a plug-in must never write to one —
and gToGuiThread is queued for the editor to drain. One slot is enough: these are user actions,
arriving one click at a time.

HELD IN THE DOCUMENT, so with two editors open each drains only its own: a menu action chosen in
one instance's window must open its browser there, not in whichever editor draws next.
&gToGuiThread is the current document's queue too, which is what makes the test below per document.

## 6. `synthlib_request_redraw()`

── Ambient state ───────────────────────────────────────────────────────────────────────────────

gMutator and param_overlay_note_param() used to be faked here. They are REAL now — mutatorUI.c and
paramOverlay.c are linked, so the Mutator panel and the parameter overlay work rather than being
pretended at.

## 7. `synthlib_request_redraw()`

synthlibGlobals.c is NOT linked in: its synthlib_request_redraw() calls glfwPostEmptyEvent(), so
taking the file would take GLFW with it. Only these two are actually reached from the renderer.

Redraw is where the plug-in and the application genuinely differ rather than merely stub out, and
it is NOT a stub — it is the real thing, routed differently. The application posts an empty event
to wake a blocked GLFW loop; here the view is marked dirty and AppKit schedules the frame. Every
part of the editor that changes something already calls this, so wiring this one function is what
makes the whole canvas repaint on change.

## 8. `notify_full_patch_change()`

A NEW PATCH HAS REPLACED EVERYTHING ON THE CANVAS. The application's version (graphics.c) also
puts both scrollbars back to top-left, and those live in that file's own gScrollState with
set_x_scroll_bar()/set_y_scroll_bar() beside them - none of which is in this build, because the
plug-in scrolls through the host's view rather than through a pair of drawn bars.

What DOES carry over is the location: a patch loaded while the editor was showing the FX area
would otherwise open showing a variation of it that no longer exists.

## 9. `undo_push_module_replace()`

THE PLUG-IN HAS NO UNDO STACK - not for module replace and not for anything else. undo.c is not
in this build, and nothing in the plug-in's menus offers Undo, so this is a genuine no-op rather
than a piece of the application quietly missing: a replace here is as final as every other edit.
The moment the plug-in grows an Edit menu, undo.c joins the source list and this goes.

## 10. `gDialMode`

Settable from the View menu, and REMEMBERED between sessions the way the application remembers
it — through the same SynthLib prefs store, though under the plug-in's own name. See
g2_plugin_prefs_init() for why the file is not shared with the application's.

Rotary is the default because the other two want a hidden, warped cursor that a host view does not
give us — see the note in g2Menu.c's View menu.
