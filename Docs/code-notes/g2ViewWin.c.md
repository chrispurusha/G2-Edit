# g2ViewWin.c - notes

The Windows editor window of the G2 Alike plug-in: the counterpart to `g2View.m`, with the same entry
points (`g2_view_create()`, `g2_view_destroy()`, `g2_view_request_redraw()`, `cursor_*()`) so `g2Plugin.c`
and the shared drawing and input code (`g2Draw.c`, `g2Input.c`) are unchanged. Built only by
`tools/do-windows-plugin`. Written 2026-10-04, untried in a host when written.

## 1. What it is

A Win32 window of its own class, created as a hidden popup and made a child of the host's window by
SynthLib's `synthlibPluginVst3ViewWin.cpp` (the plug-in contract's `createView()` takes no parent). It
draws with OpenGL through SynthLib's `renderBackendGL.c` - the backend the Windows application uses -
into its own WGL context, and presents with SwapBuffers. Sizes are pixels, the scale 1.0: a host's DPI
scaling (IPlugViewContentScaleSupport) is not handled yet, so on a scaled display the editor is small.

## 2. `cursor_capture()`

Hidden, not locked and not warped: the same choice as the Windows application (mouseHandle notes §37),
because Parallels' absolute pointer cannot be re-centred. A drag reports movement as the difference
between successive WM_MOUSEMOVE positions; the pointer is held by SetCapture() from the press.

## 3. `register_class()`

The class name carries the DLL's load address. A class a DLL registers is NOT unregistered when the DLL
is unloaded, and a host that unloads and reloads the plug-in would otherwise find the old class, whose
window procedure points into the unloaded image.

## 4. `ensure_root_context()`

Every editor window has its own GL context, all sharing objects (wglShareLists) with one hidden root
context that is never destroyed. The glyph atlas and other textures are made once per process and
reused (`gFontReady`, the text cache), so a second editor - or the first one reopened - would otherwise
draw with texture names its own context never had. The root holds them while every editor closes.

## 5. `shortcut_char_for_vk()` - Ctrl for Cmd

The plug-in's shortcuts (`g2_input_key()`: Cmd +/- zoom) are taken with Ctrl on Windows, read from the
virtual key in WM_KEYDOWN because WM_CHAR delivers control characters while Ctrl is held. Popup keys
(the file browser's filename box, Escape, Enter) go first, as GLFW key codes, through
`g2_input_popup_key_glfw()`; printable characters arrive as WM_CHAR. The modifier state itself maps as
GLFW maps it for the application: the Windows key is Cmd, Ctrl is Ctrl.

## 6. `recover_lost_release()`

As g2View.m's: a release that never arrives (the pointer let go outside a host that swallowed it) must
not leave the pointer hidden, so the tick timer releases the drag once no button is down.

## 7. `g2_view_create()`

The document is selected and the context made current BEFORE g2_draw_init(), which sets up the current
document's top bar and builds the glyph textures in the current context. The backend is chosen
explicitly (OpenGL, the only one linked on Windows).
