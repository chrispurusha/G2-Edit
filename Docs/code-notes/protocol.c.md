# protocol.c notes

The longer comments from `protocol.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope


Reference credit on some of the excellent G2 comms protocol work by
Bruno Verhue in his Delphi editor application:

https://www.bverhue.nl/g2dev/

## 2. `record_param_count_mismatch()`

Shares gStringCopyMutex (defined in globalVars.c) with the COPY_STRING macro used
everywhere on the UI thread to read these same name buffers — a previous private
mutex here provided no actual exclusion against those reads (two different locks
don't exclude each other), which raced against the UI thread's wake-driven render
loop and could leave the topbar showing a name mid-clear until the next redraw.
A module arriving with a DIFFERENT parameter count than paramLocationList lists for its type is a
gap in that table. This used to EXIT_IN_DEBUG() when the G2 declared MORE than we know about — a
deliberate development tripwire, on the reasoning that stopping is how a missing row gets noticed.

It no longer stops, because in practice the tripwire destroyed the very evidence it existed to
collect. It fires while a patch is arriving from the G2, so it kills the session before the
offending module can be looked at, and a mismatch rare enough not to be reproducible on demand is
exactly the kind you only get one look at.

Both directions are survivable: the count is 7 bits so it cannot exceed 127, and module->param is
MAX_NUM_PARAMETERS (128) wide, so every value read lands in bounds either way. What the extra
device parameters lack is a table row to draw them with — they are stored, just not shown.

Recorded to ~/G2_param_count_mismatch.log in APPEND mode, the point being to outlive the session
it happened in (usbLog.c opens with "w" and is gated behind ENABLE_USB_LOG, so it is no use here).
Once per module type per session, so a patch full of the same offender writes one line, not one
per instance. USB-thread only, which is what makes the plain static safe.

## 3. `peek_patch_category()`

Extracts just the category byte from a raw .pch2/.prf2 body (parse_patch()/parse_perf()'s own
chunk stream: repeating [type:8][count:16][payload:count bytes], type SUB_RESPONSE_SEL_PARAM_PAGE
excepted). Used by restore_bank() (usbComms.c) to refresh gPatchNameTable/gPerfNameTable's
category for a freshly-pushed location — deliberately NOT parse_patch_descr() itself, which
writes into gPatchDescr[slot] and touches topbar highlight state keyed by a live edit-buffer
slot; a bank/location being restored isn't a slot, so re-using that function would corrupt
whatever real slot happened to share its index. Returns 0 (patchTypeStrMap's first entry) if no
SUB_RESPONSE_PATCH_DESCRIPTION chunk is found.

## 4. in `parse_module_list()`

The wire field is 4 bits, so anything up to 15 is expressible whatever our table says. Log
it and carry on rather than exit()ing: the surplus values are still READ below (just not
stored), so the bit stream stays aligned and the rest of the patch parses. Exiting here
took the whole editor down, in Release as well as Debug, and — LOG_MODULE_DATA being
compiled out in every configuration — did it without printing anything at all.

## 5. in `write_module_list()`

modeCount is written as it was received, so the stream stays the shape the reader expects
even for a module that declares more modes than we can hold. The surplus values are gone —
parse_module_list() logs when it drops them — so write a defined 0 rather than reading off
the end of mode[].

## 6. in `parse_param_list()`

Expected to always be 10 (live USB) or 9 (a .pch2/.prf2 file) — this function has no way to
tell which source it's parsing, so this only flags anything outside that pair rather than
enforcing the exact value for the actual source.

Except when the section is EMPTY. A location with no modules is written by the G2 (and by
every .pch2 with an unused area) as moduleCount 0, variation count 0, and there is nothing
wrong with that — half the files in PatchTestFiles have one, and each was logging an error on
load. write_param_list() writes 9/10 even when empty, so this is about what we READ, not what
we produce.

## 7. in `parse_param_list()`

Compared against the DEVICE parameter count, not the raw row count: SeqNote carries two
paramTypeCustomData rows in slots the G2 never transmits, so against module_param_count()
it reported a permanent 37-vs-39 mismatch. A check that cries wolf on a module that is
correct is worse than no check, because it trains you to ignore the one that isn't.

## 8. in `write_param_list()`

The count the section header declares. It used to be assigned only inside the per-module loop
below, so a location holding no module with parameters wrote 0 here — and parse_param_list()
rejects a variation count of 0 outright, losing the whole section rather than reading it as
empty.

## 9. in `write_param_list()`

actualParamCount is only populated by parse_param_list(), i.e. by data that came from the
G2 or from a file that already had a good parameter section. A module created in the
editor, or one loaded from a file whose parameters were dropped by this very bug, has 0
here — and skipping it would write yet another file with no parameter values, carrying the
breakage forward every time such a patch is re-saved. The module's type knows how many
parameters it has, so fall back to that.

## 10. in `parse_param_names()`

AN ENTRY IS THREE BYTES OF HEADER PLUS ITS PAYLOAD, so there has to be room for all three
before another one is read. This used to loop on `j < moduleLength`, which reads a whole
header out of whatever is left even when that is a single byte - and the two bytes it
takes beyond the section belong to the NEXT one. That is where a length of 241 and a param
index of 108 came from on a perfectly ordinary patch: not data, just the following
section misread. Whatever the trailing byte is for, it is not the start of an entry.

## 11. in `parse_param_names()`

NOT EVERY RECORD IN THIS SECTION IS A LIST OF NAME STRINGS, and assuming otherwise is
what made loading this patch from the instrument stop dead. Captured from a real G2 on
2026-08-20: a SeqNote (module type 121, at index 22) emits a six-byte record reading

```
    37 37 00 00 00 00

```
where a name record reads 01 <len> <paramIndex> <len-1 bytes of text>. 37 is SeqNote's
own parameter count, so this is evidently a per-parameter record of some other shape
rather than a name list. Read as a name record it claims a 36-byte payload inside a
6-byte module section, which trips the assert below — and that is exactly what "loads
from a file but not from the flash bank" was: the file copy of this patch carries no
such record, the copy stored on the instrument does.

Every genuine name record carries isString == 1, so test for that POSITIVELY rather
than waiting for a length to look wrong. The module's remaining bytes are stepped past
by the squaring-up loop after this one, which is what keeps the following modules — and
the following sections — correctly positioned.

WHAT THIS RECORD MEANS IS NOT DECODED, and nothing here needs it: parameter names are
cosmetic. If it is ever wanted it wants more captures, not a guess from
a single one.

## 12. in `parse_param_names()`

A payload that would run past the section means the record is genuinely malformed, and
stopping dead is deliberate — see EXIT_IN_DEBUG in defs.h. It stays an assert BECAUSE
the isString test above now turns away the one record that was reaching here
legitimately; anything still arriving is corruption worth halting on rather than
limping past. The hex dump is what made the SeqNote record identifiable, so it stays
too: if this ever fires again it should be answerable from one capture.

## 13. in `parse_param_names()`

A NAME SECTION WE CANNOT STORE IS SKIPPED, NOT FATAL. These three bounds used to
call exit(1), so one unexpected patch took the whole editor down - losing whatever
else was open - when the section is self-delimiting and the rest of the patch
parses perfectly well without it. Names are cosmetic; the modules and cables are
not, and they come later in the same stream.

Every branch consumes exactly paramLength - 1 bytes so the stream stays in step,
which is what lets parsing carry on rather than desynchronising from here on.

## 14. in `parse_midi_cc()`

A record is two bytes — a first byte then the controller NUMBER — and where several are sent
they are SEPARATED by a repeat of the 0x80 sub-response byte. Two shapes arrive here:

```
  * UNSOLICITED, one record, no separator, when the synth receives a CC assigned to nothing.
    The capture that settled it is 82 01 04 00 80 00 10 e5 cd — sub-response, record, CRC.
    The slot it belongs to is the one in the message header.
  * THE REPLY TO send_get_midi_cc(), four records, one PER SLOT in order, giving the last CC
    each slot saw. 0xff means that slot has seen none. This is what makes Learn work straight
    after the editor starts, without the user having to move the controller again.

```
Both halves of this have been wrong before: the original guard asked for three bytes and so
never ran on a two-byte payload; a flat two-byte stride then walked the four-record reply out
of step, which showed up as impossible channel numbers and invented controllers.

## 15. in `parse_patch()`

WRITE-LOCKED FOR THE WHOLE PARSE. This is the operation the lock exists for: it replaces
modules, cables and parameters wholesale, and a render pass or a snapshot build walking the
tables while it runs sees a patch that never existed. Taken here rather than at the call sites
because all three of them - both USB paths and the file load - want exactly this scope.

Safe against the read lock because no caller holds one: the file load runs from a menu action
or the backdoor, both outside render_frame(), and the USB paths are on their own thread.

## 16. in `parse_patch()`

NOT ALL PATCH LOADS CLEAR THE SLOT FIRST — a patch arriving because the device's own patch
changed is parsed straight over what was there. Bumping here as well as in the two slot-clear
paths means every route that can replace a module ends up counted exactly once or twice, and
the watcher only cares that the number moved.

## 17. `send_param_value_to_links()`

Fans a parameter value out to every LINKED variation (see variation_is_linked() in globalVars.h)
other than the one just edited — same value, same wire command, one message each, and the local
database updated to match so the canvas shows it the moment you select one of them.

Safe as a burst: send_param_value() above is COMMAND_WRITE_NO_RESP (see send_param_morph() below
for the same reasoning), so a run of them carries no acks and cannot lose a patch-version race the
way a run of per-entry bulk edits does. No whole-patch write is needed.

A linked variation is under no obligation to have been holding the same value as the edited one,
so the undo entry per variation carries that variation's OWN previous value. They undo one
variation per step rather than as a single atomic group — this app has no generic undo grouping,
and one correct step per variation beats one step that only half-reverses.

The DRAG path calls this once on RELEASE rather than on every mouse-move. A linked variation is
not on screen while you drag, so nothing is lost visually, and it keeps both the wire traffic and
the undo stack to one entry per variation instead of one per event.

## 18. `send_param_morph()`

A parameter's morph RANGE for one morph group, as distinct from its value. Same shape and the same
COMMAND_WRITE_NO_RESP class as send_param_value() above, so it carries no ack and cannot lose a
patch-version race — which is what makes it safe to send several in a row (clearing every group of
one parameter, say) rather than having to go through a whole-patch write.

Extracted from canvasDrag.c, which built this message inline. It has a second caller now — the
param context menu's morph reset — and two hand-built copies of a wire message is one too many.

## 19. in `update_module_up_rates()`

Up-rate
only propagates through a MULTI-BANDWIDTH destination connector — one that can
actually switch rate (Control/Logic), never a fixed-rate Audio input. The whole per-cable
decision is gated on the destination being multi-bandwidth (dest type != Audio) before the
source is looked at. A source is "audio rate" if its module
is already up-rated OR its connector is natively Audio. Previously the dest-type guard was only applied
to the native-Audio-source branch, so an up-rated module feeding another module's
Audio input would wrongly up-rate the destination.

## 20. in `update_module_up_rates()`

Retroactively recolour any cable already attached to one of this module's OUTPUTS,
as the original editor does — walk a bandwidth-changed module's outputs and recolour any
connected cable right when the change happens, not only at
cable-creation time. Without this, a cable drawn before its source module up-rated (or
after it de-rates) keeps showing whatever colour it inherited at creation, going stale.
Audio connectors are skipped — they're always top-rate, upRate never changes their
colour (see effective_connector_type()'s own comment, moduleResourcesAccess.h).

## 21. `write_bank_upload_file()`

Writes a .pch2/.prf2 file from pre-serialized raw content taken directly off the wire during
a Bank Upload (backup) request — NOT from the live in-memory database. The content
(version byte, type byte, patch/performance descriptor, ... , trailing CRC) is byte-identical
to a normal .pch2/.prf2 file's binary body (confirmed by diffing captured responses against
real sample files of both types), so it's written verbatim — no re-serialization or CRC
recompute needed. typeLabel is "Patch" or "Performance", matching the file's own "Type=" line.

## 22. `read_bank_upload_file()`

Reads a .pch2/.prf2 file for Bank Restore and hands back the raw binary body — the counterpart
to write_bank_upload_file's text header. The header lines it writes ("Version=...", "Type=...",
etc.) are plain ASCII followed by a single 0x00 separator byte, then the binary body begins; the
header text itself never contains a null byte, so the first 0x00 in the file unambiguously marks
where the body starts. Fills outContent (caller-owned, size outContentSize) rather than
allocating, matching this codebase's static-buffer convention (see sBankUploadContent).
