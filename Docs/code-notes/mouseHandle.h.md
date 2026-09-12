# mouseHandle.h notes

The longer comments from `mouseHandle.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `stop_dragging()`

shift_modifier_held(), cmd_modifier_held(), alt_modifier_held() and multi_select_modifier_held()
USED TO BE DECLARED HERE, each answered by a glfwGetKey() poll. They are now in SynthLib's
inputState.h, answered from state the shell pushes — see that header for why, and
modifier_bits_from_glfw() in mouseHandle.c for this application's end of it. Nothing that asks
about a modifier needs GLFW any more, which is what lets moduleGraphics.c and mutatorUI.c link
into the plug-in, and it means the plug-in gets real modifiers instead of a stub answering false.

## 2. `recover_lost_cursor()`

True while any drag that hides the cursor (CURSOR_DISABLED) is active —
param/tempo/perf-tempo/vibrato-rate/vibrato-amount/glide-time dragging.
During these, the reported cursor position is a virtual/relative-delta
accumulator, not a real on-screen point — it can drift over an unrelated
control, so anything that hover-highlights "what's under the mouse"
(e.g. render_knob_assignment_overlay()'s per-param hover check) needs to
suppress itself while this is true, or it'll highlight the wrong control.
Restores the pointer if it is hidden with no drag running — call once per frame. See the note in
mouseHandle.c for why this rather than debouncing the mouse button.
