# globalVars.h notes

The longer comments from `globalVars.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `gTempoDragging`

OUTSIDE THE DOCUMENT, because file-scope tables hold their addresses (synthSettingsResources.c's
rows point at their own rectangles; usbComms.c's name sweep points at the tables it fills) and an
address inside the current document is not a constant. None is part of a patch: the first is the
Synth Settings panel's layout, the other two are the names in a connected G2's banks.

The same goes for these: editor panels and drag flags that static tables point at (graphics.c's
floating panels, mouseHandle.c's tempo, vibrato and glide drags). They belong to an EDITOR rather
than to a G2, so they stay shared between open plug-in editors for now - see todo.md.

## 2. `tG2Document`

── THE DOCUMENT ─────────────────────────────────────────────────────────────────────────────────

EVERYTHING THE APPLICATION KNOWS ABOUT ONE G2, IN ONE PLACE: the four-slot patch database, the patch
and performance settings, and the editor's own state for them. Until 2026-09-11 these were ~120
separate globals. That was fine for an application, which edits one G2, and it was the whole
reason G2 Alike could only be loaded once per process: two instances wrote into the same globals.

THE NAMES DID NOT CHANGE. Each one below is a macro onto a field of the CURRENT document, so the
renderer, the protocol code and the engine read and write exactly what they always did, and none
of them had to be edited. The application has one document, selected on every thread from the
start, so it behaves as it always has; each plug-in instance owns one and selects it at every
entry - the host's audio thread, its UI thread, the editor's events - see g2Plugin.c.

THREAD-LOCAL, because a host renders different instances on different threads at the same time,
and each thread must be looking at its own instance's document.

TRAP: a field cannot be reached through any other pointer - `doc->gSlot` expands gSlot as well. Code
that has to work on a document other than the current one selects it first (globalVars.c).

## 3. file scope

BUMPED WHENEVER A SLOT'S CONTENTS ARE REPLACED — a patch parsed into it, or the slot cleared. The
UI thread watches it to know that anything it was holding a module key for is gone; see
selection_validate(). Atomic because patches arrive on the USB thread and this is read on the
render thread, which is also why the selection is not simply cleared at the point of the load.

## 4. file scope

```
extern _Atomic uint8_t     gPerfMode;
extern char                gPatchName[MAX_SLOTS][PATCH_NAME_SIZE + 1];
```
IS THE G2 ANSWERING? Set the moment it replies to the patch-version request, which is the point
at which traffic is demonstrably working both ways. Deliberately SEPARATE from gCommsState:
that is a load SEQUENCE (waiting-ready, awaiting-sync-decision, online-and-loaded) and it moves
on through several values while the device is sitting there answering perfectly well. The lamp
wants the simple question, and asking the sequence gave the wrong answer twice over — "Offline"
for the 8 seconds of the initial pull, and "Offline" again while the editor asks the user to
choose between their offline edits and the G2's patches.

## 5. `device_ready()`

IS IT SAFE TO ISSUE A DEVICE OPERATION? One predicate, because this was open-coded as
"gCommsState == eCommsOnLine" in more than thirty places — twelve bulk operations in usbComms.c,
thirteen menu actions, the menu enable/disable gates, MIDI-to-synth forwarding and the backdoor —
and a rule spread over thirty copies is a rule that gets changed in twenty-nine.

Note what it is NOT: it is not "is a G2 attached". eCommsOnLine is only reached once the initial
pull has finished, so this asks "has the editor finished loading and is it safe to start
something big" — which is exactly right for a Backup or a bank Store, and exactly wrong for the
Online lamp, which should light as soon as the device answers. Splitting those two meanings is
what this predicate exists to make possible.

## 6. `variation_is_linked()`

── Linked variations ───────────────────────────────────────────────────────

The set of variations that a parameter edit fans out to as well as the selected one — built by
shift-clicking variation buttons in the topbar, one set per slot.

Membership is EXPLICIT and independent of which variation is selected, which is what lets you
audition another variation without dismantling the group. The selected variation always receives
its own edits whether or not it is a member; being a member is what makes it keep receiving them
once you have moved on. If the selected variation were only ever an implicit member, selecting a
different one would silently drop it from the group with nothing on screen having changed.

Editor-only state: deliberately NOT part of tPatchDescr, which is a wire structure the G2 reads
back. The device knows nothing about this and each fanned-out write reaches it as an ordinary
per-variation parameter change.

Variations are 0-based here; VARIATION_INIT is not a real variation and is never a member.
