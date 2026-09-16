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

## 5-6. `default_patch_path()` and `load_patch()`, removed 2026-09-16

NOTHING LOADS A PATCH BY ITSELF ANY MORE (CT: "We shouldn't be loading the last loaded patch
file"). Both functions are gone, and with them the whole startup chain: the path the host had
restored with the project, `$G2_PLUGIN_PATCH` and the older `$G2_VST3_PATCH`, and the fixed
`~/Documents/G2-Edit/plugin.pch2`. An instance comes up on the empty patch g2_create() makes, in
all four slots, and a patch arrives only through File > Open Patch File...

WHY, AND WHAT REPLACES IT. Storing a path made a project's sound depend on a file that could move,
change or vanish underneath it - and reopening a project silently reloaded whatever was at that
path now, which is not the same thing as restoring what was saved. The replacement is for the host
to store the patch DATA, which is the next piece of work; until it lands, a reopened project is
honestly empty rather than wrong. The numbers are left as a gap so the remaining markers resolve.

What was here, for the record: the chain above in priority order, and a note that the built-in
compiled-in patch it replaced had itself been a scaffold from before the plug-in had an editor.

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

UPDATED 2026-09-14: the record also carries the editor's mouse mode - `dialmode=` (Rotary 0, Vertical 1,
Horizontal 2). Same header: an older build skips the new key.

UPDATED 2026-09-16: AND THE VOICE/FX SPLIT - `split=`, one barPosition per slot, comma separated. This
REVERSES the 09-14 decision that used to stand here ("the split is NOT in the record: it is the patch's
own"), on CT's call: the divider is patch data, so a project reopened in the host came back with the
divider where the FILE said rather than where it was left. It is applied LAST in g2_set_state() (§11). A
missing key, or a slot the list does not reach, is -1 and leaves the slot's own alone, so a project saved
before this change opens as it always did.

`split=` IS INTERIM, and knowing that should stop it growing roots (CT, 2026-09-16): barPosition is part
of the patch, so once the host stores the patch DATA the divider is restored with it and this key goes.
It exists because the patch data is not stored yet and the divider was being lost in the meantime.

UPDATED 2026-09-15: and the engine's drone mode - `drone=0|1`, Settings > Drone Mode (sound-engine-notes §20).
Per instance, unlike the mouse mode. A record without it, from a project saved before it existed, gets the
default, on: the record replaces what the instance holds (§11).

NO FILE NAMES AT ALL SINCE 2026-09-16 (CT). The record used to identify each slot's patch by PATH -
`perf=` in performance mode, otherwise `slot0=`..`slot3=` - and g2_set_state() reopened them, so a
project came back playing whatever was at those paths now. That is gone. The record carries editor
state only:

```
    G2Alike state 2
    perfmode=0|1
    selected=0..3
    dialmode=0|1|2
    drone=0|1
    split=<slot0>,<slot1>,<slot2>,<slot3>

```
The header stays at 2. A record written before the change still carries `perf=`/`slot0..3=`, and
parse_state_line() simply does not know those keys any more, so they are skipped exactly as a
future key would be by an older build - which is precisely the wanted behaviour: an old project
opens empty rather than reloading its file.

EDITS NOT SAVED TO A FILE ARE STILL NOT STORED, and now neither is anything else about the patch.
Storing the patch DATA is the next piece of work; the point of doing the removal first is that a
reopened project is honestly empty instead of confidently wrong.

THE OLD BARE-PATH FORMAT IS READ AND IGNORED: a blob without the header held nothing but a path, so
there is nothing left in it to restore. NO TERMINATOR IS WRITTEN: the blob's length is its length.

## 11. in `g2_set_state()`

THE RECORD REPLACES WHAT THE INSTANCE HOLDS. A host restores state into a fresh instance, but
also into a live one when a preset is recalled, so what was there before must not survive: every
slot goes back to the application's new empty patch before anything in the record is applied.

THE REMEMBERED PATHS ARE CLEARED WITH THEM (2026-09-16). gSavedPatchPath/gSavedPerfPath are what
File > Save writes back to, so leaving a restored path in place while loading nothing would aim
Save at a file whose contents were never read - one keystroke from overwriting a real patch with
an empty one.

ORDER STILL MATTERS, for a different reason than it used to. init_patch() sets barPosition to
SPLIT_POS_MAX, so `split=` has to be applied after the slots are made or the empty patch overwrites
it a moment later. It is no longer a race against the .pch2, because no .pch2 is opened here.

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
