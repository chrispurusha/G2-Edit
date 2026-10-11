# slotSets.c - notes

## 1. `perf_mode_set()` - a set of four slots per mode

The G2 keeps one set of four slots for Patch mode and another for performance mode, and the mode switch swaps
them: entering performance mode loads the stored performance, leaving it brings the Patch-mode slots back as they
were (findings 2026-10-11, measured with DEVPERF). With a G2 connected the switch is the G2's and the editor reads
the new set back, as before. Offline and in the plug-in there was one set, so the switch changed only the keyboard
routing and a performance's sequencers ran on in Patch mode (CT chose to follow the G2, 2026-10-11).

Now the set being left is kept, whole, as a performance image with what only the engine has (patchWrite.c notes
§3) - `gOtherSlotSet` in the document, so each plug-in instance has its own - with its performance's name, and the
other is put back; a mode never used yet starts with four empty patches, as the G2's first switch does. Every
way of switching comes through here: the topbar button, the backdoor's DEVPERF and the plug-in's editor. The plug-in
saves both sets in a project (g2Plugin.c notes §17).

## 2. `file_image_body()`, `perf_image_parse()`

Where a .pch2/.prf2 image's data starts (after its text header, version and type bytes, its CRC checked), and a
performance image read into the four slots - parse_perf() and the Sampler lines of its header (patchWrite.c
notes §3), under the database's write lock, which parse_perf() does not take. Moved here from the plug-in's
g2Patch.c so the application's mode switch can use them; the plug-in calls them too.

## 3. `slot_set_other_take()`

Replaces the kept set with a copy of an image and its name - the mode switch's, or a plug-in project's.
