# g2View.h notes

The longer comments from `g2View.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

An OpenGL surface that lives inside a window somebody else owns.

This is the experiment described in vst3/plugin-gui-notes.md: the application's renderer draws
through GLFW, which insists on creating its own window, and a plug-in is handed an NSView by the
host instead. The question this answers is the only one that decides whether the editor canvas
can ever appear in a plug-in — will an OpenGL context attached to a host-provided NSView draw at
all, inside a real host, alongside that host's own rendering.

It deliberately does NOT use the application's renderer yet. See the notes file for why: the
drawing code is reachable (all of it funnels through SynthLib's utilsGraphics.c) but pulling it in
drags GLFW along through synthlibScale.c, and that untangling is only worth doing once the surface
itself is known to work.

## 2. `g2_view_request_redraw()`

Mark the surface as needing to be redrawn. Safe from ANY thread: the view must be touched on the
main thread, and this hops there itself rather than making every caller remember to.

Declared outside the Objective-C section deliberately. This is what plain C reaches for — it is
how synthlib_request_redraw() is answered in the plug-in (g2AppStubs.c), which is the whole
mechanism by which a change anywhere in the editor causes a repaint. The application posts an
empty event to wake a blocked GLFW loop; here, AppKit schedules the frame.

## 3. `g2_view_create()`

The editor view, built for a plug-in wrapper that knows no Cocoa.

PLAIN C AND A void *, because the caller is g2Plugin.c, which fills in SynthLib's format-free
descriptor and must not include an AppKit header to do it. RETAINED (+1) on the way out, as
SynthLib's createView() contract requires — see synthlibPlugin.h.

`doc` is the instance's document (a tG2Document *): the view selects it before every frame and every
event, so two open editors each draw and edit their own instance.
