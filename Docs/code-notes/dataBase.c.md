# dataBase.c notes

The longer comments from `dataBase.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `database_read_lock()`

── THE DATABASE LOCK ────────────────────────────────────────────────────────────────────────────

gModule and gCable are read and written by THREE threads, and until now by none of them safely:
```
  - the USB thread WRITES on every patch load and parameter change
  - the render thread READS throughout a frame
  - the CoreMIDI thread READS all of it to rebuild the sound engine's snapshot, which
    midiInput.c does on a morph change so mod wheel response is not capped at the frame rate
```
The audio thread is deliberately NOT among them - it only ever reads the engine's own seqlock
snapshot - and that is what makes a blocking lock viable here at all.

COARSE, AND THAT IS THE POINT. Locking inside get_module()/get_cable() would be both expensive and
useless: a torn read of one aligned uint32 was never the problem, and walking a module list while
a patch load rewrites it is. So the lock is held across whole OPERATIONS - a patch parse, a render
pass, a snapshot build - and the accessors below stay exactly as they were.

NON-RECURSIVE, AND THE DISCIPLINE MATTERS. pthread_rwlock_t is not recursive, and taking a read
lock twice on one thread can deadlock outright on an implementation that lets a waiting writer
jump the queue. So it is taken at the OUTERMOST operation and nowhere inside:
sound_engine_update_from_patch() does NOT lock, because render_frame() already holds the read lock
when it calls it - its other two callers take the lock themselves instead.

ONE PER DOCUMENT since 2026-09-11: gDatabaseLock, gModule and gCable are fields of the current
document (globalVars.h), so each plug-in instance locks its own patch and never waits on another's.

## 2. `database_delete_modules_by_slot()`

THE GENERATION BUMP LIVES HERE, not in the callers. Anything keyed to this slot's modules -
a selection above all - is stale the moment they are wiped, and selection_validate() spots that
by watching gPatchGeneration. Three callers bumped it by hand and two did not:

```
  - init_patch() deletes every module and cable for a New Patch and did NOT bump it, so a
    selection SURVIVED New Patch. Reproduced: select a module, New Patch, add a fresh one, and
    the new module comes up already wearing the yellow selection border - it has inherited the
    slot/location/index key of the module that used to be there.
  - the backdoor's NEWPATCH calls the two delete functions directly, with the same result.

```
Bumping here makes it impossible to wipe a slot without invalidating what points into it. The
callers that already bump are left alone: the counter is only ever compared for INEQUALITY, so
counting twice for one clear costs nothing and removing their bumps would be a wider change than
this fix needs.

## 3. `init_patch()`

A brand-new, empty patch. Moved here from mouseHandle.c, where its own comment asked where it
really belonged: nothing in it touches a window, and clear_slot_data() below came here for the
same reason — a GUI-less build could not reach it otherwise.

THE VST3 PLUG-IN NEEDS THIS BEFORE IT HAS LOADED ANYTHING. Without it gPatchDescr is all zeroes,
and a zero barPosition means "Voice Area takes no height" — which put the pane divider hard
against the top of an empty plug-in window instead of partway down it.

## 4. in `init_patch()`

Voice Area pane height in pixels — see splitView.h. A NEW PATCH OPENS WITH THE FX AREA
MINIMISED, because that is where the work starts (CT, 2026-08-30: "initialising patch for new
patch, should minimise the FX area. We're initially working in the VA area for new patches").

This REVERSES an earlier call recorded here — G2-Edit used to open a new patch at 300, showing
both areas, on the reasoning that the divider is the point of the window. Noting the reversal
rather than quietly overwriting it, since the comment stated it as deliberate.

It also lands back on what the original editor does: its own default is 4000, larger than any
window, so it clamps to "Voice Area takes everything" exactly as SPLIT_POS_MAX does here. The
divider stays on screen at the bottom either way — see splitView.h, there is no separate
one-area mode — so nothing is hidden, and dragging it back up costs one gesture.

Patches loaded from file or from the G2 carry their own value and are untouched by this.

## 5. in `init_patch()`

database_delete_modules_by_slot() above zeroes every module for this slot, including the
morph-groups pseudo-module (locationMorph/patchModuleMorph) that every patch structurally has.
render_morph_groups() reads it via get_module(), which returns NULL unless active is set, so
without this the morph knobs would silently render nothing for a freshly-initialised patch —
reactivate it here with its key set, same as parse_module_list() does when a real patch
arrives from the device.

## 6. in `init_patch()`

Each morph group's "mode" param (index i+NUM_MORPHS) is 0 for plain manual-knob mode,
nonzero for assigned-to-a-fixed-source mode (see render_morph_groups()'s isKnob check) —
default every group to its fixed source (Wheel, Vel, Keyb, ... per morphStrMap[i]) rather
than leaving all 8 as unnamed knobs, across every variation so it holds regardless of
which one is active.
