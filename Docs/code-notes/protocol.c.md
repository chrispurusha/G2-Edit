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

A PATCH SETTING HAS NO TYPE (2026-10-09). `init_patch()` created the Morph and Volume records
with a count of 0 (it now creates all seven with their counts, dataBase notes §5), and
`module_param_count(0)` is 0 too, so a patch made with File > New (or the backdoor's NEWPATCH) was
saved - and pushed - with neither: its Patch Level and Mute came
back as 127 and on whatever they had been set to. A record with no count now falls back to `patch_setting_param_count()` (dataBase notes §6).

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
is already up-rated OR its connector is Audio. Previously the dest-type guard was only applied
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

## 23. `parse_patch()` and `parse_perf()` - a patch with no Volume

A patch the editor saved before 2026-09-27 carries no Volume at all: new patches then had none,
and the writer writes only what the database holds. Left alone, the slot's Volume stays inactive, so
the top bar draws no dial and the engine reads the empty module as Level 0, off - silence with no
control to lift it (reference §63). Such a patch is given Level 100, switched on - what the G2 itself
plays it at (measured 2026-10-09: a pushed patch with no Volume came out 6.1 dB under the same patch at
127, which is the G2's own 127-to-100 step to the tenth; until then it was given 127). A patch from
the G2 always carries one, so this changes nothing for it; a USB parse of a single section may fill
the default in first, and the section that carries the real Volume overwrites it.

## 24. `parse_param_list()` - an engine-only module saved with fewer parameters than it now has

A module's parameter count on a G2 patch is fixed by the instrument, and the writer repeats whatever count the
module was read with (`actualParamCount`). An engine-only module is ours, and gains parameters as it grows -
the Sampler went from two to five on 2026-10-10. A file saved before that carries the old count, and the old
count would then have been written back on every later save, so the newer dials never reached the file (CT:
"params like release aren't saving with the patch .pchx"). Such a module is read at its full count instead:
the file's values first, the rest at their defaults in every variation.

## 25. `parse_module_list()` - a module saved with fewer modes than its type has

A patch file from an older format can carry a module with fewer modes than the instrument now gives its
type. Every ClkDiv in an old backup of BCHydro_DZLW (format version 19) carries none, where the G2 has one,
DivMode. Loaded as it was, the module went to the instrument with no mode, and the clock dividers - and
everything they clock - ran wrongly on the G2 (CT: sequences cut short, a pad wobbling as if under an LFO),
while the same performance loaded from the G2's own flash played correctly. The missing modes are now filled
from the mode table's defaults, so the module is whole both for the engine and for what is sent to the G2.

## 26. The performance settings section (`parse_perf()`, `parse_performance_settings()`, `write_perf_header()`)

The section is 0x11, a 16-bit LENGTH (the bytes after it, to the end of the fourth slot), the performance's
category, a byte holding the selected slot x 4 with the performance-pages setting in its low two bits, Key
Range (1 on), master clock BPM, the clock keyboard trigger (a key restarts the master clock), clock running, 0,
0; then per slot its name and ten bytes - enabled, Keyboard, Hold, bank, patch (where the slot's patch was loaded
from), range low, range high, the slot's MIDI channel (a synth setting copied in by the G2 that saved it, 16 = off - kept as read), 0, 0. A .prf2
holds it as is; the G2's performance-settings message over USB is the performance name followed by the same
section.

Until 2026-10-11 the editor got most of this wrong. The reader kept only each slot's name and enabled, so a
.prf2 loaded from disk (the application's File menu and the plug-in's) had every slot's Keyboard off: the
keyboard and a host's MIDI played no slot, and only a performance that plays itself made a sound. The length
word was read as "0, master volume" and written back as whatever had been read - 0 for a new performance, so
BCHydro_DZLW re-saved by the editor carried a length of 0. Category, performance pages and the clock keyboard
trigger were dropped, and the writer put Keyboard on for slot A and any slot with modules, "has modules" for
enabled, bank and patch, a full range, Key Range off, and 2 / 16 for the MIDI channels. CHECKED on 29
performances from CT's G2 backups: 28 carry a length equal to their real length (BCHydro_DZLW, the editor's,
does not), every one decodes to sensible switches and ranges (e.g. Jupiter (SC): A 0-64 and C 65-127, Key Range
on), and loading then saving each of the 28 reproduces its section byte for byte (BCHydro_DZLW gets its length, 86). A new document starts as the G2's own
empty performance: slot A enabled and on the keyboard, every range 0-127 (globalVars.c `document_defaults()`).

The editor SENDS the same section to the G2 when it pushes a performance (`send_perf_header()` in usbComms.c,
message type 0x11 followed by the section from its length on). It used to build its own, in a third layout:
"global mode", "range and flags", Key Range, 0, 0, performance mode, 0, 0, and per slot no bank and patch - so
the G2 took the slots' range bytes as bank and patch and read every range as 0-0, and (with the reader dropping
Keyboard) every slot's Keyboard off; clock and run were set again by the messages after it. Since 2026-10-11 it
sends `write_perf_header()`'s bytes. A performance stored on the G2 from the editor before then may hold those
settings.


## 27. The morph groups' names (`write_morph_group_names()`)

Every patch the G2 writes has three parameter-names sections, Morph first: the morph module (patch-settings index
1) with one 7-character name on each of its parameters 8-15 - "Group 1" to "Group 8" until renamed, e.g. "Odd"
and "Even" in VintageOrgan3, "Detune", "rate", "repeats", "Echo" in DreamPluck DLX. The reader always stored them
(they are ordinary parameter names); the writer left the section out, so every save from the editor lost them.
It now writes all eight - a stored name, else the default - as the G2 does, in files and in the patch pushed to
the G2. CHECKED: loading and saving 47 patches (44 the G2 wrote, 3 factory ones with renamed groups) reproduces
the section exactly in 29 and in all but the last byte's unused padding bits, which the G2 leaves set, in 18.

## 28. Patches saved before format 22 (`protocol_upgrade_patch()`)

The G2 converts a patch saved in an older format when it loads it, and a few parameters changed their laws or
places at format 22. The editor read such files (format 19 and earlier - the factory BCHydro_DZLW, Laika, DualBob,
KlangKit, Lounge, MorphingDrumDemo, RedDevil, RedMonday; Evolutions and jingle bellscape are 18) as they stood, so
they played wrong in the engine, and the push sent the G2 the unconverted values (CT 2026-10-12: BCHydro_DZLW's
sequenced voices sliding in pitch, in the engine and on the G2, where the G2's own stored copy was right).
Every file loader now calls this with the file's version byte after parsing; data from the G2 and the editor's own
saves are current and pass through. For format < 22:
- Glide Time, when Shape is 1: the old law's position to the new law's, `kGlideTimeFrom19` (12 -> 0, 48 -> 26,
  64 -> 38, 85 -> 54: BCHydro_DZLW's factory file against the G2's stored copy).
- LfoA and LfoC Rate on the two random waves (Wave / wave mode 4 or 5), by Range: Sub 2x+1, Lo and Hi x+12, BPM
  piecewise, Clk x+8 below 48 else x+12, each saturating at 127.
- FltHP Freq: x-5 below 101, x-3 to 110, unchanged above.
- The morph groups at [4] and [5] trade places: the morph module's dials (params 4, 5), sources (12, 13), every
  parameter's morph amounts, and knobs and controllers aimed at those morph dials; the last group's source (15)
  back to 0.
- Morph amounts dropped on parameters that lost them (CtrlRcv, CtrlSend, NoteRcv, NoteSend, NoteZone and a few
  others); MIDI controllers dropped on CtrlRcv's and CtrlSend's.
- A module saved with fewer parameters than its type has (Mix4-1S without Curve) takes the rest at their defaults.
CHECKED: BCHydro_DZLW's factory file, loaded and saved, now matches the G2's stored copy in every parameter, mode,
morph, cable and controller (the selected variation and the MIDI channels are the G2's state, not content).
NOT DONE: the format also reorders FreqShift's parameters, inputs and outputs, and removes one module type
(Unknown77) - todo.

## 29. Where one slot of a performance ends (`parse_perf()`)

A .prf2 holds four slots one after another, each opening with its patch description and closing with its patch
notes. The reader used to move to the next slot on an EMPTY notes section, which is what the editor's own writer
put there - but the G2 writes each slot's real notes, so a slot with notes kept the reader on it: BeatBox (factory,
slot B has notes) loaded C's modules into B, D's into C, and left D (its effects) empty (CT 2026-10-12: the factory
file failed where the same performance from the G2 worked). It now moves on at each slot's description, and the
writer saves each slot's notes (it wrote them empty, so a performance saved by the editor lost them).
