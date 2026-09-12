# g2Plugin.c notes

The longer comments from `g2Plugin.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

EVERYTHING ABOUT "G2 Alike" THAT A PLUG-IN FORMAT NEEDS TO KNOW, and nothing about any format.

This file is the whole of what used to be g2Vst3.cpp's G2-specific half - ten parameters, a patch
path, notes, and a canvas to draw. The VST3 plumbing that surrounded it, and the Audio Unit
plumbing that would have had to be written a second time beside it, are now in SynthLib's
plugin/ folder and are shared. `./do-plugin au` and `./do-plugin vst3` compile this identical file.

It is C, and so is SynthLib's descriptor, so nothing here needs C++ or Objective-C: the two
places that do - a VST3 vtable and a Cocoa view - are on the other side of the seam.

EVERY INSTANCE IS ITS OWN G2 (2026-09-11). Each one owns a DOCUMENT - the application's whole
state for one G2, four slots of patch included, since performance mode needs all four - and claims
an ENGINE of its own. Both are chosen per thread (globalVars.h, soundEngine.c), so every callback
below starts by selecting its instance: the host calls from its UI thread and from as many audio
threads as it likes, and two tracks of G2 Alike can be rendering at the same moment.

Until then the engine and the database were plain globals, and two copies in one project fought
over them. The engine plays the SELECTED slot, as the application's does; playing a whole
performance is still to come (see sound_engine_bind_slot()).

## 2. `gProcessorUid`

THESE BYTES MAY NEVER CHANGE. A host remembers a plug-in by them, so a project saved against this
build must find the same numbers next time or it reopens with an empty slot.

They are the same two ids g2Vst3.cpp declared as FUIDs, written out as bytes. A VST3 FUID built
from four uint32s lays each one out big-endian on every platform except Windows, which is what
makes 0x7D14B03C the four bytes 7D 14 B0 3C - so this is the identical plug-in to a host that
already knows it, not a new one.

## 3. `G2_MORPH_AFTERTOUCH`

WHICH MORPH GROUP THE G2 WIRES EACH PHYSICAL CONTROL TO (midiInput.c's MORPH_GROUP_*), and
therefore which of parameters 0-7 a MIDI control should drive. The G2 hard-wires its wheels and
pedals to particular morph groups - morphStrMap in moduleResources.h names them - and those groups
are already parameters, so most of the table below maps a control onto a parameter that exists
rather than inventing one. Only pitch bend needs a parameter of its own, having no morph group.

The wiring is in the table's midiControl column; this one is named because the poly-pressure path
does not go through the table - see g2_poly_pressure().

## 4. `level_db()`

A MORPH DOES NOT REACH THE AUDIO THREAD BY ITSELF, and morphSnapshotDirty is how the plug-in copes.

sound_engine_set_morph() only records the position. Unlike pitch bend, which the audio thread
reads directly, a morph is folded into the parameter SNAPSHOT, and that snapshot is only rebuilt
by sound_engine_update_from_patch(). The standalone editor rebuilds it on every redraw, which is
why moving a morph there requires asking for one - and why its mod wheel response is capped at the
frame rate, since a full canvas repaint sits between the wheel and the sound.

A plug-in cannot borrow that arrangement: it has to work with the editor window closed. So the
rebuild happens in render() instead, once per block, and only when something actually moved.

A FLAG RATHER THAN REBUILDING ON THE SPOT, because the snapshot is published through a SEQLOCK
(gParamsSeq in soundEngine.c). A seqlock tolerates exactly one writer; the audio thread is already
its reader, and a parameter change can arrive on the host's UI thread. Letting both write would
corrupt it. So every setter merely sets this, and render() - one thread, once per block - is the
only writer. Per instance, since each has its own snapshot.

## 5. `default_patch_path()`

Where the patch comes from when the host has not restored one. Checked in order:
```
  1. the path the host restored with the project (g2_set_state below)
  2. $G2_PLUGIN_PATCH, then the older $G2_VST3_PATCH
  3. ~/Documents/G2-Edit/plugin.pch2

```
The host-stored path is what makes a project reopen sounding as it did; the environment variable
is for driving it from a test script - a host launched from the Dock inherits no shell environment,
so it only ever applies to a scripted run - and the fixed location is so it does something
sensible with neither set.

## 6. in `load_patch()`

THE PATH, not a patch compiled into the binary. The built-in patch was a scaffold from before
the plug-in had an editor: with no way to choose a file, embedding one removed a whole class of
"why is it silent" while the rest was proven. File > Open Patch File... has replaced it, and a
plug-in that quietly plays somebody else's lead patch on load is worse than one that starts
empty.

An empty path or a missing file simply leaves the canvas empty, which is honest.

A patch into slot A, or a whole performance. g2_plugin_open_file() also records the path as
what File > Save writes back to and what the project stores (g2_get_state()) - the same fields
the editor sets when it opens or saves a file, so the two cannot disagree.

A FILE THAT IS NOT THERE IS STILL REMEMBERED, so a project whose patch sits on an unmounted
drive keeps naming it rather than forgetting it on the next save.

## 7. in `g2_set_active()`

THE CHAIN IS RESOLVED HERE, not when the patch was read. sound_engine_update_from_patch()
returns immediately while the engine is inactive, so calling it at initialize() time -
which is the obvious place, and where this used to be - silently did nothing and the
plug-in rendered silence from a perfectly good patch.

## 8. in `g2_process()`

EACH NOTE AT ITS OWN SAMPLE. The block is rendered up to a note's offset, the note is started,
and the render carries on from there - so a note lands where the host put it rather than at the
start of the block, which at 512 frames and 44.1 kHz was up to 12 ms early. The engine takes
queued notes one per internal sample, so a chord still spreads over consecutive samples.

## 9. `g2_note_on()`

THROUGH THE SHARED NOTE STACK, NOT STRAIGHT TO THE ENGINE. The engine is monophonic, so releasing
a note has to fall back to whatever is still held or legato playing breaks - hold D, play F, let F
go, and the D under your finger must come back rather than the sound stopping. noteStack.c is the
application's own logic, moved out of midiInput.c so both get it from one place.

OMNI, AND AT ITS OWN SAMPLE: the channel is ignored, and the note waits in the instance's queue
until g2_process() reaches its offset (2026-09-11; it used to land at the start of the block).

## 10. `G2_STATE_HEADER`

THE PATCHES ARE IDENTIFIED BY PATH rather than embedded wholesale. A .pch2 is small enough to embed,
and doing so would make a project self-contained, but it would also freeze a copy: edit the patch
in G2-Edit and the project would go on playing the old one, silently. Storing the path keeps one
patch with one meaning. The same holds for a performance.

ALL FOUR SLOTS SINCE 2026-09-12. An instance holds a whole performance, so the state is a short
text record: the performance's path when the instance is in performance mode and has one,
otherwise each slot's patch path, and in both cases the selected slot:

```
    G2Alike state 2
    perf=/path/to/set.prf2          or   slot0=/path/a.pch2 ... slot3=/path/d.pch2
    perfmode=0|1
    selected=0..3

```
A slot with nothing loaded from a file is simply absent. EDITS NOT SAVED TO A FILE ARE NOT STORED,
as before - the project holds paths, not patches.

THE OLD FORMAT IS STILL READ: a blob without the header is a bare patch path, which is what every
project saved before this held, and it goes into slot A. NO TERMINATOR IS WRITTEN in either form:
the blob's length is its length.

## 11. in `g2_set_state()`

THE RECORD REPLACES WHAT THE INSTANCE HOLDS. A host restores state into a fresh instance, but
also into a live one when a preset is recalled, and a slot the record does not name must not
keep whatever was there. A performance names all four; otherwise an unnamed slot becomes the
application's new empty patch.

A FILE THAT HAS GONE leaves its slot (or, for a performance, all four) empty but is still
remembered, as load_patch() does, so the project goes on naming it.

## 12. `g2_create_view()`

THE EDITOR IS THE APPLICATION'S OWN CANVAS, not a second renderer. g2View.m is the NSView, g2Draw.c
draws the frame by calling render_modules() / render_cables(), and the menu bar is the
application's too - so the editor has File, Settings, Controls, Tools, View and Help.

What made that possible was moving the drawing behind a render backend: the application reaches
its window through GLFW, which creates and owns one, while a plug-in is handed an NSView the HOST
owns and GLFW has no "adopt this existing NSView". gfx_attach_window() takes the host's view,
SynthLib's utilsGraphics.c is the only thing that draws, and the same canvas code serves both.

## 13. in `g2_editor_width_save()`

900 points wide, and the height follows the ratio the application locks its own window to
(TARGET_FRAME_BUFF_WIDTH : TARGET_FRAME_BUFF_HEIGHT, 2560:1440).

THE LOCK IS WHAT COMPLETES THE SCALING. gGlobalGuiScale is derived from WIDTH alone, so on its
own a taller window would simply uncover more rows rather than drawing the patch larger. The
application never shows that because its window cannot be made taller without also becoming
wider. Below 640 the module text stops being legible; there is no maximum, since everything
scales.
